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
static void DrawHorzDottedLine(Graphics &g, int x, int y, int length)
{
	DWORD aColor = g.mColor;
	int i=0;
	if(x&1)
		i++;

	for(; i<length; i+=2)
		g.SetPixel(x+i,y,aColor);
}

static void DrawVertDottedLine(Graphics &g, int x, int y, int length)
{
	DWORD aColor = g.mColor;
	int i=0;
	if(y&1)
		i++;

	for(; i<length; i+=2)
		g.SetPixel(x,y+i,aColor);
}

// FUNCTION: WON 0x00714210
// Tracks which wrapped item is under the mouse (exit/enter as it changes), shifts x by the indent and returns it;
// after an exit it enters the new item only on the next call
ListItem* TreeItem::GetMouseItem(ListArea *theListArea, int &x, int &y)
{
	TreeArea *aTree = static_cast<TreeArea*>(theListArea);
	ListItem *aNewMouseItem = this->mItem.get();
	ListItem *aMouseColItem = aTree->mMouseColItem;
	if(aMouseColItem!=NULL && (aMouseColItem!=aNewMouseItem || !aMouseColItem->Contains(x,y+this->mTop)))
	{
		aMouseColItem->MouseExit(theListArea);
		theListArea->SetCursor(NULL);

		aTree->mMouseColItem = NULL;
	}


	if(aMouseColItem==NULL)
	{
		if(aNewMouseItem!=NULL && aNewMouseItem->Contains(x,y+this->mTop))
		{
			aTree->mMouseColItem = aNewMouseItem;
			aNewMouseItem->MouseEnter(theListArea,x,y);
		}
	}

	x-=this->mX;
	return aTree->mMouseColItem;

}

// FUNCTION: WON 0x007142c0
// Updates the item under the mouse when the mouse enters the tree row
void TreeItem::MouseEnter(ListArea *theListArea, int x, int y)
{
	this->GetMouseItem(theListArea,x,y);
}

// FUNCTION: WON 0x007142e0
// Sends mouse-exit to the wrapped item under the mouse and clears the tree's mouse item (not the column item)
void TreeItem::MouseExit(ListArea *theListArea)
{
	ListItem *anItem = static_cast<TreeArea*>(theListArea)->mMouseColItem;
	if(anItem!=NULL)
	{
		anItem->MouseExit(theListArea);
		static_cast<TreeArea*>(theListArea)->mMouseItem = NULL;
	}
}

// FUNCTION: WON 0x00714310
// Forwards a mouse move to the wrapped item under the mouse
void TreeItem::MouseMove(ListArea *theListArea, int x, int y)
{
	ListItem *anItem = this->GetMouseItem(theListArea,x,y);
	if(anItem!=NULL)
		anItem->MouseMove(theListArea,x,y);
}

// FUNCTION: WON 0x00714340
// Forwards a mouse drag to the wrapped item under the mouse, shifted by the indent
void TreeItem::MouseDrag(ListArea *theListArea, int x, int y)
{
	TreeArea *aList = static_cast<TreeArea*>(theListArea);
	ListItem *anItem = aList->mMouseColItem;
	if(anItem!=NULL)
		anItem->MouseDrag(theListArea,x-this->mX,y);
}

// FUNCTION: WON 0x00714370
// Forwards a mouse release to the wrapped item under the mouse
void TreeItem::MouseUp(ListArea *theListArea, int x, int y, int theButton)
{	
	ListItem *anItem = this->GetMouseItem(theListArea,x,y);
	if(anItem!=NULL)
		anItem->MouseUp(theListArea,x,y,theButton);
}

// FUNCTION: WON 0x007143b0
// Toggles the item open on a left click on its +/- box (or a double click when the tree allows it); otherwise
// forwards the press to the wrapped item under the mouse
void TreeItem::MouseDown(ListArea *theListArea, int x, int y, int theButton)
{
	if(theButton==0)
	{
		TreeArea *aTreeArea = static_cast<TreeArea*>(theListArea);
		if((x < this->mX && x > this->mX-14) || (theListArea->CheckDoubleClick(theButton) && aTreeArea->TreeFlagSet(8)))
		{
			this->ToggleOpen(aTreeArea, aTreeArea->TreeFlagSet(4));
			return;
		}
	}

	ListItem *anItem = this->GetMouseItem(theListArea,x,y);
	if(anItem!=NULL)
		anItem->MouseDown(theListArea,x,y,theButton);
}

// FUNCTION: WON 0x00714440
// Returns the child at the given position, or NULL when out of range
TreeItem* TreeItem::GetChild(int thePos)
{
	if(thePos<0 || thePos>=this->mChildren.size())
		return NULL;

	return this->mChildren[thePos].get();
}

// FUNCTION: WON 0x00714470
// Returns the next item after this subtree (next sibling, else the parent's next), or NULL at the end
TreeItem* TreeItem::GetNextNonChild()
{
	if(this->mParent==NULL)
		return NULL;
	else if(this->mIndex>=this->mParent->mChildren.size()-1)
		return this->mParent->GetNextNonChild();
	else
		return this->mParent->mChildren[this->mIndex+1].get();
}

// FUNCTION: WON 0x007144b0
// Returns the list row of the next item after this subtree, or -1
int TreeItem::GetNextNonChildPos(TreeArea *theTreeArea)
{
	TreeItem *anItem = this->GetNextNonChild();
	if(anItem!=NULL)
		return theTreeArea->GetItemPos(anItem);
	else
		return -1;
}

// FUNCTION: WON 0x007144d0
// Returns where a new child goes: the given position when unsorted, else its sorted position among the children
int TreeItem::GetInsertPos(TreeArea* tree,TreeItem* item,int position) {
    if(tree->mTreeSortType==ListSortType_None) return position;
    std::vector<SmartPtr<TreeItem> >::iterator itr=std::lower_bound(this->mChildren.begin(),this->mChildren.end(),item,ListArea::LineSortPred(tree,tree->GetSortFunc(),tree->mTreeSortType));
    return itr-this->mChildren.begin();
}

// FUNCTION: WON 0x00714570
// Inserts a child (sorted when the tree sorts), sets its indent and the children's indices, and adds it to the
// list rows when this item is open
void TreeItem::InsertChild(TreeArea *theTreeArea, TreeItem *theItem, int thePos)
{
	thePos = this->GetInsertPos(theTreeArea,theItem,thePos);
	if(this->mParent==NULL && !theTreeArea->TreeFlagSet(2 | 1))
		theItem->mX = 0;
	else
		theItem->mX = this->mX + 15;

	if(thePos<0 || thePos>this->mChildren.size())
		thePos = this->mChildren.size();

	bool onEnd = thePos==this->mChildren.size();

	this->mChildren.insert(this->mChildren.begin() + thePos, SmartPtr<TreeItem>(theItem));
	theItem->mParent = this;

	for(int i=thePos; i<this->mChildren.size(); i++)
	{
		TreeItem *anItem = this->mChildren[i].get();
		anItem->mIndex = i;
	}

	if(!this->IsOpen())
		return;
	int aPos;
	if(onEnd)
		aPos = this->GetNextNonChildPos(theTreeArea);
	else
		aPos = theTreeArea->GetItemPos(this->mChildren[thePos+1].get());

	theTreeArea->ListArea::InsertItem(theItem, aPos);
}

// FUNCTION: WON 0x00714710
// Closes the item, drops all its children and repaints the tree
void TreeItem::RemoveAllChildren(TreeArea *theTreeArea)
{
	this->SetOpen(theTreeArea,false,false,false);
	this->mChildren.clear();
	theTreeArea->Invalidate();
}

// FUNCTION: WON 0x00714770
// Closes and removes a child (and its rows when this item is open), renumbers the rest and repaints when none remain
void TreeItem::RemoveChild(TreeArea *theTreeArea, TreeItem *theItem)
{
	theTreeArea->BeginUpdate();

	if(theItem->IsOpen())
		theItem->SetOpen(theTreeArea, false,false,false);

	if(this->IsOpen())
		theTreeArea->ListArea::RemoveItem(theItem);

	this->mChildren.erase(this->mChildren.begin() + theItem->mIndex);
	for(int i=0; i<this->mChildren.size(); i++)
		this->mChildren[i]->mIndex = i;

	if(this->mChildren.empty())
		theTreeArea->Invalidate(); // need to do this in order to redraw the button (don't need button anymore)

	theTreeArea->EndUpdate();

}

// FUNCTION: WON 0x00714830
// Removes this item from its parent, if it has one
void TreeItem::RemoveFromParent(TreeArea *theTreeArea)
{
	if(this->mParent==NULL)
		return;

	this->mParent->RemoveChild(theTreeArea, this);	
}

// FUNCTION: WON 0x00714850
// Marks this item and its whole subtree closed (rows are not touched)
void TreeItem::CloseAll()
{
	this->mOpen = false;
	std::vector<SmartPtr<TreeItem> >::iterator anItr = this->mChildren.begin();
	while(anItr!=this->mChildren.end())
	{
		(*anItr)->CloseAll();
		++anItr;
	}
}

// FUNCTION: WON 0x00714880
// Adds the rows of the children (and of open grandchildren) at the given list position
void TreeItem::InsertChildren(TreeArea *theTreeArea, int thePos)
{
	for(int i=this->mChildren.size()-1; i>=0; i--)
	{
		TreeItem *anItem = this->mChildren[i].get();
		theTreeArea->ListArea::InsertItem(anItem,thePos);
		if(anItem->IsOpen())
			anItem->InsertChildren(theTreeArea, thePos+1);
	}
}

// FUNCTION: WON 0x007148e0
// Opens or closes the item: adds or deletes its children's rows, scrolls so the opened subtree shows, optionally
// closes the whole subtree and sends an item-opened event
void TreeItem::SetOpen(TreeArea *theTreeArea, bool isOpen, bool closeAll, bool sendEvent)
{
	if(this->mOpen==isOpen)
		return;

	this->mOpen = isOpen;

	if(this->mChildren.empty())
		return;

	if(!isOpen)
	{
		int aPos1 = theTreeArea->GetItemPos(this->mChildren.front().get());
		int aPos2 = this->GetNextNonChildPos(theTreeArea);
		theTreeArea->DeleteRange(aPos1, aPos2);
		if(closeAll)
			this->CloseAll();
	}
	else
	{
		theTreeArea->BeginUpdate();
		this->InsertChildren(theTreeArea, theTreeArea->GetItemPos(this)+1);
		theTreeArea->EndUpdate();

		int aSiblingYPos = theTreeArea->mTotalHeight;
		TreeItem *anItem = this->GetNextNonChild();
		if(anItem!=NULL)
			aSiblingYPos = anItem->mTop;

		int aVertOffset = theTreeArea->mVertOffset;
		if(aSiblingYPos > aVertOffset + theTreeArea->mScrollArea.height)
		{
			aVertOffset = aSiblingYPos - theTreeArea->mScrollArea.height;
			if(this->mTop < aVertOffset)
				aVertOffset = this->mTop;

			theTreeArea->SetVertOffset(aVertOffset);
		}		
	}

	if(sendEvent)
		theTreeArea->DeliverEvent(new TreeItemOpenedEvent(theTreeArea,this),false);
}

// FUNCTION: WON 0x00714af0
// Opens the item when closed and closes it when open
void TreeItem::ToggleOpen(TreeArea *theTreeArea, bool closeAll, bool sendEvent)
{
	this->SetOpen(theTreeArea, !this->mOpen, closeAll, sendEvent);
}

// FUNCTION: WON 0x00714b10
// Returns whether the item is its parent's last child (false for the root)
bool TreeItem::IsLastChild()
{
	return this->mParent!=NULL && this->mIndex>=this->mParent->mChildren.size()-1;
}

// FUNCTION: WON 0x00714b40
// Moves the row and places the wrapped item at the indent
void TreeItem::SetPos(int x, int y)
{
	ListItem::SetPos(x,y);
	this->mItem->SetPos(this->mX,y);
}

// FUNCTION: WON 0x00714b60
// Draws the row: dotted connector lines and the +/- box when the tree shows them, then the wrapped item at the indent
void TreeItem::Paint(Graphics &g, ListArea *theListArea)
{
	TreeArea *aTreeArea = static_cast<TreeArea*>(theListArea);

	bool drawButton = aTreeArea->TreeFlagSet(2) && !this->mChildren.empty();

	int squareLeft		= this->mX - 14;
	int squareRight		= squareLeft+8;
	int squareTop		= (this->mHeight - 8)/2;
	int squareBottom	= squareTop+8;

	int lineTop = squareTop;
	int lineBottom = squareBottom;
	int lineLeft = squareRight;
	if(!drawButton)
	{
		lineTop = lineBottom = squareTop+4;
		lineLeft = squareLeft+6;
	}

	if(static_cast<TreeArea*>(theListArea)->TreeFlagSet(1))
	{
		g.SetSysColor(StandardColor_3DShadow);
		if(this->mTop!=0)
			DrawVertDottedLine(g,squareLeft+4,0,lineTop+1);

		if(!this->IsLastChild())
			DrawVertDottedLine(g,squareLeft+4,lineBottom,this->mHeight-lineBottom);

		DrawHorzDottedLine(g,lineLeft,squareTop+4,this->mX-lineLeft);
		TreeItem *anItem = this->mParent;
		while(anItem!=NULL && anItem->mX > 0)
		{
			if(!anItem->IsLastChild())
				DrawVertDottedLine(g,anItem->mX-10,0,this->mHeight);

			anItem = anItem->mParent;
		}
	}

	if(drawButton)
	{
		g.SetSysColor(StandardColor_3DShadow);
		g.DrawRect(squareLeft,squareTop,9,9);
		g.SetSysColor(StandardColor_Text);
		g.DrawLine(squareLeft+2, squareTop+4, squareRight-2, squareTop+4);
		if(!this->mOpen)
			g.DrawLine(squareLeft+4, squareTop+2, squareLeft+4, squareBottom-2);
	}

	g.Translate(this->mX,0);//mTop);
	this->mItem->mSelected = this->mSelected;
	this->mItem->Paint(g, theListArea);
	g.Translate(-this->mX,0);//-mTop);
}

// FUNCTION: WON 0x00714dd0
// Passes the viewport on to the wrapped item
void TreeItem::SetViewport(ListArea *theListArea, Container *theViewport)
{
	this->mItem->SetViewport(theListArea, theViewport);
}

// FUNCTION: WON 0x00714df0
// Sizes the row as the wrapped item plus the indent
void TreeItem::CalcDimensions(ListArea *theListArea)
{
	this->mItem->CalcDimensions(theListArea);
	this->mWidth = this->mX+this->mItem->mWidth;
	this->mHeight = this->mItem->mHeight;
}

// FUNCTION: WON 0x00714e20
// Compares two tree items by their wrapped items
int TreeItem::Compare(const ListItem *theItem, ListArea *theListArea) const
{
	return this->mItem->Compare(((TreeItem*)theItem)->mItem.get(), theListArea);
}

// FUNCTION: WON 0x00714e40
// Returns whether the item is selectable and the point is right of the indent
bool TreeItem::CanSelect(ListArea *theListArea, int x, int y)
{
	return this->mSelectable && x>=this->mX;
}

// FUNCTION: WON 0x00714e60
// Returns whether the point lies on the wrapped item (right of the indent)
bool TreeItem::ContainsPoint(int x, int y)
{
	if(x<this->mX)
		return false;

	return this->mItem->Contains(x-this->mX,y+this->mTop);
}

}

namespace WONAPI {
// Sorts the children (and optionally every subtree) and renumbers them (not identified in the original exe)
void TreeItem::Sort(TreeArea *theTreeArea, bool recursive)
{
	if(this->mChildren.empty())
		return;

	std::sort(this->mChildren.begin(), this->mChildren.end(), ListArea::LineSortPred(theTreeArea, theTreeArea->GetSortFunc(), theTreeArea->mTreeSortType));

	std::vector<SmartPtr<TreeItem> >::iterator anItr = this->mChildren.begin();
	int i=0;
	while(anItr!=this->mChildren.end())
	{
		TreeItem *anItem = anItr->get();
		anItem->mIndex = i++;
	
		if(recursive)
			anItem->Sort(theTreeArea,recursive);
		
		++anItr;
	}
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00714d30
// (void __cdecl WONAPI::DrawHorzDottedLine(class WONAPI::Graphics &,int,int,int)    [?DrawHorzDottedLine@WONAPI@@YAXAAVGraphics@1@HHH@Z])
// TEMPLATE: WON 0x00714d80
// (void __cdecl WONAPI::DrawVertDottedLine(class WONAPI::Graphics &,int,int,int)    [?DrawVertDottedLine@WONAPI@@YAXAAVGraphics@1@HHH@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00714a40
// (public: virtual void * __thiscall WONAPI::TreeItemOpenedEvent::`scalar deleting destructor'(unsigned int)    [??_GTreeItemOpenedEvent@WONAPI@@UAEPAXI@)
// TEMPLATE: WON 0x00714a60
// (public: virtual __thiscall WONAPI::TreeItemOpenedEvent::~TreeItemOpenedEvent(void)    [??1TreeItemOpenedEvent@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071b880
// (public: class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::TreeItem>,int,class WONAPI::SmartPtr<class WONAPI::TreeItem> *,class WONAPI::SmartPtr<c)
// TEMPLATE: WON 0x0071d500
// (public: void __thiscall std::vector<class WONAPI::SmartPtr<class WONAPI::TreeItem>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::TreeItem>)
// TEMPLATE: WON 0x00723f80
// (class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::TreeItem>,int,class WONAPI::SmartPtr<class WONAPI::TreeItem> *,class WONAPI::SmartPtr<class WON)
