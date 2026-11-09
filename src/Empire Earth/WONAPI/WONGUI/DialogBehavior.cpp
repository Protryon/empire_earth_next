// Dialog behaviour: dragging, focus, Escape/Enter keys and the default button
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
// FUNCTION: WON 0x006f7990
// Makes Escape end the dialog with the given result id
void Dialog::SetEndOnEscape(int id)
{
    this->mEndOnEscape=true;
    this->mEscapeId=id;
}
// FUNCTION: WON 0x006f79b0
// Stops Escape from ending the dialog
void Dialog::SetDontEndOnEscape()
{
    this->mEndOnEscape=false;
}
// FUNCTION: WON 0x006f79c0
// Sets the button that Enter activates (held by reference)
void Dialog::SetDefaultButton(Button* button)
{
    this->mDefaultButton=button;
}
// FUNCTION: WON 0x006f7a20
// Registers the dialog's default button as the active default, then runs the container's focus gain
void Dialog::GotFocus()
{
    Button::SetDialogDefaultButton(this->mDefaultButton.get());
    Container::GotFocus();
}
// FUNCTION: WON 0x006f7a40
// Unregisters the dialog's default button as the active default, then runs the container's focus loss
void Dialog::LostFocus()
{
    Button::UnSetDialogDefaultButton(this->mDefaultButton.get());
    Container::LostFocus();
}
// FUNCTION: WON 0x006f7a60
// Cancels any drag in progress, then runs the container's handler for this slot
void Dialog::Virtual08()
{
    this->mDragging=false;
    Container::Virtual08();
}
// FUNCTION: WON 0x006f7a70
// Passes the press to the children; a left press takes the focus and, when dragging is allowed and the
// child under the mouse is a drag handle (flag 0x20), brings the dialog to the top and starts a drag
void Dialog::MouseDown(int x,int y,int button)
{
    Container::MouseDown(x,y,button);
    if(button!=0) return;
    this->RequestFocus();
    if(!this->mAllowDrag || this->mMouseChild==NULL || !this->mMouseChild->ComponentFlagSet(ComponentFlag_ParentDrag))
        return;
    if(this->GetNextSibling(true,false)!=NULL) this->mParent->BringToTop(this);
    this->mDragging=true;
    this->mDragX=x;
    this->mDragY=y;
}
// FUNCTION: WON 0x006f7af0
// Passes the release to the children; a left release ends any drag
void Dialog::MouseUp(int x,int y,int button)
{
    Container::MouseUp(x,y,button);
    if(button==0) this->mDragging=false;
}
// FUNCTION: WON 0x006f7b20
// Passes the mouse exit to the children and ends any drag
void Dialog::MouseExit()
{
    Container::MouseExit();
    this->mDragging=false;
}
// FUNCTION: WON 0x006f7b40
// Moves a dragged dialog by the mouse offset, optionally kept inside the root container, and repaints
// the old and new areas; without a drag passes the move to the children
void Dialog::MouseDrag(int x,int y)
{
    if(!this->mDragging) {
        Container::MouseDrag(x,y);
        return;
    }
    // The original clamps against GetRootContainer(), where the newer SDK uses GetParent()
    Container* root=this->GetRootContainer();
    int newX=this->Left()+(x-this->mDragX);
    int newY=this->Top()+(y-this->mDragY);
    if(this->mKeepInRoot) {
        if(newX+this->Width()>root->Width()) newX=root->Width()-this->Width();
        if(newX<0) newX=0;
        if(newY+this->Height()>root->Height()) newY=root->Height()-this->Height();
        if(newY<0) newY=0;
    }
    if(newX==this->Left() && newY==this->Top()) return;
    WONRectangle r(0,0,this->Width(),this->Height());
    this->mParent->ChildInvalidated(this);
    this->InvalidateRect(r);
    this->SetPos(newX,newY);
    this->InvalidateRect(r);
    this->Invalidate();
}
// FUNCTION: WON 0x006f7c30
// Gives the key to the children first; otherwise Escape ends the dialog (if enabled) and Enter
// activates the default button; returns whether the key was used
bool Dialog::KeyDown(int key)
{
    if(Container::KeyDown(key)) return true;
    if(key==27 && this->mEndOnEscape) {
        this->GetRoot()->EndDialog(this->mEscapeId,this);
        return true;
    } else if(key==13) return this->ActivateDefaultButton();
    return false;
}
// FUNCTION: WON 0x006f7c90
// Returns whether the dialog may close; a modal dialog closes only if Escape ends it, which ends
// it with the escape result
bool Dialog::CanClose()
{
    Window* window=this->GetRoot();
    if(!Container::CanClose()) return false;
    if(window->mManager->mModalComponent==this) {
        if(this->mEndOnEscape) window->EndDialog(this->mEscapeId,NULL);
        else return false;
    }
    return true;
}
// FUNCTION: WON 0x006f7ce0
// Clicks the default button if it is set, enabled and still the default; returns whether it did
bool Dialog::ActivateDefaultButton()
{
    if(this->mDefaultButton.get()!=NULL && !this->mDefaultButton->IsDisabled() && this->mDefaultButton->IsDefaultButton()) {
        this->mDefaultButton->Fn2F3440();
        return true;
    }
    return false;
}
// FUNCTION: WON 0x006f7d20
// Activates the default button on event 20, ends the dialog with the button's control id when a
// button with flag 0x100 is clicked (event 15), and passes other events on
void Dialog::HandleComponentEvent(ComponentEvent* event)
{
    if(event->mType==ComponentEvent_InputReturn) { this->ActivateDefaultButton(); return; }
    if(event->mType==ComponentEvent_ButtonPressed && ((static_cast<Button*>(event->mComponent)->mButtonFlags&0x100)?true:false)) {
        this->GetRoot()->EndDialog(event->mComponent->mControlId,NULL);
        return;
    }
    Container::HandleComponentEvent(event);
}
}
