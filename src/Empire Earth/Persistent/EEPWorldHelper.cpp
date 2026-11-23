// C:\EEX\Empire Earth\Persistent\EEPWorldHelper.cpp (file name inferred: no __FILE__; the class family and its keys
// "EEPWorldHelper-..."). The saved state of the world: 0x14bd64..0x14c6b1. Unit flags: the exe's module set.
// Data: the vtable 0x4422bc; the 15 class static keys (guard bytes 0x531c58, 0x531bd0).
#include <windows.h>
#include <deque>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGameState.h"
#include "ee/EETeam.h"
#include "rust/sim/app.h"
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
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EECPExperimentTimers.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEUGRust.h"
#include <algorithm>
#include <memory>
#include <vector>
#include "rust/sim/combat.h"
#include "rust/sim/units.h"


#include "rust/game/gamecpp.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"
#include "ee/EECommunications.h"
#include "ee/EEPhysics.h"
#include "ee/EEPMultiplayerHelper.h"
#include "ee/EEHumanPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEPPlayerHelper.h"
#include "ee/EECPEngine.h"
#include "ee/EECPNext.h"
#include "ee/EECPNextUnits.h"
#include "ee/EEGameOptions.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EEPChecksum.h"
#include "rust/base/error.h"

// The saved world's bodies are ee_game gamecpp persist_world.rs (game core lane, round 9): the constructors, Load,
// Save, Restore, the teams' load and save, the host of ee_sim's world_save.rs (the running state from save 0.254 on),
// its two parts of this file (the tech nodes not allowed, 0.282; the tiles' terrain types, 0.283) and the loaded
// ghosts' making. Their C++ calls and members are EEPWorldOp's cases below.

// Creates an empty saved-world chunk (no teams, improvements, team lock and cheats allowed)
EEPWorldHelper::EEPWorldHelper()
    : FSPersistent(kPersistentWorld)
{
    ee_gamecpp_pwh_init(this);
}

// Creates the saved-world chunk from the world's teams, time of day, zoom, speed, limits, victory and cheat settings
// and id counters
EEPWorldHelper::EEPWorldHelper(EEWorld& inWorld)
    : FSPersistent(kPersistentWorld)
{
    ee_gamecpp_pwh_from_world(this, &inWorld);
}

// Creates an empty saved-world chunk (persistent factory)
FSPersistent* EEPWorldHelper::CreateDefault() const
{
    return new EEPWorldHelper;
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

EEUnit* EEFindLoadedGhostUnit(unsigned long inID)
{
    return static_cast<EEUnit*>(ee_gamecpp_find_loaded_ghost_unit(inID));
}

// Reads the teams, world settings and id counters (Carbon flag from version 0.239 on), the running state (from 0.254:
// world_save.rs), then the chat log and the main game form; returns only the main game form's error
long EEPWorldHelper::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_gamecpp_pwh_load(this, inMajorVersion, inMinorVersion, &inFile);
}

// Copies the loaded teams, world settings and id counters into the game state (and its scenario manager, if any)
void EEPWorldHelper::Restore(EEGameState& ioState)
{
    ee_gamecpp_pwh_restore(this, &ioState);
}

// Writes the teams, world settings, id counters and Carbon flag, the running state (from 0.254: world_save.rs),
// then the chat log and the main game form; returns only the main game form's error
long EEPWorldHelper::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return ee_gamecpp_pwh_save(this, inMajorVersion, inMinorVersion, &outFile);
}

// Adds nothing to the checksum (empty: folded into a shared empty function)
void EEPWorldHelper::GetChecksum(ESChecksum& ioChecksum) const
{
}

// ------------------------------------------------------------------------- the saved world's C++ (gamecpp persist_world.rs)

int64_t EECore9OpD6(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EEPLoaderOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

#define P64(x) ((int64_t)(intptr_t)(x))
#define PWH ((EEPWorldHelper*)p)
#define FILE_ (*(FSFile*)p)
#define VERSION (unsigned long)a[0], (unsigned long)a[1]
#define UNIT ((EEUnit*)p)
#define RTS ((RTSUnit*)p)
#define CX ((EEComplexUnit*)p)
#define PL ((EEPlayer*)p)
#define FOREST ((EEAIForest*)p)
#define POINTS (*(std::vector<U2DPoint<long> >*)p)

#define SERVER dynamic_cast<NESynchronousServer*>(gCommunications)
#define CLIENT dynamic_cast<NESynchronousClient*>(gCommunications)
#define COMMS dynamic_cast<NESynchronousCommunications*>(gCommunications)
#define PPH ((EEPPlayerHelper*)p)
#define CP ((EEComputerPlayer*)p)
#define STR (*(std::string*)p)
#define BYTES std::string((const char*)a[0], (size_t)a[1])
#define HDR ((EEPSaveGameHeader*)p)
#define AFILE (*(FSFile*)a[0])
// The saved world's C++ calls and members (EESaveLoad.cpp's EECore9OpC default): one statement each
int64_t EEPWorldOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::PwhFields: return P64(&PWH->mTimeOfDay);
    case O::PwhScenario: return P64(PWH->mScenarioManager);
    case O::PwhSetScenario: PWH->mScenarioManager = (EEScenarioManager*)a[0];
        break;
    case O::PwhTeamsFromWorld: PWH->mTeams = ((EEWorld*)a[0])->mTeams;
        break;
    case O::PwhTeamsCount: return (int64_t)PWH->mTeams.size();
    case O::PwhTeamsResize: PWH->mTeams.resize((size_t)a[0]);
        break;
    case O::PwhTeamLoad: return PWH->mTeams[a[0]].Load((unsigned long)a[1], (unsigned long)a[2], *(FSFile*)a[3]);
    case O::PwhTeamSave: return PWH->mTeams[a[0]].Save((unsigned long)a[1], (unsigned long)a[2], *(FSFile*)a[3]);
    case O::PwWorldPopulationLimit: return P64(&((EEWorld*)p)->mPopulationLimit);
    case O::PwStSetScenario: ((EEGameState*)p)->SetScenarioManager((EEScenarioManager*)a[0]);
        break;
    case O::PwStTeamsFromHelper: ((EEGameState*)p)->mTeams = ((EEPWorldHelper*)a[0])->mTeams;
        break;
    case O::PwHasLiveState: return EESaveHasLiveState(VERSION);
    case O::PwChatLogLoad: return EEUIECLEFormManager::Load(VERSION, FILE_);
    case O::PwChatLogSave: return EEUIECLEFormManager::Save(VERSION, FILE_);
    case O::PwMainFormLoad: return gUIManager->GetMainGameForm()->Load(VERSION, FILE_);
    case O::PwMainFormSave: return gUIManager->GetMainGameForm()->Save(VERSION, FILE_);
    case O::PwPrimitive: return EESavePrimitive(FILE_, (int32_t)a[0], (EeUnitsPlayerSaveIo*)a[1]);
    case O::PwHasRunningState: return EESaveHasRunningState(VERSION);
    case O::PwHasRegionOrder: return EESaveHasRegionOrder(VERSION);
    case O::PwHasExperimentTimers: return EESaveHasExperimentTimers(VERSION);
    case O::PwHasTowns: return EESaveHasTowns(VERSION);
    case O::PwHasTownAreas: return EESaveHasTownAreas(VERSION);
    case O::PwHasGhosts: return EESaveHasGhosts(VERSION);
    case O::PwHasGhostResources: return EESaveHasGhostResources(VERSION);
    case O::PwHasGoalGhosts: return EESaveHasGoalGhosts(VERSION);
    case O::PwHasContinentRegions: return EESaveHasContinentRegions(VERSION);
    case O::PwHasResourceState: return EESaveHasResourceState(VERSION);
    case O::PwHasPlayerUnitOrder: return EESaveHasPlayerUnitOrder(VERSION);
    case O::PwHasPathSearches: return EESaveHasPathSearches(VERSION);
    case O::PwHasActiveGroups: return EESaveHasActiveGroups(VERSION);
    case O::PwHasGraphSearches: return EESaveHasGraphSearches(VERSION);
    case O::PwHasTechAllowed: return EESaveHasTechAllowed(VERSION);
    case O::PwHasTileTypes: return EESaveHasTileTypes(VERSION);
    case O::PwPathRunningLoad: return gPathManager.LoadRunningState(FILE_);
    case O::PwPathRunningSave: gPathManager.SaveRunningState(FILE_);
        break;
    case O::PwAreaEffectsLoad: return EEAELoadRunningState(FILE_);
    case O::PwAreaEffectsSave: EEAESaveRunningState(FILE_);
        break;
    case O::PwExperimentTimersLoad: return EECPLoadExperimentTimers(FILE_);
    case O::PwExperimentTimersSave: EECPSaveExperimentTimers(FILE_);
        break;
    case O::PwTownsLoad: return EELoadTowns(FILE_, a[0] != 0);
    case O::PwTownsSave: EESaveTowns(FILE_);
        break;
    case O::PwContinentRegionsLoad: return EELoadContinentRegions(FILE_);
    case O::PwContinentRegionsSave: EESaveContinentRegions(FILE_);
        break;
    case O::PwResourceStateLoad: return EECPLoadResourceState(FILE_);
    case O::PwResourceStateSave: EECPSaveResourceState(FILE_);
        break;
    case O::PwPathSearchesLoad: return gPathManager.LoadSearches(FILE_);
    case O::PwPathSearchesSave: gPathManager.SaveSearches(FILE_);
        break;
    case O::PwActiveGroupsLoad: return EELoadActiveGroups(FILE_);
    case O::PwActiveGroupsSave: EESaveActiveGroups(FILE_);
        break;
    case O::PwGraphSearchesLoad: return gPathManager.LoadGraphSearches(FILE_);
    case O::PwGraphSearchesSave: gPathManager.SaveGraphSearches(FILE_);
        break;
    case O::PwDebugText: ESDebugOut("%s", (const char*)a[0]);
        break;
    case O::PwChainFirst: return P64(gWorld.mUnits.mFirst);
    case O::PwChainHead: return P64(reinterpret_cast<RTSUnit*>(&gWorld.mUnits));
    case O::PwTileNodeNext: return P64(RTS->mTileNode->mNext);
    case O::PwNodePrev: return P64(((RTSUnitLink*)p)->mPrev);
    case O::PwLink: RTS->Link((RTSUnit*)a[0]);
        break;
    case O::PwComplexOfSavedId: return P64((EEComplexUnit*)EEUnitHandle<EEComplexUnit>::FromSavedID((uint32_t)a[0]));
    case O::PwHandleId: return EEUnitHandleID(CX);
    case O::PwUnitReference: UNIT->Reference();
        break;
    case O::PwUnitDereference: UNIT->Dereference();
        break;
    case O::PwComplexOfId: return P64(dynamic_cast<EEComplexUnit*>(static_cast<EEUnit*>(gWorld.GetUnit((unsigned long)a[0]))));
    case O::PwForestCount: return (int64_t)gContinentManager.GetForests().size();
    case O::PwForestAt: return P64(gContinentManager.GetForests()[a[0]]);
    case O::PwForestRenewal: return (int64_t)FOREST->GetNextRenewal();
    case O::PwForestSetRenewal: FOREST->SetNextRenewal((unsigned long)a[0]);
        break;
    case O::PwForestSize: return (int64_t)(unsigned long)FOREST->GetNumTiles();
    case O::PwForestTiles: FOREST->GetTileSet().BuildPointList(*(std::vector<U2DPoint<long> >*)a[0], 1);
        break;
    case O::PwForestAtTile: return P64(gContinentManager.GetForestAt(U2DPoint<long>((long)a[0], (long)a[1])));
    case O::PwPointsNew: return P64(new std::vector<U2DPoint<long> >);
    case O::PwPointsDelete: delete &POINTS;
        break;
    case O::PwPointsCount: return (int64_t)POINTS.size();
    case O::PwPointX: return POINTS[a[0]].mX;
    case O::PwPointY: return POINTS[a[0]].mY;
    case O::PwMap: return P64(gWorld.mMap);
    case O::PwMapSize: return ((EEMap*)p)->mSize;
    case O::PwMapTileAt: return P64(((EEMap*)p)->mTiles[a[0]]);
    case O::PwMapRegions: return P64(((EEMap*)p)->mUnitRegions);
    case O::PwNodeUnit: return P64(((RTSTileUnitNode*)p)->mUnit);
    case O::PwNodeSetUnit: ((RTSTileUnitNode*)p)->mUnit = (EEUnit*)a[0];
        break;
    case O::PwNodeNext: return P64(((RTSTileUnitNode*)p)->mNext);
    case O::PwTileSetTerrainType: ((EEMapTile*)p)->mTerrainType = (long)a[0];
        break;
    case O::PwNumPlayers: return (int64_t)gWorld.mNumPlayers;
    case O::PwPlayerFirstUnit: return P64(PL->mFirstUnit);
    case O::PwPlayerGetUnit: return P64(PL->GetUnit((unsigned long)a[0]));
    case O::PwPlayerSetBucketFirst: PL->mUnitBuckets[a[0]].mFirst = (EEUnit*)a[1];
        break;
    case O::PwPlayerSetFirstUnit: PL->mFirstUnit = (EEUnit*)a[0];
        break;
    case O::PwPlayerSetLastUnit: PL->mLastUnit = (EEUnit*)a[0];
        break;
    case O::PwUnitNext: return P64(RTS->mNext);
    case O::PwUnitSetPrevious: RTS->mPrevious = (RTSUnit*)a[0];
        break;
    case O::PwUnitSetNext: RTS->mNext = (RTSUnit*)a[0];
        break;
    case O::PwDynComplex: return P64(dynamic_cast<EEComplexUnit*>(UNIT));
    case O::PwCxLastAttacker: return P64(CX->mLastAttacker);
    case O::PwCxGoalQueueCount: return (int64_t)CX->mGoalQueues.size();
    case O::PwCxGoalCount: return (int64_t)CX->mGoalQueues[a[0]].mGoals.size();
    case O::PwCxGoalAt: return P64(CX->mGoalQueues[a[0]].mGoals[a[1]]);
    case O::PwAttackUnitCast: return P64(EEAttackUnitCast((EEUnitGoal*)p));
    case O::PwAttackTarget: return P64(((EEUGRust*)p)->GetAttackTarget());
    case O::PwGatherGoalCast: return P64(EEGatherGoalCast((EEUnitGoal*)p));
    case O::PwGatherResource: return P64(((EEUGRust*)p)->GetGatherResource());
    case O::PwGatherStorage: return P64(((EEUGRust*)p)->GetGatherStorage());
    case O::PwCxGetPlacement: *(GETransformation*)a[0] = CX->mPhysicalModel.GetLocation();
        break;
    case O::PwCxSetPlacement: CX->mPhysicalModel.SetLocation(*(GETransformation*)a[0]);
        break;
    case O::PwUnitClassCount: return (int64_t)EEUnitClassCount();
    case O::PwUnitClassAt: return P64(gUnitClassTable[a[0]]);
    case O::PwComplexCreate: return P64(EEComplexUnit::Create());
    case O::PwCxInitUnit: CX->EEUnit::Init((EEUnitClass*)a[0], (EEPlayer*)a[1]);
        break;
    case O::PwUnitCreate: return P64(EEUnit::Create());
    case O::PwUnitInit: UNIT->Init((EEUnitClass*)a[0], (long)a[1], (long)a[2], (EEPlayer*)a[3]);
        break;
    case O::PwUnitSetTile: UNIT->mTile = (EEMapTile*)a[0];
        break;
    case O::PwRegisterUnit: RTSRegisterUnit(UNIT);
        break;
    case O::PwPlayerTechTree: return P64(PL->GetTechTree());
    case O::PwTreeNodeIds: ((EETechTree*)p)->GetNodeIDs(*(std::vector<long>*)a[0]);
        break;
    case O::PwTreeGet: return P64(((EETechTree*)p)->Get((long)a[0]));
    case O::PwNodeAllowed: return ((EETechTreeNode*)p)->mIsAllowed;
    case O::PwNodeSetAllowed: ((EETechTreeNode*)p)->mIsAllowed = a[0] != 0;
        break;
    // (EEPMultiplayerOp) The multiplayer state's C++ calls (EEPSaveGameHeader.cpp's EEPHeaderOp default): one statement each
    case O::PmPhysicsLoad: return gPhysics->Load((unsigned long)a[0], (unsigned long)a[1], *(FSFile*)p);
    case O::PmPhysicsSave: return gPhysics->Save((unsigned long)a[0], (unsigned long)a[1], *(FSFile*)p);
    case O::PmServerSetPhysicsTick: SERVER->SetCurrentPhysicsTick(*(unsigned long*)a[0]);
        break;
    case O::PmClientSetPhysicsTick: CLIENT->SetCurrentPhysicsTick(*(unsigned long*)a[0]);
        break;
    case O::PmCommCtsProcessed: return COMMS->GetNumberOfCTsProcessedInPhysics();
    case O::PmCommLastTick: return COMMS->GetLastTickReceived();
    case O::PmServerLastCt: return SERVER->GetLastCTMessageID();
    case O::PmServerCt: return SERVER->GetCurrentCommunicationsTick();
    case O::PmServerPt: return SERVER->GetCurrentPhysicsTick();
    case O::PmServerMessageNumber: return SERVER->GetMessageManagerCurrentMessageNumber();
    case O::PmClientCt: return CLIENT->GetCurrentCommunicationsTick();
    case O::PmClientPt: return CLIENT->GetCurrentPhysicsTick();
    // (EEPHelpersOp) The saved players' C++ calls and members (EEGameStateLoader.cpp's EEPLoaderOp default): one statement each
    case O::PphFields: return P64(&PPH->mLocalPlayerIndex);
    case O::PphPush: PPH->mPlayers.push_back((EEPlayer*)a[0]);
        break;
    case O::PphCopyPlayers: *(std::vector<EEPlayer*>*)a[0] = PPH->mPlayers;
        break;
    case O::PphPlayerChecksum: PL->GetChecksum(*(ESChecksum*)a[0]);
        break;
    case O::PphChecksumLocal: ((ESChecksum*)p)->AddChecksum("EEPPlayerHelper::mLocalPlayerIndex", (unsigned long)a[0]);
        break;
    case O::PphNewHuman: return P64(static_cast<EEPlayer*>(new EEHumanPlayer));
    case O::PphNewComputer: return P64(static_cast<EEPlayer*>(new EEComputerPlayer));
    case O::PphVerifyType3: ESVERIFY_AT(false, 251);
        break;
    case O::PphPlayerLoad: return PL->Load((unsigned long)a[0], (unsigned long)a[1], *(FSFile*)a[2]);
    case O::PphPlayerSave: return PL->Save((unsigned long)a[0], (unsigned long)a[1], *(FSFile*)a[2]);
    case O::PphComputerOf: return P64(PL->mComputerPlayer);
    case O::PphHasHuman: return PL->mHumanPlayer != NULL;
    case O::PphVerifyPlayerType: ESVERIFY_AT(a[0] != 0, 274);
        break;
    case O::PphCpSetEngineNext: CP->mEngine = kEECPEngineNext;
        break;
    case O::PphCpEngine: return CP->mEngine;
    case O::PphAiKnown: return EECPAIByID(BYTES) != NULL;
    case O::PphLogUnknownAi: EECPEngineLog("the saved game's player %lu plays the AI '%s', which Data/ai/ais.json does not list: the game cannot be loaded", PL->mPlayerID, BYTES.c_str());
        break;
    case O::PphCpSetAi: EECPNextSetAI(CP, BYTES);
        break;
    case O::PphCpLoadState: return EECPNextLoadState(CP, BYTES);
    case O::PphCpUnitsLoad: return EECPUnitsLoad(CP, BYTES);
    case O::PphHasUnitsLuaState: return EESaveHasUnitsLuaState((unsigned long)a[0], (unsigned long)a[1]);
    case O::PphIsAllied: return PL->IsAlliedWith((EEPlayer*)a[0]);
    case O::PphAddAlly: PL->mAllies.push_back((EEPlayer*)a[0]);
        break;
    case O::PphStrNew: return P64(new std::string);
    case O::PphStrDelete: delete &STR;
        break;
    case O::PphStrSize: return (int64_t)STR.size();
    case O::PphStrData: return P64(STR.data());
    case O::PphCpEngineAi: *(std::string*)a[0] = EECPEngineAI(CP);
        break;
    case O::PphClassicAiPresent: return EECPClassicAI() != NULL;
    case O::PphClassicAiId: *(std::string*)a[0] = EECPAIID(EECPClassicAI());
        break;
    case O::PphCpSaveState: EECPNextSaveState(CP, *(std::string*)a[0]);
        break;
    case O::PphCpUnitsSave: EECPUnitsSave(CP, *(std::string*)a[0]);
        break;
    // (EEPHeaderOp) The header's C++ calls and members (EEPPlayerHelper.cpp's EEPHelpersOp default): one statement each
    case O::HdrNameLoad: return HDR->mName.Load(AFILE);
    case O::HdrNameSave: return HDR->mName.Save(AFILE);
    case O::HdrDescriptionLoad: return HDR->mDescription.Load(AFILE);
    case O::HdrDescriptionSave: return HDR->mDescription.Save(AFILE);
    case O::HdrPlayerNamesLoad: return HDR->mPlayerNames.Load(AFILE);
    case O::HdrPlayerNamesSave: return HDR->mPlayerNames.Save(AFILE);
    case O::HdrDateLoad: return HDR->mDate.Load(AFILE);
    case O::HdrDateSave: return HDR->mDate.Save(AFILE);
    case O::HdrOptionsLoad: return HDR->mGameOptions.Load(HDR->mMajorVersion, HDR->mMinorVersion, AFILE);
    case O::HdrOptionsSave: return HDR->mGameOptions.Save(AFILE);
    case O::HdrMediaLoad: return HDR->mMediaStrings.Load(HDR->mMajorVersion, HDR->mMinorVersion, AFILE);
    case O::HdrMediaSave: return HDR->mMediaStrings.Save(HDR->mMajorVersion, HDR->mMinorVersion, AFILE);
    // (EEPChecksumOp) The checksum's C++ calls and members (EEPCalamityHelper.cpp's EEPCalamityOp default; the end of lane C): one
    // statement each
    case O::PcRust: return P64(static_cast<EEPChecksum*>((FSPersistent*)p)->GetRust());
    case O::PcUStrLoad: return ((UString*)p)->Load(*(FSFile*)a[0]);
    case O::PcUStrText: return P64((const char*)*(UString*)p);
    case O::PcSaveText: return UString((const char*)a[0]).Save(*(FSFile*)p);
    default: return EECore9OpD6(inOp, p, a, f);
    }
    return 0;
}

double EEPWorldOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::PwCameraZoom: return gWorld.mCameraControl.GetZoom();
    case O::PwWorldGameSpeed: return ((EEWorld*)p)->GetGameSpeed();
    default: return EEPLoaderOpF(inOp, p, a, f);
    }
}
