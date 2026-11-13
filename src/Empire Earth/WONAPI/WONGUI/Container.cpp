// Container (R6 WON round 26): the Rust Container's calls out (rust/crates/ee_won/src/ctobj.rs, EeWonGuiOp's
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
#include "rust/ee_won.h"
#include "ee/wonapi/WONGUIDraw.h"
using namespace WONAPI;

typedef EeWonGuiOp Op;
typedef EeWonCtVirt V;

static Component* Comp(intptr_t a) { return (Component*)a; }

// a component's virtual (or, for the base, Component's own: inBase)
static intptr_t CtVirtCall(Component* c, bool inBase, V v, intptr_t a, intptr_t b)
{
    if (inBase) {
        switch (v) {
        case V::SizeChanged: c->Component::Virtual08(); return 0;
        case V::PrePaint: c->Component::PrePaint(*(Graphics*)a); return 0;
        case V::Paint: c->Component::Paint(*(Graphics*)a); return 0;
        case V::EnableControlsById: c->Component::EnableControlsById(*(const ControlIdSet*)a, b != 0); return 0;
        case V::NotifyRootVisibility: c->Component::NotifyRootVisibility(a != 0); return 0;
        case V::HitTest: return c->Component::HitTest((int)a, (int)b) ? 1 : 0;
        case V::MouseEnter: c->Component::MouseEnter((int)a, (int)b); return 0;
        case V::MouseExit: c->Component::MouseExit(); return 0;
        case V::MouseHover: c->Component::MouseHover((unsigned long)a); return 0;
        case V::HandleBroadcastEvent: c->Component::HandleBroadcastEvent((ComponentEvent*)a); return 0;
        case V::AddedToParent: c->Component::AddedToParent(); return 0;
        case V::MarkInvalid: c->Component::MarkInvalid(); return 0;
        case V::ClearGrabBG: c->Component::ClearGrabBG(); return 0;
        case V::GotFocus: c->Component::GotFocus(); return 0;
        case V::LostFocus: c->Component::LostFocus(); return 0;
        default: return 0;
        }
    }
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
    case V::Invalidate: c->Invalidate(); return 0;
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
    case V::RequestTimer: c->RequestTimer(a != 0); return 0;
    case V::RequestFocus: c->RequestFocus(); return 0;
    case V::SetCursor: c->SetCursor((Cursor*)a); return 0;
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
    default: return 0;
    }
}

namespace WONAPI {
// every container op, for the Rust Container
intptr_t ContainerHostCall(Container* t, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    switch (op) {
    case Op::CtBase: return CtVirtCall(t, true, (V)a, b, c);
    case Op::CtChild: return CtVirtCall(Comp(a), false, (V)b, c, d);
    case Op::CtChildMouseDown: Comp(a)->MouseDown((int)b, (int)c, (int)d); return 0;
    case Op::CtChildMouseUp: Comp(a)->MouseUp((int)b, (int)c, (int)d); return 0;
    case Op::CtParentCall:
        if ((V)a == V::SetCursor) { t->mParent->SetCursor((Cursor*)b); return 0; }
        return CtOwnCall(t->mParent, (V)a, (intptr_t)static_cast<Component*>(t), 0);
    case Op::CtVirt: return CtOwnCall(t, (V)a, b, c);
    case Op::CtDetach: Comp(a)->mParent->RemoveChild(Comp(a)); return 0;
    case Op::CtSetParent: Comp(a)->mParent = b != 0 ? t : NULL; return 0;
    case Op::CtSetFocusId: Comp(a)->mFocusId = (unsigned char)b; return 0;
    case Op::CtSetInvalidState: Comp(a)->mInvalidState = (char)b; return 0;
    case Op::CtSetTimerFlags: t->mTimerFlags = (char)a; return 0;
    case Op::CtAddBG: t->GetRootContainer()->AddBG((BackgroundGrabber*)a); return 0;
    case Op::CtLayout:
        switch (a) {
        case 0: t->mChildLayout->ExecuteInstructions(t); return 0;
        case 1: t->mChildLayout->RemoveComponent(Comp(b)); return 0;
        case 2: t->mChildLayout->Clear(); return 0;
        default: delete t->mChildLayout; t->mChildLayout = NULL; return 0;
        }
    case Op::CtCursorHold: ((Cursor*)a)->AddRef(); return 0;
    case Op::CtCursorRelease: ((Cursor*)a)->Release(); return 0;
    case Op::CtSetScheme: ((Graphics*)a)->mColorScheme = (ColorScheme*)b; return 0;
    case Op::CtBits: return (uint32_t)Comp(a)->mComponentBits;
    case Op::CtParentOf: return (intptr_t)static_cast<Component*>(Comp(a)->mParent);
    case Op::CtTimerFlags: return Comp(a)->mTimerFlags;
    case Op::CtInvalidState: return Comp(a)->mInvalidState;
    case Op::CtFocusId: return Comp(a)->mFocusId;
    case Op::CtContains: return Comp(a)->Contains((int)b, (int)c) ? 1 : 0;
    case Op::CtIntersects: return Comp(a)->Intersects(Comp(b)) ? 1 : 0;
    case Op::CtGrabBG: return (intptr_t)Comp(a)->GetGrabBG();
    case Op::CtOwnScheme: return (intptr_t)Comp(a)->GetOwnColorScheme();
    case Op::CtScheme: return (intptr_t)((Graphics*)a)->mColorScheme;
    case Op::CtRoot: return (intptr_t)static_cast<Component*>(t->GetRootContainer());
    case Op::CtRootFocused: {
        Window* aWindow = t->GetRoot(); // GetWindow in the SDK
        return aWindow->mManager->mFocusWindow == aWindow ? 1 : 0;
    }
    case Op::CtMouseInClient: return t->GetRoot()->mMouseIsInClientArea ? 1 : 0;
    case Op::CtDefaultCursor: return (intptr_t)t->GetDefaultCursor();
    case Op::CtWantFocus: return t->WantFocus() ? 1 : 0;
    case Op::CtNextSibling: return (intptr_t)Comp(a)->GetNextSibling(b != 0, c != 0);
    case Op::VpAreaRectVisible: return static_cast<ScrollArea*>(Comp(a))->IsRectVisible(*(WONRectangle*)b) ? 1 : 0;
    case Op::CtContainerOf: {
        Container* aContainer = dynamic_cast<Container*>(Comp(a));
        return aContainer != NULL ? (intptr_t)aContainer->mCt : 0;
    }
    case Op::ChildHold: Comp(a)->AddRef(); return 0;
    case Op::ChildRelease: Comp(a)->Release(); return 0;
    case Op::ChildLeft: return Comp(a)->Left();
    case Op::ChildTop: return Comp(a)->Top();
    case Op::ChildWidth: return Comp(a)->Width();
    case Op::ChildHeight: return Comp(a)->Height();
    case Op::ChildInvalidate: Comp(a)->Invalidate(); return 0;
    case Op::ChildSetFlags: Comp(a)->SetComponentFlags((int)b, c != 0); return 0;
    case Op::ChildControlId: return Comp(a)->mControlId;
    case Op::Invalidate: t->Invalidate(); return 0;
    case Op::RequestFocus: t->RequestFocus(); return 0;
    case Op::GetMousePos: t->GetMousePos(*(int*)a, *(int*)b); return 0;
    case Op::KeyMod: return t->GetKeyMod();
    case Op::Draw: {
        WONDrawSlots aSlots;
        WONDrawNoCall aNoCall;
        WONDrawOne(*(Graphics*)a, *(const EeWonDrawOp*)b, aSlots, aNoCall);
        return 0;
    }
    default: return 0;
    }
}
}

extern "C" intptr_t ee_won_guicpp_ct_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    return ContainerHostCall(static_cast<Container*>((Component*)inComp), op, a, b, c, d);
}

namespace WONAPI {
bool Container::mMouseWheelGoesToFocusChildDef=false;
// FUNCTION: WON 0x006f5b70
// Creates an empty, focusable container (the Rust container: no child, no mouse or focus child, no cursor, no modal
// state) with an empty child layout
Container::Container()
{
    this->mCt=ee_won_ct_new(static_cast<Component*>(this),ee_won_guicpp_ct_call,Container::mMouseWheelGoesToFocusChildDef);
    this->mChildLayout=new ChildLayoutManager;
    this->SetComponentFlags(ComponentFlag_WantFocus,true);
}
// FUNCTION: WON 0x006f5e80
// Destroys the container: the Rust container detaches the children, then releases the cursor, the child layout, the
// timer children and the children
Container::~Container() { ee_won_ct_free(this->mCt); }
}
