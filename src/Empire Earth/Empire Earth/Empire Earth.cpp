// C:\EEX\Empire Earth\Empire Earth\Empire Earth.cpp (__FILE__ 0x13f2ae's asserts). The game's main unit:
// 0x13e967..0x14622a (its 49 global objects' init chains first, with the inline tables' deleting dtors between them,
// then the functions). Unit flags: the exe's module set. Global names are provisional unless another unit already
// bound them.
#include <windows.h>
#include "rust/game/gamecpp.h"   // the bodies (ee_game gamecpp)
#include <vector>

#include "lle/LowLevelEngine.h"
#include "EEDataText.h"   // EEDbFindFile: the db tables' text forms
#include "platform/PlatMessage.h"
// TEST ONLY: the UI fixture (EETestFixture.cpp registers its configure step here, at static initialization: this unit
// names no symbol of the test drivers, so they stay out of its dependencies) and the headless simulation's driver
// (ee_game fixture and sim_driver, called directly)
#include "rust/game/fixture.h"
#include "rust/game/sim_driver.h"
extern void (*gEETestFixtureConfigure)();   // EEGlobals.cpp: the harness clock, seed and no-defeat switches
#include "platform/PlatSim.h"
unsigned long EEMenuWidth();    // port: the menus' resolution (below)
unsigned long EEMenuHeight();
void GXGameSize(unsigned long inPercent, bool inWindowed, unsigned long* outWidth, unsigned long* outHeight);   // gx/GX.h
#include "platform/PlatWindow.h"
#include "platform/PlatFile.h"
#include <stdio.h>
#include <string.h>
#include "platform/PlatPath.h"
#include <string>
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GETerrainMesh.h"

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
#include "lle/FSRestorable.h"
#include "lle/UResourceFile.h"
#include "lle/TMTimer.h"
#include "lle/FSPersistent.h"
#include "lle/FSPersistentList.h"
#include "ee/EEMap.h"
#include "ee/EETeam.h"
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
#include "ee/EEComputerPlayer.h"
#include "ee/EECPEngine.h"
#include "ee/EECPNextUnits.h"

#include "ee/EEGlobalStorage.h"

class GEFont;
class FSPath;
// The ones no header this unit includes declares:
extern GEFont *gFontArial8, *gFontArial9, *gU531930, *gU531934, *gU531938, *gFontArial16, *gFontArial18,
    *gFontArial20, *gFontArial24, *gFontArial36, *gFontArial48, *gFontArial72, *gFontArialBold8, *gFontArialBold9,
    *gFontArialBold10, *gFontArialBold12, *gFontArialBold14, *gFontArialBold16, *gFontTimesNewRoman9,
    *gFontTimesNewRoman10, *gFontTimesNewRoman12, *gFontTimesNewRoman14, *gFontCourier8, *gFontCourier10,
    *gFontCourier12, *gFontCourier14, *gFontTahoma8, *gU531994, *gFontTahoma10, *gFontTahoma12, *gFontTahoma14,
    *gFontTahoma16, *gFontTahoma18, *gFontTahoma20, *gFontTahoma24, *gFontTahoma36, *gFontTahoma48, *gFontTahoma72;
extern UString gU530BE0, gExecutableName, gSavedGameExtension, gScenarioExtension, gU530B88, gCampaignExtension,
    gPhysicsThreadName, gGraphicsThreadName, gDataPreLoadingThreadName, gMousePointerThreadName,
    gScenarioEditorThreadName, gUserInterfaceThreadName, gCommSendThreadName, gCommRecvThreadName,
    gCommMainThreadName, gCommLagLossThreadName, gCommRecPlayThreadName, gSoundScavengerThreadName,
    gCDAudioThreadName;
extern UWideString gU530BC8, gCampaignAOCRoman, gCampaignAOCPacific, gCampaignAOCAsian, gCampaignEEDemo,
    gCampaignEELearning, gU52A280;
extern EEEvent gEvent;
extern EEToHitTable gToHitTable;
extern EEAIUnitTargetingTable gAIUnitTargetingTable;
extern EETerrainTable gTerrainTable;
extern EETerrainColorTable gTerrainColorTable;
extern EETerrainTypeTable gTerrainTypeTable;
extern EEComputerPlayerBehaviorTable gComputerPlayerBehaviorTable;

// The game's globals (gWorld, gPhysics, gUIManager, the subsystem pointers, the save version, ...) are
// EEGlobals.cpp's, a data-only unit: there the 48 global objects are only storage (ee/EEGlobalStorage.h). They are
// constructed here, where their definitions were, in the order of the original's init chains (0x53e967..0x53f27c),
// each registering its destructor with atexit as its definition did.
// chain at 0x53e967 (wrapper)
EE_CONSTRUCT_GLOBAL(UString, gU530BE0, ("B7B226DDB7B226DDBCD9344624C93DE6C4A90DD4B7B20375"));
// chain at 0x53e999 (wrapper)
EE_CONSTRUCT_GLOBAL(UWideString, gU530BC8, );
// chain at 0x53e9c5 (wrapper)
EE_CONSTRUCT_GLOBAL(UString, gExecutableName, ("Empire Earth.exe"));
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
// chain at 0x53f11e (wrapper)
EE_CONSTRUCT_GLOBAL(UString, gUserInterfaceThreadName, ("UserIntfc"));
// chain at 0x53f150 (wrapper)
EE_CONSTRUCT_GLOBAL(UString, gCommSendThreadName, ("CommSend"));
// chain at 0x53f182 (wrapper)
EE_CONSTRUCT_GLOBAL(UString, gCommRecvThreadName, ("CommRecv"));
// chain at 0x53f1b4 (wrapper)
EE_CONSTRUCT_GLOBAL(UString, gCommMainThreadName, ("CommMain"));
// chain at 0x53f1e6 (wrapper)
EE_CONSTRUCT_GLOBAL(UString, gCommLagLossThreadName, ("CommLagLoss"));
// chain at 0x53f218 (wrapper)
EE_CONSTRUCT_GLOBAL(UString, gCommRecPlayThreadName, ("CommRecPlay"));
// chain at 0x53f24a (wrapper)
EE_CONSTRUCT_GLOBAL(UString, gSoundScavengerThreadName, ("SoundScavngr"));
// chain at 0x53f27c (wrapper)
EE_CONSTRUCT_GLOBAL(UString, gCDAudioThreadName, ("CDAudio"));
extern FSPath* gDataPath;
extern FSPath* gCivilizationDataPath;

// The unit's own data (provisional names): the start screen's window class (0x4c05e8, a char array: both
// RegisterClassEx and CreateWindowEx take its address), its bitmap, the bitmap's BITMAP and the memory DC
// (0x52a18c / 0x52a190 / 0x52a188)
#ifdef _WIN32
static char sStartScreenClassName[] = "Loading Game Window";
static HBITMAP sStartScreenBitmap;
static BITMAP sStartScreenBitmapInfo;
static HDC sStartScreenDC;
#endif
static TMTimer* sTimer;                     // 0x5319ec (EECreateGameTimer's assert names it)
extern HINSTANCE gInstance;                 // 0x5319d8 (WinMain's instance, set by 0x540d73)
volatile bool gStartScreenPainted;              // 0x5319f0 (the start screen's first WM_PAINT sets it)
char gGameWindowTitle[4];                   // 0x5319f4 the start screen's title (only use 0x13f520 pushes it; never written; size 1..4 inferred)

void EESetThreadFloatingPoint(const char* inThreadName);

// the loading splash screen ("Loading Game Window", thread "StartScreenMainThread"; the class has no virtuals: no
// RTTI; provisional names, mEndThreadEvent from the ctor's assert)
class EEStartScreen {
public:
    EEStartScreen(HINSTANCE inInstance);
    ~EEStartScreen();
    void Start();
    void Stop();
#ifdef _WIN32
    static LRESULT CALLBACK WindowProc(HWND inWindow, UINT inMessage, WPARAM inWParam, LPARAM inLParam);
#endif
    static unsigned long ThreadMain(void* inParameter);
    HINSTANCE mInstance;                    // +0x00
    TSThread* mThread;                      // +0x04
    TSEvent* mEndThreadEvent;               // +0x08
    bool mQuit;                             // +0x0c
};
// Creates the loading splash screen for the instance, with its end-of-thread event
EEStartScreen::EEStartScreen(HINSTANCE inInstance)
{
    this->mInstance = inInstance;
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mEndThreadEvent) == keseOK, 411);
    this->mQuit = false;
}

// Destroys the splash screen's end-of-thread event
EEStartScreen::~EEStartScreen()
{
    TSScheduler::DestroyEvent(this->mEndThreadEvent);
}

// In full screen only: starts the splash screen thread and waits until it has painted the splash bitmap
// (Win32 only: elsewhere there is no splash window yet; PORT.md step 4)
void EEStartScreen::Start()
{
#ifdef _WIN32
    if (gGamePreferences.mSettings.GetFullScreen() != true)
        return;
    gStartScreenPainted = false;
    TSScheduler::CreateThread(this->mThread, EEStartScreen::ThreadMain, this);
    while (gStartScreenPainted == false)
        PlatSleep(1);
#endif
}

// In full screen only: tells the splash screen thread to quit, waits for it to end and destroys it
void EEStartScreen::Stop()
{
#ifdef _WIN32
    if (gGamePreferences.mSettings.GetFullScreen() != true)
        return;
    this->mQuit = true;
    this->mEndThreadEvent->Wait(kTSWaitInfinite);
    TSScheduler::DestroyThread(this->mThread);
#endif
}

#ifdef _WIN32

// Splash window procedure: hides the cursor, skips background erases and paints the splash bitmap (flagging it painted)
LRESULT CALLBACK EEStartScreen::WindowProc(HWND inWindow, UINT inMessage, WPARAM inWParam, LPARAM inLParam)
{
    HDC theDC;
    HGDIOBJ theOldObject;
    switch (inMessage) {
    case WM_SETCURSOR:
        ShowCursor(FALSE);
    case WM_ERASEBKGND:
        return 0;
    case WM_PAINT:
        theDC = GetDC(inWindow);
        theOldObject = SelectObject(sStartScreenDC, sStartScreenBitmap);
        BitBlt(theDC, 0, 0, sStartScreenBitmapInfo.bmWidth, sStartScreenBitmapInfo.bmHeight, sStartScreenDC, 0, 0,
               SRCCOPY);
        ReleaseDC(inWindow, theDC);
        SelectObject(sStartScreenDC, theOldObject);
        gStartScreenPainted = true;
        return 0;
    }
    return DefWindowProc(inWindow, inMessage, inWParam, inLParam);
}

// Splash screen thread: loads the splash bitmap, shows it in a centred popup window and pumps messages until
// told to quit, then signals its end event and frees the bitmap
unsigned long EEStartScreen::ThreadMain(void* inParameter)
{
    EEStartScreen* theScreen = static_cast<EEStartScreen*>(inParameter);
    unsigned long theMessage;
    void* theParameter1;
    void* theParameter2;
    sStartScreenBitmap = LoadBitmap(theScreen->mInstance, MAKEINTRESOURCE(0x70));
    if (sStartScreenBitmap != NULL) {
        GetObject(sStartScreenBitmap, sizeof(BITMAP), &sStartScreenBitmapInfo);
        WNDCLASSEX theClass;
        theClass.cbSize = sizeof(WNDCLASSEX);
        theClass.style = CS_HREDRAW | CS_VREDRAW;
        theClass.lpfnWndProc = EEStartScreen::WindowProc;
        theClass.cbClsExtra = 0;
        theClass.cbWndExtra = 0;
        theClass.hInstance = theScreen->mInstance;
        theClass.hIcon = NULL;
        theClass.hCursor = LoadCursor(gInstance, MAKEINTRESOURCE(0x68));
        theClass.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
        theClass.lpszMenuName = NULL;
        theClass.lpszClassName = sStartScreenClassName;
        theClass.hIconSm = NULL;
        RegisterClassEx(&theClass);
        RECT theDesktop;
        HWND theDesktopWindow = GetDesktopWindow();
        GetWindowRect(theDesktopWindow, &theDesktop);
        theDesktop.left = UMFloatToLong((theDesktop.right - theDesktop.left) * 0.5f)
                          - UMFloatToLong(sStartScreenBitmapInfo.bmWidth * 0.5f);
        theDesktop.top = UMFloatToLong((theDesktop.bottom - theDesktop.top) * 0.5f)
                         - UMFloatToLong(sStartScreenBitmapInfo.bmHeight * 0.5f);
        HWND theWindow = CreateWindowEx(WS_EX_TOOLWINDOW, sStartScreenClassName, gGameWindowTitle,
                                        WS_POPUP | WS_VISIBLE, theDesktop.left, theDesktop.top,
                                        sStartScreenBitmapInfo.bmWidth, sStartScreenBitmapInfo.bmHeight, NULL, NULL,
                                        theScreen->mInstance, NULL);
        if (theWindow != NULL) {
            HDC theDC = GetDC(theWindow);
            sStartScreenDC = CreateCompatibleDC(theDC);
            ReleaseDC(theWindow, theDC);
            // the splash window's own message pump (TSThread::Receive no longer dispatches Win32 messages)
            do {
                MSG theWin32Message;
                theMessage = 0;
                if (PeekMessage(&theWin32Message, NULL, 0, 0, PM_REMOVE)) {
                    theMessage = theWin32Message.message;
                    TranslateMessage(&theWin32Message);
                    DispatchMessage(&theWin32Message);
                }
                EESetThreadFloatingPoint("StartScreenMainThread");
                if (theScreen->mQuit == true)
                    PostQuitMessage(0);
                PlatSleep(1);
            } while (theMessage != 0x12);
            theScreen->mEndThreadEvent->Set();
            ReleaseDC(theWindow, sStartScreenDC);
        }
        DeleteObject(sStartScreenBitmap);
    }
    return 0;
}
#endif   // _WIN32: the start screen's window

// Puts the calling thread's FPU into 53-bit precision and round-to-nearest mode (the thread name is unused)
// (every game thread calls it first, passing its name)
void EESetThreadFloatingPoint(const char* inThreadName)
{
    ee_gamecpp_startup_thread_fp();
}

// Makes every computer player at war with the given player attack it; with the flag set, the others (its allies
// and its own computer player) defend it
void EEAttackPlayers(long inPlayer, unsigned char inFlag)
{
    ee_gamecpp_startup_attack_players(inPlayer, inFlag);
}

// Sums a checksum over the directory's files matching the pattern (name order, one encrypted name skipped): file
// bytes, or write time and size when asked
#include <algorithm>
#include "lle/UEncryption.h"
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
    FSFileSpec theFile;
    std::vector<FSFileSpec> theFiles;
    UString theExcluded;
    UWideString thePath;
    unsigned long theChecksum = 0;
    if (inDirectory.GetFirstFile(inPattern, theFile) != kErrorOK)
        return theChecksum;
    UEDecryptUString(gU530BE0, theExcluded);
    do {
        thePath = theFile.GetFile();
        if (PlatStrICmp(theFile.GetFile(), theExcluded) != 0)
            theFiles.push_back(theFile);
    } while (inDirectory.GetNextFile(theFile) == kErrorOK);
    std::sort(theFiles.begin(), theFiles.end(), EEFileNameSorter());
    unsigned char* theBuffer = new unsigned char[0x800];
    for (std::vector<FSFileSpec>::iterator i = theFiles.begin(); i != theFiles.end(); i++) {
        theFile = *i;
        if ((unsigned char)inUseFileTimes != 0) {
            thePath = theFile.GetVolume();
            thePath += theFile.GetDirectory();
            thePath += theFile.GetFile();
            // the write time's high word plus the size (GetFileAttributesEx's ftLastWriteTime.dwHighDateTime,
            // nFileSizeHigh and nFileSizeLow; files stay below 4 GB)
            PlatFile* theNative = PlatFileOpen(thePath, PLAT_FILE_READ);
            if (theNative != NULL) {
                uint64_t theTime = 0;
                PlatFileGetWriteTime(theNative, &theTime);
                theChecksum += (unsigned long)(theTime >> 32) + PlatFileGetSize(theNative);
                PlatFileClose(theNative);
            }
        } else {
            // Fix: a file that cannot be opened is skipped, and a read that returns no data (an error, or the file
            // shrank) ends the loop; both used to loop forever.
            if (theFile.Open(kFileAccessRead) != kErrorOK)
                continue;
            unsigned long theLength = 0;
            theFile.GetLength(theLength);
            unsigned long theRead;
            while (theLength > 0) {
                unsigned long theAmount = min(theLength, 0x800UL);
                theRead = 0;
                theFile.Read(theBuffer, theAmount, theRead);
                if (theRead == 0 || theRead > theAmount)
                    break;
                for (long j = 0; j < (long)theRead; j++)
                    theChecksum += theBuffer[j];
                theLength -= theRead;
            }
            theFile.Close();
        }
    }
    delete[] theBuffer;
    return theChecksum;
}

// Reads the five campaign checksums from string resources 0x32..0x34, 0x37, 0x36; a missing string keeps the value
void EELoadNetworkSettings()
{
    ee_gamecpp_startup_network_settings();
}

#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/NEProtocol.h"
#include "lle/NEMessage.h"
#include "ee/EEMessage.h"

void EEEmptyShutdown(bool inShutdown);

#include "GEWindow.h"
#include "lle/GEFont.h"
#include "lle/UIFontTable.h"
#include "ee/EERenderObject.h"
#include "ee/EERenderLists.h"
#include "ee/EEROTerrain.h"
#include "ee/EEROGroundShadows.h"
#include "ee/EEROUnits.h"
#include "ee/EEROWater.h"
#include "ee/EEROEffects.h"

void EESetVideoMode(unsigned long, unsigned long, unsigned long, bool);
void EEGraphicsShutdown(bool inShutdown); // registered as a shutdown routine (0x543ece)
// the game view and water ripple constants (.rdata 0x442090..0x4420a8, read from memory by both graphics initializers)
static const float kStartViewLeft = 0.0f;
static const float kStartViewRight = 1600.0f;
static const float kStartViewTop = 35.0f;
static const float kStartViewBottom = 1085.0f;
static const float kWaterRipplePeriod = 3.1415927f;
static const float kWaterRippleSpeed = 0.025f;
static const float kWaterRippleAmplitude = 0.05f;
#include "lle/Input System.h"
#include "ee/EEUserInterface.h"

void EEReleaseInstanceMutex(bool inShutdown); // a shutdown routine; the argument is unused
bool EEInitializeDisplay();
void EEInitializeGraphics();
bool EEInitializeSounds(); // 0x543557; EEInitializePart1 tests its result in AL
void EEInitializeNetwork();
void EEInitializeDatabase();
void EEInitializeGameObjects();

// Starts the engine (input, threads, timers), opens the display and initializes graphics, sound, network, the
// database, game objects and the user interface; returns false (after a message box) when sound fails
bool EEInitializePart1()
{
    return ee_gamecpp_startup_part1();
}

#include "lle/GERhombus.h"
#include "ee/EEMouse.h"
#include "ee/RTSMousePointer.h"
#include "ee/EEDataPreLoading.h"
#include "ee/EEPhysics.h"

// New (R5a.6): the RTS layer's way to the input mouse (ee/RTSMouse.h), without the game's mouse header
RTSMouse* RTSGetInputMouse() { return gMouse; }

// The three threads' stop members (declared in their headers; their COMDATs 0x140999, 0x144488, 0x1444ac are this
// unit's; the same body as RTSGraphics::Shutdown)
inline void RTSMousePointer::Shutdown()
{
    this->mThread->Post(kThreadMessageQuit, NULL, NULL, true);
    this->mThreadDone->Wait(kTSWaitInfinite);
}

inline void RTSPhysics::Shutdown()
{
    this->mThread->Post(kThreadMessageQuit, NULL, NULL, true);
    this->mThreadDone->Wait(kTSWaitInfinite);
}

inline void EEDataPreLoading::Shutdown()
{
    this->mThread->Post(kThreadMessageQuit, NULL, NULL, true);
    this->mThreadDone->Wait(kTSWaitInfinite);
}
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"

void EEShutdownThreads(bool inShutdown); // registered as a shutdown routine (0x544324)

#include <list>
#include "lle/NESynchronousCommunications.h"
#include "ee/EECommunications.h"

#include <typeinfo>
#include "ee/EEUnit.h"

// Creates the texture and model managers, activates the screen at 640x480, sets the default render states,
// creates the render objects and every UI font, and registers the graphics shutdown
void EEInitializeGraphics()
{
    ee_gamecpp_startup_graphics();
}

// Starts the network engine, registers the message types and checksums the engine DLLs, databases, AI scripts,
// models and archive timestamps; appends the checksum's low 13 bits to the game version
void EEInitializeNetwork()
{
    FSFileSpec theFile;
    FSFileSpec theDirectory;
    UString theUnused;
    UWideString theVersion(gGameVersion);
    UWideString theTag;
    UWideString theSuffix;
    long theTimestamp = 0;
    theDirectory.SetDirectory(gStartupDirectory);
    long theResult = NEInitialize(theDirectory);

    ESVERIFY_AT(theResult == keseOK, 1578);
    NEMessage::RegisterKnownMessages();
    EEMessage::RegisterKnownMessages();
    theResult = URegisterShutdown(EEEmptyShutdown);

    ESVERIFY_AT(theResult == keseOK, 1586);
    theFile.SetVolume(gStartupVolume);
    TMGetTime();
    theFile.SetDirectory(gStartupDirectory);
    // the game's code is all in its exe (the original summed the engine, Dinkum and TCP Protocol DLLs)
    char theExeName[PLAT_MAX_PATH];
    if (!PlatPathGetExecutableName(theExeName, sizeof(theExeName)))
        theExeName[0] = 0;
    uint32_t theChecksum = EEGetFileChecksum(theFile, theExeName, 0);   // a 32-bit sum on every data model (LP64: the server compares it with the request's)
    theFile.SetDirectory(gStartupDirectory + "Data\\Db\\");
    // the db tables, which the game reads only as text (EEDbFindFile; the original summed the .dat files here)
    theChecksum += EEGetFileChecksum(theFile, "*.csv", 0);
    theChecksum += EEGetFileChecksum(theFile, "*.yaml", 0);
    theFile.SetDirectory(gStartupDirectory + "Data\\Unit AI Scripts\\");
    theChecksum += EEGetFileChecksum(theFile, "*.tai", 0);
    theFile.SetDirectory(gStartupDirectory + "Data\\Models\\");
    // the models (glTF only since D9: GEFindModelFile)
    theChecksum += EEGetFileChecksum(theFile, "*.glb", 0);
    theChecksum += EEGetFileChecksum(theFile, "*.gltf", 0);
    theChecksum += EEGetFileChecksum(theFile, "*.bin", 0);
    // port: the computer players' AIs (Data\ai: the manifest ais.json and every Lua script; ee/EECPEngine.h), so a
    // joiner whose AI folder differs from the host's is refused like any other data mismatch (kConnectionBadChecksum)
    theChecksum += EECPAIFolderHash();
    for (unsigned long i = 0; i < gDataPath->GetArchiveCount(); i++) {
        bool theHaveTimestamp = gDataPath->GetArchive(i).GetTag(UWideString("Timestamp"), theTag) == kErrorOK;
        if (theHaveTimestamp) {
            sscanf(theTag, "%ld", &theTimestamp);
            theChecksum += theTimestamp;
        }
    }
    TMGetTime();
    gBinaryChecksum = theChecksum;
    gGameVersion = theVersion.GetIndexedSubstring(theVersion.Find("v", 0), theVersion.GetLength() - 1);
    sprintf(theSuffix, ".%04ld", (long)(gBinaryChecksum & 0x1fff));
    gGameVersion += theSuffix;
}

// Registers the saved game's persistent sections (checksum, map, scenario, players, world, effects, ...)
// (called from the start-up 0x540d73)
void EERegisterPersistents()
{
    ee_gamecpp_startup_persistents();
}

// Creates the game timer
void EECreateGameTimer()
{
    sTimer = new TMTimer(0, 0, true);
    ESVERIFY_AT(sTimer != NULL, 1876);
}


void CreatePlaneModels(); // actual cdecl effect-media reset, defined in EEEffect.cpp

// Loads the status box icons, mini map, effect, unit, sky and ground shadow media
void EELoadGameMedia()
{
    ee_gamecpp_startup_load_media();
}



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
static const float kGameViewScaleX = 0.000625f;
static const float kGameViewScaleY = 0.00083333335f;

// Sets the viewport to the rectangle (given in 1600x1200 layout units, scaled to the window unless the camera is
// free) and stops camera following
void EESetGameViewRectangle(const GERectangle& inRectangle)
{
    ee_gamecpp_startup_game_view(reinterpret_cast<const EeGeRectangle*>(&inRectangle));
}


// Creates the music manager, mouse pointer, graphics, data preloading and physics threads; registers their shutdown
void EEInitializeThreads()
{
    ee_gamecpp_startup_threads();
}

// Switches the screen to the mode of that size and depth (optionally the lowest refresh above 60 Hz), resizes
// the window and clears both surfaces; does nothing when the mode is already active
void EESetVideoMode(unsigned long inWidth, unsigned long inHeight, unsigned long inBitCount, bool inPreferRefresh)
{
    ee_gamecpp_startup_video_mode(inWidth, inHeight, inBitCount, inPreferRefresh);
}

// Stops the music, ambient sounds (disabling them) and every one-shot sound
// (EEWorld::Clear and the main game form call it)
void EEStopAllSounds()
{
    ee_gamecpp_startup_stop_sounds();
}

void ReleasePlaneModels(); // effect media cleanup, defined in EEEffect.cpp

// Releases the status box icons, mini map, effect, unit, sky and ground shadow media
void EEUnloadGameMedia()
{
    ee_gamecpp_startup_unload_media();
}

// Points the texture, model and sound managers at the data search path
void EESetDataPaths()
{
    gTextureManager->SetTexturePath(gDataPath);
    gModelManager->SetModelPath(gDataPath);
    gSoundManager->SetPath(gDataPath);
}


// Ends the current game: pauses graphics and physics, releases the scene, deletes the communications, and
// clears the world and game state under the world lock
void EEClearGame()
{
    ee_gamecpp_startup_clear_game();
}

// Creates the game threads and registers the saved game's persistent sections
void EEInitializePart2()
{
    ee_gamecpp_startup_part2();
}

// Sizes the memory pools for tile unit lists, sounds, doppelgangers and 2D/3D particles
void EEInitGamePools()
{
    ee_gamecpp_startup_game_pools();
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

void EEChangeGameScreen(unsigned long inWidth, unsigned long inHeight, bool inFullScreen); // defined below

// Switches to the menus' screen after start-up. The original played the Sierra, Mad Doc and Empire Earth intro movies
// here first (unless the command line hosted or joined a game); the port has no movie player and plays none.
void EEStartMenuScreen()
{
    ee_gamecpp_startup_menu_screen();
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
void DeleteEffectBatchBuffers(); // effect-render cleanup, defined in EEEffect.cpp
static unsigned long sOriginalVideoMode; // 0x52a1a8: the video mode index found at start-up, restored at shutdown

// Releases models, textures and fonts, restores the start-up video mode, destroys the game window and deletes
// the render objects and the texture and model managers
void EEGraphicsShutdown(bool inShutdown)
{
    ee_gamecpp_startup_graphics_shutdown();
}

#include "ee/EEMiniMap.h"

// Releases the particle and complex unit pools and logs the list node pools' statistics
// (EEDeleteDatabase passes true; the argument is not read)
void EEReleaseGamePools(bool inShutdown)
{
    EEComplexUnit::ReleasePool();
    UList<EEComplexUnit*>::sNodePool.DumpStats(false);
}

// Releases the tile unit list and doppelganger pools (the attack target pool went with the attack sub groups, stage 5)
void EEReleaseUnitPools()
{
    EETile::ReleaseUnitListPool();
    gDoppelgangerManager.Release();
}

// Does nothing (the network start-up registers it as its shutdown routine)
void EEEmptyShutdown(bool inShutdown)
{
}

// Deletes the sound manager, logs its memory pool and frees the pool and its locks
// (no direct caller: registered as a shutdown routine)
void EEDeleteSoundManager(bool inShutdown)
{
    ee_gamecpp_startup_delete_sound_manager();
}

#include "ee/EEWonNet.h"
#include "ee/EEUnitPicker.h"

// Stops physics, deletes the communications and WON connection, then shuts down and deletes the physics, data
// preloading, graphics and mouse pointer threads, the music manager and the user interface
void EEShutdownThreads(bool inShutdown)
{
    ee_gamecpp_startup_shutdown_threads();
}

#include "ee/EEButtonObject.h"

void CreateEffectBatchBuffers(); // effect reset, defined in EEEffect.cpp
extern bool sIconsLoaded; // the status box's icons-loaded flag (defined in EEUIFMGStatusBox's unit)

// Re-creates the display at the new size (full screen or not): reactivates the rasterizer, resets render states
// and the camera, reloads mouse, textures, buttons and icons and restarts the mouse pointer thread
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
    if (gGameWindowHeight <= 768 || gGameWindowHeight == 0)
        return gGameWindowWidth;
    unsigned long theWidth = (gGameWindowWidth * 768 + gGameWindowHeight / 2) / gGameWindowHeight;
    theWidth &= ~1UL;
    return theWidth < 1024 ? 1024 : theWidth;   // at least the original's width (a 5:4 screen: 1024x768)
}

void EEChangeGameScreen(unsigned long inWidth, unsigned long inHeight, bool inFullScreen)
{
    ee_gamecpp_startup_change_screen(inWidth, inHeight, inFullScreen);
}

#include "lle/Graphics Engine.h"

void EEEnumerateDisplayDevices();
void CreateEffectBatchBuffers(); // effect reset, defined in EEEffect.cpp

// Starts the graphics engine, picks the preferred rasterizer, validates screen, depths and resolution (falling
// back to 800x600x16), creates and captures the game window; returns true
bool EEInitializeDisplay()
{
    return ee_gamecpp_startup_display();
}

#ifdef _WIN32
#include <objbase.h>
#endif

bool EEParseCommandLine(const char* inCommandLine); // 0x52d703 (after start-up)
HINSTANCE gInstance;
#pragma optimize("g", off)   // rule 4, owner 2026-11-01: WinMain is no-/Og (memory locals, constant push 1/pop eax test)
// Runs the game: single-instance check, start-up, splash and intro, the user interface loop, then shutdown;
// returns 1 when the original Empire Earth install is missing, else 0
// (the previous instance and show command arguments are unused)
// EESimPreferences (TEST ONLY; was EESim.cpp's): the settings the simulation runs with, over config.yaml (EERun,
// after gGamePreferences.Load): a window (hidden: DX7Screen::CreateAWindow), the harness's pins (32-bit, Min = Max PTs
// per Second), no autosave, no music
static void EESimPreferences()
{
    long thePTRate = 40, theAutoSave = 0;
    ee_sim_driver_preferences(&thePTRate, &theAutoSave);
    gGamePreferences.mSettings.mFullScreen = false;
    gGamePreferences.mSettings.mGameBitDepth = 32;
    gGamePreferences.mSettings.mTextureBitDepth = 32;
    gGamePreferences.mSettings.mGameWindowWidth = 800;
    gGamePreferences.mSettings.mGameWindowHeight = 600;
    gGamePreferences.mSettings.mMinPTsPerSecond = thePTRate;
    gGamePreferences.mSettings.mMaxPTsPerSecond = thePTRate;
    gGamePreferences.mSettings.mAutoSaveInMilliseconds = (unsigned long)theAutoSave;
    gGamePreferences.mSettings.mMusicEnabled = false;
    gGamePreferences.mSettings.mWaitForVSync = false;
}

// (They were EEUserInterface.cpp's: tools/cpp_deps.py, PORT_LEARNINGS "Cycle cuts, round 3")
// Receive's window event pump on the window thread (TSThread::SetEventPump): one event per call
static bool EEPumpWindowEvent()
{
    PlatInputEvent theEvent;
    if (!PlatWindowPollEvent(&theEvent))
        return false;
    if (theEvent.mType == PLAT_EVENT_WAKE)   // the WON lobby's wake-up (PlatWindowWake) while it is not running
        return true;
    static const bool sLog = getenv("EE_WINDOW_EVENT_LOG") != NULL;   // opt-in: every event as a debug string
    if (sLog)
        ESDebugOut("EE-WINDOW-EVENT %u (key %u/%u, text '%s') ui %d window %lux%lu\n", (unsigned)theEvent.mType,
                   (unsigned)theEvent.mKey.mScanCode, (unsigned)theEvent.mKey.mVirtualKey,
                   theEvent.mType == PLAT_EVENT_TEXT ? theEvent.mText.mText : "", gUserInterface != NULL,
                   gGameWindow ? gGameWindow->GetWidth() : 0, gGameWindow ? gGameWindow->GetHeight() : 0);
    ISHandleEvent(theEvent);   // the Input System's keyboard and mouse (DirectInput's devices before)
    EEUserInterface::HandleEvent(theEvent);
    return true;
}

#ifdef _WIN32
// The Win32 messages that are not window events (PlatWindowSetNativeMessageHook): the WON lobby's two messages,
// handled as the window procedure did; nonzero drops the message. (CD audio's MCI notifications went to SECD, which
// is gone: the port never opened a CD drive.)
static int EENativeMessageHook(void* inWindow, uint32_t inMessage, uint64_t inWParam, int64_t inLParam)
{
    switch (inMessage) {
    case 0x433:
        PostMessageA((HWND)inWindow, 0x434, 0, 0);
        if (gWonNet != NULL)
            gWonNet->FnCFFDD(0);
        return 1;
    case 0x434:
        if (gWonNet != NULL)
            gWonNet->FnCFFF2();
        return 1;
    }
    return 0;
}
#endif

void EERustInit();   // EERust.cpp

static int EERun(HINSTANCE inInstance, char* inCommandLine)
{
    EERustInit();   // first: a panic anywhere after this reaches a log
    if (gEETestFixtureConfigure != NULL)
        gEETestFixtureConfigure();   // TEST ONLY: nothing unless EE_TEST_UI is set (before any clock or thread starts)
    long theResult;
    bool theInitialized;
    EEStartScreen theSplashScreen(inInstance);
    if (gPlatSimMode || EECreateInstanceMutex()) {   // the simulation: any number of copies at once
        // the window system (platform/PlatWindow.h): the window class "Empire Earth Expansion" with the game's icon,
        // its events pumped by the UI thread's TSThread::Receive (this thread: EEUserInterface::Run runs on it)
        PlatWindowSetAppName("Empire Earth Expansion", 0x66);
        if (!PlatWindowSystemStart()) {
            PlatMessageShow("Empire Earth", PlatWindowLastError(), PLAT_MESSAGE_ERROR);
            return 1;
        }
        TSThread::SetEventPump(EEPumpWindowEvent, PlatCurrentThreadId());
#ifdef _WIN32
        PlatWindowSetNativeMessageHook(EENativeMessageHook);
#endif
#ifdef _WIN32
        HRESULT theHResult = CoInitialize(NULL);

        ESVERIFY_AT(theHResult == S_OK || theHResult == S_FALSE, 3600);
#endif
        UMSetFloatingPointPrecision(kFPPrecision53);
        UMFastMath::BuildSqrtTable();
        {   // (the original's first command-line pass here only chose whether to play the intro movies)
            gInstance = inInstance;
            ESInitialize(inInstance);
            // the game's directory is the data root (the executable's directory, or EE_DATA_ROOT), wherever it was
            // started from: its Data, Users and config.yaml are found relative to it (the original used the current
            // directory)
            PlatPathEnterDataRoot();
            FSInitialize("");
            gResource = new UResourceFile(inInstance);   // the language resources are linked into the exe

            ESVERIFY_AT(gResource, 3632);
            EEInitializeDefaultSettings();
            theResult = gGamePreferences.Load();

            ESVERIFY_AT(theResult == keseOK, 3637);
            if (gPlatSimMode)
                EESimPreferences();
            EELoadNetworkSettings();
            EEShowSinglePlayerScreen();
            // the original tests the constant 1 here (push 1; pop eax; test eax,eax; je, no-/Og)
            if (1) {
                if (!gPlatSimMode)   // the simulation shows no splash window
                    theSplashScreen.Start();
                theInitialized = EEInitializePart1();
                if (!gPlatSimMode)
                    theSplashScreen.Stop();
                if (theInitialized) {
                    EEStartMenuScreen();
                    EEInitializePart2();
                    ee_fixture_start();   // TEST ONLY: the UI fixture's thread, only when EE_TEST_UI is set
                    if (gPlatSimMode)
                        ee_sim_driver_start();   // TEST ONLY: the simulation driver's thread (was EESimStart)
                    if (EEParseCommandLine(inCommandLine)) {
                        theResult = gUserInterface->Run((void*)&(const UString&)inCommandLine);
                        theInitialized = theResult == kErrorOK;
                        EEClearGame();
                        UShutdown(theInitialized, false);
                        gDataPath->Dereference();
                        gCivilizationDataPath->Dereference();
                    }
                } else {
                    UShutdown(theInitialized, false);
                    gDataPath->Dereference();
                    gCivilizationDataPath->Dereference();
                }
            }
        }
#ifdef _WIN32
        CoUninitialize();
#endif
        PlatWindowSystemStop();
    }
    return 0;
}

#ifdef _WIN32
// the Windows entry point (the previous instance and show command arguments are unused)
int WINAPI WinMain(HINSTANCE inInstance, HINSTANCE inPreviousInstance, LPSTR inCommandLine, int inShowCommand)
{
    return EERun(inInstance, inCommandLine);
}
#else
// The POSIX entry point. The game's data is the data root (EE_DATA_ROOT, else the executable's directory, a macOS app
// bundle's Contents/Resources: platform/PlatPath.h), which becomes the current directory; the arguments are joined into
// the command line WinMain gets. --smoke-test checks what needs no game data (CI): the window system, a window, the
// renderer's wgpu device and presented frames, then reports whether the data root holds the game's data.
#include "gx/GX.h"

static GXSurface* EESmokeSurface(GXDirectDraw* inDirectDraw, unsigned long inCaps, unsigned long inWidth,
                                 unsigned long inHeight)
{
    GXSURFACEDESC theDescription = {};
    theDescription.dwSize = sizeof(theDescription);
    theDescription.dwFlags = GXSD_CAPS | GXSD_WIDTH | GXSD_HEIGHT | GXSD_PIXELFORMAT;
    theDescription.dwWidth = inWidth;
    theDescription.dwHeight = inHeight;
    theDescription.ddsCaps.dwCaps = inCaps;
    GXPIXELFORMAT& theFormat = theDescription.ddpfPixelFormat;
    theFormat.dwSize = sizeof(theFormat);
    theFormat.dwFlags = GXPF_RGB;
    theFormat.dwRGBBitCount = 32;
    theFormat.dwRBitMask = 0xff0000;
    theFormat.dwGBitMask = 0xff00;
    theFormat.dwBBitMask = 0xff;
    GXSurface* theSurface = NULL;
    if (inDirectDraw->CreateSurface(&theDescription, &theSurface, NULL) != GX_OK)
        return NULL;
    return theSurface;
}

// Whether the data root holds the game (its Data folder with the db tables) -> nonzero; the root into outRoot
static int EEDataPresent(char* outRoot, uint32_t inSize)
{
    if (!PlatPathGetDataRoot(outRoot, inSize))
        return 0;
    std::string theDb = std::string(outRoot) + "Data\\Db";
    uint32_t theAttributes = PlatPathGetAttributes(theDb.c_str());
    return theAttributes != PLAT_ATTR_INVALID && (theAttributes & PLAT_ATTR_DIRECTORY) != 0;
}

static void EEReportMissingData(const char* inRoot)
{
    char theText[1024];
    snprintf(theText, sizeof(theText),
             "Empire Earth: The Art of Conquest: game data not found in %s (no Data\\Db folder there).\n"
             "Copy the installed game (its Data and Users folders) there, or point EE_DATA_ROOT at a copy of it: "
             "see README.md, 'Linux and macOS'.", inRoot && *inRoot ? inRoot : "the data root");
    PlatMessageShow("Empire Earth", theText, PLAT_MESSAGE_ERROR);
}

// -> 0: the window, the wgpu device and 30 presented frames worked (the data root is reported); 2: no display or no
// GPU adapter; 1: a check failed
static int EESmokeTest()
{
    char theRoot[PLAT_MAX_PATH] = "";
    int theData = EEDataPresent(theRoot, sizeof(theRoot));
    PlatWindowSetAppName("Empire Earth Expansion", 0);
    if (!PlatWindowSystemStart()) {
        fprintf(stderr, "smoke test: no window system: %s\n", PlatWindowLastError());
        return 2;
    }
    PlatWindow* theWindow = PlatWindowCreate("Empire Earth: The Art of Conquest (smoke test)", 640, 480, 0);
    if (theWindow == NULL) {
        fprintf(stderr, "smoke test: no window: %s\n", PlatWindowLastError());
        return 2;
    }
    GXDirectDraw* theDirectDraw = NULL;
    if (GXDirectDrawCreate(NULL, &theDirectDraw) != GX_OK) {
        fprintf(stderr, "smoke test: no wgpu device (no GPU adapter)\n");
        return 2;
    }
    theDirectDraw->SetCooperativeLevel(theWindow, GXSCL_NORMAL);
    GXSurface* thePrimary = EESmokeSurface(theDirectDraw, GXSCAPS_PRIMARYSURFACE, 0, 0);
    GXSurface* theBack = EESmokeSurface(theDirectDraw, GXSCAPS_OFFSCREENPLAIN | GXSCAPS_3DDEVICE, 640, 480);
    if (thePrimary == NULL || theBack == NULL) {
        fprintf(stderr, "smoke test: FAIL surfaces\n");
        return 1;
    }
    int theFailed = 0;
    for (int theFrame = 0; theFrame < 30; theFrame++) {
        GXBLTFX theFill = {sizeof(GXBLTFX), (unsigned long)(0x00102030 * (theFrame % 8))};
        theBack->Blt(NULL, NULL, NULL, GXBLT_COLORFILL, &theFill);
        GXSURFACEDESC theLocked = {};
        theLocked.dwSize = sizeof(theLocked);
        RECT theAll = {0, 0, 640, 480};
        if (theBack->Lock(NULL, &theLocked, 0, NULL) == GX_OK) {
            uint32_t thePixel = *(const uint32_t*)((const unsigned char*)theLocked.lpSurface + 10 * theLocked.lPitch + 40);
            theBack->Unlock(NULL);
            if ((thePixel & 0xffffff) != (theFill.dwFillColor & 0xffffff))
                theFailed = 1;
        } else
            theFailed = 1;
        thePrimary->Blt(&theAll, theBack, NULL, 0, NULL);   // presents into the window
        PlatInputEvent theEvent;
        while (PlatWindowPollEvent(&theEvent))
            ;
        PlatSleep(16);
    }
    theBack->Release();
    thePrimary->Release();
    theDirectDraw->Release();
    PlatWindowDestroy(theWindow);
    printf("smoke test: window, wgpu device, 30 frames presented and read back%s\n", theFailed ? ": FAIL" : "");
    if (theData)
        printf("smoke test: the game's data is in %s\n", theRoot);
    else
        printf("smoke test: game data not found in %s (EE_DATA_ROOT; README.md 'Linux and macOS')\n",
               *theRoot ? theRoot : "the data root");
    PlatWindowSystemStop();
    return theFailed ? 1 : 0;
}

int EESimMain(int argc, char** argv);   // EESim.cpp (TEST ONLY): the headless simulation, "ee-aoc --sim ..."
void EESimGameEnded(int inResult);

int main(int argc, char** argv)
{
    std::string theCommandLine;
    // the simulation: "ee-aoc --sim ...", or the exe started as ee-sim (the link tools/xbuild.py game makes beside it)
    const char* theName = argc > 0 && argv[0] ? argv[0] : "";
    theName = strrchr(theName, '/') ? strrchr(theName, '/') + 1 : theName;
    bool theSimFlag = argc > 1 && !strcmp(argv[1], "--sim");
    if (theSimFlag || !strncmp(theName, "ee-sim", 6)) {
        int theSkip = theSimFlag ? 1 : 0;
        int theResult = EESimMain(argc - theSkip, argv + theSkip);
        if (theResult != -1)
            return theResult;
        argc = 1;   // the simulation's own options are not the game's command line
    }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--smoke-test"))
            return EESmokeTest();
        if (i > 1)
            theCommandLine += ' ';
        theCommandLine += argv[i];
    }
    char theRoot[PLAT_MAX_PATH] = "";
    if (!EEDataPresent(theRoot, sizeof(theRoot))) {
        EEReportMissingData(theRoot);
        return 3;
    }
    if (!PlatPathEnterDataRoot()) {
        PlatMessageShow("Empire Earth", "The game's data directory could not be entered (EE_DATA_ROOT).",
                        PLAT_MESSAGE_ERROR);
        return 1;
    }
    std::vector<char> theBuffer(theCommandLine.begin(), theCommandLine.end());
    theBuffer.push_back(0);
    int theResult = EERun(NULL, &theBuffer[0]);
    if (gPlatSimMode)
        EESimGameEnded(theResult);   // (does not return)
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

#include "ee/EEDbObject.h"
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

void EEDeleteDatabase(bool inShutdown); // Original registered callback 0x143d0c.
void EEDeleteGameObjects(bool inShutdown);
long EEFn0FCA98(); // 0x4fca98: a post-load check that returns 0
long EEClearRandomMapGenerators(); // clears the random-map generator list; returns 0

// Starts the sound engine and music conductor from the preferences and creates the sound manager; returns false
// if the sound engine fails, else true
bool EEInitializeSounds()
{
    return ee_gamecpp_startup_sounds();
}

// Loads every database table in start-up order, creates the user folders (campaigns, saved games, scenarios,
// users), loads the ambient sounds, sizes the game pools and registers the database shutdown
void EEInitializeDatabase()
{
    ee_gamecpp_startup_database();
}

// Sets up random maps, game options, physics models and sight rings, creates the mouse, unit classes, UI
// manager, AI, cheat codes and game timer, and registers their shutdown
void EEInitializeGameObjects()
{
    ee_gamecpp_startup_game_objects();
}

// Deletes the game timer, AI, cheat codes, UI manager, mouse and mini map and clears the world and game state
void EEDeleteGameObjects(bool inShutdown)
{
    ee_gamecpp_startup_delete_game_objects();
}


// Cleanup routines defined in other units
void EEReportLeakingUnits();
void EEDeleteMediaTable();

// Unloads every database table and its media, deletes the calamity manager and language resources, frees the pools
void EEDeleteDatabase(bool inShutdown)
{
    ee_gamecpp_startup_delete_database(inShutdown);
}

// ------------------------------------------------------------------------- the start-up's C++ (gamecpp startup.rs)

#include <utility>

int64_t EEGraphicsOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
int64_t EEStartupOpNext(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EEStartupOpNextF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

// the constants startup.rs passes
static_assert(IDS_ARIAL == 1 && IDS_TIMES_NEW_ROMAN == 2 && IDS_COURIER_NEW == 3 && IDS_TAHOMA == 4 && IDS_CIVILIZATIONS == 29783 && IDS_EMPIRE_EARTH_ART_OF_CONQUEST_REQUIRES_SOUND == 5865 && IDS_ERROR_CANT_RUN_16EA == 5866 && kSoundHotKeyAlarm == 0x3c2 && kPixelFormatZPrefer16 == 12, "gamecpp startup.rs");
static_assert(sizeof(GEVideoMode) == 5 * sizeof(unsigned long) && sizeof(GE2DPoint) == 2 * sizeof(float) && sizeof(GERectangle) == sizeof(EeGeRectangle), "gamecpp startup.rs");

// the game's fonts in creation order (startup.rs FONTS, index for index)
static GEFont** const kFonts[38] = {
    &gFontArial8, &gFontArial9, &gU531930, &gU531934, &gU531938, &gFontArial16, &gFontArial18, &gFontArial20,
    &gFontArial24, &gFontArial36, &gFontArial48, &gFontArial72, &gFontArialBold8, &gFontArialBold9, &gFontArialBold10,
    &gFontArialBold12, &gFontArialBold14, &gFontArialBold16, &gFontTimesNewRoman9, &gFontTimesNewRoman10,
    &gFontTimesNewRoman12, &gFontTimesNewRoman14, &gFontCourier8, &gFontCourier10, &gFontCourier12, &gFontCourier14,
    &gFontTahoma8, &gU531994, &gFontTahoma10, &gFontTahoma12, &gFontTahoma14, &gFontTahoma16, &gFontTahoma18,
    &gFontTahoma20, &gFontTahoma24, &gFontTahoma36, &gFontTahoma48, &gFontTahoma72};
// the campaign checksums EELoadNetworkSettings reads (string resources 0x32..0x34, 0x37, 0x36)
static long* const kCampaignChecksums[5] = {&gAOCRomanChecksum, &gAOCPacificChecksum, &gAOCAsianChecksum, &gEEDemoChecksum, &gEELearningChecksum};
// the db tables' names in start-up order (startup.rs TABLES_1..3)
static const UString* const kDbTableNames[26] = {
    &kDbStartingResourcesFile, &kDbUnitBehaviorFile, &kDbSoundsFile, &kDbGraphicsFile, &kDbGfxEffectsFile,
    &kDbMusicFile, &kDbWorldFile, &kDbFamilyFile, &kDbButtonsFile, &kDbTechTreeFile, &kDbAnimalsFile,
    &kDbColorTableFile, &kDbAIUnitTargetingFile, &kDbTerrainFile, &kDbTerrainTypeFile, &kDbCliffTerrainFile,
    &kDbCivPowersFile, &kDbCivilizationFile, &kDbWeaponToHitFile, &kDbCalamityFile, &kDbUnitSetFile,
    &kDbAreaEffectFile, &kDbUpgradeFile, &kDbCPBehaviorFile, &kDbGameVariantFile, &kDbPremadeCivsFile};
// the start-up directory as a file spec (the engines' initialisations take one)
static FSFileSpec& EEStartupDirectorySpec()
{
    static FSFileSpec theDirectory;
    theDirectory.SetDirectory(gStartupDirectory);
    return theDirectory;
}
// the start-up's user folders (startup.rs FileSpecSetDirStartupSub): data, its campaigns, saved games and scenarios,
// Users and its default
static const char* const kStartupFolders[6] = {"data\\", "data\\Campaigns\\", "data\\Saved Games\\", "data\\Scenarios\\", "Users\\", "Users\\default\\"};

#define P64(x) ((int64_t)(intptr_t)(x))
#define WSTR (*(UWideString*)p)
#define USTR (*(UString*)p)
#define SPEC ((FSFileSpec*)p)
#define SETTINGS gGamePreferences.mSettings
#define SCREEN_ID gGamePreferences.mSettings.GetScreenID()
static GEColor sStartupBlack(0.0f, 0.0f, 0.0f);   // the surfaces' clear colour

// The start-up's C++ calls and members (EeGameCpp's start-up section; EEMessage.cpp's EEMessageOp default): one
// statement each
int64_t EEStartupOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    // the floating point, the engines
    case O::FpPrecision: return UMGetFloatingPointPrecision();
    case O::FpPrecision53: return kFPPrecision53;
    case O::FpSetPrecision53: UMSetFloatingPointPrecision(kFPPrecision53);
        break;
    case O::FpSetPrecision: UMSetFloatingPointPrecision((unsigned long)a[0]);
        break;
    case O::FpRounding: return UMGetFloatingPointRoundingMode();
    case O::FpSetRoundingNearest: UMSetFloatingPointRoundingMode(0);
        break;
    case O::TsInitialize: return TSInitialize();
    case O::TmInitialize: return TMInitialize();
    case O::IsInitialize: return ISInitialize(gInstance, gGameWindow, gResource, 100, 250);
    case O::GeInitialize: return GEInitialize(EEStartupDirectorySpec(), gInstance);
    case O::SeInitialize: return SEInitialize(EEStartupDirectorySpec(), gGameWindow);
    case O::InitializeNetwork: EEInitializeNetwork();
        break;
    case O::RegisterShutdownMutex: return URegisterShutdown(EEReleaseInstanceMutex);
    case O::RegisterShutdownGraphics: return URegisterShutdown(EEGraphicsShutdown);
    case O::RegisterShutdownPersistents: return URegisterShutdown(FSPersistentList::Shutdown);
    case O::RegisterShutdownThreads: return URegisterShutdown(EEShutdownThreads);
    case O::RegisterShutdownSound: return URegisterShutdown(EEDeleteSoundManager);
    case O::RegisterShutdownDatabase: return URegisterShutdown(EEDeleteDatabase);
    case O::RegisterShutdownGameObjects: return URegisterShutdown(EEDeleteGameObjects);
    case O::WindowHide: PlatWindowShow(gGameWindow->GetOSRepresentation(), 0);
        break;
    case O::MessageShowError: PlatMessageShow(WSTR, *(UWideString*)a[0], PLAT_MESSAGE_ERROR | PLAT_MESSAGE_SYSTEM_MODAL | PLAT_MESSAGE_FOREGROUND);
        break;
    case O::NewUserInterface: gUserInterface = new EEUserInterface;
        break;
    case O::SetCampaignChecksum: *kCampaignChecksums[a[0]] = strtoul(WSTR, NULL, 10);
        break;
    // the graphics: the managers, the window, the render objects, the fonts
    case O::NewTextureManager: gTextureManager = new GETextureManager(gDataPath);
        break;
    case O::TextureManagerPresent: return gTextureManager != NULL;
    case O::NewModelManager: gModelManager = new GEModelManager(gDataPath);
        break;
    case O::ModelManagerPresent: return gModelManager != NULL;
    case O::PrefScreenID: return SCREEN_ID;
    case O::PrefScreenIDRaw: return SETTINGS.mScreenID;
    case O::PrefGameBitDepth: return SETTINGS.mGameBitDepth;
    case O::PrefTextureBitDepth: return SETTINGS.mTextureBitDepth;
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
    case O::FreeCamera: return gFreeCamera;
    case O::RectRoundToNearest: ((GERectangle*)a[0])->RoundToNearest();
        break;
    case O::ViewportSetViewRectangle: gViewport.SetViewRectangle(*(GERectangle*)a[0]);
        break;
    case O::SceneFlagsClear: gB529066 = false, gCameraFollowing = false, gB529068 = false;
        break;
    case O::EnumerateDisplayDevices: EEEnumerateDisplayDevices();
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
    case O::ScreenRestoreOriginalMode: gRasterizer->GetScreen(SCREEN_ID)->SetMode(sOriginalVideoMode);
        break;
    case O::SaveOriginalVideoMode: sOriginalVideoMode = gRasterizer->GetScreen(SCREEN_ID)->GetCurrentModeIndex();
        break;
    case O::ScreenDesktopRect: *(GERectangle*)a[0] = gRasterizer->GetScreen(SCREEN_ID)->GetDesktopRectangle();
        break;
    case O::Screen32BitDevice: return gRasterizer->GetScreen(SCREEN_ID)->Get32BitDeviceSupport();
    case O::Screen32BitTexture: return gRasterizer->GetScreen(SCREEN_ID)->Get32BitTextureSupport();
    case O::DebugVideoModeActive: ESDebugOut("!::EESetVideoMode: Requested video mode is already active.\n");
        break;
    case O::DebugVideoModeChanged: ESDebugOut("!::EESetVideoMode: Video mode changed to %ldx%ldx%ld at %ldHz.\n", ((GEVideoMode*)a[0])->mWidth, ((GEVideoMode*)a[0])->mHeight, ((GEVideoMode*)a[0])->mBitCount, ((GEVideoMode*)a[0])->mRefreshRate);
        break;
    case O::CreateEffectBatchBuffers: CreateEffectBatchBuffers();
        break;
    case O::DeleteEffectBatchBuffers: DeleteEffectBatchBuffers();
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
    case O::FontCreate: *kFonts[a[0]] = UIFontTable::FindOrCreateFont(USTR, (unsigned long)a[1], 2, (unsigned long)a[2]);
        break;
    case O::FontAt: return P64(*kFonts[a[0]]);
    case O::FontDereference: (*kFonts[a[0]])->Dereference();
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
    case O::IconsLoaded: return sIconsLoaded == true;
    case O::CreatePlaneModels: CreatePlaneModels();
        break;
    case O::ReleasePlaneModels: ReleasePlaneModels();
        break;
    case O::RoUnitsLoadMedia: EEROUnits::LoadMedia();
        break;
    case O::RoUnitsReleaseMedia: EEROUnits::ReleaseMedia();
        break;
    case O::RoSkyCreate: EEROTerrain::CreateSkyModel();
        break;
    case O::RoSkyRelease: EEROTerrain::ReleaseSkyModels();
        break;
    case O::RoShadowCreate: EEROGroundShadows::CreateShadowModel();
        break;
    case O::RoShadowRelease: EEROGroundShadows::ReleaseShadowModel();
        break;
    case O::ButtonsReload: EEButtonObject::Reload();
        break;
    // the data paths and folders
    case O::NewFsPath: return P64(new FSPath);
    case O::FsPathAddArchive: ((FSPath*)p)->AddArchive(*(FSArchive*)a[0], true);
        break;
    case O::FsPathAddDirectory: ((FSPath*)p)->AddDirectory(*(FSFileSpec*)a[0], true);
        break;
    case O::FileSpecSetDirStartupSub: SPEC->SetDirectory(gStartupDirectory + kStartupFolders[a[0]]);
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
    case O::DataPathPresent: return gDataPath != NULL;
    case O::DataPathDereference: gDataPath->Dereference();
        break;
    case O::CivPathPresent: return gCivilizationDataPath != NULL;
    case O::CivPathDereference: gCivilizationDataPath->Dereference();
        break;
    case O::SetDataPath: gDataPath = (FSPath*)p;
        break;
    case O::SetCivPath: gCivilizationDataPath = (FSPath*)p;
        break;
    // the threads
    case O::NewMusicManager: gMusicManager = new EEMusicManager(&gConductor->GetConductor(), 0);
        break;
    case O::MusicManagerPresent: return gMusicManager != NULL;
    case O::MusicManagerDelete: delete std::exchange(gMusicManager, nullptr);
        break;
    case O::MusicStop: gMusicManager->Stop();
        break;
    case O::NewMousePointer: gMousePointer = new RTSMousePointer(gMouse, gGameWindow, 0);
        break;
    case O::MousePointerPresent: return gMousePointer != NULL;
    case O::MousePointerPost: gMousePointer->mThread->Post((unsigned long)a[0], NULL, NULL, true);
        break;
    case O::MousePointerShown: return gMousePointer->mIsShown;
    case O::MousePointerShutdown: gMousePointer->Shutdown();
        break;
    case O::MousePointerDelete: delete std::exchange(gMousePointer, nullptr);
        break;
    case O::MousePointerPostSurface: gMousePointer->mThread->Post(0x405, gRasterizer->GetPrimarySurface(SCREEN_ID), NULL, true);
        break;
    case O::NewGraphics: gGraphics = new EEGraphics(gRasterizer, SCREEN_ID, gMouse, gUIManager, 0);
        break;
    case O::GraphicsPresent: return gGraphics != NULL;
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
    case O::DataPreLoadingPresent: return gDataPreLoading != NULL;
    case O::PreloadShutdown: gDataPreLoading->Shutdown();
        break;
    case O::PreloadDelete: delete std::exchange(gDataPreLoading, nullptr);
        break;
    case O::PreloadWaitForQueue: gDataPreLoading->WaitForMessageQueue();
        break;
    case O::NewPhysics: gPhysics = new EEPhysics(gGraphics, sTimer);
        break;
    case O::PhysShutdown: gPhysics->Shutdown();
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
    case O::SetGameScreenActive: gGameScreenActive = a[0] != 0;
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
    case O::MixerGet: return SEMixer::GetMixer(0, gConductor);
    case O::MixerUse: return SEMixer::UseMixer(gConductor);
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
    case O::SoundPoolCleanup: SESoundManager::CleanupMemoryPoolAndDestroyCriticalSections();
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
    case O::DumpRules: EEDumpRules();
        break;
    case O::DbFindTable: return EEDbFindFile(gDataPath, *kDbTableNames[a[0]], *(FSFileSpec**)a[1]);
    case O::DbFindEvents: return EEDbFindFile(gDataPath, kDbEventsFile, *(FSFileSpec**)a[0]);
    case O::DbFindEffects: return EEDbFindFile(gDataPath, kDbEffectsFile, *(FSFileSpec**)a[0]);
    case O::DbFindAmbientSounds: return EEDbFindFile(gDataPath, kDbAmbientSoundsFile, *(FSFileSpec**)a[0]);
    case O::LoadEvents: return EEEvent::LoadEvents(*SPEC, *(FSFileSpec*)a[0]);
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
    case O::UiInitialize: return UIInitialize();
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
    case O::UnitActionDeleteScripts: EEUnitAction::DeleteScripts();
        break;
    case O::ReleaseUnitClasses: EEUnitClass::ReleaseUnitClasses();
        break;
    case O::AnimalsUnload: EEAnimal::Unload();
        break;
    case O::DamageTablesClear: EETerrainTypeTable::ClearDamageTables();
        break;
    case O::CalamityClassesDestroy: EECalamityManager::GetInstance()->DestroyCalamityClasses(a[0] != 0);
        break;
    case O::DatabaseObjectsClear: gDatabaseObjects.Clear();
        break;
    case O::ButtonsUnload: EEButtonObject::Unload();
        break;
    case O::GfxEffectsUnloadTable: EEGraphicEffect::UnloadTable();
        break;
    case O::DeleteMediaTable: EEDeleteMediaTable();
        break;
    case O::MusicObjectsClear: EEMusicObject::ClearAll();
        break;
    case O::SoundObjectsUnloadTable: EESoundObject::UnloadTable();
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
    case O::LobbyOptionsSetDefaults: gGameOptions.SetDefaults();
        break;
    case O::WorldOptionsSetDefaults: gWorld.mGameOptions.SetDefaults();
        break;
    case O::InitPhysicsModels: EEInitPhysicsModels();
        break;
    case O::FreePhysicsModels: EEFreePhysicsModels();
        break;
    case O::ClearRandomMapGenerators: EEClearRandomMapGenerators();
        break;
    case O::NewMouse: gMouse = new EEMouse;
        break;
    case O::MousePresent: return gMouse != NULL;
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
    case O::ArtificialIntelligencePresent: return gArtificialIntelligence != NULL;
    case O::DeleteArtificialIntelligence: delete std::exchange(gArtificialIntelligence, nullptr);
        break;
    case O::NewCheatCodes: gCheatCodes = new EECheatCodes;
        break;
    case O::CheatCodesPresent: return gCheatCodes != NULL;
    case O::DeleteCheatCodes: delete std::exchange(gCheatCodes, nullptr);
        break;
    case O::NewGameTimer: sTimer = new TMTimer(0, 0, true);
        break;
    case O::GameTimerPresent: return sTimer != NULL;
    case O::DeleteGameTimer: delete std::exchange(sTimer, nullptr);
        break;
    case O::ContinentsShutdown: EEAIContinentManager::Shutdown();
        break;
    case O::MiniMapPresent: return gMiniMap != NULL;
    case O::AeManagerClear: gAEManager.Clear();
        break;
    case O::CpOnAttackPlayer: EECPUnitsOnAttackPlayer((EEComputerPlayer*)p, (long)a[0], a[1] != 0);
        break;
    default: return EEStartupOpNext(inOp, p, a, f);
    }
    return 0;
}

double EEStartupOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::PrefMaxSounds: return gGamePreferences.mSettings.GetMaxNumberOfSounds();
    default: return EEStartupOpNextF(inOp, p, a, f);
    }
}

// the graphics bodies' (EEGraphics.cpp)
int64_t EEStartupOpNext(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    return EEGraphicsOp(inOp, p, a, f);
}

double EEStartupOpNextF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    ESVERIFY_AT(inOp == EeGameCpp::CxEffectHeight, 1);
    return 0.0;
}

// The deferred COMDATs at the end of the unit (0x14494d..0x14622a: the inline-class deleting dtors and every template
// instantiation) are compiled without /Og, while every user function is -O1: the file ends with an unmatched
// `#pragma optimize("g", off)` (VC6 compiles deferred inline/template bodies at the end of the TU with the pragma
// state in force there; probe build/scratch_exe-core/probes/p2.cpp). Evidence: with it vector<EEUnitSet*>::_Buy
// 0x54541f goes 17.65 -> exact and nothing else changes.
#pragma optimize("g", off)

long EEFn0FCA98()
{
    return 0;
}
