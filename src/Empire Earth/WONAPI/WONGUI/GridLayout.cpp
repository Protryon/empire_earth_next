// Grid layout of components in rows and columns (not identified in the original exe)
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
#include "ee/wonapi/WONGridLayout.h"

using namespace WONAPI;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GridLayout::GridLayout(int theNumRows, int theNumColumns)
{
	this->mNumRows = theNumRows;
	this->mNumColumns = theNumColumns;

	this->mColumnInfo = new ColumnInfo[theNumColumns];
	this->mRowInfo = new RowInfo[theNumRows];

	this->mCellInfo = new CellInfo*[theNumRows];
	for(int i=0; i<theNumRows; i++)
		this->mCellInfo[i] = new CellInfo[theNumColumns];

	this->mRightPad = this->mBottomPad = 10;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GridLayout::~GridLayout()
{
	delete [] this->mColumnInfo;
	delete [] this->mRowInfo;
	for(int i=0; i<this->mNumRows; i++)
		delete [] this->mCellInfo[i];

	delete [] this->mCellInfo;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GridLayout::CellInfo::SetPosSize(int x, int y, int width, int height)
{
	if(this->mComponent!=NULL)
		this->mComponent->SetPosSize(x,y,width,height);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GridLayout::SetCell(int theRow, int theColumn, Component *theComponent)
{
	CellInfo &aCellInfo = this->mCellInfo[theRow][theColumn];
	aCellInfo.mComponent = theComponent;
	aCellInfo.mMinWidth = theComponent->Width();
	aCellInfo.mMinHeight = theComponent->Height();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GridLayout::SetPosSize(int x, int y, int width, int height, bool commit)
{
	this->mBounds.x = x;
	this->mBounds.y = y;
	this->mBounds.width = width;
	this->mBounds.height = height;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GridLayout::SetPadding(int theRightPad, int theBottomPad)
{
	this->mRightPad = theRightPad;
	this->mBottomPad = theBottomPad;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GridLayout::SetColumnPad(int theColumn, int thePad)
{
	this->mColumnInfo[theColumn].mRightPad = thePad;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GridLayout::SetRowPad(int theRow, int thePad)
{
	this->mRowInfo[theRow].mBottomPad = thePad;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GridLayout::CalcColumnWidths()
{
	int aRow, aCol;
	int x = 0;
	for(aCol=0; aCol<this->mNumColumns; aCol++)
	{
		int aMaxWidth = 0;
		for(aRow=0; aRow<this->mNumRows; aRow++)
		{
			CellInfo &anInfo = this->mCellInfo[aRow][aCol];
			int aWidth = anInfo.GetMinWidth();
			if(aWidth>aMaxWidth)
				aMaxWidth = aWidth;
		}

		ColumnInfo &aColumnInfo = this->mColumnInfo[aCol];

		if(aCol==this->mNumColumns-1 && x+aMaxWidth<this->Width())
			aMaxWidth = this->Width()-x;

		aColumnInfo.mWidth = aMaxWidth;
		aColumnInfo.mx = x;
		int aPad = aColumnInfo.mRightPad;
		if(aPad<0)
			aPad = this->mRightPad;

		x+=aColumnInfo.mWidth+aPad;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GridLayout::CalcRowHeights()
{
	int aRow, aCol;
	int y=0;
	for(aRow=0; aRow<this->mNumRows; aRow++)
	{
		int aMaxHeight = 0;
		for(aCol=0; aCol<this->mNumColumns; aCol++)
		{
			CellInfo &anInfo = this->mCellInfo[aRow][aCol];
			int aHeight = anInfo.GetMinHeight();
			if(aHeight > aMaxHeight)
				aMaxHeight = aHeight;
		}

		RowInfo &aRowInfo = this->mRowInfo[aRow];
		aRowInfo.mHeight = aMaxHeight;
		aRowInfo.my = y;
		
		int aPad = aRowInfo.mBottomPad;
		if(aPad<0)
			aPad = this->mBottomPad;

		y+=aRowInfo.mHeight + aPad;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GridLayout::PositionCells()
{
	int aRow, aCol;
	for(aRow=0; aRow<this->mNumRows; aRow++)
	{
		RowInfo &aRowInfo = this->mRowInfo[aRow];
		for(aCol=0; aCol<this->mNumColumns; aCol++)
		{
			ColumnInfo &aColumnInfo = this->mColumnInfo[aCol];
			this->mCellInfo[aRow][aCol].SetPosSize(aColumnInfo.mx,aRowInfo.my,aColumnInfo.mWidth,aRowInfo.mHeight);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GridLayout::Execute()
{
	this->CalcColumnWidths();
	this->CalcRowHeights();
	this->PositionCells();
}

