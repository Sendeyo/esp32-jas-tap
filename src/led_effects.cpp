#include "led_effects.h"

void showSolidEffect(uint32_t color, unsigned long durationMs, Adafruit_NeoPixel& strip) {
  strip.clear();
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
  }
  strip.show();
  delay(durationMs);
  strip.clear();
  strip.show();
}
