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
#include "rust/ee_netmsg.h"   // ee_netmsg: the engine messages' wire layout

NEMessage* (*NEMessage::sConstructors[256])(unsigned char*);

// ---- the codec (ee_netmsg engine.rs, docs/RUST_PLAN.md R5b.4): each message hands its members to the Rust as slots
// in wire order, the header's four first; the layout (sizes, order, the strings' and inputs' forms) is the Rust
// table's ----
static const size_t kNetMsgMaxSlots = 12;

template <class T> static inline EeNetMsgSlot NetMsgField(T& inMember)
{
    EeNetMsgSlot theSlot = {(uint8_t*)&inMember, (uint32_t)sizeof(T), 0, 0};
    return theSlot;
}

// a string's bytes (UWideString::Export's strlen); on import the Rust reports where the string is
static inline EeNetMsgSlot NetMsgString(const UWideString& inString)
{
    const char* theBytes = inString;
    EeNetMsgSlot theSlot = {(uint8_t*)theBytes, (uint32_t)strlen(theBytes), 0, 0};
    return theSlot;
}

// the players' inputs: one slot per input of the list with its ExportSize, or (outBuffers given) its exported bytes,
// which the caller frees with NetMsgFree; the Rust takes the first (unsigned char)size() of them
static EeNetMsgSlot NetMsgInputs(std::vector<NEMessage*>& inInputs, std::vector<EeNetMsgSlot>& outParts,
                                 std::vector<unsigned char*>* outBuffers)
{
    outParts.clear();
    for (size_t i = 0; i < inInputs.size(); i++) {
        EeNetMsgSlot thePart = {NULL, 0, 0, 0};
        if (outBuffers != NULL) {
            unsigned char* theData = NULL;
            unsigned long theSize = 0;
            inInputs[i]->ExportMessage(&theData, theSize);
            outBuffers->push_back(theData);
            thePart.data = theData;
            thePart.size = (uint32_t)theSize;
        } else {
            thePart.size = (uint32_t)inInputs[i]->ExportSize();
        }
        outParts.push_back(thePart);
    }
    EeNetMsgSlot theSlot = {outParts.empty() ? NULL : (uint8_t*)&outParts[0], 0, (uint32_t)outParts.size(), 0};
    return theSlot;
}

static void NetMsgFree(std::vector<unsigned char*>& ioBuffers)
{
    for (size_t i = 0; i < ioBuffers.size(); i++)
        delete[] ioBuffers[i];
    ioBuffers.clear();
}

// one nested input built from its bytes (ConvertMemoryToMessage) into the list: its ExportSize, 0 for a type nobody
// builds (the walk then stays where it is, as the original's)
static uint32_t NetMsgBuildInput(void* inList, const uint8_t* inData)
{
    NEMessage* theMessage = NEMessage::ConvertMemoryToMessage((unsigned char*)inData);
    if (theMessage == NULL)
        return 0;
    static_cast<NEMPlayerInput*>(inList)->mPlayerInputs.push_back(theMessage);
    return (uint32_t)theMessage->ExportSize();
}

// a new buffer of the message's size, filled by the Rust (the original's ExportMessage shape)
static void NetMsgExport(uint32_t inClass, EeNetMsgSlot* inSlots, size_t inCount, unsigned char** outPayloadPointer,
                         unsigned long& outMessageSize)
{
    unsigned long theMessageSize = ee_netmsg_export_size(inClass, inSlots, inCount);
    *outPayloadPointer = new unsigned char[theMessageSize];
    outMessageSize = ee_netmsg_export(inClass, inSlots, inCount, *outPayloadPointer, theMessageSize);
}

// New (port): the header's members (reserved, type, target slot, originating slot) as the codec's first four slots
void NEMessage::GetHeaderSlots(EeNetMsgSlot* outSlots)
{
    outSlots[0] = NetMsgField(this->mReserved);
    outSlots[1] = NetMsgField(this->mType);
    outSlots[2] = NetMsgField(this->mForSpecificSlot);
    outSlots[3] = NetMsgField(this->mOriginatingSlot);
}

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

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMAddPlayer* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mPlayerIndex);
    outSlots[5] = NetMsgString(inMessage->mPlayerName);
    return 6;
}

// FUNCTION: LLE 0x1003e751
// Returns the exported size of an add-player message: header, player index and name
unsigned long NEMAddPlayer::ExportSize()
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    return ee_netmsg_export_size(EE_NETMSG_NE_ADD_PLAYER, theSlots, Slots(this, theSlots));
}

// FUNCTION: LLE 0x1003e75d
// Exports an add-player message (player index and name) into a new buffer
void NEMAddPlayer::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_ADD_PLAYER, theSlots, Slots(this, theSlots), outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003e7de
// Imports an add-player message (player index and name)
void NEMAddPlayer::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 277);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_ADD_PLAYER, inPayloadPointer, theSlots, Slots(this, theSlots));
    this->mPlayerName.Import(inPayloadPointer + theSlots[5].offset);
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMAddPlayerResponse* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    return 4;
}

unsigned long NEMAddPlayerResponse::ExportSize()   // header only (4 bytes); ICF-folded at 0x3fb10
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_ADD_PLAYER_RESPONSE);
}

// FUNCTION: LLE 0x1003e829
// Exports an add-player response (header only) into a new buffer
void NEMAddPlayerResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_ADD_PLAYER_RESPONSE, theSlots, Slots(this, theSlots), outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003e873
// Imports an add-player response (header only)
void NEMAddPlayerResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 333);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_ADD_PLAYER_RESPONSE, inPayloadPointer, theSlots, Slots(this, theSlots));
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMClientConnectionRequest* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mIsBehindFirewall);
    outSlots[5] = NetMsgString(inMessage->mVersion);
    outSlots[6] = NetMsgString(inMessage->mPlayerName);
    outSlots[7] = NetMsgField(inMessage->mBinaryChecksum);
    return 8;
}

// FUNCTION: LLE 0x1003e8a3
// Returns the exported size of a client connection request: firewall flag, version, player name and checksum
unsigned long NEMClientConnectionRequest::ExportSize()
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    return ee_netmsg_export_size(EE_NETMSG_NE_CLIENT_CONNECTION_REQUEST, theSlots, Slots(this, theSlots));
}

// FUNCTION: LLE 0x1003e8c0
// Exports a client connection request (firewall flag, version, player name, binary checksum) into a new buffer
void NEMClientConnectionRequest::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_CLIENT_CONNECTION_REQUEST, theSlots, Slots(this, theSlots),
                 outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003e971
// Imports a client connection request (firewall flag, version, player name, binary checksum)
void NEMClientConnectionRequest::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 400);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_CLIENT_CONNECTION_REQUEST, inPayloadPointer, theSlots, Slots(this, theSlots));
    this->mVersion.Import(inPayloadPointer + theSlots[5].offset);
    this->mPlayerName.Import(inPayloadPointer + theSlots[6].offset);
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMClientConnectionRequestResponse* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mError);
    outSlots[5] = NetMsgField(inMessage->mUISlotNumber);
    outSlots[6] = NetMsgField(inMessage->mSendToSlot);
    outSlots[7] = NetMsgField(inMessage->mCurrentCT);
    outSlots[8] = NetMsgString(inMessage->mVersion);
    return 9;
}

// FUNCTION: LLE 0x1003e9e5
// Returns the exported size of a connection response: error, UI slot, send-to slot, current tick and version
unsigned long NEMClientConnectionRequestResponse::ExportSize()
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    return ee_netmsg_export_size(EE_NETMSG_NE_CLIENT_CONNECTION_REQUEST_RESPONSE, theSlots, Slots(this, theSlots));
}

// FUNCTION: LLE 0x1003e9f1
// Exports a connection response (error, UI slot, send-to slot, current tick, version) into a new buffer
void NEMClientConnectionRequestResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_CLIENT_CONNECTION_REQUEST_RESPONSE, theSlots, Slots(this, theSlots),
                 outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003eaad
// Imports a connection response (error, UI slot, send-to slot, current tick, version)
void NEMClientConnectionRequestResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 482);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_CLIENT_CONNECTION_REQUEST_RESPONSE, inPayloadPointer, theSlots,
                     Slots(this, theSlots));
    this->mVersion.Import(inPayloadPointer + theSlots[8].offset);
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMClientDisconnectionNotification* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mIsExiting);
    return 5;
}

// FUNCTION: LLE 0x1003eb31
// Returns the exported size of a client disconnection notice: header and exiting flag (5 bytes)
unsigned long NEMClientDisconnectionNotification::ExportSize()
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_CLIENT_DISCONNECTION_NOTIFICATION);
}

// FUNCTION: LLE 0x1003eb35
// Exports a client disconnection notice (exiting flag) into a new buffer
void NEMClientDisconnectionNotification::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_CLIENT_DISCONNECTION_NOTIFICATION, theSlots, Slots(this, theSlots),
                 outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003eb9e
// Imports a client disconnection notice (exiting flag)
void NEMClientDisconnectionNotification::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 552);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_CLIENT_DISCONNECTION_NOTIFICATION, inPayloadPointer, theSlots, Slots(this, theSlots));
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMClientDisconnectionNotificationResponse* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mError);
    return 5;
}

unsigned long NEMClientDisconnectionNotificationResponse::ExportSize()   // header and one long (8 bytes); ICF-folded at 0x3fc79
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_CLIENT_DISCONNECTION_NOTIFICATION_RESPONSE);
}

// FUNCTION: LLE 0x1003ebdf
// Exports a disconnection response (error code) into a new buffer
void NEMClientDisconnectionNotificationResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_CLIENT_DISCONNECTION_NOTIFICATION_RESPONSE, theSlots, Slots(this, theSlots),
                 outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003ec4a
// Imports a disconnection response (error code)
void NEMClientDisconnectionNotificationResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 610);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_CLIENT_DISCONNECTION_NOTIFICATION_RESPONSE, inPayloadPointer, theSlots,
                     Slots(this, theSlots));
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMClientReconnectionRequest* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mCurrentCT);
    outSlots[5] = NetMsgField(inMessage->mPlayerNumber);
    return 6;
}

unsigned long NEMClientReconnectionRequest::ExportSize()   // header and two longs (12 bytes); ICF-folded at 0x3fb8e
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_CLIENT_RECONNECTION_REQUEST);
}

// FUNCTION: LLE 0x1003ec8b
// Exports a client reconnection request (current tick, player number) into a new buffer
void NEMClientReconnectionRequest::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_CLIENT_RECONNECTION_REQUEST, theSlots, Slots(this, theSlots),
                 outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003ed09
// Imports a client reconnection request (current tick, player number)
void NEMClientReconnectionRequest::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 672);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_CLIENT_RECONNECTION_REQUEST, inPayloadPointer, theSlots, Slots(this, theSlots));
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMClientReconnectionRequestResponse* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mError);
    outSlots[5] = NetMsgField(inMessage->mNewSendToSlot);
    outSlots[6] = NetMsgField(inMessage->mCurrentCT);
    return 7;
}

// FUNCTION: LLE 0x1003ed5e
// Returns the exported size of a reconnection response: header, error, new send-to slot and tick (13 bytes)
unsigned long NEMClientReconnectionRequestResponse::ExportSize()
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_CLIENT_RECONNECTION_REQUEST_RESPONSE);
}

// FUNCTION: LLE 0x1003ed62
// Exports a reconnection response (error, new send-to slot, current tick) into a new buffer
void NEMClientReconnectionRequestResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_CLIENT_RECONNECTION_REQUEST_RESPONSE, theSlots, Slots(this, theSlots),
                 outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003edf1
// Imports a reconnection response (error, new send-to slot, current tick)
void NEMClientReconnectionRequestResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 742);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_CLIENT_RECONNECTION_REQUEST_RESPONSE, inPayloadPointer, theSlots,
                     Slots(this, theSlots));
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMClientRequestPause* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    return 4;
}

unsigned long NEMClientRequestPause::ExportSize()   // header only (4 bytes); ICF-folded at 0x3fb10
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_CLIENT_REQUEST_PAUSE);
}

// FUNCTION: LLE 0x1003ee55
// Exports a client pause request (header only) into a new buffer
void NEMClientRequestPause::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_CLIENT_REQUEST_PAUSE, theSlots, Slots(this, theSlots), outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003ee9f
// Imports a client pause request (header only)
void NEMClientRequestPause::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 802);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_CLIENT_REQUEST_PAUSE, inPayloadPointer, theSlots, Slots(this, theSlots));
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
    std::vector<EeNetMsgSlot> theParts;
    NetMsgInputs(this->mPlayerInputs, theParts, NULL);
    return ee_netmsg_inputs_size(theParts.empty() ? NULL : &theParts[0], theParts.size());
}

// FUNCTION: LLE 0x1003efab
// Writes the input count and each exported input into the caller's buffer (no header, no allocation)
void NEMPlayerInput::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    ESVERIFY_AT(this->mPlayerInputs.size() <= 255, 903);
    if (*outPayloadPointer == NULL)
        return;
    std::vector<EeNetMsgSlot> theParts;
    std::vector<unsigned char*> theBuffers;
    NetMsgInputs(this->mPlayerInputs, theParts, &theBuffers);
    const EeNetMsgSlot* theInputs = theParts.empty() ? NULL : &theParts[0];
    unsigned long theMessageSize = ee_netmsg_inputs_size(theInputs, theParts.size());
    outMessageSize = ee_netmsg_inputs_export(theInputs, theParts.size(), *outPayloadPointer, theMessageSize);
    NetMsgFree(theBuffers);
}

// FUNCTION: LLE 0x1003f08b
// Reads the input count and rebuilds each input message; an unknown input type is skipped without advancing
void NEMPlayerInput::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 934);
    ee_netmsg_inputs_import(inPayloadPointer, NetMsgBuildInput, this);
}

// FUNCTION: LLE 0x1003f0fb
// Destroys a communications tick (its player inputs are released by the base)
NEMCommunicationsTick::~NEMCommunicationsTick()
{
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMCommunicationsTick* inMessage, EeNetMsgSlot* outSlots, std::vector<EeNetMsgSlot>& outParts,
                    std::vector<unsigned char*>* outBuffers)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mCurrentCommunicationsTick);
    outSlots[5] = NetMsgField(inMessage->mPhysicsTicksToPerform);
    outSlots[6] = NetMsgField(inMessage->mPhysicsTicksPerSecond);
    outSlots[7] = NetMsgField(inMessage->mGenerateChecksum);
    outSlots[8] = NetMsgField(inMessage->mDumpTheWorld);
    outSlots[9] = NetMsgInputs(inMessage->mPlayerInputs, outParts, outBuffers);
    return 10;
}

// FUNCTION: LLE 0x1003f106
// Returns the exported size of a communications tick: header, tick fields and player inputs
unsigned long NEMCommunicationsTick::ExportSize()
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    std::vector<EeNetMsgSlot> theParts;
    return ee_netmsg_export_size(EE_NETMSG_NE_COMMUNICATIONS_TICK, theSlots, Slots(this, theSlots, theParts, NULL));
}

// FUNCTION: LLE 0x1003f10f
// Exports a communications tick (tick number, ticks to perform, rate, checksum/dump flags, inputs) into a new buffer
void NEMCommunicationsTick::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    std::vector<EeNetMsgSlot> theParts;
    std::vector<unsigned char*> theBuffers;
    NetMsgExport(EE_NETMSG_NE_COMMUNICATIONS_TICK, theSlots, Slots(this, theSlots, theParts, &theBuffers),
                 outPayloadPointer, outMessageSize);
    NetMsgFree(theBuffers);
}

// FUNCTION: LLE 0x1003f1df
// Imports a communications tick (tick number, ticks to perform, rate, checksum/dump flags, inputs)
void NEMCommunicationsTick::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1035);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    std::vector<EeNetMsgSlot> theParts;
    ee_netmsg_import(EE_NETMSG_NE_COMMUNICATIONS_TICK, inPayloadPointer, theSlots,
                     Slots(this, theSlots, theParts, NULL));
    NEMPlayerInput::ImportMessage(inPayloadPointer + theSlots[9].offset);
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

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMCommunicationsTickResponse* inMessage, EeNetMsgSlot* outSlots, std::vector<EeNetMsgSlot>& outParts,
                    std::vector<unsigned char*>* outBuffers)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mLastCommunicationsTickProcessed);
    outSlots[5] = NetMsgField(inMessage->mServersCommunicationTick);
    outSlots[6] = NetMsgField(inMessage->mAveragePhysicsTickTime);
    outSlots[7] = NetMsgInputs(inMessage->mPlayerInputs, outParts, outBuffers);
    return 8;
}

// FUNCTION: LLE 0x1003f289
// Returns the exported size of a tick response: header, tick numbers, physics time and player inputs
unsigned long NEMCommunicationsTickResponse::ExportSize()
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    std::vector<EeNetMsgSlot> theParts;
    return ee_netmsg_export_size(EE_NETMSG_NE_COMMUNICATIONS_TICK_RESPONSE, theSlots, Slots(this, theSlots, theParts, NULL));
}

// FUNCTION: LLE 0x1003f292
// Exports a tick response (last tick processed, server tick, average physics time, inputs) into a new buffer
void NEMCommunicationsTickResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    std::vector<EeNetMsgSlot> theParts;
    std::vector<unsigned char*> theBuffers;
    NetMsgExport(EE_NETMSG_NE_COMMUNICATIONS_TICK_RESPONSE, theSlots, Slots(this, theSlots, theParts, &theBuffers),
                 outPayloadPointer, outMessageSize);
    NetMsgFree(theBuffers);
}

// FUNCTION: LLE 0x1003f343
// Imports a tick response (last tick processed, server tick, average physics time, inputs)
void NEMCommunicationsTickResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1131);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    std::vector<EeNetMsgSlot> theParts;
    ee_netmsg_import(EE_NETMSG_NE_COMMUNICATIONS_TICK_RESPONSE, inPayloadPointer, theSlots,
                     Slots(this, theSlots, theParts, NULL));
    NEMPlayerInput::ImportMessage(inPayloadPointer + theSlots[7].offset);
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMPauseGame* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mUniqueID);
    outSlots[5] = NetMsgField(inMessage->mSendToAllAndWaitMessageID);
    return 6;
}

unsigned long NEMPauseGame::ExportSize()   // ICF-folded at 0x3fb8e
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_PAUSE_GAME);
}

// FUNCTION: LLE 0x1003f3b7
// Exports a pause-game message (unique ID, wait message ID) into a new buffer
void NEMPauseGame::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_PAUSE_GAME, theSlots, Slots(this, theSlots), outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003f435
// Imports a pause-game message (unique ID, wait message ID)
void NEMPauseGame::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1201);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_PAUSE_GAME, inPayloadPointer, theSlots, Slots(this, theSlots));
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

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMPauseGameResponse* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mUniqueID);
    return 5;
}

unsigned long NEMPauseGameResponse::ExportSize()   // ICF-folded at 0x3fc79
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_PAUSE_GAME_RESPONSE);
}

// FUNCTION: LLE 0x1003f49e
// Exports a pause-game response (unique ID) into a new buffer
void NEMPauseGameResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_PAUSE_GAME_RESPONSE, theSlots, Slots(this, theSlots), outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003f509
// Imports a pause-game response (unique ID)
void NEMPauseGameResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1270);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_PAUSE_GAME_RESPONSE, inPayloadPointer, theSlots, Slots(this, theSlots));
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMPhysicsTick* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mNumberOfPlayerInputs);
    outSlots[5] = NetMsgField(inMessage->mPhysicsTicksToPerform);
    outSlots[6] = NetMsgField(inMessage->mGenerateChecksum);
    outSlots[7] = NetMsgField(inMessage->mDumpTheWorld);
    return 8;
}

unsigned long NEMPhysicsTick::ExportSize()   // ICF-folded at 0x3fb8e
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_PHYSICS_TICK);
}

// FUNCTION: LLE 0x1003f54a
// Exports a physics tick (input count, ticks to perform, checksum and dump flags) into a new buffer
void NEMPhysicsTick::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_PHYSICS_TICK, theSlots, Slots(this, theSlots), outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003f5e8
// Imports a physics tick (input count, ticks to perform, checksum and dump flags)
void NEMPhysicsTick::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1341);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_PHYSICS_TICK, inPayloadPointer, theSlots, Slots(this, theSlots));
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMPlayerConnectionCheck* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mPingTimeStart);
    return 5;
}

unsigned long NEMPlayerConnectionCheck::ExportSize()   // ICF-folded at 0x3fc79
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_PLAYER_CONNECTION_CHECK);
}

// FUNCTION: LLE 0x1003f65d
// Exports a connection check (ping start time) into a new buffer
void NEMPlayerConnectionCheck::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_PLAYER_CONNECTION_CHECK, theSlots, Slots(this, theSlots),
                 outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003f6c8
// Imports a connection check (ping start time)
void NEMPlayerConnectionCheck::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1408);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_PLAYER_CONNECTION_CHECK, inPayloadPointer, theSlots, Slots(this, theSlots));
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMPlayerConnectionCheckResponse* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mPingTimeStart);
    return 5;
}

unsigned long NEMPlayerConnectionCheckResponse::ExportSize()   // ICF-folded at 0x3fc79
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_PLAYER_CONNECTION_CHECK_RESPONSE);
}

// FUNCTION: LLE 0x1003f709
// Exports a connection check response (ping start time) into a new buffer
void NEMPlayerConnectionCheckResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_PLAYER_CONNECTION_CHECK_RESPONSE, theSlots, Slots(this, theSlots),
                 outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003f774
// Imports a connection check response (ping start time)
void NEMPlayerConnectionCheckResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1466);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_PLAYER_CONNECTION_CHECK_RESPONSE, inPayloadPointer, theSlots, Slots(this, theSlots));
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMRemovePlayer* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mPlayerIndex);
    outSlots[5] = NetMsgField(inMessage->mPlayerIsOOS);
    outSlots[6] = NetMsgField(inMessage->mGameIsExiting);
    outSlots[7] = NetMsgField(inMessage->mWasBooted);
    return 8;
}

unsigned long NEMRemovePlayer::ExportSize()   // ICF-folded at 0x3fc79
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_REMOVE_PLAYER);
}

// FUNCTION: LLE 0x1003f7b5
// Exports a remove-player message (player index, out-of-sync, game exiting and booted flags) into a new buffer
void NEMRemovePlayer::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_REMOVE_PLAYER, theSlots, Slots(this, theSlots), outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003f851
// Imports a remove-player message (player index, out-of-sync, game exiting and booted flags)
void NEMRemovePlayer::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1536);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_REMOVE_PLAYER, inPayloadPointer, theSlots, Slots(this, theSlots));
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMServerIsAlive* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    return 4;
}

unsigned long NEMServerIsAlive::ExportSize()   // ICF-folded at 0x3fb10
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_SERVER_IS_ALIVE);
}

// FUNCTION: LLE 0x1003f8c4
// Exports a server-is-alive message (header only) into a new buffer
void NEMServerIsAlive::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_SERVER_IS_ALIVE, theSlots, Slots(this, theSlots), outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003f90e
// Imports a server-is-alive message (header only)
void NEMServerIsAlive::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1600);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_SERVER_IS_ALIVE, inPayloadPointer, theSlots, Slots(this, theSlots));
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMStartGame* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mOffsetStartTime);
    return 5;
}

unsigned long NEMStartGame::ExportSize()   // ICF-folded at 0x3fc79
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_START_GAME);
}

// FUNCTION: LLE 0x1003f93e
// Exports a start-game message (start time offset) into a new buffer
void NEMStartGame::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_START_GAME, theSlots, Slots(this, theSlots), outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003f9a9
// Imports a start-game message (start time offset)
void NEMStartGame::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1655);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_START_GAME, inPayloadPointer, theSlots, Slots(this, theSlots));
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMStartGameResponse* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mApplicationTimeAtStart);
    return 5;
}

unsigned long NEMStartGameResponse::ExportSize()   // ICF-folded at 0x3fc79
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_START_GAME_RESPONSE);
}

// FUNCTION: LLE 0x1003f9ea
// Exports a start-game response (application time at start) into a new buffer
void NEMStartGameResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_START_GAME_RESPONSE, theSlots, Slots(this, theSlots), outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003fa55
// Imports a start-game response (application time at start)
void NEMStartGameResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1713);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_START_GAME_RESPONSE, inPayloadPointer, theSlots, Slots(this, theSlots));
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMStopGame* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    return 4;
}

unsigned long NEMStopGame::ExportSize()   // ICF-folded at 0x3fb10
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_STOP_GAME);
}

// FUNCTION: LLE 0x1003fa96
// Exports a stop-game message (header only) into a new buffer
void NEMStopGame::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_STOP_GAME, theSlots, Slots(this, theSlots), outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003fae0
// Imports a stop-game message (header only)
void NEMStopGame::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1768);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_STOP_GAME, inPayloadPointer, theSlots, Slots(this, theSlots));
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMStopGameResponse* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    return 4;
}

// FUNCTION: LLE 0x1003fb10
// Returns the exported size of a stop-game response: the 4-byte header (shared by every header-only message)
unsigned long NEMStopGameResponse::ExportSize()
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_STOP_GAME_RESPONSE);
}

// FUNCTION: LLE 0x1003fb14
// Exports a stop-game response (header only) into a new buffer
void NEMStopGameResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_STOP_GAME_RESPONSE, theSlots, Slots(this, theSlots), outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003fb5e
// Imports a stop-game response (header only)
void NEMStopGameResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1819);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_STOP_GAME_RESPONSE, inPayloadPointer, theSlots, Slots(this, theSlots));
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMUnPauseGame* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mUniqueID);
    outSlots[5] = NetMsgField(inMessage->mSendToAllAndWaitMessageID);
    return 6;
}

// FUNCTION: LLE 0x1003fb8e
// Returns the exported size of an unpause message: header and two longs (12 bytes; shared by other messages)
unsigned long NEMUnPauseGame::ExportSize()
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_UN_PAUSE_GAME);
}

// FUNCTION: LLE 0x1003fb92
// Exports an unpause message (unique ID, wait message ID) into a new buffer
void NEMUnPauseGame::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_UN_PAUSE_GAME, theSlots, Slots(this, theSlots), outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003fc10
// Imports an unpause message (unique ID, wait message ID)
void NEMUnPauseGame::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1878);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_UN_PAUSE_GAME, inPayloadPointer, theSlots, Slots(this, theSlots));
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

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMUnPauseGameResponse* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mUniqueID);
    return 5;
}

// FUNCTION: LLE 0x1003fc79
// Returns the exported size of an unpause response: header and one long (8 bytes; shared by other messages)
unsigned long NEMUnPauseGameResponse::ExportSize()
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_UN_PAUSE_GAME_RESPONSE);
}

// FUNCTION: LLE 0x1003fc7d
// Exports an unpause response (unique ID) into a new buffer
void NEMUnPauseGameResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_UN_PAUSE_GAME_RESPONSE, theSlots, Slots(this, theSlots),
                 outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003fce8
// Imports an unpause response (unique ID)
void NEMUnPauseGameResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 1947);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_UN_PAUSE_GAME_RESPONSE, inPayloadPointer, theSlots, Slots(this, theSlots));
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMWorldDatabaseChecksum* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mPhysicsTickTrigger);
    outSlots[5] = NetMsgField(inMessage->mDumpWorld);
    return 6;
}

// FUNCTION: LLE 0x1003fd29
// Returns the exported size of a world checksum request: header, trigger tick and dump flag (9 bytes)
unsigned long NEMWorldDatabaseChecksum::ExportSize()
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_WORLD_DATABASE_CHECKSUM);
}

// FUNCTION: LLE 0x1003fd2d
// Exports a world checksum request (physics tick trigger, dump flag) into a new buffer
void NEMWorldDatabaseChecksum::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_WORLD_DATABASE_CHECKSUM, theSlots, Slots(this, theSlots),
                 outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003fda9
// Imports a world checksum request (physics tick trigger, dump flag)
void NEMWorldDatabaseChecksum::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 2009);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_WORLD_DATABASE_CHECKSUM, inPayloadPointer, theSlots, Slots(this, theSlots));
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMWorldDatabaseChecksumResponse* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mCommunicationsTick);
    outSlots[5] = NetMsgField(inMessage->mWorldDatabaseChecksum);
    return 6;
}

unsigned long NEMWorldDatabaseChecksumResponse::ExportSize()   // ICF-folded at 0x3fb8e
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_WORLD_DATABASE_CHECKSUM_RESPONSE);
}

// FUNCTION: LLE 0x1003fdf9
// Exports a world checksum response (communications tick, checksum) into a new buffer
void NEMWorldDatabaseChecksumResponse::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_WORLD_DATABASE_CHECKSUM_RESPONSE, theSlots, Slots(this, theSlots),
                 outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003fe77
// Imports a world checksum response (communications tick, checksum)
void NEMWorldDatabaseChecksumResponse::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 2074);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_WORLD_DATABASE_CHECKSUM_RESPONSE, inPayloadPointer, theSlots, Slots(this, theSlots));
}

// FUNCTION: LLE 0x1003fec7
// Returns the exported size of any file transfer message: header, ID, data size and the full 1 KB data block
unsigned long NEMXferMessage::StaticExportSize()
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_XFER_MESSAGE);
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMXferMessage* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mID);
    outSlots[5] = NetMsgField(inMessage->mDataSize);
    outSlots[6] = NetMsgField(inMessage->mData);
    return 7;
}

unsigned long NEMXferMessage::ExportSize()
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_XFER_MESSAGE);
}

// FUNCTION: LLE 0x1003fecd
// Exports a file transfer block (ID, data size, the whole 1 KB data) into a new buffer
void NEMXferMessage::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_XFER_MESSAGE, theSlots, Slots(this, theSlots), outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x1003ff68
// Imports a file transfer block (ID, data size, 1 KB data)
void NEMXferMessage::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 2153);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_XFER_MESSAGE, inPayloadPointer, theSlots, Slots(this, theSlots));
}

// the codec's slots: the header, then the members in wire order (ee_netmsg engine.rs)
static size_t Slots(NEMXferStatus* inMessage, EeNetMsgSlot* outSlots)
{
    inMessage->GetHeaderSlots(outSlots);
    outSlots[4] = NetMsgField(inMessage->mClientSlot);
    outSlots[5] = NetMsgField(inMessage->mPercentDone);
    outSlots[6] = NetMsgField(inMessage->mXferID);
    outSlots[7] = NetMsgField(inMessage->mStatusID);
    return 8;
}

// FUNCTION: LLE 0x1003ffe0
// Returns the exported size of a transfer status: header, client slot, percent, transfer ID and status (11 bytes)
unsigned long NEMXferStatus::ExportSize()
{
    return ee_netmsg_fixed_size(EE_NETMSG_NE_XFER_STATUS);
}

// FUNCTION: LLE 0x1003ffe4
// Exports a transfer status (client slot, percent done, transfer ID, status) into a new buffer
void NEMXferStatus::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    NetMsgExport(EE_NETMSG_NE_XFER_STATUS, theSlots, Slots(this, theSlots), outPayloadPointer, outMessageSize);
}

// FUNCTION: LLE 0x10040082
// Imports a transfer status (client slot, percent done, transfer ID, status)
void NEMXferStatus::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 2231);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    ee_netmsg_import(EE_NETMSG_NE_XFER_STATUS, inPayloadPointer, theSlots, Slots(this, theSlots));
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
