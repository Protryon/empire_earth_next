// C:\EEX\Empire Earth\Empire Earth\EEWorld.cpp (name from RTTI EEWorld; no __FILE__). 0x137ba3..0x13e967: the game
// world (ctor, ??_G, dtor, the team/diplomacy helpers, the game setup from an EEGameState, the end-of-game statistics,
// the owning setters, the vtable slots), then a file-scope UString("default") whose init chain sits mid-file at
// 0x13c740, the rest of the members (0x13c772..0x13cdb9) and the unit's template COMDATs up to Empire Earth.cpp's
// chains (0x13e967). EEVoting.cpp ends at 0x137ba3. Unit flags: the exe's module set. Names are provisional unless a
// header names them.
#include <windows.h>
#include "platform/PlatSim.h"
#include <stdio.h>
#include <string.h>
#include <deque>
#include <map>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GERectangle.h"
#include "lle/NEMessage.h"
#include "lle/U2DPoint.h"
#include "lle/UIEvent.h"
#include "lle/UResourceFile.h"
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
#include "lle/GEModelManager.h"
#include "lle/FSFileSpec.h"
#include "lle/FSFile.h"
#include "lle/FSMemoryFile.h"
#include "lle/ESChecksum.h"
#include "lle/TSScheduler.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DRectangle.h"
#include "lle/USingleton.h"
#include "lle/SESoundManager.h"
#include "lle/UIControl.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIFormManager.h"
#include "lle/GELine.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "lle/NESynchronousClient.h"
#include "lle/NESynchronousServer.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitStore.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EERules.h"
#include "ee/EEUnitClassIDs.h"
#include "ee/EETechTreeTechID.h"
#include "ee/EEUnit.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEHumanPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EECPASGFlags.h"
#include "ee/EETeam.h"
#include "ee/EEVictoryCondition.h"
#include "ee/EEVCWonder.h"
#include "ee/EEGameState.h"
#include "ee/EEGameOptions.h"
#include "ee/EEStatisticsRecord.h"
#include "ee/EEDatabase.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEMiniMap.h"
#include "ee/EEUserInterface.h"
#include "ee/EECommunications.h"
#include "ee/EEClient.h"
#include "ee/EEServer.h"
#include "ee/EEWonNet.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEPhysics.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEEffectManager.h"
#include "ee/EECalamityManager.h"
#include "ee/EEPCalamityHelper.h"
#include "ee/EEPathManager.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEInfluenceTable.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EECPTrace.h"
#include "ee/EEArtificialIntelligence.h"
#include "ee/EEPPatrolGroup.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEButtonObject.h"
#include "ee/EEGameSound.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEMusicManager.h"
#include "ee/EEAmbientSoundManager.h"
#include "ee/EECampaignProgress.h"
#include "ee/EECivilizationBuilder.h"
#include "ee/EECivilization.h"
#include "ee/EEGameVariant.h"
#include "ee/EEColorTable.h"
#include "ee/EEStartingResources.h"
#include "ee/EERenderLists.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIEChatLogEvents.h"
#include "ee/EEUIMiniMap.h"
#include "ee/EEGameUI.h"
#include "ee/EEAEManager.h"
#include "ee/EEStringIDs.h"
#include "ee/EEWorldLayout.h"
#include "rust/sim/app.h"   // EeAppStats* (ee_sim app statistics.rs)
#include "rust/game/gamecpp.h"   // the bodies (ee_game gamecpp world.rs)

// Unit-local declarations of functions and globals no header declares yet (provisional names)
extern bool gLocalPlayerOut;                // 0x4e6099 (ee/EEMessage.h)
extern bool gSightUpdatesEnabled;
extern bool& gAmbientSoundEnabled;           // 0x5308de (the ambient sound switch)
extern bool& gDisplayWeatherEffects;
extern long gHerdUnreadValue;                       // 0x4e5e40 (only use: cleared with `and dword ptr, 0`: an integer)
extern float gViewportHalfWidth;
extern float gViewportXCenter;
extern float gViewportYCenter;
extern UWideString& gWonPlayerName;          // 0x53077c (the WON player name)
extern UString gDefaultCampaignUser;        // 0x52a170 ("default"; defined below, its init chain mid-file)
extern char gEmptyString[];                 // 0x4e5b98 (pooled "")
void EEStopAllSounds();
void EEDumpGraphicObjectTable(FSFileSpec& inFile);
void EEDumpLeakedUnits(FSFileSpec& inFile);
void EEReportLoadFailure(const UString& inMessage, bool inArg, ESChecksum* inChecksum);
void EEUpdateLandContinents();
void EEUpgradeHitPoints(EEUnitClassUpgrade* inUpgrade, long inType, float inFactor, EEPlayer* inPlayer);

// EEGameState's team count: inline (its refused copy 0x13cff7 sits in this unit's template tail); defined here where
// EETeam is complete
inline unsigned long EEGameState::GetNumTeams() const
{
    return this->mTeams.size();
}

// Takes a player out of the game: marks them out, drops them from their teams, reveals the map to them (except a lost
// scenario), stamps their end time and ends any cinematic; for the local player it redraws the minimap. Its own
// lines: the test profile's harness hook (tools/build.py HARNESS_HOOKS)
void EEWorld::RemovePlayerFromGame(bool inHasWon, unsigned long inPlayerID)
{
    ee_gamecpp_world_remove_player(this, inHasWon, inPlayerID);
}

// Creates the game world with default settings (time of day 0.35, cheats and unit improvements allowed), no managers
// and empty statistics
EEWorld::EEWorld()
{
    ee_gamecpp_world_init(this);   // the members (ee_game gamecpp world.rs)
}

// The statistics history's ctor (inline in the real header; its refused copy follows its first user):
// Creates an empty statistics history
EEStatisticsHistory::EEStatisticsHistory()
{
    this->Reset();
}

// Destroys the world, deleting its influence table, ghost units and effect, scenario, environment and calamity managers
EEWorld::~EEWorld()
{
    ee_gamecpp_world_destroy(this);   // (gamecpp world.rs)
}

// Grants each player the wonder power 4 population share bonus when they hold it with no enemy holder or only allies
// hold it, and takes it away when only enemies hold it (mixed allies and enemies: no change)
void EEWorld::ApplyShareBonus()
{
    EEPlayerShareBonus(false);   // (ee_units player_units.rs: the player lane, round 2)
}


// Adds the world's state and the game's random seed to a sync checksum
void EEWorld::GetChecksum(ESChecksum& ioChecksum)
{
    RTSWorld::GetChecksum(ioChecksum);
    unsigned long theSeed = gUserInterface->mRandomSeed;
    ioChecksum.AddChecksum("EEWorld", theSeed);
}

// Returns the calamity helper's contagion (plague) manager
EEContagionManager* EEWorld::GetContagionManager()
{
    return (static_cast<EEPCalamityHelper*>(this->mCalamityHelper))->mContagionManager;
}

// Returns whether a map tile's height is below 0
bool EEWorld::IsTileBlocked(unsigned long inX, unsigned long inY)
{
    return this->mMap->GetTile(inX, inY)->GetHeight() < 0.0f;
}


// Replaces the world's map, deleting the old one
void EEWorld::SetMap(EEMap* inMap)
{
    delete this->mMap;
    this->mMap = inMap;
}

// Replaces the scenario manager, deleting the old one
void EEWorld::SetScenarioManager(EEScenarioManager* inManager)
{
    delete this->mScenarioManager;
    this->mScenarioManager = inManager;
}

// Creates the influence table for a map and the world's player count
void EEWorld::CreateInfluenceTable(EEMap* inMap)
{
    delete this->mInfluenceTable;   // SetInfluenceTable 0x13c201
    this->mInfluenceTable = new EEInfluenceTable(inMap->mSize, this->mNumPlayers);
}

// Replaces the minimap with a new one for the world's map, viewed by the game state's local player
void EEWorld::CreateMiniMap(EEGameState* inState)
{
    delete gMiniMap;
    gMiniMap = new EEMiniMap(GERasterizer::GetActiveRasterizer()->GetScreen(gScreenIndex), 0, inState->GetPlayer(inState->mLocalPlayer));
    gMiniMap->SetMap(this->mMap);
    gEEGameUI.mMiniMapUpdate(gMiniMap);   // EEUIMiniMap::Update (ee/EEGameUI.h)
}

// Builds the post-game statistics: the history's fields here, the scores, the computers' top-ups, the records' order
// and flags and the computers' play times in Rust (ee_sim app statistics.rs), then the records filled
extern "C" void ee_gamecpp_world_record_statistics(uintptr_t inWorld);   // gamecpp world_stats.rs (cbindgen:ignore: its
                                                                        // statistics imports name a generic)
void EEWorld::RecordStatistics()
{
    ee_gamecpp_world_record_statistics((uintptr_t)this);
}

// Creates an empty statistics record
EEStatisticsRecord::EEStatisticsRecord()
{
    this->Clear();
}

// Applies the queued unit activations and deactivations to the list of units updated every tick
void EEWorld::UpdateActiveUnits()
{
    ee_gamecpp_world_update_active();   // (gamecpp world.rs: QueueBornDeactivations, then the lists' update)
}

// Undoes the wonder power 4 population share bonus ApplyShareBonus gave or took for every player
void EEWorld::RemoveShareBonus()
{
    EEPlayerShareBonus(true);   // (ee_units player_units.rs)
}

// Moves a unit to its place in the world's unit list, which is sorted by tile x
void EEWorld::ResortUnit(RTSUnit* inUnit)
{
    ee_gamecpp_world_resort(static_cast<EEUnit*>(inUnit));
}

// Moves the world and viewport camera unless a scenario camera move is running (SetCamera 0x1394b2 unforced)
void EEWorld::SetCameraTransformation(const GETransformation& inTransformation)
{
    if (this->mScenarioManager == NULL || !this->mScenarioManager->mIsMovingCamera) {
        RTSWorld::SetCameraTransformation(inTransformation);
        gViewport.SetCamera(inTransformation);
    }
}

// Replaces the environment (weather) manager, deleting the old one
void EEWorld::SetEnvironmentManager(EEEnvironmentManager* inManager)
{
    delete this->mEnvironmentManager;
    this->mEnvironmentManager = inManager;
}

// A file-scope global defined mid-file (its init chain sits between MovePlayerToSlot and UnloadMedia; no reader):
UString gDefaultCampaignUser("default");

// The end-of-game check's world calls (ee_units game_over.rs reads the players and units through the world views)
extern "C" int64_t ee_units_game_over_host(int32_t inOp, int64_t inA, int64_t inB)
{
    switch (inOp) {
    case EE_UNITS_GAME_OVER_SCENARIO: return gWorld.mGameOptions.mIsScenarioGame;
    case EE_UNITS_GAME_OVER_DEFEAT_RULE: return EEGetRules().mGameOptions.mDefeatWithoutRecovery;
    case EE_UNITS_GAME_OVER_TEAMS: return (int64_t)gWorld.mTeams.size();
    case EE_UNITS_GAME_OVER_TEAM_STATE: return gWorld.mTeams[(size_t)inA].GetVictoryState();
    case EE_UNITS_GAME_OVER_TEAM_SIZE: return (int64_t)gWorld.mTeams[(size_t)inA].mPlayers.size();
    case EE_UNITS_GAME_OVER_TEAM_PLAYER: return (int64_t)gWorld.mTeams[(size_t)inA].mPlayers[(size_t)inB];
    case EE_UNITS_GAME_OVER_IS_HUMAN: return gWorld.mPlayers[inA]->IsHuman();
    case EE_UNITS_GAME_OVER_REMOVE: gWorld.RemovePlayerFromGame(inB != 0, (unsigned long)inA); return 0;
    case EE_UNITS_GAME_OVER_SET_RESULT: gWorld.mPlayers[inA]->SetGameResult((long)inB); return 0;
    case EE_UNITS_GAME_OVER_CAN_TRAIN: {
        // New (gameoptions.yaml defeat_without_recovery): the means to train a citizen
        EEPlayer* thePlayer = gWorld.mPlayers[inA];
        EETechTreeNode* theCitizen = thePlayer->mTechnologies != NULL ? thePlayer->mTechnologies->Get(kTechCitizen) : NULL;
        return theCitizen == NULL || thePlayer->EEPlayer::CanAfford(theCitizen, kUnitTypeCitizen, 1);
    }
    }
    return 0;
}

// Updates the physics timing after the game speed changed
void EEWorld::GameSpeedChanged()
{
    gPhysics->UpdateTiming();
}

// EETeam's ctors: COMDATs here after their first users (each news the team's EEVCWonder)
// Creates an empty team with a wonder victory condition
EETeam::EETeam()
{
    this->mVictoryCondition = new EEVCWonder(this);
}


// Returns the unit with the id (its top byte is the player), 0 if none
EEUnit* EEWorld::GetUnit(unsigned long inUnitID)
{
    return static_cast<EEUnit*>(this->RTSWorld::GetUnit(inUnitID));
}

// ------------------------------------------------------------------------- the world bodies' C++ (gamecpp world.rs)

#define P64(x) ((int64_t)(intptr_t)(x))
#define W ((EEWorld*)p)
#define REC (W->mStatistics.mRecords[(size_t)a[0]])
#define PL0 (W->mPlayers[a[0]])
#define PL1 (W->mPlayers[a[1]])
#define ST ((EEGameState*)p)
#define PL ((EEPlayer*)p)
#define UN ((EEUnit*)p)
#define CX ((EEComplexUnit*)p)
#define UC ((EEUnitClass*)p)
#define FILE_SPEC (*(FSFileSpec*)p)

int64_t EEGameStateOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EEGameStateOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

// The world bodies' C++ calls and members (EeGameCpp's world section; EEGameOps.cpp's default case): one statement each
int64_t EEWorldOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    // the world's members (an address for each plain one)
    case O::WOptions: return P64(static_cast<EeAppOptionsFields*>(&W->mGameOptions));
    case O::WNumPlayers: return P64(&W->mNumPlayers);
    case O::WPlayer: return P64(W->mPlayers[a[0]]);
    case O::WCurrentPlayer: return P64(&W->mCurrentPlayer);
    case O::WCurrentPlayerPtr: return P64(W->GetCurrentPlayer());
    case O::WMediaLoaded: return P64(&W->mMediaLoaded);
    case O::WMap: return P64(W->mMap);
    case O::WClearing: return P64(&W->mClearing);
    case O::WWindYaw: return P64(&W->mWindYaw);
    case O::WUB80: return P64(&W->mUB80);
    case O::WUB30: return P64(&W->mUB30);
    case O::WUB58: return P64(&W->mUB58);
    case O::WSavedGameSpeed: return P64(&W->mSavedGameSpeed);
    case O::WWondersToWin: return P64(&W->mNumberOfWondersToWin);
    case O::WGameScreenActive: return P64(&W->mGameScreenActive);
    case O::WMaxPlayers: return P64(&W->mMaxPlayers);
    case O::WLoadingSavedGame: return P64(&W->mLoadingSavedGame);
    case O::WUnitImprovementsAllowed: return P64(&W->mUnitImprovementsAllowed);
    case O::WCheatsEnabled: return P64(&W->mCheatsEnabled);
    case O::WTeamsLocked: return P64(&W->mTeamsLocked);
    case O::WTownIDCounter: return P64(&W->mTownIDCounter);
    case O::WHerdIDCounter: return P64(&W->mHerdIDCounter);
    case O::WStartTime: return P64(&W->mStartTime);
    case O::WLagStatus: return P64(&W->mLagStatus);
    case O::WLoadingPlayerMedia: return P64(&W->mLoadingPlayerMedia);
    case O::WUserPaused: return P64(&W->mUserPaused);
    case O::WUsingCarbon: return P64(&W->mUsingCarbon);
    case O::WInCinematic: return P64(&W->mInCinematic);
    case O::WShowCollisionSpheres: return P64(&W->mShowCollisionSpheres);
    case O::WBB9D: return P64(&W->mBB9D);
    case O::WCollisionFlags: return P64(&W->mCollisionDetectionFlags);
    case O::WExecutingLocalSelection: return P64(&W->mExecutingLocalSelection);
    case O::WVictoryEnabled: return P64(&W->mVictoryEnabled);
    case O::WTimeOfDay: return P64(&W->mTimeOfDay);
    case O::WSunDirection: return P64(&W->mSunDirection);
    case O::WMoonDirection: return P64(&W->mMoonDirection);
    case O::WEffectManager: return P64(W->mEffectManager);
    case O::WEnvironment: return P64(W->mEnvironmentManager);
    case O::WInfluence: return P64(W->mInfluenceTable);
    case O::WFirstUnit: return P64(W->mUnits.mFirst);
    case O::WUnitsHead: return P64(reinterpret_cast<RTSUnit*>(&W->mUnits));
    case O::WCalamityList: return P64(static_cast<EEPCalamityHelper*>(W->mCalamityHelper)->mCalamities);
    case O::WCampaignNameLength: return W->mGameOptions.mCampaignName.GetLength();
    // the world's own setters and calls
    case O::WSetEffectManagerRaw: W->mEffectManager = (EEEffectManager*)a[0];
        break;
    case O::WSetEnvironmentRaw: W->mEnvironmentManager = (EEEnvironmentManager*)a[0];
        break;
    case O::WSetCalamityHelperRaw: W->mCalamityHelper = (FSPersistent*)a[0];
        break;
    case O::WSetInfluenceRaw: W->mInfluenceTable = (EEInfluenceTable*)a[0];
        break;
    case O::WSetScenarioRaw: W->mScenarioManager = (EEScenarioManager*)a[0];
        break;
    case O::WSetFirstUnitRaw: W->mUnits.mFirst = (RTSUnit*)a[0];
        break;
    case O::WDeleteEnvironment: delete W->mEnvironmentManager;
        break;
    case O::WDeleteScenario: delete W->mScenarioManager;
        break;
    case O::WDeleteCalamityHelper: delete W->mCalamityHelper;
        break;
    case O::WDeleteInfluence: delete W->mInfluenceTable;
        break;
    case O::WCreateGhostLock: TSScheduler::CreateCriticalSection(W->mGhostUnitsLock);
        break;
    case O::WStatisticsReset: W->mStatistics.Reset();
        break;
    case O::WGhostLock: W->mGhostUnitsLock->Enter();
        break;
    case O::WGhostUnlock: W->mGhostUnitsLock->Leave();
        break;
    case O::WGhostCount: return (int64_t)W->mGhostUnits.size();
    case O::C13bGhostLockFree: TSScheduler::DestroyCriticalSection(W->mGhostUnitsLock);   // the world's dtor (world.rs ee_gamecpp_world_destroy)
        break;
    case O::C13bGhostAt: return P64(W->mGhostUnits[(size_t)a[0]]);
    case O::WGhostBack: return P64(W->mGhostUnits.back());
    case O::WGhostPop: W->mGhostUnits.pop_back();
        break;
    case O::GhostDelete: delete (EEGhostUnit*)p;
        break;
    case O::WTeamsClear: W->mTeams.clear();
        break;
    case O::C12TeamsResize: W->mTeams.resize((size_t)a[0]);   // round 12, fork D (gamecpp teams.rs)
        break;
    case O::C12TeamSetPlayers: W->mTeams[(size_t)a[0]].mPlayers.assign((const uint32_t*)a[1], (const uint32_t*)a[1] + a[2]);
        break;
    case O::C12PlayerObserver: return ((EEPlayer*)p)->mObserverMode;
    case O::C12PlayerEnemy: return ((EEPlayer*)p)->mEnemies[a[0]] != 0;
    case O::WQueuedCount: return (int64_t)W->mQueuedPlacements.size();
    case O::WQueuedUnit: return P64(W->mQueuedPlacements[a[0]].mUnit);
    case O::WQueuedAfter: return P64(W->mQueuedPlacements[a[0]].mInsertAfter);
    case O::WQueuedClear: W->mQueuedPlacements.clear();
        break;
    case O::WSetPlayer: W->SetPlayer((long)a[0], (EEPlayer*)a[1]);
        break;
    case O::WWriteLock: W->mLock->WriteLock();
        break;
    case O::WWriteUnlock: W->mLock->WriteUnlock();
        break;
    case O::WCopyGameOptions: W->mGameOptions = gGameState.mGameOptions;
        break;
    case O::WSetMap: W->SetMap((EEMap*)a[0]);
        break;
    case O::WSetEffectManager: delete W->mEffectManager;   // the owning setter 0x13a219
        W->mEffectManager = (EEEffectManager*)a[0];
        break;
    case O::WCreateInfluenceTable: W->CreateInfluenceTable((EEMap*)a[0]);
        break;
    case O::WClearContinents: gContinentManager.Clear(W->mMap);
        break;
    case O::WCreateMiniMap: W->CreateMiniMap((EEGameState*)a[0]);
        break;
    case O::WSetGameSpeed: W->SetGameSpeed(f[0]);
        break;
    case O::WSetCalamityHelper: if ((FSPersistent*)a[0] != W->mCalamityHelper) {   // 0x13c1ad: the same object is kept
            delete W->mCalamityHelper;
            W->mCalamityHelper = (FSPersistent*)a[0];
        }
        break;
    case O::WOptionsPlayerColorFix: W->mGameOptions.mPlayers[a[0] - 1].mColor = EEColorTable::FindIndex(W->mPlayers[a[0]]->mColor);
        break;
    case O::WOptionsPlayerGet: *(EEGameOptionsPlayer*)a[1] = W->mGameOptions.mPlayers[a[0]];
        break;
    case O::WOptionsPlayerSet: W->mGameOptions.mPlayers[a[0]] = *(EEGameOptionsPlayer*)a[1];
        break;
    case O::WResourceUserHas: return W->mResourceUsers.count((EEUnit*)a[0]);
    case O::WResourceUserGet: return (int64_t)W->mResourceUsers.find((EEUnit*)a[0])->second;
    case O::WResourceUserSet: W->mResourceUsers[(EEUnit*)a[0]] = (unsigned long)a[1];
        break;
    case O::RtsClear: W->RTSWorld::Clear();
        break;
    case O::RtsReset: W->RTSWorld::Reset();
        break;
    case O::RtsSetPopulationLimit: W->RTSWorld::SetPopulationLimit((unsigned long)a[0]);
        break;
    case O::RtsSetNumPlayers: W->RTSWorld::SetNumPlayers((unsigned long)a[0]);
        break;
    case O::WorldLayoutRegister: EEWorldLayoutRegister();   // port: where Rust's views read the world's objects (ee/EEWorldLayout.h)
        break;
    // the game state
    case O::StOptions: return P64(static_cast<EeAppOptionsFields*>(&ST->mGameOptions));
    case O::LobbyOptions: return P64(static_cast<EeAppOptionsFields*>(&gGameOptions));
    case O::LobbyCampaignNameLength: return gGameOptions.mCampaignName.GetLength();
    case O::LobbyCampaignMapName: return gGameOptions.GetCampaignMapName(*(UWideString*)p);
    case O::StIsLoadedGame: return P64(&ST->mIsLoadedGame);
    case O::StInScenarioEditor: return P64(&ST->mInScenarioEditor);
    case O::StRandomSeed: return P64(&ST->mRandomSeed);
    case O::StNumPlayersMade: return P64(&ST->mNumPlayersMade);
    case O::StTownIDCounter: return P64(&ST->mTownIDCounter);
    case O::StHerdIDCounter: return P64(&ST->mHerdIDCounter);
    case O::StLocalPlayer: return P64(&ST->mLocalPlayer);
    case O::StUsingCarbon: return P64(&ST->mUsingCarbon);
    case O::StTimeOfDay: return P64(&ST->mTimeOfDay);
    case O::StGameSpeed: return P64(&ST->mGameSpeed);
    case O::StUnitImprovementsAllowed: return P64(&ST->mUnitImprovementsAllowed);
    case O::StRevealMap: return P64(&ST->mRevealMap);
    case O::StMajorVersion: return P64(&ST->mMajorVersion);
    case O::StMinorVersion: return P64(&ST->mMinorVersion);
    case O::StMapCenter: return P64(&ST->mMapCenter);
    case O::StMap: return P64(ST->mMap);
    case O::StEffectManager: return P64(ST->mEffectManager);
    case O::StScenarioManager: return P64(ST->mScenarioManager);
    case O::StEnvironmentManager: return P64(ST->mEnvironmentManager);
    case O::StCalamityHelper: return P64(ST->mCalamityHelper);
    case O::StNumPlayers: return ST->GetNumPlayers();
    case O::StPlayerCount: return (int64_t)ST->mPlayers.size();
    case O::StPlayerAt: return P64(ST->mPlayers[a[0]]);
    case O::StLoadedPlayerCount: return (int64_t)ST->mLoadedPlayers.size();
    case O::StLoadedPlayerAt: return P64(ST->mLoadedPlayers[a[0]]);
    case O::StNumTeams: return ST->GetNumTeams();
    case O::StTeamToRust: ee_app_teams_set(ee_app_teams_get(SIZE_MAX, NULL, 0), std::vector<uint32_t>(ST->mTeams[a[0]].mPlayers.begin(), ST->mTeams[a[0]].mPlayers.end()).data(), ST->mTeams[a[0]].mPlayers.size());
        break;
    case O::StReadLock: ST->ReadLock();
        break;
    case O::StReadUnlock: ST->ReadUnlock();
        break;
    case O::StPlayerIsLocalName: return PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)ST->mPlayers[a[0]]->mName), reinterpret_cast<const unsigned char*>((const char*)(a[1] ? gWonPlayerName : gPlayerName))) == 0;
    case O::ReportGameLoaded: EEReportLoadFailure(UString("Game loaded."), false, &ST->mChecksum);
        break;
    // the players
    case O::PlFirstUnit: return P64(PL->mFirstUnit);
    case O::PlReleasePending: PL->ReleasePendingUnits();
        break;
    case O::PlAddPendingUnits: PL->AddPendingUnits(a[0] != 0, a[1] != 0);
        break;
    case O::PlNumPending: return PL->GetNumPendingUnits();
    case O::PlPendingAt: return P64(PL->GetPendingUnit((unsigned long)a[0]));
    case O::PlAlliesCount: return (int64_t)PL->mAllies.size();
    case O::PlAllyAt: return P64(PL->mAllies[a[0]]);
    case O::PlAlliesPush: PL->mAllies.push_back((EEPlayer*)a[0]);
        break;
    case O::PlSaveTo: PL->EEPlayer::Save((unsigned long)a[0], (unsigned long)a[1], *(FSMemoryFile*)a[2]);
        break;
    case O::PlLoadFrom: PL->EEPlayer::Load((unsigned long)a[0], (unsigned long)a[1], *(FSMemoryFile*)a[2]);
        break;
    case O::PlRepairPointers: PL->RepairPointers((unsigned long)a[0], (unsigned long)a[1]);
        break;
    case O::PlTechnologies: return P64(PL->mTechnologies);
    case O::PlTechTree: return P64(PL->GetTechTree());
    case O::PlUpgradeAt: return P64(PL->mUnitClassUpgrades[a[0]]);
    case O::PlUpgradeLoadMedia: PL->mUnitClassUpgrades[a[0]]->LoadMedia(PL, (long)a[1]);
        break;
    case O::PlUpgradeUnloadMedia: PL->mUnitClassUpgrades[a[0]]->UnloadMedia(PL);
        break;
    case O::PlLoadButtons: PL->LoadUnitClassButtons((long)a[0]);
        break;
    case O::PlUnloadClassMedia: PL->UnloadUnitClassMedia((unsigned long)a[0]);
        break;
    case O::PlUnloadClassButtons: PL->UnloadUnitClassButtons((unsigned long)a[0]);
        break;
    case O::PlSetHackClassNone: PL->SetHackUnitClass(kNoUnitClass);
        break;
    case O::PlSetStance: PL->SetDiplomaticStance((EEPlayer*)a[0], (long)a[1], (EEMap*)a[2]);
        break;
    case O::PlSetEndTimeNow: PL->mSavedGroups.mEndTime = gPhysics->mGameTime;
        break;
    case O::PlColorIsPlayerColor: return EEGetPlayerColor((unsigned long)a[0]) == PL->mColor;
    case O::PlSetPlayerColor: PL->SetColor(EEGetPlayerColor((unsigned long)a[0]));
        break;
    case O::PlCivApplyDeferred: PL->mCivilization.ApplyDeferred(PL, ((EEGameState*)a[0])->mMajorVersion, ((EEGameState*)a[0])->mMinorVersion);
        break;
    case O::PlCivApplyLevel21: PL->mCivilization.ApplyLevel21ToWorld();
        break;
    case O::PlCivSetName: PL->mCivilization.SetName(*(UWideString*)a[0]);
        break;
    case O::PlComputerPlayer: return P64(PL->mComputerPlayer);
    case O::PlRefreshTownGroups: PL->RefreshTownGroups();
        break;
    case O::PlAddOwnSight: PL->AddOwnSight((EEMapTile*)a[0], (long)a[1], (long)a[2]);
        break;
    case O::PlRemoveOwnSight: PL->RemoveOwnSight((EEMapTile*)a[0]);
        break;
    case O::PlRestartTowns: PL->RestartTowns();
        break;
    case O::PlIsHuman: return PL->IsHuman();
    case O::HumanGetName: *(UWideString*)a[0] = static_cast<EEHumanPlayer*>(PL)->GetName();
        break;
    case O::HumanSetName: static_cast<EEHumanPlayer*>(PL)->SetName(*(UWideString*)a[0]);
        break;
    case O::PlDelete: delete PL;
        break;
    case O::NewHumanPlayer: return P64(static_cast<EEPlayer*>(new EEHumanPlayer));
    case O::NewComputerPlayer: return P64(static_cast<EEPlayer*>(new EEComputerPlayer(*(UWideString*)p, (unsigned long)a[0])));
    case O::CpRefreshResourceCenters: ((EEComputerPlayer*)p)->RefreshResourceCenters();
        break;
    case O::CpReplaceOnLoad: return ((EEComputerPlayer*)p)->mReplaceOnLoad;
    case O::SetDeferComputerRestore: gDeferComputerPlayerRestore = a[0] != 0;
        break;
    case O::SaveHasTowns: return EESaveHasTowns((unsigned long)a[0], (unsigned long)a[1]);
    case O::PlayerShareBonus: EEPlayerShareBonus(a[0] != 0);
        break;
    case O::TechSetPlayer: ((EETechTree*)p)->SetPlayer((EEPlayer*)a[0]);
        break;
    case O::TechIsLoaded: return ((EETechTree*)p)->mIsLoaded;
    case O::TechResetEpochRange: ((EETechTree*)p)->ResetEpochRange((long)a[0], (long)a[1]);
        break;
    case O::TechApplySaved: ((EETechTree*)p)->ApplySavedState(gLoadingSavedGame, true);
        break;
    case O::TechUpdateInterface: ((EETechTree*)p)->UpdateInterface(a[0] != 0);
        break;
    case O::UpgradeUnitType: return ((EEUnitClassUpgrade*)p)->GetUnitClass()->mUnitType;
    case O::UpgradeHitPoints: EEUpgradeHitPoints((EEUnitClassUpgrade*)p, (long)a[0], f[0], (EEPlayer*)a[1]);
        break;
    case O::OptionsPlayerNew: return P64(new EEGameOptionsPlayer);
    case O::OptionsPlayerDelete: delete (EEGameOptionsPlayer*)p;
        break;
    case O::OptionsPlayerSetType: ((EEGameOptionsPlayer*)p)->mType = (unsigned long)a[0];
        break;
    // the units
    case O::UnNext: return P64(static_cast<EEUnit*>(UN->mNext));
    case O::UnTileNext: return P64(static_cast<EEUnit*>(UN->mTileNode->mNext));
    case O::UnTilePrev: return P64(static_cast<EEUnit*>(UN->mTileNode->mPrev));
    case O::UnTile: return P64(UN->mTile);
    case O::UnIsFoundation: return UN->IsFoundation();
    case O::UnLoadMedia: UN->LoadMedia(a[0] != 0);
        break;
    case O::UnSightOnHere: UN->SightOnHere();
        break;
    case O::UnSightOffHere: UN->SightOffHere();
        break;
    case O::UnLink: ((RTSUnit*)p)->Link((RTSUnit*)a[0]);
        break;
    case O::UnUnlink: ((RTSUnit*)p)->Unlink();
        break;
    case O::RtsUpdateTile: ((RTSUnit*)p)->UpdateTile((EEMap*)a[0]);
        break;
    case O::DummyHead: return P64(RTSDummyHead((RTSUnit*)p));
    case O::CxClearCollisionState: CX->ClearCollisionState();
        break;
    case O::CxMakeAreaEffects: CX->MakeAreaEffects();
        break;
    case O::CxClearContacts: CX->ClearContacts();
        break;
    case O::CxFindContacts: CX->FindContacts();
        break;
    case O::CxPreviousPart: return P64(CX->mPreviousPart);
    case O::CxContainer: return P64(CX->mContainer);
    case O::RefUnitReference: UN->Reference();
        break;
    case O::RefUnitDereference: ((RTSUnit*)p)->Dereference();
        break;
    case O::UcAddToTile: UC->AddToTile((EEUnit*)a[0]);
        break;
    case O::UcAddToRegion: UC->AddToRegion((EEUnit*)a[0]);
        break;
    case O::UnitClassCount: return EEUnitClassCount();
    case O::LogSkipRemoved: fprintf(stderr, "EE-UNITLIST: placement skips a removed unit (id %08lx, %s, tile %ld,%ld)\n", UN->mUnitID, (const char*)UN->mUnitClass->mName, UN->mTileX, UN->mTileY);
        break;
    case O::LogAfterGone: fprintf(stderr, "EE-UNITLIST: placement after a unit no longer in the list (id %08lx), from the head\n", ((RTSUnit*)p)->mUnitID);
        break;
    // the map, the managers and the globals
    case O::MapSize: return ((EEMap*)p)->mSize;
    case O::MapPlayerCount: return ((EEMap*)p)->mU19561C;
    case O::MapFogSeen: ((EEMap*)p)->FogSeen((EEPlayer*)a[0]);
        break;
    case O::MeshReferenceTile: gWorld.mMap->mTerrainMesh.ReferenceTile((long)a[0], (long)a[1]);
        break;
    case O::MeshDereferenceTile: gWorld.mMap->mTerrainMesh.DereferenceTile((long)a[0], (long)a[1]);
        break;
    case O::SightRevealScopeNew: return P64(new EESightScope(kSightRevealMap, NULL));   // TSGT (TEST ONLY)
    case O::SightScopeDelete: delete (EESightScope*)p;
        break;
    case O::InfluenceAddUnit: ((EEInfluenceTable*)p)->AddUnit((EEUnit*)a[0], (long)a[1], (long)a[2]);
        break;
    case O::MiniMapDelete: delete gMiniMap;
        break;
    case O::MiniMapSetNull: gMiniMap = NULL;
        break;
    case O::MiniMapRedraw: gMiniMap->Redraw();
        break;
    case O::MiniMapRedrawAll: gMiniMap->RedrawAll();
        break;
    case O::MiniMapRefresh: gMiniMap->Refresh();
        break;
    case O::MiniMapSetViewer: gMiniMap->mViewingPlayer = (EEPlayer*)a[0];
        break;
    case O::MiniMapMoveUnit: gMiniMap->MoveUnit(UN, (long)a[0], (long)a[1], (long)a[2], (long)a[3]);
        break;
    case O::PickerLock: gUnitPicker.GetLock()->Enter();
        break;
    case O::PickerReset: gUnitPicker.Reset();
        break;
    case O::PickerUnlock: gUnitPicker.GetLock()->Leave();
        break;
    case O::UiManagerPresent: return gUIManager != NULL;
    case O::UiEditorForm: return P64(gUIManager->GetScenarioEditorForm());
    case O::UiEditorClearSelection: gUIManager->GetScenarioEditorForm()->ClearSelection();
        break;
    case O::UiInScenarioEditor: return gUIManager->mInScenarioEditor;
    case O::UserInterfacePresent: return gUserInterface != NULL;
    case O::UiPostUpdateBackground: gUserInterface->PostUpdateBackgroundMedia();
        break;
    case O::UiPostPauseInformation: gUserInterface->PostPauseInformation(a[0] != 0);
        break;
    case O::UiSetRandomSeed: gUserInterface->mRandomSeed = (long)a[0];
        break;
    case O::DisplayWeatherEffects: return gDisplayWeatherEffects;
    case O::EnvSetEnabled: ((EEEnvironmentManager*)p)->SetEnabled(a[0] != 0);
        break;
    case O::EnvFnE123D: ((EEEnvironmentManager*)p)->FnE123D();
        break;
    case O::PathFreePaths: gPathManager.FreePaths();
        break;
    case O::PathReset: gPathManager.Reset();
        break;
    case O::PathRequestPending: gPathManager.RequestPendingPaths((EEMap*)a[0]);
        break;
    case O::EffClearEffects: ((EEEffectManager*)p)->ClearEffects();
        break;
    case O::AeClearEffects: gAEManager.ClearEffects();
        break;
    case O::CalamitiesDestroy: USingleton<EECalamityManager>::GetInstance()->DestroyCalamities();
        break;
    case O::CalamitiesRestore: USingleton<EECalamityManager>::GetInstance()->Restore(((EEGameState*)a[0])->mMajorVersion, ((EEGameState*)a[0])->mMinorVersion, *(std::vector<EECalamity*>*)p);
        break;
    case O::NewCalamityHelper: return P64(static_cast<FSPersistent*>(new EEPCalamityHelper));
    case O::SetLocalPlayerOut: gLocalPlayerOut = a[0] != 0;
        break;
    case O::SetSightUpdates: gSightUpdatesEnabled = a[0] != 0;
        break;
    case O::SetHerdUnreadValue: gHerdUnreadValue = (long)a[0];
        break;
    case O::SetCameraFollowing: gCameraFollowing = a[0] != 0;
        break;
    case O::SoundManagerPresent: return gSoundManager != NULL;
    case O::SoundSetMuted: gSoundManager->SetIsMuted(a[0] != 0);
        break;
    case O::WonNetPresent: return gWonNet != NULL;
    case O::WonNetClearGameList: gWonNet->ClearGameList();
        break;
    case O::CivBuilderFree: CivBuilderFreeCivilization();   // the edited civilization deleted (ee_screens civ_whole.rs)
        break;
    case O::SimNoHumanDefeat: return gPlatSimNoHumanDefeat;
    case O::SimNoHumanDefeatHit: gPlatSimNoHumanDefeatHits++;
        break;
    case O::ScenarioEndCinematic: gWorld.mScenarioManager->EndCinematic();
        break;
    case O::ScenarioPrepare: gWorld.mScenarioManager->Prepare();
        break;
    case O::ScenarioApplyPendingEffects: gWorld.mScenarioManager->ApplyPendingEffects();
        break;
    case O::CommNumHumanPlayers: return gCommunications->GetNumHumanPlayers();
    case O::CommSetPlayerNumber: gCommunications->SetPlayerNumber((unsigned long)a[0]);
        break;
    case O::CommIsServer: return gCommunications->IsServer();
    case O::CommCTsProcessed: return gCommunications->GetNumberOfCTsProcessedInPhysics();
    case O::CommIsMultiplayer: return gCommunications->GetIsMultiplayerSession();
    case O::CommServer: return P64(dynamic_cast<EEServer*>(gCommunications));
    case O::ServerIsPaused: return ((EEServer*)p)->GetIsGamePaused();
    case O::ServerPause: ((EEServer*)p)->PauseGame();
        break;
    case O::ServerUnpause: ((EEServer*)p)->UnpauseGame();
        break;
    case O::ServerFastSelect: (static_cast<EEServer*>(gCommunications))->mFastSelectionID = static_cast<EEServer*>(gCommunications)->mFastSelection[(unsigned char)a[0]];
        break;
    case O::ClientFastSelect: (static_cast<EEClient*>(gCommunications))->mFastSelectionID = static_cast<EEClient*>(gCommunications)->mFastSelection[(unsigned char)a[0]];
        break;
    case O::ContinentRestoreRegions: EERestoreContinentRegions();
        break;
    case O::ContinentBuildRegions: gContinentManager.BuildRegions((EEMap*)a[0]);
        break;
    case O::UpdateLandContinents: EEUpdateLandContinents();
        break;
    case O::AiCollectComputerPlayers: gArtificialIntelligence->CollectComputerPlayers();
        break;
    case O::CameraClearUnitA: gWorld.mCameraControl.ClearUnitA();
        break;
    case O::CameraClearUnitB: gWorld.mCameraControl.ClearUnitB();
        break;
    case O::CameraResetPath: gWorld.mCameraControl.ResetPath();
        break;
    case O::CameraResetZoom: gWorld.mCameraControl.ResetZoom();
        break;
    case O::CameraSetChanged: gWorld.mCameraControl.SetChanged();
        break;
    case O::GraphicsReleaseScene: gGraphics->PostReleaseScene();
        break;
    case O::GraphicsInitialiseScene: gGraphics->PostInitialiseScene();
        break;
    case O::MusicSetIntensity: gMusicManager->SetIntensity((long)a[0]);
        break;
    case O::MusicStart: gMusicManager->Start();
        break;
    case O::AmbientEnable: gAmbientSoundManager.SetEnabled(gAmbientSoundEnabled);
        break;
    case O::AmbientRestart: gAmbientSoundManager.Restart();
        break;
    case O::PhysSetGameStarted: gPhysics->mGameStarted = true;
        break;
    case O::PhysClearAutoSave: gPhysics->mAutoSaveClock = 0;
        break;
    case O::CampaignProgressNew: return P64(new EECampaignProgress);
    case O::CampaignProgressDelete: delete (EECampaignProgress*)p;
        break;
    case O::CampaignRestoreCivilization: ((EECampaignProgress*)p)->RestoreCivilization(UWideString(gDefaultCampaignUser), ((EEGameState*)a[0])->mGameOptions.mCampaignName, ((EEGameState*)a[0])->mGameOptions.mScenarioName, ((EEGameState*)a[0])->mGameOptions.mCampaignChecksum);
        break;
    case O::ClearFormManager: ee_screens_chat_log_clear();
        break;
    case O::StrNew: return P64(new UWideString);
    case O::StrFree: delete (UWideString*)p;
        break;
    case O::StrComputerName: gResource->GetString(IDS_COMPUTER, *(UWideString*)p);
        break;
    case O::MemFileNew: return P64(new FSMemoryFile((unsigned long)a[0], a[1] != 0, (long)a[2]));
    case O::MemFileRewind: ((FSMemoryFile*)p)->SetPosition(0, kFileOriginBegin, false);
        break;
    case O::MemFileDelete: delete (FSMemoryFile*)p;
        break;
    case O::GfxEffectLoad: EEGraphicEffect::Get((long)a[0])->Load(a[1] != 0);
        break;
    case O::GfxEffectUnload: EEGraphicEffect::Get((long)a[0])->Unload();
        break;
    case O::SoundObjectLoad: EESoundObject::Get((long)a[0])->LoadMedia();
        break;
    case O::SoundObjectUnload: EESoundObject::Get((long)a[0])->UnloadMedia();
        break;
    // DumpMedia's report (p the file)
    case O::DumpBlankLines: fprintf(FILE_SPEC, "\n\n");
        break;
    case O::DumpEffectIds: EEGraphicEffect::DumpIDs(FILE_SPEC);
        break;
    case O::DumpGraphicObjects: EEDumpGraphicObjectTable(FILE_SPEC);
        break;
    case O::DumpSounds: EESoundObject::Dump(FILE_SPEC);
        break;
    case O::DumpButtons: EEButtonObject::DumpAllLoadedMedia(FILE_SPEC);
        break;
    case O::DumpTerrain: EETerrainTable::DumpAllLoadedMedia(FILE_SPEC);
        break;
    case O::DumpTextures: gTextureManager->DumpAllLoadedMedia(FILE_SPEC);
        break;
    case O::DumpModels: gModelManager->DumpAllLoadedMedia(FILE_SPEC);
        break;
    case O::DumpLeakedUnits: EEDumpLeakedUnits(FILE_SPEC);
        break;
    case O::DumpLeakTitle: fprintf(FILE_SPEC, "REFERENCE COUNTED MEMORY LEAKS\n\n");   // the reference-leak report: its title (nothing tracks references)
        break;
    // RecordStatistics' C++ calls and members (gamecpp world_stats.rs; p the world)
    case O::WsStart: W->mStatistics.mStartTime = W->mStartTime;
        break;
    case O::WsTiles: W->mStatistics.mTileCount = W->mMap->GetTileCount();
        break;
    case O::WsVictory: W->mStatistics.mVictoryEnabled = W->mVictoryEnabled;
        break;
    case O::WsTeamsLocked: W->mStatistics.mTeamsLocked = W->mTeamsLocked;
        break;
    case O::WsScenarioGame: return W->mGameOptions.mIsScenarioGame;
    case O::WsCampaignOptions: return W->mGameOptions.mCampaignName.GetLength() > 0 && W->mGameOptions.mCampaignChecksum > 0;
    case O::WsSetScenario: W->mStatistics.mIsScenario = a[0] != 0;
        break;
    case O::WsSetCampaign: W->mStatistics.mIsCampaign = a[0] != 0;
        break;
    case O::WsEndTime: return W->mStatistics.mEndTime = gPhysics->mGameTime;
    case O::WsCampaignName: W->mStatistics.mCampaignName = W->mGameOptions.mCampaignName;
        break;
    case O::WsNumPlayers: return W->mNumPlayers;
    case O::WsPlayer: *(EeAppStatsPlayer*)a[2] = {PL0->GetPlayerID(), PL0->IsHuman(), PlatMbsCmp(reinterpret_cast<const unsigned char*>((const char*)PL0->mName), reinterpret_cast<const unsigned char*>((const char*)*(UWideString*)a[1])) == 0, PL0->mObserverMode, PL0->mDisconnected, PL0->mGameResult, PL0->mSavedGroups.mStats};
        break;
    case O::WsCurrent: return W->mCurrentPlayer;
    case O::WsBase: return EEStartingResources::sTable[W->mGameOptions.mResources]->mAmounts[a[0]];
    case O::WsLevel: return EEGetResourceLevel(W->mGameOptions.mResources)->mAmounts[a[0]];
    case O::WsStatsSet: gWorld.mPlayers[a[0]]->mSavedGroups.Set((long)a[1], (long)a[2]);
        break;
    case O::WsStatsCap: return gWorld.mPlayers[a[0]]->mResourceCaps[a[1]];
    case O::WsStatsSetCap: gWorld.mPlayers[a[0]]->mResourceCaps[a[1]] = (long)a[2];
        break;
    case O::WsRecordsAssign: W->mStatistics.mRecords.assign((size_t)a[0], EEStatisticsRecord());
        break;
    case O::WsRecScore: REC.mScore = (unsigned long)a[1];
        break;
    case O::WsRecEconomy: REC.mEconomyScore = (unsigned long)a[1];
        break;
    case O::WsRecEpoch: REC.mEpochScore = (unsigned long)a[1];
        break;
    case O::WsRecMilitary: REC.mMilitaryScore = (unsigned long)a[1];
        break;
    case O::WsRecTech: REC.mTechScore = (unsigned long)a[1];
        break;
    case O::WsRecColor: REC.mColor = PL1->mColor;
        break;
    case O::WsRecName: REC.mName = PL1->mName + UWideString((const char*)a[2]);
        break;
    case O::WsRecStatistics: REC.mStatistics = PL1->mSavedGroups;
        break;
    case O::WsRecSlot: REC.mSlot = PL1->mSlot;
        break;
    case O::WsRecResult: REC.mResult = PL1->mGameResult;
        break;
    case O::WsRecEndTime: REC.mEndTime = PL1->mSavedGroups.mEndTime != 0 ? PL1->mSavedGroups.mEndTime : W->mStatistics.mEndTime;
        break;
    case O::WsRecTeam: REC.mTeam = PL1->GetPlayerID() > 0 ? W->mGameOptions.mPlayers[PL1->GetPlayerID() - 1].mTeam : 0;
        break;
    case O::WsRecResource: REC.mResources[a[2]] = PL1->mResourceCaps[a[2]];
        break;
    case O::WsRecLocal: REC.mIsLocal = a[1] != 0;
        break;
    case O::WsRecHuman: REC.mIsHuman = a[1] != 0;
        break;
    case O::WsSetCurrentRecord: W->mStatistics.mCurrentRecordIndex = (long)a[0];
        break;
    case O::WsRecPlayTime: REC.mStatistics.Set(0x18, (long)a[1]);
        break;
    default: return EEGameStateOp(inOp, p, a, f);   // the game state bodies' (EEGameState.cpp)
    }
    return 0;
}

double EEWorldOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::VariantFactor: return EEGameVariant::GetGameVariant(W->mGameOptions.GetVariant())->GetFactor((long)a[0]);
    case O::ViewportHalfWidth: return gViewport.GetHalfWidth();
    case O::ViewportOrthoWidth: return gViewport.GetOrthographicRectangle().GetWidth();
    case O::ViewportXCenter: return gViewport.GetXCenter();
    case O::ViewportYCenter: return gViewport.GetYCenter();
    case O::WsRandom: return gPhysics->GetRandom(f[0], f[1]);
    default: return EEGameStateOpF(inOp, p, a, f);
    }
    return 0.0;
}
