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
#include <time.h>
#include "ee/EmpireEarth.h"
#include "ee/EESaveLoad.h"
#include <stdio.h>
#include <string.h>
#include <map>
#include "lle/UResourceFile.h"
#include "lle/SESoundManager.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEPCalamityHelper.h"
#include "ee/EEUserInterface.h"
#include "ee/EEGameSound.h"
#include "ee/EEDataPreLoading.h"
#include "ee/EEUIFScenarioEditor.h"
#include "lle/FSChunk.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include <functional>
#include "ee/EEGameStateLoader.h"
#include "ee/EEPersistentTemplates.h"
#include "ee/EEStringIDs.h"
#include <utility>
#include "lle/USingleton.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEContagionManager.h"

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

int64_t EECommandLineOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EECommandLine.cpp

#define P64(x) ((int64_t)(intptr_t)(x))
void EEReportLoadFailure(const UString& inMessage, bool inArg, ESChecksum* inChecksum);  // EEWorld.cpp
extern bool sLoadingScenario;                                     // EEGameStateLoader.cpp
std::vector<EECalamity*>* EEActiveCalamities();                   // EEPCalamityHelper.cpp
void EEAfterLoadRust(void (*inRun)(uintptr_t), uintptr_t inData);  // EESaveLoad.cpp
#define PWH ((EEPWorldHelper*)p)
#define FILE_ (*(FSFile*)p)
#define VERSION (unsigned long)a[0], (unsigned long)a[1]
#define UNIT ((EEUnit*)p)
#define RTS ((RTSUnit*)p)
#define CX ((EEComplexUnit*)p)
#define PL ((EEPlayer*)p)
#define FOREST ((EEAIForest*)p)
#define POINTS (*(std::vector<U2DPoint<long> >*)p)

#define PPH ((EEPPlayerHelper*)p)
#define CP ((EEComputerPlayer*)p)
#define STR (*(std::string*)p)
#define BYTES std::string((const char*)a[0], (size_t)a[1])
#define HDR ((EEPSaveGameHeader*)p)
#define AFILE (*(FSFile*)a[0])
#define PCH ((EEPCalamityHelper*)p)
#define CL ((EECalamity*)p)
#define CALAMITIES (*(std::vector<EECalamity*>*)p)
#define CAFILE (*(FSFile*)a[2])
#define SPEC ((FSFileSpec*)p)
#define LIST (*(std::vector<FSPersistent*>*)p)
#define CHUNKS (*(std::vector<FSChunk>*)p)
#define CHUNK ((FSChunk*)p)
#define SPECS (*(std::vector<FSFileSpec>*)p)
#define WSTRS (*(std::vector<UWideString>*)p)
#define WSTR (*(UWideString*)p)
#define ST ((EEGameState*)p)
#define MEM ((FSMemoryFile*)p)
#define PERSISTENT ((FSPersistent*)p)
extern unsigned long& gMaxPlayers;       // 0x5318cc (EESERBUnitTypes.h's declaration; the save header's mL4E8)
extern EEGameState gGameState;
extern char gEmptyString[];             // 0x4e5b98 (the empty string; EECampaignProgress.cpp's declaration)
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
    case O::PwMainFormLoad: return gUIManager->GetMainGameForm()->Load(VERSION, FILE_);
    case O::PwMainFormSave: return gUIManager->GetMainGameForm()->Save(VERSION, FILE_);
    case O::PwAreaEffectsLoad: return EEAELoadRunningState(FILE_);
    case O::PwAreaEffectsSave: EEAESaveRunningState(FILE_);
        break;
    case O::PwTownsLoad: return EELoadTowns(FILE_, a[0] != 0);
    case O::PwTownsSave: EESaveTowns(FILE_);
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
    case O::PphLogUnknownAi: EECPEngineLog("the saved game's player %lu plays the AI '%s', which Data/ai/ais.json does not list: the game cannot be loaded", PL->mPlayerID, BYTES.c_str());
        break;
    case O::PphCpSetAi: EECPNextSetAI(CP, BYTES);
        break;
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
    // (EECore11OpC8) the scenario extension's text (gamecpp persist_text.rs; EETechCapture.cpp's EECore11OpC7 default)
    case O::C11ScenarioExtension: return (int64_t)(intptr_t)(const char*)gScenarioExtension;
    // (EEPLoaderOp) The loader's C++ (EEPWorldHelper.cpp's EEPWorldOp default): one statement each
    case O::PsSetLoadingScenario: sLoadingScenario = EEIsScenarioFile(((FSFile*)p)->GetFile());
        break;
    case O::PsClearLoadingScenario: sLoadingScenario = false;
        break;
    // (EECore10OpB8) the chunk's constructor's, destructor's, Reset's and copy's C++ (gamecpp persist_more.rs; p the chunk): one
    // statement each
    case O::PchClearContagion: PCH->ClearContagionManager();
        break;
    case O::PchNewContagion: PCH->mContagionManager = new EEContagionManager;
        break;
    case O::PchContagion: return P64(PCH->mContagionManager);
    case O::PchCopyCalamities: *PCH->mCalamities = *((const EEPCalamityHelper*)a[0])->mCalamities;
        break;
    case O::PchCopyContagion: *PCH->mContagionManager = *((const EEPCalamityHelper*)a[0])->mContagionManager;
        break;
    // (EEPCalamityOp) The calamities' C++ calls and members (EEPMultiplayerHelper.cpp's EEPMultiplayerOp default): one statement each
    case O::PchClearCalamities: PCH->ClearCalamities();
        break;
    case O::PchCalamities: return P64(PCH->mCalamities);
    case O::PchNewCalamities: PCH->mCalamities = new std::vector<EECalamity*>;
        break;
    case O::PchNewCalamity: return P64(new EECalamity);
    case O::PchCalamityLoad: return CL->Load((unsigned long)a[0], (unsigned long)a[1], CAFILE);
    case O::PchCalamitySave: return CL->Save((unsigned long)a[0], (unsigned long)a[1], CAFILE);
    case O::PchPush: PCH->mCalamities->push_back((EECalamity*)a[0]);
        break;
    case O::PchContagionLoad: return PCH->mContagionManager->Load((unsigned long)a[0], (unsigned long)a[1], CAFILE);
    case O::PchCount: return (int64_t)PCH->mCalamities->size();
    case O::PchAt: return P64((*PCH->mCalamities)[a[0]]);
    case O::PchPlaySound: CL->PlaySound();
        break;
    case O::PchDeleteAt: delete std::exchange((*PCH->mCalamities)[a[0]], nullptr);
        break;
    case O::PchRestoreContagions: return PCH->mContagionManager->RestorePointers(*PCH->mCalamities);
    case O::PchActive: return P64(EEActiveCalamities());
    case O::PchVerifyActive: ESVERIFY_AT(a[0] != 0, 334);
        break;
    case O::VecCalamitiesCount: return (int64_t)CALAMITIES.size();
    case O::VecCalamitiesAt: return P64(CALAMITIES[a[0]]);
    case O::PchClassIndex: return USingleton<EECalamityManager>::GetInstance()->GetCalamityClassIndex(CL->mClass, *(long*)a[0]);
    case O::PchVerifyClassIndex: ESVerify(__FILE__, 349, "EECalamityManager::Instance()->GetCalamityClassOffsetToSave(theCurrentCalamityClass, theCurrentClassOffset)");   // the original assert text
        break;
    case O::PchGetTarget: return CL->GetTargetUnit(*(EEUnit**)a[0]);
    case O::PchContagionSave: return gWorld.GetContagionManager()->Save((unsigned long)a[0], (unsigned long)a[1], CAFILE);
    // (EECore9OpC) The save and load bodies' C++ calls and members (EEGraphics.cpp's chain: EECore9OpB's default): one statement each
    case O::PfReadULong: return FILE_.Read(*(unsigned long*)a[0]);
    case O::PfReadLong: return FILE_.Read(*(long*)a[0]);
    case O::PfReadU16: return FILE_.Read(*(unsigned short*)a[0]);
    case O::PfReadI8: return FILE_.Read(*(signed char*)a[0]);
    case O::PfReadU8: return FILE_.Read(*(unsigned char*)a[0]);
    case O::PfReadF32: return FILE_.Read(*(float*)a[0]);
    case O::PfReadBool: return FILE_.Read(*(bool*)a[0]);
    case O::PfReadChars: return FILE_.Read((char*)a[0], (unsigned long)a[1], *(unsigned long*)a[2]);
    case O::PfWriteULong: return FILE_.Write(*(const unsigned long*)a[0]);
    case O::PfWriteLong: return FILE_.Write(*(const long*)a[0]);
    case O::PfWriteU16: return FILE_.Write(*(const unsigned short*)a[0]);
    case O::PfWriteI8: return FILE_.Write(*(const signed char*)a[0]);
    case O::PfWriteU8: return FILE_.Write(*(const unsigned char*)a[0]);
    case O::PfWriteF32: return FILE_.Write(*(const float*)a[0]);
    case O::PfWriteBool: return FILE_.Write(*(const bool*)a[0]);
    case O::PfWriteChars: return FILE_.Write((const char*)a[0], (unsigned long)a[1], *(unsigned long*)a[2]);
    case O::PvLongsNew: return P64(new std::vector<long>);
    case O::PvLongsDelete: delete (std::vector<long>*)p;
        break;
    case O::PvLongsCount: return (int64_t)((std::vector<long>*)p)->size();
    case O::PvLongsAt: return (*(std::vector<long>*)p)[a[0]];
    case O::PvAfterLoad: EEAfterLoadRust((void (*)(uintptr_t))a[0], (uintptr_t)a[1]);
        break;
    case O::PsMajorVersion: return kSaveGameMajorVersion;
    case O::PsListNew: return P64(new std::vector<FSPersistent*>);
    case O::PsListDelete: delete &LIST;
        break;
    case O::PsListPush: LIST.push_back((FSPersistent*)a[0]);
        break;
    case O::PsListCount: return (int64_t)LIST.size();
    case O::PsListAt: return P64(LIST[a[0]]);
    case O::PsListClear: LIST.clear();
        break;
    case O::PsPersistentTag: return PERSISTENT->GetTagID();
    case O::PsPersistentDelete: delete PERSISTENT;
        break;
    case O::PsNewChecksumChunk: return P64(static_cast<FSPersistent*>(new EEPChecksum(false)));
    case O::PsWorldChecksum: gWorld.GetChecksum(*static_cast<EEPChecksum*>(PERSISTENT));
        break;
    case O::PsNewSeedChunk: return P64(static_cast<FSPersistent*>(new EEPTGeneric<unsigned long>(gUserInterface->mRandomSeed, 2)));
    case O::PsNewMapChunk: return P64(static_cast<FSPersistent*>(new EEPTPRestorable<EEMap>(gWorld.GetMap(), 4)));
    case O::PsMapChunkUnown: static_cast<EEPTPRestorable<EEMap>*>(PERSISTENT)->mOwned = false;
        break;
    case O::PsMapChunkObject: return P64(static_cast<EEPTPRestorable<EEMap>*>(PERSISTENT)->mObject);
    case O::PsNewCameraChunk: return P64(static_cast<FSPersistent*>(new EEPTRestorable<GE3DPoint>(*(GE3DPoint*)a[0], 3)));
    case O::PsCameraChunkValue: return P64(&static_cast<EEPTRestorable<GE3DPoint>*>(PERSISTENT)->mValue);
    case O::PsNewScenarioChunk: return P64(static_cast<FSPersistent*>(new EEPTPRestorable<EEScenarioManager>(gWorld.mScenarioManager, 6)));
    case O::PsScenarioChunkUnown: static_cast<EEPTPRestorable<EEScenarioManager>*>(PERSISTENT)->mOwned = false;
        break;
    case O::PsScenarioChunkObject: return P64(static_cast<EEPTPRestorable<EEScenarioManager>*>(PERSISTENT)->mObject);
    case O::PsNewEffectsChunk: return P64(static_cast<FSPersistent*>(new EEPTPRestorable<EEEffectManager>(gWorld.mEffectManager, 10)));
    case O::PsEffectsChunkUnown: static_cast<EEPTPRestorable<EEEffectManager>*>(PERSISTENT)->mOwned = false;
        break;
    case O::PsEffectsChunkObject: return P64(static_cast<EEPTPRestorable<EEEffectManager>*>(PERSISTENT)->mObject);
    case O::PsNewEnvironmentChunk: return P64(static_cast<FSPersistent*>(new EEPTPRestorable<EEEnvironmentManager>(gWorld.mEnvironmentManager, 12)));
    case O::PsEnvironmentChunkUnown: static_cast<EEPTPRestorable<EEEnvironmentManager>*>(PERSISTENT)->mOwned = false;
        break;
    case O::PsEnvironmentChunkObject: return P64(static_cast<EEPTPRestorable<EEEnvironmentManager>*>(PERSISTENT)->mObject);
    case O::PsNewPlayerHelper: return P64(static_cast<FSPersistent*>(new EEPPlayerHelper(gWorld)));
    case O::PsNewWorldHelper: return P64(static_cast<FSPersistent*>(new EEPWorldHelper(gWorld)));
    case O::PsNewMultiplayerHelper: return P64(static_cast<FSPersistent*>(new EEPMultiplayerHelper()));
    case O::PsNewCalamityHelper: return P64(static_cast<FSPersistent*>(new EEPCalamityHelper()));
    case O::PsCastChecksum: return P64(dynamic_cast<EEPChecksum*>(PERSISTENT));
    case O::PsCastSeed: return P64(dynamic_cast<EEPTGeneric<unsigned long>*>(PERSISTENT));
    case O::PsSeedValue: return (int64_t)((EEPTGeneric<unsigned long>*)p)->mValue;
    case O::PsCastEffects: return P64(static_cast<FSPersistent*>(dynamic_cast<EEPTPRestorable<EEEffectManager>*>(PERSISTENT)));
    case O::PsCastCamera: return P64(static_cast<FSPersistent*>(dynamic_cast<EEPTRestorable<GE3DPoint>*>(PERSISTENT)));
    case O::PsCastMap: return P64(static_cast<FSPersistent*>(dynamic_cast<EEPTPRestorable<EEMap>*>(PERSISTENT)));
    case O::PsCastPlayers: return P64(dynamic_cast<EEPPlayerHelper*>(PERSISTENT));
    case O::PsCastWorld: return P64(dynamic_cast<EEPWorldHelper*>(PERSISTENT));
    case O::PsCastScenario: return P64(static_cast<FSPersistent*>(dynamic_cast<EEPTPRestorable<EEScenarioManager>*>(PERSISTENT)));
    case O::PsCastCalamities: return P64(dynamic_cast<EEPCalamityHelper*>(PERSISTENT));
    case O::PsCastEnvironment: return P64(static_cast<FSPersistent*>(dynamic_cast<EEPTPRestorable<EEEnvironmentManager>*>(PERSISTENT)));
    case O::PsHeaderNew: return P64(new EEPSaveGameHeader);
    case O::PsHeaderDelete: delete HDR;
        break;
    case O::PsHeaderSave: return HDR->Save((unsigned long)a[0], (unsigned long)a[1], *(FSFile*)a[2]);
    case O::PsHeaderLoad: return HDR->Load((unsigned long)a[0], (unsigned long)a[1], *(FSFile*)a[2]);
    case O::PsHdrSetName: HDR->mName = *(const UString*)a[0];
        break;
    case O::PsHdrDefaultDescription: HDR->mDescription = UString("No game description provided.");
        break;
    case O::PsHdrVersions: return P64(&HDR->mMajorVersion);
    case O::PsHdrNumbers: return P64(&HDR->mL4E4);
    case O::PsHdrNamesClear: HDR->mPlayerNames = UString();
        break;
    case O::PsHdrNamesAppendPlayer: HDR->mPlayerNames += UString((const char*)gWorld.mPlayers[a[0]]->mName);
        break;
    case O::PsHdrNamesAppendSpace: HDR->mPlayerNames += " ";
        break;
    case O::PsHdrSetDate: HDR->mDate = GetDateText();
        break;
    case O::PsHdrTagsClear: HDR->mChunkTypes.clear();
        break;
    case O::PsHdrTagsPush: HDR->mChunkTypes.push_back((long)a[0]);
        break;
    case O::PsHdrTagsCount: return (int64_t)HDR->mChunkTypes.size();
    case O::PsHdrTagAt: return HDR->mChunkTypes[a[0]];
    case O::PsWorldScenario: return P64(gWorld.mScenarioManager);
    case O::PsHdrMediaFromScenario: HDR->mMediaStrings = gWorld.mScenarioManager->mMediaStrings;
        break;
    case O::PsHdrOptionsFromWorld: HDR->mGameOptions = gWorld.mGameOptions;
        break;
    case O::PsMaxPlayers: return gMaxPlayers;
    case O::PsSpecNew: return P64(new FSFileSpec);
    case O::PsSpecDelete: delete SPEC;
        break;
    case O::PsSpecSetDirectory: SPEC->SetDirectory(*(const UString*)a[0]);
        break;
    case O::PsSpecFirstFile: return SPEC->GetFirstFile(*(const UString*)a[0], *(FSFileSpec*)a[1]);
    case O::PsSpecNextFile: return SPEC->GetNextFile(*(FSFileSpec*)a[0]);
    case O::PsSpecsNew: return P64(new std::vector<FSFileSpec>);
    case O::PsSpecsDelete: delete &SPECS;
        break;
    case O::PsSpecsCount: return (int64_t)SPECS.size();
    case O::PsSpecLess: return *SPEC < (*(std::vector<FSFileSpec>*)a[0])[a[1]];
    case O::PsSpecsInsert: SPECS.insert(SPECS.begin() + a[0], *(FSFileSpec*)a[1]);
        break;
    case O::PsSpecsDeleteFirst: SPECS[0].Delete();
        break;
    case O::PsSpecsEraseFirst: SPECS.erase(SPECS.begin());
        break;
    case O::PsSpecSetStartupVolume: SPEC->SetVolume(gStartupVolume);
        break;
    case O::PsSpecSetStartupDirectory: SPEC->SetDirectory(gStartupDirectory + *(const UString*)a[0]);
        break;
    case O::PsSpecSetFile: SPEC->SetFile(*(const UString*)a[0]);
        break;
    case O::PsSpecDeleteFile: SPEC->Delete();
        break;
    case O::PsSpecCreate: return SPEC->Create();
    case O::PsSpecOpen: return SPEC->Open((unsigned long)a[0]);
    case O::PsSpecClose: return FILE_.Close();
    case O::PsSpecOpenAny: return FILE_.Open((unsigned long)a[0]);
    case O::PsFileIsOpen: return FILE_.GetIsOpen();
    case O::PsFileCloseAny: return FILE_.Close();
    case O::PsChunksNew: return P64(new std::vector<FSChunk>);
    case O::PsChunksDelete: delete &CHUNKS;
        break;
    case O::PsChunksResize: CHUNKS.resize((size_t)a[0]);
        break;
    case O::PsChunksClear: CHUNKS.clear();
        break;
    case O::PsChunkSavePersistent: return CHUNKS[a[0]].SavePersistentToChunk((unsigned long)a[1], (unsigned long)a[2], (FSPersistent*)a[3]);
    case O::PsChunkSaveToFile: return CHUNKS[a[0]].SaveChunkToFile(*(FSFile*)a[1]);
    case O::PsChunkNew: return P64(new FSChunk);
    case O::PsChunkDelete: delete CHUNK;
        break;
    case O::PsChunkLoadHeader: return CHUNK->LoadChunkHeader(*(FSFile*)a[0]);
    case O::PsChunkGetSize: return CHUNK->GetChunkSize(*(unsigned long*)a[0]);
    case O::PsChunkSkip: return CHUNK->SkipChunk(*(FSFile*)a[0]);
    case O::PsChunkLoadFromFile: return CHUNK->LoadChunkFromFile(*(FSFile*)a[0]);
    case O::PsChunkLoadPersistent: return CHUNK->LoadPersistentFromChunk(*(FSPersistent**)a[0]);
    case O::PsMemFileNew: return P64(new FSMemoryFile(0x400, true, 1000));
    case O::PsMemFileNew1000: return P64(new FSMemoryFile(1000, true, 1));
    case O::PsMemFileDelete: delete MEM;
        break;
    case O::PsMemFileSave: return MEM->Save(*(FSFile*)a[0]);
    case O::PsMemFileLoad: return MEM->Load(*(FSFile*)a[0]);
    case O::PsUStrNew: return P64(new UString);
    case O::PsUStrDelete: delete (UString*)p;
        break;
    case O::PsSaveTextName: *(UString*)a[0] = GetSaveTextName(SPEC->GetFile(), a[1] != 0);
        break;
    case O::PsMessageString: gResource->GetString((long)a[0], gLoadSaveErrorMessage);
        break;
    case O::PsMessageStringVerify: ESVERIFY_AT(::gResource->GetString((long)a[0], ::gLoadSaveErrorMessage) == keseOK, 669);
        break;
    case O::PsResourceString: gResource->GetString((long)a[0], WSTR);
        break;
    case O::PsResourceStringVerify: ESVERIFY_AT(::gResource->GetString((long)a[0], WSTR) == keseOK, 967);
        break;
    case O::PsGameState: return P64(&gGameState);
    case O::PsValidateIntoMessage: return EEValidateSaveGameFile(FILE_, gLoadSaveErrorMessage);
    case O::PsMapApplyVertexColors: gWorld.GetMap()->ApplyVertexColors();
        break;
    case O::PsSetMasterVolume: gSoundManager->SetMasterVolume(f[0]);
        break;
    case O::PsSetMusicVolume: gConductor->GetConductor().SetVolume(f[0]);
        break;
    case O::PsShowSplash: gUIManager->SwapForm(gUIManager->GetFormByID(kFormSplash));
        break;
    case O::PsShowSelectionAreas: gWorld.GetCurrentPlayer()->mActiveGroup->ShowSelectionAreas();
        break;
    case O::PsOpenMainGameForm: gUserInterface->OpenForm(gUIManager->GetMainGameForm());
        break;
    case O::PsWaitRenderNow: gGraphics->mRenderNow->Wait(kTSWaitInfinite);
        break;
    case O::PsSetLoadingSavedGame: gLoadingSavedGame = a[0] != 0;
        break;
    case O::PsStClear: ST->Clear();
        break;
    case O::PsStReset: ST->Reset();
        break;
    case O::PsStInit: ST->Init();
        break;
    case O::PsStWriteLock: ST->WriteLock();
        break;
    case O::PsStWriteUnlock: ST->WriteUnlock();
        break;
    case O::PsStFinishLoad: ST->FinishLoad((unsigned long)a[0], (unsigned long)a[1]);
        break;
    case O::PsStOptionsFromHeader: ST->mGameOptions = ((EEPSaveGameHeader*)a[0])->mGameOptions;
        break;
    case O::PsStSetChecksum: ST->mChecksum = *(EEPChecksum*)a[0];
        break;
    case O::PsStSetEffectManager: ST->SetEffectManager((EEEffectManager*)a[0]);
        break;
    case O::PsStSetMap: ST->SetMap((EEMap*)a[0]);
        break;
    case O::PsStSetEnvironmentManager: ST->SetEnvironmentManager((EEEnvironmentManager*)a[0]);
        break;
    case O::PsStRestorablePushEffects: ST->mRestorables.push_back((EEEffectManager*)a[0]);
        break;
    case O::PsStRestorablePushCalamities: ST->mRestorables.push_back((EEPCalamityHelper*)a[0]);
        break;
    case O::PsStRestorablePushEnvironment: ST->mRestorables.push_back((EEEnvironmentManager*)a[0]);
        break;
    case O::PsStLoadedPlayerPush: ST->mLoadedPlayers.push_back((EEPlayer*)a[0]);
        break;
    case O::PsPlayerRemoveAlly: ((EEPlayer*)p)->RemoveAlly((EEPlayer*)a[0]);
        break;
    case O::PsEditorFormPresent: return gUIManager->GetScenarioEditorForm() != NULL;
    case O::PsEditorClearSelection: gUIManager->GetScenarioEditorForm()->ClearSelection();
        break;
    case O::PsWorldClear: gWorld.Clear();
        break;
    case O::PsInScenarioEditor: return gUIManager->mInScenarioEditor;
    case O::PsDebugCorrupt: ESDebugOut("EE load failure: Load game file is corrupt.\n");
        break;
    case O::PsDataPathFind: return gDataPath->FindFile(*(const UString*)a[0], *(FSFileSpec**)a[1]);
    case O::PsMapL195610: return ((EEMap*)p)->mL195610;
    case O::PsMapL195614: return ((EEMap*)p)->mL195614;
    case O::PsPphMaxPlayers: return (int64_t)((EEPPlayerHelper*)p)->mMaxPlayers;
    case O::PsPphLocalIndex: return ((EEPPlayerHelper*)p)->GetLocalPlayerIndex(*(long*)a[0]);
    case O::PsPphPlayerCount: return (int64_t)((EEPPlayerHelper*)p)->mPlayers.size();
    case O::PsPphPlayerAt: return P64(((EEPPlayerHelper*)p)->mPlayers[a[0]]);
    case O::PsWStrsNew: return P64(new std::vector<UWideString>);
    case O::PsWStrsDelete: delete &WSTRS;
        break;
    case O::PsWStrsClear: WSTRS.clear();
        break;
    case O::PsWStrsPush: WSTRS.push_back(*(UWideString*)a[0]);
        break;
    case O::PsWStrsPushSlotName: WSTRS.push_back(((EEGameOptions*)a[0])->mPlayers[a[1]].mName);
        break;
    case O::PsWStrsCount: return (int64_t)WSTRS.size();
    case O::PsWStrsAt: return P64(&WSTRS[a[0]]);
    case O::PsWStrsContains: return std::find(WSTRS.begin(), WSTRS.end(), *(UWideString*)a[0]) != WSTRS.end();
    case O::PsWStrNew: return P64(new UWideString);
    case O::PsWStrDelete: delete &WSTR;
        break;
    case O::PsWStrClear: WSTR = gEmptyString;
        break;
    case O::PsWStrLength: return WSTR.GetLength();
    case O::PsSprintfNumber: sprintf(WSTR, *(UWideString*)a[0], (unsigned long)a[1]);
        break;
    case O::PsSprintfText: sprintf(WSTR, *(UWideString*)a[0], (const char*)*(UWideString*)a[1]);
        break;
    case O::PsSprintfTwoNumbers: sprintf(WSTR, *(UWideString*)a[0], (size_t)a[1], (unsigned long)a[2]);
        break;
    default: return EECommandLineOp(inOp, p, a, f);
    }
    return 0;
}

double EECore9OpD7F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EETechTreeNode.cpp
double EEPWorldOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::PwCameraZoom: return gWorld.mCameraControl.GetZoom();
    case O::PwWorldGameSpeed: return ((EEWorld*)p)->GetGameSpeed();
    // (EECore9OpCF)
    case O::PsMasterVolume: return gSoundManager->GetMasterVolume();
    case O::PsMusicVolume: return gConductor->GetConductor().GetVolume();
    default: return EECore9OpD7F(inOp, p, a, f);
    }
}
