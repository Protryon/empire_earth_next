// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// FUNCTION: WON 0x00715030
// Sets how the tree's items are sorted and re-sorts when it changes
void TreeArea::SetSortType(ListSortType theType)
{
	if(this->mTreeSortType==theType)
		return;

	this->mTreeSortType = theType;
	this->Sort();
}

// FUNCTION: WON 0x00715060
// Forgets the hovered item and the item whose expand button the mouse is over
void TreeArea::NotifyMouseItemGone()
{
	ListArea::NotifyMouseItemGone();
	this->mMouseColItem = NULL;
}

// FUNCTION: WON 0x00715080
// Wraps a list item in a tree item and inserts it under a parent (the root when none) at a position;
// returns the tree item
TreeItem* TreeArea::InsertTreeItem(TreeItem *theParent, ListItem *theItem, int thePos)
{
	if(theParent==NULL)
		theParent = this->mRoot.get();

	SmartPtr<TreeItem> anItem = new TreeItem;
	anItem->mItem = theItem;
	anItem->mSelectable = theItem->mSelectable;
	theParent->InsertChild(this, anItem.get(),thePos);
	return anItem.get();
}

// FUNCTION: WON 0x00715190
// Removes a tree item from its parent
void TreeArea::RemoveTreeItem(TreeItem *theItem)
{
	theItem->RemoveFromParent(this);
}

// FUNCTION: WON 0x007151a0
// Removes every item from the tree (the event flag is ignored)
void TreeArea::ClearTree(bool fireEventOnChange)
{
	this->RemoveAllItems(NULL);
}

// FUNCTION: WON 0x007151b0
// Removes a parent's children, or with no parent clears the whole list and the root's children
void TreeArea::RemoveAllItems(TreeItem *theParent)
{
	if(theParent==NULL)
	{
		ListArea::Clear(true);
		this->mRoot->mChildren.clear();
	}
	else
		theParent->RemoveAllChildren(this);
}

// FUNCTION: WON 0x00715220
// Expands a tree item, optionally sending an event
void TreeArea::OpenItem(TreeItem *theItem, bool sendEvent)
{
	theItem->SetOpen(this, true, false, sendEvent);
}

// FUNCTION: WON 0x00715240
// Expands or collapses a tree item (collapsing its subtree too when closeAll is set)
void TreeArea::ToggleItem(TreeItem *theItem, bool closeAll, bool sendEvent)
{
	theItem->ToggleOpen(this, closeAll, sendEvent);
}

// FUNCTION: WON 0x00715260
// Handles a mouse press as the list area does
void TreeArea::MouseDown(int x, int y, int theButton)
{
	ListArea::MouseDown(x,y,theButton);	
}

// FUNCTION: WON 0x00715280
// Toggles the item next to a pressed expand button (control id 100003); passes other events to the
// list area
void TreeArea::HandleComponentEvent(ComponentEvent *theEvent)
{
	if(theEvent->mType==ComponentEvent_ButtonPressed && theEvent->mComponent->mControlId==100003)
	{
			TreeItem* anItem = static_cast<TreeItem*>(this->FindItemAt(theEvent->mComponent->Top()));
			if(anItem!=NULL)
				this->ToggleItem(anItem,this->TreeFlagSet(4));
	}
	else
		ListArea::HandleComponentEvent(theEvent);

}

bool TreeArea::Sort() { return false; }
}

namespace WONAPI {
// FUNCTION: WON 0x00714ea0
// Creates an empty tree with an open root item, list flags 0x30 off and 0x200 on, and tree flags 3
TreeArea::TreeArea()
{
    this->mRoot=new TreeItem;
    this->mRoot->SetOpen(this,true,false,false);
    this->SetListFlags(0x30,false);
    this->SetListFlags(0x200,true);
    this->mTreeFlags=3;
    this->mMouseColItem=NULL;
}
}

namespace WONAPI {
// Tree sorting, string items, closing and tree flags (not identified in the original exe)
void TreeArea::Sort(TreeItem *theItem, bool recursive)
{
	if(this->mTreeSortType==ListSortType_None)
		return;

	if(theItem==NULL)
	{
		// Do Root
		ListArea::Clear(false);
		this->mRoot->Sort(this,recursive);
		this->mRoot->InsertChildren(this,0);
		return;
	}
		
	bool isVisible = theItem->IsOpen() && this->GetItemPos(theItem)>=0;

	// if the item open and visible then remove the children, sort them and then reinsert them
	if(isVisible)
		theItem->SetOpen(this,false,false,false);

	theItem->Sort(this, recursive);

	if(isVisible)
		theItem->SetOpen(this,true,false,false);
}

TreeItem* TreeArea::InsertString(TreeItem *theParent, const GUIString &theStr, Image *theIcon, int thePos)
{
	SmartPtr<TextListItem> anItem = new TextListItem;
	anItem->mText = theStr;
	anItem->mColor = this->mTextColor;
	anItem->mFont = this->mFont;	
	anItem->mImage = theIcon;
	return this->InsertTreeItem(theParent, anItem.get(), thePos);
}

void TreeArea::CloseItem(TreeItem *theItem, bool closeAll, bool sendEvent)
{
	theItem->SetOpen(this, false, closeAll, sendEvent); 
}

void TreeArea::SetTreeFlags(int theFlags, bool on)
{
	if(on)
		this->mTreeFlags |= theFlags;
	else
		this->mTreeFlags &= ~theFlags;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00714fa0
// (public: virtual void * __thiscall WONAPI::TreeArea::`scalar deleting destructor'(unsigned int)    [??_GTreeArea@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00714fc0
// (public: virtual __thiscall WONAPI::TreeArea::~TreeArea(void)    [??1TreeArea@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071cc20
// (public: class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::TreeItem>,int,class WONAPI::SmartPtr<class WONAPI::TreeItem> *,class WONAPI::SmartPtr<c)
// TEMPLATE: WON 0x00723d30
// (class WONAPI::SmartPtr<class WONAPI::TreeItem> * __cdecl std::copy(class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::TreeItem>,int,class WONAPI::)
