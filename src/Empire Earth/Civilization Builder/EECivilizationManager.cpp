// C:\EEX\Empire Earth\Civilization Builder\EECivilizationManager.cpp (file name from the class; no __FILE__). The
// civilization categories, premade civilizations and upgrade ids (RTTI EECivilizationManager, vtable 0x44bfd4). Range
// 0x2813f4..0x2830d0; EECivPowersManager.cpp's 0x2830d0..0x283b2e folded in (civilization builder lane, round 2).
// Whole in Rust (ee_tech civ.rs: the tables and queries; civ_apply.rs: ApplyUpgrade, ApplyUpgradeLevel with
// EECBProcessUpgrade and EECBUpgradeArmor, the powers' ApplyPower): the two classes keep their names and methods as
// forwarders. UI lane round 12: their remaining bodies (the db files' reads, the premade copies, ApplyUpgrade with
// its world host, the out-parameter members) are ee_game screens civ_manager_obj.rs over the shims of this unit; the
// world calls are civ_manager_calls.rs'. Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GERhombus.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEUnitSetManager.h"
#include "lle/GEText.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "ee/EECBUIListBox.h"
#include "ee/EECivilizationManager.h"
#include "ee/EEGameVariant.h"
#include "ee/EEStringIDs.h"
#include "ee/EERulesWorld.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGraphicEffect.h"
#include "rust/sim/tech.h"
#include "rust/game/screens.h"   // the host and the premade list (civ_manager_calls.rs)
#include "ee/EEScreensShims.h"

// 0x24f0e9 (EEFOFindNewTargetNearOldTarget.cpp; as ee/EEUnitUtilities.h and EEWorld.cpp declare it)
#include "ee/EEUnitUtilities.h"   // EEUpgradeHitPoints

// chain at 0x681426 (wrapper)
//   object 0x9654c0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
const UString kCivilizationsKey("Civilizations");

// Creates the civilization manager (the tables are Rust's: it binds the game's rules for them)
EECivilizationManager::EECivilizationManager()
{
    ee_tech_civ_bind_rules(EEGetRules().RustRules());
}

// ------------------------------------------------------------------------------------------------ the simulation's host

#define CIVH_P(p) (::gWorld.mPlayers[p])
#define CIVH_U(u) ((EEUnitClassUpgrade*)(u))
#define CIVH_K(k) ((EEUnitClass*)(k))
extern "C" bool ee_screenscpp_civh_set_exists(int64_t inSet)
{ return gUnitSetManager.GetSet((unsigned long)inSet) != NULL; }
extern "C" int64_t ee_screenscpp_civh_class_count() { return EEUnitClassCount(); }
extern "C" bool ee_screenscpp_civh_set_contains(int64_t inSet, int64_t inKind)
{ return gUnitSetManager.GetSet((unsigned long)inSet)->Contains((unsigned long)inKind); }
extern "C" bool ee_screenscpp_civh_class_listed(int64_t inKind) { return gUnitClassTable[(long)inKind] != NULL; }
extern "C" uintptr_t ee_screenscpp_civh_upgrade(int32_t inPlayer, int64_t inKind)
{ return (uintptr_t)CIVH_P(inPlayer)->mUnitClassUpgrades[(long)inKind]; }
extern "C" uintptr_t ee_screenscpp_civh_upgrade_class(uintptr_t inUp) { return (uintptr_t)CIVH_U(inUp)->mUnitClass; }
extern "C" uintptr_t ee_screenscpp_civh_upgrade_fields(uintptr_t inUp)
{ return (uintptr_t)static_cast<EeUnitsUnitClassUpgradeFields*>(CIVH_U(inUp)); }
extern "C" uintptr_t ee_screenscpp_civh_class_fields(uintptr_t inK)
{ return (uintptr_t)static_cast<EeUnitsUnitClassFields*>(CIVH_K(inK)); }
extern "C" void ee_screenscpp_civh_put_range(uintptr_t inUp, float inV) { CIVH_U(inUp)->SetRange(inV); }
extern "C" void ee_screenscpp_civh_put_max_velocity(uintptr_t inUp, float inV) { CIVH_U(inUp)->SetMaxVelocity(inV); }
extern "C" float ee_screenscpp_civh_variant_factor(int64_t inKind)
{ return ::gWorld.GetGameVariant()->GetFactor((long)inKind); }
extern "C" void ee_screenscpp_civh_hit_points(int32_t inPlayer, int64_t inKind, float inFactor)
{ EEUpgradeHitPoints(CIVH_P(inPlayer)->mUnitClassUpgrades[(long)inKind], (long)inKind, inFactor, CIVH_P(inPlayer)); }
extern "C" void ee_screenscpp_civh_sight_on(uintptr_t inUnit) { ((EEUnit*)inUnit)->SightOnHere(); }
extern "C" void ee_screenscpp_civh_sight_off(uintptr_t inUnit) { ((EEUnit*)inUnit)->SightOffHere(); }
extern "C" int64_t ee_screenscpp_civh_population_limit(int32_t inPlayer)
{ return CIVH_P(inPlayer)->GetPopulationLimit(0); }
extern "C" void ee_screenscpp_civh_recompute_area_effects(int32_t inPlayer)
{ CIVH_P(inPlayer)->SuggestRecomputeAllAreaEffects(); }
extern "C" void ee_screenscpp_civh_show_selection_areas(int32_t inPlayer)
{ CIVH_P(inPlayer)->mSelectedGroup->ShowSelectionAreas(); }
extern "C" void ee_screenscpp_civh_research(int32_t inPlayer, int64_t inKind)
{ CIVH_P(inPlayer)->GetTechTree()->Research((long)inKind, NULL); }
extern "C" uintptr_t ee_screenscpp_civh_node(int32_t inPlayer, int64_t inKind)
{ return (uintptr_t)CIVH_P(inPlayer)->GetTechTree()->Get((long)inKind); }
extern "C" bool ee_screenscpp_civh_node_researched(uintptr_t inNode)
{ return ((EETechTreeNode*)inNode)->mIsResearched; }
extern "C" void ee_screenscpp_civh_node_research(uintptr_t inNode) { ((EETechTreeNode*)inNode)->ResearchNode(NULL); }
extern "C" void ee_screenscpp_civh_node_set_allowed(uintptr_t inNode, bool inV)
{ ((EETechTreeNode*)inNode)->mIsAllowed = inV; }
extern "C" void ee_screenscpp_civh_node_set_obsolete(uintptr_t inNode)
{ ((EETechTreeNode*)inNode)->SetObsolete(true, false); }
extern "C" uintptr_t ee_screenscpp_civh_sound(int64_t inKind) { return (uintptr_t)EESoundObject::Get((long)inKind); }
extern "C" void ee_screenscpp_civh_sound_load(uintptr_t inSound) { ((EESoundObject*)inSound)->LoadMedia(); }
extern "C" void ee_screenscpp_civh_add_sound(int32_t inPlayer, uintptr_t inSound)
{ CIVH_P(inPlayer)->AddLoadedMedia((EESoundObject*)inSound); }
extern "C" uintptr_t ee_screenscpp_civh_effect(int64_t inKind) { return (uintptr_t)EEGraphicEffect::Get((long)inKind); }
extern "C" void ee_screenscpp_civh_effect_load(uintptr_t inEffect) { ((EEGraphicEffect*)inEffect)->LoadMedia(); }
extern "C" void ee_screenscpp_civh_add_effect(int32_t inPlayer, uintptr_t inEffect)
{ CIVH_P(inPlayer)->AddLoadedMedia((EEGraphicEffect*)inEffect); }
extern "C" void ee_screenscpp_civh_pulse_sight(int32_t inPlayer, float inRadius, int64_t inKind)
{ CIVH_P(inPlayer)->PulseSight(false, inRadius, (long)inKind); }
extern "C" void ee_screenscpp_civh_camouflage(int32_t inPlayer) { CIVH_P(inPlayer)->EnableCamouflagePower(); }
extern "C" bool ee_screenscpp_cbl_tabable(uintptr_t inList) { return ((EECBUIListBox*)inList)->GetTabable(); }

// ------------------------------------------------------------------------------------------------ the powers

// the db files' reads, the premade copies and the powers' test: civ_manager_obj.rs over
// these (UI lane round 12)
extern "C" int32_t ee_screenscpp_civm_file_open(uintptr_t inFile) { return ((FSFile*)inFile)->Open(kFileAccessRead); }
extern "C" int32_t ee_screenscpp_civm_read_chunk(uintptr_t inFile)
{ return ((FSFile*)inFile)->Read(EEScratch<unsigned char[0x1000]>(kScratchCivmChunk), sizeof(EEScratch<unsigned char[0x1000]>(kScratchCivmChunk)), EEScratch<unsigned long>(kScratchCivmRead)); }
extern "C" void ee_screenscpp_civm_chunk_bytes(void* inOut)
{ ee_screens_shim_out(inOut, EEScratch<unsigned char[0x1000]>(kScratchCivmChunk), sizeof(EEScratch<unsigned char[0x1000]>(kScratchCivmChunk))); }
extern "C" int32_t ee_screenscpp_civm_read_byte(uintptr_t inFile) { return ((FSFile*)inFile)->Read(EEScratch<unsigned char>(kScratchCivmByte)); }
extern "C" uint8_t ee_screenscpp_civm_byte() { return EEScratch<unsigned char>(kScratchCivmByte); }
extern "C" void ee_screenscpp_civm_file_close(uintptr_t inFile) { ((FSFile*)inFile)->Close(); }
extern "C" int64_t ee_screenscpp_civm_powers_cost()
{ return (int64_t)EERuleLong(EEGetRules().mCivBuilder.mPowersCost); }
extern "C" void ee_screenscpp_civm_civs_clear(uintptr_t inList)
{ ((std::vector<EECivilizationEntry>*)inList)->clear(); }
extern "C" void ee_screenscpp_civm_civs_push(uintptr_t inList, uintptr_t inEntry)
{ ((std::vector<EECivilizationEntry>*)inList)->push_back(*(const EECivilizationEntry*)inEntry); }
extern "C" uintptr_t ee_screenscpp_civm_purchased_count(uintptr_t inPlayer)
{ return ((EEPlayer*)inPlayer)->mCivilization.GetPurchasedUpgrades().size(); }
extern "C" uintptr_t ee_screenscpp_civm_purchased_data(uintptr_t inPlayer)
{ return (uintptr_t)((EEPlayer*)inPlayer)->mCivilization.GetPurchasedUpgrades().data(); }

long EECivPowersManager::GetName(long inID, UWideString& outName)
{
    return gResource->GetString(inID + IDS_CIV_POWER_BASE, outName);
}
