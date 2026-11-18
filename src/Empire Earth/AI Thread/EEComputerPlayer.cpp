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

#include "ee/EEAIFOUnitScreeners.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEAIRandom.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EECPTrace.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EECPEngine.h"
#include "ee/EEUnit.h"
#include "ee/EEMap.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EECPResourceCenter.h"
#include "ee/EECPCombatManager.h"
#include "ee/EECPUMGatherUnitsScreenBase.h"
#include "ee/EECPPlacement.h"
#include "ee/EECPReflexes.h"
#include "ee/EEUnitAction.h"
#include "ee/EEUGRust.h"
#include "ee/UReferenceContainer.h"
#include "ee/EEAIFOUnitsToKillScoringBase.h"
#include <EEAIFunctionObjects.h>
#include "ee/EECalamityManager.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EECPUnitTileDistanceLess.h"
#include "ee/EETeam.h"
#include "ee/EEVictoryCondition.h"
#include "ee/EEVCWonder.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIPathArea.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEGameOptions.h"
#include "ee/EERules.h"
#include "ee/EECPBehavior.h"
#include "ee/EECPNextUnits.h"
#include "ee/EECPNext.h"
#include "ee/EECPExperimentTimers.h"
#include "rust/sim/cp.h"            // the legacy blocks' readers (ee_sim::cp::legacy_load)
// The existing civilization-loader helper declared in EEAResetAllPlayerInfo.h (0x2806e7).
long EEApplyCivilizationFile(UString& ioName, EEPlayer* inPlayer);

// Orders resource centers by the distance of their centres from mCenter (0x2ceb4; the sort COMDATs 0x408ae.. take it
// by value: EETileDistanceLess's copy ctor). Unit-local (provisional name)
struct EECPCenterDistanceLess {
    EECPCenterDistanceLess(const U2DPoint<long>& inCenter) : mCenter(inCenter) {}
    long DistanceSquared(const EETileCoordinate& inTile) const;
    bool operator()(EECPResourceCenter* inFirst, EECPResourceCenter* inSecond) const;

    EETileCoordinate mCenter;
};

// EECPUnitTileDistanceLess (0x2cf1e; the sort COMDATs 0x40dc4..): ee/EECPUnitTileDistanceLess.h

// chain at 0x42ce2e (wrapper)
//   object 0x8e5c70  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
//   guarded dtor (byte ptr [0x8e5c68], 1): a class static data member (statics of one class share the guard byte, one bit each) unless the ctor is guarded too (template static)
const UString EEComputerPlayer::sData("EEComputerPlayer-Data");   // class static member (owner from the key prefix; EEComputerPlayer declares `static const UString ...;`) (provisional name)

// chain at 0x42ce71 (wrapper)
//   object 0x8e5c58  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
//   guarded dtor (byte ptr [0x8e5c68], 2): a class static data member (statics of one class share the guard byte, one bit each) unless the ctor is guarded too (template static)
const UString EEComputerPlayer::sCPUOnlyFlag("EEComputerPlayer-CPU Only Flag");   // class static member (owner from the key prefix; EEComputerPlayer declares `static const UString ...;`) (provisional name)

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

// Creates a named computer player with the given player id and initialises its AI tuning and managers
EEComputerPlayer::EEComputerPlayer(const UWideString& inName, unsigned long inPlayerID)
    : EEPlayer(inName, inPlayerID)
{
    this->mNextState = NULL;
    this->mUnitsState = NULL;
    this->Init();
    this->mComputerPlayer = dynamic_cast<EEComputerPlayer*>(this);
}

// Sets the AI's combat state, clearing the next manager-enable time when the state changes
void EEComputerPlayer::SetCombatState(unsigned long inCombatState)
{
    if (inCombatState != this->mCombatState)
        this->mNextEnableTime = 0;
    this->mCombatState = inCombatState;
}

// Creates an unnamed computer player (for loading) and initialises its AI tuning and managers
EEComputerPlayer::EEComputerPlayer()
{
    this->mNextState = NULL;
    this->mUnitsState = NULL;
    this->Init();
    this->mComputerPlayer = dynamic_cast<EEComputerPlayer*>(this);
}

// Destroys the computer player: releases the units queued to kill, deletes its managers and resource centers
EEComputerPlayer::~EEComputerPlayer()
{
    for (long i = this->mUnitsToKill.size() - 1; i >= 0; i--) {
        if (this->mUnitsToKill[i] != NULL)
            this->mUnitsToKill[i]->Dereference();
    }
    this->mUnitsToKill.clear();
    delete this->GetCombatManager();
    delete this->mPlacement;
    DeleteVectorContents(this->mResourceCenters);
    EECPNextDeleteState(this);
    EECPUnitsDelete(this);
}

// Adds a resource amount to the player and records it as spent with the AI's resource manager
void EEComputerPlayer::AddResource(long inIndex, long inAmount)
{
    EEPlayer::AddResource(inIndex, inAmount);
    if (gCPTraceOn)
        EECPTraceIncome(this, inIndex, inAmount);
    if (this->mNextState != NULL)
        EECPNextIncome(this, inIndex, inAmount);   // the next engine's view (ee/EECPNext.h)
}

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
            if (gWorld.mTeams[i].mVictoryCondition->HasEnoughWonders() && !gWorld.mTeams[i].ContainsPlayer(this->GetPlayerID()))
                theFound = true;
            else
                theFound = false;
        }
        if (!theFound && !theFlag) {
            for (unsigned long j = 1; j < gWorld.GetNumPlayers(); j++) {
                EEPlayer* thePlayer = gWorld.mPlayers[j];
                if (thePlayer->IsHuman()) {
                    long theCount = thePlayer->GetTotalPopulation(true, true);
                    if (theCount >= theOwn)
                        theCount = theOwn;
                    if (theMax <= theCount)
                        theMax = theCount;
                }
            }
            if (!EECPCheatOn(this, kAICheatPopBand))
                theMax = theOwn;   // EE_AI_CHEATS without popband: no pull toward the human population
            this->mPopulationCap = UMFloatToLong((theOwn * theOwnWeight + theMax * theOtherWeight) / (theOwnWeight + theOtherWeight));
            this->mPopulationCap = UMFloatToLong(this->GetPopLimitAdjustment() * this->mPopulationCap);
            if (this->mPopulationCap > theOwn)
                this->mPopulationCap = theOwn;   // new: never above the game's population limit (Hard's row 98 is 140%)
        } else
            this->mPopulationCap = theOwn;
    }
}

// Detaches a unit from AI control: marks it, removes it from the combat manager and sets its AI state
void EEComputerPlayer::DetachUnitFromAI(EEComplexUnit* inUnit)
{
    if (inUnit->mIsDetachedFromAI != true)
        inUnit->mIsDetachedFromAI = true;
    if (inUnit->mAIState != 290008)
        inUnit->mAIState = 290008;
}

// Groups the enabled unit types that pass the screener by AI kind into the map, allocating new type lists
void EEComputerPlayer::GetScreenedTypesByLine(std::map<long, std::vector<long>*>& ioMap, EEAIFOUnitSelectionScreener& inScreener)
{
    long i;
    EEUnitClass* theClass;
    long theValue;
    long theType;
    for (i = 0; i < EEUnitClassCount(); i++) {
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

// Tells the combat manager and then the player about a target player notice
void EEComputerPlayer::NoticeTargetPlayer(EEPlayer* inPlayer, bool inFlag)
{
    EEPlayer::NoticeTargetPlayer(inPlayer, inFlag);
}

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

// Creates a favorite-unit screener for the allowed movement kinds, with the epoch clamped to 0..14
EEAIFOFavoriteUnitScreener::EEAIFOFavoriteUnitScreener(EEComputerPlayer* inPlayer, bool inAllowAir, bool inAllowLand, bool inAllowSea,
    bool inAllowSpace, long inEpoch, long inEpochSpan)
    : EEAIFOUnitSelectionScreener(), mPlayer(inPlayer), mEpochSpan(inEpochSpan), mAllowAir(inAllowAir), mAllowSea(inAllowSea), mAllowLand(inAllowLand), mAllowSpace(inAllowSpace)
{
    if (inEpoch >= 0 && inEpoch < EEEpochCount())
        this->mEpoch = inEpoch;
    else
        this->mEpoch = 0;
}

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

// Forwards to the player's handler for this slot
void EEComputerPlayer::Virtual60()
{
    EEPlayer::Virtual60();
}

// Handles a finished unit; a land building is offered as drop site to every resource center and rebalances production
void EEComputerPlayer::UnitCompleted(EEComplexUnit* inUnit)
{
    EEPlayer::UnitCompleted(inUnit);
    if (this->mUnitsState != NULL)
        EECPUnitsOnCompleted(this, inUnit);
    if (inUnit->mUnitClass->IsLandBuilding()) {
        unsigned long i, n;
        for (i = 0, n = this->mResourceCenters.size(); i < n; i++)
            this->mResourceCenters[i]->AdoptDropSite(inUnit);
    }
}

// Counts the AI's heroes plus hero requests via the unit manager, falling back to the player's count
long EEComputerPlayer::GetHeroCount(bool inByClass, bool inRequests)
{
    EECPUGatherHeroesCount theGatherer(this, inByClass, inRequests);
    EECPGatherUnits(this, theGatherer);
    return theGatherer.mUnits + theGatherer.mRequested;
}

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
    theTown.mX = this->mPlacement->mTownCenterTile.mX;
    theTown.mY = this->mPlacement->mTownCenterTile.mY;
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
        EEComplexUnit* theFirstCapitol = NULL;   // (the unit manager's first tracked capitol)
        for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL && theFirstCapitol == NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
            if (theUnit->mUnitClass->mUnitType == 0x29 && theUnit->mComplexUnit != NULL && theUnit->mHitPoints > 0)
                theFirstCapitol = theUnit->mComplexUnit;
        if (theFirstCapitol != NULL) {
            EEComplexUnit* theTownCenter = theFirstCapitol;
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


// Returns the most frequent path area id among the tiles around the unit (0 when none has one)
unsigned short EEComputerPlayer::GetObjectRegion(EEComplexUnit* inUnit)
{
    std::map<unsigned short, unsigned long>::iterator theBest;
    std::map<unsigned short, unsigned long> theCounts;
    std::vector<EETileCoordinate> theTiles;
    EEGetSquareRingTiles(inUnit->mTileX, inUnit->mTileY, 1, theTiles);
    for (std::vector<EETileCoordinate>::iterator it = theTiles.begin(); it != theTiles.end(); ++it) {
        EEAIPathArea* theArea = gContinentManager.GetPathAreaAt(*it);
        unsigned short theID = theArea != NULL ? theArea->GetID() : 0;
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

// Returns true for a deathmatch game (resource setting 4, not a scenario)
bool EEComputerPlayer::IsDeathmatch()
{
    bool theResult = false;
    if (gWorld.mGameOptions.mResources == 4)
        theResult = gWorld.mGameOptions.mIsScenarioGame == false;
    return theResult;
}

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

// Returns the AI's population cap, working it out first if unset and inactive; the player's limit when ignoring others
long EEComputerPlayer::GetPopulationLimit(long inArg)
{
    if (this->mIgnoreOthers)
        return EEPlayer::GetPopulationLimit(inArg);
    if (this->mPopulationCap == 0 && !this->mActive)
        this->WorkOutPopulation();
    return this->mPopulationCap;
}

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

// Returns true on an island or planets map once the next island expansion time has come
bool EEComputerPlayer::IsIslandExpansionDue()
{
    return gWorld.mMap != NULL && ((gWorld.mMap->mMapCharacteristics & kRMMapIslandMap) == kRMMapIslandMap || (gWorld.mMap->mMapCharacteristics & kRMMapPlanetsMap) == kRMMapPlanetsMap)
        && gPhysics->mGameTime >= this->mNextIslandExpansionTime;
}

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
    theTile.mX = this->mPlacement->mTownCenterTile.GetX();
    theTile.mY = this->mPlacement->mTownCenterTile.GetY();
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
                        theID = theArea->GetID();
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

// Returns 50 for unit classes 0x20 and 0x1e0 while a garrison or fortress is planned, else 1
long EEComputerPlayer::GetExtraPopulation(long inUnitClass)
{
    long theResult = 1;
    (void)inUnitClass;   // (50 for citizens while the classic's garrison or fortress was planned: gone in stage 6)
    return theResult;
}

// Returns the resource center at an index
EECPResourceCenter* EEComputerPlayer::GetResourceCenter(unsigned long inIndex)
{
    return this->mResourceCenters[inIndex];
}

// New: the epoch advance the research manager has queued and not yet started (kTechNone: none)
long EECPPendingEpoch(EEComputerPlayer* inPlayer)
{
    // stage 6: an epoch researched or queued at one of the player's buildings (the research manager's queue is gone)
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* c = theUnit->mComplexUnit;
        if (c == NULL || !c->mUnitClass->IsBuilding() || !(theUnit->mHitPoints > 0))
            continue;
        if (c->mCurrentResearch != kTechNone && EEIsEpochID(c->mCurrentResearch))
            return c->mCurrentResearch;
        for (unsigned long q = 0; q < c->GetBuildQueueSize(); q++)
            if (EEIsEpochID((*(c->mBuildQueue.begin() + q)).mTechnology))
                return (*(c->mBuildQueue.begin() + q)).mTechnology;
    }
    return kTechNone;
}

// Returns true for epoch technologies; otherwise asks the player whether it can afford the cost
bool EEComputerPlayer::CanAfford(EETechnology* inTechnology, long inUnitType, long inCount)
{
    // the plain check and the strategy's reserve (ee/EECPNext.h; the C++ classic's saving rules were deleted in stage 5)
    return EECPNextCanAfford(this, inTechnology, inUnitType, inCount);   // (the plain check; the free epochs cheat)
}

// Records that the player took damage: the time, and whether a land building or a citizen was hit
void EEComputerPlayer::DamageTaken(long inPlayer, long inAmount, EEUnit* inUnit)
{
    long theCategory = inUnit->mUnitClass->mCategory;
    EEPlayer::DamageTaken(inPlayer, inAmount, inUnit);
    this->mLastDamagedTime = gPhysics->mGameTime;
    if (this->mUnitsState != NULL)
        EECPUnitsOnDamage(this, inPlayer, inAmount, inUnit);   // an event for the script (ee/EECPNextUnits.h)
    if (inUnit->mUnitClass->IsLandBuilding())
        this->mBuildingDamaged = true;
    // the experiment "navalflee" (docs/AI_RESEARCH.md §8.27; the owner: units should leave a bombardment they cannot
    // answer, fishing boats should run): EECPResourceManager.cpp EECPRMNavalFlee
    if (inPlayer > 0 && EECPNavalFleeOn(this) && !inUnit->mUnitClass->IsLandBuilding() && EECPUnitsReflexOn(this, "naval_flee")
        && inUnit->mComplexUnit != NULL
        && (unsigned long)inPlayer < gWorld.mNumPlayers && gWorld.mPlayers[inPlayer] != NULL && !this->IsAlly(gWorld.mPlayers[inPlayer]))
        EECPRMNavalFlee(this, inUnit, inUnit->mComplexUnit->mLastAttacker);
    if (theCategory == kUnitCategoryNavalCitizen || theCategory == kUnitCategoryLandCitizen) {
        this->mCitizenDamaged = true;
        if (this->mNextState != NULL && gCPTraceOn)
            EECPNextTraceHit(this, inPlayer, inUnit);   // the next engine's losses trace (nothing the game reads)
        // new: a hit by a human player's unit marks a danger zone the gatherers run from (EECPResourceManager.cpp
        // EECPRMNoteDanger; against computer players only with the experiment "flee": docs/AI_RESEARCH.md §8.22)
        if (inPlayer > 0 && theCategory == kUnitCategoryLandCitizen
            && (unsigned long)inPlayer < gWorld.mNumPlayers && gWorld.mPlayers[inPlayer] != NULL
            && !this->IsAlly(gWorld.mPlayers[inPlayer])
            && (gWorld.mPlayers[inPlayer]->IsHuman() || EECPExperiment(this, "flee") || EECPMilitiaOn(this))
            && EECPUnitsReflexOn(this, "danger")
            && (!EECPUnitsHolds(this, inUnit->mComplexUnit) || EECPUnitsReflexOn(this, "held")))
            EECPRMNoteDanger(this, inUnit, (unsigned long)inPlayer);
    }
}

// Initialises the computer player: difficulty, default favourite unit types, timers and flags, every tuning
// value from the AI behaviour records, then creates the exploration, civilization, combat and other managers
// The tuning members come from the computer player behaviour records (gCPBehaviors, db\dbcpbehavior.dat:
// 0x4e6118 + 0x38 * index). As in the original, both the attack-frequency and the attack-delay bounds read
// record 19 (Attack Frequency Boundaries); record 21 (Attack Frequency Boundary) is only set by scenario effects
void EEComputerPlayer::Init()
{
    this->mDifficulty = EEDifficultyAILevel(gWorld.mLobbyOptions.mDifficulty);   // gameoptions.yaml ai_level
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
    this->mIsCheating = EECPCheatOn(this, kAICheatCost);   // EE_AI_CHEATS (the original: always true)
    this->mCanDefend = true;
    this->mBF4E = true;
    this->mCanExplore = true;
    this->mCanGather = true;
    this->mIsCitizenPercentageSet = true;
    this->mReplaceOnLoad = false;
    this->mRegroupPending = false;
    this->mAttackStarted = false;
    this->mPlacement = new EECPPlacement(this);
    this->mCombatManager = new EECPCombatManager(this);
}

// Loads the computer player from a saved game (the current format only since stage 5): the base player, the combat
// and civilization managers, the resource centers and the tuning values; returns the first read error
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
    // stage 5 (docs/AI_RESEARCH.md §8.90): only the original's last two blocks load (from 0.229: the Learning
    // Campaign's scenarios are 0.231 files; the seventh block lacks four fields, read below from 0.232); the six older
    // blocks went (save compatibility is not a goal)
    if (theResult == kErrorOK && inMajorVersion == 0 && inMinorVersion < 0xe5)
        return kErrorFailed;
    // port: the merge of main's stages 5 and 6 (save 0.281): the computer player's block without the C++ managers'
    // timers and intelligence files (0.269, 0.271), the next engine's state block main's version 6; an older save
    // does not load
    // A retail file (to 0.250: the campaigns' scenarios) holds the deleted C++ managers' blocks in the original's layout:
    // stepped over (ee_sim::cp::legacy_load), the build manager's town centre tile kept as the placement's. A port save of
    // 0.251-0.280 holds them with the port's additions and does not load
    bool theLegacy = inMajorVersion == 0 && inMinorVersion <= EE_CP_LEGACY_LAST_VERSION;
    if (theResult == kErrorOK && !theLegacy && !EESaveHasLuaOnlyComputers(inMajorVersion, inMinorVersion))
        return kErrorFailed;
    if (theResult == kErrorOK)
        theResult = theLegacy ? ee_cp_skip_legacy_combat(&inFile, &EESaveFilePrimitive, inMajorVersion, inMinorVersion)
                              : this->mCombatManager->Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == kErrorOK && theLegacy) {
        U2DPoint<long>& theTile = this->mPlacement->mTownCenterTile;
        int32_t theXY[2] = {(int32_t)theTile.mX, (int32_t)theTile.mY};
        theResult = ee_cp_skip_legacy_civilization(&inFile, &EESaveFilePrimitive, inMajorVersion, inMinorVersion, theXY);
        theTile.mX = theXY[0];
        theTile.mY = theXY[1];
        if (theResult == kErrorOK)
            theResult = ee_cp_skip_legacy_exploration(&inFile, &EESaveFilePrimitive, inMajorVersion, inMinorVersion);
    } else if (theResult == kErrorOK)
        theResult = this->mPlacement->Load(inFile);   // (stage 6: the managers' block is the town centre tile)
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
    // (0.229-0.231, the original's seventh block: the same fields up to here, without these four)
    if (inMajorVersion > 0 || inMinorVersion >= 0xe8) {
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
    // port (0.254 on, no format change): the favourite land, sea, air and space types and their history as saved. The
    // load enters the player's epochs again (EnterEpoch), which picks new favourites at random after this Load, so the
    // loaded game trained and costed another army (seed 2's 2100 s save: class 207 for the saved 24 at the first
    // update); put back at the first tick (EEAfterLoad)
    if (theResult == kErrorOK && EESaveHasLiveState(inMajorVersion, inMinorVersion)) {
        EEComputerPlayer* theSelf = this;
        unsigned long thePlayerID = this->mPlayerID;
        unsigned long theLand = this->mFavoriteLandType, theSea = this->mFavoriteSeaType;
        unsigned long theAir = this->mFavoriteAirType, theSpace = this->mFavoriteSpaceType;
        std::vector<std::pair<unsigned long, long> > theHistory(this->mFavoriteTypeHistory.begin(), this->mFavoriteTypeHistory.end());
        EEAfterLoad([theSelf, thePlayerID, theLand, theSea, theAir, theSpace, theHistory]() {
            if (thePlayerID >= gWorld.mNumPlayers || gWorld.mPlayers[thePlayerID] == NULL
                || gWorld.mPlayers[thePlayerID]->mComputerPlayer != theSelf)
                return;   // not the world's computer player of that id any more
            theSelf->mFavoriteLandType = theLand;
            theSelf->mFavoriteSeaType = theSea;
            theSelf->mFavoriteAirType = theAir;
            theSelf->mFavoriteSpaceType = theSpace;
            theSelf->mFavoriteTypeHistory.assign(theHistory.begin(), theHistory.end());
        });
    }
    // port (0.256): the running values the original saved otherwise or not at all (Save), put back at the first tick
    // after RepairPointers (which asks for a regroup) and before the first Update (EEAfterLoad): the pop cap
    // (WorkOutPopulation's), the enable-time flag (saved as false until now: Update set the time again), the regroup
    // flag, the population time (unsaved: Update worked the population out at once) and the last damage time
    if (theResult == kErrorOK && EESaveHasRunningState(inMajorVersion, inMinorVersion)) {
        long thePopulationCap = 0;
        bool theEnableTimeSet = false;
        bool theRegroupPending = false;
        long theNextPopulationTime = 0;
        long theLastDamagedTime = 0;
        if ((theResult = inFile.Read(thePopulationCap)) == kErrorOK && (theResult = inFile.Read(theEnableTimeSet)) == kErrorOK
            && (theResult = inFile.Read(theRegroupPending)) == kErrorOK && (theResult = inFile.Read(theNextPopulationTime)) == kErrorOK)
            theResult = inFile.Read(theLastDamagedTime);
        if (theResult == kErrorOK) {
            EEComputerPlayer* theSelf = this;
            unsigned long thePlayerID = this->mPlayerID;
            EEAfterLoad([theSelf, thePlayerID, thePopulationCap, theEnableTimeSet, theRegroupPending, theNextPopulationTime,
                         theLastDamagedTime]() {
                if (thePlayerID >= gWorld.mNumPlayers || gWorld.mPlayers[thePlayerID] == NULL
                    || gWorld.mPlayers[thePlayerID]->mComputerPlayer != theSelf)
                    return;   // not the world's computer player of that id any more
                theSelf->mPopulationCap = thePopulationCap;
                theSelf->mIsEnableTimeSet = theEnableTimeSet;
                theSelf->mRegroupPending = theRegroupPending;
                theSelf->mNextPopulationTime = theNextPopulationTime;
                theSelf->mLastDamagedTime = theLastDamagedTime;
            });
        }
    }
    return theResult;
}

#pragma optimize("g", on)

// Loads the epoch's kind-7 tech-tree media for a real epoch, then the player's own epoch media
void EEComputerPlayer::EpochMediaLoaded(long inEpoch)
{
    if (EEIsEpochID(inEpoch))
        static_cast<EETechTreeEpoch*>(this->mTechnologies->Get(inEpoch))->LoadMediaOfKind(7);
    EEPlayer::EpochMediaLoaded(inEpoch);
}

static void EECPRetireObsoleteUnits(EEComputerPlayer* inPlayer);   // (below)

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
        EECPRetireObsoleteUnits(this);
    }
}

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
            EEApplyCivilizationFile(theNames[theIndex], this);
            this->mCivilizationFile = theNames[theIndex];
        }
    }
}

// Answers an ally's flare by sending the combat manager to attack at the flared tile (not in observer mode)
void EEComputerPlayer::FlareReceived(EEPlayer* inFrom, const U2DPoint<long>& inTile)
{
    // port: the seat's Lua AI gets the flare as an event (the original's combat manager attacked there)
    if (!this->mObserverMode)
        EECPUnitsOnFlare(this, inFrom != NULL ? (long)inFrom->mPlayerID : 0, inTile.mX, inTile.GetY());
}


// Rebuilds or updates the resource centers (regrouping if none exist), then refreshes combat, resources and town groups
void EEComputerPlayer::RefreshResourceCenters()
{
    if (this->mResourceCenters.size() == 0)
        this->Regroup();
    else
        this->UpdateGroups();
    if (this->mResourceCenters.size() == 0)
        this->Regroup();
    this->RefreshTownGroups();
}

// Let the combat personnel draft the units it deferred (stage 5: no personnel; the Lua AIs take their units)
void EEComputerPlayer::RefreshTownGroups()
{
}

// Passes replaced units to the combat, civilization and exploration managers and the base player; a replacing
// drop site is offered to every resource center
void EEComputerPlayer::UnitsSwapped(std::vector<EEPUnitSwap>& ioSwaps)
{
    if (this->mUnitsState != NULL)
        EECPUnitsOnSwapped(this, ioSwaps);   // the held set and the script's ids follow the new units
    for (std::vector<EEPUnitSwap>::iterator it = ioSwaps.begin(); it != ioSwaps.end(); it++) {
        EEComplexUnit* theUnit = it->mNew;
        if (theUnit->GetUnitClass()->IsDropSite()) {
            for (unsigned long i = 0, n = this->mResourceCenters.size(); i < n; i++)
                this->mResourceCenters[i]->AdoptDropSite(theUnit);
        }
    }
    EEPlayer::UnitsSwapped(ioSwaps);
}

// Clears the player: deletes its resource centers, resets the civilization manager and recreates the
// exploration and combat managers
void EEComputerPlayer::Clear()
{
    EEPlayer::Clear();
    DeleteVectorContents(this->mResourceCenters);
    this->mIsStarted = false;
    this->mPlacement->Reset();
    this->mDangerZones.clear();
    delete this->mCombatManager;
    this->mCombatManager = new EECPCombatManager(this);
}

// After loading, restores the managers and resource centers, rebases saved times to the game clock, fixes the
// attack bounds and missing favourite types, and flags stranded units; returns the first error
// port: the computer players repaired while gDeferComputerPlayerRestore was set, with EEPlayer::RepairPointers' result
static std::vector<std::pair<EEComputerPlayer*, long> > sDeferredRestores;

long EEComputerPlayer::RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    long theResult = EEPlayer::RepairPointers(inMajorVersion, inMinorVersion);
    if (gDeferComputerPlayerRestore) {   // port: EEWorld::CreatePlayers restores the managers once every player is repaired
        sDeferredRestores.push_back(std::make_pair(this, theResult));
        return theResult;
    }
    return this->RestoreManagers(inMajorVersion, inMinorVersion, theResult);
}

// port: set by EEWorld::CreatePlayers while it repairs the players of a loaded game from save version 0.260 on; the
// computer players' managers are then restored after every player's RepairPointers, not after their own. A manager's
// restore re-finds its tasks' areas (EECPILocation::Update), and a task in another player's town found none when
// that player's towns were not rebuilt yet (seed 3's 1350 s save: player 3's attack sub group lost its task's town,
// player 2 having been repaired first, and drew a random number the saved game did not)
bool gDeferComputerPlayerRestore = false;

void EECPRestoreDeferredManagers(unsigned long inMajorVersion, unsigned long inMinorVersion)
{
    std::vector<std::pair<EEComputerPlayer*, long> > theRestores;
    theRestores.swap(sDeferredRestores);
    for (size_t i = 0; i < theRestores.size(); i++)
        theRestores[i].first->RestoreManagers(inMajorVersion, inMinorVersion, theRestores[i].second);
}

// the rest of RepairPointers (EXE 0x43cff8 after the EEPlayer call): the managers, the resource centres, the timers
long EEComputerPlayer::RestoreManagers(unsigned long inMajorVersion, unsigned long inMinorVersion, long inResult)
{
    long theResult = inResult;
    if (theResult == kErrorOK)
        theResult = this->mPlacement->Restore();
    if (theResult == kErrorOK) {
        this->mNextAttackTime += gPhysics->mGameTime;
        for (unsigned long i = 0, n = this->mResourceCenters.size(); i < n; i++) {
            if (theResult != kErrorOK)
                break;
            theResult = this->mResourceCenters[i]->Restore(inMajorVersion, inMinorVersion);
        }
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
#include "ee/EEUGRust.h"
#include "ee/EEUUUnitFunctor.h"
#include "ee/EEUnitUtilities.h"

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
            EEUnitGoal* theGoal = EEUGRust::NewLocation(theTransformation.GetX(), theTransformation.GetY(),
                theTransformation.GetZ(), false, true, false, 0.0f, false, false);
            inUnit->QueueGoal(theGoal, 0);
            this->DetachUnitFromAI(inUnit);
        }
    }
}


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

// From the Atomic - WW1 epoch on, kills the player's living obsolete units of 17 unit types that no human player can
// see (EE_AI_CHEATS retire; extracted from the deleted EECPPersonnel::RetireObsoleteUnits 0x48707b, stage 5)
static void EECPRetireObsoleteUnits(EEComputerPlayer* inPlayer)
{
    std::vector<EEPlayer*> thePlayers;
    std::vector<EEComplexUnit*> theUnits;
    long theEpoch = inPlayer->mTechnologies->mCurrentEpoch;
    long i;
    if (!EECPCheatOn(inPlayer, kAICheatRetire) || theEpoch < kTechEpochAtomicWW1)
        return;
    for (i = (long)gWorld.mNumPlayers - 1; i > 0; i--) {
        if (gWorld.mPlayers[i] != NULL && gWorld.mPlayers[i]->IsHuman())
            thePlayers.push_back(gWorld.GetPlayer(i));
    }
    if (thePlayers.size() == 0)
        return;
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        EEComplexUnit* theComplexUnit = theUnit->mComplexUnit;
        if (theComplexUnit == NULL || theComplexUnit->GetTechTreeNode() == NULL
            || !theComplexUnit->GetTechTreeNode()->GetIsObsolete())
            continue;
        switch (theComplexUnit->mUnitClass->mUnitType) {
        case kUnitTypeArch06CavalryArcher:
        case kUnitTypeGunSpear04StoneThrower:
        case kUnitTypeArch05CrossBow:
        case kUnitTypeArch06LongBow:
        case kUnitTypeInf04Phalanx:
        case kUnitTypeArcheryRange:
        case kUnitTypeStable:
        case kUnitTypeInf05Pilum:
        case kUnitTypeGunSiege06Trebuchet:
        case kUnitTypeGunSiege09SerpentineCannon:
        case kUnitTypeCav06Knight:
        case kUnitTypeGunCannon09BronzeCannon:
        case kUnitTypeGunSpear06Balistae:
        case kUnitTypeS09RoyalGalleon:
        case kUnitTypeCav07CarabineersGun:
        case kUnitTypeCav09ImperialCuirassierSword:
        case kUnitTypeInf08Halberdier: {
            // retired only when no human player sees it (was: when any one human does not)
            bool theFound = theComplexUnit->mHitPoints > 0;
            for (i = (long)thePlayers.size() - 1; i >= 0 && theFound; i--)
                theFound = !EEIsSeenBy(theComplexUnit, thePlayers[i], true);
            if (theFound)
                theUnits.push_back(theComplexUnit);
            break;
        }
        }
    }
    for (i = (long)theUnits.size() - 1; i >= 0; i--)
        theUnits[i]->Kill(true, false, false);
}

// Tells the research manager about a new epoch, enters it if it is a real epoch, then runs the base handling
void EEComputerPlayer::EpochReached(long inEpoch)
{
    // (the research manager's EpochReached: the reached epoch's SOL count starts over)
    if (EEIsEpochID(inEpoch) && !gLoadingSavedGame && this->mTechnologies != NULL)
        this->mTechnologies->mEpochs[this->mTechnologies->mCurrentEpoch].mCurrentSOL = 0;
    if (EEIsEpochID(inEpoch))
        this->EnterEpoch(inEpoch);
    EEPlayer::EpochReached(inEpoch);
}

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

// Schedules the next island expansion the difficulty's island timer (minutes) from now
void EEComputerPlayer::SetNextIslandExpansionTime()
{
    unsigned long theNow = gPhysics->mGameTime;
    this->mNextIslandExpansionTime = this->GetIslandExpansionTimer() * 60000 + theNow;
}

long EEComputerPlayer::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theCount;
    unsigned long i;
    long theResult = EEPlayer::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == kErrorOK)
        theResult = this->mCombatManager->Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == kErrorOK)
        theResult = this->mPlacement->Save(outFile);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mLBD4);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mFavoriteLandType);
    // the flag is saved as false (a loaded game schedules its next manager-enable check afresh), without
    // clearing it in the running game
    bool theEnableTimeSet = false;
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mNextEnableTime);
    if (theResult == kErrorOK)
        theResult = outFile.Write(theEnableTimeSet);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mBuildingDamaged);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mCitizenDamaged);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mIsStarted);
    if (theResult == kErrorOK) {
        unsigned long theTime = this->mNextAttackTime - gPhysics->mGameTime;
        theResult = outFile.Write(theTime);
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
                theResult = outFile.Write((this->mFavoriteTypeHistory.begin() + i)->first);
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
    // port (0.256): the running values (Load), without changing the running game
    if (theResult == kErrorOK && EESaveHasRunningState(inMajorVersion, inMinorVersion)) {
        if ((theResult = outFile.Write(this->mPopulationCap)) == kErrorOK && (theResult = outFile.Write(this->mIsEnableTimeSet)) == kErrorOK
            && (theResult = outFile.Write(this->mRegroupPending)) == kErrorOK && (theResult = outFile.Write(this->mNextPopulationTime)) == kErrorOK)
            theResult = outFile.Write(this->mLastDamagedTime);
    }
    return theResult;
}

// Turns the AI on or off, handing all units to the unit manager or releasing them when the state changes
void EEComputerPlayer::SetActive(bool inActive)
{
    if (inActive && !this->mActive) {
        // (the unit manager's AddAllUnits: the town centre tile from the units the player has)
        for (EEUnit* theUnit = this->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
            if (theUnit->mComplexUnit != NULL)
                this->mPlacement->UnitAdded(theUnit->mComplexUnit);
    }
    this->mActive = inActive;
}

// Sets the share of the population kept as citizens and marks it as set
void EEComputerPlayer::SetCitizenPercentage(float inValue)
{
    this->mCitizenPercentage = inValue;
    this->mIsCitizenPercentageSet = true;
}

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

// Queues a stranded unit to be killed on the next update; a stranded citizen is also reported as a threat
void EEComputerPlayer::UnitStranded(EEComplexUnit* inUnit)
{
    inUnit->Reference();
    this->mUnitsToKill.push_back(inUnit);
}

// Takes a resource amount from the player, ignoring zero or negative amounts
void EEComputerPlayer::TakeResource(long inIndex, long inAmount)
{
    if (inAmount > 0)
        EEPlayer::TakeResource(inIndex, inAmount);
}

// Pays for a technology, except epoch advances, which are free for the computer player
void EEComputerPlayer::Pay(EETechnology* inTechnology, long inUnitType, float inFraction)
{
    if (!EEIsEpochID(inTechnology->mData->mID) || !EECPCheatOn(this, kAICheatEpochs)) {
        if (gCPTraceOn) {   // TEST ONLY (EE_AI_EXP spending): what the payment buys, by kind (the stock delta, as paid)
            long theBefore[5], theCost[5];
            for (long r = 0; r < 5; r++)
                theBefore[r] = this->mResourceCaps[kResourceFood + r];
            EEPlayer::Pay(inTechnology, inUnitType, inFraction);
            for (long r = 0; r < 5; r++)
                theCost[r] = theBefore[r] - this->mResourceCaps[kResourceFood + r];
            long theKind = 3;
            // (the class the technology makes; inUnitType is the producer's)
            long theMade = (long)inTechnology->mData->mUnitClass;
            EEUnitClass* theClass = theMade > 0 && (unsigned long)theMade < EEUnitClassCount() ? gUnitClassTable[theMade] : NULL;
            if (EEIsEpochID(inTechnology->mData->mID))
                theKind = 4;
            else if (theClass != NULL && inTechnology->mData->mUnitClass != 0) {
                if (theClass->mCategory == kUnitCategoryLandCitizen)
                    theKind = 0;
                else if (theClass->mCategory == kUnitCategoryNavalCitizen)
                    theKind = 5;
                else if (theClass->IsBuilding())
                    theKind = 2;
                else
                    theKind = theClass->mIsAttackUnit || theClass->mIsDefenseUnit ? 1 : 7;
            }
            EECPTraceSpend(this, theKind, theCost);
            return;
        }
        EEPlayer::Pay(inTechnology, inUnitType, inFraction);
    }
}

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
                    (void)theAmount;   // (the resource manager's needs ledger: gone in stage 6)
                }
            }
        }
    }
    EEPlayer::PayImprovement(inClass, inImprovement);
}


// Registers a new unit with the player and, while the AI is active, with the civilization and combat managers
void EEComputerPlayer::UnitPlaced(EEComplexUnit* inUnit)
{
    EEPlayer::UnitPlaced(inUnit);
    if (this->mUnitsState != NULL)
        EECPUnitsOnPlaced(this, inUnit);
    if (this->mActive) {
        this->mPlacement->UnitAdded(inUnit);
    }
}

// Forgets a unit leaving the player: base bookkeeping, then the civilization, combat and exploration managers
// and, for a land building, every resource center
void EEComputerPlayer::UnitTaken(RTSUnit* inUnit, bool inSwapped)
{
    EEPlayer::UnitTaken(inUnit, inSwapped);
    EEComplexUnit* theUnit = static_cast<EEComplexUnit*>(inUnit);
    if (this->mUnitsState != NULL)
        EECPUnitsOnTaken(this, theUnit, inSwapped);
    if (gCPTraceOn && !inSwapped)
        EECPTraceUnitLost(this, theUnit);
    if (gCPTraceOn && !inSwapped && this->mNextState != NULL)
        EECPNextTraceDeath(this, theUnit);   // the next engine's losses trace (nothing the game reads)
    if (this->mNextState != NULL && !inSwapped && theUnit->mUnitClass != NULL)
        EECPNextEventAdd(this, kNextEventUnitLost, theUnit->mUnitClass->mUnitType, 1);   // the next engine's view
    if (theUnit->mUnitClass->IsLandBuilding()) {
        unsigned long i, n;
        for (i = 0, n = this->mResourceCenters.size(); i < n; i++)
            this->mResourceCenters[i]->ReleaseUnit(theUnit);
    }
}

// an idle transport not a script's (the resource manager's TransportIdle, kept in stage 6): the citizens it was loading
// stop, and it unloads at its rendezvous; an unloading one that was detached tries its destination again
static void EECPTransportIdle(EEComputerPlayer* inPlayer, EEComplexUnit* inUnit)
{
    EEUnitGoal* theGoal = inUnit->mActiveGoal;
    EEUGRust* theLoad = EELoadTransportGoalCast(theGoal);
    EEUGRust* theUnload = EEUnloadGoalCast(theGoal);
    bool theChanged = false;
    if (theLoad != NULL) {
        if (theLoad->GetNumLoadUnits() > 0) {
            for (EEUnit* theCitizen = inPlayer->mFirstUnit; theCitizen != NULL; theCitizen = static_cast<EEUnit*>(theCitizen->mNext)) {
                EEComplexUnit* theUnit = theCitizen->mComplexUnit;
                long theType = theCitizen->mUnitClass->mUnitType;
                if (theUnit == NULL || (theType != 0x20 && theType != 0x1e0))
                    continue;
                if (!theLoad->HasLoadUnit(theUnit))
                    continue;
                theUnit->ClearGoalQueue(0);
                theUnit->StartAction(kActionIdle);
                theUnit->mUnitClass->ActionChanged(theUnit);
                theChanged = true;
            }
            if (theChanged) {
                if (inUnit->GetNumCargoUnits() > 0 && theLoad->EvaluateCondition(inUnit, kTriggerHaveReachedRendezvousPoint)) {
                    U2DPoint<long> theTile = theLoad->GetLoadContainerTile();
                    inUnit->UnloadCargoAtTile(theTile.GetX(), theTile.GetY(), true);
                }
                inUnit->ClearGoalQueue(0);
            }
        }
    } else if (theUnload != NULL) {
        if (inUnit->GetNumCargoUnits() > 0 && inUnit->mIsDetachedFromAI) {
            long theX;
            long theY;
            theUnload->GetUnloadTransport(&theX, &theY);
            EEUGRust* theNew = EEUGRust::NewUnloadTransport(inUnit, theX, theY, true);
            if (theNew->CanUnloadAtDestination())
                inUnit->QueueGoal(theNew, 0);
        }
    }
}

// Handles a unit finishing a goal: idles transports (sending empty damaged ones home), returns detached units to
// the combat manager, marks citizens as idle gatherers and drops nearly idle units from their group
void EEComputerPlayer::UnitGoalDone(EEComplexUnit* inUnit)
{
    unsigned long theGoals = inUnit->mGoalQueues[0].mGoals.size();
    if (inUnit->GetClass()->IsTransport() && !EECPUnitsHolds(this, inUnit)) {
        EECPTransportIdle(this, inUnit);
        if (theGoals == 1 && inUnit->IsRepairable() && inUnit->GetNumCargoUnits() == 0)
            this->SendToNearestBase(inUnit);
        if (inUnit->GetGoalState() != NULL && inUnit->GetGoalState()->GetKind() == kGoalLoadTransport && inUnit->GetNumCargoUnits() == 0)
            inUnit->ClearGoalQueue(0);
    }
    theGoals = inUnit->mGoalQueues[0].mGoals.size();
    if (this->mUnitsState != NULL && theGoals <= 1)
        EECPUnitsOnGoalDone(this, inUnit);
    if (EECPUnitsHolds(this, inUnit)) {   // the script's unit (ee/EECPNextUnits.h): no runner takes it back
        if (theGoals <= 1 && inUnit->GetActiveGroup() != NULL)
            inUnit->GetActiveGroup()->RemoveUnit(inUnit);
        return;
    }
    if (inUnit->GetIsDetachedFromAI() && theGoals <= 1) {
        inUnit->mIsDetachedFromAI = false;
    }
    if (theGoals <= 1 && inUnit->GetActiveGroup() != NULL)
        inUnit->GetActiveGroup()->RemoveUnit(inUnit);
}


// Unloads the epoch's kind-7 tech-tree media for a real epoch, then the player's own epoch media
void EEComputerPlayer::EpochMediaUnloaded(long inEpoch)
{
    if (EEIsEpochID(inEpoch))
        static_cast<EETechTreeEpoch*>(this->mTechnologies->Get(inEpoch))->UnloadMediaOfKind(7);
    EEPlayer::EpochMediaUnloaded(inEpoch);
}

// Runs the AI tick: one-time start-up on the first call, then the enable, attack-state, population and
// expansion timers and the manager updates; finally kills the units queued as stranded
void EEComputerPlayer::Update(unsigned long inTime)
{
    UWideString theUnused1;
    UWideString theUnused2;
    EECPTraceScope theScope("cp");
    if (!this->mIsStarted) {
        // port: the seat's script loads first (ee/EECPEngine.h); a seat whose AI cannot run stands idle (logged)
        if (!this->mObserverMode)
            EECPNextPrepare(this);
        if (EECPCheatOn(this, kAICheatReveal))
            gWorld.mMap->ExploreUnseen(this);
        EE_CP_TRACE(this, "start", "difficulty=%lu personality=%s cheats=%s cheating=%d cost100=%ld extra=%ld aggression=%ld demand=%d exp=%s engine=%s%s%s",
            this->mDifficulty, EECPPersonality(this).mID.c_str(), EECPCheatNames(this), this->mIsCheating ? 1 : 0, this->ScaleCost(100),
            EECPCheatOn(this, kAICheatExtra) ? this->mExtraResources : 0L, (long)this->mComputerAggression, EECPDemandSplitOn(this) ? 1 : 0, EECPExperimentNames(this),
            EECPEngineName(this).c_str(), EECPEngineSeesAll(this) ? " sees_all=1" : "",
            EECPEngineFillsGarrison(this) ? " fills_garrison=1" : "");
        this->SetNextIslandExpansionTime();
        if (this->mResourceCenters.size() == 0)
            this->Regroup();
        if (gGameOptions.mLoadType == 0)
            this->ChooseCivilization();
        const long theBonus = EECPCheatOn(this, kAICheatExtra) ? this->mExtraResources : 0;
        this->mResourceCaps[kResourceGold] += theBonus;
        this->mResourceCaps[kResourceIron] += theBonus;
        this->mIsStarted = true;
    }
    // port: the next engine's tick (ee/EECPEngine.h; docs/AI_LUA_PLAN.md S0): the executors, then the seat's Lua AI.
    // The original's decision path (the manager enable timer, the attack, expansion and population timers, the
    // civilization, combat, diplomacy and exploration managers' updates) was deleted in stage 5.
    if (!this->mObserverMode && this->mFirstUnit != NULL) {
        EECPTraceScope theNext("next");
        EECPNextUpdate(this, inTime);
    }
    for (long i = this->mUnitsToKill.size() - 1; i >= 0; i--) {
        this->mUnitsToKill[i]->Kill(true, false, false);
        this->mUnitsToKill[i]->Dereference();
    }
    this->mUnitsToKill.clear();
}

// Assigns ungrouped units to resource centers, creating new inactive centers for the rest, then re-sorts the
// centers by distance from the town center and activates gaps; new centers only gather units if centers existed
void EEComputerPlayer::UpdateGroups()
{
    std::vector<EEUnit*> theUnits;
    U2DPoint<long> theCenter;
    EECPResourceCenter* theNewCenter = NULL;
    theCenter = this->mPlacement->mTownCenterTile;
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

// Has the combat manager respond to an enemy wonder being started
void EEComputerPlayer::WonderStarted(EEComplexUnit* inUnit)
{
    // port: the seat's Lua AI answers from ai.wonders (classic_lua's update_wonders; the original's combat manager
    // responded here)
    (void)inUnit;
}

// Returns the number of teams in the world
unsigned long EEWorld::GetNumTeams()
{
    return this->mTeams.size();
}

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

// port (ee/EECPExperimentTimers.h): the experiments' timers and the resource state, saved from 0.258 and 0.265 (main's
// stages 5 and 6 left the timers unused; the block keeps its layout)
EECPExperimentTimers gCPExperimentTimers;

template <class T>
static long EECPSaveTimerArray(FSFile& outFile, const T (&inValues)[EECPExperimentTimers::kPlayers])
{
    long theResult = outFile.Write((unsigned long)EECPExperimentTimers::kPlayers);
    for (long i = 0; i < EECPExperimentTimers::kPlayers && theResult == kErrorOK; i++)
        theResult = outFile.Write(inValues[i]);
    return theResult;
}

template <class T>
static long EECPLoadTimerArray(FSFile& inFile, T (&outValues)[EECPExperimentTimers::kPlayers])
{
    unsigned long theCount = 0;
    long theResult = inFile.Read(theCount);
    for (unsigned long i = 0; i < theCount && theResult == kErrorOK; i++) {
        T theValue = 0;
        theResult = inFile.Read(theValue);
        if (i < (unsigned long)EECPExperimentTimers::kPlayers)
            outValues[i] = theValue;
    }
    return theResult;
}

long EECPSaveExperimentTimers(FSFile& outFile)
{
    const EECPExperimentTimers& t = gCPExperimentTimers;
    long theResult = EECPSaveTimerArray(outFile, t.mBoats);
    if (theResult == kErrorOK) theResult = EECPSaveTimerArray(outFile, t.mBoatHold);
    if (theResult == kErrorOK) theResult = EECPSaveTimerArray(outFile, t.mSettleLast);
    if (theResult == kErrorOK) theResult = EECPSaveTimerArray(outFile, t.mGuardLast);
    if (theResult == kErrorOK) theResult = EECPSaveTimerArray(outFile, t.mMilitiaLast);
    if (theResult == kErrorOK) theResult = EECPSaveTimerArray(outFile, t.mNavalFleeLast);
    if (theResult == kErrorOK) theResult = EECPSaveTimerArray(outFile, t.mScheduleEpoch);
    if (theResult == kErrorOK) theResult = EECPSaveTimerArray(outFile, t.mScheduleStart);
    if (theResult == kErrorOK) theResult = EECPSaveTimerArray(outFile, t.mThreatChecked);
    if (theResult == kErrorOK) theResult = EECPSaveTimerArray(outFile, t.mThreatUntil);
    return theResult;
}

// read into a copy (zero where the file has fewer players), put in place at the loaded game's first tick
long EECPLoadExperimentTimers(FSFile& inFile)
{
    EECPExperimentTimers t = {};
    long theResult = EECPLoadTimerArray(inFile, t.mBoats);
    if (theResult == kErrorOK) theResult = EECPLoadTimerArray(inFile, t.mBoatHold);
    if (theResult == kErrorOK) theResult = EECPLoadTimerArray(inFile, t.mSettleLast);
    if (theResult == kErrorOK) theResult = EECPLoadTimerArray(inFile, t.mGuardLast);
    if (theResult == kErrorOK) theResult = EECPLoadTimerArray(inFile, t.mMilitiaLast);
    if (theResult == kErrorOK) theResult = EECPLoadTimerArray(inFile, t.mNavalFleeLast);
    if (theResult == kErrorOK) theResult = EECPLoadTimerArray(inFile, t.mScheduleEpoch);
    if (theResult == kErrorOK) theResult = EECPLoadTimerArray(inFile, t.mScheduleStart);
    if (theResult == kErrorOK) theResult = EECPLoadTimerArray(inFile, t.mThreatChecked);
    if (theResult == kErrorOK) theResult = EECPLoadTimerArray(inFile, t.mThreatUntil);
    if (theResult == kErrorOK)
        EEAfterLoad([t]() { gCPExperimentTimers = t; });
    return theResult;
}

// the claim ranges' two arrays, then per player id (gWorld.mNumPlayers of them) the danger zones of its computer
// player's resource manager: the count, then each zone's tile and game ms (a player with none writes 0)
long EECPSaveResourceState(FSFile& outFile)
{
    long theResult = EECPSaveTimerArray(outFile, gCPExperimentTimers.mClaimSteps);
    if (theResult == kErrorOK)
        theResult = EECPSaveTimerArray(outFile, gCPExperimentTimers.mClaimLast);
    if (theResult == kErrorOK)
        theResult = outFile.Write((unsigned long)gWorld.mNumPlayers);
    for (unsigned long p = 0; p < gWorld.mNumPlayers && theResult == kErrorOK; p++) {
        EEPlayer* thePlayer = gWorld.mPlayers[p];
        EEComputerPlayer* theCP = thePlayer != NULL ? thePlayer->mComputerPlayer : NULL;
        EEComputerPlayer* theManager = theCP;   // (the danger zones are the computer player's since main's stage 6)
        unsigned long theCount = theManager != NULL ? (unsigned long)theManager->mDangerZones.size() : 0;
        theResult = outFile.Write(theCount);
        for (unsigned long z = 0; z < theCount && theResult == kErrorOK; z++) {
            theResult = outFile.Write((long)theManager->mDangerZones[z].first.mX);
            if (theResult == kErrorOK)
                theResult = outFile.Write((long)theManager->mDangerZones[z].first.mY);
            if (theResult == kErrorOK)
                theResult = outFile.Write((unsigned long)theManager->mDangerZones[z].second);
        }
    }
    return theResult;
}

long EECPLoadResourceState(FSFile& inFile)
{
    EECPExperimentTimers t = {};
    long theResult = EECPLoadTimerArray(inFile, t.mClaimSteps);
    if (theResult == kErrorOK)
        theResult = EECPLoadTimerArray(inFile, t.mClaimLast);
    unsigned long thePlayers = 0;
    if (theResult == kErrorOK)
        theResult = inFile.Read(thePlayers);
    std::vector<std::vector<std::pair<U2DPoint<long>, unsigned long> > > theZones;
    for (unsigned long p = 0; p < thePlayers && theResult == kErrorOK; p++) {
        unsigned long theCount = 0;
        theResult = inFile.Read(theCount);
        theZones.push_back(std::vector<std::pair<U2DPoint<long>, unsigned long> >());
        for (unsigned long z = 0; z < theCount && theResult == kErrorOK; z++) {
            long theX = 0, theY = 0;
            unsigned long theTime = 0;
            theResult = inFile.Read(theX);
            if (theResult == kErrorOK)
                theResult = inFile.Read(theY);
            if (theResult == kErrorOK)
                theResult = inFile.Read(theTime);
            theZones.back().push_back(std::make_pair(U2DPoint<long>(theX, theY), theTime));
        }
    }
    if (theResult == kErrorOK)
        EEAfterLoad([t, theZones]() {   // after the experiment timers' fixup, which zeroes the claim ranges
            for (long i = 0; i < EECPExperimentTimers::kPlayers; i++) {
                gCPExperimentTimers.mClaimSteps[i] = t.mClaimSteps[i];
                gCPExperimentTimers.mClaimLast[i] = t.mClaimLast[i];
            }
            for (unsigned long p = 0; p < theZones.size() && p < gWorld.mNumPlayers; p++) {
                EEPlayer* thePlayer = gWorld.mPlayers[p];
                EEComputerPlayer* theCP = thePlayer != NULL ? thePlayer->mComputerPlayer : NULL;
                if (theCP != NULL)
                    theCP->mDangerZones = theZones[p];
            }
        });
    return theResult;
}

