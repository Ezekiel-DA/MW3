#include <SPI.h>
#include <MFRC522.h>
#include <FastLED.h>
#include <AceButton.h>
using namespace ace_button;

#include "MW3_PIN_LAYOUT.h"
#include "config.h"
#include "lights.h"
#include "buttons.h"
#include "rfid.h"

MFRC522 rfid(MW_SPI_CS, UINT8_MAX); // RST pin (NRSTPD on MFRC522) not connected; setting it to this will let the library switch to using soft reset only

PatternLightLEDStrip<MW_STRIP_0_DATA, MW_STRIP_1_DATA> windows(NUM_LEDS_WINDOWS);
PatternLightLEDStrip<MW_STRIP_2_DATA, MW_STRIP_3_DATA> groundLights(NUM_LEDS_GROUNDLIGHTS);
FairyLightsController fairyLights(MW_STRIP_4_DATA);
PatternLightLEDStrip<MW_STRIP_5_DATA, MW_STRIP_6_DATA, MW_STRIP_7_DATA> moat(NUM_LEDS_WATERFALL_CENTER, NUM_LEDS_WATERFALL_SIDES, NUM_LEDS_WATERFALL_SIDES);
PatternLightPWMPort starfield(MW_5V_OUT_1);

ILight *lights[] = {&windows, &groundLights, & fairyLights, &moat, &starfield};
extern const byte NUM_LIGHTOBJECTS = sizeof(lights) / sizeof(void *);

static const byte defaultLightConfiguration[][15] = {
  {0x06, 0x1E, 0xFF, 0x05, 0x48, 0xFF, 0x02, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x02, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
};

byte whichObject = 0;

void setup()
{
  setupButtons();

  SPI.begin();
  rfid.PCD_Init();

  for (byte i = 0; i < NUM_LIGHTOBJECTS; ++i)
  {
    lights[i]->setup();
  }

  FastLED.setBrightness(BRIGHTNESS);
  groundLights.setMaxBrightness(180);
  FastLED.setMaxRefreshRate(60); // 60 FPS cap
  FastLED.clear();

  Serial.begin(115200);
  Serial.print("MW3 ready; ");
  Serial.print(NUM_LIGHTOBJECTS);
  Serial.println(" lights available.");

  // pulse the selected light, also serves as a boot up complete indicator
  lights[whichObject]->pulse();
};

void debug_printFPS()
{
  static uint16_t prevDebugOut = millis();
  uint16_t now = millis();
  if ((uint16_t)(now - prevDebugOut) >= 1000)
  {
    Serial.println(FastLED.getFPS());
    prevDebugOut = now;
  }
};

void writeLightSettingsToTag()
{
  // reserve space for up to 15 lights:
  // - 3 data blocks of 16 bytes each
  // - each light is encoded over 3 bytes, so up to 5 lights per block, and a padding value in the last byte of the block
  byte lightsDataBuffer[MW_RFID_DATA_BLOCK_COUNT][16];
  memset8(lightsDataBuffer, 0, sizeof(lightsDataBuffer));
  for (byte i = 0; i < 3; ++i)
    lightsDataBuffer[i][15] = 0xFF;

  for (byte lightIdx = 0; lightIdx < NUM_LIGHTOBJECTS; ++lightIdx)
  {
    lights[lightIdx]->serialize(&((LightDataBlock*)(lightsDataBuffer[lightIdx / 5]))[lightIdx % 5]);
  }

  for (byte i = 0; i < MW_RFID_DATA_BLOCK_COUNT; ++i)
  {
    Serial.print("lightsData block #"); Serial.print(i); Serial.println(" is:");
    dump_byte_array(lightsDataBuffer[i], 16); Serial.println();
  }
  
  Serial.println("Writing data to tag...");
  for (byte block = 0; block < MW_RFID_DATA_BLOCK_COUNT; ++block)
  {
    MFRC522::StatusCode ret = writeBlock(rfid, MW_RFID_DATA_BLOCK_ADDR + block, lightsDataBuffer[block]);
    if (ret != MFRC522::STATUS_OK)
    {
      Serial.print(F("Internal failure while writing to tag: "));
      Serial.println(rfid.GetStatusCodeName(ret));
      return;
    }
  }

  Serial.println("Wrote lights data to tag.");
};

void readLightSettingsFromTag()
{
  byte buffer[18]; // minimum of 16 (size of a block) + 2 (CRC)
  byte size = sizeof(buffer);

  for (byte blockOffset = 0; blockOffset < MW_RFID_DATA_BLOCK_COUNT; ++blockOffset)
  {
    byte blockAddr = MW_RFID_DATA_BLOCK_ADDR + blockOffset;
    // Serial.print("Trying to read block #"); Serial.print(blockAddr); Serial.print(" (because MW_RFID_DATA_BLOCK_ADDR="); Serial.print(MW_RFID_DATA_BLOCK_ADDR); Serial.print(" + blockOffset="); Serial.print(blockOffset); Serial.println(")");
    MFRC522::StatusCode ret = readBlock(rfid, blockAddr, buffer, &size);
    if (ret != MFRC522::STATUS_OK)
    {
      Serial.print(F("Internal failure in RFID reader: "));
      Serial.print(rfid.GetStatusCodeName(ret)); Serial.print(" while reading block #"); Serial.println(blockAddr);
      return;
    }

    Serial.print("Data in block #"); Serial.print(blockAddr); Serial.print(": ");
    dump_byte_array(buffer, 16); Serial.println();

    for (byte i = 0; i < 5; ++i)
    {
      byte lightIdx = blockOffset * 5 + i;
      if (lightIdx >= NUM_LIGHTOBJECTS) {
        // Serial.print("Light #"); Serial.print(lightIdx); Serial.println(" not installed; terminating light deserialization for this block.");
        break; // TODO: we should probably skip the next block too but whatever!
      }
      // Serial.print("Deserializing light #"); Serial.println(lightIdx);
      lights[lightIdx]->deserialize( &(((LightDataBlock*) buffer)[i]) );
    }
  }

  Serial.println("Programmed lights with tag data.");
};

void loop()
{
  //debug_printFPS();

  checkButtons();

  for (byte i = 0; i < NUM_LIGHTOBJECTS; ++i)
  {
    lights[i]->update();
  }

  FastLED.show();

  if (rfidGlobalOverride) { // apply some default lights without querying RFID reader
    for (byte block = 0; block < MW_RFID_DATA_BLOCK_COUNT; ++block)
    {
      for (byte i = 0; i < 5; ++i)
      {
        byte lightIdx = block * 5 + i;
        if (lightIdx >= NUM_LIGHTOBJECTS)
          break;
        lights[lightIdx]->deserialize( &(((LightDataBlock*) defaultLightConfiguration[block])[i]) );
      }
    }
    rfidGlobalOverride = false;
  }
  else // Get light info (or save it) from RFID
  {
    static uint16_t prevRFIDCheck = millis();
    uint16_t now = millis();
    if ((uint16_t)(now - prevRFIDCheck) < 500) // don't check the RFID reader too often; even a simple PICC_IsNewCardPresent() is costly and drops FPS from ~50 to 30
    {
      return;
    }

    prevRFIDCheck = now;
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
      ReaderSession reader(rfid); // used for automatic cleanup, regardless of errors

      printTagDebug(rfid);
      if (!checkCompatibleTag(rfid))
        return;

      if (rfidWrite)
      {
        writeLightSettingsToTag();
      } else // read tag and apply settings
      {
        readLightSettingsFromTag();
      }
    }
  }

};
