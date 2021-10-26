#pragma once

// RFID settings
#define MW_RFID_DATA_BLOCK_ADDR 4
#define MW_RFID_DATA_BLOCK_COUNT 3 // this caps the number of connected lights to 15... but we only have 12 distinct ports anyway

// LED settings
#define NUM_LEDS_WINDOWS 16
#define NUM_LEDS_GROUNDLIGHTS 144+83 // NOTE: one side is actually only 142, and the other is identical + the back strip; it should be okay to drive both at the longer length
#define NUM_LEDS_WATERFALL_SIDES 20
#define NUM_LEDS_WATERFALL_CENTER 85
#define NUM_LEDS_ADMIN_RING 7
#define BRIGHTNESS 255
