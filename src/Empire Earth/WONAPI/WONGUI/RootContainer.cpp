// RootContainer (R6 WON round 29): the Rust RootContainer's calls out (rust/crates/ee_won/src/rootobj.rs, EeWonGuiOp's
// root ops; the container's fall back to Container.cpp's), its constructor and destructor. The original's
// RootContainer.cpp (the invalid rectangle, the queued backgrounds, the cursor's component, the window drag) is the
// Rust object.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
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
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/ee_won.h"
using namespace WONAPI;

typedef EeWonGuiOp Op;

static Component* Comp(intptr_t a) { return (Component*)a; }

extern "C" intptr_t ee_won_guicpp_root_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    RootContainer* t = static_cast<RootContainer*>((Component*)inComp);
    switch (op) {
    case Op::RootCursorComponent:
        // (no cursor: no component; the original's MouseMove read through a NULL current cursor)
        if (a == 0) { *(int*)b = *(int*)c = 0; return 0; }
        return (intptr_t)((Cursor*)a)->GetComponent(*(int*)b, *(int*)c);
    case Op::RootOneComponentWindow: return t->mWindow->mIsOneComponentWindow ? 1 : 0;
    case Op::RootWindowMouse: *(int*)a = t->mWindow->mMouseX; *(int*)b = t->mWindow->mMouseY; return 0;
    case Op::RootWindowRect: t->mWindow->GetRect(*(WONRectangle*)a, false); return 0;
    case Op::RootWindowSetPos: t->mWindow->SetPosSize((int)a, (int)b, -1, -1); return 0;
    case Op::RootMouseChildOf: return (intptr_t)Comp(a)->GetMouseChild();
    case Op::RootScheme: return (intptr_t)t->GetColorScheme();
    case Op::RootGrabberRead: {
        BackgroundGrabber* g = (BackgroundGrabber*)a;
        EeWonRootGrab* q = (EeWonRootGrab*)b;
        q->need = g->NeedDrawBG() ? 1 : 0;
        q->order = (int)g->mImageOrder;
        q->x = g->mImageX;
        q->y = g->mImageY;
        q->clip.x = g->mClipRect.x; q->clip.y = g->mClipRect.y; q->clip.w = g->mClipRect.width; q->clip.h = g->mClipRect.height;
        q->image = (intptr_t)g->mBGImage.get();
        return 0;
    }
    case Op::RootGrabberClear: ((BackgroundGrabber*)a)->Clear(); return 0;
    case Op::RootGrabberDrop: ((BackgroundGrabber*)a)->Clear(); ((BackgroundGrabber*)a)->mBGImage = NULL; return 0;
    case Op::RootImageHold: ((NativeImage*)a)->AddRef(); return 0;
    case Op::RootImageRelease: ((NativeImage*)a)->Release(); return 0;
    case Op::RootImageSize: *(int*)b = ((NativeImage*)a)->mWidth; *(int*)c = ((NativeImage*)a)->mHeight; return 0;
    case Op::RootDrawImage: {
        const int* q = (const int*)c;
        ((Graphics*)a)->DrawImage((NativeImage*)b, q[0], q[1], q[2], q[3], q[4], q[5]);
        return 0;
    }
    case Op::RootEnsureOnTop: Comp(a)->EnsureOnTopOf(Comp(b)); return 0;
    case Op::ChildSetPos: Comp(a)->SetPos((int)b, (int)c); return 0;
    case Op::ChildSetVisible: Comp(a)->SetVisible(b != 0); return 0;
    default: return ContainerHostCall(t, op, a, b, c, d);
    }
}

// FUNCTION: WON 0x007040d0
// Creates the top-level container of a window: arrow cursor, window dragging allowed, tab focus kept inside (the Rust
// root: no invalid rectangle, no cursor component, no drag, no background)
RootContainer::RootContainer(Window *theWindow)
{
	this->mRootObj = ee_won_root_new(static_cast<Component*>(this), ee_won_guicpp_root_call, this->mCt);
	this->mWindow = theWindow;
	this->SetDefaultCursor(Cursor::GetStandardCursor(StandardCursor_Arrow));
	this->SetComponentFlags(0x8 | 0x40000,true);
	this->SetIsolateTabFocus(true);
}

// TEMPLATE: WON 0x007043c0
// Destroys the root: the Rust root gives the queued backgrounds' images back (the implicit destructor in the original)
RootContainer::~RootContainer() { ee_won_root_free(this->mRootObj); }
