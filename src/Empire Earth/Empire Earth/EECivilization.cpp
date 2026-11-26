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
#include "rust/game/civilization.h"   // round 10, lane D (ee_game civilization.rs)


// the six key statics' guarded chains 0x52c3eb..0x52c57d (guard byte 0x529a88)

// (the defaults, the points, the save, the purchases and the reapplied order: ee_game civilization.rs, the members
// inline in the class)

// Returns whether the civilization can afford an upgrade (always, when custom civilizations are off) and its cost
// against a list (by default the purchased upgrades): ee_tech civilization.rs
bool EECivilization::CanAfford(EECivUpgrade inUpgrade, unsigned long* outCost, std::vector<EECivUpgrade>* inList)
{
    return ee_tech_civ_can_afford(inUpgrade, this->List(inList).data(), this->List(inList).size(), this->mAvailablePoints,
                                  gWorld.mLobbyOptions.mAllowCustomCivs, outCost);
}

// Returns whether the civilization can afford every upgrade in a list, bought one after another
bool EECivilization::CanAffordAll(std::vector<EECivUpgrade>& inUpgrades)
{
    return ee_tech_civ_can_afford_all(this->mPurchasedUpgrades.data(), this->mPurchasedUpgrades.size(),
                                      inUpgrades.data(), inUpgrades.size(), this->mAvailablePoints,
                                      gWorld.mLobbyOptions.mAllowCustomCivs);
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

// (round 10, lane D: the civilization's cases are EEGameLog.cpp's EECore11OpB, round 11 lane B)
