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
#include "lle/GETextureManager.h"
#include "rust/won_gui/platcursor.h"

namespace WONAPI {

void WONImageThrow(int inStatus) { FileReaderCheck(inStatus); }

// A blank (black) image of the given size at the lobby surface's pixel scale (the windows' offscreen images and
// what the GUI composes for them)
static NativeImage* NewOffscreen(int inWidth, int inHeight)
{
    WindowManager* theManager = WindowManager::GetDefaultWindowManager();
    return new Image(ee_won_image_new_native(WONImageHost, inWidth, inHeight, theManager != NULL && theManager->mRust != NULL ? theManager->GetPixelScale() : 1.0f));
}

SmartPtr<NativeImage> DisplayContext::CreateImage(int width, int height) { return NewOffscreen(width, height); }

// imgobj.rs reads a handle's mRust and a context's members in place (R6 WON GUI round 38)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
static_assert(offsetof(Image, mRust) == EE_WON_IMAGE_RUST_AT && offsetof(DisplayContext, mSurface) == EE_WON_CONTEXT_AT &&
              offsetof(DisplayContext, mGraphics) == EE_WON_CONTEXT_AT + sizeof(void*) &&
              offsetof(DisplayContext, mPixelScale) == EE_WON_CONTEXT_AT + 2 * sizeof(void*),
              "imgobj.rs's EeWonDisplayContextRec");
#pragma clang diagnostic pop

static intptr_t Held(RefCount* inObject)
{
    if (inObject != NULL)
        inObject->AddRef();
    return (intptr_t)inObject;
}

intptr_t WONImageHost(EeWonImageHost op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonImageHost H;
    switch (op) {
        case H::Wrap: {
            EeWonImage* theRust = (EeWonImage*)a;
            Image* theImage;
            switch (ee_won_image_kind(theRust)) {
                case EeWonImageKind::Stretch: theImage = new StretchImage(theRust); break;
                case EeWonImageKind::StretchCenter: theImage = new StretchCenterImage(theRust); break;
                default: theImage = new Image(theRust); break;
            }
            return (intptr_t)theImage;
        }
        // (a Graphics' translation, context, plan and image draw, its making and freeing are gfxrun.rs's: R6 WON GUI
        // round 39)
        // (the surface is the Rust image's own: no SurfaceBase, R6 WON GUI round 36)
        case H::NewContext: return Held(new DisplayContext((EeWonSurface*)a, *(const float*)b));
        case H::CreateImage: return Held(NewOffscreen((int)b, (int)c));
        case H::Decode: {
            SmartPtr<NativeImage> theImage = WindowManager::GetDefaultWindowManager()->DecodeImage((const char*)a);
            return Held(theImage.get());
        }
        case H::PackShift:
            return WindowManager::GetDefaultWindowManager() != NULL ? (intptr_t)GETextureManager::GetTexturePackLogicalShift() : 0;
        default: break;
    }
    return 0;
}

} // namespace WONAPI
