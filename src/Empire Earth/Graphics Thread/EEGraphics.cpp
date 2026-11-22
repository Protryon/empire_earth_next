// C:\EEX\Empire Earth\Graphics Thread\EEGraphics.cpp (__FILE__ 0x4be0bc). The game's graphics thread object (RTTI
// EEGraphics > RTSGraphics, vtable 0x4412c0): 0x10972b..0x10a9b0 (two file-scope objects' init chains, the
// second's implicit dtor, the members in vtable order, the message posts, RTSMouse's inline member and the second
// object's ctor; EEMouse.cpp's ctor starts at 0x10a9b0). Unit flags: the exe's module set.
#include <windows.h>
#include "rust/game/gamecpp.h"   // the bodies (ee_game gamecpp)
#include <deque>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/GESurface.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEUIManager.h"
#include "ee/EmpireEarth.h"
#include "ee/EEMouse.h"
#include "ee/RTSMousePointer.h"
#include "ee/EEGraphics.h"
#include "ee/EERenderSnapshot.h"
#include "rust/render/gfx.h"
// chain at 0x50972b (wrapper, no dtor): an object whose inline ctor clears one bool (provisional type and name)
struct EEGraphicsU8E8EB4 {
    EEGraphicsU8E8EB4() : mB00(false) {}
    bool mB00;
};
EEGraphicsU8E8EB4 gU8E8EB4;

// chain at 0x509738: three deques (ICF-shared ~deque 0x2003a4) and a bool; implicit dtor 0x509762; nothing else
// reads it (provisional type and names; the deque element types are unknown: any POD gives the same code)
struct EEGraphicsQueues {
    EEGraphicsQueues() : mB40(false) {}
    std::deque<long> mQ00;
    std::deque<long> mQ14;
    std::deque<long> mQ28;
    long mL3C;
    bool mB40;
};
EEGraphicsQueues gGraphicsQueues;

// Creates the game's graphics thread object for a screen (full screen as the screen is), with the frame lock
// mode 0 and the message-queue-done event
EEGraphics::EEGraphics(GERasterizer* inRasterizer, unsigned long inScreen, RTSMouse* inMouse, UIManager* inUIManager,
                       long inThreadPriority)
    : RTSGraphics(inRasterizer, inScreen, inMouse, inUIManager,
                  inRasterizer->GetScreen(inScreen)->GetIsFullScreen(), inThreadPriority)
{
    this->mLockMode = 0;
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mMessageQueueDone) == keseOK, 78);
    ee_gfx_main_viewport_set(reinterpret_cast<EeGeViewport*>(&gViewport));   // port: the Rust readers' (input_views.rs)
}

// Destroys the graphics object, releasing the scene and the message-queue-done event
EEGraphics::~EEGraphics()
{
    ee_gfx_main_viewport_set(NULL);
    EEReleaseScene();
    TSScheduler::DestroyEvent(this->mMessageQueueDone);
}

// Starts a frame: redraws the whole UI when needed, then copies the frame data under the world lock (read
// lock and UI lock for modes 0/1, write lock for 2/3; modes 1/3 keep the lock until FrameDone)
void EEGraphics::FrameStarting()
{
    ee_gamecpp_gfx_frame_starting(this);
}

// Handles the game's graphics messages (fades, letterbox, scene init/release, UI redraw, screenshot, lock mode,
// queue-done signal); passes others to RTSGraphics
void EEGraphics::HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2)
{
    ee_gamecpp_gfx_handle_message(this, (uint32_t)inMessage, inParameter1, inParameter2);
}

// Restores lost surfaces, then hides the mouse pointer, redraws the UI (and posts two more redraws), reloads
// the mouse cursor and shows the pointer again if it was shown; returns the restore result
long EEGraphics::RestoreSurfaces()
{
    return ee_gamecpp_gfx_restore_surfaces(this);
}

// Starts the scene (RTSGraphics' behaviour)
void EEGraphics::SceneStarting()
{
    RTSGraphics::SceneStarting();
}

// Renders the scene (RTSGraphics' behaviour)
void EEGraphics::RenderScene()
{
    RTSGraphics::RenderScene();
}

// Finishes the frame and releases the lock kept since FrameStarting (lock modes 1 and 3)
void EEGraphics::FrameDone()
{
    ee_gamecpp_gfx_frame_done(this);
}

// Renders a capture into the first free file EE###.bmp (or .jpg), or EEPostGame-M-D-Y-###.jpg for the
// post-game map, then signals the capture done
void EEGraphics::SaveScreenShot(EEScreenCapture* inCapture)
{
    ee_gamecpp_gfx_screenshot(this, inCapture);
}

// Renders a capture: the current view, or the whole map top-down tile by tile into a new surface of the asked
// width (zooming out until a tile fits the window), restoring the camera afterwards
// RenderTo's constants: unit-local .rdata (0x4412ac.. before the vtable, 0x4412e8.. after it), not __real pool
// literals (provisional names)
static const float kCaptureLeft = 0.0f;     // 0x4412ac (the free camera's capture rectangle, scaled by 0.64)
static const float kCaptureRight = 1600.0f;
static const float kCaptureTop = 35.0f;
static const float kCaptureBottom = 1085.0f;
static const double kMinusOne = -1.0;
static const double kEpsilon = 0.001f;      // 0x4412f0 (a float value widened)
static const double kMinusHalf = -0.5;
static const double kHalf = 0.5;
static const double kTileFactor = 0.70710677f;   // 0x441308 (a float value widened)
static const double kOne = 1.0;
static const float kCaptureScale = 0.64f;
void EEGraphics::RenderTo(EEScreenCapture* inCapture)
{
    EEMap* theMap = gWorld.mMap;
    GESurfaceDescription theDescription;
    GEViewport theViewport;
    GE3DPlane thePlane1;
    GE3DPlane theFrontPlane;
    GE3DPlane theLeftPlane;
    GE3DPlane thePlane4;
    GETransformation theTransformation;
    GEOrientation theOrientation;
    GERectangle theDestination;
    GERectangle theSource;
    GE3DPoint theSavedTarget;
    GE3DLine theCorner;
    GE3DLine theLine2;
    GE3DLine theLine;
    GE3DPoint theLocation;
    GE3DPoint theColumnStep;
    GE3DPoint theRowStep;
    GE3DPoint theSavedLocation;
    GERectangle theSavedView;
    float theZoom = gWorld.mCameraControl.mZoom;
    unsigned long theScreen = gScreenIndex;
    bool theSavedB5308E2 = gOrthographicView;
    long theRow = 0;
    long theColumn = 0;
    bool theSavedB529065 = gB529065;
    GERectangle theSavedOrthographic;
    unsigned long theWidth;
    unsigned long theHeight;
    float theT;

    gWorld.mLock->ReadLock();
    gB529065 = true;
    if (inCapture->mWidth == -1) {
        this->Render(false, false);
        inCapture->mSurface = gRasterizer->GetRenderTarget(theScreen);
    } else {
        gWorld.mCameraControl.GetLocation(gWorld.mCameraControl.mFraction, theSavedLocation);
        gWorld.mCameraControl.GetLocation(gWorld.mCameraControl.mFraction, theSavedTarget);
        theSavedView = gViewport.GetViewRectangle();
        theSavedOrthographic = gViewport.GetOrthographicRectangle();
        if (!gFreeCamera) {
            theWidth = gGameWindowWidth;
            theHeight = gGameWindowHeight;
        } else {
            theWidth = 1024;
            theHeight = 768;
            theSource.SetLeft((float)(kCaptureScale * kCaptureLeft));
            theSource.SetRight(kCaptureScale * kCaptureRight);
            theSource.SetTop(kCaptureScale * kCaptureTop);
            theSource.SetBottom(kCaptureScale * kCaptureBottom);
            gViewport.SetViewRectangle(theSource);
        }
        gOrthographicView = true;
        gRasterizer->SetProjectionMode(0, theScreen);
        gWorld.mCameraControl.SetZoom(1.0f);
        gWorld.mCameraControl.SetChanged();
        gWorld.mCameraControl.Update(0.0f);
        theTransformation = gWorld.mTransformation;
        theTransformation.ConvertToOrientation(theOrientation);
        double theCos = UMCos(1.5707964f - theOrientation.GetPitch() * -1.0f);
        double theAspect = (double)theMap->mL195610 / theMap->mL195614;
        if (theAspect > kOne)
            inCapture->mWidth = (long)(inCapture->mWidth / (theAspect * theCos));
        double theViewWidth = gViewport.GetOrthographicRectangle().GetWidth();
        double theViewHeight = gViewport.GetOrthographicRectangle().GetHeight();
        double theMapHeight = theMap->mL195614;
        double theRatio = inCapture->mWidth / theMapHeight;
        double theTileWidth = theRatio * theViewWidth * kTileFactor;
        double theTileHeight = theTileWidth * theCos;
        while (theTileWidth > theWidth || theHeight < theTileHeight) {
            if (gWorld.mCameraControl.mZoom <= 0.0f) {
                inCapture->mWidth /= 2;
                theRatio = inCapture->mWidth / theMapHeight;
                gWorld.mCameraControl.SetZoom(1.0f);
            } else
                gWorld.mCameraControl.SetZoom(gWorld.mCameraControl.mZoom - 0.1f);
            gWorld.mCameraControl.SetChanged();
            gWorld.mCameraControl.Update(0.0f);
            theViewWidth = gViewport.GetOrthographicRectangle().GetWidth();
            theViewHeight = gViewport.GetOrthographicRectangle().GetHeight();
            theTileWidth = theRatio * theViewWidth * kTileFactor;
            theTileHeight = theTileWidth * theCos;
        }

        theDescription.mWidth = inCapture->mWidth;
        theDescription.mHeight = (unsigned long)(inCapture->mWidth * theAspect * theCos);
        theDescription.mType = kSurfaceSystemMemory;
        theDescription.mPixelFormat = gRasterizer->GetRenderTarget(theScreen)->GetPixelFormat();
        theDescription.mPixelDepth = gRasterizer->GetRenderTarget(theScreen)->GetPixelDepth();
        theDescription.mSurfaceVolatility = 0;
        theDescription.m14 = 0;
        theDescription.mMipLevelCount = 0;
        theDescription.mIs3DRenderable = false;
        theDescription.mIsTransparencyEnabled = false;
        long theResult = gRasterizer->GetScreen(theScreen)->CreateSurface(theDescription, &inCapture->mSurface);
        ESVERIFY_AT(theResult == keseOK, 810);

        double theFractionX = theDescription.mWidth / theTileWidth - (long)(theDescription.mWidth / theTileWidth);
        double theFractionY = theDescription.mHeight / theTileHeight - (long)(theDescription.mHeight / theTileHeight);
        float theTileHeightF = theTileHeight;
        theSource.SetBottom(theTileHeightF);
        theSource.SetTop(0.0f);
        float theTileWidthF = theTileWidth;
        theSource.SetRight(theTileWidthF);
        theSource.SetLeft(0.0f);
        gViewport.SetViewRectangle(theSource);
        theDestination.SetTop(0.0f);
        theDestination.SetBottom(theTileHeightF);
        theDestination.SetLeft(0.0f);
        theDestination.SetRight(theTileWidthF);

        const GE3DPlane& theMapFront = theMap->mTerrainMesh.GetFrontPlane();
        theFrontPlane.Define(theMapFront.GetPoint(), theMapFront.GetNormal());
        const GE3DPlane& theMapLeft = theMap->mTerrainMesh.GetLeftPlane();
        theLeftPlane.Define(theMapLeft.GetPoint(), theMapLeft.GetNormal());
        theLeftPlane.Intersects(theFrontPlane, theCorner);
        theLocation.SetX(theCorner.GetStart().GetX());
        theLocation.SetY(theCorner.GetStart().GetY());
        theLocation.SetZ(0.0f);
        gWorld.mCameraControl.SetLocation(theLocation, false, true);
        gWorld.mCameraControl.SetChanged();
        gWorld.mCameraControl.Update(0.0f);

        theTransformation = gWorld.mTransformation;
        theRowStep.mX = theViewWidth * kHalf;
        theRowStep.mZ = gViewport.GetYClippingPlane() * theViewHeight * kMinusHalf;
        theTransformation.Apply(theRowStep, theLocation);
        theLine.SetStart(gWorld.mLine.GetStart());
        theLine.SetDirection(gWorld.mLine.GetDirection());
        theLine.SetStart(theLocation);
        theLine.SolveForZ(0.0f, theT, theLocation);
        gWorld.mCameraControl.SetLocation(theLocation, false, true);
        gWorld.mCameraControl.SetChanged();
        gWorld.mCameraControl.Update(0.0f);

        long theColumns = (float)UMCeil(theDescription.mWidth / theTileWidthF);
        long theRows = (float)UMCeil(theDescription.mHeight / theTileHeightF);
        theColumnStep.SetX(theViewWidth);
        do {
            do {
                gB529066 = false;
                gCameraFollowing = false;
                gB529068 = false;
                this->Render(false, false);
                if (theDestination.GetBottom() > (float)theDescription.mHeight)
                    theDestination.SetBottom((float)theDescription.mHeight);
                if (theDestination.GetRight() > (float)theDescription.mWidth)
                    theDestination.SetRight((float)theDescription.mWidth);
                gRasterizer->GetRenderTarget(theScreen)->CopyTo(inCapture->mSurface, theSource, theDestination, false);
                theColumn++;
                theDestination.SetLeft((float)(theColumn * theTileWidth));
                theDestination.SetRight((float)((theColumn + 1) * theTileWidth));
                theTransformation = gWorld.mTransformation;
                theTransformation.Apply(theColumnStep, theLocation);
                theLine = gWorld.mLine;
                theLine.SetStart(theLocation);
                theLine.SolveForZ(0.0f, theT, theLocation);
                gWorld.mCameraControl.SetLocation(theLocation, false, true);
                gWorld.mCameraControl.SetChanged();
                gWorld.mCameraControl.Update(0.0f);
                if (theColumn + 1 == theColumns && theFractionX > kEpsilon)
                    theSource.SetRight((float)(theFractionX * theTileWidth));
            } while (theColumn < theColumns);
            theRow++;
            theDestination.SetTop((float)(theRow * theTileHeight));
            theDestination.SetBottom((float)((theRow + 1) * theTileHeight));
            theColumn = 0;
            theDestination.SetRight(theTileWidthF);
            theDestination.SetLeft(0.0f);
            theSource.SetRight(theTileWidthF);
            theSource.SetLeft(0.0f);
            if (theRow + 1 == theRows && theFractionY > kEpsilon)
                theSource.SetBottom((float)(theFractionY * theTileHeight));
            theLine = gWorld.mLine;
            theLine.SetStart(theCorner.GetStart());
            theLine.SolveForZ(0.0f, theT, theLocation);
            gWorld.mCameraControl.SetLocation(theLocation, false, true);
            gWorld.mCameraControl.SetChanged();
            gWorld.mCameraControl.Update(0.0f);
            theRowStep.mZ = gViewport.GetYClippingPlane() * (theRow + kHalf) * theViewHeight * kMinusOne;
            theTransformation = gWorld.mTransformation;
            theTransformation.Apply(theRowStep, theLocation);
            theLine = gWorld.mLine;
            theLine.SetStart(theLocation);
            theLine.SolveForZ(0.0f, theT, theLocation);
            gWorld.mCameraControl.SetLocation(theLocation, false, true);
            gWorld.mCameraControl.SetChanged();
            gWorld.mCameraControl.Update(0.0f);
        } while (theRow < theRows);

        gViewport.SetViewRectangle(theSavedView);
        gViewport.SetOrthographicRectangle(theSavedOrthographic);
        gOrthographicView = theSavedB5308E2;
        gRasterizer->SetProjectionMode(1, theScreen);
        gWorld.mCameraControl.SetZoom(theZoom);
        gWorld.mCameraControl.SetLocation(theSavedLocation, false, true);
        gWorld.mCameraControl.SetChanged();
        gWorld.mCameraControl.Update(0.0f);
    }
    gB529066 = false;
    gCameraFollowing = false;
    gB529068 = false;
    gB529065 = theSavedB529065;
    gWorld.mLock->ReadUnlock();
}

// Posts the initialise-scene message (0x503) to the graphics thread
void EEGraphics::PostInitialiseScene()
{
    this->mThread->Post(0x503, NULL, NULL, true);
}

// Posts the redraw-UI message (0x50b) to the graphics thread
void EEGraphics::PostRedrawUI()
{
    this->mThread->Post(0x50b, NULL, NULL, true);
}

// Posts a screenshot request (0x505) for a capture to the graphics thread
void EEGraphics::PostScreenShot(EEScreenCapture* inCapture)
{
    this->mThread->Post(0x505, inCapture, NULL, true);
}

// Posts the release-scene message (0x504) to the graphics thread
void EEGraphics::PostReleaseScene()
{
    this->mThread->Post(0x504, NULL, NULL, true);
}

// Posts a light fade-in (0x506) of the given duration to the graphics thread
void EEGraphics::PostFadeIn(unsigned long inValue)
{
    this->mThread->Post(0x506, reinterpret_cast<void*>(inValue), NULL, true);
}

// Posts a light fade-out (0x507) of the given duration to the graphics thread
void EEGraphics::PostFadeOut(unsigned long inValue)
{
    this->mThread->Post(0x507, reinterpret_cast<void*>(inValue), NULL, true);
}

// Posts a letterbox-in (0x508) of the given duration to the graphics thread
void EEGraphics::PostLetterboxIn(unsigned long inValue)
{
    this->mThread->Post(0x508, reinterpret_cast<void*>(inValue), NULL, true);
}

// Posts a letterbox-out (0x509) of the given duration to the graphics thread
void EEGraphics::PostLetterboxOut(unsigned long inValue)
{
    this->mThread->Post(0x509, reinterpret_cast<void*>(inValue), NULL, true);
}

// Posts the queue-done message (0x50a) and waits until the graphics thread has handled every earlier
// message; returns the post result
long EEGraphics::PostMessage50AAndWait()
{
    long theResult = this->mThread->Post(0x50a, NULL, NULL, true);
    if (theResult == 0)
        this->mMessageQueueDone->Wait(kTSWaitInfinite);
    return theResult;
}

// ------------------------------------------------------------------------- the graphics bodies' C++ (gamecpp graphics.rs)


int64_t EEGraphicsOpNext(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

#define P64(x) ((int64_t)(intptr_t)(x))
#define GFX ((EEGraphics*)p)
#define CAPTURE ((EEScreenCapture*)p)

// The graphics thread object's C++ calls and members (EeGameCpp's graphics section; Empire Earth.cpp's EEStartupOp
// default): one statement each
int64_t EEGraphicsOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::GfxLockMode: return P64(&GFX->mLockMode);
    case O::GfxQueueDoneSet: GFX->mMessageQueueDone->Set();
        break;
    case O::GfxPostRedrawUI: GFX->PostRedrawUI();
        break;
    case O::GfxRenderTo: GFX->RenderTo((EEScreenCapture*)a[0]);
        break;
    case O::RtsGfxFrameStarting: GFX->RTSGraphics::FrameStarting();
        break;
    case O::RtsGfxFrameDone: GFX->RTSGraphics::FrameDone();
        break;
    case O::RtsGfxHandleMessage: GFX->RTSGraphics::HandleMessage((unsigned long)a[0], (void*)a[1], (void*)a[2]);
        break;
    case O::RtsGfxRestoreSurfaces: return GFX->RTSGraphics::RestoreSurfaces();
    case O::UiCurrentFormPresent: return gUIManager->GetCurrentForm() != NULL;
    case O::UiRedrawAll: gUIManager->RedrawAll();
        break;
    case O::UiLockForCopy: gUIManager->LockUIForCopyData();
        break;
    case O::UiUnlockForCopy: gUIManager->UnlockUIForCopyData();
        break;
    case O::AntiAliasing: return gAntiAliasing;
    case O::RastDirtyRectangleSupport: return gRasterizer->GetDirtyRectangleSupport(gScreenIndex);
    case O::WorldReadLock: gWorld.mLock->ReadLock();
        break;
    case O::WorldReadUnlock: gWorld.mLock->ReadUnlock();
        break;
    case O::WorldWriteLock: gWorld.mLock->WriteLock();
        break;
    case O::WorldWriteUnlock: gWorld.mLock->WriteUnlock();
        break;
    case O::SceneFlags6668Clear: gB529066 = false, gB529068 = false;
        break;
    case O::TakeRenderSnapshot: EETakeRenderSnapshot();
        break;
    case O::LightFadeIn: EEStartLightFadeIn((unsigned long)a[0]);
        break;
    case O::LightFadeOut: EEStartLightFadeOut((unsigned long)a[0]);
        break;
    case O::LetterboxIn: EEStartLetterboxIn((unsigned long)a[0]);
        break;
    case O::LetterboxOut: EEStartLetterboxOut((unsigned long)a[0]);
        break;
    case O::InitialiseScene: EEInitialiseScene();
        break;
    case O::MouseBeginFrame: gMouse->BeginFrame();
        break;
    case O::MouseLoadScreenIndex: gMouse->Load(gRasterizer->GetScreen(gScreenIndex));
        break;
    case O::MouseEndFrame: gMouse->EndFrame();
        break;
    case O::TimeOfDay: TMGetTimeOfDay(*(TMTime*)a[0]);
        break;
    case O::TakeJpgScreenshots: return gTakeJPGScreenshots;
    case O::CaptureIsPostGame: return CAPTURE->mIsPostGame;
    case O::CaptureSave: CAPTURE->mSurface->Save(*(FSFileSpec*)a[0], (long)a[1]);
        break;
    case O::CaptureDoneSet: CAPTURE->mCaptureComplete->Set();
        break;
    case O::FileSpecSetFileC: ((FSFileSpec*)p)->SetFile((const char*)a[0]);
        break;
    case O::FileSpecCreate: return ((FSFileSpec*)p)->Create();
    default: return EEGraphicsOpNext(inOp, p, a, f);
    }
    return 0;
}

// round 9's cases (EERenderObject.cpp's EECore9OpA, then B..E)
int64_t EECore9OpA(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
int64_t EEGraphicsOpNext(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    return EECore9OpA(inOp, p, a, f);
}
