// C:\EEX\Empire Earth\AI Thread\EECPCombatManager.cpp (file name inferred: no __FILE__; RTTI class
// EECPCombatManager, vtable 0x43c090). A computer player's combat manager: 0x71c7f..0x77084 (EECPCivilizationManager.cpp
// ends at 0x71c7f; no $E chain; the sub group filters' out-of-line operator() opens the unit, then the ctor; the
// template COMDATs from 0x75d24 close it; EECPCombatStrategy.cpp starts at 0x77084).
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
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
#include "lle/FSFile.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"
#include "ee/EEStringIDs.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EETeam.h"
#include "ee/EEVictoryCondition.h"
#include "ee/EEVCWonder.h"
#include "ee/EEAIRandom.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEAIAStarStorage.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGAttackUnit.h"
#include "ee/EECPBuildManager.h"
#include "ee/EECPResourceManager.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIRectangleArea.h"
#include "ee/EETown.h"
#include "ee/EEAIContinent.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EECPILocation.h"
#include "ee/EECPAGTask.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EECPASGLand.h"
#include "ee/EECPASGTransport.h"
#include "ee/EECPASGAirTransport.h"
#include "ee/EECPAttackGroup.h"
#include "ee/EECPAttackCommander.h"
#include "ee/EECPUnitRequirement.h"
#include "ee/EECPCombatStrategy.h"
#include "ee/EECPMilitaryAidPackage.h"
#include "ee/EECPDefenseCommander.h"
#include "ee/EECPIntelligence.h"
#include "ee/EECPUnitSelector.h"
#include "ee/EECPPersonnel.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPUnitManager.h"
#include "ee/EECPPlayerInteractionManager.h"
#include "ee/EECPCombatManager.h"
#include "LostDebug.h"   // synthetic, NOT original source (LOST-DEBUG: EE_LOST_DEBUG)
void Fn5B397(long inChunkSize, long inMaxChunks);  // 0x45b397 (cdecl; 0x200, 0x19 from the ctor) (provisional name)
float Fn102F3D(float inMinimum, float inMaximum);   // 0x502f3d (cdecl; a value in [inMinimum, inMaximum]) (provisional name)
extern long gDCGameSpeed;                   // 0x530d24 (EECPDefenseCommander.cpp's name; EpochReached: >= 250) (provisional name)
// 0x563938 = gUnitClassTable[0xa3] (EpochReached: its technology must be researched)
// 0x56420c = gUnitClassTable[0x2d8] (Attack: a transport class whose technology must be researched)

// unit-local .rdata floats (0x43c070..0x43c088 before the vtable, 0x43c098 after it) (provisional names)
static const float kCMFactor = 0.95f;      // 0x43c07c (GetUnitsAvailable/GetUnitLimit without mIsWonderRace)
static const float kCMSoundPitch = 1.17f;  // 0x43c098 (AttackPlayer)
static const float kCMCannonChance[3] = { 0.2f, 0.4f, 0.6f };   // 0x43c080 (Update: by mPlayer->mDifficulty)

// The sub group filters (EECPAttackCommander::Collect / EECPAttackGroup::Collect predicates) (provisional names)
// 0x71c7f: the only out-of-line one (it opens the unit); six flags by the sub group kind (+0x1d8)
class EECPCMSubGroupFilter {
public:
    bool operator()(EECPAttackSubGroup* inGroup);

    bool mNearWater;
    bool mInSpace;
    bool mUseLand;
    bool mUseSea;
    bool mUseAir;
    bool mUseSpace;
};

// 0x76b66 (inline COMDAT after its first user 0x76b19): the sub groups that hold units, are not in state 2006 and
// have a task of kind 10
class EECPCMTaskFilter {
public:
    bool operator()(EECPAttackSubGroup* inGroup)
    {
        return inGroup->GetNumUnits() > 0 && inGroup->mState != kASGStateInTransit && inGroup->mTask.mIsActive
            && inGroup->mTask.mType == kAGTaskAttackLocation;
    }
};

// 0x76bec (inline COMDAT after its first user 0x76c5c): flags +1/+2 by the sub group kind
class EECPCMKindFilter {
public:
    bool operator()(EECPAttackSubGroup* inGroup)
    {
        // single exit: theResult = false covers the nuclear-target case (the original: xor bl, bl inside the chain, mov bl, 1
        // before the switch). NEAR: the original zeroes bl after loading mUnit and does not CSE 1 into ebx
        bool theResult = false;
        EETechTreeNode* theNode;
        if (!inGroup->mTask.mIsActive || inGroup->mTask.mMode != kAGModeWonder || !inGroup->mTask.mUnit
            || (theNode = inGroup->mTask.mUnit->GetTechTreeNode()) == NULL || theNode->mData->mKind != 4) {
            theResult = true;
            switch (inGroup->mKind) {
            case kASGKindSea:
                theResult = this->mNearWater;
                break;
            case kASGKindSeaTransport:
            case kASGKindAirTransport:
            case kASGKindSpaceTransport:
                theResult = false;
                break;
            case kASGKindSpaceship:
                theResult = this->mNearMapEdge;
                break;
            }
        }
        return theResult;
    }

    bool mB0;
    bool mNearWater;
    bool mNearMapEdge;
};

// FUNCTION: EXE 0x471c7f
// Returns true for a sub group that may take a new attack: it holds units, is not in transit or on mission 2, is
// not on a live wonder task, and its kind is wanted by the flags (transports never)
bool EECPCMSubGroupFilter::operator()(EECPAttackSubGroup* inGroup)
{
    bool theResult;
    if (inGroup->GetNumUnits() == 0 || inGroup->mState == kASGStateInTransit || inGroup->mMission == 2
        || (inGroup->mTask.mIsActive && inGroup->mTask.mMode == kAGModeWonder)) {
        theResult = false;
        if (inGroup->mTask.mMode == kAGModeWonder && inGroup->GetNumUnits() != 0 && inGroup->mMission != 2) {
            EEComplexUnit* theUnit = inGroup->mTask.mUnit;
            EETechTreeNode* theNode = NULL;
            if (theUnit)
                theNode = theUnit->GetTechTreeNode();
            if (!theNode || theNode->mData->mKind != 4)
                theResult = true;
        }
    } else {
        theResult = true;
    }
    if (theResult) {
        switch (inGroup->mKind) {
        case kASGKindSea:
        case kASGKindSubmarine:
            theResult = this->mNearWater && this->mUseSea;
            break;
        case kASGKindLand:
            theResult = this->mUseLand;
            break;
        case kASGKindAir:
            theResult = this->mUseAir;
            break;
        case kASGKindSeaTransport:
        case kASGKindAirTransport:
        case kASGKindSpaceTransport:
            theResult = false;
            break;
        case kASGKindHelicopter:
            theResult = this->mUseLand || this->mUseAir;
            break;
        case kASGKindSpaceship:
        case kASGKindSpaceFighter:
            theResult = this->mInSpace && this->mUseSpace;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x471d74
// Creates the combat manager with its personnel, intelligence, attack and defense commanders and unit selector
EECPCombatManager::EECPCombatManager(EEComputerPlayer* inPlayer)
{
    this->mPlayer = inPlayer;
    this->mWasAttacked = false;
    this->mIsWonderRace = false;
    this->mNextGroupID = 1;
    this->mNextSubGroupID = 1;
    this->mPersonnel = new EECPPersonnel(inPlayer);
    this->mIntelligence = new EECPIntelligence(inPlayer);
    this->mAttackCommander = new EECPAttackCommander(inPlayer, this->mPersonnel, this->mIntelligence);
    this->mDefenseCommander = new EECPDefenseCommander(inPlayer);
    this->mDefenseCommander->mAttackCommander = this->mAttackCommander;
    this->mUnitSelector = new EECPUnitSelector(inPlayer);
    Fn5B397(0x200, 0x19);
    this->mImprovementsEnabled = false;
    this->mB63 = false;
    this->mTracksThreats = true;
}

// SYNTHETIC: EXE 0x471e8a
// (the scalar deleting destructor)

// FUNCTION: EXE 0x471ea6
// Destroys the combat manager with its commanders, unit selector, intelligence and personnel
EECPCombatManager::~EECPCombatManager()
{
    delete this->mDefenseCommander;
    delete this->mAttackCommander;
    delete this->mUnitSelector;
    delete this->mIntelligence;
    delete this->mPersonnel;
    this->mPlayer = NULL;
}

// FUNCTION: EXE 0x471f16
// Queues an attack task on a unit (a location task for an own unit) or, without one, on an area against a random
// enemy; a unit id that no longer resolves queues nothing
void EECPCombatManager::AddAttackTask(unsigned long inUnitID, unsigned long inAreaSerial)
{
    EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(inUnitID));
    EEAIArea* theArea = gContinentManager.GetArea(inAreaSerial);
    EECPAGTask theTask;
    EECPILocation theLocation;
    if (inUnitID != 0 && inUnitID != -1) {
        // the unit is gone: there is nothing to attack (its complex unit used to be read through NULL)
        if (theUnit == NULL)
            return;
        theTask.mPlayer = gWorld.mPlayers[theUnit->mPlayer->mPlayerID];
        if (theUnit->mPlayer == this->mPlayer)
            theTask.mType = kAGTaskLocation;
        theTask.SetUnit(theUnit->mComplexUnit);
        theTask.mMode = kAGModeCombat;
        if (theTask.mType == kAGTaskNone)
            theTask.mType = kAGTaskUnit;
    } else {
        if (inAreaSerial == 0)
            return;
        EEPlayer* thePlayer = this->mIntelligence->GetRandomEnemy(this->mPlayer);
        if (thePlayer)
            theTask.mPlayer = gWorld.mPlayers[thePlayer->mPlayerID];
        theTask.mType = kAGTaskLocation;
        if (theArea) {
            long theType = theArea->mAreaType;
            theLocation.mSerial = inAreaSerial;
            theLocation.mType = theType;
        } else {
            theLocation.mType = kAIAreaNone;
            theLocation.mSerial = inAreaSerial;
        }
        theTask.mLocation = theLocation;
        theTask.mMode = kAGModeCombat;
    }
    this->mAttackTasks.push_back(theTask);
}

// FUNCTION: EXE 0x47203a
// Queues a defend task on a unit (a location task for an own unit) or, without one, on an area against a random
// enemy
void EECPCombatManager::AddDefendTask(unsigned long inUnitID, unsigned long inAreaSerial)
{
    EEComplexUnit* theUnit = dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(inUnitID));
    EEAIArea* theArea = gContinentManager.GetArea(inAreaSerial);
    EECPAGTask theTask;
    EECPILocation theLocation;
    if (inUnitID != 0 && inUnitID != -1) {
        if (theUnit) {
            theTask.mPlayer = gWorld.mPlayers[theUnit->mPlayer->mPlayerID];
            if (theUnit->mPlayer == this->mPlayer)
                theTask.mType = kAGTaskLocation;
        }
        theTask.SetUnit(theUnit);
        theTask.mMode = kAGModeCombat;
        if (theTask.mType == kAGTaskNone)
            theTask.mType = kAGTaskUnit;
    } else {
        if (inAreaSerial == 0)
            return;
        EEPlayer* thePlayer = this->GetIntelligence()->GetRandomEnemy(this->mPlayer);
        if (thePlayer)
            theTask.mPlayer = gWorld.mPlayers[thePlayer->mPlayerID];
        theTask.mType = kAGTaskLocation;
        if (theArea) {
            long theType = theArea->mAreaType;
            theLocation.mSerial = inAreaSerial;
            theLocation.mType = theType;
        } else {
            theLocation.mType = kAIAreaNone;
            theLocation.mSerial = inAreaSerial;
        }
        theTask.mLocation = theLocation;
        theTask.mMode = kAGModeCombat;
    }
    this->mDefendTasks.push_back(theTask);
}

// FUNCTION: EXE 0x47215c
// Offers a new unit that is not a citizen or building to the attack commander; returns true if a group took it
bool EECPCombatManager::OfferUnit(EEComplexUnit* inOld, EEComplexUnit* inUnit)
{
    bool theResult = false;
    EEUnitClass* theClass = inUnit->mUnitClass;
    if (theClass->mCategory != kUnitCategoryLandCitizen && theClass->mCategory != kUnitCategoryNavalCitizen && !theClass->IsLandBuilding())
        theResult = this->mAttackCommander->Offer(inOld, inUnit);
    return theResult;
}

// the cannon's targets, strongest first (sort COMDAT 0x76710: _Median 0x76db5 compares +0x3c, mHitPoints; the
// predicate is an empty by-value struct) (provisional name)
struct EECPHitPointsGreater {
    bool operator()(EEUnit* inFirst, EEUnit* inSecond) const { return inFirst->mHitPoints > inSecond->mHitPoints; }
};

// FUNCTION: EXE 0x47219a
// When an attack is due, picks an enemy (biggest front overlap, nearest, weakest gatherer of a needed resource or
// most damage done) for a new attack group; after being attacked, targets the leading wonder racer's nearest wonder
// NEAR: the original calls GetRandom, begin()+i and the U2DPoint copy out of line
void EECPCombatManager::Update(unsigned long inTime)
{
    std::vector<EEComplexUnit*> theUnits;
    std::vector<EEPlayer*> theHumans;
    std::vector<EEPlayer*> theComputers;
    std::vector<EECPIntelligencePlayerFile*> theFiles;
    EEAIAStarBounds theOverlap;
    EEAIAStarBounds theBounds;
    EECPILocation theLocation;
    EECPIntelligencePlayerFile* theLargest = NULL;
    EECPIntelligencePlayerFile* theWeakest = NULL;
    EEPlayer* theTarget = NULL;
    EECPAGTask theTask;
    float theChance = this->mPlayer->mComputerAggression * 0.01f;
    long theLargestArea = 0;
    long theDistance = 0;
    long theWeakCount = 0;
    long theAmountUsed = 0;
    unsigned long theMostAid = 0;
    bool theUseHumans = false;
    EETileLocation theHome(this->mPlayer->mCivilizationManager->mBuildManager->mTownCenterTile.mX,
        this->mPlayer->mCivilizationManager->mBuildManager->mTownCenterTile.mY);
    if (theHome.mX == -10000 || theHome.mY == -10000 || (!this->mPlayer->IsAttackDue(inTime) && !this->mWasAttacked))
        return;
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
        EEPlayer* thePlayer = gWorld.mPlayers[i];
        if (!thePlayer || thePlayer == this->mPlayer || !this->mPlayer->mEnemies[thePlayer->mPlayerID] || thePlayer->mObserverMode)
            continue;
        if (thePlayer->IsHuman())
            theHumans.push_back(thePlayer);
        else
            theComputers.push_back(thePlayer);
    }
    unsigned long theCount;
    if (theHumans.size() > 0 && theComputers.size() > 0 && gPhysics->GetRandom() < theChance) {
        theCount = theComputers.size();
    } else if (theHumans.size() > 0) {
        theUseHumans = true;
        theCount = theHumans.size();
    } else {
        theCount = theComputers.size();
    }
    for (unsigned long j = 0; j < theCount; j++) {
        EEPlayer* thePlayer = *((theUseHumans ? theHumans : theComputers).begin() + j);
        EECPIntelligencePlayerFile* theFile = this->mIntelligence->GetFile(thePlayer);
        theFile->Update(this->mPlayer, true, false);
        theFiles.push_back(theFile);
    }
    if (theFiles.size() > 0) {
        if (this->mWasAttacked) {
            this->mIntelligence->UpdateFile(this->mPlayer, this->mPlayer);
            EECPIntelligencePlayerFile* theBest = NULL;
            for (unsigned long k1 = 0; k1 < theFiles.size(); k1++) {
                EECPIntelligencePlayerFile* theFile = *(theFiles.begin() + k1);
                if (!theBest || theFile->mVictoryPercent > theBest->mVictoryPercent)
                    theBest = theFile;
            }
            EEComplexUnit* theWonder = NULL;
            if (theBest && theBest->mVictoryPercent > 0) {
                theTask.mPlayer = theBest->mPlayer;
                unsigned long theTeam = theBest->mPlayer->GetTeamIndex();
                for (unsigned long m = 1; m < gWorld.mNumPlayers; m++) {
                    EEPlayer* thePlayer = gWorld.mPlayers[m];
                    if (thePlayer->mObserverMode || thePlayer->GetTeamIndex() != theTeam)
                        continue;
                    for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
                        EEComplexUnit* theComplex = theUnit->mComplexUnit;
                        if (!theComplex || !this->mPlayer->mTechnologies->Get(theUnit->GetUnitClass()->mTechnology)
                            || this->mPlayer->mTechnologies->Get(theUnit->GetUnitClass()->mTechnology)->mData->mKind != 4)
                            continue;
                        if (!theWonder || theWonder->DistanceSquaredToTileXY(theHome.mX, theHome.mY)
                            > theComplex->DistanceSquaredToTileXY(theHome.mX, theHome.mY))
                            theWonder = theComplex;
                    }
                }
                theTask.SetUnit(theWonder);
                theLocation.mTile.mX = theWonder->mTileX;
                theLocation.mTile.mY = theWonder->mTileY;
                theTask.mLocation = theLocation;
                theTask.mType = kAGTaskUnit;
                theTask.mMode = kAGModeWonder;
                theTask.mIsActive = true;
                this->mAttackCommander->CreateGroup(theTask, 0);
            }
            this->mWasAttacked = false;
            if (theWonder) {
                std::vector<EEUnit*> theCannons;
                EEGetPlayerUnitsOfType(kUnitTypeCpICBM, this->mPlayer->mPlayerID, theCannons);
                if (theCannons.size() > 0) {
                    EEComplexUnit* theCannon = (*(theCannons.begin() + 0))->mComplexUnit;
                    if (theCannon->mContainer) {
                        theCannon->mContainer->SetGoal(new EEUGAttackUnit(theCannon->mContainer, theWonder, true, false),
                            0, 0);
                        theCannon->mContainer->StartAction(kActionInitialAttackState);
                        theCannon->mContainer->GetUnitClass()->ActionChanged(theCannon->mContainer);
                    }
                }
            }
        } else {
            this->mIntelligence->UpdateFile(this->mPlayer, this->mPlayer);
            EECPIntelligencePlayerFile* theOwn = this->mIntelligence->GetFile(this->mPlayer);
            for (unsigned long k2 = 0; k2 < theFiles.size(); k2++) {
                EECPIntelligencePlayerFile* theFile = *(theFiles.begin() + k2);
                theBounds = theFile->mBounds;
                if (!theOwn->mBounds.Intersect(theBounds, theOverlap))
                    continue;
                theUnits.clear();
                std::less<EEComplexUnit*> theLess;
                gWorld.mMap->mUnitRegions->CollectValues(theOverlap, theUnits, theLess);
                unsigned long theOurs = 0;
                unsigned long theTheirs = 0;
                for (unsigned long n = 0; n < theUnits.size(); n++) {
                    EEPlayer* theOwner = (*(theUnits.begin() + n))->mPlayer;
                    if (theOwner == theFile->mPlayer)
                        theTheirs++;
                    else if (theOwner == this->mPlayer)
                        theOurs++;
                }
                if (theTheirs <= 0 || theOurs <= 0)
                    continue;
                long theArea = (theBounds.mRight - theBounds.mLeft) * (theBounds.mBottom - theBounds.mTop);
                if (!theLargest || theArea > theLargestArea) {
                    theLargest = theFile;
                    theLargestArea = theArea;
                }
            }
            if (!theLargest) {
                for (unsigned long k3 = 0; k3 < theFiles.size(); k3++) {
                    EECPIntelligencePlayerFile* theFile = theFiles[k3];
                    EETileLocation theOwnCenter;
                    theOwn->mBounds.GetCenter(theOwnCenter);
                    EETileLocation theCenter;
                    theFile->mBounds.GetCenter(theCenter);
                    long theSquared = theOwnCenter.DistanceToSquared(theCenter);
                    if (!theLargest || theSquared < theDistance) {
                        theLargest = theFile;
                        theDistance = theSquared;
                    }
                }
            }
            for (long r = 2; r < 7; r++) {
                long theAmount = this->mPlayer->mCivilizationManager->mResourceManager->mNeeds[r];
                if (theAmount <= 0 || theOwn->mResources[r] != 0)
                    continue;
                for (unsigned long k4 = 0; k4 < theFiles.size(); k4++) {
                    EECPIntelligencePlayerFile* theFile = theFiles[k4];
                    long theGatherers = theFile->mResources[r];
                    if (theGatherers <= 0
                        || (theWeakest && (theGatherers >= theWeakCount || theAmount <= theAmountUsed)))
                        continue;
                    theAmountUsed = theAmount;
                    theWeakCount = theGatherers;
                    theWeakest = theFile;
                }
            }
            EECPIntelligencePlayerFile* theAided = NULL;
            for (unsigned long k5 = 0; k5 < theFiles.size(); k5++) {
                EECPIntelligencePlayerFile* theFile = theFiles[k5];
                unsigned long theAid = this->mPlayer->mDamageFrom[theFile->mPlayer->mPlayerID];
                if (theAid > 0 && (!theAided || theAid > theMostAid)) {
                    theAided = theFile;
                    theMostAid = theAid;
                }
            }
            long theFirst = this->mPlayer->mAttackEnemyChance;
            long theSecond = this->mPlayer->mAttackToClaimResourceChance + theFirst;
            float theRoll = gPhysics->mRandom.Generate();
            EECPIntelligencePlayerFile* theChoice = NULL;
            if (theWeakest) {
                if (!theLargest)
                    theChoice = theWeakest;
                else if (!theAided)
                    theChoice = theRoll > theFirst ? theWeakest : theLargest;
                else if (theRoll > theFirst)
                    theChoice = theRoll > theSecond ? theAided : theWeakest;
                else
                    theChoice = theLargest;
            } else if (theLargest) {
                theChoice = theAided && theRoll > theSecond ? theAided : theLargest;
            } else {
                theChoice = theAided;
            }
            if (theChoice)
                theTarget = theChoice->mPlayer;
            theTask.mPlayer = theTarget;
            this->mAttackCommander->CreateGroup(theTask, 0);
            if (gPhysics->mRandom.Generate() <= kCMCannonChance[this->mPlayer->mDifficulty]) {
                std::vector<EEUnit*> theCannons;
                EEGetPlayerUnitsOfType(kUnitTypeCpICBM, this->mPlayer->mPlayerID, theCannons);
                if (theCannons.size() > 0 && theCannons[0]->mComplexUnit->mContainer) {
                    EEComplexUnit* theCannon = theCannons[0]->mComplexUnit->mContainer;
                    std::vector<EEUnit*> theTargets;
                    for (EEUnit* theUnit = theTarget->mFirstUnit; theUnit; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
                        if (theUnit->mStats->mMaxHitPoints >= 1500 && theUnit->mTile
                            && static_cast<EETile*>(theUnit->mTile)->IsSeenBy(this->mPlayer))
                            theTargets.push_back(theUnit);
                    }
                    if (theTargets.size() > 0) {
                        std::sort(theTargets.begin(), theTargets.end(), EECPHitPointsGreater());
                        EEUnit* theVictim = theTargets[0];
                        theCannon->SetGoal(new EEUGAttackUnit(theCannon, theVictim, true, false), 0, 0);
                        theCannon->StartAction(kActionInitialAttackState);
                        theCannon->GetUnitClass()->ActionChanged(theCannon);
                    }
                }
            }
        }
    }
    this->mPlayer->SetNextAttackTime(inTime);
}

// FUNCTION: EXE 0x472c03
// Deletes every attack group
void EECPCombatManager::DeleteGroups()
{
    this->mAttackCommander->DeleteGroups();
}

// FUNCTION: EXE 0x472c0b
// Passes a target-player notice to the attack and defense commanders
void EECPCombatManager::TargetPlayerNotice(EEPlayer* inPlayer, bool inFlag)
{
    this->mAttackCommander->TargetPlayerNotice(inPlayer, inFlag);
    this->mDefenseCommander->TargetPlayerNotice(inPlayer, inFlag);
}

// FUNCTION: EXE 0x472c32
// Counts threats per influence region (5 for a unit heading somewhere, else 1); past 20 in one region, sends groups
// to the unit's tile and forgets the region
void EECPCombatManager::NoteThreat(EEComplexUnit* inUnit, bool inUseTarget)
{
    EETileLocation theKey;
    GE3DPoint theTarget;
    long theCount = 1;
    if (!inUnit || !gWorld.mInfluenceTable || gLoadingSavedGame || gWorldClearing || !this->mTracksThreats || this->mPlayer->mObserverMode
        || inUnit->mTileX == -10000)
        return;
    if (inUseTarget && inUnit->mActiveGoal) {
        inUnit->mActiveGoal->GetGoalLocation(theTarget);
        if (theTarget.mX > 0.0f || theTarget.GetY() > 0.0f) {
            long theY = UMFloor(theTarget.GetY());
            long theX = UMFloor(theTarget.mX);
            theCount = 5;
            theKey.mX = theX / gWorld.mInfluenceTable->mRegionSize;
            theKey.mY = theY / gWorld.mInfluenceTable->mRegionSize;
        } else {
            theKey.mX = inUnit->mTileX / gWorld.mInfluenceTable->mRegionSize;
            theKey.mY = inUnit->mTileY / gWorld.mInfluenceTable->mRegionSize;
        }
    } else {
        long theY = inUnit->mTileY / gWorld.mInfluenceTable->mRegionSize;
        long theX = inUnit->mTileX / gWorld.mInfluenceTable->mRegionSize;
        theKey = EETileLocation(theX, theY);
    }
    std::map<EETileLocation, long>::iterator it = this->mTileCounts.find(theKey);
    if (it == this->mTileCounts.end()) {
        this->mTileCounts.insert(std::make_pair(theKey, theCount));
    } else {
        it->second += theCount;
        if (it->second > 20) {
            EETileLocation theTile(inUnit->mTileX, inUnit->mTileY);
            this->Attack(this->mPlayer, theTile, false, false, true, true, true, true);
            this->mTileCounts.erase(it);
        }
    }
}

// FUNCTION: EXE 0x472e2f
// Sends air, sea and space groups to cover a land sub group's unload tile (two attacks for an air transport);
// always returns false
bool EECPCombatManager::CoverLanding(EECPAttackSubGroup* inGroup)
{
    EECPASGLand* theLand = dynamic_cast<EECPASGLand*>(inGroup);
    if (!theLand || !gWorld.mMap->GetTileAtLocation(theLand->mUnloadLocation.mTile))
        return false;
    EECPASGTransport* theTransport = theLand->mTransport;
    if (dynamic_cast<EECPASGAirTransport*>(theTransport)) {
        this->Attack(this->mPlayer, theLand->mUnloadLocation.mTile, false, false, false, true, false, true);
        this->Attack(this->mPlayer, theLand->mUnloadLocation.mTile, false, false, false, false, true, true);
    } else {
        this->Attack(this->mPlayer, theLand->mUnloadLocation.mTile, false, false, false, true, true, true);
    }
    return false;
}

// FUNCTION: EXE 0x472ec3
// Attacks an enemy player's most valuable unit, taunts and plays the attack sound, and halves the aid timers
void EECPCombatManager::AttackPlayer(EEPlayer* inPlayer)
{
    std::map<long, EECPMilitaryAidPackage*>::iterator it;
    if (!inPlayer || inPlayer->mObserverMode || this->mPlayer->mObserverMode || inPlayer == this->mPlayer
        || !this->mPlayer->mEnemies[inPlayer->mPlayerID])
        return;
    EEComplexUnit* theUnit = this->ChooseTargetUnit(inPlayer);
    if (!theUnit || theUnit->mPlayer->mObserverMode)
        return;
    this->AttackUnit(theUnit);
    this->mPlayer->mInteractionManager->SendChat(IDS_CHAT_FACE_ME_COWARD, 0x3a98);
    EESoundObject* theSound = EESoundObject::Get(kSoundComputerAttack);
    if (theSound)
        theSound->Play(Fn102F3D(0.45f, 1.0f), Fn102F3D(0.1f, 1.0f), Fn102F3D(0.8f, kCMSoundPitch));
    for (it = this->mDefenseCommander->mPackages.begin(); it != this->mDefenseCommander->mPackages.end(); ++it)
        if (it->second)
            it->second->mNextSendTime >>= 1;
    this->mDefenseCommander->mNextAidTime >>= 1;
}

// FUNCTION: EXE 0x472ff5
// Sends groups to defend a player's most valuable unit and halves the aid timers
void EECPCombatManager::DefendPlayer(EEPlayer* inPlayer)
{
    std::map<long, EECPMilitaryAidPackage*>::iterator it;
    if (!inPlayer || inPlayer->mObserverMode)
        return;
    EEComplexUnit* theUnit = this->ChooseTargetUnit(inPlayer);
    if (!theUnit)
        return;
    this->DefendUnit(theUnit);
    for (it = this->mDefenseCommander->mPackages.begin(); it != this->mDefenseCommander->mPackages.end(); ++it)
        if (it->second)
            it->second->mNextSendTime >>= 1;
    this->mDefenseCommander->mNextAidTime >>= 1;
}

// FUNCTION: EXE 0x47305d
// Collects the attack commander's defending units into inUnits
void EECPCombatManager::CollectDefendingUnits(std::vector<EEComplexUnit*>& inUnits)
{
    this->mAttackCommander->CollectDefendingUnits(inUnits);
}

// FUNCTION: EXE 0x47306c
// Picks a random defend task on the continent (a unit on it, or an area with wanted tiles there, weighted by tile)
// into outTask, aimed at the unit's tile; returns true if that tile has a continent
bool EECPCombatManager::ChooseTask(EECPAGTask& outTask, bool inLand, bool inWater, bool inSpace, unsigned short inContinent)
{
    std::vector<long> theIndices;
    EECPILocation theLocation;
    EEMap* theMap = gWorld.mMap;
    bool theResult = false;
    this->DropLostTasks();
    if (inContinent != 0) {
        for (long i = this->mDefendTasks.size() - 1; i >= 0; i--) {
            EEAIArea* theArea;
            if (this->mDefendTasks[i].mLocation.mSerial)
                theArea = gContinentManager.GetArea(this->mDefendTasks[i].mLocation.mSerial);
            else
                theArea = NULL;
            EEComplexUnit* theUnit = this->mDefendTasks[i].mUnit;
            if (theUnit && theUnit->GetHitPoints() > 0) {
                EEAIContinent* theContinent = gContinentManager.GetContinentAt(theUnit);
                if (theContinent && theContinent->mID == inContinent)
                    theIndices.push_back(i);
            } else if (theArea) {
                EEAIRectangleArea* theRectangle = dynamic_cast<EEAIRectangleArea*>(theArea);
                if (theRectangle) {
                    long x;
                    long y;
                    if (inLand && theRectangle->mNumLandTiles > 0) {
                        if (inContinent == theRectangle->mContinentID) {
                            theIndices.push_back(i);
                        } else {
                            for (x = theRectangle->mRect.mLeft; x <= theRectangle->mRect.mRight; x++) {
                                for (y = theRectangle->mRect.mTop; y <= theRectangle->mRect.mBottom; y++) {
                                    if (!theMap->GetTile(x, y))
                                        continue;
                                    EEAIContinent* theContinent = gContinentManager.GetContinentAt(x, y);
                                    if (theContinent && theContinent->mID == inContinent)
                                        theIndices.push_back(i);
                                }
                            }
                        }
                    }
                    if (inWater && theRectangle->mNumWaterTiles > 0) {
                        if (inContinent == theRectangle->mContinentID) {
                            theIndices.push_back(i);
                        } else {
                            for (x = theRectangle->mRect.mLeft; x <= theRectangle->mRect.mRight; x++) {
                                for (y = theRectangle->mRect.mTop; y <= theRectangle->mRect.mBottom; y++) {
                                    if (theMap->GetTile(x, y)) {
                                        EEAIContinent* theContinent = gContinentManager.GetContinentAt(x, y);
                                        if (theContinent && theContinent->mID == inContinent)
                                            theIndices.push_back(i);
                                    }
                                }
                            }
                        }
                    }
                    if (inSpace && theRectangle->mNumSpaceTiles > 0) {
                        if (inContinent == theRectangle->mContinentID) {
                            theIndices.push_back(i);
                        } else {
                            for (x = theRectangle->mRect.mLeft; x <= theRectangle->mRect.mRight; x++) {
                                for (y = theRectangle->mRect.mTop; y <= theRectangle->mRect.mBottom; y++) {
                                    if (theMap->GetTile(x, y)) {
                                        EEAIContinent* theContinent = gContinentManager.GetContinentAt(x, y);
                                        if (theContinent && theContinent->mID == inContinent)
                                            theIndices.push_back(i);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        if (theIndices.size() > 0) {
            long theIndex = gPhysics->GetRandom(0L, theIndices.size() - 1);
            outTask = this->mDefendTasks[theIndices[theIndex]];
            theLocation = outTask.mLocation;
            if (outTask.mUnit) {
                EETileLocation theTile(outTask.mUnit->mTileX, outTask.mUnit->mTileY);
                theLocation = EECPILocation(theTile);
            } else {
                theLocation.IsValid();
            }
            outTask.mLocation = theLocation;
            theResult = gContinentManager.GetContinentAt(outTask.mLocation.mTile.mX, outTask.mLocation.mTile.mY) != NULL;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x47342e
// Returns how many more military units fit in the military share of the population limit (95% in a wonder race)
long EECPCombatManager::GetUnitsAvailable()
{
    EECPUnitManager* theUnitManager = this->mPlayer->mCivilizationManager->mUnitManager;
    long theResult = 0;
    long theCount = this->mPlayer->GetPopulationLimit(0);
    long theUnits = theUnitManager->CountMilitaryUnits(true, true);
    if (theCount > 0) {
        float theRatio = (float)theUnits / (float)theCount;
        float theFactor;
        if (!this->mIsWonderRace)
            theFactor = 1.0f - this->mPlayer->GetCitizenPercentage();
        else
            theFactor = kCMFactor;
        theResult = UMCeil((theFactor - theRatio) * theCount);
    }
    return theResult;
}

// FUNCTION: EXE 0x4734af
// Returns the military share of the population limit (95% in a wonder race)
long EECPCombatManager::GetUnitLimit()
{
    long theResult = 0;
    long theCount = this->mPlayer->GetPopulationLimit(0);
    if (theCount > 0) {
        float theFactor;
        if (!this->mIsWonderRace)
            theFactor = 1.0f - this->mPlayer->GetCitizenPercentage();
        else
            theFactor = kCMFactor;
        theResult = (long)(theCount * theFactor);
    }
    return theResult;
}

// FUNCTION: EXE 0x4734fa
// Deactivates every sub group task aimed at the given area
void EECPCombatManager::ForgetTarget(unsigned long inSerial)
{
    if (inSerial) {
        for (long i = this->mAttackCommander->mGroups.size() - 1; i >= 0; i--) {
            EECPAttackGroup* theGroup = this->mAttackCommander->GetGroup(i);
            if (theGroup) {
                EE_LOST_DEBUG(9);   // LOST-DEBUG: none: exact only (the original keeps GetGroup inline, its operator[] out of line)
                for (long j = theGroup->GetNumSubGroups() - 1; j >= 0; j--) {
                    EECPAttackSubGroup* theSubGroup = theGroup->mSubGroups[j];
                    if (theSubGroup && theSubGroup->mTask.mLocation.mSerial == inSerial)
                        theSubGroup->mTask.mIsActive = false;
                }
            }
        }
    }
}

// FUNCTION: EXE 0x47355b
// Deactivates every sub group task aimed at the given unit
void EECPCombatManager::ForgetTargetUnit(unsigned long inUnitID)
{
    if (inUnitID != -1) {
        for (long i = this->mAttackCommander->mGroups.size() - 1; i >= 0; i--) {
            EECPAttackGroup* theGroup = this->mAttackCommander->GetGroup(i);
            if (theGroup) {
                for (long j = theGroup->GetNumSubGroups() - 1; j >= 0; j--) {
                    EECPAttackSubGroup* theSubGroup = theGroup->GetSubGroupAt(j);
                    EE_LOST_DEBUG(9);   // LOST-DEBUG: none: exact only (the original calls begin() + j operator+ out of line)
                    if (theSubGroup && theSubGroup->mTask.mUnit && theSubGroup->mTask.mUnit->mUnitID == inUnitID)
                        theSubGroup->mTask.mIsActive = false;
                }
            }
        }
    }
}

// FUNCTION: EXE 0x4735e2
// Returns true when the attack commander keeps the land unit (never while the world is clearing)
bool EECPCombatManager::KeepsUnit(EEComplexUnit* inUnit)
{
    bool theResult = false;
    if (!gWorldClearing && (inUnit->mUnitClass->mDomains & kMovementLand))
        theResult = this->mAttackCommander->KeepsUnit(inUnit);
    return theResult;
}

// FUNCTION: EXE 0x47360a
// Returns true when the strategy or list is not empty and the task's area or unit is still a valid target (hostile,
// or allied for a task on an own location)
bool EECPCombatManager::IsValidTask(EECPAGTask& inTask, EECPCombatStrategy& inStrategy, std::vector<long>& inList)
{
    long theUnitID = -1;
    bool theIsOwn = inTask.mType == kAGTaskLocation;
    bool theResult = false;
    if (inStrategy.GetNumRequirements() > 0 || inList.size() > 0) {
        if (inTask.mUnit)
            theUnitID = inTask.mUnit->mUnitID;
        unsigned long theSerial = inTask.mLocation.mSerial;
        theResult = this->IsValidTarget(theSerial, theUnitID, theIsOwn == false);
    }
    return theResult;
}

// FUNCTION: EXE 0x473684
// Returns true when the area exists (a town only if hostile or allied as inEnemy asks) or, without one, when the
// unit is hostile or allied as asked (allied includes nature)
bool EECPCombatManager::IsValidTarget(unsigned long inSerial, unsigned long inUnitID, bool inEnemy)
{
    EEAIArea* theArea = gContinentManager.GetArea(inSerial);
    EETown* theTown = dynamic_cast<EETown*>(theArea);
    EEComplexUnit* theUnit;
    theUnit = inUnitID != -1 ? dynamic_cast<EEComplexUnit*>(gWorld.GetUnit(inUnitID)) : NULL;
    bool theResult = false;
    if (theArea) {
        theResult = true;
        if (theTown) {
            if (!inEnemy)
                theResult = this->mPlayer->IsAlly(theTown->mPlayer);
            else
                theResult = this->mPlayer->IsAlly(theTown->mPlayer) == false;
        }
    } else if (theUnit) {
        if (inEnemy)
            theResult = this->mPlayer->IsAlly(theUnit->mPlayer) == false;
        else
            theResult = this->mPlayer->IsAlly(theUnit->mPlayer)
                || theUnit->mPlayer->mPlayerID == 0;
    }
    return theResult;
}

// FUNCTION: EXE 0x47377e
// Returns true for a live, AI-controlled unit of a military class that can move
bool EECPCombatManager::IsMilitaryUnit(EEComplexUnit* inUnit)
{
    if (!inUnit->mIsDetachedFromAI && this->IsMilitaryClass(inUnit->mUnitClass) && inUnit->mStats && inUnit->mHitPoints > 0 && !inUnit->mPreviousPart
        && inUnit->GetMaxVelocity() > 0.0f)
        return true;
    return false;
}

// FUNCTION: EXE 0x4737c9
// Returns true for a mobile military class (not a citizen, building, projectile or naval mine)
bool EECPCombatManager::IsMilitaryClass(EEUnitClass* inClass)
{
    bool theResult = false;
    if (inClass && inClass->mMaxVelocity > 0.0f && inClass->mCategory != kUnitCategoryLandCitizen && inClass->mCategory != kUnitCategoryNavalCitizen && !inClass->mIsProjectile) {
        switch (inClass->mCategory) {
        case kUnitCategoryAmbient:
        case kUnitCategoryResource:
        case kUnitCategoryBuilding:
        case kUnitCategoryLandCitizen:
        case kUnitCategoryProjectile:
        case kUnitCategoryBomb:
        case kUnitCategoryMissile:
        case kUnitCategoryBuilding19:
        case kUnitCategoryTorpedo:
        case kUnitCategoryNavalCitizen:
        case kUnitCategoryMiningBuilding:
        case kUnitCategoryGate:
        case kUnitCategoryFortress:
        case kUnitCategoryBuildingPriest:
            theResult = false;
            break;
        case kUnitCategoryAirplane:
        case kUnitCategoryNaval:
        case kUnitCategoryLand:
        case kUnitCategoryHelicopter:
        case kUnitCategoryLand8:
        case kUnitCategorySubmergedNaval:
        case kUnitCategoryLandPriest:
        case kUnitCategoryNavalPriest:
        case kUnitCategoryLand15:
        case kUnitCategoryLandTransport:
        case kUnitCategoryHelicopterTransport:
        case kUnitCategoryNavalTransport:
        case kUnitCategoryNavalBuilding:
        case kUnitCategoryLandMilitaryHero:
        case kUnitCategoryHelicopterPriest:
        case kUnitCategorySeaAnimal:
        case kUnitCategoryAirAnimal:
        case kUnitCategorySpaceship:
        case kUnitCategorySpaceBuilding:
        case kUnitCategorySpaceTransport:
        case kUnitCategoryPlanetSpaceship:
            theResult = inClass->mUnitType != kUnitTypeS10NavalMine;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x47384c
// Reads the commanders, group ids, defend and attack tasks, wonder-race state and threat counts from a saved game
// (older versions stop earlier); returns the first read error
long EECPCombatManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult;
    long theCount;
    long i;
    if (inMajorVersion == 0 && inMinorVersion < 0xde) {
        theResult = this->mPersonnel->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult != 0)
            return theResult;
        theResult = this->mAttackCommander->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult != 0)
            return theResult;
        theResult = this->mDefenseCommander->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult != 0)
            return theResult;
        theResult = inFile.Read(this->mNextGroupID);
        if (theResult != 0)
            return theResult;
        theResult = inFile.Read(this->mNextSubGroupID);
        if (theResult != 0)
            return theResult;
        theResult = inFile.Read(this->mWasAttacked);
        if (theResult != 0)
            return theResult;
        theResult = inFile.Read(this->mImprovementsEnabled);
        if (theResult != 0)
            return theResult;
        theResult = inFile.Read(theCount);
        this->mDefendTasks.resize(theCount);
        for (i = theCount - 1; i >= 0; i--)
            if (theResult == 0)
                theResult = this->mDefendTasks[i].Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult != 0)
            return theResult;
        theResult = inFile.Read(theCount);
        this->mAttackTasks.resize(theCount);
        for (i = theCount - 1; i >= 0; i--)
            if (theResult == 0)
                theResult = this->mAttackTasks[i].Load(inMajorVersion, inMinorVersion, inFile);
        return theResult;
    }
    if (inMajorVersion == 0 && inMinorVersion < 0xe4) {
        theResult = this->mPersonnel->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult != 0)
            return theResult;
        theResult = this->mAttackCommander->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult != 0)
            return theResult;
        theResult = this->mDefenseCommander->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult != 0)
            return theResult;
        theResult = inFile.Read(this->mNextGroupID);
        if (theResult != 0)
            return theResult;
        theResult = inFile.Read(this->mNextSubGroupID);
        if (theResult != 0)
            return theResult;
        theResult = inFile.Read(this->mWasAttacked);
        if (theResult != 0)
            return theResult;
        theResult = inFile.Read(this->mImprovementsEnabled);
        if (theResult != 0)
            return theResult;
        theResult = inFile.Read(theCount);
        this->mDefendTasks.resize(theCount);
        for (i = theCount - 1; i >= 0; i--)
            if (theResult == 0)
                theResult = this->mDefendTasks[i].Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult != 0)
            return theResult;
        theResult = inFile.Read(theCount);
        this->mAttackTasks.resize(theCount);
        for (i = theCount - 1; i >= 0; i--)
            if (theResult == 0)
                theResult = this->mAttackTasks[i].Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult != 0)
            return theResult;
        theResult = inFile.Read(this->mIsWonderRace);
        if (theResult != 0)
            return theResult;
        theCount = 0;
        theResult = inFile.Read(theCount);
        this->mWonderAttackTimes.resize(theCount, 0);
        for (i = 0; i < theCount; i++) {
            if (theResult != 0)
                return theResult;
            theResult = inFile.Read(this->mWonderAttackTimes[i]);
        }
        return theResult;
    }
    theResult = this->mPersonnel->Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == 0 && (theResult = this->mAttackCommander->Load(inMajorVersion, inMinorVersion, inFile)) == 0
        && (theResult = this->mDefenseCommander->Load(inMajorVersion, inMinorVersion, inFile)) == 0
        && (theResult = inFile.Read(this->mNextGroupID)) == 0 && (theResult = inFile.Read(this->mNextSubGroupID)) == 0
        && (theResult = inFile.Read(this->mWasAttacked)) == 0 && (theResult = inFile.Read(this->mImprovementsEnabled)) == 0) {
        theResult = inFile.Read(theCount);
        this->mDefendTasks.resize(theCount);
        for (i = theCount - 1; i >= 0; i--)
            if (theResult == 0)
                theResult = this->mDefendTasks[i].Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == 0) {
            theResult = inFile.Read(theCount);
            this->mAttackTasks.resize(theCount);
            for (i = theCount - 1; i >= 0; i--)
                if (theResult == 0)
                    theResult = this->mAttackTasks[i].Load(inMajorVersion, inMinorVersion, inFile);
            if (theResult == 0 && (theResult = inFile.Read(this->mIsWonderRace)) == 0) {
                theCount = 0;
                theResult = inFile.Read(theCount);
                this->mWonderAttackTimes.resize(theCount, 0);
                for (i = 0; i < theCount; i++) {
                    if (theResult != 0)
                        return theResult;
                    theResult = inFile.Read(this->mWonderAttackTimes[i]);
                }
                if (theResult == 0) {
                    theCount = 0;
                    theResult = inFile.Read(theCount);
                    this->mTileCounts.clear();
                    for (i = 0; i < theCount; i++) {
                        if (theResult != 0)
                            return theResult;
                        EETileLocation theKey;
                        theResult = theKey.Load(inFile);
                        if (theResult != 0)
                            continue;
                        long theValue = 0;
                        theResult = inFile.Read(theValue);
                        if (theResult == 0)
                            this->mTileCounts[theKey] = theValue;
                    }
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x473dc1
// On reaching the WW1 epoch in a game started earlier, sends an attack group at a random enemy, plus 1-4 cruisers
// when gDCGameSpeed >= 250 and cruisers are available
void EECPCombatManager::EpochReached(long inEpoch)
{
    EECPCombatStrategy theStrategy;
    EECPUnitRequirement theRequirement;
    EECPAGTask theTask;
    std::vector<long> theUnitIDs;
    if (inEpoch == kTechEpochAtomicWW1 && !gLoadingSavedGame && gGameScreenActive && this->mPlayer->GetTechTree() && this->mPlayer->GetTechTree()->mStartEpoch < kTechEpochAtomicWW1) {
        EEPlayer* theEnemy = this->mIntelligence->GetRandomEnemy(this->mPlayer);
        if (theEnemy) {
            theTask.mPlayer = gWorld.mPlayers[theEnemy->mPlayerID];
            theStrategy.Reset();
            theStrategy.mAddFavorites = true;
            theStrategy.mKind = 1;
            theRequirement.Clear();
            theRequirement.mKind = kRequirementSpecial;
            theStrategy.mRequirements.push_back(theRequirement);
            theRequirement.Clear();
            theRequirement.mKind = kRequirementCounter;
            theStrategy.mRequirements.push_back(theRequirement);
            this->mAttackCommander->CreateGroupFromStrategy(theTask, theStrategy, theUnitIDs);
            if (gDCGameSpeed >= 250) {
                EEUnitClass* theClass = gUnitClassTable[kUnitTypeS10Cruiser];
                if (theClass && this->mPlayer->GetTechTree()->Get(theClass->GetTechnology())
                    && this->mPlayer->GetTechTree()->Get(theClass->GetTechnology())->GetIsAvailable()) {
                    theStrategy.Reset();
                    theStrategy.mAddFavorites = false;
                    long theCount = gPhysics->GetRandom(1L, 4L);
                    theStrategy.mNumUnits = theCount;
                    theStrategy.mNumRemaining = theCount;
                    theStrategy.mKind = 1;
                    theRequirement.Clear();
                    theRequirement.mKind = kRequirementAlreadyDefined;
                    theRequirement.mUnitClass = 0xa3;
                    theRequirement.mL04 = 1;
                    theRequirement.mL08 = 4;
                    theStrategy.mRequirements.push_back(theRequirement);
                    this->mAttackCommander->CreateGroupFromStrategy(theTask, theStrategy, theUnitIDs);
                }
            }
        }
    }
}

// FUNCTION: EXE 0x473fc6
// Returns the player's most valuable non-wonder unit (influence, fixed buildings first, plus its tile's influence),
// or NULL
// multimap with greater<long> (insert 0x3fcf6 compares with setg): begin() is the HIGHEST weight
EEComplexUnit* EECPCombatManager::ChooseTargetUnit(EEPlayer* inPlayer)
{
    std::multimap<long, EEComplexUnit*, std::greater<long> > theUnits;
    EEInfluenceTable* theTable = gWorld.mInfluenceTable;
    EEComplexUnit* theResult = NULL;
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (!theUnit->GetComplexUnit() || !inPlayer->GetTechTree()->Get(theUnit->GetUnitClass()->GetTechnology())
            || inPlayer->GetTechTree()->Get(theUnit->GetUnitClass()->GetTechnology())->mData->mKind == 4)
            continue;
        long theWeight;
        if (theUnit->GetUnitClass()->IsLandBuilding() && !theUnit->GetUnitClass()->mIsWall && theUnit->GetUnitClass()->mMaxVelocity == 0.0f)
            theWeight = theUnit->GetUnitClass()->mInfluence + 5000;
        else
            theWeight = theUnit->GetUnitClass()->mInfluence + 10;
        EETileLocation theTile(theUnit->mTileX, theUnit->mTileY);
        theWeight += theTable->GetTileValue(inPlayer->GetPlayerID(), theTile);
        theUnits.insert(std::multimap<long, EEComplexUnit*, std::greater<long> >::value_type(theWeight, theUnit->GetComplexUnit()));
    }
    if (theUnits.size() > 0)
        theResult = theUnits.begin()->second;
    return theResult;
}

// FUNCTION: EXE 0x4740ee
// Sends the nearest groups of the wanted kinds to attack a tile (groups already attacking when it is unreachable),
// tells a human target by chat, and may create a new group
// NEAR: the original calls GetTile, the U2DPoint copy and the _Ptrit/vector accessors out of line
void EECPCombatManager::Attack(EEPlayer* inTarget, const EETileLocation& inTile, bool inAllowNewGroup, bool inSendChat,
    bool inUseLand, bool inUseAir, bool inUseSea, bool inUseSpace)
{
    EECPCMSubGroupFilter theFilter = { false, false, inUseLand, inUseSea, inUseAir, inUseSpace };
    EECPAGTask theTask;
    std::vector<EECPAttackSubGroup*> theCandidates;
    std::vector<EECPAttackSubGroup*> theAttacking;
    std::vector<EECPAttackSubGroup*> theSelected;
    std::multimap<long, EECPAttackSubGroup*> theByDistance;
    EECPCMTaskFilter theTaskFilter;
    EEAIContinent* theContinent = gContinentManager.GetContinentAt(inTile.mX, inTile.mY);
    EEMapTile* theTile = gWorld.mMap->GetTile(inTile.mX, inTile.mY);
    long theUnits = 0;
    long theNeeded = this->GetUnitLimit();
    bool theNoTransport = false;
    bool theAttack = true;
    theTask.mPlayer = this->mIntelligence->GetRandomEnemy(this->mPlayer);
    theTask.mMode = kAGModeCombat;
    theTask.mType = kAGTaskAttackLocation;
    theTask.mLocation = EECPILocation(inTile);
    theTask.mIsActive = true;
    switch (this->mPlayer->mDifficulty) {
    case kDifficultyEasy:
        theNeeded = UMCeil((float)theNeeded);
        break;
    case kDifficultyMedium:
        theNeeded = UMCeil(theNeeded * 0.75f);
        break;
    default:
        theNeeded = UMCeil(theNeeded * 0.5f);
        break;
    }
    this->mAttackCommander->Collect(theAttacking, theTaskFilter);
    for (long i = theAttacking.size() - 1; i >= 0 && theAttack == true; i--) {
        theUnits += (*(theAttacking.begin() + i))->GetNumUnits();
        theAttack = theUnits < theNeeded;
    }
    if (theContinent && theContinent->mCategory != kContinentCategoryMapEdge) {
        EETileLocation theHome(this->mPlayer->mCivilizationManager->mBuildManager->mTownCenterTile.mX,
            this->mPlayer->mCivilizationManager->mBuildManager->mTownCenterTile.mY);
        // theAttack keeps the unit-limit test above (was reset to true here)
        if (gWorld.mMap->mTerrainMesh.GetIsSpaceMap()) {
            if (theContinent != gContinentManager.GetContinentAt(theHome.mX, theHome.mY)
                || (theTile && theTile->mTerrainType == kTerrainSpace)) {
                theNoTransport = !this->mPlayer->mTechnologies->Get(gUnitClassTable[kUnitTypeSpaceDock]->mTechnology)->GetIsAvailable();
                if (theNoTransport)
                    theAttack = false;
            }
        }
    } else {
        theAttack = false;
    }
    if (theAttack) {
        if (theTile && (theTile->mTerrainType == kTerrainDeepWater || theTile->mTerrainType == kTerrainShallowWater)) {
            theFilter.mNearWater = true;
        } else if (gDCGameSpeed > 300) {
            for (long j = theContinent->mShoreTiles.size() - 1; j >= 0 && !theFilter.mNearWater; j--)
                theFilter.mNearWater = inTile.DistanceToSquared(*(theContinent->mShoreTiles.begin() + j)) <= 36;
        } else {
            theFilter.mNearWater = false;
        }
        if (theTile && theTile->mTerrainType == kTerrainSpace) {
            theFilter.mUseLand = false;
            theFilter.mUseAir = false;
            theFilter.mUseSea = false;
            theFilter.mInSpace = true;
        }
        this->mAttackCommander->Collect(theCandidates, theFilter);
        for (long k = theCandidates.size() - 1; k >= 0; k--) {
            long theDistance = (*(theCandidates.begin() + k))->GetPosition().DistanceToSquared(inTile);
            theByDistance.insert(std::multimap<long, EECPAttackSubGroup*>::value_type(theDistance,
                *(theCandidates.begin() + k)));
        }
        if (theByDistance.size() > 0) {
            for (std::multimap<long, EECPAttackSubGroup*>::iterator it = theByDistance.begin();
                 it != theByDistance.end(); ++it) {
                theSelected.push_back(it->second);
                if (it->second->mState != kASGStateAttack && it->second->mState != kASGStateRetreatToTown)
                    break;
            }
        }
    } else {
        for (long k = theAttacking.size() - 1; k >= 0; k--) {
            long theDistance = (*(theAttacking.begin() + k))->GetPosition().DistanceToSquared(inTile);
            theByDistance.insert(std::multimap<long, EECPAttackSubGroup*>::value_type(theDistance,
                *(theAttacking.begin() + k)));
        }
        for (std::multimap<long, EECPAttackSubGroup*>::iterator it = theByDistance.begin();
             it != theByDistance.end(); ++it) {
            EECPAttackSubGroup* theGroup = it->second;
            theGroup->mNextTaskCheckTime = 0;
            theGroup->CheckTask(gPhysics->mGameTime);
            if (!theGroup->mTask.mIsActive
                || (theGroup->GetNumUnits() > 0 && theGroup->GetPosition().DistanceToSquared(inTile) <= 300)) {
                theSelected.push_back(theGroup);
                break;
            }
        }
    }
    if (!theSelected.empty()) {
        if (gWorld.mPlayers[gWorld.mCurrentPlayer] == inTarget && inSendChat) {
            switch (theSelected.front()->mKind) {
            case kASGKindSea:
            case kASGKindSubmarine:
                this->mPlayer->mInteractionManager->SendChat(IDS_CHAT_MY_WARSHIPS_ARE_EN_ROUTE, 15000);
                break;
            case kASGKindAir:
            case kASGKindHelicopter:
                this->mPlayer->mInteractionManager->SendChat(IDS_CHAT_FIRE_WILL_RAIN_FROM_SKIES_ON_OUR, 15000);
                break;
            case kASGKindSpaceship:
                this->mPlayer->mInteractionManager->SendChat(IDS_CHAT_UP_UP_AND_AWAY, 15000);
                break;
            default:
                this->mPlayer->mInteractionManager->SendChat(IDS_CHAT_PERHAPS_I_COULD_SPARE_SOME_TROOPS, 15000);
                break;
            }
        }
        for (long n = theSelected.size() - 1; n >= 0; n--)
            theSelected[n]->SetTask(theTask);
    } else {
        theAttack = theAttack && inAllowNewGroup;
        if (gWorld.mPlayers[gWorld.mCurrentPlayer] == inTarget && inSendChat)
            this->mPlayer->mInteractionManager->SendChat(theNoTransport ? IDS_CHAT_CANT_DO_THAT_WITHOUT_SPACE_DOCK_SIR : IDS_CHAT_I_CAN_SPARE_NO_MORE_TROOPS, 15000);
    }
    if (theAttack)
        this->mAttackCommander->CreateGroup(theTask, 0);
}

// FUNCTION: EXE 0x47475d
// Updates the attack groups and refreshes the defense commander
void EECPCombatManager::Refresh()
{
    this->mAttackCommander->UpdateGroups();
    this->mDefenseCommander->Refresh();
}

// FUNCTION: EXE 0x474772
// Passes swapped units to the attack and defense commanders and personnel
void EECPCombatManager::UnitsSwapped(std::vector<EEPUnitSwap>& ioSwaps)
{
    this->mAttackCommander->UnitsSwapped(ioSwaps);
    this->mDefenseCommander->UnitsSwapped(ioSwaps);
    this->mPersonnel->UnitsSwapped(ioSwaps);
}

// FUNCTION: EXE 0x47479a
// Releases the unit from the attack commander's area groups
void EECPCombatManager::ReleaseFromAreaGroups(EEComplexUnit* inUnit)
{
    this->mAttackCommander->ReleaseFromAreaGroups(inUnit);
}

// FUNCTION: EXE 0x4747a9
// Drops defend tasks whose area is gone and whose unit is gone or dead
void EECPCombatManager::DropLostTasks()
{
    for (long i = this->mDefendTasks.size() - 1; i >= 0; i--) {
        EEAIArea* theArea;
        theArea = this->mDefendTasks[i].mLocation.mSerial ? gContinentManager.GetArea(this->mDefendTasks[i].mLocation.mSerial) : NULL;
        EEComplexUnit* theUnit = (this->mDefendTasks.begin() + i)->mUnit;
        if (!theArea && (!theUnit || theUnit->mHitPoints <= 0))
            this->mDefendTasks.erase(this->mDefendTasks.begin() + i);
        EE_LOST_DEBUG(2);   // LOST-DEBUG: none: exact only (the original calls both mDefendTasks[i] operator+ out of line)
    }
}

// FUNCTION: EXE 0x474873
// Drops the defend task on the area (and deactivates sub group tasks on it when inRelease); returns true if found
bool EECPCombatManager::DropAreaTask(unsigned long inSerial, bool inRelease)
{
    EECPAGTask theTask;
    bool theFound = false;
    if (inSerial) {
        for (long i = this->mDefendTasks.size() - 1; i >= 0; i--) {
            if (theFound)
                break;
            if (this->mDefendTasks[i].mLocation.mSerial != inSerial)
                continue;
            theTask = this->mDefendTasks[i];
            this->mDefendTasks.erase(this->mDefendTasks.begin() + i);
            theFound = true;
        }
        if (theFound && inRelease)
            this->ForgetTarget(inSerial);
    }
    return theFound;
}

// FUNCTION: EXE 0x474940
// Drops the defend task on the unit (and deactivates sub group tasks on it when inRelease); returns true if found
bool EECPCombatManager::DropUnitTask(unsigned long inUnitID, bool inRelease)
{
    EECPAGTask theTask;
    bool theFound = false;
    if (inUnitID != -1) {
        for (long i = this->mDefendTasks.size() - 1; i >= 0; i--) {
            if (theFound)
                break;
            if (!this->mDefendTasks[i].mUnit || (this->mDefendTasks.begin() + i)->mUnit->mUnitID != inUnitID)
                continue;
            theTask = *(this->mDefendTasks.begin() + i);
            this->mDefendTasks.erase(this->mDefendTasks.begin() + i);
            theFound = true;
        }
        if (theFound && inRelease)
            this->ForgetTargetUnit(inUnitID);
    }
    return theFound;
}

// NEAR: the version < 0xcb fix-up of the units' sub groups (0x474b8f..0x474d5e) is only roughly reconstructed
// FUNCTION: EXE 0x474a2f
// Restores personnel, tasks and commanders after loading; saves before 0xcb also leave each military unit in at
// most one sub group (detached units in none); returns the first error
long EECPCombatManager::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    long theResult = this->mPersonnel->Restore(inMajorVersion, inMinorVersion);
    long i;
    for (i = this->mDefendTasks.size() - 1; i >= 0; i--)
        if (theResult == 0)
            theResult = (this->mDefendTasks.begin() + i)->Restore(inMajorVersion, inMinorVersion);
    for (i = this->mAttackTasks.size() - 1; i >= 0; i--)
        if (theResult == 0)
            theResult = (this->mAttackTasks.begin() + i)->Restore(inMajorVersion, inMinorVersion);
    if (theResult == 0 && (theResult = this->mAttackCommander->Restore(inMajorVersion, inMinorVersion)) == 0)
        theResult = this->mDefenseCommander->Restore(inMajorVersion, inMinorVersion);
    if (inMajorVersion != 0 || inMinorVersion >= 0xcb)
        return theResult;
    std::vector<EECPAttackSubGroup*> theGroups;
    for (EEUnit* theUnit = this->mPlayer->mFirstUnit; theUnit; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (!theUnit->mComplexUnit)
            continue;
        EEUnitClass* theClass = theUnit->mUnitClass;
        if (theClass->mCategory == kUnitCategoryLandCitizen || theClass->mCategory == kUnitCategoryNavalCitizen || theClass->mIsProjectile
            || !(theClass->mMaxVelocity > 0.0f))
            continue;
        theGroups.clear();
        this->mAttackCommander->ListUnit(theUnit->mComplexUnit, theGroups);
        if (theGroups.size() > 1) {
            for (long j = theGroups.size() - 1; j >= 0 && theGroups.size() > 1; j--) {
                if (theGroups[j]->mGroup->mContinentID != 0
                    && !dynamic_cast<EECPASGTransport*>(theGroups[j]))
                    continue;
                theGroups[j]->mGroup->RemoveUnit(theUnit);
                theGroups.erase(theGroups.begin() + j);
            }
            while (theGroups.size() > 1) {
                theGroups.back()->mGroup->RemoveUnit(theUnit);
                theGroups.pop_back();
            }
            if (!theGroups.empty() && (theGroups[0]->mGroup->mContinentID == 0
                || dynamic_cast<EECPASGTransport*>(theGroups[0])))
                this->mPersonnel->RemoveUnit(theUnit->mComplexUnit);
        }
        if (!theUnit->mComplexUnit->mIsDetachedFromAI)
            continue;
        while (!theGroups.empty()) {
            theGroups.back()->mGroup->RemoveUnit(theUnit);
            theGroups.pop_back();
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x474d63
// Writes the commanders, group ids, tasks, wonder-race state and threat counts to the saved game; returns the first
// write error
long EECPCombatManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    std::map<EETileLocation, long>::iterator it;
    unsigned long theSize = 0;
    long theResult = this->mPersonnel->Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == 0 && (theResult = this->mAttackCommander->Save(inMajorVersion, inMinorVersion, outFile)) == 0
        && (theResult = this->mDefenseCommander->Save(inMajorVersion, inMinorVersion, outFile)) == 0
        && (theResult = outFile.Write(this->mNextGroupID)) == 0 && (theResult = outFile.Write(this->mNextSubGroupID)) == 0
        && (theResult = outFile.Write(this->mWasAttacked)) == 0 && (theResult = outFile.Write(this->mImprovementsEnabled)) == 0) {
        long theCount = this->mDefendTasks.size();
        theResult = outFile.Write(theCount);
        for (theCount = this->mDefendTasks.size() - 1; theCount >= 0; theCount--)
            if (theResult == 0)
                theResult = (this->mDefendTasks.begin() + theCount)->Save(inMajorVersion, inMinorVersion, outFile);
        if (theResult == 0) {
            theCount = this->mAttackTasks.size();
            theResult = outFile.Write(theCount);
            for (theCount = this->mAttackTasks.size() - 1; theCount >= 0; theCount--)
                if (theResult == 0)
                    theResult = this->mAttackTasks[theCount].Save(inMajorVersion, inMinorVersion, outFile);
            if (theResult == 0 && (theResult = outFile.Write(this->mIsWonderRace)) == 0) {
                theSize = this->mWonderAttackTimes.size();
                theResult = outFile.Write(theSize);
                for (theCount = 0; theCount < (long)theSize; theCount++) {
                    if (theResult != 0)
                        return theResult;
                    theResult = outFile.Write(this->mWonderAttackTimes[theCount]);
                }
                if (theResult == 0) {
                    theSize = this->mTileCounts.size();
                    theResult = outFile.Write(theSize);
                    it = this->mTileCounts.begin();
                    while (it != this->mTileCounts.end() && theResult == 0) {
                        theResult = it->first.Save(outFile);
                        if (theResult == 0)
                            theResult = outFile.Write(it->second);
                        ++it;
                    }
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x474fd8
// Hands a new military unit to personnel (not while loading, nor for most unit swaps)
void EECPCombatManager::UnitAdded(EEUnit* inUnit)
{
    if (!inUnit->mComplexUnit || gLoadingSavedGame || !this->mPlayer->mActive || !this->IsMilitaryUnit(inUnit->mComplexUnit))
        return;
    if (gPhysics && gPhysics->GetNumUnitSwaps()) {
        long theKind = gPhysics->GetUnitSwapKind(inUnit->mComplexUnit);
        if (theKind && theKind != 5 && theKind != 7)
            return;
    }
    this->mPersonnel->AddNewUnit(inUnit->mComplexUnit);
}

// FUNCTION: EXE 0x475047
// Removes a lost military unit from personnel and the attack groups; a lost citizen is noted as a threat (unit
// swaps excepted)
void EECPCombatManager::UnitRemoved(EEUnit* inUnit)
{
    EEComplexUnit* theUnit = inUnit->GetComplexUnit();
    if (!theUnit || (gLoadingSavedGame && !gLoadingPlayerMedia))
        return;
    if (this->IsMilitaryClass(inUnit->GetUnitClass())) {
        if (gPhysics && gPhysics->GetNumUnitSwaps() && gPhysics->GetUnitSwapKind(inUnit->GetComplexUnit()) == 1)
            return;
        this->mPersonnel->RemoveUnit(theUnit);
        this->mAttackCommander->RemoveUnit(inUnit);
    } else if (theUnit->mIsCitizen) {
        if (gPhysics && gPhysics->GetNumUnitSwaps() && gPhysics->GetUnitSwapKind(theUnit) == 1)
            return;
        this->NoteThreat(theUnit, false);
    }
}

// FUNCTION: EXE 0x4750f2
// Removes the unit from its attack group, returning it to personnel when asked; returns true if it was in one
bool EECPCombatManager::TakeFromAttack(EEUnit* inUnit, bool inReturnUnit)
{
    bool theResult = this->mAttackCommander->RemoveUnit(inUnit);
    if (theResult && inReturnUnit)
        this->mPersonnel->ReturnUnit(inUnit);
    return theResult;
}

// FUNCTION: EXE 0x475122
// Returns true when the unit type's class is in a military category
bool EECPCombatManager::IsMilitaryType(unsigned long inUnitType)
{
    EEUnitClass* theClass = gUnitClassTable[inUnitType];
    bool theResult = false;
    if (theClass) {
        switch (theClass->mCategory) {
        case kUnitCategoryAirplane:
        case kUnitCategoryNaval:
        case kUnitCategoryLand:
        case kUnitCategoryHelicopter:
        case kUnitCategoryLand8:
        case kUnitCategorySubmergedNaval:
        case kUnitCategoryLandPriest:
        case kUnitCategoryNavalPriest:
        case kUnitCategoryLand15:
        case kUnitCategoryLandTransport:
        case kUnitCategoryHelicopterTransport:
        case kUnitCategoryNavalTransport:
        case kUnitCategoryNavalBuilding:
        case kUnitCategoryLandMilitaryHero:
        case kUnitCategoryHelicopterPriest:
        case kUnitCategorySeaAnimal:
        case kUnitCategoryAirAnimal:
        case kUnitCategorySpaceship:
        case kUnitCategorySpaceBuilding:
        case kUnitCategorySpaceTransport:
        case kUnitCategoryPlanetSpaceship:
            theResult = true;
            break;
        case kUnitCategoryAmbient:
        case kUnitCategoryResource:
        case kUnitCategoryBuilding:
        case kUnitCategoryLandCitizen:
        case kUnitCategoryProjectile:
        case kUnitCategoryBomb:
        case kUnitCategoryMissile:
        case kUnitCategoryBuilding19:
        case kUnitCategoryTorpedo:
        case kUnitCategoryNavalCitizen:
        case kUnitCategoryMiningBuilding:
        case kUnitCategoryGate:
        case kUnitCategoryFortress:
        case kUnitCategoryBuildingPriest:
            theResult = false;
            break;
        default:
            theResult = false;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x47517d
// Returns true when the unit's type is military
bool EECPCombatManager::IsMilitaryTypeUnit(EEUnit* inUnit)
{
    if (inUnit)
        return EECPCombatManager::IsMilitaryType(inUnit->mUnitClass->mUnitType);
    return false;
}

// FUNCTION: EXE 0x475198
// Runs the attack and defense commanders, personnel, the attack choice and the wonder race each tick
void EECPCombatManager::Tick(unsigned long inTime)
{
    this->mAttackCommander->Update(inTime);
    if (inTime >= this->mDefenseCommander->mNextUpdateTime)
        this->mDefenseCommander->Update(inTime);
    if (this->mPersonnel->mNextUpdateTime <= inTime)
        this->mPersonnel->Update(inTime);
    this->Update(inTime);
    if (this->mIsWonderRace)
        this->UpdateWonderRace(inTime);
}

// FUNCTION: EXE 0x4751e0
// Checks whether an enemy team has enough wonders; while it does, attacks a random wonder of that team every
// 150 s, otherwise clears the wonder attack times
void EECPCombatManager::UpdateWonderRace(unsigned long inTime)
{
    std::vector<long> theTeams;
    std::vector<EEComplexUnit*> theWonders;
    EETeam* theTeam = NULL;
    if (!this->mIsWonderRace)
        return;
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
        EEPlayer* thePlayer = gWorld.mPlayers[i];
        if (!thePlayer || thePlayer->mObserverMode || thePlayer == this->mPlayer || !this->mPlayer->mEnemies[thePlayer->mPlayerID])
            continue;
        long theIndex = thePlayer->GetTeamIndex();
        if (std::find(theTeams.begin(), theTeams.end(), theIndex) == theTeams.end())
            theTeams.push_back(thePlayer->GetTeamIndex());
    }
    this->mIsWonderRace = false;
    for (long j = theTeams.size() - 1; j >= 0 && !this->mIsWonderRace; j--) {
        if (gWorld.mTeams[theTeams[j]].mVictoryCondition
            && gWorld.mTeams[theTeams[j]].mVictoryCondition->HasEnoughWonders()) {
            theTeam = &gWorld.mTeams[theTeams[j]];
            this->mIsWonderRace = true;
        }
    }
    if (this->mIsWonderRace) {
        if (!this->mWonderAttackTimes.empty() && this->mWonderAttackTimes.back() <= inTime && theTeam) {
            for (long k = theTeam->mPlayers.size() - 1; k >= 0; k--) {
                EEPlayer* thePlayer = gWorld.mPlayers[theTeam->mPlayers[k]];
                if (!thePlayer)
                    continue;
                for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
                    if (theUnit->GetComplexUnit() && theUnit->GetComplexUnit()->GetUnitClass()->IsLandBuilding()
                        && this->mPlayer->GetTechTree()->Get(theUnit->GetComplexUnit()->GetUnitClass()->GetTechnology())
                        && this->mPlayer->GetTechTree()->Get(theUnit->GetComplexUnit()->GetUnitClass()->GetTechnology())->mData->mKind == 4)
                        theWonders.push_back(theUnit->GetComplexUnit());
                }
            }
            if (!theWonders.empty()) {
                EEAIRandomShuffle(theWonders.begin(), theWonders.end());
                this->AttackUnit(theWonders.front());
                this->mWonderAttackTimes.pop_back();
                inTime += 150000;
                this->mWonderAttackTimes.push_back(inTime);
            } else {
                this->mWonderAttackTimes.clear();
            }
        }
    } else {
        this->mWonderAttackTimes.clear();
    }
}

// FUNCTION: EXE 0x4754e7
// Sends a difficulty-sized share of the groups at a wonder and creates a group; a non-wonder target or a team
// about to win gets every group and sets the wonder race (five new groups for a wonder)
void EECPCombatManager::AttackUnit(EEComplexUnit* inTarget)
{
    EECPCMKindFilter theFilter = { false, false, false };
    EECPAGTask theTask;
    UWideString theString;
    std::vector<EECPAttackSubGroup*> theGroups;
    std::vector<EECPAttackSubGroup*> theSelected;
    EETeam& theTeam = gWorld.mTeams[inTarget->mPlayer->GetTeamIndex()];
    bool theAll = false;
    long theCount = 0;
    long theStrength = 0;
    bool theAttack = true;
    theTask.mPlayer = gWorld.mPlayers[inTarget->mPlayer->mPlayerID];
    theTask.mMode = kAGModeWonder;
    theTask.mType = kAGTaskAttackWonder;
    theTask.SetUnit(inTarget);
    theTask.mLocation = EECPILocation(EETileLocation(inTarget->mTileX, inTarget->mTileY));
    theTask.mIsActive = true;
    EETown* theTown = inTarget->FindNearestTown();
    if (theTown) {
        theFilter.mNearWater = theTown->mNearWater;
        theFilter.mNearMapEdge = theTown->mNearMapEdge;
    }
    this->mAttackCommander->CollectAll(theGroups, theFilter);
    for (long i = theTeam.mPlayers.size() - 1; i >= 0; i--)
        theStrength += gWorld.mPlayers[theTeam.mPlayers[i]]->GetNumWonderUnits();
    if (inTarget->mPlayer->mTechnologies->Get(inTarget->mUnitClass->mTechnology)
        && inTarget->mPlayer->mTechnologies->Get(inTarget->mUnitClass->mTechnology)->mData->mKind != 4)
        theAttack = false;
    if (theStrength < gWorld.mNumberOfWondersToWin && theAttack) {
        if (theTown && theGroups.size() > 3) {
            switch (this->mPlayer->mDifficulty) {
            case kDifficultyEasy:
                theCount = UMCeil(gPhysics->GetRandom(0.05f, 0.1f) * theGroups.size());
                break;
            case kDifficultyMedium:
                theCount = UMCeil(gPhysics->GetRandom(0.1f, 0.25f) * theGroups.size());
                break;
            case kDifficultyHard:
                theCount = UMCeil(gPhysics->GetRandom(0.25f, 0.4f) * theGroups.size());
                break;
            }
            if (theCount >= (long)theGroups.size())
                theCount = theGroups.size();
            theSelected.reserve(theCount);
            for (long j = 0; j < theCount; j++)
                theSelected.push_back(theGroups[j]);
        }
    } else {
        this->mIsWonderRace = true;
        theSelected = theGroups;
        theAll = true;
    }
    for (long k = theSelected.size() - 1; k >= 0; k--)
        theSelected[k]->SetTask(theTask);
    long theTimes = (theAttack && theAll) ? 5 : 1;
    for (long n = 0; n < theTimes; n++)
        this->mAttackCommander->CreateGroup(theTask, 0);
    if (theAttack && this->mWonderAttackTimes.empty() && !gWorld.mGameOptions.mIsScenarioGame
        && (inTarget->mPlayer == gWorld.mPlayers[gWorld.mCurrentPlayer]
            || gWorld.mPlayers[gWorld.mCurrentPlayer]->mEnemies[inTarget->mPlayer->mPlayerID] == 0)
        && (this->mIsWonderRace || gWorld.mPlayers[gWorld.mCurrentPlayer] == inTarget->mPlayer))
        this->mPlayer->mInteractionManager->SendChat(IDS_CHAT_THIS_IS_YOUR_LAST_ACT, 15000);
}

// FUNCTION: EXE 0x47590f
// Sends a difficulty-sized share of the groups to defend a wonder (every group otherwise), creates a defend group
// and tells a human owner by chat
void EECPCombatManager::DefendUnit(EEComplexUnit* inTarget)
{
    EECPCMKindFilter theFilter;
    theFilter.mB0 = false;
    theFilter.mNearWater = false;
    // also clear the spaceship flag: without a nearest town it was left uninitialised
    theFilter.mNearMapEdge = false;
    EECPAGTask theTask;
    std::vector<EECPAttackSubGroup*> theGroups;
    std::vector<EECPAttackSubGroup*> theSelected;
    EETeam& theTeam = gWorld.mTeams[inTarget->mPlayer->GetTeamIndex()];
    long theCount = 0;
    long theStrength = 0;
    bool theDefend = true;
    theTask.mPlayer = this->GetIntelligence()->GetRandomEnemy(this->mPlayer);
    theTask.mMode = kAGModeCombat;
    theTask.mType = kAGTaskDefendWonder;
    theTask.SetUnit(inTarget);
    theTask.mLocation = EECPILocation(EETileLocation(inTarget->mTileX, inTarget->mTileY));
    theTask.mIsActive = true;
    EETown* theTown = inTarget->FindNearestTown();
    if (theTown) {
        theFilter.mNearWater = theTown->mNearWater;
        theFilter.mNearMapEdge = theTown->mNearMapEdge;
    }
    this->mAttackCommander->CollectAll(theGroups, theFilter);
    for (long i = theTeam.mPlayers.size() - 1; i >= 0; i--)
        theStrength += gWorld.mPlayers[theTeam.mPlayers[i]]->GetNumWonderUnits();
    if (inTarget->mPlayer->GetTechTree()->Get(inTarget->GetUnitClass()->GetTechnology())
        && inTarget->mPlayer->GetTechTree()->Get(inTarget->GetUnitClass()->GetTechnology())->mData->mKind != 4)
        theDefend = false;
    if (theDefend && theStrength < gWorld.mNumberOfWondersToWin) {
        if (theTown && theGroups.size() > 3) {
            switch (this->mPlayer->mDifficulty) {
            case kDifficultyEasy:
                theCount = UMCeil(gPhysics->GetRandom(0.3f, 0.4f) * theGroups.size());
                break;
            case kDifficultyMedium:
                theCount = UMCeil(gPhysics->GetRandom(0.2f, 0.3f) * theGroups.size());
                break;
            case kDifficultyHard:
                theCount = UMCeil(gPhysics->GetRandom(0.1f, 0.15f) * theGroups.size());
                break;
            }
            if (theCount >= (long)theGroups.size())
                theCount = theGroups.size();
            theSelected.reserve(theCount);
            for (long j = 0; j < theCount; j++)
                theSelected.push_back(theGroups[j]);
        }
    } else {
        theSelected = theGroups;
    }
    for (long k = theSelected.size() - 1; k >= 0; k--)
        theSelected[k]->SetTask(theTask);
    this->mAttackCommander->CreateGroup(theTask, 1);
    if (theDefend && gWorld.mPlayers[gWorld.mCurrentPlayer] == inTarget->mPlayer && !gWorld.mGameOptions.mIsScenarioGame)
        this->mPlayer->mInteractionManager->SendChat(IDS_CHAT_WE_WILL_HELP_YOU_PROTECT, 15000);
}

// FUNCTION: EXE 0x475c90
// Defends an own or allied unit, or attacks an enemy one (marking the manager attacked and starting the wonder
// attack timer in a wonder race)
void EECPCombatManager::RespondToUnit(EEComplexUnit* inUnit)
{
    if (this->mPlayer->mObserverMode || inUnit->mPlayer->mObserverMode)
        return;
    if (inUnit->mPlayer == this->mPlayer || this->mPlayer->mEnemies[inUnit->mPlayer->mPlayerID] == 0) {
        this->mDefenseCommander->RespondToUnit(inUnit);
        this->DefendUnit(inUnit);
    } else {
        this->mWasAttacked = true;
        this->AttackUnit(inUnit);
        if (this->mIsWonderRace && this->mWonderAttackTimes.empty()) {
            unsigned long theTime = gPhysics->mGameTime + 150000;
            this->mWonderAttackTimes.push_back(theTime);
        }
    }
}

// TEMPLATE: EXE 0x47669d
// (EEAIAStarBounds::Intersect: the inline body in ee/EEAIAStarStorage.h, emitted after its first user Update)

// Template and inline COMDATs this unit's code instantiates (the bodies come from the headers; each was compared
// against the original by name)
// TEMPLATE: EXE 0x47366e
// TEMPLATE: EXE 0x475d24
// TEMPLATE: EXE 0x475d52
// TEMPLATE: EXE 0x475d7d
// TEMPLATE: EXE 0x475e3d
// TEMPLATE: EXE 0x475e51
// TEMPLATE: EXE 0x475e6d
// TEMPLATE: EXE 0x475e99
// TEMPLATE: EXE 0x475f0d
// TEMPLATE: EXE 0x475f4b
// TEMPLATE: EXE 0x475f76
// TEMPLATE: EXE 0x475fe7
// TEMPLATE: EXE 0x47602b
// TEMPLATE: EXE 0x4760ee
// TEMPLATE: EXE 0x476145
// TEMPLATE: EXE 0x47617c
// TEMPLATE: EXE 0x4761b5
// TEMPLATE: EXE 0x4761ce
// TEMPLATE: EXE 0x47623f
// TEMPLATE: EXE 0x476271
// TEMPLATE: EXE 0x4762a3
// TEMPLATE: EXE 0x4762e4
// TEMPLATE: EXE 0x4765c1
// TEMPLATE: EXE 0x47662c
// TEMPLATE: EXE 0x47665e
// TEMPLATE: EXE 0x476689
// TEMPLATE: EXE 0x476710
// TEMPLATE: EXE 0x4767cd
// TEMPLATE: EXE 0x476831
// TEMPLATE: EXE 0x476895
// TEMPLATE: EXE 0x4768bc
// TEMPLATE: EXE 0x476918
// TEMPLATE: EXE 0x476941
// TEMPLATE: EXE 0x47696d
// TEMPLATE: EXE 0x47698a
// TEMPLATE: EXE 0x4769b6
// TEMPLATE: EXE 0x4769da
// TEMPLATE: EXE 0x476b19
// TEMPLATE: EXE 0x476b66
// TEMPLATE: EXE 0x476b9f
// TEMPLATE: EXE 0x476bec
// TEMPLATE: EXE 0x476c5c

// Template and inline COMDATs this unit already emits (compared by name)
// TEMPLATE: EXE 0x475f1f

// Template COMDATs named by their mangled names
// TEMPLATE: EXE 0x4769ec
// ?_Insertion_sort@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPHitPointsGreater@@@Z
// TEMPLATE: EXE 0x476a15
// ?_Sort@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0HUEECPHitPointsGreater@@@Z
// TEMPLATE: EXE 0x476ae8
// ?_Unguarded_insertion_sort@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPHitPointsGreater@@PAPAVEEUnit@@@Z
// TEMPLATE: EXE 0x476ca9
// ?_Insertion_sort_1@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPHitPointsGreater@@PAPAVEEUnit@@@Z
// TEMPLATE: EXE 0x476d3a
// ?make_heap@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPHitPointsGreater@@@Z
// TEMPLATE: EXE 0x476d74
// ?sort_heap@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPHitPointsGreater@@@Z
// TEMPLATE: EXE 0x476db5
// ?_Unguarded_partition@std@@YA?AV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@V21@0UEECPHitPointsGreater@@PAPAVEEUnit@@@Z
// TEMPLATE: EXE 0x476eb3
// ?_Unguarded_insert@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@PAVEEUnit@@UEECPHitPointsGreater@@@Z
// TEMPLATE: EXE 0x476edc
// ?_Make_heap@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPHitPointsGreater@@PAHPAPAVEEUnit@@@Z
// TEMPLATE: EXE 0x476f20
// ?pop_heap@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPHitPointsGreater@@@Z
// TEMPLATE: EXE 0x476f49
// ?_Adjust_heap@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@HHPAVEEUnit@@UEECPHitPointsGreater@@@Z
// TEMPLATE: EXE 0x476fae
// ?_Pop_heap_0@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@0UEECPHitPointsGreater@@PAPAVEEUnit@@@Z
// TEMPLATE: EXE 0x477001
// ?_Push_heap@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@HHPAVEEUnit@@UEECPHitPointsGreater@@@Z
// TEMPLATE: EXE 0x477040
// ?_Pop_heap@std@@YAXV?$_Ptrit@PAVEEUnit@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@1@00PAVEEUnit@@UEECPHitPointsGreater@@PAH@Z
