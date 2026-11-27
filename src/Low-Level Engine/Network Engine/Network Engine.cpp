// C:\EEX\Low-Level Engine\Network Engine\NESynchronousClient.cpp, NESynchronousCommunications.cpp and
// NESynchronousServer.cpp (0x45c00..0x4b04f): the Network Engine's one unit but NEMessage.cpp (tests/net links that one
// alone). LLE/UI round 3. Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS.
// Each original unit is a section, headed by its path and keeping its comment.
#include <windows.h>
#include <string.h>   // (used directly: kept for the POSIX build)

#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousClient.h"
#include "lle/NESynchronousServer.h"
#include "platform/PlatSim.h"

// ======== Network Engine/NESynchronousClient.cpp ========
// C:\EEX\Low-Level Engine\Network Engine\NESynchronousClient.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// NESynchronousClient whole in Rust (ee_net client.rs; PORT_LEARNINGS "LLE Network Engine, round 3"): the members
// forward to the Rust object, whose calls out each op of Host answers with the statement the original made (the
// original unit is tests/net/orig_c/NESynchronousClient.cpp, the golden's reference).

// The client's calls out, each the statement NESynchronousClient.cpp made (the protocol ops are the Rust's own:
// ee_net ffi_protocol.rs)
ptrdiff_t NESynchronousClient::Host(void* inClient, EeNetClOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    switch (inOp) {
    case EeNetClOp::XferExportSize:
        return (ptrdiff_t)NEMRecord::XferExportSize();
    case EeNetClOp::MsgType:
        return reinterpret_cast<NEMessage*>(a)->GetType();
    case EeNetClOp::MsgCountsTowardsThrottle:
        return reinterpret_cast<NEMessage*>(a)->GetCountsTowardsPlayerInputThrottle();
    case EeNetClOp::MsgCanSendWhilePaused:
        return reinterpret_cast<NEMessage*>(a)->GetCanSendWhilePaused();
    case EeNetClOp::MsgIsAvailableToSend:
        return reinterpret_cast<NEMessage*>(a)->GetIsAvailableToSend();
    case EeNetClOp::MsgSetOriginatingSlot:
        reinterpret_cast<NEMessage*>(a)->SetOriginatingSlot((unsigned char)b);
        return 0;
    case EeNetClOp::MsgSetForSpecificSlot:
        reinterpret_cast<NEMessage*>(a)->SetForSpecificSlot((unsigned char)b);
        return 0;
    case EeNetClOp::MsgS2awId:
        return reinterpret_cast<NEMessage*>(a)->GetSendToAllAndWaitMessageID(*reinterpret_cast<unsigned long*>(b));
    case EeNetClOp::MsgGet:
        return (ptrdiff_t)static_cast<NEMRecord*>(reinterpret_cast<NEMessage*>(a))->Value((uint32_t)b);
    case EeNetClOp::MsgNew:
        return (ptrdiff_t) new NEMRecord((unsigned char)a, reinterpret_cast<const uint32_t*>(b), c);
    case EeNetClOp::MsgImport:
        return (ptrdiff_t) new NEMRecord(reinterpret_cast<unsigned char*>(a));
    case EeNetClOp::MsgAddInput:
        return static_cast<NEMRecord*>(reinterpret_cast<NEMessage*>(a))->AddPlayerInput(reinterpret_cast<NEMessage*>(b));
    case EeNetClOp::MsgExportNew:
        reinterpret_cast<NEMessage*>(a)->ExportMessage(reinterpret_cast<unsigned char**>(b), *reinterpret_cast<unsigned long*>(c));
        return 0;
    case EeNetClOp::BytesFree:
        delete[] reinterpret_cast<unsigned char*>(a);
        return 0;
    case EeNetClOp::FsWrite:
        return reinterpret_cast<FSFile*>(a)->Write(reinterpret_cast<const unsigned char*>(b), (unsigned long)c, *reinterpret_cast<unsigned long*>(d));
    case EeNetClOp::FsClose:
        reinterpret_cast<FSFile*>(a)->Close();
        return 0;
    case EeNetClOp::FsDelete:
        reinterpret_cast<FSFile*>(a)->Delete();
        return 0;
    case EeNetClOp::FsFree:
        delete reinterpret_cast<FSFile*>(a);
        return 0;
    // (ProcessLocalUserInput: the Rust client's own, ee_net client.rs local_user_input; the game's calls: GameHost)
    }
    return 0;
}

// ======== Network Engine/NESynchronousCommunications.cpp ========
// C:\EEX\Low-Level Engine\Network Engine\NESynchronousCommunications.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// The class's inline members are emitted by NEMessageManager.cpp (first includer); this unit holds the out-of-line
// members only.

// The calls out of the lag simulator and the recorder (ee_net lagloss.rs, recplay.rs) that the original units made
// on the engine and the Rust does not make itself (the clock, the events, the threads, the verifications and the
// floating-point control are its own: ee_net native.rs): each op is the statement NELagLossSimulator.cpp,
// NERecordPlayback.cpp or NETimestampedMessage.cpp made (the owner answers Deliver, the callbacks)
ptrdiff_t NESynchronousCommunications::QueueHostCommon(EeNetQOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    switch (inOp) {
    case EeNetQOp::MsgReference:
        reinterpret_cast<NEMessage*>(a)->Reference();
        return 0;
    case EeNetQOp::MsgDereference:
        reinterpret_cast<NEMessage*>(a)->Dereference();
        return 0;
    case EeNetQOp::MsgExportSize:
        return (ptrdiff_t)reinterpret_cast<NEMessage*>(a)->ExportSize();
    case EeNetQOp::MsgExport: {
        unsigned char* theData = NULL;
        unsigned long theSize = 0;
        reinterpret_cast<NEMessage*>(a)->ExportMessage(&theData, theSize);
        memcpy(reinterpret_cast<void*>(b), theData, theSize < c ? theSize : c);
        delete[] theData;
        return (ptrdiff_t)theSize;
    }
    case EeNetQOp::MsgFromBytes:
        return (ptrdiff_t)NEMessage::ConvertMemoryToMessage(reinterpret_cast<unsigned char*>(a));
    default:   // (Time, the events, the threads, Verify, FpuStart, FpuCheck: ee_net native.rs; Deliver: the owner's)
        return 0;
    case EeNetQOp::FileCreate:
        return reinterpret_cast<FSFileSpec*>(a)->Create();
    case EeNetQOp::FileDelete:
        return reinterpret_cast<FSFileSpec*>(a)->Delete();
    case EeNetQOp::FileOpen:
        return reinterpret_cast<FSFileSpec*>(a)->Open((unsigned long)b);
    case EeNetQOp::FileClose:
        reinterpret_cast<FSFileSpec*>(a)->Close();
        return 0;
    case EeNetQOp::FileWrite:
        return reinterpret_cast<FSFileSpec*>(a)->Write(reinterpret_cast<const unsigned char*>(b), (unsigned long)c, Temp(0ul));
    case EeNetQOp::FileWriteU32:
        return reinterpret_cast<FSFileSpec*>(a)->Write((uint32_t)b);
    case EeNetQOp::FileReadU32:
        return reinterpret_cast<FSFileSpec*>(a)->Read(*reinterpret_cast<uint32_t*>(b));
    case EeNetQOp::FileRead:
        return reinterpret_cast<FSFileSpec*>(a)->Read(reinterpret_cast<unsigned char*>(b), (unsigned long)c, Temp(0ul));
    }
    return 0;
}

// ======== Network Engine/NESynchronousServer.cpp ========
// C:\EEX\Low-Level Engine\Network Engine\NESynchronousServer.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// NESynchronousServer whole in Rust (ee_net server.rs; PORT_LEARNINGS "LLE Network Engine, round 4"): the members
// forward to the Rust object, whose calls out the three hosts answer with the statements the original made (the
// original unit is tests/net/orig_s/NESynchronousServer.cpp, the golden's reference). The message manager, the
// tuning and the recorder are the Rust object's own.

// ---- the calls out

// The message manager's calls out that reach the C++ (ee_net msgmgr.rs; the server's own fields, the forced tick and
// the send-to-all-and-wait's start are the Rust server's): the messages, the published server, the base class's
// fields, the game, a transfer's file spec (the manager's own copy, FileClone/FileFree)
ptrdiff_t NESynchronousServer::MessageManagerHost(void* inServer, EeNetMmOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    switch (inOp) {
    case EeNetMmOp::MsgReference:
        reinterpret_cast<NEMessage*>(a)->Reference();
        return 0;
    case EeNetMmOp::MsgDereference:
        reinterpret_cast<NEMessage*>(a)->Dereference();
        return 0;
    case EeNetMmOp::MsgType:
        return reinterpret_cast<NEMessage*>(a)->GetType();
    case EeNetMmOp::MsgIsResponse:
        return reinterpret_cast<NEMessage*>(a)->IsResponse(reinterpret_cast<NEMessage*>(b));
    case EeNetMmOp::MsgSlot:
        return reinterpret_cast<NEMessage*>(a)->GetOriginatingSlot();
    case EeNetMmOp::NewResponse: {
        // the sent message's first member: the tick's tick, the pause's or unpause's unique ID
        uint32_t theSent = reinterpret_cast<NEMRecord*>(b)->Get<uint32_t>(0);
        NEMessage* theResponse;
        if (a == kNetMessageCommunicationsTick)
            theResponse = new NEMRecord(kNetMessageCommunicationsTickResponse,
                                        {static_cast<NESynchronousServer*>(inServer)->GetNumberOfCTsProcessedInPhysics(), theSent,
                                         (unsigned short)static_cast<NESynchronousServer*>(inServer)->GetAveragePhysicsUpdateTime()});
        else
            theResponse = new NEMRecord(
                a == kNetMessageUnPauseGame ? kNetMessageUnPauseGameResponse : kNetMessagePauseGameResponse, {theSent});
        theResponse->SetOriginatingSlot(kSlotServer);
        return reinterpret_cast<ptrdiff_t>(theResponse);
    }
    case EeNetMmOp::FileClone:   // (the copy constructor: the names, closed, read access, as default and assignment)
        return reinterpret_cast<ptrdiff_t>(new FSFileSpec(*reinterpret_cast<FSFileSpec*>(a)));
    case EeNetMmOp::FileFree:
        delete reinterpret_cast<FSFileSpec*>(a);
        return 0;
    case EeNetMmOp::FileIsOpen:
        return reinterpret_cast<FSFileSpec*>(a)->GetIsOpen();
    case EeNetMmOp::FileOpen:
        return reinterpret_cast<FSFileSpec*>(a)->Open(kFileAccessRead) == kErrorOK;
    case EeNetMmOp::FileClose:
        reinterpret_cast<FSFileSpec*>(a)->Close();
        return 0;
    case EeNetMmOp::FileLength: {
        unsigned long theLength = 0;
        reinterpret_cast<FSFileSpec*>(a)->GetLength(theLength);
        return (uint32_t)theLength;
    }
    case EeNetMmOp::FileRead: {
        unsigned long theRead = 0;
        if (reinterpret_cast<FSFileSpec*>(a)->Read(reinterpret_cast<unsigned char*>(b), 0x400, theRead) != kErrorOK)
            return -1;
        return (uint32_t)theRead;
    }
    case EeNetMmOp::FileName:
        return reinterpret_cast<ptrdiff_t>(static_cast<const char*>(reinterpret_cast<FSFileSpec*>(a)->GetFile()));
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
    switch (inOp) {
    case EeNetSvOp::SimMode:
        return gPlatSimMode;
    case EeNetSvOp::MsgType:
        return reinterpret_cast<NEMessage*>(a)->GetType();
    case EeNetSvOp::MsgSetOriginatingSlot:
        reinterpret_cast<NEMessage*>(a)->SetOriginatingSlot((unsigned char)b);
        return 0;
    case EeNetSvOp::MsgSetForSpecificSlot:
        reinterpret_cast<NEMessage*>(a)->SetForSpecificSlot((unsigned char)b);
        return 0;
    case EeNetSvOp::MsgCanSendWhilePaused:
        return reinterpret_cast<NEMessage*>(a)->GetCanSendWhilePaused();
    case EeNetSvOp::MsgIsAvailableToSend:
        return reinterpret_cast<NEMessage*>(a)->GetIsAvailableToSend();
    case EeNetSvOp::MsgSlot:
        return reinterpret_cast<NEMessage*>(a)->GetOriginatingSlot();
    case EeNetSvOp::MsgGet:
        return (ptrdiff_t)static_cast<NEMRecord*>(reinterpret_cast<NEMessage*>(a))->Value((uint32_t)b);
    case EeNetSvOp::MsgNew:
        return (ptrdiff_t) new NEMRecord((unsigned char)a, reinterpret_cast<const uint32_t*>(b), c);
    case EeNetSvOp::MsgTickResponseInput:
        return static_cast<NEMRecord*>(reinterpret_cast<NEMessage*>(a))->GetPlayerInput((unsigned long)b, *reinterpret_cast<NEMessage**>(c));
    case EeNetSvOp::MsgAddInput:
        return static_cast<NEMRecord*>(reinterpret_cast<NEMessage*>(a))->AddPlayerInput(reinterpret_cast<NEMessage*>(b));
    case EeNetSvOp::MsgSetS2awId:
        reinterpret_cast<NEMessage*>(a)->SetSendToAllAndWaitMessageID((unsigned long)b);
        return 0;
    case EeNetSvOp::MsgExportNew:
        reinterpret_cast<NEMessage*>(a)->ExportMessage(reinterpret_cast<unsigned char**>(b), *reinterpret_cast<unsigned long*>(c));
        return 0;
    case EeNetSvOp::BytesFree:
        delete[] reinterpret_cast<unsigned char*>(a);
        return 0;
    }
    return 0;
}
