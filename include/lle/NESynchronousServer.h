// C:\EEX\Low-Level Engine\Network Engine\NESynchronousServer.h (RECONSTRUCTION; NESynchronousServer.cpp 0x47400..0x4b04f).
// The lock-step multiplayer server: three threads (main: incoming player inputs and responses; send: the
// communications tick; xfer: file transfers) over one published NEServer, plus a message manager that tracks which
// client has answered which message. Abstract: the game implements the pure virtual callbacks.
// Whole in Rust (ee_net server.rs; PORT_LEARNINGS "LLE Network Engine, round 4"): the class keeps the type callers
// hold, its virtual members, the base class's fields and Publish's name; every other member forwards to the Rust
// object. The original layout (sizeof 0x208) is gone with its fields; Publish's name, announcement and adapter went
// to the Rust object with the protocol calls (round 7).
#ifndef LLE_NESYNCHRONOUSSERVER_H
#define LLE_NESYNCHRONOUSSERVER_H

#include <stddef.h>
#include <stdint.h>
#include <vector>
#include "lle/NESynchronousCommunications.h"
#include "lle/NEProtocol.h"
#include "rust/net/net.h"

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

    void AllowNewPlayers(bool inAllow) { ee_net_sv_allow_new_players(this->mRust, inAllow); }
    void ClearSendToSlotArray() { ee_net_sv_clear_send_to_slot_array(this->mRust); }
    long GetClientAddress(unsigned long inIndex, UWideString& outAddress)
    {
        return ee_net_pr_client_address(this->Protocol(), this->Server(), (uint32_t)inIndex, NEProtocol::SetTo<UWideString>,
                                        &outAddress);
    }
    virtual unsigned long GetCurrentCommunicationsTick() const
    {
        return ee_net_sv_get(this->mRust, EeNetSvGet::CurrentCommunicationsTick);
    }
    unsigned long GetCurrentPhysicsTick() const { return ee_net_sv_get(this->mRust, EeNetSvGet::CurrentPhysicsTick); }
    unsigned char GetGameState() const { return (unsigned char)ee_net_sv_get(this->mRust, EeNetSvGet::GameState); }
    bool GetGameWasPublished() const { return ee_net_sv_get(this->mRust, EeNetSvGet::GameWasPublished) != 0; }
    // the recorder's mode (NERecordPlaybackMode; ee_net recplay.rs)
    unsigned char GetRecordPlaybackMode() const { return ee_net_sv_record_playback_mode(this->mRust); }
    unsigned long GetLastCTMessageID() const { return ee_net_sv_get(this->mRust, EeNetSvGet::LastCtMessageId); }
    unsigned long GetMaxPlayers() const { return ee_net_sv_get(this->mRust, EeNetSvGet::MaxPlayers); }
    unsigned char GetNumberOfPlayers() const
    {
        return (unsigned char)ee_net_sv_get(this->mRust, EeNetSvGet::NumberOfPlayers);
    }
    void LockSendCommunicationsTick() { ee_net_sv_lock_send_communications_tick(this->mRust); }
    virtual long ProcessGuaranteedLocalUserInput(NEMessage* inMessage, unsigned char inPlayer)
        { return this->ProcessLocalUserInput(inMessage, inPlayer); }
    void ResetClientConnection(unsigned long inIndex)
    {
        ee_net_pr_reset(this->Protocol(), this->Server(), false, (uint32_t)inIndex);
    }
    void ResetXfer() { ee_net_pr_reset(this->Protocol(), this->Server(), true, 0); }
    void ResumeMigratedGame() { ee_net_sv_set_event(this->mRust, EeNetSvEvent::ResumeMigratedGame); }
    virtual void SetCurrentCommunicationsTick(unsigned long inTick)
    {
        ee_net_sv_set(this->mRust, EeNetSvSet::CurrentCommunicationsTick, (uint32_t)inTick);
    }
    void SetCurrentPhysicsTick(unsigned long& inTick)
    {
        ee_net_sv_set(this->mRust, EeNetSvSet::CurrentPhysicsTick, (uint32_t)inTick);
    }
    void SetGameState(unsigned char inState) { ee_net_sv_set(this->mRust, EeNetSvSet::GameState, inState); }
    void SetSendInterval(unsigned long inInterval) { ee_net_sv_set_send_interval(this->mRust, (uint32_t)inInterval); }
    void SetSendToSlotForPlayer(unsigned char inPlayer, unsigned char inSlot)
    {
        ee_net_sv_set_send_to_slot_for_player(this->mRust, inPlayer, inSlot);
    }
    void StartGame() { ee_net_sv_set_event(this->mRust, EeNetSvEvent::StartGame); }
    void StartPing() { ee_net_sv_set(this->mRust, EeNetSvSet::Ping, 1); }
    void StartWaitingForReconnects(unsigned long inTime)
    {
        ee_net_sv_start_waiting_for_reconnects(this->mRust, (uint32_t)inTime);
    }
    void StopGame() { ee_net_sv_set_event(this->mRust, EeNetSvEvent::StopGame); }
    void StopPing() { ee_net_sv_set(this->mRust, EeNetSvSet::Ping, 0); }
    void UnlockSendCommunicationsTick() { ee_net_sv_unlock_send_communications_tick(this->mRust); }
    // TEST ONLY: the headless simulation (platform/PlatSim.h). The physics thread sends the next communications tick
    // itself; the send thread sends none while the game runs.
    // TEST ONLY (platform/PlatSim.h): the headless simulation's next communications tick, sent by the physics thread as
    // soon as it has run the last one, at the configured maximum rate, never tuned (ee_net server.rs)
    void SimCommunicationsTick() { ee_net_sv_sim_communications_tick(this->mRust); }
    bool SimGameRunning() const { return ee_net_sv_sim_game_running(this->mRust); }   // running, paused

    void AbortXfer(unsigned long inXferID) { ee_net_sv_abort_xfer(this->mRust, (uint32_t)inXferID); }
    unsigned long GetMessageManagerCurrentMessageNumber()
    {
        return ee_net_sv_message_manager_current_number(this->mRust);
    }
    unsigned char GetSendToSlotForPlayer(unsigned char inPlayer)
    {
        return ee_net_sv_send_to_slot_for_player(this->mRust, inPlayer);
    }
    // Collects world checksums per tick; once every human has answered, marks the out-of-sync slots
    void LogChecksumResponse(NEMRecord* inMessage)   // a world database checksum response
    {
        // the world database checksum response: its tick, its checksum
        ee_net_sv_log_checksum_response(this->mRust, inMessage->Get<uint32_t>(0), inMessage->Get<uint32_t>(1),
                                        inMessage->GetOriginatingSlot());
    }
    long LogResponse(NEMessage* inMessage)
    {
        ee_net_sv_log_response(this->mRust, inMessage);
        return kErrorOK;
    }
    void PauseGame() { ee_net_sv_pause_game(this->mRust, true); }
    virtual long ProcessLocalUserInput(NEMessage* inMessage, unsigned char inPlayer)
    {
        return ee_net_sv_process_local_user_input(this->mRust, inMessage, inPlayer);
    }
    void RegisterSlot(unsigned char inSlot) { ee_net_sv_register_slot(this->mRust, inSlot); }
    void RemovePlayerFromGameInProgress(unsigned char inSlot, bool inWasBooted, bool inPlayerIsOOS, bool inGameIsExiting)
    {
        ee_net_sv_remove_player_from_game_in_progress(this->mRust, inSlot, inWasBooted, inPlayerIsOOS, inGameIsExiting);
    }
    virtual long SearchForGame(bool inStart) { return ee_net_sv_search_for_game(this->mRust, inStart); }
    unsigned long SendToAllAndWait(NEMessage* inMessage, unsigned long inTimeout, unsigned long inRetries)
    {
        return (unsigned long)ee_net_sv_send_to_all_and_wait(this->mRust, inMessage, (uint32_t)inTimeout, (uint32_t)inRetries);
    }
    void SetMessageManagerLowestCTs(unsigned long inCT)
    {
        ee_net_sv_set_message_manager_lowest_cts(this->mRust, (uint32_t)inCT);
    }
    void UnpauseGame() { ee_net_sv_pause_game(this->mRust, false); }
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
    virtual void StartThreads() { ee_net_sv_start_threads(this->mRust); }   // +0x3c
    virtual void StopThreads() { ee_net_sv_stop_threads(this->mRust); }   // +0x40
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
    // Sends a message to every present player's slot; stops at the first send error and returns it
    long BroadcastMessage(NEMessage* inMessage) { return ee_net_sv_broadcast_message(this->mRust, inMessage); }
    void ForceNextCT() { ee_net_sv_set_event(this->mRust, EeNetSvEvent::ForceNextCt); }
    long SendMessageTo(unsigned char inSlot, NEMessage* inMessage, bool inGuaranteed)
    {
        return (long)ee_net_sv_send_message_to(this->mRust, inSlot, inMessage, inGuaranteed);
    }
    long SendMessageToAll(NEMessage* inMessage, bool inGuaranteed)
    {
        return (long)ee_net_sv_send_message_to_all(this->mRust, inMessage, inGuaranteed);
    }
    void UnregisterSlot(unsigned char inSlot) { ee_net_sv_unregister_slot(this->mRust, inSlot); }

private:
    EeNetProtocol* Protocol() const { return static_cast<EeNetProtocol*>(ee_net_sv_protocol(this->mRust)); }   // protocol 0 (ee_net ffi_protocol.rs)
    void* Server() const { return ee_net_sv_server(this->mRust); }   // the published server (the plug-in's handle)
    // the server's calls out (ee_net's EeNetSvOp): the engine, the protocol and its server, the messages, the base
    // class's fields, the game's virtual members
    static ptrdiff_t Host(void* inServer, EeNetSvOp inOp, size_t a, size_t b, size_t c, size_t d);
    // the message manager's calls out that reach the C++ (ee_net's EeNetMmOp): the messages, the server, the files
    static ptrdiff_t MessageManagerHost(void* inServer, EeNetMmOp inOp, size_t a, size_t b, size_t c, size_t d);
    // the queues' common calls out (ee_net's EeNetQOp)
    static ptrdiff_t QueueHost(void* inServer, EeNetQOp inOp, size_t a, size_t b, size_t c, size_t d);

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
