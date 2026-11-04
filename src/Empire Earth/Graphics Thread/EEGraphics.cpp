// C:\EEX\Empire Earth\Graphics Thread\EEGraphics.cpp (__FILE__ 0x4be0bc). The game's graphics thread object (RTTI
// EEGraphics > RTSGraphics, vtable 0x4412c0): 0x10972b..0x10a9b0 (two file-scope objects' init chains, the
// second's implicit dtor, the members in vtable order, the message posts, RTSMouse's inline member and the second
// object's ctor; EEMouse.cpp's ctor starts at 0x10a9b0). Unit flags: the exe's module set.
#include <windows.h>
#include <deque>
#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEUIManager.h"
#include "ee/EmpireEarth.h"
#include "ee/EEMouse.h"
#include "ee/RTSMousePointer.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
// chain at 0x50972b (wrapper, no dtor): an object whose inline ctor clears one bool (provisional type and name)
struct EEGraphicsU8E8EB4 {
    EEGraphicsU8E8EB4() : mB00(false) {}
    bool mB00;
};
EEGraphicsU8E8EB4 gU8E8EB4;
// SYNTHETIC: EXE 0x50972b   (wrapper)
// SYNTHETIC: EXE 0x509730   (ctor)

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
// SYNTHETIC: EXE 0x509738   (wrapper)
// SYNTHETIC: EXE 0x509742   (ctor)
// SYNTHETIC: EXE 0x50974c   (registrar)
// SYNTHETIC: EXE 0x509758   (dtor)
// FUNCTION: EXE 0x509762

// FUNCTION: EXE 0x50977e
// Creates the game's graphics thread object for a screen (full screen as the screen is), with the frame lock
// mode 0 and the message-queue-done event
EEGraphics::EEGraphics(GERasterizer* inRasterizer, unsigned long inScreen, RTSMouse* inMouse, UIManager* inUIManager,
                       long inThreadPriority)
    : RTSGraphics(inRasterizer, inScreen, inMouse, inUIManager,
                  inRasterizer->GetScreen(inScreen)->GetIsFullScreen(), inThreadPriority)
{
    this->mLockMode = 0;
    ESVERIFY_AT(TSScheduler::CreateEvent(this->mMessageQueueDone) == keseOK, 78);
}

// SYNTHETIC: EXE 0x509803
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x50981f
// Destroys the graphics object, releasing the scene and the message-queue-done event
EEGraphics::~EEGraphics()
{
    EEReleaseScene();
    TSScheduler::DestroyEvent(this->mMessageQueueDone);
}

// FUNCTION: EXE 0x509840
// Starts a frame: redraws the whole UI when needed, then copies the frame data under the world lock (read
// lock and UI lock for modes 0/1, write lock for 2/3; modes 1/3 keep the lock until FrameDone)
void EEGraphics::FrameStarting()
{
    if (gUIManager != NULL && gUIManager->GetCurrentForm() != NULL) {
        if (gAntiAliasing)
            gUIManager->RedrawAll();
        else {
            unsigned long theScreen = gScreenIndex;
            if (!gRasterizer->GetDirtyRectangleSupport(theScreen))
                gUIManager->RedrawAll();
        }
    }
    if (this->mLockMode == 0 || this->mLockMode == 1) {
        gUIManager->LockUIForCopyData();
        gWorld.mLock->ReadLock();
    } else {
        gWorld.mLock->WriteLock();
    }
    gB529066 = false;
    gB529068 = false;
    RTSGraphics::FrameStarting();
    if (this->mLockMode == 0) {
        gWorld.mLock->ReadUnlock();
        gUIManager->UnlockUIForCopyData();
    } else if (this->mLockMode == 2) {
        gWorld.mLock->WriteUnlock();
    }
}

// FUNCTION: EXE 0x509902
// Handles the game's graphics messages (fades, letterbox, scene init/release, UI redraw, screenshot, lock mode,
// queue-done signal); passes others to RTSGraphics
void EEGraphics::HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2)
{
    switch (inMessage) {
    case 0x506:
        EEStartLightFadeIn((unsigned long)reinterpret_cast<intptr_t>(inParameter1));
        break;
    case 0x507:
        EEStartLightFadeOut((unsigned long)reinterpret_cast<intptr_t>(inParameter1));
        break;
    case 0x508:
        EEStartLetterboxIn((unsigned long)reinterpret_cast<intptr_t>(inParameter1));
        break;
    case 0x509:
        EEStartLetterboxOut((unsigned long)reinterpret_cast<intptr_t>(inParameter1));
        break;
    case 0x503:
        EEInitialiseScene();
        break;
    case 0x504:
        EEReleaseScene();
        break;
    case 0x50b:
        if (gUIManager != NULL) {
            gUIManager->LockUIForCopyData();
            gUIManager->RedrawAll();
            gUIManager->UnlockUIForCopyData();
        }
        break;
    case 0x505:
        this->SaveScreenShot(static_cast<EEScreenCapture*>(inParameter1));
        break;
    case 0x502:
        this->mLockMode = (long)reinterpret_cast<intptr_t>(inParameter1);
        break;
    case 0x50a:
        this->mMessageQueueDone->Set();
        break;
    case 0x501:
        break;
    default:
        RTSGraphics::HandleMessage(inMessage, inParameter1, inParameter2);
        break;
    }
}

// FUNCTION: EXE 0x5099dc
// Restores lost surfaces, then hides the mouse pointer, redraws the UI (and posts two more redraws), reloads
// the mouse cursor and shows the pointer again if it was shown; returns the restore result
long EEGraphics::RestoreSurfaces()
{
    bool theWasDrawing = gMousePointer->mIsShown;
    long theResult = RTSGraphics::RestoreSurfaces();
    if (theResult == 0) {
        gMousePointer->mThread->Post(0x402, NULL, NULL, true);
        while (gMousePointer->mIsShown)
            Sleep(1);
        while (gMousePointer->mIsShown)
            Sleep(1);
        gWorld.mLock->WriteLock();
        gUIManager->RedrawAll();
        this->PostRedrawUI();
        this->PostRedrawUI();
        gMouse->BeginFrame();
        gMouse->Load(gRasterizer->GetScreen(gScreenIndex));
        gMouse->EndFrame();
        gWorld.mLock->WriteUnlock();
        if (theWasDrawing)
            gMousePointer->mThread->Post(0x404, NULL, NULL, true);
    }
    return theResult;
}

// FUNCTION: EXE 0x509ada
// Starts the scene (RTSGraphics' behaviour)
void EEGraphics::SceneStarting()
{
    RTSGraphics::SceneStarting();
}

// FUNCTION: EXE 0x509adf
// Renders the scene (RTSGraphics' behaviour)
void EEGraphics::RenderScene()
{
    RTSGraphics::RenderScene();
}

// FUNCTION: EXE 0x509ae4
// Finishes the frame and releases the lock kept since FrameStarting (lock modes 1 and 3)
void EEGraphics::FrameDone()
{
    RTSGraphics::FrameDone();
    if (this->mLockMode == 1) {
        gWorld.mLock->ReadUnlock();
        gUIManager->UnlockUIForCopyData();
    } else if (this->mLockMode == 3) {
        gWorld.mLock->WriteUnlock();
    }
}

// FUNCTION: EXE 0x509b22
// Renders a capture into the first free file EE###.bmp (or .jpg), or EEPostGame-M-D-Y-###.jpg for the
// post-game map, then signals the capture done
void EEGraphics::SaveScreenShot(EEScreenCapture* inCapture)
{
    FSFileSpec theFile;
    UString theTime;
    UString theExtension;
    TMTime theNow;
    long theIndex = 0;
    TMGetTimeOfDay(theNow);
    if (gTakeJPGScreenshots || inCapture->mIsPostGame)
        sprintf(theExtension, ".%s", "jpg");
    else
        sprintf(theExtension, ".%s", "bmp");
    do {
        if (inCapture->mIsPostGame) {
            sprintf(theTime, "%u-%u-%u-%.3u", theNow.mMonth + 1, theNow.mDay, theNow.mYear, theIndex);
            theFile.SetFile(UString("EEPostGame-") + theTime + theExtension);
        } else {
            sprintf(theTime, "%.3u", theIndex);
            theFile.SetFile(UString("EE") + theTime + theExtension);
        }
        theIndex++;
    } while (theFile.Create() == kErrorAlreadyExists);
    this->RenderTo(inCapture);
    if (gTakeJPGScreenshots || inCapture->mIsPostGame)
        inCapture->mSurface->Save(theFile, kImageFileJPG);
    else
        inCapture->mSurface->Save(theFile, kImageFileBMP);
    inCapture->mCaptureComplete->Set();
}

// FUNCTION: EXE 0x509cd4
// Renders a capture: the current view, or the whole map top-down tile by tile into a new surface of the asked
// width (zooming out until a tile fits the window), restoring the camera afterwards
// RenderTo's constants: unit-local .rdata (0x4412ac.. before the vtable, 0x4412e8.. after it), not __real pool
// literals (provisional names)
static const float kCaptureLeft = 0.0f;     // 0x4412ac (the free camera's capture rectangle, scaled by 0.64)
static const float kCaptureRight = 1600.0f; // 0x4412b0
static const float kCaptureTop = 35.0f;     // 0x4412b4
static const float kCaptureBottom = 1085.0f;   // 0x4412b8
static const double kMinusOne = -1.0;       // 0x4412e8
static const double kEpsilon = 0.001f;      // 0x4412f0 (a float value widened)
static const double kMinusHalf = -0.5;      // 0x4412f8
static const double kHalf = 0.5;            // 0x441300
static const double kTileFactor = 0.70710677f;   // 0x441308 (a float value widened)
static const double kOne = 1.0;             // 0x441310
static const float kCaptureScale = 0.64f;   // 0x441318
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
        theRowStep.mZ = gViewportYClippingPlane * theViewHeight * kMinusHalf;
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
            theRowStep.mZ = gViewportYClippingPlane * (theRow + kHalf) * theViewHeight * kMinusOne;
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

// FUNCTION: EXE 0x50a862
// Posts the initialise-scene message (0x503) to the graphics thread
void EEGraphics::PostInitialiseScene()
{
    this->mThread->Post(0x503, NULL, NULL, true);
}

// FUNCTION: EXE 0x50a877
// Posts the redraw-UI message (0x50b) to the graphics thread
void EEGraphics::PostRedrawUI()
{
    this->mThread->Post(0x50b, NULL, NULL, true);
}

// FUNCTION: EXE 0x50a88c
// Posts a screenshot request (0x505) for a capture to the graphics thread
void EEGraphics::PostScreenShot(EEScreenCapture* inCapture)
{
    this->mThread->Post(0x505, inCapture, NULL, true);
}

// FUNCTION: EXE 0x50a8a5
// Posts the release-scene message (0x504) to the graphics thread
void EEGraphics::PostReleaseScene()
{
    this->mThread->Post(0x504, NULL, NULL, true);
}

// FUNCTION: EXE 0x50a8ba
// Posts a light fade-in (0x506) of the given duration to the graphics thread
void EEGraphics::PostFadeIn(unsigned long inValue)
{
    this->mThread->Post(0x506, reinterpret_cast<void*>(inValue), NULL, true);
}

// FUNCTION: EXE 0x50a8d3
// Posts a light fade-out (0x507) of the given duration to the graphics thread
void EEGraphics::PostFadeOut(unsigned long inValue)
{
    this->mThread->Post(0x507, reinterpret_cast<void*>(inValue), NULL, true);
}

// FUNCTION: EXE 0x50a8ec
// Posts a letterbox-in (0x508) of the given duration to the graphics thread
void EEGraphics::PostLetterboxIn(unsigned long inValue)
{
    this->mThread->Post(0x508, reinterpret_cast<void*>(inValue), NULL, true);
}

// FUNCTION: EXE 0x50a905
// Posts a letterbox-out (0x509) of the given duration to the graphics thread
void EEGraphics::PostLetterboxOut(unsigned long inValue)
{
    this->mThread->Post(0x509, reinterpret_cast<void*>(inValue), NULL, true);
}

// FUNCTION: EXE 0x50a91e
// Posts the queue-done message (0x50a) and waits until the graphics thread has handled every earlier
// message; returns the post result
long EEGraphics::PostMessage50AAndWait()
{
    long theResult = this->mThread->Post(0x50a, NULL, NULL, true);
    if (theResult == 0)
        this->mMessageQueueDone->Wait(INFINITE);
    return theResult;
}

// FUNCTION: EXE 0x50a94c
// Ends the mouse cursor's frame, releasing its surface and the render lock
void RTSMouse::EndFrame()
{
    this->mCurrentSurface->Dereference();
    this->mCurrentSurface = NULL;
    this->mInFrame = false;
    this->mRenderLock->Leave();
}

// FUNCTION: EXE 0x50a96e
// Creates the three empty queues of gGraphicsQueues with the flag cleared (EEGraphicsQueues' ctor)
