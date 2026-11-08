// Combo box: an input box, a drop button and a popup list box
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONComponentFlags.h"
namespace WONAPI {
// Creates a combo box that takes focus, with the list hidden and a 200 x 150 list box
ComboBox::ComboBox()
{
    this->SetComponentFlags(ComponentFlag_WantFocus,true);
    this->SetOnlyChildInput(true); // byte at +0x7c
    this->mListBoxVisible=false;
    this->mNoType=false;
    this->mShowButtonWhenDisabled=true;
    this->mListBoxWidth=200;
    this->mListBoxHeight=150;
}
// FUNCTION: WON 0x006f3a50
// Destroys the combo box, first detaching it from its list box's listeners
ComboBox::~ComboBox()
{
    if(this->mListBox.get()!=NULL) this->mListBox->AddListener(NULL);
}
// FUNCTION: WON 0x006f3b70
// Inserts a text item into the list at the position (-1 = end) and resizes the list box; returns the item,
// or NULL when there is no list area
ListItem* ComboBox::AddString(const GUIString& inText,int inPos)
{
    if(this->mListArea.get()!=NULL) {
        ListItem* item=this->mListArea->InsertString(inText,inPos);
        this->CalcListBoxSize();
        return item;
    }
    return NULL;
}
// FUNCTION: WON 0x006f3bb0
// Inserts an item into the list at the position and resizes the list box; without a list area the item is
// taken and released (freed if nothing else holds it)
void ComboBox::InsertItem(TextListItem* inItem,int inPos)
{
    if(this->mListArea.get()!=NULL) {
        this->mListArea->InsertItem(inItem,inPos);
        this->CalcListBoxSize();
    } else {
        SmartPtr<TextListItem> held=inItem;
    }
}
// FUNCTION: WON 0x006f3c10
// Deletes the list item at the position and resizes the list box
void ComboBox::DeleteItem(int inPos)
{
    if(this->mListArea.get()!=NULL) {
        this->mListArea->DeleteItem(inPos);
        this->CalcListBoxSize();
    }
}
// FUNCTION: WON 0x006f3c40
// Redraws a changed list item
void ComboBox::UpdateItem(TextListItem* inItem)
{
    if(this->mListArea.get()!=NULL) this->mListArea->RefreshItem(inItem);
}
// FUNCTION: WON 0x006f3c60
// Empties the list, optionally clears the input text, and resizes the list box
void ComboBox::Clear(bool inClearInput)
{
    this->mListArea->Clear(true);
    if(inClearInput) this->mEdit->SetText(GUIString(new GUIStringData("")),true);
    this->CalcListBoxSize();
}
// FUNCTION: WON 0x006f3d40
// Returns the position of the selected list item, or -1 when none is selected
int ComboBox::GetSelItem()
{
    ListItem* item=this->mListArea->mSelItem;
    return item==NULL ? -1 : this->mListArea->GetItemPos(item);
}
// FUNCTION: WON 0x006f3d60
// Selects the list item at the position and copies its text into the input box (empty when nothing ends up
// selected), optionally firing a selection event
void ComboBox::SetSelItem(int inPos,bool inNotify)
{
    if(this->mListArea.get()==NULL) return;
    this->mListArea->SetSelItem(inPos,true);
    if(this->mListArea->mSelItem==NULL)
        this->mEdit->SetText(GUIString(new GUIStringData("")),inNotify);
    else this->SetInputBoxToListSelection(false,inNotify);
}
// The original reads the new list box's scroll area before testing it for NULL
// FUNCTION: WON 0x006f3e70
// Sets the popup list box and attaches the list area to it if it is not already its scroll area
void ComboBox::SetListBox(Scroller* inListBox)
{
    this->mListBox=inListBox;
    if(this->mListArea.get()!=NULL && inListBox->GetScrollArea()!=this->mListArea.get() && inListBox!=NULL)
        this->mListArea->SetScroller(this->mListBox.get());
}
// FUNCTION: WON 0x006f3ef0
// Adds the input box and button as children on first attachment, then does the container's work
void ComboBox::AddedToParent()
{
    if(this->mEdit->mParent!=this) this->AddControls();
    Container::AddedToParent();
}
// FUNCTION: WON 0x006f3f10
// Adds the input box and drop button, sets up the list area and button flags, and listens to the list box
void ComboBox::AddControls()
{
    this->AddChild(this->mEdit.get());
    this->AddChild(this->mButton.get());
    this->mListArea->SetListFlags(0x1000,true);
    this->mListArea->SetComponentFlags(ComponentFlag_WantFocus,false);
    this->mListBox->AddListener(new ComponentListenerComp(this));
    this->mButton->SetButtonFlags(0x40,true);
    this->mButton->SetComponentFlags(ComponentFlag_WantFocus,false);
}
// FUNCTION: WON 0x006f3fa0
// Sizes the hidden list box to fit its items, at least the combo's width and at most the configured
// list box width and height
void ComboBox::CalcListBoxSize()
{
    if(this->mListBoxVisible) return;
    int width=this->mListBoxWidth;
    int contentWidth=this->mListBox->Width()-this->mListArea->Width()+this->mListArea->GetRightPad()+
        this->mListArea->GetLeftPad()+this->mListArea->GetTotalWidth();
    if(width>contentWidth) width=contentWidth;
    if(width<this->Width()) width=this->Width();
    int height=this->mListArea->GetTotalHeight()+4;
    if(height>this->mListBoxHeight) height=this->mListBoxHeight;
    this->mListBox->SetSize(width,height);
}
// FUNCTION: WON 0x006f4010
// Handles a size change: does the container's work, then resizes the list box
void ComboBox::Virtual08() { Container::Virtual08(); this->CalcListBoxSize(); }
// FUNCTION: WON 0x006f4030
// Sets the maximum list box width and height and resizes the list box
void ComboBox::SetListBoxDimensions(int w,int h) { this->mListBoxWidth=w;this->mListBoxHeight=h;this->CalcListBoxSize(); }
// FUNCTION: WON 0x006f4050
// Sets the maximum list box width and resizes the list box
void ComboBox::SetListBoxWidth(int w) { this->mListBoxWidth=w;this->CalcListBoxSize(); }
// FUNCTION: WON 0x006f4070
// Sets the maximum list box height and resizes the list box
void ComboBox::SetListBoxHeight(int h) { this->mListBoxHeight=h;this->CalcListBoxSize(); }
// FUNCTION: WON 0x006f4090
// Takes focus and passes it to the input box when no child has focus
void ComboBox::GotFocus()
{
    Container::GotFocus();
    if(this->mFocusChild==NULL) this->mEdit->RequestFocus();
}
// FUNCTION: WON 0x006f40b0
// Loses focus (container behaviour only)
void ComboBox::LostFocus() { Container::LostFocus(); }
// FUNCTION: WON 0x006f40c0
// Copies the selected list item's text into the input box and selects it (cursor at the end when too wide),
// optionally firing a selection-changed event
void ComboBox::SetInputBoxToListSelection(bool inActive,bool inFireEvent)
{
    TextListItem* item=static_cast<TextListItem*>(this->mListArea->mSelItem);
    if(item==NULL) return;
    this->mEdit->SetText(item->mText,true);
    if(item->mWidth>this->mEdit->Width()-10) this->mEdit->SelectRange(item->mText.length(),0);
    else this->mEdit->SelectRange(0,-1);
    if(inFireEvent) this->DeliverEvent(new ComboSelChangedEvent(this,item,inActive),false);
}
// FUNCTION: WON 0x006f42b0
// Sets the input box text and redraws the combo box
void ComboBox::SetText(const GUIString& inText,bool inNotify)
{
    this->mEdit->SetText(inText,inNotify);
    this->Invalidate();
}
// FUNCTION: WON 0x006f42e0
// Selects the first list item whose text equals the string; sets the input text when none does
void ComboBox::SetTextAndSel(const GUIString& inText,bool inNotify)
{
    if(this->mListArea.get()!=NULL) {
        int count=this->mListArea->GetNumItems();
        for(int i=0;i<count;++i) {
            TextListItem* item=static_cast<TextListItem*>(this->mListArea->GetItem(i));
            if(item!=NULL && item->mText==inText) {
                this->SetSelItem(i,inNotify);
                return;
            }
        }
    }
    this->SetText(inText,inNotify);
}
// FUNCTION: WON 0x006f4370
// Opens the list box as a popup below the combo box, selecting and scrolling to the item that matches the
// input text (or clearing the selection when none does)
void ComboBox::ShowList()
{
    if(this->mListBoxVisible) return;
    this->CalcListBoxSize();
    Window* window=this->GetRoot();
    int x=0,y=this->Height();
    this->LocalToRoot(x,y);
    PopupParams params(this->mListBox.get(),0x2347,this,x,y);
    params.mInputComponent=this; // input component (+0x08)
    params.mAvoidRect.y=y-this->Height();
    params.mAvoidRect.height=this->Height();
    ListItem* selected=this->mListArea->mSelItem;
    if(selected!=NULL) {
        TextListItem* textItem=static_cast<TextListItem*>(selected);
        if(textItem->mText!=this->mEdit->GetText()) {
            int i=0;
            while(i<this->mListArea->GetNumItems()) {
                TextListItem* item=static_cast<TextListItem*>(this->mListArea->GetItem(i));
                if(item->mText==this->mEdit->GetText()) {
                    this->mListArea->SetSelItem(i,false);
                    this->mListArea->EnsureVisible(item);
                    break;
                }
                ++i;
            }
            if(i==this->mListArea->GetNumItems()) this->mListArea->SetSelItem(-1,false);
        } else this->mListArea->EnsureVisible(selected);
    }
    window->OpenPopup(params);
    this->mListBoxVisible=true;
}
// FUNCTION: WON 0x006f44e0
// Toggles the list on the drop button, takes a left-clicked list item and closes the list, notes the list
// closing, and re-fires input box events 20 and 21 as the combo box's own
void ComboBox::HandleComponentEvent(ComponentEvent* inEvent)
{
    if(inEvent->mComponent==this->mButton.get()) {
        if(!this->mListBoxVisible) {
            this->mUndoStr=this->mEdit->GetText();
            this->ShowList();
            return;
        }
        this->mListBoxVisible=false;
        this->GetRoot()->ClosePopup(this->mListBox.get());
        return;
    }
    if(inEvent->mType==ComponentEvent_ListItemClicked) {
        if(static_cast<ListItemClickedEvent*>(inEvent)->mButton!=0) return;
        this->SetInputBoxToListSelection(true,true);
        this->mListBoxVisible=false;
        this->GetRoot()->ClosePopup(this->mListBox.get());
        return;
    }
    if(inEvent->mType==ComponentEvent_EndPopup) {
        if(this->mListBoxVisible) this->mListBoxVisible=false;
        return;
    }
    if(inEvent->mType==ComponentEvent_InputTextChanged) {
        this->Fn2F5380(inEvent->mType,1);
        return;
    }
    if(inEvent->mType==ComponentEvent_InputReturn) {
        this->Fn2F5380(inEvent->mType,1);
        return;
    }
    Component::HandleComponentEvent(inEvent);
}
// FUNCTION: WON 0x006f4600
// Handles a mouse press; a left click on a no-typing input box presses the drop button, and any left click
// takes focus
void ComboBox::MouseDown(int x,int y,int button)
{
    Container::MouseDown(x,y,button);
    if(button==0) {
        if(this->mMouseChild==this->mEdit.get() && this->mNoType) this->mButton->Fn2F3440();
        this->RequestFocus();
    }
}
// FUNCTION: WON 0x006f4650
// Scrolls the open list with the mouse wheel; returns false when the list is closed
bool ComboBox::MouseWheel(int amount)
{
    return this->mListBoxVisible ? this->mListArea->MouseWheel(amount) : false;
}
// FUNCTION: WON 0x006f4680
// Handles keys: Down opens the list, arrows/page keys move the open list's selection, Enter takes it and
// Escape restores the earlier text (both closing the list); other keys go to the container
bool ComboBox::KeyDown(int key)
{
    if(key==40 && !this->mListBoxVisible) {
        this->mUndoStr=this->mEdit->GetText();
        this->SetInputBoxToListSelection(false,true);
        this->ShowList();
        return true;
    }
    if(this->mListBoxVisible && (key==40 || key==38 || key==33 || key==34)) {
        this->mListArea->KeyDown(key);
        this->SetInputBoxToListSelection(false,true);
        return true;
    }
    if(this->mListBoxVisible && key==13) {
        this->SetInputBoxToListSelection(true,true);
        this->mButton->Fn2F3440();
        return true;
    }
    if(key==27 && this->mListBoxVisible) {
        this->mEdit->SetText(this->mUndoStr,true);
        this->mButton->Fn2F3440();
        return true;
    }
    return Container::KeyDown(key);
}
// FUNCTION: WON 0x006f47a0
// Turns typing in the input box on or off, dropping input focus from it when typing is turned off
void ComboBox::SetNoType(bool inNoType)
{
    if(inNoType==this->mNoType) return;
    this->mNoType=inNoType;
    this->mEdit->SetComponentFlags(ComponentFlag_WantFocus,!inNoType);
    this->mEdit->SetAllowSelection(!inNoType);
    if(inNoType && this->mFocusChild==this->mEdit.get()) this->SetFocusChild(NULL);
}
// FUNCTION: WON 0x006f4800
// Enables or disables the combo box and its input box and button, closing an open list on disable and
// hiding the button when it is not shown while disabled
void ComboBox::SetEnabled(bool inEnabled)
{
    if(inEnabled==!this->IsDisabled()) return;
    Component::SetEnabled(inEnabled);
    this->mEdit->SetEnabled(inEnabled);
    this->mButton->SetEnabled(inEnabled);
    if(!inEnabled && this->mListBoxVisible) {
        this->mListBoxVisible=false;
        this->GetRoot()->ClosePopup(this->mListBox.get());
    }
    if(!this->mShowButtonWhenDisabled) {
        this->mButton->SetVisible(inEnabled);
        this->Relayout();
    }
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f3a30
// (public: virtual void * __thiscall WONAPI::ComboBox::`scalar deleting destructor'(unsigned int)    [??_GComboBox@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006f4250
// (public: virtual __thiscall WONAPI::ComponentEvent::~ComponentEvent(void)    [??1ComponentEvent@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f41a0
// (public: virtual void * __thiscall WONAPI::ComboSelChangedEvent::`scalar deleting destructor'(unsigned int)    [??_GComboSelChangedEvent@WONAPI@@UAEPAX)
// TEMPLATE: WON 0x006f41c0
// (public: virtual __thiscall WONAPI::ComboSelChangedEvent::~ComboSelChangedEvent(void)    [??1ComboSelChangedEvent@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f3970
// (public: __thiscall WONAPI::ComboBox::ComboBox(void)    [??0ComboBox@WONAPI@@QAE@XZ])
