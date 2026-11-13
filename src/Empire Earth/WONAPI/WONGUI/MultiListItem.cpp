// Multi-column list row: per-column cells, sorting, painting and mouse routing (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONGUIDraw.h"
namespace WONAPI {
// FUNCTION: WON 0x7026e0
// Sizes every cell; the row is as tall as its tallest cell and as wide as all columns together
void MultiListItem::CalcDimensions(ListArea* inList)
{
    this->mHeight=0;
    for(ItemVector::iterator anItr=this->mItems.begin();anItr!=this->mItems.end();++anItr) {
        ListItem* anItem=anItr->get();
        if(anItem==NULL)
            continue;
        anItem->CalcDimensions(inList);
        int aHeight=anItem->mHeight;
        if(aHeight>this->mHeight) this->mHeight=aHeight;
    }
    this->mWidth=static_cast<MultiListArea*>(inList)->mTotalColumnWidth;
}
// FUNCTION: WON 0x702740
// Compares two rows by their sort-column cells (missing cells first), with the column's sort function if set
int MultiListItem::Compare(const ListItem* inOther,ListArea* inList) const
{
    MultiListArea* anArea=static_cast<MultiListArea*>(inList);
    int aColumn=anArea->GetSortColumn();
    ListItem* anItem=this->GetItem(aColumn);
    ListItem* anOther=static_cast<const MultiListItem*>(inOther)->GetItem(aColumn);
    if(anItem==anOther) return 0;
    if(anItem==NULL) return -1;
    if(anOther==NULL) return 1;
    if(anArea->mSortColFunc!=NULL) return anArea->mSortColFunc(inList,anItem,anOther);
    return anItem->Compare(anOther,inList);
}
// FUNCTION: WON 0x702910
// Sets the row's top and moves every cell to that height at its column's left edge
void MultiListItem::SetPos(int inX,int inY)
{
    this->mTop=inY;
    for(ItemVector::iterator anItr=this->mItems.begin();anItr!=this->mItems.end();++anItr) {
        ListItem* anItem=anItr->get();
        if(anItem!=NULL) anItem->SetPos(0,inY);
    }
}
// FUNCTION: WON 0x007027b0
// Returns the cell in the column under x, making x relative to that column
ListItem* MultiListItem::GetMouseItem(ListArea* inList,int& ioX)
{
    MultiListArea* anArea=static_cast<MultiListArea*>(inList);
    int aColumn=anArea->GetColumnAt(ioX);
    ioX-=anArea->GetColumnPos(aColumn);
    return this->GetItem(aColumn);
}
// FUNCTION: WON 0x7027f0
// Updates which cell the mouse hovers (exit/enter notifications, cursor reset) and returns the hovered cell
ListItem* MultiListItem::GetMouseItem(ListArea* inList,int& ioX,int& ioY)
{
    MultiListArea* anArea=static_cast<MultiListArea*>(inList);
    int aColumn=anArea->GetColumnAt(ioX);
    ioX-=anArea->GetColumnPos(aColumn);
    ListItem* anItem=this->GetItem(aColumn);
    ListItem* anOld=anArea->mMouseColItem;
    if(anOld!=NULL) {
        if(anOld!=anItem || !anOld->Contains(ioX,ioY+this->mTop)) {
            anOld->MouseExit(inList);
            anArea->SetCursor(NULL);
            anArea->mMouseColItem=NULL;
        }
    } else if(anItem!=NULL && anItem->Contains(ioX,ioY+this->mTop)) {
        anArea->mMouseColItem=anItem;
        anArea->mMouseColNum=aColumn;
        anItem->MouseEnter(inList,ioX,ioY);
    }
    return anArea->mMouseColItem;
}
// FUNCTION: WON 0x702530
// Paints the selection bar in the all-columns pass, else the current column's cell aligned in its column
// (cells other than the first show selection only with full-row selection)
void MultiListItem::Paint(Graphics& inGraphics,ListArea* inList)
{
    MultiListArea* anArea=static_cast<MultiListArea*>(inList);
    int aColumn=anArea->mColumnPaintContext;
    // the selection bar in the all-columns pass, else the column's cell at its alignment (guidraw.rs multi_row);
    // the cell is read only where the column is one of the row's
    ListItem* anItem=static_cast<unsigned int>(aColumn)<this->mItems.size() ? this->mItems[aColumn].get() : NULL;
    EeWonMultiRowPaint r;
    r.column=aColumn;
    r.selected=this->mSelected;
    r.full_row_select=anArea->ListFlagSet(0x10);
    r.has_cell=anItem!=NULL;
    r.cell_count=(uint32_t)this->mItems.size();
    r.selection_color=(uint32_t)anArea->GetSelColor().GetBackgroundColor(inGraphics);
    r.total_width=anArea->mTotalColumnWidth;
    r.height=this->mHeight;
    r.align=anItem!=NULL ? anArea->mColumnInfoVector[aColumn].mHAlign : 0;
    r.paint_width=anArea->GetPaintWidth();
    r.cell_width=anItem!=NULL ? anItem->mWidth : 0;
    WONDrawOps anOps([&](EeWonDrawOp* o,size_t n) { return ee_won_gui_draw_multi_row(&r,o,n); });
    WONDrawRun(inGraphics,anOps,WONDrawSlots(),[&](const EeWonDrawOp& o) {
        if(o.a==0) {
            anItem->mHeight=o.b;
            anItem->mSelected=o.c!=0;
        } else
            anItem->Paint(inGraphics,inList);
    });
}
// FUNCTION: WON 0x702650
// Gives each cell its column's viewport, or detaches them all when the viewport is NULL
void MultiListItem::SetViewport(ListArea* inList,Container* inViewport)
{
    unsigned int aColumn=0;
    for(ItemVector::iterator anItr=this->mItems.begin();anItr!=this->mItems.end();aColumn++,++anItr) {
        std::vector<MultiListArea::ColumnInfo>& aVec=static_cast<MultiListArea*>(inList)->mColumnInfoVector;
        if(anItr->get()!=NULL && aColumn<aVec.size()) {
            Container* aViewport=inViewport==NULL ? NULL : aVec[aColumn].mContainer.get();
            (*anItr)->SetViewport(inList,aViewport);
        }
    }
}
// FUNCTION: WON 0x7028d0
// Returns whether the row can be selected at a point: not when unselectable, else as the cell there says
bool MultiListItem::CanSelect(ListArea* inList,int inX,int inY)
{
    if(!this->mSelectable) return false;
    ListItem* anItem=this->GetMouseItem(inList,inX);
    if(anItem!=NULL) return anItem->CanSelect(inList,inX,inY);
    return true;
}
// FUNCTION: WON 0x00702950
// Updates the hovered cell when the mouse enters the row
void MultiListItem::MouseEnter(ListArea* inList,int inX,int inY)
{
    this->GetMouseItem(inList,inX,inY);
}
// The original clears the list's hovered row (+0x154), not the hovered cell (+0x194)
// FUNCTION: WON 0x00702970
// Tells the hovered cell the mouse left and clears the list's hovered row (the hovered cell pointer stays set)
void MultiListItem::MouseExit(ListArea* inList)
{
    MultiListArea* anArea=static_cast<MultiListArea*>(inList);
    if(anArea->mMouseColItem!=NULL) {
        anArea->mMouseColItem->MouseExit(inList);
        inList->ClearMouseItem();
    }
}
// FUNCTION: WON 0x007029a0
// Updates the hovered cell and passes the move to it
void MultiListItem::MouseMove(ListArea* inList,int inX,int inY)
{
    ListItem* anItem=this->GetMouseItem(inList,inX,inY);
    if(anItem!=NULL) anItem->MouseMove(inList,inX,inY);
}
// FUNCTION: WON 0x007029d0
// Passes a drag to the cell where it started, relative to that cell's column
void MultiListItem::MouseDrag(ListArea* inList,int inX,int inY)
{
    MultiListArea* anArea=static_cast<MultiListArea*>(inList);
    ListItem* anItem=anArea->mMouseColItem;
    if(anItem!=NULL) anItem->MouseDrag(inList,inX-anArea->GetColumnPos(anArea->mMouseColNum),inY);
}

// FUNCTION: WON 0x00702a10
// Updates the hovered cell and passes the button press to it
void MultiListItem::MouseDown(ListArea* inList,int inX,int inY,int inButton)
{
    ListItem* anItem=this->GetMouseItem(inList,inX,inY);
    if(anItem!=NULL) anItem->MouseDown(inList,inX,inY,inButton);
}
// FUNCTION: WON 0x00702a50
// Updates the hovered cell and passes the button release to it
void MultiListItem::MouseUp(ListArea* inList,int inX,int inY,int inButton)
{
    ListItem* anItem=this->GetMouseItem(inList,inX,inY);
    if(anItem!=NULL) anItem->MouseUp(inList,inX,inY,inButton);
}
// No negative-index guard: the original compares the vector size with the index converted to unsigned
// FUNCTION: WON 0x00702480
// Puts a cell into a column, growing the row's cell list as needed
void MultiListItem::SetItem(ListItem* inItem,int inColumn)
{
    if(this->mItems.size()<=inColumn) this->mItems.resize(inColumn+1);
    this->mItems[inColumn]=inItem;
}
}
namespace WONAPI {
// Appends a cell as the next column (not identified in the original exe)
void MultiListItem::InsertItem(ListItem *theItem)
{
	this->mItems.push_back(theItem);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071c4b0
// (public: void __thiscall std::vector<class WONAPI::SmartPtr<class WONAPI::ListItem>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::ListItem>)
// TEMPLATE: WON 0x0071b3c0
// (public: void __thiscall std::vector<class WONAPI::SmartPtr<class WONAPI::ListItem>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::ListItem>)
