// C:\EEX\Empire Earth\Empire Earth\Empire Earth.cpp (__FILE__ 0x13f2ae's asserts). The game's main unit:
// 0x13e967..0x14622a (its 49 global objects' init chains first, with the inline tables' deleting dtors between them,
// then the functions). Unit flags: the exe's module set. Global names are provisional unless another unit already
// bound them.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
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
#include "lle/FSFile.h"

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
#include "lle/FSRestorable.h"
#include "lle/TSEvent.h"
#include "lle/TSThread.h"
#include "lle/TSScheduler.h"
#include "lle/UResourceFile.h"
#include "lle/TMTimer.h"
#include "lle/FSPersistent.h"
#include "lle/FSPersistentList.h"
#include "lle/GE3DPoint.h"
#include "ee/EEMap.h"
#include "ee/EETeam.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEEffectManager.h"
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
#include "ee/EECPCombatManager.h"

// chain at 0x53e967 (wrapper)
UString gU530BE0("B7B226DDB7B226DDBCD9344624C93DE6C4A90DD4B7B20375");
// SYNTHETIC: EXE 0x53e967   (wrapper)
// SYNTHETIC: EXE 0x53e971   (ctor)
// SYNTHETIC: EXE 0x53e982   (registrar)
// SYNTHETIC: EXE 0x53e98e   (dtor)
// chain at 0x53e999 (wrapper)
UWideString gU530BC8;
// SYNTHETIC: EXE 0x53e999   (wrapper)
// SYNTHETIC: EXE 0x53e9a3   (ctor)
// SYNTHETIC: EXE 0x53e9ae   (registrar)
// SYNTHETIC: EXE 0x53e9ba   (dtor)
// chain at 0x53e9c5 (wrapper)
UString gExecutableName("Empire Earth.exe");
// SYNTHETIC: EXE 0x53e9c5   (wrapper)
// SYNTHETIC: EXE 0x53e9cf   (ctor)
// SYNTHETIC: EXE 0x53e9e0   (registrar)
// SYNTHETIC: EXE 0x53e9ec   (dtor)
// chain at 0x53e9f7 (wrapper)
UString gSavedGameExtension(".ees");
// SYNTHETIC: EXE 0x53e9f7   (wrapper)
// SYNTHETIC: EXE 0x53ea01   (ctor)
// SYNTHETIC: EXE 0x53ea12   (registrar)
// SYNTHETIC: EXE 0x53ea1e   (dtor)
// chain at 0x53ea29 (wrapper)
UString gScenarioExtension(".scn");
// SYNTHETIC: EXE 0x53ea29   (wrapper)
// SYNTHETIC: EXE 0x53ea33   (ctor)
// SYNTHETIC: EXE 0x53ea44   (registrar)
// SYNTHETIC: EXE 0x53ea50   (dtor)
// chain at 0x53ea5b (wrapper)
UString gU530B88(".ssa");
// SYNTHETIC: EXE 0x53ea5b   (wrapper)
// SYNTHETIC: EXE 0x53ea65   (ctor)
// SYNTHETIC: EXE 0x53ea76   (registrar)
// SYNTHETIC: EXE 0x53ea82   (dtor)
// chain at 0x53ea8d (wrapper)
UString gCampaignExtension(".ssa");
// SYNTHETIC: EXE 0x53ea8d   (wrapper)
// SYNTHETIC: EXE 0x53ea97   (ctor)
// SYNTHETIC: EXE 0x53eaa8   (registrar)
// SYNTHETIC: EXE 0x53eab4   (dtor)
// chain at 0x53eabf (wrapper)
GEViewport gViewport;
// SYNTHETIC: EXE 0x53eabf   (wrapper)
// SYNTHETIC: EXE 0x53eac9   (ctor)
// SYNTHETIC: EXE 0x53ead4   (registrar)
// SYNTHETIC: EXE 0x53eae0   (dtor)
// chain at 0x53eaeb (wrapper)
EEGamePreferences gGamePreferences;
// SYNTHETIC: EXE 0x53eaeb   (wrapper)
// SYNTHETIC: EXE 0x53eaf5   (ctor)
// SYNTHETIC: EXE 0x53eaff   (registrar)
// SYNTHETIC: EXE 0x53eb0b   (dtor)
// chain at 0x53eb15 (wrapper)
EEWorld gWorld;
// SYNTHETIC: EXE 0x53eb15   (wrapper)
// SYNTHETIC: EXE 0x53eb1f   (ctor)
// SYNTHETIC: EXE 0x53eb29   (registrar)
// SYNTHETIC: EXE 0x53eb35   (dtor)
// chain at 0x53eb3f (wrapper)
EEEvent gEvent;
// SYNTHETIC: EXE 0x53eb3f   (wrapper)
// SYNTHETIC: EXE 0x53eb49   (ctor)
// SYNTHETIC: EXE 0x53eb53   (registrar)
// SYNTHETIC: EXE 0x53eb5f   (dtor)
// chain at 0x53eb69 (wrapper)
EEDatabase gDatabaseObjects;
// SYNTHETIC: EXE 0x53eb69   (wrapper)
// SYNTHETIC: EXE 0x53eb73   (ctor)
// SYNTHETIC: EXE 0x53eb7d   (registrar)
// SYNTHETIC: EXE 0x53eb89   (dtor)
// chain at 0x53eb93 (wrapper)
EETechTree gTechTree;
// SYNTHETIC: EXE 0x53eb93   (wrapper)
// SYNTHETIC: EXE 0x53eb9d   (ctor)
// SYNTHETIC: EXE 0x53eba7   (registrar)
// SYNTHETIC: EXE 0x53ebb3   (dtor)
// chain at 0x53ebbd (wrapper)
EEToHitTable gToHitTable;
// SYNTHETIC: EXE 0x53ebbd   (wrapper)
// SYNTHETIC: EXE 0x53ebc7   (ctor)
// SYNTHETIC: EXE 0x53ebef   (registrar)
// SYNTHETIC: EXE 0x53ebfb   (dtor)

// FUNCTION: EXE 0x53ebd2
// Destroys the to-hit table and frees it when asked (scalar deleting destructor)

// chain at 0x53ec06 (wrapper)
EEAIUnitTargetingTable gAIUnitTargetingTable;
// SYNTHETIC: EXE 0x53ec06   (wrapper)
// SYNTHETIC: EXE 0x53ec10   (ctor)
// SYNTHETIC: EXE 0x53ec1b   (registrar)
// SYNTHETIC: EXE 0x53ec27   (dtor)
// chain at 0x53ec32 (wrapper)
EEPathManager gPathManager;
// SYNTHETIC: EXE 0x53ec32   (wrapper)
// SYNTHETIC: EXE 0x53ec3c   (ctor)
// SYNTHETIC: EXE 0x53ec46   (registrar)
// SYNTHETIC: EXE 0x53ec52   (dtor)
// chain at 0x53ec5c (wrapper)
EEAIContinentManager gContinentManager;
// SYNTHETIC: EXE 0x53ec5c   (wrapper)
// SYNTHETIC: EXE 0x53ec66   (ctor)
// SYNTHETIC: EXE 0x53ec70   (registrar)
// SYNTHETIC: EXE 0x53ec7c   (dtor)
// chain at 0x53ec86 (wrapper)
EEWorldTable gWorldTable;
// SYNTHETIC: EXE 0x53ec86   (wrapper)
// SYNTHETIC: EXE 0x53ec90   (ctor)
// SYNTHETIC: EXE 0x53ec9a   (registrar)
// SYNTHETIC: EXE 0x53eca6   (dtor)
// chain at 0x53ecc6 (wrapper)
EETerrainTable gTerrainTable;
// SYNTHETIC: EXE 0x53ecc6   (wrapper)
// SYNTHETIC: EXE 0x53ecd0   (ctor)
// SYNTHETIC: EXE 0x53ecda   (registrar)
// SYNTHETIC: EXE 0x53ece6   (dtor)
// chain at 0x53ecf0 (wrapper)
EETerrainColorTable gTerrainColorTable;
// SYNTHETIC: EXE 0x53ecf0   (wrapper)
// SYNTHETIC: EXE 0x53ecfa   (ctor)
// SYNTHETIC: EXE 0x53ed04   (registrar)
// SYNTHETIC: EXE 0x53ed10   (dtor)
// chain at 0x53ed1b (wrapper)
EETerrainTypeTable gTerrainTypeTable;
// SYNTHETIC: EXE 0x53ed1b   (wrapper)
// SYNTHETIC: EXE 0x53ed25   (ctor)
// SYNTHETIC: EXE 0x53ed2f   (registrar)
// SYNTHETIC: EXE 0x53ed3b   (dtor)
// chain at 0x53ed46 (wrapper)
EERandomMapTable gRandomMapTable;
// SYNTHETIC: EXE 0x53ed46   (wrapper)
// SYNTHETIC: EXE 0x53ed50   (ctor)
// SYNTHETIC: EXE 0x53ed5a   (registrar)
// SYNTHETIC: EXE 0x53ed66   (dtor)
// chain at 0x53ed71 (wrapper)
EEGameState gGameState;
// SYNTHETIC: EXE 0x53ed71   (wrapper)
// SYNTHETIC: EXE 0x53ed7b   (ctor)
// SYNTHETIC: EXE 0x53ed85   (registrar)
// SYNTHETIC: EXE 0x53ed91   (dtor)
// chain at 0x53ed9b (wrapper)
EEComputerPlayerBehaviorTable gComputerPlayerBehaviorTable;
// SYNTHETIC: EXE 0x53ed9b   (wrapper)
// SYNTHETIC: EXE 0x53eda5   (ctor)
// SYNTHETIC: EXE 0x53edcd   (registrar)
// SYNTHETIC: EXE 0x53edd9   (dtor)

// FUNCTION: EXE 0x53edb0
// Destroys the computer player behaviour table and frees it when asked (scalar deleting destructor)

// chain at 0x53ede4 (wrapper)
EEUnitSetManager gUnitSetManager;
// SYNTHETIC: EXE 0x53ede4   (wrapper)
// SYNTHETIC: EXE 0x53edee   (ctor)
// SYNTHETIC: EXE 0x53ee10   (registrar)
// SYNTHETIC: EXE 0x53ee1c   (dtor)
// chain at 0x53ee26 (wrapper)
EEAEManager gAEManager;
// SYNTHETIC: EXE 0x53ee26   (wrapper)
// SYNTHETIC: EXE 0x53ee30   (ctor)
// SYNTHETIC: EXE 0x53ee3a   (registrar)
// SYNTHETIC: EXE 0x53ee46   (dtor)
// chain at 0x53ee50 (wrapper)
EEAmbientSoundManager gAmbientSoundManager;
// SYNTHETIC: EXE 0x53ee50   (wrapper)
// SYNTHETIC: EXE 0x53ee5a   (ctor)
// SYNTHETIC: EXE 0x53ee64   (registrar)
// SYNTHETIC: EXE 0x53ee70   (dtor)
// chain at 0x53ee7a (wrapper)
EECivilizationManager gCivilizationManager;
// SYNTHETIC: EXE 0x53ee7a   (wrapper)
// SYNTHETIC: EXE 0x53ee84   (ctor)
// SYNTHETIC: EXE 0x53ee8e   (registrar)
// SYNTHETIC: EXE 0x53ee9a   (dtor)
// chain at 0x53eea4 (wrapper)
EEDoppelgangerManager gDoppelgangerManager;
// SYNTHETIC: EXE 0x53eea4   (wrapper)
// SYNTHETIC: EXE 0x53eeae   (ctor)
// SYNTHETIC: EXE 0x53eeb8   (registrar)
// SYNTHETIC: EXE 0x53eec4   (dtor)
// chain at 0x53eece (wrapper)
EECivPowersManager gCivPowersManager;
// SYNTHETIC: EXE 0x53eece   (wrapper)
// SYNTHETIC: EXE 0x53eed8   (ctor)
// SYNTHETIC: EXE 0x53eee2   (registrar)
// SYNTHETIC: EXE 0x53eeee   (dtor)
// chain at 0x53eef8 (wrapper)
UWideString gCampaignAOCRoman("AOCRoman");
// SYNTHETIC: EXE 0x53eef8   (wrapper)
// SYNTHETIC: EXE 0x53ef02   (ctor)
// SYNTHETIC: EXE 0x53ef13   (registrar)
// SYNTHETIC: EXE 0x53ef1f   (dtor)
// chain at 0x53ef2a (wrapper)
UWideString gCampaignAOCPacific("AOCPacific");
// SYNTHETIC: EXE 0x53ef2a   (wrapper)
// SYNTHETIC: EXE 0x53ef34   (ctor)
// SYNTHETIC: EXE 0x53ef45   (registrar)
// SYNTHETIC: EXE 0x53ef51   (dtor)
// chain at 0x53ef5c (wrapper)
UWideString gCampaignAOCAsian("AOCAsian");
// SYNTHETIC: EXE 0x53ef5c   (wrapper)
// SYNTHETIC: EXE 0x53ef66   (ctor)
// SYNTHETIC: EXE 0x53ef77   (registrar)
// SYNTHETIC: EXE 0x53ef83   (dtor)
// chain at 0x53ef8e (wrapper)
UWideString gCampaignEEDemo("EEDemo");
// SYNTHETIC: EXE 0x53ef8e   (wrapper)
// SYNTHETIC: EXE 0x53ef98   (ctor)
// SYNTHETIC: EXE 0x53efa9   (registrar)
// SYNTHETIC: EXE 0x53efb5   (dtor)
// chain at 0x53efc0 (wrapper)
UWideString gCampaignEELearning("EELearningCampaign");
// SYNTHETIC: EXE 0x53efc0   (wrapper)
// SYNTHETIC: EXE 0x53efca   (ctor)
// SYNTHETIC: EXE 0x53efdb   (registrar)
// SYNTHETIC: EXE 0x53efe7   (dtor)
// chain at 0x53eff2 (wrapper)
UWideString gU52A280("een1ve1s_");
// SYNTHETIC: EXE 0x53eff2   (wrapper)
// SYNTHETIC: EXE 0x53effc   (ctor)
// SYNTHETIC: EXE 0x53f00d   (registrar)
// SYNTHETIC: EXE 0x53f019   (dtor)
// chain at 0x53f024 (wrapper)
UString gPhysicsThreadName("Physics");
// SYNTHETIC: EXE 0x53f024   (wrapper)
// SYNTHETIC: EXE 0x53f02e   (ctor)
// SYNTHETIC: EXE 0x53f03f   (registrar)
// SYNTHETIC: EXE 0x53f04b   (dtor)
// chain at 0x53f056 (wrapper)
UString gGraphicsThreadName("Graphics");
// SYNTHETIC: EXE 0x53f056   (wrapper)
// SYNTHETIC: EXE 0x53f060   (ctor)
// SYNTHETIC: EXE 0x53f071   (registrar)
// SYNTHETIC: EXE 0x53f07d   (dtor)
// chain at 0x53f088 (wrapper)
UString gDataPreLoadingThreadName("DataPreLd");
// SYNTHETIC: EXE 0x53f088   (wrapper)
// SYNTHETIC: EXE 0x53f092   (ctor)
// SYNTHETIC: EXE 0x53f0a3   (registrar)
// SYNTHETIC: EXE 0x53f0af   (dtor)
// chain at 0x53f0ba (wrapper)
UString gMousePointerThreadName("MousePtr");
// SYNTHETIC: EXE 0x53f0ba   (wrapper)
// SYNTHETIC: EXE 0x53f0c4   (ctor)
// SYNTHETIC: EXE 0x53f0d5   (registrar)
// SYNTHETIC: EXE 0x53f0e1   (dtor)
// chain at 0x53f0ec (wrapper)
UString gScenarioEditorThreadName("ScenEdit");
// SYNTHETIC: EXE 0x53f0ec   (wrapper)
// SYNTHETIC: EXE 0x53f0f6   (ctor)
// SYNTHETIC: EXE 0x53f107   (registrar)
// SYNTHETIC: EXE 0x53f113   (dtor)
// chain at 0x53f11e (wrapper)
UString gUserInterfaceThreadName("UserIntfc");
// SYNTHETIC: EXE 0x53f11e   (wrapper)
// SYNTHETIC: EXE 0x53f128   (ctor)
// SYNTHETIC: EXE 0x53f139   (registrar)
// SYNTHETIC: EXE 0x53f145   (dtor)
// chain at 0x53f150 (wrapper)
UString gCommSendThreadName("CommSend");
// SYNTHETIC: EXE 0x53f150   (wrapper)
// SYNTHETIC: EXE 0x53f15a   (ctor)
// SYNTHETIC: EXE 0x53f16b   (registrar)
// SYNTHETIC: EXE 0x53f177   (dtor)
// chain at 0x53f182 (wrapper)
UString gCommRecvThreadName("CommRecv");
// SYNTHETIC: EXE 0x53f182   (wrapper)
// SYNTHETIC: EXE 0x53f18c   (ctor)
// SYNTHETIC: EXE 0x53f19d   (registrar)
// SYNTHETIC: EXE 0x53f1a9   (dtor)
// chain at 0x53f1b4 (wrapper)
UString gCommMainThreadName("CommMain");
// SYNTHETIC: EXE 0x53f1b4   (wrapper)
// SYNTHETIC: EXE 0x53f1be   (ctor)
// SYNTHETIC: EXE 0x53f1cf   (registrar)
// SYNTHETIC: EXE 0x53f1db   (dtor)
// chain at 0x53f1e6 (wrapper)
UString gCommLagLossThreadName("CommLagLoss");
// SYNTHETIC: EXE 0x53f1e6   (wrapper)
// SYNTHETIC: EXE 0x53f1f0   (ctor)
// SYNTHETIC: EXE 0x53f201   (registrar)
// SYNTHETIC: EXE 0x53f20d   (dtor)
// chain at 0x53f218 (wrapper)
UString gCommRecPlayThreadName("CommRecPlay");
// SYNTHETIC: EXE 0x53f218   (wrapper)
// SYNTHETIC: EXE 0x53f222   (ctor)
// SYNTHETIC: EXE 0x53f233   (registrar)
// SYNTHETIC: EXE 0x53f23f   (dtor)
// chain at 0x53f24a (wrapper)
UString gSoundScavengerThreadName("SoundScavngr");
// SYNTHETIC: EXE 0x53f24a   (wrapper)
// SYNTHETIC: EXE 0x53f254   (ctor)
// SYNTHETIC: EXE 0x53f265   (registrar)
// SYNTHETIC: EXE 0x53f271   (dtor)
// chain at 0x53f27c (wrapper)
UString gCDAudioThreadName("CDAudio");
// SYNTHETIC: EXE 0x53f27c   (wrapper)
// SYNTHETIC: EXE 0x53f286   (ctor)
// SYNTHETIC: EXE 0x53f297   (registrar)
// SYNTHETIC: EXE 0x53f2a3   (dtor)

// The unit's own data (provisional names): the network settings EELoadNetworkSettings reads from the string resources
// (0x4c05d4.., .data, all 1), the start screen's window class (0x4c05e8, a char array: both RegisterClassEx and
// CreateWindowEx take its address), its bitmap, the bitmap's BITMAP and the memory DC (0x52a18c / 0x52a190 / 0x52a188)
long gAOCRomanChecksum = 1;
long gAOCPacificChecksum = 1;
long gAOCAsianChecksum = 1;
long gEEDemoChecksum = 1;
long gEELearningChecksum = 1;
static char sStartScreenClassName[] = "Loading Game Window";
static HBITMAP sStartScreenBitmap;
static BITMAP sStartScreenBitmapInfo;
static HDC sStartScreenDC;
static TMTimer* sTimer;                     // 0x5319ec (EECreateGameTimer's assert names it)
static HHOOK sCallWndProcHook;             // 0x5319e0 (the start-up's SetWindowsHookEx(WH_CALLWNDPROC, ...))
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
    static LRESULT CALLBACK WindowProc(HWND inWindow, UINT inMessage, WPARAM inWParam, LPARAM inLParam);
    static unsigned long ThreadMain(void* inParameter);
    HINSTANCE mInstance;                    // +0x00
    TSThread* mThread;                      // +0x04
    TSEvent* mEndThreadEvent;               // +0x08
    bool mQuit;                             // +0x0c
};
// FUNCTION: EXE 0x53f2ae
// Creates the loading splash screen for the instance, with its end-of-thread event
EEStartScreen::EEStartScreen(HINSTANCE inInstance)
{
    this->mInstance = inInstance;
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mEndThreadEvent) == keseOK, 411);
    this->mQuit = false;
}

// FUNCTION: EXE 0x53f2eb
// Destroys the splash screen's end-of-thread event
EEStartScreen::~EEStartScreen()
{
    TSScheduler::DestroyEvent(this->mEndThreadEvent);
}

// FUNCTION: EXE 0x53f2f6
// In full screen only: starts the splash screen thread and waits until it has painted the splash bitmap
void EEStartScreen::Start()
{
    if (gGamePreferences.mSettings.GetFullScreen() != true)
        return;
    gStartScreenPainted = false;
    TSScheduler::CreateThread(this->mThread, EEStartScreen::ThreadMain, this);
    while (gStartScreenPainted == false)
        Sleep(1);
}

// FUNCTION: EXE 0x53f331
// In full screen only: tells the splash screen thread to quit, waits for it to end and destroys it
void EEStartScreen::Stop()
{
    if (gGamePreferences.mSettings.GetFullScreen() != true)
        return;
    this->mQuit = true;
    this->mEndThreadEvent->Wait(INFINITE);
    TSScheduler::DestroyThread(this->mThread);
}

// FUNCTION: EXE 0x53f358
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

// FUNCTION: EXE 0x53f3f9
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
            do {
                theScreen->mThread->Receive(theMessage, theParameter1, theParameter2);
                EESetThreadFloatingPoint("StartScreenMainThread");
                if (theScreen->mQuit == true)
                    PostQuitMessage(0);
                Sleep(1);
            } while (theMessage != 0x12);
            theScreen->mEndThreadEvent->Set();
            ReleaseDC(theWindow, sStartScreenDC);
        }
        DeleteObject(sStartScreenBitmap);
    }
    return 0;
}

// FUNCTION: EXE 0x53f5be
// Puts the calling thread's FPU into 53-bit precision and round-to-nearest mode (the thread name is unused)
// (every game thread calls it first, passing its name)
void EESetThreadFloatingPoint(const char* inThreadName)
{
    if (UMGetFloatingPointPrecision() != kFPPrecision53)
        UMSetFloatingPointPrecision(kFPPrecision53);
    if (UMGetFloatingPointRoundingMode() != 0)
        UMSetFloatingPointRoundingMode(0);
}

// FUNCTION: EXE 0x53f5e9
// Makes every computer player at war with the given player attack it; with the flag set, the others (its allies
// and its own computer player) defend it
void EEAttackPlayers(long inPlayer, unsigned char inFlag)
{
    std::vector<EEComputerPlayer*> theAllies;
    std::vector<EEComputerPlayer*> theEnemies;
    EEPlayer* thePlayer = gWorld.mPlayers[inPlayer];
    if (thePlayer != NULL) {
        for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
            EEPlayer* theOther = gWorld.mPlayers[i];
            if (theOther == NULL || theOther->mComputerPlayer == NULL || theOther->mObserverMode != false)
                continue;
            EEComputerPlayer* theComputer = theOther->mComputerPlayer;
            if (theComputer == thePlayer || thePlayer->mEnemies[theComputer->mPlayerID] == 0)
                theAllies.push_back(theComputer);
            else
                theEnemies.push_back(theComputer);
        }
        long j;
        for (j = theEnemies.size() - 1; j >= 0; j--)
            theEnemies[j]->mCombatManager->AttackPlayer(thePlayer);
        if (inFlag)
            for (j = theAllies.size() - 1; j >= 0; j--)
                theAllies[j]->mCombatManager->DefendPlayer(thePlayer);
    }
}

// FUNCTION: EXE 0x53f719
// Destroys the AI unit targeting table and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x53f736
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
        if (_stricmp(theFile.GetFile(), theExcluded) != 0)
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
            WIN32_FILE_ATTRIBUTE_DATA theAttributes;
            GetFileAttributesExA(thePath, GetFileExInfoStandard, &theAttributes);
            theChecksum += theAttributes.ftLastWriteTime.dwHighDateTime
                + theAttributes.nFileSizeHigh + theAttributes.nFileSizeLow;
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

// FUNCTION: EXE 0x53f991
// Reads the five campaign checksums from string resources 0x32..0x34, 0x37, 0x36; a missing string keeps the value
void EELoadNetworkSettings()
{
    UWideString theString;
    char* theEnd;
    if (gResource->GetString(IDS_0x32, theString) == kErrorOK)
        gAOCRomanChecksum = strtoul(theString, &theEnd, 10);
    if (gResource->GetString(IDS_0x33, theString) == kErrorOK)
        gAOCPacificChecksum = strtoul(theString, &theEnd, 10);
    if (gResource->GetString(IDS_0x34, theString) == kErrorOK)
        gAOCAsianChecksum = strtoul(theString, &theEnd, 10);
    if (gResource->GetString(IDS_0x37, theString) == kErrorOK)
        gEEDemoChecksum = strtoul(theString, &theEnd, 10);
    if (gResource->GetString(IDS_0x36, theString) == kErrorOK)
        gEELearningChecksum = strtoul(theString, &theEnd, 10);
}

#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/NEProtocol.h"
#include "lle/NEMessage.h"
#include "ee/EEMessage.h"

void EEEmptyShutdown(bool inShutdown);
unsigned long gBinaryChecksum; // 0x5319e4: the install checksum EEInitializeNetwork computes (sent to clients)

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
SEMixer* gConductor;                        // 0x5319c0
SESoundManager* gSoundManager;              // 0x5319c4
NESynchronousCommunications* gCommunications;   // 0x5319c8
EEWonNet* gWonNet;                          // 0x5319cc
EEArtificialIntelligence* gArtificialIntelligence;   // 0x5319d0
EECheatCodes* gCheatCodes;                  // 0x5319d4
EEUIManager* gUIManager;                    // 0x53191c
EEMouse* gMouse;                            // 0x531924
bool gFreeCamera;                           // 0x5319e8 (EmpireEarth.h)
// the game view and water ripple constants (.rdata 0x442090..0x4420a8, read from memory by both graphics initializers)
static const float kStartViewLeft = 0.0f;
static const float kStartViewRight = 1600.0f;
static const float kStartViewTop = 35.0f;
static const float kStartViewBottom = 1085.0f;
static const float kWaterRipplePeriod = 3.1415927f;
static const float kWaterRippleSpeed = 0.025f;
static const float kWaterRippleAmplitude = 0.05f;
#include "ee/EESaveGameFiles.h"
// the save-game version (.rdata 0x4420ac/0x4420b0, before this unit's vtables 0x4420c0; values from the original)
const unsigned long kSaveGameMajorVersion = 0;
const unsigned long kSaveGameMinorVersion = 250;
#include "lle/PIMModule.h"
#include "lle/Input System.h"
#include "ee/EEUserInterface.h"

void EEReleaseInstanceMutex(bool inShutdown); // a shutdown routine; the argument is unused
bool EEInitializeDisplay();
void EEInitializeGraphics();
bool EEInitializeSounds(); // 0x543557; EEInitializePart1 tests its result in AL
void EEInitializeNetwork();
void EEInitializeDatabase(); // 0x5414a3
void EEInitializeGameObjects(); // 0x542363
EEUserInterface* gUserInterface; // 0x5318f8, created by EEInitializePart1

// FUNCTION: EXE 0x5412a0
// Starts the engine (input, threads, timers), opens the display and initializes graphics, sound, network, the
// database, game objects and the user interface; returns false (after a message box) when sound fails
bool EEInitializePart1()
{
    long theResult = URegisterShutdown(EEReleaseInstanceMutex);

    ESVERIFY_AT(theResult == keseOK, 796);
    theResult = PIMInitialize();

    ESVERIFY_AT(theResult == keseOK, 800);
    EESetThreadFloatingPoint("EEInitializePart1:PIMInitialize");
    theResult = TSInitialize();

    ESVERIFY_AT(theResult == keseOK, 805);
    EESetThreadFloatingPoint("EEInitializePart1:TSInitialize");
    theResult = TMInitialize();

    ESVERIFY_AT(theResult == keseOK, 810);
    EESetThreadFloatingPoint("EEInitializePart1:TMInitialize");
    bool theInitialized = EEInitializeDisplay();
    if (theInitialized) {
        EEInitializeGraphics();
        EESetThreadFloatingPoint("EEInitializePart1:EEInitializeGraphics");
        theResult = ISInitialize(gInstance, gGameWindow, gResource, 100, 250);

        ESVERIFY_AT(theResult == keseOK, 823);
        EESetThreadFloatingPoint("EEInitializePart1:ISInitialize");
        if (EEInitializeSounds() == false) {
            ShowWindow(gGameWindow->GetOSRepresentation(), SW_HIDE);
            UWideString theMessage;
            UWideString theTitle;
            gResource->GetString(IDS_EMPIRE_EARTH_ART_OF_CONQUEST_REQUIRES_SOUND, theMessage);
            gResource->GetString(IDS_ERROR_CANT_RUN_16EA, theTitle);
            MessageBoxA(NULL, theMessage, theTitle, MB_ICONHAND | MB_SYSTEMMODAL | MB_SETFOREGROUND);
            return false;
        }
        EESetThreadFloatingPoint("EEInitializePart1:InitializeSounds");
        EEInitializeNetwork();
        EESetThreadFloatingPoint("EEInitializePart1:EEInitializeNetwork");
        EEInitializeDatabase();
        EESetThreadFloatingPoint("EEInitializePart1:EEInitializeDatabase");
        EEInitializeGameObjects();
        EESetThreadFloatingPoint("EEInitializePart1:EEInitializeGameObjects");
        gUserInterface = new EEUserInterface;

        if (gUserInterface == NULL) ESVerify(__FILE__, 853, "gUserInterfaceThread != NULL");
    }
    return theInitialized;
}

#include "lle/GERhombus.h"
#include "ee/EEMouse.h"
#include "ee/RTSMousePointer.h"
#include "ee/EEDataPreLoading.h"
#include "ee/EEPhysics.h"

// The three threads' stop members (declared in their headers; their COMDATs 0x140999, 0x144488, 0x1444ac are this
// unit's; the same body as RTSGraphics::Shutdown)
inline void RTSMousePointer::Shutdown()
{
    this->mThread->Post(kThreadMessageQuit, NULL, NULL, true);
    this->mThreadDone->Wait(INFINITE);
}

inline void RTSPhysics::Shutdown()
{
    this->mThread->Post(kThreadMessageQuit, NULL, NULL, true);
    this->mThreadDone->Wait(INFINITE);
}

inline void EEDataPreLoading::Shutdown()
{
    this->mThread->Post(kThreadMessageQuit, NULL, NULL, true);
    this->mThreadDone->Wait(INFINITE);
}
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"

void EEShutdownThreads(bool inShutdown); // registered as a shutdown routine (0x544324)
EEMusicManager* gMusicManager;
RTSMousePointer* gMousePointer;
EEGraphics* gGraphics;
EEDataPreLoading* gDataPreLoading;
EEPhysics* gPhysics;

#include <list>
#include "lle/GEPhysicalModel.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/EECommunications.h"

#include <typeinfo>
#include "ee/EEUnit.h"

// FUNCTION: EXE 0x54265e
// Creates the texture and model managers, activates the screen at 640x480, sets the default render states,
// creates the render objects and every UI font, and registers the graphics shutdown
void EEInitializeGraphics()
{
    UWideString theResourceName;
    UString theFontName;

    GERectangle theGameView;
    gTextureManager = new GETextureManager(gDataPath);

    ESVERIFY_AT(gTextureManager != NULL, 1332);
    gModelManager = new GEModelManager(gDataPath);

    ESVERIFY_AT(gModelManager != NULL, 1336);
    EESetVideoMode(640, 480, gGamePreferences.mSettings.mGameBitDepth, true);
    gGameWindow->Resize(640, 480);
    long theResult = gRasterizer->ActivateScreen(gGamePreferences.mSettings.GetScreenID(), gTextureManager, gGameWindow,
                                                gGamePreferences.mSettings.GetScreenMode(), false, kPixelFormatZPrefer16);

    ESVERIFY_AT(theResult == keseOK, 1346);
    theGameView.SetLeft(kStartViewLeft); theGameView.SetRight(kStartViewRight); theGameView.SetTop(kStartViewTop); theGameView.SetBottom(kStartViewBottom);
    EESetGameViewRectangle(theGameView);
    gRasterizer->SetFog(0, gGamePreferences.mSettings.GetScreenID());
    GEColor theColor(0.0f, 0.0f, 0.0f);
    gRasterizer->SetFogColor(theColor, gGamePreferences.mSettings.GetScreenID());
    gRasterizer->SetFogType(0, gGamePreferences.mSettings.GetScreenID());
    theColor = GEColor(1.0f, 1.0f, 1.0f);
    gRasterizer->SetAmbientColor(theColor, gGamePreferences.mSettings.GetScreenID());
    gRasterizer->SetAmbientIntensity(0.0f, gGamePreferences.mSettings.GetScreenID());
    gRasterizer->SetColorKeying(1, gGamePreferences.mSettings.GetScreenID());
    gRasterizer->SetCulling(kCullCW);
    gRasterizer->SetLighting(1, gGamePreferences.mSettings.GetScreenID());
    gRasterizer->SetProjectionMode(1, gGamePreferences.mSettings.GetScreenID());
    gRasterizer->SetShading(1, gGamePreferences.mSettings.GetScreenID());
    gRasterizer->SetSpecular(0, gGamePreferences.mSettings.GetScreenID());
    gRasterizer->SetZBuffer(kZBufferDisabled, gGamePreferences.mSettings.GetScreenID());
    gRasterizer->SetZCompare(kCompareLessEqual, gGamePreferences.mSettings.GetScreenID());
    gRasterizer->SetDithering(1, gGamePreferences.mSettings.GetScreenID());
    gRasterizer->SetAntiAliasing(gGamePreferences.mSettings.mAntiAliasing != false, gGamePreferences.mSettings.GetScreenID());
    gRasterizer->SetAlphaTesting(1, gGamePreferences.mSettings.GetScreenID());
    gRasterizer->SetAlphaTestingCompareState(kCompareGreater, gGamePreferences.mSettings.GetScreenID());
    gRasterizer->SetAlphaTestingRefValue(0.0f, gGamePreferences.mSettings.GetScreenID());
    gRasterizer->SetWaitForVSync(gGamePreferences.mSettings.mWaitForVSync, gGamePreferences.mSettings.GetScreenID());
    gRasterizer->SetWaterRippleEffect(kWaterRipplePeriod, kWaterRippleSpeed, kWaterRippleAmplitude);
    gU56535C = new EEROTerrain;
    gU565358 = new EEROGroundShadows;
    gU565354 = new EEROUnits;
    gU565350 = new EEROWater;
    gU565360 = new EEROEffects;
    gResource->GetString(IDS_ARIAL, theResourceName);
    theFontName = (const char*)theResourceName;
    gFontArial8 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 8);

    ESVERIFY_AT(gFontArial8, 1399);
    gFontArial9 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 9);

    ESVERIFY_AT(gFontArial9, 1402);
    gU531930 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 10);

    if (!gU531930) ESVerify(__FILE__, 1405, "gFontArial10"); // Preserve original assert spelling over provisional external identity.
    gU531934 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 12);

    if (!gU531934) ESVerify(__FILE__, 1408, "gFontArial12"); // Preserve original assert spelling over provisional external identity.
    gU531938 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 14);

    if (!gU531938) ESVerify(__FILE__, 1411, "gFontArial14"); // Preserve original assert spelling over provisional external identity.
    gFontArial16 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 16);

    ESVERIFY_AT(gFontArial16, 1414);
    gFontArial18 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 18);

    ESVERIFY_AT(gFontArial18, 1417);
    gFontArial20 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 20);

    ESVERIFY_AT(gFontArial20, 1420);
    gFontArial24 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 24);

    ESVERIFY_AT(gFontArial24, 1423);
    gFontArial36 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 36);

    ESVERIFY_AT(gFontArial36, 1426);
    gFontArial48 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 48);

    ESVERIFY_AT(gFontArial48, 1429);
    gFontArial72 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 72);

    ESVERIFY_AT(gFontArial72, 1432);
    gFontArialBold8 = UIFontTable::FindOrCreateFont(theFontName, 1, 2, 8);

    ESVERIFY_AT(gFontArial8, 1435);
    gFontArialBold9 = UIFontTable::FindOrCreateFont(theFontName, 1, 2, 9);

    ESVERIFY_AT(gFontArial9, 1438);
    gFontArialBold10 = UIFontTable::FindOrCreateFont(theFontName, 1, 2, 10);

    if (!gU531930) ESVerify(__FILE__, 1441, "gFontArial10"); // Preserve original assert spelling over provisional external identity.
    gFontArialBold12 = UIFontTable::FindOrCreateFont(theFontName, 1, 2, 12);

    if (!gU531934) ESVerify(__FILE__, 1444, "gFontArial12"); // Preserve original assert spelling over provisional external identity.
    gFontArialBold14 = UIFontTable::FindOrCreateFont(theFontName, 1, 2, 14);

    if (!gU531938) ESVerify(__FILE__, 1447, "gFontArial14"); // Preserve original assert spelling over provisional external identity.
    gFontArialBold16 = UIFontTable::FindOrCreateFont(theFontName, 1, 2, 16);

    ESVERIFY_AT(gFontArial16, 1450);
    gResource->GetString(IDS_TIMES_NEW_ROMAN, theResourceName);
    theFontName = (const char*)theResourceName;
    gFontTimesNewRoman9 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 9);

    ESVERIFY_AT(gFontTimesNewRoman9, 1457);
    gFontTimesNewRoman10 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 10);

    ESVERIFY_AT(gFontTimesNewRoman10, 1460);
    gFontTimesNewRoman12 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 12);

    ESVERIFY_AT(gFontTimesNewRoman12, 1463);
    gFontTimesNewRoman14 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 14);

    ESVERIFY_AT(gFontTimesNewRoman14, 1466);
    gResource->GetString(IDS_COURIER_NEW, theResourceName);
    theFontName = (const char*)theResourceName;
    gFontCourier8 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 8);

    ESVERIFY_AT(gFontCourier8, 1473);
    gFontCourier10 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 10);

    ESVERIFY_AT(gFontCourier10, 1476);
    gFontCourier12 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 12);

    ESVERIFY_AT(gFontCourier12, 1479);
    gFontCourier14 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 14);

    ESVERIFY_AT(gFontCourier14, 1482);
    gResource->GetString(IDS_TAHOMA, theResourceName);
    theFontName = (const char*)theResourceName;
    gFontTahoma8 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 8);

    ESVERIFY_AT(gFontTahoma8, 1489);
    gU531994 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 9);

    if (!gU531994) ESVerify(__FILE__, 1492, "gFontTahoma9"); // Preserve original assert spelling over provisional external identity.
    gFontTahoma10 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 10);

    ESVERIFY_AT(gFontTahoma10, 1495);
    gFontTahoma12 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 12);

    ESVERIFY_AT(gFontTahoma12, 1498);
    gFontTahoma14 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 14);

    ESVERIFY_AT(gFontTahoma14, 1501);
    gFontTahoma16 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 16);

    ESVERIFY_AT(gFontTahoma16, 1504);
    gFontTahoma18 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 18);

    ESVERIFY_AT(gFontTahoma18, 1507);
    gFontTahoma20 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 20);

    ESVERIFY_AT(gFontTahoma20, 1510);
    gFontTahoma24 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 24);

    ESVERIFY_AT(gFontTahoma24, 1513);
    gFontTahoma36 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 36);

    ESVERIFY_AT(gFontTahoma36, 1516);
    gFontTahoma48 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 48);

    ESVERIFY_AT(gFontTahoma48, 1519);
    gFontTahoma72 = UIFontTable::FindOrCreateFont(theFontName, 0, 2, 72);

    ESVERIFY_AT(gFontTahoma72, 1522);
    theResult = URegisterShutdown(EEGraphicsShutdown);

    ESVERIFY_AT(theResult == keseOK, 1526);
}

// FUNCTION: EXE 0x5431dc
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
    unsigned long theChecksum = EEGetFileChecksum(theFile, "Low-Level Engine.dll", 0);
    theChecksum += EEGetFileChecksum(theFile, "sxlrt233.dll", 0);
    theFile.SetDirectory(gStartupDirectory);
    theChecksum += EEGetFileChecksum(theFile, "TCP Protocol.dll", 0);
    theFile.SetDirectory(gStartupDirectory + "Data\\Db\\");
    theChecksum += EEGetFileChecksum(theFile, "*.dat", 0);
    theFile.SetDirectory(gStartupDirectory + "Data\\Unit AI Scripts\\");
    theChecksum += EEGetFileChecksum(theFile, "*.tai", 0);
    theFile.SetDirectory(gStartupDirectory + "Data\\Models\\");
    theChecksum += EEGetFileChecksum(theFile, "*.cem", 0);
    for (unsigned long i = 0; i < gDataPath->GetArchiveCount(); i++) {
        bool theHaveTimestamp = gDataPath->GetArchive(i).GetTag(UWideString("Timestamp"), theTag) == kErrorOK;
        if (theHaveTimestamp) {
            sscanf(theTag, "%d", &theTimestamp);
            theChecksum += theTimestamp;
        }
    }
    TMGetTime();
    gBinaryChecksum = theChecksum;
    gGameVersion = theVersion.GetIndexedSubstring(theVersion.Find("v", 0), theVersion.GetLength() - 1);
    sprintf(theSuffix, ".%04d", gBinaryChecksum & 0x1fff);
    gGameVersion += theSuffix;
}

// FUNCTION: EXE 0x53fa74
// Registers the saved game's persistent sections (checksum, map, scenario, players, world, effects, ...)
// (called from the start-up 0x540d73)
void EERegisterPersistents()
{
    std::vector<EETeam> theUnused;
    FSPersistentList::Instance()->RegisterPersistent(new EEPChecksum(true));
    FSPersistentList::Instance()->RegisterPersistent(new EEPTGeneric<unsigned long>(2));
    FSPersistentList::Instance()->RegisterPersistent(new EEPTPRestorable<EEMap>(4));
    FSPersistentList::Instance()->RegisterPersistent(new EEPTRestorable<GE3DPoint>(GE3DPoint(), 3));
    FSPersistentList::Instance()->RegisterPersistent(new EEPPlayerHelper);
    FSPersistentList::Instance()->RegisterPersistent(new EEPTPRestorable<EEScenarioManager>(6));
    FSPersistentList::Instance()->RegisterPersistent(new EEPWorldHelper);
    FSPersistentList::Instance()->RegisterPersistent(new EEPMultiplayerHelper);
    FSPersistentList::Instance()->RegisterPersistent(new EEPTPRestorable<EEEffectManager>(0xa));
    FSPersistentList::Instance()->RegisterPersistent(new EEPCalamityHelper);
    FSPersistentList::Instance()->RegisterPersistent(new EEPTPRestorable<EEEnvironmentManager>(0xc));
    long theResult = URegisterShutdown(FSPersistentList::Shutdown);
    ESVERIFY_AT(theResult == keseOK, 1863);
}

// FUNCTION: EXE 0x53fc7c
// Creates the game timer
void EECreateGameTimer()
{
    sTimer = new TMTimer(0, 0, true);
    ESVERIFY_AT(sTimer != NULL, 1876);
}

#include "lle/GE3DLine.h"
#include "lle/GELine.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/UIControl.h"
#include "ee/EEUIFMGStatusBox.h"
#include "ee/EEUIMiniMap.h"
#include "ee/EERenderObject.h"
#include "ee/EERenderLists.h"
#include "ee/EEROUnits.h"
#include "ee/EEROTerrain.h"
#include "ee/EEROGroundShadows.h"
#include "ee/EEEffect.h"
#include "ee/EEE2DParticle.h"
#include "ee/EEE3DParticle.h"

void Fn1030AB(); // actual cdecl effect-media reset, defined in EEEffect.cpp

// FUNCTION: EXE 0x53fcbc
// Loads the status box icons, mini map, effect, unit, sky and ground shadow media
void EELoadGameMedia()
{
    EEUIFMGStatusBox::LoadIcons();
    EEUIMiniMap::LoadMedia();
    Fn1030AB();
    EEROUnits::LoadMedia();
    EEROTerrain::CreateSkyModel();
    EEROGroundShadows::CreateShadowModel();
}


// The two original zero-initialized search-path pointers, written here after the replacements are populated.
FSPath* gDataPath;
FSPath* gCivilizationDataPath; // 0x53190c (provisional name)

// FUNCTION: EXE 0x53fe97
// Builds the data search path (data archive and folder of this install, then of the original game) and the
// civilization path, replacing the previous ones
void EEShowSinglePlayerScreen()
{
    FSArchive theArchive;
    FSFileSpec theArchiveFile;
    FSFileSpec theDirectory;
    FSPath* theDataPath = new FSPath;
    FSPath* theCivilizationPath = new FSPath;
    theArchiveFile.SetVolume(gStartupVolume);
    theArchiveFile.SetDirectory(gStartupDirectory + "data\\");
    theArchiveFile.SetFile("data" + gU530B88);
    theArchive.SetArchive(theArchiveFile);
    theDataPath->AddArchive(theArchive, true);
    theArchiveFile.SetVolume(gGamePreferences.mSettings.mOriginalInstalledFromVolume);
    theArchiveFile.SetDirectory(gGamePreferences.mSettings.mOriginalInstalledFromDirectory + "data\\");
    theArchiveFile.SetFile("data" + gU530B88);
    theArchive.SetArchive(theArchiveFile);
    theDataPath->AddArchive(theArchive, true);
    theDirectory.SetVolume(gStartupVolume);
    theDirectory.SetDirectory(gStartupDirectory + "data\\");
    theDataPath->AddDirectory(theDirectory, true);
    theDirectory.SetVolume(gGamePreferences.mSettings.mOriginalInstalledFromVolume);
    theDirectory.SetDirectory(gGamePreferences.mSettings.mOriginalInstalledFromDirectory + "data\\");
    theDataPath->AddDirectory(theDirectory, true);
    theDirectory.SetVolume(gStartupVolume);
    theDirectory.SetDirectory(EEGetCivilizationDirectory(true));
    theCivilizationPath->AddDirectory(theDirectory, true);
    if (gDataPath != NULL)
        gDataPath->Dereference();
    if (gCivilizationDataPath != NULL)
        gCivilizationDataPath->Dereference();
    gCivilizationDataPath = theCivilizationPath;
    gDataPath = theDataPath;
}

// FUNCTION: EXE 0x54013d
// Creates the game state, empty
EEGameState::EEGameState()
    : mChecksum(true)
{
    this->Init();
}

// FUNCTION: EXE 0x5401da
// Destroys the game state, clearing it
EEGameState::~EEGameState()
{
    this->Clear();
}

// the game view in 1600 x 1200 layout units -> the window (reloaded at each use: named consts; 0x44217c, 0x4413bc)
static const float kGameViewScaleX = 0.000625f;
static const float kGameViewScaleY = 0.00083333335f;

// FUNCTION: EXE 0x540251
// Sets the viewport to the rectangle (given in 1600x1200 layout units, scaled to the window unless the camera is
// free) and stops camera following
void EESetGameViewRectangle(const GERectangle& inRectangle)
{
    GERectangle theRectangle;
    if (gFreeCamera == false) {
        float theWidth = (float)gGameWindowWidth;
        theRectangle.SetLeft(inRectangle.GetLeft() * kGameViewScaleX * theWidth);
        theRectangle.SetRight(inRectangle.GetRight() * kGameViewScaleX * theWidth);
        float theHeight = (float)gGameWindowHeight;
        theRectangle.SetTop(inRectangle.GetTop() * kGameViewScaleY * theHeight);
        theRectangle.SetBottom(inRectangle.GetBottom() * kGameViewScaleY * theHeight);
        theRectangle.RoundToNearest();
    } else
        theRectangle = inRectangle;
    gViewport.SetViewRectangle(theRectangle);
    gB529066 = false;
    gCameraFollowing = false;
    gB529068 = false;
}

// FUNCTION: EXE 0x540976
// Sets the mouse's bounds and clamps its position into them

// FUNCTION: EXE 0x540999
// Tells the mouse pointer thread to quit and waits until it has
// RTSMousePointer::Shutdown (inline, defined above)

#include "GEWindow.h"

// FUNCTION: EXE 0x54382b
// Creates the music manager, mouse pointer, graphics, data preloading and physics threads; registers their shutdown
void EEInitializeThreads()
{
    gMusicManager = new EEMusicManager(&gConductor->GetConductor(), 0);

    ESVERIFY_AT(gMusicManager != NULL, 2006);
    gMousePointer = new RTSMousePointer(gMouse, gGameWindow, 0);

    if (gMousePointer == NULL) ESVerify(__FILE__, 2010, "gMousePointerThread != NULL");
    gGraphics = new EEGraphics(gRasterizer, gGamePreferences.mSettings.GetScreenID(), gMouse, gUIManager, 0);

    if (gGraphics == NULL) ESVerify(__FILE__, 2018, "gGraphicsThread != NULL");
    gDataPreLoading = new EEDataPreLoading(0);

    if (gDataPreLoading == NULL) ESVerify(__FILE__, 2022, "gDataPreLoadingThread != NULL");
    gPhysics = new EEPhysics(gGraphics, sTimer);

    if (gPhysics == NULL) ESVerify(__FILE__, 2026, "gPhysicsThread != NULL");
    long theResult = URegisterShutdown(EEShutdownThreads);

    ESVERIFY_AT(theResult == keseOK, 2030);
}

// FUNCTION: EXE 0x5409bd
// Switches the screen to the mode of that size and depth (optionally the lowest refresh above 60 Hz), resizes
// the window and clears both surfaces; does nothing when the mode is already active
void EESetVideoMode(unsigned long inWidth, unsigned long inHeight, unsigned long inBitCount, bool inPreferRefresh)
{
    unsigned long theModeCount = gRasterizer->GetScreen(gGamePreferences.mSettings.GetScreenID())->GetModeCount();
    unsigned long theScreen = gGamePreferences.mSettings.GetScreenID();
    unsigned long theRefresh = 0;
    GEVideoMode theVideoMode = gRasterizer->GetScreen(theScreen)->GetCurrentMode();
    bool theChanged = theVideoMode.mWidth != inWidth || theVideoMode.mHeight != inHeight || theVideoMode.mBitCount != inBitCount;
    if (theChanged) {
    unsigned long theMode;
    if (inPreferRefresh) {
        theMode = theModeCount;
        for (unsigned long i = 0; i < theModeCount; i++) {
            theVideoMode = gRasterizer->GetScreen(theScreen)->GetMode(i);
            if (theVideoMode.mWidth == inWidth && theVideoMode.mHeight == inHeight && theVideoMode.mBitCount == inBitCount
                && (theMode == theModeCount || (theRefresh == 60 && theVideoMode.mRefreshRate > 60)
                    || (theVideoMode.mRefreshRate > 60 && theVideoMode.mRefreshRate < theRefresh))) {
                theRefresh = theVideoMode.mRefreshRate;
                theMode = i;
            }
        }
    } else {
        for (theMode = 0; theMode < theModeCount; theMode++) {
            theVideoMode = gRasterizer->GetScreen(theScreen)->GetMode(theMode);
            if (theVideoMode.mWidth == inWidth && theVideoMode.mHeight == inHeight && theVideoMode.mBitCount == inBitCount)
                break;
        }
    }

    ESVERIFY_AT(theMode < theModeCount, 2477);
    gRasterizer->GetScreen(theScreen)->SetMode(theMode);
    gGameWindow->Resize(inWidth, inHeight);
    GE2DPoint theOrigin = gGameWindow->GetClientOrigin();
    GE2DPoint theEnd(theOrigin.GetX() + (float)gGameWindow->GetWidth(), theOrigin.GetY() + (float)gGameWindow->GetHeight());
    if (gRasterizer->GetPrimarySurface(theScreen) != NULL) {
        GEColor theBlack(0.0f, 0.0f, 0.0f);
        GERectangle theRectangle(theOrigin, theEnd);
        gRasterizer->GetPrimarySurface(theScreen)->Clear(theRectangle, theBlack, 1.0f);
    }
    if (gRasterizer->GetSecondarySurface(theScreen) != NULL) {
        GEColor theBlack(0.0f, 0.0f, 0.0f);
        GERectangle theRectangle(theOrigin, theEnd);
        gRasterizer->GetSecondarySurface(theScreen)->Clear(theRectangle, theBlack, 1.0f);
    }
    theVideoMode = gRasterizer->GetScreen(theScreen)->GetMode(theMode);
    ESDebugOut("!::EESetVideoMode: Video mode changed to %dx%dx%d at %dHz.\n",
               theVideoMode.mWidth, theVideoMode.mHeight, theVideoMode.mBitCount, theVideoMode.mRefreshRate);
    } else {
        ESDebugOut("!::EESetVideoMode: Requested video mode is already active.\n");
    }
}

// SYNTHETIC: EXE 0x540ccf
// (the scalar deleting destructor, emitted with the vtable; slot EEGameState+0x0)

// FUNCTION: EXE 0x540ceb
// Stops the music, ambient sounds (disabling them) and every one-shot sound
// (EEWorld::Clear and the main game form call it)
void EEStopAllSounds()
{
    if (gMusicManager)
        gMusicManager->Stop();
    gAmbientSoundManager.StopSounds();
    gAmbientSoundManager.SetEnabled(false);
    gSoundManager->StopAllOneShotSounds();
}

void Fn104275(); // effect media cleanup, defined in EEEffect.cpp

// FUNCTION: EXE 0x540d1e
// Releases the status box icons, mini map, effect, unit, sky and ground shadow media
void EEUnloadGameMedia()
{
    EEUIFMGStatusBox::UnloadIcons();
    EEUIMiniMap::UnloadMedia();
    Fn104275();
    EEROUnits::ReleaseMedia();
    EEROTerrain::ReleaseSkyModels();
    EEROGroundShadows::ReleaseShadowModel();
}

// FUNCTION: EXE 0x540d3c
// Points the texture, model and sound managers at the data search path
void EESetDataPaths()
{
    gTextureManager->SetTexturePath(gDataPath);
    gModelManager->SetModelPath(gDataPath);
    gSoundManager->SetPath(gDataPath);
}

#include "lle/TSReadWriteLock.h"

// FUNCTION: EXE 0x5411a2
// Ends the current game: pauses graphics and physics, releases the scene, deletes the communications, and
// clears the world and game state under the world lock
void EEClearGame()
{
    gGraphics->PostPause();
    for (bool theStarted = gGraphics->mIsPaused; theStarted == false; theStarted = gGraphics->mIsPaused)
        Sleep(1);
    gPhysics->mThread->Post(0x402, NULL, NULL, true);
    while (gPhysics->mTimerRunning)
        Sleep(1);
    gGraphics->PostReleaseScene();
    gGraphics->PostMessage50AAndWait();
    gDataPreLoading->WaitForMessageQueue();
    if (gCommunications != NULL) {
        NESynchronousCommunications* theCommunications = gCommunications;
        gCommunications = NULL;
        delete theCommunications;
    }
    gWorld.mLock->WriteLock();
    gWorld.Clear();
    gGameState.Clear();
    gPhysics->PostInit();
    gGameScreenActive = false;
    gWorld.mLock->WriteUnlock();
}

// FUNCTION: EXE 0x54126f
// Window hook that swallows WM_SETCURSOR messages and passes everything else on
// (the WH_CALLWNDPROC hook)
LRESULT CALLBACK EECallWndProcHook(int inCode, WPARAM inWParam, LPARAM inLParam)
{
    CWPSTRUCT* theMessage = (CWPSTRUCT*)inLParam;
    if (inCode >= 0 && (theMessage == NULL || theMessage->message == WM_SETCURSOR))
        return 0;
    return CallNextHookEx(sCallWndProcHook, inCode, inWParam, inLParam);
}

// FUNCTION: EXE 0x541481
// Creates the game threads and registers the saved game's persistent sections
void EEInitializePart2()
{
    EEInitializeThreads();
    EESetThreadFloatingPoint("EEInitializePart2:EEInitializeThreads");
    EERegisterPersistents();
    EESetThreadFloatingPoint("EEInitializePart2:EEInitializePersistentList");
}

// FUNCTION: EXE 0x54260b
// Creates the AI scheduler with no computer players and a 1000 ms update interval

// SYNTHETIC: EXE 0x542639
// (the scalar deleting destructor, emitted with the vtable; slot EEArtificialIntelligence+0x0, stores EEArtificialIntelligence vtable)

// FUNCTION: EXE 0x54318c
// Sizes the memory pools for tile unit lists, sounds, doppelgangers and 2D/3D particles
void EEInitGamePools()
{
    EETile::SetUnitListPoolSize(500, 100);
    SESoundManager::InitializeSoundsPool(500, 100, 10);
    gDoppelgangerManager.SetPoolSize(500, 100);
    gDoppelgangerManager.SetComplexPoolSize(100, 100);
    EEE2DParticle::SetPoolSize(256, 20);
    EEE3DParticle::SetPoolSize(256, 20);
}


static HANDLE sInstanceMutex;               // 0x5319dc

// FUNCTION: EXE 0x54398d
// Creates the single-instance mutex; returns false when another copy of the game already holds it
// (called by the start-up 0x540d73)
bool EECreateInstanceMutex()
{
    sInstanceMutex = CreateMutex(NULL, FALSE, "MadDocSoftwarePresentsEmpireEarthExpansion");
    if (sInstanceMutex != NULL && GetLastError() == 0)
        return true;
    return false;
}

#include "lle/GEMovie.h"
#include "lle/ISEntity.h"

void EEChangeGameScreen(unsigned long inWidth, unsigned long inHeight, bool inFullScreen); // defined below

// FUNCTION: EXE 0x5439b5
// Plays the Sierra, Mad Doc and Empire Earth intro movies unless skipped (Escape, Space or a click skips one),
// then switches to 1024x768
void EEPlayIntroMovies(bool inSkipMovies)
{
    std::vector<UString> theMovies;
    GERectangle theRectangle(0.0f, 0.0f, 640.0f, 480.0f);
    long thePriority = TSScheduler::GetCurrentThread()->GetPriority();
    TSScheduler::GetCurrentThread()->SetPriority(THREAD_PRIORITY_NORMAL);
    SetFocus(gGameWindow->GetOSRepresentation());
    gRasterizer->SetWaitForVSync(true, gGamePreferences.mSettings.GetScreenID());
    gISMouse->Resume();
    gISKeyboard->Resume();
    theMovies.push_back("Movies\\Sierra.bik");
    theMovies.push_back("Movies\\MadDocSoftware.bik");
    theMovies.push_back("Movies\\Empire Earth.bik");
    for (std::vector<UString>::iterator it = theMovies.begin(); it != theMovies.end() && inSkipMovies == false; it++) {
        FSFileSpec* theFile;
        if (gDataPath->FindFile(*it, theFile) != kErrorOK)
            continue;
        GEMovie* theMovie = gRasterizer->CreateMovie(*theFile, gGamePreferences.mSettings.GetScreenID(), theRectangle, false);
        if (theMovie != NULL) {
            long theResult;
            do {
                unsigned long theMessage;
                void* theParameter1;
                void* theParameter2;
                gUserInterface->mThread->Receive(theMessage, theParameter1, theParameter2);
                theResult = theMovie->RenderFrame();
                if (theResult == kErrorOK)
                    theResult = gRasterizer->ShowScene(gGamePreferences.mSettings.GetScreenID());
                if (theResult != kErrorOK && theResult != 7)
                    gRasterizer->GetScreen(gGamePreferences.mSettings.GetScreenID())->RestoreAllSurfaces();
                gISMouse->Refresh();
                gISKeyboard->Refresh();
                ISEntity* theEntity;
                gISKeyboard->GetEntity(kKeyEscape, theEntity);
                if (theEntity->JustPressed())
                    theResult = 7;
                gISKeyboard->GetEntity(kKeySpace, theEntity);
                if (theEntity->JustPressed())
                    theResult = 7;
                gISMouse->GetEntity(kMouseLeftButton, theEntity);
                if (theEntity->JustPressed())
                    theResult = 7;
                Sleep(1);
            } while (theResult != 7);
            gRasterizer->Clear(GEColor(0.0f, 0.0f, 0.0f), theRectangle, gGamePreferences.mSettings.GetScreenID());

            gRasterizer->ShowScene(gGamePreferences.mSettings.GetScreenID());
            theMovie->Stop();
            theMovie->Dereference();
        }
        delete theFile;
    }
    gRasterizer->Clear(GEColor(0.0f, 0.0f, 0.0f), theRectangle, gGamePreferences.mSettings.GetScreenID());

    gRasterizer->ShowScene(gGamePreferences.mSettings.GetScreenID());
    EEChangeGameScreen(1024, 768, false);
    gISMouse->Suspend();
    gISKeyboard->Suspend();
    gRasterizer->SetWaitForVSync(gGamePreferences.mSettings.mWaitForVSync, gGamePreferences.mSettings.GetScreenID());
    TSScheduler::GetCurrentThread()->SetPriority(thePriority);
}

// FUNCTION: EXE 0x543cf0
// Closes the single-instance mutex, ends the debug recording and saves the preferences
// (no direct caller: registered as a shutdown routine)
void EEReleaseInstanceMutex(bool inShutdown)
{
    CloseHandle(sInstanceMutex);
    ESEndRecording();
    gGamePreferences.Save();
}

// Database shutdown is defined with the startup family below.


#include "ee/EEDisplayDevices.h"

extern std::vector<GETexture*> gU4E8EF0; // defined in EERenderObject.cpp
void Fn102E1B(); // effect-render cleanup, defined in EEEffect.cpp
static unsigned long sOriginalVideoMode; // 0x52a1a8: the video mode index found at start-up, restored at shutdown

// FUNCTION: EXE 0x543ece
// Releases models, textures and fonts, restores the start-up video mode, destroys the game window and deletes
// the render objects and the texture and model managers
void EEGraphicsShutdown(bool inShutdown)
{
    gModelManager->RemoveAllModels();
    for (unsigned long i = 0; i < gU4E8EF0.size(); i++)
        gU4E8EF0[i]->Dereference();
    gTextureManager->RemoveAllTextures();
    gRasterizer->GetScreen(gGamePreferences.mSettings.GetScreenID())->SetMode(sOriginalVideoMode);
    gRasterizer->GetScreen(gGamePreferences.mSettings.GetScreenID())->Release(gGameWindow);
    gFontTimesNewRoman9->Dereference();
    gFontTimesNewRoman10->Dereference();
    gFontTimesNewRoman12->Dereference();
    gFontTimesNewRoman14->Dereference();
    gFontArial8->Dereference();
    gFontArial9->Dereference();
    gU531930->Dereference();
    gU531934->Dereference();
    gU531938->Dereference();
    gFontArial16->Dereference();
    gFontArial18->Dereference();
    gFontArial20->Dereference();
    gFontArial24->Dereference();
    gFontArial36->Dereference();
    gFontArial48->Dereference();
    gFontArial72->Dereference();
    gFontArialBold8->Dereference();
    gFontArialBold9->Dereference();
    gFontArialBold10->Dereference();
    gFontArialBold12->Dereference();
    gFontArialBold14->Dereference();
    gFontArialBold16->Dereference();
    gFontCourier8->Dereference();
    gFontCourier10->Dereference();
    gFontCourier12->Dereference();
    gFontCourier14->Dereference();
    gFontTahoma8->Dereference();
    gU531994->Dereference();
    gFontTahoma10->Dereference();
    gFontTahoma12->Dereference();
    gFontTahoma14->Dereference();
    gFontTahoma16->Dereference();
    gFontTahoma18->Dereference();
    gFontTahoma20->Dereference();
    gFontTahoma24->Dereference();
    gFontTahoma36->Dereference();
    gFontTahoma48->Dereference();
    gFontTahoma72->Dereference();
    gDisplayRasterizers.clear();
    delete gGameWindow;
    gGameWindow = NULL;
    UnregisterClassA("Empire Earth Expansion", gInstance);
    Fn102E1B();
    gRasterizer->Deactivate();
    delete gU565350;
    gU565350 = NULL;
    delete gU565358;
    gU565358 = NULL;
    delete gU56534C;
    gU56534C = NULL;
    delete gU56535C;
    gU56535C = NULL;
    delete gU565354;
    gU565354 = NULL;
    delete gU565360;
    gU565360 = NULL;
    delete gTextureManager;
    gTextureManager = NULL;
    delete gModelManager;
    gModelManager = NULL;
}

// SYNTHETIC: EXE 0x54423e
// (the scalar deleting destructor, emitted with the vtable; )

// SYNTHETIC: EXE 0x54425b
// (the scalar deleting destructor, emitted with the vtable; )

// SYNTHETIC: EXE 0x544278
// (the scalar deleting destructor, emitted with the vtable; )

#include "ee/EECPILocation.h"
#include "ee/EECPAttackSubGroup.h"
#include "ee/EEMiniMap.h"

class GELODPolygon;
class GELODVertex;

// FUNCTION: EXE 0x544295
// Releases the particle and complex unit pools and logs the list node pools' statistics
// (EEDeleteDatabase passes true; the argument is not read)
void EEReleaseGamePools(bool inShutdown)
{
    EEE3DParticle::ReleasePool();
    EEE2DParticle::ReleasePool();
    EEComplexUnit::ReleasePool();
    UList<EEMiniMapDot>::sNodePool.DumpStats(false);
    UList<EEMiniMapUnitDot>::sNodePool.DumpStats(false);
    UList<GELODPolygon*>::sNodePool.DumpStats(false);
    UList<GELODVertex*>::sNodePool.DumpStats(false);
    UList<EEComplexUnit*>::sNodePool.DumpStats(false);
}

// FUNCTION: EXE 0x5442e0
// Releases the attack target, tile unit list and doppelganger pools
void EEReleaseUnitPools()
{
    EECPAttackSubGroup::ReleaseTargetPool();
    EETile::ReleaseUnitListPool();
    gDoppelgangerManager.Release();
}

// FUNCTION: EXE 0x5442f4
// Does nothing (the network start-up registers it as its shutdown routine)
void EEEmptyShutdown(bool inShutdown)
{
}

// FUNCTION: EXE 0x5442f5
// Deletes the sound manager, logs its memory pool and frees the pool and its locks
// (no direct caller: registered as a shutdown routine)
void EEDeleteSoundManager(bool inShutdown)
{
    delete gSoundManager;
    gSoundManager = NULL;
    SESoundManager::DumpMemoryPoolInfo();
    SESoundManager::CleanupMemoryPoolAndDestroyCriticalSections();
}

#include "ee/EEWonNet.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUnitPicker.h"

// FUNCTION: EXE 0x544324
// Stops physics, deletes the communications and WON connection, then shuts down and deletes the physics, data
// preloading, graphics and mouse pointer threads, the music manager and the user interface
void EEShutdownThreads(bool inShutdown)
{
    gPhysics->mThread->Post(0x402, NULL, NULL, true);
    while (gPhysics->mTimerRunning)
        Sleep(1);
    delete gCommunications;
    gCommunications = NULL;
    EEWonNet* theWonNet = gWonNet;
    gWonNet = NULL;
    delete theWonNet;
    gMousePointer->mThread->Post(0x402, NULL, NULL, true);
    while (gMousePointer->mIsShown)
        Sleep(1);
    gGraphics->PostPause();
    for (bool theStarted = gGraphics->mIsPaused; theStarted == false; theStarted = gGraphics->mIsPaused)
        Sleep(1);
    gPhysics->Shutdown();
    delete gPhysics;
    gPhysics = NULL;
    gDataPreLoading->Shutdown();
    delete gDataPreLoading;
    gDataPreLoading = NULL;
    gGraphics->Shutdown();
    delete gGraphics;
    gGraphics = NULL;
    gMousePointer->Shutdown();
    delete gMousePointer;
    gMousePointer = NULL;
    delete gMusicManager;
    gMusicManager = NULL;
    delete gUserInterface;
    gUserInterface = NULL;
    gUnitPicker.Reset();
}

// FUNCTION: EXE 0x544488
// Tells the physics thread to quit and waits until it has
// RTSPhysics::Shutdown (inline, defined above)

// FUNCTION: EXE 0x5444ac
// Tells the data preloading thread to quit and waits until it has
// EEDataPreLoading::Shutdown (inline, defined above)

#include "ee/EECameraControl.h"
#include "ee/EEButtonObject.h"

void Fn10269C(); // effect reset, defined in EEEffect.cpp
extern bool sIconsLoaded; // the status box's icons-loaded flag (defined in EEUIFMGStatusBox's unit)

// FUNCTION: EXE 0x540306
// Re-creates the display at the new size (full screen or not): reactivates the rasterizer, resets render states
// and the camera, reloads mouse, textures, buttons and icons and restarts the mouse pointer thread
void EEChangeGameScreen(unsigned long inWidth, unsigned long inHeight, bool inFullScreen)
{
    GERectangle theGameView;
    GE2DPoint theOrigin = gGameWindow->GetClientOrigin();
    unsigned long theScreen = gGamePreferences.mSettings.GetScreenID();
    GERectangle theOldRectangle(theOrigin, GE2DPoint(theOrigin.mX + (float)gGameWindow->GetWidth(),
                                                    theOrigin.GetY() + (float)gGameWindow->GetHeight()));
    unsigned long thePrecision = UMGetFloatingPointPrecision();
    bool theHadPointer = false;
    if (gMousePointer != NULL) {
        theHadPointer = true;
        gMousePointer->Shutdown();
        delete gMousePointer;
        gMousePointer = NULL;
    }
    gMouse->Release();
    if (gRasterizer->GetIsActivated()) {
        GEColor theBlack(0.0f, 0.0f, 0.0f);
        gRasterizer->GetPrimarySurface(theScreen)->Clear(theOldRectangle, theBlack, 1.0f);
        theBlack = GEColor(0.0f, 0.0f, 0.0f);
        gRasterizer->GetSecondarySurface(theScreen)->Clear(theOldRectangle, theBlack, 1.0f);
        gRasterizer->GetScreen(theScreen)->Release(gGameWindow);
        Fn102E1B();
        gRasterizer->Deactivate();
    }
    long theResult = gRasterizer->Activate();

    ESVERIFY_AT(theResult == keseOK, 2978);
    Fn10269C();
    theResult = gRasterizer->GetScreen(theScreen)->Capture(gGameWindow, gGamePreferences.mSettings.GetFullScreen());

    ESVERIFY_AT(theResult == keseOK, 2987);
    EESetVideoMode(inWidth, inHeight, gGamePreferences.mSettings.mGameBitDepth, true);
    gGameWindow->Resize(inWidth, inHeight);
    theResult = gRasterizer->ActivateScreen(theScreen, gTextureManager, gGameWindow,
                                           gGamePreferences.mSettings.GetScreenMode(), inFullScreen,
                                           gGamePreferences.mSettings.mGameBitDepth == 16 ? 12 : 13);

    ESVERIFY_AT(theResult == keseOK, 3004);
    if (gGraphics != NULL)
        gGraphics->SetSurface(gRasterizer->GetSecondarySurface(theScreen));
    gWorld.mCameraControl.ResetZoom();
    gWorld.mCameraControl.SetChanged();
    theGameView.SetLeft(kStartViewLeft); theGameView.SetRight(kStartViewRight); theGameView.SetTop(kStartViewTop); theGameView.SetBottom(kStartViewBottom);
    EESetGameViewRectangle(theGameView);
    gRasterizer->SetFog(0, theScreen);
    GEColor theColor(0.0f, 0.0f, 0.0f);
    gRasterizer->SetFogColor(theColor, theScreen);
    gRasterizer->SetFogType(0, theScreen);
    theColor = GEColor(1.0f, 1.0f, 1.0f);
    gRasterizer->SetAmbientColor(theColor, theScreen);
    gRasterizer->SetAmbientIntensity(0.0f, theScreen);
    gRasterizer->SetColorKeying(1, theScreen);
    gRasterizer->SetCulling(kCullCW);
    gRasterizer->SetLighting(1, theScreen);
    gRasterizer->SetShading(1, theScreen);
    gRasterizer->SetSpecular(0, theScreen);
    gRasterizer->SetZBuffer(kZBufferDisabled, theScreen);
    gRasterizer->SetZCompare(kCompareLessEqual, theScreen);
    gRasterizer->SetDithering(1, theScreen);
    gRasterizer->SetAntiAliasing(gGamePreferences.mSettings.mAntiAliasing != false, theScreen);
    gRasterizer->SetAlphaTesting(1, theScreen);
    gRasterizer->SetAlphaTestingCompareState(kCompareGreater, theScreen);
    gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    gRasterizer->SetWaitForVSync(gGamePreferences.mSettings.mWaitForVSync, theScreen);
    gRasterizer->SetWaterRippleEffect(kWaterRipplePeriod, kWaterRippleSpeed, kWaterRippleAmplitude);
    GERectangle theRectangle(theOrigin, GE2DPoint(theOrigin.mX + (float)gGameWindow->GetWidth(),
                                                 theOrigin.mY + (float)gGameWindow->GetHeight()));
    theColor = GEColor(0.0f, 0.0f, 0.0f);
    gRasterizer->GetPrimarySurface(theScreen)->Clear(theRectangle, theColor, 1.0f);
    theColor = GEColor(0.0f, 0.0f, 0.0f);
    theRectangle = GERectangle(theOrigin, GE2DPoint(theOrigin.mX + (float)gGameWindow->GetWidth(),
                                                 theOrigin.mY + (float)gGameWindow->GetHeight()));
    gRasterizer->GetSecondarySurface(theScreen)->Clear(theRectangle, theColor, 1.0f);
    theRectangle = GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight());
    gMouse->SetBounds(theRectangle);
    gMouse->Load(gRasterizer->GetScreen(theScreen));
    theResult = gTextureManager->RefreshAllTextures();

    ESVERIFY_AT(theResult == keseOK, 3070);
    EEButtonObject::Reload();
    if (sIconsLoaded == true) {
        EEUIFMGStatusBox::ReloadIcons();
        EEUIFMGStatusBox::UnloadIcons();
        EEUIFMGStatusBox::LoadIcons();
    }
    if (theHadPointer) {
        gMousePointer = new RTSMousePointer(gMouse, gGameWindow, 0);

        if (gMousePointer == NULL) ESVerify(__FILE__, 3094, "gMousePointerThread != NULL");
    }
    if (gMousePointer != NULL)
        gMousePointer->mThread->Post(0x405, gRasterizer->GetPrimarySurface(gGamePreferences.mSettings.GetScreenID()), NULL, true);
    UMSetFloatingPointPrecision(thePrecision);
    if (UMGetFloatingPointRoundingMode() != 0)
        UMSetFloatingPointRoundingMode(0);
}

#include "lle/Graphics Engine.h"
#include "ee/EEUserInterface.h"

void EEEnumerateDisplayDevices(); // 0x548514
void Fn10269C(); // effect reset, defined in EEEffect.cpp
GERasterizer* gRasterizer;
GEWindow* gGameWindow;

// FUNCTION: EXE 0x5444d0
// Starts the graphics engine, picks the preferred rasterizer, validates screen, depths and resolution (falling
// back to 800x600x16), creates and captures the game window; returns true
bool EEInitializeDisplay()
{
    GE2DPoint theOrigin;
    FSFileSpec theDirectory;
    theDirectory.SetDirectory(gStartupDirectory);
    long theResult = GEInitialize(theDirectory, gInstance);

    ESVERIFY_AT(theResult == keseOK, 3369);
    EEEnumerateDisplayDevices();
    unsigned long theRasterizer;
    for (theRasterizer = 0; theRasterizer < GERasterizer::GetRasterizerCount(); theRasterizer++) {
        gRasterizer = GERasterizer::GetRasterizer(theRasterizer);
        if (gRasterizer->GetName() == gGamePreferences.mSettings.mRasterizerName)
            break;
    }
    if (theRasterizer < GERasterizer::GetRasterizerCount()) {
        gRasterizer = GERasterizer::GetRasterizer(theRasterizer);
    } else {
        gRasterizer = GERasterizer::GetRasterizer(0);
        gGamePreferences.mSettings.mRasterizerName = gRasterizer->GetName();
    }
    theResult = gRasterizer->Activate();

    ESVERIFY_AT(theResult == keseOK, 3402);
    if (gRasterizer->GetScreenCount() <= gGamePreferences.mSettings.mScreenID)
        gScreenIndex = 0;
    gRasterizer->SetScreenID(gGamePreferences.mSettings.GetScreenID());
    if ((gGamePreferences.mSettings.mGameBitDepth != 32 && gGamePreferences.mSettings.mGameBitDepth != 16)
        || (gGamePreferences.mSettings.mGameBitDepth == 32 && !gRasterizer->GetScreen(gGamePreferences.mSettings.GetScreenID())->Get32BitDeviceSupport()))
        gGamePreferences.mSettings.mGameBitDepth = 16;
    if ((gGamePreferences.mSettings.mTextureBitDepth != 32 && gGamePreferences.mSettings.mTextureBitDepth != 16)
        || (gGamePreferences.mSettings.mTextureBitDepth == 32 && !gRasterizer->GetScreen(gGamePreferences.mSettings.GetScreenID())->Get32BitTextureSupport()))
        gGamePreferences.mSettings.mTextureBitDepth = 16;
    GEScreen* theScreen = gRasterizer->GetScreen(gGamePreferences.mSettings.GetScreenID());
    unsigned long theMode;
    for (theMode = 0; theMode < theScreen->GetModeCount(); theMode++) {
        if (theScreen->GetMode(theMode).mWidth == gGameWindowWidth
            && theScreen->GetMode(theMode).mHeight == gGameWindowHeight
            && theScreen->GetMode(theMode).mBitCount == gGamePreferences.mSettings.mGameBitDepth)
            break;
    }
    if (theScreen->GetModeCount() <= theMode) {
        gGameWindowWidth = 800;
        gGameWindowHeight = 600;
        gGamePreferences.mSettings.mGameBitDepth = 16;
    }
    gGamePreferences.Save();
    Fn10269C();
    WNDCLASSEXA theClass;
    theClass.cbSize = sizeof(theClass);
    theClass.style = CS_HREDRAW | CS_VREDRAW;
    theClass.lpfnWndProc = EEUserInterface::WindowProc;
    theClass.cbClsExtra = 0;
    theClass.cbWndExtra = 0;
    theClass.hInstance = gInstance;
    theClass.hIcon = LoadIcon(gInstance, MAKEINTRESOURCE(0x66));
    theClass.hCursor = LoadCursor(gInstance, MAKEINTRESOURCE(0x68));
    theClass.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    theClass.lpszMenuName = NULL;
    theClass.lpszClassName = "Empire Earth Expansion";
    theClass.hIconSm = NULL;
    RegisterClassExA(&theClass);
    GERectangle theDesktop = gRasterizer->GetScreen(gGamePreferences.mSettings.GetScreenID())->GetDesktopRectangle();
    theScreen = gRasterizer->GetScreen(gGamePreferences.mSettings.GetScreenID());
    theOrigin.SetXY(theDesktop.GetLeft(), theDesktop.GetTop());
    if (gGamePreferences.mSettings.GetFullScreen() == false) {
        theResult = theScreen->CreateAWindow(theOrigin, 640, 480, UString("Empire Earth Expansion"),
                                             UString("Empire Earth: The Art of Conquest"), gGameWindow, 1, NULL);
    } else {
        theResult = theScreen->CreateAWindow(theOrigin, (unsigned long)theDesktop.GetWidth(),
                                             (unsigned long)theDesktop.GetHeight(), UString("Empire Earth Expansion"),
                                             UString("Empire Earth: The Art of Conquest"), gGameWindow, 0, NULL);
    }

    ESVERIFY_AT(theResult == keseOK, 3523);
    theResult = gRasterizer->GetScreen(gGamePreferences.mSettings.GetScreenID())->Capture(gGameWindow, gGamePreferences.mSettings.GetFullScreen());

    ESVERIFY_AT(theResult == keseOK, 3527);
    sOriginalVideoMode = gRasterizer->GetScreen(gGamePreferences.mSettings.GetScreenID())->GetCurrentModeIndex();
    return true;
}

#include <objbase.h>

bool EEParseCommandLineFirstPass(const char* inCommandLine, bool& outShowMovies); // 0x52d9e5
bool EEParseCommandLine(const char* inCommandLine); // 0x52d703 (after start-up)
HINSTANCE gInstance;
UResourceFile* gResource;
#pragma optimize("g", off)   // rule 4, owner 2026-11-01: WinMain is no-/Og (memory locals, constant push 1/pop eax test)
// FUNCTION: EXE 0x540d73
// Runs the game: single-instance check, start-up, splash and intro, the user interface loop, then shutdown;
// returns 1 when the original Empire Earth install is missing, else 0
// (the previous instance and show command arguments are unused)
int WINAPI WinMain(HINSTANCE inInstance, HINSTANCE inPreviousInstance, LPSTR inCommandLine, int inShowCommand)
{
    EEStartScreen theStartScreen(inInstance);
    FSFileSpec theLanguageFile;
    FSFileSpec theRecordingFile;
    if (EECreateInstanceMutex()) {
        sCallWndProcHook = SetWindowsHookExA(WH_CALLWNDPROC, EECallWndProcHook, NULL, 0);
        HRESULT theHResult = CoInitialize(NULL);

        ESVERIFY_AT(theHResult == S_OK || theHResult == S_FALSE, 3600);
        UMSetFloatingPointPrecision(kFPPrecision53);
        UMFastMath::BuildSqrtTable();
        bool theShowMovies;
        if (EEParseCommandLineFirstPass(inCommandLine, theShowMovies)) {
            gInstance = inInstance;
            ESInitialize(inInstance);
            FSInitialize(UString(""));
            theRecordingFile.SetVolume(gStartupVolume);
            theRecordingFile.SetDirectory(gStartupDirectory);
            ESBeginRecording(theRecordingFile);
            theLanguageFile.SetVolume(gStartupVolume);
            theLanguageFile.SetDirectory(gStartupDirectory);
            theLanguageFile.SetFile(UString("Language.dll"));
            gResource = new UResourceFile(theLanguageFile);

            ESVERIFY_AT(gResource, 3632);
            EEInitializeDefaultSettings();
            long theResult = gGamePreferences.Load();

            ESVERIFY_AT(theResult == keseOK, 3637);
            if (gGamePreferences.mSettings.mOriginalInstalledFromDirectory == "") {
                UWideString theMessage;
                UWideString theTitle;
                gResource->GetString(IDS_EMPIRE_EARTH_ART_OF_CONQUEST_REQUIRES_ORIGINAL, theMessage);
                gResource->GetString(IDS_ERROR_CANT_RUN, theTitle);
                MessageBoxA(NULL, theMessage, theTitle, 0);
                CoUninitialize();
                UnhookWindowsHookEx(sCallWndProcHook);
                return 1;
            }
            EELoadNetworkSettings();
            EEShowSinglePlayerScreen();
            theStartScreen.Start();
            bool theInitialized = EEInitializePart1();
            theStartScreen.Stop();
            if (theInitialized) {
                EEPlayIntroMovies(theShowMovies == false);
                EEInitializePart2();
                if (EEParseCommandLine(inCommandLine)) {
                    long theResult;
                    {
                        UString theCommandLine(inCommandLine);
                        theResult = gUserInterface->Run(&theCommandLine);
                    }
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
        CoUninitialize();
        UnhookWindowsHookEx(sCallWndProcHook);
    }
    return 0;
}
#pragma optimize("g", on)   // ??_GUResourceFile 0x54494d (no-/Og in the original) is emitted under WinMain's state
// FUNCTION: EXE 0x54494d
// Destroys the resource file and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x544977
// Returns an iterator to the first world table entry

// FUNCTION: EXE 0x54499e
// Returns an iterator past the last world table entry

// FUNCTION: EXE 0x5449c5
// Appends a copy of the file spec to the vector

// FUNCTION: EXE 0x5449f6
// Copies a world table entry iterator

// FUNCTION: EXE 0x544a15
// Empties the display rasterizer list

// TEMPLATE: EXE 0x544a68
// USingleton<EECalamityManager>::DestroyInstance (inline; EEDeleteDatabase): deletes the calamity manager

// FUNCTION: EXE 0x544aa0
// Creates a new saved-game number section with the same tag

// FUNCTION: EXE 0x544b08
// Adds nothing to the checksum (value sections)
// (shared by EEPTGeneric<unsigned long> and EEPTRestorable<GE3DPoint>)

// FUNCTION: EXE 0x544b13
// Reads the section's number from the saved game; a short read fails with error 0xe

// FUNCTION: EXE 0x544b59
// Writes the section's number to the saved game; a short write fails with error 0xe

// FUNCTION: EXE 0x544ba2
// Creates a new, empty saved-game map section with the same tag

// FUNCTION: EXE 0x544c38
// Loads the section's object from the saved game
// (shared by EEPTPRestorable<EEMap>, <EEScenarioManager>, <EEEffectManager> and <EEEnvironmentManager>)

// FUNCTION: EXE 0x544c68
// Saves the section's object to the saved game
// (shared by EEPTPRestorable<EEMap>, <EEScenarioManager>, <EEEffectManager> and <EEEnvironmentManager>)

// FUNCTION: EXE 0x544c98
// Creates a saved-game point section with the given tag and point

// FUNCTION: EXE 0x544d1b
// Creates a new saved-game point section (at the origin) with the same tag

// FUNCTION: EXE 0x544d86
// Reads the section's point from the saved game

// FUNCTION: EXE 0x544dab
// Writes the section's point to the saved game

// FUNCTION: EXE 0x544dd0
// Creates a new, empty saved-game scenario section with the same tag

// FUNCTION: EXE 0x544e66
// Creates a new, empty saved-game effects section with the same tag

// FUNCTION: EXE 0x544ef9
// Creates a new, empty saved-game environment section with the same tag

// FUNCTION: EXE 0x544f8c
// Adds the section's object to the checksum
// (shared by EEPTPRestorable<EEMap>, <EEScenarioManager>, <EEEffectManager> and <EEEnvironmentManager>)

// FUNCTION: EXE 0x544faa
// Destroys a saved-game value section and frees it when asked (scalar deleting destructor)
// (shared by EEPTGeneric<unsigned long> and EEPTRestorable<GE3DPoint>)

// FUNCTION: EXE 0x544fd6
// Destroys the saved-game map section and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x545001
// Destroys the saved-game map section, deleting the map when it owns it

// FUNCTION: EXE 0x54505d
// Destroys the saved-game scenario section and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x545088
// Destroys the saved-game scenario section, deleting the scenario manager when it owns it

// FUNCTION: EXE 0x5450e4
// Destroys the saved-game effects section and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x54510f
// Destroys the saved-game effects section, deleting the effect manager when it owns it

// FUNCTION: EXE 0x54516b
// Destroys the saved-game environment section and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x545196
// Destroys the saved-game environment section, deleting the environment manager when it owns it

// FUNCTION: EXE 0x5451f2
// Inserts a copy of the file spec before the position; returns an iterator to it

// FUNCTION: EXE 0x5452a6
// Allocates room for the given number of file specs; returns whether any was allocated

// FUNCTION: EXE 0x545305
// Destroys every file spec in the vector and frees its storage

// FUNCTION: EXE 0x545388
// Removes the display rasterizers in the range; returns an iterator to the first one after it

// FUNCTION: EXE 0x54541f
// Allocates room for the given number of unit set pointers; returns whether any was allocated

// FUNCTION: EXE 0x54547c
// Inserts the given number of copies of the file spec before the position, growing as needed

// FUNCTION: EXE 0x545b5d
// Destroys the file specs in the range

// FUNCTION: EXE 0x545b89
// Returns the file spec iterator moved forward by the count

// FUNCTION: EXE 0x545bb7
// Copy-constructs the given number of copies of the file spec into raw storage; returns the end

// FUNCTION: EXE 0x545c41
// Throws length_error for a file spec vector that grew too long

// FUNCTION: EXE 0x545cbd
// Returns the file spec iterator moved back by the count

// FUNCTION: EXE 0x545ceb
// Prints the complex unit list node pool's statistics (to the debugger when asked)

// FUNCTION: EXE 0x545d72
// Prints the mini map dot list node pool's statistics (to the debugger when asked)

// FUNCTION: EXE 0x545df9
// Prints the mini map unit dot list node pool's statistics (to the debugger when asked)

// FUNCTION: EXE 0x545e80
// Sorts the file specs by file name

// FUNCTION: EXE 0x545f4f
// Prints the LOD vertex list node pool's statistics (to the debugger when asked)

// FUNCTION: EXE 0x545fd6
// Prints the LOD polygon list node pool's statistics (to the debugger when asked)

// FUNCTION: EXE 0x54605d
// Copies a range of display rasterizers; returns the end of the destination

// FUNCTION: EXE 0x5460a0
// Copy-constructs a range of file specs into raw storage; returns the end

// FUNCTION: EXE 0x546146
// Assigns the file spec to every element of the range

// FUNCTION: EXE 0x54617e
// Copies a range of file specs backwards to end at the destination; returns its start

// FUNCTION: EXE 0x5461c8
// Destroys one display rasterizer in place

// FUNCTION: EXE 0x5461f6
// Copy-constructs a file spec in place

#include "lle/SECD.h"
#include "lle/UMFastMath.h"
#include <string.h>

// FUNCTION: EXE 0x53fcda
// Loads and starts a .wav or .mp3 sound (a bare name tries .wav, then .mp3), registering it as a one-shot when
// asked; returns the sample and instance, -1 on failure
void EELoadSound(const UString& inName, unsigned long inType, unsigned long& outSample,
                 unsigned long& outInstance, unsigned long inFlag)
{
    FSFileSpec theUnusedFile;
    UString theOtherName;
    UString theName;
    unsigned long theSample = (unsigned long)-1;
    unsigned long theInstance = (unsigned long)-1;
    if (inName.GetLength() > 4 &&
        (inName.GetSubstring(inName.GetLength() - 4, 4).EqualsIgnoreCase(".wav", 4) ||
         inName.GetSubstring(inName.GetLength() - 4, 4).EqualsIgnoreCase(".mp3", 4))) {
        theSample = gSoundManager->Load(inName, inType, 1);
        if (theSample != (unsigned long)-1) {
            theInstance = gSoundManager->Play(theSample, 1.0f, 0.0f, 1.0f, false, NULL);
            if (theInstance == (unsigned long)-1) {
                gSoundManager->Unload(theSample);
                theSample = (unsigned long)-1;
            } else if ((unsigned char)inFlag == 1)
                gSoundManager->RegisterOneShotSound(theSample, theInstance);
        }
    } else {
        theName = inName;
        theName += ".wav";
        EELoadSound(theName, inType, theSample, theInstance, inFlag);
        if (theSample == (unsigned long)-1) {
            theName = inName;
            theName += ".mp3";
            EELoadSound(theName, inType, theSample, theInstance, inFlag);
        }
    }
    outSample = theSample;
    outInstance = theInstance;
}

#include "ee/EEDbObject.h"
#include "ee/EEStartingResources.h"
#include "ee/EEMedia.h"
#include "lle/USingleton.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EESoundObject.h"
#include "ee/EEMusicObject.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEButtonObject.h"
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
void EEBuildSightRings(); // the start-up's sight distance tables
long EEClearRandomMapGenerators(); // clears the random-map generator list; returns 0

// FUNCTION: EXE 0x543557
// Starts the sound engine and music conductor from the preferences and creates the sound manager; returns false
// if the sound engine fails, else no defined value (see below)
// The original's success path returns no value (after the three destructors AL holds whatever the last
// one left), yet the caller tests AL
bool EEInitializeSounds()
{
    FSFileSpec theStartupDirectory;
    FSFileSpec theUnusedFile;
    UWideString theMusicDirectory(260);
    theStartupDirectory.SetDirectory(gStartupDirectory);
    if (SEInitialize(theStartupDirectory, gGameWindow, false) != kErrorOK)
        return false;
    long theResult = SEMixer::GetMixer(0, gConductor);
    if (theResult != kErrorOK) ESVerify(__FILE__, 0x770, "theResult == keseOK");
    theResult = SEMixer::UseMixer(gConductor);
    if (theResult != kErrorOK) ESVerify(__FILE__, 0x772, "theResult == keseOK");
    theResult = gConductor->GetConductor().SetChannelGroupCount(8);
    if (theResult != kErrorOK) ESVerify(__FILE__, 0x775, "theResult == keseOK");
    if (gGamePreferences.mSettings.mMusicEnabled) {
        theResult = gConductor->GetConductor().SetConductorState(1);
        if (theResult != kErrorOK && theResult != 2)
            ESVerify(__FILE__, 0x77a, "theResult == keseOK || theResult == keseAlready");
    } else {
        theResult = gConductor->GetConductor().SetConductorState(0);
        if (theResult != kErrorOK) ESVerify(__FILE__, 0x77f, "theResult == keseOK");
    }
    if (gGamePreferences.mSettings.mReverbEnabled) gConductor->GetConductor().SetReverb(1); else gConductor->GetConductor().SetReverb(0);
    gConductor->GetConductor().SetVolume(gGamePreferences.mSettings.mMusicVolume);
    gConductor->GetConductor().SetIntensity(0, false);
    sprintf(theMusicDirectory, "%s%s%s", (const char*)gGamePreferences.mSettings.mOriginalInstalledFromVolume,
            (const char*)gGamePreferences.mSettings.mOriginalInstalledFromDirectory, "Data\\Music\\");
    theResult = gConductor->GetConductor().SetMusicFolder(theMusicDirectory);
    if (theResult != kErrorOK) ESVerify(__FILE__, 0x7a4, "theResult == keseOK");
    gSoundManager = new SESoundManager(gDataPath, gConductor, 50, 0);
    if (gSoundManager == NULL) ESVerify(__FILE__, 0x7a8, "gSoundManager");
    float thePercentage = gGamePreferences.mSettings.mMaxNumberOfSounds;
    float theChannels = (float)gSoundManager->GetMaxChannels() * thePercentage;
    unsigned long theMaximum = UMFloatToLong(theChannels);
    gSoundManager->SetMaxChannels(theMaximum < 8 ? 8 : theMaximum);
    gSoundManager->SetMasterVolume(gGamePreferences.mSettings.mSoundVolume);
    gSoundManager->SetCategoryPriority(0x31, 0);
    gSoundManager->SetCategoryPercentage(0x31, 1.0f);
    SECD::SetNotificationRecipient(gGameWindow);
    gSoundManager->SetLatency(200);
    theResult = URegisterShutdown(EEDeleteSoundManager);
    if (theResult != kErrorOK) ESVerify(__FILE__, 0x7c4, "theResult == keseOK");
}

// FUNCTION: EXE 0x5414a3
// Loads every database table in start-up order, creates the user folders (campaigns, saved games, scenarios,
// users), loads the ambient sounds, sizes the game pools and registers the database shutdown
void EEInitializeDatabase()
{
    FSFileSpec theDirectory;
    UString thePath;
    UWideString theUserDirectory;
    FSFileSpec* theDbFile;
    long theResult;
    theResult = gDataPath->FindFile(kDbStartingResourcesFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 909, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 910, "theDbFile != NULL");
    theResult = EEStartingResources::Load(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 912, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbUnitBehaviorFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 917, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 918, "theDbFile != NULL");
    theResult = EEUnitBehavior::Load(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 920, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbSoundsFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 925, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 926, "theDbFile != NULL");
    theResult = EESoundObject::LoadTable(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 928, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbGraphicsFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 933, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 934, "theDbFile != NULL");
    theResult = EEGraphicObject::LoadTable(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 936, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbGfxEffectsFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 941, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 942, "theDbFile != NULL");
    theResult = EEGraphicEffect::LoadTable(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 944, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbMusicFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 949, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 950, "theDbFile != NULL");
    theResult = EEMusicObject::LoadTable(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 952, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbWorldFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 962, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 963, "theDbFile != NULL");
    theResult = gWorldTable.Load(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 965, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbFamilyFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 970, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 971, "theDbFile != NULL");
    theResult = EETerrainTypeTable::LoadDamageTables(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 973, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbButtonsFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 978, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 979, "theDbFile != NULL");
    theResult = EEButtonObject::Load(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 981, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbTechTreeFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 986, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 987, "theDbFile != NULL");
    theResult = gTechTree.Load(*theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 989, "theResult == keseOK");
    delete theDbFile;
    FSFileSpec* theEvents;
    FSFileSpec* theEffects;
    theResult = gDataPath->FindFile(kDbEventsFile, theEvents);
    if (theResult != kErrorOK) ESVerify(__FILE__, 0x3e2, "theResult == keseOK");
    if (theEvents == NULL) ESVerify(__FILE__, 0x3e3, "theDbEventsFile");
    theResult = gDataPath->FindFile(kDbEffectsFile, theEffects);
    if (theResult != kErrorOK) ESVerify(__FILE__, 0x3e5, "theResult == keseOK");
    if (theEffects == NULL) ESVerify(__FILE__, 0x3e6, "theDbEffectsFile");
    theResult = EEEvent::LoadEvents(*theEvents, *theEffects);
    if (theResult != kErrorOK) ESVerify(__FILE__, 0x3e8, "theResult == keseOK");
    delete theEvents;
    delete theEffects;
    theResult = gDataPath->FindFile(kDbAnimalsFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1006, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 1007, "theDbFile != NULL");
    theResult = EEAnimal::Load(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1009, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbColorTableFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1014, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 1015, "theDbFile != NULL");
    theResult = EEColorTable::Load(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1017, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbAIUnitTargetingFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1022, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 1023, "theDbFile != NULL");
    theResult = EEAIUnitTargetingTable::Load(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1025, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbTerrainFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1030, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 1031, "theDbFile != NULL");
    theResult = EETerrainTable::Load(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1033, "theResult == keseOK");
    delete theDbFile;
    theResult = EETerrainColorTable::Load();
    if (theResult != kErrorOK) ESVerify(__FILE__, 0x40e, "theResult == keseOK");
    theResult = gDataPath->FindFile(kDbTerrainTypeFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1042, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 1043, "theDbFile != NULL");
    theResult = EETerrainTypeTable::Load(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1045, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbCliffTerrainFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1050, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 1051, "theDbFile != NULL");
    theResult = EECliffTerrainTable::Load(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1053, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbCivPowersFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1058, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 1059, "theDbFile != NULL");
    theResult = gCivPowersManager.Load(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1061, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbCivilizationFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1066, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 1067, "theDbFile != NULL");
    theResult = gCivilizationManager.Load(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1069, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbRandomMapFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1074, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 1075, "theDbFile != NULL");
    theResult = EERandomMapTable::Load(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1077, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbWeaponToHitFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1082, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 1083, "theDbFile != NULL");
    theResult = EEToHitTable::Load(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1085, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbCalamityFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1090, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 1091, "theDbFile != NULL");
    theResult = EECalamityManager::GetInstance()->LoadCalamityClasses(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1093, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbUnitSetFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1098, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 1099, "theDbFile != NULL");
    theResult = gUnitSetManager.Load(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1101, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbAreaEffectFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1106, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 1107, "theDbFile != NULL");
    theResult = gAEManager.Load(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1109, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbUpgradeFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1114, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 1115, "theDbFile != NULL");
    theResult = EEUCUpgradeTable::Load(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1117, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbCPBehaviorFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1122, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 1123, "theDbFile != NULL");
    theResult = EECPBehavior::Load(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1125, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbGameVariantFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1130, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 1131, "theDbFile != NULL");
    theResult = EEGameVariant::Load(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1133, "theResult == keseOK");
    delete theDbFile;
    theResult = gDataPath->FindFile(kDbPremadeCivsFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1138, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 1139, "theDbFile != NULL");
    theResult = gCivilizationManager.LoadPremadeCivilizations(theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 1141, "theResult == keseOK");
    delete theDbFile;
    theResult = UIInitialize();
    if (theResult != kErrorOK) ESVerify(__FILE__, 0x47a, "theResult == keseOK");
    theDirectory.SetVolume(gStartupVolume);
    theDirectory.SetDirectory(gStartupDirectory + "data\\" + "Campaigns\\");
    if (!theDirectory.IsDirectory()) theDirectory.CreateDirectory();
    theDirectory.SetVolume(gStartupVolume);
    theDirectory.SetDirectory(gStartupDirectory + "data\\" + "Saved Games\\");
    if (!theDirectory.IsDirectory()) theDirectory.CreateDirectory();
    theDirectory.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\");
    if (!theDirectory.IsDirectory()) theDirectory.CreateDirectory();
    theDirectory.SetDirectory(gStartupDirectory + "Users\\");
    if (!theDirectory.IsDirectory()) theDirectory.CreateDirectory();
    thePath = gStartupDirectory + "Users\\default\\";
    theDirectory.SetDirectory(thePath);
    if (!theDirectory.IsDirectory()) theDirectory.CreateDirectory();
    theResult = gResource->GetString(IDS_CIVILIZATIONS, theUserDirectory);
    if (theResult != kErrorOK) ESVerify(__FILE__, 0x4a8, "theResult == keseOK");
    thePath = thePath + (const char*)theUserDirectory + "\\";
    theDirectory.SetDirectory(thePath);
    if (!theDirectory.IsDirectory()) theDirectory.CreateDirectory();
    theResult = gDataPath->FindFile(kDbAmbientSoundsFile, theDbFile);
    if (theResult != kErrorOK) ESVerify(__FILE__, 0x4b3, "theResult == keseOK");
    if (theDbFile == NULL) ESVerify(__FILE__, 0x4b4, "theDbFile != NULL");
    gAmbientSoundManager.Load(*theDbFile);
    delete theDbFile;
    if (EESoundObject::Get(kSoundHotKeyAlarm) != NULL)
        EESoundObject::Get(kSoundHotKeyAlarm)->LoadMedia();
    EEGraphicEffect::SortMediaByKind();
    theResult = EEFn0FCA98();
    if (theResult != kErrorOK) ESVerify(__FILE__, 0x4c1, "theResult == keseOK");
    Fn1030AB();
    EEInitGamePools();
    theResult = URegisterShutdown(EEDeleteDatabase);
    if (theResult != kErrorOK) ESVerify(__FILE__, 0x4cb, "theResult == keseOK");
}

// FUNCTION: EXE 0x542363
// Sets up random maps, game options, physics models and sight rings, creates the mouse, unit classes, UI
// manager, AI, cheat codes and game timer, and registers their shutdown
void EEInitializeGameObjects()
{
    FSFileSpec theDirectory;
    theDirectory.SetDirectory(gStartupDirectory);
    EERandomMap::Initialize(theDirectory);
    gGameOptions.SetDefaults();
    gWorld.mGameOptions.SetDefaults();
    EEInitPhysicsModels();
    EEBuildSightRings();
    gMouse = new EEMouse;
    if (gMouse == NULL) ESVerify(__FILE__, 0x4ee, "gMouse != NULL");
    long theResult = gMouse->Load(gRasterizer->GetScreen(gGamePreferences.mSettings.GetScreenID()));
    if (theResult != kErrorOK) ESVerify(__FILE__, 0x4f2, "theResult == keseOK");
    gMouse->SetBounds(GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight()));
    gMouse->SetPosition(GE2DPoint(512.0f, 384.0f));
    gMouse->SetSensitivity(gGamePreferences.mSettings.mMouseSensitivity);
    gMouse->mPointer = 0;
    EEUnitClass::CreateUnitClasses();
    EETechTree::BuildUnitClassInfo();
    gUIManager = new EEUIManager(gRasterizer, gGamePreferences.mSettings.GetScreenMode() + 1, gGamePreferences.mSettings.GetScreenID(), "db\\");
    if (gUIManager == NULL) ESVerify(__FILE__, 0x50a, "gUIManager != NULL");
    UIManager::SetMouseLeftHanded(gGamePreferences.mSettings.mRightHandMouseOrientation == false);
    UIControl::SetBorderColors(GEColor(0.34765625f, 0.39453125f, 0.390625f),
                              GEColor(0.69140625f, 0.76171875f, 0.7578125f));
    gArtificialIntelligence = new EEArtificialIntelligence;
    if (gArtificialIntelligence == NULL) ESVerify(__FILE__, 0x514, "gArtificialIntelligence != NULL");
    gCheatCodes = new EECheatCodes;
    if (gCheatCodes == NULL) ESVerify(__FILE__, 0x518, "gCheatCodes != NULL");
    EECreateGameTimer();
    theResult = URegisterShutdown(EEDeleteGameObjects);
    if (theResult != kErrorOK) ESVerify(__FILE__, 0x51f, "theResult == keseOK");
}

// FUNCTION: EXE 0x543e04
// Deletes the game timer, AI, cheat codes, UI manager, mouse and mini map and clears the world and game state
void EEDeleteGameObjects(bool inShutdown)
{
    delete sTimer;
    sTimer = NULL;
    EEAIContinentManager::Shutdown();
    delete gCheatCodes;
    gCheatCodes = NULL;
    delete gArtificialIntelligence;
    gArtificialIntelligence = NULL;
    delete gUIManager;
    gUIManager = NULL;
    gMouse->Release();
    delete gMouse;
    gMouse = NULL;
    gWorld.Clear();
    if (gMiniMap != NULL) {
        delete gMiniMap;
        gMiniMap = NULL;
    }
    gAEManager.Clear();
    gGameState.Clear();
    EEFreePhysicsModels();
    EEClearRandomMapGenerators();
}

#include "ee/EEUnitAction.h"

// Cleanup routines defined in other units
void EEReportLeakingUnits();
void EEUnloadBuiltInGraphicObjects();
void EEDeleteMediaTable();

// FUNCTION: EXE 0x543d0c
// Unloads every database table and its media, deletes the calamity manager and language resources, frees the pools
void EEDeleteDatabase(bool inShutdown)
{
    gPathManager.Clear();
    EEReleaseUnitPools();
    EEReportLeakingUnits();
    EEUnitAction::DeleteScripts();
    EEUnitClass::ReleaseUnitClasses();
    EEFn0FCA98();
    unsigned long theSound = 962;
    if (EESoundObject::Get(theSound) != NULL)
        EESoundObject::Get(theSound)->UnloadMedia();
    EEUnloadBuiltInGraphicObjects();
    EEAnimal::Unload();
    gAmbientSoundManager.ClearSounds();
    EETerrainTypeTable::ClearDamageTables();
    EECalamityManager::GetInstance()->DestroyCalamityClasses(inShutdown);
    gDatabaseObjects.Clear();
    EEButtonObject::Unload();
    EEGraphicEffect::UnloadTable();
    EEDeleteMediaTable();
    EEMusicObject::ClearAll();
    EESoundObject::UnloadTable();
    EEColorTable::Unload();
    EEAIUnitTargetingTable::Unload();
    EETerrainTable::Unload();
    EETerrainColorTable::Unload();
    EETerrainTypeTable::Unload();
    EECliffTerrainTable::Unload();
    EERandomMapTable::Unload();
    EEUCUpgradeTable::Unload();
    EEUnitBehavior::Unload();
    EEStartingResources::Unload();
    EEGameVariant::Unload();
    gWorldTable.Clear();
    gTechTree.UnloadAll();
    USingleton<EECalamityManager>::DestroyInstance();
    delete gResource;
    EEReleaseGamePools(true);
}

// The deferred COMDATs at the end of the unit (0x14494d..0x14622a: the inline-class deleting dtors and every template
// instantiation) are compiled without /Og, while every user function is -O1: the file ends with an unmatched
// `#pragma optimize("g", off)` (VC6 compiles deferred inline/template bodies at the end of the TU with the pragma
// state in force there; probe build/scratch_exe-core/probes/p2.cpp). Evidence: with it vector<EEUnitSet*>::_Buy
// 0x54541f goes 17.65 -> exact and nothing else changes.
#pragma optimize("g", off)

// TEMPLATE: EXE 0x54630b
// TEMPLATE: EXE 0x5463c4
// TEMPLATE: EXE 0x546a13

// TEMPLATE: EXE 0x54640a
// ??4EEDisplayRasterizer@@QAEAAU0@ABU0@@Z
// TEMPLATE: EXE 0x546442
// ??4?$vector@UEEDisplayScreen@@V?$allocator@UEEDisplayScreen@@@std@@@std@@QAEAAV01@ABV01@@Z
// TEMPLATE: EXE 0x5468dc
// ?begin@?$vector@UEEDisplayMode@@V?$allocator@UEEDisplayMode@@@std@@@std@@QBE?AV?$_Ptrit@UEEDisplayMode@@HPBU1@ABU1@PAU1@AAU1@@2@XZ
// TEMPLATE: EXE 0x546903
// ?size@?$vector@UEEDisplayScreen@@V?$allocator@UEEDisplayScreen@@@std@@@std@@QBEIXZ
// TEMPLATE: EXE 0x546934
// ?_Buy@?$vector@UEEDisplayScreen@@V?$allocator@UEEDisplayScreen@@@std@@@std@@IAE_NI@Z
// TEMPLATE: EXE 0x546993
// ?_Tidy@?$vector@UEEDisplayScreen@@V?$allocator@UEEDisplayScreen@@@std@@@std@@IAEXXZ
// TEMPLATE: EXE 0x546a7a
// ?_Insertion_sort@std@@YAXV?$_Ptrit@VFSFileSpec@@HPAV1@AAV1@PAV1@AAV1@@1@0UEEFileNameSorter@@@Z
// TEMPLATE: EXE 0x546aa6
// ?_Sort@std@@YAXV?$_Ptrit@VFSFileSpec@@HPAV1@AAV1@PAV1@AAV1@@1@0HUEEFileNameSorter@@@Z
// TEMPLATE: EXE 0x546b87
// ?_Unguarded_insertion_sort@std@@YAXV?$_Ptrit@VFSFileSpec@@HPAV1@AAV1@PAV1@AAV1@@1@0UEEFileNameSorter@@PAVFSFileSpec@@@Z
// TEMPLATE: EXE 0x546be0
// ??D?$_Ptrit@VFSFileSpec@@HPAV1@AAV1@PAV1@AAV1@@std@@QBEAAVFSFileSpec@@XZ
// TEMPLATE: EXE 0x546bee
// ?_Ucopy@?$vector@UEEDisplayScreen@@V?$allocator@UEEDisplayScreen@@@std@@@std@@IAEPAUEEDisplayScreen@@V?$_Ptrit@UEEDisplayScreen@@HPBU1@ABU1@PAU1@AAU1@@2@0PAU3@@Z
// TEMPLATE: EXE 0x546c84
// ?_Destroy@std@@YAXPAUEEDisplayScreen@@@Z
// TEMPLATE: EXE 0x546cb2
// ??4EEDisplayScreen@@QAEAAU0@ABU0@@Z
// TEMPLATE: EXE 0x546cea
// ??_GEEDisplayScreen@@QAEPAXI@Z
// TEMPLATE: EXE 0x546d21
// ??4?$vector@UEEDisplayMode@@V?$allocator@UEEDisplayMode@@@std@@@std@@QAEAAV01@ABV01@@Z
// TEMPLATE: EXE 0x547145
// ?end@?$vector@UEEDisplayMode@@V?$allocator@UEEDisplayMode@@@std@@@std@@QBE?AV?$_Ptrit@UEEDisplayMode@@HPBU1@ABU1@PAU1@AAU1@@2@XZ
// TEMPLATE: EXE 0x54716c
// ?size@?$vector@UEEDisplayMode@@V?$allocator@UEEDisplayMode@@@std@@@std@@QBEIXZ
// TEMPLATE: EXE 0x54719d
// ?_Buy@?$vector@UEEDisplayMode@@V?$allocator@UEEDisplayMode@@@std@@@std@@IAE_NI@Z
// TEMPLATE: EXE 0x5471fc
// ?_Tidy@?$vector@UEEDisplayMode@@V?$allocator@UEEDisplayMode@@@std@@@std@@IAEXXZ
// TEMPLATE: EXE 0x54728e
// ?_Insertion_sort_1@std@@YAXV?$_Ptrit@VFSFileSpec@@HPAV1@AAV1@PAV1@AAV1@@1@0UEEFileNameSorter@@PAVFSFileSpec@@@Z
// TEMPLATE: EXE 0x5473ac
// ??E?$_Ptrit@VFSFileSpec@@HPAV1@AAV1@PAV1@AAV1@@std@@QAEAAV01@XZ
// TEMPLATE: EXE 0x5473c5
// ?make_heap@std@@YAXV?$_Ptrit@VFSFileSpec@@HPAV1@AAV1@PAV1@AAV1@@1@0UEEFileNameSorter@@@Z
// TEMPLATE: EXE 0x547404
// ?sort_heap@std@@YAXV?$_Ptrit@VFSFileSpec@@HPAV1@AAV1@PAV1@AAV1@@1@0UEEFileNameSorter@@@Z
// TEMPLATE: EXE 0x54744c
// ?_Unguarded_partition@std@@YA?AV?$_Ptrit@VFSFileSpec@@HPAV1@AAV1@PAV1@AAV1@@1@V21@0UEEFileNameSorter@@PAVFSFileSpec@@@Z
// TEMPLATE: EXE 0x547719
// ?_Unguarded_insert@std@@YAXV?$_Ptrit@VFSFileSpec@@HPAV1@AAV1@PAV1@AAV1@@1@VFSFileSpec@@UEEFileNameSorter@@@Z
// TEMPLATE: EXE 0x54778b
// ?_Ucopy@?$vector@UEEDisplayMode@@V?$allocator@UEEDisplayMode@@@std@@@std@@IAEPAUEEDisplayMode@@V?$_Ptrit@UEEDisplayMode@@HPBU1@ABU1@PAU1@AAU1@@2@0PAU3@@Z
// TEMPLATE: EXE 0x547820
// ??4EEDisplayMode@@QAEAAU0@ABU0@@Z
// TEMPLATE: EXE 0x54785b
// ??0EEDisplayScreen@@QAE@ABU0@@Z
// TEMPLATE: EXE 0x547895
// ??_GEEDisplayMode@@QAEPAXI@Z
// TEMPLATE: EXE 0x5478bf
// ??0?$vector@UEEDisplayMode@@V?$allocator@UEEDisplayMode@@@std@@@std@@QAE@ABV01@@Z
// TEMPLATE: EXE 0x547959
// ?_Make_heap@std@@YAXV?$_Ptrit@VFSFileSpec@@HPAV1@AAV1@PAV1@AAV1@@1@0UEEFileNameSorter@@PAHPAVFSFileSpec@@@Z
// TEMPLATE: EXE 0x5479c9
// ?pop_heap@std@@YAXV?$_Ptrit@VFSFileSpec@@HPAV1@AAV1@PAV1@AAV1@@1@0UEEFileNameSorter@@@Z
// TEMPLATE: EXE 0x5479f5
// ?iter_swap@std@@YAXV?$_Ptrit@VFSFileSpec@@HPAV1@AAV1@PAV1@AAV1@@1@0@Z
// TEMPLATE: EXE 0x547a1e
// ?_Construct@std@@YAXPAUEEDisplayMode@@ABU2@@Z
// The UList<EEComplexUnit*>::sNodePool chain pieces and the memory pool's COMDATs, the FSFileSpec heap helpers
// TEMPLATE: EXE 0x54622a
// TEMPLATE: EXE 0x54624d
// TEMPLATE: EXE 0x546276
// TEMPLATE: EXE 0x546286
// TEMPLATE: EXE 0x546296
// TEMPLATE: EXE 0x5462a6
// TEMPLATE: EXE 0x5462d9
// TEMPLATE: EXE 0x546866
// TEMPLATE: EXE 0x547a6a
// TEMPLATE: EXE 0x547bb9
// TEMPLATE: EXE 0x547c29
// TEMPLATE: EXE 0x547c5f
// TEMPLATE: EXE 0x547d39
// TEMPLATE: EXE 0x547d8a
// TODO 0x547da5, 0x547dcd,
// 0x547dd9, 0x547de5, 0x547df1: COMDATs not emitted/named yet
