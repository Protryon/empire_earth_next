// Component (R6 WON round 38): its methods are Rust (rust/crates/ee_won_gui/src/compobj.rs); this unit keeps the
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
#include "rust/won_gui/compmem.h"
#include "rust/won_gui/compobj.h"
#include "rust/won_gui/guihost.h"
#include "rust/won_gui/guilayout.h"
#include "rust/won_gui/gamewin.h"
#include "ee/wonapi/EELobbySound.h"
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
              EE_COMP_AT(mInvalidState, invalid) && EE_COMP_AT(mTimerFlags, timer) && EE_COMP_AT(mExtraStuff, extra) && EE_COMP_AT(mCt, ct),
              "compmem.rs's block");
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
    case K::WQueue: w->QueueEvent((ComponentEvent*)c); return 0;
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
