// GridLayout (not identified in the original exe).
// SDK Execute() is a direct-call overload; does not override base Execute(Component*).
#ifndef __GRIDLAYOUT_H__
#define __GRIDLAYOUT_H__

#include "ee/wonapi/WONLobbyGUI.h"

#include <vector>

namespace WONAPI
{

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class GridLayout : public ChildLayoutInstructionBase
{
protected:
	struct ColumnInfo
	{
		int mx;
		int mWidth;
		int mRightPad;

		ColumnInfo() : mRightPad(-1) { }
	};

	struct RowInfo
	{
		int my;
		int mHeight;
		int mBottomPad;

		RowInfo() : mBottomPad(-1) { }
	};

	struct CellInfo
	{
		Component *mComponent;
		int mMinWidth, mMinHeight;
	
		
		CellInfo() : mComponent(NULL), mMinWidth(0), mMinHeight(0) { }
		int GetMinWidth() { return this->mMinWidth; }
		int GetMinHeight() { return this->mMinHeight; }

		void SetPosSize(int x, int y, int width, int height);

	};

	ColumnInfo	*mColumnInfo;
	RowInfo		*mRowInfo;
	CellInfo	**mCellInfo;
	int mRightPad, mBottomPad;

	int mNumRows, mNumColumns;
	WONRectangle mBounds;

	void CalcColumnWidths();
	void CalcRowHeights();
	void PositionCells();
	virtual ~GridLayout();
public:
	GridLayout(int theNumRows, int theNumCols);
	void SetCell(int theRow, int theColumn, Component *theComponent);
	void SetPosSize(int x, int y, int width, int height, bool commit);
	void SetPadding(int theRightPad, int theBottomPad);
	void SetColumnPad(int theColumn, int thePad);
	void SetRowPad(int theRow, int thePad);

	virtual int Width() { return this->mBounds.width; }
	virtual int Height() { return this->mBounds.height; }
	virtual int Left() { return this->mBounds.x; }
	virtual int Top() { return this->mBounds.y; }
	virtual void Execute();
};
typedef SmartPtr<GridLayout> GridLayoutPtr;

}

#endif