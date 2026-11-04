// C:\EEX\Empire Earth\AI Thread\EECPAttackSubGroupUnit.cpp (file name inferred: no __FILE__; RTTI class
// EECPAttackSubGroupUnit, vtable 0x43bf4c). One unit of a computer player's attack sub group: 0x62f8a..0x66af0
// (three class-static `$E` chains open the unit, then two scoring helpers, Prejudice::Matches, the ctors, the dtor and
// the slots; the set/map/vector COMDATs from 0x6631c close it, the last one EECalamityManager::GetCalamityClass
// 0x66a89; EECPAttackCommander.cpp ... starts at 0x66af0).
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "ee/EmpireEarth.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/FSRestorable.h"
#include "lle/USingleton.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"

#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGLocation.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EEUGCastCalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EEUnitCalamityClass.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "ee/EEMapDimensions.h"
#include "ee/EEMap.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEGlobalConstants.h"
#include "ee/EEAEManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAIContinentManager.h"
#include "ee/AStarGraphableObjectBase.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEAIPathSubSector.h"
#include "ee/EECalamityManager.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAGTask.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPASGLand.h"
#include "ee/EECPAttackGroup.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPUnitManager.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPAttackSubGroupUnit.h"
#include "ee/EECPASTargetUnit.h"
#include "ee/EEAIUnitTargetingTable.h"

// The flee screeners (EEFleeFromLocationScreenBase > ..CheckPathAreaID): ee/EEFleeFromLocationScreenBase.h.
#include "ee/EEFleeFromLocationScreenBase.h"
#include "ee/EEFOLocationScreenBase.h"
#include "ee/EEAIKindIDs.h"

extern EEToHitTable gToHitTable;           // 0x5318e8
extern long gNumPlayers;                    // 0x530e0c (provisional name)

static const float kASGURange0 = 256.0f;    // 0x43bf38
static const float kASGURange1 = 400.0f;    // 0x43bf3c
static const float kASGURange2 = 576.0f;    // 0x43bf40
static const float kASGUThree = 3.0f;       // 0x43bf44
// 0x43bf10 is the pooled `/ 10.0f` literal (RandomizePersonality 0x464f98)
// 0x43bffc is the pooled 81.0f literal (Virtual0C 0x463b33)

// FUNCTION: EXE 0x46307d
// Rates a unit as a calamity target: 1 for a wonder, 0.9 tower, 0.8 gate, 0.6 hospital, 0.5 university,
// 0.4 temple, 0.7 plus a small hit-point term for a unit-producing building, else 0
static float ASGUWonderValue(EEComplexUnit* inUnit)
{
    float theResult = 0.0f;
    if (inUnit->GetTechTreeNode()->mData->mKind == 4)
        theResult = 1.0f;
    else if (inUnit->mUnitClass->mIsTower)
        theResult = 0.9f;
    else if (dynamic_cast<EEUCGate*>(inUnit->mUnitClass) != NULL)
        theResult = 0.8f;
    else {
        switch (inUnit->mUnitClass->mUnitType) {
        case kUnitTypeHospital:
            theResult = 0.6f;
            break;
        case kUnitTypeTemple:
            theResult = 0.4f;
            break;
        case kUnitTypeUniversity:
            theResult = 0.5f;
            break;
        case kUnitTypeBarracks:
        case kUnitTypeDock:
        case kUnitTypeArcheryRange:
        case kUnitTypeSiegeFactory:
        case kUnitTypeStable:
        case kUnitTypeNavyYard:
        case kUnitTypeAirport:
        case kUnitTypeTankFactory:
        case kUnitTypeCyberFactory:
        case kUnitTypeCyberLaboratory:
        case kUnitTypeSpaceDock:
            theResult = (float)inUnit->mHitPoints / ((float)inUnit->mUnitClass->mHitPoints * 100.0f) + 0.7f;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x46315b
// Rates a unit as a calamity target the same way as ASGUWonderValue (a second entry point)
static float ASGUWonderValue2(EEComplexUnit* inUnit)
{
    return ASGUWonderValue(inUnit);
}

// FUNCTION: EXE 0x463166
// Tells whether a unit class falls under the prejudice: by AI unit line if one is set, else by family,
// else by unit type
bool EECPAttackSubGroupUnitPrejudice::Matches(EEUnitClass* inClass)
{
    bool theResult;
    if (this->mUnitLine != kAIKindNone)
        theResult = inClass->mAIKind == this->mUnitLine;
    else if (this->mUnitFamily != kUnitFamilyNoFamily)
        theResult = inClass->mFamily == this->mUnitFamily;
    else
        theResult = inClass->mUnitType == this->mUnitType;
    return theResult;
}

// FUNCTION: EXE 0x46319f
// Creates the attack entry for a unit in a sub group (both referenced), idle and without target or escort;
// importance is the class influence times a hero/siege/calamity/range factor; then rolls a personality
EECPAttackSubGroupUnit::EECPAttackSubGroupUnit(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup)
    : mL3C(0), mL40(0)
{
    EEUnitClass* theClass = inUnit->mUnitClass;
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    this->mUnit = inUnit;
    this->mType = kASGUKindDefault;
    this->GetUnit()->Reference();
    this->mSubGroup = inSubGroup;
    if (this->mSubGroup != NULL)
        this->mSubGroup->Reference();
    this->mTarget = NULL;
    this->mF68 = this->mCourage = this->mAggression = this->mF6C = 0.5f;
    this->mNextRetargetTime = 0;
    this->mIsImportant = false;
    this->mIsOutOfFight = false;
    this->mNeedsOrders = false;
    this->mNextUpdateTime = 0;
    this->mTargetPickResult = 0;
    this->mType = kASGUKindDefault;
    this->mState = kASGUStateIdle;
    this->mEscort = NULL;
    this->mPostAttackMode = 0;
    this->mImportance = theClass->mInfluence;
    long theFactor = 1;
    if (theClass->mAIKind == kAIKindHero)
        theFactor = 0x4c;
    if (theClass->mAIKind == kAIKindSiege)
        theFactor += 3;
    if (theClass->mHasCalamityPowers)
        theFactor += 5;
    if (theStats->GetRange() > kASGUThree)
        theFactor += 2;
    this->mImportance = this->mImportance * theFactor;
    this->RandomizePersonality();
}

// SYNTHETIC: EXE 0x4632a5
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4632c1
// Creates an empty entry for a saved game to load into: references the sub group, no unit, idle
EECPAttackSubGroupUnit::EECPAttackSubGroupUnit(EECPAttackSubGroup* inSubGroup)
    : mL3C(0), mL40(0)
{
    this->mSubGroup = inSubGroup;
    this->mSubGroup->Reference();
    this->mF68 = this->mCourage = this->mAggression = this->mF6C = 0.5f;
    this->mUnit = NULL;
    this->mImportance = 0;
    this->mTarget = NULL;
    this->mNextRetargetTime = 0;
    this->mIsImportant = false;
    this->mIsOutOfFight = false;
    this->mNeedsOrders = false;
    this->mNextUpdateTime = 0;
    this->mTargetPickResult = 0;
    this->mType = kASGUKindDefault;
    this->mEscort = NULL;
    this->mState = kASGUStateIdle;
    this->mPostAttackMode = 0;
}

// FUNCTION: EXE 0x463360
// Destroys the entry, releasing its target, unit, escort and sub group references
EECPAttackSubGroupUnit::~EECPAttackSubGroupUnit()
{
    this->ReleaseTarget();
    this->ReleaseUnit();
    if (this->mEscort != NULL)
        this->mEscort->Dereference();
    this->mEscort = NULL;
    this->ReleaseSubGroup();
}

// FUNCTION: EXE 0x4633ad
// Returns 0 when the target's type counters this unit, 1 when this unit counters the target, else 0.5
float EECPAttackSubGroupUnit::GetCounterFactor(EECPASTargetUnit* inTarget)
{
    float theResult = 0.5f;
    if (this->GetUnit()->GetUnitClass()->IsCounteredBy(inTarget->mUnit->GetUnitClass()->GetUnitType()))
        theResult = 0.0f;
    else if (inTarget->mUnit->GetUnitClass()->IsCounteredBy(this->GetUnit()->GetUnitClass()->GetUnitType()))
        theResult = 1.0f;
    return theResult;
}

// FUNCTION: EXE 0x463408
// Returns how badly the target is damaged: 1 minus its hit-point fraction (at least 0)
float EECPAttackSubGroupUnit::GetTargetDamage(EECPASTargetUnit* inTarget)
{
    float theRatio = (float)inTarget->mUnit->mHitPoints / inTarget->mUnit->mStats->mMaxHitPoints;
    if (1.0f < theRatio)
        theRatio = 1.0f;
    float theResult = 1.0f - theRatio;
    return theResult;
}

// FUNCTION: EXE 0x463431
// Returns the target class's influence divided by this unit's (0 when either is not positive)
float EECPAttackSubGroupUnit::GetImportanceRatio(EECPASTargetUnit* inTarget)
{
    float theResult = 0.0f;
    long theOwn = this->GetUnit()->mUnitClass->mInfluence;
    if (theOwn > 0) {
        long theOther = inTarget->mUnit->mUnitClass->mInfluence;
        if (theOther > 0)
            theResult = (float)theOther / theOwn;
    }
    return theResult;
}

// FUNCTION: EXE 0x46346c
// Picks the best-scored target that can still catch one of the unit's contagions and moves onto it
void EECPAttackSubGroupUnit::SpreadContagion()
{
    EEComplexUnit* theUnit = this->PickContagionTarget(true);
    if (theUnit != NULL) {
        this->SetTarget(theUnit);
        this->MoveTo(theUnit, false, false);
    }
}

// NEAR (77.7): frame slots (0x1c vs 0x10: the original keeps the clamped terms on the FP stack) and the armour
// ratio's operand order (`fld; fld; fdivp`: an accessor value?).
// FUNCTION: EXE 0x463494
// Scores an enemy target for this unit from to-hit chance, targeting weight, distance, importance, damage,
// attack ratio, counters (added twice: x12 and x9), threat and alliance bonuses, and crowding by other attackers
float EECPAttackSubGroupUnit::ScoreTarget(EECPASTargetUnit* inTarget)
{
    EEComplexUnit* theUnit = inTarget->mUnit;
    float theScore = 0.0f;
    float theToHit = this->GetToHit(inTarget);
    if (theToHit > 0.0f) {
        EEUnitClassUpgrade* theStats = this->mUnit->mStats;
        theScore = theToHit * 9.0f;
        float theValue = (float)this->GetTargetWeight(inTarget);
        float theMaximum;
        float theRange;
        if (this->mUnit->mUnitClass->mIsMelee) {
            theMaximum = (float)EEAIUnitTargetingTable::sTable[0][9];
            theRange = kASGURange0;
        } else if (theStats->GetRange() <= theStats->mLineOfSight + 0.5f) {
            theMaximum = (float)EEAIUnitTargetingTable::sTable[1][9];
            theRange = kASGURange1;
        } else {
            theMaximum = (float)EEAIUnitTargetingTable::sTable[2][9];
            theRange = kASGURange2;
        }
        float theTerm = theValue / (theMaximum * 3.0f) * 20.0f;
        if (20.0f < theTerm)
            theTerm = 20.0f;
        theScore += theTerm;
        float theRatio = this->mUnit->DistanceSquaredTo(inTarget->mUnit) / theRange;
        if (theRatio >= 1.0f)
            theRatio = 1.0f;
        theScore += (1.0f - theRatio) * 20.0f;
        float theValue68 = this->GetImportanceRatio(inTarget) * 0.5f;
        theScore += (1.0f < theValue68 ? 1.0f : theValue68) * 8.0f;   // the clamp stays on the FPU (fcomp st(1))
        float theValue64 = this->GetTargetDamage(inTarget);
        theScore += theValue64 * 8.0f;
        if (this->mUnit->mStats->GetAttack() != 0.0f && inTarget->mUnit->mStats->GetAttack() != 0.0f) {
            float theArmor = inTarget->mUnit->mStats->GetAttack() / this->mUnit->mStats->GetAttack();
            if (theArmor > 1.5f)
                theArmor = 1.5f;
            else if (theArmor < kHeightEpsilon)
                theArmor = kHeightEpsilon;
            theScore += theArmor * 9.0f;
        }
        float theValue60 = this->GetCounterFactor(inTarget);
        theScore += theValue60 * 12.0f;
        theScore += theValue60 * 9.0f;
        float theExperience = (float)theUnit->mExperience * 0.083333336f;
        if (theExperience >= 1.0f)
            theExperience = 1.0f;
        theScore += theExperience;
        if (theUnit->mStats->mAreaEffect > 1.0f)
            theScore += 5.0f;
        if (theStats->mAreaEffect > 1.0f)
            theScore += 10.0f;
        if (theUnit->mUnitClass->mIsWall && this->mSubGroup->mState == kASGStateClearSubSector) {
            EECPASGLand* theLand = dynamic_cast<EECPASGLand*>(this->mSubGroup);
            if (theLand != NULL && theLand->mNextSubSectorTile == theUnit->GetTile()) {
                if (theScore < 0.0f)
                    theScore *= 0.25f;
                else
                    theScore *= 1.75f;
            }
        }
        if (this->mUnit->mUnitClass->mAIKind == kAIKindSiege && theUnit->mUnitClass->mIsTower)
            theScore += 40.0f;
        if (theUnit->GetNumCargoUnits() != 0)
            theScore += (float)theUnit->GetNumCargoUnits() * 5.0f;
        if (theUnit->mUnitClass->mAIKind == kAIKindSpecial)
            theScore += 8.0f;
        EEPlayer* thePlayer = ::gWorld.mPlayers[this->mSubGroup->mTask.GetPlayerID()];
        if (thePlayer == theUnit->mPlayer || theUnit->mPlayer->mEnemies[thePlayer->mPlayerID] == 0)
            theScore += 10.0f;
        if (theUnit->mUnitClass->IsLandBuilding() && theUnit->mUnitClass->mMaxVelocity == 0.0f
            && !theUnit->mUnitClass->IsCounteredBy(this->mUnit->mUnitClass->mUnitType)) {
            if (theScore < 0.0f) {
                if (theUnit->mUnitClass->mIsWall)
                    theScore *= 2.1f;
                else
                    theScore *= 1.4f;
            } else
                theScore *= 0.4f;
        }
        if (this->mUnit->DistanceSquaredTo(theUnit) <= theStats->GetRange() * theStats->GetRange()) {
            EEUnitClass* theClass = this->mUnit->mUnitClass;
            if (!theClass->mIsMelee) {
                if (theScore > 0.0f || (theClass->mDomains & kMovementSea)) {
                    if (theClass->mAIKind != kAIKindFieldWeapon)
                        theScore *= 1.4f;
                    else
                        theScore *= 6.75f;
                }
            } else
                theScore *= 1.1f;
        }
        if (this->mSubGroup->mTask.mIsActive && this->mSubGroup->mTask.mUnit != NULL && this->mSubGroup->mTask.mUnit->GetTechTreeNode() != NULL
            && this->mSubGroup->mTask.mUnit->GetTechTreeNode()->mData->mKind == 4) {
            if (theUnit->GetTechTreeNode() != NULL && theUnit->GetTechTreeNode()->mData->mKind == 4) {
                float theTripled = theScore * 3.0f;
                theScore = theTripled < 0.0f ? -theTripled : theTripled;
            }
            thePlayer = ::gWorld.mPlayers[this->mSubGroup->mTask.GetPlayerID()];
            if (theUnit->mPlayer->IsAlly(thePlayer)) {
                if (theScore > 0.0f)
                    theScore *= 5.0f;
                else
                    theScore *= 0.1f;
            } else {
                if (theScore > 0.0f)
                    theScore *= 0.25f;
                else
                    theScore *= 1.25f;
            }
        }
        if (theScore != 0.0f && theUnit->mNumAttackers > 0) {
            float theCrowd;
            if (this->mSubGroup->mTargets.size() > 0) {
                float theShare = (float)(unsigned long)this->mSubGroup->GetNumUnits() / this->mSubGroup->mTargets.size();
                theCrowd = (float)theUnit->mNumAttackers;
                if (theShare < theCrowd)
                    theCrowd = (theCrowd - theShare + theShare) * 0.068f;
                else
                    theCrowd *= 0.068f;
            } else
                theCrowd = (float)theUnit->mNumAttackers * 0.068f;
            if (theCrowd != 0.0f && (this->mUnit->mUnitClass->mAIKind == kAIKindFieldWeapon || this->mUnit->mUnitClass->mAIKind == kAIKindSiege)) {
                if (this->mUnit->DistanceSquaredTo(theUnit) <= theStats->GetRange() * theStats->GetRange())
                    theCrowd *= 0.5f;
            }
            if (theCrowd >= 0.98f)
                theCrowd = 0.98f;
            theScore *= theScore > 0.0f ? 1.0f - theCrowd : theCrowd + 1.0f;
            if (theScore == 0.0f)
                theScore = 1.0f;
        }
    }
    float theDistance = this->mUnit->DistanceSquaredTo(theUnit);
    return theScore;
}

// FUNCTION: EXE 0x463aef
// Returns the to-hit table's attack factor of this unit's ammo against the target's class and player
float EECPAttackSubGroupUnit::GetToHit(EECPASTargetUnit* inTarget)
{
    EEComplexUnit* theTarget = inTarget->mUnit;
    EEComplexUnit* theUnit = this->GetUnit();
    unsigned long theTargetPlayer = theTarget->mPlayer->mPlayerID;
    EEUnitClass* theTargetClass = theTarget->mUnitClass;
    unsigned long thePlayer = theUnit->mPlayer->mPlayerID;
    long theKind = theUnit->mUnitClass->mAmmoType;
    return gToHitTable.GetAttackFactor(theKind, theTargetClass, thePlayer, theTargetPlayer);
}

// FUNCTION: EXE 0x463b2a
// Returns 1 for any target (overridden by the unit kinds)
float EECPAttackSubGroupUnit::Virtual5C(EECPASTargetUnit* inTarget)
{
    return 1.0f;
}

// GetUnit()/GetPlayer()/GetPlayerID() as the later inline sites make VC6 refuse the _Tree ctor (0x298e2b) and the
// first mTargets begin() as in the original; the sub group in a local gives its lea ecx, [eax+0x30].
// FUNCTION: EXE 0x463b33
// Picks a sub group target outside a protective area effect that can catch one of the unit's calamities:
// the first found, or with inScore the best by nearby targets, distance and a random bonus; NULL if none
EEComplexUnit* EECPAttackSubGroupUnit::PickContagionTarget(bool inScore)
{
    std::multimap<float, EEComplexUnit*, std::greater<float> > theScores;
    std::vector<unsigned long> theCalamities;
    EEComplexUnit* theResult = NULL;
    if (this->mUnit->mCalamityFlags != 0) {
        EESplitCalamityFlags(this->mUnit->mCalamityFlags, theCalamities);
        for (long i = this->mSubGroup->mTargets.size() - 1; i >= 0 && theResult == NULL; i--) {
            EECPAttackSubGroup* theSubGroup = this->mSubGroup;
            EEComplexUnit* theTarget = theSubGroup->mTargets[i]->mUnit;
            if (gAEManager.HasAreaEffect2(3, theTarget->GetTileX(), theTarget->GetTileY(), this->GetUnit()->GetPlayer()->GetPlayerID()))
                continue;
            for (long j = theCalamities.size() - 1; j >= 0; j--) {
                if (theTarget->HasCalamityFlag(theCalamities[j]))
                    continue;
                if (!theTarget->mUnitClass->IsAffectedByCalamity(theCalamities[j]))
                    continue;
                if (!inScore) {
                    theResult = theTarget;
                    break;
                }
                float theScore = this->mSubGroup->mTargets[i]->mNumNearbyTargets * 10.0f;
                float theDistance = this->GetUnit()->DistanceSquaredTo(theTarget);
                if (theDistance <= 25.0f)
                    theScore += 40.0f;
                else if (theDistance <= 81.0f)
                    theScore += 20.0f;
                else
                    theScore += 5.0f;
                theScore += gPhysics->GetRandom(5.0f, 25.0f);
                theScores.insert(std::make_pair(theScore, theTarget));
            }
        }
        if (theScores.size() > 0)
            theResult = theScores.begin()->second;
    }
    return theResult;
}

// EEIsLineToUnitBlocked's third out-argument is the blocking unit (compared with a unit pointer here).
// FUNCTION: EXE 0x463d50
// Chooses the next target from the scored sub group targets, preferring armed ones and the current one unless
// beaten by 28; drops blocked, unreachable or rejected-sector targets; may return an attack tile in outTile
EECPASTargetUnit* EECPAttackSubGroupUnit::ChooseTarget(bool inTakeFirst, EETileCoordinate& outTile)
{
    std::multimap<float, EECPASTargetUnit*, std::greater<float> > theScores;
    std::multimap<float, EECPASTargetUnit*, std::greater<float> >::iterator theIt;
    std::set<unsigned short> theRejected;
    U2DPoint<long> theTile(0, 0);
    U2DPoint<long> theLandTile(0, 0);
    EECPASTargetUnit* theResult = NULL;
    EECPASTargetUnit* theCurrent = NULL;
    EEAIPathSubSector* theSubSector = gContinentManager.GetPathSubSectorAt(this->mUnit);
    this->mTargetPickResult = -1;
    float theCurrentScore = 0.0f;
    bool theDone = false;
    outTile.mX = 0;
    outTile.mY = 0;
    EEUnit* theBlocker = NULL;
    long theX = 0;
    long theY = 0;
    long thePass = 0;
    if (this->mTarget != NULL && !this->IsTargetValid())
        this->ReleaseTarget();
    EEUnitClassUpgrade* theStats = this->mUnit->mStats;
    float theRange = theStats->GetRange();
    float theRangeSquared = theRange * theRange;
    float theMinimum = this->mUnit->GetUnitClass()->mMinRange;
    float theMinimumSquared = theMinimum * theMinimum;
    long theKind;
    if (this->mUnit->GetUnitClass()->mIsMelee)
        theKind = 0;
    else if (theStats->GetRange() <= theStats->mLineOfSight)
        theKind = 1;
    else
        theKind = 2;
    while (thePass < 2) {
        std::vector<EECPASTargetUnit*>* theTargets;
        if (thePass == 0) {
            if (this->mUnit->GetUnitClass()->mAIKind == kAIKindSiege)
                theTargets = &this->mSubGroup->mSiegeTargets;
            else
                theTargets = &this->mSubGroup->mTargets;
        } else {
            if (this->mUnit->GetUnitClass()->mAIKind == kAIKindSiege)
                theTargets = &this->mSubGroup->mTargets;
            else
                theTargets = &this->mSubGroup->mSiegeTargets;
        }
        for (long i = theTargets->size() - 1; i >= 0; i--) {
            if (this->mUnit->IsTargetIgnored((*theTargets)[i]->mUnit, true))
                continue;
            float theScore = this->ScoreTarget((*theTargets)[i]);
            if (theScore == 0.0f)
                continue;
            if ((*theTargets)[i]->mUnit == this->mTarget) {
                theCurrentScore = theScore;
                theCurrent = (*theTargets)[i];
            }
            theScores.insert(std::make_pair(theScore, (*theTargets)[i]));
            if (inTakeFirst)
                break;
        }
        if (theScores.size() > 0) {
            while (!theDone && theScores.size() > 0) {
                std::multimap<float, EECPASTargetUnit*, std::greater<float> >::iterator theBest = theScores.begin();
                theResult = theBest->second;
                float theBestScore = theBest->first;
                EEUnitClass* theClass = theResult->mUnit->GetUnitClass();
                if (theClass->IsLandBuilding() && theResult->mUnit->GetUnitClass()->mMaxVelocity == 0.0f
                    && theResult->mUnit->GetUnitClass()->mAttack == 0)
                    theDone = true;
                else
                    theDone = false;
                bool theCurrentFlies;
                if (theCurrent != NULL && theCurrent->mUnit->GetUnitClass()->IsLandBuilding()
                    && theCurrent->mUnit->GetUnitClass()->mMaxVelocity == 0.0f && theCurrent->mUnit->GetUnitClass()->mAttack == 0)
                    theCurrentFlies = true;
                else
                    theCurrentFlies = false;
                if (theDone) {
                    if (!theCurrentFlies) {
                        theIt = theScores.begin();
                        while (true) {
                            ++theIt;
                            if (theIt == theScores.end())
                                break;
                            EEUnitClass* theOther = theIt->second->mUnit->GetUnitClass();
                            if (theOther->IsLandBuilding() && theIt->second->mUnit->GetUnitClass()->mMaxVelocity == 0.0f
                                && theIt->second->mUnit->GetUnitClass()->mAttack == 0)
                                continue;
                            theResult = theIt->second;
                            theBest = theIt;
                            break;
                        }
                    } else if (theResult->mUnit->GetUnitClass()->mIsWall || !theCurrent->mUnit->GetUnitClass()->mIsWall
                               || theCurrentScore + 28.0f > theBestScore)
                        theResult = theCurrent;
                } else if (theCurrent != NULL && !theCurrentFlies && theCurrentScore + 28.0f > theBestScore)
                    theResult = theCurrent;
                outTile.mX = 0;
                outTile.mY = 0;
                theDone = true;
                if (theResult != NULL && theResult != theCurrent) {
                    EEComplexUnit* theTarget = theResult->mUnit;
                    unsigned short theID = gContinentManager.GetPathSubSectorID(theTarget);
                    if (theID != 0 && theRejected.find(theID) != theRejected.end()) {
                        theResult = NULL;
                        theDone = false;
                        this->mUnit->IgnoreTarget(theTarget);
                        theScores.erase(theBest);
                        continue;
                    }
                    theTarget = theResult->mUnit;
                    float theDistance = this->mUnit->DistanceSquaredTo(theTarget);
                    float theDouble = theRangeSquared + theRangeSquared;
                    if (theDistance < theDouble
                        && ((theTarget->mVelocity == 0.0f && theKind == 1 && theDistance >= theRangeSquared)
                            || theDistance < theMinimumSquared
                            || (!this->mUnit->GetUnitClass()->mShootsHigh && !this->mUnit->mStats->mCanFireOverWalls
                                && EEIsLineToUnitBlocked(this->mUnit, theTarget, &theBlocker, &theX, &theY)))
                        && theBlocker != theTarget) {
                        theDone = false;
                        theResult = NULL;
                        this->mUnit->IgnoreTarget(theTarget);
                    }
                    if (theResult != NULL && theResult != theCurrent) {
                        this->mSubGroup->mGroup->mCommander->mUpdatesThisTick++;
                        theTarget = theResult->mUnit;
                        bool theLand = false;
                        theDone = EEHasPathToTarget(this->mUnit, theResult->mUnit, &theTile, 1, -1);
                        if (theDone && theTarget->GetUnitClass()->mIsWall) {
                            theLand = true;
                            theDone = EEIsAttackReachable(this->mUnit, theTarget, &theLandTile);
                        }
                        if (!theDone) {
                            this->mUnit->IgnoreTarget(theResult->mUnit);
                            theID = gContinentManager.GetPathSubSectorID(theResult->mUnit);
                            if (theID != 0 && !theLand)
                                theRejected.insert(theID);
                            theResult = NULL;
                            theScores.erase(theBest);
                            continue;
                        }
                        theDone = false;
                        EEAIPathSubSector* theTargetSector = gContinentManager.GetPathSubSectorAt(theResult->mUnit);
                        if (theTargetSector != NULL) {
                            if (theTargetSector == theSubSector || (theTile.mX == 0 && theTile.mY == 0)) {
                                theDone = true;
                                this->mTargetPickResult = 1;
                                continue;
                            }
                            EEAIPathSubSector* theTileSector = gContinentManager.GetPathSubSectorAt(theTile.mX, theTile.mY);
                            if (theTileSector == NULL || theTileSector == theTargetSector) {
                                theDone = true;
                                this->mTargetPickResult = 1;
                                continue;
                            }
                            long theSectorKind = theTargetSector->mKind;
                            if ((theSectorKind == kAITileWall || theSectorKind == kAITileGate || theSectorKind == kAITileUnitObstacle)
                                && (theDone = theTileSector->IsNeighbor(theTargetSector)) != false) {
                                EECPASGFlags theFlags;
                                this->mUnit->GetUnitClass()->GetTerrainFlags(this->mUnit, theFlags);
                                theDone = theTargetSector->AreLinksBlocked(theTileSector, theFlags);
                                if (theDone) {
                                    this->mTargetPickResult = 1;
                                    continue;
                                }
                            }
                            if (!this->mUnit->GetUnitClass()->mIsMelee) {
                                U2DPoint<long> thePosition(theResult->mUnit->GetTileX(), theResult->mUnit->mTileY);
                                if (theTile.DistanceToSquaredF(thePosition) < theDouble) {
                                    if (this->mUnit->GetUnitClass()->mShootsHigh || this->mUnit->mStats->mCanFireOverWalls
                                        || !EEIsLineBlocked(theTile.mX, theTile.mY, theTarget->GetTileX(), theTarget->mTileY,
                                                       &theBlocker, &theX, &theY)) {
                                        outTile.mX = theTile.mX;
                                        outTile.mY = theTile.mY;
                                        theDone = true;
                                        this->mTargetPickResult = 1;
                                        continue;
                                    }
                                    if (theBlocker != theTarget) {
                                        theDone = false;
                                        this->mUnit->IgnoreTarget(theTarget);
                                        theRejected.insert(theTargetSector->mID);
                                        theTarget = NULL;
                                    }
                                }
                            }
                        }
                        theResult = NULL;
                        if (theTarget == NULL) {
                            theScores.erase(theBest);
                            continue;
                        }
                        this->mUnit->IgnoreTarget(theTarget);
                    }
                }
                if (theDone) {
                    this->mTargetPickResult = 1;
                    continue;
                }
                theScores.erase(theBest);
            }
        }
        thePass++;
        if (theDone)
            break;
    }
    return theResult;
}

// FUNCTION: EXE 0x46449e
// Casts a calamity if it can; else chooses a target and orders an attack unless the unit already attacks it;
// returns whether the unit has a target
bool EECPAttackSubGroupUnit::Retarget()
{
    if (this->TryCastCalamity())
        return true;
    EETileCoordinate theTile;
    theTile.mX = 0;
    theTile.mY = 0;
    EECPASTargetUnit* theTarget = this->ChooseTarget(false, theTile);
    if (theTarget != NULL && this->GetUnit()->mUnitClass->FindAction(kActionInitialAttackState) != NULL) {
        bool theNew = this->mTarget != theTarget->mUnit;
        if (!theNew) {
            EEUGAttackUnit* theGoal = dynamic_cast<EEUGAttackUnit*>(this->GetUnit()->GetLoadGoal());
            if (theGoal == NULL && this->GetUnit()->mGoalQueues[0].mGoals.size() > 0)
                theGoal = dynamic_cast<EEUGAttackUnit*>(this->GetUnit()->mGoalQueues[0].mGoals.back());
            theNew = theGoal == NULL || theGoal->mTarget != theTarget->mUnit;
        }
        if (theNew) {
            this->SetTarget(theTarget->mUnit);
            this->mSubGroup->AttackWith(this, this->mTarget->mComplexUnit, &theTile);
        }
    }
    return this->mTarget != NULL;
}

// The unit class's slot +0x40 is called with mUnit here.
// FUNCTION: EXE 0x4645ba
// Moves the unit away from danger: out of a nearby calamity area or from contagious targets, back towards
// the sub group's centre, else lets the unit class flee and runs from the nearest target
void EECPAttackSubGroupUnit::Flee()
{
    std::multimap<float, EEComplexUnit*> theNearest;
    GE3DPoint theTo;
    EEAIAStarBounds theBounds;
    U2DPoint<long> theTile;
    GE3DPoint theFrom;
    std::deque<EEComplexUnit*>::iterator theIt;
    EEComplexUnit* theNearestUnit = NULL;
    float theDistance = 0.0f;
    float theBest = 0.0f;
    if (this->mState == kASGUStateAvoidCalamity) {
        std::deque<EEComplexUnit*>* theUnits =
            this->mSubGroup->mPlayer2->mCivilizationManager->mUnitManager->GetUnits(0x27);
        if (theUnits != NULL) {
            for (theIt = theUnits->begin(); theIt != theUnits->end(); theIt++) {
                theDistance = this->mUnit->DistanceSquaredTo(*theIt);
                if (theNearestUnit == NULL || theDistance < theBest) {
                    theBest = theDistance;
                    theNearestUnit = *theIt;
                }
            }
            if (theBest != 0.0f && theBest <= 256.0f && theNearestUnit != NULL   // the nearest one (was the last distance)
                && gContinentManager.GetContinentAt(theNearestUnit) == gContinentManager.GetContinentAt(this->mUnit)
                && EEGetAreaEffectBounds(3, theNearestUnit, &theBounds)) {
                EEFOLocationScreenEnsureContinentAndPathOverlay theScreen;
                bool theFlag = (this->mUnit->GetUnitClass()->mDomains >> 2) & 1;
                theScreen.mContinentID = gContinentManager.GetContinentID(this->mUnit);
                theScreen.mUseWaterAreas = !theFlag;
                if (EEFOFindLocation(theBounds, theTile, gPhysics->mRandom, theScreen, 15)) {
                    EEUGLocation* theGoal = new EEUGLocation(gPhysics->GetRandom(0.05f, 0.95f) + theTile.GetX(),
                        gPhysics->GetRandom(0.05f, 0.95f) + theTile.GetY(), 0.0f, true, true, false, 0.0f, false, false);
                    if (this->mSubGroup->GiveGoal(this->mUnit, theGoal, 0, kActionPrepareToMove))
                        return;
                    if (theGoal != NULL)
                        delete theGoal;
                }
            }
        }
        std::vector<EECPASTargetUnit*>& theTargets = this->mSubGroup->mTargets;
        for (long i = theTargets.size() - 1; i >= 0; i--) {
            if (theTargets[i]->mUnit->mCalamityFlags == 0)
                continue;
            unsigned long theFlags = 0;
            this->mUnit->GetCatchableContagions(theTargets[i]->mUnit, theFlags);
            if (theFlags == 0)
                continue;
            theDistance = this->mUnit->DistanceSquaredTo(theTargets[i]->mUnit);
            theNearest.insert(std::make_pair(theDistance, theTargets[i]->mUnit));
        }
        if (theNearest.size() > 0) {
            EETileCoordinate theUnitTile = this->mUnit->GetTile();
            EEFleeFromLocationScreenCheckPathAreaID theScreen;
            bool theKind = !(this->mUnit->GetUnitClass()->mDomains & kMovementSea);
            theScreen.mAreaID = gContinentManager.GetContinent(theUnitTile, (this->mUnit->GetUnitClass()->mDomains & kMovementSea) ? 1 : 2);
            theScreen.mUseWaterAreas = theKind;
            theNearest.begin()->second->GetPosition(theTo);
            if (this->mUnit->FindFleePoint(&theTo, &theFrom, 12, -18.0f, 18.0f, 3.0f, 8.0f, &theScreen)) {
                EEUGLocation* theGoal = new EEUGLocation(&theFrom, true, true, false, 0.0f, false, false);
                if (this->mSubGroup->GiveGoal(this->mUnit, theGoal, 0, kActionPrepareToMove))
                    return;
                if (theGoal != NULL)
                    delete theGoal;
            }
        }
    }
    theTile = this->mSubGroup->ComputeCenterPoint(0);
    if (::gWorld.mMap->GetTileAtLocation(theTile) != NULL && (theTile.GetX() != this->mUnit->mTileX || theTile.GetY() != this->mUnit->mTileY)) {
        U2DPoint<long> theUnitPoint(this->mUnit->mTileX, this->mUnit->mTileY);
        if (theTile.DistanceToSquared(theUnitPoint) > 400) {
            EETileCoordinate theUnitTile = this->mUnit->GetTile();
            EEFleeFromLocationScreenCheckPathAreaID theScreen;
            bool theKind = !(this->mUnit->GetUnitClass()->mDomains & kMovementSea);
            theScreen.mAreaID = gContinentManager.GetContinent(theUnitTile, (this->mUnit->GetUnitClass()->mDomains & kMovementSea) ? 1 : 2);
            theScreen.mUseWaterAreas = theKind;
            theTo.mX = theTile.GetX() + 0.5f;
            theTo.mY = theTile.GetY() + 0.5f;
            this->mUnit->GetPosition(theFrom);
            float theHeading = EEGetHeadingBetween(&theFrom, &theTo);
            theTo.mX = UMCos(theHeading) * 2.0f + theFrom.GetX();
            theTo.mY = UMCos(1.5707964f - theHeading) * 2.0f + theFrom.GetY();
            if (this->mUnit->FindFleePoint(&theTo, &theFrom, 12, -18.0f, 18.0f, 3.0f, 8.0f, &theScreen)) {
                EEUGLocation* theGoal = new EEUGLocation(&theFrom, true, true, false, 0.0f, false, false);
                if (this->mSubGroup->GiveGoal(this->mUnit, theGoal, 0, kActionPrepareToMove))
                    return;   // success ends the flee, as the other two steps (was: return on failure)
                if (theGoal != NULL)
                    delete theGoal;
            }
        }
    }
    this->mUnit->GetUnitClass()->Flee(this->mUnit);
    if ((this->mUnit->mCommand != NULL ? this->mUnit->mCommand->mType : 0) != kActionIdle)
        return;
    theBest = 0.0f;
    theNearestUnit = NULL;
    for (long j = this->mSubGroup->mTargets.size() - 1; j >= 0; j--) {
        float theTargetDistance = this->mUnit->DistanceSquaredTo(this->mSubGroup->mTargets[j]->mUnit);
        if (theBest == 0.0f || theTargetDistance < theBest) {
            theBest = theTargetDistance;
            theNearestUnit = this->mSubGroup->mTargets[j]->mUnit;
        }
    }
    if (theNearestUnit != NULL) {
        EETileCoordinate theUnitTile = this->mUnit->GetTile();
        EEFleeFromLocationScreenCheckPathAreaID theScreen;
        bool theKind = !(this->mUnit->GetUnitClass()->mDomains & kMovementSea);
        theScreen.mAreaID = gContinentManager.GetContinent(theUnitTile, (this->mUnit->GetUnitClass()->mDomains & kMovementSea) ? 1 : 2);
        theScreen.mUseWaterAreas = theKind;
        theNearestUnit->GetPosition(theTo);
        this->mUnit->GetPosition(theFrom);
        if (this->mUnit->FindFleePoint(&theTo, &theFrom, 12, -18.0f, 18.0f, 3.0f, 8.0f, &theScreen)) {
            EEUGLocation* theGoal = new EEUGLocation(&theFrom, true, true, false, 0.0f, false, false);
            if (!this->mSubGroup->GiveGoal(this->mUnit, theGoal, 0, kActionPrepareToMove) && theGoal != NULL)
                delete theGoal;
        }
    }
}

// FUNCTION: EXE 0x464d4f
// Accepts a flee point on the map whose land or water area matches the screen's area
bool EEFleeFromLocationScreenCheckPathAreaID::Screen(const GE3DPoint& inPoint)
{
    EETileCoordinate theTile;
    theTile.mX = (long)inPoint.GetX();
    theTile.mY = (long)inPoint.GetY();
    if ((unsigned long)theTile.mX < gMapWidth && (unsigned long)theTile.mY < gMapHeight
        && this->mAreaID == gContinentManager.GetContinent(theTile, this->mUseWaterAreas ? 2 : 1))
        return true;
    return false;
}

// FUNCTION: EXE 0x464dcb
// Returns the targeting-table weight of a target: bonuses when it attacks this unit, is in range, hits harder,
// is faster or immobile, closeness, plus a to-hit term (looked up against this unit's own class)
long EECPAttackSubGroupUnit::GetTargetWeight(EECPASTargetUnit* inTarget)
{
    EEComplexUnit* theTarget = inTarget->mUnit;
    EEUnitClassUpgrade* theStats = this->mUnit->mStats;
    float theRange = theStats->GetRange();
    EEUnitClassUpgrade* theTargetStats = theTarget->mStats;
    float theRangeSquared = theRange * theRange;
    long theKind = 2;
    float theToHit = gToHitTable.GetAttackFactor(this->mUnit->mUnitClass->mAmmoType, theTarget->mUnitClass, this->mUnit->mPlayer->mPlayerID,   // the target's class, as GetToHit (was this unit's)
                                          theTarget->mPlayer->mPlayerID);
    long theX = theTarget->mTileX;
    long theY = theTarget->mTileY;
    long theResult = 5;
    this->mUnit->GetNearestFootprintTile(theTarget, &theX, &theY);
    long theDX = this->mUnit->mTileX - theX;
    long theDY = this->mUnit->mTileY - theY;
    float theDistance = (float)(theDX * theDX + theDY * theDY);
    if (!(theDistance > 0.0f))
        theDistance = 1.0f;
    if (!(theRange > 2.0f))
        theKind = 0;
    else if (theStats->mLineOfSight + 0.5f >= theRange)
        theKind = 1;
    EEUGAttackUnit* theGoal = dynamic_cast<EEUGAttackUnit*>(theTarget->GetGoal());
    if (theGoal != NULL && theGoal->mTarget->mComplexUnit == this->mUnit)
        theResult = EEAIUnitTargetingTable::sTable[theKind][0] + 5;
    if (!(theDistance > theRangeSquared))
        theResult += EEAIUnitTargetingTable::sTable[theKind][2];
    if (theStats->GetAttack() < theTargetStats->GetAttack())
        theResult += EEAIUnitTargetingTable::sTable[theKind][4];
    if (theStats->GetMaxVelocity() < theTargetStats->GetMaxVelocity())
        theResult += EEAIUnitTargetingTable::sTable[theKind][3];
    if (theTargetStats->mMaxVelocity == 0.0f)
        theResult += EEAIUnitTargetingTable::sTable[theKind][8];
    if (!theTarget->mUnitClass->IsLandBuilding())
        theResult += (long)(EEAIUnitTargetingTable::sTable[theKind][7] / theDistance);
    return (long)(EEAIUnitTargetingTable::sTable[theKind][6] * theToHit) + theResult;
}

// FUNCTION: EXE 0x464f98
// Rolls the unit's courage, aggression and two other personality traits from its class (melee, ranged
// or unarmed), raising two of them for veterans
void EECPAttackSubGroupUnit::RandomizePersonality()
{
    float theValue;
    this->mF68 = gPhysics->GetRandom(0.0f, 1.0f);
    this->mAggression = gPhysics->GetRandom(0.0f, 1.0f);
    this->mCourage = gPhysics->GetRandom(0.0f, 1.0f);
    this->mF6C = gPhysics->GetRandom(0.0f, 1.0f);
    EEUnitClass* theClass = this->GetUnit()->mUnitClass;
    if (theClass->mAttack != 0) {
        if (theClass->mIsMelee) {
            this->mF68 = gPhysics->GetRandom(0.45f, 0.95f);
            this->mF6C = gPhysics->GetRandom(kHeightEpsilon, 0.1f);
            if (!this->GetUnit()->mUnitClass->mHasCalamityPowers)
                this->mCourage = gPhysics->GetRandom(0.75f, 1.0f);
            else
                this->mCourage = gPhysics->GetRandom(0.05f, 0.25f);
            if (gPhysics->mRandom.Generate() > 0.1f)
                this->mAggression = gPhysics->GetRandom(0.45f, 0.95f);
            else
                this->mAggression = gPhysics->GetRandom(0.65f, 1.0f);
        } else {
            if (this->GetUnit()->mStats->mAreaEffect != 0.0f)
                this->mF6C = gPhysics->GetRandom(0.69f, 1.0f);
            else
                this->mF6C = gPhysics->GetRandom(0.35f, 0.7f);
            if (this->GetUnit()->mStats->mMaxVelocity > 0.5f) {
                this->mCourage = gPhysics->GetRandom(0.5f, 1.0f);
                if (this->GetUnit()->mStats->mMaxVelocity > 0.75f)
                    this->mAggression = gPhysics->GetRandom(0.35f, 1.0f);
                else
                    this->mAggression = gPhysics->GetRandom(0.35f, 0.55f);
            } else {
                this->mAggression = gPhysics->GetRandom(0.05f, 0.8f);
                this->mCourage = gPhysics->GetRandom(0.25f, 0.75f);
            }
            if (this->GetUnit()->mExperience > 10) {
                theValue = this->mF6C + 0.1f;
                if (1.0f < theValue)
                    theValue = 1.0f;
                this->mF6C = theValue;
            }
        }
    } else {
        this->mF68 = 0.0f;
        this->mAggression = gPhysics->GetRandom(0.5f, 1.0f);
        this->mCourage = 0.0f;
        this->mF6C = gPhysics->GetRandom(0.0f, 1.0f);
    }
    if (!(this->GetUnit()->mExperience > 10))
        return;
    float theBonus = ((float)this->GetUnit()->mExperience / 10.0f) * 0.1f;
    if (theBonus > 0.0f) {
        theBonus += this->mF6C;
        if (1.0f < theBonus)
            theBonus = 1.0f;
        this->mF6C = theBonus;
    }
    theBonus = ((float)this->GetUnit()->mExperience / 10.0f) * 0.1f;
    if (!(theBonus > 0.0f))
        return;
    theBonus += this->mCourage;
    if (1.0f < theBonus)
        theBonus = 1.0f;
    this->mCourage = theBonus;
}

// FUNCTION: EXE 0x4652e0
// Tells whether the unit is out of the fight or dead
bool EECPAttackSubGroupUnit::IsDone()
{
    return this->mIsOutOfFight || this->GetUnit()->mHitPoints == 0;
}

// NEAR (59.3): case layout as the original; it keeps 0 in edi and loads the commander counter and gNumPlayers
// before the GetRandom call.
// FUNCTION: EXE 0x4652f6
// Enters a state and runs its entry action (retarget and schedule the next one, move to the escort, flee,
// spread contagion, retreat, or release the target and pick the post-attack mode)
void EECPAttackSubGroupUnit::SetState(long inState, unsigned long inTime)
{
    this->mState = inState;
    switch (inState) {
    case kASGUStateAttack:
        this->Retarget();
        if (this->GetUnit()->mUnitClass->mAIKind != kAIKindFieldWeapon) {
            long theCount = this->mSubGroup->mGroup->mCommander->mUpdatesThisTick;
            long theFactor = gNumPlayers;
            this->mNextRetargetTime = (theCount + theFactor * 5) * 100 + inTime + gPhysics->GetRandom(4000L, 8000L);
        } else
            this->mNextRetargetTime = gPhysics->GetRandom(2000L, 4000L) + inTime;
        break;
    case kASGUStateEscort:
        this->MoveTo(this->mEscort != NULL ? this->mEscort->mUnit : NULL, true, false);
        break;
    case kASGUStateIdle:
        if (this->mEscort != NULL)
            this->mEscort->Dereference();
        this->mEscort = NULL;
        this->ReleaseTarget();
        this->mPostAttackMode = 0;
        break;
    case kASGUStateSpreadContagion:
        this->SpreadContagion();
        break;
    case kASGUStateRetreat:
        this->Retreat();
        break;
    case kASGUStateFlee:
        this->Flee();
        break;
    case kASGUStateAvoidCalamity:
        this->Flee();
        break;
    case kASGUStateChooseAction:
        // the target is tested before it is released (was after: always NULL)
        if (this->mTargetPickResult == -1 && (this->mTarget == NULL || this->mTarget->mHitPoints <= 0)) {
            this->mTargetPickResult = 0;
            this->mPostAttackMode = 1;
        } else
            this->mPostAttackMode = 2;
        this->ReleaseTarget();
        break;
    }
}

// NEAR (60.5): the original keeps the !Virtual50(..) flags of the other cases in a dead byte store ([ebp+0xb], an
// ebp frame); ours drops them. Case order and tail merges follow the original.
// FUNCTION: EXE 0x46544c
// Advances the unit's state machine by checking the current state's conditions; next update in 1 second
void EECPAttackSubGroupUnit::Update(unsigned long inTime)
{
    bool theDone;
    if (this->GetUnit()->mTileX == -10000 && this->mState != kASGUStateContained)
        this->mState = kASGUStateContained;
    switch (this->mState) {
    case kASGUStateRetreat:
        theDone = !this->CheckCondition(kASGUConditionSubGroupHasTargets, inTime);
        if (theDone)
            this->SetState(kASGUStateIdle, inTime);
        else {
            theDone = !this->CheckCondition(kASGUConditionShouldRetreat, inTime);
            if (theDone)
                this->SetState(kASGUStateChooseAction, inTime);
            else if (this->CheckCondition(kASGUConditionNearPathEnd, inTime))
                this->SetState(kASGUStateRetreat, inTime);
        }
        break;
    case kASGUStateAvoidCalamity:
        theDone = !this->CheckCondition(kASGUConditionSubGroupHasTargets, inTime);
        if (theDone)
            this->SetState(kASGUStateIdle, inTime);
        else {
            if (this->CheckCondition(kASGUConditionCalamityFlags, inTime)) {
                if (this->CheckCondition(kASGUConditionContagionTargetFound, inTime)) {
                    this->SetState(kASGUStateSpreadContagion, inTime);
                    break;
                }
                theDone = !this->CheckCondition(kASGUConditionExposedToCalamity, inTime);
                if (theDone) {
                    this->SetState(kASGUStateIdle, inTime);
                    break;
                }
            }
            if (this->CheckCondition(kASGUConditionNearPathEnd, inTime)) {
                if (this->CheckCondition(kASGUConditionExposedToCalamity, inTime))
                    this->SetState(kASGUStateAvoidCalamity, inTime);
                else
                    this->SetState(kASGUStateIdle, inTime);
            }
        }
        break;
    case kASGUStateChooseAction:
        theDone = !this->CheckCondition(kASGUConditionSubGroupHasTargets, inTime);
        if (theDone)
            this->SetState(kASGUStateIdle, inTime);
        else {
            switch (this->mPostAttackMode) {
            case 1:
                this->SetState(kASGUStateFlee, inTime);
                theDone = true;
                break;
            case 2:
                this->SetState(kASGUStateAttack, inTime);
                theDone = true;
                break;
            case 5:
                this->SetState(kASGUStateContained, inTime);
                theDone = true;
                break;
            }
        }
        if (!theDone && this->CheckCondition(kASGUConditionShouldRetreat, inTime))
            this->SetState(kASGUStateRetreat, inTime);
        break;
    case kASGUStateContained:
        if (this->CheckCondition(kASGUConditionOnMap, inTime))
            this->SetState(kASGUStateIdle, inTime);
        break;
    case kASGUStateSpreadContagion:
        theDone = !this->CheckCondition(kASGUConditionSubGroupHasTargets, inTime);
        if (theDone)
            this->SetState(kASGUStateIdle, inTime);
        else if (this->CheckCondition(kASGUConditionContagionTargetLost, inTime)) {
            if (this->CheckCondition(kASGUConditionCalamityFlags, inTime) && this->CheckCondition(kASGUConditionContagionTargetFound, inTime))
                this->SetState(kASGUStateSpreadContagion, inTime);
            else
                this->SetState(kASGUStateIdle, inTime);
        }
        break;
    case kASGUStateAttack:
        theDone = !this->CheckCondition(kASGUConditionSubGroupHasTargets, inTime);
        if (theDone)
            this->SetState(kASGUStateIdle, inTime);
        else if (this->CheckCondition(kASGUConditionCalamityFlags, inTime) && this->CheckCondition(kASGUConditionContagionTargetFound, inTime))
            this->SetState(kASGUStateSpreadContagion, inTime);
        else if (this->CheckCondition(kASGUConditionExposedToCalamity, inTime))
            this->SetState(kASGUStateAvoidCalamity, inTime);
        else if (this->CheckCondition(kASGUConditionShouldRetreat, inTime))
            this->SetState(kASGUStateRetreat, inTime);
        else {
            theDone = !this->CheckCondition(kASGUConditionHasTarget, inTime);
            if (theDone)
                this->SetState(kASGUStateChooseAction, inTime);
            else if (this->CheckCondition(kASGUConditionRetargetDue, inTime))
                this->SetState(kASGUStateAttack, inTime);
        }
        break;
    case kASGUStateFlee:
        theDone = !this->CheckCondition(kASGUConditionSubGroupHasTargets, inTime);
        if (theDone)
            this->SetState(kASGUStateIdle, inTime);
        else if (this->CheckCondition(kASGUConditionInDanger, inTime))
            this->SetState(kASGUStateFlee, inTime);
        else if (this->CheckCondition(kASGUConditionNearPathEnd, inTime))
            this->SetState(kASGUStateIdle, inTime);
        break;
    case kASGUStateIdle:
        if (this->CheckCondition(kASGUConditionSubGroupHasTargets, inTime))
            this->SetState(kASGUStateChooseAction, inTime);
        break;
    }
    this->mNextUpdateTime = inTime + 1000;
}

// FUNCTION: EXE 0x46577b
// Tells whether the unit can damage the target and is not ignoring it
bool EECPAttackSubGroupUnit::CanAttack(EECPASTargetUnit* inTarget)
{
    EEUnitClass* theClass = inTarget->mUnit->mUnitClass;
    long theKind = this->GetUnit()->mUnitClass->mAmmoType;
    long theArmor = theClass->mFamily;
    bool theResult = false;
    if (theClass->CanBeAttacked()) {
        if (gU4E8708[theArmor][theKind] != 0.0f) {
            EEComplexUnit* theTarget = inTarget->mUnit;
            theResult = !this->GetUnit()->IsTargetIgnored(theTarget, true);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4657e5
// Tells whether the unit is below a quarter health and was attacked or is targeted by a sub group target
bool EECPAttackSubGroupUnit::IsInDanger()
{
    EEComplexUnit* theUnit = this->GetUnit();
    bool theResult = false;
    if ((float)theUnit->mHitPoints / (float)theUnit->mStats->mMaxHitPoints < 0.25f) {
        theResult = theUnit->mLastAttacker != NULL;
        if (!theResult) {
            EECPAttackSubGroup* theSubGroup = this->mSubGroup;
            for (long i = theSubGroup->mTargets.size() - 1; i >= 0 && !theResult; i--) {
                EEUnitGoal* theGoal = theSubGroup->mTargets[i]->mUnit->mGoalState;
                if (theGoal != NULL && theGoal->mKind == kGoalAttackUnit)
                    theResult = static_cast<EEUGAttackUnit*>(theGoal)->mTarget == theUnit;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x465859
// Tells whether a calamity-carrying target within 8 tiles could infect the unit (not under protection)
bool EECPAttackSubGroupUnit::IsExposedToCalamity()
{
    std::vector<unsigned long> theCalamities;
    bool theResult = false;
    if (this->mSubGroup->mCounts.mHasCalamity) {
        for (long i = this->mSubGroup->mTargets.size() - 1; i >= 0 && !theResult; i--) {
            EEComplexUnit* theUnit = this->mSubGroup->mTargets[i]->mUnit;
            if (theUnit->mCalamityFlags == 0
                || gAEManager.HasAreaEffect2(3, this->mUnit->GetTileX(), this->mUnit->GetTileY(), theUnit->mPlayer->GetPlayerID()))
                continue;
            if (this->mUnit->DistanceSquaredTo(theUnit) > 64.0f)
                theResult = false;
            else {
                theCalamities.clear();
                EESplitCalamityFlags(theUnit->mCalamityFlags, theCalamities);
                theResult = false;
                for (long j = theCalamities.size() - 1; j >= 0 && !theResult; j--) {
                    if (!this->mUnit->HasCalamityFlag(theCalamities[j]) && this->mUnit->GetUnitClass()->IsAffectedByCalamity(theCalamities[j]))
                        theResult = true;
                    else
                        theResult = false;
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4659a0
// Prejudices do not change targeting through this slot: no effect
void EECPAttackSubGroupUnit::ApplyPrejudices(EECPASTargetUnit* inTarget)
{
    // the search for a prejudice matching the target's class discarded its result, so it is gone
}

// FUNCTION: EXE 0x4659ee
// Reads the entry from a saved game (location, unit/target/escort ids, timers, traits, state); returns the
// first read error
long EECPAttackSubGroupUnit::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theID = 0;
    long theResult = this->mLocation.Load(inMajorVersion, inMinorVersion, inFile);
    theID = 0;
    if (theResult == kErrorOK)
        theResult = inFile.Read(theID);
    this->mUnit = reinterpret_cast<EEComplexUnit*>(theID);
    theID = 0;
    if (theResult == kErrorOK)
        theResult = inFile.Read(theID);
    this->mTarget = reinterpret_cast<EEComplexUnit*>(theID);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mNextRetargetTime);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mImportance);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mIsImportant);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mNeedsOrders);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mIsOutOfFight);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mType);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mF6C);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mAggression);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mCourage);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mF68);
    if (theResult != kErrorOK)
        return theResult;
    theID = 0;
    theResult = inFile.Read(theID);
    this->mEscort = reinterpret_cast<EECPAttackSubGroupUnit*>(theID);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mState);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mPostAttackMode);
    return theResult;
}

// NEAR: the original reads the last waypoint's y before converting its x (a float pair converted in one place?).
// FUNCTION: EXE 0x465b2d
// Orders the unit to move to another unit's tile (or its last waypoint) unless that unit is contained
void EECPAttackSubGroupUnit::MoveTo(EEComplexUnit* inUnit, bool inArg2, bool inUseDestination)
{
    std::vector<EETileCoordinate> theTiles;
    GE3DPoint thePoint;
    EETileCoordinate theTile;
    if (inUnit != NULL && inUnit->mContainer == NULL) {
        if (inUseDestination && inUnit->mWaypoints.size() != 0) {
            float theY = inUnit->mWaypoints.back().mLocation.mY;
            theTile.mX = (long)inUnit->mWaypoints.back().mLocation.mX;
            theTile.mY = (long)theY;
        } else {
            theTile.mX = inUnit->mTileX;
            theTile.mY = inUnit->mTileY;
        }
        thePoint.mX = theTile.mX + 0.5f;
        thePoint.mY = theTile.mY + 0.5f;
        EEUGLocation* theGoal = new EEUGLocation(&thePoint, true, !inArg2, false, 0.0f, false, false);
        if (!this->mSubGroup->GiveGoal(this->GetUnit(), theGoal, 0, kActionPrepareToMove) && theGoal != NULL)
            delete theGoal;
    }
}

// FUNCTION: EXE 0x465c2f
// Drops the target reference and clears the retarget time
void EECPAttackSubGroupUnit::ReleaseTarget()
{
    if (this->mTarget != NULL)
        this->mTarget->Dereference();
    this->mTarget = NULL;
    this->mNextRetargetTime = 0;
}

// FUNCTION: EXE 0x465c48
// Resolves the saved unit, target and escort ids into references after loading; returns 0
long EECPAttackSubGroupUnit::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    this->mLocation.Restore(inMajorVersion, inMinorVersion);
    if (reinterpret_cast<long>(this->GetUnit()) != -1) {
        this->mUnit = dynamic_cast<EEComplexUnit*>(static_cast<EEUnit*>(gWorld.GetUnit(reinterpret_cast<unsigned long>(this->GetUnit()))));
        if (this->GetUnit() != NULL)
            this->GetUnit()->Reference();
    } else
        this->mUnit = NULL;
    if (reinterpret_cast<long>(this->mTarget) != -1) {
        this->mTarget = dynamic_cast<EEComplexUnit*>(static_cast<EEUnit*>(gWorld.GetUnit(reinterpret_cast<unsigned long>(this->mTarget))));
        if (this->mTarget != NULL)
            this->mTarget->Reference();
    } else
        this->mTarget = NULL;
    if (reinterpret_cast<long>(this->mEscort) != -1) {
        this->mEscort = this->mSubGroup->FindUnit(dynamic_cast<EEComplexUnit*>(static_cast<EEUnit*>(gWorld.GetUnit(reinterpret_cast<unsigned long>(this->mEscort)))));
        if (this->mEscort != NULL)
            this->mEscort->Reference();
    } else
        this->mEscort = NULL;
    return 0;
}

// FUNCTION: EXE 0x465d0c
// Drops the sub group reference
void EECPAttackSubGroupUnit::ReleaseSubGroup()
{
    if (this->mSubGroup != NULL)
        this->mSubGroup->Dereference();
    this->mSubGroup = NULL;
}

// FUNCTION: EXE 0x465d22
// Drops the unit reference
void EECPAttackSubGroupUnit::ReleaseUnit()
{
    if (this->GetUnit() != NULL)
        this->GetUnit()->Dereference();
    this->mUnit = NULL;
}

// FUNCTION: EXE 0x465d37
// Writes the entry to the saved game (location, unit/target/escort ids, timers, traits, state); returns the
// first write error
long EECPAttackSubGroupUnit::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theID = 0;
    long theResult = this->mLocation.Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == kErrorOK) {
        theID = this->GetUnit() != NULL ? this->GetUnit()->GetUnitID() : 0xffffffff;
        theResult = outFile.Write(theID);
    }
    theID = this->mTarget != NULL ? this->mTarget->GetUnitID() : 0xffffffff;
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(theID);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mNextRetargetTime);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mImportance);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mIsImportant);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mNeedsOrders);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mIsOutOfFight);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mType);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mF6C);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mAggression);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mCourage);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mF68);
    if (theResult != kErrorOK)
        return theResult;
    theID = this->mEscort != NULL ? this->mEscort->mUnit->GetUnitID() : 0xffffffff;
    theResult = outFile.Write(theID);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mState);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mPostAttackMode);
    return theResult;
}

// FUNCTION: EXE 0x465eb6
// Replaces the target with a referenced unit
void EECPAttackSubGroupUnit::SetTarget(EEComplexUnit* inUnit)
{
    this->ReleaseTarget();
    this->mTarget = inUnit;
    this->mTarget->Reference();
}

// FUNCTION: EXE 0x465ecc
// Replaces the controlled unit with a referenced unit
void EECPAttackSubGroupUnit::SetUnit(EEComplexUnit* inUnit)
{
    this->ReleaseUnit();
    this->mUnit = inUnit;
    this->GetUnit()->Reference();
}

// NEAR (69.2): the original places `theResult = true` inline after the tests (ours at the end) and loads the player
// id before the target's tile.
// FUNCTION: EXE 0x465ee2
// Tells whether the contagion target is gone, dead, contained, or standing in the unit's own protection area
bool EECPAttackSubGroupUnit::IsContagionTargetLost()
{
    std::vector<unsigned long> theCalamities;
    bool theResult = false;
    bool theGone = this->mTarget == NULL || this->mTarget->mHitPoints <= 0 || this->mTarget->mContainer != NULL;
    if (theGone)
        theResult = true;
    else if (this->GetUnit()->mCalamityFlags != 0) {
        EESplitCalamityFlags(this->GetUnit()->mCalamityFlags, theCalamities);
        theResult = gAEManager.HasAreaEffect2(3, this->mTarget->GetTileX(), this->mTarget->GetTileY(), this->GetUnit()->mPlayer->GetPlayerID()) != 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x465f65
// Evaluates one state-machine condition (targets, escort, danger, calamity, waypoints, retarget timer ...)
bool EECPAttackSubGroupUnit::CheckCondition(long inState, unsigned long inTime)
{
    bool theResult = false;
    switch (inState) {
    case kASGUConditionShouldRetreat:
        theResult = this->ShouldRetreat();
        break;
    case kASGUConditionExposedToCalamity:
        theResult = this->IsExposedToCalamity();
        break;
    case kASGUConditionCalamityFlags:
        theResult = this->GetUnit()->mCalamityFlags != 0;
        break;
    case kASGUConditionNearPathEnd:
        theResult = (long)this->GetUnit()->mWaypoints.size() <= 2;
        break;
    case kASGUConditionPostAttackRetarget:
        theResult = this->mPostAttackMode == 2;
        break;
    case kASGUConditionPostAttackFlee:
        theResult = this->mPostAttackMode == 1;
        break;
    case kASGUConditionOnMap:
        theResult = this->GetUnit()->mTileX != -10000;
        break;
    case kASGUConditionContagionTargetLost:
        theResult = this->IsContagionTargetLost();
        break;
    case kASGUConditionContagionTargetFound:
        theResult = this->PickContagionTarget(false) != NULL;
        break;
    case kASGUConditionInDanger:
        theResult = this->IsInDanger();
        break;
    case kASGUConditionRetargetDue:
        theResult = !(inTime < this->mNextRetargetTime);
        break;
    case kASGUConditionSubGroupHasTargets:
        theResult = this->mSubGroup->GetNumTargets() > 0;
        break;
    case kASGUConditionHasEscort:
        if (this->mEscort != NULL && this->mEscort->mUnit->mHitPoints > 0)
            theResult = true;
        break;
    case kASGUConditionHasTarget:
        if (this->mTarget != NULL && this->mTarget->mHitPoints > 0)
            theResult = true;
        break;
    case kASGUConditionAlways:
        theResult = true;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x466066
// Lets an SAS explosive expert or cyber ninja (65% of the time, with enough power) cast its calamity on
// the highest-rated building target; returns whether the order was given
bool EECPAttackSubGroupUnit::TryCastCalamity()
{
    long theType = this->GetUnit()->mUnitClass->mUnitType;
    if ((theType != kUnitTypeInf10SASExplosiveExpert && theType != kUnitTypeInf15CyberNinja) || !(gPhysics->mRandom.Generate() <= 0.65))
        return false;
    EEUnitCalamityClass* theClass = NULL;
    unsigned long theCalamity = (this->GetUnit()->mUnitClass->mUnitType != kUnitTypeInf15CyberNinja) + 0x1c;
    USingleton<EECalamityManager>::GetInstance()->GetCalamityClass(theCalamity, theClass);
    if (!(this->GetUnit()->GetPower() >= theClass->GetCost()))
        return false;
    EECPASTargetUnit* theChoice = NULL;
    float theBest = 0.0f;
    std::vector<EECPASTargetUnit*>& theTargets = this->mSubGroup->mTargets;
    for (long i = theTargets.size() - 1; i >= 0; i--) {
        EECPASTargetUnit* theTarget = theTargets[i];
        if (this->GetUnit()->IsTargetIgnored(theTarget->mUnit, true) || !theClass->IsValidTarget(this->GetUnit(), theTarget->mUnit))
            continue;
        float theValue;
        if (theCalamity == 0x1c)
            theValue = ASGUWonderValue(theTarget->mUnit);
        else
            theValue = ASGUWonderValue2(theTarget->mUnit);
        if (theValue > theBest) {
            theBest = theValue;
            theChoice = theTarget;
        }
    }
    if (theChoice == NULL)
        return false;
    EEUGCastCalamity* theGoal = new EEUGCastCalamity(this->GetUnit(), theChoice->mUnit, this->GetUnit()->GetTileX(),
        this->GetUnit()->GetTileY(), theCalamity == 0x1c ? 0x3aa8 : 0x277a, true);
    this->SetTarget(theChoice->mUnit);
    if (this->mSubGroup->GiveGoal(this->GetUnit(), theGoal, 0, kActionPrepareToMove))
        return true;
    if (theGoal != NULL)
        delete theGoal;
    return false;
}

// FUNCTION: EXE 0x466209
// Tells whether the unit (or its destination) is farther than the given squared distance from its escort
bool EECPAttackSubGroupUnit::IsFarFromEscort(float inDistanceSquared)
{
    GE3DPoint theOwn;
    GE3DPoint theOther;
    bool theResult = false;
    if (this->mEscort != NULL) {
        this->mEscort->mUnit->GetPosition(theOther);
        if ((long)this->GetUnit()->mWaypoints.size() > 0)
            theOwn = this->GetUnit()->mWaypoints.back().mLocation;
        else
            this->GetUnit()->GetPosition(theOwn);
        theResult = theOwn.DistanceToSquared(theOther) > inDistanceSquared;
    }
    return theResult;
}

// FUNCTION: EXE 0x466295
// Retreats by fleeing
void EECPAttackSubGroupUnit::Retreat()
{
    this->Flee();
}

// NEAR (72.7): the original keeps theResult in bl (ours spills it to a frame byte).
// FUNCTION: EXE 0x46629a
// Tells whether the target is still a live, on-map enemy the unit is attacking (or has no target)
bool EECPAttackSubGroupUnit::IsTargetValid()
{
    EEComplexUnit* theTarget = this->mTarget;
    if (theTarget != NULL
        && !(theTarget->mHitPoints > 0 && theTarget->mPlayer != this->mSubGroup->mPlayer2
             && this->mSubGroup->mPlayer2->mEnemies[theTarget->mPlayer->mPlayerID] != 0 && theTarget->mTileX != -10000
             && this->GetUnit()->mActiveGoal != NULL
             && (this->GetUnit()->mActiveGoal->mKind == kGoalAttackUnit
                 || (this->GetUnit()->mGoalQueues[0].mGoals.back() != NULL && this->GetUnit()->mGoalQueues[0].mGoals.back()->mKind == 2))))
        return false;
    return true;
}

// The three `$E` chains 0x462f8a/0x462fdb/0x46302c construct three unreferenced class statics of type
// std::map<unsigned long, std::vector<unsigned long> > (0x4e5ce8 guard 0x4e5cc1 bit 1; 0x4e5cd8 and 0x4e5cc8 guard
// 0x4e5cc0 bits 1/2: two classes), dtor COMDAT 0x46631c. Their declarations belong in a shared header; the
// definitions go here once declared.
// SYNTHETIC: EXE 0x462f8a
// SYNTHETIC: EXE 0x462fdb
// SYNTHETIC: EXE 0x46302c

// TEMPLATE: EXE 0x466372
// TEMPLATE: EXE 0x46639d
// TEMPLATE: EXE 0x466457
// TEMPLATE: EXE 0x466493
// TEMPLATE: EXE 0x46652f
// TEMPLATE: EXE 0x4665a0
// TEMPLATE: EXE 0x466843
// TEMPLATE: EXE 0x466951
// TEMPLATE: EXE 0x466983
// TEMPLATE: EXE 0x4669b5
// TEMPLATE: EXE 0x4669ea
// TEMPLATE: EXE 0x466a15
// TEMPLATE: EXE 0x464dae
// TEMPLATE: EXE 0x466347
// TEMPLATE: EXE 0x4665e4
// TEMPLATE: EXE 0x46663e
// TEMPLATE: EXE 0x4666af
// TEMPLATE: EXE 0x4668a5
// TEMPLATE: EXE 0x466a57
// TEMPLATE: EXE 0x466a89

// ICF-FOLDED: EXE 0x4fca98
// (no address of its own: folded into the shared `xor eax, eax; ret` body the vtable slot points at)
long EECPAttackSubGroupUnit::Virtual18()
{
    return 0;
}

// ICF-FOLDED: EXE 0x4e123d
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EECPAttackSubGroupUnit::Virtual48()
{
}

// ICF-FOLDED: EXE 0x67c0b4
// (no address of its own: folded into the shared `xor al, al; ret` body the vtable slot points at)
bool EECPAttackSubGroupUnit::ShouldRetreat()
{
    return false;
}
