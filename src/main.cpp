#include <Wire.h>
#include <Adafruit_PN532.h>
#include <Adafruit_NeoPixel.h>
#include <LittleFS.h>
#include <ArduinoJson.h>


#define SDA_PIN 21
#define SCL_PIN 22
#define LED_PIN 13
#define NUM_PIXELS 24

Adafruit_PN532 nfc(SDA_PIN, SCL_PIN);
Adafruit_NeoPixel pixels(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);


uint32_t getColorForUID(uint8_t* uid, uint8_t length) {
  // Use the UID bytes to create a color

  return pixels.Color(0, 255, 0);
}


void setup() {
  Serial.begin(115200);
  Serial.println("Starting NFC + LED Ring...");

  pixels.begin();
  pixels.clear();
  pixels.show();

  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("PN532 not found.");
    while (1);
  }

  nfc.SAMConfig();
  Serial.println("Ready to read NFC cards...");

  if (!LittleFS.begin()) {
  Serial.println("Failed to mount LittleFS");
  return;
  }

  Serial.println("LittleFS mounted successfully.");

}

void loop() {
  uint8_t uid[7];
  uint8_t uidLength;

  if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
    Serial.print("Card UID: ");
    for (uint8_t i = 0; i < uidLength; i++) {
      Serial.print(uid[i], HEX);
    }
    Serial.println();

    // Get a color for the UID
    uint32_t color = getColorForUID(uid, uidLength);

    // Fill all pixels
    for (int i = 0; i < NUM_PIXELS; i++) {
      pixels.setPixelColor(i, color);
    }
    pixels.show();

    delay(2000); // Keep color for 2 seconds

    // Optional blink for confirmation

    // Clear after
    pixels.clear();
    pixels.show();
  }
}

