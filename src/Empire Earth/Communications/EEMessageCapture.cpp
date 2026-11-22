// New (port), TEST ONLY: the game messages' records of the simulation capture (ee/EEMessageCapture.h;
// tools/sim_capture.py; docs/RUST_PLAN.md R5b.4): NMIM, NMEX, NMSW, NMCU, NMNE (NMDC, the Execute decisions: ee_sim
// netmsg decisions.rs). port (round 10, lane C): written by gamecpp msg_capture.rs (the records described there); this
// unit sets the hooks the factory, the physics and EE-SIM call and answers the records' C++ reads (one statement
// each). Reads the messages, writes nothing to the world.
#include <windows.h>
#include "platform/PlatSim.h"
#include <string.h>
#include <stdlib.h>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "ee/EEMessage.h"
#include "ee/EEMessageCapture.h"
#include "rust/sim/netmsg.h"
#include "rust/game/gamecpp.h"

static void EEMessageCaptureImportImpl(NEMessage* inMessage, const unsigned char* inData)
{
    ee_gamecpp_msgcap_import(inMessage, inData);
}

static void EEMessageCaptureExecutedImpl(NEMessage* inMessage)
{
    ee_gamecpp_msgcap_executed(inMessage);
}

static void EEMessageCaptureSweepImpl()
{
    ee_gamecpp_msgcap_sweep();
}

// Hooks set during static initialisation; the original code is unchanged by these TEST ONLY records
static const bool sCaptureHooksSet = (EEMessageCaptureImport = EEMessageCaptureImportImpl,
                                      EEMessageCaptureExecuted = EEMessageCaptureExecutedImpl,
                                      EEMessageCaptureSweep = EEMessageCaptureSweepImpl, true);

// ------------------------------------------------------------------------- round 10, lane C: the records' C++ reads

int64_t EECore10OpD(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEGameUtilities.cpp (lane D)

#define P64(x) ((int64_t)(intptr_t)(x))
#define MCMSG ((NEMessage*)p)
#define MCNE (static_cast<NEMRecord*>(MCMSG))
#define MCMR (static_cast<EEMRecord*>(MCMSG))

// the C++ calls and members of gamecpp msg_capture.rs and mrecord.rs (p a message): one statement each
int64_t EECore10OpC3(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::LcMsgType: return MCMSG->GetType();
    case O::LcMsgForSlot: return MCMSG->GetForSpecificSlot();
    case O::LcMsgOrigSlot: return MCMSG->GetOriginatingSlot();
    case O::LcMsgIsGame: return EEGameMessageCast(MCMSG) != NULL;
    case O::LcMsgExportSize: return (int64_t)MCMSG->ExportSize();
    case O::LcMsgExport: MCMSG->ExportMessage((unsigned char**)a[0], *(unsigned long*)a[1]);
        break;
    case O::LcBytesDelete: delete[] (unsigned char*)p;
        break;
    case O::LcMsgCodecSlots: return (int64_t)EEGameMessageCast(MCMSG)->GetCodecSlots((EeNetMsgSlot*)a[0], *(uint32_t*)a[1]);
    case O::LcMsgSelectIDs: EESelectUnitsIDs(static_cast<EEMRecord*>(EEGameMessageCast(MCMSG)), *(std::vector<long>*)a[0]);
        break;
    case O::LcMsgBuildGame: return P64(EEMessage::BuildEEMessage((unsigned char*)a[0]));
    case O::LcMsgBuildEngine: return P64(NEMessage::BuildNEMessage((unsigned char*)a[0]));
    case O::LcMsgDereference: MCMSG->Dereference();
        break;
    case O::LcNeInputs: return (int64_t)MCNE->mPlayerInputs.size();
    case O::LcNeInputAt: return P64(MCNE->mPlayerInputs[a[0]]);
    case O::LcNeString: return P64(&MCNE->GetString((uint32_t)a[0]));
    case O::LcNeRecord: return P64(MCNE->mRecord);
    case O::LcWideLength: return (int64_t)((const UWideString*)p)->GetLength();
    // the messages' codec and the records (gamecpp mrecord.rs; p the message)
    case O::LcEmCodecSlots: return (int64_t)static_cast<EEMessage*>(MCMSG)->GetCodecSlots((EeNetMsgSlot*)a[0], *(uint32_t*)a[1]);
    case O::LcBytesNew: return P64(new unsigned char[a[0]]);
    case O::LcMrRecord: return P64(MCMR->mRecord);
    case O::LcMrFastId: return P64(&MCMR->mFastSelectionID);
    case O::LcMrHeaderSlots: MCMR->GetHeaderSlots((EeNetMsgSlot*)a[0]);
        break;
    case O::LcMrSetString: MCMR->mString = *(const UWideString*)a[0];
        break;
    case O::LcMrStringText: return P64((const char*)MCMR->mString);
    case O::LcMrStringImport: MCMR->mString.Import((unsigned char*)a[0]);
        break;
    case O::LcMrListAppend: MCMR->mList.insert(MCMR->mList.end(), (const unsigned char*)a[0], (const unsigned char*)a[0] + a[1]);
        break;
    case O::LcMrListAssign: MCMR->mList.assign((const unsigned char*)a[0], (const unsigned char*)a[0] + a[1]);
        break;
    case O::LcMrListClear: MCMR->mList.clear();
        break;
    case O::LcMrListData: return P64(MCMR->mList.empty() ? NULL : &MCMR->mList[0]);
    case O::LcMrListSize: return (int64_t)MCMR->mList.size();
    default: return EECore10OpD(inOp, p, a, f);
    }
    return 0;
}
