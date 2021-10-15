#pragma once

void setAllLEDs(CRGB c, CRGB* strip, uint16_t numLeds) {
  for (uint16_t i = 0; i < numLeds; ++i) {
    strip[i] = c;
  }
}

void setAllLEDs(CHSV c, CRGB* strip, uint16_t numLeds) {
  for (uint16_t i = 0; i < numLeds; ++i) {
    strip[i] = c;
  }
}
