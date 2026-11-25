// New (port), TEST ONLY: the civilization builder lane's capture records (tools/sim_capture.py).
// Reads the manager, the powers, the players and their class upgrades; changes nothing but the capture file (CVMG
// loads the premade civilizations' files through EELoadCivilization, as the game's choice form does). UI lane round
// 12: the records are Rust's (ee_game screens civ_capture.rs) over the one-statement shims below.
#include <windows.h>
#include <algorithm>
#include <string.h>
#include <string>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EECivilizationManager.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEScreensShims.h"
#include "rust/sim/units.h"   // ee_units_lists_len
#include "rust/game/screens.h"   // ee_screens_civc_* (civ_capture.rs)

#define CP(p) ((EEPlayer*)(p))
#define CN(n) ((EETechTreeNode*)(n))
#define CV(c) ((EECivilization*)(c))
// the unit types of a set, in its order
class EECivcCollect : public EEUnitSetFunctor {
public:
    virtual bool Execute(long inUnitType) { return EEScratch<std::vector<long>>(kScratchCivcTypes).push_back(inUnitType), true; }
};
// a temporary's address for the full expression (the set walker's functor argument)
template <class T> static T* EECivcTemp(T&& inValue) { return &inValue; }
extern "C" uintptr_t ee_screenscpp_civc_player(int64_t inIndex) { return (uintptr_t)::gWorld.mPlayers[inIndex]; }
extern "C" float ee_screenscpp_civc_conversion(uintptr_t inPlayer) { return CP(inPlayer)->mConversionFactor; }
extern "C" float ee_screenscpp_civc_high_ground(uintptr_t inPlayer) { return CP(inPlayer)->mHighGroundBonus; }
extern "C" int64_t ee_screenscpp_civc_share(uintptr_t inPlayer) { return (int64_t)CP(inPlayer)->mShareBonus; }
extern "C" int64_t ee_screenscpp_civc_mining(uintptr_t inPlayer) { return (int64_t)CP(inPlayer)->mMiningLimit; }
extern "C" bool ee_screenscpp_civc_set_exists(int64_t inSet) { return gUnitSetManager.GetSet((long)inSet) != NULL; }
extern "C" void ee_screenscpp_civc_types_clear() { EEScratch<std::vector<long>>(kScratchCivcTypes).clear(); }
extern "C" void ee_screenscpp_civc_collect(int64_t inSet)
{ gUnitSetManager.ForEach((long)inSet, EECivcTemp(EECivcCollect())); }
extern "C" uintptr_t ee_screenscpp_civc_types_count() { return EEScratch<std::vector<long>>(kScratchCivcTypes).size(); }
extern "C" int64_t ee_screenscpp_civc_type_at(uintptr_t inIndex) { return (int64_t)EEScratch<std::vector<long>>(kScratchCivcTypes)[inIndex]; }
extern "C" bool ee_screenscpp_civc_class_exists(int64_t inKind) { return gUnitClassTable[(long)inKind] != NULL; }
extern "C" uintptr_t ee_screenscpp_civc_class_upgrade(uintptr_t inPlayer, int64_t inKind)
{ return (uintptr_t)CP(inPlayer)->mUnitClassUpgrades[(long)inKind]; }
extern "C" uint64_t ee_screenscpp_civc_media_count(uintptr_t inPlayer)
{ return (uint64_t)ee_units_lists_len(CP(inPlayer)->mLists, EE_UNITS_LISTS_MEDIA); }
extern "C" uintptr_t ee_screenscpp_civc_purchased_count(uintptr_t inPlayer)
{ return CP(inPlayer)->mCivilization.GetPurchasedUpgrades().size(); }
extern "C" uintptr_t ee_screenscpp_civc_purchased_data(uintptr_t inPlayer)
{ return (uintptr_t)CP(inPlayer)->mCivilization.GetPurchasedUpgrades().data(); }
extern "C" uintptr_t ee_screenscpp_civc_tree(uintptr_t inPlayer) { return (uintptr_t)CP(inPlayer)->GetTechTree(); }
extern "C" void ee_screenscpp_civc_ids_clear() { EEScratch<std::vector<long>>(kScratchCivcIDs).clear(); }
extern "C" void ee_screenscpp_civc_ids_fill(uintptr_t inTree) { ((EETechTree*)inTree)->GetNodeIDs(EEScratch<std::vector<long>>(kScratchCivcIDs)); }
extern "C" uintptr_t ee_screenscpp_civc_ids_count() { return EEScratch<std::vector<long>>(kScratchCivcIDs).size(); }
extern "C" int64_t ee_screenscpp_civc_id_at(uintptr_t inIndex) { return (int64_t)EEScratch<std::vector<long>>(kScratchCivcIDs)[inIndex]; }
extern "C" uintptr_t ee_screenscpp_civc_node(uintptr_t inTree, int64_t inId)
{ return (uintptr_t)((EETechTree*)inTree)->Get((long)inId); }
extern "C" bool ee_screenscpp_civc_node_researched(uintptr_t inNode) { return CN(inNode)->mIsResearched; }
extern "C" bool ee_screenscpp_civc_node_obsolete(uintptr_t inNode) { return CN(inNode)->mIsObsolete; }
extern "C" bool ee_screenscpp_civc_node_available(uintptr_t inNode) { return CN(inNode)->mIsAvailable; }
extern "C" bool ee_screenscpp_civc_node_allowed(uintptr_t inNode) { return CN(inNode)->mIsAllowed; }
extern "C" int64_t ee_screenscpp_civc_civ_load(EeScreensShimText inFile)
{ return EELoadCivilization(EEShimString(inFile), EEGetCivilizationDirectory(false), EEScratch<UWideString>(kScratchCivcName), EEScratch<std::vector<EECivUpgrade>>(kScratchCivcLoaded)); }
extern "C" void ee_screenscpp_civc_loaded_clear() { EEScratch<std::vector<EECivUpgrade>>(kScratchCivcLoaded).clear(); }
extern "C" void ee_screenscpp_civc_name_clear() { EEScratch<UWideString>(kScratchCivcName) = UWideString(); }
extern "C" void ee_screenscpp_civc_name(void* inOut) { EEShimOut(inOut, EEScratch<UWideString>(kScratchCivcName)); }
extern "C" uint32_t ee_screenscpp_civc_name_length() { return (uint32_t)EEScratch<UWideString>(kScratchCivcName).GetLength(); }
extern "C" uintptr_t ee_screenscpp_civc_loaded_count() { return EEScratch<std::vector<EECivUpgrade>>(kScratchCivcLoaded).size(); }
extern "C" uint32_t ee_screenscpp_civc_loaded_at(uintptr_t inIndex) { return EEScratch<std::vector<EECivUpgrade>>(kScratchCivcLoaded)[inIndex]; }
extern "C" bool ee_screenscpp_civc_is_user_directory(EeScreensShimText inDirectory)
{ return EEShimString(inDirectory).Equals(EEGetCivilizationDirectory(true)); }
extern "C" uintptr_t ee_screenscpp_civc_civ(uintptr_t inPlayer) { return (uintptr_t)&CP(inPlayer)->mCivilization; }
extern "C" uintptr_t ee_screenscpp_civc_civ_purchased_count(uintptr_t inCiv)
{ return CV(inCiv)->mPurchasedUpgrades.size(); }
extern "C" uint32_t ee_screenscpp_civc_civ_purchased_at(uintptr_t inCiv, uintptr_t inIndex)
{ return CV(inCiv)->mPurchasedUpgrades[inIndex]; }
extern "C" uintptr_t ee_screenscpp_civc_civ_deferred_count(uintptr_t inCiv)
{ return CV(inCiv)->mDeferredUpgrades.size(); }
extern "C" uint32_t ee_screenscpp_civc_civ_deferred_at(uintptr_t inCiv, uintptr_t inIndex)
{ return CV(inCiv)->mDeferredUpgrades[inIndex]; }
extern "C" void ee_screenscpp_civc_civ_name(uintptr_t inCiv, void* inOut)
{ EEShimOutC(inOut, (const char*)CV(inCiv)->mName); }
extern "C" uint32_t ee_screenscpp_civc_civ_available(uintptr_t inCiv) { return (uint32_t)CV(inCiv)->mAvailablePoints; }
extern "C" uint32_t ee_screenscpp_civc_civ_max(uintptr_t inCiv) { return (uint32_t)CV(inCiv)->mMaxPoints; }
extern "C" bool ee_screenscpp_civc_civ_no_points(uintptr_t inCiv) { return CV(inCiv)->mNoPointsAdded; }
extern "C" bool ee_screenscpp_civc_civ_unchanged(uintptr_t inCiv) { return CV(inCiv)->mUnchanged; }
extern "C" bool ee_screenscpp_civc_civ_purchased_flag(uintptr_t inPlayer)
{ return CP(inPlayer)->mCivUpgradesPurchased; }
