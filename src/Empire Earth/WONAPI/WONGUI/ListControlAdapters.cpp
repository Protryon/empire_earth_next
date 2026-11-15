// List box text colour and number sort (file name inferred)
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
namespace WONAPI {
// FUNCTION: WON 0x006fb520
// Returns the list box's text colour
int ListArea::GetTextColor() { return this->mTextColor; }
// FUNCTION: WON 0x006faaf0
// Compares two text items by the numbers their texts start with
int ListArea::NumberSortFunc(ListArea*,const ListItem* left,const ListItem* right)
{
    return ((TextListItem*)left)->mText.mData->ToInt(0)-((TextListItem*)right)->mText.mData->ToInt(0);
}
}
