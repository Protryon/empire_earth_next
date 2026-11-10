// C:\EEX\Low-Level Engine\Network Engine\NEMessage.h (RECONSTRUCTION; NEMessage.cpp 0x3e05c..0x400f7). The network
// message base class and every built-in message. Requires UReferenceCounter, UWideString (lle/LowLevelEngine.h).
//
// Inline members are exactly the ones the engine emits as exported COMDATs in the first Network Engine unit
// (NELagLossSimulator.cpp, 0x3bc2d..0x3d5e4), in declaration order: per class the declared inlines (ctors, dtor,
// getters, GetMessageLabel), then the implicit copy ctor / operator= and the vector deleting dtor. Export/Import,
// ExportSize, IsResponse overrides and the NEMPlayerInput list live in NEMessage.cpp. Field names follow the getters.
// Not seen by the plug-ins: sConstructors (a static data member) stays engine-only.
#ifndef LLE_NEMESSAGE_H
#define LLE_NEMESSAGE_H

#include <stdint.h>   // int32_t/uint32_t: the wire fields' fixed widths

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
    virtual bool IsResponse(NEMessage* inMessage) = 0;                          // +0x24
    virtual void Execute() = 0;                                                 // +0x28
    virtual char* GetMessageLabel() const = 0;                                  // +0x2c
    unsigned char GetType() const { return this->mType; }
    unsigned char GetForSpecificSlot() const { return this->mForSpecificSlot; }
    unsigned char GetOriginatingSlot() const { return this->mOriginatingSlot; }
    void SetForSpecificSlot(unsigned char inSlot) { this->mForSpecificSlot = inSlot; }
    void SetOriginatingSlot(unsigned char inSlot) { this->mOriginatingSlot = inSlot; }

    static NEMessage* ConvertMemoryToMessage(unsigned char* inData);
    static NEMessage* BuildNEMessage(unsigned char* inData);
    static void RegisterKnownMessages();
    unsigned long ExportHeader(unsigned char* outData);
    unsigned long ImportHeader(unsigned char* inData);
    void GetHeaderSlots(EeNetMsgSlot* outSlots);   // New (port): the header as the codec's first four slots
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_CURRENT_HEADERS)   // the exe registers its game messages here (imports it; 0xb8cda)
    static NEMessage* (*sConstructors[256])(unsigned char*);   // ?sConstructors@NEMessage@@2PAP6APAV1@PAE@ZA (0x12d9cc)
#endif

protected:
    virtual ~NEMessage() {}

    unsigned char mType;                    // +0x08
    unsigned char mForSpecificSlot;         // +0x09
    unsigned char mOriginatingSlot;         // +0x0a
    unsigned char mReserved;                // +0x0b provisional name (always 0)
};

class LLE_API NEMAddPlayer : public NEMessage {   // proven sizeof 0x24 (vector deleting dtor 0x3be35)
public:
    NEMAddPlayer(unsigned char& inPlayerIndex, const UWideString& inPlayerName)
        : NEMessage(kNetMessageAddPlayer), mPlayerName(inPlayerName), mPlayerIndex(inPlayerIndex) {}
    NEMAddPlayer(unsigned char* inData) { this->ImportMessage(inData); }
    virtual ~NEMAddPlayer() {}
    unsigned char GetPlayerIndex() const { return this->mPlayerIndex; }
    const UWideString& GetPlayerName() const { return this->mPlayerName; }
    virtual char* GetMessageLabel() const { return "Add Player"; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    UWideString mPlayerName;                // +0x0c
    unsigned char mPlayerIndex;             // +0x20
};

class LLE_API NEMAddPlayerResponse : public NEMessage {
public:
    NEMAddPlayerResponse() : NEMessage(kNetMessageAddPlayerResponse) {}
    NEMAddPlayerResponse(unsigned char* inData) { this->ImportMessage(inData); }
    virtual char* GetMessageLabel() const { return "Add Player Response"; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return false; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}
};

class LLE_API NEMClientConnectionRequest : public NEMessage {   // proven sizeof 0x3c (vector deleting dtor 0x3c079)
public:
    NEMClientConnectionRequest(const UWideString& inPlayerName, const UWideString& inVersion,
                               unsigned long inBinaryChecksum, bool inIsBehindFirewall)
        : NEMessage(kNetMessageClientConnectionRequest), mPlayerName(inPlayerName)
    {
        this->mVersion = inVersion;
        this->mBinaryChecksum = inBinaryChecksum;
        this->mIsBehindFirewall = inIsBehindFirewall;
    }
    NEMClientConnectionRequest(unsigned char* inData) { this->ImportMessage(inData); }
    virtual ~NEMClientConnectionRequest() {}
    virtual char* GetMessageLabel() const { return "Client Connection Request"; }
    const UWideString& GetPlayerName() const { return this->mPlayerName; }
    const UWideString& GetVersion() const { return this->mVersion; }
    unsigned long GetBinaryChecksum() const { return this->mBinaryChecksum; }
    bool GetIsBehindFirewall() const { return this->mIsBehindFirewall; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    UWideString mPlayerName;                // +0x0c
    UWideString mVersion;                   // +0x20
    uint32_t mBinaryChecksum;               // +0x34
    bool mIsBehindFirewall;                 // +0x38
};

class LLE_API NEMClientConnectionRequestResponse : public NEMessage {   // proven sizeof 0x2c (0x3c20c)
public:
    NEMClientConnectionRequestResponse(long inError, const UWideString& inVersion, unsigned char inSendToSlot,
                                       unsigned long inCurrentCT, unsigned char inUISlotNumber)
        : NEMessage(kNetMessageClientConnectionRequestResponse), mError(inError)
    {
        this->mVersion = inVersion;
        this->mUISlotNumber = inUISlotNumber;
        this->mSendToSlot = inSendToSlot;
        this->mCurrentCT = inCurrentCT;
    }
    NEMClientConnectionRequestResponse(unsigned char* inData) { this->ImportMessage(inData); }
    virtual ~NEMClientConnectionRequestResponse() {}
    virtual char* GetMessageLabel() const { return "Client Connection Request Response"; }
    unsigned char GetSendToSlot() const { return this->mSendToSlot; }
    long GetError() const { return this->mError; }
    const UWideString& GetVersion() const { return this->mVersion; }
    unsigned long GetCurrentCT() const { return this->mCurrentCT; }
    unsigned char GetUISlotNumber() { return this->mUISlotNumber; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return false; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    int32_t mError;                         // +0x0c
    UWideString mVersion;                   // +0x10
    uint32_t mCurrentCT;                    // +0x24
    unsigned char mUISlotNumber;            // +0x28
    unsigned char mSendToSlot;              // +0x29
};

class LLE_API NEMClientDisconnectionNotification : public NEMessage {
public:
    NEMClientDisconnectionNotification(bool inIsExiting) : NEMessage(kNetMessageClientDisconnectionNotification), mIsExiting(inIsExiting) {}
    NEMClientDisconnectionNotification(unsigned char* inData) { this->ImportMessage(inData); }
    virtual char* GetMessageLabel() const { return "Client Disconnection Notification"; }
    bool GetIsExiting() const { return this->mIsExiting; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    bool mIsExiting;                        // +0x0c
};

class LLE_API NEMClientDisconnectionNotificationResponse : public NEMessage {
public:
    NEMClientDisconnectionNotificationResponse(long inError) : NEMessage(kNetMessageClientDisconnectionNotificationResponse), mError(inError) {}
    NEMClientDisconnectionNotificationResponse(unsigned char* inData) { this->ImportMessage(inData); }
    virtual char* GetMessageLabel() const { return "Client Disconnection Notification Response"; }
    long GetError() const { return this->mError; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return false; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    int32_t mError;                         // +0x0c
};

class LLE_API NEMClientReconnectionRequest : public NEMessage {
public:
    NEMClientReconnectionRequest(unsigned long inCurrentCT, unsigned long inPlayerNumber) : NEMessage(kNetMessageClientReconnectionRequest), mCurrentCT(inCurrentCT), mPlayerNumber(inPlayerNumber) {}
    NEMClientReconnectionRequest(unsigned char* inData) { this->ImportMessage(inData); }
    virtual ~NEMClientReconnectionRequest() {}
    virtual char* GetMessageLabel() const { return "Client Reconnection Request"; }
    unsigned long GetCurrentCT() const { return this->mCurrentCT; }
    unsigned long GetPlayerNumber() const { return this->mPlayerNumber; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return false; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    uint32_t mCurrentCT;                    // +0x0c
    uint32_t mPlayerNumber;                 // +0x10
};

class LLE_API NEMClientReconnectionRequestResponse : public NEMessage {
public:
    NEMClientReconnectionRequestResponse(long inError, unsigned char inNewSendToSlot, unsigned long inCurrentCT)
        : NEMessage(kNetMessageClientReconnectionRequestResponse), mError(inError), mCurrentCT(inCurrentCT), mNewSendToSlot(inNewSendToSlot) {}
    NEMClientReconnectionRequestResponse(unsigned char* inData) { this->ImportMessage(inData); }
    virtual char* GetMessageLabel() const { return "Client Reconnection Request Response"; }
    unsigned char GetNewSendToSlot() const { return this->mNewSendToSlot; }
    long GetError() const { return this->mError; }
    unsigned long GetCurrentCT() const { return this->mCurrentCT; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return false; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    int32_t mError;                         // +0x0c
    uint32_t mCurrentCT;                    // +0x10
    unsigned char mNewSendToSlot;           // +0x14
};

class LLE_API NEMClientRequestPause : public NEMessage {
public:
    NEMClientRequestPause() : NEMessage(kNetMessageClientRequestPause) {}
    NEMClientRequestPause(unsigned char* inData) { this->ImportMessage(inData); }
    virtual char* GetMessageLabel() const { return "Client Request Pause"; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}
};

class LLE_API NEMPlayerInput : public NEMessage {   // proven sizeof 0x1c: NEMessage + vector<NEMessage*> (+0xc)
public:
    NEMPlayerInput(unsigned char inType = 0xff) : NEMessage(inType) {}
    NEMPlayerInput(unsigned char* inData) { this->ImportMessage(inData); }
    virtual ~NEMPlayerInput();
    unsigned long GetNumberOfPlayerInputs() const { return this->mPlayerInputs.size(); }
    long AddPlayerInput(NEMessage* inInput);
    long GetPlayerInput(unsigned long inIndex, NEMessage*& outInput);
    // abstract: GetMessageLabel stays pure (vtable 0x984e0 slot 11 is _purecall); the overrides are protected
protected:
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

public:
    std::vector<NEMessage*> mPlayerInputs;  // +0x0c
};

class LLE_API NEMCommunicationsTick : public NEMPlayerInput {   // proven sizeof 0x24
public:
    NEMCommunicationsTick(unsigned long& inCurrentCommunicationsTick, unsigned char& inPhysicsTicksToPerform,
                          unsigned char inPhysicsTicksPerSecond, bool inGenerateChecksum, bool inDumpTheWorld)
        : NEMPlayerInput(kNetMessageCommunicationsTick), mCurrentCommunicationsTick(inCurrentCommunicationsTick), mPhysicsTicksPerSecond(inPhysicsTicksPerSecond), mPhysicsTicksToPerform(inPhysicsTicksToPerform), mGenerateChecksum(inGenerateChecksum), mDumpTheWorld(inDumpTheWorld) {}
    NEMCommunicationsTick(unsigned char* inData) { this->ImportMessage(inData); }
    virtual ~NEMCommunicationsTick();
    uint32_t& GetCurrentCommunicationsTick() { return this->mCurrentCommunicationsTick; }
    bool GetDumpTheWorld() { return this->mDumpTheWorld; }
    bool GetGenerateChecksum() { return this->mGenerateChecksum; }
    unsigned char& GetPhysicsTicksPerSecond() { return this->mPhysicsTicksPerSecond; }
    unsigned char GetPhysicsTicksToPerform() const { return this->mPhysicsTicksToPerform; }
    virtual char* GetMessageLabel() const { return "Communications Tick"; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return false; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    uint32_t mCurrentCommunicationsTick;        // +0x1c
    unsigned char mPhysicsTicksPerSecond;       // +0x20
    unsigned char mPhysicsTicksToPerform;       // +0x21
    bool mGenerateChecksum;                     // +0x22
    bool mDumpTheWorld;                         // +0x23
};

class LLE_API NEMCommunicationsTickResponse : public NEMPlayerInput {   // proven sizeof 0x28
public:
    NEMCommunicationsTickResponse(unsigned long inLastCommunicationsTickProcessed,
                                  unsigned long inServersCommunicationTick, unsigned short inAveragePhysicsTickTime)
        : NEMPlayerInput(kNetMessageCommunicationsTickResponse), mLastCommunicationsTickProcessed(inLastCommunicationsTickProcessed), mServersCommunicationTick(inServersCommunicationTick), mAveragePhysicsTickTime(inAveragePhysicsTickTime) {}
    NEMCommunicationsTickResponse(unsigned char* inData) { this->ImportMessage(inData); }
    unsigned short GetAveragePhysicsTickTime() const { return this->mAveragePhysicsTickTime; }
    unsigned long GetLastCommunicationsTickProcessed() const { return this->mLastCommunicationsTickProcessed; }
    virtual char* GetMessageLabel() const { return "Communications Tick Response"; }
    unsigned long GetServersCommunicationTick() const { return this->mServersCommunicationTick; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return false; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    uint32_t mLastCommunicationsTickProcessed;        // +0x1c
    uint32_t mServersCommunicationTick;               // +0x20
    unsigned short mAveragePhysicsTickTime;           // +0x24
};

class LLE_API NEMPauseGame : public NEMessage {
public:
    NEMPauseGame(unsigned long inUniqueID) : NEMessage(kNetMessagePauseGame), mUniqueID(inUniqueID) {}
    NEMPauseGame(unsigned char* inData) { this->ImportMessage(inData); }
    virtual char* GetMessageLabel() const { return "Pause Game"; }
    virtual bool GetSendToAllAndWaitMessageID(unsigned long& outID) const
        { outID = this->mSendToAllAndWaitMessageID; return true; }
    virtual void SetSendToAllAndWaitMessageID(unsigned long inID) { this->mSendToAllAndWaitMessageID = inID; }
    unsigned long GetUniqueID() const { return this->mUniqueID; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    uint32_t mSendToAllAndWaitMessageID;        // +0x0c
    uint32_t mUniqueID;                         // +0x10
};

class LLE_API NEMPauseGameResponse : public NEMessage {
public:
    NEMPauseGameResponse(unsigned long inUniqueID) : NEMessage(kNetMessagePauseGameResponse), mUniqueID(inUniqueID) {}
    NEMPauseGameResponse(unsigned char* inData) { this->ImportMessage(inData); }
    virtual char* GetMessageLabel() const { return "Pause Game Response"; }
    unsigned long GetUniqueID() const { return this->mUniqueID; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return false; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    uint32_t mUniqueID;                     // +0x0c
};

class LLE_API NEMPhysicsTick : public NEMessage {
public:
    NEMPhysicsTick(unsigned char& inPhysicsTicksToPerform, unsigned long& inNumberOfPlayerInputs,
                   unsigned char& inPhysicsTicksPerSecond, bool inGenerateChecksum, bool inDumpTheWorld)
        : NEMessage(kNetMessagePhysicsTick), mNumberOfPlayerInputs(inNumberOfPlayerInputs), mPhysicsTicksToPerform(inPhysicsTicksToPerform), mPhysicsTicksPerSecond(inPhysicsTicksPerSecond), mGenerateChecksum(inGenerateChecksum), mDumpTheWorld(inDumpTheWorld) {}
    NEMPhysicsTick(unsigned char* inData) { this->ImportMessage(inData); }
    unsigned long GetNumberOfPlayerInputs() const { return this->mNumberOfPlayerInputs; }
    bool GetDumpTheWorld() { return this->mDumpTheWorld; }
    virtual char* GetMessageLabel() const { return "Physics Tick"; }
    unsigned char GetPhysicsTicksPerSecond() const { return this->mPhysicsTicksPerSecond; }
    bool GetGenerateChecksum() { return this->mGenerateChecksum; }
    unsigned char GetPhysicsTicksToPerform() const { return this->mPhysicsTicksToPerform; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return false; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    uint32_t mNumberOfPlayerInputs;         // +0x0c
    unsigned char mPhysicsTicksToPerform;   // +0x10
    unsigned char mPhysicsTicksPerSecond;   // +0x11
    bool mGenerateChecksum;                 // +0x12
    bool mDumpTheWorld;                     // +0x13
};

class LLE_API NEMPlayerConnectionCheck : public NEMessage {
public:
    NEMPlayerConnectionCheck(unsigned long inPingTimeStart) : NEMessage(kNetMessagePlayerConnectionCheck), mPingTimeStart(inPingTimeStart) {}
    NEMPlayerConnectionCheck(unsigned char* inData) { this->ImportMessage(inData); }
    virtual char* GetMessageLabel() const { return "Player Connection Check"; }
    unsigned long GetPingTimeStart() { return this->mPingTimeStart; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return false; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    uint32_t mPingTimeStart;                // +0x0c
};

class LLE_API NEMPlayerConnectionCheckResponse : public NEMessage {
public:
    NEMPlayerConnectionCheckResponse(unsigned long inPingTimeStart) : NEMessage(kNetMessagePlayerConnectionCheckResponse), mPingTimeStart(inPingTimeStart) {}
    NEMPlayerConnectionCheckResponse(unsigned char* inData) { this->ImportMessage(inData); }
    virtual char* GetMessageLabel() const { return "Player Connection Check Response"; }
    unsigned long GetPingTimeStart() { return this->mPingTimeStart; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return false; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    uint32_t mPingTimeStart;                // +0x0c
};

class LLE_API NEMRemovePlayer : public NEMessage {
public:
    NEMRemovePlayer(unsigned char& inPlayerIndex, bool inWasBooted, bool inPlayerIsOOS, bool inGameIsExiting)
        : NEMessage(kNetMessageRemovePlayer), mPlayerIndex(inPlayerIndex), mPlayerIsOOS(inPlayerIsOOS), mGameIsExiting(inGameIsExiting), mWasBooted(inWasBooted) {}
    NEMRemovePlayer(unsigned char* inData) { this->ImportMessage(inData); }
    bool GetGameIsExiting() const { return this->mGameIsExiting; }
    unsigned char GetPlayerIndex() const { return this->mPlayerIndex; }
    bool GetPlayerIsOOS() const { return this->mPlayerIsOOS; }
    bool GetWasBooted() const { return this->mWasBooted; }
    virtual char* GetMessageLabel() const { return "Remove Player"; }
    virtual bool GetCanSendWhilePaused() const { return true; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return false; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    unsigned char mPlayerIndex;             // +0x0c
    bool mPlayerIsOOS;                      // +0x0d
    bool mGameIsExiting;                    // +0x0e
    bool mWasBooted;                        // +0x0f
};

class LLE_API NEMServerIsAlive : public NEMessage {
public:
    NEMServerIsAlive() : NEMessage(kNetMessageServerIsAlive) {}
    NEMServerIsAlive(unsigned char* inData) { this->ImportMessage(inData); }
    virtual char* GetMessageLabel() const { return "Server Is Alive"; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return false; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}
};

class LLE_API NEMStartGame : public NEMessage {
public:
    NEMStartGame(unsigned long& inOffsetStartTime) : NEMessage(kNetMessageStartGame), mOffsetStartTime(inOffsetStartTime) {}
    NEMStartGame(unsigned char* inData) { this->ImportMessage(inData); }
    virtual char* GetMessageLabel() const { return "Start Game"; }
    unsigned long GetOffsetStartTime() const { return this->mOffsetStartTime; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return false; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    uint32_t mOffsetStartTime;              // +0x0c
};

class LLE_API NEMStartGameResponse : public NEMessage {
public:
    NEMStartGameResponse(unsigned long& inApplicationTimeAtStart) : NEMessage(kNetMessageStartGameResponse), mApplicationTimeAtStart(inApplicationTimeAtStart) {}
    NEMStartGameResponse(unsigned char* inData) { this->ImportMessage(inData); }
    virtual char* GetMessageLabel() const { return "Start Game Response"; }
    unsigned long GetApplicationTimeAtStart() const { return this->mApplicationTimeAtStart; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return false; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    uint32_t mApplicationTimeAtStart;       // +0x0c
};

class LLE_API NEMStopGame : public NEMessage {
public:
    NEMStopGame() : NEMessage(kNetMessageStopGame) {}
    NEMStopGame(unsigned char* inData) { this->ImportMessage(inData); }
    virtual char* GetMessageLabel() const { return "Stop Game"; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}
};

class LLE_API NEMStopGameResponse : public NEMessage {
public:
    NEMStopGameResponse() : NEMessage(kNetMessageStopGameResponse) {}
    NEMStopGameResponse(unsigned char* inData) { this->ImportMessage(inData); }
    virtual char* GetMessageLabel() const { return "Stop Game Response"; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return false; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}
};

class LLE_API NEMUnPauseGame : public NEMessage {
public:
    NEMUnPauseGame(unsigned long inUniqueID) : NEMessage(kNetMessageUnPauseGame), mUniqueID(inUniqueID) {}
    NEMUnPauseGame(unsigned char* inData) { this->ImportMessage(inData); }
    virtual char* GetMessageLabel() const { return "Un-Pause Game"; }
    virtual bool GetSendToAllAndWaitMessageID(unsigned long& outID) const
        { outID = this->mSendToAllAndWaitMessageID; return true; }
    virtual void SetSendToAllAndWaitMessageID(unsigned long inID) { this->mSendToAllAndWaitMessageID = inID; }
    unsigned long GetUniqueID() const { return this->mUniqueID; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    uint32_t mSendToAllAndWaitMessageID;        // +0x0c
    uint32_t mUniqueID;                         // +0x10
};

class LLE_API NEMUnPauseGameResponse : public NEMessage {
public:
    NEMUnPauseGameResponse(unsigned long inUniqueID) : NEMessage(kNetMessageUnPauseGameResponse), mUniqueID(inUniqueID) {}
    NEMUnPauseGameResponse(unsigned char* inData) { this->ImportMessage(inData); }
    virtual char* GetMessageLabel() const { return "Un-Pause Game Response"; }
    unsigned long GetUniqueID() const { return this->mUniqueID; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return false; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    uint32_t mUniqueID;                     // +0x0c
};

class LLE_API NEMWorldDatabaseChecksum : public NEMessage {
public:
    NEMWorldDatabaseChecksum(unsigned long& inPhysicsTickTrigger, bool inDumpWorld) : NEMessage(kNetMessageWorldDatabaseChecksum), mPhysicsTickTrigger(inPhysicsTickTrigger), mDumpWorld(inDumpWorld) {}
    NEMWorldDatabaseChecksum(unsigned char* inData) { this->ImportMessage(inData); }
    virtual char* GetMessageLabel() const { return "World Database Checksum"; }
    unsigned long GetPhysicsTickTrigger() const { return this->mPhysicsTickTrigger; }
    bool GetDumpWorld() { return this->mDumpWorld; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return false; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    uint32_t mPhysicsTickTrigger;           // +0x0c
    bool mDumpWorld;                        // +0x10
};

class LLE_API NEMWorldDatabaseChecksumResponse : public NEMessage {
public:
    NEMWorldDatabaseChecksumResponse(unsigned long& inCommunicationsTick, unsigned long& inWorldDatabaseChecksum)
        : NEMessage(kNetMessageWorldDatabaseChecksumResponse), mCommunicationsTick(inCommunicationsTick), mWorldDatabaseChecksum(inWorldDatabaseChecksum) {}
    NEMWorldDatabaseChecksumResponse(unsigned char* inData) { this->ImportMessage(inData); }
    virtual char* GetMessageLabel() const { return "World Database Checksum Response"; }
    uint32_t& GetCommunicationsTick() { return this->mCommunicationsTick; }
    unsigned long GetWorldDatabaseChecksum() const { return this->mWorldDatabaseChecksum; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    uint32_t mCommunicationsTick;           // +0x0c
    uint32_t mWorldDatabaseChecksum;        // +0x10
};

class LLE_API NEMXferMessage : public NEMessage {   // proven sizeof 0x414
public:
    NEMXferMessage(unsigned char& inID, unsigned char* inData, unsigned long& inDataSize)
        : NEMessage(kNetMessageXferMessage), mDataSize(inDataSize), mID(inID)
    {
        memset(this->mData, 0, sizeof(this->mData));
        if (inDataSize > 0)
            memcpy(this->mData, inData, inDataSize);
    }
    NEMXferMessage(unsigned char* inData) { this->ImportMessage(inData); }
    unsigned char* GetData() const { return (unsigned char*)this->mData; }
    unsigned long GetDataSize() const { return this->mDataSize; }
    unsigned char GetID() const { return this->mID; }
    static unsigned long StaticExportSize();
    virtual char* GetMessageLabel() const { return "Xfer Message"; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return false; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    unsigned char mData[0x400];             // +0x0c
    uint32_t mDataSize;                     // +0x40c
    unsigned char mID;                      // +0x410
};

class LLE_API NEMXferStatus : public NEMessage {
public:
    NEMXferStatus(unsigned char& inClientSlot, unsigned char& inPercentDone, uint32_t& inXferID,
                  unsigned char& inStatusID) : NEMessage(kNetMessageXferStatus), mXferID(inXferID), mStatusID(inStatusID), mClientSlot(inClientSlot), mPercentDone(inPercentDone) {}
    NEMXferStatus(unsigned char* inData) { this->ImportMessage(inData); }
#ifdef LLE_EXE_HEADERS
    // user-declared: the exe's ??_G 0xccfbb stores NEMXferStatus's vptr first (the DLL exports
    // ??1NEMXferStatus); exe view only until the engine side is checked
    virtual ~NEMXferStatus() {}
#endif
    unsigned char GetClientSlot() const { return this->mClientSlot; }
    unsigned char GetPercentDone() const { return this->mPercentDone; }
    virtual char* GetMessageLabel() const { return "Xfer Status"; }
    unsigned char GetStatusID() const { return this->mStatusID; }
    unsigned long GetXferID() const { return this->mXferID; }
    virtual bool GetCountsTowardsPlayerInputThrottle() const { return false; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute() {}

    uint32_t mXferID;                       // +0x0c
    unsigned char mStatusID;                // +0x10
    unsigned char mClientSlot;              // +0x11
    unsigned char mPercentDone;             // +0x12
};

#endif
