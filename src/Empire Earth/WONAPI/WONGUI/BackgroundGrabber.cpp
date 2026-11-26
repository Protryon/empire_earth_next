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
static_assert(offsetof(BackgroundGrabber, mBGImage) == 0 && sizeof(SmartPtr<NativeImage>) == sizeof(void*), "grabobj.rs reads the image");
static_assert(ComponentFlag_DebugBG == EE_WON_GRAB_FLAG_DEBUG_BG && ComponentFlag_Translucent == EE_WON_GRAB_FLAG_TRANSLUCENT,
              "grabobj.rs flags");

// the grabber's calls out, each one of the original's statements (R6 WON round 34: their runs are grabobj.rs's; the
// component's and the background image's are grabobj.rs's own in the game, R6 WON GUI round 39)
extern "C" intptr_t ee_won_guicpp_grab_call(void* inMe, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    BackgroundGrabber* t = (BackgroundGrabber*)inMe;
    Component* comp = (Component*)b;
    if (op != EeWonGuiOp::Grab)
        return 0;
    switch ((Gb)a) {
    case Gb::NewImage: t->mBGImage = comp->GetRoot()->CreateImage((int)c, (int)d); return 0;
    case Gb::DebugExists: return t->mDebugWindow.get() != NULL ? 1 : 0;
    case Gb::DebugNew: t->mDebugWindow = CreateNativeWindow(); return 0;
    case Gb::DebugParent: t->mDebugWindow->SetParent(comp->GetRoot()); return 0;
    case Gb::DebugStyle: t->mDebugWindow->SetStyle(0x40); return 0;
    case Gb::DebugCreateAt: t->mDebugWindow->Create(WONRectangle(0, 0, (int)c, (int)d)); return 0;
    case Gb::DebugRemoveAll: t->mDebugWindow->mRoot.get()->RemoveAllChildren(); return 0;
    case Gb::DebugAddImage: t->mDebugWindow->AddComponent(new ImageComponent(t->mBGImage.get())); return 0;
    case Gb::ScreenRect: comp->GetRoot()->GetRect(*(WONRectangle*)c, false); return 0;
    case Gb::DebugPos: t->mDebugWindow->SetPosSize((int)c, (int)d, -1, -1); return 0;
    case Gb::DebugInvalidate: t->mDebugWindow->mRoot.get()->Invalidate(); return 0;
    case Gb::Offscreen: return (intptr_t)comp->GetRoot()->GetOffscreenImage();
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
