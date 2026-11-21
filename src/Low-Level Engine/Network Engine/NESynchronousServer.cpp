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
#include "rust/net/net.h"
#include "platform/PlatSim.h"

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

// Destroys the server: frees events, threads, locks and the recorder, then closes the published server
NESynchronousServer::~NESynchronousServer()
{
    ee_net_sv_free(this->mRust);
}

// ---- the members (ee_net server.rs)

long NESynchronousServer::XferFileToClient(FSFileSpec& inFile, unsigned long inUserData, unsigned char inSlot,
                                           unsigned long& outXferID, bool inP5)
{
    uint32_t theXferID = 0;
    if (!ee_net_sv_xfer_file_to_client(this->mRust, &inFile, (uint32_t)inUserData, inSlot, &theXferID))
        return kErrorNotFound;
    outXferID = theXferID;
    return kErrorOK;
}

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
