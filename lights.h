#pragma once

#include "quakeFlicker.h"
#include "pacifica.h"
#include "LED_functions.h"

// TODO
// * It would likely make sense, and make this code simpler, to separate conceptual lights and physical light controllers

/**
 * Data structure for serialization of a light's settings
 */
struct LightDataBlock {
  uint8_t cycleColor : 1;
  uint8_t patternID : 7; // max 128 patterns, wich should be more than fine!
  byte hue;
  byte saturation;
};

// ----------------------------------------------------------------
// ILight interface to be used to refer to all lights
// ----------------------------------------------------------------
class ILight
{
protected:
  byte _selectedPatternID = 0;

public:
  bool _cycleColor = false;

  byte _hue = 0;
  byte _saturation = 0;
  byte _val = 0;

  byte getSelectedPattern() { return _selectedPatternID; };

  /**
   * Switch to the next pattern in the available patterns. To be implemented by derived class based on how they actually implement patterns!
   */
  virtual byte nextPattern() = 0;

  /**
   * Perform some sort of update step for this light.
   * Returns true if the upate actually changed any state. Callers should do with that information what they will for performance reasons!
   * 
   * Derived classes should call their base class (which will call it's own, etc.) and check the returned value.
   */
  virtual bool update() = 0;

  /**
   * 
   */
  virtual void setup() = 0;

  /**
   * Pulse light briefly, to indicate it is in programming mode. This is a BLOCKING operation by design, to simplify having to deal with existing patterns.
   * Each subclass is in charge of implementing some way to do this and not interfere with anything.
   */
  virtual void pulse() = 0;

  void serialize(LightDataBlock* ioDataBlock)
  {
    ioDataBlock->cycleColor = _cycleColor;
    ioDataBlock->patternID = _selectedPatternID;
    ioDataBlock->hue = _hue;
    ioDataBlock->saturation = _saturation;
  };

  void deserialize(LightDataBlock* iDataBlock)
  {
    _cycleColor = iDataBlock->cycleColor;
    _selectedPatternID = iDataBlock->patternID;
    _hue = iDataBlock->hue;
    _saturation = iDataBlock->saturation;
  }
};

// ----------------------------------------------------------------
// Base light implementation with color cycle handling, if needed
// ----------------------------------------------------------------
template <bool colorSupport>
class Light : public ILight
{
private:
  uint16_t _lastColorCycleUpdate = 0;

public:
  virtual bool update() { return false; };
  virtual void setup() {};
};

template <>
bool Light<true>::update()
{
  uint16_t now = millis();

  if ((uint8_t)(now - _lastColorCycleUpdate) > 20)
  { // don't cycle colors too fast when button is held
    _lastColorCycleUpdate = now;
    if (_cycleColor) {
      ++_hue;
      return true;
    }
  }

  return true;
};

template <>
void Light<true>::setup()
{
  _lastColorCycleUpdate = millis();
};

// ----------------------------------------------------------------
// Pattern light implementation
// ----------------------------------------------------------------
template <bool colorSupport>
class PatternLight : public Light<colorSupport>
{
protected:
  byte _patternStep = 0;
  byte _prevPatternID = 0;
  uint16_t _lastLightUpdate = 0;

public:
  virtual byte nextPattern() {
    this->_selectedPatternID = ++(this->_selectedPatternID) % NUM_LIGHTSTYLES;
  };

  virtual bool update()
  {
    Light<colorSupport>::update(); // safe to ignore whether or not color cycling made any changes; we'll determine if we wanna skip updating on our own, for animation purposes
    uint16_t now = millis();

    if ((uint8_t)(now - _lastLightUpdate) < 16)
    { // no need to update faster than 60FPS
      return false;
    }

    if (this->_selectedPatternID < NUM_LIGHTSTYLES) // don't perform Quake style flicker if we're out of range of those; we'll do Pacifica instead
      this->_val = enhancedQuakeFlicker(this->_lastLightUpdate, this->_selectedPatternID, this->_prevPatternID, this->_patternStep);
    
    return true;
  };

  virtual void setup()
  {
    Light<colorSupport>::setup();
    _lastLightUpdate = millis();
    ;
  };
};

// ----------------------------------------------------------------
// A PatternLight on a WS2812B LED string (with color)
// ----------------------------------------------------------------
template <int dataPin, int cloneDataPin=0>
class PatternLightLEDStrip : public PatternLight<true>
{
  int _numLEDs;
  CRGB *_leds = nullptr;

public:
  PatternLightLEDStrip(int numLEDs) : _numLEDs(numLEDs){};

  virtual byte nextPattern() {
    this->_selectedPatternID = ++(this->_selectedPatternID) % (NUM_LIGHTSTYLES + 1); // add one to support Pacifica as an additional style, which is not handled by the quakeFlicker code
  };

  void setup()
  {
    _leds = new CRGB[_numLEDs];
    FastLED.addLeds<WS2812B, dataPin, GRB>(_leds, _numLEDs).setCorrection(TypicalLEDStrip);
    if (cloneDataPin)
      FastLED.addLeds<WS2812B, cloneDataPin, GRB>(_leds, _numLEDs).setCorrection(TypicalLEDStrip);

    PatternLight::setup();
  };

  bool update()
  {
    if (PatternLight::update())
    {
      if (_selectedPatternID < NUM_LIGHTSTYLES)
        setAllLEDs(CHSV(_hue, _saturation, _val), _leds, _numLEDs);
      else
        pacifica_loop(_leds, _numLEDs);
    }

    return true;
  };

  void pulse()
  {
    for (byte i = 0; i < 4; ++i)
    {
      setAllLEDs(CRGB::White, _leds, _numLEDs);
      FastLED.show();
      delay(100);
      setAllLEDs(CRGB::Black, _leds, _numLEDs);
      FastLED.show();
      delay(100);
    }
  };
};

// ----------------------------------------------------------------
// A PatternLight on a PWM port (no color)
// ----------------------------------------------------------------
class PatternLightPWMPort : public PatternLight<false>
{
  int _pin;

public:
  PatternLightPWMPort(int pin) : _pin(pin){};

  void setup()
  {
    pinMode(_pin, OUTPUT);
    analogWrite(_pin, 0);

    PatternLight::setup();
  };

  bool update()
  {
    if (PatternLight::update())
    {
      analogWrite(_pin, _val);
    }

    return true;
  };

  void pulse()
  {
    for (byte i = 0; i < 4; ++i)
    {
      analogWrite(_pin, 255);
      delay(100);
      analogWrite(_pin, 0);
      delay(100);
    }
  };
};

// ----------------------------------------------------------------
// A PatternLight on a digital port (no color, no PWM)
// BROKEN, DO NOT USE: need to implement a way to eliminate incompatible patterns, otherwise we'll cycle through a bunch of indistinguishable bs.
// ----------------------------------------------------------------
class PatternLightDigitalPort : public PatternLight<false>
{
  int _pin;

public:
  PatternLightDigitalPort(int pin) : _pin(pin){};

  void setup()
  {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);

    PatternLight::setup();
  };

  bool update()
  {
    if (PatternLight::update())
    {
      digitalWrite(_pin, _val);
    }

    return true;
  };

  void pulse()
  {
    for (byte i = 0; i < 4; ++i)
    {
      digitalWrite(_pin, HIGH);
      delay(100);
      digitalWrite(_pin, LOW);
      delay(100);
    }
  };
};


// ----------------------------------------------------------------
// A type of fairy lights that come with their own controllers
// We'll just use a data pin to simulate clicking the button.
// ----------------------------------------------------------------
class FairyLightsController : public ILight
{
  int _pin;
  byte _numPatternsAvailable;
  byte _offPatternID;
  byte _onPatternID;

  byte _prevPatternID = 0;

  byte patternDistance(byte currentPattern, byte desiredPattern)
  {
    int distance = desiredPattern - currentPattern;
    return distance < 0 ? distance + _numPatternsAvailable : distance;
  };

  void clickFairyLights(byte numClicks) {
    for (byte i = 0; i < numClicks; ++i) {
      digitalWrite(_pin, LOW);
      delay(40);
      digitalWrite(_pin, HIGH);
      delay(40);
    }
  }

public:
  FairyLightsController(int pin, byte numPatternsAvailable=9, byte offPatternID=0, byte onPatternID=8) : _pin(pin), _numPatternsAvailable(numPatternsAvailable), _offPatternID(offPatternID), _onPatternID(onPatternID) {};

  virtual bool update()
  {
    if (_selectedPatternID != _prevPatternID)
    {
      byte distance = patternDistance(_prevPatternID, _selectedPatternID);
      //Serial.print("updating FL controller, distance between SelectedID (#"); Serial.print(_selectedPatternID); Serial.print(") and previousID (#"); Serial.print(_prevPatternID); Serial.print(") is "); Serial.println(distance);
      clickFairyLights(distance);
      _prevPatternID = _selectedPatternID;
      return true;
    }
    return false;
  };

  virtual void setup()
  {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, HIGH);
  };

  // NB this probably needs to move to some base class shared with PatternLight but I can't be bothered right now
  virtual byte nextPattern() {
    this->_selectedPatternID = ++(this->_selectedPatternID) % _numPatternsAvailable;
  };

  void pulse()
  {
    clickFairyLights(patternDistance(_selectedPatternID, _offPatternID));
    for (byte i = 0; i < 3; ++i)
    {
      clickFairyLights(patternDistance(_offPatternID, _onPatternID));
      delay(50);
      clickFairyLights(patternDistance(_onPatternID, _offPatternID));
      delay(50);
    }
    clickFairyLights(patternDistance(_offPatternID, _selectedPatternID));
  };
};
