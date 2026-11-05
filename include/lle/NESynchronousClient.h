// C:\EEX\Low-Level Engine\Network Engine\NESynchronousClient.h (RECONSTRUCTION; NESynchronousClient.cpp 0x45c00..0x47301).
// The lock-step multiplayer client: a main thread (connect retries, outgoing/incoming message dispatch) and a receive
// thread over one NEConnection, plus an xfer connection for file transfer; messages pass through the lag/loss
// simulator and the record/playback stage. Abstract: the game implements the pure virtual callbacks.
// Inline members are exported COMDATs emitted by NESynchronousClient.cpp's own inline block (0x45c00..0x45d1c);
// several getters are ICF-folded with other classes' (0x403c8, 0x5a058, 0x5a11f, 0x5a126, 0x61137).
#ifndef LLE_NESYNCHRONOUSCLIENT_H
#define LLE_NESYNCHRONOUSCLIENT_H

#include <deque>
#include "lle/NESynchronousCommunications.h"
#include "lle/NEProtocol.h"
#include "lle/NEConnection.h"
#include "lle/NERecordPlayback.h"

class FSFile;
class NELagLossSimulator;
class NEMCommunicationsTick;
class NEMessage;
class TSEvent;
class TSThread;

struct NECDIClient {                        // client debug info (GetDebugInfo: this + 0x30); layout inferred
    NECDIClient() : mU00(0) {}              // the client ctor zeroes +0x30 before its vptr store
    unsigned long mU00;
};

class LLE_API NESynchronousClient : public NESynchronousCommunications {   // proven sizeof 0xdc (??_E: imul 0xdc)
public:
    NESynchronousClient();
    virtual ~NESynchronousClient();

    void ClearIncomingMessages()
    {
        this->mIncomingMessagesLock->Enter();
        while (this->mIncomingMessages.size() > 0) {
            NEMessage* theMessage = this->mIncomingMessages.front();
            this->mIncomingMessages.pop_front();
            theMessage->Dereference();
        }
        this->mIncomingMessagesLock->Leave();
    }
    NEConnection* GetConnection() const { return this->mConnection; }
    virtual unsigned long GetCurrentCommunicationsTick() const { return this->mCurrentCommunicationsTick; }
    unsigned long GetCurrentPhysicsTick() const { return this->mCurrentPhysicsTick; }
    const NECDIClient& GetDebugInfo() const { return this->mDebugInfo; }
    unsigned long GetDetectedGameCount() const { return this->mProtocol->GetDetectedGameCount(); }
    NELagLossSimulator* GetLagLossSimulator() const { return this->mLagLossSimulator; }
    unsigned char GetMySendToSlot() const { return this->mMySendToSlot; }
    unsigned long GetNumConnectionTrys() const { return this->mNumConnectionTrys; }
    NERecordPlayback* GetRecordPlayback() const { return this->mRecordPlayback; }
    void SendExitMessage()
    {
        this->mReceiveThreadExitEvent->Set();
        this->mReceiveThreadFinishedEvent->Wait(kTSWaitInfinite);
        this->mMainThreadExitEvent->Set();
        this->mMainThreadFinishedEvent->Wait(kTSWaitInfinite);
    }
    void SendStartConnectMessage(NEMessage* inMessage)
    {
        this->mMainThreadStartConnectEvent->Set();
        this->SetConnectionState(1);
        this->mStartConnectMessage = inMessage;
        inMessage->Reference();
    }
    void SendEndConnectMessage() { this->mMainThreadEndConnectEvent->Set(); }
    virtual void SetCurrentCommunicationsTick(unsigned long inTick) { this->mCurrentCommunicationsTick = inTick; }
    void SetCurrentPhysicsTick(unsigned long& inTick) { this->mCurrentPhysicsTick = inTick; }
    void SetMaxNumConnectionTrys(unsigned long inMax) { this->mMaxNumConnectionTrys = inMax; }
    void SetMySendToSlot(unsigned char inSlot) { this->mMySendToSlot = inSlot; }
    long XferReceive(unsigned long inSize, unsigned char* outBuffer, unsigned long& outReceived)
        { return this->mProtocol->XferReceive(inSize, outBuffer, outReceived); }

    long Connect(const UString& inName, const UString& inHost, const UWideString& inAddress);
    long ConnectForXfer();
    long Disconnect();
    long DisconnectFromXfer();
    long GetDetectedGame(unsigned long inIndex, UString& outName, UString& outHost, UWideString& outAddress);
    virtual long ProcessGuaranteedLocalUserInput(NEMessage* inMessage, unsigned char inPlayer);
    virtual long ProcessLocalUserInput(NEMessage* inMessage, unsigned char inPlayer);
    virtual long SearchForGame(bool inStart);

    // vtable 0x98994 slots 9..22: the client's own virtuals. StartThreads/StopThreads are implemented; the rest are
    // pure (the game implements them). Names and signatures of the pure ones inferred (slots by offset).
    virtual FSFile* CreateXferFile(const UWideString& inName, unsigned long inSize) = 0;  // +0x24 (Receive: opens the file of an incoming xfer)
    virtual bool IsPlaybackRecordingSession() = 0;                              // +0x28 (ProcessUserInputMessage)
    virtual unsigned long GetConnectTimeout() = 0;                              // +0x2c (Connect: the last argument)
    virtual unsigned char GetClientSlot() = 0;                                  // +0x30 (Receive: the slot in an NEMXferStatus)
    virtual void StartThreads();                                                // +0x34
    virtual void StopThreads();                                                 // +0x38
    virtual bool IgnoreConnectionTimeout() = 0;                                 // +0x3c (Receive: true skips the timeout disconnect)
    virtual void SetConnectionState(long inState) = 0;                          // +0x40 (1 connecting, 2 ended, 0)
    virtual void HandleMessage(NEMessage* inMessage) = 0;                       // +0x44 (unhandled messages)
    virtual void HandleUserInput(NEMessage* inMessage) = 0;                     // +0x48
    virtual void HandleConnectionLost() = 0;                                    // +0x4c (Receive: after a 75 s timeout)
    virtual unsigned long HandleStartGame(unsigned long inParameter) = 0;       // +0x50
    virtual void HandleStopGame() = 0;                                          // +0x54
    virtual void HandleConnectionTimeoutWarning(unsigned long inRemaining) = 0;  // +0x58 (Receive: ms left before the 75 s timeout)

private:
    static void LagReceiveLossCallback(NEMessage* inMessage, void* inThisPointer);
    static long LagSendLossCallback(NEMessage* inMessage, void* inThisPointer);
    static unsigned long Main(void* inClientObject);
    long ProcessUserInputMessage(NEMessage* inMessage, unsigned char inPlayer, bool inGuaranteed);
    void ProcessCommunicationsTickMessage(NEMCommunicationsTick* inMessage);
    void ProcessMessage(NEMessage* inMessage);
    static unsigned long Receive(void* inClientObject);
    static void RecordPlaybackCallback(NEMessage* inMessage, void* inThisPointer);
    long SendAMessage(NEMessage* inMessage);

    NECDIClient mDebugInfo;                 // +0x30
    std::deque<NEMessage*> mOutgoingMessages;   // +0x34 (mOutgoingMessagesLock)
    std::deque<NEMessage*> mIncomingMessages;   // +0x48 (mIncomingMessagesLock)
    NEMessage* mLastCommunicationsTickResponse;   // +0x5c (resent for a repeated tick)
    NEProtocol* mProtocol;                  // +0x60 protocol 0
    NEConnection* mConnection;              // +0x64
    NEConnection* mXferConnection;          // +0x68
    NEMessage* mStartConnectMessage;        // +0x6c
    TSThread* mMainThread;                  // +0x70
    TSEvent* mIncomingMessageAvailableEvent;    // +0x74
    TSEvent* mMainThreadExitEvent;          // +0x78
    TSEvent* mMainThreadStartedEvent;       // +0x7c
    TSEvent* mMainThreadFinishedEvent;      // +0x80
    TSEvent* mMainThreadStartConnectEvent;  // +0x84
    TSEvent* mMainThreadEndConnectEvent;    // +0x88
    TSThread* mReceiveThread;               // +0x8c
    TSEvent* mIncomingPacketAvailableEvent; // +0x90
    TSEvent* mXferIncomingAvailableEvent;   // +0x94
    TSEvent* mReceiveThreadCanGoEvent;      // +0x98
    TSEvent* mReceiveThreadExitEvent;       // +0x9c
    TSEvent* mReceiveThreadStartedEvent;    // +0xa0
    TSEvent* mReceiveThreadFinishedEvent;   // +0xa4
    TSCriticalSection* mIncomingMessagesLock;   // +0xa8
    TSCriticalSection* mOutgoingMessagesLock;   // +0xac
    NELagLossSimulator* mLagLossSimulator;  // +0xb0
    NERecordPlayback* mRecordPlayback;      // +0xb4
    unsigned long mUB8;                     // +0xb8 (provisional name)
    unsigned long mCurrentPhysicsTick;      // +0xbc
    unsigned long mCurrentCommunicationsTick;   // +0xc0
    unsigned long mLastUserInputTime;       // +0xc4 (ProcessUserInputMessage: once a second)
    unsigned long mUserInputsThisSecond;    // +0xc8 (at most 10)
    unsigned long mLastPlaybackTick;        // +0xcc (RecordPlaybackCallback)
    unsigned long mMaxNumConnectionTrys;    // +0xd0
    unsigned long mNumConnectionTrys;       // +0xd4
    unsigned char mMySendToSlot;            // +0xd8 (0xff: none)
};

#endif
