#include <Wire.h>
#include <Adafruit_PN532.h>
#include <Adafruit_NeoPixel.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config_manager.h"
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WebServer.h>  // ✅ Replaces ESPAsyncWebServer

#define SDA_PIN 21
#define SCL_PIN 22
#define LED_PIN 13
#define NUM_PIXELS 24

Adafruit_PN532 nfc(SDA_PIN, SCL_PIN);
Adafruit_NeoPixel pixels(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

DeviceConfig deviceConfig;

WebServer server(80);  // ✅ Synchronous server

bool effectActive = false;
unsigned long effectStartTime = 0;
uint32_t currentColor = 0;
String lastCardUID = "";

uint32_t parseHexColor(const String &hexColor) {
  String hex = hexColor;
  if (hex.charAt(0) == '#') hex = hex.substring(1);
  long number = strtol(hex.c_str(), NULL, 16);
  byte r = (number >> 16) & 0xFF;
  byte g = (number >> 8) & 0xFF;
  byte b = number & 0xFF;
  return pixels.Color(r, g, b);
}

String loadConfigAsString() {
  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) return "{}";
  String content = configFile.readString();
  configFile.close();
  return content;
}

bool saveConfigFromString(const String &jsonString) {
  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) return false;
  configFile.print(jsonString);
  configFile.close();
  return true;
}

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

void startSolidEffect(uint32_t color) {
  for (int i = 0; i < NUM_PIXELS; i++) {
    pixels.setPixelColor(i, color);
  }
  pixels.show();
  effectStartTime = millis();
  effectActive = true;
  currentColor = color;
}

void clearLEDs() {
  pixels.clear();
  pixels.show();
  effectActive = false;
  lastCardUID = "";
}

void connectToWiFi() {
  Serial.println("Setting up AP + STA...");
  WiFi.mode(WIFI_AP_STA);
  bool apSuccess = WiFi.softAP("JasTapBox 1", "12345678");

  if (apSuccess) {
    Serial.println("Access Point started");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("Failed to start AP mode");
  }

  WiFi.begin(deviceConfig.wifi.ssid.c_str(), deviceConfig.wifi.password.c_str());

  unsigned long startTime = millis();
  const unsigned long timeout = 10000;
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < timeout) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi STA connected!");
    Serial.print("STA IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to Wi-Fi (STA), but AP remains active.");
  }
}

// --- 🧠 Web Handlers ---
void handleRoot() {
  String html = "<html><body><h2>Edit Config</h2>";
  html += "<form method='POST' action='/save'>";
  html += "<textarea name='config' rows='30' cols='80'>";
  html += loadConfigAsString();
  html += "</textarea><br><br>";
  html += "<input type='submit' value='Save & Reboot'>";
  html += "</form></body></html>";
  server.send(200, "text/html", html);
}

void handleSave() {
  if (!server.hasArg("config")) {
    server.send(400, "text/plain", "Missing config data.");
    return;
  }

  String raw = server.arg("config");
  if (saveConfigFromString(raw)) {
    server.send(200, "text/html", "<html><body><h3>Saved! Rebooting...</h3></body></html>");
    delay(1000);
    ESP.restart();
  } else {
    server.send(500, "text/plain", "Failed to save config.");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting NFC + LED Ring...");

  pixels.begin();
  pixels.clear();
  pixels.setBrightness(128);
  pixels.show();

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed!");
    return;
  }

  Serial.println("LittleFS mounted.");

  if (loadDeviceConfig(deviceConfig)) {
    printDeviceConfig(deviceConfig);
    pixels.setBrightness(deviceConfig.ledBrightness);
    connectToWiFi();
  }

  nfc.begin();
  if (!nfc.getFirmwareVersion()) {
    Serial.println("PN532 not found.");
    while (true);
  }

  // ✅ Replace Async handlers with sync server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
  Serial.println("HTTP server started");

  nfc.SAMConfig();
  Serial.println("Ready to read NFC cards...");
}

void loop() {
  server.handleClient();  // ✅ Required for WebServer to handle requests

  uint8_t uid[7];
  uint8_t uidLength;

  if (!effectActive && nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    String uidStr = "";
    for (uint8_t i = 0; i < uidLength; i++) {
      uidStr += String(uid[i], HEX);
    }
    uidStr.toUpperCase();
    Serial.println("Card UID: " + uidStr);

    if (uidStr == lastCardUID) return;
    lastCardUID = uidStr;

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

  if (effectActive) {
    unsigned long now = millis();
    if (now - effectStartTime >= deviceConfig.light.lightDuration) {
      clearLEDs();
    }
  }
}
