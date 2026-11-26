// C:\EEX\Empire Earth\Empire Earth\Empire Earth.cpp (__FILE__ 0x13f2ae's asserts). The game's main unit:
// 0x13e967..0x14622a (its 49 global objects' init chains first, with the inline tables' deleting dtors between them,
// then the functions). Unit flags: the exe's module set. Global names are provisional unless another unit already
// bound them.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "EEDataText.h"   // EEDbFindFile: the db tables' text forms
#include "platform/PlatMessage.h"
#include "platform/PlatSim.h"      // gPlatSimMode (the POSIX main)
// TEST ONLY: the UI fixture (EETestFixture.cpp registers its configure step here, at static initialization: this unit
// names no symbol of the test drivers, so they stay out of its dependencies) and the headless simulation's driver
// (ee_game fixture and sim_driver, called directly)
void GXGameSize(unsigned long inPercent, bool inWindowed, unsigned long* outWidth, unsigned long* outHeight);   // gx/GX.h
#include <stdio.h>
#include <string.h>
#include <string>
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GERasterizer.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
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
#include "lle/UResourceFile.h"
#include "lle/TMTimer.h"
#include "lle/FSPersistentList.h"
#include "ee/EEMap.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEPChecksum.h"
#include "ee/EEPPlayerHelper.h"
#include "ee/EEPWorldHelper.h"
#include "ee/EEPMultiplayerHelper.h"
#include "ee/EEPCalamityHelper.h"
#include "ee/EEPersistentTemplates.h"
#include "lle/GEModelManager.h"
#include "lle/SESoundManager.h"
#include "ee/EEMusicManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEGraphics.h"
#include "ee/EECPNextUnits.h"

#include "ee/EEGlobalStorage.h"

class FSPath;
// The ones no header this unit includes declares:
extern UString gU530B88, gCampaignExtension, gPhysicsThreadName, gGraphicsThreadName, gMousePointerThreadName,
    gScenarioEditorThreadName;
extern EEEvent gEvent;
extern EEToHitTable gToHitTable;
extern EEAIUnitTargetingTable gAIUnitTargetingTable;
extern EETerrainTable gTerrainTable;
extern EETerrainColorTable gTerrainColorTable;
extern EETerrainTypeTable gTerrainTypeTable;
extern EEComputerPlayerBehaviorTable gComputerPlayerBehaviorTable;

// The game's globals (gWorld, gPhysics, gUIManager, the subsystem pointers, the save version, ...) are Rust's
// storage (ee_game gamecpp global_objects.rs and globals.rs; ee/EEGlobalStorage.h). The 38 global objects are
// constructed here, where their definitions were, in the order of the original's init chains (0x53e967..0x53f27c),
// each registering its destructor with atexit as its definition did.
// chain at 0x53e999 (wrapper)
EE_CONSTRUCT_GLOBAL(UWideString, gU530BC8, );
// chain at 0x53e9f7 (wrapper)
EE_CONSTRUCT_GLOBAL(UString, gSavedGameExtension, (".ees"));
// chain at 0x53ea29 (wrapper)
EE_CONSTRUCT_GLOBAL(UString, gScenarioExtension, (".scn"));
// chain at 0x53ea5b (wrapper)
EE_CONSTRUCT_GLOBAL(UString, gU530B88, (".ssa"));
// chain at 0x53ea8d (wrapper)
EE_CONSTRUCT_GLOBAL(UString, gCampaignExtension, (".ssa"));
// chain at 0x53eabf (wrapper)
EE_CONSTRUCT_GLOBAL(GEViewport, gViewport, );
// chain at 0x53eaeb (wrapper)
EE_CONSTRUCT_GLOBAL(EEGamePreferences, gGamePreferences, );
// chain at 0x53eb15 (wrapper)
EE_CONSTRUCT_GLOBAL(EEWorld, gWorld, );
// chain at 0x53eb3f (wrapper)
EE_CONSTRUCT_GLOBAL(EEEvent, gEvent, );
// chain at 0x53eb69 (wrapper)
EE_CONSTRUCT_GLOBAL(EEDatabase, gDatabaseObjects, );
// chain at 0x53eb93 (wrapper)
EE_CONSTRUCT_GLOBAL(EETechTree, gTechTree, );
// chain at 0x53ebbd (wrapper)
EE_CONSTRUCT_GLOBAL(EEToHitTable, gToHitTable, );
// chain at 0x53ec06 (wrapper)
EE_CONSTRUCT_GLOBAL(EEAIUnitTargetingTable, gAIUnitTargetingTable, );
// chain at 0x53ec32 (wrapper)
EE_CONSTRUCT_GLOBAL(EEPathManager, gPathManager, );
// chain at 0x53ec5c (wrapper)
EE_CONSTRUCT_GLOBAL(EEAIContinentManager, gContinentManager, );
// chain at 0x53ec86 (wrapper)
EE_CONSTRUCT_GLOBAL(EEWorldTable, gWorldTable, );
// chain at 0x53ecc6 (wrapper)
EE_CONSTRUCT_GLOBAL(EETerrainTable, gTerrainTable, );
// chain at 0x53ecf0 (wrapper)
EE_CONSTRUCT_GLOBAL(EETerrainColorTable, gTerrainColorTable, );
// chain at 0x53ed1b (wrapper)
EE_CONSTRUCT_GLOBAL(EETerrainTypeTable, gTerrainTypeTable, );
// chain at 0x53ed71 (wrapper)
EE_CONSTRUCT_GLOBAL(EEGameState, gGameState, );
// chain at 0x53ed9b (wrapper)
EE_CONSTRUCT_GLOBAL(EEComputerPlayerBehaviorTable, gComputerPlayerBehaviorTable, );
// chain at 0x53ede4 (wrapper)
EE_CONSTRUCT_GLOBAL(EEUnitSetManager, gUnitSetManager, );
// chain at 0x53ee26 (wrapper)
EE_CONSTRUCT_GLOBAL(EEAEManager, gAEManager, );
// chain at 0x53ee50 (wrapper)
EE_CONSTRUCT_GLOBAL(EEAmbientSoundManager, gAmbientSoundManager, );
// chain at 0x53ee7a (wrapper)
EE_CONSTRUCT_GLOBAL(EECivilizationManager, gCivilizationManager, );
// chain at 0x53eea4 (wrapper)
EE_CONSTRUCT_GLOBAL(EEDoppelgangerManager, gDoppelgangerManager, );
// chain at 0x53eece (wrapper)
EE_CONSTRUCT_GLOBAL(EECivPowersManager, gCivPowersManager, );
// chain at 0x53eef8 (wrapper)
EE_CONSTRUCT_GLOBAL(UWideString, gCampaignAOCRoman, ("AOCRoman"));
// chain at 0x53ef2a (wrapper)
EE_CONSTRUCT_GLOBAL(UWideString, gCampaignAOCPacific, ("AOCPacific"));
// chain at 0x53ef5c (wrapper)
EE_CONSTRUCT_GLOBAL(UWideString, gCampaignAOCAsian, ("AOCAsian"));
// chain at 0x53ef8e (wrapper)
EE_CONSTRUCT_GLOBAL(UWideString, gCampaignEEDemo, ("EEDemo"));
// chain at 0x53efc0 (wrapper)
EE_CONSTRUCT_GLOBAL(UWideString, gCampaignEELearning, ("EELearningCampaign"));
// chain at 0x53eff2 (wrapper)
EE_CONSTRUCT_GLOBAL(UWideString, gU52A280, ("een1ve1s_"));
// chain at 0x53f024 (wrapper)
EE_CONSTRUCT_GLOBAL(UString, gPhysicsThreadName, ("Physics"));
// chain at 0x53f056 (wrapper)
EE_CONSTRUCT_GLOBAL(UString, gGraphicsThreadName, ("Graphics"));
// chain at 0x53f088 (wrapper)
EE_CONSTRUCT_GLOBAL(UString, gDataPreLoadingThreadName, ("DataPreLd"));
// chain at 0x53f0ba (wrapper)
EE_CONSTRUCT_GLOBAL(UString, gMousePointerThreadName, ("MousePtr"));
// chain at 0x53f0ec (wrapper)
EE_CONSTRUCT_GLOBAL(UString, gScenarioEditorThreadName, ("ScenEdit"));
extern FSPath* gCivilizationDataPath;

HINSTANCE gInstance;                        // 0x5319d8 (WinMain's instance, set by 0x540d73)

// (the loading splash screen, EEStartScreen: ee_game gamecpp start_screen.rs; the start-up and shutdown bodies, the
// game timer sTimer 0x5319ec and the start-up video mode 0x52a1a8: startup.rs, which calls them by path)

// Sums a checksum over the directory's files matching the pattern (name order, one encrypted name skipped): file
// bytes, or write time and size when asked
#include <algorithm>
#include "ee/EEStringIDs.h"
// The original's sort helpers (0x547719..) call FSFileSpec's GetFile (vtable +0x1c), then UString's operator<;
// the comparison object is empty (one byte)
struct EEFileNameSorter {
    bool operator()(const FSFileSpec& inFirst, const FSFileSpec& inSecond) const
    {
        return inFirst.GetFile() < inSecond.GetFile();
    }
};

unsigned long EEGetFileChecksum(FSFileSpec& inDirectory, const UString& inPattern, long inUseFileTimes)
{
    return ee_gamecpp_entry_file_checksum(&inDirectory, &inPattern, inUseFileTimes);
}

#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/NEProtocol.h"
#include "lle/NEMessage.h"
#include "ee/EEMessage.h"

#include "lle/UIFontTable.h"
#include "ee/EERenderObject.h"
#include "ee/EERenderLists.h"
#include "ee/EEROTerrain.h"
#include "ee/EEROGroundShadows.h"
#include "ee/EEROUnits.h"
#include "ee/EEROWater.h"
#include "ee/EEROEffects.h"

// the game view and water ripple constants (.rdata 0x442090..0x4420a8, read from memory by both graphics initializers)
#include "lle/Input System.h"
#include "ee/EEUserInterface.h"

#include "lle/GERhombus.h"
#include "ee/EEMouse.h"
#include "ee/RTSMousePointer.h"
#include "ee/EEDataPreLoading.h"
#include "ee/EEPhysics.h"

#include "lle/UIControl.h"
#include "ee/EEUIManager.h"

#include <list>
#include "lle/NESynchronousCommunications.h"
#include "ee/EECommunications.h"

#include <typeinfo>
#include "ee/EEUnit.h"

// Builds the data search path and the civilization path, replacing the previous ones. The data is the game's own
// Data folder (the data root's: the executable's directory), holding the content of both retail archives and the
// original game's loose data (tools/open_data.py). The original searched four places: this install's Data\data.ssa
// and Data\, then the original Empire Earth's (its install path from the registry); the original game is no longer
// needed. A Data\data.ssa is still mounted when present (an install copy), behind the folder as before.
void EEShowSinglePlayerScreen()
{
    ee_gamecpp_startup_data_paths();
}

// Creates the game state, empty
EEGameState::EEGameState()
    : mChecksum(true)
{
    this->Init();
}

// Destroys the game state, clearing it
EEGameState::~EEGameState()
{
    this->Clear();
}

// the game view in 1600 x 1200 layout units -> the window (reloaded at each use: named consts; 0x44217c, 0x4413bc)

// Sets the viewport to the rectangle (given in 1600x1200 layout units, scaled to the window unless the camera is
// free) and stops camera following
void EESetGameViewRectangle(const GERectangle& inRectangle)
{
    ee_gamecpp_startup_game_view(reinterpret_cast<const EeGeRectangle*>(&inRectangle));
}

// Points the texture, model and sound managers at the data search path
void EESetDataPaths()
{
    ee_gamecpp_startup_set_data_paths();
}

#ifdef _WIN32
static HANDLE sInstanceMutex;
#endif

// Creates the single-instance mutex; returns false when another copy of the game already holds it
// (called by the start-up 0x540d73)
bool EECreateInstanceMutex()
{
#ifdef _WIN32   // (the test harness finds the GetLastError call here: keep it direct)
    // TEST ONLY: under the UI fixture (EE_TEST_UI), EE_TEST_INSTANCE=<name> names the mutex per instance, so that a
    // two-instance test (tools/harness/lan_game.py) can run both games on one machine
    std::string theName("MadDocSoftwarePresentsEmpireEarthExpansion");
    const char* theInstance = getenv("EE_TEST_INSTANCE");
    if (getenv("EE_TEST_UI") != NULL && theInstance != NULL && *theInstance != 0)
        theName += std::string("-") + theInstance;
    sInstanceMutex = CreateMutex(NULL, FALSE, theName.c_str());
    if (sInstanceMutex != NULL && GetLastError() == 0)
        return true;
    return false;
#else
    return PlatSystemSingleInstance("MadDocSoftwarePresentsEmpireEarthExpansion") != 0;
#endif
}

// Closes the single-instance mutex and saves the preferences
// (no direct caller: registered as a shutdown routine)
void EEReleaseInstanceMutex(bool inShutdown)
{
#ifdef _WIN32
    CloseHandle(sInstanceMutex);
#endif
    gGamePreferences.Save();
}

// Database shutdown is defined with the startup family below.

#include "ee/EEDisplayDevices.h"

extern std::vector<GETexture*> gU4E8EF0; // defined in EERenderObject.cpp

#include "ee/EEWonNet.h"

#include "ee/EEButtonObject.h"

// port: the menus' resolution. Their controls are laid out for 1024x768 (pixel sizes; the forms anchor to their
// edges), so they keep its height of 768 and take the game resolution's aspect ratio: the frame scales to the window
// without the 4:3 stretch, and each control keeps its designed proportions. A game resolution under 768 lines is used
// as it is.
unsigned long EEMenuHeight()
{
    return gGameWindowHeight > 768 ? 768 : gGameWindowHeight;
}

unsigned long EEMenuWidth()
{
    return ee_gamecpp_menu_width(gGameWindowWidth, gGameWindowHeight);   // (gamecpp posix_main.rs)
}

#include "lle/Graphics Engine.h"

// Starts the graphics engine, picks the preferred rasterizer, validates screen, depths and resolution (falling
// back to 800x600x16), creates and captures the game window; returns true
bool EEInitializeDisplay()
{
    return ee_gamecpp_startup_display();
}

#ifdef _WIN32
#include <objbase.h>
#endif

#pragma optimize("g", off)   // rule 4, owner 2026-11-01: WinMain is no-/Og (memory locals, constant push 1/pop eax test)
// Runs the game: single-instance check, start-up, splash and intro, the user interface loop, then shutdown;
// returns 1 when the original Empire Earth install is missing, else 0
// (the previous instance and show command arguments are unused)
// (EESimPreferences, the window's event pump and the native message hook: gamecpp entry.rs, window_pump.rs)

// The game's run (ee_game gamecpp entry.rs): the Rust library's start first, the window system, the engine, the
// start-up under the splash screen, the menus, the command line and the user interface's run, then the shutdown
static int EERun(HINSTANCE inInstance, char* inCommandLine)
{
    return ee_gamecpp_entry_run((void*)inInstance, inCommandLine);
}

#ifdef _WIN32
// the Windows entry point (the previous instance and show command arguments are unused)
int WINAPI WinMain(HINSTANCE inInstance, HINSTANCE inPreviousInstance, LPSTR inCommandLine, int inShowCommand)
{
    return EERun(inInstance, inCommandLine);
}
#else
// The POSIX entry point: EE-SIM's run (ee-aoc --sim ..., or an ee-sim executable), else the game. The arguments'
// test, the command line, the data root's check and entry and --smoke-test are ee_game gamecpp posix_main.rs's.
int EESimMain(int argc, char** argv);   // EESim.cpp (TEST ONLY): the headless simulation, "ee-aoc --sim ..."

int main(int argc, char** argv)
{
    int theSkip = ee_gamecpp_posix_sim_skip(argc, argv);
    if (theSkip >= 0) {
        int theResult = EESimMain(argc - theSkip, argv + theSkip);
        if (theResult != -1)
            return theResult;
        argc = 1;   // the simulation's own options are not the game's command line
    }
    char* theCommandLine = NULL;
    int theCode = ee_gamecpp_posix_start(argc, argv, &theCommandLine);
    if (theCode >= 0)
        return theCode;
    int theResult = EERun(NULL, theCommandLine);
    if (gPlatSimMode)
        ee_sim_driver_game_ended(theResult);   // (does not return)
    return theResult;
}
#endif
#pragma optimize("g", on)   // ??_GUResourceFile 0x54494d (no-/Og in the original) is emitted under WinMain's state

// Loads and starts a .wav or .mp3 sound (a bare name tries .wav, then .mp3), registering it as a one-shot when
// asked; returns the sample and instance, -1 on failure
void EELoadSound(const UString& inName, unsigned long inType, unsigned long& outSample,
                 unsigned long& outInstance, unsigned long inFlag)
{
    ee_gamecpp_startup_load_sound(&inName, inType, &outSample, &outInstance, inFlag);
}

#include "ee/EEStartingResources.h"
#include "ee/EEMedia.h"
#include "lle/USingleton.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EESoundObject.h"
#include "ee/EEMusicObject.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEAnimal.h"
#include "ee/EEColorTable.h"
#include "ee/EEUCUpgradeTable.h"
#include "ee/EECPBehavior.h"
#include "ee/EEGameVariant.h"
#include "ee/EECalamityManager.h"
#include "ee/EERandomMapClass.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEArtificialIntelligence.h"
#include "ee/EECheatCodes.h"

#include "lle/User Interface.h"

// Starts the sound engine and music conductor from the preferences and creates the sound manager; returns false
// if the sound engine fails, else true
bool EEInitializeSounds()
{
    return ee_gamecpp_startup_sounds();
}

// Cleanup routines defined in other units
void EEReportLeakingUnits();

// ------------------------------------------------------------------------- the start-up's C++ (gamecpp startup.rs)

#include <utility>
#include "rust/audio/sound.h"
#include "rust/ui/bgtableobj.h"

int64_t EEGraphicsOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

// the constants startup.rs passes
static_assert(IDS_ARIAL == 1 && IDS_TIMES_NEW_ROMAN == 2 && IDS_COURIER_NEW == 3 && IDS_TAHOMA == 4 && IDS_CIVILIZATIONS == 29783 && IDS_EMPIRE_EARTH_ART_OF_CONQUEST_REQUIRES_SOUND == 5865 && IDS_ERROR_CANT_RUN_16EA == 5866 && kSoundHotKeyAlarm == 0x3c2 && kPixelFormatZPrefer16 == 12, "gamecpp startup.rs");
static_assert(sizeof(GEVideoMode) == 5 * sizeof(unsigned long) && sizeof(GE2DPoint) == 2 * sizeof(float) && sizeof(GERectangle) == sizeof(EeGeRectangle), "gamecpp startup.rs");

// the start-up directory as a file spec (the engines' initialisations take one)
static FSFileSpec& EEStartupDirectorySpec()
{
    static FSFileSpec theDirectory;
    theDirectory.SetDirectory(gStartupDirectory);
    return theDirectory;
}

#define P64(x) ((int64_t)(intptr_t)(x))
#define WSTR (*(UWideString*)p)
#define USTR (*(UString*)p)
#define SPEC ((FSFileSpec*)p)
#define SPECS ((std::vector<FSFileSpec>*)p)
#define SETTINGS gGamePreferences.mSettings
#define SCREEN_ID gGamePreferences.mSettings.GetScreenID()
static GEColor sStartupBlack(0.0f, 0.0f, 0.0f);   // the surfaces' clear colour (GESurface::Clear takes a reference)

// The start-up's C++ calls and members (EeGameCpp's start-up section; EEMessage.cpp's EEMessageOp default): one
// statement each
int64_t EEStartupOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    // the floating point, the engines
    case O::IsInitialize: return ISInitialize(gInstance, gGameWindow, gResource, 100, 250);
    case O::GeInitialize: return GEInitialize(EEStartupDirectorySpec(), gInstance);
    case O::SeInitialize: return SEInitialize(EEStartupDirectorySpec(), gGameWindow);
    case O::RegisterShutdownMutex: return URegisterShutdown(EEReleaseInstanceMutex);
    case O::RegisterShutdownPersistents: return URegisterShutdown(FSPersistentList::Shutdown);
    case O::WindowHide: PlatWindowShow(gGameWindow->GetOSRepresentation(), 0);
        break;
    case O::MessageShowError: PlatMessageShow(WSTR, *(UWideString*)a[0], PLAT_MESSAGE_ERROR | PLAT_MESSAGE_SYSTEM_MODAL | PLAT_MESSAGE_FOREGROUND);
        break;
    case O::NewUserInterface: gUserInterface = new EEUserInterface;
        break;
    // the graphics: the managers, the window, the render objects, the fonts
    case O::NewTextureManager: gTextureManager = new GETextureManager(gDataPath);
        break;
    case O::NewModelManager: gModelManager = new GEModelManager(gDataPath);
        break;
    case O::TexturesSetPath: gTextureManager->SetTexturePath(gDataPath);
        break;
    case O::ModelsSetPath: gModelManager->SetModelPath(gDataPath);
        break;
    case O::SoundSetPath: gSoundManager->SetPath(gDataPath);
        break;
    case O::PrefScreenID: return SCREEN_ID;
    case O::PrefScreenIDRaw: return SETTINGS.mScreenID;
    case O::PrefGameBitDepth: return SETTINGS.mGameBitDepth;
    case O::PrefTextureBitDepth: return SETTINGS.mTextureBitDepth;
    case O::PrefGameBitDepthValid: return !(SETTINGS.mGameBitDepth != 32 && SETTINGS.mGameBitDepth != 16);
    case O::PrefTextureBitDepthValid: return !(SETTINGS.mTextureBitDepth != 32 && SETTINGS.mTextureBitDepth != 16);
    case O::PrefSetGameBitDepth16: SETTINGS.mGameBitDepth = 16;
        break;
    case O::PrefSetTextureBitDepth16: SETTINGS.mTextureBitDepth = 16;
        break;
    case O::PrefFullScreen: return SETTINGS.GetFullScreen();
    case O::PrefSave: gGamePreferences.Save();
        break;
    case O::PrefSetRasterizerName: SETTINGS.mRasterizerName = gRasterizer->GetName();
        break;
    case O::PrefMusicEnabled: return SETTINGS.mMusicEnabled;
    case O::PrefReverbEnabled: return SETTINGS.mReverbEnabled;
    case O::WindowResize: gGameWindow->Resize((unsigned long)a[0], (unsigned long)a[1]);
        break;
    case O::WindowWidth: return gGameWindow->GetWidth();
    case O::WindowHeight: return gGameWindow->GetHeight();
    case O::WindowClientOrigin: *(GE2DPoint*)a[0] = gGameWindow->GetClientOrigin();
        break;
    case O::WindowRaise: PlatWindowRaise(gGameWindow->GetOSRepresentation());
        break;
    case O::WindowSetFixedSize: PlatWindowSetFixedSize(gGameWindow->GetOSRepresentation(), 1);
        break;
    case O::WindowTextInputStart: PlatTextInputStart(gGameWindow->GetOSRepresentation(), 0, 0, 0, 0);
        break;
    case O::DisplayLogicalSize: return PlatWindowGetDisplayLogicalSize(NULL, (int32_t*)a[0], (int32_t*)a[1]);
    case O::CreateGameWindow: return gRasterizer->GetScreen(SCREEN_ID)->CreateAWindow(GE2DPoint(f[0], f[1]), (unsigned long)a[0], (unsigned long)a[1], UString("Empire Earth Expansion"), UString("Empire Earth: The Art of Conquest"), gGameWindow, (long)a[2], NULL);
    case O::DeleteGameWindow: delete std::exchange(gGameWindow, nullptr);
        break;
    case O::GameSizeFromPreferences: GXGameSize((unsigned long)SETTINGS.mUIScale, !SETTINGS.GetFullScreen(), &gGameWindowWidth, &gGameWindowHeight);
        break;
    case O::GameWindowWidth: return gGameWindowWidth;
    case O::GameWindowHeight: return gGameWindowHeight;
    case O::SetGameWindowFallback: gGameWindowWidth = 800, gGameWindowHeight = 600, SETTINGS.mGameBitDepth = 16;
        break;
    case O::MenuWidth: return EEMenuWidth();
    case O::MenuHeight: return EEMenuHeight();
    case O::RectRoundToNearest: ((GERectangle*)a[0])->RoundToNearest();
        break;
    case O::ViewportSetViewRectangle: gViewport.SetViewRectangle(*(GERectangle*)a[0]);
        break;
    case O::SceneFlagsClear: gB529066 = false, gCameraFollowing = false, gB529068 = false;
        break;
    case O::DisplayRasterizersClear: gDisplayRasterizers.clear();
        break;
    case O::RasterizerCount: return GERasterizer::GetRasterizerCount();
    case O::RasterizerSelect: gRasterizer = GERasterizer::GetRasterizer((unsigned long)a[0]);
        break;
    case O::RasterizerIsPreferred: return gRasterizer->GetName() == SETTINGS.mRasterizerName;
    case O::RastActivate: return gRasterizer->Activate();
    case O::RastIsActivated: return gRasterizer->GetIsActivated();
    case O::RastDeactivate: gRasterizer->Deactivate();
        break;
    case O::RastScreenCount: return gRasterizer->GetScreenCount();
    case O::RastSetScreenID: gRasterizer->SetScreenID(SCREEN_ID);
        break;
    case O::SetScreenIndexZero: gScreenIndex = 0;
        break;
    case O::RastActivateScreen: return gRasterizer->ActivateScreen(SCREEN_ID, gTextureManager, gGameWindow, SETTINGS.GetScreenMode(), a[0] != 0, (long)a[1]);
    case O::RastClearStart: gRasterizer->Clear(GEColor(0.0f, 0.0f, 0.0f), GERectangle(0.0f, 0.0f, 640.0f, 480.0f), (unsigned long)a[0]);
        break;
    case O::RastShowScene: gRasterizer->ShowScene((unsigned long)a[0]);
        break;
    case O::RastPrimaryPresent: return gRasterizer->GetPrimarySurface((unsigned long)a[0]) != NULL;
    case O::RastSecondaryPresent: return gRasterizer->GetSecondarySurface((unsigned long)a[0]) != NULL;
    case O::RastPrimaryClear: gRasterizer->GetPrimarySurface((unsigned long)a[0])->Clear(*(GERectangle*)a[1], sStartupBlack, 1.0f);
        break;
    case O::RastSecondaryClear: gRasterizer->GetSecondarySurface((unsigned long)a[0])->Clear(*(GERectangle*)a[1], sStartupBlack, 1.0f);
        break;
    case O::RastSetFog: gRasterizer->SetFog((long)a[0], (unsigned long)a[1]);
        break;
    case O::RastSetFogColor: gRasterizer->SetFogColor(GEColor(f[0], f[1], f[2]), (unsigned long)a[0]);
        break;
    case O::RastSetFogType: gRasterizer->SetFogType((long)a[0], (unsigned long)a[1]);
        break;
    case O::RastSetAmbientColor: gRasterizer->SetAmbientColor(GEColor(f[0], f[1], f[2]), (unsigned long)a[0]);
        break;
    case O::RastSetAmbientIntensity: gRasterizer->SetAmbientIntensity(f[0], (unsigned long)a[0]);
        break;
    case O::RastSetColorKeying: gRasterizer->SetColorKeying((long)a[0], (unsigned long)a[1]);
        break;
    case O::RastSetCullingCW: gRasterizer->SetCulling(kCullCW);
        break;
    case O::RastSetLighting: gRasterizer->SetLighting((long)a[0], (unsigned long)a[1]);
        break;
    case O::RastSetProjectionMode: gRasterizer->SetProjectionMode((long)a[0], (unsigned long)a[1]);
        break;
    case O::RastSetShading: gRasterizer->SetShading((long)a[0], (unsigned long)a[1]);
        break;
    case O::RastSetSpecular: gRasterizer->SetSpecular((long)a[0], (unsigned long)a[1]);
        break;
    case O::RastSetZBufferOff: gRasterizer->SetZBuffer(kZBufferDisabled, (unsigned long)a[0]);
        break;
    case O::RastSetZCompareLessEqual: gRasterizer->SetZCompare(kCompareLessEqual, (unsigned long)a[0]);
        break;
    case O::RastSetDithering: gRasterizer->SetDithering((long)a[0], (unsigned long)a[1]);
        break;
    case O::RastSetAntiAliasingPref: gRasterizer->SetAntiAliasing(SETTINGS.mAntiAliasing != false, (unsigned long)a[0]);
        break;
    case O::RastSetAlphaTesting: gRasterizer->SetAlphaTesting((long)a[0], (unsigned long)a[1]);
        break;
    case O::RastSetAlphaCompareGreater: gRasterizer->SetAlphaTestingCompareState(kCompareGreater, (unsigned long)a[0]);
        break;
    case O::RastSetAlphaRef: gRasterizer->SetAlphaTestingRefValue(f[0], (unsigned long)a[0]);
        break;
    case O::RastSetVSyncPref: gRasterizer->SetWaitForVSync(SETTINGS.mWaitForVSync, (unsigned long)a[0]);
        break;
    case O::RastSetWaterRipple: gRasterizer->SetWaterRippleEffect(f[0], f[1], f[2]);
        break;
    case O::ScreenModeCount: return gRasterizer->GetScreen((unsigned long)a[0])->GetModeCount();
    case O::ScreenCurrentMode: *(GEVideoMode*)a[1] = gRasterizer->GetScreen((unsigned long)a[0])->GetCurrentMode();
        break;
    case O::ScreenMode: *(GEVideoMode*)a[2] = gRasterizer->GetScreen((unsigned long)a[0])->GetMode((unsigned long)a[1]);
        break;
    case O::ScreenSetMode: gRasterizer->GetScreen((unsigned long)a[0])->SetMode((unsigned long)a[1]);
        break;
    case O::ScreenCapture: return gRasterizer->GetScreen(SCREEN_ID)->Capture(gGameWindow, SETTINGS.GetFullScreen());
    case O::ScreenReleaseWindow: gRasterizer->GetScreen(SCREEN_ID)->Release(gGameWindow);
        break;
    case O::ScreenCurrentModeIndex: return gRasterizer->GetScreen(SCREEN_ID)->GetCurrentModeIndex();
    case O::ScreenDesktopRect: *(GERectangle*)a[0] = gRasterizer->GetScreen(SCREEN_ID)->GetDesktopRectangle();
        break;
    case O::Screen32BitDevice: return gRasterizer->GetScreen(SCREEN_ID)->Get32BitDeviceSupport();
    case O::Screen32BitTexture: return gRasterizer->GetScreen(SCREEN_ID)->Get32BitTextureSupport();
    case O::DebugVideoModeActive: ESDebugOut("!::EESetVideoMode: Requested video mode is already active.\n");
        break;
    case O::DebugVideoModeChanged: ESDebugOut("!::EESetVideoMode: Video mode changed to %ldx%ldx%ld at %ldHz.\n", ((GEVideoMode*)a[0])->mWidth, ((GEVideoMode*)a[0])->mHeight, ((GEVideoMode*)a[0])->mBitCount, ((GEVideoMode*)a[0])->mRefreshRate);
        break;
    case O::NewRoTerrain: gU56535C = new EEROTerrain;
        break;
    case O::NewRoGroundShadows: gU565358 = new EEROGroundShadows;
        break;
    case O::NewRoUnits: gU565354 = new EEROUnits;
        break;
    case O::NewRoWater: gU565350 = new EEROWater;
        break;
    case O::NewRoEffects: gU565360 = new EEROEffects;
        break;
    case O::DeleteRoWater: delete std::exchange(gU565350, nullptr);
        break;
    case O::DeleteRoGroundShadows: delete std::exchange(gU565358, nullptr);
        break;
    case O::DeleteRo4C: delete std::exchange(gU56534C, nullptr);
        break;
    case O::DeleteRoTerrain: delete std::exchange(gU56535C, nullptr);
        break;
    case O::DeleteRoUnits: delete std::exchange(gU565354, nullptr);
        break;
    case O::DeleteRoEffects: delete std::exchange(gU565360, nullptr);
        break;
    case O::DeleteTextureManager: delete std::exchange(gTextureManager, nullptr);
        break;
    case O::DeleteModelManager: delete std::exchange(gModelManager, nullptr);
        break;
    case O::ModelsRemoveAll: gModelManager->RemoveAllModels();
        break;
    case O::TexturesRemoveAll: gTextureManager->RemoveAllTextures();
        break;
    case O::TexturesRefreshAll: return gTextureManager->RefreshAllTextures();
    case O::EffectTextureCount: return (int64_t)gU4E8EF0.size();
    case O::EffectTextureDereference: gU4E8EF0[a[0]]->Dereference();
        break;
    case O::UStrNew: return P64(new UString);
    case O::UStrFree: delete (UString*)p;
        break;
    case O::UStrFromW: USTR = (const char*)*(UWideString*)a[0];
        break;
    case O::FontCreate: return P64(UIFontTable::FindOrCreateFont(USTR, (unsigned long)a[0], 2, (unsigned long)a[1]));
    case O::FontDereference: ((GEFont*)p)->Dereference();
        break;
    case O::PersistChecksum: FSPersistentList::RegisterPersistent(new EEPChecksum(true));
        break;
    case O::PersistGeneric2: FSPersistentList::RegisterPersistent(new EEPTGeneric<unsigned long>(2));
        break;
    case O::PersistMap: FSPersistentList::RegisterPersistent(new EEPTPRestorable<EEMap>(4));
        break;
    case O::PersistPoint3: FSPersistentList::RegisterPersistent(new EEPTRestorable<GE3DPoint>(GE3DPoint(), 3));
        break;
    case O::PersistPlayers: FSPersistentList::RegisterPersistent(new EEPPlayerHelper);
        break;
    case O::PersistScenario: FSPersistentList::RegisterPersistent(new EEPTPRestorable<EEScenarioManager>(6));
        break;
    case O::PersistWorld: FSPersistentList::RegisterPersistent(new EEPWorldHelper);
        break;
    case O::PersistMultiplayer: FSPersistentList::RegisterPersistent(new EEPMultiplayerHelper);
        break;
    case O::PersistEffects: FSPersistentList::RegisterPersistent(new EEPTPRestorable<EEEffectManager>(0xa));
        break;
    case O::PersistCalamity: FSPersistentList::RegisterPersistent(new EEPCalamityHelper);
        break;
    case O::PersistEnvironment: FSPersistentList::RegisterPersistent(new EEPTPRestorable<EEEnvironmentManager>(0xc));
        break;
    // the media
    case O::UiLoadIcons: gEEGameUI.mLoadIcons();
        break;
    case O::UiUnloadIcons: gEEGameUI.mUnloadIcons();
        break;
    case O::UiReloadIcons: gEEGameUI.mReloadIcons();
        break;
    case O::UiMiniMapLoadMedia: gEEGameUI.mMiniMapLoadMedia();
        break;
    case O::UiMiniMapUnloadMedia: gEEGameUI.mMiniMapUnloadMedia();
        break;
    case O::RoUnitsLoadMedia: EEROUnits::LoadMedia();
        break;
    // the data paths and folders
    case O::NewFsPath: return P64(new FSPath);
    case O::FsPathAddArchive: ((FSPath*)p)->AddArchive(*(FSArchive*)a[0], true);
        break;
    case O::FsPathAddDirectory: ((FSPath*)p)->AddDirectory(*(FSFileSpec*)a[0], true);
        break;
    case O::FileSpecSetArchiveName: SPEC->SetFile("data" + gU530B88);
        break;
    case O::FileSpecSetCivilizationDir: SPEC->SetDirectory(EEGetCivilizationDirectory(true));
        break;
    case O::FileSpecSetUserCivilizationsDir: SPEC->SetDirectory(gStartupDirectory + "Users\\default\\" + (const char*)*(UWideString*)a[0] + "\\");
        break;
    case O::FileSpecIsDirectory: return SPEC->IsDirectory();
    case O::FileSpecCreateDirectory: SPEC->CreateDirectory();
        break;
    case O::DataArchivePresent: return PlatPathGetAttributes(gStartupVolume + gStartupDirectory + "data\\data" + gU530B88) != PLAT_ATTR_INVALID;
    case O::DataPathDereference: gDataPath->Dereference();
        break;
    case O::CivPathDereference: gCivilizationDataPath->Dereference();
        break;
    case O::SetDataPath: gDataPath = (FSPath*)p;
        break;
    case O::SetCivPath: gCivilizationDataPath = (FSPath*)p;
        break;
    // the threads
    case O::NewMusicManager: gMusicManager = new EEMusicManager(&gConductor->GetConductor(), 0);
        break;
    case O::MusicManagerDelete: delete std::exchange(gMusicManager, nullptr);
        break;
    case O::MusicStop: gMusicManager->Stop();
        break;
    case O::NewMousePointer: gMousePointer = new RTSMousePointer(gMouse, gGameWindow, 0);
        break;
    case O::MousePointerPost: gMousePointer->mThread->Post((unsigned long)a[0], NULL, NULL, true);
        break;
    case O::MousePointerShown: return gMousePointer->mIsShown;
    case O::MousePointerShutdown: ee_gamecpp_startup_quit_thread(gMousePointer->mThread, gMousePointer->mThreadDone);
        break;
    case O::MousePointerDelete: delete std::exchange(gMousePointer, nullptr);
        break;
    case O::MousePointerPostSurface: gMousePointer->mThread->Post(0x405, gRasterizer->GetPrimarySurface(SCREEN_ID), NULL, true);
        break;
    case O::NewGraphics: gGraphics = new EEGraphics(gRasterizer, SCREEN_ID, gMouse, gUIManager, 0);
        break;
    case O::GfxShutdown: gGraphics->Shutdown();
        break;
    case O::GfxDelete: delete std::exchange(gGraphics, nullptr);
        break;
    case O::GfxPostReleaseScene: gGraphics->PostReleaseScene();
        break;
    case O::GfxPostMessage50AAndWait: gGraphics->PostMessage50AAndWait();
        break;
    case O::GfxSetSecondarySurface: gGraphics->SetSurface(gRasterizer->GetSecondarySurface(SCREEN_ID));
        break;
    case O::NewDataPreLoading: gDataPreLoading = new EEDataPreLoading(0);
        break;
    case O::PreloadShutdown: ee_gamecpp_startup_quit_thread(gDataPreLoading->mThread, gDataPreLoading->mThreadDone);
        break;
    case O::PreloadDelete: delete std::exchange(gDataPreLoading, nullptr);
        break;
    case O::PreloadWaitForQueue: gDataPreLoading->WaitForMessageQueue();
        break;
    case O::NewPhysics: gPhysics = new EEPhysics(gGraphics, (TMTimer*)p);
        break;
    case O::PhysShutdown: ee_gamecpp_startup_quit_thread(gPhysics->mThread, gPhysics->mThreadDone);
        break;
    case O::PhysDelete: delete std::exchange(gPhysics, nullptr);
        break;
    case O::PhysTimerRunning: return gPhysics->mTimerRunning;
    case O::PhysPostInit: gPhysics->PostInit();
        break;
    case O::CommDelete: delete gCommunications;
        break;
    case O::CommDeleteDetached: delete std::exchange(gCommunications, nullptr);
        break;
    case O::WonNetDeleteDetached: delete std::exchange(gWonNet, nullptr);
        break;
    case O::UserInterfaceDelete: delete std::exchange(gUserInterface, nullptr);
        break;
    case O::WorldClear: gWorld.Clear();
        break;
    case O::GameStateClear: gGameState.Clear();
        break;
    case O::SetGameScreenActive: gWorld.mGameScreenActive = a[0] != 0;
        break;
    case O::IsMouseResume: gISMouse->Resume();
        break;
    case O::IsMouseSuspend: gISMouse->Suspend();
        break;
    case O::IsKeyboardResume: gISKeyboard->Resume();
        break;
    case O::IsKeyboardSuspend: gISKeyboard->Suspend();
        break;
    // the sound
    case O::MixerGet: return (long)ee_sound_mixer_get((uint32_t)0, reinterpret_cast<EeSoundMixer**>(&gConductor));
    case O::MixerUse: return (long)ee_sound_mixer_use(reinterpret_cast<EeSoundMixer*>(gConductor));
    case O::ConductorSetChannelGroups: return gConductor->GetConductor().SetChannelGroupCount((unsigned long)a[0]);
    case O::ConductorSetState: return gConductor->GetConductor().SetConductorState((long)a[0]);
    case O::ConductorSetReverb: gConductor->GetConductor().SetReverb((long)a[0]);
        break;
    case O::ConductorSetVolumePref: gConductor->GetConductor().SetVolume(SETTINGS.mMusicVolume);
        break;
    case O::ConductorSetIntensity0: gConductor->GetConductor().SetIntensity(0, false);
        break;
    case O::StrFormatMusicFolder: sprintf(WSTR, "%s%s%s", (const char*)gStartupVolume, (const char*)gStartupDirectory, "Data\\Music\\");
        break;
    case O::ConductorSetMusicFolder: return gConductor->GetConductor().SetMusicFolder(WSTR);
    case O::NewSoundManager: gSoundManager = SESoundManager::Create(gDataPath, gConductor, 50, 0);
        break;
    case O::SoundMaxChannels: return gSoundManager->GetMaxChannels();
    case O::SoundSetMaxChannels: gSoundManager->SetMaxChannels((unsigned long)a[0]);
        break;
    case O::SoundSetMasterVolumePref: gSoundManager->SetMasterVolume(SETTINGS.mSoundVolume);
        break;
    case O::SoundSetCategoryPriority: gSoundManager->SetCategoryPriority((unsigned long)a[0], (long)a[1]);
        break;
    case O::SoundSetCategoryPercentage: gSoundManager->SetCategoryPercentage((unsigned long)a[0], f[0]);
        break;
    case O::SoundSetLatency: gSoundManager->SetLatency((unsigned long)a[0]);
        break;
    case O::SoundStopOneShots: gSoundManager->StopAllOneShotSounds();
        break;
    case O::SoundManagerDestroy: SESoundManager::Destroy(std::exchange(gSoundManager, nullptr));
        break;
    case O::SoundPoolDump: SESoundManager::DumpMemoryPoolInfo();
        break;
    case O::SoundsPoolInit: SESoundManager::InitializeSoundsPool((unsigned long)a[0], (unsigned long)a[1], (unsigned long)a[2]);
        break;
    case O::SoundLoad: return gSoundManager->Load(USTR, (unsigned long)a[0], 1);
    case O::SoundPlaySample: return gSoundManager->Play((unsigned long)a[0], 1.0f, 0.0f, 1.0f, false, NULL);
    case O::SoundRegisterOneShot: gSoundManager->RegisterOneShotSound((unsigned long)a[0], (unsigned long)a[1]);
        break;
    case O::UStrHasSoundExtension: return USTR.GetLength() > 4 && (USTR.GetSubstring(USTR.GetLength() - 4, 4).EqualsIgnoreCase(".wav", 4) || USTR.GetSubstring(USTR.GetLength() - 4, 4).EqualsIgnoreCase(".mp3", 4));
    case O::UStrWithExtension: USTR = *(const UString*)a[0] + (a[1] ? ".mp3" : ".wav");
        break;
    case O::AmbientStop: gAmbientSoundManager.StopSounds();
        break;
    case O::AmbientSetEnabled: gAmbientSoundManager.SetEnabled(a[0] != 0);
        break;
    case O::AmbientLoad: gAmbientSoundManager.Load(*SPEC);
        break;
    case O::AmbientClearSounds: gAmbientSoundManager.ClearSounds();
        break;
    // the database
    case O::LoadRules: EELoadRules(gDataPath);
        break;
    case O::TerrainColorsLoad: return EETerrainColorTable::Load();
    case O::DbLoadStartingResources: return EEStartingResources::Load(SPEC);
    case O::DbLoadUnitBehavior: return EEUnitBehavior::Load(SPEC);
    case O::DbLoadSounds: return EESoundObject::LoadTable(SPEC);
    case O::DbLoadGraphics: return EEGraphicObject::LoadTable(SPEC);
    case O::DbLoadGfxEffects: return EEGraphicEffect::LoadTable(SPEC);
    case O::DbLoadMusic: return EEMusicObject::LoadTable(SPEC);
    case O::DbLoadWorld: return gWorldTable.Load(SPEC);
    case O::DbLoadFamily: return EETerrainTypeTable::LoadDamageTables(SPEC);
    case O::DbLoadButtons: return EEButtonObject::Load(SPEC);
    case O::DbLoadTechTree: return gTechTree.Load(*SPEC);
    case O::DbLoadAnimals: return EEAnimal::Load(SPEC);
    case O::DbLoadColorTable: return EEColorTable::Load(SPEC);
    case O::DbLoadAIUnitTargeting: return EEAIUnitTargetingTable::Load(SPEC);
    case O::DbLoadTerrain: return EETerrainTable::Load(SPEC);
    case O::DbLoadTerrainType: return EETerrainTypeTable::Load(SPEC);
    case O::DbLoadCliffTerrain: return EECliffTerrainTable::Load(SPEC);
    case O::DbLoadCivPowers: return gCivPowersManager.Load(SPEC);
    case O::DbLoadCivilization: return gCivilizationManager.Load(SPEC);
    case O::DbLoadWeaponToHit: return EEToHitTable::Load(SPEC);
    case O::DbLoadCalamity: return EECalamityManager::GetInstance()->LoadCalamityClasses(SPEC);
    case O::DbLoadUnitSet: return gUnitSetManager.Load(SPEC);
    case O::DbLoadAreaEffect: return gAEManager.Load(SPEC);
    case O::DbLoadUpgrade: return EEUCUpgradeTable::Load(SPEC);
    case O::DbLoadCPBehavior: return EECPBehavior::Load(SPEC);
    case O::DbLoadGameVariant: return EEGameVariant::Load(SPEC);
    case O::DbLoadPremadeCivs: return gCivilizationManager.LoadPremadeCivilizations(SPEC);
    case O::UiInitialize: return ee_ui_initialize();
    case O::GfxEffectsSortMedia: EEGraphicEffect::SortMediaByKind();
        break;
    case O::TileSetUnitListPool: EETile::SetUnitListPoolSize((unsigned long)a[0], (unsigned long)a[1]);
        break;
    case O::TileReleaseUnitListPool: EETile::ReleaseUnitListPool();
        break;
    case O::DoppelgangerSetPool: gDoppelgangerManager.SetPoolSize((unsigned long)a[0], (unsigned long)a[1]);
        break;
    case O::DoppelgangerSetComplexPool: gDoppelgangerManager.SetComplexPoolSize((unsigned long)a[0], (unsigned long)a[1]);
        break;
    case O::DoppelgangerRelease: gDoppelgangerManager.Release();
        break;
    case O::PathManagerClear: gPathManager.Clear();
        break;
    case O::ReportLeakingUnits: EEReportLeakingUnits();
        break;
    case O::ReleaseUnitClasses: EEUnitClass::ReleaseUnitClasses();
        break;
    case O::AnimalsUnload: EEAnimal::Unload();
        break;
    case O::CalamityClassesDestroy: EECalamityManager::GetInstance()->DestroyCalamityClasses(a[0] != 0);
        break;
    case O::DatabaseObjectsClear: gDatabaseObjects.Clear();
        break;
    case O::ButtonsUnload: EEButtonObject::Unload();
        break;
    case O::MusicObjectsClear: EEMusicObject::ClearAll();
        break;
    case O::ColorTableUnload: EEColorTable::Unload();
        break;
    case O::TargetingTableUnload: EEAIUnitTargetingTable::Unload();
        break;
    case O::TerrainTableUnload: EETerrainTable::Unload();
        break;
    case O::TerrainColorsUnload: EETerrainColorTable::Unload();
        break;
    case O::TerrainTypesUnload: EETerrainTypeTable::Unload();
        break;
    case O::CliffTerrainUnload: EECliffTerrainTable::Unload();
        break;
    case O::UpgradeTableUnload: EEUCUpgradeTable::Unload();
        break;
    case O::UnitBehaviorUnload: EEUnitBehavior::Unload();
        break;
    case O::StartingResourcesUnload: EEStartingResources::Unload();
        break;
    case O::GameVariantUnload: EEGameVariant::Unload();
        break;
    case O::WorldTableClear: gWorldTable.Clear();
        break;
    case O::TechTreeUnloadAll: gTechTree.UnloadAll();
        break;
    case O::CalamityManagerDestroy: USingleton<EECalamityManager>::DestroyInstance();
        break;
    case O::DeleteResource: delete gResource;
        break;
    case O::ComplexUnitReleasePool: EEComplexUnit::ReleasePool();
        break;
    case O::NodePoolDumpStats: UList<EEComplexUnit*>::sNodePool.DumpStats(false);
        break;
    // the game objects
    case O::RandomMapInitialize: EERandomMap::Initialize(EEStartupDirectorySpec());
        break;
    case O::LobbyOptionsSetDefaults: gWorld.mLobbyOptions.SetDefaults();
        break;
    case O::WorldOptionsSetDefaults: gWorld.mGameOptions.SetDefaults();
        break;
    case O::InitPhysicsModels: EEInitPhysicsModels();
        break;
    case O::FreePhysicsModels: EEFreePhysicsModels();
        break;
    case O::NewMouse: gMouse = new EEMouse;
        break;
    case O::MouseLoad: return gMouse->Load(gRasterizer->GetScreen(SCREEN_ID));
    case O::MouseRelease: gMouse->Release();
        break;
    case O::DeleteMouse: delete std::exchange(gMouse, nullptr);
        break;
    case O::MouseSetWindowBounds: gMouse->SetBounds(GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight()));
        break;
    case O::MouseSetPositionXY: gMouse->SetPosition(GE2DPoint(f[0], f[1]));
        break;
    case O::MouseSetSensitivityPref: gMouse->SetSensitivity(SETTINGS.mMouseSensitivity);
        break;
    case O::CreateUnitClasses: EEUnitClass::CreateUnitClasses();
        break;
    case O::TechBuildUnitClassInfo: EETechTree::BuildUnitClassInfo();
        break;
    case O::NewUIManager: gUIManager = gEEGameUI.mNewManager(gRasterizer, SETTINGS.GetScreenMode() + 1, SCREEN_ID, "db\\");
        break;
    case O::DeleteUIManager: delete std::exchange(gUIManager, nullptr);
        break;
    case O::UiSetLeftHandedPref: UIManager::SetMouseLeftHanded(SETTINGS.mRightHandMouseOrientation == false);
        break;
    case O::UiSetBorderColors: UIControl::SetBorderColors(GEColor(f[0], f[1], f[2]), GEColor(f[3], f[4], f[5]));
        break;
    case O::NewArtificialIntelligence: gArtificialIntelligence = new EEArtificialIntelligence;
        break;
    case O::DeleteArtificialIntelligence: delete std::exchange(gArtificialIntelligence, nullptr);
        break;
    case O::NewCheatCodes: gCheatCodes = new EECheatCodes;
        break;
    case O::DeleteCheatCodes: delete std::exchange(gCheatCodes, nullptr);
        break;
    case O::NewGameTimer: return P64(new TMTimer(0, 0, true));
    case O::DeleteGameTimer: delete (TMTimer*)p;
        break;
    case O::ContinentsShutdown: EEAIContinentManager::Shutdown();
        break;
    case O::AeManagerClear: gAEManager.Clear();
        break;
    case O::CpOnAttackPlayer: EECPUnitsOnAttackPlayer((EEComputerPlayer*)p, (long)a[0], a[1] != 0);
        break;
    // the entry's bodies (gamecpp entry.rs)
    case O::EntrySpecFirstFile: return SPEC->GetFirstFile(*(const UString*)a[0], *(FSFileSpec*)a[1]);
    case O::EntrySpecNextFile: return SPEC->GetNextFile(*(FSFileSpec*)a[0]);
    case O::EntrySpecFileIsNot: return PlatStrICmp(SPEC->GetFile(), (const char*)a[0]) != 0;
    case O::EntrySpecVecNew: return P64(new std::vector<FSFileSpec>);
    case O::EntrySpecVecDelete: delete SPECS;
        break;
    case O::EntrySpecVecPush: SPECS->push_back(*(FSFileSpec*)a[0]);
        break;
    case O::EntrySpecVecSort: std::sort(SPECS->begin(), SPECS->end(), EEFileNameSorter());
        break;
    case O::EntrySpecVecSize: return (int64_t)SPECS->size();
    case O::EntrySpecVecGet: *(FSFileSpec*)a[1] = (*SPECS)[(size_t)a[0]];
        break;
    case O::EntrySpecOpenRead: return SPEC->Open(kFileAccessRead);
    case O::EntrySpecLength: return SPEC->GetLength(*(unsigned long*)a[0]);
    case O::EntrySpecRead: return SPEC->Read((unsigned char*)a[0], (unsigned long)a[1], *(unsigned long*)a[2]);
    case O::EntrySpecClose: SPEC->Close();
        break;
    case O::EntrySpecFullPath: *(UString*)a[0] = SPEC->GetVolume() + SPEC->GetDirectory() + SPEC->GetFile();
        break;
    case O::EntryUStrData: return P64((const char*)USTR);
    case O::EntryUStrSet: USTR = (const char*)a[0];
        break;
    case O::EntryNeInitialize: return NEInitialize(*SPEC);
    case O::EntryRegisterNeMessages: NEMessage::RegisterKnownMessages();
        break;
    case O::EntryRegisterEeMessages: EEMessage::RegisterKnownMessages();
        break;
    case O::EntryArchiveCount: return gDataPath->GetArchiveCount();
    case O::EntryArchiveTimestamp: return gDataPath->GetArchive((unsigned long)a[0]).GetTag(UWideString("Timestamp"), WSTR) == kErrorOK;
    case O::EntryStrData: return P64((const char*)WSTR);
    case O::EntryVersionTrim: gGameVersion = gGameVersion.GetIndexedSubstring(gGameVersion.Find("v", 0), gGameVersion.GetLength() - 1);
        break;
    case O::EntryVersionAppend: gGameVersion += (const char*)a[0];
        break;
    case O::RunInstanceMutex: return EECreateInstanceMutex();
    // round 12, fork D (gamecpp window_pump.rs)
    case O::C12GameWindowSize: return gGameWindow == NULL ? 0 : a[0] == 0 ? gGameWindow->GetWidth() : gGameWindow->GetHeight();
    case O::C12DebugText: ESDebugOut("%s", (const char*)a[0]);
        break;
    case O::C12WonNetCffdd: ((EEWonNet*)p)->FnCFFDD(0);
        break;
    case O::C12WonNetCfff2: ((EEWonNet*)p)->FnCFFF2();
        break;
    case O::RunComInit:
#ifdef _WIN32
        return CoInitialize(NULL);
#endif
        break;
    case O::RunComUninit:
#ifdef _WIN32
        CoUninitialize();
#endif
        break;
    case O::RunSqrtTable: UMFastMath::BuildSqrtTable();
        break;
    case O::RunSetInstance: gInstance = (HINSTANCE)a[0];
        break;
    case O::RunFsInitialize: FSInitialize("");
        break;
    case O::RunNewResource: return (gResource = new UResourceFile((HINSTANCE)a[0])) != NULL;   // the language resources are linked into the exe
    case O::RunPreferencesLoad: return gGamePreferences.Load();
    case O::RunUserInterface: return gUserInterface->Run((void*)&(const UString&)UString((const char*)a[0]));
    case O::RunReleaseDataPath: gDataPath->Dereference();
        break;
    case O::RunReleaseCivilizationPath: gCivilizationDataPath->Dereference();
        break;
    default: return EEGraphicsOp(inOp, p, a, f);
    }
    return 0;
}

// The deferred COMDATs at the end of the unit (0x14494d..0x14622a: the inline-class deleting dtors and every template
// instantiation) are compiled without /Og, while every user function is -O1: the file ends with an unmatched
// `#pragma optimize("g", off)` (VC6 compiles deferred inline/template bodies at the end of the TU with the pragma
// state in force there; probe build/scratch_exe-core/probes/p2.cpp). Evidence: with it vector<EEUnitSet*>::_Buy
// 0x54541f goes 17.65 -> exact and nothing else changes.
#pragma optimize("g", off)
