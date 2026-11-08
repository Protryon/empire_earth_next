// Table element construction cursor state and current row and column attributes (file name inferred)
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
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTableElement.h"
namespace WONAPI {
// FUNCTION: WON 0x0070dfe0
// Resets the build cursor to before the first row, with no current cell
void TableElement::StartTraversal() { this->mCurRow=this->mCurCol=-1; this->mCurCell=NULL; }
// FUNCTION: WON 0x0070e070
// Returns the current row's horizontal alignment
HorzAlign TableElement::GetCurRowAlign() { return (HorzAlign)this->mRowVector[this->mCurRow].mHAlign; }
// FUNCTION: WON 0x0070e240
// Sets the current row's background color (ignored when there is no current row)
void TableElement::SetRowBGColor(int color)
{
    if(this->mCurRow<0 || this->mCurRow>=this->mRowVector.size()) return;
    this->mRowVector[this->mCurRow].mBGColor=color;
}
// FUNCTION: WON 0x0070e280
// Sets the current row's horizontal and vertical alignment (ignored when there is no current row)
void TableElement::SetRowAlign(int horizontal,int vertical)
{
    if(this->mCurRow<0 || this->mCurRow>=this->mRowVector.size()) return;
    RowInfo& row=this->mRowVector[this->mCurRow];
    row.mHAlign=horizontal;
    row.mVAlign=vertical;
}
// FUNCTION: WON 0x0070e2d0
// Lowers the current column's width weight to the given value when it is smaller (percentages are negative)
void TableElement::SetColumnWidth(int width)
{
    if(this->mCurCol<0 || this->mCurCol>=this->mColumnVector.size()) return;
    ColumnInfo& column=this->mColumnVector[this->mCurCol];
    if(width<column.mWeight) column.mWeight=width;
}
}
