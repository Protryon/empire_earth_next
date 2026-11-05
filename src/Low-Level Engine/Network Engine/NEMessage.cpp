// C:\EEX\Low-Level Engine\Network Engine\NEMessage.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// Message (de)serialization: a 4-byte header (reserved, type, for-slot, originating slot) then each message's
// fields in its own order.
#include <windows.h>
#include <string.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UReferenceCounter.h"
#include "lle/NEPlayer.h"
#include "lle/NEMessage.h"

NEMessage* (*NEMessage::sConstructors[256])(unsigned char*);

// FUNCTION: LLE 0x1003e05c
// Builds a message from received bytes with the constructor registered for its type; NULL for an unknown type
NEMessage* NEMessage::ConvertMemoryToMessage(unsigned char* inMemory)
{
    unsigned char theType = inMemory[1];
    ESVERIFY_AT(inMemory != NULL, 31);
    if (NEMessage::sConstructors[theType] == NULL)
        return NULL;
    return NEMessage::sConstructors[theType](inMemory);
}

// FUNCTION: LLE 0x1003e093
// Writes the 4-byte message header (reserved, type, target slot, originating slot); returns 4
unsigned long NEMessage::ExportHeader(unsigned char* outData)
{
    memcpy(outData, &this->mReserved, sizeof(this->mReserved));
    memcpy(outData + 1, &this->mType, sizeof(this->mType));
    memcpy(outData + 2, &this->mForSpecificSlot, sizeof(this->mForSpecificSlot));
    memcpy(outData + 3, &this->mOriginatingSlot, sizeof(this->mOriginatingSlot));
    return 4;
}

// FUNCTION: LLE 0x1003e0df
// Reads the 4-byte message header (reserved, type, target slot, originating slot); returns 4
unsigned long NEMessage::ImportHeader(unsigned char* inData)
{
    memcpy(&this->mReserved, inData, sizeof(this->mReserved));
    memcpy(&this->mType, inData + 1, sizeof(this->mType));
    memcpy(&this->mForSpecificSlot, inData + 2, sizeof(this->mForSpecificSlot));
    memcpy(&this->mOriginatingSlot, inData + 3, sizeof(this->mOriginatingSlot));
    return 4;
}

// FUNCTION: LLE 0x1003e12b
// Registers the engine's own message builder for the engine message types 0x20..0x3c
void NEMessage::RegisterKnownMessages()
{
    unsigned long i;
    for (i = 0x20; i <= 0x3c; i++)
        NEMessage::sConstructors[i] = NEMessage::BuildNEMessage;
}

// FUNCTION: LLE 0x1003e13d
// Builds the engine message of the type in the header from received bytes; asserts on an unknown type
NEMessage* NEMessage::BuildNEMessage(unsigned char* inData)
{
    NEMessage* theMessage = NULL;
    switch (inData[1]) {
    case kNetMessageAddPlayer: theMessage = new NEMAddPlayer(inData); break;
    case kNetMessageAddPlayerResponse: theMessage = new NEMAddPlayerResponse(inData); break;
    case kNetMessageClientConnectionRequest: theMessage = new NEMClientConnectionRequest(inData); break;
    case kNetMessageClientConnectionRequestResponse: theMessage = new NEMClientConnectionRequestResponse(inData); break;
    case kNetMessageClientDisconnectionNotification: theMessage = new NEMClientDisconnectionNotification(inData); break;
    case kNetMessageClientDisconnectionNotificationResponse: theMessage = new NEMClientDisconnectionNotificationResponse(inData); break;
    case kNetMessageClientReconnectionRequest: theMessage = new NEMClientReconnectionRequest(inData); break;
    case kNetMessageClientReconnectionRequestResponse: theMessage = new NEMClientReconnectionRequestResponse(inData); break;
    case kNetMessageClientRequestPause: theMessage = new NEMClientRequestPause(inData); break;
    case kNetMessageCommunicationsTick: theMessage = new NEMCommunicationsTick(inData); break;
    case kNetMessageCommunicationsTickResponse: theMessage = new NEMCommunicationsTickResponse(inData); break;
    case kNetMessagePauseGame: theMessage = new NEMPauseGame(inData); break;
    case kNetMessagePauseGameResponse: theMessage = new NEMPauseGameResponse(inData); break;
    case kNetMessagePhysicsTick: theMessage = new NEMPhysicsTick(inData); break;
    case kNetMessagePlayerConnectionCheck: theMessage = new NEMPlayerConnectionCheck(inData); break;
    case kNetMessagePlayerConnectionCheckResponse: theMessage = new NEMPlayerConnectionCheckResponse(inData); break;
    case kNetMessageRemovePlayer: theMessage = new NEMRemovePlayer(inData); break;
    case kNetMessageServerIsAlive: theMessage = new NEMServerIsAlive(inData); break;
    case kNetMessageStartGame: theMessage = new NEMStartGame(inData); break;
    case kNetMessageStartGameResponse: theMessage = new NEMStartGameResponse(inData); break;
    case kNetMessageStopGame: theMessage = new NEMStopGame(inData); break;
    case kNetMessageStopGameResponse: theMessage = new NEMStopGameResponse(inData); break;
    case kNetMessageUnPauseGame: theMessage = new NEMUnPauseGame(inData); break;
    case kNetMessageUnPauseGameResponse: theMessage = new NEMUnPauseGameResponse(inData); break;
    case kNetMessageWorldDatabaseChecksum: theMessage = new NEMWorldDatabaseChecksum(inData); break;
    case kNetMessageWorldDatabaseChecksumResponse: theMessage = new NEMWorldDatabaseChecksumResponse(inData); break;
    case kNetMessageXferMessage: theMessage = new NEMXferMessage(inData); break;
    case kNetMessageXferStatus: theMessage = new NEMXferStatus(inData); break;
    default:
        ESVERIFY_AT("Undefined message type" == NULL, 227);
    }
    return theMessage;
}

// FUNCTION: LLE 0x1003e751
// Returns the exported size of an add-player message: header, player index and name
unsigned long NEMAddPlayer::ExportSize()
{
    return this->mPlayerName.GetExportSize() + 5;
}

// FUNCTION: LLE 0x1003e75d
// Exports an add-player message (player index and name) into a new buffer
void NEMAddPlayer::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mPlayerIndex, sizeof(this->mPlayerIndex));
    theLocation += sizeof(this->mPlayerIndex);
    this->mPlayerName.Export(*outPayloadPointer + theLocation);
    theLocation += this->mPlayerName.GetExportSize();
    ESVERIFY_AT(theLocation == theMessageSize, 267);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003e7de
// Imports an add-player message (player index and name)
void NEMAddPlayer::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 277);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mPlayerIndex, inPayloadPointer + theLocation, sizeof(this->mPlayerIndex));
    theLocation += sizeof(this->mPlayerIndex);
    this->mPlayerName.Import(inPayloadPointer + theLocation);
}

unsigned long NEMAddPlayerResponse::ExportSize()   // header only (4 bytes); ICF-folded at 0x3fb10
{
    return 4;
}

// FUNCTION: LLE 0x1003e829
// Exports an add-player response (header only) into a new buffer
void NEMAddPlayerResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    ESVERIFY_AT(theLocation == theMessageSize, 323);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003e873
// Imports an add-player response (header only)
void NEMAddPlayerResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 333);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
}

// FUNCTION: LLE 0x1003e8a3
// Returns the exported size of a client connection request: firewall flag, version, player name and checksum
unsigned long NEMClientConnectionRequest::ExportSize()
{
    return this->mVersion.GetExportSize() + this->mPlayerName.GetExportSize() + 9;
}

// FUNCTION: LLE 0x1003e8c0
// Exports a client connection request (firewall flag, version, player name, binary checksum) into a new buffer
void NEMClientConnectionRequest::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mIsBehindFirewall, sizeof(this->mIsBehindFirewall));
    theLocation += sizeof(this->mIsBehindFirewall);
    this->mVersion.Export(*outPayloadPointer + theLocation);
    theLocation += this->mVersion.GetExportSize();
    this->mPlayerName.Export(*outPayloadPointer + theLocation);
    theLocation += this->mPlayerName.GetExportSize();
    memcpy(*outPayloadPointer + theLocation, &this->mBinaryChecksum, sizeof(this->mBinaryChecksum));
    theLocation += sizeof(this->mBinaryChecksum);
    ESVERIFY_AT(theLocation == theMessageSize, 390);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003e971
// Imports a client connection request (firewall flag, version, player name, binary checksum)
void NEMClientConnectionRequest::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 400);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mIsBehindFirewall, inPayloadPointer + theLocation, sizeof(this->mIsBehindFirewall));
    theLocation += sizeof(this->mIsBehindFirewall);
    theLocation += this->mVersion.Import(inPayloadPointer + theLocation);
    theLocation += this->mPlayerName.Import(inPayloadPointer + theLocation);
    memcpy(&this->mBinaryChecksum, inPayloadPointer + theLocation, sizeof(this->mBinaryChecksum));
}

// FUNCTION: LLE 0x1003e9e5
// Returns the exported size of a connection response: error, UI slot, send-to slot, current tick and version
unsigned long NEMClientConnectionRequestResponse::ExportSize()
{
    return this->mVersion.GetExportSize() + 14;
}

// FUNCTION: LLE 0x1003e9f1
// Exports a connection response (error, UI slot, send-to slot, current tick, version) into a new buffer
void NEMClientConnectionRequestResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mError, sizeof(this->mError));
    theLocation += sizeof(this->mError);
    memcpy(*outPayloadPointer + theLocation, &this->mUISlotNumber, sizeof(this->mUISlotNumber));
    theLocation += sizeof(this->mUISlotNumber);
    memcpy(*outPayloadPointer + theLocation, &this->mSendToSlot, sizeof(this->mSendToSlot));
    theLocation += sizeof(this->mSendToSlot);
    memcpy(*outPayloadPointer + theLocation, &this->mCurrentCT, sizeof(this->mCurrentCT));
    theLocation += sizeof(this->mCurrentCT);
    this->mVersion.Export(*outPayloadPointer + theLocation);
    theLocation += this->mVersion.GetExportSize();
    ESVERIFY_AT(theLocation == theMessageSize, 472);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003eaad
// Imports a connection response (error, UI slot, send-to slot, current tick, version)
void NEMClientConnectionRequestResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 482);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mError, inPayloadPointer + theLocation, sizeof(this->mError));
    theLocation += sizeof(this->mError);
    memcpy(&this->mUISlotNumber, inPayloadPointer + theLocation, sizeof(this->mUISlotNumber));
    theLocation += sizeof(this->mUISlotNumber);
    memcpy(&this->mSendToSlot, inPayloadPointer + theLocation, sizeof(this->mSendToSlot));
    theLocation += sizeof(this->mSendToSlot);
    memcpy(&this->mCurrentCT, inPayloadPointer + theLocation, sizeof(this->mCurrentCT));
    theLocation += sizeof(this->mCurrentCT);
    this->mVersion.Import(inPayloadPointer + theLocation);
}

// FUNCTION: LLE 0x1003eb31
// Returns the exported size of a client disconnection notice: header and exiting flag (5 bytes)
unsigned long NEMClientDisconnectionNotification::ExportSize()
{
    return 5;
}

// FUNCTION: LLE 0x1003eb35
// Exports a client disconnection notice (exiting flag) into a new buffer
void NEMClientDisconnectionNotification::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mIsExiting, sizeof(this->mIsExiting));
    theLocation += sizeof(this->mIsExiting);
    ESVERIFY_AT(theLocation == theMessageSize, 542);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003eb9e
// Imports a client disconnection notice (exiting flag)
void NEMClientDisconnectionNotification::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 552);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mIsExiting, inPayloadPointer + theLocation, sizeof(this->mIsExiting));
}

unsigned long NEMClientDisconnectionNotificationResponse::ExportSize()   // header and one long (8 bytes); ICF-folded at 0x3fc79
{
    return 8;
}

// FUNCTION: LLE 0x1003ebdf
// Exports a disconnection response (error code) into a new buffer
void NEMClientDisconnectionNotificationResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mError, sizeof(this->mError));
    theLocation += sizeof(this->mError);
    ESVERIFY_AT(theLocation == theMessageSize, 600);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003ec4a
// Imports a disconnection response (error code)
void NEMClientDisconnectionNotificationResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 610);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mError, inPayloadPointer + theLocation, sizeof(this->mError));
}

unsigned long NEMClientReconnectionRequest::ExportSize()   // header and two longs (12 bytes); ICF-folded at 0x3fb8e
{
    return 12;
}

// FUNCTION: LLE 0x1003ec8b
// Exports a client reconnection request (current tick, player number) into a new buffer
void NEMClientReconnectionRequest::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mCurrentCT, sizeof(this->mCurrentCT));
    theLocation += sizeof(this->mCurrentCT);
    memcpy(*outPayloadPointer + theLocation, &this->mPlayerNumber, sizeof(this->mPlayerNumber));
    theLocation += sizeof(this->mPlayerNumber);
    ESVERIFY_AT(theLocation == theMessageSize, 662);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003ed09
// Imports a client reconnection request (current tick, player number)
void NEMClientReconnectionRequest::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 672);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mCurrentCT, inPayloadPointer + theLocation, sizeof(this->mCurrentCT));
    theLocation += sizeof(this->mCurrentCT);
    memcpy(&this->mPlayerNumber, inPayloadPointer + theLocation, sizeof(this->mPlayerNumber));
}

// FUNCTION: LLE 0x1003ed5e
// Returns the exported size of a reconnection response: header, error, new send-to slot and tick (13 bytes)
unsigned long NEMClientReconnectionRequestResponse::ExportSize()
{
    return 13;
}

// FUNCTION: LLE 0x1003ed62
// Exports a reconnection response (error, new send-to slot, current tick) into a new buffer
void NEMClientReconnectionRequestResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mError, sizeof(this->mError));
    theLocation += sizeof(this->mError);
    memcpy(*outPayloadPointer + theLocation, &this->mNewSendToSlot, sizeof(this->mNewSendToSlot));
    theLocation += sizeof(this->mNewSendToSlot);
    memcpy(*outPayloadPointer + theLocation, &this->mCurrentCT, sizeof(this->mCurrentCT));
    theLocation += sizeof(this->mCurrentCT);
    ESVERIFY_AT(theLocation == theMessageSize, 732);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003edf1
// Imports a reconnection response (error, new send-to slot, current tick)
void NEMClientReconnectionRequestResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 742);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mError, inPayloadPointer + theLocation, sizeof(this->mError));
    theLocation += sizeof(this->mError);
    memcpy(&this->mNewSendToSlot, inPayloadPointer + theLocation, sizeof(this->mNewSendToSlot));
    theLocation += sizeof(this->mNewSendToSlot);
    memcpy(&this->mCurrentCT, inPayloadPointer + theLocation, sizeof(this->mCurrentCT));
}

unsigned long NEMClientRequestPause::ExportSize()   // header only (4 bytes); ICF-folded at 0x3fb10
{
    return 4;
}

// FUNCTION: LLE 0x1003ee55
// Exports a client pause request (header only) into a new buffer
void NEMClientRequestPause::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    ESVERIFY_AT(theLocation == theMessageSize, 792);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003ee9f
// Imports a client pause request (header only)
void NEMClientRequestPause::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 802);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
}

// FUNCTION: LLE 0x1003eecf
// Destroys the player-input list, releasing every input message it holds
NEMPlayerInput::~NEMPlayerInput()
{
    while (this->mPlayerInputs.size() > 0) {
        this->mPlayerInputs.back()->Dereference();
        this->mPlayerInputs.pop_back();
    }
}

// FUNCTION: LLE 0x1003ef11
// Adds a player input message; kErrorBadPath once the list holds 255
long NEMPlayerInput::AddPlayerInput(NEMessage* inInput)
{
    if (this->mPlayerInputs.size() < 255) {
        this->mPlayerInputs.push_back(inInput);
        return kErrorOK;
    }
    return kErrorBadPath;
}

// FUNCTION: LLE 0x1003ef3f
// Returns the player input message at an index; kErrorBeyondEnd past the end
long NEMPlayerInput::GetPlayerInput(unsigned long inIndex, NEMessage*& outInput)
{
    if (inIndex < this->mPlayerInputs.size()) {
        outInput = this->mPlayerInputs[inIndex];
        return kErrorOK;
    }
    return kErrorBeyondEnd;
}

// FUNCTION: LLE 0x1003ef6c
// Returns the exported size of the player inputs: a count byte plus each input's size
unsigned long NEMPlayerInput::ExportSize()
{
    unsigned char theCount = (unsigned char)this->mPlayerInputs.size();
    unsigned long theSize = 1;
    unsigned long i;
    for (i = 0; i < theCount; i++)
        theSize += this->mPlayerInputs[i]->ExportSize();
    return theSize;
}

// FUNCTION: LLE 0x1003efab
// Writes the input count and each exported input into the caller's buffer (no header, no allocation)
void NEMPlayerInput::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = NEMPlayerInput::ExportSize();
    unsigned char theCount = (unsigned char)this->mPlayerInputs.size();
    unsigned long theLocation;
    unsigned long i;
    unsigned char* theData;
    unsigned long theSize;
    ESVERIFY_AT(this->mPlayerInputs.size() <= 255, 903);
    if (*outPayloadPointer == NULL)
        return;
    theLocation = 1;

    memcpy(*outPayloadPointer, &theCount, sizeof(theCount));
    for (i = 0; i < theCount; i++) {
        this->mPlayerInputs[i]->ExportMessage(&theData, theSize);
        memcpy(*outPayloadPointer + theLocation, theData, theSize);
        theLocation += theSize;
        delete theData;
    }
    ESVERIFY_AT(theLocation == theMessageSize, 922);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003f08b
// Reads the input count and rebuilds each input message; an unknown input type is skipped without advancing
void NEMPlayerInput::ImportMessage(unsigned char* inPayloadPointer)
{
    unsigned char theCount;
    unsigned long theLocation;
    NEMessage* theMessage;
    ESVERIFY_AT(inPayloadPointer != NULL, 934);
    theLocation = 1;
    memcpy(&theCount, inPayloadPointer, sizeof(theCount));
    for (; theCount > 0; theCount--) {
        theMessage = NEMessage::ConvertMemoryToMessage(inPayloadPointer + theLocation);
        if (theMessage != NULL) {
            theLocation += theMessage->ExportSize();
            this->mPlayerInputs.push_back(theMessage);
        }
    }
}

// FUNCTION: LLE 0x1003f0fb
// Destroys a communications tick (its player inputs are released by the base)
NEMCommunicationsTick::~NEMCommunicationsTick()
{
}

// FUNCTION: LLE 0x1003f106
// Returns the exported size of a communications tick: header, tick fields and player inputs
unsigned long NEMCommunicationsTick::ExportSize()
{
    return NEMPlayerInput::ExportSize() + 12;
}

// FUNCTION: LLE 0x1003f10f
// Exports a communications tick (tick number, ticks to perform, rate, checksum/dump flags, inputs) into a new buffer
void NEMCommunicationsTick::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    unsigned char* thePlayerInputs;
    unsigned long thePlayerInputsSize;
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mCurrentCommunicationsTick, sizeof(this->mCurrentCommunicationsTick));
    theLocation += sizeof(this->mCurrentCommunicationsTick);
    memcpy(*outPayloadPointer + theLocation, &this->mPhysicsTicksToPerform, sizeof(this->mPhysicsTicksToPerform));
    theLocation += sizeof(this->mPhysicsTicksToPerform);
    memcpy(*outPayloadPointer + theLocation, &this->mPhysicsTicksPerSecond, sizeof(this->mPhysicsTicksPerSecond));
    theLocation += sizeof(this->mPhysicsTicksPerSecond);
    memcpy(*outPayloadPointer + theLocation, &this->mGenerateChecksum, sizeof(this->mGenerateChecksum));
    theLocation += sizeof(this->mGenerateChecksum);
    memcpy(*outPayloadPointer + theLocation, &this->mDumpTheWorld, sizeof(this->mDumpTheWorld));
    theLocation += sizeof(this->mDumpTheWorld);
    thePlayerInputs = *outPayloadPointer + theLocation;
    NEMPlayerInput::ExportMessage(&thePlayerInputs, thePlayerInputsSize);
    theLocation += thePlayerInputsSize;
    ESVERIFY_AT(theLocation == theMessageSize, 1025);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003f1df
// Imports a communications tick (tick number, ticks to perform, rate, checksum/dump flags, inputs)
void NEMCommunicationsTick::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1035);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mCurrentCommunicationsTick, inPayloadPointer + theLocation, sizeof(this->mCurrentCommunicationsTick));
    theLocation += sizeof(this->mCurrentCommunicationsTick);
    memcpy(&this->mPhysicsTicksToPerform, inPayloadPointer + theLocation, sizeof(this->mPhysicsTicksToPerform));
    theLocation += sizeof(this->mPhysicsTicksToPerform);
    memcpy(&this->mPhysicsTicksPerSecond, inPayloadPointer + theLocation, sizeof(this->mPhysicsTicksPerSecond));
    theLocation += sizeof(this->mPhysicsTicksPerSecond);
    memcpy(&this->mGenerateChecksum, inPayloadPointer + theLocation, sizeof(this->mGenerateChecksum));
    theLocation += sizeof(this->mGenerateChecksum);
    memcpy(&this->mDumpTheWorld, inPayloadPointer + theLocation, sizeof(this->mDumpTheWorld));
    theLocation += sizeof(this->mDumpTheWorld);
    NEMPlayerInput::ImportMessage(inPayloadPointer + theLocation);
}

// FUNCTION: LLE 0x1003f270
// Returns whether a message is the tick response for this communications tick
bool NEMCommunicationsTick::IsResponse(NEMessage* inMessage)
{
    bool theResult = false;
    if (inMessage->GetType() == kNetMessageCommunicationsTickResponse &&
        this->mCurrentCommunicationsTick == static_cast<NEMCommunicationsTickResponse*>(inMessage)->GetServersCommunicationTick())
        theResult = true;
    return theResult;
}

// FUNCTION: LLE 0x1003f289
// Returns the exported size of a tick response: header, tick numbers, physics time and player inputs
unsigned long NEMCommunicationsTickResponse::ExportSize()
{
    return NEMPlayerInput::ExportSize() + 14;
}

// FUNCTION: LLE 0x1003f292
// Exports a tick response (last tick processed, server tick, average physics time, inputs) into a new buffer
void NEMCommunicationsTickResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    unsigned char* thePlayerInputs;
    unsigned long thePlayerInputsSize;
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mLastCommunicationsTickProcessed,
           sizeof(this->mLastCommunicationsTickProcessed));
    theLocation += sizeof(this->mLastCommunicationsTickProcessed);
    memcpy(*outPayloadPointer + theLocation, &this->mServersCommunicationTick, sizeof(this->mServersCommunicationTick));
    theLocation += sizeof(this->mServersCommunicationTick);
    memcpy(*outPayloadPointer + theLocation, &this->mAveragePhysicsTickTime, sizeof(this->mAveragePhysicsTickTime));
    theLocation += sizeof(this->mAveragePhysicsTickTime);
    thePlayerInputs = *outPayloadPointer + theLocation;
    NEMPlayerInput::ExportMessage(&thePlayerInputs, thePlayerInputsSize);
    theLocation += thePlayerInputsSize;
    ESVERIFY_AT(theLocation == theMessageSize, 1121);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003f343
// Imports a tick response (last tick processed, server tick, average physics time, inputs)
void NEMCommunicationsTickResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1131);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mLastCommunicationsTickProcessed, inPayloadPointer + theLocation,
           sizeof(this->mLastCommunicationsTickProcessed));
    theLocation += sizeof(this->mLastCommunicationsTickProcessed);
    memcpy(&this->mServersCommunicationTick, inPayloadPointer + theLocation, sizeof(this->mServersCommunicationTick));
    theLocation += sizeof(this->mServersCommunicationTick);
    memcpy(&this->mAveragePhysicsTickTime, inPayloadPointer + theLocation, sizeof(this->mAveragePhysicsTickTime));
    theLocation += sizeof(this->mAveragePhysicsTickTime);
    NEMPlayerInput::ImportMessage(inPayloadPointer + theLocation);
}

unsigned long NEMPauseGame::ExportSize()   // ICF-folded at 0x3fb8e
{
    return 12;
}

// FUNCTION: LLE 0x1003f3b7
// Exports a pause-game message (unique ID, wait message ID) into a new buffer
void NEMPauseGame::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mUniqueID, sizeof(this->mUniqueID));
    theLocation += sizeof(this->mUniqueID);
    memcpy(*outPayloadPointer + theLocation, &this->mSendToAllAndWaitMessageID, sizeof(this->mSendToAllAndWaitMessageID));
    theLocation += sizeof(this->mSendToAllAndWaitMessageID);
    ESVERIFY_AT(theLocation == theMessageSize, 1191);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003f435
// Imports a pause-game message (unique ID, wait message ID)
void NEMPauseGame::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1201);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mUniqueID, inPayloadPointer + theLocation, sizeof(this->mUniqueID));
    theLocation += sizeof(this->mUniqueID);
    memcpy(&this->mSendToAllAndWaitMessageID, inPayloadPointer + theLocation, sizeof(this->mSendToAllAndWaitMessageID));
}

// FUNCTION: LLE 0x1003f485
// Returns whether a message is the pause response with this pause's unique ID
bool NEMPauseGame::IsResponse(NEMessage* inMessage)
{
    bool theResult = false;
    if (inMessage->GetType() == kNetMessagePauseGameResponse && this->mUniqueID == static_cast<NEMPauseGameResponse*>(inMessage)->GetUniqueID())
        theResult = true;
    return theResult;
}

unsigned long NEMPauseGameResponse::ExportSize()   // ICF-folded at 0x3fc79
{
    return 8;
}

// FUNCTION: LLE 0x1003f49e
// Exports a pause-game response (unique ID) into a new buffer
void NEMPauseGameResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mUniqueID, sizeof(this->mUniqueID));
    theLocation += sizeof(this->mUniqueID);
    ESVERIFY_AT(theLocation == theMessageSize, 1260);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003f509
// Imports a pause-game response (unique ID)
void NEMPauseGameResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1270);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mUniqueID, inPayloadPointer + theLocation, sizeof(this->mUniqueID));
}

unsigned long NEMPhysicsTick::ExportSize()   // ICF-folded at 0x3fb8e
{
    return 11;
}

// FUNCTION: LLE 0x1003f54a
// Exports a physics tick (input count, ticks to perform, checksum and dump flags) into a new buffer
void NEMPhysicsTick::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mNumberOfPlayerInputs, sizeof(this->mNumberOfPlayerInputs));
    theLocation += sizeof(this->mNumberOfPlayerInputs);
    memcpy(*outPayloadPointer + theLocation, &this->mPhysicsTicksToPerform, sizeof(this->mPhysicsTicksToPerform));
    theLocation += sizeof(this->mPhysicsTicksToPerform);
    memcpy(*outPayloadPointer + theLocation, &this->mGenerateChecksum, sizeof(this->mGenerateChecksum));
    theLocation += sizeof(this->mGenerateChecksum);
    memcpy(*outPayloadPointer + theLocation, &this->mDumpTheWorld, sizeof(this->mDumpTheWorld));
    theLocation += sizeof(this->mDumpTheWorld);
    ESVERIFY_AT(theLocation == theMessageSize, 1331);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003f5e8
// Imports a physics tick (input count, ticks to perform, checksum and dump flags)
void NEMPhysicsTick::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1341);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mNumberOfPlayerInputs, inPayloadPointer + theLocation, sizeof(this->mNumberOfPlayerInputs));
    theLocation += sizeof(this->mNumberOfPlayerInputs);
    memcpy(&this->mPhysicsTicksToPerform, inPayloadPointer + theLocation, sizeof(this->mPhysicsTicksToPerform));
    theLocation += sizeof(this->mPhysicsTicksToPerform);
    memcpy(&this->mGenerateChecksum, inPayloadPointer + theLocation, sizeof(this->mGenerateChecksum));
    theLocation += sizeof(this->mGenerateChecksum);
    memcpy(&this->mDumpTheWorld, inPayloadPointer + theLocation, sizeof(this->mDumpTheWorld));
}

unsigned long NEMPlayerConnectionCheck::ExportSize()   // ICF-folded at 0x3fc79
{
    return 8;
}

// FUNCTION: LLE 0x1003f65d
// Exports a connection check (ping start time) into a new buffer
void NEMPlayerConnectionCheck::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mPingTimeStart, sizeof(this->mPingTimeStart));
    theLocation += sizeof(this->mPingTimeStart);
    ESVERIFY_AT(theLocation == theMessageSize, 1398);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003f6c8
// Imports a connection check (ping start time)
void NEMPlayerConnectionCheck::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1408);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mPingTimeStart, inPayloadPointer + theLocation, sizeof(this->mPingTimeStart));
}

unsigned long NEMPlayerConnectionCheckResponse::ExportSize()   // ICF-folded at 0x3fc79
{
    return 8;
}

// FUNCTION: LLE 0x1003f709
// Exports a connection check response (ping start time) into a new buffer
void NEMPlayerConnectionCheckResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mPingTimeStart, sizeof(this->mPingTimeStart));
    theLocation += sizeof(this->mPingTimeStart);
    ESVERIFY_AT(theLocation == theMessageSize, 1456);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003f774
// Imports a connection check response (ping start time)
void NEMPlayerConnectionCheckResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1466);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mPingTimeStart, inPayloadPointer + theLocation, sizeof(this->mPingTimeStart));
}

unsigned long NEMRemovePlayer::ExportSize()   // ICF-folded at 0x3fc79
{
    return 8;
}

// FUNCTION: LLE 0x1003f7b5
// Exports a remove-player message (player index, out-of-sync, game exiting and booted flags) into a new buffer
void NEMRemovePlayer::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mPlayerIndex, sizeof(this->mPlayerIndex));
    theLocation += sizeof(this->mPlayerIndex);
    memcpy(*outPayloadPointer + theLocation, &this->mPlayerIsOOS, sizeof(this->mPlayerIsOOS));
    theLocation += sizeof(this->mPlayerIsOOS);
    memcpy(*outPayloadPointer + theLocation, &this->mGameIsExiting, sizeof(this->mGameIsExiting));
    theLocation += sizeof(this->mGameIsExiting);
    memcpy(*outPayloadPointer + theLocation, &this->mWasBooted, sizeof(this->mWasBooted));
    theLocation += sizeof(this->mWasBooted);
    ESVERIFY_AT(theLocation == theMessageSize, 1526);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003f851
// Imports a remove-player message (player index, out-of-sync, game exiting and booted flags)
void NEMRemovePlayer::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1536);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mPlayerIndex, inPayloadPointer + theLocation, sizeof(this->mPlayerIndex));
    theLocation += sizeof(this->mPlayerIndex);
    memcpy(&this->mPlayerIsOOS, inPayloadPointer + theLocation, sizeof(this->mPlayerIsOOS));
    theLocation += sizeof(this->mPlayerIsOOS);
    memcpy(&this->mGameIsExiting, inPayloadPointer + theLocation, sizeof(this->mGameIsExiting));
    theLocation += sizeof(this->mGameIsExiting);
    memcpy(&this->mWasBooted, inPayloadPointer + theLocation, sizeof(this->mWasBooted));
}

unsigned long NEMServerIsAlive::ExportSize()   // ICF-folded at 0x3fb10
{
    return 4;
}

// FUNCTION: LLE 0x1003f8c4
// Exports a server-is-alive message (header only) into a new buffer
void NEMServerIsAlive::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    ESVERIFY_AT(theLocation == theMessageSize, 1590);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003f90e
// Imports a server-is-alive message (header only)
void NEMServerIsAlive::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1600);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
}

unsigned long NEMStartGame::ExportSize()   // ICF-folded at 0x3fc79
{
    return 8;
}

// FUNCTION: LLE 0x1003f93e
// Exports a start-game message (start time offset) into a new buffer
void NEMStartGame::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mOffsetStartTime, sizeof(this->mOffsetStartTime));
    theLocation += sizeof(this->mOffsetStartTime);
    ESVERIFY_AT(theLocation == theMessageSize, 1645);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003f9a9
// Imports a start-game message (start time offset)
void NEMStartGame::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1655);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mOffsetStartTime, inPayloadPointer + theLocation, sizeof(this->mOffsetStartTime));
}

unsigned long NEMStartGameResponse::ExportSize()   // ICF-folded at 0x3fc79
{
    return 8;
}

// FUNCTION: LLE 0x1003f9ea
// Exports a start-game response (application time at start) into a new buffer
void NEMStartGameResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mApplicationTimeAtStart, sizeof(this->mApplicationTimeAtStart));
    theLocation += sizeof(this->mApplicationTimeAtStart);
    ESVERIFY_AT(theLocation == theMessageSize, 1703);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003fa55
// Imports a start-game response (application time at start)
void NEMStartGameResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1713);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mApplicationTimeAtStart, inPayloadPointer + theLocation, sizeof(this->mApplicationTimeAtStart));
}

unsigned long NEMStopGame::ExportSize()   // ICF-folded at 0x3fb10
{
    return 4;
}

// FUNCTION: LLE 0x1003fa96
// Exports a stop-game message (header only) into a new buffer
void NEMStopGame::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    ESVERIFY_AT(theLocation == theMessageSize, 1758);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003fae0
// Imports a stop-game message (header only)
void NEMStopGame::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1768);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
}

// FUNCTION: LLE 0x1003fb10
// Returns the exported size of a stop-game response: the 4-byte header (shared by every header-only message)
unsigned long NEMStopGameResponse::ExportSize()
{
    return 4;
}

// FUNCTION: LLE 0x1003fb14
// Exports a stop-game response (header only) into a new buffer
void NEMStopGameResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    ESVERIFY_AT(theLocation == theMessageSize, 1809);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003fb5e
// Imports a stop-game response (header only)
void NEMStopGameResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1819);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
}

// FUNCTION: LLE 0x1003fb8e
// Returns the exported size of an unpause message: header and two longs (12 bytes; shared by other messages)
unsigned long NEMUnPauseGame::ExportSize()
{
    return 12;
}

// FUNCTION: LLE 0x1003fb92
// Exports an unpause message (unique ID, wait message ID) into a new buffer
void NEMUnPauseGame::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mUniqueID, sizeof(this->mUniqueID));
    theLocation += sizeof(this->mUniqueID);
    memcpy(*outPayloadPointer + theLocation, &this->mSendToAllAndWaitMessageID, sizeof(this->mSendToAllAndWaitMessageID));
    theLocation += sizeof(this->mSendToAllAndWaitMessageID);
    ESVERIFY_AT(theLocation == theMessageSize, 1868);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003fc10
// Imports an unpause message (unique ID, wait message ID)
void NEMUnPauseGame::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1878);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mUniqueID, inPayloadPointer + theLocation, sizeof(this->mUniqueID));
    theLocation += sizeof(this->mUniqueID);
    memcpy(&this->mSendToAllAndWaitMessageID, inPayloadPointer + theLocation, sizeof(this->mSendToAllAndWaitMessageID));
}

// FUNCTION: LLE 0x1003fc60
// Returns whether a message is the unpause response with this unpause's unique ID
bool NEMUnPauseGame::IsResponse(NEMessage* inMessage)
{
    bool theResult = false;
    if (inMessage->GetType() == kNetMessageUnPauseGameResponse && this->mUniqueID == static_cast<NEMUnPauseGameResponse*>(inMessage)->GetUniqueID())
        theResult = true;
    return theResult;
}

// FUNCTION: LLE 0x1003fc79
// Returns the exported size of an unpause response: header and one long (8 bytes; shared by other messages)
unsigned long NEMUnPauseGameResponse::ExportSize()
{
    return 8;
}

// FUNCTION: LLE 0x1003fc7d
// Exports an unpause response (unique ID) into a new buffer
void NEMUnPauseGameResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mUniqueID, sizeof(this->mUniqueID));
    theLocation += sizeof(this->mUniqueID);
    ESVERIFY_AT(theLocation == theMessageSize, 1937);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003fce8
// Imports an unpause response (unique ID)
void NEMUnPauseGameResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1947);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mUniqueID, inPayloadPointer + theLocation, sizeof(this->mUniqueID));
}

// FUNCTION: LLE 0x1003fd29
// Returns the exported size of a world checksum request: header, trigger tick and dump flag (9 bytes)
unsigned long NEMWorldDatabaseChecksum::ExportSize()
{
    return 9;
}

// FUNCTION: LLE 0x1003fd2d
// Exports a world checksum request (physics tick trigger, dump flag) into a new buffer
void NEMWorldDatabaseChecksum::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mPhysicsTickTrigger, sizeof(this->mPhysicsTickTrigger));
    theLocation += sizeof(this->mPhysicsTickTrigger);
    memcpy(*outPayloadPointer + theLocation, &this->mDumpWorld, sizeof(this->mDumpWorld));
    theLocation += sizeof(this->mDumpWorld);
    ESVERIFY_AT(theLocation == theMessageSize, 1999);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003fda9
// Imports a world checksum request (physics tick trigger, dump flag)
void NEMWorldDatabaseChecksum::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 2009);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mPhysicsTickTrigger, inPayloadPointer + theLocation, sizeof(this->mPhysicsTickTrigger));
    theLocation += sizeof(this->mPhysicsTickTrigger);
    memcpy(&this->mDumpWorld, inPayloadPointer + theLocation, sizeof(this->mDumpWorld));
}

unsigned long NEMWorldDatabaseChecksumResponse::ExportSize()   // ICF-folded at 0x3fb8e
{
    return 12;
}

// FUNCTION: LLE 0x1003fdf9
// Exports a world checksum response (communications tick, checksum) into a new buffer
void NEMWorldDatabaseChecksumResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mCommunicationsTick, sizeof(this->mCommunicationsTick));
    theLocation += sizeof(this->mCommunicationsTick);
    memcpy(*outPayloadPointer + theLocation, &this->mWorldDatabaseChecksum, sizeof(this->mWorldDatabaseChecksum));
    theLocation += sizeof(this->mWorldDatabaseChecksum);
    ESVERIFY_AT(theLocation == theMessageSize, 2064);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003fe77
// Imports a world checksum response (communications tick, checksum)
void NEMWorldDatabaseChecksumResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 2074);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mCommunicationsTick, inPayloadPointer + theLocation, sizeof(this->mCommunicationsTick));
    theLocation += sizeof(this->mCommunicationsTick);
    memcpy(&this->mWorldDatabaseChecksum, inPayloadPointer + theLocation, sizeof(this->mWorldDatabaseChecksum));
}

// FUNCTION: LLE 0x1003fec7
// Returns the exported size of any file transfer message: header, ID, data size and the full 1 KB data block
unsigned long NEMXferMessage::StaticExportSize()
{
    return 4 + sizeof(unsigned char) + sizeof(uint32_t) + 0x400;   // the data size is 4 bytes on the wire
}

unsigned long NEMXferMessage::ExportSize()
{
    return 4 + sizeof(this->mID) + sizeof(this->mDataSize) + sizeof(this->mData);
}

// FUNCTION: LLE 0x1003fecd
// Exports a file transfer block (ID, data size, the whole 1 KB data) into a new buffer
void NEMXferMessage::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mID, sizeof(this->mID));
    theLocation += sizeof(this->mID);
    memcpy(*outPayloadPointer + theLocation, &this->mDataSize, sizeof(this->mDataSize));
    theLocation += sizeof(this->mDataSize);
    memcpy(*outPayloadPointer + theLocation, this->mData, sizeof(this->mData));
    theLocation += sizeof(this->mData);
    ESVERIFY_AT(theLocation == theMessageSize, 2143);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x1003ff68
// Imports a file transfer block (ID, data size, 1 KB data)
void NEMXferMessage::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 2153);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mID, inPayloadPointer + theLocation, sizeof(this->mID));
    theLocation += sizeof(this->mID);
    memcpy(&this->mDataSize, inPayloadPointer + theLocation, sizeof(this->mDataSize));
    theLocation += sizeof(this->mDataSize);
    memset(this->mData, 0, sizeof(this->mData));
    memcpy(this->mData, inPayloadPointer + theLocation, sizeof(this->mData));
}

// FUNCTION: LLE 0x1003ffe0
// Returns the exported size of a transfer status: header, client slot, percent, transfer ID and status (11 bytes)
unsigned long NEMXferStatus::ExportSize()
{
    return 11;
}

// FUNCTION: LLE 0x1003ffe4
// Exports a transfer status (client slot, percent done, transfer ID, status) into a new buffer
void NEMXferStatus::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    unsigned long theMessageSize = this->ExportSize();
    *outPayloadPointer = new unsigned char[theMessageSize];
    if (*outPayloadPointer == NULL)
        return;
    unsigned long theLocation = this->ExportHeader(*outPayloadPointer);
    memcpy(*outPayloadPointer + theLocation, &this->mClientSlot, sizeof(this->mClientSlot));
    theLocation += sizeof(this->mClientSlot);
    memcpy(*outPayloadPointer + theLocation, &this->mPercentDone, sizeof(this->mPercentDone));
    theLocation += sizeof(this->mPercentDone);
    memcpy(*outPayloadPointer + theLocation, &this->mXferID, sizeof(this->mXferID));
    theLocation += sizeof(this->mXferID);
    memcpy(*outPayloadPointer + theLocation, &this->mStatusID, sizeof(this->mStatusID));
    theLocation += sizeof(this->mStatusID);
    ESVERIFY_AT(theLocation == theMessageSize, 2221);
    outMessageSize = theMessageSize;
}

// FUNCTION: LLE 0x10040082
// Imports a transfer status (client slot, percent done, transfer ID, status)
void NEMXferStatus::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 2231);
    unsigned long theLocation = this->ImportHeader(inPayloadPointer);
    memcpy(&this->mClientSlot, inPayloadPointer + theLocation, sizeof(this->mClientSlot));
    theLocation += sizeof(this->mClientSlot);
    memcpy(&this->mPercentDone, inPayloadPointer + theLocation, sizeof(this->mPercentDone));
    theLocation += sizeof(this->mPercentDone);
    memcpy(&this->mXferID, inPayloadPointer + theLocation, sizeof(this->mXferID));
    theLocation += sizeof(this->mXferID);
    memcpy(&this->mStatusID, inPayloadPointer + theLocation, sizeof(this->mStatusID));
}

// The trivial IsResponse overrides (no message answers these) fold into one body (0x3ed59; at the end, after the
// asserts)
bool NEMAddPlayer::IsResponse(NEMessage* inMessage) { return false; }
bool NEMAddPlayerResponse::IsResponse(NEMessage* inMessage) { return false; }
bool NEMClientConnectionRequest::IsResponse(NEMessage* inMessage) { return false; }
bool NEMClientConnectionRequestResponse::IsResponse(NEMessage* inMessage) { return false; }
bool NEMClientDisconnectionNotification::IsResponse(NEMessage* inMessage) { return false; }
bool NEMClientDisconnectionNotificationResponse::IsResponse(NEMessage* inMessage) { return false; }
bool NEMClientReconnectionRequest::IsResponse(NEMessage* inMessage) { return false; }
bool NEMClientReconnectionRequestResponse::IsResponse(NEMessage* inMessage) { return false; }
bool NEMClientRequestPause::IsResponse(NEMessage* inMessage) { return false; }
bool NEMPlayerInput::IsResponse(NEMessage* inMessage) { return false; }
bool NEMCommunicationsTickResponse::IsResponse(NEMessage* inMessage) { return false; }
bool NEMPauseGameResponse::IsResponse(NEMessage* inMessage) { return false; }
bool NEMPhysicsTick::IsResponse(NEMessage* inMessage) { return false; }
bool NEMPlayerConnectionCheck::IsResponse(NEMessage* inMessage) { return false; }
bool NEMPlayerConnectionCheckResponse::IsResponse(NEMessage* inMessage) { return false; }
bool NEMRemovePlayer::IsResponse(NEMessage* inMessage) { return false; }
bool NEMServerIsAlive::IsResponse(NEMessage* inMessage) { return false; }
bool NEMStartGame::IsResponse(NEMessage* inMessage) { return false; }
bool NEMStartGameResponse::IsResponse(NEMessage* inMessage) { return false; }
bool NEMStopGame::IsResponse(NEMessage* inMessage) { return false; }
bool NEMStopGameResponse::IsResponse(NEMessage* inMessage) { return false; }
bool NEMUnPauseGameResponse::IsResponse(NEMessage* inMessage) { return false; }
bool NEMWorldDatabaseChecksum::IsResponse(NEMessage* inMessage) { return false; }
bool NEMWorldDatabaseChecksumResponse::IsResponse(NEMessage* inMessage) { return false; }
bool NEMXferMessage::IsResponse(NEMessage* inMessage) { return false; }
bool NEMXferStatus::IsResponse(NEMessage* inMessage) { return false; }

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x1003ed59
// SYNTHETIC: LLE 0x100400f7
