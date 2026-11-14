// ne_client_test stand-in for the engine messages: a message of the world with a type, the virtual flags the client
// reads, the typed messages' fields and a count of references. Its export is [type][id 4][slots 2]; a received packet
// is the id of a message the driver made (0: none). Every effect is a world line; the getters make none.
#pragma once
#include <vector>
#include "c_world.h"

enum NEConnectionResult { kConnectionAccepted = 1 };
enum NEMessageID {
    kNetMessageClientConnectionRequest = 0x22,
    kNetMessageClientConnectionRequestResponse = 0x23,
    kNetMessageClientDisconnectionNotification = 0x24,
    kNetMessageCommunicationsTick = 0x26,
    kNetMessageCommunicationsTickResponse = 0x27,
    kNetMessageStartGame = 0x2d,
    kNetMessageStartGameResponse = 0x2e,
    kNetMessageStopGame = 0x2f,
    kNetMessageStopGameResponse = 0x30,
    kNetMessageWorldDatabaseChecksum = 0x31,
    kNetMessagePauseGame = 0x33,
    kNetMessagePauseGameResponse = 0x34,
    kNetMessageUnPauseGame = 0x35,
    kNetMessageUnPauseGameResponse = 0x36,
    kNetMessageClientReconnectionRequest = 0x37,
    kNetMessageClientReconnectionRequestResponse = 0x38,
    kNetMessageClientRequestPause = 0x39,
    kNetMessageServerIsAlive = 0x3a,
    kNetMessageXferMessage = 0x3b,
    kNetMessageXferStatus = 0x3c,
};
enum NESlot { kSlotServer = 0xfe, kSlotNone = 0xff };

const unsigned long kCXferRecord = 40;   // NEMXferMessage::StaticExportSize(): [id][data 38][data size]

class NEMessage {
public:
    explicit NEMessage(unsigned char inType) : mType(inType)
    {
        this->mID = gQ->mNextMessage++;
        gQ->mMessages[this->mID] = this;
        gQ->Line("msg new m%d %02x", this->mID, inType);
    }
    virtual ~NEMessage() {}
    void Reference()
    {
        this->mCount++;
        gQ->Line("msg ref m%d %d", this->mID, this->mCount);
    }
    void Dereference()
    {
        this->mCount--;
        gQ->Line("msg deref m%d %d", this->mID, this->mCount);
        if (this->mCount == 0) {
            gQ->mMessages.erase(this->mID);
            delete this;
        }
    }
    bool GetIsAvailableToSend() const { return this->mAvailable; }
    bool GetCanSendWhilePaused() const { return this->mWhilePaused; }
    bool GetCountsTowardsPlayerInputThrottle() const { return this->mCounts; }
    bool GetSendToAllAndWaitMessageID(unsigned long& outID) const
    {
        if (!this->mS2aw)
            return false;
        outID = this->mS2awID;
        return true;
    }
    void ExportMessage(unsigned char** outData, unsigned long& outSize)
    {
        gQ->Line("msg export m%d", this->mID);
        *outData = new unsigned char[7];
        (*outData)[0] = this->mType;
        memcpy(*outData + 1, &this->mID, 4);
        (*outData)[5] = this->mForSlot;
        (*outData)[6] = this->mFromSlot;
        outSize = 7;
    }
    char* GetMessageLabel() const { return (char*)"message"; }
    unsigned char GetType() const { return this->mType; }
    void SetForSpecificSlot(unsigned char inSlot)
    {
        this->mForSlot = inSlot;
        gQ->Line("msg forslot m%d %d", this->mID, inSlot);
    }
    void SetOriginatingSlot(unsigned char inSlot)
    {
        this->mFromSlot = inSlot;
        gQ->Line("msg fromslot m%d %d", this->mID, inSlot);
    }
    static NEMessage* ConvertMemoryToMessage(unsigned char* inData)
    {
        int theID;
        memcpy(&theID, inData, 4);
        NEMessage* m = theID != 0 && gQ->mMessages.count(theID) ? gQ->mMessages[theID] : NULL;
        gQ->Line("msg convert m%d", m != NULL ? m->mID : 0);
        return m;
    }
    // a tick response's inputs: at most three
    long AddPlayerInput(NEMessage* inInput)
    {
        long r = this->mInputs.size() < 3 ? 0 : 9;
        if (r == 0)
            this->mInputs.push_back(inInput->mID);
        gQ->Line("msg addinput m%d m%d = %ld", this->mID, inInput->mID, r);
        return r;
    }
    // the typed messages' getters
    uint32_t& GetCurrentCommunicationsTick() { return this->mTick; }
    unsigned long GetServersCommunicationTick() const { return this->mServersTick; }
    unsigned long GetOffsetStartTime() const { return this->mOffset; }
    unsigned char GetSendToSlot() const { return this->mSlot; }
    unsigned char GetNewSendToSlot() const { return this->mSlot; }
    long GetError() const { return this->mError; }
    unsigned long GetCurrentCT() const { return this->mCT; }
    unsigned long GetUniqueID() const { return this->mS2awID; }
    unsigned char* GetData() const { return (unsigned char*)this->mData; }
    unsigned long GetDataSize() const { return this->mDataSize; }
    unsigned char GetID() const { return this->mXferID; }
    static unsigned long StaticExportSize() { return kCXferRecord; }

    unsigned char mType;
    int mID;
    int mCount = 1;
    bool mAvailable = true, mWhilePaused = false, mCounts = true, mS2aw = false;
    unsigned char mForSlot = 0, mFromSlot = 0;
    uint32_t mTick = 0;
    unsigned long mServersTick = 0, mOffset = 0, mCT = 0, mS2awID = 0;
    unsigned char mSlot = 0;
    long mError = 0;
    std::vector<int> mInputs;
    unsigned char mXferID = 0;
    unsigned char mData[kCXferRecord - 2] = {};
    unsigned long mDataSize = 0;
};

class NEMCommunicationsTick : public NEMessage {};
class NEMStartGame : public NEMessage {};
class NEMClientConnectionRequestResponse : public NEMessage {};
class NEMClientReconnectionRequestResponse : public NEMessage {};
class NEMPauseGame : public NEMessage {};
class NEMUnPauseGame : public NEMessage {};
class NEMCommunicationsTickResponse : public NEMessage {
public:
    NEMCommunicationsTickResponse(unsigned long inProcessed, unsigned long inTick, unsigned long inAverage)
        : NEMessage(kNetMessageCommunicationsTickResponse)
    {
        this->mServersTick = inTick;
        this->mCounts = false;
        gQ->Line("msg tickresponse m%d %lu %lu %lu", this->mID, inProcessed, inTick, inAverage);
    }
};
class NEMStopGameResponse : public NEMessage {
public:
    NEMStopGameResponse() : NEMessage(kNetMessageStopGameResponse) {}
};
class NEMStartGameResponse : public NEMessage {
public:
    NEMStartGameResponse(unsigned long& inTime) : NEMessage(kNetMessageStartGameResponse)
    {
        gQ->Line("msg startresponse m%d %lu", this->mID, inTime);
    }
};
class NEMPauseGameResponse : public NEMessage {
public:
    NEMPauseGameResponse(unsigned long inID) : NEMessage(kNetMessagePauseGameResponse)
    {
        gQ->Line("msg pauseresponse m%d %lu", this->mID, inID);
    }
};
class NEMUnPauseGameResponse : public NEMessage {
public:
    NEMUnPauseGameResponse(unsigned long inID) : NEMessage(kNetMessageUnPauseGameResponse)
    {
        gQ->Line("msg unpauseresponse m%d %lu", this->mID, inID);
    }
};
class NEMXferMessage : public NEMessage {
public:
    NEMXferMessage(unsigned char* inData) : NEMessage(kNetMessageXferMessage)
    {
        this->mXferID = inData[0];
        memcpy(this->mData, inData + 1, kCXferRecord - 2);
        this->mDataSize = inData[kCXferRecord - 1] % (kCXferRecord - 1);
        gQ->Line("msg xfer m%d %s", this->mID, CHex(inData, kCXferRecord).c_str());
    }
};
class NEMXferStatus : public NEMessage {
public:
    NEMXferStatus(unsigned char& inSlot, unsigned char& inPercent, uint32_t& inXferID, unsigned char& inStatus)
        : NEMessage(kNetMessageXferStatus)
    {
        gQ->Line("msg xferstatus m%d %d %d %u %d", this->mID, inSlot, inPercent, inXferID, inStatus);
    }
};
