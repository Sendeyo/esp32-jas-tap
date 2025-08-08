#include "config_manager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

bool loadDeviceConfig(DeviceConfig &config) {
  File file = LittleFS.open("/config.json", "r");
  if (!file) {
    Serial.println("Failed to open config.json");
    return false;
  }

  StaticJsonDocument<2048> doc; // Increased size to fit all fields
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return false;
  }

  // General
  config.deviceName = doc["deviceName"] | "TapBox";
  config.ledBrightness = doc["ledBrightness"] | 255;
  config.mode = doc["mode"] | 0;
  config.hotspotPassword = doc["hotspotPassword"] | "12345678";

  // Light
  config.light.knownDefaultColor = doc["light"]["knownDefaultColor"] | "#00FF00";
  config.light.unknownDefaultColor = doc["light"]["unknownDefaultColor"] | "#FF00FF";
  config.light.knownCardAnimation = doc["light"]["knownCardAnimation"] | "solid";
  config.light.unknownCardAnimation = doc["light"]["unknownCardAnimation"] | "blink";
  config.light.lightDuration = doc["light"]["lightDuration"] | 1000;
  config.light.numberOfBlinks = doc["light"]["numberOfBlinks"] | 2;

  // Sound
  config.sound.tapDetection = doc["sound"]["tapDetection"] | false;
  config.sound.volume = doc["sound"]["volume"] | 100;
  config.sound.onStatus = doc["sound"]["onStatus"] | false;
  config.sound.duration = doc["sound"]["duration"] | 10;

  // WiFi
  config.wifi.ssid = doc["wifi"]["ssid"] | "";
  config.wifi.password = doc["wifi"]["password"] | "";

  // Server
  config.server.address = doc["server"]["address"] | "";
  config.server.port = doc["server"]["port"] | 80;

  // MQTT
  config.mqtt.enable = doc["mqtt"]["enable"] | false;
  config.mqtt.host = doc["mqtt"]["host"] | "";
  config.mqtt.port = doc["mqtt"]["port"] | 1883;
  config.mqtt.topic = doc["mqtt"]["topic"] | "";
  config.mqtt.user = doc["mqtt"]["user"] | "";
  config.mqtt.pass = doc["mqtt"]["pass"] | "";

  // IoT
  config.iot.enabled = doc["iot"]["enabled"] | false;

  return true;
}

void printDeviceConfig(const DeviceConfig &config) {
  Serial.println("------ DEVICE CONFIGURATION ------");
  Serial.println("Device Name: " + config.deviceName);
  Serial.println("LED Brightness: " + String(config.ledBrightness));
  Serial.println("Mode: " + String(config.mode));
  Serial.println("Hotspot Password: " + config.hotspotPassword);

  Serial.println("Light:");
  Serial.println("  Known Color: " + config.light.knownDefaultColor);
  Serial.println("  Unknown Color: " + config.light.unknownDefaultColor);
  Serial.println("  Known Animation: " + config.light.knownCardAnimation);
  Serial.println("  Unknown Animation: " + config.light.unknownCardAnimation);
  Serial.println("  Duration: " + String(config.light.lightDuration));
  Serial.println("  Blinks: " + String(config.light.numberOfBlinks));

  Serial.println("Sound:");
  Serial.println("  Tap Detection: " + String(config.sound.tapDetection));
  Serial.println("  Volume: " + String(config.sound.volume));
  Serial.println("  On Status: " + String(config.sound.onStatus));
  Serial.println("  Duration: " + String(config.sound.duration));

  Serial.println("WiFi:");
  Serial.println("  SSID: " + config.wifi.ssid);
  Serial.println("  Password: " + config.wifi.password);

  Serial.println("Server:");
  Serial.println("  Address: " + config.server.address);
  Serial.println("  Port: " + String(config.server.port));

  Serial.println("MQTT:");
  Serial.println("  Enabled: " + String(config.mqtt.enable));
  Serial.println("  Host: " + config.mqtt.host);
  Serial.println("  Port: " + String(config.mqtt.port));
  Serial.println("  Topic: " + config.mqtt.topic);
  Serial.println("  User: " + config.mqtt.user);
  Serial.println("  Pass: " + config.mqtt.pass);

  Serial.println("IoT Enabled: " + String(config.iot.enabled));
  Serial.println("----------------------------------");
}
