// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// BackgroundGrabber (R6 WON round 37): the Rust grabber (rust/crates/ee_won_gui/src/grabobj.rs) holds the captured image's
// place, its clip, the flags, the translucent colour and level and the capture's order and runs every method (the
// capture, the debug window, the tint, putting the background back); this is its host (the component, the Graphics,
// the background image, the debug window) and the constructor and destructor.
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPlatform.h" // CreateNativeWindow
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/won_gui/grabobj.h"
#include "rust/won_gui/guihost.h"

using namespace WONAPI;

typedef EeWonGrab Gb;
static_assert(ComponentFlag_DebugBG == EE_WON_GRAB_FLAG_DEBUG_BG && ComponentFlag_Translucent == EE_WON_GRAB_FLAG_TRANSLUCENT,
              "grabobj.rs flags");

// the grabber's calls out, each a run of the original's statements
extern "C" intptr_t ee_won_guicpp_grab_call(void* inMe, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    BackgroundGrabber* t = (BackgroundGrabber*)inMe;
    Component* comp = (Component*)b;
    Graphics* g = (Graphics*)b;
    if (op != EeWonGuiOp::Grab)
        return 0;
    switch ((Gb)a) {
    case Gb::LocalToRoot: comp->LocalToRoot(((int*)c)[0], ((int*)c)[1]); return 0;
    case Gb::DrawImage: g->DrawImage(t->mBGImage.get(), (int)c, (int)d); return 0;
    case Gb::Clip: {
        int* r = (int*)c;
        r[0] = g->mClipRect.x; r[1] = g->mClipRect.y; r[2] = g->mClipRect.width; r[3] = g->mClipRect.height;
        return 0;
    }
    case Gb::CompSize: ((int*)c)[0] = comp->Width(); ((int*)c)[1] = comp->Height(); return 0;
    case Gb::ImageSize:
        if (t->mBGImage.get() == NULL)
            return 0;
        ((int*)c)[0] = t->mBGImage->GetWidth(); ((int*)c)[1] = t->mBGImage->GetHeight();
        return 1;
    case Gb::NewImage: t->mBGImage = comp->GetRoot()->CreateImage((int)c, (int)d); return 0;
    case Gb::FlagSet: return comp->ComponentFlagSet((int)c) ? 1 : 0;
    case Gb::DebugExists: return t->mDebugWindow.get() != NULL ? 1 : 0;
    case Gb::DebugCreate: {
        t->mDebugWindow = CreateNativeWindow();
        t->mDebugWindow->SetParent(comp->GetRoot());
        t->mDebugWindow->SetStyle(0x40);
        WONRectangle aRect(0, 0, t->mBGImage->GetWidth(), t->mBGImage->GetHeight());
        t->mDebugWindow->Create(aRect);
        return 0;
    }
    case Gb::DebugShow: {
        t->mDebugWindow->mRoot.get()->RemoveAllChildren();
        t->mDebugWindow->AddComponent(new ImageComponent(t->mBGImage.get()));
        int x = 0, y = 0;
        comp->LocalToRoot(x, y);
        WONRectangle aScreenRect;
        comp->GetRoot()->GetRect(aScreenRect, false);
        t->mDebugWindow->SetPosSize(x + aScreenRect.x, y + aScreenRect.y, -1, -1);
        return 0;
    }
    case Gb::DebugInvalidate: t->mDebugWindow->mRoot.get()->Invalidate(); return 0;
    case Gb::Capture: {
        NativeImage* anImage = comp->GetRoot()->GetOffscreenImage();
        int x = 0, y = 0;
        comp->RootToChild(x, y);
        t->mBGImage->GetGraphics().DrawImage(anImage, x, y);
        return 0;
    }
    case Gb::BlendSupported: return g->BlendSupported() ? 1 : 0;
    case Gb::Blend: {
        const int* p = (const int*)d;
        g->SetColor((int)c);
        g->FillRectBlend(0, 0, p[0], p[1], p[2]);
        return 0;
    }
    case Gb::Tint: {
        SmartPtr<RawImage32> aRawImage = t->mBGImage->GetRaw();
        ee_won_grab_tint((uint32_t*)aRawImage->mImageData, (size_t)aRawImage->GetWidth() * aRawImage->GetHeight(), (int)c, (int)d);
        g->DrawImage(aRawImage.get(), 0, 0);
        return 0;
    }
    default: return 0;
    }
}

// Creates a grabber with no captured background and a half-strength translucent tint (the Rust grabber's state)
BackgroundGrabber::BackgroundGrabber()
{
    this->mRust = ee_won_grab_new(this, ee_won_guicpp_grab_call);
}

// Destroys the grabber: the Rust grabber goes, then the image and the debug window
BackgroundGrabber::~BackgroundGrabber()
{
    ee_won_grab_free(this->mRust);
}
