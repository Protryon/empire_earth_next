// C:\EEX\Empire Earth\Civilization Builder\EECBCivilizationCreator.cpp (name from RTTI EECBCivilizationCreator; no
// __FILE__). 0x27c8b9..0x27c9db: the editable civilization of the creator (points to spend, not points left): the
// ctor and the four slots that differ. Its `return true` slots are ICF-folded into 0x60b1. EECBEditableCivilization
// starts at 0x27c9db. Unit flags: the exe's module set.
#include <windows.h>
#include <vector>
#include <algorithm>

#include "lle/LowLevelEngine.h"

#include "ee/EECivilization.h"
#include "ee/EECBCivilizationCreator.h"

// FUNCTION: EXE 0x67c8b9
// Creates the civilization creator's editable copy of a civilization; an unlimited one starts with 0 points
EECBCivilizationCreator::EECBCivilizationCreator(EECivilization* inCivilization, bool inIsUnlimited)
    : EECBEditableCivilization(inCivilization, false)
{
    this->mIsUnlimited = inIsUnlimited;
    if (inIsUnlimited)
        this->mPointsLeft = 0;
}

// FUNCTION: EXE 0x67c8e2
// Returns the points left to spend, always 0 in unlimited mode
unsigned long EECBCivilizationCreator::GetPointsLeft()
{
    if (this->mIsUnlimited)
        return 0;
    return EECBEditableCivilization::GetPointsLeft();
}

// FUNCTION: EXE 0x67c8f0
// Returns the maximum points; in unlimited mode the points spent so far
long EECBCivilizationCreator::GetMaxPoints()
{
    if (this->mIsUnlimited)
        return this->mPointsLeft;
    return EECBEditableCivilization::GetMaxPoints();
}

// FUNCTION: EXE 0x67c8ff
// Adds an upgrade if its cost is affordable (always in unlimited mode, where the cost counts up as spent)
bool EECBCivilizationCreator::AddUpgrade(EECivUpgrade inCount)
{
    bool theResult = false;
    if (inCount > 0) {
        unsigned long theCost = this->GetCost(inCount);
        if (this->mIsUnlimited) {
            this->mUpgrades.push_back(inCount);
            this->mPointsLeft += theCost;
            theResult = true;
        } else if (theCost <= this->GetPointsLeft()) {
            this->mUpgrades.push_back(inCount);
            this->mPointsLeft -= theCost;
            theResult = true;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x67c954
// Removes an upgrade and refunds its cost (unlimited mode: subtracts it from the points spent)
bool EECBCivilizationCreator::RemoveUpgrade(EECivUpgrade inUpgrade)
{
    bool theResult = false;
    std::vector<EECivUpgrade>::iterator theIterator = std::find(this->mUpgrades.begin(), this->mUpgrades.end(), inUpgrade);
    if (theIterator != this->mUpgrades.end()) {
        long theDelta = this->GetRefund(inUpgrade);
        if (this->mIsUnlimited)
            this->mPointsLeft -= theDelta;
        else
            this->mPointsLeft += theDelta;
        this->mUpgrades.erase(theIterator);
        theResult = true;
    }
    return theResult;
}
