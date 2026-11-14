// C:\EEX\Empire Earth\Persistent\EEPWorldHelper.cpp (file name inferred: no __FILE__; the class family and its keys
// "EEPWorldHelper-..."). The saved state of the world: 0x14bd64..0x14c6b1. Unit flags: the exe's module set.
// Data: the vtable 0x4422bc; the 15 class static keys (guard bytes 0x531c58, 0x531bd0).
#include <windows.h>
#include <deque>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "lle/TMPoll.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIFormManager.h"
#include "lle/UIManager.h"
#include "lle/FSBasicFile.h"
#include "lle/TSScheduler.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGameState.h"
#include "ee/EETeam.h"
#include "ee/EEPWorldHelper.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIEChatLogEvents.h"
#include "ee/EEPersistentTag.h"
#include "ee/EEUnit.h"
#include "ee/EESaveGameFiles.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAIForest.h"
#include "ee/EEArtificialIntelligence.h"
#include "ee/EEPathManager.h"
#include "ee/EEAEManager.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/RTSRegionTable.h"
#include "ee/EECPExperimentTimers.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnitHandle.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUGRust.h"
#include <algorithm>
#include <vector>
#include "rust/ee_combat.h"

const UString EEPWorldHelper::sTheScenarioManager("EEWorldHelper-The Scenario Manager");
const UString EEPWorldHelper::sTheTeams("EEPWorldHelper-The Teams");
const UString EEPWorldHelper::sTheNumberOfWondersToWin("EEPWorldHelper-The Number Of Wonders To Win");
const UString EEPWorldHelper::sTheTimeOfDay("EEPWorldHelper-The Time Of Day");
const UString EEPWorldHelper::sTheWaterLevel("EEPWorldHelper-The Water Level");
const UString EEPWorldHelper::sTheZoom("EEPWorldHelper-The Zoom");
const UString EEPWorldHelper::sThePopulationLimit("EEPWorldHelper-The Population Limit");
const UString EEPWorldHelper::sTheGameSpeed("EEPWorldHelper-The Game Speed");
const UString EEPWorldHelper::sTheVictoryEnabled("EEPWorldHelper-The Victory Enabled");
const UString EEPWorldHelper::sTheUnitImprovementsAllowed("EEPWorldHelper-The Unit Improvements Allowed");
const UString EEPWorldHelper::sTheTeamsLocked("EEPWorldHelper-The Teams Locked");
const UString EEPWorldHelper::sTheCheatCodesAllowed("EEPWorldHelper-The Cheat Codes Allowed");
const UString EEPWorldHelper::sTownIDCounter("EEPWorldHelper-TownID Counter");
const UString EEPWorldHelper::sHerdIDCounter("EEPWorldHelper-HerdID Counter");
const UString EEPWorldHelper::sUsingCarbon("EEPWorldHelper-Using Carbon");

static long LoadTeams(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile,
                      std::vector<EETeam>& outTeams);
static long SaveTeams(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile,
                      std::vector<EETeam>& inTeams);

// FUNCTION: EXE 0x54c151
// Creates an empty saved-world chunk (no teams, improvements, team lock and cheats allowed)
EEPWorldHelper::EEPWorldHelper()
    : FSPersistent(kPersistentWorld)
{
    this->mScenarioManager = NULL;
    this->mTimeOfDay = 0.0f;
    this->mZoom = 0.0f;
    this->mGameSpeed = 0.0f;
    this->mPopulationLimit = 0;
    this->mNumberOfWondersToWin = 0;
    this->mC36 = 0;
    this->mVictoryEnabled = false;
    this->mTownIDCounter = 0;
    this->mHerdIDCounter = 0;
    this->mUsingCarbon = false;
    this->mUnitImprovementsAllowed = true;
    this->mTeamsLocked = true;
    this->mCheatsEnabled = true;
}

// SYNTHETIC: EXE 0x54c1bc
// (the scalar deleting destructor, emitted with the vtable; the implicit dtor inline)

// FUNCTION: EXE 0x54c1e2
// Creates the saved-world chunk from the world's teams, time of day, zoom, speed, limits, victory and cheat settings
// and id counters
EEPWorldHelper::EEPWorldHelper(EEWorld& inWorld)
    : FSPersistent(kPersistentWorld)
{
    this->mScenarioManager = NULL;
    this->mTimeOfDay = inWorld.GetTimeOfDay();
    this->mZoom = gWorld.mCameraControl.GetZoom();
    this->mGameSpeed = inWorld.GetGameSpeed();
    this->mPopulationLimit = inWorld.mPopulationLimit;
    this->mNumberOfWondersToWin = inWorld.mNumberOfWondersToWin;
    this->mC36 = 0;

    this->mTeams = inWorld.mTeams;
    this->mVictoryEnabled = inWorld.mVictoryEnabled;
    this->mUnitImprovementsAllowed = inWorld.mUnitImprovementsAllowed;
    this->mTeamsLocked = inWorld.mTeamsLocked;
    this->mCheatsEnabled = inWorld.mCheatsEnabled;
    this->mTownIDCounter = inWorld.mTownIDCounter;
    this->mHerdIDCounter = inWorld.mHerdIDCounter;
    this->mUsingCarbon = inWorld.mUsingCarbon;
}

// FUNCTION: EXE 0x54c2a6
// Creates an empty saved-world chunk (persistent factory)
FSPersistent* EEPWorldHelper::CreateDefault() const
{
    return new EEPWorldHelper;
}

// port (save versions 0.254 to 0.279): the world's running state the original did not save (the order of the unit
// chain, the stepped units, the tiles', regions' and players' units; the forests' renewals; the computer players'
// schedule; the dead units the living hold; the other systems' parts in their places) is ee_units world_save.rs's,
// over this host: the file (EESavePrimitive), the version's predicates, the other systems' parts, the first tick's
// restores (EEAfterLoad) and the world's units by address.
struct EEPWorldContext {
    FSFile* mFile;
    unsigned long mMajor;
    unsigned long mMinor;
};

static int64_t EEPWorldFill(EeUnitsWorldIo* io, size_t inIndex, uint64_t inValue)
{
    if (inIndex < io->cap)
        io->list[inIndex] = inValue;
    return 0;
}

template <class T> static int64_t EEPWorldFillAll(EeUnitsWorldIo* io, const std::vector<T*>& inItems)
{
    for (size_t i = 0; i < inItems.size(); i++)
        EEPWorldFill(io, i, (uint64_t)(uintptr_t)inItems[i]);
    return (int64_t)inItems.size();
}

// 0.272/0.275: a unit a goal holds out of the world, once (simple units too: a gathered tree or mine gone)
static void AddHeldGhost(EEUnit* inUnit, std::vector<EEComplexUnit*>& ioComplex, std::vector<EEUnit*>& ioSimple)
{
    if (inUnit == NULL || gWorld.GetUnit(inUnit->mUnitID) == inUnit)
        return;
    EEComplexUnit* theComplex = dynamic_cast<EEComplexUnit*>(inUnit);   // (a removed unit's store fields read 0)
    if (theComplex != NULL) {
        if (std::find(ioComplex.begin(), ioComplex.end(), theComplex) == ioComplex.end())
            ioComplex.push_back(theComplex);
    } else if (std::find(ioSimple.begin(), ioSimple.end(), inUnit) == ioSimple.end())
        ioSimple.push_back(inUnit);
}

// 0.262: the dead attackers the units hold, those the gatherer lists hold, then (0.272, 0.275) the goals' resources,
// drop sites and attack targets out of the world: the complex ones first, then the simple ones
static void GatherGhosts(std::vector<EEComplexUnit*>& outDead, std::vector<EEUnit*>& outSimple)
{
    for (unsigned long p = 0; p < gWorld.mNumPlayers; p++) {
        EEPlayer* thePlayer = gWorld.mPlayers[p];
        for (EEUnit* theUnit = thePlayer != NULL ? thePlayer->mFirstUnit : NULL; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            EEComplexUnit* theAttacker = theUnit->mComplexUnit != NULL ? theUnit->mComplexUnit->mLastAttacker : NULL;
            if (theAttacker == NULL || gWorld.GetUnit(theAttacker->mUnitID) == theAttacker
                || std::find(outDead.begin(), outDead.end(), theAttacker) != outDead.end())
                continue;
            outDead.push_back(theAttacker);
        }
    }
    EECPOutOfWorldGatherers(outDead);
    for (unsigned long p = 0; p < gWorld.mNumPlayers; p++) {
        EEPlayer* thePlayer = gWorld.mPlayers[p];
        for (EEUnit* theUnit = thePlayer != NULL ? thePlayer->mFirstUnit : NULL; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            EEComplexUnit* theComplex = theUnit->mComplexUnit;
            for (size_t q = 0; theComplex != NULL && q < theComplex->mGoalQueues.size(); q++) {
                const EEGoalDeque& theGoals = theComplex->mGoalQueues[q].mGoals;
                for (size_t g = 0; g < theGoals.size(); g++) {
                    if (EEUGRust* theAttack = EEAttackUnitCast(theGoals[g])) {
                        AddHeldGhost(theAttack->GetAttackTarget(), outDead, outSimple);
                    } else if (EEUGRust* theGather = EEGatherGoalCast(theGoals[g])) {
                        AddHeldGhost(theGather->GetGatherResource(), outDead, outSimple);
                        AddHeldGhost(theGather->GetGatherStorage(), outDead, outSimple);
                    }
                }
            }
        }
    }
}

static int64_t EEPWorldHost(void* inContext, int32_t inOp, EeUnitsWorldIo* io);

static int64_t EEPWorldPart(EEPWorldContext& inContext, bool inLoad, int64_t inPart, bool inTownAreas)
{
    FSFile& theFile = *inContext.mFile;
    switch (inPart) {
    case EE_UNITS_WORLD_PART_PATH_RUNNING: if (inLoad) return gPathManager.LoadRunningState(theFile); gPathManager.SaveRunningState(theFile); return 0;
    case EE_UNITS_WORLD_PART_AREA_EFFECTS: if (inLoad) return EEAELoadRunningState(theFile); EEAESaveRunningState(theFile); return 0;
    case EE_UNITS_WORLD_PART_EXPERIMENT_TIMERS: if (inLoad) return EECPLoadExperimentTimers(theFile); EECPSaveExperimentTimers(theFile); return 0;
    case EE_UNITS_WORLD_PART_TOWNS: if (inLoad) return EELoadTowns(theFile, inTownAreas); EESaveTowns(theFile); return 0;
    case EE_UNITS_WORLD_PART_CONTINENT_REGIONS: if (inLoad) return EELoadContinentRegions(theFile); EESaveContinentRegions(theFile); return 0;
    case EE_UNITS_WORLD_PART_RESOURCE_STATE: if (inLoad) return EECPLoadResourceState(theFile); EECPSaveResourceState(theFile); return 0;
    case EE_UNITS_WORLD_PART_PATH_SEARCHES: if (inLoad) return gPathManager.LoadSearches(theFile); gPathManager.SaveSearches(theFile); return 0;
    case EE_UNITS_WORLD_PART_ACTIVE_GROUPS: if (inLoad) return EELoadActiveGroups(theFile); EESaveActiveGroups(theFile); return 0;
    case EE_UNITS_WORLD_PART_GRAPH_SEARCHES: if (inLoad) return gPathManager.LoadGraphSearches(theFile); gPathManager.SaveGraphSearches(theFile); return 0;
    }
    return 0;
}

static bool EEPWorldHas(const EEPWorldContext& inContext, int64_t inWhich)
{
    unsigned long theMajor = inContext.mMajor;
    unsigned long theMinor = inContext.mMinor;
    switch (inWhich) {
    case EE_UNITS_WORLD_HAS_RUNNING_STATE: return EESaveHasRunningState(theMajor, theMinor);
    case EE_UNITS_WORLD_HAS_REGION_ORDER: return EESaveHasRegionOrder(theMajor, theMinor);
    case EE_UNITS_WORLD_HAS_EXPERIMENT_TIMERS: return EESaveHasExperimentTimers(theMajor, theMinor);
    case EE_UNITS_WORLD_HAS_TOWNS: return EESaveHasTowns(theMajor, theMinor);
    case EE_UNITS_WORLD_HAS_TOWN_AREAS: return EESaveHasTownAreas(theMajor, theMinor);
    case EE_UNITS_WORLD_HAS_GHOSTS: return EESaveHasGhosts(theMajor, theMinor);
    case EE_UNITS_WORLD_HAS_GHOST_RESOURCES: return EESaveHasGhostResources(theMajor, theMinor);
    case EE_UNITS_WORLD_HAS_GOAL_GHOSTS: return EESaveHasGoalGhosts(theMajor, theMinor);
    case EE_UNITS_WORLD_HAS_CONTINENT_REGIONS: return EESaveHasContinentRegions(theMajor, theMinor);
    case EE_UNITS_WORLD_HAS_RESOURCE_STATE: return EESaveHasResourceState(theMajor, theMinor);
    case EE_UNITS_WORLD_HAS_PLAYER_UNIT_ORDER: return EESaveHasPlayerUnitOrder(theMajor, theMinor);
    case EE_UNITS_WORLD_HAS_PATH_SEARCHES: return EESaveHasPathSearches(theMajor, theMinor);
    case EE_UNITS_WORLD_HAS_ACTIVE_GROUPS: return EESaveHasActiveGroups(theMajor, theMinor);
    case EE_UNITS_WORLD_HAS_GRAPH_SEARCHES: return EESaveHasGraphSearches(theMajor, theMinor);
    }
    return false;
}

static int64_t EEPWorldHost(void* inContext, int32_t inOp, EeUnitsWorldIo* io)
{
    EEPWorldContext* theContext = static_cast<EEPWorldContext*>(inContext);
    if (EESaveIsPrimitive(inOp)) {
        EeUnitsPlayerSaveIo theIo = {io->a, io->f, (uint8_t)(io->b != 0 ? 1 : 0)};
        int32_t theResult = EESavePrimitive(*theContext->mFile, inOp, &theIo);
        io->a = theIo.i;
        io->f = theIo.f;
        io->b = theIo.b;
        return theResult;
    }
    RTSUnit* theUnit = reinterpret_cast<RTSUnit*>((uintptr_t)io->a);
    EEMap* theMap = gWorld.mMap;
    switch (inOp) {
    case EE_UNITS_WORLD_HAS: return EEPWorldHas(*theContext, io->a) ? 1 : 0;
    case EE_UNITS_WORLD_PART_LOAD: return EEPWorldPart(*theContext, true, io->a, io->b != 0);
    case EE_UNITS_WORLD_PART_SAVE: return EEPWorldPart(*theContext, false, io->a, false);
    case EE_UNITS_WORLD_AFTER_LOAD: {
        void* thePending = reinterpret_cast<void*>((uintptr_t)io->a);
        EEAfterLoad([thePending]() { ee_units_world_restore(thePending, NULL, &EEPWorldHost); });
        return 0;
    }
    case EE_UNITS_WORLD_DEBUG: ESDebugOut("%s", reinterpret_cast<const char*>(io->list)); return 0;
    case EE_UNITS_WORLD_UNIT_ID: return (int64_t)theUnit->mUnitID;
    case EE_UNITS_WORLD_CHAIN: {
        size_t theCount = 0;
        for (RTSUnit* theChain = gWorld.mUnits.mFirst; theChain != NULL; theChain = theChain->mTileNode->mNext)
            EEPWorldFill(io, theCount++, (uint64_t)(uintptr_t)theChain);
        return (int64_t)theCount;
    }
    case EE_UNITS_WORLD_UNIT: return (int64_t)(uintptr_t)gWorld.GetUnit((unsigned long)io->a);
    case EE_UNITS_WORLD_IN_CHAIN:
        return theUnit->mTileNode != NULL && (theUnit->mTileNode->mPrev != NULL || gWorld.mUnits.mFirst == theUnit);
    case EE_UNITS_WORLD_TILE_X: return theUnit->mTileX;
    case EE_UNITS_WORLD_LINK:
        theUnit->Link(io->b != 0 ? reinterpret_cast<RTSUnit*>((uintptr_t)io->b) : reinterpret_cast<RTSUnit*>(&gWorld.mUnits));
        return 0;
    case EE_UNITS_WORLD_STEP_LIST:
    case EE_UNITS_WORLD_STEP_SET:
    case EE_UNITS_WORLD_STEP_DROP:
    case EE_UNITS_WORLD_STEP_ADD: {
        std::deque<EEUnitHandle<EEComplexUnit> >* theLists[3] = {&gWorld.mUpdateUnits, &gWorld.mActivateQueue, &gWorld.mDeactivateQueue};
        std::deque<EEUnitHandle<EEComplexUnit> >& theList = *theLists[io->a];
        EEComplexUnit* theComplex = reinterpret_cast<EEComplexUnit*>((uintptr_t)io->b);
        if (inOp == EE_UNITS_WORLD_STEP_LIST) {
            for (size_t i = 0; i < theList.size(); i++)
                EEPWorldFill(io, i, (uint64_t)(uintptr_t)(EEComplexUnit*)theList[i]);
            return (int64_t)theList.size();
        }
        if (inOp == EE_UNITS_WORLD_STEP_SET) {   // the references UpdateActiveUnits keeps: the ops below
            std::deque<EEUnitHandle<EEComplexUnit> > theOrder;
            for (size_t i = 0; i < io->cap; i++)
                theOrder.push_back(reinterpret_cast<EEComplexUnit*>((uintptr_t)io->list[i]));
            theList = theOrder;
        } else if (inOp == EE_UNITS_WORLD_STEP_DROP) {
            theComplex->Dereference();
            if (io->a == 0) {
                theComplex->Dereference();
                gWorld.mActiveUnits.erase(theComplex->mUnitID);
            }
        } else {
            theComplex->Reference();
            if (io->a == 0) {
                theComplex->Reference();
                gWorld.mActiveUnits[theComplex->mUnitID] = theComplex;
            }
        }
        return 0;
    }
    case EE_UNITS_WORLD_COMPLEX: return (int64_t)(uintptr_t)dynamic_cast<EEComplexUnit*>(static_cast<EEUnit*>(gWorld.GetUnit((unsigned long)io->a)));
    case EE_UNITS_WORLD_FORESTS: return EEPWorldFillAll(io, gContinentManager.mForests);
    case EE_UNITS_WORLD_FOREST_RENEWAL: return (int64_t)reinterpret_cast<EEAIForest*>((uintptr_t)io->a)->mNextRenewal;
    case EE_UNITS_WORLD_FOREST_TILES: {
        std::vector<U2DPoint<long> > theTiles;
        reinterpret_cast<EEAIForest*>((uintptr_t)io->a)->mTiles.BuildPointList(theTiles, 1);
        for (size_t t = 0; t < theTiles.size(); t++)
            EEPWorldFill(io, t, (uint64_t)(uint32_t)theTiles[t].mX | ((uint64_t)(uint32_t)theTiles[t].mY << 32));
        return (int64_t)theTiles.size();
    }
    case EE_UNITS_WORLD_FOREST_AT: {
        U2DPoint<long> theTile;
        theTile.mX = (long)io->a;
        theTile.mY = (long)io->b;
        return (int64_t)(uintptr_t)gContinentManager.GetForestAt(theTile);
    }
    case EE_UNITS_WORLD_FOREST_SET_RENEWAL: reinterpret_cast<EEAIForest*>((uintptr_t)io->a)->mNextRenewal = (unsigned long)io->b; return 0;
    case EE_UNITS_WORLD_FOREST_SIZE: return (int64_t)(unsigned long)reinterpret_cast<EEAIForest*>((uintptr_t)io->a)->mTiles.Size();
    case EE_UNITS_WORLD_AI_SCHEDULE:
        io->a = (int64_t)gArtificialIntelligence->mNextPlayer;
        io->b = (int64_t)gArtificialIntelligence->mNextUpdateTime;
        return (int64_t)gArtificialIntelligence->mComputerPlayers.size();
    case EE_UNITS_WORLD_AI_SET_SCHEDULE:
        gArtificialIntelligence->mNextPlayer = (unsigned long)io->a;
        gArtificialIntelligence->mNextUpdateTime = (unsigned long)io->b;
        return 0;
    case EE_UNITS_WORLD_TILE_COUNT: return theMap != NULL ? (int64_t)theMap->mSize * theMap->mSize : 0;
    case EE_UNITS_WORLD_TILE_UNITS:
    case EE_UNITS_WORLD_TILE_SET: {
        EEMapTile* theTile = theMap->mTiles[io->a];
        size_t theCount = 0;
        for (RTSTileUnitNode* theNode = theTile != NULL ? theTile->mUnits : NULL; theNode != NULL; theNode = theNode->mNext, theCount++) {
            if (inOp == EE_UNITS_WORLD_TILE_UNITS)
                EEPWorldFill(io, theCount, (uint64_t)(uintptr_t)theNode->mUnit);
            else if ((int64_t)theCount == io->b)
                theNode->mUnit = reinterpret_cast<EEUnit*>((uintptr_t)io->list[0]);
        }
        return (int64_t)theCount;
    }
    case EE_UNITS_WORLD_REGION_ORDER:
    case EE_UNITS_WORLD_REGION_RESTORE: {
        EeCombatUnitRegions* theTable = theMap != NULL ? theMap->mUnitRegions : NULL;
        if (inOp == EE_UNITS_WORLD_REGION_RESTORE) {
            std::vector<uint32_t> theIDs;
            for (size_t i = 0; i < io->cap; i++)
                theIDs.push_back((uint32_t)io->list[i]);
            return ee_combat_regions_restore(theTable, (uint32_t)io->a, theIDs.data(), theIDs.size());
        }
        std::vector<uint32_t> theOrder(ee_combat_regions_order(theTable, NULL, 0));
        ee_combat_regions_order(theTable, theOrder.data(), theOrder.size());
        for (size_t k = 0; k < theOrder.size(); k++)
            EEPWorldFill(io, k, theOrder[k]);
        return (int64_t)theOrder.size();
    }
    case EE_UNITS_WORLD_GHOSTS: {
        std::vector<EEComplexUnit*> theDead;
        std::vector<EEUnit*> theSimple;
        GatherGhosts(theDead, theSimple);
        std::vector<EEUnit*> theAll(theDead.begin(), theDead.end());
        theAll.insert(theAll.end(), theSimple.begin(), theSimple.end());
        io->a = (int64_t)theDead.size();
        return EEPWorldFillAll(io, theAll);
    }
    case EE_UNITS_WORLD_GHOST_FIELDS: {
        EEUnit* theGhost = static_cast<EEUnit*>(theUnit);
        io->v[0] = (int64_t)theGhost->mUnitID;
        io->v[1] = theGhost->mUnitClass != NULL ? (int64_t)theGhost->mUnitClass->mUnitType : -1;
        io->v[2] = theGhost->mPlayer != NULL ? (int64_t)theGhost->mPlayer->mPlayerID : -1;
        io->v[3] = theGhost->mHitPoints;
        io->v[4] = theGhost->mTileX;
        io->v[5] = theGhost->mTileY;
        io->v[7] = (int64_t)theGhost->mResourceType;
        io->a = (long)theGhost->mResourceAmount;
        if (io->b != 0) {   // a complex unit: built flag and placement
            EEComplexUnit* theComplex = static_cast<EEComplexUnit*>(theGhost);
            io->v[6] = theComplex->mIsBuilt;
            GETransformation thePlacement = theComplex->mPhysicalModel.GetLocation();
            memcpy(io->m, thePlacement.mMatrix, sizeof(io->m));
        }
        return 0;
    }
    case EE_UNITS_WORLD_DEREFERENCE: static_cast<EEUnit*>(theUnit)->Dereference(); return 0;
    case EE_UNITS_WORLD_PLAYER_COUNT: return (int64_t)gWorld.mNumPlayers;
    case EE_UNITS_WORLD_PLAYER_UNITS:
    case EE_UNITS_WORLD_PLAYER_UNIT:
    case EE_UNITS_WORLD_PLAYER_SET_LIST:
    case EE_UNITS_WORLD_PLAYER_SET_BUCKET: {
        EEPlayer* thePlayer = gWorld.mPlayers[io->a];
        if (inOp == EE_UNITS_WORLD_PLAYER_UNIT)
            return thePlayer != NULL ? (int64_t)(uintptr_t)thePlayer->GetUnit((unsigned long)io->b) : 0;
        if (inOp == EE_UNITS_WORLD_PLAYER_SET_BUCKET) {
            thePlayer->mUnitBuckets[io->b].mFirst = reinterpret_cast<EEUnit*>((uintptr_t)io->list[0]);
            return 0;
        }
        if (inOp == EE_UNITS_WORLD_PLAYER_SET_LIST) {
            RTSUnit** theOrder = reinterpret_cast<RTSUnit**>(io->list);
            for (size_t i = 0; i < io->cap; i++) {
                theOrder[i]->mPrevious = i > 0 ? theOrder[i - 1] : NULL;
                theOrder[i]->mNext = i + 1 < io->cap ? theOrder[i + 1] : NULL;
            }
            thePlayer->mFirstUnit = io->cap == 0 ? NULL : static_cast<EEUnit*>(theOrder[0]);
            thePlayer->mLastUnit = io->cap == 0 ? NULL : static_cast<EEUnit*>(theOrder[io->cap - 1]);
            return 0;
        }
        size_t theCount = 0;
        for (RTSUnit* theOwned = thePlayer != NULL ? thePlayer->mFirstUnit : NULL; theOwned != NULL; theOwned = theOwned->mNext)
            EEPWorldFill(io, theCount++, (uint64_t)(uintptr_t)theOwned);
        return (int64_t)theCount;
    }
    }
    return 0;
}

// port (save version 0.262): the dead attackers (and from 0.272/0.275 the units the goals hold out of the world). A
// unit's mLastAttacker holds a reference, so the running game keeps an attacker that died since as a unit out of the
// world, and reads it (the triggers, the AI, a predator's strike back); the save wrote its id and the load found no
// unit. The records are world_save.rs's until the first tick; a load makes a unit from one as its holders repair
// their pointers (EEComplexUnit::RepairPointers, the goals'), held by world_save.rs until the first tick.
EEComplexUnit* EEFindLoadedGhost(unsigned long inID)
{
    return dynamic_cast<EEComplexUnit*>(EEFindLoadedGhostUnit(inID));
}

static unsigned long EEGhostUnitID(void* inUnit)
{
    return static_cast<EEUnit*>(inUnit)->mUnitID;
}

EEUnit* EEFindLoadedGhostUnit(unsigned long inID)
{
    if (void* theMade = ee_units_world_ghost_made_find(inID, &EEGhostUnitID))
        return static_cast<EEUnit*>(theMade);
    EeUnitsWorldGhost theGhost;
    if (!ee_units_world_ghost_find(inID, &theGhost))
        return NULL;
    EEUnitClass* theClass = theGhost.unit_type < EEUnitClassCount() ? gUnitClassTable[theGhost.unit_type] : NULL;
    EEPlayer* thePlayer = theGhost.player < gWorld.mNumPlayers ? gWorld.mPlayers[theGhost.player] : NULL;
    if (theClass == NULL || thePlayer == NULL)
        return NULL;
    EEUnit* theUnit;   // one reference, world_save.rs's until the first tick
    if (theGhost.is_complex) {
        EEComplexUnit* theComplex = EEComplexUnit::Create();
        theComplex->EEUnit::Init(theClass, thePlayer);
        theComplex->mIsBuilt = theGhost.is_built;
        GETransformation thePlacement = theComplex->mPhysicalModel.GetLocation();
        memcpy(thePlacement.mMatrix, theGhost.placement, sizeof(thePlacement.mMatrix));
        theComplex->mPhysicalModel.SetLocation(thePlacement);
        theUnit = theComplex;
    } else {   // 0.275: a simple unit, off the map as the load makes one (EEPlayer's Load: tile -10000)
        theUnit = EEUnit::Create();
        theUnit->Init(theClass, -10000, -10000, thePlayer);
    }
    theUnit->mUnitID = theGhost.id;
    theUnit->mHitPoints = theGhost.hit_points;
    theUnit->mTileX = theGhost.tile_x;
    theUnit->mTileY = theGhost.tile_y;
    // a used-up simple resource keeps its tile pointer, off the tile's lists (the gather goal's ResourceAccessible
    // reads its terrain), as MoveTo sets it without adding the unit to the tile
    if (!theGhost.is_complex && gWorld.mMap != NULL)
        theUnit->mTile = gWorld.mMap->GetTile(theGhost.tile_x, theGhost.tile_y);
    if (theGhost.resource_type != (unsigned long)-1) {   // 0.272
        theUnit->mResourceType = theGhost.resource_type;
        theUnit->mResourceAmount = theGhost.resource_amount;
    }
    theUnit->mIsValid = false;     // out of the world, as the dead unit was (FreeNode asserts it)
    RTSRegisterUnit(theUnit);      // the running game's handles still resolve a dead unit's id until its last reference
    ee_units_world_ghost_made(theUnit);
    return theUnit;
}

// FUNCTION: EXE 0x54c2bd
// Reads the teams, world settings and id counters (Carbon flag from version 0.239 on), the running state (from 0.254:
// world_save.rs), then the chat log and the main game form; returns only the main game form's error
long EEPWorldHelper::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    LoadTeams(inMajorVersion, inMinorVersion, inFile, this->mTeams);
    inFile.Read(this->mNumberOfWondersToWin);
    inFile.Read(this->mTimeOfDay);
    inFile.Read(this->mC36);
    inFile.Read(this->mZoom);
    inFile.Read(this->mPopulationLimit);
    inFile.Read(this->mGameSpeed);
    inFile.Read(this->mVictoryEnabled);
    inFile.Read(this->mUnitImprovementsAllowed);
    inFile.Read(this->mTeamsLocked);
    inFile.Read(this->mCheatsEnabled);
    inFile.Read(this->mTownIDCounter);
    inFile.Read(this->mHerdIDCounter);
    if (inMajorVersion <= 0 && inMinorVersion < 0xef)
        this->mUsingCarbon = false;
    else
        inFile.Read(this->mUsingCarbon);
    if (EESaveHasLiveState(inMajorVersion, inMinorVersion)) {
        EEPWorldContext theContext = {&inFile, inMajorVersion, inMinorVersion};
        ee_units_world_load_live(&theContext, &EEPWorldHost);
    }
    EEUIECLEFormManager::Load(inMajorVersion, inMinorVersion, inFile);
    return gUIManager->mMainGameForm->Load(inMajorVersion, inMinorVersion, inFile);
}

// FUNCTION: EXE 0x54c3a7
// Copies the loaded teams, world settings and id counters into the game state (and its scenario manager, if any)
void EEPWorldHelper::Restore(EEGameState& ioState)
{
    if (this->mScenarioManager != NULL)
        ioState.SetScenarioManager(this->mScenarioManager);
    ioState.mTeams = this->mTeams;
    ioState.mGameOptions.mWondersForVictory = this->mNumberOfWondersToWin;
    ioState.mTimeOfDay = this->GetTimeOfDay();
    ioState.mC4FA = this->mC36;
    ioState.mZoom = this->GetZoom();
    ioState.mGameOptions.mUnitLimit = this->mPopulationLimit;
    ioState.mGameSpeed = this->GetGameSpeed();
    ioState.mGameOptions.mVictory = this->mVictoryEnabled;
    ioState.mUnitImprovementsAllowed = this->mUnitImprovementsAllowed;
    ioState.mGameOptions.mCheatCodes = this->mCheatsEnabled;
    ioState.mTeamsLocked = this->mTeamsLocked;
    ioState.mTownIDCounter = this->mTownIDCounter;
    ioState.mHerdIDCounter = this->mHerdIDCounter;
    ioState.mUsingCarbon = this->mUsingCarbon;
}

// FUNCTION: EXE 0x54c449
// Writes the teams, world settings, id counters and Carbon flag, the running state (from 0.254: world_save.rs),
// then the chat log and the main game form; returns only the main game form's error
long EEPWorldHelper::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    SaveTeams(inMajorVersion, inMinorVersion, outFile, this->mTeams);
    outFile.Write(this->mNumberOfWondersToWin);
    outFile.Write(this->mTimeOfDay);
    outFile.Write(this->mC36);
    outFile.Write(this->mZoom);
    outFile.Write(this->mPopulationLimit);
    outFile.Write(this->mGameSpeed);
    outFile.Write(this->mVictoryEnabled);
    outFile.Write(this->mUnitImprovementsAllowed);
    outFile.Write(this->mTeamsLocked);
    outFile.Write(this->mCheatsEnabled);
    outFile.Write(this->mTownIDCounter);
    outFile.Write(this->mHerdIDCounter);
    outFile.Write(this->mUsingCarbon);
    if (EESaveHasLiveState(inMajorVersion, inMinorVersion)) {
        EEPWorldContext theContext = {&outFile, inMajorVersion, inMinorVersion};
        ee_units_world_save_live(&theContext, &EEPWorldHost);
    }
    EEUIECLEFormManager::Save(inMajorVersion, inMinorVersion, outFile);
    return gUIManager->mMainGameForm->Save(inMajorVersion, inMinorVersion, outFile);
}

// FUNCTION: EXE 0x54c530
// Reads the team count and that many teams; returns the first read error
static long LoadTeams(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile,
                      std::vector<EETeam>& outTeams)
{
    unsigned long theCount;
    long theResult = inFile.Read(theCount);
    // A failed count read leaves theCount unset: do not resize to it.
    if (theResult != kErrorOK)
        return theResult;
    outTeams.resize(theCount);
    for (std::vector<EETeam>::iterator it = outTeams.begin(); it != outTeams.end() && theResult == kErrorOK; ++it)
        theResult = it->Load(inMajorVersion, inMinorVersion, inFile);
    return theResult;
}

// FUNCTION: EXE 0x54c581
// Writes the team count and each team; returns the first write error
static long SaveTeams(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile,
                      std::vector<EETeam>& inTeams)
{
    unsigned long theCount = inTeams.size();
    long theResult = outFile.Write(theCount);
    for (std::vector<EETeam>::iterator it = inTeams.begin(); it != inTeams.end() && theResult == kErrorOK; ++it)
        theResult = it->Save(inMajorVersion, inMinorVersion, outFile);
    return theResult;
}

// TEMPLATE: EXE 0x54c5d9
// std::vector<EETeam>::resize(size_type) (first used by LoadTeams)

// TEMPLATE: EXE 0x54c5f5
// std::vector<EETeam>::resize(size_type, EETeam)

// ICF-FOLDED: EXE 0x557269
// Adds nothing to the checksum (empty: folded into a shared empty function)
void EEPWorldHelper::GetChecksum(ESChecksum& ioChecksum) const
{
}

// SYNTHETIC: EXE 0x54bd64
// SYNTHETIC: EXE 0x54bd6e
// SYNTHETIC: EXE 0x54bd7f
// SYNTHETIC: EXE 0x54bd8b
// SYNTHETIC: EXE 0x54bda7
// SYNTHETIC: EXE 0x54bdb1
// SYNTHETIC: EXE 0x54bdc2
// SYNTHETIC: EXE 0x54bdce
// SYNTHETIC: EXE 0x54bdea
// SYNTHETIC: EXE 0x54bdf4
// SYNTHETIC: EXE 0x54be05
// SYNTHETIC: EXE 0x54be11
// SYNTHETIC: EXE 0x54be2d
// SYNTHETIC: EXE 0x54be37
// SYNTHETIC: EXE 0x54be48
// SYNTHETIC: EXE 0x54be54
// SYNTHETIC: EXE 0x54be70
// SYNTHETIC: EXE 0x54be7a
// SYNTHETIC: EXE 0x54be8b
// SYNTHETIC: EXE 0x54be97
// SYNTHETIC: EXE 0x54beb3
// SYNTHETIC: EXE 0x54bebd
// SYNTHETIC: EXE 0x54bece
// SYNTHETIC: EXE 0x54beda
// SYNTHETIC: EXE 0x54bef6
// SYNTHETIC: EXE 0x54bf00
// SYNTHETIC: EXE 0x54bf11
// SYNTHETIC: EXE 0x54bf1d
// SYNTHETIC: EXE 0x54bf39
// SYNTHETIC: EXE 0x54bf43
// SYNTHETIC: EXE 0x54bf54
// SYNTHETIC: EXE 0x54bf60
// SYNTHETIC: EXE 0x54bf7c
// SYNTHETIC: EXE 0x54bf86
// SYNTHETIC: EXE 0x54bf97
// SYNTHETIC: EXE 0x54bfa3
// SYNTHETIC: EXE 0x54bfbf
// SYNTHETIC: EXE 0x54bfc9
// SYNTHETIC: EXE 0x54bfda
// SYNTHETIC: EXE 0x54bfe6
// SYNTHETIC: EXE 0x54c002
// SYNTHETIC: EXE 0x54c00c
// SYNTHETIC: EXE 0x54c01d
// SYNTHETIC: EXE 0x54c029
// SYNTHETIC: EXE 0x54c045
// SYNTHETIC: EXE 0x54c04f
// SYNTHETIC: EXE 0x54c060
// SYNTHETIC: EXE 0x54c06c
// SYNTHETIC: EXE 0x54c088
// SYNTHETIC: EXE 0x54c092
// SYNTHETIC: EXE 0x54c0a3
// SYNTHETIC: EXE 0x54c0af
// SYNTHETIC: EXE 0x54c0cb
// SYNTHETIC: EXE 0x54c0d5
// SYNTHETIC: EXE 0x54c0e6
// SYNTHETIC: EXE 0x54c0f2
// SYNTHETIC: EXE 0x54c10e
// SYNTHETIC: EXE 0x54c118
// SYNTHETIC: EXE 0x54c129
// SYNTHETIC: EXE 0x54c135
