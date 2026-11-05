// C:\EEX\Empire Earth\Civilization Builder\EECBEditableCivilization.cpp (name from RTTI; no __FILE__).
// 0x27c9db..0x27cd52: the civilization being edited in the builder (ctor, Apply, the cost/point slots, the helpers),
// then EECBCivilizationCreator.cpp's range ends before it (0x27c9db). The form events that follow (0x27cd62..) belong
// to the next unit. Unit flags: the exe's module set.
#include <windows.h>
#include <vector>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/UResourceFile.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EmpireEarth.h"
#include "ee/EECommunications.h"
#include "ee/EEMessage.h"
#include "ee/EECivilization.h"
#include "ee/EECivilizationManager.h"
#include "ee/EECBEditableCivilization.h"
#include "ee/EEStringIDs.h"
// FUNCTION: EXE 0x67c9db
// Creates the builder's editable copy of a civilization for a player, starting from its points and name
EECBEditableCivilization::EECBEditableCivilization(EECivilization* inCivilization, unsigned char inPlayer)
    : mCivilization(inCivilization), mPlayer(inPlayer)
{
    this->ResetToStart();
    this->mName = inCivilization->mName;
}

// FUNCTION: EXE 0x67ca2b
// Sends the chosen upgrades and name as a civilization-upgrade message (to the server in multiplayer, run
// directly otherwise); returns false when nothing was chosen
bool EECBEditableCivilization::Apply()
{
    UWideString theName(this->mName);
    bool theResult = false;
    if (this->HasChoices()) {
        NEMessage* theMessage = new EEMCivUpgradesRequested(this->mPlayer, this->mUpgrades, theName, 0);
        if (gCommunications->GetIsMultiplayerSession())
            gCommunications->ProcessLocalUserInput(theMessage, kSlotServer);
        else {
            theMessage->Execute();
            theMessage->Dereference();
        }
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x67cab7
// Returns the cost of an upgrade given how many of its category are already picked
long EECBEditableCivilization::GetCost(EECivUpgrade inUpgrade)
{
    long theFirst = this->mCivilization->GetBaseCost(inUpgrade);
    long theSecond = this->mCivilization->GetExtraCost(inUpgrade);
    return this->mCivilization->ComputeCost(theFirst, theSecond, this->PickedInCategory(inUpgrade));
}

// FUNCTION: EXE 0x67caf1
// Returns the refund for removing an upgrade: its cost with one fewer picked in its category
long EECBEditableCivilization::GetRefund(EECivUpgrade inUpgrade)
{
    long theFirst = this->mCivilization->GetBaseCost(inUpgrade);
    long theSecond = this->mCivilization->GetExtraCost(inUpgrade);
    unsigned long theCount = this->PickedInCategory(inUpgrade);
    if (theCount > 0)
        theCount--;
    return this->mCivilization->ComputeCost(theFirst, theSecond, theCount);
}

// FUNCTION: EXE 0x67cb30
// Resets the edit: default civilization name, the civilization's available points, no upgrades chosen
void EECBEditableCivilization::ResetToStart()
{
    UWideString theString;
    gResource->GetString(IDS_DEFAULT_CIVILIZATION, theString);
    this->mName = theString;
    this->mPointsLeft = this->mCivilization->GetAvailablePoints();
    this->mUpgrades.clear();
}

// FUNCTION: EXE 0x67cb83
// Counts the upgrades of the same category, chosen here and already owned by the civilization
unsigned long EECBEditableCivilization::PickedInCategory(EECivUpgrade inUpgrade)
{
    unsigned long theCount = gCivilizationManager.CountInCategory(inUpgrade, &this->mUpgrades);
    return this->mCivilization->CountInCategory(inUpgrade, NULL) + theCount;
}

// FUNCTION: EXE 0x67cbb0
// Tells whether the civilization may be edited: unchanged in multiplayer, current player still playing otherwise
bool EECBEditableCivilization::CanChange()
{
    bool theResult = false;
    if (gCommunications->GetIsMultiplayerSession()) {
        if (this->mCivilization->GetUnchanged())
            theResult = true;
    } else {
        theResult = ::gWorld.GetCurrentPlayer()->mGameResult == 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x67cbe6
// Returns the chosen upgrade ids followed by the civilization's purchased ones
std::vector<EECivUpgrade> EECBEditableCivilization::CopyUpgradeIDs()
{
    std::vector<EECivUpgrade> theResult;
    theResult = this->mUpgrades;
    std::vector<EECivUpgrade>::iterator theIterator = this->mCivilization->GetPurchasedUpgrades().begin();
    while (theIterator != this->mCivilization->GetPurchasedUpgrades().end()) {
        theResult.push_back(*theIterator);
        ++theIterator;
    }
    return theResult;
}

// FUNCTION: EXE 0x67cc4e
// Tells whether anything was chosen: an upgrade or a changed name
bool EECBEditableCivilization::HasChoices()
{
    return this->mUpgrades.size() > 0 || this->mName != this->mCivilization->mName;
}

// (no address of its own: ICF folded it into 0xb6fee)
unsigned long EECBEditableCivilization::GetPointsLeft()
{
    return this->mPointsLeft;
}

// FUNCTION: EXE 0x67cc7c
// Tells whether the civilization is still unchanged
bool EECBEditableCivilization::GetIsUnchanged()
{
    return this->mCivilization->GetUnchanged();
}

// FUNCTION: EXE 0x67cc84
// Returns the civilization's maximum points
long EECBEditableCivilization::GetMaxPoints()
{
    return this->mCivilization->GetMaxPoints();
}

// FUNCTION: EXE 0x67cc8c
// Returns the edited civilization name
UWideString* EECBEditableCivilization::GetName()
{
    return &this->mName;
}

// FUNCTION: EXE 0x67cc90
// Returns the civilization's purchased upgrades
std::vector<EECivUpgrade>& EECBEditableCivilization::GetCivilizationUpgrades()
{
    return this->mCivilization->GetPurchasedUpgrades();
}

// FUNCTION: EXE 0x67cc98
// Adds an upgrade and spends its cost if enough points are left
bool EECBEditableCivilization::AddUpgrade(EECivUpgrade inCount)
{
    bool theResult = false;
    if (inCount > 0) {
        unsigned long theCost = this->GetCost(inCount);
        if (theCost <= this->GetPointsLeft()) {
            this->mUpgrades.push_back(inCount);
            this->mPointsLeft -= theCost;
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x67ccd6
// Removes a chosen upgrade and refunds its points
bool EECBEditableCivilization::RemoveUpgrade(EECivUpgrade inUpgrade)
{
    bool theResult = false;
    std::vector<EECivUpgrade>::iterator theIterator = std::find(this->mUpgrades.begin(), this->mUpgrades.end(), inUpgrade);
    if (theIterator != this->mUpgrades.end()) {
        this->mPointsLeft += this->GetRefund(inUpgrade);
        this->mUpgrades.erase(theIterator);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x67cd52
// Sets the edited civilization name
void EECBEditableCivilization::SetName(const UWideString& inName)
{
    this->mName = inName;
}
