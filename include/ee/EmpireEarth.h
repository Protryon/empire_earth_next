// C:\EEX\Empire Earth\Empire Earth\Empire Earth.h (RECONSTRUCTION; file name inferred). Game globals defined by
// Empire Earth.cpp (0x13f2ae..) and read by the UI events. Names are provisional
// unless an assert string gives them.
#ifndef EE_EMPIREEARTH_H
#define EE_EMPIREEARTH_H
#include "ee/EECivUpgrade.h"

#include <stdint.h>

class GE2DPoint;
class UResourceFile;
class EEUserInterface;

extern UResourceFile* gResource;            // 0x5318ec ("::gResource->GetString(...)" asserts; new'd at 0x140ed6)
extern EEUserInterface* gUserInterface;     // 0x5318f8 (new EEUserInterface, 0x60 bytes, at 0x141448)
// 0x5308cd: set by the game-settings form (0x1d812e); the hot-key zoom (0x1d9cc8) and the wheel zoom (0x1e9f46) do
// nothing while it is clear. Provisional name; probably a field of a game-options global
extern bool& gZoomEnabled;
// 0x4e917c: the cursor location the unit picker and the aircraft hot keys use (provisional name)
// 0x529067: cleared when the unit hot keys move the camera (24 users; provisional name)
extern bool gCameraFollowing;
// 0x5308e8 / 0x5308e9 (after gGamePreferences): EEGraphics saves screen shots as jpg while the first is set and
// redraws the whole UI every frame while the second is set (provisional names)
extern bool& gTakeJPGScreenshots;
extern bool& gAntiAliasing;
// 0x52fa88 is not a global: gGameState.mGameOptions.mIsMultiplayer (ee/EEGameState.h); the main screen sets it
bool GetIsLoadingScenario();                // 0x14a09e: returns the byte 0x531a40 (EEScenarioManager::Load drops the mPendingEffects effects while set; provisional name)
extern long& gGameStateLoadType;            // 0x52fa58 (= 2 when a local server starts a game: SPLE LoadFile, 0x12ccd0; provisional name)
// game functions the EEM* messages call (provisional names and argument types)
class EEComplexUnit;
class EEUnitClass;
template <class T> class U2DPoint; typedef U2DPoint<long> EETileCoordinate;
class GE3DPoint;
class EEPlayer;
// 0x244e95 (cdecl): inUnit replaced by a new unit of inClass for inPlayer (EEMPackUnpack; provisional name)
EEComplexUnit* EEReplaceUnit(EEComplexUnit* inUnit, unsigned long inPlayer, EEUnitClass* inClass);
// 0x24edf6 (cdecl): places inUnit of inClass for inPlayer at inTile (EEMPackUnpack; provisional name)
template <class T> class U2DPoint;
void EEPlaceUnit(EEComplexUnit* inUnit, EEUnitClass* inClass, EEPlayer* inPlayer, const U2DPoint<long>& inTile, bool inArg);
// 0x24f21d (cdecl): can a unit of inType stand at (inX, inY) (EEMPackUnpack's helper; provisional name)
bool EECanPlace(EEPlayer* inPlayer, long inType, GE3DPoint* inLocation, long inX, long inY, bool inSkipPlayerChecks, long inIgnoreOwner);
class EEPlayer;
class EESoundObject;
unsigned long EEPlayForLocalPlayer(EEPlayer* inPlayer, EESoundObject* inSound);  // 0x14831c (cdecl; returns -1 or Play's result; EEMSavedGroupSelect; plays inSound for the current player)
void EEAttackPlayers(long inPlayer, unsigned char inFlag);   // 0x13f5e9 (Empire Earth.cpp; EEMAttackPlayers)
extern unsigned long gCollisionDetectionUnit;   // 0x4c2378 (unsigned: EEPhysics 0x15401e's fidiv) (initialised .data; EEMToggleCollisionDetection sets it)
class UWideString;
// 0x280a05 (Civilization Builder; EEMCivUpgradesRequested): requires <vector>
void EECBRequestUpgrades(unsigned char inPlayer, UWideString& inName, std::vector<EECivUpgrade>& inUpgrades);

class UString;
// the saved-game / scenario file extensions and the files the lobby's load screens picked (provisional names; init
// chains 0x13ea01 ".ees" / 0x13ea33 ".scn"; the lobby reads and assigns the two names)
extern UString gSavedGameExtension;        // 0x530ba8 ".ees"
extern UString gScenarioExtension;         // 0x530b98 ".scn"
extern UString& gLoadSavedGameName;         // 0x5307f4
extern UString& gLoadScenarioName;          // 0x530804
extern long& gDifficultyLevel;              // 0x530870 (the lobby difficulty also sets it; provisional name)
// 0x5319e8: the free camera is on (the mouse revolve/rotate events need it; 10+ users around 0xeda02..0x109eee;
// provisional name)
extern bool gFreeCamera;
extern UWideString& gPlayerName;            // 0x530768 the single player's name (the SP screen's edit box 8; provisional name)
extern UWideString gGameVersion;           // 0x529a08 (the version a client sends with its connection request; provisional name)
extern uint32_t gBinaryChecksum;      // 0x5319e4 (likewise, the binary checksum; provisional name)
extern unsigned long& gNetworkAdapter;      // 0x5308a0 (passed to the local server's ctor by the SP random map; provisional name)
extern long& gDifficultyLevel2;             // 0x530704 (likewise; provisional name)
extern unsigned long& gStartTime;           // 0x5318d0 (EEClient 0xb80ef/EEServer 0xcc34e: NEMStartGame's offset start time; provisional name)

// 0x27a432 (User Interface Thread): the text of a resource reference through gResource->GetString, else a copy
// (provisional name and meaning; the campaign editor shows its fields through it)
void EETranslateString(const UWideString& inText, UWideString& outText);
void EEDisplayScenarioIntro();             // 0x27a470 (the briefing; provisional name)
void EEExitGame();                         // 0x27a93e (disconnects and leaves the game; provisional name)
class EEUnit;
// 0x27a737 (cdecl): the tile picked at inPoint (EEMap::PickScreenTile; true) or else the tile under it (false) and its
// coordinates (the out pointer is an EETile*, both stores come from EEMap tile lookups);
// EEACalamity::Cast/Check pass (location, &unit, mX, mY, true) (provisional names; types inferred)
class EETile;
// EEUnitGoal.cpp's goal counter 0x564480 (EEPlayer's Save and Load use it; not in ee/EEUnitGoal.h: EEPlayer.cpp does
// not see EEUnitGoal's statics)
extern unsigned long sGoalCounter;
long EESetCivBuilderButtonBlinking(bool inBlinking);  // 0x27fcf0 (cdecl; the main game form's blinking buttons; EEPlayer::RepairPointers keeps the result) (provisional name)
bool EEPointUnitOrTileXY(const GE2DPoint& inPoint, EETile*& outTile, long& outX, long& outY, bool inFlag);
// 0x27a77a (cdecl): the map location under inPoint (EEMap::GetLocationAt into outTile/outPoint; true when found),
// else the tile centre at its mesh height (false) (provisional name; types inferred; EEUICCommandUnits)
class GE3DPoint;
bool EEPointUnitOrTile(const GE2DPoint& inPoint, EETile*& outTile, GE3DPoint& outPoint, bool inFlag);
// 0x27a60c (cdecl): EEACalamity::Cast's feedback at inPoint for inUnit (provisional name)
void EEOrderAtCursor(const GE2DPoint& inPoint, EEUnit* inUnit);
// 0x27a644 (cdecl): the flare effect at inLocation (graphic effect 0xd1 on the ground, 0xd3 on inUnit) for the
// current player (EEOrderAtCursor, 0x2700f7; provisional name)
void EEMakeCalamityEffect(const GE3DPoint& inLocation, EEUnit* inUnit);
class EEComplexUnit;
// 0x27a701 (cdecl): the current player's first selected unit (false when nothing is selected) (provisional name)
bool EEGetFirstSelectedUnit(EEComplexUnit*& outUnit);
// 0x27a7f5 (cdecl): message 0x41b for the keyboard's modifier state in the game / scenario editor forms (0x27518a)
// (provisional name)
void EEPostF9Key();
class EEUIFMainGame;
extern EEUIFMainGame* gSavedMainGameForm;  // 0x563144 (LoadScenario stores gUIManager->mMainGameForm; provisional name)
extern UString gMultiplayerString;         // 0x562fd8 (defined by EEUIEMultiplayerScreenEvents.cpp, ""; provisional name)
bool EEIsMultiplayerGame(bool inFlag);     // 0x2989a9 (inFlag ? byte 0x531664 : true; provisional name)
void EEStartExternalLobby();               // 0x297276 (the main screen's MultiPlayer when the check passes; provisional name)
void EEShowSinglePlayerScreen();           // 0x13fe97 (the campaign choice's Back, after Pop; provisional name)
// the built-in campaigns' names (Empire Earth.cpp init chains 0x13eef8..; provisional names)
extern UWideString gCampaignAOCRoman;      // 0x52a2f8 "AOCRoman"
extern UWideString gCampaignAOCPacific;    // 0x52a2e0 "AOCPacific"
extern UWideString gCampaignAOCAsian;      // 0x52a2c8 "AOCAsian"
extern UWideString gCampaignEEDemo;        // 0x52a2b0 "EEDemo"
extern UWideString gCampaignEELearning;    // 0x52a298 "EELearningCampaign"
// the built-in campaigns' checksums (Empire Earth.cpp .data, all 1; EECampaignFormManager::GetBuiltInCampaign
// returns them; provisional names)
extern long gAOCRomanChecksum;
extern long gAOCPacificChecksum;
extern long gAOCAsianChecksum;
extern long gEEDemoChecksum;
extern long gEELearningChecksum;

// (provisional names, from a usage census): the game's rasterizer
// (115 functions; e.g. the game settings' InGameOk 0x1d6cf2: GERasterizer::GetScreen(gScreenIndex) on it) and the
// screen index passed with it (170 functions)
class GERasterizer;
extern GERasterizer* gRasterizer;          // 0x531910
extern unsigned long& gScreenIndex;         // 0x5308a8
// the managers Empire Earth.cpp's start-up news (0x14265e: asserts "gTextureManager != NULL", "gModelManager !=
// NULL")
class GETextureManager;
class GEModelManager;
extern GETextureManager* gTextureManager;  // 0x531918
extern GEModelManager* gModelManager;      // 0x531914

// 0x21080d (cdecl): EEMBuildGate::Execute's work (the tile of the gate's point, then the message's three longs)
// (provisional name and argument names)
template <class T> class U2DPoint; typedef U2DPoint<long> EETileCoordinate;
void EEBuildGate(EETileCoordinate& inTile, long inL20, long inL18, long inL1C);
class GERectangle;
// 0x140251 (cdecl): sets the game view rectangle (scaled to the game window unless the free camera is on); the game
// settings pass (0, 35, 1600, 1085) (provisional name)
void EESetGameViewRectangle(const GERectangle& inRectangle);
// (provisional names): what EEDataPreLoading::LoadGame (0xd439f) reads and calls
void EELoadGameMedia();                    // 0x13fcbc (six resets in a row; when the first argument is false)
void EECreateGameTimer();                  // 0x13fc7c (EEPhysics::CreateGameTimer; provisional name)
extern bool& gLoadingSavedGame;             // 0x5318d8
extern bool& gWorldClearing;                // 0x5318d9 (EETown 0xac573 tests it after gLoadingSavedGame; provisional name)
extern unsigned long& gEffectDetail;        // 0x53089c (compared with an effect's +0x84 before it plays: a detail level? EEUCProjectile::UpdateEffects; provisional name)
extern bool& gLoadingPlayerMedia;           // 0x5318e3 (EECPCombatManager 0x475047 tests it after gLoadingSavedGame; provisional name)
extern bool& gGameScreenActive;             // 0x5318d7 (24 users; the save/load events test it with gUIManager->mInScenarioEditor)
extern bool& gContinentManagerClearing;     // 0x530d3c (EECPPersonnel::DraftUnit: draft through the attack commander when clear) (provisional name)
// 0x13fcda (cdecl): loads a ".wav"/".mp3" sound file (SIE SoundOver: (name, 0x31, theSample, sSoundID, 0), then
// RegisterOneShotSound(theSample, sSoundID)) (provisional name and parameter names)
void EELoadSound(const UString& inName, unsigned long inType, unsigned long& outSample, unsigned long& outInstance,
                 unsigned long inFlag);
extern UString gDataPreLoadingThreadName;  // 0x52a250 "DataPreLd" (the thread's SetName)

// 0x529510: 9 or 10 selects which collision sphere EEMChangeSelectedUnitCollisionSphere grows/shrinks (a debug
// setting; provisional name)
extern long gCollisionSphereMode;
// 0x531908: the game's file search path (74 users; the texture, model and sound managers share it through
// 0x140d3c; the campaign form adds the campaign's archive to it). (provisional names)
class FSPath;
extern FSPath* gDataPath;
void EESetDataPaths();                     // 0x140d3c (SetTexturePath/SetModelPath/SetPath(gDataPath))
extern UWideString gU52A280;               // 0x52a280 "een1ve1s_" (prefixed to campaign scenario and media file names)
extern UWideString gU530BC8;               // 0x530bc8 (Empire Earth.cpp; EEUIESSEOpen appends it to label 0's text)

// 0x4e8708: a float table indexed [unit class +0x1c8][other unit class +0x1cc] (19 readers, e.g. the AI Hero unit;
// two stores). Name is provisional
extern float** gU4E8708;

// (provisional name for the window; the font names are Empire Earth.cpp's assert strings, e.g. "gFontTahoma12"): the
// game window (EEUIFError: GetWidth +0xc, GetHeight +0x10 scale its layout) and the Tahoma fonts
class GEWindow;
class GEFont;
extern GEWindow* gGameWindow;              // 0x531920
extern GEFont* gFontTahoma10;              // 0x531998 (EEE3DGoal's countdown; provisional name by the order)
extern GEFont* gFontTahoma12;              // 0x53199c
extern GEFont* gFontTahoma14;              // 0x5319a0
extern GEFont* gFontTahoma16;              // 0x5319a4
extern bool& gUnitImprovementsAllowed;      // 0x5318db (EEUIBUpgrade only acts when it is true; provisional name)

// 0x13f736 (cdecl; Empire Earth.cpp): a checksum of the file inFile in inDirectory (EECampaignEditorBehavior::Reset
// compares it with the archived scenario's byte sum; name and the third argument provisional name)
class FSFileSpec;
unsigned long EEGetFileChecksum(FSFileSpec& inDirectory, const UString& inFile, long inUseFileTimes);

// Actual existing global531940, original main definition and GamePage reads.
class GEFont;
extern GEFont* gFontArial18;

#endif
