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


#define P64(x) ((int64_t)(intptr_t)(x))
#define MCMSG ((NEMessage*)p)
#define MCNE (static_cast<NEMRecord*>(MCMSG))
#define MCMR (static_cast<EEMRecord*>(MCMSG))

