/*  Zero-II I²C  →  ESP8266 D1 mini  →  MQTT (matrigs/zeroii/*)
    ------------------------------------------------------------------
    • Wi-Fi STA + captive AP fallback  (web portal & OTA updater)
    • EEPROM persistence for SSID / pass / MQTT host / port / station-name
    • MQTT:
          matrigs/zeroii/cmd   – ASCII freq [Hz]
          matrigs/zeroii/data  – JSON reply
          matrigs/zeroii/debug – verbose log
    • No LEDs / WS2812FX.
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <Wire.h>
#include <RigExpertZeroII_I2C.h>
#include <ArduinoJson.h>

/* ---------------- user constants ---------------------------------- */
#define FW_VER           "ZeroII-MQTT v0.2  (S53ZO Jul-2025)"
#define USB_BAUD         115200
#define I2C_FREQ         400000UL
#define PIN_RST          D0            // GPIO16 → RST pad (open-drain)
#define AP_SSID          "ESP8266_Setup"
#define MQTT_MAX_PACKET_SIZE 2048

/* ---------------- global buffers / settings ----------------------- */
char ssid[32] = "", pass[32] = "", mqtt_host[40] = "";
int  mqtt_port = 1883;
char station_name[32] = "ZeroII-Node";

WiFiClient   net;
PubSubClient mqtt(net);
ESP8266WebServer      web(80);
ESP8266HTTPUpdateServer ota;

bool   debugEnabled   = true;            // verbose first 60 s
uint32_t debugDeadline;

static const char TOPIC_CMD[]  = "matrigs/zeroii/cmd";
static const char TOPIC_DATA[] = "matrigs/zeroii/data";
static const char TOPIC_DBG[]  = "matrigs/zeroii/debug";

/* ---------------- RigExpert object ------------------------------- */
RigExpertZeroII_I2C zeroII;

/* === EEPROM helpers (unchanged) ================================= */
#include "eeprom_load_save.h"     // put your loadConfigFromEEPROM(), saveetc

/* === Web-portal / OTA helpers (unchanged) ======================== */
#include "wifi_web_ota_blob.h"    // handleRoot(), handleSave(), setupWiFi()

/* ---------------- debug publish helper -------------------------- */
void dbg(const char *fmt, ...)
{
  char buf[256];
  va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
  Serial.println(buf);
  if (debugEnabled && mqtt.connected()) mqtt.publish(TOPIC_DBG, buf);
}

/* ---------------- reset pulse ----------------------------------- */
void pulseReset()
{
  pinMode(PIN_RST, OUTPUT_OPEN_DRAIN);
  digitalWrite(PIN_RST, LOW);  delay(25);
  pinMode(PIN_RST, INPUT);     delay(90);
  dbg("[RST] pulse sent");
}

/* ---------------- MQTT callback --------------------------------- */
void mqttCallback(char *topic, byte *payload, unsigned len)
{
  if (strcmp(topic, TOPIC_CMD) != 0) return;        // only one topic

  char line[32];
  if (len >= sizeof(line)) { dbg("[ERR] cmd too long"); return; }
  memcpy(line, payload, len); line[len] = 0;

  uint32_t f = strtoul(line, nullptr, 10);
  if (f < 100000UL || f > 1000000000UL) {
    dbg("[ERR] freq out of range");
    return;
  }
  dbg("[CMD] %lu Hz", f);

  zeroII.startMeasure(f);
  delay(60);                                        // conversion time

  float R = zeroII.getR(),  X = zeroII.getX(),
        SWR = zeroII.getSWR(), RL = zeroII.getRL();

  if (isnan(R) || isnan(X) || isnan(SWR) || isnan(RL)) {
    StaticJsonDocument<48> err;  err["error"] = "timeout";
    char js[64]; serializeJson(err, js);
    mqtt.publish(TOPIC_DATA, js);
    dbg("[ERR] timeout – sent error JSON");
    pulseReset();
    return;
  }

  StaticJsonDocument<160> doc;
  doc["f"] = f; doc["R"] = R; doc["X"] = X;
  doc["SWR"] = SWR; doc["RL"] = RL;
  doc["Zmag"] = sqrtf(R*R + X*X);
  doc["phase"] = atan2f(X, R) * 57.2957795f;

  char js[192];  size_t n = serializeJson(doc, js);
  mqtt.publish(TOPIC_DATA, js, n);
  dbg("[OK] JSON sent");
}

/* ---------------- MQTT connect ---------------------------------- */
void mqttConnect()
{
  if (mqtt.connected()) return;
  dbg("[MQTT] connecting …");
  char cid[40]; snprintf(cid, sizeof(cid), "ZeroII_%06X", ESP.getChipId());
  if (!mqtt.connect(cid)) { dbg("[MQTT] fail"); return; }
  mqtt.subscribe(TOPIC_CMD);
  dbg("[MQTT] ok – sub %s", TOPIC_CMD);
}

/* ---------------------------------------------------------------- */
void setup()
{
  Serial.begin(USB_BAUD); delay(200);
  Serial.println(); Serial.println(FW_VER);
  debugDeadline = millis() + 60000;

  loadConfigFromEEPROM();                // your helper (kept unchanged)

  setupWiFi(web, ota, ssid, pass, station_name,
            mqtt_host, mqtt_port, debugEnabled);    // no LED arg now

  mqtt.setBufferSize(MQTT_MAX_PACKET_SIZE);
  mqtt.setServer(mqtt_host, mqtt_port);
  mqtt.setCallback(mqttCallback);

  Wire.begin(D2, D1); Wire.setClock(I2C_FREQ);
  pulseReset();
  if (!zeroII.startZeroII()) dbg("[ERR] Zero-II not found");

  dbg("Topics:  cmd=%s  data=%s  debug=%s", TOPIC_CMD, TOPIC_DATA, TOPIC_DBG);
}

void loop()
{
  web.handleClient();

  if (millis() > debugDeadline) debugEnabled = false;

  if (WiFi.status() == WL_CONNECTED) mqttConnect();
  mqtt.loop();
}
