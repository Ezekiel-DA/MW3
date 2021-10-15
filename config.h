#pragma once

#include "MW3_PIN_LAYOUT.h"

// pins
#define STARFIELD_PIN MW_5V_OUT_1
#define FAIRYLIGHTS_PWM_PIN MW_5V_OUT_2
#define FAIRYLIGHTS_CONTROLLER_PIN MW_STRIP_9_DATA

// RFID settings
#define MW_RFID_DATA_BLOCK_ADDR 4
#define MW_RFID_DATA_BLOCK_COUNT 3 // this caps the number of connected lights to 15... but we only have 12 distinct ports anyway

// LED settings
#define NUM_LEDS_WINDOWS 16
#define NUM_LEDS_GROUNDLIGHTS 144
#define NUM_LEDS_WATERFALL_SIDES 10
#define NUM_LEDS_WATERFALL_CENTER 50
#define NUM_LEDS_ADMIN_RING 7
#define BRIGHTNESS 255
