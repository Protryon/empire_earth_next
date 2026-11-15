// C:\EEX\Low-Level Engine\Network Engine\NESynchronousServer.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// NESynchronousServer whole in Rust (ee_net server.rs; PORT_LEARNINGS "LLE Network Engine, round 4"): the members
// forward to the Rust object, whose calls out the three hosts answer with the statements the original made (the
// original unit is tests/net/orig_s/NESynchronousServer.cpp, the golden's reference). The message manager, the
// tuning and the recorder are the Rust object's own.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousServer.h"
#include "rust/ee_net.h"
#include "platform/PlatSim.h"

// FUNCTION: LLE 0x10047400
// Creates the game server on protocol 0 and publishes it as "EEService"; on failure stops with no threads/events
NESynchronousServer::NESynchronousServer(unsigned char inMaxPlayers, unsigned long inDefaultSendInterval, unsigned char inPTsPerCT,
                                         unsigned long inMaxPhysicsTicksPerSecond, unsigned long inMinPhysicsTicksPerSecond, float inPhysicsTickCostFactor, unsigned long inMaxSendInterval,
                                         unsigned long inSendIntervalIncrement, unsigned long inSendIntervalDecrement,
                                         unsigned long inCurrentCommunicationsTick,
                                         unsigned long inCurrentPhysicsTick, UWideString& inName, bool inAllowNewConnections,
                                         bool inAnnounce, unsigned long inNetworkAdapter)
    : NESynchronousCommunications(inCurrentCommunicationsTick)
{
    EeNetSvArgs theArgs;
    theArgs.max_players = inMaxPlayers;
    theArgs.default_send_interval = (uint32_t)inDefaultSendInterval;
    theArgs.pts_per_ct = inPTsPerCT;
    theArgs.max_pts_per_second = (uint32_t)inMaxPhysicsTicksPerSecond;
    theArgs.min_pts_per_second = (uint32_t)inMinPhysicsTicksPerSecond;
    theArgs.cost_factor = inPhysicsTickCostFactor;
    theArgs.max_send_interval = (uint32_t)inMaxSendInterval;
    theArgs.send_interval_increment = (uint32_t)inSendIntervalIncrement;
    theArgs.send_interval_decrement = (uint32_t)inSendIntervalDecrement;
    theArgs.current_communications_tick = (uint32_t)inCurrentCommunicationsTick;
    theArgs.current_physics_tick = (uint32_t)inCurrentPhysicsTick;
    theArgs.allow_new_connections = inAllowNewConnections;
    // the server calls the protocol itself (ee_net ffi_protocol.rs): Publish's name, announcement and adapter
    theArgs.tcp = true;
    theArgs.name = reinterpret_cast<const uint8_t*>(static_cast<const char*>(inName));
    theArgs.name_length = strlen(static_cast<const char*>(inName));
    theArgs.announce = inAnnounce;
    theArgs.network_adapter = (uint32_t)inNetworkAdapter;
    this->mRust = ee_net_sv_new(this, NESynchronousServer::Host, NESynchronousServer::MessageManagerHost,
                                NESynchronousServer::QueueHost, &theArgs, this->mComms);
}

// FUNCTION: LLE 0x10047c65
// Destroys the server: frees events, threads, locks and the recorder, then closes the published server
NESynchronousServer::~NESynchronousServer()
{
    ee_net_sv_free(this->mRust);
}

// ---- the members (ee_net server.rs)

void NESynchronousServer::AllowNewPlayers(bool inAllow) { ee_net_sv_allow_new_players(this->mRust, inAllow); }
void NESynchronousServer::ClearSendToSlotArray() { ee_net_sv_clear_send_to_slot_array(this->mRust); }
long NESynchronousServer::GetClientAddress(unsigned long inIndex, UWideString& outAddress)
{
    return ee_net_pr_client_address(this->Protocol(), this->Server(), (uint32_t)inIndex, NEProtocol::SetTo<UWideString>,
                                    &outAddress);
}
unsigned long NESynchronousServer::GetCurrentCommunicationsTick() const
{
    return ee_net_sv_get(this->mRust, EeNetSvGet::CurrentCommunicationsTick);
}
unsigned long NESynchronousServer::GetCurrentPhysicsTick() const
{
    return ee_net_sv_get(this->mRust, EeNetSvGet::CurrentPhysicsTick);
}
unsigned char NESynchronousServer::GetGameState() const { return (unsigned char)ee_net_sv_get(this->mRust, EeNetSvGet::GameState); }
bool NESynchronousServer::GetGameWasPublished() const { return ee_net_sv_get(this->mRust, EeNetSvGet::GameWasPublished) != 0; }
unsigned long NESynchronousServer::GetLastCTMessageID() const { return ee_net_sv_get(this->mRust, EeNetSvGet::LastCtMessageId); }
unsigned long NESynchronousServer::GetMaxPlayers() const { return ee_net_sv_get(this->mRust, EeNetSvGet::MaxPlayers); }
unsigned char NESynchronousServer::GetNumberOfPlayers() const
{
    return (unsigned char)ee_net_sv_get(this->mRust, EeNetSvGet::NumberOfPlayers);
}
void NESynchronousServer::LockSendCommunicationsTick() { ee_net_sv_lock_send_communications_tick(this->mRust); }
void NESynchronousServer::UnlockSendCommunicationsTick() { ee_net_sv_unlock_send_communications_tick(this->mRust); }
void NESynchronousServer::ResetClientConnection(unsigned long inIndex)
{
    ee_net_pr_reset(this->Protocol(), this->Server(), false, (uint32_t)inIndex);
}
void NESynchronousServer::ResetXfer() { ee_net_pr_reset(this->Protocol(), this->Server(), true, 0); }
void NESynchronousServer::ResumeMigratedGame() { ee_net_sv_set_event(this->mRust, EeNetSvEvent::ResumeMigratedGame); }
void NESynchronousServer::SetCurrentCommunicationsTick(unsigned long inTick)
{
    ee_net_sv_set(this->mRust, EeNetSvSet::CurrentCommunicationsTick, (uint32_t)inTick);
}
void NESynchronousServer::SetCurrentPhysicsTick(unsigned long& inTick)
{
    ee_net_sv_set(this->mRust, EeNetSvSet::CurrentPhysicsTick, (uint32_t)inTick);
}
void NESynchronousServer::SetGameState(unsigned char inState) { ee_net_sv_set(this->mRust, EeNetSvSet::GameState, inState); }
void NESynchronousServer::SetSendInterval(unsigned long inInterval) { ee_net_sv_set_send_interval(this->mRust, (uint32_t)inInterval); }
void NESynchronousServer::SetSendToSlotForPlayer(unsigned char inPlayer, unsigned char inSlot)
{
    ee_net_sv_set_send_to_slot_for_player(this->mRust, inPlayer, inSlot);
}
void NESynchronousServer::StartGame() { ee_net_sv_set_event(this->mRust, EeNetSvEvent::StartGame); }
void NESynchronousServer::StartPing() { ee_net_sv_set(this->mRust, EeNetSvSet::Ping, 1); }
void NESynchronousServer::StopPing() { ee_net_sv_set(this->mRust, EeNetSvSet::Ping, 0); }
void NESynchronousServer::StartWaitingForReconnects(unsigned long inTime)
{
    ee_net_sv_start_waiting_for_reconnects(this->mRust, (uint32_t)inTime);
}
void NESynchronousServer::StopGame() { ee_net_sv_set_event(this->mRust, EeNetSvEvent::StopGame); }
void NESynchronousServer::ForceNextCT() { ee_net_sv_set_event(this->mRust, EeNetSvEvent::ForceNextCt); }
unsigned char NESynchronousServer::GetRecordPlaybackMode() const { return ee_net_sv_record_playback_mode(this->mRust); }

// TEST ONLY (platform/PlatSim.h): the headless simulation's next communications tick, sent by the physics thread as
// soon as it has run the last one, at the configured maximum rate, never tuned (ee_net server.rs)
void NESynchronousServer::SimCommunicationsTick() { ee_net_sv_sim_communications_tick(this->mRust); }
bool NESynchronousServer::SimGameRunning() const { return ee_net_sv_sim_game_running(this->mRust); }

// FUNCTION: LLE 0x10047e73
void NESynchronousServer::AbortXfer(unsigned long inXferID) { ee_net_sv_abort_xfer(this->mRust, (uint32_t)inXferID); }

// FUNCTION: LLE 0x10048011
// Sends a message to every present player's slot; stops at the first send error and returns it
long NESynchronousServer::BroadcastMessage(NEMessage* inMessage) { return ee_net_sv_broadcast_message(this->mRust, inMessage); }

// FUNCTION: LLE 0x10048093
unsigned long NESynchronousServer::GetMessageManagerCurrentMessageNumber()
{
    return ee_net_sv_message_manager_current_number(this->mRust);
}

// FUNCTION: LLE 0x100480a0
unsigned char NESynchronousServer::GetSendToSlotForPlayer(unsigned char inPlayer)
{
    return ee_net_sv_send_to_slot_for_player(this->mRust, inPlayer);
}

// FUNCTION: LLE 0x10048139
// Collects world checksums per tick; once every human has answered, marks the out-of-sync slots
void NESynchronousServer::LogChecksumResponse(NEMRecord* inMessage)
{
    // the world database checksum response: its tick, its checksum
    ee_net_sv_log_checksum_response(this->mRust, inMessage->Get<uint32_t>(0), inMessage->Get<uint32_t>(1),
                                    inMessage->GetOriginatingSlot());
}

// FUNCTION: LLE 0x10048472
long NESynchronousServer::LogResponse(NEMessage* inMessage)
{
    ee_net_sv_log_response(this->mRust, inMessage);
    return kErrorOK;
}

// FUNCTION: LLE 0x10048484
void NESynchronousServer::PauseGame() { ee_net_sv_pause_game(this->mRust, true); }

// FUNCTION: LLE 0x1004976e
void NESynchronousServer::UnpauseGame() { ee_net_sv_pause_game(this->mRust, false); }

// FUNCTION: LLE 0x1004855d
long NESynchronousServer::ProcessLocalUserInput(NEMessage* inMessage, unsigned char inPlayer)
{
    return ee_net_sv_process_local_user_input(this->mRust, inMessage, inPlayer);
}

// FUNCTION: LLE 0x100488df
void NESynchronousServer::RegisterSlot(unsigned char inSlot) { ee_net_sv_register_slot(this->mRust, inSlot); }

// FUNCTION: LLE 0x10048983
void NESynchronousServer::RemovePlayerFromGameInProgress(unsigned char inSlot, bool inWasBooted, bool inPlayerIsOOS, bool inGameIsExiting)
{
    ee_net_sv_remove_player_from_game_in_progress(this->mRust, inSlot, inWasBooted, inPlayerIsOOS, inGameIsExiting);
}

// FUNCTION: LLE 0x10048a4a
long NESynchronousServer::SearchForGame(bool inStart) { return ee_net_sv_search_for_game(this->mRust, inStart); }

// FUNCTION: LLE 0x10048e31
long NESynchronousServer::SendMessageTo(unsigned char inSlot, NEMessage* inMessage, bool inGuaranteed)
{
    return (long)ee_net_sv_send_message_to(this->mRust, inSlot, inMessage, inGuaranteed);
}

// FUNCTION: LLE 0x10048e4b
long NESynchronousServer::SendMessageToAll(NEMessage* inMessage, bool inGuaranteed)
{
    return (long)ee_net_sv_send_message_to_all(this->mRust, inMessage, inGuaranteed);
}

// FUNCTION: LLE 0x10048e61
unsigned long NESynchronousServer::SendToAllAndWait(NEMessage* inMessage, unsigned long inTimeout, unsigned long inRetries)
{
    return (unsigned long)ee_net_sv_send_to_all_and_wait(this->mRust, inMessage, (uint32_t)inTimeout, (uint32_t)inRetries);
}

// FUNCTION: LLE 0x10048eb4
void NESynchronousServer::SetMessageManagerLowestCTs(unsigned long inCT)
{
    ee_net_sv_set_message_manager_lowest_cts(this->mRust, (uint32_t)inCT);
}

// FUNCTION: LLE 0x10048ed9
void NESynchronousServer::StartThreads() { ee_net_sv_start_threads(this->mRust); }

// FUNCTION: LLE 0x10049028
void NESynchronousServer::StopThreads() { ee_net_sv_stop_threads(this->mRust); }

// FUNCTION: LLE 0x10049753
void NESynchronousServer::UnregisterSlot(unsigned char inSlot) { ee_net_sv_unregister_slot(this->mRust, inSlot); }

// FUNCTION: LLE 0x100497c8
long NESynchronousServer::XferFileToClient(FSFileSpec& inFile, unsigned long inUserData, unsigned char inSlot,
                                           unsigned long& outXferID, bool inP5)
{
    uint32_t theXferID = 0;
    if (!ee_net_sv_xfer_file_to_client(this->mRust, &inFile, (uint32_t)inUserData, inSlot, &theXferID))
        return kErrorNotFound;
    outXferID = theXferID;
    return kErrorOK;
}

EeNetProtocol* NESynchronousServer::Protocol() const { return static_cast<EeNetProtocol*>(ee_net_sv_protocol(this->mRust)); }
void* NESynchronousServer::Server() const { return ee_net_sv_server(this->mRust); }

// ---- the calls out

// The message manager's calls out that reach the C++ (ee_net msgmgr.rs; the server's own fields, the forced tick and
// the send-to-all-and-wait's start are the Rust server's): the messages, the published server, the base class's
// fields, the game, a transfer's file spec (the manager's own copy, FileClone/FileFree)
ptrdiff_t NESynchronousServer::MessageManagerHost(void* inServer, EeNetMmOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    NESynchronousServer* theServer = static_cast<NESynchronousServer*>(inServer);
    NEMessage* theMessage = reinterpret_cast<NEMessage*>(a);
    FSFileSpec* theFile = reinterpret_cast<FSFileSpec*>(a);
    switch (inOp) {
    case EeNetMmOp::MsgReference:
        theMessage->Reference();
        return 0;
    case EeNetMmOp::MsgDereference:
        theMessage->Dereference();
        return 0;
    case EeNetMmOp::MsgType:
        return theMessage->GetType();
    case EeNetMmOp::MsgIsResponse:
        return theMessage->IsResponse(reinterpret_cast<NEMessage*>(b));
    case EeNetMmOp::MsgSlot:
        return theMessage->GetOriginatingSlot();
    case EeNetMmOp::NewResponse: {
        // the sent message's first member: the tick's tick, the pause's or unpause's unique ID
        uint32_t theSent = reinterpret_cast<NEMRecord*>(b)->Get<uint32_t>(0);
        NEMessage* theResponse;
        if (a == kNetMessageCommunicationsTick)
            theResponse = new NEMRecord(kNetMessageCommunicationsTickResponse,
                                        {theServer->GetNumberOfCTsProcessedInPhysics(), theSent,
                                         (unsigned short)theServer->GetAveragePhysicsUpdateTime()});
        else
            theResponse = new NEMRecord(
                a == kNetMessageUnPauseGame ? kNetMessageUnPauseGameResponse : kNetMessagePauseGameResponse, {theSent});
        theResponse->SetOriginatingSlot(kSlotServer);
        return reinterpret_cast<ptrdiff_t>(theResponse);
    }
    case EeNetMmOp::ProcessSystemMessage:
        theServer->ProcessSystemMessage(theMessage);
        return 0;
    case EeNetMmOp::XferStatus: {
        unsigned char theSlot = (unsigned char)a, thePercent = (unsigned char)b, theStatus = (unsigned char)d;
        uint32_t theID = (uint32_t)c;
        theServer->HandleXferStatus(theSlot, thePercent, theID, theStatus);
        return 0;
    }
    case EeNetMmOp::FileClone: {
        FSFileSpec* theCopy = new FSFileSpec;
        *theCopy = *theFile;
        return reinterpret_cast<ptrdiff_t>(theCopy);
    }
    case EeNetMmOp::FileFree:
        delete theFile;
        return 0;
    case EeNetMmOp::FileIsOpen:
        return theFile->GetIsOpen();
    case EeNetMmOp::FileOpen:
        return theFile->Open(kFileAccessRead) == kErrorOK;
    case EeNetMmOp::FileClose:
        theFile->Close();
        return 0;
    case EeNetMmOp::FileLength: {
        unsigned long theLength = 0;
        theFile->GetLength(theLength);
        return (uint32_t)theLength;
    }
    case EeNetMmOp::FileRead: {
        unsigned long theRead = 0;
        if (theFile->Read(reinterpret_cast<unsigned char*>(b), 0x400, theRead) != kErrorOK)
            return -1;
        return (uint32_t)theRead;
    }
    case EeNetMmOp::FileName:
        return reinterpret_cast<ptrdiff_t>(static_cast<const char*>(theFile->GetFile()));
    default:
        // (ForceCt, LastCtMessageId, StartSendToAllAndWait and Time are the Rust server's; MsgSendTo, MsgTick,
        // ResetClientConnection and XferSend its host's, ee_net ffi_server.rs)
        return 0;
    }
}

// The recorder's and the server's queue calls out: the common ones (its callback is the Rust server's own)
ptrdiff_t NESynchronousServer::QueueHost(void*, EeNetQOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    return NESynchronousCommunications::QueueHostCommon(inOp, a, b, c, d);
}

// The server's calls out, each the statement NESynchronousServer.cpp made (the protocol ops are the Rust's own:
// ee_net ffi_protocol.rs)
ptrdiff_t NESynchronousServer::Host(void* inServer, EeNetSvOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    NESynchronousServer* theServer = static_cast<NESynchronousServer*>(inServer);
    NEMessage* theMessage = reinterpret_cast<NEMessage*>(a);
    NEMRecord* theRecord = static_cast<NEMRecord*>(theMessage);   // an engine message (the ops that read members)
    switch (inOp) {
    case EeNetSvOp::SimMode:
        return gPlatSimMode;
    case EeNetSvOp::MsgType:
        return theMessage->GetType();
    case EeNetSvOp::MsgSetSlot:
        if (b != 0)
            theMessage->SetOriginatingSlot((unsigned char)c);
        else
            theMessage->SetForSpecificSlot((unsigned char)c);
        return 0;
    case EeNetSvOp::MsgFlag:
        if ((EeNetSvMsgFlag)b == EeNetSvMsgFlag::CanSendWhilePaused)
            return theMessage->GetCanSendWhilePaused();
        return theMessage->GetIsAvailableToSend();
    case EeNetSvOp::MsgSlot:
        return theMessage->GetOriginatingSlot();
    case EeNetSvOp::MsgGet:
        return (ptrdiff_t)theRecord->Value((uint32_t)b);
    case EeNetSvOp::MsgNew:
        return (ptrdiff_t) new NEMRecord((unsigned char)a, reinterpret_cast<const uint32_t*>(b), c);
    case EeNetSvOp::MsgTickResponseInput: {
        NEMessage* theInput = NULL;
        long theResult = theRecord->GetPlayerInput((unsigned long)b, theInput);
        *reinterpret_cast<NEMessage**>(c) = theInput;
        return theResult;
    }
    case EeNetSvOp::MsgAddInput:
        return theRecord->AddPlayerInput(reinterpret_cast<NEMessage*>(b));
    case EeNetSvOp::MsgSetS2awId:
        theMessage->SetSendToAllAndWaitMessageID((unsigned long)b);
        return 0;
    case EeNetSvOp::MsgExportNew: {
        unsigned char* theData = NULL;
        unsigned long theSize = 0;
        theMessage->ExportMessage(&theData, theSize);
        *reinterpret_cast<uint32_t*>(b) = (uint32_t)theSize;
        return (ptrdiff_t)theData;
    }
    case EeNetSvOp::BytesFree:
        delete[] reinterpret_cast<unsigned char*>(a);
        return 0;
    case EeNetSvOp::BootPlayer:
        theServer->BootPlayer((unsigned char)a, b != 0, c != 0);
        return 0;
    case EeNetSvOp::ServerSlot28:
        return theServer->ServerSlot28();
    case EeNetSvOp::IsMigrating:
        return theServer->IsMigrating();
    case EeNetSvOp::BeforeRemovingPlayers:
        theServer->BeforeRemovingPlayers();
        return 0;
    case EeNetSvOp::HandleReconnectWaitTimedOut:
        theServer->HandleReconnectWaitTimedOut();
        return 0;
    case EeNetSvOp::PlayerSendToSlot:
        return theServer->GetPlayerSendToSlot((unsigned char)a);
    case EeNetSvOp::FilterPlayerInput:
        return theServer->FilterPlayerInput(theMessage);
    case EeNetSvOp::PrepareLocalInput:
        theServer->PrepareLocalInput(theMessage);
        return 0;
    case EeNetSvOp::IsPlayerPresent:
        return theServer->IsPlayerPresent((unsigned char)a);
    case EeNetSvOp::ReportLaggers: {
        const unsigned char* theFirst = reinterpret_cast<const unsigned char*>(a);
        std::vector<unsigned char> theSlots(theFirst, theFirst + b);
        theServer->ReportLaggers(theSlots, c != 0);
        return 0;
    }
    case EeNetSvOp::ProcessSystemMessage:
        theServer->ProcessSystemMessage(theMessage);
        return 0;
    case EeNetSvOp::ProcessLocalUserInput:
        return theServer->ProcessLocalUserInput(theMessage, (unsigned char)b);
    }
    return 0;
}
