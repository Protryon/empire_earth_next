// Button (R6 WON round 30): the Rust Button's calls out (rust/crates/ee_won/src/btnobj.rs, EeWonGuiOp's button ops and
// the component ops it shares with the container), its constructor and destructor, the static functions' forwarders,
// the two virtuals subclasses override (HandleActivate, SetTextHook) and the click sound. The original's Button.cpp
// (the flags, the press and release, the keys, the repeat, the radio group, the default buttons) is the Rust object;
// each op here is a run of its C++ statements with no decision between them.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/ee_won.h"
using namespace WONAPI;

typedef EeWonGuiOp Op;
typedef EeWonCtVirt V;

static Component* Comp(intptr_t a) { return (Component*)a; }

// t is NULL for the static functions' ops (those on a)
extern "C" intptr_t ee_won_guicpp_btn_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)d;
    Button* t = static_cast<Button*>((Component*)inComp);
    switch (op) {
    case Op::CtBase:
        switch ((V)a) {
        case V::AddedToParent: t->Component::AddedToParent(); return 0;
        case V::MouseEnter: t->Component::MouseEnter((int)b, (int)c); return 0;
        case V::MouseExit: t->Component::MouseExit(); return 0;
        case V::GotFocus: t->Component::GotFocus(); return 0;
        case V::LostFocus: t->Component::LostFocus(); return 0;
        case V::MouseHover: t->Component::MouseHover((unsigned long)b); return 0;
        default: return 0;
        }
    case Op::CtChild: if ((V)b == V::RequestFocus) Comp(a)->RequestFocus(); return 0; // (the next radio's)
    case Op::CtBits: return (uint32_t)Comp(a)->mComponentBits;
    case Op::CtParentOf: return (intptr_t)static_cast<Component*>(Comp(a)->mParent);
    case Op::CtWantFocus: return t->WantFocus() ? 1 : 0;
    case Op::CtNextSibling: return (intptr_t)Comp(a)->GetNextSibling(b != 0, c != 0);
    case Op::CtContainerOf: {
        Container* aContainer = dynamic_cast<Container*>(Comp(a));
        return aContainer != NULL ? (intptr_t)aContainer->mCt : 0;
    }
    case Op::BtnOf: {
        Button* aButton = dynamic_cast<Button*>(Comp(a));
        return aButton != NULL ? (intptr_t)aButton->mBtn : 0;
    }
    case Op::ChildHold: Comp(a)->AddRef(); return 0;
    case Op::ChildRelease: Comp(a)->Release(); return 0;
    case Op::ChildWidth: return Comp(a)->Width();
    case Op::ChildHeight: return Comp(a)->Height();
    case Op::ChildInvalidate: Comp(a)->Invalidate(); return 0;
    case Op::ChildSetFlags: Comp(a)->SetComponentFlags((int)b, c != 0); return 0;
    case Op::Invalidate: t->Invalidate(); return 0;
    case Op::RequestFocus: t->RequestFocus(); return 0;
    case Op::RequestTimer: t->RequestTimer(a != 0); return 0;
    case Op::BtnHandleActivate: return t->HandleActivate() ? 1 : 0;
    case Op::BtnSetTextHook: t->SetTextHook(*(const GUIString*)a); return 0;
    case Op::BtnHasClickSound: return t->mClickSound.get() != NULL ? 1 : 0;
    case Op::BtnPlayClick: t->mClickSound->Play(); return 0;
    // the type is Rust's map, which the golden checks against the enum
    case Op::FireEvent: t->Fn2F5380(ee_won_gui_event_type((EeWonGuiEvent)a), b != 0); return 0;
    default: return 0;
    }
}

namespace WONAPI {
// FUNCTION: WON 0x006f3070
// Creates a button that takes focus (the Rust button: no flags, not pressed)
Button::Button()
{
    this->mBtn=ee_won_btn_new(static_cast<Component*>(this),ee_won_guicpp_btn_call);
    this->SetComponentFlags(ComponentFlag_WantTabFocus,true);
}
// Destroys the button (the Rust button; the click sound is a member)
Button::~Button() { ee_won_btn_free(this->mBtn); }

// FUNCTION: WON 0x006f30d0
// Lets an activation fire its event (always true here)
bool Button::HandleActivate() { return true; }

void Button::SetTextHook(const GUIString&) {}
// FUNCTION: WON 0x006f31c0
// Makes a button the dialog's default button and the current default
void Button::SetDialogDefaultButton(Button* b)
{
    ee_won_btn_static(ee_won_guicpp_btn_call,EeWonBtnStatic::SetDialogDefault,(intptr_t)static_cast<Component*>(b));
}
// FUNCTION: WON 0x006f3230
// Clears the dialog's default button if it is this one, and drops it as the current default
void Button::UnSetDialogDefaultButton(Button* b)
{
    ee_won_btn_static(ee_won_guicpp_btn_call,EeWonBtnStatic::UnSetDialogDefault,(intptr_t)static_cast<Component*>(b));
}
// FUNCTION: WON 0x006f3280
// Makes a button the current default (the one Enter presses), repainting the old and the new one
void Button::SetDefaultButton(Button* b)
{
    ee_won_btn_static(ee_won_guicpp_btn_call,EeWonBtnStatic::SetDefault,(intptr_t)static_cast<Component*>(b));
}
// FUNCTION: WON 0x006f3300
// Falls back to the dialog's default button as the current default (the original ignores the argument)
void Button::UnSetDefaultButton(Button* b)
{
    ee_won_btn_static(ee_won_guicpp_btn_call,EeWonBtnStatic::UnSetDefault,(intptr_t)static_cast<Component*>(b));
}
// FUNCTION: WON 0x006f3310
// Copies another button's flags and click sound
void Button::CopyAttributes(Button* from)
{
    this->BCall(EeWonBtnCall::CopyAttributes,(intptr_t)from->mBtn);
    this->mClickSound=from->mClickSound;
}
// FUNCTION: WON 0x006f3910
// Sets the sound played when the button is pressed
void Button::SetClickSound(Sound* sound) { this->mClickSound=sound; }
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f30e0
// (public: virtual void * __thiscall WONAPI::Button::`scalar deleting destructor'(unsigned int)    [??_GButton@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f3100
// (public: virtual __thiscall WONAPI::Button::~Button(void)    [??1Button@WONAPI@@UAE@XZ])
// SYNTHETIC: WON 0x6f2fd0
// SYNTHETIC: WON 0x6f2fe0
// SYNTHETIC: WON 0x6f2ff0
// SYNTHETIC: WON 0x6f3000
// SYNTHETIC: WON 0x6f3020
// SYNTHETIC: WON 0x6f3030
// SYNTHETIC: WON 0x6f3040
// SYNTHETIC: WON 0x6f3050
