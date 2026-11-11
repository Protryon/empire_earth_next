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
#include "rust/ee_rust.h"
#include "ee/wonapi/WONGUIDraw.h"
// The column at an x, the column resizes and positions, the dynamic column, the auto-grow test and the partner
// links are ee_won's guimulti module (rust/crates/ee_won/src/guimulti.rs); this unit keeps the columns' viewports,
// the rows and cells, sorting and the paint.
namespace WONAPI {
// FUNCTION: WON 0x007039c0
// Returns the first column whose right edge is at or past x (the last column when x is beyond all of them)
int MultiListArea::GetColumnAt(int x)
{
    std::vector<int> aXs,aWidths;
    for(size_t i=0;i<this->mColumnInfoVector.size();++i) {
        aXs.push_back(this->mColumnInfoVector[i].mx);
        aWidths.push_back(this->mColumnInfoVector[i].mWidth);
    }
    return ee_won_gui_ml_column_at(aXs.empty()?NULL:&aXs[0],aWidths.empty()?NULL:&aWidths[0],aXs.size(),x);
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
	size_t aCount = this->mColumnInfoVector.size();
	std::vector<int> aWidths(aCount), aPartners(aCount), aXs(aCount);
	std::vector<unsigned char> aModes(aCount);
	for(size_t i=0; i<aCount; i++)
	{
		aWidths[i] = this->mColumnInfoVector[i].mWidth;
		aPartners[i] = this->mColumnInfoVector[i].mPartnerColumn;
		aModes[i] = this->mColumnInfoVector[i].mSizeMode;
	}
	if(!ee_won_gui_ml_set_width(aCount?&aWidths[0]:NULL, aCount?&aModes[0]:NULL, aCount?&aPartners[0]:NULL,
	                            aCount?&aXs[0]:NULL, aCount, &this->mTotalColumnWidth, this->mMaxTotalColumnWidth,
	                            theCol, theWidth, fromSizeChanged))
		return;

	for(size_t i=0; i<aCount; i++)
	{
		this->mColumnInfoVector[i].mWidth = aWidths[i];
		ScrollAreaViewport *aContainer = this->mColumnInfoVector[i].mContainer.get();
		if(aContainer!=NULL)
			aContainer->SetPos(aXs[i],0);

		this->mColumnInfoVector[i].mx = aXs[i];
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
	int aSizeDelta = ee_won_gui_ml_dynamic_delta(this->mDynamicSizeColumn, this->mColumnInfoVector.size(),
	                                             this->mScrollArea.width, oldWidth, this->mTotalColumnWidth);

	if(aSizeDelta!=0)
		this->SetColumnWidth(this->mDynamicSizeColumn, this->mColumnInfoVector[this->mDynamicSizeColumn].mWidth + aSizeDelta,true);
}

// FUNCTION: WON 0x00703C70
// Paints the list once across all columns, then once per column clipped to that column
void MultiListArea::ScrollPaint(Graphics &g)
{
	// the rows once over all columns, then once per column clipped to it (guidraw.rs multi_list)
	std::vector<int> aWidths;
	for(size_t i=0; i<this->mColumnInfoVector.size(); i++)
		aWidths.push_back(this->mColumnInfoVector[i].mWidth);
	WONDrawOps anOps([&](EeWonDrawOp* o, size_t n) {
		return ee_won_gui_draw_multi_list(aWidths.empty() ? NULL : &aWidths[0],aWidths.size(),this->mColumnBorderWidth,
			this->mTotalColumnWidth,this->mVertOffset,this->mScrollArea.height,o,n); });
	WONDrawRun(g,anOps,WONDrawSlots(),[&](const EeWonDrawOp& o) {
		if(o.a==0)
		{
			this->mColumnPaintContext = o.b;
			this->mPaintWidth = o.c;
		}
		else
			ListArea::ScrollPaint(g);
	});
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
			if(anItem==NULL)
				continue;
			int aWidth = ee_won_gui_ml_grow(anItem->mWidth, this->mColumnBorderWidth, this->mColumnInfoVector[i].mWidth);
			if(aWidth>=0)
				this->SetColumnWidth(i,aWidth,false);
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

	if(!ee_won_gui_ml_relink((unsigned char)anOldMode, theMode))
		return;

	size_t aCount = this->mColumnInfoVector.size();
	std::vector<unsigned char> aModes(aCount);
	std::vector<int> aPartners(aCount);
	for(size_t i=0; i<aCount; i++)
	{
		aModes[i] = this->mColumnInfoVector[i].mSizeMode;
		aPartners[i] = this->mColumnInfoVector[i].mPartnerColumn;
	}
	ee_won_gui_ml_link_partners(aCount?&aModes[0]:NULL, aCount?&aPartners[0]:NULL, aCount);
	for(size_t i=0; i<aCount; i++)
		this->mColumnInfoVector[i].mPartnerColumn = aPartners[i];
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
