// List box sort type, text colour and number sort; multi-column mouse item reset (file name inferred)
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
// FUNCTION: WON 0x006facd0
// Changes the list box's sort type and re-sorts it; does nothing when the type is unchanged
void ListArea::SetSortType(ListSortType type)
{
    if(this->mSortType==type) return;
    this->mSortType=type;
    this->Sort();
}
// FUNCTION: WON 0x006fb520
// Returns the list box's text colour
int ListArea::GetTextColor() { return this->mTextColor; }
// FUNCTION: WON 0x006faaf0
// Compares two text items by the numbers their texts start with
int ListArea::NumberSortFunc(ListArea*,const ListItem* left,const ListItem* right)
{
    return ((TextListItem*)left)->mText.mData->ToInt(0)-((TextListItem*)right)->mText.mData->ToInt(0);
}
// FUNCTION: WON 0x007035d0
// Forgets the item under the mouse, including the column item
void MultiListArea::NotifyMouseItemGone()
{
    ListArea::NotifyMouseItemGone();
    this->mMouseColItem=NULL;
}
}
