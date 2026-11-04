// C:\EEX\Empire Earth\AI Thread\EECPASGUProphet.cpp (file name inferred: no __FILE__). 0x908a..0xd1b6: the
// prophet's target screens and calamity analyzers 0x908a..0xb2be, the EECPASGUProphet class from 0xb2be, then the
// unit's COMDATs. The screens and analyzers are not a separate object: their COMDATs 0xc0a5..0xd150 sit in this range
#include <windows.h>
#include <vector>
#include <set>
#include <map>
#include <math.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPILocation.h"
#include "ee/EECPASTargetUnit.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIChokePoint.h"
#include "ee/EETown.h"
#include "ee/EECPASGUProphetCalamityAnalyzerBase.h"
#include "ee/EECPASGUProphet.h"
#include "ee/EECPASGUPCTargetIsUnitSetScreenBase.h"

#include "ee/EECPAITargetPriority.h"

struct EECPAITargetNearest {
    EECPAITargetNearest(const EETileCoordinate& inCenter) { this->mCenter = inCenter; }
    EECPAITargetNearest(const EECPAITargetNearest& inOther) : mCenter(inOther.mCenter) {}
    ~EECPAITargetNearest() {}
    bool operator()(EECPASTargetUnit* inFirst, EECPASTargetUnit* inSecond) const
    {
        EETileDistanceLess theFirst(inFirst->mUnit->GetTile());
        EETileDistanceLess theSecond(inSecond->mUnit->GetTile());
        return theFirst.DistanceSquared(this->mCenter) < theSecond.DistanceSquared(this->mCenter);
    }
    EETileCoordinate mCenter;
};



#include "ee/EEComputerPlayer.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUGCastCalamity.h"
#include "ee/EEUGLocation.h"
#include "LostDebug.h"   // synthetic, NOT original source (LOST-DEBUG: EE_LOST_DEBUG)
// 0x40c6cd (cdecl; only EECPASGUProphet::ReleaseAnalyzers calls it) (provisional name)
long DeleteAnalyzers(std::vector<EECPASGUProphetCalamityAnalyzerBase*>& inAnalyzers);

// FUNCTION: EXE 0x40908a
// Creates a target screen for a prophet's analyzer, optionally restricted to the analyzer's unit set
EECPASGUPCTargetIsUnitSetScreenBase::EECPASGUPCTargetIsUnitSetScreenBase(
    EECPASGUProphet* inProphet, EECPASGUProphetCalamityAnalyzerBase* inAnalyzer, bool inUseUnitSet)
    : mProphet(inProphet), mAnalyzer(inAnalyzer), mUseUnitSet(inUseUnitSet)
{
}

// FUNCTION: EXE 0x4090aa
// Accepts a target whose unit type is in the analyzer's unit set (any target when not restricted or without a set)
bool EECPASGUPCTargetIsUnitSetScreenBase::Passes(EECPASTargetUnit* inTarget)
{
    bool theResult = true;
    if (this->mUseUnitSet) {
        EEUnitSet* theSet = this->mAnalyzer->GetUnitSet();
        if (theSet != NULL)
            theResult = theSet->Contains(inTarget->mUnit->GetUnitClass()->GetUnitType());
    }
    return theResult;
}

// FUNCTION: EXE 0x4090d4
// Creates a calamity analyzer for one of the prophet's calamity technologies: looks up the calamity class, its
// target unit set and power cost; no score, target or exclusion yet
EECPASGUProphetCalamityAnalyzerBase::EECPASGUProphetCalamityAnalyzerBase(EECPASGUProphet* inProphet, long inTechID)
    : mTile(0, 0), mTechID(inTechID)
{
    this->mProphet = inProphet;
    this->mUnitSet = NULL;
    this->mCalamityIndex = this->mProphet->mUnit->mPlayer->GetTechTree()->Get(this->mTechID)->mData->mCalamity;
    USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(this->mCalamityIndex, this->mCalamityClass);
    this->mUnitSet = gUnitSetManager.GetSet(this->mCalamityClass->GetUnitSet());
    this->mCost = this->mCalamityClass->GetCost();
    this->mScore = 0.0f;
    this->mUnit = NULL;
    this->mB35 = false;
    this->mL28 = 0;
    this->mL2C = 0;
    this->mL30 = 0;
    this->mIsExcluded = false;
}

// FUNCTION: EXE 0x409162
// Scales a value by the prophet's power over the calamity's cost, clamped to 0.25..2 (unchanged for a free calamity)
float EECPASGUProphetCalamityAnalyzerBase::ScaleByPower(float inValue)
{
    float theResult = inValue;
    if (this->mCost != 0) {
        float theRatio = (float)this->mProphet->mUnit->mPower / this->mCost;
        theRatio = max(0.25f, theRatio);
        theRatio = min(2.0f, theRatio);
        theResult = theRatio * inValue;
    }
    return theResult;
}

// FUNCTION: EXE 0x4091b5
// Tells whether the prophet has enough power to cast the calamity
bool EECPASGUProphetCalamityAnalyzerBase::CanAfford()
{
    return this->mProphet->mUnit->mPower >= this->mCost;
}

// FUNCTION: EXE 0x4091cc
// Releases the held target unit and clears the score, exclusion and target tile
void EECPASGUProphetCalamityAnalyzerBase::Reset()
{
    this->ReleaseUnit();
    this->mScore = 0;
    this->mIsExcluded = false;
    this->mTile.mX = 0;
    this->mTile.mY = 0;
    this->mL28 = 0;
    this->mL2C = 0;
    this->mL30 = 0;
}

// FUNCTION: EXE 0x4091ef
// Returns the unit-set target nearest to the prophet, or NULL when there is none (stores nothing)
EEComplexUnit* EECPASGUProphetCalamityAnalyzerBase::PickTarget()
{
    std::vector<EECPASTargetUnit*> theTargets;
    EEComplexUnit* theResult = NULL;
    this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenBase(this->mProphet, this, true));
    if (theTargets.size() > 0) {
        std::sort(theTargets.begin(), theTargets.end(), EECPAITargetNearest(this->mProphet->GetUnit()->GetTile()));
        theResult = theTargets.front()->mUnit;
    }
    return theResult;
}

// FUNCTION: EXE 0x4092a2
// Fills a list with the sub group's targets that pass a screen, last target first
void EECPASGUProphetCalamityAnalyzerBase::Gather(std::vector<EECPASTargetUnit*>& outTargets,
                                                 EECPASGUPCTargetIsUnitSetScreenBase* inScreen)
{
    std::vector<EECPASTargetUnit*>& theTargets = this->mProphet->mSubGroup->mTargets;
    outTargets.clear();
    for (long i = theTargets.size() - 1; i >= 0; i--) {
        if (inScreen->Passes(theTargets[i]))
            outTargets.push_back(theTargets[i]);
    }
}

// FUNCTION: EXE 0x409303
// Releases the analyzer's reference to its target unit
void EECPASGUProphetCalamityAnalyzerBase::ReleaseUnit()
{
    if (this->mUnit != NULL)
        this->mUnit->Dereference();
    this->mUnit = NULL;
}

// FUNCTION: EXE 0x409318
// Multiplies the score by 1.75 when the prophet stands at a choke point; when asked, aims the volcano at the targets'
// centre inside it (or a random point of it), score 0 when neither is castable; outFound tells whether it aimed
float EECPASGUProphetCalamityAnalyzerVolcano::ApplyChokePointBonus(float inScore, bool inUseTargets, bool& outFound)
{
    std::vector<EETileCoordinate> theTiles;
    std::vector<EECPASTargetUnit*>& theTargets = this->mProphet->mSubGroup->mTargets;
    U2DPoint<long> theTile;
    EEAIChokePoint* theChokePoint = gContinentManager.GetChokePointAt(this->mProphet->mUnit);
    float theResult = inScore;
    outFound = false;
    if (theChokePoint != NULL) {
        // The original's float at 0x839740 is 1.75 (bytes 00 00 e0 3f), read from the frozen image.
        theResult = inScore * 1.75f;
        if (inUseTargets) {
            theTiles.resize(theTargets.size());
            for (long i = theTargets.size() - 1; i >= 0; i--) {
                (*(theTiles.begin() + i)).mX = (*(theTargets.begin() + i))->mUnit->mTileX;
                (*(theTiles.begin() + i)).mY = (*(theTargets.begin() + i))->mUnit->GetTileY();
            }
            theTile = U2DComputeCenterPoint(theTiles, false);
            if (!theChokePoint->Contains(theTile) && !theChokePoint->mHull.BoundedContains(theTile))
                theTile = theChokePoint->GetClosestHullPoint(theTile);
            GE3DPoint theLocation(theTile.GetX() + 0.5f, theTile.GetY() + 0.5f, 0.0f);
            bool theAllowed = this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(), theLocation, theTile.GetX(), theTile.GetY(), true, NULL) == 0;
            if (!theAllowed) {
                theTile = theChokePoint->GetRandomPoint();
                theLocation.SetXYZ(theTile.GetX() + 0.5f, theTile.GetY() + 0.5f, 0.0f);
                theAllowed = this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(), theLocation, theTile.GetX(), theTile.GetY(), true, NULL) == 0;
            }
            if (theAllowed) {
                this->mTile.mX = theTile.GetX();
                this->mTile.mY = theTile.GetY();
                outFound = true;   // the choke point aim stands (Evaluate then skips PickTarget)
            } else
                theResult = 0.0f;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x409518
// Scrambles the building unit-set targets at random and picks the last castable one; stores its tile and unit
// (NULL when none)
EEComplexUnit* EECPASGUProphetCalamityAnalyzerVolcano::PickTarget()
{
    std::vector<EECPASTargetUnit*> theTargets;
    EEComplexUnit* theResult = NULL;
    this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenOnlyBuildings(this->mProphet, this, true));
    if (!theTargets.empty())
        Fn4EEF9(theTargets, UMFloatToLong(gPhysics->mRandom.GenerateUnit()));
    while (!theTargets.empty()) {
        EETileCoordinate theTile(theTargets.back()->mUnit->mTileX, theTargets.back()->mUnit->mTileY);
        GE3DPoint theLocation(theTile.GetX() + 0.5f, theTile.GetY() + 0.5f, 0.0f);
        if (this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(), theLocation,
                                   theTile.GetX(), theTile.GetY(), true, theTargets.back()->mUnit) == 0) {
            this->mTile.mX = theTile.GetX();
            this->mTile.mY = theTile.GetY();
            this->SetUnit(theTargets.back()->mUnit);
            theResult = this->mUnit;
            break;
        }
        theTargets.pop_back();
    }
    return theResult;
}

// FUNCTION: EXE 0x409661
// Accepts a land building target, when restricted only if its unit type is in the analyzer's unit set
bool EECPASGUPCTargetIsUnitSetScreenOnlyBuildings::Passes(EECPASTargetUnit* inTarget)
{
    bool theResult = inTarget->mUnit->GetUnitClass()->IsLandBuilding();
    if (theResult && this->mUseUnitSet)
        theResult = this->mAnalyzer->GetUnitSet()->Contains(inTarget->mUnit->GetUnitClass()->GetUnitType());
    return theResult;
}

// FUNCTION: EXE 0x40969b
// Holds a unit as the analyzer's target, moving the unit reference from the old one
void EECPASGUProphetCalamityAnalyzerBase::SetUnit(EEComplexUnit* inUnit)
{
    if (inUnit == this->mUnit)
        return;
    if (this->mUnit != NULL)
        this->mUnit->Dereference();
    this->mUnit = inUnit;
    if (inUnit != NULL)
        inUnit->Reference();
}

// FUNCTION: EXE 0x4096c2
// Tells whether a volcano is worth considering: with enough targets or in danger, needs more than 8 targets, building
// targets, the prophet at a choke point, or the last target inside one of its owner's towns
bool EECPASGUProphetCalamityAnalyzerVolcano::IsApplicable()
{
    unsigned long theCount = this->mProphet->mSubGroup->mTargets.size();
    bool theResult = false;
    if (theCount >= this->mProphet->mSubGroup->GetNumUnits() || theCount >= 5 || this->mProphet->IsInDanger()) {
        bool theMany = theCount > 8 || this->mProphet->mSubGroup->mCounts.mNumBuildings > 0;
        if (theMany)
            theResult = true;
        else {
            theResult = gContinentManager.GetChokePointAt(this->mProphet->mUnit) != NULL;
            if (!theResult && this->mProphet->mSubGroup->mTargets.size() > 0) {
                EEComplexUnit* theUnit = this->mProphet->mSubGroup->mTargets.back()->mUnit;
                theResult = theUnit->mPlayer->GetTownOf(theUnit) != NULL;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x40975e
// Rates casting a volcano: random 0.45..0.95 times (aggression + 0.65), own-town penalty, choke point bonus; 0
// without a castable target; stores and returns the score
float EECPASGUProphetCalamityAnalyzerVolcano::Evaluate()
{
    std::vector<EECPASTargetUnit*> theTargets = this->mProphet->mSubGroup->mTargets;
    bool theFound = false;
    float theRandom = gPhysics->GetRandom(0.45f, 0.95f);
    float theFactor = this->mProphet->mAggression + 0.65f;
    float theScore = theRandom;
    theScore *= theFactor;
    theScore = this->ApplyWeakTownPenalty(theScore);
    theScore = this->ApplyChokePointBonus(theScore, true, theFound);
    if (theScore != 0.0f && !theFound && this->PickTarget() == NULL)
        theScore = 0.0f;
    this->mScore = theScore;
    return theScore;
}

// FUNCTION: EXE 0x409802
// Multiplies the score by 0.25 when the prophet stands in one of its own player's towns
float EECPASGUProphetCalamityAnalyzerVolcano::ApplyWeakTownPenalty(float inScore)
{
    float theResult = inScore;
    if (this->mProphet->mUnit->mPlayer->GetTownOf(this->mProphet->mUnit) != NULL)
        theResult = inScore * 0.25f;
    return theResult;
}

// FUNCTION: EXE 0x409831
// Multiplies the score by 1.35 when the first building target lies in a town and none of its owner's towns is
// worth more
float EECPASGUProphetCalamityAnalyzerEarthquake::ApplyLargestTownBonus(float inScore)
{
    std::vector<EECPASTargetUnit*> theTargets;
    float theResult = inScore;
    this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenOnlyBuildings(this->mProphet, this, true));
    if (theTargets.size() > 0) {
        EEComplexUnit* theUnit = theTargets[0]->mUnit;
        EETown* theTown = theUnit->mPlayer->GetTownOf(theUnit);
        if (theTown != NULL) {
            const EEPlayer* thePlayer = theTargets[0]->mUnit->mPlayer;
            unsigned long theValue = theTown->mValue;
            EETown* theLargest = theTown;
            for (long i = thePlayer->GetNumTowns() - 1; i >= 0; i--) {
                if (thePlayer->mTowns[i] != NULL && thePlayer->mTowns[i]->mValue > theValue) {
                    theLargest = NULL;
                    break;
                }
            }
            if (theLargest != NULL)
                theResult = inScore * 1.35f;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4098f0
// Returns the number of the player's towns
unsigned long EEPlayer::GetNumTowns() const
{
    return this->mTowns.size();
}

// FUNCTION: EXE 0x409909
// Picks a castable target: a wonder first (sorted nearest first, tried from the farthest), else the building on
// the buildings' centre tile, else the last castable building; stores its tile and unit
EEComplexUnit* EECPASGUProphetCalamityAnalyzerEarthquake::PickTarget()
{
    std::vector<EETileCoordinate> theTiles;
    std::vector<EECPASTargetUnit*> theTargets;
    EEComplexUnit* theResult = NULL;
    if (this->mProphet->mSubGroup->mCounts.mNumWonders > 0) {
        this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenOnlyWonders(this->mProphet, this, true));
        if (theTargets.size() > 0) {
            std::sort(theTargets.begin(), theTargets.end(), EECPAITargetNearest(this->mProphet->GetUnit()->GetTile()));
            while (theTargets.size() > 0) {
                EETileCoordinate theTile(theTargets.back()->mUnit->mTileX, theTargets.back()->mUnit->mTileY);
                GE3DPoint theLocation(theTile.GetX() + 0.5f, theTile.GetY() + 0.5f, 0.0f);
                if (this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(), theLocation, theTile.GetX(), theTile.GetY(), true, theTargets.back()->mUnit) == 0) {
                    this->mTile.mX = theTile.GetX();
                    this->mTile.mY = theTile.GetY();
                    this->SetUnit(theTargets.back()->mUnit);
                    theResult = this->mUnit;
                    break;
                }
                theTargets.pop_back();
            }
        }
    }
    if (theResult == NULL) {
        theTargets.clear();
        this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenOnlyBuildings(this->mProphet, this, true));
        if (theTargets.size() > 0) {
            theTiles.resize(theTargets.size());
            for (long i = theTargets.size() - 1; i >= 0; i--)
                theTiles[i] = theTargets[i]->mUnit->GetTile();
            U2DPoint<long> theTile = U2DComputeCenterPoint(theTiles, true);
            for (i = theTargets.size() - 1; i >= 0; i--) {
                EETileCoordinate theUnitTile = theTargets[i]->mUnit->GetTile();
                if (theUnitTile.mX != theTile.mX || theUnitTile.GetY() != theTile.GetY())
                    continue;
                EEComplexUnit* theUnit = theTargets[i]->mUnit;
                if (theUnit != NULL) {
                    GE3DPoint theLocation(theTile.mX + 0.5f, theTile.GetY() + 0.5f, 0.0f);
                    if (this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(), theLocation, theTile.mX, theTile.GetY(), true, theUnit) == 0) {
                        this->SetUnit(theUnit);
                        this->mTile.mX = theTile.mX;
                        this->mTile.mY = theTile.GetY();
                        theResult = this->mUnit;
                    }
                }
                break;
            }
            while (theTargets.size() > 0 && theResult == NULL) {
                EETileCoordinate theTile(theTargets.back()->mUnit->mTileX, theTargets.back()->mUnit->mTileY);
                GE3DPoint theLocation(theTile.GetX() + 0.5f, theTile.GetY() + 0.5f, 0.0f);
                if (this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(), theLocation, theTile.GetX(), theTile.GetY(), true, theTargets.back()->mUnit) == 0) {
                    this->mTile.mX = theTile.GetX();
                    this->mTile.mY = theTile.GetY();
                    this->SetUnit(theTargets.back()->mUnit);
                    theResult = this->mUnit;
                } else
                    theTargets.pop_back();
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x409d18
// Accepts a wonder target, when restricted only if its unit type is in the analyzer's unit set
bool EECPASGUPCTargetIsUnitSetScreenOnlyWonders::Passes(EECPASTargetUnit* inTarget)
{
    EETechTreeNode* theNode = inTarget->mUnit->GetTechTreeNode();
    bool theResult = theNode != NULL && theNode->mData->mKind == 4;
    if (theResult && this->mUseUnitSet)
        theResult = this->mAnalyzer->GetUnitSet()->Contains(inTarget->mUnit->GetUnitClass()->GetUnitType());
    return theResult;
}

// FUNCTION: EXE 0x409d5e
// Returns the owner's tech tree node of the technology that makes the unit's class
EETechTreeNode* EEUnit::GetTechTreeNode()
{
    return this->GetPlayer()->GetTechTree()->Get(this->GetUnitClass()->GetTechnology());
}

// FUNCTION: EXE 0x409d77
// Rates casting an earthquake: random 0.35..0.7, own-town penalty, largest-town bonus, x1.45 with wonder targets; 0
// without a castable target; stores and returns the score
float EECPASGUProphetCalamityAnalyzerEarthquake::Evaluate()
{
    std::vector<EECPASTargetUnit*> theTargets = this->mProphet->mSubGroup->mTargets;
    float theValue = this->ApplyLargestTownBonus(this->ApplyTownPenalty((gPhysics->GetRandom(0.35f, 0.7f) + 0.0f)));  // STAND-IN (owner 2026-11-01)
    if (this->mProphet->mSubGroup->mCounts.mNumWonders > 0)
        theValue *= 1.45f;
    float theScore = theValue;
    if (this->PickTarget() == NULL)
        theScore = 0.0f;
    this->mScore = theScore;
    return theScore;
}

// FUNCTION: EXE 0x409e07
// Picks a castable target: splash units first, then any unit-set target, each list priority-sorted and tried from
// the back; stores its tile and unit (NULL when none)
EEComplexUnit* EECPASGUProphetCalamityAnalyzerFire::PickTarget()
{
    std::vector<EECPASTargetUnit*> theTargets;
    EEComplexUnit* theResult = NULL;
    this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenOnlySplashUnits(this->mProphet, this, true));
    if (theTargets.size() > 0) {
        std::sort(theTargets.begin(), theTargets.end(), EECPAITargetPriority(this->mProphet->GetUnit()->GetTile()));
        while (theTargets.size() != 0) {
            EETileCoordinate theTile(theTargets.back()->mUnit->mTileX, theTargets.back()->mUnit->mTileY);
            GE3DPoint theLocation(theTile.GetX() + 0.5f, theTile.GetY() + 0.5f, 0.0f);
            if (this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(), theLocation, theTile.GetX(), theTile.GetY(), true, theTargets.back()->mUnit) == 0) {
                this->mTile.mX = theTile.GetX();
                this->mTile.mY = theTile.GetY();
                this->SetUnit(theTargets.back()->mUnit);
                theResult = this->mUnit;
                break;
            }
            theTargets.pop_back();
        }
    }
    if (theResult == NULL) {
        theTargets.clear();
        this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenBase(this->mProphet, this, true));
        if (theTargets.size() > 0) {
            std::sort(theTargets.begin(), theTargets.end(), EECPAITargetPriority(this->mProphet->GetUnit()->GetTile()));
            while (!theTargets.empty()) {
                long theX = theTargets.back()->mUnit->mTileX;
                long theY = theTargets.back()->mUnit->GetTileY();
                GE3DPoint theLocation(theX + 0.5f, theY + 0.5f, 0.0f);
                if (this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(), theLocation, theX, theY, true, theTargets.back()->mUnit) == 0) {
                    this->mTile.mX = theX;
                    this->mTile.mY = theY;
                    this->SetUnit(theTargets.back()->mUnit);
                    theResult = this->mUnit;
                    break;
                }
                theTargets.pop_back();
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x40a0ba
// Rates casting a fire: random 0.75..0.95, own-town penalty, splash bonus; 0 without a castable target; stores and
// returns the score
float EECPASGUProphetCalamityAnalyzerFire::Evaluate()
{
    std::vector<EECPASTargetUnit*> theTargets = this->mProphet->mSubGroup->mTargets;
    float theScore = this->ApplySplashBonus(this->ApplyTownPenalty((gPhysics->GetRandom(0.75f, 0.95f) + 0.0f)));  // STAND-IN (owner 2026-11-01)
    if (this->PickTarget() == NULL)
        theScore = 0.0f;
    this->mScore = theScore;
    return theScore;
}

// FUNCTION: EXE 0x40a131
// Raises the score 75% per nearby target of the unit-set target with the most targets near it
float EECPASGUProphetCalamityAnalyzerFire::ApplySplashBonus(float inScore)
{
    float theResult = inScore;
    long theMax = 0;
    std::vector<EECPASTargetUnit*>& theTargets = this->mProphet->mSubGroup->mTargets;
    for (long i = theTargets.size() - 1; i >= 0; i--) {
        if (!this->mUnitSet->Contains(theTargets[i]->mUnit->GetUnitClass()->GetUnitType()))
            continue;
        long theValue = theTargets[i]->mNumNearbyTargets;
        if (theValue > 0)
            theMax = theValue = theMax > theValue ? theMax : theValue;
    }
    EE_LOST_DEBUG(6);   // LOST-DEBUG: none: exact only (the same n in the Fire/Famine twins)
    if (theMax > 0)
        theResult = (theMax * 0.75f + 1.0f) * inScore;
    return theResult;
}

// FUNCTION: EXE 0x40a1d9
// Picks a castable target: splash units first, then ranged units, each list priority-sorted and tried from the
// back; stores its tile and unit (NULL when none)
EEComplexUnit* EECPASGUProphetCalamityAnalyzerPlague::PickTarget()
{
    std::vector<EECPASTargetUnit*> theTargets;
    EEComplexUnit* theResult = NULL;
    this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenOnlySplashUnits(this->mProphet, this, true));
    if (theTargets.size() > 0) {
        std::sort(theTargets.begin(), theTargets.end(), EECPAITargetPriority(this->mProphet->GetUnit()->GetTile()));
        while (theTargets.size() != 0) {
            EETileCoordinate theTile(theTargets.back()->mUnit->mTileX, theTargets.back()->mUnit->mTileY);
            GE3DPoint theLocation(theTile.GetX() + 0.5f, theTile.GetY() + 0.5f, 0.0f);
            if (this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(), theLocation, theTile.GetX(), theTile.GetY(), true, theTargets.back()->mUnit) == 0) {
                this->mTile.mX = theTile.GetX();
                this->mTile.mY = theTile.GetY();
                this->SetUnit(theTargets.back()->mUnit);
                theResult = this->mUnit;
                break;
            }
            theTargets.pop_back();
        }
    }
    if (theResult == NULL) {
        theTargets.clear();
        this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenRangedUnits(this->mProphet, this, true));
        if (theTargets.size() > 0) {
            std::sort(theTargets.begin(), theTargets.end(), EECPAITargetPriority(this->mProphet->GetUnit()->GetTile()));
            while (!theTargets.empty()) {
                long theX = theTargets.back()->mUnit->mTileX;
                long theY = theTargets.back()->mUnit->GetTileY();
                GE3DPoint theLocation(theX + 0.5f, theY + 0.5f, 0.0f);
                if (this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(), theLocation, theX, theY, true, theTargets.back()->mUnit) == 0) {
                    this->mTile.mX = theX;
                    this->mTile.mY = theY;
                    this->SetUnit(theTargets.back()->mUnit);
                    theResult = this->mUnit;
                    break;
                }
                theTargets.pop_back();
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x40a4a0
// Accepts a ranged target (range above 1), when restricted only if its unit type is in the analyzer's unit set
bool EECPASGUPCTargetIsUnitSetScreenRangedUnits::Passes(EECPASTargetUnit* inTarget)
{
    bool theResult = false;
    EEComplexUnit* theUnit = inTarget->mUnit;
    if (theUnit->mStats->mRange > 1.0f) {
        if (!this->mUseUnitSet || this->mAnalyzer->GetUnitSet()->Contains(theUnit->GetUnitClass()->GetUnitType()))
            theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x40a4e4
// Rates casting a plague: random factor, own-town penalty, citizen and splash bonuses; 0 without a castable target;
// stores and returns the score
float EECPASGUProphetCalamityAnalyzerPlague::Evaluate()
{
    std::vector<EECPASTargetUnit*> theTargets = this->mProphet->mSubGroup->mTargets;
    float theScore = this->ApplySplashBonus(this->ApplyCitizenBonus(this->ApplyStrongTownPenalty(this->ApplyRandomFactor(1.0f))));
    if (this->PickTarget() == NULL)
        theScore = 0.0f;
    this->mScore = theScore;
    return theScore;
}

// FUNCTION: EXE 0x40a554
// Multiplies the score by 0.02 when the prophet stands in one of its own player's towns
float EECPASGUProphetCalamityAnalyzerPlague::ApplyStrongTownPenalty(float inScore)
{
    float theResult = inScore;
    if (this->mProphet->mUnit->mPlayer->GetTownOf(this->mProphet->mUnit) != NULL)
        theResult = inScore * 0.02f;
    return theResult;
}

// FUNCTION: EXE 0x40a583
// Multiplies the score by a random 0.45..0.9
float EECPASGUProphetCalamityAnalyzerPlague::ApplyRandomFactor(float inScore)
{
    return gPhysics->GetRandom(0.45f, 0.9f) * inScore;
}

// FUNCTION: EXE 0x40a5a9
// Raises the score 5.5% per land or naval citizen among the sub group's targets
float EECPASGUProphetCalamityAnalyzerPlague::ApplyCitizenBonus(float inScore)
{
    float theResult = inScore;
    long theCount = 0;
    std::vector<EECPASTargetUnit*>& theTargets = this->mProphet->mSubGroup->mTargets;
    for (long i = theTargets.size() - 1; i >= 0; i--) {
        EEUnitClass* theClass = theTargets[i]->mUnit->GetUnitClass();
        if (theClass->mCategory == kUnitCategoryLandCitizen || theClass->mCategory == kUnitCategoryNavalCitizen)
            theCount++;
    }
    if (theCount > 0)
        theResult = (theCount * 0.055f + 1.0f) * inScore;
    return theResult;
}

// FUNCTION: EXE 0x40a61a
// Raises the score 15% per nearby target of the unit-set target with the most targets near it
float EECPASGUProphetCalamityAnalyzerPlague::ApplySplashBonus(float inScore)
{
    float theResult = inScore;
    long theMax = 0;
    std::vector<EECPASTargetUnit*>& theTargets = this->mProphet->mSubGroup->mTargets;
    for (long i = theTargets.size() - 1; i >= 0; i--) {
        if (!this->mUnitSet->Contains(theTargets[i]->mUnit->GetUnitClass()->GetUnitType()))
            continue;
        long theValue = theTargets[i]->mNumNearbyTargets;
        if (theValue > 0)
            theMax = theMax > theValue ? theMax : theValue;
    }
    if (theMax > 0)
        theResult = (theMax * 0.15f + 1.0f) * inScore;
    return theResult;
}

// FUNCTION: EXE 0x40a74c
// Picks a castable target: splash units first, then ranged units, each list priority-sorted and tried from the
// back; stores its tile and unit (NULL when none)
EEComplexUnit* EECPASGUProphetCalamityAnalyzerFamine::PickTarget()
{
    std::vector<EECPASTargetUnit*> theTargets;
    EEComplexUnit* theResult = NULL;
    this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenOnlySplashUnits(this->mProphet, this, true));
    if (theTargets.size() > 0) {
        std::sort(theTargets.begin(), theTargets.end(), EECPAITargetPriority(this->mProphet->GetUnit()->GetTile()));
        while (theTargets.size() != 0) {
            EETileCoordinate theTile(theTargets.back()->mUnit->mTileX, theTargets.back()->mUnit->mTileY);
            GE3DPoint theLocation(theTile.GetX() + 0.5f, theTile.GetY() + 0.5f, 0.0f);
            if (this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(), theLocation, theTile.GetX(), theTile.GetY(), true, theTargets.back()->mUnit) == 0) {
                this->mTile.mX = theTile.GetX();
                this->mTile.mY = theTile.GetY();
                this->SetUnit(theTargets.back()->mUnit);
                theResult = this->mUnit;
                break;
            }
            theTargets.pop_back();
        }
    }
    if (theResult == NULL) {
        theTargets.clear();
        this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenRangedUnits(this->mProphet, this, true));
        if (theTargets.size() > 0) {
            std::sort(theTargets.begin(), theTargets.end(), EECPAITargetPriority(this->mProphet->GetUnit()->GetTile()));
            while (!theTargets.empty()) {
                long theX = theTargets.back()->mUnit->mTileX;
                long theY = theTargets.back()->mUnit->GetTileY();
                GE3DPoint theLocation(theX + 0.5f, theY + 0.5f, 0.0f);
                if (this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(), theLocation, theX, theY, true, theTargets.back()->mUnit) == 0) {
                    this->mTile.mX = theX;
                    this->mTile.mY = theY;
                    this->SetUnit(theTargets.back()->mUnit);
                    theResult = this->mUnit;
                    break;
                }
                theTargets.pop_back();
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x40aa09
// Rates casting a famine: random 0.45..0.9, own-town penalty, citizen and splash bonuses; 0 without a castable
// target; stores and returns the score
float EECPASGUProphetCalamityAnalyzerFamine::Evaluate()
{
    std::vector<EECPASTargetUnit*> theTargets = this->mProphet->mSubGroup->mTargets;
    float theScore = this->ApplySplashBonus(this->ApplyCitizenBonus(this->ApplyTownPenalty((gPhysics->GetRandom(0.45f, 0.9f) + 0.0f))));  // STAND-IN (owner 2026-11-01)
    if (this->PickTarget() == NULL)
        theScore = 0.0f;
    this->mScore = theScore;
    return theScore;
}

// FUNCTION: EXE 0x40aa8b
// Multiplies the score by 0.05 when the prophet stands in one of its own player's towns
float EECPASGUProphetCalamityAnalyzerBase::ApplyTownPenalty(float inScore)
{
    float theResult = inScore;
    if (this->mProphet->mUnit->mPlayer->GetTownOf(this->mProphet->mUnit) != NULL)
        theResult = inScore * 0.05f;
    return theResult;
}

// FUNCTION: EXE 0x40aaba
// Raises the score 3.5% per land or naval citizen among the sub group's targets
float EECPASGUProphetCalamityAnalyzerFamine::ApplyCitizenBonus(float inScore)
{
    float theResult = inScore;
    long theCount = 0;
    std::vector<EECPASTargetUnit*>& theTargets = this->mProphet->mSubGroup->mTargets;
    for (long i = theTargets.size() - 1; i >= 0; i--) {
        EEUnitClass* theClass = theTargets[i]->mUnit->GetUnitClass();
        if (theClass->mCategory == kUnitCategoryLandCitizen || theClass->mCategory == kUnitCategoryNavalCitizen)
            theCount++;
    }
    if (theCount > 0)
        theResult = (theCount * 0.035f + 1.0f) * inScore;
    return theResult;
}

// FUNCTION: EXE 0x40ab2b
// Raises the score 25% per nearby target of the unit-set target with the most targets near it
float EECPASGUProphetCalamityAnalyzerFamine::ApplySplashBonus(float inScore)
{
    float theResult = inScore;
    long theMax = 0;
    std::vector<EECPASTargetUnit*>& theTargets = this->mProphet->mSubGroup->mTargets;
    for (long i = theTargets.size() - 1; i >= 0; i--) {
        if (!this->mUnitSet->Contains(theTargets[i]->mUnit->GetUnitClass()->GetUnitType()))
            continue;
        long theValue = theTargets[i]->mNumNearbyTargets;
        if (theValue > 0)
            theMax = theValue = theMax > theValue ? theMax : theValue;
    }
    EE_LOST_DEBUG(6);   // LOST-DEBUG: none: exact only (the same n in the Fire/Famine twins)
    if (theMax > 0)
        theResult = (theMax * 0.25f + 1.0f) * inScore;
    return theResult;
}

// FUNCTION: EXE 0x40abd3
// With ship targets in the sub group, adds every unit-set target's raw experience to the score (unscaled)
float EECPASGUProphetCalamityAnalyzerHurricane::AddTargetVeterancy(float inScore)
{
    std::vector<EECPASTargetUnit*> theTargets;
    float theScore = inScore;
    if (this->mProphet->mSubGroup->mCounts.mNumShips > 0) {
        this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenBase(this->mProphet, this, true));
        for (long i = theTargets.size() - 1; i >= 0; i--)
            theScore += theTargets[i]->mUnit->mExperience;
    }
    return theScore;
}

// FUNCTION: EXE 0x40ac63
// Picks a castable unit-set target: the one on the targets' centre tile, else the last castable one; stores its
// tile and unit (NULL when none)
EEComplexUnit* EECPASGUProphetCalamityAnalyzerHurricane::PickTarget()
{
    std::vector<EETileCoordinate> theTiles;
    std::vector<EECPASTargetUnit*> theTargets;
    EEComplexUnit* theResult = NULL;
    this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenBase(this->mProphet, this, true));
    if (theTargets.size() > 0) {
        theTiles.resize(theTargets.size());
        for (long i = theTargets.size() - 1; i >= 0; i--)
            theTiles[i] = theTargets[i]->mUnit->GetTile();
        U2DPoint<long> theTile = U2DComputeCenterPoint(theTiles, true);
        for (i = theTargets.size() - 1; i >= 0; i--) {
            EETileCoordinate theUnitTile = theTargets[i]->mUnit->GetTile();
            if (theUnitTile.GetX() != theTile.GetX() || theUnitTile.GetY() != theTile.GetY())
                continue;
            EEComplexUnit* theUnit = theTargets[i]->mUnit;
            if (theUnit != NULL) {
                GE3DPoint theLocation(theTile.GetX() + 0.5f, theTile.GetY() + 0.5f, 0.0f);
                if (this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(), theLocation, theTile.GetX(), theTile.GetY(), true, theUnit) == 0) {
                    this->SetUnit(theUnit);
                    this->mTile.mX = theTile.GetX();
                    this->mTile.mY = theTile.GetY();
                    theResult = this->mUnit;
                }
            }
            break;
        }
        while (theTargets.size() > 0 && theResult == NULL) {
            EETileCoordinate theTile(theTargets.back()->mUnit->mTileX, theTargets.back()->mUnit->mTileY);
            GE3DPoint theLocation(theTile.GetX() + 0.5f, theTile.GetY() + 0.5f, 0.0f);
            if (this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(), theLocation, theTile.GetX(), theTile.GetY(), true, theTargets.back()->mUnit) == 0) {
                this->mTile.mX = theTile.GetX();
                this->mTile.mY = theTile.GetY();
                this->SetUnit(theTargets.back()->mUnit);
                theResult = this->mUnit;
            } else
                theTargets.pop_back();
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x40af05
// Rates casting a hurricane: random 0.6..1, ship factor, plus target experience; 0 without a castable target;
// stores and returns the score
float EECPASGUProphetCalamityAnalyzerHurricane::Evaluate()
{
    std::vector<EECPASTargetUnit*> theTargets = this->mProphet->mSubGroup->mTargets;
    float theScore = this->AddTargetVeterancy(this->ApplyShipFactor((gPhysics->GetRandom(0.6f, 1.0f) + 0.0f)));  // STAND-IN (owner 2026-11-01)
    if (this->PickTarget() == NULL)
        theScore = 0.0f;
    this->mScore = theScore;
    return theScore;
}

// FUNCTION: EXE 0x40af78
// Multiplies the score by 0.28 per ship target when the sub group has ship targets
float EECPASGUProphetCalamityAnalyzerHurricane::ApplyShipFactor(float inScore)
{
    float theResult = inScore;
    long theCount = this->mProphet->mSubGroup->mCounts.mNumShips;
    if (theCount > 0 && this->mProphet->mSubGroup->mTargets.size() > 0)
        theResult = inScore * (theCount * 0.28f);
    return theResult;
}

// FUNCTION: EXE 0x40afb5
// Scores the unit-set targets and picks the best; stores its tile and unit, returns its score (0 and NULL when
// none scores)
float EECPASGUProphetCalamityAnalyzerNegativeMorale::PickBest(EEComplexUnit*& outUnit)
{
    std::multimap<float, EECPASTargetUnit*, std::greater<float> > theRanked;
    std::vector<EECPASTargetUnit*> theTargets;
    outUnit = NULL;
    this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenBase(this->mProphet, this, true));
    for (long i = theTargets.size() - 1; i >= 0; i--) {
        EEComplexUnit* theUnit = theTargets[i]->mUnit;
        EE_LOST_DEBUG(3); // LOST-DEBUG: none: exact only
        float theScore = this->ScoreTarget(theUnit);
        if (theScore != 0.0f) {
            EECPASTargetUnit* theTarget = theTargets[i];
            theRanked.insert(std::make_pair(theScore, theTarget));
        }
    }
    float theResult = 0.0f;
    if (theRanked.size() > 0) {
        outUnit = theRanked.begin()->second->mUnit;
        theResult = theRanked.begin()->first;
        this->mTile = outUnit->GetTile();
        this->SetUnit(outUnit);
    }
    return theResult;
}

// FUNCTION: EXE 0x40b0d0
// Rates the calamity: random 0.45..1 times 0.45 plus the best target's score; below 0.12 releases the target and
// excludes the analyzer; stores and returns the score
float EECPASGUProphetCalamityAnalyzerNegativeMorale::Evaluate()
{
    std::vector<EECPASTargetUnit*> theTargets = this->mProphet->mSubGroup->mTargets;
    this->mIsExcluded = false;
    EEComplexUnit* theUnit = NULL;
    float theScore = gPhysics->GetRandom(0.45f, 1.0f);
    float theValue = this->PickBest(theUnit);
    if (theUnit != NULL)
        theScore = theScore * 0.45f + theValue;
    else
        theScore = 0.0f;
    if (theScore < 0.12f) {
        this->ReleaseUnit();
        theScore = 0.0f;
        this->mIsExcluded = true;
    }
    this->mScore = theScore;
    return theScore;
}

// FUNCTION: EXE 0x40b169
// Scores a castable enemy: experience, special, naval building or calamity units, the sub group's main AI kind,
// being targeted by the prophet or an important unit, and attack strength (0 when not castable)
float EECPASGUProphetCalamityAnalyzerNegativeMorale::ScoreTarget(EEComplexUnit* inUnit)
{
    float theScore = 0.0f;
    if (inUnit != NULL) {
        if (this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(),
                GE3DPoint(inUnit->GetTileX() + 0.5f, inUnit->GetTileY() + 0.5f, 0.0f),
                inUnit->GetTileX(), inUnit->GetTileY(), true, inUnit) == 0) {
            EEUnitClass* theClass = inUnit->GetUnitClass();
            long theCount = min(27, inUnit->mExperience);
            theScore = theCount * 0.06f;
            if (theClass->mAIKind == kAIKindSpecial)
                theScore += 0.18f;
            if (theClass->mCategory == kUnitCategoryNavalBuilding)
                theScore += 0.1f;
            if (theClass->mHasCalamityPowers)
                theScore += 0.23f;
            EECPAttackSubGroup* theSubGroup = this->mProphet->mSubGroup;
            if (theSubGroup->mStats.mL34 == theClass->mAIKind)
                theScore += 0.33f;
            EECPAttackSubGroupUnit* theAttacker = theSubGroup->FindUnitTargetedBy(inUnit);
            if (theAttacker != NULL) {
                if (theAttacker == this->mProphet)
                    theScore += 0.18f;
                else if (theAttacker->mIsImportant)
                    theScore += 0.23f;
            }
            float theValue = inUnit->mStats->GetAttack() * 0.0032258064f;
            if (theValue >= 1.0f)
                theValue = 1.0f;
            theScore = theValue * 0.32f + theScore;
        }
    }
    return theScore;
}

// FUNCTION: EXE 0x40b2be
// Creates a prophet sub group unit for a unit, with no calamity analyzers yet
EECPASGUProphet::EECPASGUProphet(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup)
    : EECPAttackSubGroupUnit(inUnit, inSubGroup)
{
    this->mType = kASGUKindProphet;
}

// SYNTHETIC: EXE 0x40b2f5
// (the scalar deleting destructor; 0x1c bytes)

// FUNCTION: EXE 0x40b311
// Creates an empty prophet sub group unit (no unit yet; used when loading)
EECPASGUProphet::EECPASGUProphet(EECPAttackSubGroup* inSubGroup)
    : EECPAttackSubGroupUnit(inSubGroup)
{
    this->mType = kASGUKindProphet;
}

// FUNCTION: EXE 0x40b345
// Destroys the prophet sub group unit, deleting its calamity analyzers
EECPASGUProphet::~EECPASGUProphet()
{
    this->ReleaseAnalyzers();
}

// FUNCTION: EXE 0x40b367
// Deletes every calamity analyzer and empties the list; returns how many were deleted
long EECPASGUProphet::ReleaseAnalyzers()
{
    return DeleteAnalyzers(this->mAnalyzers);
}

// FUNCTION: EXE 0x40b375
// Creates the analyzer for a calamity type (fire, volcano, plague, hurricane, earthquake; leprosy uses the famine
// one; drop object, logic bomb, conscription and time warp the negative morale one); NULL for others
EECPASGUProphetCalamityAnalyzerBase* EECPASGUProphet::CreateAnalyzer(long inTechID, unsigned long inType)
{
    switch (inType) {
    case kCalamityDropObject:
    case kCalamityLogicBomb:
    case kCalamityConscription:
    case kCalamityTimeWarp:
        return new EECPASGUProphetCalamityAnalyzerNegativeMorale(this, inTechID);
    case kCalamityFire:
        return new EECPASGUProphetCalamityAnalyzerFire(this, inTechID);
    case kCalamityVolcano:
        return new EECPASGUProphetCalamityAnalyzerVolcano(this, inTechID);
    case kCalamityLeprosy:
        return new EECPASGUProphetCalamityAnalyzerFamine(this, inTechID);
    case kCalamityPlague:
        return new EECPASGUProphetCalamityAnalyzerPlague(this, inTechID);
    case kCalamityHurricane:
        return new EECPASGUProphetCalamityAnalyzerHurricane(this, inTechID);
    case kCalamityEarthquake:
        return new EECPASGUProphetCalamityAnalyzerEarthquake(this, inTechID);
    }
    return NULL;
}

// FUNCTION: EXE 0x40b4c4
// Tells whether an earthquake is worth considering: enough targets or the prophet in danger, and building targets
bool EECPASGUProphetCalamityAnalyzerEarthquake::IsApplicable()
{
    bool theResult = false;
    unsigned long theCount = this->mProphet->mSubGroup->mTargets.size();
    if (theCount >= this->mProphet->mSubGroup->GetNumUnits() || theCount >= 5 || this->mProphet->IsInDanger())
        theResult = this->mProphet->mSubGroup->mCounts.mNumBuildings > 0;
    return theResult;
}

// Fire's twin, the same test (ICF-folded into 0x40b4c4 above, no address of its own)
bool EECPASGUProphetCalamityAnalyzerFire::IsApplicable()
{
    bool theResult = false;
    unsigned long theCount = this->mProphet->mSubGroup->mTargets.size();
    if (theCount >= this->mProphet->mSubGroup->GetNumUnits() || theCount >= 5 || this->mProphet->IsInDanger())
        theResult = this->mProphet->mSubGroup->mCounts.mNumBuildings > 0;
    return theResult;
}

// FUNCTION: EXE 0x40b513
// Tells whether a famine is worth considering: enough targets or the prophet in danger, and plague targets
bool EECPASGUProphetCalamityAnalyzerFamine::IsApplicable()
{
    bool theResult = false;
    unsigned long theCount = this->mProphet->mSubGroup->mTargets.size();
    if (theCount >= this->mProphet->mSubGroup->GetNumUnits() || theCount >= 5 || this->mProphet->IsInDanger())
        theResult = this->mProphet->mSubGroup->mCounts.mNumPlagueTargets > 0;
    return theResult;
}

// Plague's twin, the same test (ICF-folded into 0x40b513 above, no address of its own)
bool EECPASGUProphetCalamityAnalyzerPlague::IsApplicable()
{
    bool theResult = false;
    unsigned long theCount = this->mProphet->mSubGroup->mTargets.size();
    if (theCount >= this->mProphet->mSubGroup->GetNumUnits() || theCount >= 5 || this->mProphet->IsInDanger())
        theResult = this->mProphet->mSubGroup->mCounts.mNumPlagueTargets > 0;
    return theResult;
}

// FUNCTION: EXE 0x40b562
// Tells whether a hurricane is worth considering: enough targets or the prophet in danger, and ship targets
bool EECPASGUProphetCalamityAnalyzerHurricane::IsApplicable()
{
    bool theResult = false;
    unsigned long theCount = this->mProphet->mSubGroup->mTargets.size();
    if (theCount >= this->mProphet->mSubGroup->GetNumUnits() || theCount >= 5 || this->mProphet->IsInDanger())
        theResult = this->mProphet->mSubGroup->mCounts.mNumShips > 0;
    return theResult;
}

// FUNCTION: EXE 0x40b5b1
// Returns the best target found by PickBest (NULL when none scores)
EEComplexUnit* EECPASGUProphetCalamityAnalyzerShieldBattery::PickTarget()
{
    EEComplexUnit* theUnit = NULL;
    this->PickBest(theUnit);
    return theUnit;
}

// FUNCTION: EXE 0x40b5c9
// Tells whether the calamity is worth considering: at least 2 targets, no fewer targets than units, or the prophet
// in danger
bool EECPASGUProphetCalamityAnalyzerNegativeMorale::IsApplicable()
{
    unsigned long theCount = this->mProphet->mSubGroup->mTargets.size();
    if (theCount >= 2 || theCount >= this->mProphet->mSubGroup->GetNumUnits() || this->mProphet->IsInDanger())
        return true;
    return false;
}

// FUNCTION: EXE 0x40b605
// Fills the analyzer list with one analyzer per calamity technology the prophet's class researches
void EECPASGUProphet::MakeAnalyzers()
{
    EECPASGUProphetCalamityAnalyzerBase* theAnalyzer = NULL;
    EEUnitClass* theClass = this->GetUnit()->GetUnitClass();
    EETechTree* theTree = this->GetUnit()->mPlayer->mTechnologies;
    this->mAnalyzers.clear();
    for (long i = theClass->GetNumResearchTechnologies() - 1; i >= 0; i--) {
        EETechTreeNode* theNode = theTree->Get(theClass->mResearchTechnologies[i]);
        if (theNode->mData->mKind != 5)
            continue;
        theAnalyzer = NULL;
        unsigned long theType = theNode->mData->mCalamity;
        long theTech = theNode->mData->mID;
        theAnalyzer = this->CreateAnalyzer(theTech, theType);
        if (theAnalyzer != NULL)
            this->mAnalyzers.push_back(theAnalyzer);
    }
}

// FUNCTION: EXE 0x40b68c
// Tells whether a calamity cast goal (the given one, else the unit's loaded goal) can still be cast at its location
// and target
bool EECPASGUProphet::CanStillCast(EEUGCastCalamity* inGoal)
{
    GE3DPoint theLocation(0.0f, 0.0f, 0.0f);
    bool theResult = false;
    EEUGCastCalamity* theGoal = inGoal;
    if (theGoal == NULL)
        theGoal = dynamic_cast<EEUGCastCalamity*>(this->mUnit->GetLoadGoal());
    EECalamityClass* theClass = NULL;
    if (theGoal != NULL) {
        long theCalamity = theGoal->mCalamityType;
        EETechTreeNode* theNode = this->mUnit->mPlayer->GetTechTree()->Get(theCalamity);
        if (theNode != NULL) {
            long theType = theNode->mData->mCalamity;
            USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(theType, theClass);
            if (theClass != NULL) {
                theGoal->GetGoalLocation(theLocation);
                EEUnit* theTarget = theGoal->mTarget;
                theResult = theClass->CanCastAt(this->mUnit, theLocation, (long)theLocation.GetX(), (long)theLocation.GetY(), true, theTarget) == 0;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x40b757
// Evaluates the affordable, applicable analyzers; unless the current cast stays good (same calamity and tile, or
// scored within 2.59x of the best), orders a cast of the best one; returns whether it did
bool EECPASGUProphet::Retarget()
{
    typedef std::multimap<float, EECPASGUProphetCalamityAnalyzerBase*, std::greater<float> > AnalyzerMap;
    AnalyzerMap theRanked;
    float theOldScore = 0.0f;
    bool theChanged = false;
    bool theKeepGoal = false;
    AnalyzerMap::iterator theIter;
    AnalyzerMap::iterator theOldIter;
    long theOldX = 0;
    long theOldY = 0;
    if (!(this->mUnit->mPower > 0) || this->mUnit->mTile == NULL)
        return theChanged;
    if (this->mAnalyzers.size() == 0)
        this->MakeAnalyzers();
    for (long i = this->mAnalyzers.size() - 1; i >= 0; i--) {
        if (!this->mAnalyzers[i]->CanAfford() || !this->mAnalyzers[i]->IsApplicable())
            continue;
        this->mAnalyzers[i]->Evaluate();
        if (this->mAnalyzers[i]->mScore > 0.0f)
            theRanked.insert(std::make_pair(this->mAnalyzers[i]->mScore,
                                                   this->mAnalyzers[i]));
    }
    if (theRanked.size() <= 0)
        return theChanged;
    EEUGCastCalamity* theGoal = dynamic_cast<EEUGCastCalamity*>(this->mUnit->mActiveGoal);
    if (theGoal != NULL && (theKeepGoal = this->CanStillCast(theGoal))) {
        theOldX = theGoal->mTileX;
        theOldY = theGoal->mTileY;
    }
    for (theIter = theRanked.begin(); theIter != theRanked.end() && !theChanged; theIter++) {
        EECPASGUProphetCalamityAnalyzerBase* theAnalyzer = theRanked.begin()->second;
        bool theSelect = !theKeepGoal;
        if (theKeepGoal) {
            if (theAnalyzer->mTechID == theGoal->mCalamityType) {
                if (theOldX == theAnalyzer->mTile.GetX() && theOldY == theAnalyzer->mTile.GetY())
                    break;
                theSelect = true;
            } else {
                for (theOldIter = theRanked.begin(); theOldIter != theRanked.end(); theOldIter++) {
                    if (theOldIter->second->mTechID == theGoal->mCalamityType) {
                        theOldScore = theOldIter->first;
                        break;
                    }
                }
                if (theOldScore != 0.0f) {
                    if (theOldScore * 2.59f < theAnalyzer->mScore)
                        theSelect = true;
                    else
                        break;
                }
            }
        }
        if (theSelect) {
            theChanged = true;
            EETileLocation theTile(theAnalyzer->mTile.GetX(), theAnalyzer->mTile.GetY());
            if (gWorld.mMap->GetTileAtLocation(theTile) != NULL) {
                theGoal = new EEUGCastCalamity(this->mUnit, theAnalyzer->mUnit, theAnalyzer->mTile.GetX(),
                                             theAnalyzer->mTile.GetY(), theAnalyzer->mTechID, true);
                if (!this->mSubGroup->GiveGoal(this->mUnit, theGoal, 0, kActionPrepareToMove) && theGoal != NULL)
                    delete theGoal;
            }
        }
    }
    return theChanged;
}

// FUNCTION: EXE 0x40ba62
// Flees like any sub group unit
void EECPASGUProphet::Flee()
{
    EECPAttackSubGroupUnit::Flee();
}

// FUNCTION: EXE 0x40ba67
// Sets the state; entering attack re-plans the calamity cast and schedules the next re-plan 10-20 s later, other
// states go through the base
void EECPASGUProphet::SetState(long inState, unsigned long inTime)
{
    this->mState = inState;
    if (inState != kASGUStateAttack) {
        EECPAttackSubGroupUnit::SetState(inState, inTime);
    } else {
        this->Retarget();
        this->mNextRetargetTime = gPhysics->GetRandom(10000L, 20000L) + inTime;
    }
}

// FUNCTION: EXE 0x40baab
// While the prophet is casting a calamity, orders it to a random point ahead (the goal is deleted if refused)
void EECPASGUProphet::MoveAwayFromCast()
{
    GE2DPoint theLocation(0.0f, 0.0f);
    EEUGCastCalamity* theCasting = dynamic_cast<EEUGCastCalamity*>(this->mUnit->GetGoal());
    if (theCasting == NULL || !EEGetRandomPointAhead(this->mUnit, &theLocation))
        return;
    EEUnitGoal* theGoal = new EEUGLocation(theLocation, true, true, false, 0.0f, false, 0);
    if (!this->mSubGroup->GiveGoal(this->mUnit, theGoal, 0, kActionPrepareToMove) && theGoal != NULL)
        delete theGoal;
}

// FUNCTION: EXE 0x40bb49
// Answers "has target" with whether the current calamity can still be cast; other conditions go to the base
bool EECPASGUProphet::CheckCondition(long inState, unsigned long inTime)
{
    switch (inState) {
    case kASGUConditionHasTarget:
        return this->CanStillCast(NULL);
    }
    return EECPAttackSubGroupUnit::CheckCondition(inState, inTime);
}

// FUNCTION: EXE 0x40bb69
// Runs the prophet's states: attack idles without targets, re-chooses when it cannot cast or is in danger, retreats
// or re-plans when due; retreat idles or re-chooses; contained when off the map; others use the base
void EECPASGUProphet::Update(unsigned long inTime)
{
    if (this->mAnalyzers.size() == 0)
        this->MakeAnalyzers();
    if (this->GetUnit()->mTileX == -10000 && this->mState != kASGUStateContained)
        this->mState = kASGUStateContained;
    switch (this->mState) {
    case kASGUStateAttack: {
        bool theFailed = !this->CheckCondition(kASGUConditionSubGroupHasTargets, inTime);
        if (theFailed)
            this->SetState(kASGUStateIdle, inTime);
        if (!theFailed) {
            bool theMove = !this->CheckCondition(kASGUConditionHasTarget, inTime);
            if (!theMove)
                theMove = this->CheckCondition(kASGUConditionInDanger, inTime);
            if (theMove)
                this->SetState(kASGUStateChooseAction, inTime);
            if (!theMove) {
                if (this->CheckCondition(kASGUConditionShouldRetreat, inTime))
                    this->SetState(kASGUStateRetreat, inTime);
                else if (this->CheckCondition(kASGUConditionRetargetDue, inTime) || this->CheckCondition(kASGUConditionInDanger, inTime))
                    this->SetState(kASGUStateAttack, inTime);
            }
        }
        break;
    }
    case kASGUStateRetreat: {
        bool theFailed = !this->CheckCondition(kASGUConditionSubGroupHasTargets, inTime);
        if (theFailed)
            this->SetState(kASGUStateIdle, inTime);
        if (!theFailed) {
            bool theCannotContinue = !this->CheckCondition(kASGUConditionShouldRetreat, inTime);
            if (theCannotContinue)
                this->SetState(kASGUStateChooseAction, inTime);
            if (!theCannotContinue && this->CheckCondition(kASGUConditionNearPathEnd, inTime))
                this->SetState(kASGUStateRetreat, inTime);
        }
        break;
    }
    default:
        EECPAttackSubGroupUnit::Update(inTime);
        break;
    }
}

// FUNCTION: EXE 0x40bcc1
// Tells whether the prophet can attack a target: some analyzer's unit set holds its type, else the base test
bool EECPASGUProphet::CanAttack(EECPASTargetUnit* inTarget)
{
    bool theResult = false;
    for (long i = this->mAnalyzers.size() - 1; i >= 0 && !theResult; i--) {
        EECPASGUProphetCalamityAnalyzerBase* theAnalyzer = this->mAnalyzers[i];
        if (!theAnalyzer->mB35 && theAnalyzer->GetUnitSet() != NULL)
            theResult = theAnalyzer->GetUnitSet()->Contains(inTarget->mUnit->GetUnitClass()->GetUnitType());
    }
    if (!theResult)
        theResult = EECPAttackSubGroupUnit::CanAttack(inTarget);
    return theResult;
}

// FUNCTION: EXE 0x40bd2b
// Tells whether to retreat: no usable calamity and a threat within 6 tiles or twice its range (squared); yes if it
// cannot hit back, else only when weak and the last attacker on it out-matches it
bool EECPASGUProphet::ShouldRetreat()
{
    bool theResult = true;
    bool theHasWeapon = this->GetUnit()->mStats->GetAttack() != 0.0f;
    long i;
    for (i = this->mAnalyzers.size() - 1; i >= 0 && theResult == true; i--) {
        if (this->mAnalyzers[i]->mIsExcluded)
            this->mAnalyzers[i]->mIsExcluded = false;
        else if (this->mAnalyzers[i]->CanAfford() && this->mAnalyzers[i]->IsApplicable())
            theResult = false;
        else
            theResult = true;
    }
    if (theResult) {
        theResult = false;
        for (i = this->mSubGroup->mTargets.size() - 1; i >= 0; i--) {
            EEComplexUnit* theUnit = (this->mSubGroup->mTargets[i])->mUnit;
            EEUnitClass* theClass = this->GetUnit()->GetUnitClass();
            long theDefense = theUnit->GetUnitClass()->mAmmoType;
            long theAttack = theClass->mFamily;
            if (theClass->CanBeAttacked() && gU4E8708[theAttack][theDefense] != 0.0f) {
                float theDistance = this->GetUnit()->DistanceSquaredTo(theUnit);
                if (theDistance <= 36.0f || theDistance <= theUnit->GetStats()->mRange + theUnit->GetStats()->mRange) {
                    theClass = theUnit->GetUnitClass();
                    theDefense = this->GetUnit()->GetUnitClass()->mAmmoType;
                    theAttack = theClass->mFamily;
                    theResult = true;
                    if (theClass->CanBeAttacked() && gU4E8708[theAttack][theDefense] != 0.0f)
                        goto CheckHealth;
                    theHasWeapon = false;
                }
            }
        }
        if (theResult && theHasWeapon) {
CheckHealth:
            float theHealth = (float)this->GetUnit()->mHitPoints / this->GetUnit()->mStats->mMaxHitPoints;
            if (theHealth <= 0.4f || theHealth <= 1.0f - this->mCourage) {
                float theDamage = this->GetUnit()->mStats->GetAttack();
                theResult = false;
                for (i = this->mSubGroup->mTargets.size() - 1; i >= 0; i--) {
                    EEComplexUnit* theUnit = (this->mSubGroup->mTargets[i])->mUnit;
                    EEUnitClass* theClass = this->GetUnit()->GetUnitClass();
                    long theDefense = theUnit->GetUnitClass()->mAmmoType;
                    long theAttack = theClass->mFamily;
                    if (theClass->CanBeAttacked() && gU4E8708[theAttack][theDefense] != 0.0f &&
                        theUnit->GetGoal() != NULL && theUnit->GetGoal()->mKind == kGoalAttackUnit &&
                        static_cast<EEUGAttackUnit*>(theUnit->GetGoal())->mTarget == this->GetUnit()) {
                        if (this->GetUnit()->GetUnitClass()->IsCounteredBy(theUnit->GetUnitClass()->GetUnitType()) ||
                            theDamage < theUnit->GetStats()->GetAttack() ||
                            (theUnit->GetStats()->GetAttack() == theDamage && this->GetUnit()->mHitPoints < theUnit->mHitPoints))
                            theResult = true;
                        else
                            theResult = false;
                    }
                }
            } else
                theResult = false;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x40c079
// Appends an analyzer to the prophet's analyzer list (std::vector push_back)

// FUNCTION: EXE 0x40c0a5
// Appends a target to a list of attack targets (std::vector push_back)

// FUNCTION: EXE 0x40c0d1
// Destroys a ranked analyzer multimap (score to analyzer, highest first), freeing every node

// FUNCTION: EXE 0x40c0fc
// Erases a range of entries from a ranked analyzer multimap; returns the iterator after them (std::_Tree erase)

// FUNCTION: EXE 0x40c16d
// Erases one entry from a short-keyed pointer map, returning the next iterator; throws out_of_range for end()
// (std::_Tree erase, a body shared through ICF)

// FUNCTION: EXE 0x40c41b
// Frees a subtree of a ranked analyzer multimap, node by node (std::_Tree _Erase)

// FUNCTION: EXE 0x40c44d
// Returns the calamity manager, creating it on first use (USingleton<EECalamityManager>::GetInstance)

// FUNCTION: EXE 0x40c473
// Looks up a calamity class by index (EECalamityManager::GetCalamityClass); returns false, leaving the output
// unset, when the index is out of range, and false with NULL when the entry is no calamity class

// FUNCTION: EXE 0x40c4d7
// Sorts targets nearest to a centre tile first (std::sort with EECPAITargetNearest)

// FUNCTION: EXE 0x40c5bb
// Returns the map tile at a tile location
EETile* EEMap::GetTileAtLocation(const EETileLocation& inLocation)
{
    return this->GetTile(inLocation.GetX(), inLocation.GetY());
}

// FUNCTION: EXE 0x40c5e9
// Sorts targets by attack priority (std::sort with EECPAITargetPriority)

// FUNCTION: EXE 0x40c6cd
// Deletes every analyzer in a list and empties it; returns how many were deleted
long DeleteAnalyzers(std::vector<EECPASGUProphetCalamityAnalyzerBase*>& inAnalyzers)
{
    long theCount = 0;
    for (std::vector<EECPASGUProphetCalamityAnalyzerBase*>::iterator i = inAnalyzers.begin(); i != inAnalyzers.end(); ++i) {
        delete *i;
        theCount++;
    }
    inAnalyzers.clear();
    return theCount;
}

// FUNCTION: EXE 0x40c6fd
// Tells whether a tile lies in the area: inside its hull and in its tile set (EEAIArea::Contains)

// FUNCTION: EXE 0x40c721
// Insertion-sorts targets nearest first (std::_Insertion_sort with EECPAITargetNearest)

// FUNCTION: EXE 0x40c754
// Sorts targets nearest first by partitioning, falling back to a heap sort (std::_Sort, EECPAITargetNearest)

// FUNCTION: EXE 0x40c85c
// Insertion-sorts a range without a lower bound check (std::_Unguarded_insertion_sort, EECPAITargetNearest)

// FUNCTION: EXE 0x40c89e
// Insertion-sorts targets by attack priority (std::_Insertion_sort with EECPAITargetPriority)

// FUNCTION: EXE 0x40c8d1
// Sorts targets by priority by partitioning, falling back to a heap sort (std::_Sort, EECPAITargetPriority)

// FUNCTION: EXE 0x40c9d9
// Insertion-sorts a range of targets (std::_Insertion_sort_1 with EECPAITargetNearest)

// FUNCTION: EXE 0x40ca8a
// Builds a heap of targets (std::make_heap with EECPAITargetNearest)

// FUNCTION: EXE 0x40cace
// Sorts a heap of targets (std::sort_heap with EECPAITargetNearest)

// FUNCTION: EXE 0x40cb1b
// Partitions targets around a median of three (std::_Unguarded_partition with EECPAITargetNearest)

// FUNCTION: EXE 0x40cc3b
// Moves one target back to its sorted place (std::_Unguarded_insert with EECPAITargetNearest)

// FUNCTION: EXE 0x40cc6e
// Insertion-sorts a range of targets (std::_Insertion_sort_1 with EECPAITargetPriority)

// FUNCTION: EXE 0x40cd1f
// Builds a heap of targets (std::make_heap with EECPAITargetPriority)

// FUNCTION: EXE 0x40cd63
// Sorts a heap of targets (std::sort_heap with EECPAITargetPriority)

// FUNCTION: EXE 0x40cdb0
// Builds a heap of targets bottom up (std::_Make_heap with EECPAITargetNearest)

// FUNCTION: EXE 0x40ce0d
// Moves the top target of a heap to its end (std::pop_heap with EECPAITargetNearest)

// FUNCTION: EXE 0x40ce40
// Builds a heap of targets bottom up (std::_Make_heap with EECPAITargetPriority)

// FUNCTION: EXE 0x40ce9d
// Moves the top target of a heap to its end (std::pop_heap with EECPAITargetPriority)

// FUNCTION: EXE 0x40ced0
// Sifts a target down a heap from a hole (std::_Adjust_heap with EECPAITargetNearest)

// FUNCTION: EXE 0x40cf4a
// Pops the top target of a heap (std::_Pop_heap_0 with EECPAITargetNearest)

// FUNCTION: EXE 0x40cfba
// Pops the top target of a heap (std::_Pop_heap_0 with EECPAITargetPriority)

// FUNCTION: EXE 0x40d02a
// Sifts a target up a heap (std::_Push_heap with EECPAITargetNearest)

// FUNCTION: EXE 0x40d079
// Moves a heap's top target to a slot and re-heaps the rest (std::_Pop_heap with EECPAITargetNearest)

// FUNCTION: EXE 0x40d0bd
// Sifts a target up a heap (std::_Push_heap with EECPAITargetPriority)

// FUNCTION: EXE 0x40d10c
// Moves a heap's top target to a slot and re-heaps the rest (std::_Pop_heap with EECPAITargetPriority)

// FUNCTION: EXE 0x40d150
// Returns the unit set at an index, or NULL when the index is out of range (EEUnitSetManager::GetSet)

// FUNCTION: EXE 0x40d16e
// Tells whether a point lies in the convex hull: inside its bounding rectangle, then the hull test
// (U2DConvexHull<long>::BoundedContains)

// FUNCTION: EXE 0x40d190
// Tells whether a point lies in the rectangle, edges included (U2DRectangle<long>::Contains)

// Template and inline COMDATs this unit already emits (identified by name)
// TEMPLATE: EXE 0x408282
// TEMPLATE: EXE 0x408a21
// TEMPLATE: EXE 0x408a38
// TEMPLATE: EXE 0x408a5c
// TEMPLATE: EXE 0x408b07
// TEMPLATE: EXE 0x408d98
// TEMPLATE: EXE 0x408dc7

// ICF-FOLDED: EXE 0x418aa9
// Tells whether the analyzer applies: always true by default
// (no address of its own: folded into the shared `mov al, 1; ret` body the vtable slot points at)
bool EECPASGUProphetCalamityAnalyzerBase::IsApplicable()
{
    return true;
}

// ICF-FOLDED: EXE 0x4e123d
// Does nothing by default
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EECPASGUProphetCalamityAnalyzerBase::Virtual1C()
{
}

// ICF-FOLDED: EXE 0x40b5b1
// Returns the best negative-morale target found by PickBest (NULL when none scores)
// (no address of its own: the same body as EECPASGUProphetCalamityAnalyzerShieldBattery::PickTarget, which ICF kept)
EEComplexUnit* EECPASGUProphetCalamityAnalyzerNegativeMorale::PickTarget()
{
    EEComplexUnit* theUnit = NULL;
    this->PickBest(theUnit);
    return theUnit;
}
