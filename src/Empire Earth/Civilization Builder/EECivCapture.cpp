// New (port), TEST ONLY: the civilization builder lane's capture records (ee/EECivCapture.h; tools/sim_capture.py).
// Reads the manager, the powers, the players and their class upgrades; changes nothing but the capture file (CVMG
// loads the premade civilizations' files through EELoadCivilization, as the game's choice form does). UI lane round
// 12: the records are Rust's (ee_game screens civ_capture.rs) over the one-statement shims below; the hooks are set to
// its entries here.
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
#include "ee/EETechCapture.h"
#include "ee/EECivCapture.h"
#include "ee/EEScreensShims.h"
#include "rust/sim/units.h"   // ee_units_lists_len
#include "rust/game/screens.h"   // ee_screens_civc_* (civ_capture.rs)

#define CR(r) ((EESimCaptureRecord*)(r))
#define CP(p) ((EEPlayer*)(p))
#define CN(n) ((EETechTreeNode*)(n))
#define CV(c) ((EECivilization*)(c))
static std::vector<long> sCivcTypes;            // a unit set's types (the original's collector's)
static std::vector<long> sCivcIDs;              // a tech tree's node ids
static UWideString sCivcName;                   // a premade file's name and upgrades as loaded
static std::vector<EECivUpgrade> sCivcLoaded;
// the unit types of a set, in its order
class EECivcCollect : public EEUnitSetFunctor {
public:
    virtual bool Execute(long inUnitType) { return sCivcTypes.push_back(inUnitType), true; }
};
// a temporary's address for the full expression (the set walker's functor argument)
template <class T> static T* EECivcTemp(T&& inValue) { return &inValue; }
extern "C" uintptr_t ee_screenscpp_civc_rec_new() { return (uintptr_t)new EESimCaptureRecord(0); }
extern "C" void ee_screenscpp_civc_rec_bytes(uintptr_t inRec, void* inOut)
{ ee_screens_shim_out(inOut, CR(inRec)->Data().data(), CR(inRec)->Data().size()); }
extern "C" void ee_screenscpp_civc_rec_free(uintptr_t inRec) { delete CR(inRec); }
extern "C" void ee_screenscpp_civc_rec_player(uintptr_t inRec, uintptr_t inPlayer)
{ EETechCapturePlayer(*CR(inRec), CP(inPlayer)); }
extern "C" void ee_screenscpp_civc_rec_upgrade(uintptr_t inRec, uintptr_t inUpgrade)
{ EETechCaptureUpgrade(*CR(inRec), (EEUnitClassUpgrade*)inUpgrade); }
extern "C" void ee_screenscpp_civc_rec_tree(uintptr_t inRec, uintptr_t inTree)
{ EETechCaptureTree(*CR(inRec), (EETechTree*)inTree); }
extern "C" uintptr_t ee_screenscpp_civc_player(int64_t inIndex) { return (uintptr_t)::gWorld.mPlayers[inIndex]; }
extern "C" float ee_screenscpp_civc_conversion(uintptr_t inPlayer) { return CP(inPlayer)->mConversionFactor; }
extern "C" float ee_screenscpp_civc_high_ground(uintptr_t inPlayer) { return CP(inPlayer)->mHighGroundBonus; }
extern "C" int64_t ee_screenscpp_civc_share(uintptr_t inPlayer) { return (int64_t)CP(inPlayer)->mShareBonus; }
extern "C" int64_t ee_screenscpp_civc_mining(uintptr_t inPlayer) { return (int64_t)CP(inPlayer)->mMiningLimit; }
extern "C" bool ee_screenscpp_civc_set_exists(int64_t inSet) { return gUnitSetManager.GetSet((long)inSet) != NULL; }
extern "C" void ee_screenscpp_civc_types_clear() { sCivcTypes.clear(); }
extern "C" void ee_screenscpp_civc_collect(int64_t inSet)
{ gUnitSetManager.ForEach((long)inSet, EECivcTemp(EECivcCollect())); }
extern "C" uintptr_t ee_screenscpp_civc_types_count() { return sCivcTypes.size(); }
extern "C" int64_t ee_screenscpp_civc_type_at(uintptr_t inIndex) { return (int64_t)sCivcTypes[inIndex]; }
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
extern "C" void ee_screenscpp_civc_ids_clear() { sCivcIDs.clear(); }
extern "C" void ee_screenscpp_civc_ids_fill(uintptr_t inTree) { ((EETechTree*)inTree)->GetNodeIDs(sCivcIDs); }
extern "C" uintptr_t ee_screenscpp_civc_ids_count() { return sCivcIDs.size(); }
extern "C" int64_t ee_screenscpp_civc_id_at(uintptr_t inIndex) { return (int64_t)sCivcIDs[inIndex]; }
extern "C" uintptr_t ee_screenscpp_civc_node(uintptr_t inTree, int64_t inId)
{ return (uintptr_t)((EETechTree*)inTree)->Get((long)inId); }
extern "C" bool ee_screenscpp_civc_node_researched(uintptr_t inNode) { return CN(inNode)->mIsResearched; }
extern "C" bool ee_screenscpp_civc_node_obsolete(uintptr_t inNode) { return CN(inNode)->mIsObsolete; }
extern "C" bool ee_screenscpp_civc_node_available(uintptr_t inNode) { return CN(inNode)->mIsAvailable; }
extern "C" bool ee_screenscpp_civc_node_allowed(uintptr_t inNode) { return CN(inNode)->mIsAllowed; }
extern "C" int64_t ee_screenscpp_civc_civ_load(EeScreensShimText inFile)
{ return EELoadCivilization(EEShimString(inFile), EEGetCivilizationDirectory(false), sCivcName, sCivcLoaded); }
extern "C" void ee_screenscpp_civc_loaded_clear() { sCivcLoaded.clear(); }
extern "C" void ee_screenscpp_civc_name_clear() { sCivcName = UWideString(); }
extern "C" void ee_screenscpp_civc_name(void* inOut) { EEShimOut(inOut, sCivcName); }
extern "C" uint32_t ee_screenscpp_civc_name_length() { return (uint32_t)sCivcName.GetLength(); }
extern "C" uintptr_t ee_screenscpp_civc_loaded_count() { return sCivcLoaded.size(); }
extern "C" uint32_t ee_screenscpp_civc_loaded_at(uintptr_t inIndex) { return sCivcLoaded[inIndex]; }
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

static EECivCaptureApply* EECivcBegin(EECivUpgrade inUpgrade, long inPlayer)
{
    return (EECivCaptureApply*)ee_screens_civc_apply_begin(inUpgrade, inPlayer);
}
static void EECivcEnd(EECivCaptureApply* inCapture, bool inResult) { ee_screens_civc_apply_end((void*)inCapture, inResult); }
static void EECivcLoad(const UString& inName, const UString& inDirectory, long inResult, const UWideString& inCivName,
                       const std::vector<EECivUpgrade>& inUpgrades)
{
    ee_screens_civc_load((const char*)inName, (const char*)inDirectory, inResult, (const char*)inCivName, inUpgrades.data(),
                         inUpgrades.size());
}
static void EECivcApplyFile(const UString& inName, EEPlayer* inPlayer, long inResult)
{
    ee_screens_civc_apply_file((const char*)inName, (uintptr_t)inPlayer, inResult);
}

// The game calls these through the header's hook pointers, set here before main: the game's units name no
// symbol of this TEST ONLY unit (tools/cpp_deps.py cuts; PORT_LEARNINGS "Cycle cuts")
static const bool sCaptureHooksSet = (EECivCaptureApplyBegin = EECivcBegin, EECivCaptureApplyEnd = EECivcEnd,
                                      EECivCaptureLoad = EECivcLoad, EECivCaptureApplyFile = EECivcApplyFile, true);
