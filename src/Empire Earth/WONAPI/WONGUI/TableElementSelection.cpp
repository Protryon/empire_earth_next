// Table element selection numbering (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTableElement.h"
namespace WONAPI {
// FUNCTION: WON 0x0070f500
// Numbers the table and then every cell's segments in row order for selection, and records the last index used
void TableElement::SetSelIndex()
{
	LineSegment::SetSelIndex();
	int aNumColumns = this->mColumnVector.size();
	int aNumRows = this->mRowVector.size();
	int aRow, aCol;
	
	for(aRow=0; aRow<aNumRows; aRow++)
	{
		RowInfo &aRowInfo = this->mRowVector[aRow];
		for(aCol=0; aCol<aNumColumns; aCol++)
		{
			MultiLineElement *aCell = this->GetCellAt(aRowInfo,aCol);
			if(aCell!=NULL)
				aCell->SetSelIndex();
		}
	}
	this->mEndSelIndex = LineSegment::mCounter-1;
}

}
