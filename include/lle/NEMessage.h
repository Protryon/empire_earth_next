// C:\EEX\Low-Level Engine\Network Engine\NEMessage.h (RECONSTRUCTION; NEMessage.cpp 0x3e05c..0x400f7). The network
// message base class and the engine's messages. Requires UReferenceCounter, UWideString (lle/LowLevelEngine.h).
// The original's 28 engine message classes (NEMAddPlayer .. NEMXferStatus, NEMPlayerInput) are one data class,
// NEMRecord (LLE Network Engine round 6); the factory's table of builders by type (the original's sConstructors) is
// the Rust's (ee_netmsg ne_records.rs): the engine and the game register theirs.
#ifndef LLE_NEMESSAGE_H
#define LLE_NEMESSAGE_H

#include <stdint.h>   // int32_t/uint32_t: the wire fields' fixed widths

#include <string.h>   // memcpy: NEMRecord's members

#include <initializer_list>
#include <type_traits>
#include <vector>
#include "lle/NEMessageID.h"
#include "rust/game/lle.h"
#include "rust/sim/netmsg.h"

struct EeNetMsgSlot;   // rust/ee_rust.h (the messages' codec)

class LLE_API NEMessage : public UReferenceCounter {   // proven sizeof 0xc (NEMServerIsAlive adds nothing)
public:
    // proven inline with all defaults: the exported default constructor closure 0x3bd20 passes 0xff, 0xff, 0xff, 0
    NEMessage(unsigned char inType = 0xff, unsigned char inForSpecificSlot = 0xff,
              unsigned char inOriginatingSlot = 0xff, unsigned char inReserved = 0)
        : mType(inType), mForSpecificSlot(inForSpecificSlot), mOriginatingSlot(inOriginatingSlot),
          mReserved(inReserved) {}
    virtual bool GetIsAvailableToSend() const { return true; }                  // +0x04
    virtual bool GetCanSendWhilePaused() const { return false; }                // +0x08
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return true; }   // +0x0c
    virtual bool GetSendToAllAndWaitMessageID(unsigned long& outID) const { return false; }   // +0x10
    virtual void SetSendToAllAndWaitMessageID(unsigned long inID) {}            // +0x14
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize) = 0;   // +0x18
    virtual unsigned long ExportSize() = 0;                                     // +0x1c
    virtual void ImportMessage(unsigned char* inData) = 0;                      // +0x20
    // +0x24: no message answers this one (the original's 26 one-line overrides, folded at 0x3ed59)
    virtual bool IsResponse(NEMessage* inMessage) { return false; }
    virtual void Execute() = 0;                                                 // +0x28
    virtual char* GetMessageLabel() const = 0;                                  // +0x2c
    unsigned char GetType() const { return this->mType; }
    unsigned char GetForSpecificSlot() const { return this->mForSpecificSlot; }
    unsigned char GetOriginatingSlot() const { return this->mOriginatingSlot; }
    void SetForSpecificSlot(unsigned char inSlot) { this->mForSpecificSlot = inSlot; }
    void SetOriginatingSlot(unsigned char inSlot) { this->mOriginatingSlot = inSlot; }
    void SetType(unsigned char inType) { this->mType = inType; }   // New (LLE round 8): the import constructor's (netrecord.rs)

    // the message the builder registered for its type makes from received bytes; NULL for a type with none
    static NEMessage* ConvertMemoryToMessage(unsigned char* inData);
    static NEMessage* BuildNEMessage(unsigned char* inData)
        { return static_cast<NEMessage*>(ee_ne_record_build(inData)); }
    static void RegisterKnownMessages() { NEMessage::RegisterBuilder(0x20, 0x3c, NEMessage::BuildNEMessage); }
    // New (round 6): registers a builder for the types inFirst..inLast (the original wrote sConstructors[256])
    static void RegisterBuilder(unsigned char inFirst, unsigned char inLast, NEMessage* (*inBuilder)(unsigned char*))
        { ee_netmsg_register_builder(inFirst, inLast, reinterpret_cast<void* (*)(uint8_t*)>(inBuilder)); }
    void GetHeaderSlots(EeNetMsgSlot* outSlots);   // New (port): the header as the codec's first four slots

    virtual ~NEMessage() {}

    unsigned char mType;                    // +0x08
    unsigned char mForSpecificSlot;         // +0x09
    unsigned char mOriginatingSlot;         // +0x0a
    unsigned char mReserved;                // +0x0b provisional name (always 0)
};

// New (LLE Network Engine round 6): a sender's value for one member of an NEMRecord, converted as the 28 classes'
// constructors converted it (an integer truncated to the member's size, a bool as != 0)
struct NEMRecordValue {
    template <class T, typename std::enable_if<std::is_integral<T>::value || std::is_enum<T>::value, int>::type = 0>
    NEMRecordValue(T inValue) : mValue((long long)inValue) {}
    long long mValue;
};

// New (LLE Network Engine round 6): the 28 engine messages (0x20..0x3c but 0x2c) as one class (LLE round 8: its
// bodies are Rust, ee_game lle netrecord.rs). The fixed members sit
// packed in wire order in mRecord (the transfer's 1 KB block among them), then the members that never travel (the
// physics tick's rate); the strings by ordinal in mStrings; the communications ticks' players' inputs in
// mPlayerInputs. The layout, the type's flags, its send-to-all-and-wait member, its response rule and its label are
// the Rust's (ee_netmsg ne_records.rs). A sender passes the fixed members in record order, then the strings:
// new NEMRecord(kNetMessageClientConnectionRequest, {theFirewall, theChecksum}, {gGameVersion, theName}).
// Members are numbered from 0 after the header, in the wire order of ee_netmsg engine.rs.
class LLE_API NEMRecord : public NEMessage {
public:
    NEMRecord(unsigned char* inData);   // the factory's: the type at inData[1], then the import
    NEMRecord(unsigned char inType, std::initializer_list<NEMRecordValue> inValues = {},
              std::initializer_list<const char*> inStrings = {});
    NEMRecord(unsigned char inType, const uint32_t* inValues, size_t inCount);   // the Rust's: fixed members only
    virtual ~NEMRecord();   // releases the players' inputs
    virtual bool GetCanSendWhilePaused() const
        { return (ee_netmsg_ne_type(this->mType).flags & EE_NETMSG_NE_PAUSED) != 0; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const
        { return (ee_netmsg_ne_type(this->mType).flags & EE_NETMSG_NE_THROTTLE) != 0; }
    virtual bool GetSendToAllAndWaitMessageID(unsigned long& outID) const { return ee_ne_record_s2aw_id(this, &outID); }
    virtual void SetSendToAllAndWaitMessageID(unsigned long inID) { ee_ne_record_set_s2aw_id(this, inID); }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize)
        { ee_ne_record_export(this, outData, &outSize); }
    virtual unsigned long ExportSize() { return ee_ne_record_export_size(this); }
    virtual void ImportMessage(unsigned char* inData) { ee_ne_record_import(this, inData); }
    virtual bool IsResponse(NEMessage* inMessage) { return ee_ne_record_is_response(this, inMessage); }
    virtual void Execute() {}
    virtual char* GetMessageLabel() const { return const_cast<char*>(ee_netmsg_ne_type(this->mType).label); }

    // member inIndex as its type (the size is verified)
    template <class T> T Get(uint32_t inIndex) const
    {
        T theValue;
        memcpy((void*)&theValue, this->Field(inIndex, sizeof(T)), sizeof(T));
        return theValue;
    }
    template <class T> void Set(uint32_t inIndex, T inValue) { memcpy(this->Field(inIndex, sizeof(T)), (const void*)&inValue, sizeof(T)); }
    unsigned char* Field(uint32_t inIndex, size_t inSize) const { return ee_ne_record_field(this, inIndex, inSize); }
    // member inIndex for the Rust: its value zero-extended, the inputs' count, a block's address
    uintptr_t Value(uint32_t inIndex) const { return ee_ne_record_value(this, inIndex); }
    const UWideString& GetString(uint32_t inIndex) const
        { return *static_cast<const UWideString*>(ee_ne_record_string(this, inIndex)); }
    // the transfer message's (fixed) size
    static unsigned long XferExportSize()
        { return ee_netmsg_fixed_size(ee_netmsg_ne_type(kNetMessageXferMessage).class_id); }

    // the players' inputs (the communications tick and its response): at most 255
    unsigned long GetNumberOfPlayerInputs() const { return this->mPlayerInputs.size(); }
    long AddPlayerInput(NEMessage* inInput) { return ee_ne_record_add_input(this, inInput); }
    long GetPlayerInput(unsigned long inIndex, NEMessage*& outInput)
        { return ee_ne_record_input(this, inIndex, reinterpret_cast<void**>(&outInput)); }

    unsigned char* mRecord;                 // the fixed members (new[] of the type's record size)
    UWideString mStrings[2];
    std::vector<NEMessage*> mPlayerInputs;

};

#endif
