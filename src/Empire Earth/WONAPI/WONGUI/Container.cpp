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
#include <set>
#include "rust/won_gui/rootobj.h"
#include "rust/won_gui/compmem.h"
#include "rust/won_gui/compobj.h"
#include "rust/won_gui/guilayout.h"
#include "rust/won_gui/gamewin.h"
#include "ee/wonapi/EELobbySound.h"
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
    // (a viewport whose area was never set: no area, not visible; the forwarder would read its object through NULL)
    case Op::VpAreaRectVisible:
        return a != 0 && static_cast<ScrollArea*>(Comp(a))->IsRectVisible(*(WONRectangle*)b) ? 1 : 0;
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

// ---- RootContainer.cpp until R6 WON GUI round 39
// RootContainer (R6 WON round 29): the Rust RootContainer (rust/crates/ee_won_gui/src/rootobj.rs: the invalid
// rectangle, the queued backgrounds, the cursor's component, the window drag), its constructor and destructor. Its
// own calls out are gamewin.rs root_op's (R6 WON GUI rounds 38-40); the container's are Container.cpp's.

// Creates the top-level container of a window: arrow cursor, window dragging allowed, tab focus kept inside (the Rust
// root: no invalid rectangle, no cursor component, no drag, no background)
RootContainer::RootContainer(Window *theWindow)
{
	this->mRootObj = ee_won_root_new(static_cast<Component*>(this), ee_won_guicpp_ct_call, this->mCt);
	this->mWindow = theWindow;
	this->SetDefaultCursor(Cursor::GetStandardCursor(StandardCursor_Arrow));
	this->SetComponentFlags(0x8 | 0x40000,true);
	this->SetIsolateTabFocus(true);
}

// Destroys the root: the Rust root gives the queued backgrounds' images back (the implicit destructor in the original)
RootContainer::~RootContainer() { ee_won_root_free(this->mRootObj); }

// ---- ComponentLifetime.cpp until R6 WON GUI round 39
// Component (R6 WON round 38): its methods are Rust (rust/crates/ee_won_gui/src/compobj.rs); this unit keeps the
// constructor, the extra data and the destructors, the fonts (the root window's) and the host: each op a run of
// the original's statements on a component, its parent, window, event, listener or background grabber.
namespace WONAPI {
static_assert(ComponentFlag_Invisible == EE_WON_COMP_FLAG_INVISIBLE && ComponentFlag_HasFocus == EE_WON_COMP_FLAG_HAS_FOCUS &&
              ComponentFlag_Disabled == EE_WON_COMP_FLAG_DISABLED && ComponentFlag_GrabBG == EE_WON_COMP_FLAG_GRAB_BG &&
              ComponentFlag_Translucent == EE_WON_COMP_FLAG_TRANSLUCENT && ComponentFlag_NoInput == EE_WON_COMP_FLAG_NO_INPUT &&
              ComponentFlag_WantTabFocus == EE_WON_COMP_FLAG_WANT_TAB_FOCUS, "compobj.rs flags");
static_assert(sizeof(WONRectangle) == sizeof(EeWonRect), "compobj.rs rectangles");
// (the extra component state, ComponentExtra, is compobj.rs's EeWonCompExtraData: R6 WON GUI round 36)
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
Component::~Component() { ee_won_comp_extra_free(this->mExtraStuff); }
SmartPtr<Font> Component::GetFont(const FontDescriptor& descriptor) { return this->GetRoot()->GetFont(descriptor); }
Font* Component::GetDefaultFont() { return this->GetRoot()->mManager->GetDefaultFont(); }
Font* Component::GetNamedFont(const std::string& name) { return this->GetRoot()->GetWindowManager()->GetNamedFont(name); }
Font* Component::GetDefaultFontMod(int style,int size) { return this->GetRoot()->GetWindowManager()->GetDefaultFontMod(style,size); }

// the Rust component's calls out (compobj.rs EeWonComp), each a run of the original's statements
extern "C" intptr_t ee_won_guicpp_comp_call(void*, EeWonGuiOp op, intptr_t k, intptr_t b, intptr_t c, intptr_t d)
{
    // Component's members as compmem.rs's block (EeWonCompMembers), at EE_WON_COMP_MEMBERS_AT (here: a friend)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#define EE_COMP_AT(m, f) (offsetof(Component, m) == EE_WON_COMP_MEMBERS_AT + offsetof(EeWonCompMembers, f))
    static_assert(EE_COMP_AT(mParent, parent) && EE_COMP_AT(mBounds, bounds) && EE_COMP_AT(mComponentBits, bits) &&
              EE_COMP_AT(mControlId, control_id) && EE_COMP_AT(mFocusId, focus_id) &&
              EE_COMP_AT(mInvalidState, invalid) && EE_COMP_AT(mTimerFlags, timer) && EE_COMP_AT(mExtraStuff, extra) && EE_COMP_AT(mCt, ct) &&
              offsetof(Cursor, mRust) == EE_WON_CURSOR_RUST_AT, "compmem.rs's block, a cursor's Rust cursor (gamewin.rs)");
#undef EE_COMP_AT
#pragma clang diagnostic pop
    typedef EeWonComp K;
    Component* t = (Component*)b;
    Container* p = static_cast<Container*>(t);
    Window* w = (Window*)b;
    BackgroundGrabber* g = (BackgroundGrabber*)b;
    if (op != EeWonGuiOp::Comp)
        return 0;
    switch ((K)k) {
    // (AllocExtra, Extra and SetExtra are a driver world's: the game's extra data is compobj.rs's own)
    case K::GNew: return (intptr_t)new BackgroundGrabber;
    case K::GDelete: delete g; return 0;
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
    case K::PSetFocusChild: p->SetFocusChild((Component*)c); return 0;
    case K::PAddTimerChild: p->AddTimerChild((Component*)c); return 0;
    case K::PChildStateChanged: p->ChildStateChanged((Component*)c); return 0;
    case K::PChildInvalidated: p->ChildInvalidated((Component*)c); return 0;
    case K::AsRoot: return (intptr_t)dynamic_cast<RootContainer*>(t);
    case K::RootWin: return (intptr_t)((RootContainer*)b)->mWindow;
    case K::RootAddBG: ((RootContainer*)b)->AddBG((BackgroundGrabber*)c); return 0;
    case K::WOpenTip: {
        const intptr_t* a = (const intptr_t*)c;
        PopupParams params((Component*)a[0], PopupFlag_StandardTip, (Component*)a[1], (int)a[2], (int)a[3]);
        w->OpenPopup(params);
        return 0;
    }
    case K::WClosePopup: w->ClosePopup((Component*)c); return 0;
    case K::WScheme: return (intptr_t)w->GetWindowManager()->mColorScheme.get();
    case K::WFontMod: {
        FontDescriptor descriptor = ((Font*)c)->mDescriptor;
        descriptor.mStyle = ((const int*)d)[0];
        if (((const int*)d)[1] != 0) descriptor.mSize = ((const int*)d)[1];
        return (intptr_t)w->GetFont(descriptor).get();
    }
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
    case K::VSetVisible: t->SetVisible(c != 0); return 0;
    case K::VMouseChild: return (intptr_t)t->GetMouseChild();
    default: return 0;
    }
}
// the one component host every Rust object calls a component through (compobj.rs comp()); an event's record
// (EeWonCompEventRec: its members, compobj.rs's in the game since R6 WON GUI round 39)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
[[maybe_unused]] static const bool sCompHost = (ee_won_comp_host_register(ee_won_guicpp_comp_call),
                                                ee_won_comp_event_register(offsetof(ComponentEvent, mEv)), true);
#pragma clang diagnostic pop
// the Rust objects' references to the game's RefCount objects (gamewin.rs rf: every host's Hold and Release)
static void RefOf(void* p, int32_t hold) { if (hold) ((RefCount*)p)->AddRef(); else ((RefCount*)p)->Release(); }
[[maybe_unused]] static const bool sRefHost = (ee_won_gui_ref_register(RefOf), true);
}

// ---- Sound.cpp until R6 WON GUI round 39
namespace WONAPI {
// Creates an empty sound descriptor with no flags (emitted at 0x00729810)
SoundDescriptor::SoundDescriptor() : mSoundFlags(0) {}
// Creates a sound descriptor for a file path with the given flags (emitted at 0x00729830)
SoundDescriptor::SoundDescriptor(const std::string& path,long flags) : mSoundFlags(flags), mPath(path) {}
// Sets or clears the given sound flags
void SoundDescriptor::SetSoundFlags(int flags,bool on)
{
    if(on) this->mSoundFlags|=flags;
    else this->mSoundFlags&=~flags;
}
// Creates a sound from a copy of the descriptor, not looping (emitted at 0x007299c0)
Sound::Sound(const SoundDescriptor& descriptor) : mDescriptor(descriptor), mIsLooping(false) {}
// Plays the sound unless it is muted
void Sound::Play()
{
    if(!this->GetMuted()) this->PlayHook();
}
// Mutes or unmutes the sound (flag 4); muting stops it
void Sound::SetMuted(bool muted)
{
    if(muted==this->GetMuted()) return;
    this->mDescriptor.SetSoundFlags(4,muted);
    if(muted) this->Stop();
}
}

// Stop and Resume forward to the playback hooks
void WONAPI::Sound::Stop()
{
    this->StopHook();
}
void WONAPI::Sound::Resume()
{
    this->ResumeHook();
}

// Returns true when the sound's muted flag (4) is set
bool WONAPI::Sound::GetMuted()
{
    return (this->mDescriptor.mSoundFlags&4)?true:false;
}

namespace WONAPI {

SoundDescriptor::SoundDescriptor(const char *thePath) : mSoundFlags(0), mPath(thePath) {}

void Sound::Rewind()
{
	this->RewindHook();
}
}

// the sound decoder base (SoundDecoder.cpp until R6 WON GUI round 37): decodes a descriptor through the virtual Decode
namespace WONAPI {
SoundDecoder::SoundDecoder() {}
SoundDecoder::~SoundDecoder() {} // (emitted at 0x00728590)
SoundPtr SoundDecoder::DecodeDescriptor(const SoundDescriptor& inDescriptor) { return this->Decode(inDescriptor); }
}
