// Component sibling/root traversal, focus, timers, tooltips and background-grab painting (file name inferred)
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
// FUNCTION: WON 0x006f5230
// Returns the next or previous sibling in the parent's child list, optionally wrapping (NULL without a parent)
Component* Component::GetNextSibling(bool forward,bool wrap)
{
    if(this->mParent==NULL) return NULL;
    std::list<SmartPtr<Component> >::iterator i=this->mChildListItr;
    if(forward) {
        ++i;
        if(i==this->mParent->mChildList.end()) {
            if(!wrap) return NULL;
            return this->mParent->mChildList.front().get();
        }
    } else {
        if(i==this->mParent->mChildList.begin()) {
            if(!wrap) return NULL;
            return this->mParent->mChildList.back().get();
        }
        --i;
    }
    return i->get();
}

// FUNCTION: WON 0x006f5160
// Hooks a newly added child into its parent: hands over or drops focus, registers timers, propagates root visibility
// and invalidates it
void Component::AddedToParent()
{
    if(this->HasFocus()) {
        if(!this->WantFocus()) this->LostFocus();
        else this->mParent->SetFocusChild(this);
    }
    if(this->mTimerFlags!=0) this->mParent->AddTimerChild(this);
    if(this->mParent->IsRootVisible() && this->IsVisible()) this->NotifyRootVisibility(true);
    this->mInvalidState=0;
    this->Invalidate();
}

// FUNCTION: WON 0x006f5720
// Converts root coordinates into this component's coordinates by subtracting every ancestor's position
void Component::RootToChild(int& x,int& y)
{
    x-=this->Left();
    y-=this->Top();
    if(this->mParent!=NULL) this->mParent->RootToChild(x,y);
}

// FUNCTION: WON 0x006f5810
// Returns whether the component can take focus: focusable, visible, enabled, and not blocked by a modal parent
bool Component::WantFocus()
{
    if(!this->ComponentFlagSet(ComponentFlag_WantFocus)) return false;
    if(!this->IsVisible() || this->IsDisabled()) return false;
    if(this->mParent!=NULL && this->mParent->mModalCount>0 && !this->ComponentFlagSet(0x8000)) return false;
    return true;
}
}

namespace WONAPI {
// FUNCTION: WON 0x006f5a00
// Returns the font matching a descriptor from the component's window
SmartPtr<Font> Component::GetFont(const FontDescriptor& descriptor)
{
    Window* root=this->GetRoot();
    return root->GetFont(descriptor);
}
// FUNCTION: WON 0x006f5a30
// Returns a variant of a font with another style and, if size > 0, another size
Font* Component::GetFontMod(Font* font,int style,int size)
{
    FontDescriptor descriptor=font->mDescriptor;
    descriptor.mStyle=style;
    if(size>0) descriptor.mSize=size;
    return this->GetFont(descriptor).get();
}
// FUNCTION: WON 0x006f54e0
// Passes a non-broadcast event to the component's listener, else its parent, else the root of the owner window
void Component::BubbleEvent(ComponentEvent* event)
{
    if(event->mBroadcast) return;
    if(this->mExtraStuff!=NULL && this->mExtraStuff->mListener.get()!=NULL)
        this->mExtraStuff->mListener->HandleComponentEvent(event);
    else if(this->mParent!=NULL)
        this->mParent->HandleComponentEvent(event);
    else {
        Window* window=this->GetRoot();
        if(window!=NULL) {
            window=window->mParent;
            if(window!=NULL && window->mRoot.get()!=NULL)
                window->mRoot->BubbleEvent(event);
        }
    }
}
}

// Tool tip delay and display time, in milliseconds.
DWORD gComponentToolTipStartTime=500;
DWORD gComponentToolTipEndTime=5000;
namespace WONAPI {
// FUNCTION: WON 0x006f4cc0
// Shows the component's default cursor when the mouse enters
void Component::MouseEnter(int x,int y)
{
    if(this->mExtraStuff!=NULL) this->SetCursor(this->mExtraStuff->mDefaultCursor.get());
}
// FUNCTION: WON 0x006f4ce0
// Closes the component's tooltip and re-arms it when the mouse leaves
void Component::MouseExit()
{
    if(this->mExtraStuff!=NULL && this->mExtraStuff->mToolTip.get()!=NULL) {
        this->mExtraStuff->mDontDoTip=false;
        this->GetRoot()->ClosePopup(this->mExtraStuff->mToolTip.get());
    }
}
// FUNCTION: WON 0x006f4d10
// Opens the tooltip beside the mouse once the hover delay passes and closes it after the display time
void Component::MouseHover(unsigned long elapsed)
{
    if(this->mExtraStuff==NULL || this->mExtraStuff->mToolTip.get()==NULL) return;
    if(elapsed>=gComponentToolTipStartTime && !this->mExtraStuff->mDontDoTip) {
        Window* window=this->GetRoot();
        int x=window->mMouseX+15; int y=window->mMouseY+15;
        window->OpenPopup(PopupParams(this->mExtraStuff->mToolTip.get(),PopupFlag_StandardTip,this,x,y));
        this->mExtraStuff->mDontDoTip=true;
    } else if(elapsed>=gComponentToolTipStartTime+gComponentToolTipEndTime)
        this->GetRoot()->ClosePopup(this->mExtraStuff->mToolTip.get());
}
}

namespace WONAPI {
// FUNCTION: WON 0x006f5590
// Records whether the component's root is visible
void Component::NotifyRootVisibility(bool visible)
{
    this->SetComponentFlags(0x40000,visible);
}
// FUNCTION: WON 0x006f5850
// Takes focus on the initial tab step if the component is a tab stop and can take focus; returns whether it did
bool Component::TabFocus(bool forward,bool init)
{
    if(init && this->ComponentFlagSet(ComponentFlag_WantTabFocus) && this->WantFocus()) {
        this->RequestFocus();
        return true;
    }
    return false;
}
}

namespace WONAPI {
// FUNCTION: WON 0x006f57d0
// Turns the component's timer request on or off and tells the parent when its timer need changes
void Component::RequestTimer(bool on)
{
    bool oldWantTimer=this->mTimerFlags!=0;
    if(on) this->mTimerFlags|=2;
    else this->mTimerFlags&=~2;
    bool wantTimer=this->mTimerFlags!=0;
    if(wantTimer!=oldWantTimer && this->mParent!=NULL) this->mParent->AddTimerChild(this);
}
// FUNCTION: WON 0x006f5890
// Gives the component focus through its parent (or directly when it has none) if it can take it
void Component::RequestFocus()
{
    if(this->HasFocus() || !this->WantFocus()) return;
    if(this->mParent==NULL) this->GotFocus();
    else this->mParent->SetFocusChild(this);
}
// FUNCTION: WON 0x006f58f0
// Returns the topmost ancestor if it is a root container, else NULL
RootContainer* Component::GetRootContainer()
{
    Component* root=this;
    while(root->mParent!=NULL) root=root->mParent;
    return dynamic_cast<RootContainer*>(root);
}
// FUNCTION: WON 0x006f5920
// Returns the window of the component's root container, or the default window when it has none
Window* Component::GetRoot()
{
    RootContainer* root=this->GetRootContainer();
    if(root!=NULL) return root->mWindow;
    return Window::GetDefaultWindow();
}
// FUNCTION: WON 0x006f5940
// Returns the window of the component's root container, or NULL when it has none
Window* Component::GetRootWindow()
{
    RootContainer* root=this->GetRootContainer();
    if(root!=NULL) return root->mWindow;
    return NULL;
}
// FUNCTION: WON 0x006f5600
// Returns whether the component's root is visible
bool Component::IsRootVisible()
{
    return this->ComponentFlagSet(0x40000);
}
// FUNCTION: WON 0x006f58d0
// Marks the component as focused
void Component::GotFocus()
{
    this->SetComponentFlags(ComponentFlag_HasFocus,true);
}
// FUNCTION: WON 0x006f58e0
// Marks the component as not focused
void Component::LostFocus()
{
    this->SetComponentFlags(ComponentFlag_HasFocus,false);
}
}

namespace WONAPI {
// FUNCTION: WON 0x006f4db0
// Returns whether the component grabs its background, creating the background grabber on first use
bool Component::CheckGrabBG()
{
    if(this->ComponentFlagSet(0x300)) {
        this->AllocateExtraStuff();
        if(this->mExtraStuff->mGrabBG==NULL)
            this->mExtraStuff->mGrabBG=new BackgroundGrabber;
        return true;
    }
    return false;
}
// FUNCTION: WON 0x006f4f60
// Returns whether an invalid component needs painting: when visible, or when its grabbed background must be drawn
bool Component::NeedsPaint()
{
    if(this->mInvalidState==0) return false;
    if(this->IsVisible()) return true;
    return this->CheckGrabBG() && this->mExtraStuff->mGrabBG->NeedDrawBG();
}
// FUNCTION: WON 0x006f5110
// Invalidates the whole component, tells the parent, and marks it invalid (no-op if already fully invalid)
void Component::Invalidate()
{
    if(this->mInvalidState==2) return;
    WONRectangle rect(0,0,this->Width(),this->Height());
    this->InvalidateRect(rect);
    if(this->mParent!=NULL) this->mParent->ChildInvalidated(this);
    this->MarkInvalid();
}
// FUNCTION: WON 0x006f51f0
// Marks the component fully invalid and queues its background grabber with the root container
void Component::MarkInvalid()
{
    this->mInvalidState=2;
    if(!this->CheckGrabBG())
        return;
    this->mExtraStuff->mGrabBG->Invalidate(this);
    RootContainer* root=this->GetRootContainer();
    if(root!=NULL) root->AddBG(this->mExtraStuff->mGrabBG);
}
// FUNCTION: WON 0x006f4e50
// Tells the background grabber that the component's size changed
void Component::Virtual08()
{
    if(this->CheckGrabBG()) this->mExtraStuff->mGrabBG->SizeChanged(this);
}
// FUNCTION: WON 0x006f4ff0
// Lets the background grabber capture the background before painting
void Component::PrePaint(Graphics& graphics)
{
    if(this->CheckGrabBG()) this->mExtraStuff->mGrabBG->PrePaint(this,graphics);
}
// FUNCTION: WON 0x006f5020
// Clears the invalid state, then lets the background grabber paint the saved background
void Component::Paint(Graphics& graphics)
{
    this->mInvalidState=0;
    if(this->CheckGrabBG()) this->mExtraStuff->mGrabBG->Paint(this,graphics);
}
// FUNCTION: WON 0x006f51d0
// Discards the grabbed background
void Component::ClearGrabBG()
{
    if(this->CheckGrabBG()) this->mExtraStuff->mGrabBG->Clear();
}
}
