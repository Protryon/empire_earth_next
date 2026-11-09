// Container modal state, child removal and reordering, and focus changes (file name inferred)
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
// FUNCTION: WON 0x006f6210
// Enters modal mode: drops mouse and focus from children that are not allowed while modal
void Container::EnterModal()
{
    ++this->mModalCount;
    if(this->mMouseChild!=NULL && !this->mMouseChild->ComponentFlagSet(0x8000)) this->MouseAt();
    if(this->mFocusChild!=NULL && !this->mFocusChild->ComponentFlagSet(0x8000)) this->SetFocusToLastFocusChild();
}
// FUNCTION: WON 0x006f6260
// Leaves modal mode completely (the count drops to zero), rechecking the mouse child and restoring focus
void Container::LeaveModal()
{
    if(!(this->mModalCount>0))
        return;
    this->mModalCount=0;
    this->MouseAt();
    if(this->mFocusChild==NULL) this->SetFocusToLastFocusChild();
}
// FUNCTION: WON 0x006f6c60
// Removes a child: repaints the area it covered, drops it from the layout, and moves focus and mouse off it
void Container::RemoveChild(Component* child)
{
    if(child->mParent!=this) return;
    SmartPtr<Component> holder; // never assigned: its frame slot and EH state 0 are in the function
    if(!this->mRemovingAllChildren && child->ComponentFlagSet(ComponentFlag_GrabBG)) {
        child->Invalidate();
        RootContainer* root=this->GetRootContainer();
        if(root!=NULL) root->AddBG(child->GetGrabBG());
    } else this->Invalidate();
    SmartPtr<Component> keep=child;
    this->mChildList.erase(child->mChildListItr);
    if(!this->mRemovingAllChildren) this->mChildLayout.RemoveComponent(child);
    if(child==this->mFocusChild) this->SetFocusToLastFocusChild();
    if(child==this->mMouseChild) this->MouseAt();
    child->mParent=NULL;
}
// FUNCTION: WON 0x006f6d80
// Removes every child, clears the child layout and resets the cursor
void Container::RemoveAllChildren()
{
    this->mRemovingAllChildren=true;
    while(!this->mChildList.empty()) this->RemoveChild(this->mChildList.front().get());
    this->mChildLayout.Clear();
    this->mRemovingAllChildren=false;
    this->SetCursor(NULL);
}
// FUNCTION: WON 0x006f6dd0
// Moves a child to the top of the drawing order, below any always-on-top children, and repaints it
void Container::BringToTop(Component* child)
{
    if(child->mParent!=this) return;
    SmartPtr<Component> keep=child;
    BackgroundGrabber* grabber=child->GetGrabBG();
    RootContainer* root=this->GetRootContainer();
    bool needInvalidate=true;
    if(grabber!=NULL) {
        child->Invalidate();
        root->AddBG(grabber);
        needInvalidate=false;
    }
    this->mChildList.erase(child->mChildListItr);
    child->mChildListItr=this->mChildList.insert(this->FindTop(),SmartPtr<Component>(child));
    if(needInvalidate) this->Invalidate();
}
// FUNCTION: WON 0x006f7440
// Gives focus to a child, taking it from the previous one and claiming focus from the parent chain
void Container::SetFocusChild(Component* child)
{
    if(!this->IsVisible() || this->IsDisabled()) return;
    if(this->mFocusChild==child && this->ComponentFlagSet(ComponentFlag_HasFocus)) return;
    if(this->mModalCount>0 && child!=NULL && !child->ComponentFlagSet(0x8000)) return;
    if(this->mFocusChild!=NULL && this->ComponentFlagSet(ComponentFlag_HasFocus)) this->mFocusChild->LostFocus();
    this->mFocusChild=child;
    // The original writes the focus id before its NULL test: a NULL child crashes here
    child->mFocusId=this->GetNextFocusId();
    if(child==NULL)
        return;
    if(!this->ComponentFlagSet(ComponentFlag_HasFocus) && this->mParent!=NULL) this->mParent->SetFocusChild(this);
    else {
        this->SetComponentFlags(ComponentFlag_HasFocus,true);
        if(this->mParent==NULL && this->GetRootContainer()==this) {
            Window* window=this->GetRoot(); // GetWindow in the SDK
            if(window->mManager->mFocusWindow!=window) return;
        }
        child->GotFocus();
    }
}
// FUNCTION: WON 0x006f7510
// Reacts to a child being hidden or disabled: takes its focus away and rechecks the child under the mouse
void Container::ChildStateChanged(Component* child)
{
    bool bad=child->IsDisabled() || !child->IsVisible();
    if(this->mFocusChild==child && bad) {
        this->mFocusChild->LostFocus();
        this->mFocusChild=NULL;
    }
    if(this->GetRootContainer()==NULL) return;
    int x,y;
    this->GetMousePos(x,y);
    if(child->Contains(x,y) || (this->mMouseChild==child && bad)) this->UpdateMouseChild(x,y);
}
}
