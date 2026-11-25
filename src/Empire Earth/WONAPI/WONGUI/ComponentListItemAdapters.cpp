// ComponentListItem: a list item that hosts a component (file name inferred); its size, viewport and place are
// Rust's (rust/crates/ee_won_gui/src/leafpaint.rs, R6 WON round 34) over this unit's host
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
#include "ee/wonapi/WONLobbyComponentListItem.h"
#include "rust/won_gui/leafpaint.h"
using namespace WONAPI;
typedef EeWonLeafOp Lo;
// the item's reads and calls for leafpaint.rs (R6 WON round 34: its size, viewport and place are Rust's)
extern "C" intptr_t ee_won_guicpp_compitem_call(void* inMe, EeWonLeafOp op, intptr_t a, intptr_t b)
{
    ComponentListItem* t = (ComponentListItem*)static_cast<ListItem*>(inMe);
    switch (op) {
    case Lo::CompItemHasComponent: return t->mComponent.get() != NULL ? 1 : 0;
    case Lo::CompItemWidth: return t->mComponent->Width();
    case Lo::CompItemHeight: return t->mComponent->Height();
    case Lo::ItemWidthAt: return (intptr_t)&t->mWidth;
    case Lo::ItemHeightAt: return (intptr_t)&t->mHeight;
    case Lo::CompItemSetParent: t->mComponent->mParent = (Container*)a; return 0;
    case Lo::CompItemAdded: t->mComponent->AddedToParent(); return 0;
    case Lo::CompItemBaseSetPos: t->ListItem::SetPos((int)a, (int)b); return 0;
    case Lo::CompItemSetPos: t->mComponent->SetPos((int)a, (int)b); return 0;
    case Lo::CompItemSizable: return t->mIsSizable ? 1 : 0;
    case Lo::CompItemListPaintWidth: return ((ListArea*)a)->GetPaintWidth();
    case Lo::CompItemSetSize: t->mComponent->SetSize((int)a, (int)b); return 0;
    case Lo::CompItemMarkInvalid: t->mComponent->MarkInvalid(); return 0;
    case Lo::CompItemPaint: t->mComponent->Paint(*(Graphics*)a); return 0;
    default: return 0;
    }
}
namespace WONAPI {
// Takes the item's size from its component, when it has one
void ComponentListItem::CalcDimensions(ListArea*) { ee_won_leaf_call(static_cast<ListItem*>(this), ee_won_guicpp_compitem_call, EeWonLeafCall::ComponentItemMeasure, 0, 0); }
// Parents the component to the list viewport and notifies it when attached
void ComponentListItem::SetViewport(ListArea*,Container* viewport)
{
    ee_won_leaf_call(static_cast<ListItem*>(this), ee_won_guicpp_compitem_call, EeWonLeafCall::ComponentItemViewport, 0, (intptr_t)viewport);
}
// Moves the item and its component
void ComponentListItem::SetPos(int x,int y) { ee_won_leaf_call(static_cast<ListItem*>(this), ee_won_guicpp_compitem_call, EeWonLeafCall::ComponentItemSetPos, x, y); }
// Draws the item's component, first stretching it to the list's paint width when the item is sizable (leafpaint.rs,
// R6 WON GUI round 36)
void ComponentListItem::Paint(Graphics& g,ListArea* area) { ee_won_leaf_call(static_cast<ListItem*>(this), ee_won_guicpp_compitem_call, EeWonLeafCall::ComponentItemPaint, (intptr_t)&g, (intptr_t)area); }
// Tests the point against the component
bool ComponentListItem::Contains(int x,int y) { return this->mComponent->Contains(x,y); }
// Forwards mouse enter to the component
void ComponentListItem::MouseEnter(ListArea*,int x,int y) { this->mComponent->MouseEnter(x,y); }
// Forwards mouse exit to the component
void ComponentListItem::MouseExit(ListArea*) { this->mComponent->MouseExit(); }
// Forwards a mouse move to the component
void ComponentListItem::MouseMove(ListArea*,int x,int y) { this->mComponent->MouseMove(x,y); }
// Forwards a mouse drag to the component
void ComponentListItem::MouseDrag(ListArea*,int x,int y) { this->mComponent->MouseDrag(x,y); }
// Forwards a mouse press to the component
void ComponentListItem::MouseDown(ListArea*,int x,int y,int button) { this->mComponent->MouseDown(x,y,button); }
// Forwards a mouse release to the component
void ComponentListItem::MouseUp(ListArea*,int x,int y,int button) { this->mComponent->MouseUp(x,y,button); }
// Destroys the list item, releasing its component (emitted at 0x006fdaa0)
ComponentListItem::~ComponentListItem() {}
}
