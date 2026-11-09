// Multi-column list item column lookup and list header pass-throughs (file name inferred)
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
// FUNCTION: WON 0x00702500
// Returns the item in a column, or NULL when the column is out of range
ListItem* MultiListItem::GetItem(int column) const
{
    if(column<0 || column>=this->mItems.size()) return NULL;
    return this->mItems[column].get();
}
// Paints the header's children (the linker folds this body into the identical ScrollAreaViewport::Paint at 0x00705e00)
void MultiListHeader::Paint(Graphics& graphics) { Container::Paint(graphics); }
// Returns the child under a point (the linker folds this body into the identical RootContainer::FindChildAt
// at 0x00704710)
Component* MultiListHeader::FindChildAt(int x,int y)
{
    return Container::FindChildAt(x,y);
}
}
