// The game's global data, out of the game's main unit (Empire Earth.cpp): the storage of its 48 global objects
// (gWorld, gContinentManager, the tables, the managers, the strings; ee/EEGlobalStorage.h: Empire Earth.cpp
// constructs them in place, in their original order), the references the original's code made to members of them
// (gNumPlayers is gWorld.mNumPlayers, ...), the subsystem and font pointers, the network checksums and the save
// version. No function and no constructor is named here, so the unit depends on no other: 197 units named
// Empire Earth.cpp only for these globals, and that put them all in one dependency cycle (tools/cpp_deps.py).
#include <windows.h>
#include <stdint.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEViewport.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEEvent.h"
#include "ee/EEDatabase.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEAIUnitTargetingTable.h"
#include "ee/EETerrainColorTable.h"
#include "ee/EEPathManager.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEGameState.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEAEManager.h"
#include "ee/EEAmbientSoundManager.h"
#include "ee/EECivilizationManager.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EECivPowersManager.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEGlobalStorage.h"

class GETextureManager;
class GEModelManager;
class RTSRenderObject;
class GEFont;
class EEUserInterface;
class EEMusicManager;
class RTSMousePointer;
class EEGraphics;
class EEDataPreLoading;
class EEPhysics;
class FSPath;
class GERasterizer;
class GEWindow;
class UResourceFile;

EE_GLOBAL_STORAGE(UString, gU530BE0, "?gU530BE0@@3VUString@@A");
EE_GLOBAL_STORAGE(UWideString, gU530BC8, "?gU530BC8@@3VUWideString@@A");
EE_GLOBAL_STORAGE(UString, gExecutableName, "?gExecutableName@@3VUString@@A");
EE_GLOBAL_STORAGE(UString, gSavedGameExtension, "?gSavedGameExtension@@3VUString@@A");
EE_GLOBAL_STORAGE(UString, gScenarioExtension, "?gScenarioExtension@@3VUString@@A");
EE_GLOBAL_STORAGE(UString, gU530B88, "?gU530B88@@3VUString@@A");
EE_GLOBAL_STORAGE(UString, gCampaignExtension, "?gCampaignExtension@@3VUString@@A");
EE_GLOBAL_STORAGE(GEViewport, gViewport, "?gViewport@@3VGEViewport@@A");
EE_GLOBAL_STORAGE(EEGamePreferences, gGamePreferences, "?gGamePreferences@@3VEEGamePreferences@@A");
// data the original placed inside gGamePreferences (it addressed them as globals): references to the members
EEGameOptions& gLobbyGameOptions = gGamePreferences.mSettings.mGameOptions;   // +0x4
long& gDifficultyLevel2 = reinterpret_cast<long&>(gGamePreferences.mSettings.mGameOptions.mDifficulty);   // +0x3fc
UWideString& gPlayerName = gGamePreferences.mSettings.mPlayerName;   // +0x460
UWideString& gWonPlayerName = gGamePreferences.mSettings.mSierraUserID;   // +0x474
UWideString& gHostedGameName = gGamePreferences.mSettings.mSierraGameName;   // +0x488
UString& gLoadSavedGameName = gGamePreferences.mSettings.mLastGameFile;   // +0x4ec
UString& gLoadScenarioName = gGamePreferences.mSettings.mLastScenarioFile;   // +0x4fc
float& gSideScrollSpeed = gGamePreferences.mSettings.mSideScrollSpeed;   // +0x520
float& gMasterVolume = gGamePreferences.mSettings.mSoundVolume;   // +0x528
float& gZoomLock = gGamePreferences.mSettings.mZoomLock;   // +0x538
long& gDifficultyLevel = gGamePreferences.mSettings.mComputerPlayerDifficultyLevel;   // +0x568
unsigned long& gGameWindowHeight = reinterpret_cast<unsigned long&>(gGamePreferences.mSettings.mGameWindowHeight);   // +0x570
unsigned long& gGameWindowWidth = reinterpret_cast<unsigned long&>(gGamePreferences.mSettings.mGameWindowWidth);   // +0x574
unsigned long& gTextureMipLevelsToDrop = reinterpret_cast<unsigned long&>(gGamePreferences.mSettings.mMIPLevelsToDrop);   // +0x58c
long& gModelDetail = gGamePreferences.mSettings.mModelDetail;   // +0x590
unsigned long& gEffectDetail = reinterpret_cast<unsigned long&>(gGamePreferences.mSettings.mEffectDetail);   // +0x594
unsigned long& gNetworkAdapter = reinterpret_cast<unsigned long&>(gGamePreferences.mSettings.mNetworkAdapter);   // +0x598
unsigned long& gScreenIndex = reinterpret_cast<unsigned long&>(gGamePreferences.mSettings.mScreenID);   // +0x5a0
unsigned long& gTerrainMIPLevelsToDrop = reinterpret_cast<unsigned long&>(gGamePreferences.mSettings.mTerrainMIPLevelsToDrop);   // +0x5a8
unsigned long& gTexturePixelDepth = reinterpret_cast<unsigned long&>(gGamePreferences.mSettings.mTextureBitDepth);   // +0x5ac
long& gAutoSaveCount = reinterpret_cast<long&>(gGamePreferences.mSettings.mNumberOfAutoSaves);   // +0x5b0
long& gHiddenUnitsMode = gGamePreferences.mSettings.mL5BC;   // +0x5c0
bool& gZoomEnabled = gGamePreferences.mSettings.mZoomEnabled;   // +0x5c5
bool& gInterpolateFrames = gGamePreferences.mSettings.mAnimationSmoothing;   // +0x5c7
bool& gFarRearClipping = gGamePreferences.mSettings.mB5C6;   // +0x5ca
bool& gDrawClouds = gGamePreferences.mSettings.mDisplayClouds;   // +0x5cb
bool& gDrawGroundShadows = gGamePreferences.mSettings.mDisplayShadows;   // +0x5cc
bool& gAmbientEffectsEnabled = gGamePreferences.mSettings.mDisplayBattlefieldAmbients;   // +0x5cd
bool& gTrilinearFilter = gGamePreferences.mSettings.mTrilinearFilter;   // +0x5d2
bool& gAmbientSoundEnabled = gGamePreferences.mSettings.mAmbientSounds;   // +0x5d6
bool& gDrawBlood = gGamePreferences.mSettings.mShowBlood;   // +0x5d7
bool& gMusicFollowsBattle = gGamePreferences.mSettings.mMusicEnabled;   // +0x5d8
bool& gOrthographicView = gGamePreferences.mSettings.mB5D6;   // +0x5da
bool& gUnitLighting = gGamePreferences.mSettings.mUnitLighting;   // +0x5db
bool& gDisplayWeatherEffects = gGamePreferences.mSettings.mDisplayWeatherEffects;   // +0x5de
bool& gTakeJPGScreenshots = gGamePreferences.mSettings.mTakeJPGScreenshots;   // +0x5e0
bool& gAntiAliasing = gGamePreferences.mSettings.mAntiAliasing;   // +0x5e1
EE_GLOBAL_STORAGE(EEWorld, gWorld, "?gWorld@@3VEEWorld@@A");
// data the original placed inside gWorld (it addressed them as globals): references to the members
long& gNumPlayers = reinterpret_cast<long&>(gWorld.mNumPlayers);   // +0xcc
long& gGameResources = reinterpret_cast<long&>(gWorld.mGameOptions.mResources);   // +0x4fc
bool& gAllowCustomCivs = gWorld.mGameOptions.mAllowCustomCivs;   // +0x516
bool& gIsScenarioGame = gWorld.mGameOptions.mIsScenarioGame;   // +0x519
EEGameOptions& gGameOptions = gWorld.mLobbyOptions;   // +0x51c
float& gCameraZoom = gWorld.mCameraControl.mZoom;   // +0xab0
EEEffectManager*& gEffectManager = gWorld.mEffectManager;   // +0xb1c
float& gSavedGameSpeed = gWorld.mSavedGameSpeed;   // +0xb70
unsigned long& gMaxPlayers = reinterpret_cast<unsigned long&>(gWorld.mMaxPlayers);   // +0xb8c
unsigned long& gStartTime = reinterpret_cast<unsigned long&>(gWorld.mStartTime);   // +0xb90
unsigned short& gLagStatus = gWorld.mLagStatus;   // +0xb94
bool& gGameScreenActive = gWorld.mGameScreenActive;   // +0xb97
bool& gLoadingSavedGame = gWorld.mLoadingSavedGame;   // +0xb98
bool& gWorldClearing = gWorld.mClearing;   // +0xb99
bool& gUnitImprovementsAllowed = gWorld.mUnitImprovementsAllowed;   // +0xb9b
bool& gShowCollisionSpheres = gWorld.mShowCollisionSpheres;   // +0xb9c
bool& gInCinematic = gWorld.mInCinematic;   // +0xba0
bool& gLoadingPlayerMedia = gWorld.mLoadingPlayerMedia;   // +0xba3
bool& gUsingCarbon = gWorld.mUsingCarbon;   // +0xba4
EE_GLOBAL_STORAGE(EEEvent, gEvent, "?gEvent@@3VEEEvent@@A");
EE_GLOBAL_STORAGE(EEDatabase, gDatabaseObjects, "?gDatabaseObjects@@3VEEDatabase@@A");
EE_GLOBAL_STORAGE(EETechTree, gTechTree, "?gTechTree@@3VEETechTree@@A");
EE_GLOBAL_STORAGE(EEToHitTable, gToHitTable, "?gToHitTable@@3VEEToHitTable@@A");

EE_GLOBAL_STORAGE(EEAIUnitTargetingTable, gAIUnitTargetingTable, "?gAIUnitTargetingTable@@3VEEAIUnitTargetingTable@@A");
EE_GLOBAL_STORAGE(EEPathManager, gPathManager, "?gPathManager@@3VEEPathManager@@A");
// data the original placed inside gPathManager (it addressed them as globals): references to the members
long& gEEAIRegionSize = gPathManager.mLC8;   // +0xc8
EE_GLOBAL_STORAGE(EEAIContinentManager, gContinentManager, "?gContinentManager@@3VEEAIContinentManager@@A");
// data the original placed inside gContinentManager (it addressed them as globals): references to the members
unsigned long& gContinentManagerChecksum = reinterpret_cast<unsigned long&>(gContinentManager.mRecord.checksum);   // +0x120
long& gDCGameSpeed = gContinentManager.mRecord.num_water_tiles;   // +0x134
long& gNumSpaceTiles = gContinentManager.mRecord.num_space_tiles;   // +0x13c
unsigned long& gMapWidth = reinterpret_cast<unsigned long&>(gContinentManager.mRecord.width);   // +0x144
unsigned long& gMapHeight = reinterpret_cast<unsigned long&>(gContinentManager.mRecord.height);   // +0x148
bool& gContinentManagerClearing = gContinentManager.mRecord.clearing;   // +0x14c
EE_GLOBAL_STORAGE(EEWorldTable, gWorldTable, "?gWorldTable@@3VEEWorldTable@@A");
EE_GLOBAL_STORAGE(EETerrainTable, gTerrainTable, "?gTerrainTable@@3VEETerrainTable@@A");
EE_GLOBAL_STORAGE(EETerrainColorTable, gTerrainColorTable, "?gTerrainColorTable@@3VEETerrainColorTable@@A");
EE_GLOBAL_STORAGE(EETerrainTypeTable, gTerrainTypeTable, "?gTerrainTypeTable@@3VEETerrainTypeTable@@A");
EE_GLOBAL_STORAGE(EEGameState, gGameState, "?gGameState@@3VEEGameState@@A");
// data the original placed inside gGameState (it addressed them as globals): references to the members
long& gGameStateLoadType = reinterpret_cast<long&>(gGameState.mGameOptions.mLoadType);   // +0x3e0
bool& gGameStateIsMultiplayer = gGameState.mGameOptions.mIsMultiplayer;   // +0x410
float& gGameStateGameSpeed = gGameState.mGameSpeed;   // +0x4dc
EE_GLOBAL_STORAGE(EEComputerPlayerBehaviorTable, gComputerPlayerBehaviorTable, "?gComputerPlayerBehaviorTable@@3VEEComputerPlayerBehaviorTable@@A");

EE_GLOBAL_STORAGE(EEUnitSetManager, gUnitSetManager, "?gUnitSetManager@@3VEEUnitSetManager@@A");
EE_GLOBAL_STORAGE(EEAEManager, gAEManager, "?gAEManager@@3VEEAEManager@@A");
EE_GLOBAL_STORAGE(EEAmbientSoundManager, gAmbientSoundManager, "?gAmbientSoundManager@@3VEEAmbientSoundManager@@A");
EE_GLOBAL_STORAGE(EECivilizationManager, gCivilizationManager, "?gCivilizationManager@@3VEECivilizationManager@@A");
EE_GLOBAL_STORAGE(EEDoppelgangerManager, gDoppelgangerManager, "?gDoppelgangerManager@@3VEEDoppelgangerManager@@A");
EE_GLOBAL_STORAGE(EECivPowersManager, gCivPowersManager, "?gCivPowersManager@@3VEECivPowersManager@@A");
EE_GLOBAL_STORAGE(UWideString, gCampaignAOCRoman, "?gCampaignAOCRoman@@3VUWideString@@A");
EE_GLOBAL_STORAGE(UWideString, gCampaignAOCPacific, "?gCampaignAOCPacific@@3VUWideString@@A");
EE_GLOBAL_STORAGE(UWideString, gCampaignAOCAsian, "?gCampaignAOCAsian@@3VUWideString@@A");
EE_GLOBAL_STORAGE(UWideString, gCampaignEEDemo, "?gCampaignEEDemo@@3VUWideString@@A");
EE_GLOBAL_STORAGE(UWideString, gCampaignEELearning, "?gCampaignEELearning@@3VUWideString@@A");
EE_GLOBAL_STORAGE(UWideString, gU52A280, "?gU52A280@@3VUWideString@@A");
EE_GLOBAL_STORAGE(UString, gPhysicsThreadName, "?gPhysicsThreadName@@3VUString@@A");
EE_GLOBAL_STORAGE(UString, gGraphicsThreadName, "?gGraphicsThreadName@@3VUString@@A");
EE_GLOBAL_STORAGE(UString, gDataPreLoadingThreadName, "?gDataPreLoadingThreadName@@3VUString@@A");
EE_GLOBAL_STORAGE(UString, gMousePointerThreadName, "?gMousePointerThreadName@@3VUString@@A");
EE_GLOBAL_STORAGE(UString, gScenarioEditorThreadName, "?gScenarioEditorThreadName@@3VUString@@A");
EE_GLOBAL_STORAGE(UString, gUserInterfaceThreadName, "?gUserInterfaceThreadName@@3VUString@@A");
EE_GLOBAL_STORAGE(UString, gCommSendThreadName, "?gCommSendThreadName@@3VUString@@A");
EE_GLOBAL_STORAGE(UString, gCommRecvThreadName, "?gCommRecvThreadName@@3VUString@@A");
EE_GLOBAL_STORAGE(UString, gCommMainThreadName, "?gCommMainThreadName@@3VUString@@A");
EE_GLOBAL_STORAGE(UString, gCommLagLossThreadName, "?gCommLagLossThreadName@@3VUString@@A");
EE_GLOBAL_STORAGE(UString, gCommRecPlayThreadName, "?gCommRecPlayThreadName@@3VUString@@A");
EE_GLOBAL_STORAGE(UString, gSoundScavengerThreadName, "?gSoundScavengerThreadName@@3VUString@@A");
EE_GLOBAL_STORAGE(UString, gCDAudioThreadName, "?gCDAudioThreadName@@3VUString@@A");

// the network settings EELoadNetworkSettings reads from the string resources (0x4c05d4.., .data, all 1)
long gAOCRomanChecksum = 1;
long gAOCPacificChecksum = 1;
long gAOCAsianChecksum = 1;
long gEEDemoChecksum = 1;
long gEELearningChecksum = 1;

uint32_t gBinaryChecksum; // 0x5319e4: the install checksum EEInitializeNetwork computes (sent to clients; 32-bit on the wire)

GETextureManager* gTextureManager;
GEModelManager* gModelManager;
RTSRenderObject* gU56535C;
RTSRenderObject* gU565358;
RTSRenderObject* gU565354;
RTSRenderObject* gU565350;
RTSRenderObject* gU565360;
GEFont* gFontArial8; // 0x531928 (UIFontTable::FindOrCreateFont)
GEFont* gFontArial9; // 0x53192c (UIFontTable::FindOrCreateFont)
GEFont* gU531930; // 0x531930 (UIFontTable::FindOrCreateFont)
GEFont* gU531934; // 0x531934 (UIFontTable::FindOrCreateFont)
GEFont* gU531938; // 0x531938 (UIFontTable::FindOrCreateFont)
GEFont* gFontArial16; // 0x53193c (UIFontTable::FindOrCreateFont)
GEFont* gFontArial18; // 0x531940 (UIFontTable::FindOrCreateFont)
GEFont* gFontArial20; // 0x531944 (UIFontTable::FindOrCreateFont)
GEFont* gFontArial24; // 0x531948 (UIFontTable::FindOrCreateFont)
GEFont* gFontArial36; // 0x53194c (UIFontTable::FindOrCreateFont)
GEFont* gFontArial48; // 0x531950 (UIFontTable::FindOrCreateFont)
GEFont* gFontArial72; // 0x531954 (UIFontTable::FindOrCreateFont)
GEFont* gFontArialBold8; // 0x531958 (UIFontTable::FindOrCreateFont)
GEFont* gFontArialBold9; // 0x53195c (UIFontTable::FindOrCreateFont)
GEFont* gFontArialBold10; // 0x531960 (UIFontTable::FindOrCreateFont)
GEFont* gFontArialBold12; // 0x531964 (UIFontTable::FindOrCreateFont)
GEFont* gFontArialBold14; // 0x531968 (UIFontTable::FindOrCreateFont)
GEFont* gFontArialBold16; // 0x53196c (UIFontTable::FindOrCreateFont)
GEFont* gFontTimesNewRoman9; // 0x531970 (UIFontTable::FindOrCreateFont)
GEFont* gFontTimesNewRoman10; // 0x531974 (UIFontTable::FindOrCreateFont)
GEFont* gFontTimesNewRoman12; // 0x531978 (UIFontTable::FindOrCreateFont)
GEFont* gFontTimesNewRoman14; // 0x53197c (UIFontTable::FindOrCreateFont)
GEFont* gFontCourier8; // 0x531980 (UIFontTable::FindOrCreateFont)
GEFont* gFontCourier10; // 0x531984 (UIFontTable::FindOrCreateFont)
GEFont* gFontCourier12; // 0x531988 (UIFontTable::FindOrCreateFont)
GEFont* gFontCourier14; // 0x53198c (UIFontTable::FindOrCreateFont)
GEFont* gFontTahoma8; // 0x531990 (UIFontTable::FindOrCreateFont)
GEFont* gU531994; // 0x531994 (UIFontTable::FindOrCreateFont)
GEFont* gFontTahoma10; // 0x531998 (UIFontTable::FindOrCreateFont)
GEFont* gFontTahoma12; // 0x53199c (UIFontTable::FindOrCreateFont)
GEFont* gFontTahoma14; // 0x5319a0 (UIFontTable::FindOrCreateFont)
GEFont* gFontTahoma16; // 0x5319a4 (UIFontTable::FindOrCreateFont)
GEFont* gFontTahoma18; // 0x5319a8 (UIFontTable::FindOrCreateFont)
GEFont* gFontTahoma20; // 0x5319ac (UIFontTable::FindOrCreateFont)
GEFont* gFontTahoma24; // 0x5319b0 (UIFontTable::FindOrCreateFont)
GEFont* gFontTahoma36; // 0x5319b4 (UIFontTable::FindOrCreateFont)
GEFont* gFontTahoma48; // 0x5319b8 (UIFontTable::FindOrCreateFont)
GEFont* gFontTahoma72; // 0x5319bc (UIFontTable::FindOrCreateFont)
// The subsystem pointers after the font table (.bss 0x5319c0..0x5319d4; their headers declare them)
class SEMixer;
class SESoundManager;
class NESynchronousCommunications;
class EEWonNet;
class EEArtificialIntelligence;
class EECheatCodes;
class EEUIManager;
class EEMouse;
SEMixer* gConductor;
SESoundManager* gSoundManager;
NESynchronousCommunications* gCommunications;
EEWonNet* gWonNet;
EEArtificialIntelligence* gArtificialIntelligence;
EECheatCodes* gCheatCodes;
EEUIManager* gUIManager;
EEMouse* gMouse;
bool gFreeCamera;                           // 0x5319e8 (EmpireEarth.h)

// the save-game version (.rdata 0x4420ac/0x4420b0, before this unit's vtables 0x4420c0; the original wrote 0/250;
// port: 251 adds the computer slots' engine, EEGameOptionsPlayer and EEPPlayerHelper, docs/AI_LUA_PLAN.md S0; 252 the
// next engine's state and its script's memory after each computer player, EEPPlayerHelper, docs/AI_LUA_PLAN.md §3.2;
// 253 the next engine's AI id after each computer player's engine, EEPPlayerHelper, and the lobby choices' new
// numbering in EEGameOptionsPlayer, ee/EECPEngine.h; 254 the running game's own values, ee/EESaveGameFiles.h
// EESaveHasLiveState, docs/RUST_PLAN.md R5a.2: 253 on the rust branch until it merged main's 253; 255 the UTF-8
// text (EESaveTextIsUtf8); 256 more of the running state (the AI's schedule, timers, targets and path requests, the
// path manager, the area effects, contacts and tile order, ee/EESaveGameFiles.h EESaveHasRunningState)
// 257 the units' order in each map region (EESaveHasRegionOrder)
// 258 the computer players' experiment timers (EESaveHasExperimentTimers)
// 259 their count of units in production (EESaveHasProductionCount)
// 260 the towns' ids, order and counters (EESaveHasTowns)
// 261 the computer players' gatherer lists as saved (EESaveHasGathererOrder)
// 262 the dead attackers the units still hold (EESaveHasGhosts)
// 263 the continent manager as it is, with the herds' and towns' serials (EESaveHasContinentRegions)
// 264 the attack sub groups' positions (EESaveHasSubGroupPosition)
// 265 the computer players' claim ranges and danger zones (EESaveHasResourceState)
// 266 the order of the units their unit managers track (EESaveHasUnitManagerOrder)
// 267 their repair and site timers (EESaveHasCivilizationTimers)
// 268 the towns' areas (EESaveHasTownAreas)
// 269 the computer players' manager timers (EESaveHasManagerTimers)
// 270 the order of each player's unit list (EESaveHasPlayerUnitOrder)
// 271 the computer players' intelligence files (EESaveHasIntelligence)
// 272 the ghosts' resources and the gather goals' dead resources (EESaveHasGhostResources)
// 273 the contagion manager's last helper id (EESaveHasContagionCounter)
// 274 the next engine's site walkers (EECPNextState::mExplore: its state block's version 4, EECPNextLua.cpp)
// 275 the attack goals' dying targets and the goals' units out of the world, simple ones too (EESaveHasGoalGhosts)
// 276 the units' tile searches in progress (EESaveHasPathSearches)
// 277 the wall manager's path request (EESaveHasWallRequest)
// 278 the graph searches in progress (EESaveHasGraphSearches)
// 279 the players' active groups (EESaveHasActiveGroups)
// 280 the per-unit Lua interface's state after the next engine's, EEPPlayerHelper, ee/EECPNextUnits.h (main's 254,
// EESaveHasUnitsLuaState)
// 281 the merge of main's stages 5 and 6: the computer players without the C++ managers' timers and intelligence files,
// main's next engine state block (EESaveHasLuaOnlyComputers)
// 282 the players' tech nodes that are not allowed (EESaveHasTechAllowed), the location goals' group path state
// 283 every map tile's terrain type (EESaveHasTileTypes)
const unsigned long kSaveGameMajorVersion = 0;
const unsigned long kSaveGameMinorVersion = 283;

EEUserInterface* gUserInterface; // 0x5318f8, created by EEInitializePart1

EEMusicManager* gMusicManager;
RTSMousePointer* gMousePointer;
EEGraphics* gGraphics;
EEDataPreLoading* gDataPreLoading;
EEPhysics* gPhysics;

// The two original zero-initialized search-path pointers (EESetDataPaths writes them, Empire Earth.cpp).
FSPath* gDataPath;
FSPath* gCivilizationDataPath;

GERasterizer* gRasterizer;
GEWindow* gGameWindow;

UResourceFile* gResource;

// TEST ONLY: the UI fixture's configure step (EETestFixture.cpp sets it at static initialization; EERun calls it first)
void (*gEETestFixtureConfigure)();
// TEST ONLY: the physics' hooks (were EEPhysics.cpp's; ee/EEPhysics.h): the unit being stepped (EE-SIM's random-draw
// trace), after each tick of RTSPhysics::Main's simulation loop, each executed input's view (the UI fixture's orders)
class EEComplexUnit;
class NEMessage;
EEComplexUnit* gPhysicsSteppingUnit = NULL;
void (*gPhysicsAfterTickHook)(void) = NULL;
void (*gEEMessageExecutedTestHook)(NEMessage* inMessage, int inPhase) = NULL;
// TEST ONLY: the render fill's capture (ee/EERenderCapture.h; EERenderCapture.cpp sets it)
struct EERenderFrameInfo;
void (*gEERenderCaptureFill)(const EERenderFrameInfo& inInfo, uint32_t inSwayClock, float inUnitCount) = NULL;

