#pragma once

#include <AceButton.h>
using namespace ace_button;

#include "MW3_PIN_LAYOUT.h"
#include "lights.h"

ButtonConfig adminButtonConfig;
AceButton adminButton(&adminButtonConfig);

ButtonConfig modeButtonConfig;
AceButton modeButton(&modeButtonConfig);

ButtonConfig colorButtonConfig;
AceButton colorButton(&colorButtonConfig);

ButtonConfig rfidButtonConfig;
AceButton rfidButton(&rfidButtonConfig);

// yes, this is fucking gross
extern ILight *lights[];
extern byte whichObject;
extern const byte NUM_LIGHTOBJECTS;

// some more globals to clean up, in theory...
bool rfidWrite = false;
bool rfidGlobalOverride = false;

void adminButtonEventHandler(AceButton *button, uint8_t eventType, uint8_t buttonState)
{
  switch (eventType)
  {
  case AceButton::kEventPressed:
    whichObject = ++whichObject % NUM_LIGHTOBJECTS;
    Serial.print("Controlling object #");
    Serial.println(whichObject);
    break;
  }
}

void modeButtonEventHandler(AceButton *button, uint8_t eventType, uint8_t buttonState)
{
  switch (eventType)
  {
  case AceButton::kEventPressed:
    lights[whichObject]->nextPattern();
    Serial.print("Object ");
    Serial.print(whichObject);
    Serial.print(" pattern #");
    Serial.println(lights[whichObject]->getSelectedPattern());
    break;
  }
}

void colorButtonEventHandler(AceButton *button, uint8_t eventType, uint8_t buttonState)
{
  switch (eventType)
  {
  case AceButton::kEventClicked:
    lights[whichObject]->_saturation = 255;
    lights[whichObject]->_cycleColor = !lights[whichObject]->_cycleColor;
    break;
  case AceButton::kEventLongPressed: // switch to white
    lights[whichObject]->_cycleColor = false;
    lights[whichObject]->_hue = 0;
    lights[whichObject]->_saturation = 0;
    break;
  }
}

void rfidButtonEventHandler(AceButton* button, uint8_t eventType, uint8_t buttonState)
{
  switch (eventType)
  {
    case AceButton::kEventPressed:
      rfidWrite = true;
      break;
    case AceButton::kEventReleased:
      rfidWrite = false;
      break;
    case AceButton::kEventDoubleClicked:
      rfidGlobalOverride = true;
      Serial.println("RFID reader overridden. Defaulting to standard lights.");
      break;
  }
}

void setupButtons()
{
  pinMode(MW_SW_0, INPUT_PULLUP);
  pinMode(MW_SW_1, INPUT_PULLUP);
  pinMode(MW_SW_2, INPUT_PULLUP);
  pinMode(MW_SW_3, INPUT_PULLUP);

  adminButton.init(MW_SW_0);
  modeButton.init(MW_SW_1);
  colorButton.init(MW_SW_2);
  rfidButton.init(MW_SW_3);

  adminButtonConfig.setEventHandler(adminButtonEventHandler);
  modeButtonConfig.setEventHandler(modeButtonEventHandler);

  colorButtonConfig.setFeature(ButtonConfig::kFeatureClick);
  colorButtonConfig.setFeature(ButtonConfig::kFeatureLongPress);
  colorButtonConfig.setEventHandler(colorButtonEventHandler);

  rfidButtonConfig.setFeature(ButtonConfig::kFeatureDoubleClick);
  rfidButtonConfig.setEventHandler(rfidButtonEventHandler);
}

void checkButtons()
{
  static uint16_t prev = millis();

  uint16_t now = millis();
  if ((uint16_t)(now - prev) >= 5)
  {
    adminButton.check();
    modeButton.check();
    colorButton.check();
    rfidButton.check();
    prev = now;
  }
}