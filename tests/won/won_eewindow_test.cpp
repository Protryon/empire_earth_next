// The EE lobby window's functions R6 WONLobby round 29 made Rust (ee_won_lobby eewindow.rs: EELobbyOpen,
// EELobbyShow, EELobbyHide, EELobbyClose, EELobbyShowGameScreen) against the deleted functions as they were
// (won_eewindow_orig.h), and the network game functions round 30 moved (ee_won_lobby eegamefn.rs: EELobbyHostGame,
// EELobbyJoinGame, EELobbyStartHostedGame, EELobbyRunSierraUpdate, EEGetLocalAddress; won_eegamefn_orig.h), over one stand-in world (won_eewindow_world.h) that logs every call. The Rust's calls out
// are answered by World below: the glue host's new one-statement cases, their statements copied, and the older cases
// the functions reach (the flags, Show and Hide as the new C++ forwards, the run loop's). Each sequence picks the
// world's answers (the flags, the window, the mouse and its position, the back buffer's size, the input devices, the
// game window and its client size, the pointer, full screen, the UI manager) and runs one to five of the functions;
// the two sides' traces and the world's state after each must be the same. The exit code is the number of
// mismatches.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include "rust/won_lobby/eeglue.h"
#include "won_eewindow_world.h"
#include "won_eewindow_orig.h"
#include "won_eegamefn_orig.h"

using namespace WinWorld;
typedef EeWonEeGlueOp Op;
typedef EeWonEeGlueCall GL;

// the glue host's statics for the network game functions' locals, as EELobbyGlueHost.cpp has them
static unsigned long sEELobbyStartTime;
static EEGameOptionsPlayer sEELobbyHostMe;
static UString sEELobbyJoinAddress;
static UString sEELobbyJoinService;
static UWideString sEELobbyJoinGameName;
static UWideString sEELobbyJoinName;
static NEMessage* sEELobbyJoinRequest;
static EEGameOptionsPlayer sEELobbyStartPlayer;
static UWideString sEELocalAddress;

static intptr_t World(EeWonEeGlueOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t, intptr_t, intptr_t)
{
    FSFileSpec* aFile = reinterpret_cast<FSFileSpec*>(a);
    switch (inOp) {
    // the older cases the network game functions reach
    case Op::MyGame: return (intptr_t)GetMyLobbyGame();
    case Op::OptionsBegin:
        gGameOptions.mLock->WriteLock();
        gGameOptions = gLobbyGameOptions;
        gGameOptions.mIsMultiplayer = true;
        gGameOptions.mCheatCodes = false;
        return 0;
    case Op::OptionsEnd: gGameOptions.mLock->WriteUnlock(); return 0;
    // the statements of the network game functions (R6 WONLobby round 30: eegamefn.rs)
    case Op::HostMeNew: sEELobbyHostMe = EEGameOptionsPlayer();
        return 0;
    case Op::HostMe: return (intptr_t)&sEELobbyHostMe;
    case Op::SetPlayerNameFromClient: gPlayerName = EELobbyToWideString(GetMyLobbyClient()->GetName());
        return 0;
    case Op::BehindFirewall: return LobbyIsBehindFirewall() ? 1 : 0;
    case Op::CommExists: return gCommunications != NULL;
    case Op::CommDelete: delete gCommunications;
        return 0;
    case Op::CommClear: gCommunications = NULL;
        return 0;
    case Op::CommIsServer: return gCommunications->IsServer() == true;
    case Op::ServerNew: gCommunications = new EEServer(UWideString("TestGame"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, false, false);
        return 0;
    case Op::ServerStartThreads: static_cast<EEServer*>(gCommunications)->StartThreads();
        return 0;
    case Op::ServerPublished: return static_cast<EEServer*>(gCommunications)->GetGameWasPublished() ? 1 : 0;
    case Op::HostMeReady: sEELobbyHostMe.mIsReady = true;
        return 0;
    case Op::HostMeHuman: sEELobbyHostMe.mType = 1;
        return 0;
    case Op::HostRequest: gCommunications->ProcessLocalUserInput(new NEMRecord(kNetMessageClientConnectionRequest, {a != 0, gBinaryChecksum}, {gGameVersion, sEELobbyHostMe.mName}), 0xff);
        return 0;
    case Op::ClientNew: gCommunications = new EEClient;
        return 0;
    case Op::ClientStartThreads: static_cast<EEClient*>(gCommunications)->StartThreads();
        return 0;
    case Op::ClientStartThreadsDynamic: dynamic_cast<EEClient*>(gCommunications)->StartThreads();
        return 0;
    case Op::JoinSetService: sEELobbyJoinService = "EEService";
        return 0;
    case Op::JoinSetGameName: sEELobbyJoinGameName = gEmptyString;
        return 0;
    case Op::JoinSetAddress: sEELobbyJoinAddress = GetMyLobbyGame()->GetAddress().GetHostString(true).c_str();
        return 0;
    case Op::JoinSetName: sEELobbyJoinName = EELobbyToWideString(GetMyLobbyClient()->GetName());
        return 0;
    case Op::JoinConnect: return static_cast<NESynchronousClient*>(gCommunications)->Connect(sEELobbyJoinAddress, sEELobbyJoinService, sEELobbyJoinGameName) == 0;
    case Op::JoinRequestNew: sEELobbyJoinRequest = new NEMRecord(kNetMessageClientConnectionRequest, {a != 0, gBinaryChecksum}, {gGameVersion, sEELobbyJoinName});
        return 0;
    case Op::JoinSendStart: static_cast<NESynchronousClient*>(gCommunications)->SendStartConnectMessage(sEELobbyJoinRequest);
        return 0;
    case Op::JoinProcessRequest: gCommunications->ProcessLocalUserInput(sEELobbyJoinRequest, 0xff);
        return 0;
    case Op::OptionsReadLock: gGameOptions.mLock->ReadLock();
        return 0;
    case Op::StartTimeSet: sEELobbyStartTime = TMGetTime();
        return 0;
    case Op::ServerAllowNewPlayersOff: static_cast<EEServer*>(gCommunications)->AllowNewPlayers(false);
        return 0;
    case Op::ServerSetLoadingState: static_cast<EEServer*>(gCommunications)->SetGameState(2);
        return 0;
    case Op::ServerClearLoadResponses: static_cast<EEServer*>(gCommunications)->mNumLoadResponses = false;
        return 0;
    case Op::StartPlayerCopy: sEELobbyStartPlayer = gWorld.GetLobbyOptions().mPlayers[a];
        return 0;
    case Op::StartPlayerType: return (intptr_t)sEELobbyStartPlayer.mType;
    case Op::ServerStopPing: static_cast<EEServer*>(gCommunications)->StopPing();
        return 0;
    case Op::ServerResetXfer: static_cast<EEServer*>(gCommunications)->ResetXfer();
        return 0;
    case Op::ServerSetLoadState: static_cast<EEServer*>(gCommunications)->mLoadState = true;
        return 0;
    case Op::SeedRandom: srand(TMGetTime());
        return 0;
    case Op::StartLoading: gCommunications->ProcessGuaranteedLocalUserInput(new EEMRecord(kMessageStartLoading, {(unsigned char)a, rand()}), 0xff);
        return 0;
    case Op::SetLastGameOptions: gGamePreferences.SetLastGameOptions(gGameOptions);
        return 0;
    case Op::OptionsReadUnlock: gGameOptions.mLock->ReadUnlock();
        return 0;
    case Op::SierraFileNew: return (intptr_t)new FSFileSpec;
    case Op::MessageNew: return (intptr_t)new UWideString;
    case Op::SierraSetVolume: aFile->SetVolume(gStartupVolume);
        return 0;
    case Op::SierraSetDirectory: aFile->SetDirectory(gStartupDirectory + UString("Sierra Update\\"));
        return 0;
    case Op::SierraSetFile: aFile->SetFile("SierraUp.exe");
        return 0;
#ifdef _WIN32   // Sierra Update is a Windows program (elsewhere the file is never opened)
    case Op::SierraOpen: return aFile->Open(1) == keseOK;
    case Op::SierraClose: aFile->Close();
        return 0;
    case Op::SierraLaunch: return (INT_PTR)ShellExecuteA(GetDesktopWindow(), "open", aFile->GetVolume() + aFile->GetDirectory() + aFile->GetFile(), NULL, aFile->GetVolume() + aFile->GetDirectory(), SW_SHOWNORMAL) > 32;
#endif
    case Op::HasUserInterface: return gUserInterface != NULL;
    case Op::PostQuit: gUserInterface->GetThread()->Post(0x12, NULL, NULL, false);
        return 0;
    case Op::SierraFailMessage: gResource->GetString(IDS_UNABLE_TO_LAUNCH_UPDATE_UTILITY, *reinterpret_cast<UWideString*>(a));
        return 0;
    case Op::MessageShow: gUIManager->DisplayMessage(*reinterpret_cast<UWideString*>(a), true, NULL, 0, true);
        return 0;
    case Op::MessageFree: delete reinterpret_cast<UWideString*>(a);
        return 0;
    case Op::SierraFileFree: delete aFile;
        return 0;
    case Op::IsMultiplayerGame: return EEIsMultiplayerGame(true) ? 1 : 0;
    case Op::ProtocolLocalAddress: return (intptr_t)&reinterpret_cast<NEProtocol*>(a)->GetLocalAddress((unsigned long)b);
    case Op::GameAddressSet: sEELocalAddress = reinterpret_cast<LobbyGame*>(a)->GetAddress().GetHostString(true).c_str();
        return 0;
    case Op::GameAddressRef: return (intptr_t)&sEELocalAddress;

    // the older cases (the C++ forwards of Show and Hide go to the Rust)
    case Op::IsLobbyOpen: return gEELobbyOpen ? 1 : 0;
    case Op::IsShown: return gEELobbyShown ? 1 : 0;
    case Op::Show: ee_won_eeglue_call(GL::Show, 1, 0, 0); return 0;
    case Op::Hide: ee_won_eeglue_call(GL::Hide, 0, 0, 0); return 0;
    case Op::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    // the new ones, as EELobbyGlueHost.cpp has them
    case Op::SetOpenTrue: gEELobbyOpen = true;
        return 0;
    case Op::SetOpenFalse: gEELobbyOpen = false;
        return 0;
    case Op::CoreNew: gEELobbyWindowManager = new WONAPICoreEx;
        return 0;
    case Op::CoreSetPumpThread: gEELobbyWindowManager->SetDoPumpThread(true);
        return 0;
    case Op::CoreStartup: gEELobbyWindowManager->Startup();
        return 0;
    case Op::RootNew: gEELobbyRootWindow = new PlatWindowManager;
        return 0;
    case Op::RootNoPopupWindows: gEELobbyRootWindow->SetPopupsInOwnWindow(false);
        return 0;
    case Op::RootAddJpg: JPGDecoder::Add(&gEELobbyRootWindow->mImageDecoder);
        return 0;
    case Op::RootAddPng: PNGDecoder::Add(&gEELobbyRootWindow->mImageDecoder);
        return 0;
    case Op::RootSoundDecoder: gEELobbyRootWindow->SetSoundDecoder(new EELobbySoundDecoder);
        return 0;
    case Op::SecondarySurface: return (intptr_t)gRasterizer->GetSecondarySurface(gScreenIndex);
    case Op::RootSetSurfaceSize: gEELobbyRootWindow->SetSurfaceSize(kEELobbySurfaceWidth, kEELobbySurfaceHeight);
        return 0;
    case Op::RootSetEventForward: gEELobbyRootWindow->SetEventForward(EELobbyForwardEvent);
        return 0;
    case Op::WindowNew: gEELobbyPlatformWindow = CreateNativeWindow();
        return 0;
    case Op::WindowForwardKeys: static_cast<PlatGUIWindow*>(gEELobbyPlatformWindow.get())->SetForwardKeyMsgToParent(true);
        return 0;
    case Op::FullScreen: return gGamePreferences.mSettings.mFullScreen ? 1 : 0;
    case Op::WindowSetStyle: gEELobbyPlatformWindow->SetStyle((long)a);
        return 0;
    case Op::WindowCreate: gEELobbyPlatformWindow->Create(WONRectangle(0, 0, 100, 100));
        return 0;
    case Op::GlueInit: EELobbyInit();
        return 0;
    case Op::ScreenHold: return (intptr_t)LobbyGetContainer()->CreateRef();
    case Op::RootLayoutScreen: gEELobbyPlatformWindow->mRoot->Fn2F6140(reinterpret_cast<Container*>(a), 3, gEELobbyPlatformWindow->mRoot.get(), 0, 0, 0, 0, 0);
        return 0;
    case Op::RootAddScreen: gEELobbyPlatformWindow->mRoot->AddChild(reinterpret_cast<Container*>(a));
        return 0;
    case Op::LobbyStart: LobbyStart();
        return 0;
    case Op::WindowExists: return gEELobbyPlatformWindow.get() != NULL;
    case Op::GameOsWindow: return (intptr_t)gGameWindow->GetOSRepresentation();
    case Op::MouseExists: return gMouse != NULL;
    case Op::MousePosition: *reinterpret_cast<GE2DPoint*>(a) = gMouse->mPosition;
        return 0;
    case Op::MouseSetPointer: gMouse->mPointer = (long)a;
        return 0;
    case Op::SurfaceWidth: return (long)reinterpret_cast<GESurface*>(a)->GetWidth();
    case Op::SurfaceHeight: return (long)reinterpret_cast<GESurface*>(a)->GetHeight();
    case Op::WindowShow: gEELobbyPlatformWindow->Show(a != 0);
        return 0;
    case Op::LobbyLock: EELobbyLock();
        return 0;
    case Op::LobbyUnlock: EELobbyUnlock();
        return 0;
    case Op::SetShown: gEELobbyShown = a != 0;
        return 0;
    case Op::KeyboardExists: return gISKeyboard != NULL;
    case Op::KeyboardSuspend: gISKeyboard->Suspend();
        return 0;
    case Op::KeyboardResume: gISKeyboard->Resume();
        return 0;
    case Op::MouseInputExists: return gISMouse != NULL;
    case Op::MouseInputSuspend: gISMouse->Suspend();
        return 0;
    case Op::MouseInputResume: gISMouse->Resume();
        return 0;
    case Op::TextInputStart: PlatTextInputStart(reinterpret_cast<PlatWindow*>(a), 0, 0, 0, 0);
        return 0;
    case Op::CursorShow: PlatCursorShow((int)a);
        return 0;
    case Op::CursorSetDefault: PlatCursorSet(NULL);
        return 0;
    case Op::ClientSize: PlatWindowGetClientSize(reinterpret_cast<PlatWindow*>(a), &reinterpret_cast<int32_t*>(b)[0], &reinterpret_cast<int32_t*>(b)[1]);
        return 0;
    case Op::MouseWarp: PlatMouseWarp(reinterpret_cast<PlatWindow*>(a), (int32_t)b, (int32_t)c);
        return 0;
    case Op::MouseGetState: PlatMouseGetState(reinterpret_cast<PlatWindow*>(a), &reinterpret_cast<int32_t*>(b)[0], &reinterpret_cast<int32_t*>(b)[1]);
        return 0;
    case Op::WindowInvalidateRoot: gEELobbyPlatformWindow->InvalidateRoot();
        return 0;
    case Op::RedrawAll: gUIManager->RedrawAll();
        return 0;
    case Op::MouseSetPosition: gMouse->SetPosition(GE2DPoint(reinterpret_cast<const float*>(a)[0], reinterpret_cast<const float*>(a)[1]));
        return 0;
    case Op::RootCloseAll: gEELobbyRootWindow->CloseAllWindows();
        return 0;
    case Op::GlueLobbyShutdown: LobbyShutdown();
        return 0;
    case Op::CoreShutdown: gEELobbyWindowManager->Shutdown();
        return 0;
    case Op::CoreDelete: delete gEELobbyWindowManager;
        return 0;
    case Op::CoreClear: gEELobbyWindowManager = NULL;
        return 0;
    case Op::WindowClear: gEELobbyPlatformWindow = NULL;
        return 0;
    case Op::RootClear: gEELobbyRootWindow = NULL;
        return 0;
    case Op::HasUiManager: return gUIManager != NULL;
    case Op::MusicVolumeRestore: gUIManager->SetMusicVolume(gGamePreferences.GetSettings().GetMusicVolume());
        return 0;
    case Op::GameDoneEventNew: return (intptr_t)new EELobbyGameDoneEvent;
    case Op::GameFormSetOpenEvent: gUIManager->GetFormByID(30)->SetFormOpenEvent(reinterpret_cast<EELobbyGameDoneEvent*>(a));
        return 0;
    case Op::EventDereference: reinterpret_cast<EELobbyGameDoneEvent*>(a)->Dereference();
        return 0;
    case Op::PushGameForm: gUIManager->Push(gUIManager->GetFormByID(30));
        return 0;
    default:
        // the run loop's and the shutdown's own ops (the same Rust on both sides): traced, 0 (the loop ends)
        Log("glue op " + N((long long)inOp));
        return 0;
    }
}

static unsigned sSeed;
static unsigned R(unsigned n)
{
    sSeed = sSeed * 1103515245u + 12345u;
    return ((sSeed >> 16) & 0x7fff) % n;
}

// one world, made from its knobs (the same for both sides)
struct Knobs {
    bool mOpen, mShown, mWindow, mMouse, mSecondary, mKeyboard, mMouseInput, mGameWindow, mFullScreen, mUi;
    float mMouseX, mMouseY, mSurfaceW, mSurfaceH;
    int32_t mClientW, mClientH, mPointerX, mPointerY;
    int mComm, mPlayers[16];
    bool mPublished, mConnectOk, mBehind, mMulti, mGame, mOpenOk, mUserInterface;
    intptr_t mLaunch;
};
static Mouse sMouse;
static Device sKeyboard, sMouseDevice;
static GESurface sSecondary;
static PlatWindow sOsWindow;
static UIManager sUi;
static Container sScreenObject;
static NEProtocol sProtocol;
static void Make(const Knobs& k)
{
    gEELobbyOpen = k.mOpen;
    gEELobbyShown = k.mShown;
    gEELobbyRootWindow = k.mWindow ? new PlatWindowManager : NULL;
    gEELobbyPlatformWindow = k.mWindow ? CreateNativeWindow() : NULL;
    gEELobbyWindowManager = k.mWindow ? new WONAPICoreEx : NULL;
    sMouse.mPosition = GE2DPoint(k.mMouseX, k.mMouseY);
    sMouse.mPointer = 3;
    gMouse = k.mMouse ? &sMouse : NULL;
    sSecondary.mWidth = k.mSurfaceW;
    sSecondary.mHeight = k.mSurfaceH;
    gRasterizer->mSecondary = k.mSecondary ? &sSecondary : NULL;
    sKeyboard.mName = "keyboard";
    sMouseDevice.mName = "mouse";
    gISKeyboard = k.mKeyboard ? &sKeyboard : NULL;
    gISMouse = k.mMouseInput ? &sMouseDevice : NULL;
    sOsWindow.mId = 77;
    gGameWindow->mOs = k.mGameWindow ? &sOsWindow : NULL;
    gGamePreferences.mSettings.mFullScreen = k.mFullScreen;
    gUIManager = k.mUi ? &sUi : NULL;
    sClientW = k.mClientW;
    sClientH = k.mClientH;
    sPointerX = k.mPointerX;
    sPointerY = k.mPointerY;
    sScreenObject.mId = 9;
    sScreenObject.mRefs = 1;
    sScreen = &sScreenObject;
    // the network game functions' world
    if (gCommunications != NULL) {
        NECommunications* aOld = gCommunications;
        gCommunications = NULL;
        sTrace.clear();
        delete aOld;
    }
    gCommunications = k.mComm == 1 ? (NECommunications*)new EEServer("x", 0, 0, 0, false, false) : k.mComm == 2 ? (NECommunications*)new EEClient : NULL;
    sPublished = k.mPublished;
    sConnectOk = k.mConnectOk;
    sBehindFirewall = k.mBehind;
    sMultiplayer = k.mMulti;
    sMyGame.mAddress.mHost = "10.0.0.7";
    sMyGamePtr = k.mGame ? &sMyGame : NULL;
    sOpenOk = k.mOpenOk;
    sLaunchResult = k.mLaunch;
    gUserInterface = k.mUserInterface ? &sUserInterface : NULL;
    sMyClient.mName = "Max";
    gPlayerName = "";
    gGameOptions.mId = 0;
    gGameOptions.mIsMultiplayer = false;
    gGameOptions.mCheatCodes = true;
    sMessageIds = 0;
    gGameOptions.mLock = &sOptionsLock;
    gLobbyGameOptions.mId = 5;
    for (int i = 0; i < 16; i++) gWorld.mLobby.mPlayers[i].mType = k.mPlayers[i];
    sTicks = 1000;
    sTrace.clear();
}
static void Dump()
{
    Log("state open " + N(gEELobbyOpen) + " shown " + N(gEELobbyShown) + " root " + N(gEELobbyRootWindow.get() != NULL) +
        " window " + N(gEELobbyPlatformWindow.get() != NULL) + " core " + N(gEELobbyWindowManager != NULL) + " pointer " +
        N(sMouse.mPointer) + " screen refs " + N(sScreenObject.mRefs) + " comm " +
        (gCommunications == NULL ? "none" : gCommunications->mServer ? "server" : "client") + " player '" + gPlayerName +
        "' options " + N(gGameOptions.mId) + N(gGameOptions.mIsMultiplayer) + N(gGameOptions.mCheatCodes));
}

static int sChecks, sFailures;
static std::map<int, int> sFailedFns;
static const char* const kNames[10] = {"Open", "Show", "Hide", "Close", "ShowGameScreen", "HostGame", "JoinGame",
                                       "StartHostedGame", "RunSierraUpdate", "LocalAddress"};

int main(int argc, char** argv)
{
    int aSequences = argc > 1 ? atoi(argv[1]) : 20000;
    ee_won_eeglue_set_host(World);
    sProtocol.mAddress = "192.168.1.4";
    static const float kPositions[] = {0.0f, 1.0f, 333.3f, 512.0f, 1023.9f, -4.5f, 1e6f};
    static const float kSizes[] = {0.0f, 640.0f, 800.0f, 1024.0f, 1919.0f};
    static const int32_t kClients[] = {0, 1, 640, 1280, 2560, -2};
    for (int aSeq = 0; aSeq < aSequences; aSeq++) {
        sSeed = (unsigned)aSeq * 2654435761u + 1;
        Knobs k;
        k.mOpen = R(3) == 0;
        k.mShown = R(3) == 0;
        k.mWindow = R(5) != 0;
        k.mMouse = R(4) != 0;
        k.mSecondary = R(4) != 0;
        k.mKeyboard = R(3) != 0;
        k.mMouseInput = R(3) != 0;
        k.mGameWindow = R(5) != 0;
        k.mFullScreen = R(2) != 0;
        k.mUi = R(5) != 0;
        k.mMouseX = kPositions[R(7)];
        k.mMouseY = kPositions[R(7)];
        k.mSurfaceW = kSizes[R(5)];
        k.mSurfaceH = kSizes[R(5)];
        k.mClientW = kClients[R(6)];
        k.mClientH = kClients[R(6)];
        k.mPointerX = (int32_t)R(3000) - 100;
        k.mPointerY = (int32_t)R(2000) - 100;
        k.mComm = (int)R(3);
        for (int i = 0; i < 16; i++) k.mPlayers[i] = (int)R(4);
        k.mPublished = R(3) != 0;
        k.mConnectOk = R(3) != 0;
        k.mBehind = R(2) != 0;
        k.mMulti = R(3) != 0;
        k.mGame = R(4) != 0;
        k.mOpenOk = R(2) != 0;
        k.mUserInterface = R(4) != 0;
        static const intptr_t kLaunch[] = {0, 2, 32, 33, 42, -1};
        k.mLaunch = kLaunch[R(6)];
        // the network game functions (5..9) only with a server when they start the hosted game or a game when
        // they join (as the lobby calls them)
        int aCount = 1 + (int)R(5);
        int aFns[5];
        for (int i = 0; i < aCount; i++) aFns[i] = (int)R(10);
        for (int i = 0; i < aCount; i++)
            if ((aFns[i] == 7 && k.mComm != 1) || (aFns[i] == 6 && !k.mGame)) aFns[i] = 9;
        std::vector<std::string> aTraces[2];
        for (int aSide = 0; aSide < 2; aSide++) {
            ee_won_eeglue_call(GL::Reset, 0, 0, 0);
            WinOrig::sEELobbyWidth = WinOrig::sEELobbyHeight = 0;
            Make(k);
            sTrace.clear();
            for (int i = 0; i < aCount; i++) {
                // the hosted game starts only on a server (the lobby's order)
                if (aFns[i] == 7 && (gCommunications == NULL || !gCommunications->mServer)) aFns[i] = 9;
                Log(std::string("call ") + kNames[aFns[i]]);
                if (aSide == 0) {
                    switch (aFns[i]) {
                    case 0: WinOrig::EELobbyOpen(); break;
                    case 1: WinOrig::EELobbyShow(true); break;
                    case 2: WinOrig::EELobbyHide(); break;
                    case 3: WinOrig::EELobbyClose(); break;
                    case 4: WinOrig::EELobbyShowGameScreen(); break;
                    case 5: Log("result " + N(WinOrig::EELobbyHostGame())); break;
                    case 6: WinOrig::EELobbyJoinGame(); break;
                    case 7: WinOrig::EELobbyStartHostedGame(); break;
                    case 8: WinOrig::EELobbyRunSierraUpdate(); break;
                    default: Log("address " + WinOrig::EEGetLocalAddress(&sProtocol, 3)); break;
                    }
                } else {
                    static const GL kCalls[10] = {GL::Open, GL::Show, GL::Hide, GL::Close, GL::ShowGameScreen, GL::HostGame,
                                                  GL::JoinGame, GL::StartHostedGame, GL::RunSierraUpdate, GL::LocalAddress};
                    intptr_t r = ee_won_eeglue_call(kCalls[aFns[i]], aFns[i] == 1 ? 1 : aFns[i] == 9 ? (intptr_t)&sProtocol : 0,
                                                    aFns[i] == 9 ? 3 : 0, 0);
                    if (aFns[i] == 5) Log("result " + N(r != 0));
                    if (aFns[i] == 9) Log("address " + *reinterpret_cast<UWideString*>(r));
                }
                Dump();
            }
            aTraces[aSide].swap(sTrace);
        }
        sChecks++;
        if (aTraces[0] == aTraces[1]) continue;
        sFailedFns[aFns[0]]++;
        if (sFailures++ < 6) {
            printf("FAIL sequence %d:\n", aSeq);
            for (size_t i = 0; i < aTraces[0].size() || i < aTraces[1].size(); i++)
                printf("  %-58s | %s\n", i < aTraces[0].size() ? aTraces[0][i].c_str() : "", i < aTraces[1].size() ? aTraces[1][i].c_str() : "");
        }
    }
    for (const auto& f : sFailedFns) printf("mismatching: first %s x %d\n", kNames[f.first], f.second);
    printf("won_eewindow_test: %d sequences, %d mismatches\n", sChecks, sFailures);
    return sFailures;
}
