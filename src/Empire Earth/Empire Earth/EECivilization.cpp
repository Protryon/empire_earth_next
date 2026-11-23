// C:\EEX\Empire Earth\Empire Earth\EECivilization.cpp (name from the class and its keys "EECivilization-..."; no
// __FILE__, no RTTI: not polymorphic). 0x12c3eb..0x12cac8: the six class static keys' guarded chains (guard byte
// 0x529a88), the ctor, the upgrade purchase helpers (forwarders to gCivilizationManager), the accessors (ICF-folded
// with other 4-byte getters), Load, Save. The dtor 0x162677 is EEPlayer.cpp's. Unit flags: the exe module set.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EECivilizationManager.h"
#include "rust/sim/tech.h"
#include "rust/game/gamecpp.h"   // round 10, lane D (ee_game gamecpp civilization.rs)

extern char gEmptyString[];                 // 0x4e5b98 (pooled "")
extern EECivilizationManager gCivilizationManager;
extern EEGameOptions& gGameOptions;

// the six key statics' guarded chains 0x52c3eb..0x52c57d (guard byte 0x529a88)

// (the defaults, the points, the save, the purchases and the reapplied order: ee_game gamecpp civilization.rs)

// Creates an unnamed civilization with no points or upgrades, marked unchanged
EECivilization::EECivilization()
{
    ee_gamecpp_civ_init(this);
}

// Returns an upgrade's cost: the base cost plus the step for each upgrade already bought in its category
long EECivilization::ComputeCost(long inBase, long inStep, long inCount)
{
    return inBase + inStep * inCount;
}

// Returns whether the civilization can afford an upgrade (always, when custom civilizations are off) and its cost
// against a list (by default the purchased upgrades): ee_tech civilization.rs
bool EECivilization::CanAfford(EECivUpgrade inUpgrade, unsigned long* outCost, std::vector<EECivUpgrade>* inList)
{
    std::vector<EECivUpgrade>& theList = inList != NULL ? *inList : this->mPurchasedUpgrades;
    return ee_tech_civ_can_afford(inUpgrade, theList.data(), theList.size(), this->mAvailablePoints,
                                  gGameOptions.mAllowCustomCivs, outCost);
}

// Returns whether the civilization can afford every upgrade in a list, bought one after another
bool EECivilization::CanAffordAll(std::vector<EECivUpgrade>& inUpgrades)
{
    return ee_tech_civ_can_afford_all(this->mPurchasedUpgrades.data(), this->mPurchasedUpgrades.size(),
                                      inUpgrades.data(), inUpgrades.size(), this->mAvailablePoints,
                                      gGameOptions.mAllowCustomCivs);
}

// Returns an upgrade's base cost (its level's bonus times 100), or 0 when unknown
long EECivilization::GetBaseCost(EECivUpgrade inUpgrade)
{
    return ee_tech_civ_base_cost(inUpgrade);
}

// Returns whether no points were ever added
bool EECivilization::GetNoPointsAdded()
{
    return this->mNoPointsAdded;
}

// Returns the extra cost per upgrade already bought in an upgrade's category
long EECivilization::GetExtraCost(EECivUpgrade inUpgrade)
{
    return gCivilizationManager.GetExtraCost(inUpgrade);
}

// Counts the upgrades of an upgrade's category in a list (by default the purchased upgrades)
long EECivilization::CountInCategory(EECivUpgrade inUpgrade, std::vector<EECivUpgrade>* inList)
{
    return gCivilizationManager.CountInCategory(inUpgrade, inList != NULL ? inList : &this->mPurchasedUpgrades);
}

// Returns whether the civilization is unchanged
bool EECivilization::GetUnchanged()
{
    return this->mUnchanged;
}

// Returns the points left to spend
long EECivilization::GetAvailablePoints()
{
    return this->mAvailablePoints;
}

// Returns the maximum points
long EECivilization::GetMaxPoints()
{
    return this->mMaxPoints;
}

// Returns the purchased upgrades
std::vector<EECivUpgrade>& EECivilization::GetPurchasedUpgrades()
{
    return this->mPurchasedUpgrades;
}

// Adds points to both the available and the maximum points
void EECivilization::AddPoints(long inAmount)
{
    ee_gamecpp_civ_add_points(this, inAmount);
}

// Loads the purchased upgrades, points, flags and name from a saved game (old versions: a narrow name); returns
// the first read error
long EECivilization::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_gamecpp_civ_load(this, inMajorVersion, inMinorVersion, &inFile);
}

// Applies an upgrade to a player, first paying for it and recording it when buying; returns whether it applied
bool EECivilization::Buy(EECivUpgrade inUpgrade, long inPlayer, bool inPurchase)
{
    return ee_gamecpp_civ_buy(this, inUpgrade, inPlayer, inPurchase);
}

// Applies a list of upgrades to a player, stopping at the first that fails
void EECivilization::AddUpgrades(std::vector<EECivUpgrade>& inUpgrades, long inPlayer, bool inFlag)
{
    ee_gamecpp_civ_add_upgrades(this, &inUpgrades, inPlayer, inFlag);
}

// Applies the purchased upgrades with attribute index 21 to the world (player 0)
void EECivilization::ApplyLevel21ToWorld()
{
    ee_gamecpp_civ_apply_level21(this);
}

// Reapplies the purchased upgrades to a player after loading, deferring those with attribute index 21; returns 13
// when one fails
long EECivilization::Reapply(EEPlayer* inPlayer, unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return ee_gamecpp_civ_reapply(this, inPlayer->mPlayerID);
}

// Applies the deferred upgrades to a player and forgets them; returns 13 when one fails
long EECivilization::ApplyDeferred(EEPlayer* inPlayer, unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    return ee_gamecpp_civ_apply_deferred(this, inPlayer->mPlayerID);
}

// Writes the purchased upgrades, points, flags and name to the saved game; returns the first error
long EECivilization::Save(FSFile& outFile)
{
    return ee_gamecpp_civ_save(this, &outFile);
}

// Sets the points left to spend
void EECivilization::SetPoints(long inPoints)
{
    this->mAvailablePoints = inPoints;
}

// Sets both the available and the maximum points
void EECivilization::SetPointsAndMax(long inEpoch)
{
    this->mMaxPoints = inEpoch;
    this->mAvailablePoints = inEpoch;
}

// Renames the civilization and marks it changed
void EECivilization::SetName(const UWideString& inName)
{
    this->mName = inName;
    this->mUnchanged = false;
}

// ------------------------------------------------------------------------- round 10, lane D: the civilization

int64_t EECore10OpD6(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEVCWonder.cpp
double EECore10OpD6F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

#define P64(x) ((int64_t)(intptr_t)(x))
#define CV ((EECivilization*)p)
#define LIST ((std::vector<EECivUpgrade>*)p)

// the C++ calls and members of the civilization's bodies in Rust (gamecpp civilization.rs): p the civilization
// unless noted, one statement each
int64_t EECore10OpD5(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::DvAvailable: return P64(&CV->mAvailablePoints);
    case O::DvMaximum: return P64(&CV->mMaxPoints);
    case O::DvNoPointsAdded: return P64(&CV->mNoPointsAdded);
    case O::DvUnchanged: return P64(&CV->mUnchanged);
    case O::DvPurchased: return P64(&CV->mPurchasedUpgrades);
    case O::DvDeferred: return P64(&CV->mDeferredUpgrades);
    case O::DvListCount: return (int64_t)LIST->size();
    case O::DvListAt: return (*LIST)[a[0]];
    case O::DvPurchasedPush: CV->mPurchasedUpgrades.push_back((EECivUpgrade)a[0]);
        break;
    case O::DvDeferredPush: CV->mDeferredUpgrades.push_back((EECivUpgrade)a[0]);
        break;
    case O::DvDeferredClear: CV->mDeferredUpgrades.clear();
        break;
    case O::DvNameEmpty: CV->mName = gEmptyString;
        break;
    case O::DvNameLoad: return CV->mName.Load(*(FSFile*)a[0]);
    case O::DvNameSave: return CV->mName.Save(*(FSFile*)a[0]);
    case O::DvUStrLoad: return ((UString*)p)->Load(*(FSFile*)a[0]);
    case O::DvNameNarrow: CV->mName = (const char*)*(UString*)a[0];
        break;
    case O::DvCanAfford: return CV->CanAfford((EECivUpgrade)a[0], (unsigned long*)a[1], NULL);
    case O::DvApplyUpgrade: return gCivilizationManager.ApplyUpgrade((EECivUpgrade)a[0], (long)a[1]);
    default: return EECore10OpD6(inOp, p, a, f);
    }
    return 0;
}

double EECore10OpD5F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    return EECore10OpD6F(inOp, p, a, f);
}
