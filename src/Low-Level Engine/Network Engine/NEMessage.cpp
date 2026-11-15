// C:\EEX\Low-Level Engine\Network Engine\NEMessage.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// Message (de)serialization: a 4-byte header (reserved, type, for-slot, originating slot) then each message's
// fields in its own order. The 28 engine messages are one data class, NEMRecord (LLE Network Engine round 6): its
// layout, flags, response rules and labels are the Rust's (ee_netmsg ne_records.rs), its wire form the codec's
// (ee_netmsg engine.rs); the factory's builders by type are the Rust's table.
#include <windows.h>
#include <string.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UReferenceCounter.h"
#include "lle/NEMessage.h"
#include "rust/ee_netmsg.h"   // ee_netmsg: the engine messages' wire layout and records

// ---- the codec (ee_netmsg engine.rs, docs/RUST_PLAN.md R5b.4): a message hands its members to the Rust as slots in
// wire order, the header's four first ----
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
    static_cast<NEMRecord*>(inList)->mPlayerInputs.push_back(theMessage);
    return (uint32_t)theMessage->ExportSize();
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
// Builds a message from received bytes with the builder registered for its type; NULL for a type with none
NEMessage* NEMessage::ConvertMemoryToMessage(unsigned char* inMemory)
{
    ESVERIFY_AT(inMemory != NULL, 31);
    return static_cast<NEMessage*>(ee_netmsg_build(inMemory));
}

void NEMessage::RegisterBuilder(unsigned char inFirst, unsigned char inLast, NEMessage* (*inBuilder)(unsigned char*))
{
    ee_netmsg_register_builder(inFirst, inLast, reinterpret_cast<void* (*)(uint8_t*)>(inBuilder));
}

// FUNCTION: LLE 0x1003e12b
// Registers the engine's own message builder for the engine message types 0x20..0x3c
void NEMessage::RegisterKnownMessages()
{
    NEMessage::RegisterBuilder(0x20, 0x3c, NEMessage::BuildNEMessage);
}

// FUNCTION: LLE 0x1003e13d
// Builds the engine message of the type in the header from received bytes; asserts on an unknown type
NEMessage* NEMessage::BuildNEMessage(unsigned char* inData)
{
    if (ee_netmsg_ne_type(inData[1]).class_id != 0xffffffffu)
        return new NEMRecord(inData);
    ESVERIFY_AT("Undefined message type" == NULL, 227);
    return NULL;
}

// ---- NEMRecord: the engine messages (ee_netmsg ne_records.rs) ----

// the import constructor: NEMessage's defaults, then the bytes (the type first: it sizes the record)
NEMRecord::NEMRecord(unsigned char* inData)
{
    this->mType = inData[1];
    this->mRecord = new unsigned char[ee_netmsg_ne_type(this->mType).record_size + 1]();
    this->ImportMessage(inData);
}

// a sender's: the fixed members in record order, then the strings (fewer leave the rest 0 or "")
static long long NetMsgValue(const NEMRecordValue& inValue) { return inValue.mValue; }
static long long NetMsgValue(uint32_t inValue) { return inValue; }

NEMRecord::NEMRecord(unsigned char inType, std::initializer_list<NEMRecordValue> inValues,
                     std::initializer_list<const char*> inStrings)
    : NEMessage(inType)
{
    this->Fill(inValues.begin(), inValues.end(), inStrings.begin(), inStrings.end());
}

// New (round 7): the Rust server's and client's (their fixed members only)
NEMRecord::NEMRecord(unsigned char inType, const uint32_t* inValues, size_t inCount)
    : NEMessage(inType)
{
    this->Fill(inValues, inValues + inCount, (const char* const*)NULL, (const char* const*)NULL);
}

template <class V>
void NEMRecord::Fill(const V* inValue, const V* inValuesEnd, const char* const* inString, const char* const* inStringsEnd)
{
    EeNetMsgNeType theType = ee_netmsg_ne_type(this->mType);
    ESVERIFY_AT(theType.class_id != 0xffffffffu, __LINE__);
    this->mRecord = new unsigned char[theType.record_size + 1]();
    for (uint32_t i = 0; i < theType.fields; i++) {
        EeNetMsgNeField theField = ee_netmsg_ne_field(this->mType, i);
        if (theField.kind == EE_NETMSG_NE_FIELD_STRING && inString != inStringsEnd) {
            this->mStrings[theField.offset] = *inString++;
        } else if ((theField.kind == EE_NETMSG_NE_FIELD_INTEGER || theField.kind == EE_NETMSG_NE_FIELD_BOOL) &&
                   inValue != inValuesEnd) {
            long long theInteger = NetMsgValue(*inValue++);
            if (theField.kind == EE_NETMSG_NE_FIELD_BOOL)
                theInteger = theInteger != 0;
            memcpy(this->mRecord + theField.offset, &theInteger, theField.size);   // little-endian: the low bytes
        }
    }
    ESVERIFY_AT(inValue == inValuesEnd && inString == inStringsEnd, __LINE__);
}

// New (round 7): member inIndex's value for the Rust (its bytes, zero-extended); the players' inputs' count for the
// inputs; a block's address
uintptr_t NEMRecord::Value(uint32_t inIndex) const
{
    EeNetMsgNeField theField = ee_netmsg_ne_field(this->mType, inIndex);
    if (theField.kind == EE_NETMSG_NE_FIELD_INPUTS)
        return this->mPlayerInputs.size();
    if (theField.size > sizeof(uint64_t))
        return (uintptr_t)this->Field(inIndex, theField.size);
    uint64_t theValue = 0;
    memcpy(&theValue, this->Field(inIndex, theField.size), theField.size);
    return (uintptr_t)theValue;
}

// FUNCTION: LLE 0x1003eecf
// Releases every player input the message holds
NEMRecord::~NEMRecord()
{
    while (this->mPlayerInputs.size() > 0) {
        this->mPlayerInputs.back()->Dereference();
        this->mPlayerInputs.pop_back();
    }
    delete[] this->mRecord;
}

unsigned char* NEMRecord::Field(uint32_t inIndex, size_t inSize) const
{
    EeNetMsgNeField theField = ee_netmsg_ne_field(this->mType, inIndex);
    ESVERIFY_AT(theField.kind != EE_NETMSG_NE_FIELD_NONE && theField.size == inSize && inSize != 0, __LINE__);
    return this->mRecord + theField.offset;
}

const UWideString& NEMRecord::GetString(uint32_t inIndex) const
{
    EeNetMsgNeField theField = ee_netmsg_ne_field(this->mType, inIndex);
    ESVERIFY_AT(theField.kind == EE_NETMSG_NE_FIELD_STRING, __LINE__);
    return this->mStrings[theField.offset];
}

bool NEMRecord::GetCanSendWhilePaused() const
{
    return (ee_netmsg_ne_type(this->mType).flags & EE_NETMSG_NE_PAUSED) != 0;
}

bool NEMRecord::GetCountsTowardsPlayerInputThrottle() const
{
    return (ee_netmsg_ne_type(this->mType).flags & EE_NETMSG_NE_THROTTLE) != 0;
}

bool NEMRecord::GetSendToAllAndWaitMessageID(unsigned long& outID) const
{
    EeNetMsgNeType theType = ee_netmsg_ne_type(this->mType);
    if (theType.s2aw_field == EE_NETMSG_NE_NONE)
        return false;
    outID = this->Get<uint32_t>(theType.s2aw_field);
    return true;
}

void NEMRecord::SetSendToAllAndWaitMessageID(unsigned long inID)
{
    EeNetMsgNeType theType = ee_netmsg_ne_type(this->mType);
    if (theType.s2aw_field != EE_NETMSG_NE_NONE)
        this->Set<uint32_t>(theType.s2aw_field, (uint32_t)inID);
}

char* NEMRecord::GetMessageLabel() const
{
    return const_cast<char*>(ee_netmsg_ne_type(this->mType).label);
}

// FUNCTION: LLE 0x1003f270
// Whether a message is this one's response, by the type's rule: a tick and the response for its tick, a pause or an
// unpause and the response with its unique ID
bool NEMRecord::IsResponse(NEMessage* inMessage)
{
    EeNetMsgNeType theType = ee_netmsg_ne_type(this->mType);
    return theType.response_type != 0 && inMessage->GetType() == theType.response_type &&
           this->Get<uint32_t>(theType.response_mine) ==
               static_cast<NEMRecord*>(inMessage)->Get<uint32_t>(theType.response_theirs);
}

// FUNCTION: LLE 0x1003ef11
// Adds a player input message; kErrorBadPath once the list holds 255
long NEMRecord::AddPlayerInput(NEMessage* inInput)
{
    if (this->mPlayerInputs.size() < 255) {
        this->mPlayerInputs.push_back(inInput);
        return kErrorOK;
    }
    return kErrorBadPath;
}

// FUNCTION: LLE 0x1003ef3f
// Returns the player input message at an index; kErrorBeyondEnd past the end
long NEMRecord::GetPlayerInput(unsigned long inIndex, NEMessage*& outInput)
{
    if (inIndex < this->mPlayerInputs.size()) {
        outInput = this->mPlayerInputs[inIndex];
        return kErrorOK;
    }
    return kErrorBeyondEnd;
}

unsigned long NEMRecord::XferExportSize()
{
    return ee_netmsg_fixed_size(ee_netmsg_ne_type(kNetMessageXferMessage).class_id);
}

// the codec's slots: the header, then the wire members (a fixed one in the record, a string, the inputs)
size_t NEMRecord::CodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass, std::vector<EeNetMsgSlot>& outParts,
                             std::vector<unsigned char*>* outBuffers)
{
    EeNetMsgNeType theType = ee_netmsg_ne_type(this->mType);
    outClass = theType.class_id;
    this->GetHeaderSlots(outSlots);
    for (uint32_t i = 0; i < theType.wire_fields; i++) {
        EeNetMsgNeField theField = ee_netmsg_ne_field(this->mType, i);
        EeNetMsgSlot theSlot = {this->mRecord + theField.offset, theField.size, 0, 0};
        if (theField.kind == EE_NETMSG_NE_FIELD_STRING)
            theSlot = NetMsgString(this->mStrings[theField.offset]);
        else if (theField.kind == EE_NETMSG_NE_FIELD_INPUTS)
            theSlot = NetMsgInputs(this->mPlayerInputs, outParts, outBuffers);
        outSlots[4 + i] = theSlot;
    }
    return 4 + theType.wire_fields;
}

// FUNCTION: LLE 0x1003e751
unsigned long NEMRecord::ExportSize()
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    std::vector<EeNetMsgSlot> theParts;
    uint32_t theClass;
    size_t theCount = this->CodecSlots(theSlots, theClass, theParts, NULL);
    return ee_netmsg_export_size(theClass, theSlots, theCount);
}

// FUNCTION: LLE 0x1003e75d
// Exports the message into a new buffer of its size
void NEMRecord::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    std::vector<EeNetMsgSlot> theParts;
    std::vector<unsigned char*> theBuffers;
    uint32_t theClass;
    size_t theCount = this->CodecSlots(theSlots, theClass, theParts, &theBuffers);
    unsigned long theMessageSize = ee_netmsg_export_size(theClass, theSlots, theCount);
    *outPayloadPointer = new unsigned char[theMessageSize];
    outMessageSize = ee_netmsg_export(theClass, theSlots, theCount, *outPayloadPointer, theMessageSize);
    NetMsgFree(theBuffers);
}

// FUNCTION: LLE 0x1003e7de
// Imports the message: the fixed members into the record, then the strings and the players' inputs where the codec
// found them
void NEMRecord::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, 277);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    std::vector<EeNetMsgSlot> theParts;
    uint32_t theClass;
    size_t theCount = this->CodecSlots(theSlots, theClass, theParts, NULL);
    ee_netmsg_import(theClass, inPayloadPointer, theSlots, theCount);
    for (size_t i = 4; i < theCount; i++) {
        EeNetMsgNeField theField = ee_netmsg_ne_field(this->mType, (uint32_t)i - 4);
        if (theField.kind == EE_NETMSG_NE_FIELD_STRING)
            this->mStrings[theField.offset].Import(inPayloadPointer + theSlots[i].offset);
        else if (theField.kind == EE_NETMSG_NE_FIELD_INPUTS)
            ee_netmsg_inputs_import(inPayloadPointer + theSlots[i].offset, NetMsgBuildInput, this);
    }
}
