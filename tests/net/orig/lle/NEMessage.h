// ne_msgmgr_test stand-in for the engine messages: a message of the world (a type, a key, a slot, a count of
// references). A response answers a message of the type before it with the same key. Every effect is a world line.
#pragma once
#include "net_world.h"
enum {
    kNetMessageAddPlayer = 0x20, kNetMessageCommunicationsTick = 0x26, kNetMessageCommunicationsTickResponse = 0x27,
    kNetMessagePauseGame = 0x33, kNetMessagePauseGameResponse = 0x34, kNetMessageUnPauseGame = 0x35,
    kNetMessageUnPauseGameResponse = 0x36, kNetMessageServerIsAlive = 0x3a, kNetMessageXferMessage = 0x3b
};
enum { kSlotServer = 0xfe, kSlotNone = 0xff };

class NEMessage {
public:
    NEMessage(unsigned char inType, unsigned long inKey) : mType(inType), mKey(inKey), mSlot(0xff), mCount(1)
    {
        this->mID = gWorld->NewMessage(this);
        if (!this->Quiet())
            gWorld->Line("new m%d %02x %lu", this->mID, inType, inKey);
    }
    // the transfer blocks are temporaries whose release the original orders differently around the send
    bool Quiet() const { return this->mType == 0x3b; }
    virtual ~NEMessage() {}
    void Reference()
    {
        this->mCount++;
        if (!this->Quiet())
            gWorld->Line("ref m%d %d", this->mID, this->mCount);
    }
    void Dereference()
    {
        this->mCount--;
        if (!this->Quiet())
            gWorld->Line("deref m%d %d", this->mID, this->mCount);
        if (this->mCount == 0) {
            gWorld->FreeMessage(this->mID);
            delete this;
        }
    }
    unsigned char GetType() const { return this->mType; }
    unsigned char GetOriginatingSlot() const { return this->mSlot; }
    void SetOriginatingSlot(unsigned char inSlot) { this->mSlot = inSlot; gWorld->Line("slot m%d %d", this->mID, inSlot); }
    bool IsResponse(NEMessage* inMessage)
    {
        bool theAnswer = (this->mType == 0x20 || this->mType == 0x26 || this->mType == 0x33 || this->mType == 0x35)
                         && inMessage->mType == this->mType + 1 && inMessage->mKey == this->mKey;
        return theAnswer;
    }
    virtual unsigned long ExportSize() { return 5; }
    // the bytes: the id, the type
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize)
    {
        *outData = new unsigned char[5];
        memcpy(*outData, &this->mID, 4);
        (*outData)[4] = this->mType;
        outSize = 5;
    }
    unsigned long GetCurrentCommunicationsTick() const { return this->mKey; }
    unsigned long GetUniqueID() const { return this->mKey; }

    unsigned char mType;
    unsigned long mKey;
    unsigned char mSlot;
    int mCount;
    int mID;
};
class NEMCommunicationsTick : public NEMessage {};
class NEMUnPauseGame : public NEMessage {};
class NEMPauseGame : public NEMessage {};
class NEMCommunicationsTickResponse : public NEMessage {
public:
    NEMCommunicationsTickResponse(unsigned long inCTs, unsigned long inTick, unsigned long inAverage)
        : NEMessage(kNetMessageCommunicationsTickResponse, inTick)
        { gWorld->Line("tick response m%d %lu %lu %lu", this->mID, inCTs, inTick, inAverage); }
};
class NEMUnPauseGameResponse : public NEMessage {
public:
    NEMUnPauseGameResponse(unsigned long inID) : NEMessage(kNetMessageUnPauseGameResponse, inID) {}
};
class NEMPauseGameResponse : public NEMessage {
public:
    NEMPauseGameResponse(unsigned long inID) : NEMessage(kNetMessagePauseGameResponse, inID) {}
};
class NEMXferMessage : public NEMessage {
public:
    NEMXferMessage(unsigned char& inID, unsigned char* inData, unsigned long& inDataSize)
        : NEMessage(kNetMessageXferMessage, inID), mBytes(inData, inData + inDataSize) {}
    virtual unsigned long ExportSize() { return 1 + (unsigned long)this->mBytes.size(); }
    // the bytes: the status, the data
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize)
    {
        outSize = 1 + (unsigned long)this->mBytes.size();
        (*outData)[0] = (unsigned char)this->mKey;
        if (!this->mBytes.empty())
            memcpy(*outData + 1, &this->mBytes[0], this->mBytes.size());
    }
    std::vector<unsigned char> mBytes;
};
