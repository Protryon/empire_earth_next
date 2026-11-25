// C:\EEX\Low-Level Engine\Network Engine\NESynchronousCommunications.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// The class's inline members are emitted by NEMessageManager.cpp (first includer); this unit holds the out-of-line
// members only.
#include <windows.h>
#include <string.h>
#include <string>

#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "lle/NEProtocol.h"
#include "lle/NESynchronousCommunications.h"
#include "rust/net/net.h"

// The calls out of the lag simulator and the recorder (ee_net lagloss.rs, recplay.rs) that the original units made
// on the engine and the Rust does not make itself (the clock, the events and the threads are its own: ee_net
// native.rs): each op is the statement NELagLossSimulator.cpp, NERecordPlayback.cpp or NETimestampedMessage.cpp made
// (the owner answers Deliver, the callbacks)
ptrdiff_t NESynchronousCommunications::QueueHostCommon(EeNetQOp inOp, size_t a, size_t b, size_t c, size_t d)
{
    static const char* const kUnits[] = {"NELagLossSimulator.cpp", "NERecordPlayback.cpp", "NETimestampedMessage.cpp",
                                         "NESynchronousClient.cpp", "NESynchronousServer.cpp"};
    switch (inOp) {
    case EeNetQOp::Verify:
        ESVerify(kUnits[a < 5 ? a : 0], (unsigned long)b, "(ee_net)");
        return 0;
    case EeNetQOp::FpuStart:
        UMSetFloatingPointPrecision(kFPPrecision53);
        return 0;
    case EeNetQOp::FpuCheck: ee_util_fp_check(); return 0;
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
    default:   // (Time, the events, the threads: ee_net native.rs; Deliver: the owner's)
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
