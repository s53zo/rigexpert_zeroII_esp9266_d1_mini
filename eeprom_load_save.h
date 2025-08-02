/*─────────────────────────────────────────────────────────────────────
  EEPROM helpers for SSID / password / MQTT host / port / station name
  – 96 bytes total (address map below)
  – change START_ADDR if you already use EEPROM elsewhere
─────────────────────────────────────────────────────────────────────*/
#pragma once
#include <EEPROM.h>

static constexpr uint16_t START_ADDR   = 0;   // first byte in EEPROM
static constexpr uint16_t BLOCK_SIZE   = 32;  // room for each string

/* extern globals (defined in the .ino) */
extern char ssid[];           // 32
extern char pass[];           // 32
extern char mqtt_host[];      // 40  (only 32 saved)
extern int  mqtt_port;
extern char station_name[];   // 32

/* ---------- read config from EEPROM ------------------------------ */
inline void loadConfigFromEEPROM()
{
  EEPROM.begin(256);

  EEPROM.get(START_ADDR,              ssid);
  EEPROM.get(START_ADDR + BLOCK_SIZE, pass);
  EEPROM.get(START_ADDR + 2*BLOCK_SIZE, mqtt_host);
  EEPROM.get(START_ADDR + 3*BLOCK_SIZE, mqtt_port);
  EEPROM.get(START_ADDR + 3*BLOCK_SIZE + sizeof(int), station_name);

  /* ensure NUL-termination */
  ssid[31] = pass[31] = mqtt_host[31] = station_name[31] = '\0';

  EEPROM.end();
}

/* ---------- save current globals back to EEPROM ------------------ */
inline void saveConfigToEEPROM()
{
  EEPROM.begin(256);

  EEPROM.put(START_ADDR,              ssid);
  EEPROM.put(START_ADDR + BLOCK_SIZE, pass);
  EEPROM.put(START_ADDR + 2*BLOCK_SIZE, mqtt_host);
  EEPROM.put(START_ADDR + 3*BLOCK_SIZE, mqtt_port);
  EEPROM.put(START_ADDR + 3*BLOCK_SIZE + sizeof(int), station_name);

  EEPROM.commit();
  EEPROM.end();
}
