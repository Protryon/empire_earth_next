// C:\EEX\Empire Earth\Empire Earth\EECivilization.cpp (name from the class and its keys "EECivilization-..."; no
// __FILE__, no RTTI: not polymorphic). 0x12c3eb..0x12cac8: the six class static keys' guarded chains (guard byte
// 0x529a88), the ctor, the upgrade purchase helpers (forwarders to gCivilizationManager), the accessors (ICF-folded
// with other 4-byte getters), Load, Save. The dtor 0x162677 is EEPlayer.cpp's. Unit flags: the exe module set.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/UWideString.h"
#include "lle/FSFile.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEGameOptions.h"
#include "ee/EECivilizationManager.h"
#include "ee/EECivilization.h"

extern char gU4E5B98[];                     // 0x4e5b98 (pooled "")
extern EECivilizationManager gCivilizationManager;
extern EEGameOptions gGameOptions;

// the six key statics' guarded chains 0x52c3eb..0x52c57d (guard byte 0x529a88)
const UString EECivilization::sNumPurchasedUpgrades("EECivilization-NumPurchasedUpgrades");
const UString EECivilization::sPurchasedUpgrade("EECivilization-PurchasedUpgrade");
const UString EECivilization::sAvailablePoints("EECivilization-AvailablePoints");
const UString EECivilization::sMaxPoints("EECivilization-MaxPoints");
const UString EECivilization::sCivBuilderLastVisited("EECivilization-CivBuilderLastVisited");
const UString EECivilization::sIsDefaultCiv("EECivilization-IsDefaultCiv");
// SYNTHETIC: EXE 0x52c3eb
// SYNTHETIC: EXE 0x52c3f5
// SYNTHETIC: EXE 0x52c406
// SYNTHETIC: EXE 0x52c412
// SYNTHETIC: EXE 0x52c42e
// SYNTHETIC: EXE 0x52c438
// SYNTHETIC: EXE 0x52c449
// SYNTHETIC: EXE 0x52c455
// SYNTHETIC: EXE 0x52c471
// SYNTHETIC: EXE 0x52c47b
// SYNTHETIC: EXE 0x52c48c
// SYNTHETIC: EXE 0x52c498
// SYNTHETIC: EXE 0x52c4b4
// SYNTHETIC: EXE 0x52c4be
// SYNTHETIC: EXE 0x52c4cf
// SYNTHETIC: EXE 0x52c4db
// SYNTHETIC: EXE 0x52c4f7
// SYNTHETIC: EXE 0x52c501
// SYNTHETIC: EXE 0x52c512
// SYNTHETIC: EXE 0x52c51e
// SYNTHETIC: EXE 0x52c53a
// SYNTHETIC: EXE 0x52c544
// SYNTHETIC: EXE 0x52c555
// SYNTHETIC: EXE 0x52c561

// FUNCTION: EXE 0x52c57d
// Creates an unnamed civilization with no points or upgrades, marked unchanged
EECivilization::EECivilization()
{
    this->mName = gU4E5B98;
    this->mAvailablePoints = 0;
    this->mMaxPoints = 0;
    this->mNoPointsAdded = true;
    this->mUnchanged = true;
    this->mDeferredUpgrades.clear();
}

// FUNCTION: EXE 0x52c5d9
// Returns an upgrade's cost: the base cost plus the step for each upgrade already bought in its category
long EECivilization::ComputeCost(long inBase, long inStep, long inCount)
{
    return inBase + inStep * inCount;
}

// FUNCTION: EXE 0x52c5e9
// Returns whether the civilization can afford an upgrade (always, when custom civilizations are off) and its cost
bool EECivilization::CanAfford(unsigned short inUpgrade, unsigned long* outCost, std::vector<unsigned short>* inList)
{
    bool theResult = false;
    long theBase = this->GetBaseCost(inUpgrade);
    long theStep = this->GetExtraCost(inUpgrade);
    unsigned long theCost = this->ComputeCost(theBase, theStep, this->CountInCategory(inUpgrade, inList));
    if (outCost != NULL)
        *outCost = theCost;
    if (theCost <= this->mAvailablePoints || !gGameOptions.mAllowCustomCivs)
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x52c648
// Returns whether the civilization can afford every upgrade in a list, bought one after another
bool EECivilization::CanAffordAll(std::vector<unsigned short>& inUpgrades)
{
    std::vector<unsigned short> theList;
    unsigned short theUpgrade = 0;
    bool theResult = true;
    theList = this->mPurchasedUpgrades;
    for (std::vector<unsigned short>::iterator i = inUpgrades.begin(); i != inUpgrades.end(); i++) {
        theUpgrade = *i;
        theResult = this->CanAfford(theUpgrade, NULL, &theList);
        if (!theResult)
            break;
        theList.push_back(theUpgrade);
    }
    return theResult;
}

// FUNCTION: EXE 0x52c6bc
// Returns an upgrade's base cost (its level value times 100), or 0 when unknown
long EECivilization::GetBaseCost(unsigned short inUpgrade)
{
    float theValue = 0.0f;
    long theResult = 0;
    if (gCivilizationManager.LookupLevel(inUpgrade, NULL, &theValue)) {
        theValue *= 100.0f;
        theResult = UMFloatToLong(theValue);
    }
    return theResult;
}

// FUNCTION: EXE 0x52c6fa
// Returns whether no points were ever added
bool EECivilization::GetNoPointsAdded()
{
    return this->mNoPointsAdded;
}

// FUNCTION: EXE 0x52c6fe
// Returns the extra cost per upgrade already bought in an upgrade's category
long EECivilization::GetExtraCost(unsigned short inUpgrade)
{
    return gCivilizationManager.GetExtraCost(inUpgrade);
}

// FUNCTION: EXE 0x52c70f
// Counts the upgrades of an upgrade's category in a list (by default the purchased upgrades)
long EECivilization::CountInCategory(unsigned short inUpgrade, std::vector<unsigned short>* inList)
{
    std::vector<unsigned short>* theList = &this->mPurchasedUpgrades;
    if (inList != NULL)
        theList = inList;
    return gCivilizationManager.CountInCategory(inUpgrade, theList);
}

// FUNCTION: EXE 0x52c72c
// Returns whether the civilization is unchanged
bool EECivilization::GetUnchanged()
{
    return this->mUnchanged;
}

// FUNCTION: EXE 0x52c730
// Returns the points left to spend
long EECivilization::GetAvailablePoints()
{
    return this->mAvailablePoints;
}

// FUNCTION: EXE 0x52c734
// Returns the maximum points
long EECivilization::GetMaxPoints()
{
    return this->mMaxPoints;
}

// FUNCTION: EXE 0x52c738
// Returns the purchased upgrades
std::vector<unsigned short>& EECivilization::GetPurchasedUpgrades()
{
    return this->mPurchasedUpgrades;
}

// FUNCTION: EXE 0x52c73b
// Adds points to both the available and the maximum points
void EECivilization::AddPoints(long inAmount)
{
    this->mAvailablePoints += inAmount;
    this->mMaxPoints += inAmount;
    this->mNoPointsAdded = false;
}

// FUNCTION: EXE 0x52c74c
// Loads the purchased upgrades, points, flags and name from a saved game (old versions: a narrow name); returns
// the first read error
long EECivilization::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    UString theOldName;
    unsigned long theCount = 0;
    long theResult = inFile.Read(theCount);
    if (theResult != kErrorOK)
        return theResult;
    for (unsigned long i = 0; i < theCount && theResult == kErrorOK; i++) {
        unsigned long theValue;
        theResult = inFile.Read(theValue);
        if (theResult == kErrorOK) {
            unsigned short theUpgrade = (unsigned short)theValue;
            this->mPurchasedUpgrades.push_back(theUpgrade);
        }
    }
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mAvailablePoints);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mMaxPoints);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mNoPointsAdded);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mUnchanged);
    if (theResult == kErrorOK) {
        if (inMajorVersion == 0 && inMinorVersion < 0xd8) {
            theResult = theOldName.Load(inFile);
            this->mName = (const char*)theOldName;
        } else {
            theResult = this->mName.Load(inFile);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x52c847
// Applies an upgrade to a player, first paying for it and recording it when buying; returns whether it applied
bool EECivilization::Buy(unsigned short inUpgrade, long inPlayer, bool inPurchase)
{
    bool theResult = false;
    unsigned long theCost = 0;
    if (!inPurchase || this->CanAfford(inUpgrade, &theCost, NULL)) {
        if (inPurchase) {
            this->mAvailablePoints -= theCost;
            this->mPurchasedUpgrades.push_back(inUpgrade);
        }
        gCivilizationManager.ApplyUpgrade(inUpgrade, inPlayer);
        this->mUnchanged = false;
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x52c898
// Applies a list of upgrades to a player, stopping at the first that fails
void EECivilization::AddUpgrades(std::vector<unsigned short>& inUpgrades, long inPlayer, bool inFlag)
{
    bool theResult = true;
    for (std::vector<unsigned short>::iterator i = inUpgrades.begin(); i != inUpgrades.end() && theResult; i++)
        theResult = this->Buy(*i, inPlayer, inFlag);
}

// FUNCTION: EXE 0x52c8ce
// Applies the purchased upgrades with attribute index 21 to the world (player 0)
void EECivilization::ApplyLevel21ToWorld()
{
    std::vector<unsigned short>::iterator i = this->mPurchasedUpgrades.begin();
    while (true) {
        if (i == this->mPurchasedUpgrades.end())
            break;
        unsigned short theUpgrade = *i;
        unsigned long theCategory;
        unsigned long theIndex;
        gCivilizationManager.SplitAttribute(theUpgrade, &theCategory, &theIndex);
        if (theIndex == 0x15)
            gCivilizationManager.ApplyUpgrade(theUpgrade, 0);
        i++;
    }
}

// FUNCTION: EXE 0x52c916
// Reapplies the purchased upgrades to a player after loading, deferring those with attribute index 21; returns 13
// when one fails
long EECivilization::Reapply(EEPlayer* inPlayer, unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    unsigned short theUpgrade = 0;
    long theResult = 0;
    bool theApplied = true;
    unsigned long thePlayerID = inPlayer->mPlayerID;
    std::vector<unsigned short>::iterator i = this->mPurchasedUpgrades.begin();
    while (true) {
        if (i == this->mPurchasedUpgrades.end() || !theApplied)
            break;
        theUpgrade = *i;
        unsigned long theCategory;
        unsigned long theIndex;
        gCivilizationManager.SplitAttribute(theUpgrade, &theCategory, &theIndex);
        if (theIndex == 0x15)
            this->mDeferredUpgrades.push_back(theUpgrade);
        else
            theApplied = gCivilizationManager.ApplyUpgrade(theUpgrade, thePlayerID);
        i++;
    }
    if (!theApplied)
        theResult = 0xd;
    return theResult;
}

// FUNCTION: EXE 0x52c9a1
// Applies the deferred upgrades to a player and forgets them; returns 13 when one fails
long EECivilization::ApplyDeferred(EEPlayer* inPlayer, unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    bool theApplied = true;
    long theResult = 0;
    unsigned long thePlayerID = inPlayer->mPlayerID;
    std::vector<unsigned short>::iterator i = this->mDeferredUpgrades.begin();
    while (true) {
        if (!(i != this->mDeferredUpgrades.end() && theApplied))
            break;
        unsigned short theId = *i;
        theApplied = gCivilizationManager.ApplyUpgrade(theId, thePlayerID);
        i++;
    }
    if (!theApplied)
        theResult = 0xd;
    else
        this->mDeferredUpgrades.clear();
    return theResult;
}

// FUNCTION: EXE 0x52c9ee
// Writes the purchased upgrades, points, flags and name to the saved game; returns the first error
long EECivilization::Save(FSFile& outFile)
{
    unsigned long theCount = this->mPurchasedUpgrades.size();
    long theResult = outFile.Write(theCount);
    if (theResult != kErrorOK)
        return theResult;
    std::vector<unsigned short>::iterator i = this->mPurchasedUpgrades.begin();
    // stop at the first failed upgrade write (each result used to overwrite the previous one)
    while (theResult == kErrorOK) {
        if (i == this->mPurchasedUpgrades.end())
            break;
        unsigned long theValue = *i;
        theResult = outFile.Write(theValue);
        i++;
    }
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mAvailablePoints);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mMaxPoints);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mNoPointsAdded);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mUnchanged);
    if (theResult == kErrorOK)
        theResult = this->mName.Save(outFile);
    return theResult;
}

// FUNCTION: EXE 0x52ca99
// Sets the points left to spend
void EECivilization::SetPoints(long inPoints)
{
    this->mAvailablePoints = inPoints;
}

// FUNCTION: EXE 0x52caa3
// Sets both the available and the maximum points
void EECivilization::SetPointsAndMax(long inEpoch)
{
    this->mMaxPoints = inEpoch;
    this->mAvailablePoints = inEpoch;
}

// FUNCTION: EXE 0x52cab0
// Renames the civilization and marks it changed
void EECivilization::SetName(const UWideString& inName)
{
    this->mName = inName;
    this->mUnchanged = false;
}
