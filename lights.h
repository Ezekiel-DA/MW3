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
// A PatternLight on one or more WS2812B LED string (with color)
// Support up to three strips; strips can be of different lengths.
// See constructor for hackish details of multiple strip support.
//
// TODO: handling of multiple strips is pretty hackish and gross, but
// at this point I don't want to clean it up for MW3. Maybe for MW4!
//
// ----------------------------------------------------------------
template <int dataPin1, int dataPin2=0, int dataPin3=0>
class PatternLightLEDStrip : public PatternLight<true>
{
  int _numLEDs1;
  int _numLEDs2;
  int _numLEDs3;
  CRGB *_leds1 = nullptr;
  CRGB *_leds2 = nullptr;
  CRGB *_leds3 = nullptr;

public:
  // Number of LEDs in the strip is optional if strips 2 and 3 are present. For any of strip 2 or 3 where the number of LEDs is not specified, two things will happen:
  // - the number of LEDs from strip 1 will be used
  // - the buffer for strip 1 will be reused
  PatternLightLEDStrip(int numLEDs1, int numLEDs2=0, int numLEDs3=0) : _numLEDs1(numLEDs1), _numLEDs2(numLEDs2), _numLEDs3(numLEDs3)
  {
    if (!_numLEDs2)
        _numLEDs2 = _numLEDs1;
    if (!_numLEDs3)
        _numLEDs3 = _numLEDs1;
  };

  virtual byte nextPattern()
  {
    this->_selectedPatternID = ++(this->_selectedPatternID) % (NUM_LIGHTSTYLES + 1); // add one to support Pacifica as an additional style, which is not handled by the quakeFlicker code
  };

  void setup()
  {
    _leds1 = new CRGB[_numLEDs1];
    FastLED.addLeds<WS2812B, dataPin1, GRB>(_leds1, _numLEDs1).setCorrection(TypicalLEDStrip);
    if (dataPin2)
    {
      if (_numLEDs2 != _numLEDs1)
        _leds2 = new CRGB[_numLEDs2];

      FastLED.addLeds<WS2812B, dataPin2, GRB>(_numLEDs2 == _numLEDs1 ? _leds1 : _leds2, _numLEDs2).setCorrection(TypicalLEDStrip);
    }
    if (dataPin3)
    {
      if (_numLEDs3 != _numLEDs1)
        _leds3 = new CRGB[_numLEDs3];

      FastLED.addLeds<WS2812B, dataPin3, GRB>(_numLEDs3 == _numLEDs1 ? _leds1 : _leds3, _numLEDs3).setCorrection(TypicalLEDStrip);
    }

    PatternLight::setup();
  };

  bool update()
  {
    if (PatternLight::update())
    {
      if (_selectedPatternID < NUM_LIGHTSTYLES)
      {
        setAllLEDs(CHSV(_hue, _saturation, _val), _leds1, _numLEDs1);
        if (_leds2)
          setAllLEDs(CHSV(_hue, _saturation, _val), _leds2, _numLEDs2);
        if (_leds3)
          setAllLEDs(CHSV(_hue, _saturation, _val), _leds3, _numLEDs3);
      }
      else
      {
        pacifica_loop(_leds1, _numLEDs1);
        if (_leds2)
          pacifica_loop(_leds2, _numLEDs2);
        if (_leds3)
          pacifica_loop(_leds3, _numLEDs3);
      }
    }

    return true;
  };

  void pulse()
  {
    for (byte i = 0; i < 4; ++i)
    {
      setAllLEDs(CRGB::White, _leds1, _numLEDs1);
      if (_leds2)
        setAllLEDs(CRGB::White, _leds2, _numLEDs2);
      if (_leds3)
        setAllLEDs(CRGB::White, _leds3, _numLEDs3);
      FastLED.show();
      delay(100);
      setAllLEDs(CRGB::Black, _leds1, _numLEDs1);
      if (_leds2)
        setAllLEDs(CRGB::Black, _leds2, _numLEDs2);
      if (_leds3)
        setAllLEDs(CRGB::Black, _leds3, _numLEDs3);
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
