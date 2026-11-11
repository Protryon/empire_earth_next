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
#include "ee/wonapi/WONComponentFlags.h"
#include "ee/wonapi/WONGUIDraw.h"
namespace WONAPI {
// ListItem's out-of-line members precede ListArea in the original object: VC6 sees the ctor's body before
// `new ListItem` in ListArea::ListArea and drops the delete-on-throw guard (0x6fa570).
// The lines, their selection walk, the keys' targets and the scrolls inserts, refreshes and deletes leave are
// ee_won's guilist module (rust/crates/ee_won/src/guilist.rs); this unit keeps the list, its items, their sorting,
// measuring, painting and events. The Rust reads the items' tops and selectable flags through these:
static int32_t ListTopOf(const void* inItems, uintptr_t i)
{
    return (*static_cast<const std::vector<SmartPtr<ListItem> >*>(inItems))[i]->mTop;
}
static bool ListSelectableOf(const void* inItems, uintptr_t i)
{
    return (*static_cast<const std::vector<SmartPtr<ListItem> >*>(inItems))[i]->mSelectable;
}
// FUNCTION: WON 0x006fa280
// Creates an unselected, selectable list item with zero position and size
ListItem::ListItem()
{
    this->mTop=this->mWidth=this->mHeight=0;
    this->mSelected=false;
    this->mSelectable=true;
}
// FUNCTION: WON 0x006fa2a0
// Draws nothing
void ListItem::Paint(Graphics&,ListArea*) {}
// FUNCTION: WON 0x006fa2b0
// Orders two items by their addresses (true when this one comes first in memory)
int ListItem::Compare(const ListItem* other,ListArea*) const { return this<other; }
// FUNCTION: WON 0x006faa00
// Starts a batch of changes: layout and sorting wait until the matching EndUpdate
void ListArea::BeginUpdate() { ++this->mDoingMultiChange; }
// FUNCTION: WON 0x006faa10
// Ends a batch of changes; after the outermost one sorts the list, or just re-lays it out when unsorted
void ListArea::EndUpdate()
{
    if(--this->mDoingMultiChange<=0) {
        this->mDoingMultiChange=0;
        if(!this->Sort()) this->EnforceInvariants();
    }
}
struct ListItemTopLess {
    bool operator()(const SmartPtr<ListItem>& a,const SmartPtr<ListItem>& b) const { return a->mTop<b->mTop; }
};
// FUNCTION: WON 0x006facf0
// Returns the item whose line covers a vertical position (the one before when it falls between items)
std::vector<SmartPtr<ListItem> >::iterator ListArea::FindLineAt(int y)
{
    this->mSearchLine->mTop=y;
    return this->mItems.begin()+ee_won_gui_list_line_at(this->mItems.size(),y,ListTopOf,&this->mItems);
}
// FUNCTION: WON 0x006fad80
// Returns the item at a vertical position, or NULL when the position is below every item
ListItem* ListArea::FindItemAt(int y)
{
    std::vector<SmartPtr<ListItem> >::iterator i=this->FindLineAt(y);
    if(i==this->mItems.end()) return NULL;
    ListItem* item=i->get();
    return ee_won_gui_list_covers(item->mTop,item->mHeight,y) ? item : NULL;
}
// FUNCTION: WON 0x006fadc0
// Records the index of the item at the top of the visible area
void ListArea::FindTopLine()
{
    std::vector<SmartPtr<ListItem> >::iterator i=this->FindLineAt(this->mVertOffset);
    this->mTopLineVectorPos=i-this->mItems.begin();
}
// FUNCTION: WON 0x006faf10
// Returns whether the item is in view (only its top needs to be when partial lines are shown)
bool ListArea::IsItemVisible(ListItem* item)
{
    return ee_won_gui_list_visible(item->mTop,item->mHeight,this->mVertOffset,this->mScrollArea.height,
                                   this->ListFlagSet(0x80));
}
// FUNCTION: WON 0x006faf60
// Scrolls the list just enough to bring the item fully into view
void ListArea::EnsureVisible(ListItem* item)
{
    int offset=ee_won_gui_list_ensure_visible(item->mTop,item->mHeight,this->mVertOffset,this->mScrollArea.height);
    if(offset!=this->mVertOffset) this->SetVertOffset(offset);
}
// FUNCTION: WON 0x006fb200
// Makes the item the current selection (adding to it in multi-select when asked); on a change drops
// the focused child, fires the selection-changed event if asked and calls the hook; true when it changed
bool ListArea::SetSelItem(ListItem* item,bool add,bool fire)
{
    if(item==NULL) return this->ClearSelectionPrv(fire)>0;
    ListItem* old=this->mSelItem;
    if(item!=this->mSelItem) {
        if(this->mSelItem!=NULL && (!this->ListFlagSet(0x400) || !add)) {
            bool selected=item->mSelected;
            this->ClearSelectionPrv(false);
            item->mSelected=selected;
        }
        this->mSelItem=item;
    }
    if(this->mSelItem!=NULL && !this->mSelItem->mSelected) this->mSelItem=NULL;
    if(item!=old) this->Invalidate();
    if(this->mSelItem!=old) {
        this->Invalidate();
        if(this->mFocusChild!=NULL) { this->mFocusChild->LostFocus(); this->mFocusChild=NULL; }
        if(fire) this->Fn2F5380(ComponentEvent_ListSelChanged,0);
        this->ListSelChangedHook();
        return true;
    }
    return false;
}
// FUNCTION: WON 0x006fb2e0
// Selects every item between two indices (clamped to the list), making the first one current
void ListArea::SelectRange(int start,int end)
{
    this->ClearSelectionPrv(false);
    int step=1;
    if(!ee_won_gui_list_select_range(start,end,(int)this->mItems.size(),&start,&end,&step)) return;
    for(int i=start;i!=end+step;i+=step) this->GetItem(i)->mSelected=true;
    this->SetSelItem(this->GetItem(start),false,true);
}
// FUNCTION: WON 0x006fb410
// Selects the item at an index and makes it the keyboard item; an index out of range clears the selection
void ListArea::SetSelItem(int index,bool fire)
{
    if(index>=0 && index<(int)this->mItems.size()) {
        ListItem* item=this->mItems[index].get();
        item->mSelected=true;
        this->mKeyItem=item;
        this->SetSelItem(item,false,fire);
    } else {
        this->SetSelItem((ListItem*)NULL,false,fire);
        this->mKeyItem=NULL;
    }
}
// FUNCTION: WON 0x006fb480
// Restarts the walk over the selected items at the first one
void ListArea::RewindSelections() { this->mSelItemItr=-1; this->FindNextSelection(); }
// FUNCTION: WON 0x006fb490
// Returns whether the walk over the selected items has an item left
bool ListArea::HasMoreSelections() { return (unsigned int)this->mSelItemItr<this->mItems.size(); }
// FUNCTION: WON 0x006fb4d0
// Advances the walk over the selected items to the next selected one
void ListArea::FindNextSelection()
{
    ++this->mSelItemItr;
    while((unsigned int)this->mSelItemItr<this->mItems.size() && !this->mItems[this->mSelItemItr]->mSelected) ++this->mSelItemItr;
}
// FUNCTION: WON 0x006fb530
// Returns the next selected item of the walk and advances it, or NULL at the end
ListItem* ListArea::GetNextSelection()
{
    if(this->HasMoreSelections()) {
        ListItem* item=this->mItems[this->mSelItemItr].get();
        this->FindNextSelection();
        return item;
    }
    return NULL;
}
// FUNCTION: WON 0x006fb560
// Deselects every item (or the current one in single-select), firing the change if asked; returns how
// many were deselected
int ListArea::ClearSelectionPrv(bool fire)
{
    if(this->mSelItem==NULL) return 0;
    int count=0;
    if(this->ListFlagSet(0x400)) {
        for(std::vector<SmartPtr<ListItem> >::iterator i=this->mItems.begin();i!=this->mItems.end();++i)
            if((*i)->mSelected) { (*i)->mSelected=false; ++count; }
    } else { count=1; this->mSelItem->mSelected=false; }
    this->mSelItem=NULL;
    this->Invalidate();
    if(fire) { this->Fn2F5380(ComponentEvent_ListSelChanged,0); this->ListSelChangedHook(); }
    return count;
}
// FUNCTION: WON 0x006fb5f0
// Deselects every item, firing the selection-changed event if asked
void ListArea::ClearSelection(bool fire) { this->ClearSelectionPrv(fire); }
// FUNCTION: WON 0x006fc450
// Returns the index of the item in the list, or -1 when it is not there
int ListArea::GetItemPos(ListItem* item)
{
    if(item==NULL) return -1;
    std::vector<SmartPtr<ListItem> >::iterator i=this->FindLineAt(item->mTop);
    while(i!=this->mItems.end()) {
        if(i->get()==item) return i-this->mItems.begin();
        if((*i)->mTop>item->mTop) break;
        ++i;
    }
    return -1;
}
// FUNCTION: WON 0x006fc4b0
// Returns the index of the item at a vertical position
int ListArea::GetItemPosAtYPos(int y)
{
    std::vector<SmartPtr<ListItem> >::iterator itr=this->FindLineAt(y);
    return itr-this->mItems.begin();
}
// FUNCTION: WON 0x006fc4e0
// Returns the item at an index, or NULL when the index is out of range
ListItem* ListArea::GetItem(int index)
{
    if(index<0 || (unsigned int)index>=this->mItems.size()) return NULL;
    return this->mItems[index].get();
}
// FUNCTION: WON 0x006faa50
// Removes every item, clears the selection (firing the change if asked) and scrolls back to the top
void ListArea::Clear(bool fire)
{
    this->mTotalHeight=0;
    this->mTotalWidth=0;
    this->mHorzOffset=0;
    this->mVertOffset=0;
    this->ClearSelection(fire);
    this->mSelItem=NULL;
    this->mKeyItem=NULL;
    if(this->mMouseItem!=NULL) this->NotifyMouseItemGone();
    this->mItems.clear();
    this->AdjustPosition(false);
}
// FUNCTION: WON 0x006fc3c0
// Stacks the items top to bottom and recomputes the list's total size (skipped during a batch)
void ListArea::EnforceInvariants()
{
    if(this->mDoingMultiChange) return;
    size_t aCount=this->mItems.size();
    std::vector<int> aHeights(aCount), aWidths(aCount), aTops(aCount);
    for(size_t n=0;n<aCount;++n) {
        aHeights[n]=this->mItems[n]->mHeight;
        aWidths[n]=this->mItems[n]->mWidth;
    }
    ee_won_gui_list_stack(aCount?&aHeights[0]:NULL,aCount?&aWidths[0]:NULL,aCount?&aTops[0]:NULL,aCount,
                          &this->mTotalHeight,&this->mTotalWidth);
    for(size_t n=0;n<aCount;++n)
        this->mItems[n]->SetPos(0,aTops[n]);
    this->AdjustPosition(false);
}
// FUNCTION: WON 0x006fcad0
// Forgets the item under the mouse
void ListArea::NotifyMouseItemGone() { this->mMouseItem=NULL; }
// FUNCTION: WON 0x006fab20
// Returns the list's compare function, or the default one when none is set
ListArea::SortFunc ListArea::GetSortFunc() { return this->mSortFunc==NULL ? ListArea::DefaultSortFunc : this->mSortFunc; }
// FUNCTION: WON 0x006fab50
// Sorts the items by the sort type and re-lays them out; returns false for an unsorted list
bool ListArea::Sort()
{
    if(this->mSortType==ListSortType_None) return false;
    std::sort(this->mItems.begin(),this->mItems.end(),LineSortPred(this,this->GetSortFunc(),this->mSortType));
    this->EnforceInvariants();
    return true;
}
// FUNCTION: WON 0x006fabe0
// Returns where an item goes: its sorted place in a sorted list outside a batch, else the given
// position (the end when out of range)
int ListArea::FindInsertPos(ListItem* item,int position)
{
    if(!this->mDoingMultiChange && this->mSortType!=ListSortType_None)
    {
        std::vector<SmartPtr<ListItem> >::iterator itr=std::lower_bound(this->mItems.begin(),this->mItems.end(),item,LineSortPred(this,this->GetSortFunc(),this->mSortType));
        return itr-this->mItems.begin();
    }
    if(position<0 || (unsigned int)position>this->mItems.size()) position=this->mItems.size();
    return position;
}
// FUNCTION: WON 0x006fadf0
// Updates the scroll range; with full-line scrolling pads the end so the last page starts on an item
// and snaps the scroll position and steps to whole items
void ListArea::AdjustPosition(bool scrollbar)
{
    this->mExtraHeight=0;
    if(this->ListFlagSet(0x40) && this->mTotalHeight>this->mScrollArea.height) {
        std::vector<SmartPtr<ListItem> >::iterator i=this->FindLineAt(this->mTotalHeight-this->mScrollArea.height+1);
        if(i!=this->mItems.end()) {
            ListItem* item=i->get();
            this->mExtraHeight=ee_won_gui_list_full_line_extra(this->mTotalHeight,this->mScrollArea.height,item->mTop,
                                                               item->mHeight);
        }
    }
    ScrollArea::AdjustPosition(scrollbar);
    this->FindTopLine();
    if(!this->ListFlagSet(0x40) || (unsigned int)this->mTopLineVectorPos>=this->mItems.size())
        return;
    ListItem* item=this->mItems[this->mTopLineVectorPos].get();
    this->mVertOffset=item->mTop;
    this->mDownScrollAmount=item->mHeight;
    if(this->mTopLineVectorPos>0) this->mUpScrollAmount=this->mItems[this->mTopLineVectorPos-1]->mHeight;
    ScrollArea::AdjustPosition(true);
}
// FUNCTION: WON 0x006faef0
// Handles a size change of the list box and updates its scroll range
void ListArea::Virtual08() { ScrollArea::Virtual08();this->AdjustPosition(false); }
// an item's top and height for the draw plan
static EeWonRowPlace ListItemPlaceOf(const void* ctx,size_t index)
{
    const std::vector<SmartPtr<ListItem> >& items=*(const std::vector<SmartPtr<ListItem> >*)ctx;
    EeWonRowPlace r;
    r.x=0;
    r.top=items[index]->mTop;
    r.height=items[index]->mHeight;
    return r;
}
// FUNCTION: WON 0x006fafb0
// Draws the visible items from the top line down, at least as wide as the view
void ListArea::ScrollPaint(Graphics& g)
{
    // the paint width, then each visible item from the top line, translated to its top (guidraw.rs list_paint)
    int paintWidth=this->mPaintWidth;
    WONDrawOps ops([&](EeWonDrawOp* o,size_t n) { return ee_won_gui_draw_list(this->mTotalWidth,this->mScrollArea.width,
        this->mPaintWidth,(size_t)this->mTopLineVectorPos,this->mItems.size(),this->mVertOffset,this->mScrollArea.height,
        this->ListFlagSet(0x80),ListItemPlaceOf,&this->mItems,&paintWidth,o,n); });
    this->mPaintWidth=paintWidth;
    WONDrawRun(g,ops,WONDrawSlots(),[&](const EeWonDrawOp& o) { this->mItems[o.b]->Paint(g,this); });
    this->mPaintWidth=-1;
}
// FUNCTION: WON 0x006fb060
// Selects the item under the mouse: Ctrl toggles and Shift selects a range in multi-select, toggle
// lists flip the item; no item or an unselectable one clears the selection when selecting only on hover
void ListArea::CheckMouseSel(ListItem* item,int x,int y,bool down)
{
    if(item==NULL) { if(this->ListFlagSet(4)) this->ClearSelection(true);return; }
    if(!this->IsItemVisible(item)) return;
    if(!item->CanSelect(this,x,y-item->mTop)) { if(this->ListFlagSet(4)) this->ClearSelection(true);return; }
    this->mKeyItem=item;
    bool add=false;
    if(down && this->ListFlagSet(0x400)) {
        int modifiers=this->GetRoot()->GetKeyMod();
        if(modifiers&4) {
            add=true;item->mSelected=!item->mSelected;this->Invalidate();
        } else if((modifiers&1) && this->mSelItem!=NULL) {
            this->SelectRange(this->GetItemPos(this->mSelItem),this->GetItemPos(item));
            return;
        } else {
            bool noChange=this->mSelItem==item && this->ClearSelectionPrv(false)==1;
            item->mSelected=true;
            if(noChange) this->mSelItem=item;
        }
    } else if(this->ListFlagSet(2) || !this->ListFlagSet(8)) item->mSelected=true;
    else item->mSelected=!item->mSelected;
    this->SetSelItem(item,add,true);
}
// FUNCTION: WON 0x006fc0a0
// Finds the first selectable item from an index up or down, wrapping round or turning back when asked;
// NULL when there is none
ListItem* ListArea::FindSelectableItem(int start,bool up,bool opposite)
{
    // with the wrap flag and neither end selectable the original called itself without end; this finds none
    long long i=ee_won_gui_list_find_selectable(this->mItems.size(),start,up,opposite,this->ListFlagSet(0x800),
                                                ListSelectableOf,&this->mItems);
    return i<0 ? NULL : this->mItems[(size_t)i].get();
}
// FUNCTION: WON 0x006fc510
// Inserts an item at its place (sorted or given), sizes it and lays the list out, keeping the view on
// the bottom or on the same items; returns the item
ListItem* ListArea::InsertItem(ListItem* item,int position)
{
    position=this->FindInsertPos(item,position);
    item->SetViewport(this,this->GetViewport());
    item->CalcDimensions(this);
    if(item->mHeight<=0) item->mHeight=1;
    item->mSelected=false;
    bool end=(unsigned int)position==this->mItems.size();
    this->mItems.insert(this->mItems.begin()+position,SmartPtr<ListItem>(item));
    if(this->mSelItemItr>=position) ++this->mSelItemItr;
    if(end) {
        item->SetPos(0,this->mTotalHeight);
        this->mTotalHeight+=item->mHeight;
        if(item->mWidth>this->mTotalWidth) this->mTotalWidth=item->mWidth;
        this->mVertOffset=ee_won_gui_list_insert_scroll(true,this->ListFlagSet(0x1000),this->OnBottom(),
            this->mTopLineVectorPos,position,this->mTotalHeight,this->mScrollArea.height,this->mVertOffset,item->mHeight);
        this->AdjustPosition(false);
    } else {
        this->mVertOffset=ee_won_gui_list_insert_scroll(false,this->ListFlagSet(0x1000),this->OnBottom(),
            this->mTopLineVectorPos,position,this->mTotalHeight,this->mScrollArea.height,this->mVertOffset,item->mHeight);
        if(this->mDoingMultiChange) {
            item->mTop=ee_won_gui_list_batch_top(position!=0,position==0 ? 0 : this->mItems[position-1]->mTop,
                                                 this->mItems[position+1]->mTop);
        } else this->EnforceInvariants();
    }
    return item;
}
// FUNCTION: WON 0x006fcc30
// Inserts a text item with the given text; returns it
ListItem* ListArea::InsertString(const GUIString& text,int position)
{
    return this->InsertItem(new TextListItem(text),position);
}
// FUNCTION: WON 0x006fc730
// Re-measures an item after a change, moves it to its new sorted place when needed and re-lays the
// list out, keeping the view steady
void ListArea::RefreshItem(ListItem* item)
{
    int oldHeight=item->mHeight;
    item->SetViewport(this,this->GetViewport());
    item->CalcDimensions(this);
    if(item->mHeight<=0) item->mHeight=1;
    if(this->mDoingMultiChange)
        return;
    int position=this->GetItemPos(item);
    if(position>=0 && this->mSortType!=ListSortType_None) {
        LineSortPred less(this,this->GetSortFunc(),this->mSortType);
        ListItem* prev=this->GetItem(position-1);
        ListItem* next=this->GetItem(position+1);
        bool sort=false;
        if(prev!=NULL && less(item,prev)) sort=true;
        else if(next!=NULL && less(next,item)) sort=true;
        if(sort) {
            SmartPtr<ListItem> hold=item;
            if(prev!=NULL) this->mItems[position]=prev;
            else this->mItems[position]=next;
            int insert=ee_won_gui_list_refresh_place(position,this->FindInsertPos(item,0),this->mItems.size());
            if(position>insert) {
                for(int i=position;i>insert;--i) { this->mItems[i]=this->mItems[i-1]; }
            } else if(position<insert) {
                for(int i=position;i<insert;++i) { this->mItems[i]=this->mItems[i+1]; }
            }
            this->mItems[insert]=item;
        }
    }
    if(position<0)
        return;
    this->mVertOffset=ee_won_gui_list_refresh_scroll(this->ListFlagSet(0x1000),this->OnBottom(),item->mTop,
        this->mVertOffset,this->mTotalHeight,this->mScrollArea.Height(),item->mHeight,oldHeight);
    this->EnforceInvariants();
}
// FUNCTION: WON 0x006fca50
// Detaches the item at an index before removal: scrolls up past it and drops it as selection,
// keyboard item and mouse item
void ListArea::DeleteItemPrv(int position)
{
    ListItem* item=this->mItems[position].get();
    item->SetViewport(this,NULL);
    this->mVertOffset=ee_won_gui_list_delete_scroll(this->ListFlagSet(0x1000),item->mTop,this->mVertOffset,item->mHeight);
    if(item==this->mSelItem) { item->mSelected=false;this->SetSelItem(-1,true); }
    if(item==this->mKeyItem) this->mKeyItem=NULL;
    if(item==this->mMouseItem) this->NotifyMouseItemGone();
}
// FUNCTION: WON 0x006fcae0
// Removes the item at an index
void ListArea::DeleteItem(int position) { this->DeleteRange(position,position+1); }
// FUNCTION: WON 0x006fcb00
// Removes the item from the list if it is there
void ListArea::RemoveItem(ListItem* item)
{
    int position=this->GetItemPos(item);
    if(position>=0) this->DeleteItem(position);
}
// FUNCTION: WON 0x006fcb20
// Removes the items from start up to end (clamped to the list) and re-lays the list out
void ListArea::DeleteRange(int start,int end)
{
    int amount=0;
    if(!ee_won_gui_list_delete_range(start,end,this->mItems.size(),&start,&amount)) return;
    for(int i=0;i<amount;++i) this->DeleteItemPrv(start+i);
    if(this->mSelItemItr>=start) this->mSelItemItr-=amount;
    std::vector<SmartPtr<ListItem> >::iterator first=this->mItems.begin()+start;
    this->mItems.erase(first,first+amount);
    this->EnforceInvariants();
}
}

namespace WONAPI {
static bool ListMouseContains(ListItem* item,int x,int y,bool widthCheck) {
    if(!item->Contains(x,y)) return false;
    if(widthCheck && x>item->mWidth) return false;
    return true;
} // True when the point is on the item (and inside its width when asked); emitted at 0x006fb770
// FUNCTION: WON 0x006fb6a0
// Converts the point to list coordinates and updates the item under the mouse, telling the old one it
// left and the new one it was entered
void ListArea::ItemMouseAt(int &x, int &y)
{
	this->ScrollTranslate(x,y);
	bool itemWidthCheckOnSel = this->ListFlagSet(0x200);
	if(this->mMouseItem!=NULL && !ListMouseContains(this->mMouseItem,x,y,itemWidthCheckOnSel))
	{
		this->mMouseItem->MouseExit(this);
		this->SetCursor(this->GetDefaultCursor());
		this->NotifyMouseItemGone();
	}

	if(this->mMouseItem!=NULL)
		return;
	this->mMouseItem = this->FindItemAt(y);
	if(this->mMouseItem==NULL)
		return;
	if(ListMouseContains(this->mMouseItem,x,y,itemWidthCheckOnSel))
		this->mMouseItem->MouseEnter(this,x,y-this->mMouseItem->mTop);
	else
		this->mMouseItem = NULL;
}

// FUNCTION: WON 0x006fbc70
// Handles a drag: selects the item under the mouse when selecting on hover and passes the drag to it
void ListArea::MouseDrag(int x, int y)
{
	ScrollArea::MouseDrag(x,y);


	if(this->ListFlagSet(2))
	{
		this->ItemMouseAt(x,y);
		this->CheckMouseSel(this->mMouseItem,x,y);
	}
	else
		this->ScrollTranslate(x,y);

	if(this->mMouseItem!=NULL)
		this->mMouseItem->MouseDrag(this,x,y-this->mMouseItem->mTop);
}

// FUNCTION: WON 0x006fc160
// Moves the keyboard selection with the arrow and page keys (Shift extends it in multi-select,
// wrapping round when set) and scrolls it into view; left and right scroll sideways
bool ListArea::KeyDown(int theKey)
{
	if(ScrollArea::KeyDown(theKey))
		return true;

	if(this->mKeyItem==NULL)
	{
		if(theKey==VK_DOWN || theKey==VK_UP || theKey==VK_NEXT || theKey==VK_PRIOR)
		{
			ListItem *anItem = this->FindItemAt(this->mVertOffset);
			if(anItem==NULL)
				return false;

			int aPos = this->GetItemPos(anItem);
			anItem = this->FindSelectableItem(aPos,theKey==VK_UP || theKey==VK_PRIOR);
			if(anItem!=NULL)
			{
				this->mKeyItem = anItem;
				anItem->mSelected = true;
				this->SetSelItem(anItem,false,true);
			}

			return true;
		}
		return false;
	}


	int y = this->mKeyItem->mTop;
	int anItemPos = -1;
	bool up = false;

	if(theKey==VK_RIGHT) this->mHorzOffset+=10;
	else if(theKey==VK_LEFT) this->mHorzOffset-=10;
	// left and right look for no line (-1), unless the selection wraps: then -1 wraps to the end as the original's
	// did
	if(!ee_won_gui_list_key_target(theKey,y,this->mKeyItem->mHeight,this->mScrollArea.height,this->mTotalHeight,
		this->ListFlagSet(0x800),&anItemPos,&up))
		return false;

	if(anItemPos!=-1)
	{
		std::vector<SmartPtr<ListItem> >::iterator anItr = this->FindLineAt(anItemPos);
		if(anItr==this->mItems.end())
			return false;

		ListItem *anItem = anItr->get();
		int aPos = this->GetItemPos(anItem);
		anItem = this->FindSelectableItem(aPos,up);

		if(anItem!=NULL)
		{
			if(this->mSelItem!=NULL && this->ListFlagSet(0x400) && this->GetRoot()->GetKeyMod()&1)
			{
				this->SelectRange(this->GetItemPos(this->mSelItem),this->GetItemPos(anItem));
			}
			else
			{
				anItem->mSelected = true;
				this->SetSelItem(anItem,false,true);
			}

			this->mKeyItem = anItem;
			this->mVertOffset = ee_won_gui_list_key_scroll(anItem->mTop,anItem->mHeight,this->mVertOffset,
				this->mScrollArea.height,this->mExtraHeight);
		}
	}

	this->AdjustPosition(false);
	return true;
}

}

namespace WONAPI {
// FUNCTION: WON 0x006fb600
// Sets or clears list flags; turning on select-only-on-hover clears the selection when the mouse is
// outside the list
void ListArea::SetListFlags(unsigned long flags,bool on) {
    bool over=this->ListFlagSet(4);
    if(on) this->mListFlags|=flags;else this->mListFlags&=~flags;
    if(over || !this->ListFlagSet(4))
        return;
    Window* window=this->GetRoot();
    if(window==NULL) return;
    int x=window->mMouseX,y=window->mMouseY;
    Container* parent=this->mParent;
    if(parent!=NULL) {
        parent->RootToChild(x,y);
        if(!this->Contains(x,y)) this->ClearSelection();
    }
}
// FUNCTION: WON 0x006fbad0
// Tracks the item under the mouse (selecting it on hover when set) and shows a tooltip for an item
// wider than the list when item tooltips are on
void ListArea::MouseMove(int x,int y) {
    ScrollArea::MouseMove(x,y);
    this->ItemMouseAt(x,y);
    if(this->ListFlagSet(2)) this->CheckMouseSel(this->mMouseItem,x,y);
    if(this->mMouseItem!=NULL) this->mMouseItem->MouseMove(this,x,y-this->mMouseItem->mTop);
    if(!this->ListFlagSet(0x2000))
        return;
    bool show=false;
    if(this->mMouseItem!=NULL)
        show=ee_won_gui_list_item_tip(this->mMouseItem->mWidth,this->mMouseItem->mHeight,this->mMouseItem->mTop,
            this->mScrollArea.width,this->mVertOffset,this->mScrollArea.height);
    if(show) {
        if(this->mItemToolTip.get()==NULL) this->mItemToolTip=new ListAreaToolTip(this);
        this->mItemToolTip->DoTip(this->mMouseItem);
    } else if(this->mItemToolTip.get()!=NULL) this->mItemToolTip->DoTip(NULL);
}
// FUNCTION: WON 0x006fbd00
// Takes focus and selects the clicked item (except for button 1), then passes the press to the item and
// sends an item-clicked event
void ListArea::MouseDown(int x,int y,int button) {
    ScrollArea::MouseDown(x,y,button);
    this->ItemMouseAt(x,y);
    if(button!=1) {
        this->RequestFocus();
        this->CheckMouseSel(this->mMouseItem,x,y,true);
    }
    if(this->mMouseItem!=NULL) {
        this->mMouseItem->MouseDown(this,x,y-this->mMouseItem->mTop,button);
        this->DeliverEvent(new ListItemClickedEvent(this,this->mMouseItem,static_cast<MouseButton>(button),this->CheckDoubleClick(button),false,x,y-this->mMouseItem->mTop),false);
    }
}
// FUNCTION: WON 0x006fbef0
// Passes a button release to the item under the mouse and sends an item-released event
void ListArea::MouseUp(int x,int y,int button) {
    ScrollArea::MouseUp(x,y,button);
    this->ItemMouseAt(x,y);
    if(this->mMouseItem!=NULL) {
        this->mMouseItem->MouseUp(this,x,y-this->mMouseItem->mTop,button);
        this->DeliverEvent(new ListItemClickedEvent(this,this->mMouseItem,static_cast<MouseButton>(button),this->CheckDoubleClick(button),true,x,y-this->mMouseItem->mTop),false);
    }
}
// FUNCTION: WON 0x006fc010
// Handles the mouse entering the list box and finds the item under it
void ListArea::MouseEnter(int x,int y) {
    ScrollArea::MouseEnter(x,y);
    this->ItemMouseAt(x,y);
}
// FUNCTION: WON 0x006fc040
// Handles the mouse leaving: tells the item under it, clears a hover-only selection and hides the
// item tooltip
void ListArea::MouseExit() {
    ScrollArea::MouseExit();
    if(this->mMouseItem!=NULL) {
        this->mMouseItem->MouseExit(this);
        this->NotifyMouseItemGone();
    }
    if(this->ListFlagSet(4)) this->ClearSelection();
    if(this->ListFlagSet(0x2000) && this->mItemToolTip.get()!=NULL) this->mItemToolTip->DoTip(NULL);
}
}

namespace WONAPI {
// FUNCTION: WON 0x006fa570
// Creates an empty, unsorted list box with the ListArea font, 2-pixel padding and default flags
// (highlighted icons, full-row select, partial lines shown) that takes focus
ListArea::ListArea()
{
    this->mSearchLine=new ListItem;
    this->mTopLineVectorPos=0;
    this->mListFlags=0xb0;
    this->mSortType=ListSortType_None;
    this->mTextColor=(int)0x8000000e;
    this->mFont=this->GetNamedFont("ListArea");
    this->mSelItem=NULL;
    this->mKeyItem=NULL;
    this->mMouseItem=NULL;
    this->SetComponentFlags(ComponentFlag_WantFocus,true);
    this->mTopPad=this->mLeftPad=this->mRightPad=this->mBottomPad=2;
    this->mPaintWidth=-1;
    this->mDoingMultiChange=0;
    this->mSortFunc=NULL;
}
// FUNCTION: WON 0x006fa860
// Destroys the list box, closing an open item tooltip first
ListArea::~ListArea()
{
    if(this->mItemToolTip.get()!=NULL) this->mItemToolTip->EndTip();
}
}

// FUNCTION: WON 0x006fa550
// Returns whether the clicked item is the list box's current selection
bool WONAPI::ListItemClickedEvent::IsSelectedItem()
{
    return this->mItem.get()==static_cast<ListArea*>(this->mComponent)->GetSelItem();
}

namespace WONAPI {

void ListArea::ListSelChangedHook()
{
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006fa770
// (public: virtual void * __thiscall WONAPI::ListArea::`scalar deleting destructor'(unsigned int)    [??_GListArea@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006fa790
// (public: virtual __thiscall WONAPI::ScrollArea::~ScrollArea(void)    [??1ScrollArea@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006fbe40
// (public: virtual void * __thiscall WONAPI::ListItemClickedEvent::`scalar deleting destructor'(unsigned int)    [??_GListItemClickedEvent@WONAPI@@UAEPAX)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006fab30
// (public: static int __cdecl WONAPI::ListArea::DefaultSortFunc(class WONAPI::ListArea *,class WONAPI::ListItem const *,class WONAPI::ListItem const *)  )

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006fb770
// (bool __cdecl WONAPI::ListMouseContains(class WONAPI::ListItem *,int,int,bool)    [?ListMouseContains@WONAPI@@YA_NPAVListItem@1@HH_N@Z])
// TEMPLATE: WON 0x0071d2b0
// (public: __thiscall std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,class WONAPI::Smart)
// TEMPLATE: WON 0x006fbe60
// (ListItemClickedEvent implicit destructor)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071b3e0
// (public: class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,class WONAPI::SmartPtr<c)
// TEMPLATE: WON 0x0071c5c0
// (public: void __thiscall std::vector<class WONAPI::SmartPtr<class WONAPI::ListItem>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::ListItem>)
// TEMPLATE: WON 0x00723a90
// (void __cdecl std::sort(class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,class WON)
// TEMPLATE: WON 0x00724000
// (void __cdecl std::fill(class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,class WON)
// TEMPLATE: WON 0x00725180
// (void __cdecl std::_Insertion_sort(class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> )
// TEMPLATE: WON 0x007251c0
// (void __cdecl std::_Sort(class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,class WO)
// TEMPLATE: WON 0x007252f0
// (void __cdecl std::_Unguarded_insertion_sort(class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::)
// TEMPLATE: WON 0x007257a0
// (void __cdecl std::_Insertion_sort_1(class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem)
// TEMPLATE: WON 0x00725970
// (void __cdecl std::make_heap(class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,clas)
// TEMPLATE: WON 0x007259d0
// (void __cdecl std::sort_heap(class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,clas)
// TEMPLATE: WON 0x00725c90
// (void __cdecl std::_Unguarded_insert(class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem)
// TEMPLATE: WON 0x00725e50
// (void __cdecl std::_Make_heap(class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,cla)
// TEMPLATE: WON 0x00725ee0
// (void __cdecl std::pop_heap(class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,class)
// TEMPLATE: WON 0x00725fc0
// (void __cdecl std::_Adjust_heap(class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,c)
// TEMPLATE: WON 0x00726110
// (void __cdecl std::_Pop_heap_0(class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,cl)
// TEMPLATE: WON 0x007262b0
// (void __cdecl std::_Push_heap(class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,cla)
// TEMPLATE: WON 0x007263d0
// (void __cdecl std::_Pop_heap(class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,clas)
// TEMPLATE: WON 0x00723ba0
// (class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,class WONAPI::SmartPtr<class WON)
// TEMPLATE: WON 0x00725360
// (class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,class WONAPI::SmartPtr<class WON)
// TEMPLATE: WON 0x00725dc0
// (void __cdecl std::_Distance(class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,clas)
// TEMPLATE: WON 0x00725f20
// (void __cdecl std::iter_swap(class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,clas)
// TEMPLATE: WON 0x007261b0
// (void __cdecl std::_Iter_swap(class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,cla)
// TEMPLATE: WON 0x007212d0
// (public: class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,class WONAPI::SmartPtr<c)
// TEMPLATE: WON 0x00723ca0
// (class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,class WONAPI::SmartPtr<class WON)
// TEMPLATE: WON 0x00725960
// (public: class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,class WONAPI::SmartPtr<c)
// TEMPLATE: WON 0x00725fa0
// (void __cdecl std::_Distance(class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,clas)
// TEMPLATE: WON 0x007264d0
// (public: int __thiscall std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,class WONAPI::S)
// TEMPLATE: WON 0x00725630
// (class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,class WONAPI::SmartPtr<class WON)
// TEMPLATE: WON 0x00725a40
// (class std::_Ptrit<class WONAPI::SmartPtr<class WONAPI::ListItem>,int,class WONAPI::SmartPtr<class WONAPI::ListItem> *,class WONAPI::SmartPtr<class WON)

#include "ee/wonapi/WONLobbyComponentListItem.h"
#include "ee/wonapi/WONLobbyMSConfig.h"
namespace WONAPI {
// FUNCTION: WON 0x006fd8b0
// Adds a separator line to the popup menu and resizes the menu to fit (at least 100 pixels wide)
void PopupMenu::AddSeperator()
{
    this->InsertItem(new ComponentListItem(new MSSeperator(6),true,false));
    this->SetSizeEqualToTotal(100,-1);
}
}
// TEMPLATE: WON 0x006fda80
// (protected: virtual void * __thiscall WONAPI::ComponentListItem::`scalar deleting destructor'(unsigned int)    [??_GComponentListItem@WONAPI@@MAEPAXI@Z)
