// C:\EEX\Low-Level Engine\Network Engine\NESynchronousClient.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// NESynchronousClient whole in Rust (ee_net client.rs; PORT_LEARNINGS "LLE Network Engine, round 3"): the members
// forward to the Rust object, whose calls out each op of Host answers with the statement the original made (the
// original unit is tests/net/orig_c/NESynchronousClient.cpp, the golden's reference).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousClient.h"

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
    case EeNetClOp::ProcessLocalUserInput:   // (the game's calls: GameHost)
        return static_cast<NESynchronousClient*>(inClient)->ProcessLocalUserInput(reinterpret_cast<NEMessage*>(a), (unsigned char)b);
    }
    return 0;
}
