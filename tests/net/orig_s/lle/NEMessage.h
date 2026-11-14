// ne_server_test stand-in for the engine messages: a message of the world with a type, a key (a tick, a pause's id)
// that its response repeats, an export size, the flags the server reads, the typed messages' fields and a count of
// references. Its export is [type][id 4][from][for][key 4][inputs' ids 4 each], zeros to its size; a received packet
// is the id of a message the driver made (0: none). Every effect is a world line; the getters make none.
#pragma once
#include <vector>
#include "LowLevelEngine.h"

enum NEMessageID {
    kNetMessageAddPlayerResponse = 0x21,
    kNetMessageClientConnectionRequest = 0x22,
    kNetMessageClientDisconnectionNotification = 0x24,
    kNetMessageCommunicationsTick = 0x26,
    kNetMessageCommunicationsTickResponse = 0x27,
    kNetMessagePlayerConnectionCheck = 0x29,
    kNetMessagePlayerConnectionCheckResponse = 0x2a,
    kNetMessageRemovePlayer = 0x2b,
    kNetMessageStartGameResponse = 0x2e,
    kNetMessageStopGame = 0x2f,
    kNetMessageStopGameResponse = 0x30,
    kNetMessageWorldDatabaseChecksumResponse = 0x32,
    kNetMessagePauseGame = 0x33,
    kNetMessagePauseGameResponse = 0x34,
    kNetMessageUnPauseGame = 0x35,
    kNetMessageUnPauseGameResponse = 0x36,
    kNetMessageClientReconnectionRequest = 0x37,
    kNetMessageClientReconnectionRequestResponse = 0x38,
    kNetMessageServerIsAlive = 0x3a,
    kNetMessageXferMessage = 0x3b,
};
enum NESlot { kSlotServer = 0xfe, kSlotNone = 0xff };

class NEMessage {
public:
    NEMessage(unsigned char inType, unsigned long inSize = 12) : mType(inType), mSize(inSize < 12 ? 12 : inSize)
    {
        this->mID = gQ->mNextMessage++;
        gQ->mMessages[this->mID] = this;
        gQ->Line("msg new m%d %02x %lu", this->mID, inType, inSize);
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
    unsigned char GetType() const { return this->mType; }
    bool IsResponse(NEMessage* inResponse) const
    {
        return inResponse->mType == this->mType + 1 && inResponse->mKey == this->mKey;
    }
    unsigned char GetOriginatingSlot() const { return this->mFromSlot; }
    void SetOriginatingSlot(unsigned char inSlot)
    {
        this->mFromSlot = inSlot;
        gQ->Line("msg fromslot m%d %d", this->mID, inSlot);
    }
    void SetForSpecificSlot(unsigned char inSlot)
    {
        this->mForSlot = inSlot;
        gQ->Line("msg forslot m%d %d", this->mID, inSlot);
    }
    void SetSendToAllAndWaitMessageID(unsigned long inID)
    {
        this->mS2awID = inID;
        gQ->Line("msg s2awid m%d %lu", this->mID, inID);
    }
    bool GetCanSendWhilePaused() const { return this->mWhilePaused; }
    bool GetIsAvailableToSend() const { return this->mAvailable; }
    char* GetMessageLabel() const { return (char*)"message"; }
    unsigned long ExportSize() const
    {
        unsigned long n = this->mSize;
        for (int i : this->mInputs)
            n += gQ->mMessages.count(i) ? gQ->mMessages[i]->mSize : 0;
        return n;
    }
    void ExportMessage(unsigned char** outData, unsigned long& outSize)
    {
        outSize = this->ExportSize();
        *outData = new unsigned char[outSize];
        memset(*outData, 0, outSize);
        (*outData)[0] = this->mType;
        memcpy(*outData + 1, &this->mID, 4);
        (*outData)[5] = this->mFromSlot;
        (*outData)[6] = this->mForSlot;
        uint32_t theKey = (uint32_t)this->mKey;
        memcpy(*outData + 7, &theKey, 4);
        for (size_t i = 0; i < this->mInputs.size() && 11 + 4 * i + 4 <= outSize; i++)
            memcpy(*outData + 11 + 4 * i, &this->mInputs[i], 4);
        gQ->Line("msg export m%d %lu", this->mID, outSize);
    }
    static NEMessage* ConvertMemoryToMessage(unsigned char* inData)
    {
        int theID;
        memcpy(&theID, inData, 4);
        NEMessage* m = theID != 0 && gQ->mMessages.count(theID) ? gQ->mMessages[theID] : NULL;
        gQ->Line("msg convert m%d", m != NULL ? m->mID : 0);
        return m;
    }

    unsigned char mType;
    int mID;
    int mCount = 1;
    unsigned long mSize;
    unsigned long mKey = 0;
    unsigned long mS2awID = 0;
    bool mAvailable = true, mWhilePaused = false;
    unsigned char mFromSlot = 0, mForSlot = 0;
    std::vector<int> mInputs;
    // the typed messages' fields
    unsigned long mProcessed = 0, mAverage = 0, mCT = 0, mChecksum = 0;
    unsigned long mExtraInputs = 0;   // a tick response's inputs counted that GetPlayerInput does not find
};

class NEMCommunicationsTick : public NEMessage {
public:
    NEMCommunicationsTick(unsigned long inTick, unsigned char inPTs, unsigned char inPTsPerSecond, bool inChecksum,
                          bool inIsOOS)
        : NEMessage(kNetMessageCommunicationsTick)
    {
        this->mKey = inTick;
        gQ->Line("msg tick m%d %lu %d %d %d %d", this->mID, inTick, inPTs, inPTsPerSecond, (int)inChecksum, (int)inIsOOS);
    }
    long AddPlayerInput(NEMessage* inInput)
    {
        this->mInputs.push_back(inInput->mID);
        gQ->Line("msg addinput m%d m%d", this->mID, inInput->mID);
        return 0;
    }
    unsigned long GetCurrentCommunicationsTick() const { return this->mKey; }
};
class NEMCommunicationsTickResponse : public NEMessage {
public:
    NEMCommunicationsTickResponse(unsigned long inProcessed, unsigned long inTick, unsigned long inAverage)
        : NEMessage(kNetMessageCommunicationsTickResponse)
    {
        this->mKey = inTick;
        this->mProcessed = inProcessed;
        this->mAverage = inAverage;
        gQ->Line("msg tickresponse m%d %lu %lu %lu", this->mID, inProcessed, inTick, inAverage);
    }
    unsigned long GetNumberOfPlayerInputs() const { return (unsigned long)this->mInputs.size() + this->mExtraInputs; }
    long GetPlayerInput(unsigned long inIndex, NEMessage*& outInput)
    {
        long r = inIndex < this->mInputs.size() && gQ->mMessages.count(this->mInputs[inIndex]) ? 0 : 9;
        if (r == 0)
            outInput = gQ->mMessages[this->mInputs[inIndex]];
        return r;
    }
    unsigned long GetLastCommunicationsTickProcessed() const { return this->mProcessed; }
    unsigned long GetAveragePhysicsTickTime() const { return this->mAverage; }
};
class NEMPauseGame : public NEMessage {
public:
    NEMPauseGame(unsigned long inTick) : NEMessage(kNetMessagePauseGame)
    {
        this->mKey = inTick;
        gS->mPauseKeys.push_back(inTick);
        gQ->Line("msg pause m%d %lu", this->mID, inTick);
    }
    unsigned long GetUniqueID() const { return this->mKey; }
};
class NEMUnPauseGame : public NEMessage {
public:
    NEMUnPauseGame(unsigned long inTick) : NEMessage(kNetMessageUnPauseGame)
    {
        this->mKey = inTick;
        gS->mPauseKeys.push_back(inTick);
        gQ->Line("msg unpause m%d %lu", this->mID, inTick);
    }
    unsigned long GetUniqueID() const { return this->mKey; }
};
class NEMPauseGameResponse : public NEMessage {
public:
    NEMPauseGameResponse(unsigned long inID) : NEMessage(kNetMessagePauseGameResponse)
    {
        this->mKey = inID;
        gQ->Line("msg pauseresponse m%d %lu", this->mID, inID);
    }
};
class NEMUnPauseGameResponse : public NEMessage {
public:
    NEMUnPauseGameResponse(unsigned long inID) : NEMessage(kNetMessageUnPauseGameResponse)
    {
        this->mKey = inID;
        gQ->Line("msg unpauseresponse m%d %lu", this->mID, inID);
    }
};
class NEMXferMessage : public NEMessage {
public:
    NEMXferMessage(unsigned char inStatus, unsigned char* inData, unsigned long inSize)
        : NEMessage(kNetMessageXferMessage, 8 + inSize)
    {
        gQ->Line("msg xfer m%d %d %s", this->mID, inStatus, SHex(inData, inSize).c_str());
    }
};
class NEMServerIsAlive : public NEMessage {
public:
    NEMServerIsAlive() : NEMessage(kNetMessageServerIsAlive) {}
};
class NEMStopGame : public NEMessage {
public:
    NEMStopGame() : NEMessage(kNetMessageStopGame) {}
};
class NEMPlayerConnectionCheck : public NEMessage {
public:
    NEMPlayerConnectionCheck(unsigned long inTime) : NEMessage(kNetMessagePlayerConnectionCheck)
    {
        gQ->Line("msg connectioncheck m%d %lu", this->mID, inTime);
    }
};
class NEMRemovePlayer : public NEMessage {
public:
    NEMRemovePlayer(unsigned char inPlayer, bool inWasBooted, bool inIsOOS, bool inGameIsExiting)
        : NEMessage(kNetMessageRemovePlayer)
    {
        gQ->Line("msg removeplayer m%d %d %d %d %d", this->mID, inPlayer, (int)inWasBooted, (int)inIsOOS,
                 (int)inGameIsExiting);
    }
};
class NEMClientReconnectionRequestResponse : public NEMessage {
public:
    NEMClientReconnectionRequestResponse(long inError, unsigned char inSlot, unsigned long inTick)
        : NEMessage(kNetMessageClientReconnectionRequestResponse)
    {
        gQ->Line("msg reconnectresponse m%d %ld %d %lu", this->mID, inError, inSlot, inTick);
    }
};
class NEMClientReconnectionRequest : public NEMessage {
public:
    unsigned long GetCurrentCT() const { return this->mCT; }
};
class NEMWorldDatabaseChecksumResponse : public NEMessage {
public:
    unsigned long GetCommunicationsTick() const { return this->mCT; }
    unsigned long GetWorldDatabaseChecksum() const { return this->mChecksum; }
};
