// C:\EEX\Low-Level Engine\Network Engine\NESynchronousServer.h (RECONSTRUCTION; NESynchronousServer.cpp 0x47400..0x4b04f).
// The lock-step multiplayer server: three threads (main: incoming player inputs and responses; send: the
// communications tick; xfer: file transfers) over one published NEServer, plus a message manager that tracks which
// client has answered which message. The game's subclass answers the callbacks (GameHost, LLE round 13).
// Whole in Rust (ee_net server.rs; PORT_LEARNINGS "LLE Network Engine, round 4"): the class keeps the type callers
// hold, the base class's fields and Publish's name; every other member forwards to the Rust
// object. The original layout (sizeof 0x208) is gone with its fields; Publish's name, announcement and adapter went
// to the Rust object with the protocol calls (round 7).
#ifndef LLE_NESYNCHRONOUSSERVER_H
#define LLE_NESYNCHRONOUSSERVER_H
#include "rust/net/msgid.h"

#include <stddef.h>
#include <stdint.h>
#include <vector>
#include "lle/NESynchronousCommunications.h"
#include "lle/NEProtocol.h"
#include "rust/net/net.h"


// (the slot values are NESlot, rust/net/msgid.h; 0xff, no slot, was knessPlayerSlotNotAvailable in the original's
// assert texts and had no user left.)

class LLE_API NESynchronousServer : public NESynchronousCommunications {
public:
    // inHost, inMessageManagerHost: the subclass's GameHost<Game>, GameMessageManagerHost<Game> (below)
    NESynchronousServer(EeNetSvHostFn inHost, EeNetMmHostFn inMessageManagerHost, unsigned char inMaxPlayers, unsigned long inDefaultSendInterval, unsigned char inPTsPerCT, unsigned long inMaxPhysicsTicksPerSecond,
                        unsigned long inMinPhysicsTicksPerSecond, float inPhysicsTickCostFactor, unsigned long inMaxSendInterval, unsigned long inSendIntervalIncrement, unsigned long inSendIntervalDecrement,
                        unsigned long inCurrentCommunicationsTick, unsigned long inCurrentPhysicsTick,
                        UWideString& inName, bool inAllowNewConnections, bool inAnnounce, unsigned long inNetworkAdapter)
        : NESynchronousCommunications(inCurrentCommunicationsTick)
    {
        this->mServer = ee_net_sv_new_game(this, inHost, inMessageManagerHost, NESynchronousServer::QueueHost, this->mComms,
                                           inMaxPlayers, inDefaultSendInterval, inPTsPerCT, inMaxPhysicsTicksPerSecond,
                                           inMinPhysicsTicksPerSecond, inPhysicsTickCostFactor, inMaxSendInterval,
                                           inSendIntervalIncrement, inSendIntervalDecrement, inCurrentCommunicationsTick,
                                           inCurrentPhysicsTick, static_cast<const char*>(inName), inAllowNewConnections,
                                           inAnnounce, inNetworkAdapter);
    }
    virtual ~NESynchronousServer() { ee_net_sv_free(this->mServer); }

    void AllowNewPlayers(bool inAllow) { ee_net_sv_allow_new_players(this->mServer, inAllow); }
    void ClearSendToSlotArray() { ee_net_sv_clear_send_to_slot_array(this->mServer); }
    long GetClientAddress(unsigned long inIndex, UWideString& outAddress)
    {
        return ee_net_pr_client_address(this->Protocol(), this->Server(), (uint32_t)inIndex, NEProtocol::SetTo<UWideString>,
                                        &outAddress);
    }
    unsigned long GetCurrentPhysicsTick() const { return ee_net_sv_get(this->mServer, EeNetSvGet::CurrentPhysicsTick); }
    unsigned char GetGameState() const { return (unsigned char)ee_net_sv_get(this->mServer, EeNetSvGet::GameState); }
    bool GetGameWasPublished() const { return ee_net_sv_get(this->mServer, EeNetSvGet::GameWasPublished) != 0; }
    // the recorder's mode (NERecordPlaybackMode; ee_net recplay.rs)
    unsigned char GetRecordPlaybackMode() const { return ee_net_sv_record_playback_mode(this->mServer); }
    unsigned long GetLastCTMessageID() const { return ee_net_sv_get(this->mServer, EeNetSvGet::LastCtMessageId); }
    unsigned long GetMaxPlayers() const { return ee_net_sv_get(this->mServer, EeNetSvGet::MaxPlayers); }
    unsigned char GetNumberOfPlayers() const
    {
        return (unsigned char)ee_net_sv_get(this->mServer, EeNetSvGet::NumberOfPlayers);
    }
    void LockSendCommunicationsTick() { ee_net_sv_lock_send_communications_tick(this->mServer); }
    void ResetClientConnection(unsigned long inIndex)
    {
        ee_net_pr_reset(this->Protocol(), this->Server(), false, (uint32_t)inIndex);
    }
    void ResetXfer() { ee_net_pr_reset(this->Protocol(), this->Server(), true, 0); }
    void ResumeMigratedGame() { ee_net_sv_set_event(this->mServer, EeNetSvEvent::ResumeMigratedGame); }
    void SetCurrentPhysicsTick(unsigned long& inTick)
    {
        ee_net_sv_set(this->mServer, EeNetSvSet::CurrentPhysicsTick, (uint32_t)inTick);
    }
    void SetGameState(unsigned char inState) { ee_net_sv_set(this->mServer, EeNetSvSet::GameState, inState); }
    void SetSendInterval(unsigned long inInterval) { ee_net_sv_set_send_interval(this->mServer, (uint32_t)inInterval); }
    void SetSendToSlotForPlayer(unsigned char inPlayer, unsigned char inSlot)
    {
        ee_net_sv_set_send_to_slot_for_player(this->mServer, inPlayer, inSlot);
    }
    void StartGame() { ee_net_sv_set_event(this->mServer, EeNetSvEvent::StartGame); }
    void StartPing() { ee_net_sv_set(this->mServer, EeNetSvSet::Ping, 1); }
    void StartWaitingForReconnects(unsigned long inTime)
    {
        ee_net_sv_start_waiting_for_reconnects(this->mServer, (uint32_t)inTime);
    }
    void StopGame() { ee_net_sv_set_event(this->mServer, EeNetSvEvent::StopGame); }
    void StopPing() { ee_net_sv_set(this->mServer, EeNetSvSet::Ping, 0); }
    void UnlockSendCommunicationsTick() { ee_net_sv_unlock_send_communications_tick(this->mServer); }
    // TEST ONLY: the headless simulation (platform/PlatSim.h). The physics thread sends the next communications tick
    // itself; the send thread sends none while the game runs.
    // TEST ONLY (platform/PlatSim.h): the headless simulation's next communications tick, sent by the physics thread as
    // soon as it has run the last one, at the configured maximum rate, never tuned (ee_net server.rs)
    void SimCommunicationsTick() { ee_net_sv_sim_communications_tick(this->mServer); }
    bool SimGameRunning() const { return ee_net_sv_sim_game_running(this->mServer); }   // running, paused

    void AbortXfer(unsigned long inXferID) { ee_net_sv_abort_xfer(this->mServer, (uint32_t)inXferID); }
    unsigned long GetMessageManagerCurrentMessageNumber()
    {
        return ee_net_sv_message_manager_current_number(this->mServer);
    }
    unsigned char GetSendToSlotForPlayer(unsigned char inPlayer)
    {
        return ee_net_sv_send_to_slot_for_player(this->mServer, inPlayer);
    }
    // Collects world checksums per tick; once every human has answered, marks the out-of-sync slots
    void LogChecksumResponse(NEMRecord* inMessage)   // a world database checksum response
    {
        // the world database checksum response: its tick, its checksum
        ee_net_sv_log_checksum_response(this->mServer, inMessage->Get<uint32_t>(0), inMessage->Get<uint32_t>(1),
                                        inMessage->GetOriginatingSlot());
    }
    long LogResponse(NEMessage* inMessage) { return ee_net_sv_log_response(this->mServer, inMessage); }   // kErrorOK
    void PauseGame() { ee_net_sv_pause_game(this->mServer, true); }
    void RegisterSlot(unsigned char inSlot) { ee_net_sv_register_slot(this->mServer, inSlot); }
    void RemovePlayerFromGameInProgress(unsigned char inSlot, bool inWasBooted, bool inPlayerIsOOS, bool inGameIsExiting)
    {
        ee_net_sv_remove_player_from_game_in_progress(this->mServer, inSlot, inWasBooted, inPlayerIsOOS, inGameIsExiting);
    }
    unsigned long SendToAllAndWait(NEMessage* inMessage, unsigned long inTimeout, unsigned long inRetries)
    {
        return (unsigned long)ee_net_sv_send_to_all_and_wait(this->mServer, inMessage, (uint32_t)inTimeout, (uint32_t)inRetries);
    }
    void SetMessageManagerLowestCTs(unsigned long inCT)
    {
        ee_net_sv_set_message_manager_lowest_cts(this->mServer, (uint32_t)inCT);
    }
    void UnpauseGame() { ee_net_sv_pause_game(this->mServer, false); }
    long XferFileToClient(FSFileSpec& inFile, unsigned long inUserData, unsigned char inSlot, unsigned long& outXferID,
                          bool inP5)
        { return ee_net_sv_xfer_file_to_client(this->mServer, &inFile, (uint32_t)inUserData, inSlot, &outXferID); }

    void StartThreads() { ee_net_sv_start_threads(this->mServer); }
    void StopThreads() { ee_net_sv_stop_threads(this->mServer); }

    // The server's calls out to the game (the original's pure virtuals, vtable 0x9891c slots 9..27; LLE round 13:
    // the subclass's members by name, no slots), then the engine's (Host): the subclass passes GameHost<itself>
    template <class Game> static Game* As(void* inServer) { return static_cast<Game*>(static_cast<NESynchronousServer*>(inServer)); }
    template <class Game> static ptrdiff_t GameHost(void* inServer, EeNetSvOp inOp, size_t a, size_t b, size_t c, size_t d)
    {
        switch (inOp) {
        case EeNetSvOp::BootPlayer: As<Game>(inServer)->BootPlayer((unsigned char)a, b != 0, c != 0); return 0;
        case EeNetSvOp::ServerSlot28: return As<Game>(inServer)->ServerSlot28();
        case EeNetSvOp::IsMigrating: return As<Game>(inServer)->IsMigrating();
        case EeNetSvOp::BeforeRemovingPlayers: As<Game>(inServer)->BeforeRemovingPlayers(); return 0;
        case EeNetSvOp::HandleReconnectWaitTimedOut: As<Game>(inServer)->HandleReconnectWaitTimedOut(); return 0;
        case EeNetSvOp::PlayerSendToSlot: return As<Game>(inServer)->GetPlayerSendToSlot((unsigned char)a);
        case EeNetSvOp::FilterPlayerInput: return As<Game>(inServer)->FilterPlayerInput(reinterpret_cast<NEMessage*>(a));
        case EeNetSvOp::PrepareLocalInput: As<Game>(inServer)->PrepareLocalInput(reinterpret_cast<NEMessage*>(a)); return 0;
        case EeNetSvOp::IsPlayerPresent: return As<Game>(inServer)->IsPlayerPresent((unsigned char)a);
        case EeNetSvOp::ReportLaggers:
            As<Game>(inServer)->ReportLaggers(Temp(std::vector<unsigned char>((const unsigned char*)a, (const unsigned char*)a + b)), c != 0);
            return 0;
        case EeNetSvOp::ProcessSystemMessage: As<Game>(inServer)->ProcessSystemMessage(reinterpret_cast<NEMessage*>(a)); return 0;
        default: return NESynchronousServer::Host(inServer, inOp, a, b, c, d);
        }
    }
    // the message manager's calls out to the game, then the engine's (MessageManagerHost)
    template <class Game>
    static ptrdiff_t GameMessageManagerHost(void* inServer, EeNetMmOp inOp, size_t a, size_t b, size_t c, size_t d)
    {
        switch (inOp) {
        case EeNetMmOp::ProcessSystemMessage: As<Game>(inServer)->ProcessSystemMessage(reinterpret_cast<NEMessage*>(a)); return 0;
        case EeNetMmOp::XferStatus: As<Game>(inServer)->HandleXferStatus(Temp((unsigned char)a), Temp((unsigned char)b), Temp((uint32_t)c), Temp((unsigned char)d)); return 0;
        default: return NESynchronousServer::MessageManagerHost(inServer, inOp, a, b, c, d);
        }
    }

    // Sends a message to every present player's slot; stops at the first send error and returns it
    long BroadcastMessage(NEMessage* inMessage) { return ee_net_sv_broadcast_message(this->mServer, inMessage); }
    void ForceNextCT() { ee_net_sv_set_event(this->mServer, EeNetSvEvent::ForceNextCt); }
    long SendMessageTo(unsigned char inSlot, NEMessage* inMessage, bool inGuaranteed)
    {
        return (long)ee_net_sv_send_message_to(this->mServer, inSlot, inMessage, inGuaranteed);
    }
    long SendMessageToAll(NEMessage* inMessage, bool inGuaranteed)
    {
        return (long)ee_net_sv_send_message_to_all(this->mServer, inMessage, inGuaranteed);
    }
    void UnregisterSlot(unsigned char inSlot) { ee_net_sv_unregister_slot(this->mServer, inSlot); }

    // the server's calls out (ee_net's EeNetSvOp): the engine, the protocol and its server, the messages, the base
    // class's fields
    static ptrdiff_t Host(void* inServer, EeNetSvOp inOp, size_t a, size_t b, size_t c, size_t d);
    // the message manager's calls out that reach the C++ (ee_net's EeNetMmOp): the messages, the server, the files
    static ptrdiff_t MessageManagerHost(void* inServer, EeNetMmOp inOp, size_t a, size_t b, size_t c, size_t d);

    EeNetProtocol* Protocol() const { return static_cast<EeNetProtocol*>(ee_net_sv_protocol(this->mServer)); }   // protocol 0 (ee_net ffi_protocol.rs)
    void* Server() const { return ee_net_sv_server(this->mServer); }   // the published server (the plug-in's handle)
    // the queues' common calls out (ee_net's EeNetQOp)
    static ptrdiff_t QueueHost(void* inServer, EeNetQOp inOp, size_t a, size_t b, size_t c, size_t d)
        { return NESynchronousCommunications::QueueHostCommon(inOp, a, b, c, d); }
};

#endif
