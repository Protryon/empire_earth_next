// C:\EEX\Empire Earth\Empire Earth\EECivilization.cpp (name from the class and its keys "EECivilization-..."; no
// __FILE__, no RTTI: not polymorphic). 0x12c3eb..0x12cac8: the six class static keys' guarded chains (guard byte
// 0x529a88), the ctor, the upgrade purchase helpers (forwarders to gCivilizationManager), the accessors (ICF-folded
// with other 4-byte getters), Load, Save. The dtor 0x162677 (EEPlayer.cpp's) is inline in the class. Unit flags: the exe module set.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EECivilizationManager.h"
#include "rust/sim/tech.h"
#include "rust/game/gamecpp.h"   // round 10, lane D (ee_game gamecpp civilization.rs)

extern EEGameOptions& gGameOptions;

// the six key statics' guarded chains 0x52c3eb..0x52c57d (guard byte 0x529a88)

// (the defaults, the points, the save, the purchases and the reapplied order: ee_game gamecpp civilization.rs)

// Creates an unnamed civilization with no points or upgrades, marked unchanged
EECivilization::EECivilization()
{
    ee_gamecpp_civ_init(this);
}

// Returns whether the civilization can afford an upgrade (always, when custom civilizations are off) and its cost
// against a list (by default the purchased upgrades): ee_tech civilization.rs
bool EECivilization::CanAfford(EECivUpgrade inUpgrade, unsigned long* outCost, std::vector<EECivUpgrade>* inList)
{
    return ee_tech_civ_can_afford(inUpgrade, this->List(inList).data(), this->List(inList).size(), this->mAvailablePoints,
                                  gGameOptions.mAllowCustomCivs, outCost);
}

// Returns whether the civilization can afford every upgrade in a list, bought one after another
bool EECivilization::CanAffordAll(std::vector<EECivUpgrade>& inUpgrades)
{
    return ee_tech_civ_can_afford_all(this->mPurchasedUpgrades.data(), this->mPurchasedUpgrades.size(),
                                      inUpgrades.data(), inUpgrades.size(), this->mAvailablePoints,
                                      gGameOptions.mAllowCustomCivs);
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

// (round 10, lane D: the civilization's cases are EEGameLog.cpp's EECore11OpB, round 11 lane B)
