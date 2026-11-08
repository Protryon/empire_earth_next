// C:\EEX\RTS Engine\Graphics\RTSGraphics.cpp (__FILE__ 0x4be990). The graphics thread's engine side (RTTI
// RTSGraphics, vtable 0x441c50): 0x11a1cc..0x11ab55 (the ctor to vector<RTSRenderObject*>::push_back; RTSMouse.cpp's
// key chain starts at 0x11ab55). Unit flags: the exe's module set.
#include <windows.h>

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
#include "lle/GEMovie.h"
#include "lle/GEPixelPack.h"
#include "lle/FSFileSpec.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/SEMixer.h"
#include "lle/Input System.h"
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

#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEGameSound.h"
#include "ee/EEMouse.h"
#include "ee/RTSGraphics.h"
#include "platform/PlatSim.h"

// FUNCTION: EXE 0x51a1cc
// Creates the graphics thread, paused, drawing a rasterizer screen with the mouse and UI, and waits until it runs
RTSGraphics::RTSGraphics(GERasterizer* inRasterizer, unsigned long inScreen, RTSMouse* inMouse,
                         UIManager* inUIManager, bool inIsFullScreen, long inThreadPriority)
{
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mRenderNow, false) == keseOK, 72);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mThreadCreated) == keseOK, 73);
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mThreadDone) == keseOK, 74);
    this->mRasterizer = inRasterizer;
    this->mSurface = inRasterizer->GetSecondarySurface(inScreen);
    this->mMouse = inMouse;
    this->mUIManager = inUIManager;
    this->mMovieDoneEvent = NULL;
    this->mScreen = inScreen;
    this->mMovie = NULL;
    this->mL50 = 0;
    this->mB54 = false;
    this->mIsMoviePlaying = false;
    this->mIsPaused = true;
    this->mIsQuitting = false;
    this->mIsPausePending = false;
    this->mIsResumePending = false;
    this->mIsWindowed = !inIsFullScreen;
    this->mIsSlowPlayback = false;
    this->mShowOverdraw = false;
    for (GESurface* theSurface = this->mSurface; theSurface != NULL; theSurface = theSurface->GetAttachedSurface(0)) {
        this->mU38 = theSurface;
        if (theSurface->GetAttachedSurfaceCount() == 0 || this->mU38->GetAttachedSurface(0)->GetType() == kSurfaceZBuffer) {
            break;
        }
    }

    ESVERIFY_AT(TSScheduler::CreateThread(this->mThread, RTSGraphics::Main, (void*)this) == keseOK, 111);
    ESVERIFY_AT(this->mThread->SetPriority(inThreadPriority) == keseOK, 112);
    this->mThreadCreated->Wait(kTSWaitInfinite);
    this->mThread->SetName(gGraphicsThreadName);
}

// SYNTHETIC: EXE 0x51a34c
// (the scalar deleting destructor)

// FUNCTION: EXE 0x51a368
// Destroys the graphics thread and its events, releasing any movie
RTSGraphics::~RTSGraphics()
{
    if (this->mMovie != NULL) {
        this->mMovie->Dereference();
        this->mMovie = NULL;
    }
    if (this->mLobbyFrame != NULL) {
        this->mLobbyFrame->Dereference();
        this->mLobbyFrame = NULL;
    }
    TSScheduler::DestroyEvent(this->mRenderNow);
    TSScheduler::DestroyEvent(this->mThreadCreated);
    TSScheduler::DestroyEvent(this->mThreadDone);
    TSScheduler::DestroyThread(this->mThread);
}

// FUNCTION: EXE 0x51a3af
// Tells every render object a frame is starting
void RTSGraphics::FrameStarting()
{
    for (unsigned long i = 0; i < this->mRenderObjects.size(); i++) {
        this->mRenderObjects[i]->FrameStarting();
    }
}

// FUNCTION: EXE 0x51a3d5
// Runs the graphics thread: handles posted messages and, unless paused, quitting or suspended, updates the mouse and
// renders a frame, every millisecond; signals when it ends
unsigned long RTSGraphics::Main(void* inGraphics)
{
    RTSGraphics* theGraphics = static_cast<RTSGraphics*>(inGraphics);
    unsigned long theMessage;
    void* theParameter1;
    void* theParameter2;
    theGraphics->mThreadCreated->Set();
    UMSetFloatingPointPrecision(kFPPrecision24);
    while (!theGraphics->mIsQuitting) {
        long theResult = theGraphics->mThread->Receive(theMessage, theParameter1, theParameter2);
        if (UMGetFloatingPointPrecision() != kFPPrecision24)
            UMSetFloatingPointPrecision(kFPPrecision24);
        if (UMGetFloatingPointRoundingMode() != 0)
            UMSetFloatingPointRoundingMode(0);
        if (theResult == kErrorOK)
            theGraphics->HandleMessage(theMessage, theParameter1, theParameter2);
        // the headless simulation (platform/PlatSim.h) draws nothing: the thread only answers its messages
        if (!gPlatSimMode && !EELobbyIsShown() && !theGraphics->mIsPaused && !theGraphics->mIsQuitting) {
            gMouse->mInputLock->Enter();
            gISMouse->RefreshXY();
            gMouse->Update();
            gMouse->mInputLock->Leave();
            theGraphics->Render(true, true);
        } else if (!gPlatSimMode && EELobbyIsShown() && !theGraphics->mIsQuitting) {
            theGraphics->RenderLobby();   // port: the lobby is drawn into the game's window
        }
        // Win32 Sleep, not PlatSleep: the test harness's time shim caps the frame rate at the Sleep import by the
        // caller's return address (inside RTSGraphics::Main; tools/harness/timeshim.c), which a wrapper would hide
#ifdef _WIN32
        Sleep(1);
#else
        PlatSleep(1);
#endif
    }
    theGraphics->mThreadDone->Set();
    return 0;
}

// FUNCTION: EXE 0x51a4c9
// Removes the last render object
void RTSGraphics::PopRenderObject()
{
    this->mRenderObjects.pop_back();
}

// FUNCTION: EXE 0x51a4d1
// Adds a render object, drawn after the others
void RTSGraphics::PushRenderObject(RTSRenderObject* inObject)
{
    this->mRenderObjects.push_back(inObject);
}

// FUNCTION: EXE 0x51a4e1
// Handles a graphics thread message: quit, pause/resume, the render object list, rasterizer and UI changes, vsync,
// movie start/stop (sound muted while it plays) and the debug display modes
void RTSGraphics::HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2)
{
    // NEAR (docs/REJECTED.md): the original pushes/stores the bool messages' parameter unnormalised (0x11a50f
    // `push [ebp+0xc]`, 0x11a6aa `mov [esi+0x10], bl`); `(bool)inParameter1` would be exact but is a re-typing read
    // (rejected by the owner, 2026-10-24). The real signature is still to be found.
    switch (inMessage) {
    case kThreadMessageQuit:
        this->mIsQuitting = true;
        break;
    case 0x40b:
        this->mRasterizer->SetWaitForVSync((bool)inParameter1, this->mScreen);
        break;
    case 0x402:
        this->mIsPausePending = false;
        this->mIsPaused = true;
        break;
    case 0x403:
        this->mIsResumePending = false;
        this->mIsPaused = false;
        break;
    case 0x404:
        this->PushRenderObject(static_cast<RTSRenderObject*>(inParameter1));
        break;
    case 0x405:
        this->PopRenderObject();
        break;
    case 0x406:
        this->mRenderObjects.clear();
        break;
    case 0x407:
        this->mRasterizer = static_cast<GERasterizer*>(inParameter1);
        this->mScreen = (unsigned long)reinterpret_cast<intptr_t>(inParameter2);
        break;
    case 0x409:
        this->mUIManager = static_cast<UIManager*>(inParameter1);
        break;
    case 0x408:
        this->mB54 = (bool)inParameter1;
        if (!this->mB54 && this->mUIManager != NULL && this->mUIManager->GetCurrentForm() != NULL) {
            this->mUIManager->RedrawAll();
        }
        break;
    case 0x40c: {
        FSFileSpec* theFile = static_cast<FSFileSpec*>(inParameter1);
        this->mMovie = this->mRasterizer->CreateMovie(*theFile, this->mScreen);
        this->mSavedConductorVolume = gConductor->GetConductor().GetVolume();
        this->mSavedMasterVolume = gSoundManager->GetMasterVolume();
        gConductor->GetConductor().SetVolume(0.0f);
        gSoundManager->SetMasterVolume(0.0f);
        if (theFile != NULL) {
            delete theFile;
        }
        break;
    }
    case 0x40d:
        if (this->mMovie != NULL) {
            this->mMovie->Stop();
            while (this->mMovie->GetIsPlaying()) {
#ifdef _WIN32
                Sleep(1);   // Win32 Sleep (see Main): HandleMessage may be inlined into Main
#else
                PlatSleep(1);
#endif
            }
            this->mIsMoviePlaying = false;
            this->mMovie->Dereference();
            this->mMovie = NULL;
        }
        gUserInterface->PostMovieDone(this->mMovieDoneEvent);
        // release the done event once and forget it (a stop after the movie had ended used to post and release
        // the already released event again)
        if (this->mMovieDoneEvent != NULL) {
            this->mMovieDoneEvent->Dereference();
            this->mMovieDoneEvent = NULL;
        }
        gConductor->GetConductor().SetVolume(this->mSavedConductorVolume);
        gSoundManager->SetMasterVolume(this->mSavedMasterVolume);
        break;
    case 0x40a:
        this->mIsSlowPlayback = (bool)inParameter1;
        break;
    case 0x40e:
        this->mRasterizer->SetDisplayOverdrawMode((bool)inParameter1, this->mScreen);
        this->mShowOverdraw = (bool)inParameter1;
        break;
    case 0x40f:
        this->mRasterizer->SetDisplayNoTextureMode((bool)inParameter1, this->mScreen);
        break;
    }
}

// FUNCTION: EXE 0x51a703
// Restores the screen's lost surfaces; returns the result
long RTSGraphics::RestoreSurfaces()
{
    return this->mRasterizer->GetScreen(this->mScreen)->RestoreAllSurfaces();
}

// FUNCTION: EXE 0x51a71c
// Draws one frame: the movie's frame while it plays (finishing it when it ends), else every render object's scene;
// then the mouse pointer and the flip as asked; restores lost surfaces on failure
void RTSGraphics::Render(bool inDrawMouse, bool inShowScene)
{
    GE2DPoint theOrigin(0.0f, 0.0f);
    long theResult;
    if (this->mMovie != NULL && ((this->mMovie->GetIsReady() && this->mMovie->GetIsPlaying()) || this->mMovie->GetJustStarted())) {
        this->mIsMoviePlaying = true;
        theResult = this->mMovie->RenderFrame();
    } else {
        if (this->mMovie != NULL && !this->mMovie->GetIsPlaying()) {
            this->mMovie->Dereference();
            this->mMovie = NULL;
            this->mIsMoviePlaying = false;
            gUserInterface->PostMovieDone(this->mMovieDoneEvent);
            // release the done event once and forget it (a later stop used to release it again)
            if (this->mMovieDoneEvent != NULL) {
                this->mMovieDoneEvent->Dereference();
                this->mMovieDoneEvent = NULL;
            }
            gConductor->GetConductor().SetVolume(this->mSavedConductorVolume);
            gSoundManager->SetMasterVolume(this->mSavedMasterVolume);
        }
        this->FrameStarting();
        this->SceneStarting();
        if (this->mShowOverdraw) {
            this->mRasterizer->Clear(GEColor(0.0f, 0.0f, 0.0f), this->mRasterizer->GetViewport(this->mScreen).GetViewRectangle(),
                               this->mScreen);
        }
        EELobbyLock();
        if (!EELobbyIsShown()) {
            theResult = this->mRasterizer->BeginScene(this->mSurface, this->mScreen);
            if (theResult == kErrorOK) {
                this->RenderScene();
                theResult = this->mRasterizer->EndScene(this->mScreen);
            }
        } else {
            theResult = 5;
        }
        EELobbyUnlock();
        this->FrameDone();
        this->mSurface->TextUnlock();
    }
    if (theResult == kErrorOK) {
        if (inDrawMouse) {
            this->mMouse->BeginFrame();
            this->mMouse->SaveUnder(this->mSurface, theOrigin);
            this->mMouse->Draw(this->mSurface, theOrigin);
        }
        if (inShowScene) {
            EELobbyLock();
            if (!EELobbyIsShown()) {
                this->mRasterizer->ShowScene(this->mScreen);
            }
            EELobbyUnlock();
        }
        if (inDrawMouse) {
            this->mMouse->RestoreUnder(this->mU38, theOrigin, this->mIsWindowed);
            this->mMouse->EndFrame();
        }
    } else if (theResult == kErrorFailed) {
        this->RestoreSurfaces();
    }
}

// port: the frame while the WON lobby is shown (it was a GDI window over the game's): packs a new lobby frame into a
// system-memory surface in the back buffer's format (as the movie player does), copies it letterboxed to the back
// buffer and shows it; an unchanged frame is shown again every 100 ms. The lobby is laid out for 1024x768 logical
// pixels and drawn at a pixel scale k: the letterboxed lobby's size in the back buffer's PIXELS (its logical size
// times its render scale) over 768, at least 1. The letterbox is the largest 4:3 rectangle of whole back-buffer
// (logical) pixels, centred on whole pixels, since a copy's rectangle is in whole logical pixels: at a whole render
// scale (1, 2) the frame's pixels land 1:1 on the back buffer's (crisp text and lines at any UI scale); at a
// fractional one they are within a pixel's phase. The frame's surface has the lobby's logical size
// (GESurface::SetLogicalSize) and the copy is in logical coordinates. k follows the back buffer (window size, UI
// scale, render resolution); until the lobby has repainted at a new k the frame is stretched. Lock order: EELobbyLock, then the lobby's frame lock.
void RTSGraphics::RenderLobby()
{
    GESurface* theTarget = this->mRasterizer->GetSecondarySurface(this->mScreen);
    if (theTarget == NULL)
        return;
    bool theChanged = false;
    EELobbyLock();
    if (theTarget->GetWidth() != this->mLobbySecondaryWidth || theTarget->GetHeight() != this->mLobbySecondaryHeight) {
        this->mLobbySecondaryWidth = theTarget->GetWidth();
        this->mLobbySecondaryHeight = theTarget->GetHeight();
        theChanged = true;
    }
    // the letterbox: the lobby's aspect (1024:768 = 4:3) in whole back-buffer pixels, centred
    float theRenderScale = theTarget->GetRenderScale() > 0.0f ? theTarget->GetRenderScale() : 1.0f;
    long theTargetWidth = (long)theTarget->GetWidth(), theTargetHeight = (long)theTarget->GetHeight();
    int theLobbyWidth, theLobbyHeight;
    EELobbyGetSurfaceSize(theLobbyWidth, theLobbyHeight);
    long theUnits = (std::min)(theTargetWidth / 4, theTargetHeight / 3);
    if (theUnits < 1)
        theUnits = 1;
    long theBoxWidth = theUnits * 4, theBoxHeight = theUnits * 3;
    long theBoxLeft = (theTargetWidth - theBoxWidth) / 2, theBoxTop = (theTargetHeight - theBoxHeight) / 2;
    float thePixelScale = (float)theBoxHeight * theRenderScale / (float)theLobbyHeight;
    thePixelScale = (std::max)(1.0f, (std::min)(thePixelScale, 4.0f));
    // every frame (a lobby opened again has a new manager); a scale it has already wakes nothing
    EELobbySetPresentation(thePixelScale, (float)theBoxLeft / theTargetWidth, (float)theBoxTop / theTargetHeight,
                           (float)theBoxWidth / theTargetWidth, (float)theBoxHeight / theTargetHeight);
    const uint32_t* thePixels;
    int theWidth, theHeight, thePitch, theLogicalWidth, theLogicalHeight;
    uint32_t theSerial;
    if (EELobbyLockFrame(thePixels, theWidth, theHeight, thePitch, theLogicalWidth, theLogicalHeight, theSerial)) {
        if (this->mLobbyFrame != NULL &&
            (this->mLobbyFrame->GetPixelWidth() != (unsigned long)theWidth ||
             this->mLobbyFrame->GetPixelHeight() != (unsigned long)theHeight ||
             this->mLobbyFrame->GetWidth() != (unsigned long)theLogicalWidth ||
             this->mLobbyFrame->GetHeight() != (unsigned long)theLogicalHeight ||
             this->mLobbyFrame->GetPixelFormat() != theTarget->GetPixelFormat())) {
            this->mLobbyFrame->Dereference();
            this->mLobbyFrame = NULL;
        }
        if (this->mLobbyFrame == NULL && theWidth > 0 && theHeight > 0 && theLogicalWidth > 0 && theLogicalHeight > 0) {
            GESurfaceDescription theDescription;
            theDescription.mType = kSurfaceSystemMemory;
            theDescription.mPixelFormat = theTarget->GetPixelFormat();
            theDescription.mPixelDepth = theTarget->GetPixelDepth();
            theDescription.mSurfaceVolatility = 0;
            theDescription.mWidth = (unsigned long)theWidth;
            theDescription.mHeight = (unsigned long)theHeight;
            theDescription.m14 = 0;
            theDescription.mMipLevelCount = 0;
            theDescription.mIs3DRenderable = false;
            theDescription.mIsTransparencyEnabled = false;
            if (this->mRasterizer->GetScreen(this->mScreen)->CreateSurface(theDescription, &this->mLobbyFrame) != kErrorOK)
                this->mLobbyFrame = NULL;
            else
                this->mLobbyFrame->SetLogicalSize((unsigned long)theLogicalWidth, (unsigned long)theLogicalHeight);
            theChanged = true;
        }
        if (this->mLobbyFrame != NULL && (theChanged || theSerial != this->mLobbySerial) &&
            this->mLobbyFrame->Lock() == kErrorOK) {
            unsigned char* theRow = this->mLobbyFrame->GetMemoryAddress();
            unsigned long theFormat = this->mLobbyFrame->GetPixelFormat();
            const unsigned char* theSource = reinterpret_cast<const unsigned char*>(thePixels);
            for (int y = 0; y < theHeight; y++, theRow += this->mLobbyFrame->GetBytesPerRow())
                GEPackBGRXRow(theSource + (size_t)y * thePitch, theRow, (unsigned long)theWidth, theFormat);
            this->mLobbyFrame->Unlock();
            this->mLobbySerial = theSerial;
            theChanged = true;
        }
        EELobbyUnlockFrame();
    }
    unsigned long theTime = TMGetTime();
    if (this->mLobbyFrame != NULL && (theChanged || theTime - this->mLobbyShownTime >= 100)) {
        float theFrameWidth = (float)this->mLobbyFrame->GetWidth(), theFrameHeight = (float)this->mLobbyFrame->GetHeight();
        GERectangle theSource(0.0f, 0.0f, theFrameWidth, theFrameHeight);
        // letterboxed (EELobbySetPresentation gave the lobby the same rectangle for its pointer mapping)
        GERectangle theDestination((float)theBoxLeft, (float)theBoxTop, (float)(theBoxLeft + theBoxWidth),
                                   (float)(theBoxTop + theBoxHeight));
        if (theBoxLeft > 0 || theBoxTop > 0)
            this->mRasterizer->Clear(GEColor(0.0f, 0.0f, 0.0f),
                                     GERectangle(0.0f, 0.0f, (float)theTargetWidth, (float)theTargetHeight), this->mScreen);
        long theResult = this->mLobbyFrame->CopyTo(theTarget, theSource, theDestination, false);
        if (theResult == kErrorOK)
            theResult = this->mRasterizer->ShowScene(this->mScreen);
        if (theResult == kErrorFailed)
            this->RestoreSurfaces();
        this->mLobbyShownTime = theTime;
    }
    EELobbyUnlock();
}

// FUNCTION: EXE 0x51a8fb
// Tells every render object a scene is starting
void RTSGraphics::SceneStarting()
{
    for (unsigned long i = 0; i < this->mRenderObjects.size(); i++) {
        this->mRenderObjects[i]->SceneStarting();
    }
}

// FUNCTION: EXE 0x51a922
// Resets the alpha test reference to 0 and has every render object draw its scene
void RTSGraphics::RenderScene()
{
    this->mRasterizer->SetAlphaTestingRefValue(0.0f, this->mScreen);
    for (unsigned long i = 0; i < this->mRenderObjects.size(); i++) {
        this->mRenderObjects[i]->RenderScene();
    }
}

// FUNCTION: EXE 0x51a95d
// Tells every render object the frame is done
void RTSGraphics::FrameDone()
{
    for (unsigned long i = 0; i < this->mRenderObjects.size(); i++) {
        this->mRenderObjects[i]->FrameDone();
    }
}

// FUNCTION: EXE 0x51a984
// Asks the graphics thread to remove every render object
void RTSGraphics::PostClearRenderObjects()
{
    this->mThread->Post(0x406, NULL, NULL, true);
}

// FUNCTION: EXE 0x51a999
// Asks the graphics thread to quit and waits until it has
void RTSGraphics::Shutdown()
{
    this->mThread->Post(kThreadMessageQuit, NULL, NULL, true);
    this->mThreadDone->Wait(kTSWaitInfinite);
}

// FUNCTION: EXE 0x51a9bd
// Asks the graphics thread to pause rendering (message 0x402) once a pending resume is handled; returns the post result
long RTSGraphics::PostPause()
{
    while (this->mIsResumePending) {
        PlatSleep(1);
    }
    this->mIsPausePending = true;
    return this->mThread->Post(0x402, NULL, NULL, true);
}

// FUNCTION: EXE 0x51a9ea
// Asks the graphics thread to play a movie file (a copy the thread deletes), keeping a reference to the done event
long RTSGraphics::PlayMovie(const FSFileSpec& inFile, UReferenceCounter* inMovieDoneEvent)
{
    FSFileSpec* theFile = new FSFileSpec(inFile);
    if (inMovieDoneEvent != NULL) {
        inMovieDoneEvent->Reference();
    }
    this->mMovieDoneEvent = inMovieDoneEvent;
    return this->mThread->Post(0x40c, theFile, NULL, true);
}

// FUNCTION: EXE 0x51aa38
// Asks the graphics thread to add a render object
void RTSGraphics::PostPushRenderObject(RTSRenderObject* inObject)
{
    this->mThread->Post(0x404, inObject, NULL, true);
}

// FUNCTION: EXE 0x51aa51
// Asks the graphics thread to set the slow-playback flag (message 0x40a)
void RTSGraphics::PostSetFlag5B(bool inFlag)
{
    this->mThread->Post(0x40a, (void*)inFlag, NULL, true);
}

// FUNCTION: EXE 0x51aa6c
// Asks the graphics thread to switch to another rasterizer and screen
void RTSGraphics::PostSetRasterizer(GERasterizer* inRasterizer, unsigned long inScreen)
{
    this->mThread->Post(0x407, inRasterizer, reinterpret_cast<void*>(inScreen), true);
}

// FUNCTION: EXE 0x51aa87
// Asks the graphics thread to set its flag +0x54 (message 0x408); clearing it redraws the whole UI
void RTSGraphics::PostSetFlag54(bool inFlag)
{
    this->mThread->Post(0x408, (void*)inFlag, NULL, true);
}

// FUNCTION: EXE 0x51aaa2
// Asks the graphics thread to resume rendering (message 0x403) once a pending pause is handled
void RTSGraphics::PostResume()
{
    while (this->mIsPausePending) {
        PlatSleep(1);
    }
    this->mIsResumePending = true;
    this->mThread->Post(0x403, NULL, NULL, true);
}

// FUNCTION: EXE 0x51aacf
// Asks the graphics thread to stop the movie, report it done and restore the sound volumes
void RTSGraphics::PostStopMovie()
{
    this->mThread->Post(0x40d, NULL, NULL, true);
}

// FUNCTION: EXE 0x51aae4
// Sets the surface to draw on and, for the mouse, the last surface of its attached chain before the z-buffer
void RTSGraphics::SetSurface(GESurface* inSurface)
{
    this->mSurface = inSurface;
    for (GESurface* theSurface = inSurface; theSurface != NULL; theSurface = theSurface->GetAttachedSurface(0)) {
        this->mU38 = theSurface;
        if (theSurface->GetAttachedSurfaceCount() == 0 || this->mU38->GetAttachedSurface(0)->GetType() == kSurfaceZBuffer) {
            break;
        }
    }
}

// TEMPLATE: EXE 0x51ab29
// std::vector<RTSRenderObject*>::push_back (first user 0x11a4d1)
