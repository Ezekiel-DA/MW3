#pragma once

#include <FastLED.h>

#define FTIME 100

// Quake style strobe lights, with added support for optional smoothing.
// lowercase characters mean no smoothing; uppercase adds additional smoothing.
// A given step being lowercase means no smoothing is applied when going towards it; smoothing may still applied while going away from it, if the step after that is uppercase.
String lightstyles[] = {
    "a",                                     // OFF
    "z",                                     // ON
    "HIJKLMNOPQRSTUVWXYZYXWVUTSRQPONMLKJIH", // PULSE
    //"WVXWZWVXVWVXVXVWX",                     // FLICKER
    "MMNMMOMMOMMNONMMONQNMMO",
    //"MMMMNNMMMMOOMMMMOOMMMMNNOONNMMMMOONNQQNNMMMMOO",
    "aaaaaaaazzzzzzzz",                      // SLOW STROBE
    "zzazazzzzazzazazaaazazzza",             // FLUORESCENT FLICKER
};

// whether or not it is appropriate to jitter colors for the corresponding lightstyle. Keep this in sync with the array above!
bool lighstyles_jitter[] = {
    false,
    false,
    false,
    false,
    false,
    false};
static const byte NUM_LIGHTSTYLES = sizeof(lighstyles_jitter) / sizeof(bool);

/**
 * Calculate the next light value in a Quake style light animation
 * ioPrev: previous millis() on last run; provide as input, set to current run time on exit - NB: should we take an elapsed time instead?
 * iPatternID: the ID of the pattern to use, from the lightstyles array
 * ioPreviousPatternID: storage for previous pattern ID, so we can handle pattern changes correctly
 * ioPatternStep: where we were time wise in the pattern, and where we are on exit
 * 
 * Returns the next light value, and modifies ioPrev, ioPrevPatternID, and ioPatternStep. Applying it to something is left to the caller.
 * 
 * Remaining "side effects" and globals used:
 *  - FTIME, the time step of animations in the lightstyles array
 *  - lightstyles information
 */
byte enhancedQuakeFlicker(uint16_t &ioPrev, byte iPatternID, byte &ioPrevPatternID, byte &ioPatternStep)
{
  uint16_t now = millis();
  uint8_t elapsed = (uint8_t)(now - ioPrev);

  if (ioPrevPatternID != iPatternID)
    ioPatternStep = 0;
  ioPrevPatternID = iPatternID; // safe to do unconditionally, NOOP if unchanged

  byte lightChar = (lightstyles[iPatternID][ioPatternStep]);
  byte nextLightChar = lightstyles[iPatternID][(ioPatternStep + 1) % lightstyles[iPatternID].length()];

  // the "destination" step is what controls whether or not smoothing is applied
  bool smoothing = nextLightChar < 97; // ASCII: A-Z is 65-90, a-z is 97-122

  // remap uppercase to lowercase
  lightChar = lightChar < 97 ? lightChar + 32 : lightChar;
  nextLightChar = nextLightChar < 97 ? nextLightChar + 32 : nextLightChar;

  // calculate "current" light value and next one in pattern
  byte value = lightChar - 'a';
  value = map(value, 0, 25, 0, 255);
  byte nextValue = nextLightChar - 'a'; // get the value in the next step of the pattern, without incrementing the current pattern step
  nextValue = map(nextValue, 0, 25, 0, 255);

  // lerp, if appropriate, and set all LED pixels
  byte lerpVal = smoothing ? lerp8by8(value, nextValue, elapsed >= FTIME ? 255 : (elapsed * 255 / FTIME)) : value; // this is probably a stupid way to scale elapsed to 0-255, representing 0-100% of FTIME, with appropriate clamping to 100%, but who cares

  if (elapsed >= FTIME)
  {
    ioPatternStep = ++ioPatternStep % lightstyles[iPatternID].length();
    ioPrev = now;
  }

  return lerpVal;
}