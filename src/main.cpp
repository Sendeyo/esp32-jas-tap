#include <Wire.h>
#include <Adafruit_PN532.h>
#include <Adafruit_NeoPixel.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config_manager.h"
#include "led_effects.h"  // ✅ NEW: Use our animation helper

// === Hardware Setup ===
#define SDA_PIN 21
#define SCL_PIN 22
#define LED_PIN 13
#define NUM_PIXELS 24

Adafruit_PN532 nfc(SDA_PIN, SCL_PIN);
Adafruit_NeoPixel pixels(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

DeviceConfig deviceConfig;

// === Convert "#RRGGBB" to NeoPixel Color ===
uint32_t parseHexColor(String hexColor) {
  if (hexColor.charAt(0) == '#') hexColor = hexColor.substring(1);
  long number = strtol(hexColor.c_str(), NULL, 16);
  byte r = (number >> 16) & 0xFF;
  byte g = (number >> 8) & 0xFF;
  byte b = number & 0xFF;
  return pixels.Color(r, g, b);
}

// === Load card config from file ===
bool loadCardColorAndAnimation(String uidStr, String &colorHex, String &animation) {
  String path = "/cards/" + uidStr + ".json";
  if (!LittleFS.exists(path)) {
    Serial.println("No config found for UID: " + uidStr);
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.println("Failed to open card config: " + path);
    return false;
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.print("JSON error: ");
    Serial.println(error.c_str());
    return false;
  }

  colorHex = doc["color"] | deviceConfig.light.knownDefaultColor;
  animation = doc["animation"] | deviceConfig.light.knownCardAnimation;
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting NFC + LED Ring...");

  pixels.begin();
  pixels.clear();
  pixels.setBrightness(128);  // Temporary until config loads
  pixels.show();

  if (!LittleFS.begin()) {
    Serial.println("Failed to mount LittleFS");
    return;
  }
  Serial.println("LittleFS mounted successfully.");

  if (!loadDeviceConfig(deviceConfig)) {
    Serial.println("Using fallback/default config");
  } else {
    printDeviceConfig(deviceConfig);
    pixels.setBrightness(deviceConfig.ledBrightness);
  }

  nfc.begin();
  if (!nfc.getFirmwareVersion()) {
    Serial.println("PN532 not found.");
    while (1);
  }

  nfc.SAMConfig();
  Serial.println("Ready to read NFC cards...");
}

void loop() {
  uint8_t uid[7];
  uint8_t uidLength;

  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    String uidStr = "";
    for (uint8_t i = 0; i < uidLength; i++) {
      uidStr += String(uid[i], HEX);
    }
    uidStr.toUpperCase();
    Serial.println("Card UID: " + uidStr);

    String colorHex, animation;
    bool knownCard = loadCardColorAndAnimation(uidStr, colorHex, animation);

    if (!knownCard) {
      colorHex = deviceConfig.light.unknownDefaultColor;
      animation = deviceConfig.light.unknownCardAnimation;
    }

    uint32_t color = parseHexColor(colorHex);

    if (animation == "solid") {
      showSolidEffect(color, deviceConfig.light.lightDuration, pixels);  // ✅ Use solid effect helper
    }

    delay(100);  // Slight delay to prevent double-tap detection
  }
}
