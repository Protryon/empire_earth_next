// Container (R6 WON round 26): the Rust Container's calls out (rust/crates/ee_won_gui/src/ctobj.rs, EeWonGuiOp's
// container ops), its constructor and destructor. The original's Container.cpp, ContainerChildLifecycle.cpp,
// ContainerInteraction.cpp, ContainerLifetime.cpp, ContainerOrdering.cpp, ContainerSetup.cpp, ContainerTimerCursor.cpp
// and MouseAt are the Rust object; each op here is a run of their C++ statements with no decision between them.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/won_gui/ctobj.h"
#include "rust/won_gui/guidraw.h"
#include "rust/won_gui/guihost.h"
#include "ee/wonapi/WONGUIDraw.h"
using namespace WONAPI;

typedef EeWonGuiOp Op;
typedef EeWonCtVirt V;

static Component* Comp(intptr_t a) { return (Component*)a; }

// a component's virtual (Component's own for the container, CtBase, and a child's Invalidate, RequestTimer and
// RequestFocus are gamewin.rs common_op's in the game: R6 WON GUI round 38)
static intptr_t CtVirtCall(Component* c, V v, intptr_t a, intptr_t b)
{
    switch (v) {
    case V::NeedsPaint: return c->NeedsPaint() ? 1 : 0;
    case V::PrePaint: c->PrePaint(*(Graphics*)a); return 0;
    case V::Paint: c->Paint(*(Graphics*)a); return 0;
    case V::EnableControlsById: c->EnableControlsById(*(const ControlIdSet*)a, b != 0); return 0;
    case V::NotifyRootVisibility: c->NotifyRootVisibility(a != 0); return 0;
    case V::HitTest: return c->HitTest((int)a, (int)b) ? 1 : 0;
    case V::MouseEnter: c->MouseEnter((int)a, (int)b); return 0;
    case V::MouseExit: c->MouseExit(); return 0;
    case V::MouseMove: c->MouseMove((int)a, (int)b); return 0;
    case V::MouseDrag: c->MouseDrag((int)a, (int)b); return 0;
    case V::MouseWheel: return c->MouseWheel((int)a) ? 1 : 0;
    case V::MouseHover: c->MouseHover((unsigned long)a); return 0;
    case V::HandleBroadcastEvent: c->HandleBroadcastEvent((ComponentEvent*)a); return 0;
    case V::AddedToParent: c->AddedToParent(); return 0;
    case V::MarkInvalid: c->MarkInvalid(); return 0;
    case V::ClearGrabBG: c->ClearGrabBG(); return 0;
    case V::TabFocus: return c->TabFocus(a != 0, b != 0) ? 1 : 0;
    case V::KeyDown: return c->KeyDown((int)a) ? 1 : 0;
    case V::KeyChar: return c->KeyChar((int)a) ? 1 : 0;
    case V::KeyUp: return c->KeyUp((int)a) ? 1 : 0;
    case V::CanClose: return c->CanClose() ? 1 : 0;
    case V::GotFocus: c->GotFocus(); return 0;
    case V::LostFocus: c->LostFocus(); return 0;
    case V::TimerEvent: return c->TimerEvent((int)a) ? 1 : 0;
    case V::SetCursor: c->SetCursor((Cursor*)a); return 0;
    case V::RemoveChild: static_cast<Container*>(c)->RemoveChild(Comp(a)); return 0; // (a container: the tab bar's)
    default: return 0;
    }
}

// a container's own virtuals (the subclasses' overrides)
static intptr_t CtOwnCall(Container* t, V v, intptr_t a, intptr_t b)
{
    switch (v) {
    case V::FindChildAt: return (intptr_t)t->FindChildAt((int)a, (int)b);
    case V::SetCursor: t->SetCursor((Cursor*)a); return 0;
    case V::TabFocus: return t->TabFocus(a != 0, b != 0) ? 1 : 0;
    case V::RemoveChild: t->RemoveChild(Comp(a)); return 0;
    case V::MouseMove: t->MouseMove((int)a, (int)b); return 0;
    case V::SetFocusChild: t->SetFocusChild(Comp(a)); return 0;
    case V::AddTimerChild: t->AddTimerChild(Comp(a)); return 0;
    case V::ChildInvalidated: return t->ChildInvalidated(Comp(a)) ? 1 : 0;
    case V::SizeChanged: t->Virtual08(); return 0;
    default: return 0;
    }
}

namespace WONAPI {
// every container op, for the Rust Container (Component's and the container's own calls are gamewin.rs
// common_op's in the game: R6 WON GUI round 37)
intptr_t ContainerHostCall(Container* t, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    switch (op) {
    case Op::CtChild: return CtVirtCall(Comp(a), (V)b, c, d);
    case Op::CtChildMouseDown: Comp(a)->MouseDown((int)b, (int)c, (int)d); return 0;
    case Op::CtChildMouseUp: Comp(a)->MouseUp((int)b, (int)c, (int)d); return 0;
    case Op::CtParentCall:
        if ((V)a == V::SetCursor) { t->mParent->SetCursor((Cursor*)b); return 0; }
        return CtOwnCall(t->mParent, (V)a, (intptr_t)static_cast<Component*>(t), 0);
    case Op::CtVirt: return CtOwnCall(t, (V)a, b, c);
    case Op::CtDetach: Comp(a)->mParent->RemoveChild(Comp(a)); return 0;
    case Op::CtSetScheme: ((Graphics*)a)->mColorScheme = (ColorScheme*)b; return 0;
    case Op::CtScheme: return (intptr_t)((Graphics*)a)->mColorScheme;
    // (a viewport whose area was never set: no area, not visible; the forwarder would read its object through NULL)
    case Op::VpAreaRectVisible:
        return a != 0 && static_cast<ScrollArea*>(Comp(a))->IsRectVisible(*(WONRectangle*)b) ? 1 : 0;
    case Op::Draw: WONDrawPlain(*(Graphics*)a, *(const EeWonDrawOp*)b); return 0;
    default: return 0;
    }
}
}

extern "C" intptr_t ee_won_guicpp_ct_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    return ContainerHostCall(static_cast<Container*>((Component*)inComp), op, a, b, c, d);
}

// the layout objects' host (ChildLayouts.cpp)
extern "C" intptr_t ee_won_guicpp_layout_call(void*, EeWonGuiOp, intptr_t, intptr_t, intptr_t, intptr_t);
namespace WONAPI {
bool Container::mMouseWheelGoesToFocusChildDef=false;
// Creates an empty, focusable container (the Rust container: no child, no mouse or focus child, no cursor, no modal
// state) with an empty child layout
Container::Container()
{ this->mCt=ee_won_ct_make(static_cast<Component*>(this),ee_won_guicpp_ct_call,ee_won_guicpp_layout_call,Container::mMouseWheelGoesToFocusChildDef); }
static_assert(ComponentFlag_WantFocus == EE_WON_COMP_FLAG_WANT_FOCUS, "compobj.rs flags");
// Destroys the container: the Rust container detaches the children, then releases the cursor, the child layout, the
// timer children and the children
Container::~Container() { ee_won_ct_free(this->mCt); this->mCt = NULL; }
}
