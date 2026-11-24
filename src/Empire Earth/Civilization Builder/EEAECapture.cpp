// New (port), TEST ONLY: the area effects' records of the simulation capture (tools/sim_capture.py,
// every profile). Reads the world; writes nothing to it. UI lane round 12: the records are Rust's (ee_game screens
// area_effect_capture.rs) over the one-statement shims below; EEAEManager's tick calls ee_screens_ae_capture_tick.
//   AETP  every area-effect template as Init left it (once, at the first area-effect tick): index, the numbers, the
//         graphic effects and media by table index
//   AEFX  the area effects after each EEAEManager::Tick: the elapsed time; per unit (players in order, each list in
//         order) that owns or is affected by an effect: its id, hit points and cloakable flag, each own effect's kind,
//         player, location, checksum, strength, radius, scale, immunity, the three modifiers of a probe value, the
//         morale count, and its Covers of its own unit; each affecting effect as its owner and index; then the active
//         list the same way and the running state's save bytes (EEAESaveRunningState)
#include <windows.h>
#include <algorithm>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"

#include "ee/EETechCapture.h"
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
