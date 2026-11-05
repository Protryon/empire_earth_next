// C:\EEX\Low-Level Engine\Network Engine\NESynchronousServer.h (RECONSTRUCTION; NESynchronousServer.cpp 0x47400..0x4b04f).
// The lock-step multiplayer server: three threads (main: incoming player inputs and responses; send: the
// communications tick; xfer: file transfers) over one published NEServer, plus an NEMessageManager that tracks which
// client has answered which message. Abstract: the game implements the pure virtual callbacks.
// Inline members are exported COMDATs emitted by NEMessageManager.cpp (0x40369..0x4058a, then the implicit copy
// ctor 0x40591, operator= 0x40ab6 and ??_E 0x4103b). Layout proven by the implicit copy ctor (member types in order)
// and the ctor 0x47400 (event names from its ESVERIFY strings). Unnamed fields are provisional (named by offset).
#ifndef LLE_NESYNCHRONOUSSERVER_H
#define LLE_NESYNCHRONOUSSERVER_H

#include <deque>
#include <map>
#include <vector>
#include "lle/NESynchronousCommunications.h"
#include "lle/NEProtocol.h"

class NEMessage;
class NEMessageManager;
class NEMCommunicationsTickResponse;
class NEMWorldDatabaseChecksumResponse;
class NERecordPlayback;
class NEServer;
class FSFileSpec;
class TSEvent;
class TSThread;

enum {                                      // slot values (ctor ESVERIFY text "inMaxPlayers < knessPlayerSlotNotAvailable+1")
    kSlotNotAvailable = 0xff,      // Provisional enum name; 0xfe is the server's own slot
    knessPlayerSlotNotAvailable = kSlotNotAvailable   // spelled this way in the original's assert strings
};

struct NECDIServerClient {                  // value of NECDIServer's map (pair 0x20 bytes, POD: _Copy uses rep movsd)
    unsigned long mU00[7];                  // (provisional name)
};

struct NECDIServer {                        // server debug info (GetDebugInfo: this + 0x30), sizeof 0x40; (provisional name)
    NECDIServer();                          // 0x47c20, out of line in NESynchronousServer.cpp (zeroes the counters)

    std::map<unsigned long, NECDIServerClient> mClients;    // +0x00 (node 0x30)
    unsigned long mU0C;                     // +0x0c
    unsigned long mU10;
    unsigned long mU14;
    unsigned long mU18;
    unsigned long mU1C;
    unsigned long mU20;
    unsigned long mU24;
    unsigned long mU28;
    unsigned long mU2C;
    unsigned long mU30;
    unsigned long mU34;
    unsigned long mU38;
    unsigned long mU3C;                     // +0x3c
};

struct NESendToAllAndWaitRequest {          // element of mSendToAllAndWaitRequests (16 bytes, POD)
    NEMessage* mMessage;                    // +0x00
    unsigned long mTimeout;                 // +0x04 (provisional name)
    unsigned long mRetries;                 // +0x08 (provisional name)
    unsigned long mU0C;                     // +0x0c (provisional name)
};

class LLE_API NESynchronousServer : public NESynchronousCommunications {   // proven sizeof 0x208 (??_E: imul 0x208)
    friend class NEMessageManager;
public:
    NESynchronousServer(unsigned char inMaxPlayers, unsigned long inDefaultSendInterval, unsigned char inPTsPerCT, unsigned long inMaxPhysicsTicksPerSecond,
                        unsigned long inMinPhysicsTicksPerSecond, float inPhysicsTickCostFactor, unsigned long inMaxSendInterval, unsigned long inSendIntervalIncrement, unsigned long inSendIntervalDecrement,
                        unsigned long inCurrentCommunicationsTick, unsigned long inCurrentPhysicsTick,
                        UWideString& inName, bool inAllowNewConnections, bool inAnnounce, unsigned long inNetworkAdapter);
    virtual ~NESynchronousServer();

    void AllowNewPlayers(bool inAllow)
    {
        if (inAllow == true)
            this->mGameState = 0;
        if (inAllow == false)
            this->mGameState = 1;
        this->mProtocol->AllowNewConnections(this->mServer, inAllow);
    }
    void ClearSendToSlotArray() { this->mSendToSlotArray.clear(); }
    long GetClientAddress(unsigned long inIndex, UWideString& outAddress)
        { return this->mProtocol->GetClientAddress(this->mServer, inIndex, outAddress); }
    virtual unsigned long GetCurrentCommunicationsTick() const { return this->mCurrentCommunicationsTick; }
    unsigned long GetCurrentPhysicsTick() const { return this->mCurrentPhysicsTick; }
    const NECDIServer& GetDebugInfo() const { return this->mDebugInfo; }
    unsigned char GetGameState() const { return this->mGameState; }
    bool GetGameWasPublished() const { return this->mGameWasPublished; }
    bool GetIsOOS() const { return this->mIsOOS; }
    NERecordPlayback* GetRecordPlayback() const { return this->mRecordPlayback; }
    unsigned long GetLastCTMessageID() const { return this->mLastCTMessageID; }
    unsigned long GetMaxPlayers() const { return this->mMaxPlayers; }
    unsigned char GetNumberOfPlayers() const { return this->mNumberOfPlayers; }
    bool GetProcessingSendToAllAndWait() const { return this->mProcessingSendToAllAndWait; }
    unsigned long GetPTsPerCT() const { return this->mPTsPerCT; }
    unsigned long GetRestartCT() const { return this->mRestartCT; }
    unsigned long GetSendInterval() const { return this->mSendInterval; }
    void LockSendCommunicationsTick() { this->mSendCommunicationsTickLock->Enter(); }
    virtual long ProcessGuaranteedLocalUserInput(NEMessage* inMessage, unsigned char inPlayer)
        { return this->ProcessLocalUserInput(inMessage, inPlayer); }
    void ResetClientConnection(unsigned long inIndex) { this->mProtocol->ResetClientConnection(this->mServer, inIndex); }
    void ResetXfer() { this->mProtocol->ResetXfer(this->mServer); }
    void ResumeMigratedGame() { this->mResumeMigratedGame->Set(); }
    void SendExitMessage()
    {
        this->mSendThreadExitEvent->Set();
        this->mSendThreadFinishedEvent->Wait(kTSWaitInfinite);
        this->mMainThreadExitEvent->Set();
        this->mMainThreadFinishedEvent->Wait(kTSWaitInfinite);
    }
    virtual void SetCurrentCommunicationsTick(unsigned long inTick) { this->mCurrentCommunicationsTick = inTick; }
    void SetCurrentPhysicsTick(unsigned long& inTick) { this->mCurrentPhysicsTick = inTick; }
    void SetGameState(unsigned char inState) { this->mGameState = inState; }
    void SetLastCTMessageID(unsigned long inID) { this->mLastCTMessageID = inID; }
    void SetLoadParams(unsigned long inTick) { this->mCurrentPhysicsTick = inTick; }
    void SetRestartCT(unsigned long inCT) { this->mRestartCT = inCT; }
    void SetSendInterval(unsigned long inInterval)
    {
        this->mSendInterval = inInterval;
        this->mChangeSendIntervalEvent->Set();
    }
    void SetSendToSlotForPlayer(unsigned char inPlayer, unsigned char inSlot)
        { this->mSendToSlotArray.insert(std::map<unsigned char, unsigned char>::value_type(inPlayer, inSlot)); }
    void StartGame() { this->mStartGameEvent->Set(); }
    void StartPing() { this->mPing = true; }
    void StartWaitingForReconnects(unsigned long inTime)
    {
        this->mWaitForReconnectsTime = inTime;
        this->mWaitForReconnectsEvent->Set();
    }
    void StopGame() { this->mStopGameEvent->Set(); }
    void StopPing() { this->mPing = false; }
    void UnlockSendCommunicationsTick() { this->mSendCommunicationsTickLock->Leave(); }
    void SetTempOutOfSynchPTs(unsigned char inPTs) { this->mTempOutOfSynchPTs = inPTs; }

    void AbortXfer(unsigned long inXferID);
    unsigned long GetMessageManagerCurrentMessageNumber();
    unsigned char GetSendToSlotForPlayer(unsigned char inPlayer);
    void LogChecksumResponse(NEMWorldDatabaseChecksumResponse* inMessage);
    long LogResponse(NEMessage* inMessage);
    void PauseGame();
    virtual long ProcessLocalUserInput(NEMessage* inMessage, unsigned char inPlayer);
    void RegisterSlot(unsigned char inSlot);
    void RemovePlayerFromGameInProgress(unsigned char inPlayer, bool inWasBooted, bool inPlayerIsOOS, bool inGameIsExiting);
    virtual long SearchForGame(bool inStart);
    unsigned long SendToAllAndWait(NEMessage* inMessage, unsigned long inTimeout, unsigned long inRetries);
    void SetMessageManagerLowestCTs(unsigned long inCT);
    void SetMessageManagerCurrentMessageNumber(unsigned long inNumber);
    void UnpauseGame();
    long XferFileToClient(FSFileSpec& inFile, unsigned long inUserData, unsigned char inSlot, unsigned long& outXferID,
                          bool inP5);

    // vtable 0x9891c slots 9..27: the server's own virtuals. StartThreads/StopThreads are implemented; the rest are
    // pure (the game implements them). Names and signatures inferred (slots by offset).
    virtual void BootPlayer(unsigned char inSlot, bool inWasBooted, bool inGameIsExiting) = 0;  // +0x24 (boot a player)
    virtual bool ServerSlot28() = 0;                                            // +0x28
    virtual bool IsMigrating() = 0;                                             // +0x2c
    virtual void BeforeRemovingPlayers() = 0;                                   // +0x30
    virtual void HandleReconnectWaitTimedOut() = 0;                             // +0x34
    virtual void HandleXferStatus(unsigned char& inSlot, unsigned char& inPercentDone, unsigned long& inXferID,
                              unsigned char& inStatusID) = 0;                    // +0x38 (xfer status)
    virtual void StartThreads();                                                // +0x3c
    virtual void StopThreads();                                                 // +0x40
#ifdef LLE_EXE_HEADERS
    // the game's overrides (EEServer 0xcc2e7 `ret 4`, 0xcbb17 returns a slot, 0xcd411 a byte flag, 0xcbb6c a name)
    virtual void PlayWarningSound(long inSound) = 0;                            // +0x44
#else
    virtual void PlayWarningSound() = 0;                                        // +0x44
#endif
    virtual void ServerSlot48() = 0;                                            // +0x48
#ifdef LLE_EXE_HEADERS
    virtual unsigned char GetFreePlayerSlot() = 0;                              // +0x4c
    virtual bool GetDebugOOSHandling() = 0;                                     // +0x50
    virtual const UWideString& ServerSlot54(unsigned char inPlayer) = 0;       // +0x54
#else
    virtual void GetFreePlayerSlot() = 0;                                       // +0x4c
    virtual void GetDebugOOSHandling() = 0;                                     // +0x50
    virtual void ServerSlot54() = 0;                                            // +0x54
#endif
    virtual unsigned char GetPlayerSendToSlot(unsigned char inPlayer) = 0;      // +0x58 (send-to slot of a player)
    virtual bool FilterPlayerInput(NEMessage* inMessage) = 0;                   // +0x5c (true: queue the input)
    virtual void PrepareLocalInput(NEMessage* inMessage) = 0;                   // +0x60
    virtual bool IsPlayerPresent(unsigned char inPlayer) = 0;                   // +0x64 (player present)
    virtual void ReportLaggers(std::vector<unsigned char>& inSlots, bool inFlag) = 0;  // +0x68
    virtual void ProcessSystemMessage(NEMessage* inMessage) = 0;                // +0x6c (unhandled messages)

protected:
    long BroadcastMessage(NEMessage* inMessage);
    void ForceNextCT() { this->mSendThreadForceCTEvent->Set(); }
    long SendMessageTo(unsigned char inSlot, NEMessage* inMessage, bool inGuaranteed);
    long SendMessageToAll(NEMessage* inMessage, bool inGuaranteed);
    void UnregisterSlot(unsigned char inSlot);

private:
    void BootAllLaggers(unsigned long inTime);
    void BootTheSlowestClientCPU();
    void BootTheSlowestResponder();
    unsigned long GetApplicationStartTime() const { return this->mApplicationStartTime; }
    unsigned long GetGameStartTime() const { return this->mGameStartTime; }
    unsigned char GetSlowestClientCPU();
    static unsigned long Main(void* inServerObject);
    void ProcessCommunicationsTickResponse(NEMCommunicationsTickResponse* inMessage);
    long ProcessLocalUserInput2(NEMessage* inMessage);
    void ProcessPendingPlayerInputs();
    void ProcessPendingPlayerInputs2(NEMessage* inMessage);
    static void RecordPlaybackCallback(NEMessage* inMessage, void* inThisPointer);
    void RemovePlayer(unsigned char inPlayer, bool inA);
    static unsigned long Send(void* inServerObject);
    void SendCommunicationsTick(bool inGenerateChecksum, bool inB);
    bool TuneForSlowCPU();
    bool TuneForSlowNetwork();
    bool TuneServer();
    static unsigned long Xfer(void* inServerObject);

    NECDIServer mDebugInfo;                 // +0x30
    UWideString mName;                      // +0x70
    std::deque<NEMessage*> mIncomingPlayerInputs;   // +0x84 (mIncomingPlayerInputsLock)
    std::vector<NESendToAllAndWaitRequest> mSendToAllAndWaitRequests;   // +0x98
    std::map<unsigned char, unsigned char> mSendToSlotArray;            // +0xa8
    std::map<unsigned long, std::multimap<unsigned long, unsigned char> > mChecksumResponses;   // +0xb4
    std::vector<unsigned char> mOOSSlots;   // +0xc0
    std::vector<unsigned char> mSlotsD0;    // +0xd0 (provisional name; TuneServer hands it to slot +0x68)
    NEMessageManager* mMessageManager;      // +0xe0
    NEProtocol* mProtocol;                  // +0xe4 protocol 0
    NEServer* mServer;                      // +0xe8
    TSThread* mMainThread;                  // +0xec
    TSThread* mSendThread;                  // +0xf0
    TSThread* mXferThread;                  // +0xf4
    TSEvent* mChangeSendIntervalEvent;      // +0xf8
    TSEvent* mInputAvailableEvent;          // +0xfc
    TSEvent* mXferInputAvailableEvent;      // +0x100
    TSEvent* mXferAcceptEvent;              // +0x104
    TSEvent* mXferStuffWaitingToGoEvent;    // +0x108
    TSEvent* mXferThreadStartedEvent;       // +0x10c
    TSEvent* mXferThreadExitEvent;          // +0x110
    TSEvent* mXferThreadFinishedEvent;      // +0x114
    TSEvent* mMainThreadExitEvent;          // +0x118
    TSEvent* mMainThreadStartedEvent;       // +0x11c
    TSEvent* mMainThreadFinishedEvent;      // +0x120
    TSEvent* mPauseGameEvent;               // +0x124
    TSEvent* mUnpauseGameEvent;             // +0x128
    TSEvent* mResumeMigratedGame;           // +0x12c
    TSEvent* mSendThreadExitEvent;          // +0x130
    TSEvent* mSendThreadStartedEvent;       // +0x134
    TSEvent* mSendThreadFinishedEvent;      // +0x138
    TSEvent* mSendThreadForceCTEvent;       // +0x13c
    TSEvent* mStartGameEvent;               // +0x140
    TSEvent* mStopGameEvent;                // +0x144
    TSEvent* mWaitForResponseEvent;         // +0x148
    TSEvent* mWaitForReconnectsEvent;       // +0x14c
    TSEvent* mStartPingEvent;               // +0x150
    TSEvent* mStopPingEvent;                // +0x154
    TSCriticalSection* mChecksumResponsesLock;  // +0x158 provisional name (LogChecksumResponse)
    TSCriticalSection* mIncomingPlayerInputsLock;   // +0x15c
    TSCriticalSection* mPauseUnpauseLock;   // +0x160
    TSCriticalSection* mSendToAllLock;      // +0x164
    TSCriticalSection* mSendCommunicationsTickLock; // +0x168
    NERecordPlayback* mRecordPlayback;      // +0x16c
    float mPhysicsTickCostFactor;           // +0x170
    unsigned char mGameState;               // +0x174 (0 open, 1 closed, 3 playing, 4 ...)
    unsigned long mCurrentPhysicsTick;      // +0x178
    unsigned long mU17C;                    // +0x17c (100)
    unsigned long mChecksumInterval;        // +0x180 (1)
    unsigned long mNextChecksumCT;          // +0x184 (40)
    unsigned long mRestartCT;               // +0x188
    unsigned long mApplicationStartTime;    // +0x18c
    unsigned long mCurrentCommunicationsTick;   // +0x190
    unsigned long mGameStartTime;           // +0x194
    unsigned long mSendInterval;            // +0x198
    unsigned long mDefaultSendInterval;     // +0x19c
    unsigned long mLastCTMessageID;         // +0x1a0
    unsigned long mLastPingTime;            // +0x1a4
    unsigned long mSendToAllAndWaitMessageNumber;   // +0x1a8
    unsigned long mSendToAllAndWaitTimeout; // +0x1ac (3000)
    unsigned long mSendToAllAndWaitRetries; // +0x1b0 (40)
    unsigned long mMaxPhysicsTicksPerSecond;  // +0x1b4
    unsigned long mMinPhysicsTicksPerSecond;  // +0x1b8
    unsigned long mSlowCPUCount;            // +0x1bc
    unsigned long mWaitForReconnectsTime;   // +0x1c0
    unsigned long mMaxSendInterval;         // +0x1c4
    unsigned long mSendIntervalIncrement;   // +0x1c8
    unsigned long mSendIntervalDecrement;   // +0x1cc
    unsigned long mU1D0;                    // +0x1d0 (1)
    unsigned long mNetTuneState;
    unsigned long mNetConsecutiveLate;
    unsigned long mNetResendCount;
    unsigned long mNetLateEpisodes;
    unsigned long mNetAtMaxCount;
    unsigned long mHighestResponseTime;
    unsigned long mNetLateCount;
    unsigned long mReconnectCount;          // +0x1f0
    unsigned long mLowestReconnectCT;       // +0x1f4 (-1)
    unsigned char mSendToAllAndWaitType;    // +0x1f8
    unsigned char mPTsPerCT;                // +0x1f9
    unsigned char mInitialPTsPerCT;         // +0x1fa
    unsigned char mPTsPerSecond;            // +0x1fb
    unsigned char mMaxPlayers;              // +0x1fc
    unsigned char mNumberOfPlayers;         // +0x1fd
    unsigned char mTempOutOfSynchPTs;       // +0x1fe
    bool mAllowNewConnections;              // +0x1ff
    bool mB200;                             // +0x200
    bool mGameWasPublished;                 // +0x201
    bool mPing;                             // +0x202
    bool mIsOOS;                            // +0x203
    bool mLagReportActive;                  // +0x204
    bool mB205;                             // +0x205 (true)
    bool mProcessingSendToAllAndWait;       // +0x206
    bool mLagWarning;                       // +0x207
};

// NESynchronousServer::mGameState (GetGameState); 2 is compared by EEServer only and stays unnamed
enum NEServerGameState {
    kServerStateOpen = 0,   // setrilo ne_synchronous.h gameState_ (0 lobby, 1 closed, 3 running, 4 paused); AllowNewPlayers(true); Main start
    kServerStateClosed = 1,  // setrilo ne_synchronous.h gameState_ (0 lobby, 1 closed, 3 running, 4 paused); AllowNewPlayers(false)
    kServerStateRunning = 3,  // setrilo ne_synchronous.h gameState_ (0 lobby, 1 closed, 3 running, 4 paused); Main: mStartGameEvent
    kServerStatePaused = 4,  // setrilo ne_synchronous.h gameState_ (0 lobby, 1 closed, 3 running, 4 paused); Main: mPauseGameEvent
};

#endif
