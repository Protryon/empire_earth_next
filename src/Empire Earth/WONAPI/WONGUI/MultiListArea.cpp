// Multi-column list area: column geometry, sorting, cell setters and per-column painting (file name inferred)
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
namespace WONAPI {
// FUNCTION: WON 0x007039c0
// Returns the first column whose right edge is at or past x (the last column when x is beyond all of them)
int MultiListArea::GetColumnAt(int x)
{
    for(int i=0;i<this->mColumnInfoVector.size();++i)
        if(this->mColumnInfoVector[i].mx+this->mColumnInfoVector[i].mWidth>=x) return i;
    return (int)this->mColumnInfoVector.size()-1;
}
// FUNCTION: WON 0x00703960
// Returns a column's width, or 0 for an invalid column
int MultiListArea::GetColumnWidth(int column)
{
    if(column<0 || column>=this->mColumnInfoVector.size()) return 0;
    return this->mColumnInfoVector[column].mWidth;
}
// FUNCTION: WON 0x00703a60
// Returns a column's left position, or -1 for an invalid column
int MultiListArea::GetColumnPos(int column)
{
    if(column<0 || column>=this->mColumnInfoVector.size()) return -1;
    return this->mColumnInfoVector[column].mx;
}
}

namespace WONAPI {

// FUNCTION: WON 0x007037E0
// Resizes a column (taking the width from its partner column, or capped by the maximum total width),
// then repositions every column and updates the header and scrolling
void MultiListArea::SetColumnWidth(int theCol, int theWidth, bool fromSizeChanged)
{
	if(theCol >= this->mColumnInfoVector.size())
		return;
	if(theWidth<0)
		theWidth = 0;

	ColumnInfo &aCol = this->mColumnInfoVector[theCol];
	if(!fromSizeChanged && aCol.mSizeMode==2 && aCol.mPartnerColumn>=0 && aCol.mPartnerColumn<this->mColumnInfoVector.size())
	{
		ColumnInfo &aCol2 = this->mColumnInfoVector[aCol.mPartnerColumn];
		int anOldWidth = aCol.mWidth;
		int aDelta = theWidth - anOldWidth;
		if(aDelta>aCol2.mWidth)
			aDelta = aCol2.mWidth;

		aCol.mWidth += aDelta;
		aCol2.mWidth -= aDelta;
	}
	else
	{
		int anOldWidth = aCol.mWidth;

		int aDelta = theWidth - anOldWidth;
		if(this->mMaxTotalColumnWidth>0 && aDelta>0 && this->mTotalColumnWidth+aDelta>this->mMaxTotalColumnWidth)
		{
			aDelta = this->mMaxTotalColumnWidth - this->mTotalColumnWidth;
			if(aDelta<0)
				aDelta = 0;
		}

		aCol.mWidth += aDelta;
		this->mTotalColumnWidth += aDelta;
	}

	int aTotalWidth = 0;
	for(int i=0; i<this->mColumnInfoVector.size(); i++)
	{
		ScrollAreaViewport *aContainer = this->mColumnInfoVector[i].mContainer.get();
		if(aContainer!=NULL)
			aContainer->SetPos(aTotalWidth,0);

		this->mColumnInfoVector[i].mx = aTotalWidth;
		aTotalWidth += this->mColumnInfoVector[i].mWidth;
	}

	this->NotifyListHeader();
	this->AdjustPosition(false);
}

// FUNCTION: WON 0x00703AC0
// Sorts the list by a column and direction, remembering the previous sort column and updating the header
void MultiListArea::SetSortColumn(int theCol, ListSortType theType)
{
	if(theCol<0 || theCol>=this->mColumnInfoVector.size())
		return;

	this->mSortColumnPrev = this->mSortColumn;
	this->mSortColumn = theCol;
	this->mSortType = theType;
	this->mSortColFunc = this->mColumnInfoVector[theCol].mSortFunc;
	if(this->mListHeader!=NULL)
		this->mListHeader->SetSortColumn(theCol,theType);

	this->Sort();
}

// FUNCTION: WON 0x00703b50
// Sets a column's sort comparison function, applying it at once if that column is the sort column
void MultiListArea::SetSortColumnFunc(int theCol, SortFunc theFunc)
{
	if(theCol<0 || theCol>=this->mColumnInfoVector.size())
		return;

	this->mColumnInfoVector[theCol].mSortFunc = theFunc;
	if(this->mSortColumn==theCol)
		this->mSortColFunc = theFunc;
}

// FUNCTION: WON 0x00703bb0
// Updates the scroll extent to the total column width and tells the header the columns changed
void MultiListArea::AdjustPosition(bool becauseOfScrollbar)
{
	this->mTotalWidth = this->mTotalColumnWidth;
	ListArea::AdjustPosition(becauseOfScrollbar);
	this->NotifyListHeader();
}

// FUNCTION: WON 0x00703BE0
// After a resize, grows or shrinks the dynamic-size column by the width change, without overflowing the view
void MultiListArea::Virtual08()
{
	int oldWidth = this->mScrollArea.width;

	ListArea::Virtual08();
	if (!(this->mDynamicSizeColumn >= 0) || !(this->mDynamicSizeColumn < (int)this->mColumnInfoVector.size()))
		return;
	int aSizeDelta = this->mScrollArea.width - oldWidth;
	if(this->mTotalColumnWidth + aSizeDelta > this->mScrollArea.width)
	{
		aSizeDelta = this->mScrollArea.width - this->mTotalColumnWidth;
		if(aSizeDelta<0)
			aSizeDelta = 0;
	}

	if(aSizeDelta!=0)
		this->SetColumnWidth(this->mDynamicSizeColumn, this->mColumnInfoVector[this->mDynamicSizeColumn].mWidth + aSizeDelta,true);
}

// FUNCTION: WON 0x00703C70
// Paints the list once across all columns, then once per column clipped to that column
void MultiListArea::ScrollPaint(Graphics &g)
{
	int anXPos = 0;
	

	// Give chance to paint over all columns
	this->mColumnPaintContext = -1;
	this->mPaintWidth = this->mTotalColumnWidth;
	ListArea::ScrollPaint(g);

	// Now paint each column
	for(int i=0; i<this->mColumnInfoVector.size(); i++)
	{		
		ColumnInfo &aCol = this->mColumnInfoVector[i];
		this->mColumnPaintContext = i;
		this->mPaintWidth = aCol.mWidth-this->mColumnBorderWidth;

		g.PushClipRect(anXPos,this->mVertOffset,this->mPaintWidth,this->mScrollArea.height);
		g.Translate(anXPos,0);
		ListArea::ScrollPaint(g);
		g.Translate(-anXPos,0);
		g.PopClipRect();


	
		anXPos += aCol.mWidth;
	}
}

// FUNCTION: WON 0x00703E50
// Refreshes a row and, with auto-grow on, widens any column too narrow for the row's cell; returns the row
MultiListItem* MultiListArea::UpdateItem(MultiListItem *theItem)
{
	ListArea::RefreshItem(theItem);
	if(this->mAutoGrowColumns)
	{
		for(int i=0; i<this->mColumnInfoVector.size(); i++)
		{
			ListItem *anItem = theItem->GetItem(i);
			if(anItem==NULL || !(anItem->mWidth>0))
				continue;
			int aWidth = this->mColumnInfoVector[i].mWidth;
			if(anItem->mWidth + this->mColumnBorderWidth > aWidth)
				this->SetColumnWidth(i,anItem->mWidth + this->mColumnBorderWidth,false);
		}
	}
	return theItem;
}

// FUNCTION: WON 0x00703EE0
// Puts an item into a row's column and refreshes the row; returns the row, or NULL if row or column is invalid
MultiListItem* MultiListArea::SetCell(int theRow, int theCol, ListItem *theCellItem)
{
	if(theCol >= (int)this->mColumnInfoVector.size())
		return NULL;

	MultiListItem* aRow = static_cast<MultiListItem*>(ListArea::GetItem(theRow));
	if(aRow==NULL)
		return NULL;

	aRow->SetItem(theCellItem, theCol);
	this->UpdateItem(aRow);
	return aRow;
}

}

namespace WONAPI {
// FUNCTION: WON 0x00703600
// Sets the number of columns, creating a viewport for each new one, and recomputes the total width
void MultiListArea::SetNumColumns(int numColumns)
{
    this->mColumnInfoVector.resize(numColumns);
    this->mTotalColumnWidth=0;
    for(int i=0;i<this->mColumnInfoVector.size();++i) {
        ScrollAreaViewport* container=this->mColumnInfoVector[i].mContainer.get();
        if(container==NULL) {
            container=new ScrollAreaViewport(this);
            this->mColumnInfoVector[i].mContainer=container;
            this->GetViewport()->AddChild(container);
        }
        this->mTotalColumnWidth+=this->mColumnInfoVector[i].mWidth;
    }
    this->NotifyListHeader();
    this->AdjustPosition(false);
}
}

namespace WONAPI {
// FUNCTION: WON 0x00703f60
// Puts a text cell (list's font and color, optional image) into a row's column; returns the row
MultiListItem* MultiListArea::SetString(int row,int column,const GUIString& text,Image* image)
{
    SmartPtr<TextListItem> item=new TextListItem;
    item->mText=text;
    item->mColor=this->mTextColor;
    item->mFont=this->mFont;
    item->mImage=image;
    return this->SetCell(row,column,item.get());
}
// FUNCTION: WON 0x007035f0
// Tells the attached header, if any, that the columns changed
void MultiListArea::NotifyListHeader()
{
    if(this->mListHeader!=NULL) this->mListHeader->NotifyColumnChange();
}
// FUNCTION: WON 0x00703e30
// Inserts a row at a position and returns it
MultiListItem* MultiListArea::InsertItem(MultiListItem* item,int position)
{
    ListArea::InsertItem(item,position);
    return item;
}
// FUNCTION: WON 0x00703790
// Sets a column's horizontal alignment (ignored for an invalid column)
void MultiListArea::SetColumnHAlign(int column,HorzAlign align)
{
    if(column<0 || column>=this->mColumnInfoVector.size()) return;
    this->mColumnInfoVector[column].mHAlign=static_cast<unsigned char>(align);
}
}

namespace WONAPI {
// Sets a column's size mode and links each paired-resize (mode 2) column to the next one; InsertRow below
// inserts an empty row (neither is identified in the original exe)
void MultiListArea::SetColumnSizeMode(int theCol, unsigned char theMode)
{
	if(theCol<0 || theCol >= this->mColumnInfoVector.size())
		return;
	
	int anOldMode = this->mColumnInfoVector[theCol].mSizeMode;
	this->mColumnInfoVector[theCol].mSizeMode = theMode;	

	if(theMode!=2 && anOldMode!=2)
		return;

	int aPrevPartner = -1;
	for(int i=0; i<this->mColumnInfoVector.size(); i++)
	{
		ColumnInfo &anInfo = this->mColumnInfoVector[i];
		if(anInfo.mSizeMode!=2)
			continue;
		if(aPrevPartner>=0)
			this->mColumnInfoVector[aPrevPartner].mPartnerColumn = i;

		aPrevPartner = i;
	}
}

MultiListItem* MultiListArea::InsertRow(int theRow)
{
	return this->InsertItem(new MultiListItem, theRow);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00706a40
// (public: virtual void * __thiscall WONAPI::ScrollAreaViewport::`scalar deleting destructor'(unsigned int)    [??_GScrollAreaViewport@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00706a60
// (public: virtual __thiscall WONAPI::ScrollAreaViewport::~ScrollAreaViewport(void)    [??1ScrollAreaViewport@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x00724b10
// (void __cdecl std::fill(class std::_Ptrit<struct WONAPI::MultiListArea::ColumnInfo,int,struct WONAPI::MultiListArea::ColumnInfo *,struct WONAPI::MultiL)
// TEMPLATE: WON 0x00724ba0
// (class std::_Ptrit<struct WONAPI::MultiListArea::ColumnInfo,int,struct WONAPI::MultiListArea::ColumnInfo *,struct WONAPI::MultiListArea::ColumnInfo &,s)
// TEMPLATE: WON 0x00724c40
// (struct WONAPI::MultiListArea::ColumnInfo * __cdecl std::copy(class std::_Ptrit<struct WONAPI::MultiListArea::ColumnInfo,int,struct WONAPI::MultiListAr)
// TEMPLATE: WON 0x00724f10
// (void __cdecl std::_Construct(struct WONAPI::MultiListArea::ColumnInfo *,struct WONAPI::MultiListArea::ColumnInfo const &)    [?_Construct@std@@YAXPAUC)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00703d60
// (public: virtual bool __thiscall WONAPI::ListItem::Virtual1C(int,int)    [?Virtual1C@ListItem@WONAPI@@UAE_NHH@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00703d90
// (public: virtual void * __thiscall WONAPI::MultiListItem::`scalar deleting destructor'(unsigned int)    [??_GMultiListItem@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00724ad0
// (protected: struct WONAPI::MultiListArea::ColumnInfo * __thiscall std::vector<struct WONAPI::MultiListArea::ColumnInfo,class std::allocator<struct WONA)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00703db0
// (public: virtual __thiscall WONAPI::MultiListItem::~MultiListItem(void))
// TEMPLATE: WON 0x0071d060
// (public: void __thiscall std::vector<struct WONAPI::MultiListArea::ColumnInfo,class std::allocator<struct WONAPI::MultiListArea::ColumnInfo> >::resize()
// TEMPLATE: WON 0x007212e0
// (public: void __thiscall std::vector<struct WONAPI::MultiListArea::ColumnInfo,class std::allocator<struct WONAPI::MultiListArea::ColumnInfo> >::insert()
// TEMPLATE: WON 0x007217a0
// (public: class std::_Ptrit<struct WONAPI::MultiListArea::ColumnInfo,int,struct WONAPI::MultiListArea::ColumnInfo *,struct WONAPI::MultiListArea::Column)
// TEMPLATE: WON 0x007238e0
// (public: class std::_Ptrit<struct WONAPI::MultiListArea::ColumnInfo,int,struct WONAPI::MultiListArea::ColumnInfo *,struct WONAPI::MultiListArea::Column)
// TEMPLATE: WON 0x00725160
// (public: class std::_Ptrit<struct WONAPI::MultiListArea::ColumnInfo,int,struct WONAPI::MultiListArea::ColumnInfo *,struct WONAPI::MultiListArea::Column)
