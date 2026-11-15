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

    // the message the builder registered for its type makes from received bytes; NULL for a type with none
    static NEMessage* ConvertMemoryToMessage(unsigned char* inData);
    static NEMessage* BuildNEMessage(unsigned char* inData);
    static void RegisterKnownMessages();
    // New (round 6): registers a builder for the types inFirst..inLast (the original wrote sConstructors[256])
    static void RegisterBuilder(unsigned char inFirst, unsigned char inLast, NEMessage* (*inBuilder)(unsigned char*));
    void GetHeaderSlots(EeNetMsgSlot* outSlots);   // New (port): the header as the codec's first four slots

protected:
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

// New (LLE Network Engine round 6): the 28 engine messages (0x20..0x3c but 0x2c) as one class. The fixed members sit
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
    virtual bool GetCanSendWhilePaused() const;
    virtual bool GetCountsTowardsPlayerInputThrottle() const;
    virtual bool GetSendToAllAndWaitMessageID(unsigned long& outID) const;
    virtual void SetSendToAllAndWaitMessageID(unsigned long inID);
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}
    virtual char* GetMessageLabel() const;

    // member inIndex as its type (the size is verified)
    template <class T> T Get(uint32_t inIndex) const
    {
        T theValue;
        memcpy((void*)&theValue, this->Field(inIndex, sizeof(T)), sizeof(T));
        return theValue;
    }
    template <class T> void Set(uint32_t inIndex, T inValue) { memcpy(this->Field(inIndex, sizeof(T)), (const void*)&inValue, sizeof(T)); }
    unsigned char* Field(uint32_t inIndex, size_t inSize) const;
    // member inIndex for the Rust: its value zero-extended, the inputs' count, a block's address
    uintptr_t Value(uint32_t inIndex) const;
    const UWideString& GetString(uint32_t inIndex) const;
    static unsigned long XferExportSize();   // the transfer message's (fixed) size

    // the players' inputs (the communications tick and its response): at most 255
    unsigned long GetNumberOfPlayerInputs() const { return this->mPlayerInputs.size(); }
    long AddPlayerInput(NEMessage* inInput);
    long GetPlayerInput(unsigned long inIndex, NEMessage*& outInput);

    unsigned char* mRecord;                 // the fixed members (new[] of the type's record size)
    UWideString mStrings[2];
    std::vector<NEMessage*> mPlayerInputs;

private:
    template <class V>
    void Fill(const V* inValue, const V* inValuesEnd, const char* const* inString, const char* const* inStringsEnd);
    size_t CodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass, std::vector<EeNetMsgSlot>& outParts,
                      std::vector<unsigned char*>* outBuffers);
};

#endif
