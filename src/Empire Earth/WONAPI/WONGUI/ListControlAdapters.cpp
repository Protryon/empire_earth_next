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
// Returns the list box's text colour
int ListArea::GetTextColor() { return this->mTextColor; }
// Compares two text items by the numbers their texts start with
int ListArea::NumberSortFunc(ListArea*,const ListItem* left,const ListItem* right)
{
    return ((TextListItem*)left)->mText.mData->ToInt(0)-((TextListItem*)right)->mText.mData->ToInt(0);
}
// ---- the default (empty) bodies of ListItem's virtuals (ListItemDefaults.cpp until R6 WON GUI round 36)
// ListItem's ctor, Virtual04 and Virtual10 live in ListArea.cpp (same object: the ctor must be seen before
// ListArea's `new ListItem`).
// Leaves the item's size unchanged
void ListItem::CalcDimensions(ListArea*) {}
// Ignores the viewport (the linker folds this body into the identical one at 0x004ac9c8)
void ListItem::SetViewport(ListArea*,Container*) {}
// Ignores the mouse entering (the linker folds this body into the identical one at 0x006907a3)
void ListItem::MouseEnter(ListArea*,int,int) {}
// Ignores the mouse leaving (the linker folds this body into the identical one at 0x00556dd8)
void ListItem::MouseExit(ListArea*) {}
// Ignores mouse movement (the linker folds this body into the identical one at 0x006907a3)
void ListItem::MouseMove(ListArea*,int,int) {}
// Ignores a mouse drag (the linker folds this body into the identical one at 0x006907a3)
void ListItem::MouseDrag(ListArea*,int,int) {}
// Ignores a mouse button press (the linker folds this body into the identical one at 0x004df42d)
void ListItem::MouseDown(ListArea*,int,int,int) {}
// Ignores a mouse button release (the linker folds this body into the identical one at 0x004df42d)
void ListItem::MouseUp(ListArea*,int,int,int) {}
}
