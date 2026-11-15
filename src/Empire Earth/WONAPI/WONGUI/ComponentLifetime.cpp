// Component (R6 WON round 38): its methods are Rust (rust/crates/ee_won/src/compobj.rs); this unit keeps the
// constructor, the extra data and the destructors, the fonts (the root window's) and the host: each op a run of
// the original's statements on a component, its parent, window, event, listener or background grabber.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONComponentFlags.h"
namespace WONAPI {
static_assert(ComponentFlag_Invisible == EE_WON_COMP_FLAG_INVISIBLE && ComponentFlag_HasFocus == EE_WON_COMP_FLAG_HAS_FOCUS &&
              ComponentFlag_Disabled == EE_WON_COMP_FLAG_DISABLED && ComponentFlag_GrabBG == EE_WON_COMP_FLAG_GRAB_BG &&
              ComponentFlag_Translucent == EE_WON_COMP_FLAG_TRANSLUCENT && ComponentFlag_NoInput == EE_WON_COMP_FLAG_NO_INPUT &&
              ComponentFlag_WantTabFocus == EE_WON_COMP_FLAG_WANT_TAB_FOCUS, "compobj.rs flags");
static_assert(sizeof(WONRectangle) == sizeof(EeWonRect), "compobj.rs rectangles");
// Creates the extra component state with no background grabber and tooltips allowed
ComponentExtra::ComponentExtra()
{
    this->mGrabBG=NULL;
    this->mDontDoTip=false;
}
// Destroys the extra state, deleting its background grabber before the other members (emitted at 0x006f48d0)
ComponentExtra::~ComponentExtra() { delete this->mGrabBG; }
// Creates a component with no parent, no flags, no control id and no extra data
Component::Component()
{
    this->mParent=NULL;
    this->mComponentBits=0;
    this->mInvalidState=0;
    this->mControlId=0;
    this->mTimerFlags=0;
    this->mFocusId=0;
    this->mExtraStuff=NULL;
}
// Destroys the component, deleting its extra state (emitted at 0x006f4a70)
Component::~Component() { delete this->mExtraStuff; }
// Allocates the extra component state on first use
void Component::AllocateExtraStuff()
{
    if(this->mExtraStuff==NULL) this->mExtraStuff=new ComponentExtra;
}
SmartPtr<Font> Component::GetFont(const FontDescriptor& descriptor) { return this->GetRoot()->GetFont(descriptor); }
Font* Component::GetDefaultFont() { return this->GetRoot()->mManager->GetDefaultFont(); }
Font* Component::GetNamedFont(const std::string& name) { return this->GetRoot()->GetWindowManager()->GetNamedFont(name); }
Font* Component::GetDefaultFontMod(int style,int size) { return this->GetRoot()->GetWindowManager()->GetDefaultFontMod(style,size); }

// the Rust component's calls out (compobj.rs EeWonComp), each a run of the original's statements
extern "C" intptr_t ee_won_guicpp_comp_call(void*, EeWonGuiOp op, intptr_t k, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonComp K;
    typedef EeWonCompExtra X;
    Component* t = (Component*)b;
    Container* p = static_cast<Container*>(t);
    Window* w = (Window*)b;
    ComponentEvent* e = (ComponentEvent*)b;
    BackgroundGrabber* g = (BackgroundGrabber*)b;
    if (op != EeWonGuiOp::Comp)
        return 0;
    switch ((K)k) {
    case K::Bits: return t->mComponentBits;
    case K::SetBits: t->mComponentBits = (int)c; return 0;
    case K::Invalid: return t->mInvalidState;
    case K::SetInvalid: t->mInvalidState = (char)c; return 0;
    case K::Timer: return t->mTimerFlags;
    case K::SetTimer: t->mTimerFlags = (char)c; return 0;
    case K::ControlId: return t->mControlId;
    case K::Bounds: *(WONRectangle*)c = t->mBounds; return 0;
    case K::SetXY: t->mBounds.x = (int)c; t->mBounds.y = (int)d; return 0;
    case K::SetWH: t->mBounds.height = (int)d; t->mBounds.width = (int)c; return 0;
    case K::Parent: return (intptr_t)(Component*)t->mParent;
    case K::HasExtra: return t->mExtraStuff != NULL;
    case K::AllocExtra: t->AllocateExtraStuff(); return 0;
    case K::Extra: case K::SetExtra: {
        ComponentExtra* x = t->mExtraStuff;
        bool set = (K)k == K::SetExtra;
        switch ((X)c) {
        case X::Listener: if (set) x->mListener = (ComponentListener*)d; return (intptr_t)x->mListener.get();
        case X::Cursor: if (set) x->mDefaultCursor = (Cursor*)d; return (intptr_t)x->mDefaultCursor.get();
        case X::Scheme: if (set) x->mColorScheme = (ColorScheme*)d; return (intptr_t)x->mColorScheme.get();
        case X::GrabBG: if (set) x->mGrabBG = new BackgroundGrabber; return (intptr_t)x->mGrabBG;
        case X::ToolTip: return (intptr_t)x->mToolTip.get();
        case X::DontDoTip: if (set) x->mDontDoTip = d != 0; return x->mDontDoTip;
        default: return 0;
        }
    }
    case K::VPositionChanged: t->PositionChanged(); return 0;
    case K::VSizeChanged: t->Virtual08(); return 0;
    case K::VDesiredSize: t->GetDesiredSize(((int*)c)[0], ((int*)c)[1]); return 0;
    case K::VHandleEvent: t->HandleComponentEvent((ComponentEvent*)c); return 0;
    case K::VHandleBroadcast: t->HandleBroadcastEvent((ComponentEvent*)c); return 0;
    case K::VInvalidateRect: t->InvalidateRect(*(WONRectangle*)c); return 0;
    case K::VInvalidate: t->Invalidate(); return 0;
    case K::VMarkInvalid: t->MarkInvalid(); return 0;
    case K::VGotFocus: t->GotFocus(); return 0;
    case K::VLostFocus: t->LostFocus(); return 0;
    case K::VNotifyRootVisibility: t->NotifyRootVisibility(c != 0); return 0;
    case K::VSetEnabled: t->SetEnabled(c != 0); return 0;
    case K::VEnabledChanged: t->EnabledChanged(c != 0); return 0;
    case K::VSetCursor: t->SetCursor((Cursor*)c); return 0;
    case K::PNextChild: return (intptr_t)p->NextChild((Component*)c, (d & 1) != 0, (d & 2) != 0);
    case K::PSetFocusChild: p->SetFocusChild((Component*)c); return 0;
    case K::PAddTimerChild: p->AddTimerChild((Component*)c); return 0;
    case K::PChildStateChanged: p->ChildStateChanged((Component*)c); return 0;
    case K::PChildInvalidated: p->ChildInvalidated((Component*)c); return 0;
    case K::PIsModal: return p->IsModal();
    case K::PEnsureOnTopOf: p->EnsureOnTopOf((Component*)c, (Component*)d); return 0;
    case K::AsRoot: return (intptr_t)dynamic_cast<RootContainer*>(t);
    case K::RootWin: return (intptr_t)((RootContainer*)b)->mWindow;
    case K::RootAddBG: ((RootContainer*)b)->AddBG((BackgroundGrabber*)c); return 0;
    case K::DefaultWindow: return (intptr_t)Window::GetDefaultWindow();
    case K::WState: return w->GetWindowState();
    case K::WBroadcast: w->mManager->BroadcastEvent((ComponentEvent*)c); return 0;
    case K::WQueue: w->QueueEvent((ComponentEvent*)c); return 0;
    case K::WParent: return (intptr_t)w->GetParent();
    case K::WRoot: return (intptr_t)(Component*)w->mRoot.get();
    case K::WMouse: ((int*)c)[0] = w->GetMouseX(); ((int*)c)[1] = w->GetMouseY(); return 0;
    case K::WOpenTip: {
        const intptr_t* a = (const intptr_t*)c;
        PopupParams params((Component*)a[0], PopupFlag_StandardTip, (Component*)a[1], (int)a[2], (int)a[3]);
        w->OpenPopup(params);
        return 0;
    }
    case K::WClosePopup: w->ClosePopup((Component*)c); return 0;
    case K::WKeyMod: return w->GetKeyMod();
    case K::WDoubleClick: return w->CheckDoubleClick(static_cast<MouseButton>(c));
    case K::WSubscribe: w->GetWindowManager()->SubscribeToBroadcast((Component*)c, d != 0); return 0;
    case K::WScheme: return (intptr_t)w->GetWindowManager()->mColorScheme.get();
    case K::WFontMod: {
        FontDescriptor descriptor = ((Font*)c)->mDescriptor;
        descriptor.mStyle = ((const int*)d)[0];
        if (((const int*)d)[1] != 0) descriptor.mSize = ((const int*)d)[1];
        return (intptr_t)w->GetFont(descriptor).get();
    }
    case K::EvHold: e->AddRef(); return 0;
    case K::EvRelease: e->Release(); return 0;
    case K::EvBroadcast: return e->mBroadcast;
    case K::EvSetBroadcast: e->mBroadcast = c != 0; return 0;
    case K::EvComp: return (intptr_t)e->mComponent;
    case K::EvSetComp: e->SetComponent((Component*)c); return 0;
    case K::EvNew: return (intptr_t)new ComponentEvent(t, (int)c);
    case K::LHandle: ((ComponentListener*)b)->HandleComponentEvent((ComponentEvent*)c); return 0;
    case K::GSizeChanged: g->SizeChanged((Component*)c); return 0;
    case K::GPrePaint: g->PrePaint((Component*)c, *(Graphics*)d); return 0;
    case K::GPaint: g->Paint((Component*)c, *(Graphics*)d); return 0;
    case K::GClear: g->Clear(); return 0;
    case K::GInvalidate: g->Invalidate((Component*)c); return 0;
    case K::GNeedDrawBG: return g->NeedDrawBG();
    case K::GTranslucent: g->SetTranslucentColor((int)c, (int)d); return 0;
    case K::IdIn: return ((const ControlIdSet*)b)->find((long)c) != ((const ControlIdSet*)b)->end();
    default: return 0;
    }
}
}
