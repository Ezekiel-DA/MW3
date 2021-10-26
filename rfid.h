#pragma once

#include <MFRC522.h>

MFRC522::MIFARE_Key mifareDefaultKey = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

void printHex(byte *buffer, byte bufferSize) {
  Serial.print("{ ");
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print("0x");
    Serial.print(buffer[i] < 0x10 ? "0" : "");
    Serial.print(buffer[i], HEX);
    Serial.print((i == bufferSize-1) ? " }" : ", ");
  }
}

/**
 * Print some debugging info for a tag.
 * Note: the input reader object must have already called PICC_ReadCardSerial()
 */
void printTagDebug(MFRC522& reader)
{
  MFRC522::PICC_Type piccType = reader.PICC_GetType(reader.uid.sak);
  
  Serial.print("Tag ID: "); printHex(reader.uid.uidByte, reader.uid.size); Serial.print(" - "); Serial.print(F("PICC type: ")); Serial.println(reader.PICC_GetTypeName(piccType));
}

bool checkCompatibleTag(MFRC522& reader)
{
  MFRC522::PICC_Type type = reader.PICC_GetType(reader.uid.sak);

  if (type != MFRC522::PICC_TYPE_MIFARE_MINI && type != MFRC522::PICC_TYPE_MIFARE_1K && type != MFRC522::PICC_TYPE_MIFARE_4K)
  {
    Serial.println(F("Unsupported PICC type; please use MIFARE Classic tags."));
    return false;
  }

  return true;
}

MFRC522::StatusCode readBlock(MFRC522& reader, byte iBlockAddr, byte* oData, byte* ioSize) {
  MFRC522::StatusCode status;
  
  byte trailerAddr = ((iBlockAddr / 4)*4)+3; // block address of the sector trailer for the block we're trying to read
  
  status = (MFRC522::StatusCode) reader.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerAddr, &mifareDefaultKey, &(reader.uid));
  if (status != MFRC522::STATUS_OK) {
      return status;
  }
  
  // Serial.print("Reading block #"); Serial.print(iBlockAddr); Serial.print(" - trailer address for read is: "); Serial.println(trailerAddr);
  return (MFRC522::StatusCode) reader.MIFARE_Read(iBlockAddr, oData, ioSize);
}

MFRC522::StatusCode writeBlock(MFRC522& reader, byte iBlockAddr, byte* iData)
{
  MFRC522::StatusCode status;
  
  byte trailerAddr = ((iBlockAddr / 4)*4)+3; // block address of the sector trailer for the block we're trying to read
  status = (MFRC522::StatusCode) reader.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerAddr, &mifareDefaultKey, &(reader.uid));
  if (status != MFRC522::STATUS_OK) {
      return status;
  }

  // Serial.print("Writing block #"); Serial.println(iBlockAddr); Serial.print(" - railer address for write is: "); Serial.println(trailerAddr);
  return reader.MIFARE_Write(iBlockAddr, iData, 16);
};

void dump_byte_array(byte *buffer, byte bufferSize)
{
  for (byte i = 0; i < bufferSize; i++)
  {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

class ReaderSession
{
  MFRC522& _reader;

public:
  ReaderSession(MFRC522& reader) : _reader(reader) {};
  ~ReaderSession() {
    _reader.PICC_HaltA();
    _reader.PCD_StopCrypto1();
  };
};
