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
#include "ee/EECPPlaceCapture.h"
#include "rust/sim/cp.h"

EeCpPlaceCtx EECPPlaceContext(EEComputerPlayer* inPlayer);   // EECPPlacement.cpp
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
    EECPPlaceScope theScope(kCpevWorkOutPopulation, this, true);   // CPEV (TEST ONLY)
    EeCpPlaceCtx theContext = EECPPlaceContext(this);
    ee_cp_work_out_population(&theContext);
}

// Detaches a unit from AI control: marks it, removes it from the combat manager and sets its AI state
void EEComputerPlayer::DetachUnitFromAI(EEComplexUnit* inUnit)
{
    if (inUnit->mIsDetachedFromAI != true)
        inUnit->mIsDetachedFromAI = true;
    if (inUnit->mAIState != 290008)
        inUnit->mAIState = 290008;
}


// Returns true for epoch technologies; otherwise asks the player and refuses a missing node or one of kind 5
bool EEComputerPlayer::CanProduce(long inTechnology, long inUnitClass, long inCount, bool inCheckCost, long inWonderMode, UWideString* outReason)
{
    EECPPlaceScope theScope(kCpevCanProduce, this, true);   // CPEV (TEST ONLY)
    theScope.I(inTechnology).I(inUnitClass).I(inCount).I(inCheckCost).I(inWonderMode);
    bool theScopeResult = [&]() -> bool {
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
    }();
    theScope.I(theScopeResult);
    return theScopeResult;
}

// Tells the combat manager and then the player about a target player notice
void EEComputerPlayer::NoticeTargetPlayer(EEPlayer* inPlayer, bool inFlag)
{
    EEPlayer::NoticeTargetPlayer(inPlayer, inFlag);
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




// Forwards to the player's handler for this slot
void EEComputerPlayer::Virtual60()
{
    EEPlayer::Virtual60();
}

// Handles a finished unit; a land building is offered as drop site to every resource center and rebalances production
void EEComputerPlayer::UnitCompleted(EEComplexUnit* inUnit)
{
    EECPPlaceScope theScope(kCpevUnitCompleted, this, true); theScope.CU(inUnit);   // CPEV (TEST ONLY)
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
    EECPPlaceScope theScope(kCpevRegroup, this, true);   // CPEV (TEST ONLY)
    EeCpPlaceCtx theContext = EECPPlaceContext(this);
    ee_cp_regroup(&theContext);
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
    EECPPlaceScope theScope(kCpevPendingEpoch, inPlayer, true);   // CPEV (TEST ONLY)
    EeCpPlaceCtx theContext = EECPPlaceContext(inPlayer);
    long theResult = ee_cp_pending_epoch(&theContext);
    theScope.I(theResult);
    return theResult;
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
    EECPPlaceScope theScope(kCpevDamageTaken, this, true); theScope.I(inPlayer).I(inAmount).U(inUnit);   // CPEV (TEST ONLY)
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

// Enters an epoch: researches its kind-7 technologies, re-picks the favourite land, sea, air and space types and,
// outside saved-game loading, retires obsolete units and tells the combat manager
void EEComputerPlayer::EnterEpoch(long inEpoch)
{
    EECPPlaceScope theScope(kCpevEnterEpoch, this, true); theScope.I(inEpoch);   // CPEV (TEST ONLY)
    if (this->mTechnologies != NULL) {
        EETechTreeEpoch* theEpoch = dynamic_cast<EETechTreeEpoch*>(this->mTechnologies->Get(inEpoch));
        if (theEpoch != NULL)
            theEpoch->ResearchKind(7, true);
    }
    EeCpPlaceCtx theContext = EECPPlaceContext(this);
    const int32_t kKinds[4] = {EE_CP_FAVORITE_LAND, EE_CP_FAVORITE_SEA, EE_CP_FAVORITE_AIR, EE_CP_FAVORITE_SPACE};
    for (int k = 0; k < 4; k++)
        ee_cp_choose_favorite(&theContext, kKinds[k]);
    if (!gLoadingSavedGame)
        ee_cp_retire_obsolete_units(&theContext);
}

// Picks a random premade civilization that fits the current epoch (some excluded on waterless maps, Japan and
// Korea only on space maps) and loads it for the player
void EEComputerPlayer::ChooseCivilization()
{
    EECPPlaceScope theScope(kCpevChooseCivilization, this, true); struct EECPCivNote { EECPPlaceScope& s; EEComputerPlayer* p; ~EECPCivNote() { const char* c = (const char*)p->mCivilizationFile; unsigned long h = 0; while (c != NULL && *c) h = h * 31 + (unsigned char)*c++; s.I((long)h); } } theScopeNote = {theScope, this};   // CPEV (TEST ONLY)
    long theEpoch = this->mTechnologies != NULL ? this->mTechnologies->mCurrentEpoch : 100000;
    bool theWaterless = gWorld.mMap != NULL && (gWorld.mMap->mMapCharacteristics & kRMMapWaterlessMap) == kRMMapWaterlessMap;
    bool theSpace = gWorld.mMap != NULL && gWorld.mMap->mTerrainMesh.GetIsSpaceMap();
    EeCpPlaceCtx theContext = EECPPlaceContext(this);
    const uint8_t* theName = ee_cp_choose_civilization(&theContext, (int32_t)theEpoch, theWaterless, theSpace);
    if (theName != NULL) {
        UString theFile((const char*)theName);
        EEApplyCivilizationFile(theFile, this);
        this->mCivilizationFile = theFile;
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
    EECPPlaceScope theScope(kCpevUnitsSwapped, this, true); theScope.I((long)ioSwaps.size());   // CPEV (TEST ONLY)
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
    EECPPlaceScope theScope(kCpevRestoreManagers, this, true);   // CPEV (TEST ONLY)
    theScope.I((long)inMajorVersion).I((long)inMinorVersion).I(inResult);
    long theScopeResult = [&]() -> long {
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
    {   // the favourites whose classes are gone chosen again (ee_sim::cp::events restore_favorites)
        EeCpPlaceCtx theContext = EECPPlaceContext(this);
        ee_cp_restore_favorites(&theContext);
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
    }();
    theScope.I(theScopeResult);
    return theScopeResult;
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


// Tells the research manager about a new epoch, enters it if it is a real epoch, then runs the base handling
void EEComputerPlayer::EpochReached(long inEpoch)
{
    EECPPlaceScope theScope(kCpevEpochReached, this, true); theScope.I(inEpoch);   // CPEV (TEST ONLY)
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
    EECPPlaceScope theScope(kCpevSetActive, this, true); theScope.I(inActive);   // CPEV (TEST ONLY)
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
    EECPPlaceScope theScope(kCpevUnitStranded, this, true); theScope.CU(inUnit);   // CPEV (TEST ONLY)
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
    EECPPlaceScope theScope(kCpevPayImprovement, this, true); theScope.I(inClass != NULL ? (long)inClass->mUnitType : -1).I(inImprovement);   // CPEV (TEST ONLY)
    // (the original credited the resource manager's needs ledger with the scaled costs first: gone in stage 6)
    EEPlayer::PayImprovement(inClass, inImprovement);
}


// Registers a new unit with the player and, while the AI is active, with the civilization and combat managers
void EEComputerPlayer::UnitPlaced(EEComplexUnit* inUnit)
{
    EECPPlaceScope theScope(kCpevUnitPlaced, this, true); theScope.CU(inUnit);   // CPEV (TEST ONLY)
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
    EECPPlaceScope theScope(kCpevUnitTaken, this, true); theScope.U(static_cast<EEUnit*>(inUnit)).I(inSwapped);   // CPEV (TEST ONLY)
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
    EECPPlaceScope theScope(kCpevUnitGoalDone, this, true); theScope.CU(inUnit);   // CPEV (TEST ONLY)
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
    EECPPlaceCaptureThread();   // CPEV (TEST ONLY)
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
    EECPPlaceScope theScope(kCpevUpdateGroups, this, true);   // CPEV (TEST ONLY)
    EeCpPlaceCtx theContext = EECPPlaceContext(this);
    ee_cp_update_groups(&theContext);
}


// Returns true for citizens and for loaded transports that carry only citizens
bool EEComputerPlayer::AvoidsEnemies(EEComplexUnit* inUnit)
{
    EECPPlaceScope theScope(kCpevAvoidsEnemies, this, true);   // CPEV (TEST ONLY)
    theScope.CU(inUnit);
    EeCpPlaceCtx theContext = EECPPlaceContext(this);
    bool theResult = ee_cp_avoids_enemies(&theContext, (uint64_t)(uintptr_t)static_cast<EEUnit*>(inUnit));
    theScope.I(theResult);
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

// The computer player's world (ee_sim::cp::events over place_game's EE_CP_PLAYER_* operations; `inA`, `inB` units or
// centres). The centres' members are their forwarders (their PLCE scopes), the centres' sort the original's.
extern "C" int64_t ee_cp_player_host(const EeCpPlaceCtx* inContext, int32_t inOp, uint64_t inA, uint64_t inB,
    EeCpPlaceIo* io)
{
    EEComputerPlayer* thePlayer = static_cast<EEComputerPlayer*>(reinterpret_cast<EEPlayer*>((uintptr_t)inContext->player));
    EECPResourceCenter* theCenter = reinterpret_cast<EECPResourceCenter*>((uintptr_t)inA);
    EEUnit* theUnit = reinterpret_cast<EEUnit*>((uintptr_t)inA);
    EEComplexUnit* theComplex = static_cast<EEComplexUnit*>(theUnit);
    EEUnit* theOther = reinterpret_cast<EEUnit*>((uintptr_t)inB);
    const int64_t* v = io->v;
    switch (inOp) {
    case EE_CP_PLAYER_BEHAVIOR_FLOAT:
        io->f[0] = gCPBehaviors[(long)v[0]].mFloatValue;
        return 0;
    case EE_CP_PLAYER_BASE_POP_LIMIT:
        return thePlayer->EEPlayer::GetPopulationLimit(0);
    case EE_CP_PLAYER_DEATHMATCH:
        return thePlayer->IsDeathmatch() ? 1 : 0;
    case EE_CP_PLAYER_CHEAT_ON:
        return EECPCheatOn(thePlayer, (long)v[0]) ? 1 : 0;
    case EE_CP_PLAYER_NUM_TEAMS:
        return (int64_t)gWorld.GetNumTeams();
    case EE_CP_PLAYER_TEAM_WONDERS:
        return gWorld.mTeams[(size_t)v[0]].mVictoryCondition->HasEnoughWonders() ? 1 : 0;
    case EE_CP_PLAYER_TEAM_CONTAINS:
        return gWorld.mTeams[(size_t)v[0]].ContainsPlayer((unsigned long)v[1]) ? 1 : 0;
    case EE_CP_PLAYER_NUM_PLAYERS:
        return (int64_t)gWorld.mNumPlayers;
    case EE_CP_PLAYER_HUMAN: {
        EEPlayer* theWorldPlayer = gWorld.mPlayers[(size_t)v[0]];
        return theWorldPlayer == NULL ? -1 : theWorldPlayer->IsHuman() ? 1 : 0;
    }
    case EE_CP_PLAYER_POPULATION:
        return gWorld.mPlayers[(size_t)v[0]]->GetTotalPopulation(true, true);
    case EE_CP_PLAYER_HANDLE:
        return (int64_t)(uintptr_t)gWorld.GetPlayer((unsigned long)v[0]);
    case EE_CP_PLAYER_PATH_AREA_ID: {
        EEAIPathArea* theArea = gContinentManager.GetPathAreaAt(EETileLocation((long)v[0], (long)v[1]));
        return theArea != NULL ? theArea->GetID() : 0;
    }
    case EE_CP_PLAYER_UNIT_CONTINENT_AT:
        return (int64_t)(uintptr_t)gContinentManager.GetContinentAt(theUnit);
    case EE_CP_PLAYER_CENTER_CREATE:
        return (int64_t)(uintptr_t)new EECPResourceCenter(thePlayer, theUnit);
    case EE_CP_PLAYER_CENTER_LIST:
        if (v[0] != 0)
            thePlayer->mResourceCenters.insert(thePlayer->mResourceCenters.begin(), theCenter);
        else
            thePlayer->mResourceCenters.push_back(theCenter);
        return 0;
    case EE_CP_PLAYER_CENTER_IS_WITHIN:
        return theCenter->IsWithinRadius(theOther) ? 1 : 0;
    case EE_CP_PLAYER_CENTER_ADD:
        theCenter->AddUnit(theOther);
        return 0;
    case EE_CP_PLAYER_CENTER_CONTAINS:
        return theCenter->Contains(theOther) ? 1 : 0;
    case EE_CP_PLAYER_CENTER_ADOPT:
        theCenter->AdoptDropSite(static_cast<EEComplexUnit*>(theOther));
        return 0;
    case EE_CP_PLAYER_CENTER_SET_MAIN:
        theCenter->SetUnit(static_cast<EEComplexUnit*>(theOther));
        return 0;
    case EE_CP_PLAYER_CENTER_GET_CENTER: {
        EETileCoordinate theTile((long)v[0], (long)v[1]);
        theCenter->GetCenter(theTile);
        io->v[0] = theTile.mX;
        io->v[1] = theTile.mY;
        return 0;
    }
    case EE_CP_PLAYER_CENTER_SET_ACTIVE:
        theCenter->SetActive(v[0] != 0, 0);
        return 0;
    case EE_CP_PLAYER_CENTERS_SORT:
        std::sort(thePlayer->mResourceCenters.begin(), thePlayer->mResourceCenters.end(),
            EECPCenterDistanceLess(U2DPoint<long>((long)v[0], (long)v[1])));
        return 0;
    case EE_CP_PLAYER_FAVORITE_CANDIDATE: {
        EEUnitClass* theClass = gUnitClassTable[(long)v[0]];
        long theKind = (long)v[1];
        EEAIFOFavoriteUnitScreener theScreener(thePlayer, theKind == EE_CP_FAVORITE_AIR, theKind == EE_CP_FAVORITE_LAND,
            theKind == EE_CP_FAVORITE_SEA, theKind == EE_CP_FAVORITE_SPACE, (long)v[2], 0);
        if (theClass == NULL || !theClass->mIsEnabled || !theScreener.Screen(theClass))
            return 0;
        io->v[0] = theClass->mAIKind;
        io->v[1] = theClass->mUnitType;
        return 1;
    }
    case EE_CP_PLAYER_HISTORY_PUSH:
        thePlayer->mFavoriteTypeHistory.push_back(std::make_pair((unsigned long)v[0], (long)v[1]));
        return 0;
    case EE_CP_PLAYER_IS_EPOCH:
        return EEIsEpochID((long)v[0]) ? 1 : 0;
    case EE_CP_PLAYER_UNIT_IS_BUILDING:
        return theComplex->mUnitClass->IsBuilding() ? 1 : 0;
    case EE_CP_PLAYER_UNIT_RESEARCH:
        ee_cp_place_push(io->list, theComplex->mCurrentResearch, 0);
        for (unsigned long q = 0; q < theComplex->GetBuildQueueSize(); q++)
            ee_cp_place_push(io->list, (*(theComplex->mBuildQueue.begin() + q)).mTechnology, 0);
        return 0;
    case EE_CP_PLAYER_UNIT_OBSOLETE:
        return theComplex->GetTechTreeNode() != NULL && theComplex->GetTechTreeNode()->GetIsObsolete() ? 1 : 0;
    case EE_CP_PLAYER_SEEN_BY:
        return EEIsSeenBy(theComplex, reinterpret_cast<EEPlayer*>((uintptr_t)inB), true) ? 1 : 0;
    case EE_CP_PLAYER_KILL:
        theComplex->Kill(true, false, false);
        return 0;
    case EE_CP_PLAYER_IS_TRANSPORT:
        return theUnit->mUnitClass->IsTransport() ? 1 : 0;
    case EE_CP_PLAYER_CARGO:
        for (unsigned long i = 0; i < theComplex->GetNumCargoUnits(); i++)
            ee_cp_place_push(io->list, (int64_t)(uintptr_t)static_cast<EEUnit*>(theComplex->GetCargo(i)), 0);
        return 0;
    }
    return 0;
}
