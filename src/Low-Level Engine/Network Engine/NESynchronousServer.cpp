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
#include "platform/PlatSim.h"

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
        return static_cast<NESynchronousServer*>(inServer)->ProcessLocalUserInput(reinterpret_cast<NEMessage*>(a), (unsigned char)b);
    }
    return 0;
}
