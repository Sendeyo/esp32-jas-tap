#include "card_manager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

String uidToHex(uint8_t* uid, uint8_t length) {
  String hex = "";
  for (uint8_t i = 0; i < length; i++) {
    if (uid[i] < 0x10) hex += "0";
    hex += String(uid[i], HEX);
  }
  hex.toUpperCase();
  return hex;
}

bool loadCardConfig(String uid, String& color, String& animation) {
  String path = "/cards/" + uid + ".json";
  File file = LittleFS.open(path, "r");

  if (!file) {
    Serial.println("No config found for UID: " + uid);
    return false;
  }

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();

  if (err) {
    Serial.println("Error reading config JSON for UID: " + uid);
    return false;
  }

  color = doc["color"] | "#FFFFFF";
  animation = doc["animation"] | "solid";
  return true;
}
