// C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\EELobbyInterface.cpp (file name inferred: no __FILE__;
// the gEELobbyInterface_* texts are its strings and it sorts between EELobbyInit.cpp and EELobbyMisc.cpp). The
// game's side of the external lobby: the lobby windows' globals, the game options read back from the lobby game,
// the local address, opening and closing the lobby through EEALobbyAction, the game-done event: 0x696aaa..0x697ee7
// (EELobbyOpen 0x6972c6 emits the ??_G of EELobbySoundDecoder 0x697dc5 and MSNativeIcon 0x697531).
// port: the lobby no longer has windows of its own. Its windows are rectangles of one lobby surface
// (ee/wonapi/WONLobbyPlatform.h) the size of the back buffer; the graphics thread shows its frames in the game's
// window while the lobby is shown (RTSGraphics::RenderLobby), on every system.
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
#include "ee/wonapi/WONLobbyPlatform.h"
#include "ee/RTSGraphics.h"
#include "platform/PlatWindow.h"
#include "ee/wonapi/EELobbyGameCode.h"
#include "ee/wonapi/EELobbySound.h"

using namespace WONAPI;

SmartPtr<PlatWindowManager> gEELobbyRootWindow;          // 0x965894
SmartPtr<Window> gEELobbyPlatformWindow;  // 0x965890

static WONAPICoreEx* gEELobbyWindowManager;      // 0x965898
static bool gEELobbyOpen;                   // 0x96589c
static bool gEELobbyShown;                  // 0x96589d
static bool gEELobbyClosing;                // 0x96589e
static long sEELobbyHeight;                 // 0x965888
static long sEELobbyWidth;                  // 0x96588c
// port: the lobby's screens are laid out for 1024x768 (the original switched the display mode); its surface keeps
// that size and RTSGraphics::RenderLobby shows it letterboxed in the window
static const int kEELobbySurfaceWidth = 1024;
static const int kEELobbySurfaceHeight = 768;
static bool gEELobbyGameDone;               // 0x9658a0
static unsigned long sEELobbyStartTime;     // 0x9658a8

void EELobbySlotToPlayer(EEGameOptionsPlayer& outPlayer, EELobbyPlayerSlot* inSlot);
void EELobbyFillPlayers(EEGameOptionsPlayer& outMyPlayer);
void EELobbyFillGameOptions(EEGameOptionsPlayer& outMyPlayer);
void EELobbySyncNetworkAdapter();
extern char gEmptyString[];                 // 0x8e5b98 (pooled ""; as EEWonNet.cpp declares it)
extern EEGameOptions& gLobbyGameOptions;    // 0x93030c (the lobby defaults; as EEGameOptions.cpp declares it)
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
    theAddress = theGame->GetAddress().GetHostString(true).c_str();
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
    gPlayerName = EELobbyToWideString(GetMyLobbyClient()->GetName());
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

// port: the game window's events the lobby passes on while it owns the event loop (close, quit, size, focus,
// exposure, the keys of its main window): handled as the game window's own (EEUserInterface::HandleEvent)
static void EELobbyForwardEvent(const PlatInputEvent& inEvent)
{
    EEUserInterface::HandleEvent(inEvent);
}

// FUNCTION: EXE 0x6972c6
// Opens the external lobby: WONAPI core, window manager, the lobby window and the lobby screen in it; then shows it
// and runs it until it is left
// port: the window manager and window are the platform backend's (WONLobbyPlatform.h): the lobby window fills the
// lobby surface (the back buffer's size), so the original's title, icon and owner window are gone
void EELobbyOpen()
{
    if (gEELobbyOpen)
        return;
    gEELobbyOpen = true;
    gEELobbyWindowManager = new WONAPICoreEx;
    gEELobbyWindowManager->SetDoPumpThread(true);
    gEELobbyWindowManager->Startup();
    gEELobbyRootWindow = new PlatWindowManager;
    gEELobbyRootWindow->mBA5 = false;
    JPGDecoder::Add(&gEELobbyRootWindow->mImageDecoder);
    PNGDecoder::Add(&gEELobbyRootWindow->mImageDecoder);
    gEELobbyRootWindow->SetSoundDecoder(new EELobbySoundDecoder);
    if (GESurface* theSecondary = gRasterizer->GetSecondarySurface(gScreenIndex))
        gEELobbyRootWindow->SetSurfaceSize(kEELobbySurfaceWidth, kEELobbySurfaceHeight);
    gEELobbyRootWindow->SetEventForward(EELobbyForwardEvent);
    gEELobbyPlatformWindow = CreateNativeWindow();
    static_cast<PlatGUIWindow*>(gEELobbyPlatformWindow.get())->SetForwardKeyMsgToParent(true);
    long theStyle = 2;
    if (gGamePreferences.mSettings.mFullScreen)
        theStyle = 0x22;
    gEELobbyPlatformWindow->mStyle = theStyle;
    WONRectangle theRect(0, 0, 100, 100);
    gEELobbyPlatformWindow->Create(theRect);
    RootContainer* theRoot = gEELobbyPlatformWindow->mRoot.get();
    EELobbyInit();
    SmartPtr<Container> theScreen = LobbyGetContainer();
    theRoot->Fn2F6140(theScreen.get(), 3, theRoot, 0, 0, 0, 0, 0);
    theRoot->AddChild(theScreen.get());
    EELobbyShow(true);
    LobbyStart();
    EELobbyRunLoop();
}

// SYNTHETIC: EXE 0x697531
// (??_GMSNativeIcon: the first user's COMDAT)

// port: the lobby's frame for RTSGraphics::RenderLobby (RTSGraphics.h). The caller holds EELobbyLock, under which
// EELobbyShow and EELobbyHide change gEELobbyShown (and EELobbyClose hides before it lets the manager go), so the
// manager outlives every locked frame.
bool EELobbyLockFrame(const uint32_t*& outPixels, int& outWidth, int& outHeight, int& outPitch, int& outLogicalWidth,
                      int& outLogicalHeight, uint32_t& outSerial)
{
    if (!gEELobbyShown || gEELobbyRootWindow.get() == NULL)
        return false;
    return gEELobbyRootWindow->LockFrame(outPixels, outWidth, outHeight, outPitch, outLogicalWidth, outLogicalHeight,
                                         outSerial);
}

// port: RenderLobby's pixel scale (the lobby at the window's pixels; the caller holds EELobbyLock)
void EELobbySetPresentation(float inScale, float inLeft, float inTop, float inWidth, float inHeight)
{
    if (gEELobbyShown && gEELobbyRootWindow.get() != NULL) {
        gEELobbyRootWindow->SetPresentRect(inLeft, inTop, inWidth, inHeight);
        gEELobbyRootWindow->SetPixelScale(inScale);
    }
}

void EELobbyGetSurfaceSize(int& outWidth, int& outHeight)
{
    outWidth = kEELobbySurfaceWidth;
    outHeight = kEELobbySurfaceHeight;
}

void EELobbyUnlockFrame()
{
    if (gEELobbyRootWindow.get() != NULL)
        gEELobbyRootWindow->UnlockFrame();
}

// FUNCTION: EXE 0x69754d
// Shows the lobby in the game's place, keeping the cursor where it was, and suspends the game's input
// port: the lobby is drawn into the game's window (RTSGraphics::RenderLobby), so the game window stays; its input is
// suspended in both modes (the mouse leaves relative mode), the system pointer shows and the lobby gets typed text.
// inFlag asked the original to flip to the GDI surface (full screen), which has no meaning here.
void EELobbyShow(bool inFlag)
{
    (void)inFlag;
    if (gEELobbyShown || gEELobbyPlatformWindow.get() == NULL)
        return;
    PlatWindow* theWindow = gGameWindow->GetOSRepresentation();
    GE2DPoint theCursor(0.0f, 0.0f);
    if (gMouse != NULL) {
        theCursor = gMouse->mPosition;
        gMouse->mPointer = 6;
    }
    if (GESurface* theSecondary = gRasterizer->GetSecondarySurface(gScreenIndex)) {
        sEELobbyWidth = (long)theSecondary->GetWidth();
        sEELobbyHeight = (long)theSecondary->GetHeight();
        gEELobbyRootWindow->SetSurfaceSize(kEELobbySurfaceWidth, kEELobbySurfaceHeight);
    }
    gEELobbyPlatformWindow->Show(true);
    EELobbyLock();
    gEELobbyShown = true;
    if (gISKeyboard != NULL)
        gISKeyboard->Suspend();
    if (gISMouse != NULL)
        gISMouse->Suspend();
    EELobbyUnlock();
    PlatTextInputStart(theWindow, 0, 0, 0, 0);
    PlatCursorShow(1);
    if (gMouse != NULL && theWindow != NULL && sEELobbyWidth > 0 && sEELobbyHeight > 0) {
        // the game's pointer is in back buffer pixels, the window's in client pixels
        int32_t theWidth = 0, theHeight = 0;
        PlatWindowGetClientSize(theWindow, &theWidth, &theHeight);
        PlatMouseWarp(theWindow, (int32_t)(theCursor.mX * theWidth / sEELobbyWidth),
                      (int32_t)(theCursor.mY * theHeight / sEELobbyHeight));
    }
    gEELobbyPlatformWindow->InvalidateRoot();
}

// FUNCTION: EXE 0x6976c7
// Runs the lobby until the player hosts or joins a game that connects (then shows the game screen) or leaves it
// (shutting the lobby down, and running Sierra Update when asked)
// Not identical: loop shape (the original keeps 0 in ebx and jumps back to the LobbyRun call)
// port: the lobby runs inside a posted action (EELobbyOpen, EELobbyReturnFromGame through EELobbyPostAction), and
// EEUserInterface::ProcessActions holds the UI manager's copy lock and the world's write lock around every action.
// The original's lobby was its own GDI window, so a graphics thread blocked on those locks (EEGraphics::FrameStarting)
// did not matter; here the graphics thread presents the lobby (RTSGraphics::RenderLobby), so they are released while
// the lobby runs and taken back, in ProcessActions' order, before the action returns.
class EELobbyActionLocksReleased {
public:
    EELobbyActionLocksReleased()
    {
        gWorld.mLock->WriteUnlock();
        gUIManager->UnlockUIForCopyData();
    }
    ~EELobbyActionLocksReleased()
    {
        gUIManager->LockUIForCopyData();
        gWorld.mLock->WriteLock();
    }
};

void EELobbyRunLoop()
{
    if (gEELobbyClosing)
        return;
    gEELobbyClosing = true;
    EELobbyActionLocksReleased theReleased;
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
    gPlayerName = EELobbyToWideString(GetMyLobbyClient()->GetName());
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
        theAddress = GetMyLobbyGame()->GetAddress().GetHostString(true).c_str();
        UWideString theName = EELobbyToWideString(GetMyLobbyClient()->GetName());
        if (static_cast<NESynchronousClient*>(gCommunications)->Connect(theAddress, theService, theGameName) == 0) {
            NEMClientConnectionRequest* theRequest =
                new NEMClientConnectionRequest(theName, gGameVersion, gBinaryChecksum, theFlag);
            static_cast<NESynchronousClient*>(gCommunications)->SendStartConnectMessage(theRequest);
            gCommunications->ProcessLocalUserInput(theRequest, 0xff);
        }
    }
}

// FUNCTION: EXE 0x697ac9
// Hides the lobby and gives the game back its window, resuming input and the game's cursor
// port: the reverse of EELobbyShow: the game's pointer goes where the system pointer is, the system pointer hides
// again (the game draws its own) with its default shape; typed text stays on (the game window always takes it)
void EELobbyHide()
{
    if (!gEELobbyShown || gEELobbyPlatformWindow.get() == NULL)
        return;
    PlatWindow* theWindow = gGameWindow->GetOSRepresentation();
    int32_t thePointerX = 0, thePointerY = 0, theWidth = 0, theHeight = 0;
    if (theWindow != NULL) {
        PlatMouseGetState(theWindow, &thePointerX, &thePointerY);
        PlatWindowGetClientSize(theWindow, &theWidth, &theHeight);
    }
    EELobbyLock();
    gEELobbyShown = false;
    EELobbyUnlock();
    gUIManager->RedrawAll();
    if (gISKeyboard != NULL)
        gISKeyboard->Resume();
    if (gISMouse != NULL)
        gISMouse->Resume();
    PlatCursorSet(NULL);
    PlatCursorShow(0);
    if (gMouse != NULL) {
        float theX = 0.0f, theY = 0.0f;
        if (theWidth > 0 && theHeight > 0) {
            theX = (float)thePointerX * sEELobbyWidth / theWidth;
            theY = (float)thePointerY * sEELobbyHeight / theHeight;
        }
        gMouse->SetPosition(GE2DPoint(theX, theY));
        gMouse->mPointer = 0;
    }
    gEELobbyPlatformWindow->Show(false);
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
        PlatCursorShow(0);   // port: closing the lobby's windows shows the system pointer; the game draws its own
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
