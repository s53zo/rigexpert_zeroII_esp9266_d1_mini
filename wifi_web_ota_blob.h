/*─────────────────────────────────────────────────────────────────────
  Minimal Wi-Fi  +  Web portal  +  OTA updater
  – No WS2812FX
  – AP fallback “ESP8266_Setup” if STA connection fails
─────────────────────────────────────────────────────────────────────*/
#pragma once
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

/* extern globals supplied by .ino */
extern char ssid[];
extern char pass[];
extern char mqtt_host[];
extern int  mqtt_port;
extern char station_name[];
extern bool debugEnabled;

/* forward declarations so handlers may call them */
void saveConfigToEEPROM();
void loadConfigFromEEPROM();

/* ---------- HTML helpers ---------------------------------------- */
static const char HEAD[] PROGMEM =
"<!DOCTYPE html><html><head><meta charset='utf-8'><title>Zero-II Wi-Fi</title>"
"<style>body{font-family:Arial;margin:20px;}input{width:260px;}</style></head><body>";

static const char FOOT[] PROGMEM =
"<p><a href=\"/update\">OTA Update</a></p></body></html>";

/* ---------- handlers -------------------------------------------- */
void handleRoot(ESP8266WebServer &srv)
{
  srv.setContentLength(CONTENT_LENGTH_UNKNOWN);
  srv.send(200, "text/html", "");
  srv.sendContent_P(HEAD);

  char form[600];
  snprintf_P(form, sizeof(form),
    PSTR("<h2>Config</h2><form action=\"/save\" method=\"post\">"
         "SSID:<br><input name=\"s\" value=\"%s\"><br>"
         "Password:<br><input name=\"p\" value=\"%s\"><br>"
         "MQTT host:<br><input name=\"h\" value=\"%s\"><br>"
         "MQTT port:<br><input name=\"o\" type=\"number\" value=\"%d\"><br>"
         "Station name:<br><input name=\"n\" value=\"%s\"><br><br>"
         "<input type=\"submit\" value=\"Save & Reboot\"></form>"),
         ssid, pass, mqtt_host, mqtt_port, station_name);

  srv.sendContent(form);
  srv.sendContent_P(FOOT);
  srv.sendContent("");
}

void handleSave(ESP8266WebServer &srv)
{
  strncpy(ssid,         srv.arg("s").c_str(), sizeof(ssid)-1);
  strncpy(pass,         srv.arg("p").c_str(), sizeof(pass)-1);
  strncpy(mqtt_host,    srv.arg("h").c_str(), sizeof(mqtt_host)-1);
  mqtt_port =           srv.arg("o").toInt();
  strncpy(station_name, srv.arg("n").c_str(), sizeof(station_name)-1);

  saveConfigToEEPROM();

  srv.send(200, "text/html",
           "<html><body><h3>Saved! Rebooting…</h3></body></html>");
  delay(1200);
  ESP.restart();
}

/* ---------- STA / AP + web/OTA bootstrap ------------------------ */
inline void setupWiFi(ESP8266WebServer &srv,
                      ESP8266HTTPUpdateServer &ota,
                      const char* ssidIn, const char* passIn,
                      const char* hostName,
                      const char* mqttHost, int mqttPort,
                      bool dbg)
{
  /* use current globals as initial credentials */
  WiFi.mode(WIFI_STA);
  WiFi.hostname(hostName);
  WiFi.begin(ssidIn, passIn);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 30000) {
    delay(250);
    if (dbg) Serial.print('.');
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (dbg) Serial.println("\n[WiFi] STA failed – starting AP fallback");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID);
  }

  if (dbg) {
    Serial.print("\n[WiFi] IP="); Serial.println(WiFi.localIP());
  }

  /* Web portal & OTA */
  srv.on("/",     [&](){ handleRoot(srv); });
  srv.on("/save", [&](){ handleSave(srv); });
  ota.setup(&srv);
  srv.begin();
}
