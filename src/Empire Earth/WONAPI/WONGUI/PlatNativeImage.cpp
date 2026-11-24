// The WONAPI GUI's images on the platform layer (WONLobbyPlatform.h): the images' host, icons, cursors and
// DisplayContext::CreateImage. Replaces the GDI backend's MSNativeImage.cpp, MSRawImage.cpp and DisplayContext.cpp.
// R6 WONLobby round 24: the images themselves (the native images in software surfaces, their masks, the raw images'
// conversions, the texture pack's copies) are ee_won_gui's imgobj.rs; this is what they ask of the C++
// (WONImageHost: references, the handles, the surfaces, display contexts and Graphics, the draw plans' run).
#include <windows.h>
#include <string>
#include <string.h>
#include <list>
#include <vector>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPlatform.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyImageDecoder.h"
#include "ee/wonapi/WONLobbyStretchImage.h"
#include "ee/wonapi/WONGUIDraw.h"
#include "lle/GETextureManager.h"
#include "rust/won_gui/platcursor.h"

namespace WONAPI {

PlatNativeIcon::~PlatNativeIcon() {}

void WONImageThrow(int inStatus) { FileReaderCheck(inStatus); }

// A blank (black) image of the given size at the lobby surface's pixel scale (the windows' offscreen images and
// what the GUI composes for them)
static NativeImage* NewOffscreen(int inWidth, int inHeight)
{
    WindowManager* theManager = WindowManager::GetDefaultWindowManager();
    return new NativeImage(NULL, inWidth, inHeight, theManager != NULL && theManager->mRust != NULL ? theManager->GetPixelScale() : 1.0f);
}

SmartPtr<NativeImage> DisplayContext::CreateImage(int width, int height) { return NewOffscreen(width, height); }

static intptr_t Held(RefCount* inObject)
{
    if (inObject != NULL)
        inObject->AddRef();
    return (intptr_t)inObject;
}

intptr_t WONImageHost(EeWonImageHost op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonImageHost H;
    DisplayContext* theContext = (DisplayContext*)a;
    Graphics* theGraphics = (Graphics*)a;
    switch (op) {
        case H::Ref: ((RefCount*)a)->AddRef(); return 0;
        case H::Unref: ((RefCount*)a)->Release(); return 0;
        case H::Wrap: {
            EeWonImage* theRust = (EeWonImage*)a;
            Image* theImage;
            switch (ee_won_image_kind(theRust)) {
                case EeWonImageKind::Raw32: theImage = new RawImage32(theRust); break;
                case EeWonImageKind::Raw8: theImage = new RawImage8(theRust); break;
                case EeWonImageKind::Raw1: theImage = new RawImage(theRust); break;
                case EeWonImageKind::Native: theImage = new NativeImage(theRust); break;
                case EeWonImageKind::Stretch: theImage = new StretchImage(theRust); break;
                case EeWonImageKind::StretchCenter: theImage = new StretchCenterImage(theRust); break;
                default: theImage = new Image(theRust); break;
            }
            return (intptr_t)theImage;
        }
        case H::RustOf: return (intptr_t)((Image*)a)->mRust;
        case H::CtxGraphics: return theContext != NULL ? (intptr_t)theContext->mGraphics : 0;
        case H::CtxSurface:
            return theContext != NULL && theContext->mSurface != NULL ? (intptr_t)theContext->mSurface->GetRust() : 0;
        case H::CtxScale: { uint32_t s; memcpy(&s, &theContext->mPixelScale, 4); return (intptr_t)s; }
        case H::GfxTrans: ((int*)b)[0] = theGraphics->mTransX; ((int*)b)[1] = theGraphics->mTransY; return 0;
        case H::GfxContext: return (intptr_t)theGraphics->mDisplayContext.get();
        case H::GfxRun: {
            WONDrawSlots theSlots;
            for (int i = 0; i < 4; i++)
                theSlots.mImages[i] = ((Image**)d)[i];
            WONDrawNoCall theNoCall;
            for (intptr_t i = 0; i < c; i++)
                WONDrawOne(*theGraphics, ((const EeWonDrawOp*)b)[i], theSlots, theNoCall);
            return 0;
        }
        case H::GfxDrawImage: theGraphics->DrawImage((Image*)b, ((int*)c)[0], ((int*)c)[1]); return 0;
        case H::NewSurface: {
            // black (CreateCompatibleBitmap)
            SmartPtr<SurfaceBase> theSurface = SurfaceBase::GetNewSurface(SurfaceFormat_32);
            int theWidth = a < 0 ? 0 : (int)a, theHeight = b < 0 ? 0 : (int)b;
            theSurface->Create(theWidth, theHeight);
            if (theSurface->GetPixelsBase() != NULL)
                memset(theSurface->GetPixelsBase(), 0, (size_t)theSurface->GetBytesPerRow() * theHeight);
            return Held(theSurface.get());
        }
        case H::SurfaceRust: return (intptr_t)((SurfaceBase*)a)->GetRust();
        case H::NewContext: {
            DisplayContext* theNew = new DisplayContext((SurfaceBase*)a);
            uint32_t s = (uint32_t)b;
            memcpy(&theNew->mPixelScale, &s, 4);
            return Held(theNew);
        }
        case H::NewGraphics: {
            Graphics* theNew = new PlatGraphics;
            theNew->Attach(theContext);
            return (intptr_t)theNew;
        }
        case H::FreeGraphics: delete theGraphics; return 0;
        case H::CreateImage: return Held(NewOffscreen((int)b, (int)c));
        case H::Decode: {
            SmartPtr<NativeImage> theImage = WindowManager::GetDefaultWindowManager()->DecodeImage((const char*)a);
            return Held(theImage.get());
        }
        case H::PackShift:
            return WindowManager::GetDefaultWindowManager() != NULL ? (intptr_t)GETextureManager::GetTexturePackLogicalShift() : 0;
    }
    return 0;
}

const EeWonPlatCursorFns PlatCursor::kFns = {(void (*)(uint32_t))PlatCursorSetSystem, PlatCursorCreate, PlatCursorSet,
                                             PlatCursorDestroy};

} // namespace WONAPI
