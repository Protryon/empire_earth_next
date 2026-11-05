// C:\EEX\Low-Level Engine\Network Engine\NETimestampedMessage.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// Layout: [timestamp 4][message size 4][message]. ESVERIFY lines 22, 59, 69.
#include <windows.h>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/NEPlayer.h"
#include "lle/NEMessage.h"
#include "lle/NETimestampedMessage.h"

// Release-build trace: compiled out, but its argument is still evaluated
inline void NETimestampedTrace(unsigned long inSize) {}

// FUNCTION: LLE 0x1004b04f
// Creates a timestamped message from a recorded buffer (timestamp, then the message)
NETimestampedMessage::NETimestampedMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 22);
    memcpy(&this->mTimestamp, inPayloadPointer, sizeof(this->mTimestamp));
    this->mMessage = NEMessage::ConvertMemoryToMessage(inPayloadPointer + 4);
    if (this->mMessage != NULL)
        NETimestampedTrace(this->mMessage->ExportSize());
}

// FUNCTION: LLE 0x1004b09b
// Exports the message as [timestamp][size][message] in a new buffer
void NETimestampedMessage::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    uint32_t theMessageSize = this->mMessage->ExportSize();   // 4 bytes in the record
    unsigned long theTimestampedMessageSize = theMessageSize + 8;
    unsigned char* theData;
    unsigned long theDataSize;
    unsigned long theLocation;
    *outPayloadPointer = new unsigned char[theTimestampedMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    memcpy(*outPayloadPointer, &this->mTimestamp, sizeof(this->mTimestamp));
    memcpy(*outPayloadPointer + 4, &theMessageSize, sizeof(theMessageSize));
    this->mMessage->ExportMessage(&theData, theDataSize);
    memcpy(*outPayloadPointer + 8, theData, theDataSize);
    theLocation = theDataSize + 8;
    delete theData;
    ESVERIFY_AT(theLocation == theTimestampedMessageSize, 59);
    outMessageSize = theTimestampedMessageSize;
}

// FUNCTION: LLE 0x1004b138
// Reads the timestamp and message from a recorded buffer
void NETimestampedMessage::ImportMessage(unsigned char* inPayloadPointer)
{
    uint32_t theMessageSize;
    ESVERIFY_AT(inPayloadPointer != NULL, 69);
    memcpy(&this->mTimestamp, inPayloadPointer, sizeof(this->mTimestamp));
    memcpy(&theMessageSize, inPayloadPointer + 4, sizeof(theMessageSize));
    this->mMessage = NEMessage::ConvertMemoryToMessage(inPayloadPointer + 8);
    NETimestampedTrace(this->mMessage->ExportSize());
}
