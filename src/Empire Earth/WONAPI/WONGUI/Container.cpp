// Container painting, hit testing, mouse routing, child insertion and lookup (file name inferred)
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
// FindChildAt's verdict on the child hit is ee_won's guifocus module (rust/crates/ee_won/src/guifocus.rs); the
// child under the mouse's exit and enter, the release's child, the desired size and the insert place under the
// always-on-top children are its guievent module (guievent.rs).
namespace WONAPI {
// Container::SizeChanged in the SDK
// FUNCTION: WON 0x006f62b0
// Handles a size change: notifies the base, then reruns the child layout instructions
void Container::Virtual08()
{
    Component::Virtual08();
    this->mChildLayout.ExecuteInstructions(this);
}
// FUNCTION: WON 0x006f62d0
// Returns the size that just encloses every child (largest right and bottom edges)
void Container::GetDesiredSize(int& width, int& height)
{
    std::vector<int32_t> aRects;
    for(std::list<SmartPtr<Component> >::iterator i=this->mChildList.begin(); i!=this->mChildList.end(); ++i)
    {
        Component* child = i->get();
        aRects.push_back(child->Left());
        aRects.push_back(child->Top());
        aRects.push_back(child->Width());
        aRects.push_back(child->Height());
    }
    width = ee_won_gui_ct_desired_size(aRects.empty() ? NULL : &aRects[0], aRects.size() / 4, &height);
}

// FUNCTION: WON 0x006f6340
// Lets every child that needs painting capture its background, topmost first, then does its own
void Container::PrePaint(Graphics& g)
{
    if(!this->mChildList.empty()) {
        std::list<SmartPtr<Component> >::iterator i=this->mChildList.end();
        do {
            --i;
            Component* child=i->get();
            if(child->NeedsPaint()) {
                g.Translate(child->Left(),child->Top());
                child->PrePaint(g);
                g.Translate(-child->Left(),-child->Top());
            }
        } while(i!=this->mChildList.begin());
    }
    Component::PrePaint(g);
}

// FUNCTION: WON 0x006f6480
// Enables or disables the controls with the given ids in itself and every child
void Container::EnableControlsById(const ControlIdSet& ids, bool enabled)
{
    Component::EnableControlsById(ids,enabled);
    for(std::list<SmartPtr<Component> >::iterator i=this->mChildList.begin(); i!=this->mChildList.end(); ++i)
        (*i)->EnableControlsById(ids,enabled);
}

// FUNCTION: WON 0x006f64d0
// Records root visibility, resets the dialog-key state and passes it on to every visible child
void Container::NotifyRootVisibility(bool visible)
{
    Component::NotifyRootVisibility(visible);
    this->mHasUsedDialogKeys=false;
    for(std::list<SmartPtr<Component> >::iterator i=this->mChildList.begin();i!=this->mChildList.end();++i) {
        Component* child=i->get();
        if(child->IsVisible()) child->NotifyRootVisibility(visible);
    }
}

// FUNCTION: WON 0x006f6520
// Returns whether a point hits the container; with child-only input, only points on a child count
bool Container::HitTest(int x,int y)
{
    if(!Component::HitTest(x,y)) return false;
    if(this->mOnlyChildInput) {
        std::list<SmartPtr<Component> >::iterator i=this->mChildList.end();
        do {
            --i; // No empty-list guard here (the original has none either)
            Component* child=i->get();
            if(child->Contains(x,y) && child->HitTest(x-child->Left(),y-child->Top())) return true;
        } while(i!=this->mChildList.begin());
        return false;
    }
    return true;
}
// FUNCTION: WON 0x006f65b0
// Returns the topmost child under a point, or NULL if that child is disabled or blocked by a modal child
Component* Container::FindChildAt(int x,int y)
{
    if(this->mChildList.empty())
        return NULL;
    std::list<SmartPtr<Component> >::iterator i=this->mChildList.end();
    do {
        --i;
        Component* child=i->get();
        if(child->Contains(x,y) && child->HitTest(x-child->Left(),y-child->Top())) {
            return ee_won_gui_hit_child(child->IsDisabled(),this->mModalCount>0,(uint32_t)child->mComponentBits) ? child
                                                                                                                : NULL;
        }
    } while(i!=this->mChildList.begin());
    return NULL;
}

// FUNCTION: WON 0x006f6650
// Tracks which child is under the mouse, sending exit and enter and restoring the cursor on a change
void Container::UpdateMouseChild(int x,int y)
{
    Component* aChild=this->FindChildAt(x,y);
    uint8_t aSteps=ee_won_gui_ct_mouse_child(aChild==this->mMouseChild,this->mMouseChild!=NULL,aChild!=NULL);
    if(aChild==this->mMouseChild)
        return;
    if(aSteps & 1) {
        this->mMouseChild->MouseExit();
        this->mMouseChild=aChild;
        this->SetCursor(this->GetDefaultCursor());
    } else this->mMouseChild=aChild;
    if(aSteps & 2) aChild->MouseEnter(x-aChild->Left(),y-aChild->Top());
}

// FUNCTION: WON 0x006f6700
// Shows the default cursor, then enters the child under the mouse
void Container::MouseEnter(int x,int y)
{
    Component::MouseEnter(x,y);
    this->UpdateMouseChild(x,y);
}
// FUNCTION: WON 0x006f6730
// Closes its tooltip, sends the exit on to the child under the mouse and forgets its cursor
void Container::MouseExit()
{
    Component::MouseExit();
    if(this->mMouseChild!=NULL)
    {
        this->mMouseChild->MouseExit();
        this->mMouseChild=NULL;
    }
    this->mCurrentCursor=NULL;
}
// FUNCTION: WON 0x006f6780
// Updates the child under the mouse and passes the move to it in its own coordinates
void Container::MouseMove(int x,int y)
{
    this->UpdateMouseChild(x,y);
    Component* aChild=this->mMouseChild;
    if(aChild!=NULL) aChild->MouseMove(x-aChild->Left(),y-aChild->Top());
}

// FUNCTION: WON 0x006f6810
// Passes a drag to the child under the mouse in its own coordinates
void Container::MouseDrag(int x,int y)
{
    Component* aChild=this->mMouseChild;
    if(aChild!=NULL) aChild->MouseDrag(x-aChild->Left(),y-aChild->Top());
}

// FUNCTION: WON 0x006f6840
// Passes a button release to the child under the mouse, only if it also got the press
void Container::MouseUp(int x,int y,int button)
{
    Component* anOld=this->mMouseChild;
    this->UpdateMouseChild(x,y);
    Component* aChild=this->mMouseChild;
    if(ee_won_gui_ct_release_to_child(aChild!=NULL,aChild==anOld)) aChild->MouseUp(x-aChild->Left(),y-aChild->Top(),button);
}

// FUNCTION: WON 0x006f6880
// Updates the child under the mouse and passes the button press to it
void Container::MouseDown(int x,int y,int button)
{
    this->UpdateMouseChild(x,y);
    Component* aChild=this->mMouseChild;
    if(aChild!=NULL) aChild->MouseDown(x-aChild->Left(),y-aChild->Top(),button);
}


// FUNCTION: WON 0x006f69d0
// FUNCTION: WON 0x006f6a40
// Returns the insert position at or below the given one that stays under the always-on-top children
std::list<SmartPtr<Component> >::iterator Container::FindTop(std::list<SmartPtr<Component> >::iterator i)
{
    // the always-on-top flags of the children before i (front to back)
    std::vector<uint8_t> anOnTop;
    for(std::list<SmartPtr<Component> >::iterator j=this->mChildList.begin(); j!=i; ++j)
        anOnTop.push_back((*j)->ComponentFlagSet(ComponentFlag_AlwaysOnTop) ? 1 : 0);
    size_t aPlace = ee_won_gui_ct_find_top(anOnTop.empty() ? NULL : &anOnTop[0], anOnTop.size(), anOnTop.size());
    std::list<SmartPtr<Component> >::iterator r = this->mChildList.begin();
    std::advance(r, aPlace);
    return r;
}


std::list<SmartPtr<Component> >::iterator Container::FindTop()
{
    return this->FindTop(this->mChildList.end());
}

// FUNCTION: WON 0x006f6a60
// Moves a component into this container, below any always-on-top children
void Container::AddChild(Component* child)
{
    if(child->mParent!=NULL) child->mParent->RemoveChild(child);
    child->mParent=this;
    child->mChildListItr=this->mChildList.insert(this->FindTop(),SmartPtr<Component>(child));
    child->AddedToParent();
}

// FUNCTION: WON 0x006f6b50
// Moves a component into this container at a list position, kept below any always-on-top children
void Container::AddChildEx(Component* child,int position)
{
    if(child->mParent!=NULL) child->mParent->RemoveChild(child);
    child->mParent=this;
    std::list<SmartPtr<Component> >::iterator i=this->mChildList.begin();
    for(int n=0;n<position && i!=this->mChildList.end();++n) ++i;
    child->mChildListItr=this->mChildList.insert(this->FindTop(i),SmartPtr<Component>(child));
    child->AddedToParent();
}

// FUNCTION: WON 0x006f7320
// Handles a broadcast event, then passes it to every child
void Container::HandleBroadcastEvent(ComponentEvent* event)
{
    Component::HandleBroadcastEvent(event);
    for(std::list<SmartPtr<Component> >::iterator i=this->mChildList.begin();i!=this->mChildList.end();++i)
        (*i)->HandleBroadcastEvent(event);
}

// FUNCTION: WON 0x006f7770
// Returns the first component with a control id in this container's tree, searched level by level, or NULL
Component* Container::FindChildById(int inControlId)
{
    std::list<Component*> pending;
    pending.push_back(this);
    for (std::list<Component*>::iterator i=pending.begin(); i!=pending.end(); ++i) {
        Component* component=*i;
        if (component->mControlId==inControlId)
            return component;
        Container* container=dynamic_cast<Container*>(component);
        if (container==NULL)
            continue;
        std::list<SmartPtr<Component> >::iterator child=container->mChildList.begin();
        while(child!=container->mChildList.end()) {
            pending.push_back(child->get());
            ++child;
        }
    }
    return NULL;
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071c2a0
// (public: void __thiscall std::list<class WONAPI::Component *,class std::allocator<class WONAPI::Component *> >::_Insert(class std::list<class WONAPI::C)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071ca10
// (public: class std::list<class WONAPI::Component *,class std::allocator<class WONAPI::Component *> >::iterator __thiscall std::list<class WONAPI::Compo)
// TEMPLATE: WON 0x0071ebb0
// (protected: void __thiscall std::list<class WONAPI::Component *,class std::allocator<class WONAPI::Component *> >::_Incsize(unsigned int)    [?_Incsize)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071dd00
// (protected: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::Component>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::Compone)
// TEMPLATE: WON 0x007241d0
// (void __cdecl std::_Construct(class WONAPI::SmartPtr<class WONAPI::Component> *,class WONAPI::SmartPtr<class WONAPI::Component> const &))
