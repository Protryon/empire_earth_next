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
namespace WONAPI {
// Current default button and the dialog's default button (0x00966224 and 0x00966228 in the original)
SmartPtr<Button> Button::mDefaultButton;
SmartPtr<Button> Button::mDialogDefaultButton;
// FUNCTION: WON 0x006f3070
// Creates a button with no flags, not pressed, that takes focus
Button::Button()
{
    mButtonFlags=0;
    this->mKeyDown=false;
    this->mMouseDownState=0;
    this->SetComponentFlags(ComponentFlag_WantTabFocus,true);
}
// Destroys the button (members only)
Button::~Button() {}

// FUNCTION: WON 0x006f30d0
// Lets an activation fire its event (always true here)
bool Button::HandleActivate() { return true; }

void Button::SetTextHook(const GUIString&) {}
// FUNCTION: WON 0x006f3180
// Sets the button text and optionally repaints
void Button::SetText(const GUIString& text,bool invalidate)
{
    this->SetTextHook(text);
    if(invalidate) this->Invalidate();
}
// FUNCTION: WON 0x006f31c0
// Makes a button the dialog's default button and the current default
void Button::SetDialogDefaultButton(Button* button)
{
    if(button==Button::mDialogDefaultButton.get()) return;
    Button::mDialogDefaultButton=button;
    Button::SetDefaultButton(button);
}
// FUNCTION: WON 0x006f3230
// Clears the dialog's default button if it is this one, and drops it as the current default
void Button::UnSetDialogDefaultButton(Button* button)
{
    if(button!=Button::mDialogDefaultButton.get()) return;
    Button::mDialogDefaultButton=NULL;
    Button::UnSetDefaultButton(button);
}
// FUNCTION: WON 0x006f3280
// Makes a button the current default (the one Enter presses), repainting the old and the new one
void Button::SetDefaultButton(Button* button)
{
    if(button==Button::mDefaultButton.get()) return;
    if(Button::mDefaultButton.get()!=NULL) Button::mDefaultButton->Invalidate();
    Button::mDefaultButton=button;
    if(button!=NULL) button->Invalidate();
}
// FUNCTION: WON 0x006f3300
// Falls back to the dialog's default button as the current default
void Button::UnSetDefaultButton(Button* button)
{
    // the original ignores the argument and falls back to the dialog's default button (0x6f3300)
    Button::SetDefaultButton(Button::mDialogDefaultButton.get());
}
// FUNCTION: WON 0x006f3310
// Copies another button's flags and click sound
void Button::CopyAttributes(Button* from)
{
    mButtonFlags=from->mButtonFlags;
    this->mClickSound=from->mClickSound;
}
// FUNCTION: WON 0x006f3170
// Returns whether the mouse is over the button
bool Button::IsMouseOver() { return mButtonFlags&0x10?true:false; }
// FUNCTION: WON 0x006f31b0
// Returns whether this is the current default button
bool Button::IsDefaultButton() { return Button::mDefaultButton.get()==this; }
// FUNCTION: WON 0x006f3370
// Sets or clears button flags; a radio button's selected state follows its check
void Button::SetButtonFlags(unsigned long flags,bool set)
{
    if(set) mButtonFlags|=flags;
    else mButtonFlags&=~flags;
    if(this->IsRadio()) this->SetComponentFlags(ComponentFlag_WantFocus,this->IsChecked());
}
// FUNCTION: WON 0x006f33c0
// Resets hover and press on joining a parent; a radio group with no focusable member makes this one focusable
void Button::AddedToParent()
{
    Component::AddedToParent();
    this->SetButtonFlags(0x30,false);
    if(!this->IsRadio())
        return;
    this->SetCheck(this->IsChecked(),false);
    if(this->WantFocus())
        return;
    Button* radio=this->GetEndRadio(false);
    while(radio!=NULL) {
        if(radio->WantFocus()) return;
        radio=radio->GetNextRadio(true);
    }
    this->SetComponentFlags(ComponentFlag_WantFocus,true);
}
// FUNCTION: WON 0x006f3440
// Activates the button unless disabled: checks a radio, toggles a check box, and fires the button event
void Button::Fn2F3440()
{
    if(this->IsDisabled() || this->IsNoActivate()) return;
    if(this->IsRadio()) this->SetCheck(true,false);
    else this->SetCheck(!this->IsChecked(),false);
    if(this->HandleActivate()) this->Fn2F5380(ComponentEvent_ButtonPressed,0);
}
// FUNCTION: WON 0x006f34a0
// Returns the next (or previous) sibling if it is a radio button, else NULL
Button* Button::GetNextRadio(bool forward)
{
    Component* aSibling=this->GetNextSibling(forward,false);
    Button* button=dynamic_cast<Button*>(aSibling);
    return button!=NULL && button->IsRadio() ? button : NULL;
}
// FUNCTION: WON 0x006f34e0
// Returns the last (or first) radio button of this button's group
Button* Button::GetEndRadio(bool forward)
{
    Button* last=this;
    while(true) {
        Button* next=last->GetNextRadio(forward);
        if(next==NULL) break;
        last=next;
    }
    return last;
}
// FUNCTION: WON 0x006f3500
// Checks or unchecks the button (a checked radio unchecks its group and takes focus); the optional
// event fires before the check state changes
void Button::SetCheck(bool checked,bool notify)
{
    if(this->IsRadio()) {
        this->SetComponentFlags(ComponentFlag_WantFocus,checked);
        if(checked) {
            Button* radio=this->GetEndRadio(false);
            while(radio!=NULL) {
                if(radio!=this) {
                    radio->SetComponentFlags(ComponentFlag_WantFocus,false);
                    if(radio->IsChecked()) { radio->SetButtonFlags(4,false); radio->Invalidate(); }
                }
                radio=radio->GetNextRadio(true);
            }
            this->RequestFocus();
        }
    }
    if(notify) this->Fn2F5380(ComponentEvent_ButtonPressed,0);
    if(this->IsChecked()!=checked) { this->SetButtonFlags(4,checked); this->Invalidate(); }
}
// FUNCTION: WON 0x006f35b0
// Marks the mouse as over the button and repaints if it highlights on hover
void Button::MouseEnter(int x,int y)
{
    Component::MouseEnter(x,y);
    this->SetButtonFlags(0x10,true);
    if(mButtonFlags&1) this->Invalidate();
}
// FUNCTION: WON 0x006f35e0
// Presses the button on a left click
void Button::MouseDown(int,int,int button)
{
    if(button==0) { this->mMouseDownState=2; this->ButtonDown(); }
}
// FUNCTION: WON 0x006f3600
// Releases the button on a left release, activating it
void Button::MouseUp(int,int,int button)
{
    if(button==0) { this->mMouseDownState=0; this->ButtonUp(true); }
}
// FUNCTION: WON 0x006f3620
// Releases the button without activating and clears the hover state
void Button::MouseExit()
{
    Component::MouseExit();
    this->mMouseDownState=0;
    this->ButtonUp(false);
    if(this->IsMouseOver() && (mButtonFlags&1)) this->Invalidate();
    this->SetButtonFlags(0x10,false);
}
// FUNCTION: WON 0x006f3660
// Presses the button: plays the click sound, starts repeating or activates on press if set, takes focus
void Button::ButtonDown()
{
    if(this->IsPressed()) return;
    if(this->mClickSound.get()!=NULL) this->mClickSound->Play();
    this->SetButtonFlags(0x20,true);
    this->Invalidate();
    if(this->IsRepeat()) { this->mDelayAcc=0; this->RequestTimer(true); this->Fn2F3440(); }
    if(this->IsActivateOnPress()) this->Fn2F3440();
    this->RequestFocus();
}
// FUNCTION: WON 0x006f36d0
// Releases a pressed button (unless the mouse or key still holds it), activating it unless dragged off
void Button::ButtonUp(bool activate)
{
    if(!this->IsPressed() || this->mMouseDownState==2 || this->mKeyDown) return;
    if(this->mMouseDownState==1) activate=false;
    this->SetButtonFlags(0x20,false);
    this->Invalidate();
    if(this->IsRepeat()) { this->RequestTimer(false); return; }
    if(activate && !this->IsActivateOnPress()) this->Fn2F3440();
}
// FUNCTION: WON 0x006f3740
// Presses the button while dragging inside it and releases it, without activating, outside
void Button::MouseDrag(int x,int y)
{
    if(this->mMouseDownState==0) return;
    if(x>=0 && y>=0 && x<this->Width() && y<this->Height()) {
        this->mMouseDownState=2;
        if(!this->IsPressed()) { this->ButtonDown(); this->mDelayAcc=500; }
    } else { this->mMouseDownState=1; this->ButtonUp(false); }
}
// FUNCTION: WON 0x006f37a0
// Takes focus, becoming the default button if it asks to
void Button::GotFocus()
{
    Component::GotFocus();
    if(this->IsDefaultOnFocus()) Button::SetDefaultButton(this);
    this->Invalidate();
}
// FUNCTION: WON 0x006f37d0
// Loses focus: releases a key press without activating and gives up the default button
void Button::LostFocus()
{
    Component::LostFocus();
    if(this->mKeyDown) { this->mKeyDown=false; this->ButtonUp(false); }
    Button::UnSetDefaultButton(this);
    this->Invalidate();
}
// FUNCTION: WON 0x006f3800
// Handles keys: Enter activates the default button, Space presses, arrows move and activate within a radio group
bool Button::KeyDown(int key)
{
    if(key==13) {
        if(this->IsDefaultButton()) { this->Fn2F3440(); return true; }
    } else if(key==32 && !this->mKeyDown) {
        this->mKeyDown=true;
        if(this->mParent!=NULL) this->mParent->mHasUsedDialogKeys=true;
        this->ButtonDown();
        return true;
    }
    if(this->IsRadio()) {
        bool forward;
        if(key==38 || key==37) forward=false;
        else if(key==40 || key==39) forward=true;
        else return false;
        if(this->mParent!=NULL) this->mParent->mHasUsedDialogKeys=true;
        Button* next=this->GetNextRadio(forward);
        if(next!=NULL) { next->Fn2F3440(); next->RequestFocus(); }
        return true;
    }
    return false;
}
// FUNCTION: WON 0x006f38b0
// Releases the button on Space, activating it
bool Button::KeyUp(int key)
{
    if(key==32) {
        this->mKeyDown=false;
        this->ButtonUp(true);
        return true;
    }
    return false;
}
// FUNCTION: WON 0x006f38d0
// Repeats the activation every 100 ms after an initial 500 ms hold
bool Button::TimerEvent(int delta)
{
    this->mDelayAcc+=delta;
    if(this->mDelayAcc>500) { this->mDelayAcc-=100; this->Fn2F3440(); }
    return true;
}
// FUNCTION: WON 0x006f3900
// Forwards mouse hover to the base component
void Button::MouseHover(unsigned long delta) { Component::MouseHover(delta); }
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
