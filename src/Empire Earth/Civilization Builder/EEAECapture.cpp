// New (port), TEST ONLY: the area effects' records of the simulation capture (ee/EEAECapture.h; tools/sim_capture.py,
// every profile). Reads the world; writes nothing to it. UI lane round 12: the records are Rust's (ee_game screens
// area_effect_capture.rs) over the one-statement shims below; the hook is set to its entry here.
#include <windows.h>
#include <algorithm>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"

#include "ee/EETechCapture.h"
#include "ee/EEAECapture.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAreaEffectTemplate.h"
#include "ee/EEAreaEffectKindIDs.h"
#include "ee/EEAEManager.h"
#include "lle/FSMemoryFile.h"
#include "ee/EEScreensShims.h"
#include "rust/game/screens.h"   // ee_screens_ae_capture_tick (area_effect_capture.rs)

extern "C" uint64_t ee_screenscpp_aec_num_players() { return (uint64_t)::gWorld.mNumPlayers; }
extern "C" uintptr_t ee_screenscpp_aec_player(uint64_t inIndex) { return (uintptr_t)::gWorld.mPlayers[inIndex]; }
extern "C" uintptr_t ee_screenscpp_aec_first_unit(uintptr_t inPlayer)
{ return (uintptr_t)static_cast<EEUnit*>(((EEPlayer*)inPlayer)->mFirstUnit); }
extern "C" uintptr_t ee_screenscpp_aec_next_unit(uintptr_t inUnit)
{ return (uintptr_t)static_cast<EEUnit*>(((EEUnit*)inUnit)->mNext); }
extern "C" bool ee_screenscpp_aec_cloakable(uintptr_t inUnit) { return ((EEComplexUnit*)inUnit)->mIsCloakable; }
extern "C" bool ee_screenscpp_aec_cloaked(uintptr_t inUnit) { return ((EEComplexUnit*)inUnit)->mIsCloaked; }
extern "C" int64_t ee_screenscpp_aec_checksum(uintptr_t inEffect)
{ return (int64_t)((EEAreaEffect*)inEffect)->Checksum(); }
extern "C" uintptr_t ee_screenscpp_aec_template_count() { return gAEManager.mTemplates.size(); }
extern "C" uintptr_t ee_screenscpp_aec_template_at(uintptr_t inIndex)
{ return (uintptr_t)gAEManager.mTemplates[inIndex]; }
extern "C" int32_t ee_screenscpp_aec_effect_index(uintptr_t inEffect)
{ return (int32_t)EEGraphicEffect::IndexOf((EEGraphicEffect*)inEffect); }
extern "C" uint64_t ee_screenscpp_aec_media_count() { return (uint64_t)EEMedia::GetCount(); }
extern "C" uintptr_t ee_screenscpp_aec_media_at(uint64_t inIndex)
{ return (uintptr_t)EEMedia::Get((unsigned long)inIndex); }
extern "C" uintptr_t ee_screenscpp_aec_mem_new() { return (uintptr_t)new FSMemoryFile; }
extern "C" void ee_screenscpp_aec_mem_save(uintptr_t inFile) { EEAESaveRunningState(*(FSMemoryFile*)inFile); }
extern "C" void ee_screenscpp_aec_mem_position(uintptr_t inFile, long* inOut)
{ ((FSMemoryFile*)inFile)->GetPosition(*inOut); }
extern "C" void ee_screenscpp_aec_mem_bytes(uintptr_t inFile, void* inOut, uintptr_t inLength)
{ ee_screens_shim_out(inOut, ((FSMemoryFile*)inFile)->GetData(), inLength); }
extern "C" void ee_screenscpp_aec_mem_free(uintptr_t inFile) { delete (FSMemoryFile*)inFile; }

// The game calls the record through the header's hook pointer, set here before main: the game's units name no
// symbol of this TEST ONLY unit (tools/cpp_deps.py cuts; PORT_LEARNINGS "Cycle cuts")
static const bool sCaptureHooksSet = (EEAECaptureTick = ee_screens_ae_capture_tick, true);
