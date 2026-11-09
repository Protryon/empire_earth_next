// Container painting, invalidation, tab traversal and keyboard/focus forwarding (file name inferred)
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
// FUNCTION: WON 0x006f63b0
// Paints every invalid visible child in its own coordinates, clip rectangle and colour scheme
void Container::Paint(Graphics& graphics)
{
    ColorScheme* scheme=graphics.mColorScheme;
    Component::Paint(graphics);
    for(std::list<SmartPtr<Component> >::iterator i=this->mChildList.begin();i!=this->mChildList.end();++i) {
        Component* child=i->get();
        if(child->mInvalidState==0 || !child->IsVisible())
            continue;
        graphics.Translate(child->Left(),child->Top());
        if(child->ComponentFlagSet(ComponentFlag_Clip)) graphics.PushClipRect(0,0,child->Width(),child->Height());
        if(child->GetOwnColorScheme()!=NULL) graphics.mColorScheme=child->GetOwnColorScheme();
        else graphics.mColorScheme=scheme;
        child->Paint(graphics);
        if(child->ComponentFlagSet(ComponentFlag_Clip)) graphics.PopClipRect();
        graphics.Translate(-child->Left(),-child->Top());
    }
}
// FUNCTION: WON 0x006f68c0
// Marks the container partly invalid and invalidates visible children above the changed one that overlap it;
// returns whether any did
bool Container::ChildInvalidated(Component* changed)
{
    if(this->mInvalidState==2) return false;
    this->mInvalidState=1;
    bool higherInvalid=false;
    std::list<SmartPtr<Component> >::iterator i=this->mChildList.end();
    while(i!=this->mChildList.begin()) {
        --i;
        Component* child=i->get();
        if(child==changed) break;
        if(child->Intersects(changed) && child->IsVisible()) {
            child->Invalidate();
            higherInvalid=true;
        }
    }
    if(this->mParent!=NULL) this->mParent->ChildInvalidated(this);
    return higherInvalid;
}
// FUNCTION: WON 0x006f6940
// Marks the container and every child fully invalid
void Container::MarkInvalid()
{
    // Unlike the later SDK, an already fully invalid container returns at once
    if(this->mInvalidState==2) return;
    Component::MarkInvalid();
    this->mInvalidState=2;
    for(std::list<SmartPtr<Component> >::iterator i=this->mChildList.begin();i!=this->mChildList.end();++i)
        (*i)->MarkInvalid();
}
// FUNCTION: WON 0x006f69a0
// Discards the grabbed background of the container and every child
void Container::ClearGrabBG()
{
    Component::ClearGrabBG();
    for(std::list<SmartPtr<Component> >::iterator i=this->mChildList.begin();i!=this->mChildList.end();++i)
        (*i)->ClearGrabBG();
}
// FUNCTION: WON 0x006f75a0
// Moves tab focus to the next or previous focusable child (wrapping in an isolated group); returns
// whether a child took it
bool Container::TabFocus(bool forward,bool init)
{
    if(!this->WantFocus()) return false;
    if(init) {
        if(this->mIsolateTabFocus) return false;
        if(this->ComponentFlagSet(ComponentFlag_WantTabFocus)) {
            this->mHasUsedDialogKeys=true;
            this->RequestFocus();
            return true;
        }
    }
    if(this->mChildList.empty()) return false;
    bool wrap=this->mIsolateTabFocus;
    Component* child=init?NULL:this->mFocusChild;
    if(child==NULL) { SmartPtr<Component> &first=forward?this->mChildList.front():this->mChildList.back(); child=first.get(); }
    else child=child->GetNextSibling(forward,wrap);
    unsigned long count=0;
    while(child!=NULL) {
        if(child->TabFocus(forward,true)) {
            this->mHasUsedDialogKeys=true;
            return true;
        }
        child=child->GetNextSibling(forward,wrap);
        if(++count>=this->mChildList.size()) break;
    }
    return false;
}
// FUNCTION: WON 0x006f7690
// Passes a key press to the focused child; an unhandled Tab (Shift+Tab backwards) moves focus
bool Container::KeyDown(int key)
{
    if(this->mFocusChild!=NULL && this->mFocusChild->KeyDown(key)) return true;
    if(key!=9) return false;
    if(this->mChildList.empty()) return false;
    bool forward=(this->GetKeyMod()&1)==0;
    return this->TabFocus(forward,false);
}
// FUNCTION: WON 0x006f7730
// Returns whether every child agrees to close
bool Container::CanClose()
{
    for(std::list<SmartPtr<Component> >::iterator i=this->mChildList.begin();i!=this->mChildList.end();++i)
        if(!(*i)->CanClose()) return false;
    return true;
}
// FUNCTION: WON 0x006f7400
// Marks the container focused and passes focus on to its focused child
void Container::GotFocus() { Component::GotFocus(); if(this->mFocusChild!=NULL) this->mFocusChild->GotFocus(); }
// FUNCTION: WON 0x006f7420
// Marks the container unfocused and takes focus from its focused child
void Container::LostFocus() { Component::LostFocus(); if(this->mFocusChild!=NULL) this->mFocusChild->LostFocus(); }
// FUNCTION: WON 0x006f7710
// Passes a typed character to the focused child; returns whether it was handled
bool Container::KeyChar(int key) { return this->mFocusChild!=NULL?this->mFocusChild->KeyChar(key):false; }
// FUNCTION: WON 0x006f76f0
// Passes a key release to the focused child; reports it handled whenever there is one
bool Container::KeyUp(int key)
{
    if(this->mFocusChild!=NULL) {
        this->mFocusChild->KeyUp(key);
        return true;
    }
    return false;
}
// FUNCTION: WON 0x006f6990
// Hooks the container into its new parent and resets the dialog-key state
void Container::AddedToParent() { Component::AddedToParent(); this->mHasUsedDialogKeys=false; }
// FUNCTION: WON 0x006f6980
// Invalidates the container (base behaviour)
void Container::Invalidate() { Component::Invalidate(); }
// FUNCTION: WON 0x006f67c0
// Passes a wheel turn to the child under the mouse, or to the focused child if so configured
bool Container::MouseWheel(int amount)
{
    Component* child=this->mMouseChild;
    if(this->mMouseWheelGoesToFocusChild) child=this->mFocusChild;
    if(child!=NULL) return child->MouseWheel(amount);
    return false;
}
// FUNCTION: WON 0x006f67f0
// Handles its own tooltip hover, then passes the hover to the child under the mouse
void Container::MouseHover(unsigned long elapsed) { Component::MouseHover(elapsed); if(this->mMouseChild!=NULL) this->mMouseChild->MouseHover(elapsed); }
}
