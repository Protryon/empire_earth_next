// Text list items: size, sort order and painting (file name inferred). R6 WON round 34: the measure and the paint are
// Rust's (rust/crates/ee_won_gui/src/leafpaint.rs); this is their host, one statement per op (MenuBehavior.cpp's menu
// items' host passes the others here).
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONGUIDraw.h"
#include "rust/won_gui/leafpaint.h"
using namespace WONAPI;
typedef EeWonLeafOp Lo;
extern "C" intptr_t ee_won_guicpp_textitem_call(void* inMe, EeWonLeafOp op, intptr_t a, intptr_t b)
{
    TextListItem* t = (TextListItem*)inMe;
    ListArea* l = (ListArea*)a;
    switch (op) {
    case Lo::ItemHasFont: return t->mFont.get() != NULL ? 1 : 0;
    case Lo::ItemTakeListFont: t->mFont = l->mFont; return 0;
    case Lo::ItemColor: return (intptr_t)t->mColor;
    case Lo::ItemTakeListColor: t->mColor = l->GetTextColor(); return 0;
    case Lo::ItemHasImage: return t->mImage.get() != NULL ? 1 : 0;
    case Lo::ItemImageWidth: return t->mImage->GetWidth();
    case Lo::ItemImageHeight: return t->mImage->GetHeight();
    case Lo::ItemTextWidth: return t->mFont->GetStringWidth(t->mText);
    case Lo::ItemFontHeight: return t->mFont->GetHeight();
    case Lo::ItemWidthAt: return (intptr_t)&t->mWidth;
    case Lo::ItemHeightAt: return (intptr_t)&t->mHeight;
    case Lo::ItemSelected: return t->mSelected ? 1 : 0;
    case Lo::ItemListFlag: return l->ListFlagSet((int)b) ? 1 : 0;
    case Lo::ItemListSel: *(EeWonSelColor*)b = WONDrawSelOf(l->GetSelColor()); return 0;
    case Lo::ItemListPaintWidth: return l->GetPaintWidth();
    case Lo::ItemDraw: WONDrawOne(*(Graphics*)a, *(const EeWonDrawOp*)b, WONDrawSlots(t->mFont.get(), &t->mText, t->mImage.get()), WONDrawNoCall()); return 0;
    default: return 0;
    }
}
namespace WONAPI {
// Sizes the item from its text and optional image, taking the list's font and text colour when unset
void TextListItem::CalcDimensions(ListArea* inList) { ee_won_leaf_call(this, ee_won_guicpp_textitem_call, EeWonLeafCall::TextItemMeasure, (intptr_t)inList, 0); }
// Orders two text items by their text, ignoring case
int TextListItem::Compare(const ListItem* inOther,ListArea* inList) const
{
    return this->mText.CompareNoCase(static_cast<const TextListItem*>(inOther)->mText);
}
// Paints the item's image and vertically centred text, with the selection background behind a selected item
void TextListItem::Paint(Graphics& inGraphics,ListArea* inList)
{
    ee_won_leaf_call(this, ee_won_guicpp_textitem_call, EeWonLeafCall::TextItemPaint, (intptr_t)&inGraphics, (intptr_t)inList);
}
}
