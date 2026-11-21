// New (port), TEST ONLY: the calamity lane's records of the simulation capture (ee/EECalamityCapture.h;
// tools/sim_capture.py --profile calamity). Reads the world; writes nothing to it.
#include <windows.h>
#include <algorithm>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/USingleton.h"

#include "ee/EEEffectManager.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEContagionManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EESoundObject.h"
#include "ee/EESaveGameFiles.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECalamityCapture.h"
#include "rust/game/gamecpp.h"

// The records are Rust (ee_game gamecpp calamity_capture.rs; game core lane, round 7)
void EECalamityCaptureClasses()
{
    ee_gamecpp_capture_calamity_classes();
}

void EECalamityCaptureContagionSave()
{
    ee_gamecpp_capture_contagion_save();
}

void EECalamityCaptureSaves()
{
    ee_gamecpp_capture_calamity_saves();
}

static void EECalamityCaptureSaveImpl(EECalamity* inCalamity, unsigned long inIndex)
{
    ee_gamecpp_capture_calamity_save(inCalamity, inIndex);
}

static void EECalamityCaptureAgedImpl(EECalamity* inCalamity, unsigned long inIndex, float inBefore)
{
    ee_gamecpp_capture_calamity_aged(inCalamity, inIndex, inBefore);
}

// The game calls these through the header's hook pointers, set here before main: the game's units name no
// symbol of this TEST ONLY unit (tools/cpp_deps.py cuts; PORT_LEARNINGS "Cycle cuts")
static const bool sCaptureHooksSet = (EECalamityCaptureAged = EECalamityCaptureAgedImpl,
                                      EECalamityCaptureSave = EECalamityCaptureSaveImpl, true);
