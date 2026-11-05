// C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\EELobbyInterface.cpp (name PROVISIONAL: no __FILE__; the
// gEELobbyInterface_* texts are its strings and it sorts between EELobbyInit.cpp and EELobbyMisc.cpp). The game's
// side of the external lobby: the lobby windows' globals, the game options read back from the lobby game, the local
// address, opening and closing the lobby through EEALobbyAction, the game-done event: 0x296aaa..0x297ee7.
// Module flags (r12-misc-h5, round 12). Every function has source (r24pre-won: 0x2972c6 with its EELobbySoundDecoder /
// MSNativeIcon ??_G 0x297dc5 / 0x297531, and 0x29754d).
// (0x29754d called IDirectDraw7::FlipToGDISurface through the DX7 screen; the renderer's GX layer stands in for it)
#include <windows.h>
#include <string>
#include <list>

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
#include "lle/FSFileSpec.h"
#include "lle/UResourceFile.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
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
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NEProtocol.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"

#include "ee/EmpireEarth.h"
#include "ee/EECommunications.h"
#include "ee/EEGameOptions.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EERandomMapGenerators.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEMouse.h"
#include "ee/EEClient.h"
#include "ee/EEServer.h"
#include "ee/EEWonNet.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEAction.h"
#include "ee/EEMessage.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/EELobbyGameCode.h"
#include "ee/wonapi/EELobbySound.h"

using namespace WONAPI;

SmartPtr<MSWindowManager> gEELobbyRootWindow;          // 0x565894
SmartPtr<MSWindow> gEELobbyPlatformWindow;  // 0x565890

static WONAPICoreEx* gEELobbyWindowManager;      // 0x565898
static bool gEELobbyOpen;                   // 0x56589c
static bool gEELobbyShown;                  // 0x56589d
static bool gEELobbyClosing;                // 0x56589e
static long sEELobbyHeight;                 // 0x565888 (the game window's size when the lobby opened; r23)
static long sEELobbyWidth;                  // 0x56588c
static bool gEELobbyGameDone;               // 0x5658a0
static unsigned long sEELobbyStartTime;     // 0x5658a8 (the external lobby's Start time; r23)

void EELobbyFn296B9A(EEGameOptionsPlayer& outPlayer, EELobbyPlayerSlot* inSlot);
void EELobbyFn296B06(EEGameOptionsPlayer& outMyPlayer);
void EELobbyFn296EAC(EEGameOptionsPlayer& outMyPlayer);
void EELobbyFn2970E3();
extern char gU4E5B98[];                     // 0x4e5b98 (pooled ""; as EEWonNet.cpp declares it)
extern EEGameOptions gLobbyGameOptions;     // 0x53030c (the lobby defaults; as EEGameOptions.cpp declares it)
extern HINSTANCE gInstance;                 // 0x5319d8 (as Empire Earth.cpp declares it)
void EELobbyInit();                         // 0x295511 (EELobbyInit.cpp)
UWideString EELobbyFn296C32(const GUIString& inText);
void EELobbyFn2972C6();
void EELobbyPostAction(void (*inFunc)());
void EELobbyFn29754D(bool inFlag);
void EELobbyFn2976C7();
void EELobbyFn29775D();
void EELobbyFn297897();
void EELobbyFn297AC9();
void EELobbyFn297BE0();
void EELobbyFn297D5D();
void Fn297F15();
void Fn297F21();
void EELobbyFn297DE1();
void EELobbyFn297E45();
bool EEIsLobbyOpen();
bool Fn297ED0();
bool EELobbyFn295C17();
void EELobbyShutdown();

// the game's actions the lobby posts to the user interface: one function to call (vtable 0x44c6c4)
class EEALobbyAction : public EEAction {
public:
    EEALobbyAction(void (*inFunc)()) : mFunc(inFunc) {}
    virtual void Virtual4() { mFunc(); }    // +0x04 0x2972c3
    virtual void Virtual8() {}              // +0x08 (ICF-folded 0x1720c6)

    void (*mFunc)();                        // +0x0c
};

// the lobby form's open event once a game is done (vtable 0x44c6e8; ??_G ICF-folded 0x1e60e7)
class EELobbyGameDoneEvent : public UIEvent {
public:
    EELobbyGameDoneEvent() : mCount(0) {}
    virtual ~EELobbyGameDoneEvent() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);

    long mCount;                            // +0x0c
};

// FUNCTION: EXE 0x696b06
// NEAR: the original keeps the game in edi and the player pointer in ebx, and materialises theMine (sete [ebp-8])
void EELobbyFn296B06(EEGameOptionsPlayer& outMyPlayer)
{
    bool theRandom;
    bool theMine;
    int i;
    EELobbyGame* theGame = (EELobbyGame*)GetMyLobbyGame();
    if (theGame == NULL)
        return;
    theRandom = theGame->HasGameFile();
    for (i = 0; i < 16; i++) {
        EEGameOptionsPlayer thePlayer;
        EELobbyFn296B9A(thePlayer, theGame->GetPlayerSlot(i));
        thePlayer.mSlot = i;
        theMine = theGame->GetMySlot() == i;
        if (theMine)
            outMyPlayer = thePlayer;
        if (theRandom)
            thePlayer.mLoadedType = thePlayer.mType;
        gGameOptions.mPlayers[i] = thePlayer;
    }
}

// FUNCTION: EXE 0x696b9a
void EELobbyFn296B9A(EEGameOptionsPlayer& outPlayer, EELobbyPlayerSlot* inSlot)
{
    if (inSlot == NULL) {
        outPlayer.mType = 4;
        return;
    }
    switch (inSlot->mOptions[0]) {
    case 0:
        outPlayer.mType = 4;
        return;
    case 1:
        outPlayer.mType = 0;
        break;
    case 2:
        outPlayer.mType = 4;
        return;
    case 3:
        outPlayer.mType = inSlot->mOptions[4] != 0 ? 2 : 1;
        break;
    }
    if (inSlot->mOptions[0] == 3)
        outPlayer.mName = EELobbyFn296C32(inSlot->GetName());
    outPlayer.mIsReady = false;
    outPlayer.mB36 = false;
    if (outPlayer.mType == 1)
        outPlayer.mColor = inSlot->mOptions[2];
    if (outPlayer.mType != 2) {
        outPlayer.mTeam = inSlot->mOptions[1];
        outPlayer.mStartingCitizens = inSlot->mOptions[3];
    }
}

// FUNCTION: EXE 0x696c32
UWideString EELobbyFn296C32(const GUIString& inText)
{
    return UWideString(WStringToString(inText, -1).c_str());
}

// FUNCTION: EXE 0x696c89
UWideString& EEGetLocalAddress(NEProtocol* inProtocol, unsigned long inAdapter)
{
    LobbyGame* theGame = NULL;
    if (EECheck2989A9(true))
        theGame = GetMyLobbyGame();
    if (theGame == NULL)
        return inProtocol->GetLocalAddress(inAdapter);
    static UWideString theAddress;
    theAddress = theGame->mAddress.GetHostString(true).c_str();
    return theAddress;
}

// FUNCTION: EXE 0x696d1e
// the host's local server for the lobby game and the host's own connection request (r23)
bool EELobbyFn296D1E()
{
    UWideString theUnused1;
    UString theUnused2;
    UString theUnused3;
    UWideString theUnused4;
    EEGameOptionsPlayer theMe;
    gPlayerName = EELobbyFn296C32(LobbyFn361370()->mName);
    EELobbyFn2970E3();
    bool theFlag = LobbyFn361B20();
    if (gCommunications != NULL) {
        delete gCommunications;
        gCommunications = NULL;
    }
    gCommunications = new EEServer(UWideString("TestGame"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, false, false);
    ((EEServer*)gCommunications)->StartThreads();
    bool theResult = false;
    if (!((EEServer*)gCommunications)->GetGameWasPublished()) {
        delete gCommunications;
        gCommunications = NULL;
    } else {
        EELobbyFn296EAC(theMe);
        theMe.mIsReady = true;
        theMe.mType = 1;
        gCommunications->ProcessLocalUserInput(
            new NEMClientConnectionRequest(theMe.mName, gGameVersion, gBinaryChecksum, theFlag), 0xff);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x696eac
// the lobby game's options into gGameOptions (from the lobby defaults), then the players (r23)
void EELobbyFn296EAC(EEGameOptionsPlayer& outMyPlayer)
{
    EELobbyGame* theGame = (EELobbyGame*)GetMyLobbyGame();
    if (theGame == NULL)
        return;
    gGameOptions.mLock->WriteLock();
    gGameOptions = gLobbyGameOptions;
    gGameOptions.mIsMultiplayer = true;
    gGameOptions.mCheatCodes = false;
    switch (theGame->GetOption(0)) {
    case 0:
        gGameOptions.mLoadType = 0;
        break;
    case 1:
        gGameOptions.mLoadType = 2;
        break;
    case 2:
        gGameOptions.mLoadType = 1;
        break;
    }
    switch (theGame->GetOption(1)) {
    case 0:
        gGameOptions.mMapSize = 0;
        break;
    case 1:
        gGameOptions.mMapSize = 1;
        break;
    case 2:
        gGameOptions.mMapSize = 2;
        break;
    case 3:
        gGameOptions.mMapSize = 3;
        break;
    case 4:
        gGameOptions.mMapSize = 4;
        break;
    case 5:
        gGameOptions.mMapSize = 5;
        break;
    }
    if (theGame->HasGameFile())
        gGameOptions.mMapType = EELobbyFn296C32(theGame->GetGameFileName());
    else
        gGameOptions.mMapType = EERMGetGeneratorName(theGame->GetOption(2));
    gGameOptions.mResources = theGame->GetOption(3);
    gGameOptions.mEpochStart = theGame->GetOption(4);
    gGameOptions.mEpochEnd = theGame->GetOption(5);
    gGameOptions.mVariant = theGame->GetOption(6) + 1;
    gGameOptions.mDifficulty = theGame->GetOption(7);
    switch (theGame->GetOption(8)) {
    case 0:
        gGameOptions.mGameSpeed = 0;
        break;
    case 1:
        gGameOptions.mGameSpeed = 1;
        break;
    case 2:
        gGameOptions.mGameSpeed = 2;
        break;
    case 3:
        gGameOptions.mGameSpeed = 3;
        break;
    }
    gGameOptions.mUnitLimit = theGame->GetOption(9);
    gGameOptions.mWondersForVictory = theGame->GetOption(10);
    gGameOptions.mRevealMap = theGame->GetOption(11) != 0;
    gGameOptions.mAllowCustomCivs = theGame->GetOption(12) != 0;
    gGameOptions.mLockTeams = theGame->GetOption(13) != 0;
    gGameOptions.mLockSpeed = theGame->GetOption(14) != 0;
    gGameOptions.mVictory = theGame->GetOption(15) != 0;
    EELobbyFn296B06(outMyPlayer);
    gGameOptions.mLock->WriteUnlock();
}

// FUNCTION: EXE 0x6970e3
void EELobbyFn2970E3()
{
    unsigned long theIndex = 0;
    void* theHandle = gLobbyNetHandle;
    if (theHandle != NULL) {
        unsigned long i = 1;
        std::list<LobbyNetAddress> theAddresses;
        LobbyGetNetAddresses(theAddresses, false);
        for (std::list<LobbyNetAddress>::iterator it = theAddresses.begin(); it != theAddresses.end(); ++it, i++) {
            if ((*it).mHandle == theHandle) {
                theIndex = i;
                break;
            }
        }
    }
    gU5308A0 = theIndex;
}

// TEMPLATE: EXE 0x698bf9
// TEMPLATE: EXE 0x698c1c
// (list<LobbyNetAddress>'s ctor and dtor for EELobbyFn2970E3's local: the linker placed the COMDATs inside
// EELobbyMisc.cpp's range, which does not instantiate them; r24-lobby-player-gfx)

// FUNCTION: EXE 0x69713f
long EELobbyGameDoneEvent::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    inForm->GetControl(11)->SetDisplayMode(0);
    inForm->GetControl(5)->SetDisplayMode(0);
    inForm->GetControl(3)->SetDisplayMode(0);
    inForm->GetControl(4)->SetDisplayMode(0);
    inForm->GetControl(6)->SetDisplayMode(0);
    inForm->GetControl(16)->SetDisplayMode(0);
    inForm->GetControl(23)->SetDisplayMode(0);
    inForm->GetControl(26)->SetDisplayMode(0);
    inForm->GetControl(10)->SetDisplayMode(0);
    inForm->GetControl(9)->SetDisplayMode(0);
    inForm->GetControl(13)->SetDisplayMode(0);
    inForm->GetControl(22)->SetDisplayMode(0);
    inForm->GetControl(2)->SetDisplayMode(0);
    inForm->GetControl(1)->SetDisplayMode(0);
    inForm->GetControl(20)->SetDisplayMode(0);
    inForm->GetControl(27)->SetDisplayMode(0);
    inForm->GetControl(0)->SetDisplayMode(0);
    inForm->GetControl(7)->SetDisplayMode(0);
    mCount++;
    if (mCount == 2 && EEIsLobbyOpen())
        EELobbyFn297E45();
    return keseOK;
}

// FUNCTION: EXE 0x697276
void EEStartExternalLobby()
{
    EELobbyPostAction(EELobbyFn2972C6);
}

// FUNCTION: EXE 0x697282
void EELobbyPostAction(void (*inFunc)())
{
    EEALobbyAction* theAction = new EEALobbyAction(inFunc);
    gUserInterface->PostAction(theAction);
    theAction->Dereference();
}

// TEMPLATE: EXE 0x6972c3
// (EEALobbyAction::Virtual4, inline: its COMDAT follows its first user)

// FUNCTION: EXE 0x6972c6
// NEAR (93.5): (1) the original loads the window and its vtable before it builds the rectangle (a temporary's
// shape), but a WONRectangle temporary here moves below theClass in the frame and loses the esi = 0x100 reuse (84.6);
// (2) it keeps theClass.hIcon in edi from the test on. r24pre-won
// opens the external lobby: the WONAPI core, the window manager with its image and sound decoders, the lobby window
// (the game window's title and icon), the lobby screen in it; then shows it and runs it until it is left (r24pre-won)
void EELobbyFn2972C6()
{
    char theText[256];
    WNDCLASSA theClass;
    if (gEELobbyOpen)
        return;
    gEELobbyOpen = true;
    gEELobbyWindowManager = new WONAPICoreEx;
    gEELobbyWindowManager->Fn2F2F90(true);
    gEELobbyWindowManager->Virtual04();
    gEELobbyRootWindow = new MSWindowManager(NULL);
    gEELobbyRootWindow->mBA5 = false;
    LobbyFn3D2A20(&gEELobbyRootWindow->mImageDecoder);
    LobbyFn3D24E0(&gEELobbyRootWindow->mImageDecoder);
    gEELobbyRootWindow->SetSoundDecoder(new EELobbySoundDecoder);
    gEELobbyPlatformWindow = new MSWindow;
    gEELobbyPlatformWindow->mBC5 = true;
    long theStyle = 2;
    if (gGamePreferences.mSettings.mFullScreen)
        theStyle = 0x22;
    gEELobbyPlatformWindow->mStyle = theStyle;
    GetWindowTextA(gGameWindow->GetOSRepresentation(), theText, 256);
    gEELobbyPlatformWindow->SetTitle(theText);
    if (gGamePreferences.mSettings.mFullScreen)
        LobbyFn3D08C0(gGameWindow->GetOSRepresentation());
    WONRectangle theRect(0, 0, 100, 100);
    gEELobbyPlatformWindow->Create(theRect);
    LobbyFn3D08C0(NULL);
    if (GetClassNameA(gGameWindow->GetOSRepresentation(), theText, 256) &&
        GetClassInfoA(gInstance, theText, &theClass) && theClass.hIcon != NULL)
        gEELobbyPlatformWindow->SetIcon(new MSNativeIcon(gEELobbyPlatformWindow->mDisplayContext, theClass.hIcon));
    RootContainer* theRoot = gEELobbyPlatformWindow->mRoot;
    EELobbyInit();
    SmartPtr<Container> theScreen = LobbyFn362250();
    theRoot->Fn2F6140(theScreen.get(), 3, theRoot, 0, 0, 0, 0, 0);
    theRoot->Fn2F6A60(theScreen.get());
    EELobbyFn29754D(true);
    LobbyFn362740();
    EELobbyFn2976C7();
}

// SYNTHETIC: EXE 0x697531
// (??_GMSNativeIcon: the first user's COMDAT)

// The DX7 screen's declarations come here, not with the other includes: DX7Screen.h declares three class statics
// (sScreens, sScreensCaptured, sFirstCaptureWasAttachedToDesktop; DX7HRDisplay.dll exports them), and the unit's
// $E numbers (init chains 0x296aaa.., EEGetLocalAddress's static 0x296d13) show none of them was declared before.
#include "gx/GX.h"
#include "dx7/DX7Screen.h"

// FUNCTION: EXE 0x69754d
// STAND-IN (owner-approved 2026-10-28, docs/LOST_DEBUG.md): the original adds the cursor's y as fld [y]; fiadd
// [point.y] (x: fild; fadd). The (double) cast on theCursor.mY in the y sum makes it exact (98.3 without it).
// the lobby takes the game window's place: the lobby window gets the game window's rectangle, the cursor keeps its
// place, input is suspended (full screen: DirectDraw flips to the GDI surface) or the game window is hidden
void EELobbyFn29754D(bool inFlag)
{
    if (gEELobbyShown || gEELobbyPlatformWindow.mPtr == NULL)
        return;
    RECT theRect;
    GetWindowRect(gGameWindow->GetOSRepresentation(), &theRect);
    GE2DPoint theCursor(0.0f, 0.0f);
    sEELobbyWidth = theRect.right - theRect.left;
    sEELobbyHeight = theRect.bottom - theRect.top;
    if (gMouse != NULL) {
        theCursor = gMouse->mPosition;
        gMouse->mPointer = 6;
    }
    gEELobbyPlatformWindow->SetPosSize(theRect.left, theRect.top, sEELobbyWidth, sEELobbyHeight);
    gEELobbyPlatformWindow->Show(true);
    if (gMouse != NULL) {
        POINT thePoint;
        thePoint.x = 0;
        thePoint.y = 0;
        ClientToScreen(gGameWindow->GetOSRepresentation(), &thePoint);
        SetCursorPos(thePoint.x + theCursor.mX, thePoint.y + (double)theCursor.mY);
    }
    Fn297F15();
    gEELobbyShown = true;
    if (gGamePreferences.mSettings.mFullScreen) {
        if (gISKeyboard != NULL)
            gISKeyboard->Suspend();
        if (gISMouse != NULL)
            gISMouse->Suspend();
        if (inFlag)
            ((GXDirectDraw*)*(DX7Screen*)gRasterizer->GetScreen(gScreenIndex))->FlipToGDISurface();
    } else {
        ShowWindow(gGameWindow->GetOSRepresentation(), SW_HIDE);
    }
    Fn297F21();
    for (int theCount = 1 - ShowCursor(TRUE); theCount > 0; theCount--)
        ShowCursor(TRUE);
    gEELobbyPlatformWindow->Fn32A1E0();
}

// FUNCTION: EXE 0x6976c7
// NEAR: loop shape (the original keeps 0 in ebx and jumps back to the LobbyFn362760 call)
void EELobbyFn2976C7()
{
    if (gEELobbyClosing)
        return;
    gEELobbyClosing = true;
    for (;;) {
        int theResult = LobbyFn362760();
        if (theResult == 2) {
            if (EELobbyFn295C17()) {
                EELobbyPostAction(EELobbyFn29775D);
                EELobbyFn297D5D();
                LobbyFn362830();
                EELobbyFn297AC9();
                break;
            }
        } else if (theResult == 1) {
            EELobbyFn297897();
            if (EELobbyFn295C17()) {
                EELobbyFn297D5D();
                LobbyFn362830();
                EELobbyFn297AC9();
                break;
            }
        } else {
            if (theResult == 3) {
                EELobbyShutdown();
                EELobbyFn297BE0();
            } else
                EELobbyShutdown();
            break;
        }
        delete gCommunications;
        gCommunications = NULL;
    }
    gEELobbyGameDone = false;
    gEELobbyClosing = false;
}

// FUNCTION: EXE 0x69775d
// the host starts the game from the external lobby (EEUIELEStart::ProcessEvent 0x5da6b3's twin; r23)
void EELobbyFn29775D()
{
    EEGameOptionsPlayer thePlayer;
    unsigned char theHumans = 0;
    gGameOptions.mLock->ReadLock();
    sEELobbyStartTime = TMGetTime();
    ((EEServer*)gCommunications)->AllowNewPlayers(false);
    ((EEServer*)gCommunications)->SetGameState(2);
    ((EEServer*)gCommunications)->mNumLoadResponses = false;
    for (int i = 0; i < 16; i++) {
        thePlayer = gWorld.GetLobbyOptions().mPlayers[i];
        if (thePlayer.mType == 1 || thePlayer.mType == 2)
            theHumans++;
    }
    ((EEServer*)gCommunications)->StopPing();
    ((EEServer*)gCommunications)->ResetXfer();
    ((EEServer*)gCommunications)->mLoadState = true;
    ((EEServer*)gCommunications)->AllowNewPlayers(false);
    srand(TMGetTime());
    gCommunications->ProcessGuaranteedLocalUserInput(new EEMStartLoading(theHumans, rand()), 0xff);
    gGamePreferences.SetLastGameOptions(gGameOptions);
    gGameOptions.mLock->ReadUnlock();
}

// FUNCTION: EXE 0x697897
// a client's connection to the lobby game's host (EEAJoinOrHostAGame::Virtual4 0x402120's join path; r23)
void EELobbyFn297897()
{
    UWideString theMessage;
    UString theAddress;
    UString theService;
    UWideString theGameName;
    EEGameOptionsPlayer thePlayer;
    gPlayerName = EELobbyFn296C32(LobbyFn361370()->mName);
    EELobbyFn2970E3();
    bool theFlag = LobbyFn361B20();
    if (gCommunications == 0) {
        gCommunications = new EEClient;
        ((EEClient*)gCommunications)->StartThreads();
    } else if (gCommunications->IsServer() == true) {
        if (gCommunications)
            delete gCommunications;
        gCommunications = 0;
        gCommunications = new EEClient;
        dynamic_cast<EEClient*>(gCommunications)->StartThreads();
    }
    if (gCommunications) {
        gGameOptions.mLock->WriteLock();
        gGameOptions = gLobbyGameOptions;
        gGameOptions.mIsMultiplayer = true;
        gGameOptions.mCheatCodes = false;
        gGameOptions.mLock->WriteUnlock();
        theService = "EEService";
        theGameName = gU4E5B98;
        theAddress = GetMyLobbyGame()->mAddress.GetHostString(true).c_str();
        UWideString theName = EELobbyFn296C32(LobbyFn361370()->mName);
        if (((NESynchronousClient*)gCommunications)->Connect(theAddress, theService, theGameName) == 0) {
            NEMClientConnectionRequest* theRequest =
                new NEMClientConnectionRequest(theName, gGameVersion, gBinaryChecksum, theFlag);
            ((NESynchronousClient*)gCommunications)->SendStartConnectMessage(theRequest);
            gCommunications->ProcessLocalUserInput(theRequest, 0xff);
        }
    }
}

// FUNCTION: EXE 0x697ac9
// back from the lobby to the game: the game window takes the lobby window's place, input resumes (r23)
void EELobbyFn297AC9()
{
    if (!gEELobbyShown || gEELobbyPlatformWindow.mPtr == NULL)
        return;
    gEELobbyShown = false;
    if (!gGamePreferences.mSettings.mFullScreen && gEELobbyPlatformWindow->Virtual4C()) {
        WONRectangle theRect;
        gEELobbyPlatformWindow->GetRect(theRect, false);
        MoveWindow(gGameWindow->GetOSRepresentation(), theRect.x, theRect.y, sEELobbyWidth, sEELobbyHeight, FALSE);
    }
    gUIManager->RedrawAll();
    if (gGamePreferences.mSettings.mFullScreen) {
        if (gISKeyboard != NULL)
            gISKeyboard->Resume();
        if (gISMouse != NULL)
            gISMouse->Resume();
    } else {
        ShowWindow(gGameWindow->GetOSRepresentation(), SW_RESTORE);
    }
    if (gMouse != NULL) {
        POINT theCursor;
        GetCursorPos(&theCursor);
        POINT thePoint;
        thePoint.x = 0;
        thePoint.y = 0;
        ClientToScreen(gGameWindow->GetOSRepresentation(), &thePoint);
        gMouse->SetPosition(GE2DPoint(thePoint.x, thePoint.y));
        gMouse->mPointer = 0;
    }
    gEELobbyPlatformWindow->Show(false);
}

// FUNCTION: EXE 0x697be0
// the lobby asked for an update: run Sierra Update and quit, or tell the player it is missing (r23)
void EELobbyFn297BE0()
{
    FSFileSpec theFile;
    UWideString theMessage;
    theFile.SetVolume(gStartupVolume);
    theFile.SetDirectory(gStartupDirectory + UString("Sierra Update\\"));
    theFile.SetFile("SierraUp.exe");
    if (theFile.Open(1) == keseOK) {
        theFile.Close();
        HINSTANCE theResult = ShellExecuteA(GetDesktopWindow(), "open",
            theFile.GetVolume() + theFile.GetDirectory() + theFile.GetFile(), NULL,
            theFile.GetVolume() + theFile.GetDirectory(), SW_SHOWNORMAL);
        if ((INT_PTR)theResult > 32) {
            PostQuitMessage(0);
            return;
        }
    }
    gResource->GetString(0x8031, theMessage);
    gUIManager->DisplayMessage(theMessage, true, 0, 0, true);
}

// FUNCTION: EXE 0x697d5d
void EELobbyFn297D5D()
{
    if (gUIManager != NULL) {
        EELobbyGameDoneEvent* theEvent = new EELobbyGameDoneEvent;
        gUIManager->GetFormByID(30)->SetFormOpenEvent(theEvent);
        theEvent->Dereference();
        gUIManager->Push(gUIManager->GetFormByID(30));
    }
}

// SYNTHETIC: EXE 0x697dc5
// (??_GEELobbySoundDecoder)

// FUNCTION: EXE 0x697de1
void EELobbyFn297DE1()
{
    if (gEELobbyOpen) {
        EELobbyFn29754D(true);
        UIForm* theForm = gUIManager->GetFormByID(30);
        if (gUIManager->GetCurrentForm() == theForm)
            gUIManager->Pop();
        LobbyFn362880();
        if (gEELobbyGameDone)
            LobbyMessageBox(gLobbyGameDoneTitle, gLobbyGameDoneText);
        EELobbyFn2976C7();
    }
}

// FUNCTION: EXE 0x697e45
void EELobbyFn297E45()
{
    EELobbyPostAction(EELobbyFn297DE1);
}

// FUNCTION: EXE 0x697e51
void EELobbyFn297E51()
{
    if (gEELobbyOpen) {
        gEELobbyOpen = false;
        EELobbyFn297AC9();
        gEELobbyRootWindow->Fn32D5B0();
        LobbyFn362170();
        gEELobbyWindowManager->Virtual08();
        delete gEELobbyWindowManager;
        gEELobbyWindowManager = NULL;
        gEELobbyPlatformWindow = NULL;
        gEELobbyRootWindow = NULL;
        if (gUIManager != NULL)
            gUIManager->SetMusicVolume(gGamePreferences.GetSettings().GetMusicVolume());
    }
}

// FUNCTION: EXE 0x697eca
bool EEIsLobbyOpen()
{
    return gEELobbyOpen;
}

// FUNCTION: EXE 0x697ed0
bool Fn297ED0()
{
    return gEELobbyShown;
}

// FUNCTION: EXE 0x697ed6
void EESetQuitFlag()
{
    if (!Fn297ED0())
        gEELobbyGameDone = true;
}

// SYNTHETIC: EXE 0x696aaa
// SYNTHETIC: EXE 0x696ab4
// SYNTHETIC: EXE 0x696abc
// SYNTHETIC: EXE 0x696ac8
// SYNTHETIC: EXE 0x696ad8
// SYNTHETIC: EXE 0x696ae2
// SYNTHETIC: EXE 0x696aea
// SYNTHETIC: EXE 0x696af6
// SYNTHETIC: EXE 0x696d13
