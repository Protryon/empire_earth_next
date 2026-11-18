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
    return ee_cp_scale_cost(this, inCost);
}







// Returns true for a deathmatch game (resource setting 4, not a scenario)
bool EEComputerPlayer::IsDeathmatch()
{
    bool theResult = false;
    if (gWorld.mGameOptions.mResources == 4)
        theResult = gWorld.mGameOptions.mIsScenarioGame == false;
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

// Initialises the computer player: the difficulty, the default favourite unit types, the timers and flags and every
// tuning value from the AI behaviour records (gCPBehaviors, db\dbcpbehavior.dat), then the placement and the combat
// manager

void EEComputerPlayer::Init()
{
    static_assert(sizeof(EeCpBehavior) == sizeof(EECPBehavior), "ee_sim::cp::init reads gCPBehaviors as EeCpBehavior");
    // the tuning values, timers and flags (ee_sim::cp::init); the difficulty is gameoptions.yaml's ai_level, the cost
    // cheat EE_AI_CHEATS' (the original: always on)
    ee_cp_init(this, reinterpret_cast<const EeCpBehavior*>(gCPBehaviors), EEDifficultyAILevel(gWorld.mLobbyOptions.mDifficulty),
               this->mPlayerID, EECPCheatOn(this, kAICheatCost));
    this->mPlacement = new EECPPlacement(this);
    this->mCombatManager = new EECPCombatManager(this);
}

// The C++ parts of Load and Save (ee_sim::cp::save): the file's primitives (EESavePrimitive), the combat manager's
// and the placement's blocks, the resource centres, the favourite types' history
struct EECPSaveContext {
    EEComputerPlayer* mPlayer;
    FSFile* mFile;
    unsigned long mMajorVersion;
    unsigned long mMinorVersion;
};

static int32_t EECPSaveOperation(void* inContext, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io)
{
    EECPSaveContext& theContext = *static_cast<EECPSaveContext*>(inContext);
    EEComputerPlayer* thePlayer = theContext.mPlayer;
    FSFile& theFile = *theContext.mFile;
    switch (inOp) {
    case EE_CP_SAVE_COMBAT_LOAD:
        return thePlayer->mCombatManager->Load(theContext.mMajorVersion, theContext.mMinorVersion, theFile);
    case EE_CP_SAVE_PLACEMENT_LOAD:
        return thePlayer->mPlacement->Load(theFile);
    case EE_CP_SAVE_TOWN_TILE:
        io->i = inIndex == 0 ? thePlayer->mPlacement->mTownCenterTile.mX : thePlayer->mPlacement->mTownCenterTile.mY;
        return kErrorOK;
    case EE_CP_SAVE_SET_TOWN_TILE:
        (inIndex == 0 ? thePlayer->mPlacement->mTownCenterTile.mX : thePlayer->mPlacement->mTownCenterTile.mY) = (long)io->i;
        return kErrorOK;
    case EE_CP_SAVE_CENTER_LOAD: {
        EECPResourceCenter* theCenter = new EECPResourceCenter(thePlayer, NULL);
        long theResult = theCenter->Load(theContext.mMajorVersion, theContext.mMinorVersion, theFile);
        thePlayer->mResourceCenters.push_back(theCenter);
        return theResult;
    }
    case EE_CP_SAVE_HISTORY_RESIZE:
        thePlayer->mFavoriteTypeHistory.resize((size_t)io->i, std::make_pair(0UL, 0L));
        return kErrorOK;
    case EE_CP_SAVE_HISTORY_FIRST:
        io->i = (int64_t)thePlayer->mFavoriteTypeHistory[inIndex].first;
        return kErrorOK;
    case EE_CP_SAVE_HISTORY_SECOND:
        io->i = (int64_t)thePlayer->mFavoriteTypeHistory[inIndex].second;
        return kErrorOK;
    case EE_CP_SAVE_HISTORY_SET_FIRST:
        thePlayer->mFavoriteTypeHistory[inIndex].first = (unsigned long)io->i;
        return kErrorOK;
    case EE_CP_SAVE_HISTORY_SET_SECOND:
        thePlayer->mFavoriteTypeHistory[inIndex].second = (long)io->i;
        return kErrorOK;
    case EE_CP_SAVE_COMBAT_SAVE:
        return thePlayer->mCombatManager->Save(theContext.mMajorVersion, theContext.mMinorVersion, theFile);
    case EE_CP_SAVE_PLACEMENT_SAVE:
        return thePlayer->mPlacement->Save(theFile);
    case EE_CP_SAVE_CENTERS:
        io->i = (int64_t)thePlayer->mResourceCenters.size();
        return kErrorOK;
    case EE_CP_SAVE_CENTER_SAVE:
        return thePlayer->mResourceCenters[inIndex]->Save(theContext.mMajorVersion, theContext.mMinorVersion, theFile);
    case EE_CP_SAVE_HISTORY_COUNT:
        io->i = (int64_t)thePlayer->mFavoriteTypeHistory.size();
        return kErrorOK;
    }
    return EESavePrimitive(theFile, inOp, io);
}

// Loads the computer player from a saved game: the base player, then ee_sim::cp::save's block (the combat manager's
// and the placement's, or a retail file's legacy blocks; the fields, the resource centres, the history; the
// refusals of the formats that no longer load); returns the first read error
long EEComputerPlayer::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = EEPlayer::Load(inMajorVersion, inMinorVersion, inFile);
    EECPSaveContext theContext = {this, &inFile, inMajorVersion, inMinorVersion};
    EeCpLoaded theLoaded;
    theResult = ee_cp_load(&theContext, &EECPSaveOperation, this, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion,
                           (int32_t)theResult, &theLoaded);
    // port (0.254 on, no format change): the favourite land, sea, air and space types and their history as saved. The
    // load enters the player's epochs again (EnterEpoch), which picks new favourites at random after this Load, so the
    // loaded game trained and costed another army (seed 2's 2100 s save: class 207 for the saved 24 at the first
    // update); put back at the first tick (EEAfterLoad)
    if (theLoaded.favorites_ok && EESaveHasLiveState(inMajorVersion, inMinorVersion)) {
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
    if (theLoaded.running) {
        EEComputerPlayer* theSelf = this;
        unsigned long thePlayerID = this->mPlayerID;
        EeCpLoaded theRunning = theLoaded;
        EEAfterLoad([theSelf, thePlayerID, theRunning]() {
            if (thePlayerID >= gWorld.mNumPlayers || gWorld.mPlayers[thePlayerID] == NULL
                || gWorld.mPlayers[thePlayerID]->mComputerPlayer != theSelf)
                return;   // not the world's computer player of that id any more
            theSelf->mPopulationCap = theRunning.population_cap;
            theSelf->mIsEnableTimeSet = theRunning.enable_time_set;
            theSelf->mRegroupPending = theRunning.regroup_pending;
            theSelf->mNextPopulationTime = theRunning.next_population_time;
            theSelf->mLastDamagedTime = theRunning.last_damaged_time;
        });
    }
    return theResult;
}

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
        ee_cp_restore_fields(this, 0, gPhysics->mGameTime, true);   // the saved relative times rebased
        for (unsigned long i = 0, n = this->mResourceCenters.size(); i < n; i++) {
            if (theResult != kErrorOK)
                break;
            theResult = this->mResourceCenters[i]->Restore(inMajorVersion, inMinorVersion);
        }
        if (theResult == kErrorOK)
            ee_cp_restore_fields(this, 1, gPhysics->mGameTime, true);
    }
    ee_cp_restore_fields(this, 2, 0, theResult == kErrorOK);   // the attack bounds, the drop-off distance, a regroup
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
    ee_cp_restore_fields(this, 3, inMinorVersion, inMajorVersion > 0);   // a pre-0.229 save's one expansion timer
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
static long EECPAttackRandom(void*, long inMinimum, long inMaximum)
{
    return gPhysics->GetRandom(inMinimum, inMaximum);
}

void EEComputerPlayer::SetNextAttackTime(unsigned long inTime)
{
    long theEpochs = gWorld.mGameOptions.mEpochEnd - gWorld.mGameOptions.mEpochStart + 1;
    ee_cp_set_next_attack_time(this, inTime, theEpochs, this->IsDeathmatch(), &EECPAttackRandom, NULL);
}

// Schedules the next island expansion the difficulty's island timer (minutes) from now
void EEComputerPlayer::SetNextIslandExpansionTime()
{
    unsigned long theNow = gPhysics->mGameTime;
    this->mNextIslandExpansionTime = this->GetIslandExpansionTimer() * 60000 + theNow;
}

long EEComputerPlayer::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEPlayer::Save(inMajorVersion, inMinorVersion, outFile);
    EECPSaveContext theContext = {this, &outFile, inMajorVersion, inMinorVersion};
    return ee_cp_save(&theContext, &EECPSaveOperation, this, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion,
                      (uint32_t)gPhysics->mGameTime, (int32_t)theResult);
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

// port (ee/EECPExperimentTimers.h): the experiments' timers and the resource state, saved from 0.258 and 0.265
// (ee_sim::cp::timers: the timers are Rust's; the danger zones the computer players', over EECPStateOperation)
struct EECPStateContext {
    FSFile* mFile;
    std::vector<std::vector<std::pair<U2DPoint<long>, unsigned long> > > mZones;   // a load's, by player id
    long mX, mY;                                                                     // the zone being pushed
};

static int32_t EECPStateOperation(void* inContext, int32_t inOp, uint32_t inIndex, EeUnitsPlayerSaveIo* io)
{
    EECPStateContext& theContext = *static_cast<EECPStateContext*>(inContext);
    EEPlayer* thePlayer = inIndex < gWorld.mNumPlayers ? gWorld.mPlayers[inIndex] : NULL;
    EEComputerPlayer* theCP = thePlayer != NULL ? thePlayer->mComputerPlayer : NULL;
    switch (inOp) {
    case EE_CP_STATE_ZONES:
        io->i = theCP != NULL ? (int64_t)theCP->mDangerZones.size() : 0;
        return kErrorOK;
    case EE_CP_STATE_ZONE: {
        const std::pair<U2DPoint<long>, unsigned long>& theZone = theCP->mDangerZones[(size_t)io->i];
        io->i = io->b == 0 ? (int64_t)theZone.first.mX : io->b == 1 ? (int64_t)theZone.first.mY : (int64_t)theZone.second;
        return kErrorOK;
    }
    case EE_CP_STATE_PUSH_PLAYER:
        theContext.mZones.push_back(std::vector<std::pair<U2DPoint<long>, unsigned long> >());
        return kErrorOK;
    case EE_CP_STATE_PUSH_ZONE:
        if (io->b == 0)
            theContext.mX = (long)io->i;
        else if (io->b == 1)
            theContext.mY = (long)io->i;
        else
            theContext.mZones.back().push_back(std::make_pair(U2DPoint<long>(theContext.mX, theContext.mY), (unsigned long)io->i));
        return kErrorOK;
    }
    return EESavePrimitive(*theContext.mFile, inOp, io);
}

long EECPSaveExperimentTimers(FSFile& outFile)
{
    return ee_cp_timers_save(&outFile, &EESaveFilePrimitive);
}

// read into a copy (zero where the file has fewer players), put in place at the loaded game's first tick
long EECPLoadExperimentTimers(FSFile& inFile)
{
    long theResult = ee_cp_timers_load(&inFile, &EESaveFilePrimitive);
    if (theResult == kErrorOK)
        EEAfterLoad([]() { ee_cp_timers_apply(0); });
    return theResult;
}

long EECPSaveResourceState(FSFile& outFile)
{
    EECPStateContext theContext;
    theContext.mFile = &outFile;
    return ee_cp_resource_state_save(&theContext, &EECPStateOperation, (uint32_t)gWorld.mNumPlayers);
}

long EECPLoadResourceState(FSFile& inFile)
{
    EECPStateContext theContext;
    theContext.mFile = &inFile;
    theContext.mX = theContext.mY = 0;
    long theResult = ee_cp_resource_state_load(&theContext, &EECPStateOperation);
    if (theResult == kErrorOK) {
        std::vector<std::vector<std::pair<U2DPoint<long>, unsigned long> > > theZones;
        theZones.swap(theContext.mZones);
        EEAfterLoad([theZones]() {   // after the experiment timers' fixup, which zeroes the claim ranges
            ee_cp_timers_apply(1);
            for (unsigned long p = 0; p < theZones.size() && p < gWorld.mNumPlayers; p++) {
                EEPlayer* thePlayer = gWorld.mPlayers[p];
                EEComputerPlayer* theCP = thePlayer != NULL ? thePlayer->mComputerPlayer : NULL;
                if (theCP != NULL)
                    theCP->mDangerZones = theZones[p];
            }
        });
    }
    return theResult;
}
