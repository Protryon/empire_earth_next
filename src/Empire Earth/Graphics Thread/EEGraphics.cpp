// C:\EEX\Empire Earth\Graphics Thread\EEGraphics.cpp (__FILE__ 0x4be0bc). The game's graphics thread object (RTTI
// EEGraphics > RTSGraphics, vtable 0x4412c0): 0x10972b..0x10a9b0 (two file-scope objects' init chains, the
// second's implicit dtor, the members in vtable order, the message posts, RTSMouse's inline member and the second
// object's ctor; EEMouse.cpp's ctor starts at 0x10a9b0). Unit flags: the exe's module set.
#include <windows.h>
#include <deque>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GERasterizer.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEUIManager.h"
#include "ee/EmpireEarth.h"
#include "ee/EEMouse.h"
#include "ee/EEGraphics.h"
// the two file-scope objects of the chains at 0x50972b and 0x509738 (a bool; three deques and a bool): nothing read
// them, so the port has neither

// Creates the game's graphics thread object for a screen (full screen as the screen is), with the frame lock
// mode 0 and the message-queue-done event
EEGraphics::EEGraphics(GERasterizer* inRasterizer, unsigned long inScreen, RTSMouse* inMouse, UIManager* inUIManager,
                       long inThreadPriority)
    : RTSGraphics(inRasterizer, inScreen, inMouse, inUIManager,
                  inRasterizer->GetScreen(inScreen)->GetIsFullScreen(), inThreadPriority)
{
    ee_gamecpp_gfx_construct(this);
}

// ------------------------------------------------------------------------- the graphics bodies' C++ (gamecpp graphics.rs)



#define P64(x) ((int64_t)(intptr_t)(x))
#define GFX ((EEGraphics*)p)
#define CAPTURE ((EEScreenCapture*)p)
#define RU_TEXT ((GEText*)p)

// The graphics thread object's C++ calls and members (EeGameCpp's graphics section; Empire Earth.cpp's EEStartupOp
// default): one statement each
int64_t EECore9OpA(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
int64_t EEGraphicsOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::GfxLockMode: return P64(&GFX->mLockMode);
    case O::GfxQueueDoneSet: GFX->mMessageQueueDone->Set();
        break;
    case O::GfxPostRedrawUI: GFX->PostRedrawUI();
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
    case O::WorldReadLock: gWorld.mLock->ReadLock();
        break;
    case O::WorldReadUnlock: gWorld.mLock->ReadUnlock();
        break;
    case O::WorldWriteLock: gWorld.mLock->WriteLock();
        break;
    case O::WorldWriteUnlock: gWorld.mLock->WriteUnlock();
        break;
    case O::MouseBeginFrame: gMouse->BeginFrame();
        break;
    case O::MouseLoadScreenIndex: gMouse->Load(gRasterizer->GetScreen(gScreenIndex));
        break;
    case O::MouseEndFrame: gMouse->EndFrame();
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
    // round 10, lane A's: the graphics object (gamecpp render_to.rs)
    case O::GtQueueDoneCreate: return TSScheduler::CreateEvent(GFX->mMessageQueueDone) == keseOK;
    case O::GtQueueDoneDestroy: TSScheduler::DestroyEvent(GFX->mMessageQueueDone);
        break;
    case O::GtQueueDoneWait: GFX->mMessageQueueDone->Wait(kTSWaitInfinite);
        break;
    case O::GtPost: return GFX->mThread->Post((unsigned long)a[0], NULL, NULL, true);
    case O::GtMainViewport: return P64(&gViewport);
    case O::RtCaptureWidth: return P64(&CAPTURE->mWidth);
    case O::RtCaptureSurface: return P64(&CAPTURE->mSurface);
    case O::RtScreenIndex: return gScreenIndex;
    case O::RtRender: GFX->Render(false, false);
        break;
    case O::RtCaptureSetRenderTarget: CAPTURE->mSurface = gRasterizer->GetRenderTarget((unsigned long)a[0]);
        break;
    case O::RtViewportViewRectangle: *(GERectangle*)a[0] = gViewport.GetViewRectangle();
        break;
    case O::RtViewportOrthoRectangle: *(GERectangle*)a[0] = gViewport.GetOrthographicRectangle();
        break;
    case O::RtSetViewportOrthoRectangle: gViewport.SetOrthographicRectangle(*(const GERectangle*)a[0]);
        break;
    case O::RtSetOrthographic: gOrthographicView = a[0] != 0;
        break;
    case O::RtMapL195610: return gWorld.mMap->mL195610;
    case O::RtMapL195614: return gWorld.mMap->mL195614;
    case O::RtTargetPixelFormat: return gRasterizer->GetRenderTarget((unsigned long)a[0])->GetPixelFormat();
    case O::RtTargetPixelDepth: return gRasterizer->GetRenderTarget((unsigned long)a[0])->GetPixelDepth();
    case O::RtCreateSurface: return gRasterizer->GetScreen((unsigned long)a[2])->CreateSurface(*(GESurfaceDescription*)a[0], (GESurface**)a[1]);
    case O::RtMeshFrontPlane: *(GE3DPlane*)a[0] = gWorld.mMap->mTerrainMesh.GetFrontPlane();
        break;
    case O::RtMeshLeftPlane: *(GE3DPlane*)a[0] = gWorld.mMap->mTerrainMesh.GetLeftPlane();
        break;
    case O::RtWorldLine: *(GE3DLine*)a[0] = gWorld.mLine;
        break;
    case O::RtCopyTo: gRasterizer->GetRenderTarget((unsigned long)a[0])->CopyTo(CAPTURE->mSurface, *(GERectangle*)a[1], *(GERectangle*)a[2], false);
        break;
    default: return EECore9OpA(inOp, p, a, f);
    }
    return 0;
}

// ------------------------------------------------------------------------- round 10, lane A: the Graphics Thread (EEGraphics.cpp, the EERO* units, EEMouse, EEUnitPicker)

// the C++ calls and members of lane A's bodies in Rust (gamecpp; EeGameCpp's round 10 lane A section): one
// statement each
double EECore10OpDF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEGameUtilities.cpp
double EECore10OpAF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::RtViewportYClipping: return gViewport.GetYClippingPlane();
    case O::RuTextHeight: return RU_TEXT->GetTextExtent().GetHeight();
    case O::GsClassScale: return gUnitClassTable[a[0]]->GetScale();
    case O::SnCameraPitch: return gWorld.GetCameraPitch();
    default: return EECore10OpDF(inOp, p, a, f);
    }
}
