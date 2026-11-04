// C:\EEX\Empire Earth\AI Thread\EEComputerPlayer.cpp (folder inferred; the class keys "EEComputerPlayer-Data" and
// "EEComputerPlayer-CPU Only Flag"; RTTI EEComputerPlayer, vtable 0x43a57c). A computer player: 0x2ce2e..0x412b1
// (EEArtificialIntelligence.cpp ends at 0x2ce2e; EECPAGOrder.cpp starts at 0x412b1). The two class keys' chains, the
// ctors/dtor, the slots and members in address order, EEAIFOFavoriteUnitScreener and EECPUGatherHeroesCount's bodies
// that sit here, then the template COMDATs
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <algorithm>
#include <math.h>

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

#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEAIRandom.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EECPCivilizationManager.h"
#include "ee/EECPResourceManager.h"
#include "ee/EECPResourceCenter.h"
#include "ee/EECPResearchManager.h"
#include "ee/EECPWallManager.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPDiplomacyManager.h"
#include "ee/EECPPlayerInteractionManager.h"
#include "ee/EECPUMGatherUnitsScreenBase.h"
#include "ee/EECPUnitManager.h"
#include "ee/EECPUnitSelector.h"
#include "ee/EECPExplorationManager.h"
#include "ee/EECPPersonnel.h"
#include "ee/EECPDefenseCommander.h"
#include "ee/EECPBuildManager.h"
#include "ee/EEAIFOUnitsToKillScoringBase.h"
#include <EEAIFunctionObjects.h>
#include "ee/EECalamityManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPUnitTileDistanceLess.h"
#include "ee/EETeam.h"
#include "ee/EEVictoryCondition.h"
#include "ee/EEVCWonder.h"
#include "ee/EECPIntelligence.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEGameOptions.h"
#include "ee/EECPBehavior.h"
#include "LostDebug.h"   // NOT original source: EE_LOST_DEBUG stand-in (RepairPointers; docs/LOST_DEBUG.md)
// The existing civilization-loader helper declared in EEAResetAllPlayerInfo.h (0x2806e7).
long Fn2806E7(UString& ioName, EEPlayer* inPlayer);

// Orders resource centers by the distance of their centres from mCenter (0x2ceb4; the sort COMDATs 0x408ae.. take it
// by value: EETileDistanceLess's copy ctor). Unit-local (provisional name)
struct EECPCenterDistanceLess {
    EECPCenterDistanceLess(const U2DPoint<long>& inCenter) : mCenter(inCenter) {}
    long DistanceSquared(const EETileCoordinate& inTile) const;   // 0x8a3f (ICF)
    bool operator()(EECPResourceCenter* inFirst, EECPResourceCenter* inSecond) const;

    EETileCoordinate mCenter;
};

// EECPUnitTileDistanceLess (0x2cf1e; the sort COMDATs 0x40dc4..): ee/EECPUnitTileDistanceLess.h

// chain at 0x42ce2e (wrapper)
//   object 0x8e5c70  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
//   guarded dtor (byte ptr [0x8e5c68], 1): a class static data member (statics of one class share the guard byte, one bit each) unless the ctor is guarded too (template static)
const UString EEComputerPlayer::sData("EEComputerPlayer-Data");   // class static member (owner from the key prefix; EEComputerPlayer declares `static const UString ...;`) (provisional name)
// SYNTHETIC: EXE 0x42ce2e   (wrapper)
// SYNTHETIC: EXE 0x42ce38   (ctor)
// SYNTHETIC: EXE 0x42ce49   (registrar)
// SYNTHETIC: EXE 0x42ce55   (dtor)

// chain at 0x42ce71 (wrapper)
//   object 0x8e5c58  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
//   guarded dtor (byte ptr [0x8e5c68], 2): a class static data member (statics of one class share the guard byte, one bit each) unless the ctor is guarded too (template static)
const UString EEComputerPlayer::sCPUOnlyFlag("EEComputerPlayer-CPU Only Flag");   // class static member (owner from the key prefix; EEComputerPlayer declares `static const UString ...;`) (provisional name)
// SYNTHETIC: EXE 0x42ce71   (wrapper)
// SYNTHETIC: EXE 0x42ce7b   (ctor)
// SYNTHETIC: EXE 0x42ce8c   (registrar)
// SYNTHETIC: EXE 0x42ce98   (dtor)

// FUNCTION: EXE 0x42ceb4
// Returns true when the first resource center lies nearer to mCenter than the second (provisional name)
// The centers by the distance of their centres from mCenter. Owner and name PROVISIONAL (a unit-local predicate over
// EETileDistanceLess). Built without /Og (rule 4 evidence: every local memory-resident, `leave`, the bool through a
// return temp); with the pragma, the hash-fitted local names and the inline DistanceSquared below it is exact (r37).
#pragma optimize("g", off)
bool EECPCenterDistanceLess::operator()(EECPResourceCenter* inFirst, EECPResourceCenter* inSecond) const
{
    EETileCoordinate theFirstTile;
    EETileCoordinate theSecondTile;
    inFirst->GetCenter(theFirstTile);
    inSecond->GetCenter(theSecondTile);
    long theLeft = this->DistanceSquared(theFirstTile);
    long theRight = this->DistanceSquared(theSecondTile);
    bool theResult = theLeft < theRight;
    return theResult;
}
#pragma optimize("g", on)


// Returns the squared tile distance from mCenter. ICF-folds into 0x8a3f (EETileDistanceLess::DistanceSquared).
// Inline: VC6's /Os post-check undoes its expansion at both no-/Og call sites above (the calls stay out of line)
// but its two locals keep their caller frame slots (the original's 16 unused bytes below the return temp)
inline long EECPCenterDistanceLess::DistanceSquared(const EETileCoordinate& inTile) const
{
    long theDY = inTile.GetY();
    theDY = this->mCenter.mY - theDY;
    long theDX = inTile.GetX();
    theDX = this->mCenter.mX - theDX;
    theDX *= theDX;
    theDY *= theDY;
    return theDX + theDY;
}

// FUNCTION: EXE 0x42cf1e
// Returns true when the first unit's tile lies nearer to mCenter than the second's (provisional name)
// Built without /Og (rule 4 evidence: memory-resident locals, `leave`; exact with the pragma and the header's undone-inline
// DistanceSquared, r37); the units by the distance of their tiles from mCenter (owner and name PROVISIONAL)
#pragma optimize("g", off)
bool EECPUnitTileDistanceLess::operator()(EEUnit* inFirst, EEUnit* inSecond) const
{
    long theLeft = this->DistanceSquared(inFirst->GetTile());
    long theRight = this->DistanceSquared(inSecond->GetTile());
    bool theResult = theLeft < theRight;
    return theResult;
}
#pragma optimize("g", on)

// EECPUnitTileDistanceLess::DistanceSquared (ICF-folds into 0x8a3f): inline in ee/EECPUnitTileDistanceLess.h

// FUNCTION: EXE 0x42cf7e
// Creates a named computer player with the given player id and initialises its AI tuning and managers
EEComputerPlayer::EEComputerPlayer(const UWideString& inName, unsigned long inPlayerID)
    : EEPlayer(inName, inPlayerID)
{
    this->Init();
    this->mComputerPlayer = dynamic_cast<EEComputerPlayer*>(this);
}

// FUNCTION: EXE 0x42d003
// Sets the AI's combat state, clearing the next manager-enable time when the state changes
void EEComputerPlayer::SetCombatState(unsigned long inCombatState)
{
    if (inCombatState != this->mCombatState)
        this->mNextEnableTime = 0;
    this->mCombatState = inCombatState;
}

// SYNTHETIC: EXE 0x42d021
// (the scalar deleting destructor, emitted with the vtable; slot EEComputerPlayer+0x0)

// FUNCTION: EXE 0x42d03d
// Creates an unnamed computer player (for loading) and initialises its AI tuning and managers
EEComputerPlayer::EEComputerPlayer()
{
    this->Init();
    this->mComputerPlayer = dynamic_cast<EEComputerPlayer*>(this);
}

// FUNCTION: EXE 0x42d0bb
// Destroys the computer player: releases the units queued to kill, deletes its managers and resource centers
EEComputerPlayer::~EEComputerPlayer()
{
    for (long i = this->mUnitsToKill.size() - 1; i >= 0; i--) {
        if (this->mUnitsToKill[i] != NULL)
            this->mUnitsToKill[i]->Dereference();
    }
    this->mUnitsToKill.clear();
    delete this->mExplorationManager;
    delete this->mDiplomacyManager;
    delete this->mCombatManager;
    delete this->mCivilizationManager;
    delete this->mInteractionManager;
    DeleteVectorContents(this->mResourceCenters);
}

// FUNCTION: EXE 0x42d1a5
// Adds a resource amount to the player and records it as spent with the AI's resource manager
void EEComputerPlayer::AddResource(long inIndex, long inAmount)
{
    EEPlayer::AddResource(inIndex, inAmount);
    this->mCivilizationManager->mResourceManager->Spend(inIndex, inAmount);
}

// FUNCTION: EXE 0x42d1c7
// Sets the AI's population cap: its own limit if another team is winning by wonders or in deathmatch,
// else a weighted average of its own limit and the largest human population, scaled by difficulty
void EEComputerPlayer::WorkOutPopulation()
{
    float theOtherWeight = gCPBehaviors[kCPBehaviorPopulationCapOtherWeight].mFloatValue;
    float theOwnWeight = gCPBehaviors[kCPBehaviorPopulationCapOwnWeight].mFloatValue;
    long theOwn = EEPlayer::GetPopulationLimit(0);
    bool theFound = false;
    long theMax = 0;
    bool theFlag = this->IsDeathmatch();
    if (!this->mIgnoreOthers) {
        for (unsigned long i = 0; i < gWorld.GetNumTeams(); i++) {
            if (theFound)
                break;
            if (gWorld.mTeams[i].mVictoryCondition->HasEnoughWonders() && !gWorld.mTeams[i].ContainsPlayer(this->mPlayerID))
                theFound = true;
            else
                theFound = false;
        }
        if (!theFound && !theFlag) {
            for (unsigned long j = 1; j < gWorld.mNumPlayers; j++) {
                EEPlayer* thePlayer = gWorld.mPlayers[j];
                if (thePlayer->IsHuman()) {
                    long theCount = thePlayer->GetTotalPopulation(true, true);
                    if (theCount >= theOwn)
                        theCount = theOwn;
                    if (theMax <= theCount)
                        theMax = theCount;
                }
            }
            this->mPopulationCap = UMFloatToLong((theOwn * theOwnWeight + theMax * theOtherWeight) / (theOwnWeight + theOtherWeight));
            this->mPopulationCap = UMFloatToLong(this->GetPopLimitAdjustment() * this->mPopulationCap);
        } else
            this->mPopulationCap = theOwn;
    }
}

// FUNCTION: EXE 0x42d334
// Chooses which AI managers (epoch, settlements, research, production, building, defense, exploration,
// combat, morale) run this turn by combat-state priority, up to the enabled-manager limit
void EEComputerPlayer::EnableManagers(unsigned long inTime)
{
    std::vector<long> thePriority;
    std::vector<long> theOther;
    unsigned long theAdvanceLimit = this->mManagerEpochAdvanceWait * 1000;
    unsigned long theWait = 0;
    unsigned long theAdvanceWait = 0;
    unsigned long theWaitLimit = this->mManagerAttackWait * 1000;
    unsigned long theLimit = this->mMaxEnabledManagers + 1;
    if (this->mNextAttackTime > inTime)
        theWait = this->mNextAttackTime - inTime;
    if (this->mCivilizationManager->mNextEpochAdvanceTime > inTime)
        theAdvanceWait = this->mCivilizationManager->mNextEpochAdvanceTime - inTime;

    this->mCivilizationManager->mEpochAdvanceEnabled = false;
    this->mCivilizationManager->mResourceManager->mSettlementsEnabled = false;
    this->mCivilizationManager->mResearchManager->mEnabled = false;
    this->mCivilizationManager->mUnitManager->mProductionEnabled = false;
    this->mCivilizationManager->mBuildingCountsEnabled = false;
    this->mCivilizationManager->mMoraleBuildingsEnabled = false;
    this->mCombatManager->mDefenseCommander->mEnabled = false;
    this->mExplorationManager->mEnabled = false;
    this->mCombatManager->mImprovementsEnabled = false;

    if (this->Fn2EC67()) {
        thePriority.push_back(0L);
        thePriority.push_back(1L);
        thePriority.push_back(2L);
        thePriority.push_back(3L);
        thePriority.push_back(4L);
        thePriority.push_back(5L);
        thePriority.push_back(6L);
        thePriority.push_back(7L);
        thePriority.push_back(8L);
        thePriority.push_back(9L);
        theLimit = thePriority.size();
    } else {
        switch (this->mCombatState) {
        case 0:
            if (theWait > theWaitLimit && this->CanEnableManager(kCPManagerProduction, 2))
                thePriority.push_back(3);
            if (this->CanEnableManager(kCPManagerResource, 3))
                thePriority.push_back(1);
            if (theAdvanceWait < theAdvanceLimit && theWait > theWaitLimit)
                thePriority.push_back(0);
            if ((this->CanEnableManager(kCPManagerDefense, 2) && this->CanEnableManager(kCPManagerDefense, 5) && this->CanEnableManager(kCPManagerDefense, 6)) || this->mBuildingDamaged || this->mCitizenDamaged)
                thePriority.push_back(6);
            if (this->CanEnableManager(kCPManagerBuild, 3) && this->CanEnableManager(kCPManagerBuild, 4))
                thePriority.push_back(4);
            if (this->CanEnableManager(kCPManagerExploration, 2) && this->CanEnableManager(kCPManagerExploration, 5) && this->CanEnableManager(kCPManagerExploration, 6))
                thePriority.push_back(7);
            if (this->CanEnableManager(kCPManagerMoraleBuildings, 3))
                theOther.push_back(9);
            if (this->CanEnableManager(kCPManagerResearch, 5))
                theOther.push_back(2);
            if (this->CanEnableManager(kCPManagerCombat, 5))
                theOther.push_back(8);
            break;
        case 1:
            if (theAdvanceWait < theAdvanceLimit && theWait > theWaitLimit)
                thePriority.push_back(0);
            if (this->CanEnableManager(kCPManagerResearch, 5))
                thePriority.push_back(2);
            if (this->CanEnableManager(kCPManagerResource, 3))
                thePriority.push_back(1);
            if (theWait > theWaitLimit && this->CanEnableManager(kCPManagerProduction, 2))
                thePriority.push_back(3);
            if (this->CanEnableManager(kCPManagerBuild, 3) && this->CanEnableManager(kCPManagerBuild, 4))
                thePriority.push_back(4);
            if (this->CanEnableManager(kCPManagerMoraleBuildings, 3))
                thePriority.push_back(9);
            if ((this->CanEnableManager(kCPManagerDefense, 2) && this->CanEnableManager(kCPManagerDefense, 5) && this->CanEnableManager(kCPManagerDefense, 6)) || this->mBuildingDamaged || this->mCitizenDamaged)
                theOther.push_back(6);
            if (this->CanEnableManager(kCPManagerExploration, 2) && this->CanEnableManager(kCPManagerExploration, 5) && this->CanEnableManager(kCPManagerExploration, 6))
                theOther.push_back(7);
            if (this->CanEnableManager(kCPManagerCombat, 5))
                theOther.push_back(8);
            break;
        case 2:
            if (this->CanEnableManager(kCPManagerCombat, 5))
                thePriority.push_back(8);
            if ((this->CanEnableManager(kCPManagerDefense, 2) && this->CanEnableManager(kCPManagerDefense, 5) && this->CanEnableManager(kCPManagerDefense, 6)) || this->mBuildingDamaged || this->mCitizenDamaged)
                thePriority.push_back(6);
            if (this->CanEnableManager(kCPManagerBuild, 3) && this->CanEnableManager(kCPManagerBuild, 4))
                thePriority.push_back(4);
            if (theWait > theWaitLimit && this->CanEnableManager(kCPManagerProduction, 2))
                thePriority.push_back(3);
            if (this->CanEnableManager(kCPManagerResource, 3))
                thePriority.push_back(1);
            if (this->CanEnableManager(kCPManagerMoraleBuildings, 3))
                thePriority.push_back(9);
            if (this->CanEnableManager(kCPManagerResearch, 5))
                theOther.push_back(2);
            if (theAdvanceWait < theAdvanceLimit && theWait > theWaitLimit)
                theOther.push_back(0);
            if (this->CanEnableManager(kCPManagerExploration, 2) && this->CanEnableManager(kCPManagerExploration, 5) && this->CanEnableManager(kCPManagerExploration, 6))
                theOther.push_back(7);
            break;
        }
        // Preserve the original guard: other requests are discarded when no priority request exists.
        if (thePriority.size() > 0 && theOther.size() > 0) {
            EEAIRandomShuffle(theOther.begin(), theOther.end());
            thePriority.insert(thePriority.end(), theOther.begin(), theOther.end());
        }
    }

    for (unsigned long n = 0, theSize = thePriority.size(); n < theSize; n++) {
        if (n >= theLimit)
            break;
        switch (thePriority[n]) {
        case 0: this->mCivilizationManager->mEpochAdvanceEnabled = true; break;
        case 1: this->mCivilizationManager->mResourceManager->mSettlementsEnabled = true; break;
        case 2: this->mCivilizationManager->mResearchManager->mEnabled = true; break;
        case 3: this->mCivilizationManager->mUnitManager->mProductionEnabled = true; break;
        case 4: this->mCivilizationManager->mBuildingCountsEnabled = true; break;
        case 6: this->mCombatManager->mDefenseCommander->mEnabled = true; break;
        case 7: this->mExplorationManager->mEnabled = true; break;
        case 8: this->mCombatManager->mImprovementsEnabled = true; break;
        case 9: this->mCivilizationManager->mMoraleBuildingsEnabled = true; break;
        }
    }
}

// FUNCTION: EXE 0x42db35
// Detaches a unit from AI control: marks it, removes it from the combat manager and sets its AI state
void EEComputerPlayer::DetachUnitFromAI(EEComplexUnit* inUnit)
{
    if (inUnit->mIsDetachedFromAI != true)
        inUnit->mIsDetachedFromAI = true;
    this->mCombatManager->UnitRemoved(inUnit);
    if (inUnit->mAIState != 290008)
        inUnit->mAIState = 290008;
}

// FUNCTION: EXE 0x42db6d
// Groups the enabled unit types that pass the screener by AI kind into the map, allocating new type lists
void EEComputerPlayer::GetScreenedTypesByLine(std::map<long, std::vector<long>*>& ioMap, EEAIFOUnitSelectionScreener& inScreener)
{
    long i;
    EEUnitClass* theClass;
    long theValue;
    long theType;
    for (i = 0; i < 0x367; i++) {
        theClass = gUnitClassTable[i];
        if (theClass != NULL && theClass->mIsEnabled && inScreener.Screen(theClass)) {
            theValue = theClass->mAIKind;
            std::map<long, std::vector<long>*>::iterator it = ioMap.find(theValue);
            if (it != ioMap.end()) {
                theType = theClass->mUnitType;
                it->second->push_back(theType);
            } else if (theClass->mAIKind != kAIKindNone) {
                std::vector<long>* theList = new std::vector<long>;
                theValue = theClass->mUnitType;     // the key's variable again (the original's one [ebp+8] slot)
                theList->push_back(theValue);
                ioMap.insert(std::make_pair(theClass->mAIKind, theList));
            }
        }
    }
}

// FUNCTION: EXE 0x42dc51
// Returns true for epoch technologies; otherwise asks the player and refuses a missing node or one of kind 5
bool EEComputerPlayer::CanProduce(long inTechnology, long inUnitClass, long inCount, bool inCheckCost, long inWonderMode, UWideString* outReason)
{
    bool theResult = true;
    if (inTechnology < 0 || inTechnology >= 15) {
        theResult = EEPlayer::CanProduce(inTechnology, inUnitClass, inCount, inCheckCost, inWonderMode, outReason);
        if (theResult) {
            EETechTreeNode* theNode = this->mTechnologies->Get(inTechnology);
            if (theNode != NULL && theNode->mData->mKind != 5)
                theResult = true;
            else
                theResult = false;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x42dca6
// Tells the combat manager and then the player about a target player notice
void EEComputerPlayer::NoticeTargetPlayer(EEPlayer* inPlayer, bool inFlag)
{
    this->mCombatManager->TargetPlayerNotice(inPlayer, inFlag);
    EEPlayer::NoticeTargetPlayer(inPlayer, inFlag);
}

// FUNCTION: EXE 0x42dccf
// Picks a random air unit type of the current epoch as the favorite and records it in the history
// Shape shared by all four ChooseFavorite*Type: the iterator default-constructed after the map, the epoch loaded
// into a local before the screener, the index drawn before begin(), the inner random index in a local, make_pair
// for the history entry
void EEComputerPlayer::ChooseFavoriteAirType()
{
    std::map<long, std::vector<long>*> theClasses;
    std::map<long, std::vector<long>*>::iterator it;     // default-constructed after the map (_Ptr = 0)
    long theEpoch = this->mTechnologies->mCurrentEpoch;  // loaded before the screener's pushes
    this->GetScreenedTypesByLine(theClasses, EEAIFOFavoriteUnitScreener(this, true, false, false, false, theEpoch, 0));
    if (theClasses.size() > 0) {
        long theIndex = gPhysics->GetRandom(0L, (long)theClasses.size() - 1);
        it = theClasses.begin();
        for (long i = 0; i < theIndex; i++)
            ++it;
        long theRandom = gPhysics->GetRandom(0L, (long)it->second->size() - 1);
        this->mFavoriteAirType = (*it->second)[theRandom];
        for (it = theClasses.begin(); it != theClasses.end(); ++it)
            delete it->second;
    }
    this->mFavoriteTypeHistory.push_back(std::make_pair(this->mFavoriteAirType, this->mTechnologies->mCurrentEpoch));
}

// FUNCTION: EXE 0x42ddfb
// Creates a favorite-unit screener for the allowed movement kinds, with the epoch clamped to 0..14
EEAIFOFavoriteUnitScreener::EEAIFOFavoriteUnitScreener(EEComputerPlayer* inPlayer, bool inAllowAir, bool inAllowLand, bool inAllowSea,
    bool inAllowSpace, long inEpoch, long inEpochSpan)
    : EEAIFOUnitSelectionScreener(), mPlayer(inPlayer), mEpochSpan(inEpochSpan), mAllowAir(inAllowAir), mAllowSea(inAllowSea), mAllowLand(inAllowLand), mAllowSpace(inAllowSpace)
{
    if (inEpoch >= 0 && inEpoch < 15)
        this->mEpoch = inEpoch;
    else
        this->mEpoch = 0;
}

// FUNCTION: EXE 0x42de43
// Base unit screener check: asserts and refuses every class (EEAIFOUnitSelectionScreener::Screen)
// Emitted here: inline in EEAIFunctionObjects.h; slot EEAIFOUnitSelectionScreener+0x4

// FUNCTION: EXE 0x42de60
// Picks a random land unit type of the current epoch as the favorite and records it in the history
void EEComputerPlayer::ChooseFavoriteLandType()
{
    std::map<long, std::vector<long>*> theClasses;
    std::map<long, std::vector<long>*>::iterator it;     // default-constructed after the map (_Ptr = 0)
    long theEpoch = this->mTechnologies->mCurrentEpoch;  // loaded before the screener's pushes
    this->GetScreenedTypesByLine(theClasses, EEAIFOFavoriteUnitScreener(this, false, true, false, false, theEpoch, 0));
    if (theClasses.size() > 0) {
        long theIndex = gPhysics->GetRandom(0L, (long)theClasses.size() - 1);
        it = theClasses.begin();
        for (long i = 0; i < theIndex; i++)
            ++it;
        long theRandom = gPhysics->GetRandom(0L, (long)it->second->size() - 1);
        this->mFavoriteLandType = (*it->second)[theRandom];
        for (it = theClasses.begin(); it != theClasses.end(); ++it)
            delete it->second;
    }
    this->mFavoriteTypeHistory.push_back(std::make_pair(this->mFavoriteLandType, this->mTechnologies->mCurrentEpoch));
}

// FUNCTION: EXE 0x42df8c
// Picks a random sea unit type of the current epoch as the favorite and records it in the history
void EEComputerPlayer::ChooseFavoriteSeaType()
{
    std::map<long, std::vector<long>*> theClasses;
    std::map<long, std::vector<long>*>::iterator it;     // default-constructed after the map (_Ptr = 0)
    long theEpoch = this->mTechnologies->mCurrentEpoch;  // loaded before the screener's pushes
    this->GetScreenedTypesByLine(theClasses, EEAIFOFavoriteUnitScreener(this, false, false, true, false, theEpoch, 0));
    if (theClasses.size() > 0) {
        long theIndex = gPhysics->GetRandom(0L, (long)theClasses.size() - 1);
        it = theClasses.begin();
        for (long i = 0; i < theIndex; i++)
            ++it;
        long theRandom = gPhysics->GetRandom(0L, (long)it->second->size() - 1);
        this->mFavoriteSeaType = (*it->second)[theRandom];
        for (it = theClasses.begin(); it != theClasses.end(); ++it)
            delete it->second;
    }
    this->mFavoriteTypeHistory.push_back(std::make_pair(this->mFavoriteSeaType, this->mTechnologies->mCurrentEpoch));
}

// FUNCTION: EXE 0x42e0b8
// Picks a random space unit type of the current epoch as the favorite and records it in the history
void EEComputerPlayer::ChooseFavoriteSpaceType()
{
    std::map<long, std::vector<long>*> theClasses;
    std::map<long, std::vector<long>*>::iterator it;     // default-constructed after the map (_Ptr = 0)
    long theEpoch = this->mTechnologies->mCurrentEpoch;  // loaded before the screener's pushes
    this->GetScreenedTypesByLine(theClasses, EEAIFOFavoriteUnitScreener(this, false, false, false, true, theEpoch, 0));
    if (theClasses.size() > 0) {
        long theIndex = gPhysics->GetRandom(0L, (long)theClasses.size() - 1);
        it = theClasses.begin();
        for (long i = 0; i < theIndex; i++)
            ++it;
        long theRandom = gPhysics->GetRandom(0L, (long)it->second->size() - 1);
        this->mFavoriteSpaceType = (*it->second)[theRandom];
        for (it = theClasses.begin(); it != theClasses.end(); ++it)
            delete it->second;
    }
    this->mFavoriteTypeHistory.push_back(std::make_pair(this->mFavoriteSpaceType, this->mTechnologies->mCurrentEpoch));
}

// FUNCTION: EXE 0x42e1e4
// Forwards to the player's handler for this slot
void EEComputerPlayer::Virtual60()
{
    EEPlayer::Virtual60();
}

// FUNCTION: EXE 0x42e1e9
// Handles a finished unit; a land building is offered as drop site to every resource center and rebalances production
void EEComputerPlayer::UnitCompleted(EEComplexUnit* inUnit)
{
    EEPlayer::UnitCompleted(inUnit);
    if (inUnit->mUnitClass->IsLandBuilding()) {
        unsigned long i, n;
        for (i = 0, n = this->mResourceCenters.size(); i < n; i++)
            this->mResourceCenters[i]->AdoptDropSite(inUnit);
        this->mCivilizationManager->mUnitManager->BalanceProduction(inUnit);
    }
}

// FUNCTION: EXE 0x42e248
// Counts the AI's heroes plus hero requests via the unit manager, falling back to the player's count
long EEComputerPlayer::GetHeroCount(bool inByClass, bool inRequests)
{
    EECPUGatherHeroesCount theGatherer(this, inByClass, inRequests);
    if (this->mCivilizationManager != NULL && this->mCivilizationManager->mUnitManager != NULL) {
        this->mCivilizationManager->mUnitManager->GatherUnits(theGatherer);
        return theGatherer.mUnits + theGatherer.mRequested;
    }
    return EEPlayer::GetHeroCount(inByClass, inRequests);
}

// FUNCTION: EXE 0x42e2a3
// Rebuilds the resource centers around nearby nature resources, sorts them by distance from the town center,
// puts the town center first, activates the ones in range (and connected on water maps) and sets fishing use
void EEComputerPlayer::Regroup()
{
    std::vector<EEUnit*> theUnits;
    U2DPoint<long> theTown;
    EETileCoordinate theCenter;
    theCenter.mX = 0;
    theCenter.mY = 0;
    std::vector<EEUnit*> theUnused;
    long theRange = this->mDropOffDistance;
    EEMap* theMap = gWorld.mMap;
    long theCount = 0;
    bool theWaterMap;
    if (theMap != NULL && ((theMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap || (theMap->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap))
        theWaterMap = true;
    else
        theWaterMap = false;
    theTown.mX = this->mCivilizationManager->mBuildManager->mTownCenterTile.mX;
    theTown.mY = this->mCivilizationManager->mBuildManager->mTownCenterTile.mY;
    if (gWorld.mPlayers[0] != NULL && theTown.mX != -10000 && theTown.mY != -10000) {
        this->GetObjectsToGroup(theUnits, theCount);
        unsigned long theNumUnits;
        unsigned long i;
        for (i = 0, theNumUnits = theUnits.size(); i < theNumUnits; i++) {
            EEUnit* theUnit = theUnits[i];
            bool theFound = false;
            unsigned long theNumCenters;
            unsigned long j;
            for (j = 0, theNumCenters = this->mResourceCenters.size(); j < theNumCenters && !theFound; j++) {
                if (this->mResourceCenters[j]->IsWithinRadius(theUnit)) {
                    this->mResourceCenters[j]->AddUnit(theUnit);
                    theFound = true;
                }
            }
            if (!theFound) {
                EECPResourceCenter* theNewCenter = new EECPResourceCenter(this, theUnit);
                this->mResourceCenters.push_back(theNewCenter);
            }
        }
        std::sort(this->mResourceCenters.begin(), this->mResourceCenters.end(), EECPCenterDistanceLess(theTown));
        std::deque<EEComplexUnit*>* theTownCenters = this->mCivilizationManager->mUnitManager->GetUnits(0x29);
        if (theTownCenters != NULL && theTownCenters->size() != 0) {
            EEComplexUnit* theTownCenter = *theTownCenters->begin();
            if (theTownCenter != NULL) {
                bool theNew = this->mResourceCenters.size() == 0;
                if (!theNew) {
                    this->mResourceCenters[0]->AdoptDropSite(theTownCenter);
                    theNew = this->mResourceCenters[0]->mUnit != theTownCenter;
                }
                if (theNew) {
                    EECPResourceCenter* theNewCenter = new EECPResourceCenter(this, NULL);
                    theNewCenter->SetUnit(theTownCenter);
                    this->mResourceCenters.insert(this->mResourceCenters.begin(), theNewCenter);
                }
                this->mCivilizationManager->mWallManager->AddGates(theTownCenter);
            }
        }
        unsigned long theNumCenters;
        unsigned long k;
        long theRangeSquared = theRange * theRange;
        for (k = 0, theNumCenters = this->mResourceCenters.size(); k < theNumCenters; k++) {
            this->mResourceCenters[k]->GetCenter(theCenter);
            if (EETileDistanceLess(theTown).DistanceSquared(theCenter) < theRangeSquared * 36
                && (!theWaterMap || gContinentManager.AreConnected(gContinentManager.GetContinentAt(theCenter.mX, theCenter.mY),
                    gContinentManager.GetContinentAt(theTown.mX, theTown.mY), this->GetCitizenPassability())))
                this->mResourceCenters[k]->SetActive(true, 0);
            else
                this->mResourceCenters[k]->SetActive(false, 0);
        }
        this->mUsesFishing = (unsigned long)theCount >= 10 && theMap != NULL && (theMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap;
    }
}


// FUNCTION: EXE 0x42e6ae
// Activates the first inactive resource center whose continent holds no enemy capitol (checked on water maps only)
void EEComputerPlayer::FlagGroup()
{
    std::vector<EEUnit*> theUnits;
    EETileCoordinate theCenter;
    theCenter.mX = 0;
    theCenter.mY = 0;
    bool theCheck;
    if (gWorld.mMap != NULL && ((gWorld.mMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap || (gWorld.mMap->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap))
        theCheck = true;
    else
        theCheck = false;
    bool theDone = false;
    for (unsigned long i = 0, n = this->mResourceCenters.size(); i < n; i++) {
        if (theDone)
            break;
        if (!this->mResourceCenters[i]->mIsActive) {
            this->mResourceCenters[i]->GetCenter(theCenter);
            bool theSafe = true;
            if (theCheck) {
                for (unsigned long j = 0; j < gWorld.mNumPlayers; j++) {
                    EEPlayer* thePlayer = gWorld.mPlayers[j];
                    if (this != thePlayer && thePlayer->mEnemies[this->mPlayerID] != 0) {
                        EEGetPlayerUnitsOfType(kUnitTypeCapitol, j, theUnits);
                        for (std::vector<EEUnit*>::iterator it = theUnits.begin(); it != theUnits.end() && theSafe; ++it) {
                            if (gContinentManager.GetContinentID(theCenter.mX, theCenter.mY)
                                == gContinentManager.GetContinentID(*it))
                                theSafe = false;
                        }
                    }
                }
            }
            if (theSafe) {
                this->mResourceCenters[i]->SetActive(true, 1);
                theDone = true;
            }
        }
    }
}

// FUNCTION: EXE 0x42e851
// Kills or queues the AI's highest-scoring units until the scorer's population count is met, within the
// citizen and transport quotas; returns the number of units chosen
long EEComputerPlayer::ChooseUnitsToKill(EEAIFOUnitsToKillScoringBase& inScorer, bool inKill)
{
    std::multimap<long, EEComplexUnit*, std::greater<long> > theScores;
    long theDone = 0;
    long theCount = 0;
    if (inScorer.mCount > 0 && !this->mIgnoreOthers) {
        for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            EEComplexUnit* theComplex = theUnit->mComplexUnit;
            if (theComplex != NULL && inScorer.CanKill(theComplex))
                theScores.insert(std::multimap<long, EEComplexUnit*, std::greater<long> >::value_type(inScorer.GetKillScore(theComplex), theComplex));
        }
        for (std::multimap<long, EEComplexUnit*, std::greater<long> >::iterator it = theScores.begin(); it != theScores.end() && theDone < inScorer.mCount;
             ++it) {
            if (it->second->mIsCitizen) {
                if (inScorer.mCitizensToKill <= 0)
                    continue;
                inScorer.mCitizensToKill--;
            } else if (it->second->mUnitClass->IsTransport()) {
                if (inScorer.mTransportsToKill <= 0)
                    continue;
                inScorer.mTransportsToKill--;
            }
            if (std::find(this->mUnitsToKill.begin(), this->mUnitsToKill.end(), it->second) == this->mUnitsToKill.end()) {
                theDone += it->second->mUnitClass->GetPopulationCost();
                if (inKill)
                    it->second->Kill(true, false, false);
                else {
                    it->second->Reference();
                    this->mUnitsToKill.push_back(it->second);
                }
                theCount++;
            }
        }
    }
    return theCount;
}

// FUNCTION: EXE 0x42e9e7
// Returns the most frequent path area id among the tiles around the unit (0 when none has one)
unsigned short EEComputerPlayer::GetObjectRegion(EEComplexUnit* inUnit)
{
    std::map<unsigned short, unsigned long>::iterator theBest;
    std::map<unsigned short, unsigned long> theCounts;
    std::vector<EETileCoordinate> theTiles;
    Fn2492C2(inUnit->mTileX, inUnit->mTileY, 1, theTiles);
    for (std::vector<EETileCoordinate>::iterator it = theTiles.begin(); it != theTiles.end(); ++it) {
        EEAIPathArea* theArea = gContinentManager.GetPathAreaAt(*it);
        unsigned short theID = theArea != NULL ? theArea->mID : 0;
        if (theID != 0)
            theCounts[theID] = theCounts.insert(std::map<unsigned short, unsigned long>::value_type(theID, 0)).first->second + 1;
    }
    // no tile around the unit has a path area: there is no first entry to read (the map head node holds no value)
    if (theCounts.empty())
        return 0;
    theBest = theCounts.begin();
    unsigned short theResult = theBest->first;
    unsigned long theMax = theBest->second;
    for (++theBest; theBest != theCounts.end(); ++theBest)
        if (theBest->second > theMax) {
            theResult = theBest->first;
            theMax = theBest->second;
        }
    return theResult;
}

// FUNCTION: EXE 0x42eace
// Returns a cost scaled by the difficulty's unit cost modifier when the AI cheats, never below 0
long EEComputerPlayer::ScaleCost(long inCost)
{
    long theResult = inCost;
    if (this->mIsCheating)
        theResult = UMFloatToLong(this->GetUnitCostModifier() * inCost);
    if (theResult < 0)
        theResult = 0;
    return theResult;
}

// FUNCTION: EXE 0x42eafc
// Returns the attack delay factor for easy or hard difficulty (percent / 100), else 1
float EEComputerPlayer::GetAttackDelayModifier()
{
    float theResult = 1.0f;
    if (this->mDifficulty == kDifficultyEasy)
        theResult = this->mAttackDelayModifierEasy * 0.01f;
    else if (this->mDifficulty == kDifficultyHard)
        theResult = this->mAttackDelayModifierHard * 0.01f;
    return theResult;
}

// FUNCTION: EXE 0x42eb26
// Returns the attack frequency factor for easy or hard difficulty (percent / 100), else 1
float EEComputerPlayer::GetAttackFrequencyModifier()
{
    float theResult = 1.0f;
    if (this->mDifficulty == kDifficultyEasy)
        theResult = this->mAttackFrequencyModifierEasy * 0.01f;
    else if (this->mDifficulty == kDifficultyHard)
        theResult = this->mAttackFrequencyModifierHard * 0.01f;
    return theResult;
}

// FUNCTION: EXE 0x42eb50
// Returns the attack group size factor for the AI's difficulty (percent / 100)
float EEComputerPlayer::GetAttackGroupModifier()
{
    float theResult;
    if (this->mDifficulty == kDifficultyEasy)
        theResult = this->mAttackGroupModifierEasy * 0.01f;
    else if (this->mDifficulty == kDifficultyHard)
        theResult = this->mAttackGroupModifierHard * 0.01f;
    else
        theResult = this->mAttackGroupModifierMedium * 0.01f;
    return theResult;
}

// FUNCTION: EXE 0x42eb7c
// Returns the unit cost factor for the AI's difficulty (percent / 100)
float EEComputerPlayer::GetUnitCostModifier()
{
    float theResult;
    if (this->mDifficulty == kDifficultyEasy)
        theResult = this->mUnitCostReductionEasy * 0.01f;
    else if (this->mDifficulty == kDifficultyHard)
        theResult = this->mUnitCostReductionHard * 0.01f;
    else
        theResult = this->mUnitCostMedium * 0.01f;
    return theResult;
}

// FUNCTION: EXE 0x42eba8
// Returns the deathmatch attack frequency factor for the AI's difficulty, else 1
float EEComputerPlayer::GetDMAttackFrequencyModifier()
{
    float theResult = 1.0f;
    if (this->mDifficulty == kDifficultyEasy)
        theResult = this->mDMAttackFrequencyModifierEasy;
    else if (this->mDifficulty == kDifficultyMedium)
        theResult = this->mDMAttackFrequencyModifierMedium;
    else if (this->mDifficulty == kDifficultyHard)
        theResult = this->mDMAttackFrequencyModifierHard;
    return theResult;
}

// FUNCTION: EXE 0x42ebd9
// Returns the deathmatch initial attack factor for the AI's difficulty, else 1
float EEComputerPlayer::GetDMInitialAttackModifier()
{
    float theResult = 1.0f;
    if (this->mDifficulty == kDifficultyEasy)
        theResult = this->mDMInitialAttackModifierEasy;
    else if (this->mDifficulty == kDifficultyMedium)
        theResult = this->mDMInitialAttackModifierMedium;
    else if (this->mDifficulty == kDifficultyHard)
        theResult = this->mDMInitialAttackModifierHard;
    return theResult;
}

// FUNCTION: EXE 0x42ec0a
// Returns the epoch advance factor: the deathmatch catch-up value, else easy/hard percent / 100, else 1
float EEComputerPlayer::GetEpochFrequencyModifier()
{
    float theResult = 1.0f;
    if (this->IsDeathmatch())
        theResult = this->mEpochCatchUpDeathmatch;
    else if (this->mDifficulty == kDifficultyEasy)
        theResult = this->mEpochFrequencyModifierEasy * 0.01f;
    else if (this->mDifficulty == kDifficultyHard)
        theResult = this->mEpochFrequencyModifierHard * 0.01f;
    return theResult;
}

// FUNCTION: EXE 0x42ec52
// Returns true for a deathmatch game (resource setting 4, not a scenario)
bool EEComputerPlayer::IsDeathmatch()
{
    bool theResult = false;
    if (gWorld.mGameOptions.mResources == 4)
        theResult = gWorld.mGameOptions.mIsScenarioGame == false;
    return theResult;
}

// FUNCTION: EXE 0x42ec67
// Returns true when the resource setting is 5 (EnableManagers then runs every manager)
bool EEComputerPlayer::Fn2EC67()
{
    return gWorld.mGameOptions.mResources == 5;
}

// FUNCTION: EXE 0x42ec74
// Returns the population cap factor for the AI's difficulty (percent / 100)
float EEComputerPlayer::GetPopLimitAdjustment()
{
    float theResult;
    if (this->mDifficulty == kDifficultyEasy)
        theResult = this->mPopLimitAdjustmentEasy * 0.01f;
    else if (this->mDifficulty == kDifficultyHard)
        theResult = this->mPopLimitAdjustmentHard * 0.01f;
    else
        theResult = this->mPopLimitAdjustmentMedium * 0.01f;
    return theResult;
}

// FUNCTION: EXE 0x42eca0
// Returns the research frequency factor for easy or hard difficulty (percent / 100), else 1
float EEComputerPlayer::GetResearchFrequencyModifier()
{
    float theResult = 1.0f;
    if (this->mDifficulty == kDifficultyEasy)
        theResult = this->mResearchFrequencyModifierEasy * 0.01f;
    else if (this->mDifficulty == kDifficultyHard)
        theResult = this->mResearchFrequencyModifierHard * 0.01f;
    return theResult;
}

// FUNCTION: EXE 0x42ecca
// Returns the AI's population cap, working it out first if unset and inactive; the player's limit when ignoring others
long EEComputerPlayer::GetPopulationLimit(long inArg)
{
    if (this->mIgnoreOthers)
        return EEPlayer::GetPopulationLimit(inArg);
    if (this->mPopulationCap == 0 && !this->mActive)
        this->WorkOutPopulation();
    return this->mPopulationCap;
}

// FUNCTION: EXE 0x42ed01
// Returns the island expansion interval for the AI's difficulty, else 60 seconds
unsigned long EEComputerPlayer::GetIslandExpansionTimer()
{
    unsigned long theResult = 60000;
    if (this->mDifficulty == kDifficultyEasy)
        theResult = this->mIslandExpansionTimerEasy;
    else if (this->mDifficulty == kDifficultyMedium)
        theResult = this->mIslandExpansionTimerMedium;
    else if (this->mDifficulty == kDifficultyHard)
        theResult = this->mIslandExpansionTimerHard;
    return theResult;
}

// FUNCTION: EXE 0x42ed2f
// Returns true when the AI may attack and its next attack time has come
bool EEComputerPlayer::IsAttackDue(unsigned long inTime)
{
    return this->mCanAttack && inTime >= this->mNextAttackTime;
}

// FUNCTION: EXE 0x42ed4b
// Returns true on an island or planets map once the next island expansion time has come
bool EEComputerPlayer::IsIslandExpansionDue()
{
    return gWorld.mMap != NULL && ((gWorld.mMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap || (gWorld.mMap->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap)
        && gPhysics->mGameTime >= this->mNextIslandExpansionTime;
}

// FUNCTION: EXE 0x42ed85
// Appends the enabled, currently buildable unit types of a category to outTypes; returns its size
unsigned long EEComputerPlayer::GetBuildableTypes(long inCategory, std::vector<long>& outTypes)
{
    EEAIFOCurrentlyBuildableUnitScreener theScreener(this);
    for (long i = 0; i < 0x367; i++) {
        EEUnitClass* theClass = gUnitClassTable[i];
        if (theClass != NULL && theClass->mCategory == inCategory && theClass->mIsEnabled && theScreener.Screen(theClass)) {
            long theType = theClass->mUnitType;
            outTypes.push_back(theType);
        }
    }
    return outTypes.size();
}

// FUNCTION: EXE 0x42edff
// Collects nature's stone, gold, iron and forage resources reachable from the town center (all on water maps)
// into outUnits and counts the fish
// NEAR: the original passes one tile local to GetPathAreaAt (U2DPoint) and GetContinentAt (EETileCoordinate)
void EEComputerPlayer::GetObjectsToGroup(std::vector<EEUnit*>& outUnits, long& outCount)
{
    EEPlayer* theNature = gWorld.mPlayers[0];
    EETileCoordinate theTile;
    theTile.mX = 0;
    theTile.mY = 0;
    bool theAnyContinent;
    if (gWorld.GetMap() != NULL && ((gWorld.GetMap()->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap || (gWorld.GetMap()->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap))
        theAnyContinent = true;
    else
        theAnyContinent = false;
    theTile.mX = this->mCivilizationManager->mBuildManager->mTownCenterTile.GetX();
    theTile.mY = this->mCivilizationManager->mBuildManager->mTownCenterTile.GetY();
    outUnits.clear();
    outCount = 0;
    for (EEUnit* theUnit = theNature->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (theUnit->mResourceAmount > 0) {
            if (theUnit->mResourceType == kResourceStone || theUnit->mResourceType == kResourceGold || theUnit->mResourceType == kResourceIron || theUnit->GetUnitClass()->mUnitType == kUnitTypeResForageBush) {
                if (theAnyContinent)
                    outUnits.push_back(theUnit);
                else {
                    unsigned short theID = 0;
                    EEAIPathArea* theArea = gContinentManager.GetPathAreaAt(EETileLocation(theTile.GetX(), theTile.GetY()));
                    if (theArea != NULL)
                        theID = theArea->mID;
                    if (theID == this->GetObjectRegion(static_cast<EEComplexUnit*>(theUnit))
                        || gContinentManager.AreConnected(gContinentManager.GetContinentAt(theTile),
                            gContinentManager.GetContinentAt(theUnit), this->GetCitizenPassability()))
                        outUnits.push_back(theUnit);
                }
            } else if (theUnit->GetUnitClass()->mUnitType == kUnitTypeResFish)
                outCount++;
        }
    }
}

// FUNCTION: EXE 0x42ef32
// Returns 50 for unit classes 0x20 and 0x1e0 while a garrison or fortress is planned, else 1
long EEComputerPlayer::GetExtraPopulation(long inUnitClass)
{
    long theResult = 1;
    if ((inUnitClass == 0x20 || inUnitClass == 0x1e0)
        && (this->mCivilizationManager->mGarrison != NULL || this->mCivilizationManager->mNextFortressTime != 0))
        theResult = 50;
    return theResult;
}

// FUNCTION: EXE 0x42ef5e
// Returns the resource center at an index
EECPResourceCenter* EEComputerPlayer::GetResourceCenter(unsigned long inIndex)
{
    return this->mResourceCenters[inIndex];
}

// FUNCTION: EXE 0x42ef6e
// Returns the resource center holding the unit, or NULL
EECPResourceCenter* EEComputerPlayer::GetResourceCenterOf(EEUnit* inUnit)
{
    std::vector<EEUnit*> theUnits;
    EECPResourceCenter* theResult = NULL;
    unsigned long i = 0;
    unsigned long theCount = this->mResourceCenters.size();
    for (; i < theCount; i++) {
        EECPResourceCenter* theCenter = this->mResourceCenters[i];
        theUnits = theCenter->mUnits;
        std::vector<EEUnit*>::iterator theFound = std::find(theUnits.begin(), theUnits.end(), inUnit);
        if (theFound != theUnits.end()) {
            theResult = theCenter;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x42f033
// Returns true for epoch technologies; otherwise asks the player whether it can afford the cost
bool EEComputerPlayer::CanAfford(EETechnology* inTechnology, long inUnitType, long inCount)
{
    if (this->mTechnologies != NULL && inTechnology->mData->mID >= kTechEpochPrehistoric && inTechnology->mData->mID < kTechEpochAll)
        return true;
    return EEPlayer::CanAfford(inTechnology, inUnitType, inCount);
}

// FUNCTION: EXE 0x42f064
// When another player is in a later epoch, pulls the AI's next epoch advance forward to a random catch-up time
void EEComputerPlayer::CatchUpEpoch(EEPlayer* inPlayer)
{
    unsigned long theNext = this->mCivilizationManager->mNextEpochAdvanceTime;
    unsigned long theNow = gPhysics->mGameTime;
    long theMinimum;
    long theMaximum;
    if (inPlayer->mTechnologies->mCurrentEpoch == kTechEpochAtomicWW1) {
        theMinimum = 60000;
        theMaximum = 120000;
    } else {
        theMaximum = this->mEpochCatchUpMax * 60000;
        theMinimum = this->mEpochCatchUpMin * 60000;
        if (this->IsDeathmatch())
            theMaximum = UMFloatToLong(theMaximum * this->mEpochCatchUpDeathmatch);
        else if (this->mDifficulty == kDifficultyEasy)
            theMaximum = UMFloatToLong(theMaximum * this->mEpochCatchUpEasy);
        else if (this->mDifficulty == kDifficultyHard)
            theMaximum = UMFloatToLong(theMaximum * this->mEpochCatchUpHard);
    }
    unsigned long theTime = gPhysics->GetRandom(theMinimum, theMaximum) + theNow;
    if (inPlayer->mTechnologies->mCurrentEpoch > this->mTechnologies->mCurrentEpoch && (theNext < theNow || theNext > theTime))
        this->mCivilizationManager->SetNextEpochAdvanceTime(theTime, true);
}

// FUNCTION: EXE 0x42f16d
// Records that the player took damage: the time, and whether a land building or a citizen was hit
void EEComputerPlayer::DamageTaken(long inPlayer, long inAmount, EEUnit* inUnit)
{
    long theCategory = inUnit->mUnitClass->mCategory;
    EEPlayer::DamageTaken(inPlayer, inAmount, inUnit);
    this->mLastDamagedTime = gPhysics->mGameTime;
    if (inUnit->mUnitClass->IsLandBuilding())
        this->mBuildingDamaged = true;
    if (theCategory == kUnitCategoryNavalCitizen || theCategory == kUnitCategoryLandCitizen)
        this->mCitizenDamaged = true;
}

// FUNCTION: EXE 0x42f1c8
// Initialises the computer player: difficulty, default favourite unit types, timers and flags, every tuning
// value from the AI behaviour records, then creates the exploration, civilization, combat and other managers
// The tuning members come from the computer player behaviour records (gCPBehaviors, db\dbcpbehavior.dat:
// 0x4e6118 + 0x38 * index). As in the original, both the attack-frequency and the attack-delay bounds read
// record 19 (Attack Frequency Boundaries); record 21 (Attack Frequency Boundary) is only set by scenario effects
void EEComputerPlayer::Init()
{
    this->mDifficulty = gWorld.mLobbyOptions.mDifficulty;
    this->mLBD4 = 0;
    this->mFavoriteLandType = 20;
    this->mFavoriteSeaType = 242;
    this->mFavoriteAirType = 72;
    this->mFavoriteSpaceType = 730;
    this->mSeed = this->mPlayerID % 10;
    this->mNextAttackTime = 0;
    this->mCanAttack = true;
    this->mIsStarted = false;
    this->mLF24 = 0;
    this->mLCC8 = 0;
    this->mLCCC = 0;
    this->mLCD0 = 0;
    this->mLCD4 = 0;
    this->mNextEnableTime = 0;
    this->mIsEnableTimeSet = false;
    this->mLastDamagedTime = 0;
    this->mBuildingDamaged = false;
    this->mCitizenDamaged = false;
    this->mActive = true;
    this->mIgnoreOthers = false;
    this->mNextPopulationTime = 0;
    this->mPopulationCap = 0;
    this->mNextExpansionTime = 0;
    this->mNextIslandExpansionTime = 0;
    this->mUsesFishing = false;
    this->mAssistAlliesDelayEasy = gCPBehaviors[kCPBehaviorTroopsToAssistAllies].mIntValue1 * 60000;
    this->mAssistAlliesDelayMedium = gCPBehaviors[kCPBehaviorTroopsToAssistAllies].mIntValue2 * 60000;
    this->mAssistAlliesDelayHard = (long)gCPBehaviors[kCPBehaviorTroopsToAssistAllies].mFloatValue * 60000;
    this->mNavalUnitWeight = gCPBehaviors[kCPBehaviorNavalUnitWeight].mFloatValue;
    this->mAirUnitWeight = gCPBehaviors[kCPBehaviorAirUnitWeight].mFloatValue;
    this->mMaxFlameThrowers = gCPBehaviors[kCPBehaviorDesiredNumberOfFlameThrowers].mIntValue1;
    this->mUnitCostMedium = gCPBehaviors[kCPBehaviorUnitCostReductionMedium].mFloatValue;
    this->mCarrierMaxPerGroup = gCPBehaviors[kCPBehaviorCarrierUsage].mIntValue1;
    this->mCarrierPopPercentage = gCPBehaviors[kCPBehaviorCarrierUsage].mFloatValue;
    this->mCarrierChance = gCPBehaviors[kCPBehaviorCarrierUsage].mIntValue2 * 0.01f;
    this->mNumAirExplorers = gCPBehaviors[kCPBehaviorNumberOfAirExplorers].mIntValue1;
    this->mComputerAggression = gCPBehaviors[kCPBehaviorComputerVsComputerAggression].mFloatValue;
    this->mAttackEnemyChance = gCPBehaviors[kCPBehaviorAttackEnemy].mIntValue1;
    this->mAttackToClaimResourceChance = gCPBehaviors[kCPBehaviorAttackToClaimResource].mIntValue1;
    this->mAttackForRevengeChance = gCPBehaviors[kCPBehaviorAttackForRevenge].mIntValue1;
    this->mAttackGroupSizeMax = gCPBehaviors[kCPBehaviorAttackGroupSize].mIntValue2;
    this->mAttackGroupSizeMin = gCPBehaviors[kCPBehaviorAttackGroupSize].mIntValue1;
    this->mAttackFrequencyPerEpoch = gCPBehaviors[kCPBehaviorAttackFrequencyPerEpoch].mFloatValue;
    this->mAttackFrequencyModifierEasy = gCPBehaviors[kCPBehaviorAttackFrequencyModifier].mIntValue1;
    this->mAttackFrequencyModifierHard = gCPBehaviors[kCPBehaviorAttackFrequencyModifier].mIntValue2;
    this->mAttackFrequencyCeiling = gCPBehaviors[kCPBehaviorAttackFrequencyBoundary].mIntValue2;
    this->mAttackFrequencyFloor = gCPBehaviors[kCPBehaviorAttackFrequencyBoundary].mIntValue1;
    this->mAttackFrequencyMax = gCPBehaviors[kCPBehaviorAttackFrequency].mIntValue2;
    this->mAttackFrequencyMin = gCPBehaviors[kCPBehaviorAttackFrequency].mIntValue1;
    this->mInitialAttackDelayPerEpoch = gCPBehaviors[kCPBehaviorInitialAttackDelayPerEpoch].mIntValue1;
    this->mAttackDelayModifierEasy = gCPBehaviors[kCPBehaviorAttackDelayModifier].mIntValue1;
    this->mAttackDelayModifierHard = gCPBehaviors[kCPBehaviorAttackDelayModifier].mIntValue2;
    this->mAttackDelayCeiling = gCPBehaviors[kCPBehaviorAttackFrequencyBoundaries].mIntValue2;
    this->mAttackDelayFloor = gCPBehaviors[kCPBehaviorAttackFrequencyBoundaries].mIntValue1;
    this->mInitialAttackDelayMax = gCPBehaviors[kCPBehaviorInitialAttackDelay].mIntValue2;
    this->mInitialAttackDelayMin = gCPBehaviors[kCPBehaviorInitialAttackDelay].mIntValue1;
    this->mSpecializedUnitPercentage = gCPBehaviors[kCPBehaviorSpecializedUnitPercentage].mFloatValue;
    this->mAttackGroupModifierEasy = gCPBehaviors[kCPBehaviorAttackGroupModifier].mIntValue1;
    this->mAttackGroupModifierMedium = gCPBehaviors[kCPBehaviorAttackGroupModifier].mIntValue2;
    this->mAttackGroupModifierHard = (long)gCPBehaviors[kCPBehaviorAttackGroupModifier].mFloatValue;
    this->mRPSSelectionNonIslands = gCPBehaviors[kCPBehaviorRPSSelectionNonIslands].mFloatValue;
    this->mRPSSelectionIslands = gCPBehaviors[kCPBehaviorRPSSelectionIslands].mFloatValue;
    this->mSpecializedUnitChance = gCPBehaviors[kCPBehaviorSpecializedUnitSelectionChance].mFloatValue;
    this->mBonzaiAttackLevel = gCPBehaviors[kCPBehaviorBonzaiAttackLevel].mIntValue1;
    this->mDMEarlyAttackTime = gCPBehaviors[kCPBehaviorDMEarlyAttack].mIntValue2 * 60000;
    this->mDMEarlyAttackScale = gCPBehaviors[kCPBehaviorDMEarlyAttack].mIntValue1 * 0.01f;
    this->mTownGuardPercentage = gCPBehaviors[kCPBehaviorTownGuardPercentage].mFloatValue;
    this->mLD78 = gCPBehaviors[17].mIntValue1;
    this->mLD7C = gCPBehaviors[16].mIntValue1;
    this->mCitizenPercentage = gCPBehaviors[kCPBehaviorCitizenPercentage].mFloatValue;
    this->mUnitCostReductionEasy = gCPBehaviors[kCPBehaviorUnitCostReductionEasyAndHard].mIntValue1;
    this->mUnitCostReductionHard = gCPBehaviors[kCPBehaviorUnitCostReductionEasyAndHard].mIntValue2;
    this->mDefenseGroupSizeMax = gCPBehaviors[kCPBehaviorDefenseGroupSizeModifierPercentage].mIntValue2;
    this->mDefenseGroupSizeMin = gCPBehaviors[kCPBehaviorDefenseGroupSizeModifierPercentage].mIntValue1;
    this->mEpochFrequencyMax = gCPBehaviors[kCPBehaviorEpochFrequency].mIntValue2;
    this->mEpochFrequencyMin = gCPBehaviors[kCPBehaviorEpochFrequency].mIntValue1;
    this->mEpochCatchUpMax = gCPBehaviors[kCPBehaviorEpochCatchUp].mIntValue2;
    this->mEpochCatchUpMin = gCPBehaviors[kCPBehaviorEpochCatchUp].mIntValue1;
    this->mEpochCatchUpEasy = gCPBehaviors[kCPBehaviorEpochCatchUpModifier].mIntValue1 * 0.01f;
    this->mEpochCatchUpHard = gCPBehaviors[kCPBehaviorEpochCatchUpModifier].mIntValue2 * 0.01f;
    this->mEpochCatchUpDeathmatch = gCPBehaviors[kCPBehaviorEpochCatchUpModifier].mFloatValue * 0.01f;
    this->mEpochFrequencyModifierEasy = gCPBehaviors[kCPBehaviorEpochFrequencyModifier].mIntValue1;
    this->mEpochFrequencyModifierHard = gCPBehaviors[kCPBehaviorEpochFrequencyModifier].mIntValue2;
    this->mExpansionTimerEasy = gCPBehaviors[kCPBehaviorExpansionTimer].mIntValue1;
    this->mExpansionTimerMedium = gCPBehaviors[kCPBehaviorExpansionTimer].mIntValue2;
    this->mExpansionTimerHard = (long)gCPBehaviors[kCPBehaviorExpansionTimer].mFloatValue;
    this->mGuardContinentPercentage = gCPBehaviors[kCPBehaviorGuardContinentPercentage].mFloatValue;
    this->mGuardChokepointAirPercentage = gCPBehaviors[kCPBehaviorGuardChokepointAirPercentage].mFloatValue;
    this->mGuardChokepointPercentage = gCPBehaviors[kCPBehaviorGuardChokepointPercentage].mFloatValue;
    this->mGuardHighGroundPercentage = gCPBehaviors[kCPBehaviorGuardHighGroundPercentage].mFloatValue;
    this->mFC38 = gCPBehaviors[79].mFloatValue;
    this->mFC3C = gCPBehaviors[83].mFloatValue;
    this->mFC40 = gCPBehaviors[80].mFloatValue;
    this->mFC44 = gCPBehaviors[74].mFloatValue;
    this->mFC48 = gCPBehaviors[84].mFloatValue;
    this->mFC4C = gCPBehaviors[81].mFloatValue;
    this->mFC50 = gCPBehaviors[75].mFloatValue;
    this->mFC54 = gCPBehaviors[85].mFloatValue;
    this->mFC58 = gCPBehaviors[76].mFloatValue;
    this->mGuardTownPercentage = gCPBehaviors[kCPBehaviorGuardTownPercentage].mFloatValue;
    this->mGuardTownLandPercentage = gCPBehaviors[kCPBehaviorGuardTownLandPercentage].mFloatValue;
    this->mGuardWaterwayPercentage = gCPBehaviors[kCPBehaviorGuardWaterwayPercentage].mFloatValue;
    this->mHeroMaxPerGroup = gCPBehaviors[kCPBehaviorHeroUsage].mIntValue1;
    this->mHeroPopPercentage = gCPBehaviors[kCPBehaviorHeroUsage].mFloatValue;
    this->mHeroChance = gCPBehaviors[kCPBehaviorHeroUsage].mIntValue2 * 0.01f;
    this->mNumImprovedUnitTypes = gCPBehaviors[kCPBehaviorUnitSelectedForImprovements].mIntValue1;
    this->mNumImprovedAttributes = gCPBehaviors[kCPBehaviorUnitAttributesSelectedForImprovements].mIntValue1;
    this->mReplenishDelayMax = gCPBehaviors[kCPBehaviorInitialDelayInReplenishmentDefenseUnits].mIntValue2;
    this->mReplenishDelayMin = gCPBehaviors[kCPBehaviorInitialDelayInReplenishmentDefenseUnits].mIntValue1;
    this->mNumLandExplorers = gCPBehaviors[kCPBehaviorNumberOfLandExplorers].mIntValue1;
    this->mNumChokePointsToGuard = gCPBehaviors[kCPBehaviorNumChokePointsToGuard].mIntValue1;
    this->mAntiAirMax = (long)gCPBehaviors[kCPBehaviorDesiredNumberOfAntiAirBuildings].mFloatValue;
    this->mAirportMax = (long)gCPBehaviors[kCPBehaviorDesiredNumberOfAirports].mFloatValue;
    this->mArcheryRangeMax = (long)gCPBehaviors[kCPBehaviorDesiredNumberOfArcheryRanges].mFloatValue;
    this->mBarracksMax = (long)gCPBehaviors[kCPBehaviorDesiredNumberOfBarracks].mFloatValue;
    this->mDockMax = (long)gCPBehaviors[kCPBehaviorDesiredNumberOfDocks].mFloatValue;
    this->mGranaryMax = (long)gCPBehaviors[kCPBehaviorDesiredNumberOfGranaries].mFloatValue;
    this->mHospitalMax = (long)gCPBehaviors[kCPBehaviorDesiredNumberOfHospitals].mFloatValue;
    this->mCyberFactoryMax = (long)gCPBehaviors[kCPBehaviorDesiredNumberOfCyberFactories].mFloatValue;
    this->mCyberLabMax = (long)gCPBehaviors[kCPBehaviorDesiredNumberOfCyberLaboratories].mFloatValue;
    this->mNavalYardMax = (long)gCPBehaviors[kCPBehaviorDesiredNumberOfNavalYards].mFloatValue;
    this->mUniversityMax = (long)gCPBehaviors[kCPBehaviorDesiredNumberOfUniversities].mFloatValue;
    this->mSiegeFactoryMax = (long)gCPBehaviors[kCPBehaviorDesiredNumberOfSiegeFactories].mFloatValue;
    this->mSpaceDockMax = (long)gCPBehaviors[kCPBehaviorDesiredNumberOfSpaceDocks].mFloatValue;
    this->mStableMax = (long)gCPBehaviors[kCPBehaviorDesiredNumberOfStables].mFloatValue;
    this->mTankFactoryMax = (long)gCPBehaviors[kCPBehaviorDesiredNumberOfTankFactories].mFloatValue;
    this->mTempleMax = (long)gCPBehaviors[kCPBehaviorDesiredNumberOfTemples].mFloatValue;
    this->mTowerMax = gCPBehaviors[kCPBehaviorDesiredNumberOfTowers].mIntValue2;
    this->mDefensiveUnitCap = gCPBehaviors[kCPBehaviorDefensiveUnitCap].mFloatValue;
    this->mSiegeWeaponPercentage = gCPBehaviors[kCPBehaviorSiegeWeaponPercentage].mFloatValue * 0.01f;
    this->mMaxAttackResponseRange = gCPBehaviors[kCPBehaviorMaximumAttackResponseRange].mFloatValue;
    this->mMinMilitaryBuildingsEasy = gCPBehaviors[kCPBehaviorEasyMinNumberMilitaryBuildings].mIntValue1;
    this->mMinMilitaryBuildingsEasyDM = gCPBehaviors[kCPBehaviorEasyMinNumberMilitaryBuildings].mIntValue2;
    this->mMinMilitaryBuildingsMedium = gCPBehaviors[kCPBehaviorMediumMinNumberMilitaryBuildings].mIntValue1;
    this->mMinMilitaryBuildingsMediumDM = gCPBehaviors[kCPBehaviorMediumMinNumberMilitaryBuildings].mIntValue2;
    this->mMinMilitaryBuildingsHard = gCPBehaviors[kCPBehaviorHardMinNumberMilitaryBuildings].mIntValue1;
    this->mMinMilitaryBuildingsHardDM = gCPBehaviors[kCPBehaviorHardMinNumberMilitaryBuildings].mIntValue2;
    this->mMilitaryPopPercentage = gCPBehaviors[kCPBehaviorNumberOfMilitaryUnitsToTrain].mFloatValue;
    this->mAidThreatThreshold = gCPBehaviors[kCPBehaviorAidThreatThreshold].mFloatValue;
    this->mPopLimitAdjustmentEasy = gCPBehaviors[kCPBehaviorPopulationLimitAdjustment].mIntValue1;
    this->mPopLimitAdjustmentMedium = gCPBehaviors[kCPBehaviorPopulationLimitAdjustment].mIntValue2;
    this->mPopLimitAdjustmentHard = (long)gCPBehaviors[kCPBehaviorPopulationLimitAdjustment].mFloatValue;
    this->mPriestMaxPerGroup = gCPBehaviors[kCPBehaviorPriestUsage].mIntValue1;
    this->mPriestPopPercentage = gCPBehaviors[kCPBehaviorPriestUsage].mFloatValue;
    this->mPriestChance = gCPBehaviors[kCPBehaviorPriestUsage].mIntValue2 * 0.01f;
    this->mProphetMaxPerGroup = gCPBehaviors[kCPBehaviorProphetUsage].mIntValue1;
    this->mProphetPopPercentage = gCPBehaviors[kCPBehaviorProphetUsage].mFloatValue;
    this->mProphetChance = gCPBehaviors[kCPBehaviorProphetUsage].mIntValue2 * 0.01f;
    this->mResearchFrequency = gCPBehaviors[kCPBehaviorResearchFrequency].mIntValue1;
    this->mResearchFrequencyModifierEasy = gCPBehaviors[kCPBehaviorResearchFrequencyModifier].mIntValue1;
    this->mResearchFrequencyModifierHard = gCPBehaviors[kCPBehaviorResearchFrequencyModifier].mIntValue2;
    this->mDropOffDistance = gCPBehaviors[kCPBehaviorResourceDropOffDistance].mIntValue1;
    this->mExtraResources = gCPBehaviors[kCPBehaviorComputerPlayerExtraResources].mIntValue1;
    this->mNumSeaExplorers = gCPBehaviors[kCPBehaviorNumberOfSeaExplorers].mIntValue1;
    this->mTownCenterUpgradeTimer = gCPBehaviors[kCPBehaviorTownCenterUpgradeTimer].mIntValue1;
    this->mMaxEnabledManagers = gCPBehaviors[kCPBehaviorMaxEnabledManagers].mIntValue1;
    this->mManagerAttackWait = gCPBehaviors[kCPBehaviorManagerAttackWait].mIntValue1;
    this->mManagerEpochAdvanceWait = gCPBehaviors[kCPBehaviorManagerEpochAdvanceWait].mIntValue1;
    this->mEnableStockProduction = gCPBehaviors[kCPBehaviorEnableStockProduction].mIntValue1;
    this->mEnableStockResource = gCPBehaviors[kCPBehaviorEnableStockResource].mIntValue1;
    this->mEnableStockCivilization = gCPBehaviors[kCPBehaviorEnableStockCivilization].mIntValue1;
    this->mEnableStockExploration = gCPBehaviors[kCPBehaviorEnableStockExploration].mIntValue1;
    this->mEnableStockDefense = gCPBehaviors[kCPBehaviorEnableStockDefense].mIntValue1;
    this->mEnableStockBuild = gCPBehaviors[kCPBehaviorEnableStockBuild].mIntValue1;
    this->mLE80 = gCPBehaviors[60].mIntValue1;
    this->mEnableStockResearch = gCPBehaviors[kCPBehaviorEnableStockResearch].mIntValue1;
    this->mEnableStockCombat = gCPBehaviors[kCPBehaviorEnableStockCombat].mIntValue1;
    this->mEnableInterval = gCPBehaviors[kCPBehaviorEnableInterval].mFloatValue;
    this->mMilitaryCountWeight = gCPBehaviors[kCPBehaviorCountWeights].mIntValue1 * 0.01f;
    this->mWorkerCountWeight = gCPBehaviors[kCPBehaviorCountWeights].mIntValue2 * 0.01f;
    this->mUnitsPerAntiAirBuilding = gCPBehaviors[kCPBehaviorDesiredNumberOfAntiAirBuildings].mIntValue1;
    this->mUnitsPerAirport = gCPBehaviors[kCPBehaviorDesiredNumberOfAirports].mIntValue1;
    this->mUnitsPerArcheryRange = gCPBehaviors[kCPBehaviorDesiredNumberOfArcheryRanges].mIntValue1;
    this->mUnitsPerBarracks = gCPBehaviors[kCPBehaviorDesiredNumberOfBarracks].mIntValue1;
    this->mUnitsPerDock = gCPBehaviors[kCPBehaviorDesiredNumberOfDocks].mIntValue1;
    this->mUnitsPerGranary = gCPBehaviors[kCPBehaviorDesiredNumberOfGranaries].mIntValue1;
    this->mUnitsPerHospital = gCPBehaviors[kCPBehaviorDesiredNumberOfHospitals].mIntValue1;
    this->mUnitsPerCyberFactory = gCPBehaviors[kCPBehaviorDesiredNumberOfCyberFactories].mIntValue1;
    this->mUnitsPerCyberLab = gCPBehaviors[kCPBehaviorDesiredNumberOfCyberLaboratories].mIntValue1;
    this->mUnitsPerNavalYard = gCPBehaviors[kCPBehaviorDesiredNumberOfNavalYards].mIntValue1;
    this->mUnitsPerUniversity = gCPBehaviors[kCPBehaviorDesiredNumberOfUniversities].mIntValue1;
    this->mUnitsPerSiegeFactory = gCPBehaviors[kCPBehaviorDesiredNumberOfSiegeFactories].mIntValue1;
    this->mUnitsPerSpaceDock = gCPBehaviors[kCPBehaviorDesiredNumberOfSpaceDocks].mIntValue1;
    this->mUnitsPerStable = gCPBehaviors[kCPBehaviorDesiredNumberOfStables].mIntValue1;
    this->mUnitsPerTankFactory = gCPBehaviors[kCPBehaviorDesiredNumberOfTankFactories].mIntValue1;
    this->mUnitsPerTemple = gCPBehaviors[kCPBehaviorDesiredNumberOfTemples].mIntValue1;
    this->mUnitsPerTower = gCPBehaviors[kCPBehaviorDesiredNumberOfTowers].mIntValue1;
    this->mCapitolUpgradeTimer = gCPBehaviors[kCPBehaviorCapitolUpgradeTimer].mIntValue1;
    this->mTowerSpacing = gCPBehaviors[kCPBehaviorDesiredNumberOfTowers].mFloatValue;
    this->mShipsToMaintainDMEasy = gCPBehaviors[kCPBehaviorShipsToMaintainDeathmatch].mIntValue1;
    this->mShipsToMaintainDMMedium = gCPBehaviors[kCPBehaviorShipsToMaintainDeathmatch].mIntValue2;
    this->mShipsToMaintainDMHard = (long)gCPBehaviors[kCPBehaviorShipsToMaintainDeathmatch].mFloatValue;
    this->mShipsToMaintainEasy = gCPBehaviors[kCPBehaviorShipsToMaintain].mIntValue1;
    this->mShipsToMaintainMedium = gCPBehaviors[kCPBehaviorShipsToMaintain].mIntValue2;
    this->mShipsToMaintainHard = (long)gCPBehaviors[kCPBehaviorShipsToMaintain].mFloatValue;
    this->mShipMaintenanceTimer = gCPBehaviors[kCPBehaviorMinimumShipMaintenanceTimer].mIntValue1;
    this->mDefensiveUnitUpdateEasy = gCPBehaviors[kCPBehaviorDefensiveUnitUpdate].mIntValue1 * 60000;
    this->mDefensiveUnitUpdateMedium = gCPBehaviors[kCPBehaviorDefensiveUnitUpdate].mIntValue2 * 60000;
    this->mDefensiveUnitUpdateHard = (long)gCPBehaviors[kCPBehaviorDefensiveUnitUpdate].mFloatValue * 60000;
    this->mDMDefensiveUnitUpdateEasy = gCPBehaviors[kCPBehaviorDefensiveUnitUpdateDeathmatch].mIntValue1 * 60000;
    this->mDMDefensiveUnitUpdateMedium = gCPBehaviors[kCPBehaviorDefensiveUnitUpdateDeathmatch].mIntValue2 * 60000;
    this->mDMDefensiveUnitUpdateHard = (long)gCPBehaviors[kCPBehaviorDefensiveUnitUpdateDeathmatch].mFloatValue * 60000;
    this->mTownGuardUpdateEasy = gCPBehaviors[kCPBehaviorTownGuardUpdate].mIntValue1 * 60000;
    this->mTownGuardUpdateMedium = gCPBehaviors[kCPBehaviorTownGuardUpdate].mIntValue2 * 60000;
    this->mTownGuardUpdateHard = (long)gCPBehaviors[kCPBehaviorTownGuardUpdate].mFloatValue * 60000;
    this->mDMTownGuardUpdateEasy = gCPBehaviors[kCPBehaviorTownGuardUpdateDeathmatch].mIntValue1 * 60000;
    this->mDMTownGuardUpdateMedium = gCPBehaviors[kCPBehaviorTownGuardUpdateDeathmatch].mIntValue2 * 60000;
    this->mDMTownGuardUpdateHard = (long)gCPBehaviors[kCPBehaviorTownGuardUpdateDeathmatch].mFloatValue * 60000;
    this->mIslandExpansionTimerEasy = gCPBehaviors[kCPBehaviorIslandExpansionTimer].mIntValue1;
    this->mIslandExpansionTimerMedium = gCPBehaviors[kCPBehaviorIslandExpansionTimer].mIntValue2;
    this->mIslandExpansionTimerHard = (long)gCPBehaviors[kCPBehaviorIslandExpansionTimer].mFloatValue;
    this->mDMAttackFrequencyModifierEasy = gCPBehaviors[kCPBehaviorDMAttackFrequencyModifier].mIntValue1 * 0.01f;
    this->mDMAttackFrequencyModifierMedium = gCPBehaviors[kCPBehaviorDMAttackFrequencyModifier].mIntValue2 * 0.01f;
    this->mDMAttackFrequencyModifierHard = gCPBehaviors[kCPBehaviorDMAttackFrequencyModifier].mFloatValue * 0.01f;
    this->mDMInitialAttackModifierEasy = gCPBehaviors[kCPBehaviorDMInitialAttackModifier].mIntValue1 * 0.01f;
    this->mDMInitialAttackModifierMedium = gCPBehaviors[kCPBehaviorDMInitialAttackModifier].mIntValue2 * 0.01f;
    this->mDMInitialAttackModifierHard = gCPBehaviors[kCPBehaviorDMInitialAttackModifier].mFloatValue * 0.01f;
    this->mLD50 = 1;
    this->mLD54 = 1;
    this->mLD48 = 1;
    this->mLD4C = 1;
    this->mLF14 = -1;
    this->mLF18 = -1;
    this->mLF1C = -1;
    this->mLF20 = -1;
    this->mDropOffDistance = UMFloatToLong((float)sqrt(this->mDropOffDistance));
    this->mCanBuild = true;
    this->mIsCheating = true;
    this->mCanDefend = true;
    this->mBF4E = true;
    this->mCanExplore = true;
    this->mCanGather = true;
    this->mIsCitizenPercentageSet = true;
    this->mReplaceOnLoad = false;
    this->mRegroupPending = false;
    this->mAttackStarted = false;
    this->mExplorationManager = new EECPExplorationManager(this);
    this->mCivilizationManager = new EECPCivilizationManager(this);
    this->mCombatManager = new EECPCombatManager(this);
    this->mInteractionManager = new EECPPlayerInteractionManager(this);
    this->mDiplomacyManager = new EECPDiplomacyManager(this);
}

// FUNCTION: EXE 0x42fd50
// Loads the computer player from a saved game, one block per save format: the base player, the combat,
// civilization and exploration managers, the resource centers and the tuning values; returns the first read error
// Built without /Og while the rest of the unit is -O1 (CLAUDE.md rule 4 evidence: 4,968 [ebp-N] references, every
// local memory-resident, args re-pushed from [ebp+N] per call, `leave`); the pragma takes the row from 1.29 to 50.66
// with no other row moving, and with it the function is exact. One version block per save format (the
// last one current). The local names are chosen for the no-/Og frame hash (docs/LEARNINGS.md): the original names
// are unknown. make_pair(0L, 0L) is the original's: two literal temporaries and the out-of-line make_pair<long, long>
// (0x440046), converted to pair<unsigned long, long> inline until the inline budget runs out (the last 3 blocks).
#pragma optimize("g", off)
long EEComputerPlayer::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult;
    long theObsoleteLong = 0;
    long theObsoleteLong2 = 0;
    float theOldFloat;
    EECPResourceCenter* theCenter;
    unsigned long theCount;
    unsigned long theIdx;
    theResult = EEPlayer::Load(inMajorVersion, inMinorVersion, inFile);
    if (inMajorVersion == 0 && inMinorVersion < 0xc6) {
        if (theResult == kErrorOK)
            theResult = this->mCombatManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = this->mCivilizationManager->Load(inMajorVersion, inMinorVersion, inFile);
        // the exploration manager's result no longer replaces an earlier error
        if (theResult == kErrorOK && inMinorVersion >= 0x20)
            theResult = this->mExplorationManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLBD4);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFavoriteLandType);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextEnableTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsEnableTimeSet);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBuildingDamaged);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCitizenDamaged);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsStarted);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextAttackTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mActive);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                // a resource center's read error is reported (it used to be discarded)
                for (theIdx = 0; theIdx < theCount && theResult == kErrorOK; theIdx++) {
                    theCenter = new EECPResourceCenter(this, NULL);
                    theResult = theCenter->Load(inMajorVersion, inMinorVersion, inFile);
                    this->mResourceCenters.push_back(theCenter);
                }
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUsesFishing);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumAirExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mComputerAggression);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackEnemyChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackToClaimResourceChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackForRevengeChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupSizeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupSizeMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyPerEpoch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyCeiling);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyFloor);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayPerEpoch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayCeiling);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayFloor);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSpecializedUnitPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theOldFloat);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLD78);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLD7C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCitizenPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theObsoleteLong);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theObsoleteLong2);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theObsoleteLong2);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefenseGroupSizeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefenseGroupSizeMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardContinentPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardChokepointAirPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardChokepointPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardHighGroundPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC38);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC3C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC40);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC44);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC48);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC4C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC50);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC54);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC58);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardTownPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardTownLandPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardWaterwayPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumImprovedUnitTypes);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumImprovedAttributes);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mReplenishDelayMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mReplenishDelayMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumLandExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumChokePointsToGuard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAirportMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mArcheryRangeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBarracksMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDockMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGranaryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHospitalMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCyberFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCyberLabMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNavalYardMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUniversityMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSiegeFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mStableMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTempleMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitCap);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxAttackResponseRange);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMilitaryPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theObsoleteLong);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAidThreatThreshold);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequency);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDropOffDistance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumSeaExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxEnabledManagers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mManagerAttackWait);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mManagerEpochAdvanceWait);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockProduction);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockResource);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockCivilization);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockExploration);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockDefense);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockBuild);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLE80);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockResearch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockCombat);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableInterval);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMilitaryCountWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mWorkerCountWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerAirport);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerArcheryRange);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerBarracks);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerDock);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerGranary);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerHospital);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerCyberFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerCyberLab);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerNavalYard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerUniversity);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerSiegeFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerStable);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerTemple);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanBuild);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsCheating);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanDefend);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBF4E);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanExplore);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanGather);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsCitizenPercentageSet);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanAttack);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIgnoreOthers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDifficulty);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostReductionEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostReductionHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequencyModifierHard);
        if (theResult == kErrorOK && inMinorVersion >= 0x8d)
            theResult = inFile.Read(this->mExpansionTimerEasy);
        if (theResult == kErrorOK && inMinorVersion >= 0x8d)
            theResult = inFile.Read(this->mNextExpansionTime);
        if (theResult == kErrorOK && inMinorVersion >= 0xb0)
            theResult = inFile.Read(this->mUnitsPerTankFactory);
        if (theResult == kErrorOK && inMinorVersion >= 0xb0)
            theResult = inFile.Read(this->mTankFactoryMax);
        if (theResult == kErrorOK && inMinorVersion >= 0xb0)
            theResult = inFile.Read(this->mMinMilitaryBuildingsEasy);
        if (theResult == kErrorOK && inMinorVersion >= 0xb0)
            theResult = inFile.Read(this->mMinMilitaryBuildingsEasyDM);
        if (theResult == kErrorOK && inMinorVersion >= 0xb0)
            theResult = inFile.Read(this->mMinMilitaryBuildingsMedium);
        if (theResult == kErrorOK && inMinorVersion >= 0xb0)
            theResult = inFile.Read(this->mMinMilitaryBuildingsMediumDM);
        if (theResult == kErrorOK && inMinorVersion >= 0xb0)
            theResult = inFile.Read(this->mMinMilitaryBuildingsHard);
        if (theResult == kErrorOK && inMinorVersion >= 0xb0)
            theResult = inFile.Read(this->mMinMilitaryBuildingsHardDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mRPSSelectionNonIslands);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mRPSSelectionIslands);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSpecializedUnitChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMEarlyAttackTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMEarlyAttackScale);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNavalUnitWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAirUnitWeight);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                this->mFavoriteTypeHistory.resize(theCount, std::make_pair(0L, 0L));
                for (theIdx = 0; theIdx < theCount; theIdx++) {
                    theResult = inFile.Read(this->mFavoriteTypeHistory[theIdx].first);
                    if (theResult == kErrorOK)
                        theResult = inFile.Read(this->mFavoriteTypeHistory[theIdx].second);
                }
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxFlameThrowers);
        if (theResult == kErrorOK && inMinorVersion >= 0xc4)
            theResult = inFile.Read(this->mAttackStarted);
    } else if (inMajorVersion == 0 && inMinorVersion < 0xca) {
        if (theResult == kErrorOK)
            theResult = this->mCombatManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = this->mCivilizationManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = this->mExplorationManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLBD4);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFavoriteLandType);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextEnableTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsEnableTimeSet);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBuildingDamaged);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCitizenDamaged);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsStarted);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextAttackTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mActive);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                // a resource center's read error is reported (it used to be discarded)
                for (theIdx = 0; theIdx < theCount && theResult == kErrorOK; theIdx++) {
                    theCenter = new EECPResourceCenter(this, NULL);
                    theResult = theCenter->Load(inMajorVersion, inMinorVersion, inFile);
                    this->mResourceCenters.push_back(theCenter);
                }
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUsesFishing);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumAirExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mComputerAggression);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackEnemyChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackToClaimResourceChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackForRevengeChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupSizeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupSizeMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyPerEpoch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyCeiling);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyFloor);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayPerEpoch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayCeiling);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayFloor);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSpecializedUnitPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theOldFloat);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLD78);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLD7C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCitizenPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theObsoleteLong);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefenseGroupSizeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefenseGroupSizeMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardContinentPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardChokepointAirPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardChokepointPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardHighGroundPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC38);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC3C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC40);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC44);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC48);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC4C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC50);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC54);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC58);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardTownPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardTownLandPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardWaterwayPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumImprovedUnitTypes);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumImprovedAttributes);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mReplenishDelayMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mReplenishDelayMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumLandExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumChokePointsToGuard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAirportMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mArcheryRangeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBarracksMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDockMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGranaryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHospitalMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCyberFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCyberLabMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNavalYardMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUniversityMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSiegeFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mStableMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTempleMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitCap);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxAttackResponseRange);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMilitaryPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theObsoleteLong);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAidThreatThreshold);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequency);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDropOffDistance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumSeaExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxEnabledManagers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mManagerAttackWait);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mManagerEpochAdvanceWait);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockProduction);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockResource);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockCivilization);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockExploration);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockDefense);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockBuild);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLE80);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockResearch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockCombat);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableInterval);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMilitaryCountWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mWorkerCountWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerAirport);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerArcheryRange);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerBarracks);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerDock);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerGranary);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerHospital);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerCyberFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerCyberLab);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerNavalYard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerUniversity);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerSiegeFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerStable);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerTemple);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanBuild);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsCheating);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanDefend);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBF4E);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanExplore);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanGather);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsCitizenPercentageSet);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanAttack);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIgnoreOthers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDifficulty);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostReductionEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostReductionHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mExpansionTimerEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextExpansionTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerTankFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTankFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsEasyDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsMediumDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsHardDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mRPSSelectionNonIslands);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mRPSSelectionIslands);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSpecializedUnitChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMEarlyAttackTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMEarlyAttackScale);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNavalUnitWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAirUnitWeight);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                this->mFavoriteTypeHistory.resize(theCount, std::make_pair(0L, 0L));
                for (theIdx = 0; theIdx < theCount; theIdx++) {
                    theResult = inFile.Read(this->mFavoriteTypeHistory[theIdx].first);
                    if (theResult == kErrorOK)
                        theResult = inFile.Read(this->mFavoriteTypeHistory[theIdx].second);
                }
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxFlameThrowers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackStarted);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateHard);
    } else if (inMajorVersion == 0 && inMinorVersion < 0xce) {
        if (theResult == kErrorOK)
            theResult = this->mCombatManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = this->mCivilizationManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = this->mExplorationManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLBD4);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFavoriteLandType);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextEnableTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsEnableTimeSet);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBuildingDamaged);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCitizenDamaged);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsStarted);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextAttackTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mActive);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                // a resource center's read error is reported (it used to be discarded)
                for (theIdx = 0; theIdx < theCount && theResult == kErrorOK; theIdx++) {
                    theCenter = new EECPResourceCenter(this, NULL);
                    theResult = theCenter->Load(inMajorVersion, inMinorVersion, inFile);
                    this->mResourceCenters.push_back(theCenter);
                }
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUsesFishing);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumAirExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mComputerAggression);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackEnemyChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackToClaimResourceChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackForRevengeChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupSizeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupSizeMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyPerEpoch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyCeiling);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyFloor);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayPerEpoch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayCeiling);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayFloor);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSpecializedUnitPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLD78);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLD7C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCitizenPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theObsoleteLong);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefenseGroupSizeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefenseGroupSizeMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardContinentPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardChokepointAirPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardChokepointPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardHighGroundPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC38);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC3C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC40);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC44);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC48);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC4C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC50);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC54);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC58);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardTownPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardTownLandPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardWaterwayPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumImprovedUnitTypes);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumImprovedAttributes);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mReplenishDelayMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mReplenishDelayMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumLandExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumChokePointsToGuard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAirportMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mArcheryRangeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBarracksMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDockMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGranaryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHospitalMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCyberFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCyberLabMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNavalYardMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUniversityMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSiegeFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mStableMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTempleMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitCap);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxAttackResponseRange);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMilitaryPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(theObsoleteLong);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAidThreatThreshold);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequency);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDropOffDistance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumSeaExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxEnabledManagers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mManagerAttackWait);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mManagerEpochAdvanceWait);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockProduction);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockResource);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockCivilization);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockExploration);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockDefense);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockBuild);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLE80);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockResearch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockCombat);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableInterval);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMilitaryCountWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mWorkerCountWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerAirport);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerArcheryRange);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerBarracks);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerDock);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerGranary);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerHospital);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerCyberFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerCyberLab);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerNavalYard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerUniversity);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerSiegeFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerStable);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerTemple);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanBuild);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsCheating);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanDefend);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBF4E);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanExplore);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanGather);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsCitizenPercentageSet);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanAttack);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIgnoreOthers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDifficulty);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostReductionEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostReductionHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mExpansionTimerEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextExpansionTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerTankFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTankFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsEasyDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsMediumDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsHardDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mRPSSelectionNonIslands);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mRPSSelectionIslands);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSpecializedUnitChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMEarlyAttackTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMEarlyAttackScale);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNavalUnitWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAirUnitWeight);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                this->mFavoriteTypeHistory.resize(theCount, std::make_pair(0L, 0L));
                for (theIdx = 0; theIdx < theCount; theIdx++) {
                    theResult = inFile.Read(this->mFavoriteTypeHistory[theIdx].first);
                    if (theResult == kErrorOK)
                        theResult = inFile.Read(this->mFavoriteTypeHistory[theIdx].second);
                }
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxFlameThrowers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackStarted);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAssistAlliesDelayEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAssistAlliesDelayMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAssistAlliesDelayHard);
    } else if (inMajorVersion == 0 && inMinorVersion < 0xd1) {
        if (theResult == kErrorOK)
            theResult = this->mCombatManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = this->mCivilizationManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = this->mExplorationManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLBD4);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFavoriteLandType);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextEnableTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsEnableTimeSet);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBuildingDamaged);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCitizenDamaged);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsStarted);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextAttackTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mActive);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                // a resource center's read error is reported (it used to be discarded)
                for (theIdx = 0; theIdx < theCount && theResult == kErrorOK; theIdx++) {
                    theCenter = new EECPResourceCenter(this, NULL);
                    theResult = theCenter->Load(inMajorVersion, inMinorVersion, inFile);
                    this->mResourceCenters.push_back(theCenter);
                }
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUsesFishing);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumAirExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mComputerAggression);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackEnemyChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackToClaimResourceChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackForRevengeChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupSizeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupSizeMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyPerEpoch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyCeiling);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyFloor);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayPerEpoch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayCeiling);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayFloor);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSpecializedUnitPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLD78);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLD7C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCitizenPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefenseGroupSizeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefenseGroupSizeMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardContinentPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardChokepointAirPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardChokepointPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardHighGroundPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC38);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC3C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC40);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC44);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC48);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC4C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC50);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC54);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC58);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardTownPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardTownLandPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardWaterwayPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumImprovedUnitTypes);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumImprovedAttributes);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mReplenishDelayMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mReplenishDelayMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumLandExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumChokePointsToGuard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAirportMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mArcheryRangeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBarracksMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDockMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGranaryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHospitalMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCyberFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCyberLabMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNavalYardMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUniversityMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSiegeFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mStableMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTempleMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitCap);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxAttackResponseRange);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMilitaryPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAidThreatThreshold);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequency);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDropOffDistance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumSeaExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxEnabledManagers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mManagerAttackWait);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mManagerEpochAdvanceWait);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockProduction);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockResource);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockCivilization);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockExploration);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockDefense);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockBuild);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLE80);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockResearch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockCombat);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableInterval);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMilitaryCountWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mWorkerCountWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerAirport);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerArcheryRange);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerBarracks);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerDock);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerGranary);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerHospital);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerCyberFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerCyberLab);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerNavalYard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerUniversity);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerSiegeFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerStable);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerTemple);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanBuild);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsCheating);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanDefend);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBF4E);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanExplore);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanGather);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsCitizenPercentageSet);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanAttack);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIgnoreOthers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDifficulty);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostReductionEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostReductionHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mExpansionTimerEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextExpansionTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerTankFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTankFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsEasyDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsMediumDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsHardDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mRPSSelectionNonIslands);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mRPSSelectionIslands);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSpecializedUnitChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMEarlyAttackTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMEarlyAttackScale);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNavalUnitWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAirUnitWeight);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                this->mFavoriteTypeHistory.resize(theCount, std::make_pair(0L, 0L));
                for (theIdx = 0; theIdx < theCount; theIdx++) {
                    theResult = inFile.Read(this->mFavoriteTypeHistory[theIdx].first);
                    if (theResult == kErrorOK)
                        theResult = inFile.Read(this->mFavoriteTypeHistory[theIdx].second);
                }
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxFlameThrowers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackStarted);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAssistAlliesDelayEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAssistAlliesDelayMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAssistAlliesDelayHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainDMEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainDMMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainDMHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipMaintenanceTimer);
    } else if (inMajorVersion == 0 && inMinorVersion < 0xd6) {
        if (theResult == kErrorOK)
            theResult = this->mCombatManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = this->mCivilizationManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = this->mExplorationManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLBD4);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFavoriteLandType);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextEnableTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsEnableTimeSet);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBuildingDamaged);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCitizenDamaged);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsStarted);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextAttackTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mActive);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                // a resource center's read error is reported (it used to be discarded)
                for (theIdx = 0; theIdx < theCount && theResult == kErrorOK; theIdx++) {
                    theCenter = new EECPResourceCenter(this, NULL);
                    theResult = theCenter->Load(inMajorVersion, inMinorVersion, inFile);
                    this->mResourceCenters.push_back(theCenter);
                }
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUsesFishing);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumAirExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mComputerAggression);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackEnemyChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackToClaimResourceChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackForRevengeChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupSizeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupSizeMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyPerEpoch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyCeiling);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyFloor);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayPerEpoch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayCeiling);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayFloor);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSpecializedUnitPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLD78);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLD7C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCitizenPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefenseGroupSizeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefenseGroupSizeMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardContinentPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardChokepointAirPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardChokepointPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardHighGroundPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC38);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC3C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC40);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC44);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC48);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC4C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC50);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC54);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC58);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardTownPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardTownLandPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardWaterwayPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumImprovedUnitTypes);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumImprovedAttributes);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mReplenishDelayMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mReplenishDelayMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumLandExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumChokePointsToGuard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAirportMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mArcheryRangeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBarracksMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDockMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGranaryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHospitalMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCyberFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCyberLabMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNavalYardMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUniversityMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSiegeFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mStableMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTempleMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitCap);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxAttackResponseRange);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMilitaryPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAidThreatThreshold);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequency);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDropOffDistance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumSeaExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxEnabledManagers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mManagerAttackWait);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mManagerEpochAdvanceWait);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockProduction);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockResource);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockCivilization);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockExploration);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockDefense);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockBuild);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLE80);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockResearch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockCombat);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableInterval);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMilitaryCountWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mWorkerCountWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerAirport);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerArcheryRange);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerBarracks);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerDock);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerGranary);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerHospital);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerCyberFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerCyberLab);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerNavalYard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerUniversity);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerSiegeFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerStable);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerTemple);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanBuild);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsCheating);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanDefend);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBF4E);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanExplore);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanGather);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsCitizenPercentageSet);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanAttack);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIgnoreOthers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDifficulty);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostReductionEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostReductionHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mExpansionTimerEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextExpansionTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerTankFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTankFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsEasyDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsMediumDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsHardDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mRPSSelectionNonIslands);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mRPSSelectionIslands);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSpecializedUnitChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMEarlyAttackTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMEarlyAttackScale);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNavalUnitWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAirUnitWeight);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                this->mFavoriteTypeHistory.resize(theCount, std::make_pair(0L, 0L));
                for (theIdx = 0; theIdx < theCount; theIdx++) {
                    theResult = inFile.Read(this->mFavoriteTypeHistory[theIdx].first);
                    if (theResult == kErrorOK)
                        theResult = inFile.Read(this->mFavoriteTypeHistory[theIdx].second);
                }
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxFlameThrowers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackStarted);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAssistAlliesDelayEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAssistAlliesDelayMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAssistAlliesDelayHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainDMEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainDMMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainDMHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipMaintenanceTimer);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSiegeWeaponPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBonzaiAttackLevel);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFavoriteSeaType);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFavoriteAirType);
    } else if (inMajorVersion == 0 && inMinorVersion < 0xe5) {
        if (theResult == kErrorOK)
            theResult = this->mCombatManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = this->mCivilizationManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = this->mExplorationManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLBD4);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFavoriteLandType);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextEnableTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsEnableTimeSet);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBuildingDamaged);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCitizenDamaged);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsStarted);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextAttackTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mActive);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                // a resource center's read error is reported (it used to be discarded)
                for (theIdx = 0; theIdx < theCount && theResult == kErrorOK; theIdx++) {
                    theCenter = new EECPResourceCenter(this, NULL);
                    theResult = theCenter->Load(inMajorVersion, inMinorVersion, inFile);
                    this->mResourceCenters.push_back(theCenter);
                }
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUsesFishing);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumAirExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mComputerAggression);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackEnemyChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackToClaimResourceChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackForRevengeChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupSizeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupSizeMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyPerEpoch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyCeiling);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyFloor);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayPerEpoch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayCeiling);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayFloor);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSpecializedUnitPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLD78);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLD7C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCitizenPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefenseGroupSizeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefenseGroupSizeMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardContinentPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardChokepointAirPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardChokepointPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardHighGroundPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC38);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC3C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC40);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC44);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC48);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC4C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC50);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC54);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC58);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardTownPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardTownLandPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardWaterwayPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumImprovedUnitTypes);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumImprovedAttributes);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mReplenishDelayMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mReplenishDelayMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumLandExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumChokePointsToGuard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAirportMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mArcheryRangeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBarracksMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDockMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGranaryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHospitalMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCyberFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCyberLabMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNavalYardMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUniversityMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSiegeFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mStableMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTempleMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitCap);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxAttackResponseRange);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMilitaryPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAidThreatThreshold);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequency);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDropOffDistance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumSeaExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxEnabledManagers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mManagerAttackWait);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mManagerEpochAdvanceWait);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockProduction);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockResource);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockCivilization);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockExploration);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockDefense);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockBuild);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLE80);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockResearch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockCombat);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableInterval);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMilitaryCountWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mWorkerCountWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerAirport);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerArcheryRange);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerBarracks);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerDock);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerGranary);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerHospital);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerCyberFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerCyberLab);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerNavalYard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerUniversity);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerSiegeFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerStable);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerTemple);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanBuild);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsCheating);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanDefend);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBF4E);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanExplore);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanGather);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsCitizenPercentageSet);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanAttack);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIgnoreOthers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDifficulty);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostReductionEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostReductionHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mExpansionTimerEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextExpansionTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerTankFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTankFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsEasyDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsMediumDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsHardDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mRPSSelectionNonIslands);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mRPSSelectionIslands);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSpecializedUnitChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMEarlyAttackTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMEarlyAttackScale);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNavalUnitWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAirUnitWeight);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                this->mFavoriteTypeHistory.resize(theCount, std::make_pair(0L, 0L));
                for (theIdx = 0; theIdx < theCount; theIdx++) {
                    theResult = inFile.Read(this->mFavoriteTypeHistory[theIdx].first);
                    if (theResult == kErrorOK)
                        theResult = inFile.Read(this->mFavoriteTypeHistory[theIdx].second);
                }
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxFlameThrowers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackStarted);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAssistAlliesDelayEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAssistAlliesDelayMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAssistAlliesDelayHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainDMEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainDMMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainDMHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipMaintenanceTimer);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSiegeWeaponPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBonzaiAttackLevel);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFavoriteSeaType);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFavoriteAirType);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextIslandExpansionTime);
    } else if (inMajorVersion == 0 && inMinorVersion < 0xe8) {
        if (theResult == kErrorOK)
            theResult = this->mCombatManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = this->mCivilizationManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = this->mExplorationManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLBD4);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFavoriteLandType);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextEnableTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsEnableTimeSet);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBuildingDamaged);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCitizenDamaged);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsStarted);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextAttackTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mActive);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                // a resource center's read error is reported (it used to be discarded)
                for (theIdx = 0; theIdx < theCount && theResult == kErrorOK; theIdx++) {
                    theCenter = new EECPResourceCenter(this, NULL);
                    theResult = theCenter->Load(inMajorVersion, inMinorVersion, inFile);
                    this->mResourceCenters.push_back(theCenter);
                }
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUsesFishing);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumAirExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mComputerAggression);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackEnemyChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackToClaimResourceChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackForRevengeChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupSizeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupSizeMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyPerEpoch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyCeiling);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyFloor);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayPerEpoch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayCeiling);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayFloor);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSpecializedUnitPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLD78);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLD7C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCitizenPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefenseGroupSizeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefenseGroupSizeMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardContinentPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardChokepointAirPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardChokepointPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardHighGroundPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC38);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC3C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC40);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC44);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC48);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC4C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC50);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC54);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC58);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardTownPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardTownLandPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardWaterwayPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumImprovedUnitTypes);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumImprovedAttributes);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mReplenishDelayMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mReplenishDelayMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumLandExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumChokePointsToGuard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAirportMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mArcheryRangeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBarracksMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDockMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGranaryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHospitalMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCyberFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCyberLabMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNavalYardMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUniversityMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSiegeFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mStableMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTempleMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitCap);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxAttackResponseRange);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMilitaryPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAidThreatThreshold);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequency);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDropOffDistance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumSeaExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxEnabledManagers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mManagerAttackWait);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mManagerEpochAdvanceWait);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockProduction);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockResource);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockCivilization);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockExploration);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockDefense);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockBuild);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLE80);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockResearch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockCombat);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableInterval);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMilitaryCountWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mWorkerCountWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerAirport);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerArcheryRange);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerBarracks);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerDock);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerGranary);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerHospital);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerCyberFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerCyberLab);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerNavalYard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerUniversity);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerSiegeFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerStable);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerTemple);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanBuild);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsCheating);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanDefend);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBF4E);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanExplore);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanGather);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsCitizenPercentageSet);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanAttack);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIgnoreOthers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDifficulty);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostReductionEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostReductionHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextExpansionTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerTankFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTankFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsEasyDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsMediumDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsHardDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mRPSSelectionNonIslands);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mRPSSelectionIslands);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSpecializedUnitChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMEarlyAttackTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMEarlyAttackScale);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNavalUnitWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAirUnitWeight);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                this->mFavoriteTypeHistory.resize(theCount, std::make_pair(0L, 0L));
                for (theIdx = 0; theIdx < theCount; theIdx++) {
                    theResult = inFile.Read(this->mFavoriteTypeHistory[theIdx].first);
                    if (theResult == kErrorOK)
                        theResult = inFile.Read(this->mFavoriteTypeHistory[theIdx].second);
                }
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxFlameThrowers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackStarted);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAssistAlliesDelayEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAssistAlliesDelayMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAssistAlliesDelayHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainDMEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainDMMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainDMHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipMaintenanceTimer);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSiegeWeaponPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBonzaiAttackLevel);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFavoriteSeaType);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFavoriteAirType);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextIslandExpansionTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mExpansionTimerEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mExpansionTimerMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mExpansionTimerHard);
    } else {
        if (theResult == kErrorOK)
            theResult = this->mCombatManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = this->mCivilizationManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = this->mExplorationManager->Load(inMajorVersion, inMinorVersion, inFile);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLBD4);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFavoriteLandType);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextEnableTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsEnableTimeSet);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBuildingDamaged);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCitizenDamaged);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsStarted);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextAttackTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mActive);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                // a resource center's read error is reported (it used to be discarded)
                for (theIdx = 0; theIdx < theCount && theResult == kErrorOK; theIdx++) {
                    theCenter = new EECPResourceCenter(this, NULL);
                    theResult = theCenter->Load(inMajorVersion, inMinorVersion, inFile);
                    this->mResourceCenters.push_back(theCenter);
                }
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUsesFishing);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCarrierChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumAirExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mComputerAggression);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackEnemyChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackToClaimResourceChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackForRevengeChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupSizeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupSizeMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyPerEpoch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyCeiling);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyFloor);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayPerEpoch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayCeiling);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayFloor);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mInitialAttackDelayMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSpecializedUnitPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLD78);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLD7C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCitizenPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefenseGroupSizeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefenseGroupSizeMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardContinentPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardChokepointAirPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardChokepointPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardHighGroundPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC38);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC3C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC40);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC44);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC48);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC4C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC50);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC54);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFC58);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardTownPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardTownLandPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGuardWaterwayPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHeroChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumImprovedUnitTypes);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumImprovedAttributes);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mReplenishDelayMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mReplenishDelayMin);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumLandExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumChokePointsToGuard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAirportMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mArcheryRangeMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBarracksMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDockMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mGranaryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mHospitalMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCyberFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCyberLabMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNavalYardMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUniversityMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSiegeFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mStableMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTempleMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitCap);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxAttackResponseRange);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMilitaryPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAidThreatThreshold);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPriestChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetMaxPerGroup);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetPopPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mProphetChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequency);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDropOffDistance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNumSeaExplorers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxEnabledManagers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mManagerAttackWait);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mManagerEpochAdvanceWait);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockProduction);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockResource);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockCivilization);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockExploration);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockDefense);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockBuild);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLE80);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockResearch);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableStockCombat);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEnableInterval);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMilitaryCountWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mWorkerCountWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerAirport);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerArcheryRange);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerBarracks);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerDock);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerGranary);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerHospital);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerCyberFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerCyberLab);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerNavalYard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerUniversity);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerSiegeFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerStable);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerTemple);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanBuild);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsCheating);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanDefend);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBF4E);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanExplore);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanGather);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIsCitizenPercentageSet);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mCanAttack);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mIgnoreOthers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDifficulty);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackDelayModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostReductionEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitCostReductionHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mEpochFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequencyModifierEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mResearchFrequencyModifierHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextExpansionTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerTankFactory);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTankFactoryMax);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsEasyDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsMediumDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMinMilitaryBuildingsHardDM);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mRPSSelectionNonIslands);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mRPSSelectionIslands);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSpecializedUnitChance);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMEarlyAttackTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMEarlyAttackScale);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNavalUnitWeight);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAirUnitWeight);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theCount);
            if (theResult == kErrorOK) {
                this->mFavoriteTypeHistory.resize(theCount, std::make_pair(0L, 0L));
                for (theIdx = 0; theIdx < theCount; theIdx++) {
                    theResult = inFile.Read(this->mFavoriteTypeHistory[theIdx].first);
                    if (theResult == kErrorOK)
                        theResult = inFile.Read(this->mFavoriteTypeHistory[theIdx].second);
                }
            }
        }
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mMaxFlameThrowers);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackStarted);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDefensiveUnitUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMDefensiveUnitUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mTownGuardUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mDMTownGuardUpdateHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mPopLimitAdjustmentMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAttackGroupModifierMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAssistAlliesDelayEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAssistAlliesDelayMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mAssistAlliesDelayHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainDMEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainDMMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainDMHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipsToMaintainHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mShipMaintenanceTimer);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSiegeWeaponPercentage);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mBonzaiAttackLevel);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFavoriteSeaType);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFavoriteAirType);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mNextIslandExpansionTime);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mExpansionTimerEasy);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mExpansionTimerMedium);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mExpansionTimerHard);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLF14);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLF1C);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLF18);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mLF20);
    }
    if (inMajorVersion > 0 || inMinorVersion >= 0xec) {
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mUnitsPerSpaceDock);
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mSpaceDockMax);
    }
    if (inMajorVersion > 0 || inMinorVersion >= 0xed) {
        if (theResult == kErrorOK)
            theResult = inFile.Read(this->mFavoriteSpaceType);
    }
    return theResult;
}
#pragma optimize("g", on)

// FUNCTION: EXE 0x43c992
// Loads the epoch's kind-7 tech-tree media for a real epoch, then the player's own epoch media
void EEComputerPlayer::EpochMediaLoaded(long inEpoch)
{
    if (inEpoch >= kTechEpochPrehistoric && inEpoch < kTechEpochAll)
        static_cast<EETechTreeEpoch*>(this->mTechnologies->Get(inEpoch))->LoadMediaOfKind(7);
    EEPlayer::EpochMediaLoaded(inEpoch);
}

// FUNCTION: EXE 0x43c9c5
// Enters an epoch: researches its kind-7 technologies, re-picks the favourite land, sea, air and space types and,
// outside saved-game loading, retires obsolete units and tells the combat manager
void EEComputerPlayer::EnterEpoch(long inEpoch)
{
    if (this->mTechnologies != NULL) {
        EETechTreeEpoch* theEpoch = dynamic_cast<EETechTreeEpoch*>(this->mTechnologies->Get(inEpoch));
        if (theEpoch != NULL)
            theEpoch->ResearchKind(7, true);
    }
    this->ChooseFavoriteLandType();
    this->ChooseFavoriteSeaType();
    this->ChooseFavoriteAirType();
    this->ChooseFavoriteSpaceType();
    if (!gLoadingSavedGame) {
        this->mCombatManager->mPersonnel->RetireObsoleteUnits();
        this->mCombatManager->EpochReached(inEpoch);
    }
}

// FUNCTION: EXE 0x43ca49
// Picks a random premade civilization that fits the current epoch (some excluded on waterless maps, Japan and
// Korea only on space maps) and loads it for the player
void EEComputerPlayer::ChooseCivilization()
{
    std::vector<UString> theNames;
    long theEpoch = this->mTechnologies != NULL ? this->mTechnologies->mCurrentEpoch : 100000;
    bool theWater = gWorld.mMap != NULL && (gWorld.mMap->mMapCharacteristics & kRMMapWaterlessMap) == kRMMapWaterlessMap;
    bool theSpace = gWorld.mMap != NULL && gWorld.mMap->mTerrainMesh.GetIsSpaceMap();
    if (theEpoch != kTechNone) {
        if (theEpoch >= kTechEpochPrehistoric && theEpoch <= kTechEpochBronze)
            theNames.push_back(UString("Babylon"));
        if (theEpoch >= kTechEpochPrehistoric && theEpoch <= kTechEpochDark && !theWater)
            theNames.push_back(UString("Classical Greece"));
        if (theEpoch >= kTechEpochPrehistoric && theEpoch <= kTechEpochDark && !theWater)
            theNames.push_back(UString("Carthage"));
        if (theEpoch >= kTechEpochPrehistoric && theEpoch <= kTechEpochDark)
            theNames.push_back(UString("Kingdom of Israel"));
        if (theEpoch >= kTechEpochPrehistoric && theEpoch <= kTechEpochDark)
            theNames.push_back(UString("Assyrian Empire"));
        if (theEpoch >= kTechEpochPrehistoric && theEpoch <= kTechEpochMiddle)
            theNames.push_back(UString("Byzantine Rome"));
        if (theEpoch >= kTechEpochDark && theEpoch <= kTechEpochIndustrial && !theWater)
            theNames.push_back(UString("England"));
        if (theEpoch >= kTechEpochDark && theEpoch <= kTechEpochIndustrial)
            theNames.push_back(UString("Franks"));
        if (theEpoch >= kTechEpochDark && theEpoch <= kTechEpochIndustrial && !theWater)
            theNames.push_back(UString("Spain"));
        if (theEpoch >= kTechEpochDark && theEpoch <= kTechEpochIndustrial)
            theNames.push_back(UString("Kingdom of Italy"));
        if (theEpoch >= kTechEpochDark && theEpoch <= kTechEpochIndustrial)
            theNames.push_back(UString("Austria"));
        if (theEpoch >= kTechEpochDark && theEpoch <= kTechEpochIndustrial)
            theNames.push_back(UString("Ottoman Empire"));
        if (theEpoch >= kTechEpochIndustrial && theEpoch <= kTechEpochSpace && !theWater)
            theNames.push_back(UString("United States"));
        if (theEpoch >= kTechEpochIndustrial && theEpoch <= kTechEpochSpace)
            theNames.push_back(UString("Germany"));
        if (theEpoch >= kTechEpochIndustrial && theEpoch <= kTechEpochSpace)
            theNames.push_back(UString("Russia"));
        if (theEpoch >= kTechEpochIndustrial && theEpoch <= kTechEpochSpace && !theWater)
            theNames.push_back(UString("Great Britain"));
        if (theEpoch >= kTechEpochIndustrial && theEpoch <= kTechEpochSpace)
            theNames.push_back(UString("Italy"));
        if (theEpoch >= kTechEpochIndustrial && theEpoch <= kTechEpochSpace)
            theNames.push_back(UString("France"));
        if (theEpoch >= kTechEpochAtomicModern && theEpoch <= kTechEpochSpace)
            theNames.push_back(UString("Rebel Forces"));
        if (theEpoch >= kTechEpochAtomicModern && theEpoch <= kTechEpochSpace)
            theNames.push_back(UString("China"));
        if (theEpoch >= kTechEpochDigital && theEpoch <= kTechEpochSpace)
            theNames.push_back(UString("Novaya Russia"));
        if (theEpoch >= kTechEpochDigital && theEpoch <= kTechEpochSpace && theSpace) {
            theNames.push_back(UString("Japan"));
            theNames.push_back(UString("Korea"));
        }
        if (theNames.size() > 0) {
            long theIndex = gPhysics->GetRandom(0L, (long)theNames.size() - 1);
            Fn2806E7(theNames[theIndex], this);
            this->mCivilizationFile = theNames[theIndex];
        }
    }
}

// FUNCTION: EXE 0x43ce2e
// Answers an ally's flare by sending the combat manager to attack at the flared tile (not in observer mode)
void EEComputerPlayer::FlareReceived(EEPlayer* inFrom, const U2DPoint<long>& inTile)
{
    if (!this->mObserverMode)
        this->mCombatManager->Attack(inFrom, inTile, true, true, true, true, true, true);
}


// FUNCTION: EXE 0x43ce56
// Rebuilds or updates the resource centers (regrouping if none exist), then refreshes combat, resources and town groups
void EEComputerPlayer::RefreshResourceCenters()
{
    if (this->mResourceCenters.size() == 0)
        this->Regroup();
    else
        this->UpdateGroups();
    if (this->mResourceCenters.size() == 0)
        this->Regroup();
    this->mCombatManager->Refresh();
    this->mCivilizationManager->mResourceManager->Refresh();
    this->RefreshTownGroups();
}

// FUNCTION: EXE 0x43cec9
// Lets the combat personnel draft the units it deferred
void EEComputerPlayer::RefreshTownGroups()
{
    this->mCombatManager->mPersonnel->DraftDeferred();
}

// FUNCTION: EXE 0x43ced7
// Passes replaced units to the combat, civilization and exploration managers and the base player; a replacing
// drop site is offered to every resource center
void EEComputerPlayer::UnitsSwapped(std::vector<EEPUnitSwap>& ioSwaps)
{
    this->mCombatManager->UnitsSwapped(ioSwaps);
    this->mCivilizationManager->UnitsSwapped(ioSwaps);
    for (std::vector<EEPUnitSwap>::iterator it = ioSwaps.begin(); it != ioSwaps.end(); it++) {
        EEComplexUnit* theUnit = it->mNew;
        if (theUnit->GetUnitClass()->IsDropSite()) {
            for (unsigned long i = 0, n = this->mResourceCenters.size(); i < n; i++)
                this->mResourceCenters[i]->AdoptDropSite(theUnit);
        }
    }
    this->mExplorationManager->UnitsSwapped(ioSwaps);
    EEPlayer::UnitsSwapped(ioSwaps);
}

// FUNCTION: EXE 0x43cf73
// Clears the player: deletes its resource centers, resets the civilization manager and recreates the
// exploration and combat managers
void EEComputerPlayer::Clear()
{
    EEPlayer::Clear();
    DeleteVectorContents(this->mResourceCenters);
    this->mIsStarted = false;
    this->mCivilizationManager->Reset();
    delete this->mExplorationManager;
    this->mExplorationManager = new EECPExplorationManager(this);
    delete this->mCombatManager;
    this->mCombatManager = new EECPCombatManager(this);
}

// FUNCTION: EXE 0x43cff8
// After loading, restores the managers and resource centers, rebases saved times to the game clock, fixes the
// attack bounds and missing favourite types, and flags stranded units; returns the first error
long EEComputerPlayer::RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    long theResult = EEPlayer::RepairPointers(inMajorVersion, inMinorVersion);
    if (theResult == kErrorOK)
        theResult = this->mCombatManager->Restore(inMajorVersion, inMinorVersion);
    if (theResult == kErrorOK)
        theResult = this->mExplorationManager->Restore(inMajorVersion, inMinorVersion);
    if (theResult == kErrorOK)
        theResult = this->mCivilizationManager->Restore(inMajorVersion, inMinorVersion);
    if (theResult == kErrorOK) {
        this->mNextAttackTime += gPhysics->mGameTime;
        for (unsigned long i = 0, n = this->mResourceCenters.size(); i < n; i++) {
            if (theResult != kErrorOK)
                break;
            theResult = this->mResourceCenters[i]->Restore(inMajorVersion, inMinorVersion);
        }
        // LOST-DEBUG: none: exact only (the original refuses mResourceCenters[i]'s _Ptrit::operator+; inline_sim)
        EE_LOST_DEBUG(12);
        if (theResult == kErrorOK)
            this->mNextExpansionTime += gPhysics->mGameTime;
    }
    this->mAttackFrequencyMin = min(this->mAttackFrequencyMin, this->mAttackFrequencyMax);
    this->mAttackFrequencyMax = max(this->mAttackFrequencyMin, this->mAttackFrequencyMax);
    this->mAttackFrequencyFloor = min(this->mAttackFrequencyFloor, this->mAttackFrequencyCeiling);
    this->mAttackFrequencyCeiling = max(this->mAttackFrequencyFloor, this->mAttackFrequencyCeiling);
    if (theResult == kErrorOK) {
        long theValue = this->mDropOffDistance;
        if (theValue > 25)
            this->mDropOffDistance = UMFloatToLong((float)sqrt(theValue));
    }
    this->mRegroupPending = true;
    if (gUnitClassTable[this->mFavoriteAirType] == NULL) {
        this->ChooseFavoriteAirType();
        if (gUnitClassTable[this->mFavoriteAirType] == NULL)
            this->mFavoriteAirType = 72;
    }
    if (gUnitClassTable[this->mFavoriteLandType] == NULL) {
        this->ChooseFavoriteLandType();
        if (gUnitClassTable[this->mFavoriteLandType] == NULL)
            this->mFavoriteLandType = 20;
    }
    if (gUnitClassTable[this->mFavoriteSeaType] == NULL) {
        this->ChooseFavoriteSeaType();
        if (gUnitClassTable[this->mFavoriteSeaType] == NULL)
            this->mFavoriteSeaType = 242;
    }
    if (gUnitClassTable[this->mFavoriteSpaceType] == NULL) {
        this->ChooseFavoriteSpaceType();
        if (gUnitClassTable[this->mFavoriteSpaceType] == NULL)
            this->mFavoriteSpaceType = 730;
    }
    this->mSeed = this->mPlayerID % 10;
    if (this->mInteractionManager != NULL)
        this->mInteractionManager->mIndexSeed = this->mSeed;
    for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (theUnit->GetTileX() != -10000 && theUnit->GetComplexUnit() != NULL && theUnit->mTile != NULL
            && theUnit->mTile->mTerrainType == kTerrainCliff && theUnit->GetUnitClass()->mUnitType != kUnitTypePathPoint
            && (theUnit->GetUnitClass()->mDomains & kMovementAirOrSea) == 0 && !theUnit->GetUnitClass()->mIsProjectile
            && !theUnit->GetUnitClass()->CanMoveOnTerrain(static_cast<EEComplexUnit*>(theUnit), 9))
            this->AddStrandedUnit(theUnit);
    }
    if (inMajorVersion == 0 && inMinorVersion < 0xe5) {
        this->mExpansionTimerMedium = this->mExpansionTimerEasy;
        this->mExpansionTimerHard = this->mExpansionTimerEasy;
    }
    return theResult;
}

// Complete goal/utility types after this unit's static definitions preserve existing initializer order.
#include "ee/EEUGLocation.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEUnitUtilities.h"

// FUNCTION: EXE 0x43d2be
// Sends an empty naval, helicopter or space transport to the berth of its nearest dock, airport or space dock
// and detaches it from the AI
void EEComputerPlayer::SendToNearestBase(EEComplexUnit* inUnit)
{
    GETransformation theTransformation;
    GE3DPoint thePoint;
    std::vector<EEUnit*> theUnits;
    long theCategory = inUnit->mUnitClass->mCategory;
    if (theCategory == kUnitCategoryNavalTransport)
        EEGetPlayerUnitsOfType(kUnitTypeDock, this->mPlayerID, theUnits);
    else if (theCategory == kUnitCategoryHelicopterTransport)
        EEGetPlayerUnitsOfType(kUnitTypeAirport, this->mPlayerID, theUnits);
    else if (theCategory == kUnitCategorySpaceTransport)
        EEGetPlayerUnitsOfType(kUnitTypeSpaceDock, this->mPlayerID, theUnits);
    if (theUnits.size() > 0) {
        std::sort(theUnits.begin(), theUnits.end(), EECPUnitTileDistanceLess(inUnit->GetTile()));
        EEUnitClass* theClass = inUnit->mUnitClass;
        if (EEGetDockShipFacing(&thePoint, theClass, theUnits.front(), false, &theTransformation)) {
            EEUnitGoal* theGoal = new EEUGLocation(theTransformation.GetX(), theTransformation.GetY(),
                theTransformation.GetZ(), false, true, false, 0.0f, false, false);
            inUnit->QueueGoal(theGoal, 0);
            this->DetachUnitFromAI(inUnit);
        }
    }
}


// FUNCTION: EXE 0x43d416
// Appends a goal to one of the unit's goal queues and refreshes its goals
void EEComplexUnit::QueueGoal(EEUnitGoal* inGoal, long inItem)
{
    EEUnitGoal* theGoal = inGoal;
    EEUnitVectorItem& theItem = this->mGoalQueues[inItem];
    theItem.mGoals.push_back(theGoal);
    theGoal->SetOwner(theItem.mUnit);
    this->RefreshGoals();
    this->RefreshActiveGoal();
}

// FUNCTION: EXE 0x43d45d
// Tells the research manager about a new epoch, enters it if it is a real epoch, then runs the base handling
void EEComputerPlayer::EpochReached(long inEpoch)
{
    this->mCivilizationManager->mResearchManager->EpochReached(inEpoch);
    if (inEpoch >= kTechEpochPrehistoric && inEpoch < kTechEpochAll)
        this->EnterEpoch(inEpoch);
    EEPlayer::EpochReached(inEpoch);
}

// FUNCTION: EXE 0x43d492
// Schedules the next manager-enable check mEnableInterval seconds after inTime
void EEComputerPlayer::SetEnableTime(long inTime)
{
    this->mNextEnableTime = UMFloatToLong(this->mEnableInterval * 1000.0f + inTime);
    this->mIsEnableTimeSet = true;
}

// FUNCTION: EXE 0x43d4cd
// Schedules the next attack: a random number of minutes (initial delay before the first, frequency after), scaled
// by epoch span and difficulty or deathmatch modifier, clamped, added to inTime
void EEComputerPlayer::SetNextAttackTime(unsigned long inTime)
{
    long theEpochs = gWorld.mGameOptions.mEpochEnd - gWorld.mGameOptions.mEpochStart + 1;
    long theMinutes;
    if (!this->mIsStarted) {
        theMinutes = gPhysics->GetRandom(this->GetInitialAttackDelayMin(), this->GetInitialAttackDelayMax());
        theMinutes += this->mInitialAttackDelayPerEpoch * theEpochs;
        if (this->IsDeathmatch())
            theMinutes = UMFloatToLong(this->GetDMInitialAttackModifier() * theMinutes);
        else
            theMinutes = UMFloatToLong(this->GetAttackDelayModifier() * theMinutes);
        long theLow = this->GetAttackDelayFloor();
        long theHigh = this->GetAttackDelayCeiling();
        if (theMinutes < theLow)
            theMinutes = theLow;
        if (theMinutes > theHigh)
            theMinutes = theHigh;
        this->mNextAttackTime = theMinutes * 60000 + inTime;
    } else {
        theMinutes = gPhysics->GetRandom(this->GetAttackFrequencyMin(), this->GetAttackFrequencyMax());
        theMinutes += UMFloatToLong(this->GetAttackFrequencyPerEpoch() * theEpochs);
        if (this->IsDeathmatch())
            theMinutes = UMFloatToLong(this->GetDMAttackFrequencyModifier() * theMinutes);
        else
            theMinutes = UMFloatToLong(this->GetAttackFrequencyModifier() * theMinutes);
        long theLow = this->GetAttackFrequencyFloor();
        long theHigh = this->GetAttackFrequencyCeiling();
        if (theMinutes < theLow)
            theMinutes = theLow;
        if (theMinutes > theHigh)
            theMinutes = theHigh;
        this->mNextAttackTime = theMinutes * 60000 + inTime;
        this->mAttackStarted = true;
    }
}

// FUNCTION: EXE 0x43d61a
// Schedules the next expansion check after the difficulty's expansion timer (seconds) from inTime
void EEComputerPlayer::SetNextExpansionTime(unsigned long inTime)
{
    long theSeconds;
    if (this->mDifficulty == kDifficultyEasy)
        theSeconds = this->mExpansionTimerEasy;
    else if (this->mDifficulty == kDifficultyMedium)
        theSeconds = this->mExpansionTimerMedium;
    else
        theSeconds = this->mExpansionTimerHard;
    this->mNextExpansionTime = theSeconds * 1000 + inTime;
}

// FUNCTION: EXE 0x43d652
// Schedules the next island expansion the difficulty's island timer (minutes) from now
void EEComputerPlayer::SetNextIslandExpansionTime()
{
    unsigned long theNow = gPhysics->mGameTime;
    this->mNextIslandExpansionTime = this->GetIslandExpansionTimer() * 60000 + theNow;
}

// FUNCTION: EXE 0x43d677
// Writes the base player, the managers, the resource centers and every tuning value to the saved game; returns
// the first write error (resource center errors are ignored; clears mIsEnableTimeSet before writing it)
// NEAR (99.3): the second save loop keeps i in a register where the original reloads it from [ebp+0x10] per use
long EEComputerPlayer::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theCount;
    unsigned long i;
    long theResult = EEPlayer::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == kErrorOK)
        theResult = this->mCombatManager->Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == kErrorOK)
        theResult = this->mCivilizationManager->Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == kErrorOK && inMinorVersion >= 0x20)
        theResult = this->mExplorationManager->Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mLBD4);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mFavoriteLandType);
    this->mIsEnableTimeSet = false;
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mNextEnableTime);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIsEnableTimeSet);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mBuildingDamaged);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCitizenDamaged);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIsStarted);
    if (theResult == kErrorOK) {
        i = this->mNextAttackTime - gPhysics->mGameTime;  // the counter variable doubles as the time (one [ebp+0x10] slot)
        theResult = outFile.Write(i);
    }
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mActive);
    if (theResult == kErrorOK) {
        theCount = this->mResourceCenters.size();
        theResult = outFile.Write(theCount);
        // a resource center's write error is reported (it used to be discarded)
        for (i = 0; i < theCount && theResult == kErrorOK; i++)
            theResult = this->mResourceCenters[i]->Save(inMajorVersion, inMinorVersion, outFile);
    }
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUsesFishing);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUnitCostMedium);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCarrierMaxPerGroup);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCarrierPopPercentage);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCarrierChance);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mNumAirExplorers);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mComputerAggression);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttackEnemyChance);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttackToClaimResourceChance);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttackForRevengeChance);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttackGroupSizeMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttackGroupSizeMin);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttackFrequencyPerEpoch);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttackFrequencyCeiling);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttackFrequencyFloor);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttackFrequencyMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttackFrequencyMin);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mInitialAttackDelayPerEpoch);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttackDelayCeiling);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttackDelayFloor);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mInitialAttackDelayMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mInitialAttackDelayMin);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mSpecializedUnitPercentage);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTownGuardPercentage);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mLD78);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mLD7C);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCitizenPercentage);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDefenseGroupSizeMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDefenseGroupSizeMin);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mEpochFrequencyMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mEpochFrequencyMin);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mGuardContinentPercentage);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mGuardChokepointAirPercentage);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mGuardChokepointPercentage);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mGuardHighGroundPercentage);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mFC38);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mFC3C);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mFC40);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mFC44);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mFC48);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mFC4C);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mFC50);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mFC54);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mFC58);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mGuardTownPercentage);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mGuardTownLandPercentage);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mGuardWaterwayPercentage);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mHeroMaxPerGroup);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mHeroPopPercentage);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mHeroChance);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mNumImprovedUnitTypes);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mNumImprovedAttributes);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mReplenishDelayMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mReplenishDelayMin);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mNumLandExplorers);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mNumChokePointsToGuard);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAirportMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mArcheryRangeMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mBarracksMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDockMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mGranaryMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mHospitalMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCyberFactoryMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCyberLabMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mNavalYardMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUniversityMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mSiegeFactoryMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mStableMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTempleMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDefensiveUnitCap);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMaxAttackResponseRange);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMilitaryPopPercentage);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAidThreatThreshold);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mPriestMaxPerGroup);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mPriestPopPercentage);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mPriestChance);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mProphetMaxPerGroup);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mProphetPopPercentage);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mProphetChance);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mResearchFrequency);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDropOffDistance);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mNumSeaExplorers);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMaxEnabledManagers);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mManagerAttackWait);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mManagerEpochAdvanceWait);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mEnableStockProduction);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mEnableStockResource);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mEnableStockCivilization);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mEnableStockExploration);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mEnableStockDefense);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mEnableStockBuild);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mLE80);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mEnableStockResearch);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mEnableStockCombat);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mEnableInterval);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMilitaryCountWeight);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mWorkerCountWeight);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUnitsPerAirport);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUnitsPerArcheryRange);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUnitsPerBarracks);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUnitsPerDock);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUnitsPerGranary);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUnitsPerHospital);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUnitsPerCyberFactory);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUnitsPerCyberLab);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUnitsPerNavalYard);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUnitsPerUniversity);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUnitsPerSiegeFactory);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUnitsPerStable);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUnitsPerTemple);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCanBuild);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIsCheating);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCanDefend);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mBF4E);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCanExplore);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCanGather);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIsCitizenPercentageSet);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCanAttack);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIgnoreOthers);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDifficulty);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttackFrequencyModifierEasy);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttackFrequencyModifierHard);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttackDelayModifierEasy);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttackDelayModifierHard);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttackGroupModifierEasy);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttackGroupModifierHard);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUnitCostReductionEasy);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUnitCostReductionHard);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mEpochFrequencyModifierEasy);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mEpochFrequencyModifierHard);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mPopLimitAdjustmentEasy);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mPopLimitAdjustmentHard);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mResearchFrequencyModifierEasy);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mResearchFrequencyModifierHard);
    if (theResult == kErrorOK) {
        unsigned long theTime2 = this->mNextExpansionTime - gPhysics->mGameTime;  // block-scoped: takes inMinorVersion's dead slot
        theResult = outFile.Write(theTime2);
    }
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUnitsPerTankFactory);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTankFactoryMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMinMilitaryBuildingsEasy);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMinMilitaryBuildingsEasyDM);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMinMilitaryBuildingsMedium);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMinMilitaryBuildingsMediumDM);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMinMilitaryBuildingsHard);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMinMilitaryBuildingsHardDM);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mRPSSelectionNonIslands);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mRPSSelectionIslands);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mSpecializedUnitChance);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDMEarlyAttackTime);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDMEarlyAttackScale);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mNavalUnitWeight);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAirUnitWeight);
    if (theResult == kErrorOK) {
        theCount = this->mFavoriteTypeHistory.size();
        theResult = outFile.Write(theCount);
        for (i = 0; i < theCount; i++) {
            if (theResult == kErrorOK)
                theResult = outFile.Write(this->mFavoriteTypeHistory[i].first);
            if (theResult == kErrorOK)
                theResult = outFile.Write(this->mFavoriteTypeHistory[i].second);
        }
    }
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMaxFlameThrowers);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttackStarted);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDefensiveUnitUpdateEasy);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDefensiveUnitUpdateMedium);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDefensiveUnitUpdateHard);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDMDefensiveUnitUpdateEasy);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDMDefensiveUnitUpdateMedium);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDMDefensiveUnitUpdateHard);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTownGuardUpdateEasy);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTownGuardUpdateMedium);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mTownGuardUpdateHard);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDMTownGuardUpdateEasy);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDMTownGuardUpdateMedium);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mDMTownGuardUpdateHard);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mPopLimitAdjustmentMedium);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAttackGroupModifierMedium);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAssistAlliesDelayEasy);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAssistAlliesDelayMedium);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mAssistAlliesDelayHard);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mShipsToMaintainDMEasy);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mShipsToMaintainDMMedium);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mShipsToMaintainDMHard);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mShipsToMaintainEasy);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mShipsToMaintainMedium);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mShipsToMaintainHard);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mShipMaintenanceTimer);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mSiegeWeaponPercentage);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mBonzaiAttackLevel);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mFavoriteSeaType);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mFavoriteAirType);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mNextIslandExpansionTime);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mExpansionTimerEasy);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mExpansionTimerMedium);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mExpansionTimerHard);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mLF14);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mLF1C);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mLF18);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mLF20);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mUnitsPerSpaceDock);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mSpaceDockMax);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mFavoriteSpaceType);
    return theResult;
}

// FUNCTION: EXE 0x43ec42
// Turns the AI on or off, handing all units to the unit manager or releasing them when the state changes
void EEComputerPlayer::SetActive(bool inActive)
{
    if (inActive) {
        if (!this->mActive)
            this->mCivilizationManager->mUnitManager->AddAllUnits();
    } else if (this->mActive)
        this->mCivilizationManager->mUnitManager->ReleaseUnits();
    this->mActive = inActive;
}

// FUNCTION: EXE 0x43ec89
// Sets the share of the population kept as citizens and marks it as set
void EEComputerPlayer::SetCitizenPercentage(float inValue)
{
    this->mCitizenPercentage = inValue;
    this->mIsCitizenPercentageSet = true;
}

// FUNCTION: EXE 0x43ec9d
// Sets the deathmatch attack frequency modifier for the player's difficulty
void EEComputerPlayer::SetDMAttackFrequencyModifier(float inValue)
{
    if (this->mDifficulty == kDifficultyEasy)
        this->mDMAttackFrequencyModifierEasy = inValue;
    else if (this->mDifficulty == kDifficultyMedium)
        this->mDMAttackFrequencyModifierMedium = inValue;
    else if (this->mDifficulty == kDifficultyHard)
        this->mDMAttackFrequencyModifierHard = inValue;
}

// FUNCTION: EXE 0x43ecd6
// Sets the deathmatch initial attack delay modifier for the player's difficulty
void EEComputerPlayer::SetDMInitialAttackModifier(float inValue)
{
    if (this->mDifficulty == kDifficultyEasy)
        this->mDMInitialAttackModifierEasy = inValue;
    else if (this->mDifficulty == kDifficultyMedium)
        this->mDMInitialAttackModifierMedium = inValue;
    else if (this->mDifficulty == kDifficultyHard)
        this->mDMInitialAttackModifierHard = inValue;
}

// FUNCTION: EXE 0x43ed0f
// Sets the island expansion timer for the player's difficulty
void EEComputerPlayer::SetIslandExpansionTimer(unsigned long inValue)
{
    if (this->mDifficulty == kDifficultyEasy)
        this->mIslandExpansionTimerEasy = inValue;
    else if (this->mDifficulty == kDifficultyMedium)
        this->mIslandExpansionTimerMedium = inValue;
    else if (this->mDifficulty == kDifficultyHard)
        this->mIslandExpansionTimerHard = inValue;
}

// FUNCTION: EXE 0x43ed48
// Queues a stranded unit to be killed on the next update; a stranded citizen is also reported as a threat
void EEComputerPlayer::UnitStranded(EEComplexUnit* inUnit)
{
    inUnit->Reference();
    this->mUnitsToKill.push_back(inUnit);
    if (inUnit->mIsCitizen)
        this->mCombatManager->NoteThreat(inUnit, true);
}

// FUNCTION: EXE 0x43ed81
// Takes a resource amount from the player, ignoring zero or negative amounts
void EEComputerPlayer::TakeResource(long inIndex, long inAmount)
{
    if (inAmount > 0)
        EEPlayer::TakeResource(inIndex, inAmount);
}

// FUNCTION: EXE 0x43ed98
// Pays for a technology, except epoch advances, which are free for the computer player
void EEComputerPlayer::Pay(EETechnology* inTechnology, long inUnitType, float inFraction)
{
    if (inTechnology->mData->mID < kTechEpochPrehistoric || inTechnology->mData->mID >= kTechEpochAll)
        EEPlayer::Pay(inTechnology, inUnitType, inFraction);
}

// FUNCTION: EXE 0x43edc0
// Credits the resource manager's accounts with the scaled cost of a unit improvement, then pays it as a normal
// player does
void EEComputerPlayer::PayImprovement(EEUnitClass* inClass, long inImprovement)
{
    EEUnitClass* theClass = gUnitClassTable[inClass->mUnitType];
    long theTechnology = theClass != NULL ? theClass->mTechnology : 100000;
    if (inClass != NULL && theTechnology != kTechNone) {
        EETechTreeNode* theNode = this->mTechnologies->Get(theTechnology);
        if (theNode != NULL) {
            EEUnitClassImprovementTable* theImprovements = inClass->mImprovements;
            EEUnitClassUpgrade* theUpgrade = this->mUnitClassUpgrades[inClass->mUnitType];
            if (theImprovements != NULL) {
                float theFactor = theImprovements->mEntries[inImprovement].mCostFactor;
                theFactor *= this->GetImprovementLevel(inClass->mUnitType, inImprovement);
                for (long i = 2; i < 7; i++) {
                    float theCost = (float)theNode->mData->mCosts[i] * theUpgrade->mCostScale;
                    theCost *= theFactor;
                    long theAmount = UMFloatToLong(theCost);
                    this->mCivilizationManager->mResourceManager->Earn(i, this->ScaleCost(theAmount));
                }
            }
        }
    }
    EEPlayer::PayImprovement(inClass, inImprovement);
}


// FUNCTION: EXE 0x43eeca
// Returns whether the stock of a resource reaches the enable threshold of a manager kind (morale buildings
// use the build threshold)
bool EEComputerPlayer::CanEnableManager(long inKind, long inResource)
{
    long theValue = this->mResourceCaps[inResource];
    bool theResult = false;
    switch (inKind) {
    case kCPManagerCivilization: theResult = theValue >= this->mEnableStockCivilization; break;
    case kCPManagerResource: theResult = theValue >= this->mEnableStockResource; break;
    case kCPManagerResearch: theResult = theValue >= this->mEnableStockResearch; break;
    case kCPManagerProduction: theResult = theValue >= this->mEnableStockProduction; break;
    case kCPManagerBuild: theResult = theValue >= this->mEnableStockBuild; break;
    case 5: theResult = theValue >= this->mLE80; break;
    case kCPManagerDefense: theResult = theValue >= this->mEnableStockDefense; break;
    case kCPManagerExploration: theResult = theValue >= this->mEnableStockExploration; break;
    case kCPManagerCombat: theResult = theValue >= this->mEnableStockCombat; break;
    case kCPManagerMoraleBuildings: theResult = theValue >= this->mEnableStockBuild; break;
    }
    return theResult;
}

// FUNCTION: EXE 0x43ef5d
// Registers a new unit with the player and, while the AI is active, with the civilization and combat managers
void EEComputerPlayer::UnitPlaced(EEComplexUnit* inUnit)
{
    EEPlayer::UnitPlaced(inUnit);
    if (this->mActive) {
        this->mCivilizationManager->UnitAdded(inUnit);
        this->mCombatManager->UnitAdded(inUnit);
    }
}

// FUNCTION: EXE 0x43ef91
// Forgets a unit leaving the player: base bookkeeping, then the civilization, combat and exploration managers
// and, for a land building, every resource center
void EEComputerPlayer::UnitTaken(RTSUnit* inUnit, bool inSwapped)
{
    EEPlayer::UnitTaken(inUnit, inSwapped);
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    if (this->mActive)
        this->mCivilizationManager->UnitLost(theUnit, inSwapped);
    if (this->mCombatManager != NULL)
        this->mCombatManager->UnitRemoved(theUnit);
    if (this->mExplorationManager != NULL && theUnit->mComplexUnit != NULL)
        this->mExplorationManager->UnitRemoved(theUnit->mComplexUnit, inSwapped);
    if (theUnit->mUnitClass->IsLandBuilding()) {
        unsigned long i, n;
        for (i = 0, n = this->mResourceCenters.size(); i < n; i++)
            this->mResourceCenters[i]->ReleaseUnit(theUnit);
    }
}

// FUNCTION: EXE 0x43f028
// Handles a unit finishing a goal: idles transports (sending empty damaged ones home), returns detached units to
// the combat manager, marks citizens as idle gatherers and drops nearly idle units from their group
void EEComputerPlayer::UnitGoalDone(EEComplexUnit* inUnit)
{
    unsigned long theGoals = inUnit->mGoalQueues[0].mGoals.size();
    if (inUnit->mUnitClass->IsTransport()) {
        this->mCivilizationManager->mResourceManager->TransportIdle(inUnit);
        if (theGoals == 1 && inUnit->IsRepairable() && inUnit->GetNumCargoUnits() == 0)
            this->SendToNearestBase(inUnit);
        if (inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalLoadTransport && inUnit->GetNumCargoUnits() == 0)
            inUnit->ClearGoalQueue(0);
    }
    theGoals = inUnit->mGoalQueues[0].mGoals.size();
    if (inUnit->mIsDetachedFromAI && theGoals <= 1) {
        inUnit->mIsDetachedFromAI = false;
        this->mCombatManager->UnitAdded(inUnit);
    }
    long theCategory = inUnit->mUnitClass->mCategory;
    if (theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen)
        this->mCivilizationManager->mResourceManager->AddIdleGatherer(inUnit);
    if (theGoals <= 1 && inUnit->mActiveGroup != NULL)
        inUnit->mActiveGroup->RemoveUnit(inUnit);
}


// FUNCTION: EXE 0x43f142
// Unloads the epoch's kind-7 tech-tree media for a real epoch, then the player's own epoch media
void EEComputerPlayer::EpochMediaUnloaded(long inEpoch)
{
    if (inEpoch >= kTechEpochPrehistoric && inEpoch < kTechEpochAll)
        static_cast<EETechTreeEpoch*>(this->mTechnologies->Get(inEpoch))->UnloadMediaOfKind(7);
    EEPlayer::EpochMediaUnloaded(inEpoch);
}

// FUNCTION: EXE 0x43f175
// Runs the AI tick: one-time start-up on the first call, then the enable, attack-state, population and
// expansion timers and the manager updates; finally kills the units queued as stranded
void EEComputerPlayer::Update(unsigned long inTime)
{
    UWideString theUnused1;
    UWideString theUnused2;
    if (!this->mIsStarted) {
        gWorld.mMap->ExploreUnseen(this);
        this->EnableManagers(inTime);
        this->SetEnableTime(inTime);
        this->SetNextAttackTime(inTime);
        this->SetNextExpansionTime(inTime);
        this->SetNextIslandExpansionTime();
        if (this->mResourceCenters.size() == 0)
            this->Regroup();
        if (gGameOptions.mLoadType == 0)
            this->ChooseCivilization();
        const long& theBonus = this->mExtraResources;
        this->mResourceCaps[kResourceGold] += theBonus;
        this->mResourceCaps[kResourceIron] += theBonus;
        this->mIsStarted = true;
        if (this->IsDeathmatch()) {
            this->mCivilizationManager->CheckBuildingCounts();
            for (long i = 0; i < 3; i++)
                this->mCombatManager->mDefenseCommander->BuildUp();
        }
    }
    if (!this->mObserverMode && this->mFirstUnit != NULL) {
        if (this->mRegroupPending) {
            this->UpdateGroups();
            this->mRegroupPending = false;
        }
        if (!this->mIsEnableTimeSet)
            this->SetEnableTime(inTime);
        if (this->mCombatState == 2 && this->mNumAttacks == 0 && inTime - this->mLastDamagedTime > 60000) {
            this->SetCombatState(1);
            this->mBuildingDamaged = false;
            this->mCitizenDamaged = false;
        }
        if (inTime >= this->mNextEnableTime) {
            this->EnableManagers(inTime);
            this->SetEnableTime(inTime);
        }
        if (inTime >= this->mNextPopulationTime) {
            this->WorkOutPopulation();
            this->mNextPopulationTime = inTime + 60000;
        }
        if (inTime >= this->mNextExpansionTime) {
            this->FlagGroup();
            this->SetNextExpansionTime(inTime);
        }
        this->mCivilizationManager->Update(inTime);
        this->mDiplomacyManager->Update(inTime);
        this->mCombatManager->Tick(inTime);
        this->mExplorationManager->Update(inTime);
    } else if (this->mCombatManager != NULL)
        this->mCombatManager->DeleteGroups();
    if (this->mUnitsToKill.size() != 0) {
        bool theSaved = this->mCombatManager->mTracksThreats;
        this->mCombatManager->mTracksThreats = false;
        for (long i = this->mUnitsToKill.size() - 1; i >= 0; i--) {
            this->mUnitsToKill[i]->Kill(true, false, false);
            this->mUnitsToKill[i]->Dereference();
        }
        this->mUnitsToKill.clear();
        this->mCombatManager->mTracksThreats = theSaved;
    }
}

// FUNCTION: EXE 0x43f3f1
// Assigns ungrouped units to resource centers, creating new inactive centers for the rest, then re-sorts the
// centers by distance from the town center and activates gaps; new centers only gather units if centers existed
void EEComputerPlayer::UpdateGroups()
{
    std::vector<EEUnit*> theUnits;
    U2DPoint<long> theCenter;
    EECPResourceCenter* theNewCenter = NULL;
    theCenter = this->mCivilizationManager->mBuildManager->mTownCenterTile;
    if (theCenter.mX != -10000 && theCenter.mY != -10000) {
        long theCount;
        this->GetObjectsToGroup(theUnits, theCount);
        std::vector<EEUnit*>::iterator theUnit;
        for (theUnit = theUnits.begin(); theUnit != theUnits.end();) {
            bool theFound = false;
            unsigned long theNumCenters;
            unsigned long i;
            for (i = 0, theNumCenters = this->mResourceCenters.size(); i < theNumCenters && !theFound; i++)
                theFound = this->mResourceCenters[i]->Contains(*theUnit);
            if (theFound)
                theUnit = theUnits.erase(theUnit);
            else
                theUnit++;
        }
        for (theUnit = theUnits.begin(); theUnit != theUnits.end();) {
            bool theFound = false;
            unsigned long theNumCenters;
            unsigned long i;
            for (i = 0, theNumCenters = this->mResourceCenters.size(); i < theNumCenters && !theFound; i++) {
                if (this->mResourceCenters[i]->IsWithinRadius(*theUnit)) {
                    this->mResourceCenters[i]->AddUnit(*theUnit);
                    theFound = true;
                }
            }
            if (theFound)
                theUnit = theUnits.erase(theUnit);
            else
                theUnit++;
        }
        while (theUnits.size() != 0) {
            EEUnit* theFirst = theUnits.back();
            theUnits.pop_back();
            theNewCenter = new EECPResourceCenter(this, theFirst);
            for (theUnit = theUnits.begin(); theUnit != theUnits.end();) {
                bool theFound = false;
                unsigned long theNumCenters;
                unsigned long i;
                for (i = 0, theNumCenters = this->mResourceCenters.size(); i < theNumCenters && !theFound; i++) {
                    if (theNewCenter->IsWithinRadius(*theUnit)) {
                        theNewCenter->AddUnit(*theUnit);
                        theFound = true;
                    }
                }
                if (theFound)
                    theUnit = theUnits.erase(theUnit);
                else
                    theUnit++;
            }
            theNewCenter->SetActive(false, 0);
            this->mResourceCenters.push_back(theNewCenter);
        }
        if (theNewCenter != NULL) {
            std::sort(this->mResourceCenters.begin(), this->mResourceCenters.end(), EECPCenterDistanceLess(theCenter));
            std::vector<EECPResourceCenter*>::iterator theFirst = this->mResourceCenters.begin();
            std::vector<EECPResourceCenter*>::iterator theEnd = this->mResourceCenters.end();
            std::vector<EECPResourceCenter*>::iterator theFound = theEnd;
            std::vector<EECPResourceCenter*>::iterator theCenter = theFirst;
            while (theCenter != theEnd && theFound == theEnd) {
                if ((*theCenter)->mIsActive)
                    theFound = theCenter;
                theCenter++;
            }
            if (theFound != theFirst && theFound != theEnd) {
                for (; theFirst != theFound; theFirst++)
                    (*theFirst)->SetActive(true, 0);
            }
            // activate inactive centers between two active ones; with a single center there is no gap (the loop
            // used to step past its end then, as begin + 1 never equals end - 1)
            for (theCenter = this->mResourceCenters.begin();
                this->mResourceCenters.size() >= 2 && ++theCenter != this->mResourceCenters.end() - 1;) {
                if (!(*theCenter)->mIsActive && (*(theCenter - 1))->mIsActive && (*(theCenter + 1))->mIsActive)
                    (*theCenter)->SetActive(true, 0);
            }
        }
    }
}


// FUNCTION: EXE 0x43f73e
// Returns true for citizens and for loaded transports that carry only citizens
bool EEComputerPlayer::AvoidsEnemies(EEComplexUnit* inUnit)
{
    long theCategory = inUnit->mUnitClass->mCategory;
    bool theResult = theCategory == kUnitCategoryLandCitizen || theCategory == kUnitCategoryNavalCitizen;
    if (!theResult) {
        if (inUnit->mUnitClass->IsTransport() && inUnit->GetNumCargoUnits() > 0) {
            unsigned long i = 0;
            theResult = true;
            while (i < inUnit->GetNumCargoUnits() && theResult) {
                long theKind = inUnit->GetCargo(i)->mUnitClass->mCategory;
                if (theKind == kUnitCategoryLandCitizen || theKind == kUnitCategoryNavalCitizen)
                    theResult = true;
                else
                    theResult = false;
                i++;
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x43f7ba
// Has the combat manager respond to an enemy wonder being started
void EEComputerPlayer::WonderStarted(EEComplexUnit* inUnit)
{
    this->mCombatManager->RespondToUnit(inUnit);
}

// FUNCTION: EXE 0x43f7cc
// Returns the network message a deque iterator points to (std::deque<NEMessage*> iterator operator*)

// FUNCTION: EXE 0x43f7f2
// Appends a unit to a unit list (std::vector push_back)

// FUNCTION: EXE 0x43f81e
// Returns the number of teams in the world
unsigned long EEWorld::GetNumTeams()
{
    return this->mTeams.size();
}

// FUNCTION: EXE 0x43f83a
// Appends a resource center to the player's list (std::vector push_back)

// TEMPLATE: EXE 0x43f866
// (vector<pair<unsigned long, long> >::resize(n, x))

// FUNCTION: EXE 0x43f911
// Appends an (unsigned long, long) pair to a list (std::vector<pair<unsigned long, long> > push_back)

// FUNCTION: EXE 0x43f93d
// Destroys a map of id to id-list pointers, freeing its nodes (std::map<long, vector<long>*> tree dtor)

// FUNCTION: EXE 0x43f968
// Destroys a map of id to complex unit, freeing its nodes (std::map<long, EEComplexUnit*> tree dtor)

// FUNCTION: EXE 0x43f993
// Returns the count stored for a short key, inserting 0 if absent (std::map<unsigned short, unsigned long> operator[])

// FUNCTION: EXE 0x43f9bd
// Destroys a map of short key to count, freeing its nodes (std::map<unsigned short, unsigned long> tree dtor)

// TEMPLATE: EXE 0x43f9e8
// (vector<pair<unsigned long, long> >::insert(P, N, X))

// FUNCTION: EXE 0x43fc85
// Erases a range of entries from a map of id to id-list pointers (std::map<long, vector<long>*> tree erase)

// FUNCTION: EXE 0x43fcf6
// Inserts a (value, complex unit) entry into a multimap sorted by descending value (std::multimap insert)

// FUNCTION: EXE 0x43fd55
// Erases a range of entries from a map of id to complex unit, or clears it if the range is all of it (std::map erase)
// Also the body of multimap<long, EEComplexUnit*, greater<long> >::erase (ICF)

// FUNCTION: EXE 0x43fdc6
// Erases a range of entries from a map of short key to count, or clears it if the range is all of it (std::map erase)

// FUNCTION: EXE 0x43fe37
// Returns whether the given player is a member of the team
bool EETeam::ContainsPlayer(unsigned long inPlayerID)
{
    std::vector<unsigned long>::iterator theEnd = this->mPlayers.end();
    std::vector<unsigned long>::iterator it;
    for (it = this->mPlayers.begin(); it != theEnd; it++)
        if (*it == inPlayerID)
            break;
    return it != theEnd;
}

// FUNCTION: EXE 0x43fe5a
// Frees a subtree of a map of id to id-list pointers, node by node (std::map _Erase)

// FUNCTION: EXE 0x43fe8c
// Frees a subtree of a map of id to complex unit, node by node (std::map _Erase)
// Also the body of multimap<long, EEComplexUnit*, greater<long> >::_Erase (ICF)

// FUNCTION: EXE 0x43febe
// Frees a subtree of a map of short key to count, node by node (std::map _Erase)

// TEMPLATE: EXE 0x43fef0
// (DeleteVectorContents(vector<EECPResourceCenter*>&) (ee/EECalamityManager.h))

// FUNCTION: EXE 0x43ff2d
// Shuffles a range of ids with the AI random generator (EEAIRandomShuffle)

// FUNCTION: EXE 0x43ff54
// Sorts resource centers by distance from a point, nearest first (std::sort with EECPCenterDistanceLess)

// FUNCTION: EXE 0x440038
// Returns the number of elements between two pointer-list iterators (std::_Ptrit operator-; several types, ICF)

// TEMPLATE: EXE 0x440046
// (std::make_pair<long, long>; emitted by Load's make_pair(0L, 0L))

// FUNCTION: EXE 0x44005c
// Sorts units by tile distance from a point, nearest first (std::sort with EECPUnitTileDistanceLess)

// FUNCTION: EXE 0x440140
// Inserts a copied range of ids before a position, growing the storage if needed (std::vector<long> _Insert; ICF)

// FUNCTION: EXE 0x440412
// Insertion-sorts a short range of resource centers by distance from a point (std::_Insertion_sort)

// FUNCTION: EXE 0x440445
// Quick-sorts resource centers by distance from a point, falling back to heap sort past the depth limit (std::_Sort)

// FUNCTION: EXE 0x44054d
// Finishes sorting nearly sorted resource centers without bounds checks (std::_Unguarded_insertion_sort)

// FUNCTION: EXE 0x44058f
// Insertion-sorts a short range of units by tile distance from a point (std::_Insertion_sort)

// FUNCTION: EXE 0x4405c2
// Quick-sorts units by tile distance from a point, falling back to heap sort past the depth limit (std::_Sort)

// FUNCTION: EXE 0x4406ca
// Copies a range of 4-byte elements to raw storage and returns its end (std::copy; shared by several types through ICF)

// FUNCTION: EXE 0x4406ea
// Shuffles a range of ids using the AI random generator (std::_Random_shuffle)

// FUNCTION: EXE 0x440741
// Adds the number of elements between two iterators to a counter (std::_Distance; shared by several types through ICF)

// FUNCTION: EXE 0x44076c
// Insertion-sorts resource centers by distance from a point, one element at a time (std::_Insertion_sort_1)

// FUNCTION: EXE 0x44081d
// Arranges resource centers into a heap ordered by distance from a point (std::make_heap)

// FUNCTION: EXE 0x440861
// Sorts a heap of resource centers by distance from a point (std::sort_heap)

// FUNCTION: EXE 0x4408ae
// Partitions resource centers around a pivot distance and returns the split point (std::_Unguarded_partition)

// FUNCTION: EXE 0x4409ce
// Moves a resource center back to its sorted place among nearer ones (std::_Unguarded_insert)

// FUNCTION: EXE 0x440a01
// Arranges units into a heap ordered by tile distance from a point (std::make_heap)

// FUNCTION: EXE 0x440a45
// Sorts a heap of units by tile distance from a point (std::sort_heap)

// FUNCTION: EXE 0x440a92
// Builds a heap of resource centers ordered by distance from a point, sifting down from the middle (std::_Make_heap)

// FUNCTION: EXE 0x440aef
// Moves the farthest resource center of a heap to the end and restores the heap (std::pop_heap)

// FUNCTION: EXE 0x440b22
// Builds a heap of units ordered by tile distance from a point, sifting down from the middle (std::_Make_heap)

// FUNCTION: EXE 0x440b7f
// Moves the farthest unit of a heap to the end and restores the heap (std::pop_heap)

// FUNCTION: EXE 0x440bb2
// Swaps the elements two iterators point to (std::iter_swap; shared by several types through ICF)

// FUNCTION: EXE 0x440bd7
// Sifts a resource center down a heap ordered by distance from a point (std::_Adjust_heap)

// FUNCTION: EXE 0x440c51
// Pops the top resource center of a heap ordered by distance from a point (std::_Pop_heap_0)

// FUNCTION: EXE 0x440cc1
// Pops the top unit of a heap ordered by tile distance from a point (std::_Pop_heap_0)

// FUNCTION: EXE 0x440d31
// Sifts a resource center up a heap ordered by distance from a point (std::_Push_heap)

// FUNCTION: EXE 0x440d80
// Moves the top resource center of a heap to a given slot and re-heaps the rest (std::_Pop_heap)

// FUNCTION: EXE 0x440dc4
// Sifts a unit up a heap ordered by tile distance from a point (std::_Push_heap; also EETileDistanceLess, ICF)

// FUNCTION: EXE 0x440e13
// Moves the top unit of a heap to a given slot and re-heaps the rest (std::_Pop_heap)

// FUNCTION: EXE 0x440e57
// Appends a string to a string list (std::vector<UString> push_back)

// FUNCTION: EXE 0x440e83
// Inserts one string before a position and returns an iterator to it (std::vector<UString> insert)

// FUNCTION: EXE 0x440ed3
// Returns an iterator n teams further on (std::_Ptrit<EETeam> operator+; shared by several types through ICF)

// FUNCTION: EXE 0x440ee8
// Destroys every wide string in a list and frees its storage (std::vector<UWideString> _Tidy)

// FUNCTION: EXE 0x440f21
// Inserts n copies of a string before a position, growing the storage if needed (std::vector<UString> insert)

// FUNCTION: EXE 0x4411ce
// Copy-constructs n copies of a string into raw storage and returns its end (std::vector<UString> _Ufill)

// TEMPLATE: EXE 0x4411ff
// (_Ptrit<pair<unsigned long, long> >::operator-(n))

// FUNCTION: EXE 0x441213
// Copy-constructs a range of strings into raw storage and returns its end (std::vector<UString> _Ucopy)

// FUNCTION: EXE 0x441249
// Assigns a string to every element of a range (std::fill<UString>)

// FUNCTION: EXE 0x441267
// Copies a range of strings backwards onto a later, overlapping range (std::copy_backward<UString>)

// FUNCTION: EXE 0x441294
// Copy-constructs a string in place (std::_Construct<UString>)
