// C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\EELobbyInterface.cpp (file name inferred: no __FILE__;
// the gEELobbyInterface_* texts are its strings and it sorts between EELobbyInit.cpp and EELobbyMisc.cpp). The
// game's side of the external lobby: the lobby windows' globals, the game options read back from the lobby game,
// the local address, opening and closing the lobby through EEALobbyAction, the game-done event: 0x696aaa..0x697ee7
// (EELobbyOpen 0x6972c6 emits the ??_G of EELobbySoundDecoder 0x697dc5 and MSNativeIcon 0x697531).
// (EELobbyShow 0x69754d called IDirectDraw7::FlipToGDISurface through the DX7 screen; the renderer's GX layer stands in
// for it)
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
#include "ee/EEStringIDs.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/EELobbyGameCode.h"
#include "ee/wonapi/EELobbySound.h"

using namespace WONAPI;

SmartPtr<MSWindowManager> gEELobbyRootWindow;          // 0x965894
SmartPtr<MSWindow> gEELobbyPlatformWindow;  // 0x965890

static WONAPICoreEx* gEELobbyWindowManager;      // 0x965898
static bool gEELobbyOpen;                   // 0x96589c
static bool gEELobbyShown;                  // 0x96589d
static bool gEELobbyClosing;                // 0x96589e
static long sEELobbyHeight;                 // 0x965888
static long sEELobbyWidth;                  // 0x96588c
static bool gEELobbyGameDone;               // 0x9658a0
static unsigned long sEELobbyStartTime;     // 0x9658a8

void EELobbySlotToPlayer(EEGameOptionsPlayer& outPlayer, EELobbyPlayerSlot* inSlot);
void EELobbyFillPlayers(EEGameOptionsPlayer& outMyPlayer);
void EELobbyFillGameOptions(EEGameOptionsPlayer& outMyPlayer);
void EELobbySyncNetworkAdapter();
extern char gEmptyString[];                 // 0x8e5b98 (pooled ""; as EEWonNet.cpp declares it)
extern EEGameOptions& gLobbyGameOptions;    // 0x93030c (the lobby defaults; as EEGameOptions.cpp declares it)
extern HINSTANCE gInstance;                 // 0x9319d8 (as Empire Earth.cpp declares it)
void EELobbyInit();                         // 0x695511 (EELobbyInit.cpp)
UWideString EELobbyToWideString(const GUIString& inText);
void EELobbyOpen();
void EELobbyPostAction(void (*inFunc)());
void EELobbyShow(bool inFlag);
void EELobbyRunLoop();
void EELobbyStartHostedGame();
void EELobbyJoinGame();
void EELobbyHide();
void EELobbyRunSierraUpdate();
void EELobbyShowGameScreen();
void EELobbyLock();
void EELobbyUnlock();
void EELobbyReturnFromGame();
void EELobbyPostReturnFromGame();
bool EEIsLobbyOpen();
bool EELobbyIsShown();
bool EELobbyRunConnectDialog();
void EELobbyShutdown();

// the game's actions the lobby posts to the user interface: one function to call (vtable 0x84c6c4)
class EEALobbyAction : public EEAction {
public:
    EEALobbyAction(void (*inFunc)()) : mFunc(inFunc) {}
    virtual void Virtual4() { this->mFunc(); }  // +0x04 0x2972c3
    virtual void Virtual8() {}              // +0x08 (ICF-folded 0x1720c6)

    void (*mFunc)();                        // +0x0c
};

// the lobby form's open event once a game is done (vtable 0x84c6e8; ??_G ICF-folded 0x5e60e7)
class EELobbyGameDoneEvent : public UIEvent {
public:
    EELobbyGameDoneEvent() : mCount(0) {}
    virtual ~EELobbyGameDoneEvent() {}
    virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity);

    long mCount;                            // +0x0c
};

// FUNCTION: EXE 0x696b06
// Copies all 16 lobby player slots into the game options' players and returns the local player's entry
void EELobbyFillPlayers(EEGameOptionsPlayer& outMyPlayer)
{
    bool theRandom;
    int i;
    EELobbyGame* theGame = static_cast<EELobbyGame*>(GetMyLobbyGame());
    if (theGame == NULL)
        return;
    theRandom = theGame->HasGameFile();
    for (i = 0; i < 16; i++) {
        EEGameOptionsPlayer thePlayer;
        EELobbySlotToPlayer(thePlayer, theGame->GetPlayerSlot(i));
        thePlayer.mSlot = i;
        thePlayer.mIsLocal = theGame->GetMySlot() == i;
        if (thePlayer.mIsLocal)
            outMyPlayer = thePlayer;
        if (theRandom)
            thePlayer.mLoadedType = thePlayer.mType;
        gGameOptions.mPlayers[i] = thePlayer;
    }
}

// FUNCTION: EXE 0x696b9a
// Converts a lobby player slot to a game options player: computer, human or observer, with name, colour (humans only),
// team and starting citizens; open, closed and missing slots become type 4
void EELobbySlotToPlayer(EEGameOptionsPlayer& outPlayer, EELobbyPlayerSlot* inSlot)
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
        outPlayer.mName = EELobbyToWideString(inSlot->GetName());
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
// Converts a lobby GUI string to a game wide string
UWideString EELobbyToWideString(const GUIString& inText)
{
    return UWideString(WStringToString(inText, -1).c_str());
}

// FUNCTION: EXE 0x696c89
// Returns the lobby game's host address in a multiplayer lobby game, else the protocol's local address
UWideString& EEGetLocalAddress(NEProtocol* inProtocol, unsigned long inAdapter)
{
    LobbyGame* theGame = NULL;
    if (EEIsMultiplayerGame(true))
        theGame = GetMyLobbyGame();
    if (theGame == NULL)
        return inProtocol->GetLocalAddress(inAdapter);
    static UWideString theAddress;
    theAddress = theGame->mAddress.GetHostString(true).c_str();
    return theAddress;
}

// FUNCTION: EXE 0x696d1e
// Hosts the lobby game: replaces the connection with a new game server and, once the game is published, sends the
// host's own connection request; returns whether it was published
bool EELobbyHostGame()
{
    UWideString theUnused1;
    UString theUnused2;
    UString theUnused3;
    UWideString theUnused4;
    EEGameOptionsPlayer theMe;
    gPlayerName = EELobbyToWideString(GetMyLobbyClient()->mName);
    EELobbySyncNetworkAdapter();
    bool theFlag = LobbyIsBehindFirewall();
    if (gCommunications != NULL) {
        delete gCommunications;
        gCommunications = NULL;
    }
    gCommunications = new EEServer(UWideString("TestGame"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, false, false);
    static_cast<EEServer*>(gCommunications)->StartThreads();
    bool theResult = false;
    if (!static_cast<EEServer*>(gCommunications)->GetGameWasPublished()) {
        delete gCommunications;
        gCommunications = NULL;
    } else {
        EELobbyFillGameOptions(theMe);
        theMe.mIsReady = true;
        theMe.mType = 1;
        gCommunications->ProcessLocalUserInput(
            new NEMClientConnectionRequest(theMe.mName, gGameVersion, gBinaryChecksum, theFlag), 0xff);
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x696eac
// Fills the game options from the lobby defaults and the lobby game's settings (load type, map, resources, epochs,
// difficulty, speed, limits, flags), then the players
void EELobbyFillGameOptions(EEGameOptionsPlayer& outMyPlayer)
{
    EELobbyGame* theGame = static_cast<EELobbyGame*>(GetMyLobbyGame());
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
        gGameOptions.mMapType = EELobbyToWideString(theGame->GetGameFileName());
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
    EELobbyFillPlayers(outMyPlayer);
    gGameOptions.mLock->WriteUnlock();
}

// FUNCTION: EXE 0x6970e3
// Sets the game's network adapter to the one-based index of the local host's address (0 when not found)
void EELobbySyncNetworkAdapter()
{
    unsigned long theIndex = 0;
    long theHandle = WONAPI::IPAddr::GetLocalHost();
    if (theHandle != 0) {
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
    gNetworkAdapter = theIndex;
}

// TEMPLATE: EXE 0x698bf9
// TEMPLATE: EXE 0x698c1c
// (list<LobbyNetAddress>'s ctor and dtor for EELobbySyncNetworkAdapter's local: the linker placed the COMDATs inside
// EELobbyMisc.cpp's range, which does not instantiate them)

// FUNCTION: EXE 0x69713f
// Hides the game form's controls; on the second time the form opens, returns to the open lobby
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
    this->mCount++;
    if (this->mCount == 2 && EEIsLobbyOpen())
        EELobbyPostReturnFromGame();
    return keseOK;
}

// FUNCTION: EXE 0x697276
// Asks the user interface thread to open the external lobby
void EEStartExternalLobby()
{
    EELobbyPostAction(EELobbyOpen);
}

// FUNCTION: EXE 0x697282
// Posts a function for the user interface thread to call
void EELobbyPostAction(void (*inFunc)())
{
    EEALobbyAction* theAction = new EEALobbyAction(inFunc);
    gUserInterface->PostAction(theAction);
    theAction->Dereference();
}

// TEMPLATE: EXE 0x6972c3
// (EEALobbyAction::Virtual4, inline: its COMDAT follows its first user)

// FUNCTION: EXE 0x6972c6
// Opens the external lobby: WONAPI core, window manager, the lobby window with the game window's title and icon and
// the lobby screen in it; then shows it and runs it until it is left
// Not identical: (1) the original loads the window and its vtable before it builds the rectangle (a temporary's
// shape), but a WONRectangle temporary here moves below theClass in the frame and loses the esi = 0x100 reuse;
// (2) the original keeps theClass.hIcon in edi from the test on
void EELobbyOpen()
{
#ifdef _WIN32   // the lobby's windows are Win32 windows (PORT.md step 4.3f): it does not open elsewhere
    char theText[256];
    WNDCLASSA theClass;
    if (gEELobbyOpen)
        return;
    gEELobbyOpen = true;
    gEELobbyWindowManager = new WONAPICoreEx;
    gEELobbyWindowManager->SetDoPumpThread(true);
    gEELobbyWindowManager->Startup();
    gEELobbyRootWindow = new MSWindowManager(NULL);
    gEELobbyRootWindow->mBA5 = false;
    JPGDecoder::Add(&gEELobbyRootWindow->mImageDecoder);
    PNGDecoder::Add(&gEELobbyRootWindow->mImageDecoder);
    gEELobbyRootWindow->SetSoundDecoder(new EELobbySoundDecoder);
    gEELobbyPlatformWindow = new MSWindow;
    gEELobbyPlatformWindow->mForwardKeyMsgToParent = true;
    long theStyle = 2;
    if (gGamePreferences.mSettings.mFullScreen)
        theStyle = 0x22;
    gEELobbyPlatformWindow->mStyle = theStyle;
    GetWindowTextA((HWND)gGameWindow->GetNativeHandle(), theText, 256);
    gEELobbyPlatformWindow->SetTitle(theText);
    if (gGamePreferences.mSettings.mFullScreen)
        SetMSWindowDefaultOwner((HWND)gGameWindow->GetNativeHandle());
    WONRectangle theRect(0, 0, 100, 100);
    gEELobbyPlatformWindow->Create(theRect);
    SetMSWindowDefaultOwner(NULL);
    if (GetClassNameA((HWND)gGameWindow->GetNativeHandle(), theText, 256) &&
        GetClassInfoA(gInstance, theText, &theClass) && theClass.hIcon != NULL)
        gEELobbyPlatformWindow->SetIcon(new MSNativeIcon(gEELobbyPlatformWindow->mDisplayContext.get(), theClass.hIcon));
    RootContainer* theRoot = gEELobbyPlatformWindow->mRoot.get();
    EELobbyInit();
    SmartPtr<Container> theScreen = LobbyGetContainer();
    theRoot->Fn2F6140(theScreen.get(), 3, theRoot, 0, 0, 0, 0, 0);
    theRoot->AddChild(theScreen.get());
    EELobbyShow(true);
    LobbyStart();
    EELobbyRunLoop();
#endif
}

// SYNTHETIC: EXE 0x697531
// (??_GMSNativeIcon: the first user's COMDAT)

// The DX7 screen's declarations come here, not with the other includes: DX7Screen.h declares three class statics
// (sScreens, sScreensCaptured, sFirstCaptureWasAttachedToDesktop; DX7HRDisplay.dll exports them), and the unit's
// $E numbers (init chains 0x296aaa.., EEGetLocalAddress's static 0x296d13) show none of them was declared before.
#include "gx/GX.h"
#include "dx7/DX7Screen.h"

// FUNCTION: EXE 0x69754d
// Puts the lobby window in the game window's place, keeping the cursor where it was, and suspends the game's input
// (full screen: flips to the GDI surface when asked) or hides the game window
// STAND-IN (owner-approved, docs/LOST_DEBUG.md): the original adds the cursor's y as fld [y]; fiadd [point.y]
// (x: fild; fadd). The (double) cast on theCursor.mY in the y sum reproduces that order.
void EELobbyShow(bool inFlag)
{
#ifdef _WIN32   // the lobby replaces the game window (Win32 only)
    if (gEELobbyShown || gEELobbyPlatformWindow.mPtr == NULL)
        return;
    RECT theRect;
    GetWindowRect((HWND)gGameWindow->GetNativeHandle(), &theRect);
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
        ClientToScreen((HWND)gGameWindow->GetNativeHandle(), &thePoint);
        SetCursorPos(thePoint.x + theCursor.mX, thePoint.y + (double)theCursor.mY);
    }
    EELobbyLock();
    gEELobbyShown = true;
    if (gGamePreferences.mSettings.mFullScreen) {
        if (gISKeyboard != NULL)
            gISKeyboard->Suspend();
        if (gISMouse != NULL)
            gISMouse->Suspend();
        if (inFlag)
            ((GXDirectDraw*)*static_cast<DX7Screen*>(gRasterizer->GetScreen(gScreenIndex)))->FlipToGDISurface();
    } else {
        ShowWindow((HWND)gGameWindow->GetNativeHandle(), SW_HIDE);
    }
    EELobbyUnlock();
    for (int theCount = 1 - ShowCursor(TRUE); theCount > 0; theCount--)
        ShowCursor(TRUE);
    gEELobbyPlatformWindow->InvalidateRoot();
#endif
}

// FUNCTION: EXE 0x6976c7
// Runs the lobby until the player hosts or joins a game that connects (then shows the game screen) or leaves it
// (shutting the lobby down, and running Sierra Update when asked)
// Not identical: loop shape (the original keeps 0 in ebx and jumps back to the LobbyRun call)
void EELobbyRunLoop()
{
    if (gEELobbyClosing)
        return;
    gEELobbyClosing = true;
    for (;;) {
        int theResult = LobbyRun();
        if (theResult == 2) {
            if (EELobbyRunConnectDialog()) {
                EELobbyPostAction(EELobbyStartHostedGame);
                EELobbyShowGameScreen();
                LobbySuspend();
                EELobbyHide();
                break;
            }
        } else if (theResult == 1) {
            EELobbyJoinGame();
            if (EELobbyRunConnectDialog()) {
                EELobbyShowGameScreen();
                LobbySuspend();
                EELobbyHide();
                break;
            }
        } else {
            if (theResult == 3) {
                EELobbyShutdown();
                EELobbyRunSierraUpdate();
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
// Starts loading the hosted game: closes it to new players, counts the humans, sends the start message with a random
// seed and saves the options as the last game's
void EELobbyStartHostedGame()
{
    EEGameOptionsPlayer thePlayer;
    unsigned char theHumans = 0;
    gGameOptions.mLock->ReadLock();
    sEELobbyStartTime = TMGetTime();
    static_cast<EEServer*>(gCommunications)->AllowNewPlayers(false);
    static_cast<EEServer*>(gCommunications)->SetGameState(2);
    static_cast<EEServer*>(gCommunications)->mNumLoadResponses = false;
    for (int i = 0; i < 16; i++) {
        thePlayer = gWorld.GetLobbyOptions().mPlayers[i];
        if (thePlayer.mType == 1 || thePlayer.mType == 2)
            theHumans++;
    }
    static_cast<EEServer*>(gCommunications)->StopPing();
    static_cast<EEServer*>(gCommunications)->ResetXfer();
    static_cast<EEServer*>(gCommunications)->mLoadState = true;
    static_cast<EEServer*>(gCommunications)->AllowNewPlayers(false);
    srand(TMGetTime());
    gCommunications->ProcessGuaranteedLocalUserInput(new EEMStartLoading(theHumans, rand()), 0xff);
    gGamePreferences.SetLastGameOptions(gGameOptions);
    gGameOptions.mLock->ReadUnlock();
}

// FUNCTION: EXE 0x697897
// Joins the lobby game: makes this a client, resets the game options and connects to the host's address
void EELobbyJoinGame()
{
    UWideString theMessage;
    UString theAddress;
    UString theService;
    UWideString theGameName;
    EEGameOptionsPlayer thePlayer;
    gPlayerName = EELobbyToWideString(GetMyLobbyClient()->mName);
    EELobbySyncNetworkAdapter();
    bool theFlag = LobbyIsBehindFirewall();
    if (gCommunications == NULL) {
        gCommunications = new EEClient;
        static_cast<EEClient*>(gCommunications)->StartThreads();
    } else if (gCommunications->IsServer() == true) {
        if (gCommunications)
            delete gCommunications;
        gCommunications = NULL;
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
        theGameName = gEmptyString;
        theAddress = GetMyLobbyGame()->mAddress.GetHostString(true).c_str();
        UWideString theName = EELobbyToWideString(GetMyLobbyClient()->mName);
        if (static_cast<NESynchronousClient*>(gCommunications)->Connect(theAddress, theService, theGameName) == 0) {
            NEMClientConnectionRequest* theRequest =
                new NEMClientConnectionRequest(theName, gGameVersion, gBinaryChecksum, theFlag);
            static_cast<NESynchronousClient*>(gCommunications)->SendStartConnectMessage(theRequest);
            gCommunications->ProcessLocalUserInput(theRequest, 0xff);
        }
    }
}

// FUNCTION: EXE 0x697ac9
// Hides the lobby window and puts the game window back in its place, resuming input and the game's cursor
void EELobbyHide()
{
#ifdef _WIN32   // back from the lobby (Win32 only)
    if (!gEELobbyShown || gEELobbyPlatformWindow.mPtr == NULL)
        return;
    gEELobbyShown = false;
    if (!gGamePreferences.mSettings.mFullScreen && gEELobbyPlatformWindow->PlatformIsVisible()) {
        WONRectangle theRect;
        gEELobbyPlatformWindow->GetRect(theRect, false);
        MoveWindow((HWND)gGameWindow->GetNativeHandle(), theRect.x, theRect.y, sEELobbyWidth, sEELobbyHeight, FALSE);
    }
    gUIManager->RedrawAll();
    if (gGamePreferences.mSettings.mFullScreen) {
        if (gISKeyboard != NULL)
            gISKeyboard->Resume();
        if (gISMouse != NULL)
            gISMouse->Resume();
    } else {
        ShowWindow((HWND)gGameWindow->GetNativeHandle(), SW_RESTORE);
    }
    if (gMouse != NULL) {
        POINT theCursor;
        GetCursorPos(&theCursor);
        POINT thePoint;
        thePoint.x = 0;
        thePoint.y = 0;
        ClientToScreen((HWND)gGameWindow->GetNativeHandle(), &thePoint);
        gMouse->SetPosition(GE2DPoint(thePoint.x, thePoint.y));
        gMouse->mPointer = 0;
    }
    gEELobbyPlatformWindow->Show(false);
#endif
}

// FUNCTION: EXE 0x697be0
// Runs Sierra Update and quits the game, or tells the player it is missing
void EELobbyRunSierraUpdate()
{
    FSFileSpec theFile;
    UWideString theMessage;
    theFile.SetVolume(gStartupVolume);
    theFile.SetDirectory(gStartupDirectory + UString("Sierra Update\\"));
    theFile.SetFile("SierraUp.exe");
#ifdef _WIN32   // Sierra Update is a Windows program
    if (theFile.Open(1) == keseOK) {
        theFile.Close();
        HINSTANCE theResult = ShellExecuteA(GetDesktopWindow(), "open",
            theFile.GetVolume() + theFile.GetDirectory() + theFile.GetFile(), NULL,
            theFile.GetVolume() + theFile.GetDirectory(), SW_SHOWNORMAL);
        if ((INT_PTR)theResult > 32) {
            if (gUserInterface != NULL)   // PostQuitMessage: WM_QUIT to the UI thread's queue
                gUserInterface->mThread->Post(0x12, NULL, NULL, false);
            return;
        }
    }
#endif
    gResource->GetString(IDS_UNABLE_TO_LAUNCH_UPDATE_UTILITY, theMessage);
    gUIManager->DisplayMessage(theMessage, true, NULL, 0, true);
}

// FUNCTION: EXE 0x697d5d
// Pushes the game form (form 30), hooking it to return to the lobby once the game is done
void EELobbyShowGameScreen()
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
// Brings the open lobby back after a game: shows it, closes the game form, resumes it, reports a dissolved game and
// runs it again
void EELobbyReturnFromGame()
{
    if (gEELobbyOpen) {
        EELobbyShow(true);
        UIForm* theForm = gUIManager->GetFormByID(30);
        if (gUIManager->GetCurrentForm() == theForm)
            gUIManager->Pop();
        LobbyResume();
        if (gEELobbyGameDone)
            LobbyMessageBox(::RoutingLogic_GameDissolvedTitle_String, ::RoutingLogic_GameDissolvedDetails_String);
        EELobbyRunLoop();
    }
}

// FUNCTION: EXE 0x697e45
// Asks the user interface thread to return to the lobby
void EELobbyPostReturnFromGame()
{
    EELobbyPostAction(EELobbyReturnFromGame);
}

// FUNCTION: EXE 0x697e51
// Closes the external lobby, its windows and its WONAPI core, and restores the game's music volume
void EELobbyClose()
{
    if (gEELobbyOpen) {
        gEELobbyOpen = false;
        EELobbyHide();
        gEELobbyRootWindow->CloseAllWindows();
        LobbyShutdown();
        gEELobbyWindowManager->Shutdown();
        delete gEELobbyWindowManager;
        gEELobbyWindowManager = NULL;
        gEELobbyPlatformWindow = NULL;
        gEELobbyRootWindow = NULL;
        if (gUIManager != NULL)
            gUIManager->SetMusicVolume(gGamePreferences.GetSettings().GetMusicVolume());
    }
}

// FUNCTION: EXE 0x697eca
// Returns whether the external lobby is open
bool EEIsLobbyOpen()
{
    return gEELobbyOpen;
}

// FUNCTION: EXE 0x697ed0
// Returns whether the lobby window is shown
bool EELobbyIsShown()
{
    return gEELobbyShown;
}

// FUNCTION: EXE 0x697ed6
// Marks the lobby game as dissolved, unless the lobby is shown
void EESetQuitFlag()
{
    if (!EELobbyIsShown())
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
