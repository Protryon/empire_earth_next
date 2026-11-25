// The list items' and backgrounds' leaf hosts (R6 WON GUI round 37: TextListItem.cpp with MenuBehavior.cpp,
// ComponentListItemAdapters.cpp, ListControlAdapters.cpp and Background.cpp). Text list items: size, sort order and
// painting (file name inferred). R6 WON round 34: the measure and the paint are Rust's
// (rust/crates/ee_won_gui/src/leafpaint.rs); this is their host, one statement per op (the menu items' host passes
// the others here).
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
#include <map>
#include <set>
#include "rust/won_gui/guidraw.h"
#include "rust/won_gui/guiwin.h"
#include "ee/wonapi/WONLobbyBackground.h"
#include <deque>
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

// ---- MenuBehavior.cpp until R6 WON GUI round 37
// Menu item measure and drawing (file name inferred; the popup menu's input is the Rust PopupMenu, menuobj.rs)
// The submenu delay, the swallowed first release, the keyboard mode's end, an item's size and the submenu's place
// are ee_won's guiwin module (rust/crates/ee_won_gui/src/guiwin.rs); the keys and the submenus' open, close and
// selection rules are its guimenu module (guimenu.rs); this unit keeps the menus and submenus; the item's paint is guidraw's menu_item.

namespace WONAPI {
// the menu item's own reads for leafpaint.rs (the text item's are TextListItem.cpp's)
static intptr_t MenuItemCall(void* inMe, EeWonLeafOp op, intptr_t a, intptr_t b)
{
    MenuItem* t = (MenuItem*)static_cast<TextListItem*>(inMe);
    switch (op) {
    case EeWonLeafOp::MenuDisabled: return t->mDisabled ? 1 : 0;
    case EeWonLeafOp::MenuHasSubMenu: return t->mSubMenu.get() != NULL ? 1 : 0;
    case EeWonLeafOp::MenuListSel: *(EeWonSelColor*)b = WONDrawSelOf(((ListArea*)a)->mSelColor); return 0;
    case EeWonLeafOp::MenuListViewWidth: return ((ListArea*)a)->GetViewWidth();
    default: return ee_won_guicpp_textitem_call(inMe, op, a, b);
    }
}
// Measures the menu item as text plus room for the icon and submenu arrow (32 wider, 4 taller)
void MenuItem::CalcDimensions(ListArea* area) { ee_won_leaf_call(static_cast<TextListItem*>(this), MenuItemCall, EeWonLeafCall::MenuItemMeasure, (intptr_t)area, 0); }
// Draws the menu item: icon, selection highlight with an icon box, text (embossed grey when disabled)
// and a submenu arrow; the box's bottom edge runs to x = height-1, not to the box width
void MenuItem::Paint(Graphics& g,ListArea* area)
{
    ee_won_leaf_call(static_cast<TextListItem*>(this), MenuItemCall, EeWonLeafCall::MenuItemPaint, (intptr_t)&g, (intptr_t)area);
}
// ---- the menu item's constructor and the popup menu's AddItem overloads (MenuItemLifetime.cpp and MenuAddItem.cpp
// until R6 WON GUI round 36)
// Creates an enabled menu item with the given text and listener and menu id 0
MenuItem::MenuItem(const GUIString& text,ComponentListener* listener)
    :TextListItem(text),mMenuId(0),mDisabled(false),mListener(listener) {}
// Adds a text item with a menu id and no listener (this overload is 0x006fd440; the listener overload
// below is 0x006fd460)
MenuItem* PopupMenu::AddItem(const GUIString& text,int id)
{
    return this->AddItem(text,static_cast<ComponentListener*>(NULL),id);
}
// Adds a text item with a listener and menu id and no icon (0x006fd460)
MenuItem* PopupMenu::AddItem(const GUIString& text,ComponentListener* listener,int id)
{
    return this->AddItem(text,static_cast<NativeImage*>(NULL),listener,id);
}
// Adds a menu item with text, icon, listener and menu id, resizes the menu to fit and returns the item
MenuItem* PopupMenu::AddItem(const GUIString& text,NativeImage* icon,ComponentListener* listener,int id)
{
    SmartPtr<MenuItem> item=new MenuItem(text,listener);
    item->mMenuId=id;
    item->mImage=icon;
    this->InsertItem(item.get());
    this->SetSizeEqualToTotal(-1,-1);
    return item.get();
}
}

// ---- ComponentListItemAdapters.cpp until R6 WON GUI round 37
// ComponentListItem: a list item that hosts a component (file name inferred); its size, viewport and place are
// Rust's (rust/crates/ee_won_gui/src/leafpaint.rs, R6 WON round 34) over this unit's host
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

// ---- ListControlAdapters.cpp until R6 WON GUI round 37
// List box text colour and number sort (file name inferred)
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

// ---- Background.cpp until R6 WON GUI round 37
// Component backgrounds: fill colour, border colour, tiled or stretched image, or a painted component

// The constructors over WONLobbyBackground.h's defaults (Init, 0x335cb0, is its member initialisers since R6 WON GUI
// round 36): the standard colour 13, a background that tiles an image, a fill and border colour, one drawn by a
// component (the out-of-line bodies keep the SmartPtr members' types incomplete in the header)
Background::Background() : mColor((int)0x8000000d) {}
Background::Background(Image* theImage) : mImage(theImage) {}
Background::Background(int theColor, int theBorderColor) : mColor(theColor), mBorderColor(theBorderColor) {}
Background::Background(Component* theComponent) : mComponent(theComponent) {}
// Sets the fill colour (-1 for none)
void Background::SetColor(int theColor)
{
	this->mColor = theColor;
}

// the background's reads and calls for leafpaint.rs (R6 WON round 34: the paint is Rust's), one statement each
extern "C" intptr_t ee_won_guicpp_background_call(void* inMe, EeWonLeafOp op, intptr_t a, intptr_t b)
{
	Background* t = (Background*)inMe;
	Graphics* g = (Graphics*)a;
	switch (op) {
	case EeWonLeafOp::BgHasImage: return t->mImage.get() != NULL ? 1 : 0;
	case EeWonLeafOp::BgStretch: return t->mStretchImage ? 1 : 0;
	case EeWonLeafOp::BgUseOffsets: return t->mUseOffsets ? 1 : 0;
	case EeWonLeafOp::BgHasComponent: return t->mComponent.get() != NULL ? 1 : 0;
	case EeWonLeafOp::BgImageWidth: return t->mImage->GetWidth();
	case EeWonLeafOp::BgImageHeight: return t->mImage->GetHeight();
	case EeWonLeafOp::BgColor: return t->mColor;
	case EeWonLeafOp::BgBorderColor: return t->mBorderColor;
	case EeWonLeafOp::BgDraw: WONDrawOne(*g, *(const EeWonDrawOp*)b, WONDrawSlots(NULL, NULL, t->mImage.get()), WONDrawNoCall()); return 0;
	case EeWonLeafOp::BgCompSetSize: t->mComponent->SetSize((int)a, (int)b); return 0;
	case EeWonLeafOp::BgCompInvalidate: t->mComponent->Invalidate(); return 0;
	case EeWonLeafOp::BgCompPaint: t->mComponent->Paint(*g); return 0;
	default: return 0;
	}
}

// Paints a rectangle: stretched image alone, or fill, border, tiled image (scrolled by the offsets) and component
void Background::Paint(Graphics &g, int xpos, int ypos, int width, int height, int xoff, int yoff)
{
	ee_won_leaf_background(this, ee_won_guicpp_background_call, (intptr_t)&g, xpos, ypos, width, height, xoff, yoff);
}

// ---- ListAreaToolTip.cpp until R6 WON GUI round 37
// ListAreaToolTip (R6 WON round 34): the tooltip a list box opens over an item it cuts off is the Rust tip's
// (rust/crates/ee_won_gui/src/tipobj.rs: the paint, DoTip and EndTip); this is its host, each op one of the original's
// statements (the class keeps the list and the item).
typedef EeWonTipOp To;
static_assert(StandardColor_ToolTipBack == EE_WON_TIP_COLOR_BACK && StandardColor_ToolTipText == EE_WON_TIP_COLOR_TEXT, "tipobj.rs colours");

// OpenPopup takes its parameters by reference
static void OpenWith(Window* inWindow, PopupParams inParams) { inWindow->OpenPopup(inParams); }

extern "C" intptr_t ee_won_guicpp_tip_call(void* inMe, EeWonTipOp op, intptr_t a, intptr_t b)
{
    ListAreaToolTip* t = (ListAreaToolTip*)inMe;
    Graphics* g = (Graphics*)a;
    switch (op) {
    case To::BasePaint: t->Component::Paint(*g); return 0;
    case To::ItemIs: return t->mItem.get() == (ListItem*)a ? 1 : 0;
    case To::HasItem: return t->mItem.get() != NULL ? 1 : 0;
    case To::HasParent: return t->mParent != NULL ? 1 : 0;
    case To::SetItem: t->mItem = (ListItem*)a; return 0;
    case To::ItemTop: return t->mItem->mTop;
    case To::ItemWidth: return t->mItem->mWidth;
    case To::ItemHeight: return t->mItem->mHeight;
    case To::HasOwnScheme: return t->GetOwnColorScheme() != NULL ? 1 : 0;
    case To::ListScheme: return (intptr_t)t->mListArea->GetColorScheme();
    case To::ListLeftPad: return t->mListArea->GetLeftPad();
    case To::ListRightPad: return t->mListArea->GetRightPad();
    case To::ListTopPad: return t->mListArea->GetTopPad();
    case To::ListVertOffset: return t->mListArea->GetVertOffset();
    case To::ListWidth: return t->mListArea->Width();
    case To::Width: return t->Width();
    case To::Height: return t->Height();
    case To::ScrollPaint: t->mListArea->ScrollPaint(*g); return 0;
    case To::ListLocalToRoot: t->mListArea->LocalToRoot(((int*)a)[0], ((int*)a)[1]); return 0;
    case To::SetSize: t->SetSize((int)a, (int)b); return 0;
    case To::OpenPopup: OpenWith(t->mListArea->GetRoot(), PopupParams(t, EE_WON_TIP_POPUP_FLAGS, t->mListArea, (int)a, (int)b)); return 0;
    case To::ClosePopup: t->mListArea->GetRoot()->ClosePopup(t); return 0;
    }
    return 0;
}
