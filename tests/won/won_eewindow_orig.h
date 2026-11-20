// The EE lobby window's functions R6 WONLobby round 29 moved to Rust (ee_won_lobby eewindow.rs), as they were
// (copied by script from EELobbyGlueHost.cpp at d26bf2cb1), for won_eewindow_test over won_eewindow_world.h's
// stand-ins.
#ifndef WON_EEWINDOW_ORIG_H
#define WON_EEWINDOW_ORIG_H
namespace WinOrig {
using namespace WinWorld;
typedef EeWonEeGlueCall GL;
static intptr_t Glue(GL inCall, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0) { return ee_won_eeglue_call(inCall, a, b, c); }
static long sEELobbyHeight;
static long sEELobbyWidth;
void EELobbyShow(bool inFlag);
void EELobbyHide();

void EELobbyOpen()
{
    if (gEELobbyOpen)
        return;
    gEELobbyOpen = true;
    gEELobbyWindowManager = new WONAPICoreEx;
    gEELobbyWindowManager->SetDoPumpThread(true);
    gEELobbyWindowManager->Startup();
    gEELobbyRootWindow = new PlatWindowManager;
    gEELobbyRootWindow->SetPopupsInOwnWindow(false);
    JPGDecoder::Add(&gEELobbyRootWindow->mImageDecoder);
    PNGDecoder::Add(&gEELobbyRootWindow->mImageDecoder);
    gEELobbyRootWindow->SetSoundDecoder(new EELobbySoundDecoder);
    if (gRasterizer->GetSecondarySurface(gScreenIndex) != NULL)
        gEELobbyRootWindow->SetSurfaceSize(kEELobbySurfaceWidth, kEELobbySurfaceHeight);
    gEELobbyRootWindow->SetEventForward(EELobbyForwardEvent);
    gEELobbyPlatformWindow = CreateNativeWindow();
    static_cast<PlatGUIWindow*>(gEELobbyPlatformWindow.get())->SetForwardKeyMsgToParent(true);
    long theStyle = 2;
    if (gGamePreferences.mSettings.mFullScreen)
        theStyle = 0x22;
    gEELobbyPlatformWindow->SetStyle(theStyle);
    WONRectangle theRect(0, 0, 100, 100);
    gEELobbyPlatformWindow->Create(theRect);
    RootContainer* theRoot = gEELobbyPlatformWindow->mRoot.get();
    EELobbyInit();
    SmartPtr<Container> theScreen = LobbyGetContainer();
    theRoot->Fn2F6140(theScreen.get(), 3, theRoot, 0, 0, 0, 0, 0);
    theRoot->AddChild(theScreen.get());
    EELobbyShow(true);
    LobbyStart();
    Glue(GL::RunLoop);
}

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

void EELobbyShowGameScreen()
{
    if (gUIManager != NULL) {
        EELobbyGameDoneEvent* theEvent = new EELobbyGameDoneEvent;
        gUIManager->GetFormByID(30)->SetFormOpenEvent(theEvent);
        theEvent->Dereference();
        gUIManager->Push(gUIManager->GetFormByID(30));
    }
}
}
#endif
