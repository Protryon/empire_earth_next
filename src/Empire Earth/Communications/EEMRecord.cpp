// New (game messages lane, round 6): the game messages' class (ee/EEMessage.h): EEMessage's codec over the slots
// and EEMRecord, the one data class every game message type is (its constructors, members, rules and codec slots;
// ee_netmsg records.rs), and the lobby's two whole-object senders. Split from EEMessage.cpp (its 0xb8cda.. range keeps
// the factory, the Execute host and the bodies) so that a program can link the messages without the world: the
// server's and client's golden (tests/net/ee_session_test.cpp).
#include <windows.h>
#include <string.h>
#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "ee/EEGameOptions.h"
#include "ee/EEMessage.h"
#include "rust/ee_netmsg.h"

// ---- the codec (ee_netmsg, docs/RUST_PLAN.md R5b.4): each message hands its members to the Rust as slots in wire
// order, the header's four first; the layout (sizes, order, the strings' and lists' forms) is the Rust table's ----
static const size_t kNetMsgMaxSlots = 32;

template <class T> static inline EeNetMsgSlot EENetMsgField(T& inMember)
{
    EeNetMsgSlot theSlot = {(uint8_t*)&inMember, (uint32_t)sizeof(T), 0, 0};
    return theSlot;
}

// a string's bytes (UWideString::Export's strlen); on import the Rust reports where the string is
static inline EeNetMsgSlot EENetMsgString(const UWideString& inString)
{
    const char* theBytes = inString;
    EeNetMsgSlot theSlot = {(uint8_t*)theBytes, (uint32_t)strlen(theBytes), 0, 0};
    return theSlot;
}

// a new buffer of the message's size, filled by the Rust (the original's ExportMessage shape)
static void EENetMsgExport(uint32_t inClass, EeNetMsgSlot* inSlots, size_t inCount, unsigned char** outPayloadPointer,
                           unsigned long& outMessageSize)
{
    unsigned long theMessageSize = ee_netmsg_export_size(inClass, inSlots, inCount);
    *outPayloadPointer = new unsigned char[theMessageSize];
    outMessageSize = ee_netmsg_export(inClass, inSlots, inCount, *outPayloadPointer, theMessageSize);
}

// New (game messages lane, round 1): the codec's side of every game message, once. The message gives its codec
// class and its members as slots in wire order (GetCodecSlots); these run the codec over them, as each class's own
// ExportMessage, ExportSize and ImportMessage did.
void EEMessage::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    uint32_t theClass;
    size_t theCount = this->GetCodecSlots(theSlots, theClass);
    EENetMsgExport(theClass, theSlots, theCount, outPayloadPointer, outMessageSize);
}

// a class of fixed members its constant, else the size of these members
unsigned long EEMessage::ExportSize()
{
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    uint32_t theClass;
    size_t theCount = this->GetCodecSlots(theSlots, theClass);
    uint32_t theFixed = ee_netmsg_fixed_size(theClass);
    return theFixed != 0 ? theFixed : ee_netmsg_export_size(theClass, theSlots, theCount);
}

void EEMessage::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, __LINE__);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    uint32_t theClass;
    size_t theCount = this->GetCodecSlots(theSlots, theClass);
    ee_netmsg_import(theClass, inPayloadPointer, theSlots, theCount);
}

// ---- EEMRecord (game messages lane, round 3): the record types' messages (ee/EEMessage.h; ee_netmsg records.rs) ----
EEMRecordValue::EEMRecordValue(const GE2DPoint& inPoint) : mInteger(0), mIsFloat(true)
{
    this->mFloats[0] = inPoint.GetX();
    this->mFloats[1] = inPoint.GetY();
}

EEMRecordValue::EEMRecordValue(const GEVector& inVector) : mInteger(0), mIsFloat(true)
{
    this->mFloats[0] = inVector.GetX();
    this->mFloats[1] = inVector.GetY();
    this->mFloats[2] = inVector.GetZ();
}

// the import constructor: EEMessage's defaults (no send flag), as the old classes' import constructors
EEMRecord::EEMRecord(unsigned char* inData)
{
    memset(this->mRecord, 0, sizeof(this->mRecord));
    this->mType = inData[1];
    this->ImportMessage(inData);
}

// a sender's: the members in wire order, then the ones off the wire (fewer leave the rest 0)
EEMRecord::EEMRecord(unsigned char inType, std::initializer_list<EEMRecordValue> inValues)
    : EEMessage(inType, (ee_netmsg_record_flags(inType) & EE_NETMSG_RECORD_WHEN_OUT) != 0)
{
    memset(this->mRecord, 0, sizeof(this->mRecord));
    ESVERIFY_AT(ee_netmsg_record_class(inType) != 0xffffffffu, __LINE__);
    uint32_t theIndex = 1;
    for (const EEMRecordValue& theValue : inValues) {
        EeNetMsgRecordField theField = ee_netmsg_record_member(inType, theIndex);
        bool theBeside = theField.kind == EE_NETMSG_RECORD_FIELD_STRING || theField.kind == EE_NETMSG_RECORD_FIELD_LIST
                         || theField.kind == EE_NETMSG_RECORD_FIELD_BLOB;
        unsigned char* theTo = theBeside ? NULL : this->Field(theIndex, theField.size);
        theIndex++;
        float theFloat = theValue.mIsFloat ? theValue.mFloats[0] : (float)theValue.mInteger;
        long long theInteger = theValue.mIsFloat ? (long long)theValue.mFloats[0] : theValue.mInteger;
        bool theBool = theInteger != 0 || (theValue.mIsFloat && theValue.mFloats[0] != 0.0f);
        switch (theField.kind) {
        case EE_NETMSG_RECORD_FIELD_STRING:   // the one string member (not in mRecord)
            ESVERIFY_AT(theValue.mString != NULL, __LINE__);
            this->mString = *theValue.mString;
            break;
        case EE_NETMSG_RECORD_FIELD_LIST:   // the one list (not in mRecord): each element's low bytes, one record
            ESVERIFY_AT(theValue.mList != NULL && theValue.mListElement >= theField.size, __LINE__);
            for (size_t i = 0; i < theValue.mListCount; i++) {
                const unsigned char* theElement = (const unsigned char*)theValue.mList + i * theValue.mListElement;
                this->mList.insert(this->mList.end(), theElement, theElement + theField.size);
            }
            break;
        case EE_NETMSG_RECORD_FIELD_BLOB:   // the blob and a member off the wire as bytes: set by their sender
        case EE_NETMSG_RECORD_FIELD_RAW:
            break;
        case EE_NETMSG_RECORD_FIELD_INTEGER:
            memcpy(theTo, &theInteger, theField.size);   // little-endian: the low bytes
            break;
        case EE_NETMSG_RECORD_FIELD_BOOL:
            *theTo = theBool ? 1 : 0;
            break;
        case EE_NETMSG_RECORD_FIELD_FLOAT:
            memcpy(theTo, &theFloat, 4);
            break;
        default:   // a point or a vector
            memcpy(theTo, theValue.mFloats, theField.size);
            break;
        }
    }
}

unsigned char* EEMRecord::Field(uint32_t inIndex, size_t inSize) const
{
    EeNetMsgRecordField theField = ee_netmsg_record_member(this->mType, inIndex);
    ESVERIFY_AT(theField.kind != EE_NETMSG_RECORD_FIELD_NONE && theField.size == inSize, __LINE__);
    return const_cast<unsigned char*>(this->mRecord) + theField.offset;
}

bool EEMRecord::GetCanSendWhilePaused() const
{
    return (ee_netmsg_record_flags(this->mType) & EE_NETMSG_RECORD_PAUSED) != 0;
}

char* EEMRecord::GetMessageLabel() const
{
    return const_cast<char*>(ee_netmsg_record_label(this->mType));
}

bool EEMRecord::GetSendToAllAndWaitMessageID(unsigned long& outID) const
{
    EeNetMsgRecordRules theRules = ee_netmsg_record_rules(this->mType);
    if (theRules.wait_field != 0)
        outID = this->Get<uint32_t>(theRules.wait_field);
    return theRules.wait_field != 0;
}

void EEMRecord::SetSendToAllAndWaitMessageID(unsigned long inID)
{
    EeNetMsgRecordRules theRules = ee_netmsg_record_rules(this->mType);
    if (theRules.wait_field != 0)
        this->Set<uint32_t>(theRules.wait_field, (uint32_t)inID);
}

// Whether a message is this one's response, by the type's rule (Load or Save: the response for its tick)
bool EEMRecord::IsResponse(NEMessage* inMessage)
{
    EeNetMsgRecordRules theRules = ee_netmsg_record_rules(this->mType);
    return theRules.response_type != 0 && inMessage->GetType() == theRules.response_type
           && this->Get<int32_t>(theRules.response_mine) == static_cast<EEMRecord*>(inMessage)->Get<int32_t>(theRules.response_theirs);
}

size_t EEMRecord::GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass)
{
    outClass = ee_netmsg_record_class(this->mType);
    this->GetHeaderSlots(outSlots);
    outSlots[4] = EENetMsgField(this->mFastSelectionID);
    size_t theCount = 5;
    for (EeNetMsgRecordField theField; (theField = ee_netmsg_record_field(outClass, (uint32_t)theCount - 4)).kind != 0; theCount++) {
        EeNetMsgSlot theSlot = {this->mRecord + theField.offset, theField.size, 0, 0};
        if (theField.kind == EE_NETMSG_RECORD_FIELD_STRING)
            theSlot = EENetMsgString(this->mString);
        else if (theField.kind == EE_NETMSG_RECORD_FIELD_LIST || theField.kind == EE_NETMSG_RECORD_FIELD_BLOB) {
            // the records: the first, a record's size, the count; the blob: its bytes (the room for an import)
            theSlot.data = this->mList.empty() ? NULL : &this->mList[0];
            theSlot.size = theField.kind == EE_NETMSG_RECORD_FIELD_LIST ? theField.size : (uint32_t)this->mList.size();
            theSlot.count = theField.kind == EE_NETMSG_RECORD_FIELD_LIST ? (uint32_t)(this->mList.size() / theField.size) : 0;
        }
        outSlots[theCount] = theSlot;
    }
    return theCount;
}

// the codec over the slots, then the string member's text from where the codec found it (UWideString::Import), the
// list's records from where it found them, the blob's bytes as many as it found
void EEMRecord::ImportMessage(unsigned char* inPayloadPointer)
{
    ESVERIFY_AT(inPayloadPointer != NULL, __LINE__);
    EeNetMsgSlot theSlots[kNetMsgMaxSlots];
    uint32_t theClass;
    this->mList.clear();   // the slots carry no room: the list and the blob are copied from where the codec found them
    size_t theCount = this->GetCodecSlots(theSlots, theClass);
    ee_netmsg_import(theClass, inPayloadPointer, theSlots, theCount);
    for (size_t i = 5; i < theCount; i++) {
        EeNetMsgRecordField theField = ee_netmsg_record_field(theClass, (uint32_t)i - 4);
        unsigned char* theFrom = inPayloadPointer + theSlots[i].offset;
        if (theField.kind == EE_NETMSG_RECORD_FIELD_STRING)
            this->mString.Import(theFrom);
        else if (theField.kind == EE_NETMSG_RECORD_FIELD_LIST)
            this->mList.assign(theFrom, theFrom + theSlots[i].count * theField.size);
        else if (theField.kind == EE_NETMSG_RECORD_FIELD_BLOB)
            this->mList.assign(theFrom, theFrom + theSlots[i].count);
    }
}

// The lobby's game options, whole (the server sends gGameOptions); the members in wire order
EEMRecord* EENewFullGameOptions(const EEGameOptions& inOptions)
{
    return new EEMRecord(kMessageFullGameOptions,
                         {inOptions.GetAllowCustomCivs(), inOptions.GetB407(), inOptions.GetCheatCodes(),
                          inOptions.GetDifficulty(), inOptions.GetEpochEnd(), inOptions.GetGameSpeed(),
                          inOptions.GetLoadType(), inOptions.GetVariant(), inOptions.GetLockSpeed(),
                          inOptions.GetLockTeams(), inOptions.GetMapSize(), inOptions.GetRecordGames(),
                          inOptions.GetResources(), inOptions.GetRevealMap(), inOptions.GetEpochStart(),
                          inOptions.GetUnitLimit(), inOptions.GetVictory(), inOptions.GetWondersForVictory(),
                          inOptions.GetMapType()});
}

// One lobby slot, whole (the server sends it to the players); the members in wire order
EEMRecord* EENewFullPlayerInformation(const EEGameOptionsPlayer& inPlayer)
{
    return new EEMRecord(kMessageFullPlayerInformation,
                         {inPlayer.mSlot, inPlayer.mType, inPlayer.mLoadedType, inPlayer.mName, inPlayer.mPing,
                          inPlayer.mColor, inPlayer.mTeam, inPlayer.mStartingCitizens, inPlayer.mIsReady,
                          inPlayer.mIsLocal, inPlayer.mB36, (int32_t)inPlayer.mEngine});   // the engine: port
}

