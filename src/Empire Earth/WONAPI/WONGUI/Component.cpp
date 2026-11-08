// Component event delivery, position, size, visibility and enable state (file name inferred)
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
namespace WONAPI {
// FUNCTION: WON 0x006f52d0
// Marks the event as a broadcast and hands it to this component's broadcast handler
void Component::BroadcastEventDown(ComponentEvent* event)
{
    SmartPtr<ComponentEvent> eventRef = event;
    event->mBroadcast = true;
    this->HandleBroadcastEvent(event);
}

// FUNCTION: WON 0x006f53f0
// Sends the event now (broadcast through the window manager or bubbled up from here) or queues it on the
// root window; drops it when there is no open root window
void Component::DeliverEvent(ComponentEvent* event, bool sendNow)
{
    Window* window = this->GetRootWindow();
    if(window == NULL || window->mWindowState != 0)
    {
        SmartPtr<ComponentEvent> eventRef = event;
        return;
    }
    if(event->mComponent == NULL)
        event->SetComponent(this);
    if(sendNow)
    {
        SmartPtr<ComponentEvent> eventRef = event;
        if(event->mBroadcast)
        {
            WindowManager* aManager = window->mManager;
            aManager->BroadcastEvent(event);
        }
        else
            this->BubbleEvent(event);
    }
    else
        window->QueueEvent(event);
}

// FUNCTION: WON 0x006f4b70
// Moves the component within its parent and notifies the position change
void Component::SetPos(int x, int y)
{
    this->mBounds.x = x;
    this->mBounds.y = y;
    this->PositionChanged();
}

// FUNCTION: WON 0x006f4b90
// Resizes the component (negative sizes become 0) and notifies the size change when it differs
void Component::SetSize(int width, int height)
{
    if(width < 0) width = 0;
    if(height < 0) height = 0;
    if(width == this->Width() && height == this->Height())
        return;
    this->mBounds.height = height;
    this->mBounds.width = width;
    this->Virtual08();
}

// FUNCTION: WON 0x006f4bc0
// Grows the component to at least the given width and height; never shrinks it
void Component::SetSizeAtLeast(int width, int height)
{
    if(width < this->Width()) width = this->Width();
    if(height < this->Height()) height = this->Height();
    if(width > this->Width() || height > this->Height())
        this->SetSize(width,height);
}


// FUNCTION: WON 0x006f4bf0
// Moves and resizes the component
void Component::SetPosSize(int x, int y, int width, int height)
{
    this->SetPos(x,y);
    this->SetSize(width,height);
}

// FUNCTION: WON 0x006f4e90
// Resizes the component to its desired size plus the extra width and height
void Component::SetDesiredSize(int extraWidth,int extraHeight)
{
    int width = 0, height = 0;
    this->GetDesiredSize(width,height);
    this->SetSize(width+extraWidth,height+extraHeight);
}

// FUNCTION: WON 0x006f4ee0
// Sets the component's width to its desired width plus the padding, keeping its height
void Component::SetDesiredWidth(int padding)
{
    int width = 0, height = 0;
    this->GetDesiredSize(width,height);
    this->SetSize(width+padding,this->Height());
}

// FUNCTION: WON 0x006f56d0
// Converts a point in the component's coordinates to root window coordinates
void Component::LocalToRoot(int& x,int& y)
{
    x += this->Left();
    y += this->Top();
    for(Component* parent=this->mParent; parent!=NULL; parent=parent->mParent)
    {
        x += parent->Left();
        y += parent->Top();
    }
}

// FUNCTION: WON 0x006f50e0
// Passes a dirty rectangle up to the parent, offset by this component's position (changes the rectangle)
void Component::InvalidateRect(WONRectangle& rect)
{
    if(this->mParent == NULL)
        return;
    rect.x += this->Left();
    rect.y += this->Top();
    this->mParent->InvalidateRect(rect);
}

// FUNCTION: WON 0x006f5660
// Enables or disables the component when its control id is in the set
void Component::EnableControlsById(const ControlIdSet& ids,bool enabled)
{
    if(this->mControlId!=0)
    {
        int id=this->mControlId;
        if(ids.find(id)!=ids.end()) this->SetEnabled(enabled);
    }
}

// FUNCTION: WON 0x006f5080
// Returns whether the point (in parent coordinates) lies inside the component
bool Component::Contains(int x,int y)
{
    return x>=this->Left() && x<this->Left()+this->Width() && y>=this->Top() && y<this->Top()+this->Height();
}

// FUNCTION: WON 0x006f55b0
// Shows or hides the component, telling the parent, redrawing and notifying the change of visibility
void Component::SetVisible(bool visible)
{
    if(visible == this->IsVisible())
        return;
    this->SetComponentFlags(ComponentFlag_Invisible,!visible);
    this->mInvalidState=0;
    if(this->mParent!=NULL) this->mParent->ChildStateChanged(this);
    this->Invalidate();
    this->NotifyRootVisibility(visible);
}

// FUNCTION: WON 0x006f5610
// Enables or disables the component, notifying itself and its parent and redrawing
void Component::SetEnabled(bool enabled)
{
    if(enabled == !this->IsDisabled())
        return;
    this->SetComponentFlags(ComponentFlag_Disabled,!enabled);
    this->EnabledChanged(enabled);
    if(this->mParent!=NULL) this->mParent->ChildStateChanged(this);
    this->Invalidate();
}
}

// FUNCTION: WON 0x006f49e0
// Creates a component with no parent, no flags, no control id and no extra data
namespace WONAPI {
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
}

namespace WONAPI {
// FUNCTION: WON 0x006f50c0
// Returns whether the component's bounds overlap the other component's bounds
bool Component::Intersects(Component* component)
{
    return this->mBounds.Intersects(component->mBounds);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f4a50
// (public: virtual void * __thiscall WONAPI::Component::`scalar deleting destructor'(unsigned int)    [??_GComponent@WONAPI@@UAEPAXI@Z])
