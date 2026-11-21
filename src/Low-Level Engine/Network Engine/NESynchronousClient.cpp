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
#include "rust/net/net.h"

NESynchronousClient::NESynchronousClient()
{
    // the client calls the protocol itself (ee_net ffi_protocol.rs)
    this->mRust = ee_net_cl_new(this, NESynchronousClient::Host, NESynchronousClient::QueueHost, this->mComms, true);
}

NESynchronousClient::~NESynchronousClient()
{
    ee_net_cl_free(this->mRust);
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
    case EeNetClOp::MsgCountsTowardsThrottle:
        return theMessage->GetCountsTowardsPlayerInputThrottle();
    case EeNetClOp::MsgCanSendWhilePaused:
        return theMessage->GetCanSendWhilePaused();
    case EeNetClOp::MsgIsAvailableToSend:
        return theMessage->GetIsAvailableToSend();
    case EeNetClOp::MsgSetOriginatingSlot:
        theMessage->SetOriginatingSlot((unsigned char)b);
        return 0;
    case EeNetClOp::MsgSetForSpecificSlot:
        theMessage->SetForSpecificSlot((unsigned char)b);
        return 0;
    case EeNetClOp::MsgS2awId:
        return theMessage->GetSendToAllAndWaitMessageID(*reinterpret_cast<unsigned long*>(b));
    case EeNetClOp::MsgGet:
        return (ptrdiff_t)theRecord->Value((uint32_t)b);
    case EeNetClOp::MsgNew:
        return (ptrdiff_t) new NEMRecord((unsigned char)a, reinterpret_cast<const uint32_t*>(b), c);
    case EeNetClOp::MsgImport:
        return (ptrdiff_t) new NEMRecord(reinterpret_cast<unsigned char*>(a));
    case EeNetClOp::MsgAddInput:
        return theRecord->AddPlayerInput(reinterpret_cast<NEMessage*>(b));
    case EeNetClOp::MsgExportNew:
        theMessage->ExportMessage(reinterpret_cast<unsigned char**>(b), *reinterpret_cast<unsigned long*>(c));
        return 0;
    case EeNetClOp::BytesFree:
        delete[] reinterpret_cast<unsigned char*>(a);
        return 0;
    case EeNetClOp::FsWrite:
        return theFile->Write(reinterpret_cast<const unsigned char*>(b), (unsigned long)c, *reinterpret_cast<unsigned long*>(d));
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
