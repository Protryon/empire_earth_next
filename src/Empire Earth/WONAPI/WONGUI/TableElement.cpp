// Table element (HTML-style table in a text view): column and row sizing, cell spans, painting, selection text
// and search (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTextSegment.h"
#include "ee/wonapi/WONLobbyTableElement.h"
namespace WONAPI {


// FUNCTION: WON 0x0070dec0
// FUNCTION: WON 0x0070df10
// Returns the info of the cell in a column of a row record, or NULL when out of range; the second overload
// below (0x0070dec0 and 0x0070df10 are the two) looks the row up by index
TableElement::CellInfo* TableElement::GetCellInfoAt(RowInfo &theInfo, int theCol)
{
	if(theCol<0 || theCol>=theInfo.mRow.size())
		return NULL;

	return &(theInfo.mRow[theCol]);
}


TableElement::CellInfo* TableElement::GetCellInfoAt(int theRow, int theCol)
{
	if(theRow<0 || theRow>=this->mRowVector.size() || theCol<0 || theCol>=this->mColumnVector.size())
		return NULL;
	
	RowInfo &anInfo = this->mRowVector[theRow];
	if(theCol>=anInfo.mRow.size())
		return NULL;

	return &(anInfo.mRow[theCol]);
}


// FUNCTION: WON 0x0070dfc0
// Returns the contents of the cell at a row and column, or NULL (an overload taking a row record follows)
MultiLineElement* TableElement::GetCellAt(int theRow, int theCol)
{
	CellInfo *anInfo = this->GetCellInfoAt(theRow, theCol);
	if(anInfo==NULL)
		return NULL;

	return anInfo->mContents.get();
}


MultiLineElement* TableElement::GetCellAt(RowInfo &theInfo, int theCol)
{
	CellInfo *anInfo = this->GetCellInfoAt(theInfo,theCol);
	if(anInfo==NULL)
		return NULL;
	else
		return anInfo->mContents.get();
}


// FUNCTION: WON 0x0070e320
// Returns the index of the row containing a vertical position (the last row starting above it)
int TableElement::FindRowAt(int theYPos)
{
	this->mSearchRow.my = theYPos;
	RowVector::iterator anItr = std::lower_bound(this->mRowVector.begin(), this->mRowVector.end(), this->mSearchRow, RowSearchPred());
	if(anItr==this->mRowVector.end() || anItr->my>theYPos)
	{
		if(anItr!=this->mRowVector.begin())
			--anItr;
	}

	return anItr - this->mRowVector.begin();
}


// FUNCTION: WON 0x0070eca0
// Returns the table's fixed width if set, otherwise its minimum width (computed on first use)
int TableElement::GetMinWidth() 
{ 
	if(this->mMinWidth==0)
		this->CalcMinMaxWidths();

	if(this->mWidthSpec>0)
		return this->mWidthSpec;
	else 
		return this->mMinWidth; 
}


// FUNCTION: WON 0x0070ecc0
// Returns the table's fixed width if set, otherwise its preferred maximum width (computed on first use); the
// border drawing helper below draws a raised or sunken frame
int TableElement::GetMaxWidth() 
{ 
	if(this->mMaxWidth==0)
		this->CalcMinMaxWidths();

	if(this->mWidthSpec>0)
		return this->mWidthSpec;
	else
		return this->mMaxWidth; 
}


static void DrawShadedRect(Graphics &g, int x, int y, int width, int height, int size, bool up)
{
	g.Translate(x,y);
	width--;
	height--;
	int light = 0xd6d6d6;
	int dark = 0x848484;
	for(int i=0; i<size; i++)
	{
		g.SetColor(up?light:dark);
		g.DrawLine(i,i,width-i,i);
		g.DrawLine(i,i,i,height-i);
		g.SetColor(up?dark:light);
		g.DrawLine(width-i,i,width-i,height);
		g.DrawLine(i,height-i,width,height-i);
	}
	g.Translate(-x,-y);
}

// FUNCTION: WON 0x0070E8C0
// Computes each column's minimum and maximum width from its cells (single-column cells first, then spanning
// cells spread their extra need across their columns) and the table's totals
void TableElement::CalcMinMaxWidths()
{
	this->mMinWidth = this->mMaxWidth = 0;
	int aNumColumns = this->mColumnVector.size();
	int aNumRows = this->mRowVector.size();
	if(aNumRows==0 || aNumColumns==0)
		return;	

	int aRow, aCol;
	int aPad = this->mCellPadding*2 + (this->mBorder?4:0);
	for(aCol=0; aCol<aNumColumns; aCol++)
		this->mColumnVector[aCol].mMinWidth = this->mColumnVector[aCol].mMaxWidth = 0;

	// first pass
	for(aCol=0; aCol<aNumColumns; aCol++) 
	{
		ColumnInfo &aColumnInfo = this->mColumnVector[aCol];
		int aMaxMinWidth = aColumnInfo.mMinWidth;
		int aMaxMaxWidth = aColumnInfo.mMaxWidth;
		for(aRow=0; aRow<aNumRows; aRow++)
		{
			CellInfo *aCellInfo = this->GetCellInfoAt(aRow,aCol);
			if(aCellInfo==NULL || aCellInfo->mColumnSpan!=1)
				continue;
			MultiLineElement *aCell = aCellInfo->mContents.get();
			if(aCell==NULL)
				continue;
			int aMinWidth = aCell->GetMinWidth();
			int aMaxWidth = aCell->GetMaxWidth();
			if(aCellInfo->mNoWrap)
				aMinWidth = aMaxWidth; 

			if(aCellInfo->mWidth > 0)
			{
				if(aMinWidth < aCellInfo->mWidth)
					aMaxWidth = aCellInfo->mWidth;
				else
					aMaxWidth = aMinWidth;
			}

			if(aMinWidth > aMaxMinWidth)
				aMaxMinWidth = aMinWidth;

			if(aMaxWidth > aMaxMaxWidth)
				aMaxMaxWidth = aMaxWidth;					
		}

		aColumnInfo.mMinWidth = aMaxMinWidth + aPad;
		aColumnInfo.mMaxWidth = aMaxMaxWidth + aPad;
		this->mMinWidth += aColumnInfo.mMinWidth;
		this->mMaxWidth += aColumnInfo.mMaxWidth;
	}

	// second pass
	for(aCol=0; aCol<aNumColumns; aCol++) 
	{
		for(aRow=0; aRow<aNumRows; aRow++)
		{
			CellInfo *aCellInfo = this->GetCellInfoAt(aRow,aCol);
			if(aCellInfo==NULL || !(aCellInfo->mColumnSpan>1))
				continue;
			MultiLineElement *aCell = aCellInfo->mContents.get();
			if(aCell==NULL)
				continue;
			int aMinWidth = aCell->GetMinWidth();
			int aMaxWidth = aCell->GetMaxWidth();
			if(aCellInfo->mNoWrap)
				aMinWidth = aMaxWidth; 

			if(aCellInfo->mWidth > 0)
			{
				if(aMinWidth < aCellInfo->mWidth)
					aMaxWidth = aCellInfo->mWidth;
				else
					aMaxWidth = aMinWidth;
			}

			// Figure out how much MinWidth and MaxWidth is available from all the columns
			int i;
			int anAvailableMinWidth = 0;
			int aTotalMaxWidth = 0;
			for(i=0; i<aCellInfo->mColumnSpan; i++)
			{
				anAvailableMinWidth += this->mColumnVector[aCol+i].mMinWidth;
				aTotalMaxWidth += this->mColumnVector[aCol+i].mMaxWidth;
			}
			anAvailableMinWidth += (aCellInfo->mColumnSpan-1)*this->mCellSpacing - aPad;

			// Need to distribute more MinWidth to the columns
			int aNeededExtra = aMinWidth - anAvailableMinWidth;
			if(aNeededExtra>0)
			{
				int aPool = aNeededExtra;
				this->mMinWidth += aNeededExtra;
				for(i=0; i<aCellInfo->mColumnSpan; i++)
				{						
					ColumnInfo &aColumnInfo = this->mColumnVector[aCol+i];
					int anAmount;
					if(i!=aCellInfo->mColumnSpan-1)
					{
						if(aTotalMaxWidth>0)
							anAmount = (aMinWidth - anAvailableMinWidth)*aColumnInfo.mMaxWidth/aTotalMaxWidth;
						else
							anAmount = (aMinWidth - anAvailableMinWidth)/aCellInfo->mColumnSpan;

						aColumnInfo.mMinWidth += anAmount;
						aPool -= anAmount;
					}
					else
						aColumnInfo.mMinWidth += aPool;
				}
			}

			// Need to distribute more MaxWidth to the columns
			aNeededExtra = aMaxWidth - aTotalMaxWidth;
			if(!(aNeededExtra > 0))
				continue;
			int aPool = aNeededExtra;
			this->mMaxWidth += aNeededExtra;
			for(i=0; i<aCellInfo->mColumnSpan; i++)
			{
				ColumnInfo &aColumnInfo = this->mColumnVector[aCol+i];
				int anAmount;
				if(i!=aCellInfo->mColumnSpan-1)
				{
					if(aTotalMaxWidth>0)
						anAmount = (aMaxWidth - aTotalMaxWidth)*aColumnInfo.mMaxWidth/aTotalMaxWidth;
					else
						anAmount = (aMaxWidth - aTotalMaxWidth)/aCellInfo->mColumnSpan;

					aPool -= anAmount;
				}
				else
					anAmount = aPool;

				aColumnInfo.mMaxWidth += anAmount;
			}

		}
	}


	this->mMinWidth += this->mCellSpacing*(aNumColumns+1) + 2*this->mBorder;
	this->mMaxWidth += this->mCellSpacing*(aNumColumns+1) + 2*this->mBorder;
	if(this->mWidthSpec>0)
	{
		if(this->mWidthSpec<this->mMinWidth)
			this->mWidthSpec = this->mMinWidth;
	}
}

// FUNCTION: WON 0x0070ECE0
// Lays out every cell to its column width and computes row positions and heights, growing a row so that
// cells spanning several rows fit; sets the table height
void TableElement::CalcRowHeights()
{
	int aRow, aCol;
	int aNumRows = this->mRowVector.size(), aNumColumns = this->mColumnVector.size();

	// First pass
	this->mHeight = this->mBorder + this->mCellSpacing;
	for(aRow=0; aRow<aNumRows; aRow++)
	{
		RowInfo &aRowInfo = this->mRowVector[aRow];
		aRowInfo.my = this->mHeight;
		int aMaxHeight = 0;
		for(aCol=0; aCol<aNumColumns; aCol++)
		{
			CellInfo *aCellInfo = this->GetCellInfoAt(aRowInfo,aCol);
			MultiLineElement *aCell = aCellInfo->mContents.get();
			if(aCell==NULL)
				continue;
			ColumnInfo &aColumnInfo = this->mColumnVector[aCol];
			int aColumnWidth = aColumnInfo.mWidth;
			if(aCellInfo->mColumnSpan>1)
			{
				ColumnInfo &anEndColumn = this->mColumnVector[aCol + aCellInfo->mColumnSpan - 1];
				aColumnWidth = anEndColumn.mx + anEndColumn.mWidth - aColumnInfo.mx;
			}

			aCell->SetMaxWidth(aColumnWidth - this->mCellPadding*2);
			if(aCellInfo->mRowSpan!=1)
				continue;
			int aHeight = aCell->GetHeight();
			if(aCellInfo->mHeight>aHeight)
				aHeight = aCellInfo->mHeight;
			if(aHeight>aMaxHeight)
				aMaxHeight = aHeight;
		}
		aRowInfo.mHeight = aMaxHeight+this->mCellPadding*2 + (this->mBorder?4:0);
		this->mHeight += aRowInfo.mHeight + this->mCellSpacing;
	}
	this->mHeight+=this->mBorder;

	// Second pass
	this->mHeight = this->mBorder + this->mCellSpacing;
	for(aRow=0; aRow<aNumRows; aRow++)
	{
		RowInfo &aRowInfo = this->mRowVector[aRow];
		aRowInfo.my = this->mHeight;
		int aMaxHeight = aRowInfo.mHeight-this->mCellPadding*2;
		for(aCol=0; aCol<aNumColumns; aCol++)
		{
			CellInfo *aCellInfo = this->GetCellInfoAt(aRowInfo,aCol);
			MultiLineElement *aCell = aCellInfo->mContents.get();
			if(aCell==NULL || !(aCellInfo->mRowSpan>1))
				continue;
			int aHeight = aCell->GetHeight();
			if(aCellInfo->mHeight>aHeight)
				aHeight = aCellInfo->mHeight;

			int i = 0;
			int anAvailableHeight = 0;
			for(i=0; i<aCellInfo->mRowSpan; i++)
				anAvailableHeight += this->mRowVector[aRow+i].mHeight;

			anAvailableHeight += this->mCellSpacing*(aCellInfo->mRowSpan-1);
			anAvailableHeight -= this->mCellPadding*2;

			if(aHeight > anAvailableHeight)
				aRowInfo.mHeight += aHeight - anAvailableHeight;
		}
		this->mHeight += aRowInfo.mHeight + this->mCellSpacing;
	}

	this->mHeight+=this->mBorder;
/**/
}

// FUNCTION: WON 0x0070EF50
// Shares a table width between the columns: percentage columns first, then the others grow from their
// minimum toward their maximum width; sets each column's position and the table width
void TableElement::CalcColumnWidths(int theWidth)
{
	int aNumColumns = this->mColumnVector.size();
	if(theWidth<this->mMinWidth)
		theWidth = this->mMinWidth;

	int aCol;
	int anExtraSpace = theWidth-this->mMinWidth, aColumnWeight = 0;
	int anOriginalExtraSpace = anExtraSpace;
	int anAvailableWidth = theWidth - 2*this->mBorder - (aNumColumns+1)*this->mCellSpacing;
	int aWidthForPercent = 0;
	int aTotalPercent = 0;
	int aNonPercentMaxWidth = this->mMaxWidth, aNonPercentMinWidth = this->mMinWidth;
	bool allColumnsPercent = true;
	for(aCol=0; aCol<aNumColumns; aCol++)
	{
		ColumnInfo &anInfo = this->mColumnVector[aCol];
		if(anInfo.mWeight < 0)
		{
			int aPercent = -anInfo.mWeight;
			anInfo.mWidth = anAvailableWidth*aPercent/100;
			if(anInfo.mWidth < anInfo.mMinWidth)
				anInfo.mWidth = anInfo.mMinWidth;
//			else
				aTotalPercent += aPercent;

			aWidthForPercent += anInfo.mWidth - anInfo.mMinWidth;
			aNonPercentMinWidth -= anInfo.mMinWidth - this->mCellSpacing;
			aNonPercentMaxWidth -= anInfo.mMaxWidth - this->mCellSpacing;
		}
		else
			allColumnsPercent = false;
	}

	if((aWidthForPercent > anExtraSpace) || (allColumnsPercent && aTotalPercent!=100))
	{
		int aLeftOver = anExtraSpace;
		ColumnInfo *aLeftOverColumn = NULL;
		for(aCol=0; aCol<aNumColumns; aCol++)
		{
			ColumnInfo &anInfo = this->mColumnVector[aCol];
			if(anInfo.mWeight<0 /*&& anInfo.mWidth>anInfo.mMinWidth*/)
			{
				int anAmount = anExtraSpace*-anInfo.mWeight/aTotalPercent;
				anInfo.mWidth = anInfo.mMinWidth + anAmount;
				aLeftOver -= anAmount;
				aLeftOverColumn = &anInfo;
			}
		}
		if(aLeftOverColumn!=NULL && aLeftOver>0)
			aLeftOverColumn->mWidth += aLeftOver;
		
		anExtraSpace = 0;
	}
	else 
		anExtraSpace -= aWidthForPercent;


	if(anExtraSpace>=aNonPercentMaxWidth-aNonPercentMinWidth)
	{
		if(this->mWidthSpec!=0)
			anExtraSpace -= aNonPercentMaxWidth-aNonPercentMinWidth;
		else
			anExtraSpace = 0;

		for(aCol=0; aCol<aNumColumns; aCol++)
		{
			ColumnInfo &anInfo = this->mColumnVector[aCol];
			if(!(anInfo.mWeight>=0))
				continue;
			anInfo.SetWidthToMax();
			anInfo.mWeight = anInfo.mMaxWidth;
			aColumnWeight += anInfo.mWeight;
		}
	}
	else 
	{
	//	anExtraSpace = theWidth-aNonPercentMinWidth;
		for(aCol=0; aCol<aNumColumns; aCol++)
		{
			ColumnInfo &anInfo = this->mColumnVector[aCol];
			if(!(anInfo.mWeight>=0))
				continue;
			anInfo.SetWidthToMin();
			anInfo.mWeight = anInfo.mMaxWidth - anInfo.mMinWidth;
			aColumnWeight += anInfo.mWeight;
		}
	}

	int aLeftOver = anExtraSpace;
	this->mWidth = this->mBorder + this->mCellSpacing;
	for(aCol=0; aCol<aNumColumns; aCol++)
	{
		ColumnInfo &aColInfo = this->mColumnVector[aCol];
		if(aColumnWeight>0 && aColInfo.mWeight>=0)
		{
			int anAmount = anExtraSpace*aColInfo.mWeight/aColumnWeight;
			aColInfo.mWidth += anAmount;
			aLeftOver -= anAmount;
		}

		aColInfo.mx = this->mWidth;
		this->mWidth += aColInfo.mWidth + this->mCellSpacing;
	}
	this->mWidth+=this->mBorder;
	if(aLeftOver<=0 || aLeftOver==anOriginalExtraSpace)
		return;
	ColumnInfo &aColInfo = this->mColumnVector.back();
	aColInfo.mWidth += aLeftOver;
	this->mWidth += aLeftOver;
}

// FUNCTION: WON 0x0070F230
// Lays the table out for an available width (or its fixed or percentage width): column widths, then rows
void TableElement::SetMaxWidth(int theWidth)
{
	this->mWidth = 0;
	this->mHeight = 0;
		
	int aNumColumns = this->mColumnVector.size();
	int aNumRows = this->mRowVector.size();
	if(aNumRows==0 || aNumColumns==0)
		return;

	if(this->mMinWidth==0)
		this->CalcMinMaxWidths();

	if(this->mWidthSpec>0)
		theWidth = this->mWidthSpec;
	else if(this->mWidthSpec<0) // percentage
		theWidth = (theWidth*-this->mWidthSpec)/100;

	this->CalcColumnWidths(theWidth);
	this->CalcRowHeights();
}

// FUNCTION: WON 0x0070F2E0
// Attaches the table and every cell's contents to a view at the cells' positions
void TableElement::SetViewport(ScrollAreaViewport *theViewport, int x, int y)
{
	LineSegment::SetViewport(theViewport,x,y);
	int aNumColumns = this->mColumnVector.size();
	int aNumRows = this->mRowVector.size();
	int aRow, aCol;
	
	for(aRow=0; aRow<aNumRows; aRow++)
	{
		RowInfo &aRowInfo = this->mRowVector[aRow];
		for(aCol=0; aCol<aNumColumns; aCol++)
		{
			CellInfoEx aCellInfoEx;
			if(this->GetCellInfoEx(aCellInfoEx,aRow,aCol,aRowInfo))
			{
				MultiLineElement *aCell = aCellInfoEx.mCellInfo->mContents.get();
				aCell->SetViewport(theViewport,x+aCellInfoEx.mCellX,y+aCellInfoEx.mCellY);
			}
		}
	}
}

// FUNCTION: WON 0x0070F3E0
// Returns the text segment under a point, descending into the cell there and making the point cell-relative;
// returns the table itself when no cell is hit (lenient picks the nearest row and last column)
LineSegment* TableElement::GetSegmentAt(int &x, int &y, bool lenient)
{

	int aNumColumns = this->mColumnVector.size();
	int aNumRows = this->mRowVector.size();

	int aRow = this->FindRowAt(y);
	if(aRow>=aNumRows)
		return this;

	RowInfo &aRowInfo = this->mRowVector[aRow];
	if(!lenient && (aRowInfo.my>y || aRowInfo.my+aRowInfo.mHeight<y))
		return this;

	CellInfoEx aCellInfoEx;
	for(int aCol=0; aCol<aNumColumns; aCol++)
	{
		ColumnInfo &aColumnInfo = this->mColumnVector[aCol];
		if(!(aColumnInfo.mx + aColumnInfo.mWidth > x) && (!lenient || aCol!=aNumColumns-1))
			continue;
		if(this->GetCellInfoEx(aCellInfoEx,aRow,aCol,aRowInfo,true,true))
		{
			x-=aCellInfoEx.mCellX;
			y-=aCellInfoEx.mCellY;
			return aCellInfoEx.mCellInfo->mContents->GetSegmentAt(x,y,lenient);
		}
		else
			return this;
	}
			
	return this;
}

// FUNCTION: WON 0x0070F5D0
// Appends the selected text of the cells inside the selection, separating cells with a space and rows with
// a line break
void TableElement::GetSelText(GUIString &theText, ElementSelectionInfo &theInfo)
{

	int aNumColumns = this->mColumnVector.size();
	int aNumRows = this->mRowVector.size();
	int aRow, aCol;
	
	bool done = false;
	for(aRow=0; aRow<aNumRows; aRow++)
	{
		RowInfo &aRowInfo = this->mRowVector[aRow];
		for(aCol=0; aCol<aNumColumns; aCol++)
		{
			MultiLineElement *aCell = this->GetCellAt(aRowInfo,aCol);
			if(aCell!=NULL && aCell->GetEndSelIndex()>=theInfo.mStartSelSegment)
			{
				done = true; 
				break;
			}
		}

		if(done)
			break;
	}

	done = false;
	for(; aRow<aNumRows; aRow++)
	{
		RowInfo &aRowInfo = this->mRowVector[aRow];
		for(; aCol<aNumColumns && !done; aCol++)
		{
			MultiLineElement *aCell = this->GetCellAt(aRowInfo,aCol);
			if(aCell==NULL)
				continue;
			if(aCell->mStartSelIndex>=theInfo.mEndSelSegment)
			{
				done = true;
				break;
			}
			else
			{
				aCell->GetSelText(theText,theInfo);
				theText.append(" ");
			}
		}
		aCol = 0;
		if(!done)
			theText.append("\r\n");
		else
			break;
	}
}

// FUNCTION: WON 0x0070F880
// Searches the cells from the search start for the text; returns whether a cell found it
bool TableElement::FindStr(ElementFindInfo &theInfo)
{
	int aNumColumns = this->mColumnVector.size();
	int aNumRows = this->mRowVector.size();
	int aRow, aCol;
	
	bool done = false;
	for(aRow=0; aRow<aNumRows; aRow++)
	{
		RowInfo &aRowInfo = this->mRowVector[aRow];
		for(aCol=0; aCol<aNumColumns; aCol++)
		{
			MultiLineElement *aCell = this->GetCellAt(aRowInfo,aCol);
			if(aCell!=NULL && aCell->GetEndSelIndex()>=theInfo.mStartSelSegment)
			{
				done = true; 
				break;
			}
		}

		if(done)
			break;
	}

	done = false;
	for(; aRow<aNumRows; aRow++)
	{
		RowInfo &aRowInfo = this->mRowVector[aRow];
		for(; aCol<aNumColumns && !done; aCol++)
		{
			MultiLineElement *aCell = this->GetCellAt(aRowInfo,aCol);
			if(aCell==NULL)
				continue;
			theInfo.mFoundStartSegment = -1;
			if(aCell->mStartSelIndex>=theInfo.mEndSelSegment)
				return false;
			else if(aCell->FindStr(theInfo))
					return true;
		}
		aCol = 0;
	}

	theInfo.mFoundStartSegment = -1;
	return false;
}

// FUNCTION: WON 0x0070E3B0
// Fills in a cell's border rectangle and its aligned contents position; for an empty cell covered by a span,
// optionally looks up the spanning cell above or to the left; returns false when there is no cell
bool TableElement::GetCellInfoEx(CellInfoEx &theInfo, int theRow, int theCol, RowInfo &theRowInfo, bool seekUp, bool seekLeft)
{
	CellInfo *aCellInfo = this->GetCellInfoAt(theRowInfo,theCol);
	if(aCellInfo==NULL)
		return false;

	MultiLineElement *aCell = aCellInfo->mContents.get();
	if(aCell==NULL) 
	{
		// See if it's a cell which spans multiple rows or columns
		bool spanner = false;
		RowInfo *aRowInfo = &theRowInfo;
		if(seekUp && aCellInfo->mRowSpan<0)
		{
			theRow = theRow + aCellInfo->mRowSpan;
			aRowInfo = &this->mRowVector[theRow];
			spanner = true;
		}

		if(seekLeft && aCellInfo->mColumnSpan<0)
		{
			theCol = theCol + aCellInfo->mColumnSpan;
			spanner = true;
		}

		if(!spanner)
			return false;

		return this->GetCellInfoEx(theInfo,theRow,theCol,*aRowInfo);
	}

	ColumnInfo &aColumnInfo = this->mColumnVector[theCol];

	int cellx = aColumnInfo.mx;
	int celly = theRowInfo.my;
	int cellw = aColumnInfo.mWidth;
	int cellh = theRowInfo.mHeight;

	if(aCellInfo->mColumnSpan>1)
	{
		ColumnInfo &anEndColumn = this->mColumnVector[theCol + aCellInfo->mColumnSpan - 1];
		cellw = anEndColumn.mx + anEndColumn.mWidth - aColumnInfo.mx;
	}
	if(aCellInfo->mRowSpan>1)
	{
		RowInfo &anEndRow = this->mRowVector[theRow + aCellInfo->mRowSpan - 1];
		cellh = anEndRow.my + anEndRow.mHeight - theRowInfo.my;
	}

	char anHAlign = aCellInfo->mHAlign, aVAlign = aCellInfo->mVAlign;
	if(anHAlign==-1) anHAlign = theRowInfo.mHAlign;
	if(aVAlign==-1) aVAlign = theRowInfo.mVAlign;

	int dx, dy;
	int aPad = this->mCellPadding + (this->mBorder?2:0);
	switch(anHAlign)
	{
		case 1: dx = (cellw-aCell->GetWidth())/2; break;
		case 2: dx = cellw-aCell->GetWidth()-aPad; break;
		default: dx = aPad;
	}

	switch(aVAlign)
	{
		case 1: dy = (cellh-aCell->GetHeight())/2; break;
		case 2: dy = cellh-aCell->GetHeight()-aPad; break;
		default: dy = aPad;
	}


	if(dx<aPad)
		dx = aPad;
	if(dy<aPad)
		dy = aPad;

	theInfo.mCellInfo = aCellInfo;
	theInfo.mBorderX = cellx;
	theInfo.mBorderY = celly;
	theInfo.mBorderWidth = cellw;
	theInfo.mBorderHeight = cellh;
	theInfo.mCellX = cellx+dx;
	theInfo.mCellY = celly+dy;
	return true;
}

// FUNCTION: WON 0x0070E590
// Paints one cell: its (or its row's) background, a sunken border when the table has borders, and its contents
void TableElement::PaintCell(Graphics &g, int x, int y, int theRow, int theCol, RowInfo &theRowInfo, ElementPaintInfo &thePaintInfo, bool seekUp)
{
	CellInfoEx aCellInfoEx;
	if(!this->GetCellInfoEx(aCellInfoEx,theRow,theCol,theRowInfo,seekUp))
		return;

	CellInfo *aCellInfo = aCellInfoEx.mCellInfo;
	MultiLineElement *aCell = aCellInfo->mContents.get();

	int aBGColor = aCellInfo->mBGColor;
	if(aBGColor==-1) aBGColor = theRowInfo.mBGColor;
	if(aBGColor>=0)
	{
		g.SetColor(aBGColor);
		g.FillRect(x + aCellInfoEx.mBorderX,y + aCellInfoEx.mBorderY,aCellInfoEx.mBorderWidth,aCellInfoEx.mBorderHeight);
	}

	if(this->mBorder)
		DrawShadedRect(g,x + aCellInfoEx.mBorderX,y + aCellInfoEx.mBorderY,aCellInfoEx.mBorderWidth,aCellInfoEx.mBorderHeight,1,false);

	aCell->Paint(g,x + aCellInfoEx.mCellX,y + aCellInfoEx.mCellY,thePaintInfo);
}

// FUNCTION: WON 0x0070E750
// Paints the table background and raised border, then the cells of every row inside the clip area
void TableElement::Paint(Graphics &g, int x, int y, ElementPaintInfo &theInfo)
{
	if(this->mBGColor!=-1)
	{
		g.SetColor(this->mBGColor);
		g.FillRect(x,y,this->mWidth,this->mHeight);
	}
	DrawShadedRect(g,x,y,this->mWidth,this->mHeight,this->mBorder,true);

	int aNumColumns = this->mColumnVector.size();
	int aNumRows = this->mRowVector.size();
	int aRow, aCol;
	int aBottom = (theInfo.mClipY+theInfo.mClipHeight) - y;

	
	int aTop = theInfo.mClipY - y;
	int aStartRow = 0;
	if(aTop > 0)
		aStartRow = this->FindRowAt(aTop);

	for(aRow=aStartRow; aRow<aNumRows; aRow++)
	{
		RowInfo &aRowInfo = this->mRowVector[aRow];
		if(aRowInfo.my > aBottom)
			break;

		for(aCol=0; aCol<aNumColumns; aCol++)
			this->PaintCell(g,x,y,aRow,aCol,aRowInfo,theInfo,aRow==aStartRow);
	}
}
}

namespace WONAPI {
// Moves to the next non-empty cell in row order; returns it, or NULL past the last row
// (not identified in the original exe)

MultiLineElement* TableElement::NextCell()
{
	if(this->mCurRow >= this->mRowVector.size())
		return NULL;

	while(true)
	{
		if(this->mCurCol < this->mColumnVector.size()-1)
			this->mCurCol++;
		else
		{
			this->mCurRow++;
			this->mCurCol = 0;
		}
		if(this->mCurRow >= this->mRowVector.size())
			return NULL;

		this->mCurCell = this->GetCellAt(this->mCurRow, this->mCurCol);
		if(this->mCurCell!=NULL)
			return this->mCurCell;
	}
}

// Sets the current cell's background color (not identified in the original exe)

void TableElement::SetCellBGColor(int theColor)
{
	if(this->mCurCell!=NULL)
	{
		CellInfo *anInfo = this->GetCellInfoAt(this->mCurRow,this->mCurCol);
		anInfo->mBGColor = theColor;
	}
}

// Sets the current cell's horizontal and vertical alignment (not identified in the original exe)

void TableElement::SetCellAlign(int theHAlign, int theVAlign)
{
	if(this->mCurCell==NULL)
		return;
	CellInfo *anInfo = this->GetCellInfoAt(this->mCurRow,this->mCurCol);
	anInfo->mHAlign = theHAlign;
	anInfo->mVAlign = theVAlign;
}
}

namespace WONAPI {
// Empties the table and gives it the given number of rows and columns (not identified in the original exe)
void TableElement::SetSize(int numRows, int numCols)
{
	this->mRowVector.clear();
	this->mColumnVector.clear();
	this->EnsureNumCols(numCols);
	this->EnsureNumRows(numRows);
}

// Makes the top-left cell the current cell (not identified in the original exe)
void TableElement::Rewind()
{
	this->mCurRow = this->mCurCol = 0;
	this->mCurCell = this->GetCellAt(this->mCurRow, this->mCurCol);
}

// Adds a text segment to the current cell (not identified in the original exe)
void TableElement::AddSegment(LineSegment *theSegment, unsigned short theFlags)
{
	if(this->mCurCell!=NULL)
	{
		this->mCurCell->AddSegment(theSegment, theFlags);
	}
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0070e670
// (void __cdecl WONAPI::DrawShadedRect(class WONAPI::Graphics &,int,int,int,int,int,bool)    [?DrawShadedRect@WONAPI@@YAXAAVGraphics@1@HHHHH_N@Z])
// TEMPLATE: WON 0x00725f70
// (void __cdecl std::_Distance(class std::_Ptrit<struct WONAPI::TableElement::RowInfo,int,struct WONAPI::TableElement::RowInfo *,struct WONAPI::TableElem)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00725510
// (class std::_Ptrit<struct WONAPI::TableElement::RowInfo,int,struct WONAPI::TableElement::RowInfo *,struct WONAPI::TableElement::RowInfo &,struct WONAPI)
// TEMPLATE: WON 0x00725e20
// (void __cdecl std::_Distance(class std::_Ptrit<struct WONAPI::TableElement::RowInfo,int,struct WONAPI::TableElement::RowInfo *,struct WONAPI::TableElem)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00723c60
// (class std::_Ptrit<struct WONAPI::TableElement::RowInfo,int,struct WONAPI::TableElement::RowInfo *,struct WONAPI::TableElement::RowInfo &,struct WONAPI)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071ce10
// (public: class std::_Ptrit<struct WONAPI::TableElement::ColumnInfo,int,struct WONAPI::TableElement::ColumnInfo *,struct WONAPI::TableElement::ColumnInf)
// TEMPLATE: WON 0x0070de70
// (public: __thiscall WONAPI::TableElement::RowInfo::~RowInfo(void))
