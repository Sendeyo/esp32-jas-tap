#include "config_manager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

bool loadDeviceConfig(DeviceConfig &config) {
  File file = LittleFS.open("/config.json", "r");
  if (!file) {
    Serial.println("Failed to open config.json");
    return false;
  }

  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return false;
  }

  config.deviceName = doc["deviceName"] | "TapBox";
  config.ledBrightness = doc["ledBrightness"] | 255;

  config.light.knownDefaultColor = doc["light"]["knownDefaultColor"] | "#00FF00";
  config.light.unknownDefaultColor = doc["light"]["unknownDefaultColor"] | "#FF00FF";
  config.light.knownCardAnimation = doc["light"]["knownCardAnimation"] | "solid";
  config.light.unknownCardAnimation = doc["light"]["unknownCardAnimation"] | "blink";
  config.light.lightDuration = doc["light"]["lightDuration"] | 1000;
  config.light.numberOfBlinks = doc["light"]["numberOfBlinks"] | 2;

  config.wifi.ssid = doc["wifi"]["ssid"] | "";
  config.wifi.password = doc["wifi"]["password"] | "";

  config.iot.enabled = doc["iot"]["enabled"] | false;
  config.iot.mode = doc["iot"]["mode"] | "none";

  return true;
}

void printDeviceConfig(const DeviceConfig &config) {
  Serial.println("------ DEVICE CONFIGURATION ------");
  Serial.println("Device Name: " + config.deviceName);
  Serial.println("LED Brightness: " + String(config.ledBrightness));
  Serial.println("Light:");
  Serial.println("  Known Color: " + config.light.knownDefaultColor);
  Serial.println("  Unknown Color: " + config.light.unknownDefaultColor);
  Serial.println("  Known Animation: " + config.light.knownCardAnimation);
  Serial.println("  Unknown Animation: " + config.light.unknownCardAnimation);
  Serial.println("  Duration: " + String(config.light.lightDuration));
  Serial.println("  Blinks: " + String(config.light.numberOfBlinks));
  Serial.println("WiFi SSID: " + config.wifi.ssid);
  Serial.println("IoT Enabled: " + String(config.iot.enabled));
  Serial.println("IoT Mode: " + config.iot.mode);
  Serial.println("----------------------------------");
}
