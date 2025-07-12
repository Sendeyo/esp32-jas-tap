#pragma once
#include <Arduino.h>

// Convert UID bytes to hex string
String uidToHex(uint8_t* uid, uint8_t length);

// Load the card's configuration
bool loadCardConfig(String uid, String& color, String& animation);
