// Table element construction cursor: row and column stepping and cell spans (file name inferred)
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
#include "ee/wonapi/WONLobbyTableElement.h"
namespace WONAPI {
// FUNCTION: WON 0x0070e000
// Moves the build cursor to the start of the next row, adding the row if needed
void TableElement::NextRow()
{
	this->mCurRow++;
	this->mCurCol = -1;
	this->EnsureNumRows(this->mCurRow+1);
}

// FUNCTION: WON 0x0070E020
// Moves the build cursor to the next cell with contents in the current row (starting a row first if
// needed), adding columns as it goes; returns that cell's info
TableElement::CellInfo* TableElement::NextColumn()
{
	if(this->mCurRow<0)
		this->NextRow();

	RowInfo &aRowInfo = this->mRowVector[this->mCurRow];
	CellInfo *aCellInfo;
	do
	{
		this->mCurCol++;
		this->EnsureNumCols(this->mCurCol+1);
		aCellInfo = this->GetCellInfoAt(aRowInfo,this->mCurCol);
		this->mCurCell = aCellInfo->mContents.get();
	} while(this->mCurCell==NULL);

	return aCellInfo;
}

// FUNCTION: WON 0x0070E080
// Makes the current cell span the given rows and columns, emptying the covered cells and marking them with
// their offset back to it; truncates the span to the table or grows the table
void TableElement::SetCellSpan(int theRowSpan, int theColumnSpan, bool truncate)
{
	if(this->mCurCell==NULL)
		return;
	if(truncate)
	{
		if(this->mCurRow + theRowSpan > this->mRowVector.size())
			theRowSpan = this->mRowVector.size()-this->mCurRow;

		if(this->mCurCol + theColumnSpan > this->mColumnVector.size())
			theColumnSpan = this->mColumnVector.size()-this->mCurCol;
	}
	else
	{
		this->EnsureNumCols(this->mCurCol + theColumnSpan);
		this->EnsureNumRows(this->mCurRow + theRowSpan);
	}

	for(int i=0; i<theRowSpan; i++)
	{
		for(int j=0; j<theColumnSpan; j++)
		{
			CellInfo* anInfo = this->GetCellInfoAt(this->mCurRow+i,this->mCurCol+j);
			if(i==0 && j==0)
				continue;
			anInfo->mContents = NULL;
			anInfo->mColumnSpan = -j;
			anInfo->mRowSpan = -i;
		}
	}

	CellInfo *anInfo = this->GetCellInfoAt(this->mCurRow,this->mCurCol);
	anInfo->mColumnSpan = theColumnSpan;
	anInfo->mRowSpan = theRowSpan;
}
}
