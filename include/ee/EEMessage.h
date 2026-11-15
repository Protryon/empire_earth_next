// C:\EEX\Empire Earth\Communications\EEMessage.h (RECONSTRUCTION; file name inferred). Base of the game's network
// messages (RTTI EEMessage > NEMessage > UReferenceCounter; vtable 0x438ae8; EEMessage.cpp around 0xb8f8e). sizeof 0x10.
// Requires lle/NEMessage.h.
#ifndef EE_EEMESSAGE_H
#define EE_EEMESSAGE_H
#include <stdint.h>   // int32_t/uint32_t: the wire fields' fixed widths
#include <stddef.h>   // size_t: the codec slots' count
#include <string.h>   // memcpy: EEMRecord's members
#include <initializer_list>
#include <vector>
#include <type_traits>

#include "lle/U2DPoint.h"                  // EEMSendFlare's inline ctor
#include "ee/EEMessageID.h"

class EEGameOptions;
class GE2DPoint;
class GERectangle;
class GEVector;
struct EeNetMsgSlot;                       // rust/ee_rust.h (the game messages' codec)
class EEGameOptionsPlayer;

extern bool gLocalPlayerOut;

class EEMessage : public NEMessage {
public:
    // inline: EEMSelectPlayerChatType's ctor (0x1d2b36) expands it; 0x32a6 is the out-of-line copy where VC6 refused it
    EEMessage(unsigned char inType = 0xff, bool inFlag = false) : NEMessage(inType), mCanSendWhenOut(inFlag) {}

    virtual ~EEMessage() {}   // user-declared: every ??_G stores EEMessage's vptr (0x26fbf7)
    // inline override of NEMessage's +0x04 (the body 0x1891 is in every EEM* vtable that does not override it and
    // sits in EEABuild.cpp, its first user in link order)
    virtual bool GetIsAvailableToSend() const { return !gLocalPlayerOut || this->mCanSendWhenOut; }

    static NEMessage* BuildEEMessage(unsigned char* inMemory);   // the factory 0xb8f8e (sConstructors[0x41..0xfd])
    static void RegisterKnownMessages();
    // the physics loop's two ways to execute a message (0x1537ba/0x1537e3; provisional names): types 0x82,
    // 0x84, 0x96 record their selection id (mU0C) per player, the rest execute only when it is the current one
    void ExecuteSelectionMessage();
    void ExecuteMessage();

    // New (game messages lane, round 1): the codec's side of every game message in one place (EEMessage.cpp): the
    // message gives its codec class (ee_netmsg) and its members as slots in wire order (round 5: every game message is
    // an EEMRecord); ExportMessage, ExportSize and ImportMessage run the codec over them; a game message answers no
    // request unless its type's rule says so (IsResponse: EEMRecord's).
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass) = 0;
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage) { return false; }

    unsigned char mFastSelectionID;         // +0x0c (provisional name; not set by the ctor; EEMSelectPlayerChatType 0x1d2b36 stores +0x0d)
    bool mCanSendWhenOut;                   // +0x0d
};

// New (port): dynamic_cast<EEMessage*>(inMessage) as the message kind of its type (ee_netmsg: a message is an
// EEMessage exactly when its type is a game type, 0x41..0xfd); NULL stays NULL
extern "C" uint8_t ee_netmsg_message_kind(uint8_t message_type);   // rust/ee_rust.h
inline EEMessage* EEGameMessageCast(NEMessage* inMessage)
{
    return inMessage != NULL && ee_netmsg_message_kind(inMessage->GetType()) == 2 /* EE_NETMSG_KIND_GAME */
               ? static_cast<EEMessage*>(inMessage)
               : NULL;
}

// New (game messages lane, round 3): a sender's value for one member of an EEMRecord, converted as the old classes'
// constructors converted it to the member's type (an integer truncated, a bool as != 0, a float, a point, a vector;
// round 5: a list, each element's low bytes as one record: the ids' longs as the wire's int32)
struct EEMRecordValue {
    template <class T, typename std::enable_if<std::is_integral<T>::value || std::is_enum<T>::value, int>::type = 0>
    EEMRecordValue(T inValue) : mInteger((long long)inValue), mIsFloat(false), mString(NULL), mList(NULL) {}
    EEMRecordValue(double inValue) : mInteger(0), mIsFloat(true), mString(NULL), mList(NULL) { this->mFloats[0] = (float)inValue; }
    EEMRecordValue(const GE2DPoint& inPoint);
    EEMRecordValue(const GEVector& inVector);
    EEMRecordValue(const UWideString& inString) : mInteger(0), mIsFloat(false), mString(&inString), mList(NULL) {}   // round 4
    template <class T>
    EEMRecordValue(const std::vector<T>& inList)
        : mInteger(0), mIsFloat(false), mString(NULL), mList(inList.empty() ? (const void*)"" : (const void*)&inList[0]),
          mListCount(inList.size()), mListElement(sizeof(T)) {}

    long long mInteger;
    float mFloats[3];
    bool mIsFloat;
    const UWideString* mString;   // a string member's value (the sender's, alive for the constructor's call)
    const void* mList;            // a list's elements (round 5), mListCount of mListElement bytes
    size_t mListCount;
    size_t mListElement;
};

// New (game messages lane, rounds 3-5): one class carries every game message type (108: the census in PORT_LEARNINGS
// "R5b.4 game messages, round 3" .. "round 5"): its codec class's fixed members after the selection id, packed in
// wire order in mRecord, then the type's members that are not on the wire (the box selection's rectangle and unit
// type); its one string (if any) in mString, its one list or blob (if any) in mList; the layout, flags, rules and
// label are the Rust's (ee_netmsg records.rs). A sender passes the members in wire order:
// new EEMRecord(kMessageCheatCode, {theCode, theParameter}); Execute runs the type's body (EEMessage.cpp); a reader
// reads Get<T>(i) (i: the member's wire index, 1 = the first after the selection id) and says which in a comment.
class EEMRecord : public EEMessage {
public:
    EEMRecord(unsigned char* inData);   // the factory's: the type at inData[1], then the import
    EEMRecord(unsigned char inType, std::initializer_list<EEMRecordValue> inValues = {});
    virtual bool GetCanSendWhilePaused() const;
    virtual bool GetSendToAllAndWaitMessageID(unsigned long& outID) const;   // the type's wait member (Load or Save)
    virtual void SetSendToAllAndWaitMessageID(unsigned long inID);
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual bool IsResponse(NEMessage* inMessage);   // the type's response rule (Load or Save: its tick's response)
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    // member inIndex (1 = the first after the selection id) as its type (the size is verified)
    template <class T> T Get(uint32_t inIndex) const
    {
        T theValue;
        memcpy((void*)&theValue, this->Field(inIndex, sizeof(T)), sizeof(T));
        return theValue;
    }
    template <class T> void Set(uint32_t inIndex, T inValue) { memcpy(this->Field(inIndex, sizeof(T)), (const void*)&inValue, sizeof(T)); }
    // the list's records as T (each its record's bytes: sizeof(T) is the record's)
    template <class T> std::vector<T> GetList() const
    {
        std::vector<T> theList(this->mList.size() / sizeof(T));
        if (!theList.empty())
            memcpy((void*)&theList[0], &this->mList[0], theList.size() * sizeof(T));
        return theList;
    }
    unsigned char* Field(uint32_t inIndex, size_t inSize) const;
    virtual void ImportMessage(unsigned char* inData);   // the codec, then the string's text and the list's records

    unsigned char mRecord[64];   // EE_NETMSG_RECORD_BYTES
    UWideString mString;         // the type's string member, if it has one (EE_NETMSG_RECORD_FIELD_STRING)
    std::vector<unsigned char> mList;   // the type's list's records or blob's bytes (EE_NETMSG_RECORD_FIELD_LIST/BLOB)
};

// New (game messages lane, round 4): the lobby's two messages that copy a whole object (the old EEMFullGameOptions
// and EEMFullPlayerInformation constructors), as records
EEMRecord* EENewFullGameOptions(const EEGameOptions& inOptions);
EEMRecord* EENewFullPlayerInformation(const EEGameOptionsPlayer& inPlayer);
// round 5: the box selection (0x4e, 0x4f, 0x50; the old EEMSelectUnits constructor): the local player's units of
// inUnitType in inRectangle (EEUnitPicker), compressed; and its units decoded (the compressed blob's ids)
EEMRecord* EENewSelectUnits(const GERectangle& inRectangle, long inUnitType, unsigned char inType);
void EESelectUnitsIDs(const EEMRecord* inMessage, std::vector<long>& outUnitIDs);
// round 5: the old EEMLoadOrSaveGame::LoadOrSave (EEPhysics::ServeLoadSaveRequests runs each queued request): saves
// the game under the request's name; returns its tick
long EELoadOrSave(EEMRecord* inRequest);

// The lobby slot fields EEMPlayerInformation updates (mField; Execute): 0 type, 1 ping, 2 colour, 3 team,
// 4 starting citizens, 5 ready; port: 6 a computer slot's engine (ee/EECPEngine.h). The lobby redraws the row with
// the field as LobbyShowPlayer's flags.
const long kPlayerInfoEngine = 6;

#endif
