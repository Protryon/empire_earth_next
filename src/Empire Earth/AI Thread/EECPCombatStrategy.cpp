// C:\EEX\Empire Earth\AI Thread\EECPCombatStrategy.cpp (file name inferred: no __FILE__; RTTI class
// EECPCombatStrategy, vtable 0x43c0a4). A computer player's combat strategy: 0x77084..0x77718 (EECPCombatManager.cpp's
// code ends at 0x77084; the vector<EECPUnitRequirement> COMDATs close the unit at 0x77718, where
// EECPDefenseCommander.cpp's gatherer members start).
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"

#include "ee/EEPlayer.h"
#include "ee/EECPIntelligence.h"
#include "ee/EECPUnitRequirement.h"
#include "ee/EECPCombatStrategy.h"

// FUNCTION: EXE 0x477084
// Creates an empty strategy: no requirements, shares or unit counts, with favourites added
EECPCombatStrategy::EECPCombatStrategy()
    : mAirShare(0.0f), mSeaShare(0.0f), mLandShare(0.0f), mSpaceShare(0.0f), mTownID(0), mContinentID(0), mNumUnits(0), mNumRemaining(0), mNumAir(0), mNumSea(0), mNumLand(0),
      mNumSpace(0), mKind(0), mAddFavorites(true)
{
}

// SYNTHETIC: EXE 0x4770da
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4770f6
// Creates a copy of a strategy, requirements included
EECPCombatStrategy::EECPCombatStrategy(const EECPCombatStrategy& inOther)
    : mRequirements(inOther.mRequirements), mAirShare(inOther.mAirShare), mSeaShare(inOther.mSeaShare), mLandShare(inOther.mLandShare),
      mSpaceShare(inOther.mSpaceShare), mTownID(inOther.mTownID), mContinentID(inOther.mContinentID), mNumUnits(inOther.mNumUnits), mNumRemaining(inOther.mNumRemaining),
      mNumAir(inOther.mNumAir), mNumSea(inOther.mNumSea), mNumLand(inOther.mNumLand), mNumSpace(inOther.mNumSpace), mKind(inOther.mKind),
      mAddFavorites(inOther.mAddFavorites)
{
}

// FUNCTION: EXE 0x47716d
// Destroys the strategy and its requirements
EECPCombatStrategy::~EECPCombatStrategy()
{
}

// FUNCTION: EXE 0x47717b
// Clears the requirements, shares and unit counts and turns favourites back on
void EECPCombatStrategy::Reset()
{
    this->mRequirements.clear();
    this->mAirShare = 0.0f;
    this->mNumUnits = 0;
    this->mSeaShare = 0.0f;
    this->mKind = 0;
    this->mAddFavorites = true;
    this->mContinentID = 0;
    this->mLandShare = 0.0f;
    this->mTownID = 0;
    this->mNumAir = 0;
    this->mNumSea = 0;
    this->mSpaceShare = 0.0f;
    this->mNumLand = 0;
    this->mNumSpace = 0;
    this->mNumRemaining = 0;
}

// NEAR (80.6): registers (the original keeps inOwn in edi, theTotal/theLeft in memory) and the theMissing == theWanted
// test is not threaded past the `theWanted > count` branch in the original.
// FUNCTION: EXE 0x4771be
// Splits the strategy's units between air, sea, land and space by the configured shares (any rest goes to land);
// the owned-unit correction divides integers and is always 0, so the shares are never scaled
void EECPCombatStrategy::Calculate(EECPIntelligencePlayerFile* inOwn, EECPIntelligencePlayerFile* inOther)
{
    float theShare1 = 0.0f;
    float theShare2 = 0.0f;
    float theShare3 = 0.0f;
    float theShare4 = 0.0f;
    float theSum = 0.0f;
    long theCount = inOwn->mPlayer->GetPopulationLimit(0);
    long theTotal = this->mNumUnits;
    long theLeft = this->mNumUnits;

    if (this->mAirShare > 0.0f) {
        long theWanted = (long)(theCount * this->mAirShare);
        if (theWanted > 0) {
            long theMissing = theWanted > inOwn->mNumAirUnits ? theWanted - inOwn->mNumAirUnits : theWanted;
            float theFraction;
            if (theMissing != theWanted)
                theFraction = 1.0f - theMissing / theWanted;
            else
                theFraction = 1.0f;
            if (theFraction > 0.0f) {
                theShare1 = theFraction * this->mAirShare;
                theSum = theShare1;
            }
        }
    }
    if (this->mSeaShare > 0.0f) {
        long theWanted = (long)(theCount * this->mSeaShare);
        if (theWanted > 0) {
            long theMissing = theWanted > inOwn->mNumSeaUnits ? theWanted - inOwn->mNumSeaUnits : theWanted;
            float theFraction;
            if (theMissing != theWanted)
                theFraction = 1.0f - theMissing / theWanted;
            else
                theFraction = 1.0f;
            if (theFraction > 0.0f) {
                theShare2 = theFraction * this->mSeaShare;
                theSum += theShare2;
            }
        }
    }
    if (this->mLandShare > 0.0f) {
        long theWanted = (long)(theCount * this->mLandShare);
        if (theWanted > 0) {
            long theMissing = theWanted > inOwn->mNumLandUnits ? theWanted - inOwn->mNumLandUnits : theWanted;
            float theFraction;
            if (theMissing != theWanted)
                theFraction = 1.0f - theMissing / theWanted;
            else
                theFraction = 1.0f;
            if (theFraction > 0.0f) {
                theShare3 = theFraction * this->mLandShare;
                theSum += theShare3;
            }
        }
    }
    if (this->mSpaceShare > 0.0f) {
        long theWanted = (long)(theCount * this->mSpaceShare);
        if (theWanted > 0) {
            long theMissing = theWanted > inOwn->mNumSpaceUnits ? theWanted - inOwn->mNumSpaceUnits : theWanted;
            float theFraction;
            if (theMissing != theWanted)
                theFraction = 1.0f - theMissing / theWanted;
            else
                theFraction = 1.0f;
            if (theFraction > 0.0f) {
                theShare4 = theFraction * this->mSpaceShare;
                theSum += theShare4;
            }
        }
    }
    if (theSum > 0.0f) {
        this->mNumAir = (long)(theShare1 / theSum * theTotal);
        this->mNumSea = (long)(theShare2 / theSum * theTotal);
        this->mNumLand = (long)(theShare3 / theSum * theTotal);
        this->mNumSpace = (long)(theShare4 / theSum * theTotal);
        theLeft -= this->mNumAir + this->mNumSea + this->mNumLand + this->mNumSpace;
    }
    if (theLeft > 0)
        this->mNumLand += theLeft;
}

// FUNCTION: EXE 0x4773bf
// Returns a new copy of the strategy
EECPCombatStrategy* EECPCombatStrategy::Clone()
{
    EECPCombatStrategy* theClone = new EECPCombatStrategy;
    theClone->mRequirements = this->mRequirements;
    theClone->mNumUnits = this->mNumUnits;
    theClone->mKind = this->mKind;
    theClone->mAddFavorites = this->mAddFavorites;
    theClone->mAirShare = this->mAirShare;
    theClone->mSeaShare = this->mSeaShare;
    theClone->mLandShare = this->mLandShare;
    theClone->mSpaceShare = this->mSpaceShare;
    theClone->mNumAir = this->mNumAir;
    theClone->mNumSea = this->mNumSea;
    theClone->mNumLand = this->mNumLand;
    theClone->mNumSpace = this->mNumSpace;
    theClone->mNumRemaining = this->mNumRemaining;
    theClone->mContinentID = this->mContinentID;
    theClone->mTownID = this->mTownID;
    return theClone;
}

// Compiler-generated for this unit (names in analysis/exe/names.txt):
// TEMPLATE: EXE 0x477443
// TEMPLATE: EXE 0x47748d
// TEMPLATE: EXE 0x4775ff
// TEMPLATE: EXE 0x477615
// TEMPLATE: EXE 0x477650
// TEMPLATE: EXE 0x4776bb
// TEMPLATE: EXE 0x4776f4
