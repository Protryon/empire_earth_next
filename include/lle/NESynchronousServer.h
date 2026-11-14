// C:\EEX\Low-Level Engine\Network Engine\NESynchronousServer.h (RECONSTRUCTION; NESynchronousServer.cpp 0x47400..0x4b04f).
// The lock-step multiplayer server: three threads (main: incoming player inputs and responses; send: the
// communications tick; xfer: file transfers) over one published NEServer, plus a message manager that tracks which
// client has answered which message. Abstract: the game implements the pure virtual callbacks.
// Whole in Rust (ee_net server.rs; PORT_LEARNINGS "LLE Network Engine, round 4"): the class keeps the type callers
// hold, its virtual members, the base class's fields and Publish's name; every other member forwards to the Rust
// object. The original layout (sizeof 0x208) is gone with its fields.
#ifndef LLE_NESYNCHRONOUSSERVER_H
#define LLE_NESYNCHRONOUSSERVER_H

#include <stddef.h>
#include <stdint.h>
#include <vector>
#include "lle/NESynchronousCommunications.h"
#include "lle/NEProtocol.h"
#include "rust/ee_net.h"

class NEMessage;
struct EeNetServer;                         // the server, whole in Rust (rust/ee_net.h)
enum class EeNetSvOp : uint32_t;
enum class EeNetMmOp : uint32_t;
class NEMRecord;
class NEServer;
class FSFileSpec;

enum {                                      // slot values (ctor ESVERIFY text "inMaxPlayers < knessPlayerSlotNotAvailable+1")
    kSlotNotAvailable = 0xff,      // Provisional enum name; 0xfe is the server's own slot
    knessPlayerSlotNotAvailable = kSlotNotAvailable   // spelled this way in the original's assert strings
};

class LLE_API NESynchronousServer : public NESynchronousCommunications {
public:
    NESynchronousServer(unsigned char inMaxPlayers, unsigned long inDefaultSendInterval, unsigned char inPTsPerCT, unsigned long inMaxPhysicsTicksPerSecond,
                        unsigned long inMinPhysicsTicksPerSecond, float inPhysicsTickCostFactor, unsigned long inMaxSendInterval, unsigned long inSendIntervalIncrement, unsigned long inSendIntervalDecrement,
                        unsigned long inCurrentCommunicationsTick, unsigned long inCurrentPhysicsTick,
                        UWideString& inName, bool inAllowNewConnections, bool inAnnounce, unsigned long inNetworkAdapter);
    virtual ~NESynchronousServer();

    void AllowNewPlayers(bool inAllow);
    void ClearSendToSlotArray();
    long GetClientAddress(unsigned long inIndex, UWideString& outAddress);
    virtual unsigned long GetCurrentCommunicationsTick() const;
    unsigned long GetCurrentPhysicsTick() const;
    unsigned char GetGameState() const;
    bool GetGameWasPublished() const;
    // the recorder's mode (NERecordPlaybackMode; ee_net recplay.rs)
    unsigned char GetRecordPlaybackMode() const;
    unsigned long GetLastCTMessageID() const;
    unsigned long GetMaxPlayers() const;
    unsigned char GetNumberOfPlayers() const;
    bool GetProcessingSendToAllAndWait() const;
    unsigned long GetPTsPerCT() const;
    unsigned long GetRestartCT() const;
    unsigned long GetSendInterval() const;
    void LockSendCommunicationsTick();
    virtual long ProcessGuaranteedLocalUserInput(NEMessage* inMessage, unsigned char inPlayer)
        { return this->ProcessLocalUserInput(inMessage, inPlayer); }
    void ResetClientConnection(unsigned long inIndex);
    void ResetXfer();
    void ResumeMigratedGame();
    void SendExitMessage();
    virtual void SetCurrentCommunicationsTick(unsigned long inTick);
    void SetCurrentPhysicsTick(unsigned long& inTick);
    void SetGameState(unsigned char inState);
    void SetLastCTMessageID(unsigned long inID);
    void SetLoadParams(unsigned long inTick);
    void SetRestartCT(unsigned long inCT);
    void SetSendInterval(unsigned long inInterval);
    void SetSendToSlotForPlayer(unsigned char inPlayer, unsigned char inSlot);
    void StartGame();
    void StartPing();
    void StartWaitingForReconnects(unsigned long inTime);
    void StopGame();
    void StopPing();
    void UnlockSendCommunicationsTick();
    void SetTempOutOfSynchPTs(unsigned char inPTs);
    // TEST ONLY: the headless simulation (platform/PlatSim.h). The physics thread sends the next communications tick
    // itself; the send thread sends none while the game runs.
    void SimCommunicationsTick();
    bool SimGameRunning() const;   // running, paused

    void AbortXfer(unsigned long inXferID);
    unsigned long GetMessageManagerCurrentMessageNumber();
    unsigned char GetSendToSlotForPlayer(unsigned char inPlayer);
    void LogChecksumResponse(NEMRecord* inMessage);   // a world database checksum response
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
    virtual void HandleXferStatus(unsigned char& inSlot, unsigned char& inPercentDone, uint32_t& inXferID,
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
    void ForceNextCT();
    long SendMessageTo(unsigned char inSlot, NEMessage* inMessage, bool inGuaranteed);
    long SendMessageToAll(NEMessage* inMessage, bool inGuaranteed);
    void UnregisterSlot(unsigned char inSlot);

private:
    NEProtocol* Protocol() const;
    NEServer* Server() const;
    // the server's calls out (ee_net's EeNetSvOp): the engine, the protocol and its server, the messages, the base
    // class's fields, the game's virtual members
    static ptrdiff_t Host(void* inServer, EeNetSvOp inOp, size_t a, size_t b, size_t c, size_t d);
    // the message manager's calls out that reach the C++ (ee_net's EeNetMmOp): the messages, the server, the files
    static ptrdiff_t MessageManagerHost(void* inServer, EeNetMmOp inOp, size_t a, size_t b, size_t c, size_t d);
    // the queues' common calls out (ee_net's EeNetQOp)
    static ptrdiff_t QueueHost(void* inServer, EeNetQOp inOp, size_t a, size_t b, size_t c, size_t d);

    UWideString mName;                      // Publish's name (+0x70)
    bool mAnnounce;                         // Publish's announcement
    unsigned long mNetworkAdapter;          // Publish's adapter
    EeNetServer* mRust;                     // the server, whole in Rust (ee_net server.rs)
};

// NESynchronousServer's game state (GetGameState); 2 is compared by EEServer only and stays unnamed
enum NEServerGameState {
    kServerStateOpen = 0,   // setrilo ne_synchronous.h gameState_ (0 lobby, 1 closed, 3 running, 4 paused); AllowNewPlayers(true); Main start
    kServerStateClosed = 1,  // setrilo ne_synchronous.h gameState_ (0 lobby, 1 closed, 3 running, 4 paused); AllowNewPlayers(false)
    kServerStateRunning = 3,  // setrilo ne_synchronous.h gameState_ (0 lobby, 1 closed, 3 running, 4 paused); Main: mStartGameEvent
    kServerStatePaused = 4,  // setrilo ne_synchronous.h gameState_ (0 lobby, 1 closed, 3 running, 4 paused); Main: mPauseGameEvent
};

#endif
