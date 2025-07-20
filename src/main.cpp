#include <Wire.h>
#include <Adafruit_PN532.h>
#include <Adafruit_NeoPixel.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config_manager.h"
#include <HardwareSerial.h>

#define SDA_PIN 21
#define SCL_PIN 22
#define LED_PIN 13
#define NUM_PIXELS 24

Adafruit_PN532 nfc(SDA_PIN, SCL_PIN);
Adafruit_NeoPixel pixels(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

DeviceConfig deviceConfig;

HardwareSerial sim800(1); // Use UART1


// --- State variables for non-blocking solid effect ---
bool effectActive = false;
unsigned long effectStartTime = 0;
uint32_t currentColor = 0;
String lastCardUID = "";

// --- Utility: Convert "#RRGGBB" to uint32_t color ---
uint32_t parseHexColor(const String &hexColor) {
  String hex = hexColor;
  if (hex.charAt(0) == '#') hex = hex.substring(1);
  long number = strtol(hex.c_str(), NULL, 16);
  byte r = (number >> 16) & 0xFF;
  byte g = (number >> 8) & 0xFF;
  byte b = number & 0xFF;
  return pixels.Color(r, g, b);
}

// --- Load a card's config from LittleFS ---
bool loadCardColorAndAnimation(String uidStr, String &colorHex, String &animation) {
  String path = "/cards/" + uidStr + ".json";
  if (!LittleFS.exists(path)) {
    Serial.println("No config for UID: " + uidStr);
    return false;
  }

  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.println("Failed to open card file: " + path);
    return false;
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return false;
  }

  colorHex = doc["color"] | deviceConfig.light.knownDefaultColor;
  animation = doc["animation"] | deviceConfig.light.knownCardAnimation;
  return true;
}

// --- Trigger the solid effect ---
void startSolidEffect(uint32_t color) {
  for (int i = 0; i < NUM_PIXELS; i++) {
    pixels.setPixelColor(i, color);
  }
  pixels.show();

  effectStartTime = millis();
  effectActive = true;
  currentColor = color;
}

// --- Reset the LED strip ---
void clearLEDs() {
  pixels.clear();
  pixels.show();
  effectActive = false;
  lastCardUID = "";
}

void initSIM800L() {
  sim800.begin(9600, SERIAL_8N1, 16, 17); // RX, TX
  Serial.println("Initializing SIM800L...");

  sim800.println("AT");
  delay(1000);
  while (sim800.available()) {
    Serial.write(sim800.read());
  }

  sim800.println("AT+CSQ");  // Signal quality
  delay(1000);
  while (sim800.available()) {
    Serial.write(sim800.read());
  }

  sim800.println("AT+CCID");  // SIM card ID
  delay(1000);
  while (sim800.available()) {
    Serial.write(sim800.read());
  }
}


void setup() {
  Serial.begin(115200);
  Serial.println("Starting NFC + LED Ring...");

  pixels.begin();
  pixels.clear();
  pixels.setBrightness(128);  // Will be overridden
  pixels.show();

  initSIM800L();


  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed!");
    return;
  }

  Serial.println("LittleFS mounted.");

  if (loadDeviceConfig(deviceConfig)) {
    printDeviceConfig(deviceConfig);
    pixels.setBrightness(deviceConfig.ledBrightness);
  }

  nfc.begin();
  if (!nfc.getFirmwareVersion()) {
    Serial.println("PN532 not found.");
    while (true);
  }

  nfc.SAMConfig();
  Serial.println("Ready to read NFC cards...");
}

void loop() {
  uint8_t uid[7];
  uint8_t uidLength;

  // 1. Check for new card tap
  if (!effectActive && nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    String uidStr = "";
    for (uint8_t i = 0; i < uidLength; i++) {
      uidStr += String(uid[i], HEX);
    }
    uidStr.toUpperCase();
    Serial.println("Card UID: " + uidStr);

    // Prevent immediate duplicate detection
    if (uidStr == lastCardUID) return;
    lastCardUID = uidStr;

    // 2. Load color + animation
    String colorHex, animation;
    bool known = loadCardColorAndAnimation(uidStr, colorHex, animation);

    if (!known) {
      colorHex = deviceConfig.light.unknownDefaultColor;
      animation = deviceConfig.light.unknownCardAnimation;
    }

    if (animation == "solid") {
      uint32_t color = parseHexColor(colorHex);
      startSolidEffect(color);
    }
  }

  // 3. Check if solid effect is done
  if (effectActive) {
    unsigned long now = millis();
    if (now - effectStartTime >= deviceConfig.light.lightDuration) {
      clearLEDs();
    }
  }
}
