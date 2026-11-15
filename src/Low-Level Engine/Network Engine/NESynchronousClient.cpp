// C:\EEX\Low-Level Engine\Network Engine\NESynchronousClient.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// NESynchronousClient whole in Rust (ee_net client.rs; PORT_LEARNINGS "LLE Network Engine, round 3"): the members
// forward to the Rust object, whose calls out each op of Host answers with the statement the original made (the
// original unit is tests/net/orig_c/NESynchronousClient.cpp, the golden's reference).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "lle/FSFile.h"
#include "lle/NESynchronousClient.h"
#include "rust/ee_net.h"

// FUNCTION: LLE 0x1004607d
NESynchronousClient::NESynchronousClient()
{
    // the client calls the protocol itself (ee_net ffi_protocol.rs)
    this->mRust = ee_net_cl_new(this, NESynchronousClient::Host, NESynchronousClient::QueueHost, this->mComms, true);
}

// FUNCTION: LLE 0x10046401
NESynchronousClient::~NESynchronousClient()
{
    ee_net_cl_free(this->mRust);
}

// FUNCTION: LLE 0x10046513
long NESynchronousClient::Connect(const UString& inName, const UString& inHost, const UWideString& inAddress)
{
    const char* theAddress = inAddress;
    EeNetClStrings theStrings = {reinterpret_cast<const uint8_t*>(static_cast<const char*>(inName)), inName.GetLength(),
                                 reinterpret_cast<const uint8_t*>(static_cast<const char*>(inHost)), inHost.GetLength(),
                                 reinterpret_cast<const uint8_t*>(theAddress), strlen(theAddress)};
    return ee_net_cl_connect(this->mRust, &theStrings);
}

// FUNCTION: LLE 0x10046562
long NESynchronousClient::Disconnect()
{
    return ee_net_cl_disconnect(this->mRust);
}

// FUNCTION: LLE 0x100465a7
long NESynchronousClient::GetDetectedGame(unsigned long inIndex, UString& outName, UString& outHost,
                                          UWideString& outAddress)
{
    EeNetClSinks theSinks = {NEProtocol::SetTo<UString>, &outName, NEProtocol::SetTo<UString>, &outHost,
                             NEProtocol::SetTo<UWideString>, &outAddress};
    return ee_net_cl_detected_game(this->mRust, (uint32_t)inIndex, &theSinks);
}

// FUNCTION: LLE 0x1004687f
long NESynchronousClient::ProcessGuaranteedLocalUserInput(NEMessage* inMessage, unsigned char inPlayer)
{
    return ee_net_cl_process_user_input(this->mRust, inMessage, inPlayer, true);
}

// FUNCTION: LLE 0x10046891
long NESynchronousClient::ProcessLocalUserInput(NEMessage* inMessage, unsigned char inPlayer)
{
    return ee_net_cl_process_user_input(this->mRust, inMessage, inPlayer, false);
}

// FUNCTION: LLE 0x10047195
long NESynchronousClient::SearchForGame(bool inStart)
{
    return ee_net_cl_search_for_game(this->mRust, inStart);
}

// FUNCTION: LLE 0x100471f7
void NESynchronousClient::StartThreads()
{
    ee_net_cl_start_threads(this->mRust);
}

// FUNCTION: LLE 0x100472cf
void NESynchronousClient::StopThreads()
{
    ee_net_cl_stop_threads(this->mRust);
}

NEConnection* NESynchronousClient::GetConnection() const
{
    return static_cast<NEConnection*>(ee_net_cl_connection(this->mRust));
}

unsigned long NESynchronousClient::GetCurrentCommunicationsTick() const
{
    return ee_net_cl_current_communications_tick(this->mRust);
}

void NESynchronousClient::SetCurrentCommunicationsTick(unsigned long inTick)
{
    ee_net_cl_set_current_communications_tick(this->mRust, (uint32_t)inTick);
}

unsigned long NESynchronousClient::GetCurrentPhysicsTick() const
{
    return ee_net_cl_current_physics_tick(this->mRust);
}

void NESynchronousClient::SetCurrentPhysicsTick(unsigned long& inTick)
{
    ee_net_cl_set_current_physics_tick(this->mRust, (uint32_t)inTick);
}

unsigned long NESynchronousClient::GetDetectedGameCount() const
{
    return ee_net_cl_detected_game_count(this->mRust);
}

unsigned char NESynchronousClient::GetMySendToSlot() const
{
    return ee_net_cl_my_send_to_slot(this->mRust);
}

void NESynchronousClient::SetMySendToSlot(unsigned char inSlot)
{
    ee_net_cl_set_my_send_to_slot(this->mRust, inSlot);
}

unsigned long NESynchronousClient::GetNumConnectionTrys() const
{
    return ee_net_cl_num_connection_trys(this->mRust);
}

void NESynchronousClient::SetMaxNumConnectionTrys(unsigned long inMax)
{
    ee_net_cl_set_max_num_connection_trys(this->mRust, (uint32_t)inMax);
}

void NESynchronousClient::SendStartConnectMessage(NEMessage* inMessage)
{
    ee_net_cl_send_start_connect_message(this->mRust, inMessage);
}

unsigned char NESynchronousClient::GetRecordPlaybackMode() const
{
    return ee_net_cl_record_playback_mode(this->mRust);
}

void NESynchronousClient::SetSlowPlayback()
{
    ee_net_cl_set_slow_playback(this->mRust);
}

void NESynchronousClient::SetNormalPlayback()
{
    ee_net_cl_set_normal_playback(this->mRust);
}

// The lag simulator's and the recorder's calls out: their callbacks are the Rust client's own (ee_net client.rs)
ptrdiff_t NESynchronousClient::QueueHost(void*, EeNetQOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    return NESynchronousCommunications::QueueHostCommon(inOp, a, b, c, d);
}

// The client's calls out, each the statement NESynchronousClient.cpp made (the protocol ops are the Rust's own:
// ee_net ffi_protocol.rs)
ptrdiff_t NESynchronousClient::Host(void* inClient, EeNetClOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    NESynchronousClient* theClient = static_cast<NESynchronousClient*>(inClient);
    NEMessage* theMessage = reinterpret_cast<NEMessage*>(a);
    NEMRecord* theRecord = static_cast<NEMRecord*>(theMessage);   // an engine message (the ops that read members)
    FSFile* theFile = reinterpret_cast<FSFile*>(a);
    switch (inOp) {
    case EeNetClOp::XferExportSize:
        return (ptrdiff_t)NEMRecord::XferExportSize();
    case EeNetClOp::MsgType:
        return theMessage->GetType();
    case EeNetClOp::MsgFlag:
        switch ((EeNetClMsgFlag)b) {
        case EeNetClMsgFlag::CountsTowardsPlayerInputThrottle:
            return theMessage->GetCountsTowardsPlayerInputThrottle();
        case EeNetClMsgFlag::CanSendWhilePaused:
            return theMessage->GetCanSendWhilePaused();
        case EeNetClMsgFlag::IsAvailableToSend:
            return theMessage->GetIsAvailableToSend();
        }
        return 0;
    case EeNetClOp::MsgSetSlot:
        if (b != 0)
            theMessage->SetOriginatingSlot((unsigned char)c);
        else
            theMessage->SetForSpecificSlot((unsigned char)c);
        return 0;
    case EeNetClOp::MsgS2awId: {
        unsigned long theID = *reinterpret_cast<uint32_t*>(b);
        bool theResult = theMessage->GetSendToAllAndWaitMessageID(theID);
        *reinterpret_cast<uint32_t*>(b) = (uint32_t)theID;
        return theResult;
    }
    case EeNetClOp::MsgGet:
        return (ptrdiff_t)theRecord->Value((uint32_t)b);
    case EeNetClOp::MsgNew:
        return (ptrdiff_t) new NEMRecord((unsigned char)a, reinterpret_cast<const uint32_t*>(b), c);
    case EeNetClOp::MsgImport:
        return (ptrdiff_t) new NEMRecord(reinterpret_cast<unsigned char*>(a));
    case EeNetClOp::MsgAddInput:
        return theRecord->AddPlayerInput(reinterpret_cast<NEMessage*>(b));
    case EeNetClOp::MsgExportNew: {
        unsigned char* thePayload = NULL;
        unsigned long thePayloadSize = 0;
        theMessage->ExportMessage(&thePayload, thePayloadSize);
        *reinterpret_cast<uint32_t*>(b) = (uint32_t)thePayloadSize;
        return (ptrdiff_t)thePayload;
    }
    case EeNetClOp::BytesFree:
        delete[] reinterpret_cast<unsigned char*>(a);
        return 0;
    case EeNetClOp::FsWrite: {
        unsigned long theWritten = *reinterpret_cast<uint32_t*>(d);
        long theResult = theFile->Write(reinterpret_cast<const unsigned char*>(b), (unsigned long)c, theWritten);
        *reinterpret_cast<uint32_t*>(d) = (uint32_t)theWritten;
        return theResult;
    }
    case EeNetClOp::FsClose:
        theFile->Close();
        return 0;
    case EeNetClOp::FsDelete:
        theFile->Delete();
        return 0;
    case EeNetClOp::FsFree:
        delete theFile;
        return 0;
    case EeNetClOp::CreateXferFile:
        return (ptrdiff_t)theClient->CreateXferFile(UWideString(reinterpret_cast<char*>(a)), (unsigned long)b);
    case EeNetClOp::IsPlaybackRecordingSession:
        return theClient->IsPlaybackRecordingSession();
    case EeNetClOp::ConnectTimeout:
        return (ptrdiff_t)theClient->GetConnectTimeout();
    case EeNetClOp::ClientSlot:
        return theClient->GetClientSlot();
    case EeNetClOp::IgnoreConnectionTimeout:
        return theClient->IgnoreConnectionTimeout();
    case EeNetClOp::SetConnectionState:
        theClient->SetConnectionState((long)(int32_t)a);
        return 0;
    case EeNetClOp::HandleMessage:
        theClient->HandleMessage(theMessage);
        return 0;
    case EeNetClOp::HandleUserInput:
        theClient->HandleUserInput(theMessage);
        return 0;
    case EeNetClOp::HandleConnectionLost:
        theClient->HandleConnectionLost();
        return 0;
    case EeNetClOp::HandleStartGame:
        return (ptrdiff_t)(uint32_t)theClient->HandleStartGame((unsigned long)a);
    case EeNetClOp::HandleStopGame:
        theClient->HandleStopGame();
        return 0;
    case EeNetClOp::HandleConnectionTimeoutWarning:
        theClient->HandleConnectionTimeoutWarning((unsigned long)a);
        return 0;
    case EeNetClOp::ProcessLocalUserInput:
        return theClient->ProcessLocalUserInput(theMessage, (unsigned char)b);
    }
    return 0;
}
