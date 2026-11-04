// C:\EEX\Empire Earth\AI Thread\EECPASGUMechProphet.cpp (file name inferred: no __FILE__). 0x65b0..0x908a: the
// mech prophet's calamity analyzers 0x65b0..0x7c45, then the EECPASGUMechProphet class from 0x7c45
#include <windows.h>
#include <vector>
#include <algorithm>
#include <set>

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
#include "lle/USingleton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EELocationCalamityClass.h"
#include "ee/EECCTeleport.h"
#include "ee/EECalamityManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPILocation.h"
#include "ee/EECPASTargetUnit.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EECPASGUPriest.h"
#include "ee/EECPASGUMechPriest.h"
#include "ee/EECPASGUProphetCalamityAnalyzerBase.h"
#include "ee/EECPASGUProphet.h"
#include "ee/EECPASGUPCTargetIsUnitSetScreenBase.h"
#include "ee/EEUnitSetManager.h"

#include "ee/EECPAITargetPriority.h"
#include "ee/EECPASGUMechProphet.h"
#include "LostDebug.h"   // synthetic, NOT original source (LOST-DEBUG: EE_LOST_DEBUG)
// FUNCTION: EXE 0x4065b0
// Scores the sub group's unit-set targets and picks the best for a time warp; stores its tile and unit, returns
// its score (0 and NULL when none scores)
float EECPASGUProphetCalamityAnalyzerTimeWarp::PickBest(EEComplexUnit*& outUnit)
{
    std::multimap<float, EECPASTargetUnit*, std::greater<float> > theRanked;
    std::vector<EECPASTargetUnit*> theTargets;
    outUnit = NULL;
    this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenBase(this->mProphet, this, true));
    for (long i = theTargets.size() - 1; i >= 0; i--) {
        EEComplexUnit* theUnit = theTargets[i]->mUnit;
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

// FUNCTION: EXE 0x4066c6
// Rates casting a time warp: random 0.45..1 times 0.35 plus the best target's score; below 0.5 releases the target
// and excludes the analyzer
float EECPASGUProphetCalamityAnalyzerTimeWarp::Evaluate()
{
    std::vector<EECPASTargetUnit*> theTargets = this->mProphet->mSubGroup->mTargets;
    this->mIsExcluded = false;
    EEComplexUnit* theUnit = NULL;
    float theScore = gPhysics->GetRandom(0.45f, 1.0f);
    float theValue = this->PickBest(theUnit);
    if (theUnit != NULL)
        theScore = theScore * 0.35f + theValue;
    else
        theScore = 0.0f;
    if (theScore < 0.5f) {
        this->ReleaseUnit();
        theScore = 0.0f;
        this->mIsExcluded = true;
    }
    this->mScore = theScore;
    return theScore;
}

// FUNCTION: EXE 0x40675f
// Scores a castable enemy for a time warp: veterancy, special or calamity units, the sub group's main AI kind,
// being targeted by the prophet or an important unit, and attack strength
float EECPASGUProphetCalamityAnalyzerTimeWarp::ScoreTarget(EEComplexUnit* inUnit)
{
    float theScore = 0.0f;
    if (inUnit != NULL) {
        if (this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(),
                GE3DPoint(inUnit->GetTileX() + 0.5f, inUnit->GetTileY() + 0.5f, 0.0f),
                inUnit->GetTileX(), inUnit->GetTileY(), true, inUnit) == 0) {
            EEUnitClass* theClass = inUnit->GetUnitClass();
            long theCount = min(25, inUnit->mExperience);
            theScore = theCount / 25.0f;
            if (theClass->mAIKind == kAIKindSpecial)
                theScore += 0.18f;
            if (theClass->mCategory == kUnitCategoryNavalBuilding)
                theScore += 0.1f;
            if (theClass->mHasCalamityPowers)
                theScore += 0.2f;
            EECPAttackSubGroup* theSubGroup = this->mProphet->mSubGroup;
            if (theSubGroup->mStats.mL34 == theClass->mAIKind)
                theScore += 0.3f;
            EECPAttackSubGroupUnit* theAttacker = theSubGroup->FindUnitTargetedBy(inUnit);
            if (theAttacker != NULL) {
                if (theAttacker == this->mProphet)
                    theScore += 0.13f;
                else if (theAttacker->mIsImportant)
                    theScore += 0.2f;
            }
            float theValue = inUnit->mStats->mAttack * 0.0033333334f;
            if (theValue >= 1.0f)
                theValue = 1.0f;
            theScore = theValue * 0.3f + theScore;
        }
    }
    return theScore;
}

// FUNCTION: EXE 0x4068b4
// Scores the sub group's targets and picks the best enemy to teleport next to; stores its tile and unit, returns
// its score (0 and NULL when none scores)
float EECPASGUProphetCalamityAnalyzerTeleport::PickBest(EEComplexUnit*& outUnit)
{
    std::multimap<float, EECPASTargetUnit*, std::greater<float> > theRanked;
    std::vector<EECPASTargetUnit*> theTargets;
    outUnit = NULL;
    this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenBase(this->mProphet, this, false));
    for (long i = theTargets.size() - 1; i >= 0; i--) {
        EEComplexUnit* theUnit = theTargets[i]->mUnit;
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

// FUNCTION: EXE 0x4069c9
// Picks the best own unit to teleport (passes MayGo, highest ScoreAlly) and holds it as the ally target; needs a
// chosen enemy; returns its score
float EECPASGUProphetCalamityAnalyzerTeleport::PickAlly(EEComplexUnit*& outUnit)
{
    std::multimap<float, EEComplexUnit*, std::greater<float> > theUnits;
    EE_LOST_DEBUG(9); // LOST-DEBUG: none: exact only
    float theResult = 0.0f;
    outUnit = NULL;
    if (this->mUnit != NULL) {
        for (long i = this->mProphet->mSubGroup->GetNumUnits() - 1; i >= 0; i--) {
            EEComplexUnit* theUnit = this->mProphet->mSubGroup->GetUnitUnit(i);
            if (!this->MayGo(this->mProphet->mSubGroup->GetUnitAt(i)))
                continue;
            float theScore = this->ScoreAlly(theUnit);
            if (theScore != 0.0f)
                theUnits.insert(std::multimap<float, EEComplexUnit*, std::greater<float> >::value_type(theScore,
                                                                                               theUnit));
        }
        theResult = 0.0f;
        if (theUnits.size() > 0) {
            outUnit = theUnits.begin()->second;
            theResult = theUnits.begin()->first;
            this->SetTarget(outUnit);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x406a9b
// Returns the number of units in the attack sub group
// (EECPAttackSubGroup::GetNumUnits: inline in ee/EECPAttackSubGroup.h)

// FUNCTION: EXE 0x406aae
// Returns the game unit of the sub group unit at an index, or NULL when the index is out of range
EEComplexUnit* EECPAttackSubGroup::GetUnitUnit(unsigned long inIndex) const
{
    return inIndex < this->mUnits.size() ? this->mUnits[inIndex]->mUnit : 0;
}

// FUNCTION: EXE 0x406ad7
// Returns the sub group unit at an index, or NULL when the index is out of range
EECPAttackSubGroupUnit* EECPAttackSubGroup::GetUnitAt(unsigned long inIndex) const
{
    return inIndex < this->mUnits.size() ? this->mUnits[inIndex] : 0;
}

// FUNCTION: EXE 0x406afd
// Holds a unit as the analyzer's ally target, moving the unit reference from the old one
void EECPASGUProphetCalamityAnalyzerTeleport::SetTarget(EEComplexUnit* inUnit)
{
    if (inUnit == this->mTarget)
        return;
    if (this->mTarget != NULL)
        this->mTarget->Dereference();
    this->mTarget = inUnit;
    if (inUnit != NULL)
        inUnit->Reference();
}

// ShieldBattery's twin, the same swap (ICF-folded into 0x406afd above, no address of its own)
void EECPASGUProphetCalamityAnalyzerShieldBattery::SetTarget(EEComplexUnit* inUnit)
{
    if (inUnit == this->mTarget)
        return;
    if (this->mTarget != NULL)
        this->mTarget->Dereference();
    this->mTarget = inUnit;
    if (inUnit != NULL)
        inUnit->Reference();
}

// FUNCTION: EXE 0x406b24
// Tells whether a unit can be teleported to the target tile: it can stand there, or a placement is found within
// the calamity's placement range
bool EECPASGUProphetCalamityAnalyzerTeleport::CanPlace(EEComplexUnit* inUnit)
{
    GE3DPoint thePoint;
    GETransformation theTransformation;
    bool theResult = true;
    if (!EECanStandOn(this->mTile, inUnit)) {
        long theRange = static_cast<EECCTeleport*>(this->mCalamityClass)->mPlacementRange;
        long theSize = this->mUnit->mUnitClass->GetTileSize();
        long theY = this->mTile.mY;
        long theX = this->mTile.GetX();
        theResult = EEFindPlacement(&thePoint, inUnit, NULL, theX, theY, theSize, theRange, 1, 1, 0, 0, &theTransformation, NULL);
    }
    return theResult;
}

// FUNCTION: EXE 0x406b9e
// Tells whether an own unit may be teleported: alive, enabled, on the map, able to damage the enemy, outside its
// range of it, brave enough, not a calamity unit or hero, armed, and placeable at the target
bool EECPASGUProphetCalamityAnalyzerTeleport::MayGo(EECPAttackSubGroupUnit* inUnit)
{
    bool theResult = false;
    EEComplexUnit* theUnit = inUnit->mUnit;
    if (theUnit->mHitPoints > 0 && !theUnit->mIsDisabled && theUnit->GetTileX() != -10000) {
        EEUnitClass* theOwnClass = this->mUnit->mUnitClass;
        EEUnitClass* theClass = theUnit->mUnitClass;
        long theDefense = theClass->mAmmoType;
        long theAttack = theOwnClass->mFamily;
        if (theOwnClass->CanBeAttacked() && gU4E8708[theAttack][theDefense] != 0.0f) {
            float theDistance = inUnit->GetUnit()->DistanceSquaredTo(this->GetUnit());
            float theRange = inUnit->mUnit->mStats->mRange;
            if (theDistance >= theRange * theRange && inUnit->mCourage >= 0.5f && !theClass->mHasCalamityPowers &&
                theClass->mAIKind != kAIKindHero && theClass->mAttack != 0)
                theResult = this->CanPlace(theUnit);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x406c84
// Rates teleporting an own unit next to an enemy (random plus enemy and ally scores); at 0.8 or more the ally
// becomes the unit to cast on, else both are released and the analyzer is excluded
float EECPASGUProphetCalamityAnalyzerTeleport::Evaluate()
{
    std::vector<EECPASTargetUnit*> theTargets = this->mProphet->mSubGroup->mTargets;
    this->mIsExcluded = false;
    EEComplexUnit* theUnit = NULL;
    float theScore = gPhysics->GetRandom(0.65f, 1.0f);
    float theValue = this->PickBest(theUnit);
    if (theUnit != NULL) {
        theUnit = NULL;
        theScore = theScore * 0.35f;
        theScore += theValue * 0.35f;
        theValue = this->PickAlly(theUnit);
        if (theUnit != NULL)
            theScore += theValue * 0.35f;
        else
            theScore = 0.0f;
    } else
        theScore = 0.0f;
    if (theScore < 0.8f) {
        this->ReleaseUnit();
        if (this->mTarget != NULL)
            this->mTarget->Dereference();
        this->mTarget = NULL;
        theScore = 0.0f;
        this->mIsExcluded = true;
    } else {
        this->SetUnit(this->mTarget);
        if (this->mTarget != NULL)
            this->mTarget->Dereference();
        this->mTarget = NULL;
    }
    this->mScore = theScore;
    return theScore;
}

// FUNCTION: EXE 0x406d6d
// Scores a castable enemy as a teleport destination: veterancy, special, calamity, priest and splash units,
// distance from the prophet (squared distance against its squared range), and the prophet being in its range
float EECPASGUProphetCalamityAnalyzerTeleport::ScoreTarget(EEComplexUnit* inUnit)
{
    float theScore = 0.0f;
    if (inUnit != NULL) {
        if (this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(),
                GE3DPoint(inUnit->GetTileX() + 0.5f, inUnit->GetTileY() + 0.5f, 0.0f),
                inUnit->GetTileX(), inUnit->GetTileY(), true, inUnit) == 0) {
            EEUnitClass* theClass = inUnit->GetUnitClass();
            long theCount = min(25, inUnit->mExperience);
            theScore = theCount * 0.05f;
            if (theClass->mAIKind == kAIKindSpecial)
                theScore += 0.2f;
            if (theClass->mHasCalamityPowers)
                theScore += 0.25f;
            if (theClass->IsPriest())
                theScore += 0.15f;
            if (inUnit->mStats->mAreaEffect > 1.0f)
                theScore += 0.18f;
            float theDistance = this->mProphet->mUnit->DistanceSquaredTo(inUnit);
            float theRange = this->mProphet->mUnit->mStats->mRange;
            theRange *= theRange;   // squared, as the squared distance (was range + range)
            if (theDistance >= theRange && theRange != 0.0f) {
                float theFactor = 1.0f - theRange / theDistance;
                if (theFactor >= 1.0f)
                    theFactor = 1.0f;
                if (theFactor <= 0.1f)
                    theFactor = 0.1f;
                theScore += theFactor * 0.35f;
            }
            theRange = inUnit->mStats->mRange;
            if (theDistance <= theRange * theRange)
                theScore += 0.15f;
        }
    }
    return theScore;
}

// FUNCTION: EXE 0x406ef9
// Tells whether the unit class is a land, naval, helicopter or building priest
bool EEUnitClass::IsPriest()
{
    long theCategory = this->mCategory;
    return theCategory == kUnitCategoryLandPriest || theCategory == kUnitCategoryNavalPriest || theCategory == kUnitCategoryHelicopterPriest || theCategory == kUnitCategoryBuildingPriest;
}

// FUNCTION: EXE 0x406f1a
// Scores an own unit other than the prophet for teleporting: non-special, melee, distance from the enemy (squared
// distance against its squared range), the enemy being countered by it, and attack strength
float EECPASGUProphetCalamityAnalyzerTeleport::ScoreAlly(EEComplexUnit* inUnit)
{
    float theScore = 0.0f;
    if (inUnit != NULL && this->mUnit != NULL && inUnit != this->mProphet->mUnit) {
        EEUnitClass* theClass = inUnit->mUnitClass;
        if (theClass->mAIKind != kAIKindSpecial)
            theScore = 0.2f;
        if (theClass->mIsMelee)
            theScore += 0.15f;
        float theDistance = inUnit->DistanceSquaredTo(this->mUnit);
        float theRange = inUnit->mStats->mRange;
        theRange *= theRange;   // squared, as the squared distance (was range + range)
        if (theDistance >= theRange && theRange != 0.0f) {
            float theFactor = 1.0f - theRange / theDistance;
            if (theFactor >= 1.0f)
                theFactor = 1.0f;
            if (theFactor <= 0.1f)
                theFactor = 0.1f;
            theScore += theFactor * 0.35f;
        }
        if (this->mUnit->GetUnitClass()->IsCounteredBy(theClass->GetUnitType()))
            theScore += 0.3f;
        float theValue = inUnit->mStats->mAttack * 0.0033333334f;
        if (theValue >= 1.0f)
            theValue = 1.0f;
        theScore = theValue * 0.35f + theScore;
    }
    return theScore;
}

// FUNCTION: EXE 0x407028
// Tells whether a unit type's AI kind is one of this class's three counter kinds (never for kind none)
bool EEUnitClass::IsCounteredBy(long inUnitType)
{
    long theKind = gUnitClassTable[inUnitType]->mAIKind;
    return theKind != kAIKindNone && (theKind == this->mCounterKind1 || theKind == this->mCounterKind2 || theKind == this->mCounterKind3);
}

// FUNCTION: EXE 0x40705f
// Returns the sub group unit's game unit
// inline in ee/EECPAttackSubGroupUnit.h; the COMDAT is emitted for the callers that refuse to inline it

// FUNCTION: EXE 0x407063
// Accepts a target that has other targets near it and whose unit type is in the analyzer's unit set
bool EECPASGUPCTargetIsUnitSetScreenOnlySplashUnits::Passes(EECPASTargetUnit* inTarget)
{
    bool theResult = false;
    if (inTarget->mNumNearbyTargets > 0)
        theResult = this->mAnalyzer->GetUnitSet()->Contains(inTarget->mUnit->mUnitClass->GetUnitType());
    return theResult;
}

// FUNCTION: EXE 0x407089
// Accepts a special or ranged (range above 1) target; when asked, only if its unit type is in the analyzer's unit set
bool EECPASGUPCTargetIsUnitSetScreenRangedOrSpecialUnits::Passes(EECPASTargetUnit* inTarget)
{
    bool theResult = false;
    EEComplexUnit* theUnit = inTarget->mUnit;
    if (theUnit->mUnitClass->mAIKind == kAIKindSpecial || theUnit->mStats->mRange > 1.0f) {
        if (!this->mUseUnitSet || this->mAnalyzer->GetUnitSet()->Contains(theUnit->mUnitClass->GetUnitType()))
            theResult = true;
    }
    return theResult;
}

// SYNTHETIC: EXE 0x4070d6
// (the scalar deleting destructor; 0x1d bytes)

// FUNCTION: EXE 0x4070f3
// Rates casting a nano virus: random score, town penalty and cluster bonus; 0 without a castable target; below
// 0.35 releases the target and excludes the analyzer
float EECPASGUProphetCalamityAnalyzerNanoVirus::Evaluate()
{
    std::vector<EECPASTargetUnit*> theTargets = this->mProphet->mSubGroup->mTargets;
    this->mIsExcluded = false;
    float theScore = this->ApplyClusterBonus(this->ApplyTownPenalty(gPhysics->GetRandom(0.35f, 1.0f) * 0.35f));
    if (this->PickTarget() == NULL)
        theScore = 0.0f;
    if (theScore < 0.35f) {
        this->ReleaseUnit();
        theScore = 0.0f;
        this->mIsExcluded = true;
    }
    this->mScore = theScore;
    return theScore;
}

// FUNCTION: EXE 0x40718e
// Raises a score by 15% per nearby target of the most clustered unit-set target of the sub group
float EECPASGUProphetCalamityAnalyzerBase::ApplyClusterBonus(float inScore)
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
    EE_LOST_DEBUG(6);   // LOST-DEBUG: none: exact only (the same n as the Fire/Famine twins 0x40a131/0x40ab2b)
    if (theMax > 0)
        theResult = (theMax * 0.15f + 1.0f) * inScore;
    return theResult;
}

// FUNCTION: EXE 0x407236
// Picks a castable target, first among clustered unit-set targets, then among ranged or special ones, trying them
// from the end of a target-priority sort; stores its tile and unit
EEComplexUnit* EECPASGUProphetCalamityAnalyzerInferno::PickTarget()
{
    std::vector<EECPASTargetUnit*> theTargets;
    EEComplexUnit* theResult = NULL;
    this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenOnlySplashUnits(this->mProphet, this, true));
    if (!theTargets.empty()) {
        std::sort(theTargets.begin(), theTargets.end(), EECPAITargetPriority(this->mProphet->GetUnit()->GetTile()));
        while (!theTargets.empty()) {
            EETileLocation theTile(theTargets.back()->mUnit->GetTileX(), theTargets.back()->mUnit->mTileY);
            GE3DPoint theLocation(theTile.GetX() + 0.5f, theTile.mY + 0.5f, 0.0f);
            if (this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(), theLocation,
                    theTile.GetX(), theTile.mY, true, theTargets.back()->mUnit) == 0) {
                this->mTile.mX = theTile.GetX();
                this->mTile.mY = theTile.mY;
                this->SetUnit(theTargets.back()->mUnit);
                theResult = this->mUnit;
                break;
            }
            theTargets.pop_back();
        }
    }
    if (theResult != NULL)
        return theResult;
    theTargets.clear();
    this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenRangedOrSpecialUnits(this->mProphet, this, true));
    if (theTargets.empty())
        return theResult;
    std::sort(theTargets.begin(), theTargets.end(), EECPAITargetPriority(this->mProphet->GetUnit()->GetTile()));
    while (!theTargets.empty()) {
        EETileLocation theTile(theTargets.back()->mUnit->GetTileX(), theTargets.back()->mUnit->mTileY);
        GE3DPoint theLocation(theTile.GetX() + 0.5f, theTile.mY + 0.5f, 0.0f);
        if (this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(), theLocation,
                theTile.GetX(), theTile.mY, true, theTargets.back()->mUnit) == 0) {
            this->mTile.mX = theTile.GetX();
            this->mTile.mY = theTile.mY;
            this->SetUnit(theTargets.back()->mUnit);
            theResult = this->mUnit;
            break;
        }
        theTargets.pop_back();
    }
    return theResult;
}

// FUNCTION: EXE 0x4074f3
// Rates casting an inferno: random score, town penalty, citizen and cluster bonuses; 0 without a castable target;
// below 0.45 releases the target and excludes the analyzer
float EECPASGUProphetCalamityAnalyzerInferno::Evaluate()
{
    std::vector<EECPASTargetUnit*> theTargets = this->mProphet->mSubGroup->mTargets;
    this->mIsExcluded = false;
    float theScore = this->ApplyClusterBonus(this->ApplyCitizenBonus(this->ApplyTownPenalty(gPhysics->GetRandom(0.35f, 0.9f) * 0.35f)));
    if (this->PickTarget() == NULL)
        theScore = 0.0f;
    if (theScore < 0.45f) {
        this->ReleaseUnit();
        theScore = 0.0f;
        this->mIsExcluded = true;
    }
    this->mScore = theScore;
    return theScore;
}

// FUNCTION: EXE 0x40759d
// Raises a score by 2.5% per land or naval citizen among the sub group's targets
float EECPASGUProphetCalamityAnalyzerInferno::ApplyCitizenBonus(float inScore)
{
    float theResult = inScore;
    long theCount = 0;
    std::vector<EECPASTargetUnit*>& theTargets = this->mProphet->mSubGroup->mTargets;
    for (long i = theTargets.size() - 1; i >= 0; i--) {
        EEUnitClass* theClass = theTargets[i]->mUnit->mUnitClass;
        if (theClass->mCategory == kUnitCategoryLandCitizen || theClass->mCategory == kUnitCategoryNavalCitizen)
            theCount++;
    }
    if (theCount > 0)
        theResult = (theCount * 0.025f + 1.0f) * inScore;
    return theResult;
}

// FUNCTION: EXE 0x40760e
// Adds the raw (unscaled) experience of every unit-set target to a score when the targets include aircraft
float EECPASGUProphetCalamityAnalyzerAntiMatterStorm::AddTargetVeterancy(float inScore)
{
    std::vector<EECPASTargetUnit*> theTargets;
    float theScore = inScore;
    if (this->mProphet->mSubGroup->mCounts.mNumAircraft > 0) {
        this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenBase(this->mProphet, this, true));
        for (long i = theTargets.size() - 1; i >= 0; i--)
            theScore += theTargets[i]->mUnit->mExperience;
    }
    return theScore;
}

// FUNCTION: EXE 0x40769e
// Rates casting an anti-matter storm: random score, aircraft bonus, target veterancy; 0 without a castable target;
// below 0.5 releases the target and excludes the analyzer
float EECPASGUProphetCalamityAnalyzerAntiMatterStorm::Evaluate()
{
    std::vector<EECPASTargetUnit*> theTargets = this->mProphet->mSubGroup->mTargets;
    this->mIsExcluded = false;
    float theScore = this->AddTargetVeterancy(this->ApplyAircraftBonus(gPhysics->GetRandom(0.25f, 0.7f) * 0.35f));
    if (this->PickTarget() == NULL)
        theScore = 0.0f;
    if (theScore < 0.5f) {
        this->ReleaseUnit();
        theScore = 0.0f;
        this->mIsExcluded = true;
    }
    this->mScore = theScore;
    return theScore;
}

// FUNCTION: EXE 0x40773d
// Adds 0.05 per target aircraft, 0.1 per bomber and 0.3 per mL20 unit to a score (only when there are aircraft)
float EECPASGUProphetCalamityAnalyzerAntiMatterStorm::ApplyAircraftBonus(float inScore)
{
    float theResult = inScore;
    if (this->mProphet->mSubGroup->mCounts.mNumAircraft > 0) {
        theResult = this->mProphet->mSubGroup->mCounts.mNumAircraft * 0.05f + inScore;
        if (this->mProphet->mSubGroup->mCounts.mNumBombers > 0)
            theResult += this->mProphet->mSubGroup->mCounts.mNumBombers * 0.1f;
        if (this->mProphet->mSubGroup->mCounts.mL20 > 0)
            theResult += this->mProphet->mSubGroup->mCounts.mL20 * 0.3f;
    }
    return theResult;
}

// FUNCTION: EXE 0x407799
// Scores the sub group's targets and picks the most threatening enemy for a shield battery; stores its tile and
// unit, returns its score (0 and NULL when none scores)
float EECPASGUProphetCalamityAnalyzerShieldBattery::PickBest(EEComplexUnit*& outUnit)
{
    std::multimap<float, EECPASTargetUnit*, std::greater<float> > theRanked;
    std::vector<EECPASTargetUnit*> theTargets;
    outUnit = NULL;
    this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenBase(this->mProphet, this, false));
    for (long i = theTargets.size() - 1; i >= 0; i--) {
        EEComplexUnit* theUnit = theTargets[i]->mUnit;
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

// FUNCTION: EXE 0x4078ae
// Picks the best own unit to shield (passes MayShield, highest ScoreAlly) and holds it as the ally target; needs a
// chosen enemy; returns its score
float EECPASGUProphetCalamityAnalyzerShieldBattery::PickAlly(EEComplexUnit*& outUnit)
{
    std::multimap<float, EEComplexUnit*, std::greater<float> > theRanked;
    float theResult = 0.0f;
    outUnit = NULL;
    if (this->mUnit != NULL) {
        for (long i = this->mProphet->mSubGroup->GetNumUnits() - 1; i >= 0; --i) {
            EECPAttackSubGroupUnit* theUnit = this->mProphet->mSubGroup->GetUnitAt(i);
            if (!this->MayShield(theUnit))
                continue;
            float theScore = this->ScoreAlly(theUnit);
            // VC6 emits the original's JE (0x407920) here, which also rejects unordered scores
            if (theScore != 0.0f)
                theRanked.insert(std::multimap<float, EEComplexUnit*, std::greater<float> >::value_type(theScore, theUnit->mUnit));
        }
        theResult = 0.0f;
        if (!theRanked.empty()) {
            outUnit = theRanked.begin()->second;
            theResult = theRanked.begin()->first;
            this->SetTarget(outUnit);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x40797c
// Tells whether an own unit may be shielded: alive, enabled, on the map and of a type in the analyzer's unit set
bool EECPASGUProphetCalamityAnalyzerShieldBattery::MayShield(EECPAttackSubGroupUnit* inUnit)
{
    EEComplexUnit* theUnit = inUnit->mUnit;
    bool theResult = false;
    if (theUnit->mHitPoints > 0 && theUnit->mIsDisabled == false && theUnit->GetTileX() != -10000)
        theResult = this->GetUnitSet()->Contains(theUnit->mUnitClass->GetUnitType());
    return theResult;
}

// FUNCTION: EXE 0x4079b0
// Rates casting a shield battery (random plus enemy and ally scores); at 0.05 or more the ally becomes the unit
// to cast on, else both are released and the analyzer is excluded
float EECPASGUProphetCalamityAnalyzerShieldBattery::Evaluate()
{
    std::vector<EECPASTargetUnit*> theTargets = this->mProphet->mSubGroup->mTargets;
    this->mIsExcluded = false;
    EEComplexUnit* theUnit = NULL;
    float theScore = gPhysics->GetRandom(0.25f, 0.7f);
    float theValue = this->PickBest(theUnit);
    if (theUnit != NULL) {
        theUnit = NULL;
        theScore = theScore * 0.35f;
        theScore += theValue * 0.35f;
        theValue = this->PickAlly(theUnit);
        if (theUnit != NULL)
            theScore += theValue * 0.45f;
        else
            theScore = 0.0f;
    } else
        theScore = 0.0f;
    if (theScore < 0.05f) {
        this->ReleaseUnit();
        if (this->mTarget != NULL)
            this->mTarget->Dereference();
        this->mTarget = NULL;
        theScore = 0.0f;
        this->mIsExcluded = true;
    } else {
        this->SetUnit(this->mTarget);
        if (this->mTarget != NULL)
            this->mTarget->Dereference();
        this->mTarget = NULL;
    }
    this->mScore = theScore;
    return theScore;
}

// FUNCTION: EXE 0x407a9d
// Scores a castable enemy as a threat: veterancy (capped at 15), plus a bonus when it is attacking one of the sub
// group's units, more if that unit is important
float EECPASGUProphetCalamityAnalyzerShieldBattery::ScoreTarget(EEComplexUnit* inUnit)
{
    float theScore = 0.0f;
    if (inUnit != NULL) {
        GE3DPoint theLocation(inUnit->GetTileX() + 0.5f, inUnit->GetTileY() + 0.5f, 0.0f);
        if (this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(), theLocation, inUnit->GetTileX(), inUnit->GetTileY(), true, inUnit) == 0) {
            unsigned short theCount = inUnit->mExperience;
            if (theCount > 15)
                theCount = 15;
            theScore = theCount * 0.05f;
            if (inUnit->mGoalState == NULL)
                return theScore;
            EEUGAttackUnit* theGoal = dynamic_cast<EEUGAttackUnit*>(inUnit->GetLoadGoal());
            if (theGoal != NULL && theGoal->mTarget != NULL) {
                EEComplexUnit* theTarget = theGoal->mTarget->mComplexUnit;
                if (theTarget != NULL && theTarget->mPlayer == this->mProphet->mUnit->mPlayer) {
                    EECPAttackSubGroupUnit* theAttacker = this->mProphet->mSubGroup->FindUnit(theTarget);
                    if (theAttacker != NULL) {
                        theScore += 0.05f;
                        if (theAttacker->mIsImportant)
                            theScore += 0.3f;
                        return theScore;
                    }
                }
            }
        }
    }
    return theScore;
}

// FUNCTION: EXE 0x407bab
// Scores an own unit for shielding (the prophet only while in danger): special units, mech Zeus units, and most
// when the chosen enemy is attacking it
float EECPASGUProphetCalamityAnalyzerShieldBattery::ScoreAlly(EECPAttackSubGroupUnit* inUnit)
{
    float theScore = 0.0f;
    if (inUnit != NULL && this->mUnit != NULL && (inUnit != this->mProphet || this->mProphet->IsInDanger())) {
        EEUnitClass* theClass = inUnit->mUnit->mUnitClass;
        if (theClass->mAIKind == kAIKindSpecial)
            theScore = 0.1f;
        if (theClass->mFamily == kUnitFamilyMechZeus)
            theScore += 0.15f;
        if (this->mUnit != NULL) {
            EEUGAttackUnit* theGoal = dynamic_cast<EEUGAttackUnit*>(this->mUnit->GetLoadGoal());
            if (theGoal != NULL && theGoal->mTarget == inUnit->mUnit)
                theScore += 0.5f;
        }
    }
    return theScore;
}

// FUNCTION: EXE 0x407c45
// Creates the mech prophet's attack sub group unit for a unit
EECPASGUMechProphet::EECPASGUMechProphet(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup)
    : EECPASGUProphet(inUnit, inSubGroup)
{
    this->mType = kASGUKindMechProphet;
}

// SYNTHETIC: EXE 0x407c68
// (the scalar deleting destructor; 0x1c bytes)

// FUNCTION: EXE 0x407c84
// Creates a mech prophet sub group unit with no game unit yet
EECPASGUMechProphet::EECPASGUMechProphet(EECPAttackSubGroup* inSubGroup)
    : EECPASGUProphet(inSubGroup)
{
    this->mType = kASGUKindMechProphet;
}

// FUNCTION: EXE 0x407ca3
// Destroys the mech prophet sub group unit (base class only)
EECPASGUMechProphet::~EECPASGUMechProphet()
{
}

// FUNCTION: EXE 0x407cae
// Creates the analyzer for a mech prophet power (nano virus, anti-matter storm, inferno, teleport, time warp,
// shield battery); other power types get the prophet's analyzers
EECPASGUProphetCalamityAnalyzerBase* EECPASGUMechProphet::CreateAnalyzer(long inTechID, unsigned long inType)
{
    switch (inType) {
    case kCalamityAEMindControl:
        return new EECPASGUProphetCalamityAnalyzerNanoVirus(this, inTechID);
    case kCalamitySpecialAttack:
        return new EECPASGUProphetCalamityAnalyzerAntiMatterStorm(this, inTechID);
    case kCalamityAEExtraDamage:
        return new EECPASGUProphetCalamityAnalyzerInferno(this, inTechID);
    case kCalamityAreaEffect:
        return new EECPASGUProphetCalamityAnalyzerTeleport(this, inTechID);
    case kCalamityTeleport:
        return new EECPASGUProphetCalamityAnalyzerTimeWarp(this, inTechID);
    case kCalamityMorale:
        return new EECPASGUProphetCalamityAnalyzerShieldBattery(this, inTechID);
    }
    return EECPASGUProphet::CreateAnalyzer(inTechID, inType);
}

// FUNCTION: EXE 0x407dd3
// Tells whether a teleport is worth considering: at least 3 targets, or no fewer targets than sub group units
bool EECPASGUProphetCalamityAnalyzerTeleport::IsApplicable()
{
    unsigned long theCount = this->mProphet->mSubGroup->mTargets.size();
    if (theCount < 3 && theCount < this->mProphet->mSubGroup->GetNumUnits())
        return false;
    return true;
}

// FUNCTION: EXE 0x407e03
// Releases the held ally target and resets the analyzer
void EECPASGUProphetCalamityAnalyzerShieldBattery::Reset()
{
    if (this->mTarget != NULL)
        this->mTarget->Dereference();
    this->mTarget = NULL;
    EECPASGUProphetCalamityAnalyzerBase::Reset();
}

// SYNTHETIC: EXE 0x407e1f
// (the scalar deleting destructor; 0x1c bytes)

// FUNCTION: EXE 0x407e3b
// Destroys the teleport analyzer, releasing its held ally target
EECPASGUProphetCalamityAnalyzerTeleport::~EECPASGUProphetCalamityAnalyzerTeleport()
{
    if (this->mTarget != NULL)
        this->mTarget->Dereference();
    this->mTarget = NULL;
}

// FUNCTION: EXE 0x407e63
// Tells whether a nano virus is worth considering: the sub group's targets include cyborgs
bool EECPASGUProphetCalamityAnalyzerNanoVirus::IsApplicable()
{
    return this->mProphet->mSubGroup->mCounts.mNumCyborgs > 0 ? true : false;
}

// FUNCTION: EXE 0x407e74
// Tells whether an inferno is worth considering: the sub group's targets include plague targets
bool EECPASGUProphetCalamityAnalyzerInferno::IsApplicable()
{
    return this->mProphet->mSubGroup->mCounts.mNumPlagueTargets > 0 ? true : false;
}

// FUNCTION: EXE 0x407e85
// Tells whether an anti-matter storm is worth considering: the sub group's targets include aircraft
bool EECPASGUProphetCalamityAnalyzerAntiMatterStorm::IsApplicable()
{
    return this->mProphet->mSubGroup->mCounts.mNumAircraft > 0 ? true : false;
}

// SYNTHETIC: EXE 0x407e96
// (the scalar deleting destructor; 0x22 bytes)

// SYNTHETIC: EXE 0x407eb8
// (the scalar deleting destructor; 0x1c bytes)

// FUNCTION: EXE 0x407ed4
// Destroys the shield battery analyzer, releasing its held ally target
EECPASGUProphetCalamityAnalyzerShieldBattery::~EECPASGUProphetCalamityAnalyzerShieldBattery()
{
    if (this->mTarget != NULL)
        this->mTarget->Dereference();
    this->mTarget = NULL;
}

// FUNCTION: EXE 0x407efc
// Retargets as a prophet; if that finds nothing and the unit has an initial attack action, retargets as a plain
// attacker; returns whether it has a target
bool EECPASGUMechProphet::Retarget()
{
    bool theResult = EECPASGUProphet::Retarget();
    if (!theResult && this->mUnit->mUnitClass->FindAction(kActionInitialAttackState) != NULL)
        theResult = EECPAttackSubGroupUnit::Retarget();
    return theResult;
}

// FUNCTION: EXE 0x407f2d
// Records the new state, then enters it as a prophet does
void EECPASGUMechProphet::SetState(long inState, unsigned long inTime)
{
    this->mState = inState;
    EECPASGUProphet::SetState(inState, inTime);
}

// FUNCTION: EXE 0x407f41
// Tests a sub group unit condition the way a prophet does
bool EECPASGUMechProphet::CheckCondition(long inState, unsigned long inTime)
{
    return EECPASGUProphet::CheckCondition(inState, inTime);
}

// FUNCTION: EXE 0x407f51
// Runs the state machine: contained while off the map; when attacking, goes idle without targets, retreats,
// or chooses an action without a target or in danger, else re-attacks when due (other states as a prophet)
void EECPASGUMechProphet::Update(unsigned long inTime)
{
    if (this->mUnit->GetTileX() == -10000 && this->mState != kASGUStateContained)
        this->mState = kASGUStateContained;
    if (this->mState != kASGUStateAttack)
        EECPASGUProphet::Update(inTime);
    else {
        bool theFailed = !this->CheckCondition(kASGUConditionSubGroupHasTargets, inTime);
        if (theFailed)
            this->SetState(kASGUStateIdle, inTime);
        if (!theFailed) {
            if (this->CheckCondition(kASGUConditionShouldRetreat, inTime))
                this->SetState(kASGUStateRetreat, inTime);
            else {
                bool theMove = !this->CheckCondition(kASGUConditionHasTarget, inTime);
                if (!theMove)
                    theMove = this->CheckCondition(kASGUConditionInDanger, inTime);
                if (theMove)
                    this->SetState(kASGUStateChooseAction, inTime);
                if (!theMove && (this->CheckCondition(kASGUConditionRetargetDue, inTime) || this->CheckCondition(kASGUConditionInDanger, inTime)))
                    this->SetState(kASGUStateAttack, inTime);
            }
        }
    }
}

// FUNCTION: EXE 0x408023
// Removes the last pointer from a list of target pointers (std::vector pop_back, a body shared by pointer vectors)

// FUNCTION: EXE 0x408057
// Destroys a ranked target multimap (score to target, highest first), freeing every node

// FUNCTION: EXE 0x408082
// Destroys a ranked unit multimap (score to unit, highest first), freeing every node

// FUNCTION: EXE 0x4080ad
// Returns the iterator moved forward n elements (std::_Ptrit operator+, a body shared by pointer vectors)

// FUNCTION: EXE 0x4080bf
// Erases a range of entries from a ranked target multimap; returns the iterator after them (std::_Tree erase)

// FUNCTION: EXE 0x408130
// Inserts a (score, pointer) entry into a highest-first float multimap (std::_Tree insert, a shared body)

// FUNCTION: EXE 0x408192
// Erases a range of entries from a ranked unit multimap; returns the iterator after them (std::_Tree erase)

// FUNCTION: EXE 0x408203
// Returns the unit's map tile (EEUnit::GetTile)

// FUNCTION: EXE 0x408215
// Throws a copy of the length error (std::length_error::_Doraise)

// FUNCTION: EXE 0x40821e
// Frees a subtree of a ranked unit multimap, node by node (std::_Tree _Erase)

// FUNCTION: EXE 0x408250
// Frees a subtree of a ranked target multimap, node by node (std::_Tree _Erase)

// 0x408282 is U2DComputeCenterPoint<long> (lle/U2DPoint.h; EEFn08282): its marker is in EECPASGUProphet.cpp, whose
// analyzers instantiate it

// FUNCTION: EXE 0x40841e
// Sorts a list of attack targets by target priority (std::sort with EECPAITargetPriority)

// FUNCTION: EXE 0x408502
// Insertion-sorts a range of attack targets by target priority (std::_Insertion_sort)

// FUNCTION: EXE 0x408535
// Quick-sorts a range of attack targets by target priority, switching to a heap sort when too deep (std::_Sort)

// FUNCTION: EXE 0x40863d
// Insertion-sorts attack targets with no lower-bound check (std::_Unguarded_insertion_sort)

// FUNCTION: EXE 0x40867f
// Arranges a range of attack targets into a heap by target priority (std::make_heap)

// FUNCTION: EXE 0x4086c3
// Sorts a heap of attack targets by target priority (std::sort_heap)

// FUNCTION: EXE 0x408710
// Partitions a range of attack targets around a pivot by target priority (std::_Unguarded_partition)

// FUNCTION: EXE 0x408830
// Shifts an attack target back into its place in a sorted range (std::_Unguarded_insert)

// FUNCTION: EXE 0x408863
// Builds a heap of attack targets by sifting down every parent (std::_Make_heap)

// FUNCTION: EXE 0x4088c0
// Moves the top attack target of a heap to its end and restores the heap (std::pop_heap)

// FUNCTION: EXE 0x4088f3
// Sifts an attack target down into its place in a heap by target priority (std::_Adjust_heap)

// FUNCTION: EXE 0x40896d
// Pops the top attack target of a heap to its end (std::_Pop_heap_0)

// FUNCTION: EXE 0x4089dd
// Moves the top attack target of a heap to a given slot and re-inserts a value (std::_Pop_heap)

// 0x408a21: its marker is in EECPASGUProphet.cpp (that object instantiates it)

// 0x408a38: its marker is in EECPASGUProphet.cpp (that object instantiates it)

// FUNCTION: EXE 0x408a3f
// Returns the squared tile distance from the stored centre tile to a given tile
long EETileDistanceLess::DistanceSquared(const EETileCoordinate& inTile) const
{
    long theDY = this->mCenter.mY - inTile.mY;
    long theDX = this->mCenter.GetX() - inTile.GetX();
    return theDX * theDX + theDY * theDY;
}

// 0x408a5c: its marker is in EECPASGUProphet.cpp (that object instantiates it)

// 0x408b07: its marker is in EECPASGUProphet.cpp (that object instantiates it)

// 0x408d98: its marker is in EECPASGUProphet.cpp (that object instantiates it)

// 0x408dc7: its marker is in EECPASGUProphet.cpp (that object instantiates it)

// FUNCTION: EXE 0x408de3
// Creates the priest's attack sub group unit for a unit
EECPASGUPriest::EECPASGUPriest(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup)
    : EECPAttackSubGroupUnit(inUnit, inSubGroup)
{
    this->mType = kASGUKindPriest;
}

// SYNTHETIC: EXE 0x408e06
// (the scalar deleting destructor; 0x1c bytes)

// FUNCTION: EXE 0x408e22
// Creates a priest sub group unit with no game unit yet
EECPASGUPriest::EECPASGUPriest(EECPAttackSubGroup* inSubGroup)
    : EECPAttackSubGroupUnit(inSubGroup)
{
    this->mType = kASGUKindPriest;
}

// FUNCTION: EXE 0x408e41
// Destroys the priest sub group unit (base class only)
EECPASGUPriest::~EECPASGUPriest()
{
}

// FUNCTION: EXE 0x408e4c
// Returns the target's hit points as a fraction of its maximum, capped at 1
float EECPASGUPriest::GetTargetDamage(EECPASTargetUnit* inTarget)
{
    float theRatio = (float)inTarget->mUnit->mHitPoints / inTarget->mUnit->mStats->mMaxHitPoints;
    if (1.0f < theRatio)
        theRatio = 1.0f;
    return theRatio;
}

// FUNCTION: EXE 0x408e6f
// Chooses a target as any attacker does, but only one the priest can convert (else NULL)
EECPASTargetUnit* EECPASGUPriest::ChooseTarget(bool inTakeFirst, EETileCoordinate& outTile)
{
    EECPASTargetUnit* theTarget = EECPAttackSubGroupUnit::ChooseTarget(inTakeFirst, outTile);
    if (theTarget && !EECanPriestConvert(this->mUnit, theTarget->mUnit))
        theTarget = NULL;
    return theTarget;
}

// FUNCTION: EXE 0x408ea0
// Picks a convertible target and, if the unit can convert, sends it after the target unless it already is;
// returns whether it has a target
bool EECPASGUPriest::Retarget()
{
    EETileCoordinate theTile(0, 0);
    EECPASTargetUnit* theTarget = this->ChooseTarget(false, theTile);
    if (theTarget && this->mUnit->mUnitClass->FindAction(kActionConvertEnemyUnit)) {
        bool theChange = this->mTarget != theTarget->mUnit;
        if (!theChange) {
            EEUGAttackUnit* theGoal = dynamic_cast<EEUGAttackUnit*>(this->mUnit->GetLoadGoal());
            if (!theGoal)
                theChange = true;
            else if (theGoal->mTarget != theTarget->mUnit)
                theChange = true;
        }
        if (theChange) {
            this->SetTarget(theTarget->mUnit);
            this->mSubGroup->AttackWith(this, this->mTarget->mComplexUnit, &theTile);
        }
    }
    return this->mTarget ? true : false;
}

// FUNCTION: EXE 0x408f37
// Tells whether the priest can attack a target: a plain attacker could, and the priest can convert it
bool EECPASGUPriest::CanAttack(EECPASTargetUnit* inTarget)
{
    bool theResult = EECPAttackSubGroupUnit::CanAttack(inTarget);
    if (theResult && !EECanPriestConvert(this->mUnit, inTarget->mUnit))
        theResult = false;
    return theResult;
}

// FUNCTION: EXE 0x408f67
// Orders two attack targets by squared tile distance to a centre, nearest first (EECPAITargetNearest::operator())
// No source here: EECPAITargetNearest is defined in EECPASGUProphet.cpp, so this object does not emit it

// TEMPLATE: EXE 0x408fd7
// Orders two attack targets for sorting: more nearby targets first, then the one farther from the centre tile
// EECPAITargetPriority::operator() (ee/EECPAITargetPriority.h), out of line and built without /Og in the original
// (frame locals, jmp chains, setg); the header's optimize pragma around the struct reproduces it. 0x408f67 is
// EECPAITargetNearest::operator() (setl), the same shape, defined in EECPASGUProphet.cpp

// ICF-FOLDED: EXE 0x407e03
// Releases the held ally target and resets the analyzer
// (no address of its own: the same body as EECPASGUProphetCalamityAnalyzerShieldBattery::Reset, which ICF kept)
void EECPASGUProphetCalamityAnalyzerTeleport::Reset()
{
    if (this->mTarget != NULL)
        this->mTarget->Dereference();
    this->mTarget = NULL;
    EECPASGUProphetCalamityAnalyzerBase::Reset();
}

// ICF-FOLDED: EXE 0x40ac63
// Picks a castable target among the unit-set targets: the one on their centre tile, else the last castable one;
// stores its tile and unit (no address of its own: ICF kept EECPASGUProphetCalamityAnalyzerHurricane::PickTarget)
EEComplexUnit* EECPASGUProphetCalamityAnalyzerAntiMatterStorm::PickTarget()
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

// ICF-FOLDED: EXE 0x407236
// Picks a castable target the same way as the inferno analyzer (clustered unit-set targets, then ranged or special)
// (no address of its own: the same body as EECPASGUProphetCalamityAnalyzerInferno::PickTarget, which ICF kept)
EEComplexUnit* EECPASGUProphetCalamityAnalyzerNanoVirus::PickTarget()
{
    std::vector<EECPASTargetUnit*> theTargets;
    EEComplexUnit* theResult = NULL;
    this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenOnlySplashUnits(this->mProphet, this, true));
    if (!theTargets.empty()) {
        std::sort(theTargets.begin(), theTargets.end(), EECPAITargetPriority(this->mProphet->GetUnit()->GetTile()));
        while (!theTargets.empty()) {
            EETileLocation theTile(theTargets.back()->mUnit->GetTileX(), theTargets.back()->mUnit->mTileY);
            GE3DPoint theLocation(theTile.GetX() + 0.5f, theTile.mY + 0.5f, 0.0f);
            if (this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(), theLocation,
                    theTile.GetX(), theTile.mY, true, theTargets.back()->mUnit) == 0) {
                this->mTile.mX = theTile.GetX();
                this->mTile.mY = theTile.mY;
                this->SetUnit(theTargets.back()->mUnit);
                theResult = this->mUnit;
                break;
            }
            theTargets.pop_back();
        }
    }
    if (theResult != NULL)
        return theResult;
    theTargets.clear();
    this->Gather(theTargets, &EECPASGUPCTargetIsUnitSetScreenRangedOrSpecialUnits(this->mProphet, this, true));
    if (theTargets.empty())
        return theResult;
    std::sort(theTargets.begin(), theTargets.end(), EECPAITargetPriority(this->mProphet->GetUnit()->GetTile()));
    while (!theTargets.empty()) {
        EETileLocation theTile(theTargets.back()->mUnit->GetTileX(), theTargets.back()->mUnit->mTileY);
        GE3DPoint theLocation(theTile.GetX() + 0.5f, theTile.mY + 0.5f, 0.0f);
        if (this->mCalamityClass->CanCastAt(this->mProphet->GetUnit(), theLocation,
                theTile.GetX(), theTile.mY, true, theTargets.back()->mUnit) == 0) {
            this->mTile.mX = theTile.GetX();
            this->mTile.mY = theTile.mY;
            this->SetUnit(theTargets.back()->mUnit);
            theResult = this->mUnit;
            break;
        }
        theTargets.pop_back();
    }
    return theResult;
}

// ICF-FOLDED: EXE 0x40b5b1
// Returns the best enemy target found by PickBest (NULL when none scores)
// (no address of its own: the same body as EECPASGUProphetCalamityAnalyzerShieldBattery::PickTarget, which ICF kept)
EEComplexUnit* EECPASGUProphetCalamityAnalyzerTeleport::PickTarget()
{
    EEComplexUnit* theUnit = NULL;
    this->PickBest(theUnit);
    return theUnit;
}

// ICF-FOLDED: EXE 0x40b5b1
// Returns the best time-warp target found by PickBest (NULL when none scores)
// (no address of its own: the same body as EECPASGUProphetCalamityAnalyzerShieldBattery::PickTarget, which ICF kept)
EEComplexUnit* EECPASGUProphetCalamityAnalyzerTimeWarp::PickTarget()
{
    EEComplexUnit* theUnit = NULL;
    this->PickBest(theUnit);
    return theUnit;
}

// ICF-FOLDED: EXE 0x407dd3
// Tells whether a time warp is worth considering: at least 3 targets, or no fewer targets than sub group units
// (no address of its own: the same body as EECPASGUProphetCalamityAnalyzerTeleport::IsApplicable, which ICF kept)
bool EECPASGUProphetCalamityAnalyzerTimeWarp::IsApplicable()
{
    unsigned long theCount = this->mProphet->mSubGroup->mTargets.size();
    if (theCount < 3 && theCount < this->mProphet->mSubGroup->GetNumUnits())
        return false;
    return true;
}
