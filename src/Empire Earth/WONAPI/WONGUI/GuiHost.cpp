// WONGUI's component objects in Rust (R6 WON rounds 21-22): the one outward call table, ee_won_guicpp_call
// (rust/crates/ee_won_gui/src/guihost.rs EeWonGuiOp), and the C++ left of the classes that moved: the TextArea's
// constructor and destructor and the segments it makes (the Rust TextArea is textobj.rs); the InputBox's
// constructor, destructor and numeric filter (the Rust InputBox is inputobj.rs); the Label's constructor and
// destructor (the Rust Label is labelobj.rs); the Splitter's (splitobj.rs); the Scrollbar's (barobj.rs); the Scroller's (scrollerobj.rs); the ScrollArea's (paneobj.rs); the ListArea's and its items'
// defaults (listobj.rs). Each op is a run of the
// original's C++ statements with no decision between them.
#include <windows.h>
#include <ctype.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <deque>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
#include <algorithm>
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/won_gui/barobj.h"
#include "rust/won_gui/gamefwd.h"
#include "rust/won_gui/guictl.h"
#include "rust/won_gui/guictor.h"
#include "rust/won_gui/guidraw.h"
#include "rust/won_gui/gamewin.h" // ee_won_text_seg_register
#include "rust/won_gui/guihost.h"
#include "rust/won_gui/guisplit.h"
#include "rust/won_gui/guitext.h"
#include "rust/won_gui/inputobj.h"
#include "rust/won_gui/labelobj.h"
#include "rust/won_gui/listobj.h"
#include "rust/won_gui/menuobj.h"
#include "rust/won_gui/multiobj.h"
#include "rust/won_gui/paneobj.h"
#include "rust/won_gui/scrollerobj.h"
#include "rust/won_gui/splitobj.h"
#include "rust/won_gui/textobj.h"
#include "rust/won_gui/treeobj.h"
#include "rust/won_gui/wrapobj.h"
#include "ee/wonapi/WONGUIDraw.h"
#include "ee/wonapi/WONGUIShell.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
#include "ee/wonapi/WONLobbyStretchImage.h"
#include <cctype>
#include <set>
#include "ee/wonapi/WONLobbyRichText.h"
#include <stdio.h>
#include "rust/won_gui/shell_msgbox.h"
#include "ee/wonapi/WONSkinLayout.h"
#include "ee/wonapi/WONLobbyPlatform.h" // CreateNativeWindow
#include "ee/wonapi/WONLobbyMSSkinImage.h"
using namespace WONAPI;

typedef EeWonGuiOp Op;
typedef EeWonGuiField Fd;

// the standard cursor a host op names (guihost.rs over cursorobj.rs's table; NULL for none)
static Cursor* CursorOf(intptr_t inCursor) { return (Cursor*)ee_won_gui_cursor(inCursor); }

// a component's services, the same for every component object (the members', Component's own calls and the
// container's are gamewin.rs common_op's in the game: R6 WON GUI round 37)
static intptr_t ComponentOp(Component* c, Op op, intptr_t a, intptr_t b)
{
    switch (op) {
    case Op::SetParentCursor: c->mParent->SetCursor(CursorOf(a)); return 0;
    // (FireEvent is gamewin.rs common_op's in the game: R6 WON GUI round 39; the type Rust's map, which the goldens
    // check against the enum: round 23's lists fired ListSelChanged here as InputTextChanged)
    // ClipboardTake, ClipboardSet, TextStore and Filter are guisplit.rs's over these and its texts (R6 WON GUI round 39)
    case Op::ClipboardText: return (intptr_t)NewGUIStringData(Clipboard::Get());
    case Op::ClipboardSetText: Clipboard::Set(*(const GUIString*)a); return 0;
    default: return 0;
    }
}

static TextSegment* Seg(intptr_t a) { return (TextSegment*)a; }
// a font's descriptor with the underline style (the text area's link font)
static FontDescriptor Underlined(FontDescriptor inDescriptor)
{
    inDescriptor.mStyle |= FontStyle_Underline;
    return inDescriptor;
}

extern "C" intptr_t ee_won_guicpp_text_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t /*d*/)
{
    TextArea* t = (TextArea*)inComp;
    switch (op) {
    // (the fields, ScrollTranslate and the segments' references are gamewin.rs list_host_op's in the game)
    // LinkActivated is guisplit.rs's over these two (R6 WON round 33); a segment's members, a link's state and the
    // copy's text are gamewin.rs text_host_op's in the game (R6 WON GUI round 40)
    case Op::TextDeliverLinkEvent: t->DeliverEvent(TextLinkActivatedEvent(t, Seg(a)), false); return 0;
    case Op::SegIsLink: return Seg(a)->IsLink() ? 1 : 0;
    case Op::TextDrawSeg: WONDrawOne(*(Graphics*)a, *(const EeWonDrawOp*)b, WONDrawSlots(Seg(c)->mFont.get(), &Seg(c)->mText), WONDrawNoCall()); return 0;
    // the constructor's statements (guictor.rs; the others, the selection colour and the added segments and parts are
    // gamefwd.rs area_op's in the game: R6 WON GUI round 41)
    case Op::CtorLinkFontUnderline: t->SetLinkFont(t->GetFont(Underlined(t->mFont->GetDescriptor()))); return 0;
    case Op::SegNew: return (intptr_t)new TextSegment;
    default: return ComponentOp(t, op, a, b);
    }
}

namespace WONAPI {
// Creates an empty text area with word wrap and selection on, the named TextArea/TextAreaLink fonts
// (the Rust area holds the flags, limits, indent and alignment; the statements are guictor.rs's)
TextArea::TextArea()
{
    ee_won_gui_construct(reinterpret_cast<void**>(&this->mRust), ee_won_text_new(this, ee_won_gui_text_game), this,
                         ee_won_gui_text_game, EeWonGuiCtor::TextArea, 0);
}
// Destroys the text area: the Rust area gives its segments back
TextArea::~TextArea() { ee_won_text_free(this->mRust); }

// Appends a text segment in the area's text colour (0x00713c60); the overloads below take a colour
void TextArea::AddSegment(const GUIString& text,bool lineBreak)
{
    this->AddSegment(text,this->mTextColor,lineBreak);
}
// Appends a text segment in the given colour and the area's font (the body at 0x00713c80)
void TextArea::AddSegment(const GUIString& text,int color,bool lineBreak)
{
    ee_won_gui_add_segment(this, ee_won_gui_text_game, &text, color, this->mFont.get(), lineBreak);
}
// Appends a text segment in the given colour and font (the body at 0x00713da0)
void TextArea::AddSegment(const GUIString& text,int color,Font* font,bool lineBreak)
{
    ee_won_gui_add_segment(this, ee_won_gui_text_game, &text, color, font, lineBreak);
}

// Appends text split at CR, LF and CRLF into one line-broken segment per line; the last line breaks
// only when lineBreak is set
void TextArea::AddFormatedText(const GUIString& text,bool lineBreak)
{
    ee_won_gui_add_formatted(this,ee_won_gui_text_game,&text,text.mData->mText,text.length(),sizeof(wchar_t),lineBreak);
}
}

// ---- the input box

extern "C" intptr_t ee_won_guicpp_input_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t /*d*/)
{
    InputBox* t = (InputBox*)inComp;
    switch (op) {
    // (the font measures, the text stores, the colours, the icon and the filter's presence are inputobj.rs
    // game_host_op's over the members in place: R6 WON GUI round 41)
    case Op::InputFilterText: return t->mInputFilter(t, *(GUIString*)a) ? 1 : 0;
    case Op::InputDrawSlots: WONDrawOne(*(Graphics*)a, *(const EeWonDrawOp*)b, WONDrawSlots(t->mFont.get(), &t->mText, t->mIcon.get()), WONDrawNoCall()); return 0;
    case Op::InputPaintBack: t->mBackground.Paint(*(Graphics*)a, 0, 0, ((const EeWonDrawOp*)b)->c, ((const EeWonDrawOp*)b)->d, ((const EeWonDrawOp*)b)->e, 0); return 0;
    case Op::InputPaintDisabledBack: t->mDisabledBackground.Paint(*(Graphics*)a, 0, 0, ((const EeWonDrawOp*)b)->c, ((const EeWonDrawOp*)b)->d, ((const EeWonDrawOp*)b)->e, 0); return 0;
    // the constructor's statements (guictor.rs)
    default: return ComponentOp(t, op, a, b);
    }
}

namespace WONAPI {
// Input filter that accepts only digits, with a leading minus sign allowed at the start of the box
bool InputBox::NumericFilter(InputBox *theInputBox, GUIString &theStr)
{
    return ee_won_input_numeric_filter(theStr.mData->mText, theStr.length(), (int)ee_won_input_call(theInputBox->mRust, EeWonInputCall::SelStart, 0, 0, 0));
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
static const EeWonInputAt kInputAt = {offsetof(InputBox, mText), offsetof(InputBox, mRealText), offsetof(InputBox, mIcon),
                                      offsetof(InputBox, mInputFilter), offsetof(InputBox, mUseDisabledColors),
                                      offsetof(InputBox, mFont), offsetof(InputBox, mTextColor),
                                      offsetof(InputBox, mDisabledTextColor), offsetof(InputBox, mSelColor), offsetof(Font, mRust)};
#pragma clang diagnostic pop
static_assert(sizeof(SmartPtr<Font>) == sizeof(void*) && sizeof(SelectionColor) == sizeof(EeWonSelColor), "inputobj.rs EeWonInputAt");
[[maybe_unused]] static const bool sInputAt = (ee_won_input_members_register(&kInputAt), true);
// Creates an empty input box with the InputBox font, system colours and the I-beam cursor (the Rust box holds the
// rest: the 500 ms blink, the 100-entry input history, the padding)
InputBox::InputBox()
{
    ee_won_gui_construct(reinterpret_cast<void**>(&this->mRust), ee_won_input_new(this, ee_won_gui_input_game), this,
                         ee_won_gui_input_game, EeWonGuiCtor::InputBox, 0);
}
// Destroys the input box: the Rust box goes
InputBox::~InputBox() { ee_won_input_free(this->mRust); }
}

// ---- the label

extern "C" intptr_t ee_won_guicpp_label_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t /*d*/)
{
    Label* t = (Label*)inComp;
    switch (op) {
    // (the wrapped text's methods are wrapobj.rs's in the game: R6 WON GUI round 40)
    case Op::PaintBackground: t->mBackground.Paint(*(Graphics*)a, 0, 0, (int)b, (int)c); return 0;
    case Op::LabelDrawWrapped: WONDrawOne(*(Graphics*)a, *(const EeWonDrawOp*)b, WONDrawSlots(NULL, NULL, NULL, t->mWrappedText.get()), WONDrawNoCall()); return 0;
    // the constructor's statements (guictor.rs)
    // (CtorWrapNew is wrapobj.rs label_new_text's: R6 WON GUI round 41)
    default: return ComponentOp(t, op, a, b);
    }
}

namespace WONAPI {
// Creates an empty, clipped label with the Label font, no wrapping and no background colour (the Rust label holds
// the colour, the top-left alignment and the zero padding)
Label::Label()
{
    ee_won_gui_construct(reinterpret_cast<void**>(&this->mRust), ee_won_label_new(this, ee_won_gui_label_game), this,
                         ee_won_gui_label_game, EeWonGuiCtor::Label, 0);
}
// Destroys the label: the Rust label goes
Label::~Label() { ee_won_label_free(this->mRust); }
}

// ---- the splitter

extern "C" intptr_t ee_won_guicpp_split_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
static Component* Comp(intptr_t a) { return (Component*)a; }

// a container's children (`a` a child `Component*`), the same for the splitter and the scroll bar
static intptr_t ContainerOp(Container* t, Op op, intptr_t a, intptr_t b, intptr_t c)
{
    switch (op) {
    // (the children's references, size and parent are gamewin.rs common_op's in the game: R6 WON GUI round 38)
    case Op::NewComponent: return (intptr_t)new Component;
    case Op::ChildLayoutDrag: ((Container*)Comp(a))->Fn2F6140(Comp(b), 3, (Container*)Comp(a), 0, 0, 0, 0, 0); return 0;
    default: return ComponentOp(t, op, a, b);
    }
}

namespace WONAPI {
// Creates an empty clipping splitter (the Rust splitter holds the split at 50% / 50% and no minimum pane sizes)
Splitter::Splitter()
{
    ee_won_gui_construct(reinterpret_cast<void**>(&this->mRust), ee_won_split_new(this, ee_won_gui_split_game), this,
                         ee_won_gui_split_game, EeWonGuiCtor::Splitter, 0);
}
// Destroys the splitter: the Rust splitter gives its buttons and drag handles back
Splitter::~Splitter() { ee_won_split_free(this->mRust); }
}

extern "C" intptr_t ee_won_guicpp_split_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t /*d*/)
{
    return ContainerOp((Splitter*)inComp, op, a, b, c); // (the buttons' calls are gamefwd.rs's: R6 WON GUI round 41)
}

// ---- the scroll bar

extern "C" intptr_t ee_won_guicpp_bar_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t /*d*/)
{
    Scrollbar* t = (Scrollbar*)inComp;
    switch (op) {
    case Op::NewButton: return (intptr_t)static_cast<Component*>(new Button);
    default: return ContainerOp(t, op, a, b, c);
    }
}

namespace WONAPI {
// Creates an empty vertical or horizontal scrollbar (the Rust bar holds the line steps 10, the page step 50 and a
// resizing thumb of at least 10)
Scrollbar::Scrollbar(bool vertical) { this->mRust=ee_won_bar_new(this,ee_won_guicpp_bar_call,vertical); }
// Destroys the scrollbar: the Rust bar gives the thumb, the track and the arrows back
Scrollbar::~Scrollbar() { ee_won_bar_free(this->mRust); }
}

// ---- the scroller (`a` the scroll area or a bar)

extern "C" intptr_t ee_won_guicpp_scroller_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t /*d*/)
{
    Scroller* t = (Scroller*)inComp;
    ScrollArea* area = static_cast<ScrollArea*>(Comp(a));
    switch (op) {
    case Op::ComponentHandleEvent: t->Component::HandleComponentEvent((ComponentEvent*)a); return 0;
    case Op::VirtSizeChanged: t->Virtual08(); return 0;
    case Op::VirtNotifyScrollChange: t->NotifyScrollChange(a != 0); return 0;
    case Op::ListenTo: Comp(a)->AddListener(new ComponentListenerComp(t)); return 0;
    case Op::AreaAdjustPosition: area->AdjustPosition(b != 0); return 0;
    case Op::AreaMouseWheel: return area->MouseWheel((int)b) ? 1 : 0;
    default: return ContainerOp(t, op, a, b, c);
    }
}

namespace WONAPI {
// Creates a scroller with no scroll area (the Rust scroller; a ScrollbarScroller's bars start without, both
// conditions Enable)
Scroller::Scroller() { this->mRust=ee_won_scroller_new(this,ee_won_guicpp_scroller_call); }
// Destroys the scroller: the Rust scroller gives the bars and the scroll area back (the implicit destructors
// 0x003001e0 and 0x00300060)
Scroller::~Scroller() { ee_won_scroller_free(this->mRust); }
}

// ---- the scroll area

extern "C" intptr_t ee_won_guicpp_pane_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t /*d*/)
{
    ScrollArea* t = (ScrollArea*)inComp;
    switch (op) {
    case Op::ContainerPaint: t->Container::Paint(*(Graphics*)a); return 0;
    case Op::VirtAdjustPosition: t->AdjustPosition(a != 0); return 0;
    case Op::VirtSizeChanged: t->Virtual08(); return 0;
    case Op::VirtScrollSizeChanged: t->ScrollSizeChanged(); return 0;
    case Op::VirtScrollPaint: t->ScrollPaint(*(Graphics*)a); return 0;
    case Op::PaneBackground: t->mBackground.Paint(*(Graphics*)a, 0, 0, t->Width(), t->Height(), (int)b, (int)c); return 0;
    case Op::ScrollerNotify: static_cast<Scroller*>(Comp(a))->NotifyScrollChange(b != 0); return 0;
    case Op::ScrollerAsBars:
        return (intptr_t)static_cast<Component*>(dynamic_cast<ScrollbarScroller*>(static_cast<Scroller*>(Comp(a))));
    case Op::NewScrollbarScroller: return (intptr_t)static_cast<Component*>(new ScrollbarScroller);
    // the constructor's statements (guictor.rs)
    case Op::CtorViewportNew: t->mViewport=new ScrollAreaViewport(t); return 0;
    default: return ContainerOp(t, op, a, b, c);
    }
}

namespace WONAPI {
// Creates an empty scroll area with its viewport as its child (the Rust area holds the scroll amounts 10 and every
// other number 0, no scroller)
ScrollArea::ScrollArea()
{
    ee_won_gui_construct(reinterpret_cast<void**>(&this->mPane), ee_won_pane_new(this, ee_won_guicpp_pane_call), this,
                         ee_won_guicpp_pane_call, EeWonGuiCtor::ScrollArea, 0);
}
// Destroys the scroll area: the Rust area goes (the implicit destructor 0x002fa790)
ScrollArea::~ScrollArea() { ee_won_pane_free(this->mPane); }
}

// ---- the list

static ListItem* Item(intptr_t a) { return (ListItem*)a; }

namespace WONAPI {
intptr_t ListAreaHostCall(ListArea* t, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    switch (op) {
    // (the fields, the scroll pane's and Component's methods, the references and the items' members are gamewin.rs's
    // in the game: list_host_op, common_op; R6 WON GUI round 38)
    case Op::VirtAdjustPosition: t->AdjustPosition(a != 0); return 0;
    case Op::VirtSort: return t->Sort() ? 1 : 0;
    case Op::VirtNotifyMouseItemGone: t->NotifyMouseItemGone(); return 0;
    case Op::VirtListSelChangedHook: t->ListSelChangedHook(); return 0;
    case Op::SetCursorDefault: t->SetCursor(t->GetDefaultCursor()); return 0;
    // PointerOutside, TipShow, TipHide, QueueSubMenu and QueueMouseUp are guisplit.rs's over these (R6 WON round 33)
    case Op::TipNew: t->mItemToolTip = new ListAreaToolTip(t); return 0;
    case Op::TipDo: t->mItemToolTip->DoTip(Item(a)); return 0;
    case Op::DeliverItemClicked: {
        const EeWonItemClick* k = (const EeWonItemClick*)b;
        t->DeliverEvent(ListItemClickedEvent(t, Item(a), static_cast<MouseButton>(k->button),
                                                 t->CheckDoubleClick(k->button), k->up, k->x, k->y), false);
        return 0;
    }
    case Op::TipExists: return t->mItemToolTip.get() != NULL ? 1 : 0;
    case Op::TipEndCall: t->mItemToolTip->EndTip(); return 0;
    case Op::SortItems: std::sort((ListItem**)a, (ListItem**)a + b, ListArea::LineSortPred(t, t->GetSortFunc(), (int)c)); return 0;
    case Op::LowerBound: return std::lower_bound((ListItem**)a, (ListItem**)a + b, Item(c), ListArea::LineSortPred(t, t->GetSortFunc(), (int)d)) - (ListItem**)a;
    case Op::Less: return ListArea::LineSortPred(t, t->GetSortFunc(), (int)c)(Item(a), Item(b)) ? 1 : 0;
    case Op::ItemSetPos: Item(a)->SetPos(0, (int)b); return 0;
    case Op::ItemContains: return Item(a)->Contains((int)b, (int)c) ? 1 : 0;
    case Op::ItemCanSelect: return Item(a)->CanSelect(t, (int)b, (int)c) ? 1 : 0;
    case Op::ItemCalcDimensions: Item(a)->CalcDimensions(t); return 0;
    case Op::ItemSetViewport: Item(a)->SetViewport(t, (Container*)b); return 0;
    case Op::ItemPaint: Item(a)->Paint(*(Graphics*)b, t); return 0;
    case Op::ItemMouseEnter: Item(a)->MouseEnter(t, (int)b, (int)c); return 0;
    case Op::ItemMouseExit: Item(a)->MouseExit(t); return 0;
    case Op::ItemMouseMove: Item(a)->MouseMove(t, (int)b, (int)c); return 0;
    case Op::ItemMouseDrag: Item(a)->MouseDrag(t, (int)b, (int)c); return 0;
    case Op::ItemMouseDown: Item(a)->MouseDown(t, (int)b, (int)c, (int)d); return 0;
    case Op::ItemMouseUp: Item(a)->MouseUp(t, (int)b, (int)c, (int)d); return 0;
    // the multi-column list and its rows (multiobj.rs)
    case Op::ItemCompare: return Item(a)->Compare(Item(b), t);
    case Op::CallSortFunc: return ((ListArea::SortFunc)a)(t, Item(b), Item(c));
    case Op::SetCursor: t->SetCursor(CursorOf(a)); return 0;
    case Op::NewViewport: return (intptr_t)new ScrollAreaViewport(t);
    // the tree list and its rows (treeobj.rs)
    case Op::NewTreeItemRaw: return (intptr_t)new TreeItem;
    case Op::ItemSetPosXY: Item(a)->SetPos((int)b, (int)c); return 0;
    case Op::DeliverTreeOpened: t->DeliverEvent(TreeItemOpenedEvent(t, (TreeItem*)a), false); return 0;
    // the popup menu (menuobj.rs)
    case Op::MenuItemOf: return (intptr_t)dynamic_cast<MenuItem*>(Item(a));
    // the constructors' statements (guictor.rs: the list area's, the multi-column list's, the menu's)
    case Op::CtorMenuBackground: static_cast<PopupMenu*>(t)->SetBackground(WONShellNew(EeWonShellKind::MsPopup)); return 0;
    default: return ComponentOp(t, op, a, b);
    }
}
}

extern "C" intptr_t ee_won_guicpp_list_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    return ListAreaHostCall((ListArea*)inComp, op, a, b, c, d);
}

namespace WONAPI {
// Creates an unselected, selectable list item with zero position and size
ListItem::ListItem() : mSelected(false), mSelectable(true), mTop(0), mWidth(0), mHeight(0) {}
// Draws nothing
void ListItem::Paint(Graphics&,ListArea*) {}
// Orders two items by their addresses (true when this one comes first in memory)
int ListItem::Compare(const ListItem* other,ListArea*) const { return this<other; }

// Creates an empty, unsorted list box with the ListArea font, 2-pixel padding and focus (the Rust list holds the
// default flags: highlighted icons, full-row select, partial lines shown)
ListArea::ListArea()
{
    ee_won_gui_construct(reinterpret_cast<void**>(&this->mRust), ee_won_list_new(this, ee_won_gui_list_game), this,
                         ee_won_gui_list_game, EeWonGuiCtor::ListArea, 0);
}
// Destroys the list box, closing an open item tooltip first; the Rust list gives its items back
ListArea::~ListArea()
{
    ee_won_gui_list_game(this, Op::TipEnd, 0, 0, 0, 0);
    ee_won_list_free(this->mRust);
}

// Adds a separator line to the popup menu and resizes the menu to fit (at least 100 pixels wide)
void PopupMenu::AddSeperator()
{
    this->InsertItem(new ComponentListItem(WONShellNew(EeWonShellKind::MsSeperator,6),true,false));
    this->SetSizeEqualToTotal(100,-1);
}
}

// ---- the multi-column list, its rows and header (multiobj.rs)

// a row's own calls out (its SetPos and its cells' references)
extern "C" intptr_t ee_won_guicpp_mrow_call(void* /*inComp*/, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t /*c*/, intptr_t /*d*/)
{
    switch (op) {
    case Op::ItemSetPos: Item(a)->SetPos(0, (int)b); return 0;
    default: return 0;
    }
}

extern "C" intptr_t ee_won_guicpp_mhead_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t /*d*/)
{
    MultiListHeader* t = (MultiListHeader*)inComp;
    switch (op) {
    case Op::RemoveChild: t->RemoveChild(Comp(a)); return 0;
    case Op::SetCursor: t->SetCursor(CursorOf(a)); return 0;
    default: return ContainerOp(t, op, a, b, c);
    }
}

namespace WONAPI {
// Creates a multi-column list area with the given column count and no sort (the Rust list holds the 5-pixel column
// border, no dynamic column, no width cap and auto grow off; the original reserved room for 11000 items)
MultiListArea::MultiListArea(int columns)
{
    ee_won_gui_construct(reinterpret_cast<void**>(&this->mMulti), ee_won_multi_new(this, ee_won_gui_list_game, this->mRust), this,
                         ee_won_gui_list_game, EeWonGuiCtor::MultiListArea, columns);
}
// Destroys the list: the Rust list gives its columns' viewports back (the ListArea base then its rows)
MultiListArea::~MultiListArea() { ee_won_multi_free(this->mMulti); }
// Puts a text cell (the list's font and colour, an optional image) into a row's column; returns the row
MultiListItem* MultiListArea::SetString(int row,int column,const GUIString& text,Image* image)
{
    SmartPtr<TextListItem> item=new TextListItem;
    item->mText=text;
    item->mColor=this->mTextColor;
    item->mFont=this->mFont;
    item->mImage=image;
    return this->SetCell(row,column,item.get());
}
// Creates an empty row (the implicit constructor at 0x3fa110); the Rust row holds the cells
MultiListItem::MultiListItem() { this->mRow=ee_won_mrow_new(this,ee_won_guicpp_mrow_call); }
// Destroys the row (0x00703db0): the Rust row gives its cells back
MultiListItem::~MultiListItem() { ee_won_mrow_free(this->mRow); }

// Creates a clipped list header with sort arrows on, no sort column, attached to a list area
// (the Rust header: no drag, no sort column, sort arrows on)
MultiListHeader::MultiListHeader(MultiListArea* area)
{
    ee_won_gui_construct(reinterpret_cast<void**>(&this->mHead), ee_won_mhead_new(this, ee_won_guicpp_mhead_call), this,
                         ee_won_guicpp_mhead_call, EeWonGuiCtor::MultiListHeader, (intptr_t)area);
}
// Destroys the header: the Rust header goes (its buttons are the container's children)
MultiListHeader::~MultiListHeader() { ee_won_mhead_free(this->mHead); }
}

// ---- the tree list and its rows (treeobj.rs)

// a row's own calls out (its SetPos, its walks' other rows, its references)
extern "C" intptr_t ee_won_guicpp_tnode_call(void* /*inComp*/, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t /*d*/)
{
    switch (op) {
    case Op::ItemSetPosXY: Item(a)->SetPos((int)b, (int)c); return 0;
    case Op::ItemContains: return Item(a)->Contains((int)b, (int)c) ? 1 : 0;
    default: return 0;
    }
}

namespace WONAPI {
// Creates a closed row at indent 0 with no parent and no index (the Rust row)
TreeItem::TreeItem() { this->mNode=ee_won_tnode_new(this,ee_won_guicpp_tnode_call); }
// Destroys the row: the Rust row gives its children and its wrapped item back
TreeItem::~TreeItem() { ee_won_tnode_free(this->mNode); }

// Creates an empty tree with an open root row, list flags 0x30 off and 0x200 on, and tree flags 3 (the Rust tree)
TreeArea::TreeArea()
{
    ee_won_gui_construct(reinterpret_cast<void**>(&this->mTree), ee_won_tree_new(this, ee_won_gui_list_game, this->mRust), this,
                         ee_won_gui_list_game, EeWonGuiCtor::TreeArea, 0);
}
// Destroys the tree: the Rust tree gives its root back (and so every row's)
TreeArea::~TreeArea() { ee_won_tree_free(this->mTree); }
}

// ---- the popup menu (menuobj.rs)

namespace WONAPI {
// Creates a popup menu with the menu font and colors, a popup background and 4-pixel padding (the Rust menu holds
// no submenu, the timer at 0 and mouse mode)
PopupMenu::PopupMenu()
{
    ee_won_gui_construct(reinterpret_cast<void**>(&this->mMenu), ee_won_menu_new(this, ee_won_gui_list_game, this->mRust), this,
                         ee_won_gui_list_game, EeWonGuiCtor::PopupMenu, 0);
}
// Destroys the menu: the Rust menu gives the submenu's item and the submenu back
PopupMenu::~PopupMenu() { ee_won_menu_free(this->mMenu); }
}

// The image and rectangle components' paints (R6 WON round 37 draw plans; ImageComponent.cpp folded in, round 38)
namespace WONAPI {
// Draws the image stretched to the component, once at the origin, or tiled over the component clipped to it
void ImageComponent::Paint(Graphics& g)
{
    Component::Paint(g);
    Image* anImage = this->mImage.get();
    WONDrawSlots aSlots;
    aSlots.mImages[0] = anImage;
    WONDrawRun(g,WONDrawOps([&](EeWonDrawOp* o, size_t n) { return ee_won_gui_draw_image_component(anImage!=NULL,this->mStretch,
        this->mTile,anImage ? anImage->GetWidth() : 0,anImage ? anImage->GetHeight() : 0,this->Width(),this->Height(),o,n); }),aSlots);
}
// Fills the component with the solid colour and draws the outline, each only when set
void RectangleComponent::Paint(Graphics &g)
{
    Component::Paint(g);
    WONDrawRun(g,WONDrawOps([&](EeWonDrawOp* o, size_t n) {
        return ee_won_gui_draw_rectangle(this->mSolidColor,this->mOutlineColor,this->Width(),this->Height(),o,n); }),WONDrawSlots());
}
// Creates a raised or sunken 3D frame of the given thickness in the scheme's highlight and shadow, unfilled
RectangleComponent3D::RectangleComponent3D(bool up, int theThickness) : mSolidColor(-1),
    mLightColor(ColorScheme::GetColorRef(StandardColor_3DHilight)), mDarkColor(ColorScheme::GetColorRef(StandardColor_3DShadow)),
    mThickness(theThickness), mUp(up) {}
// Draws a raised or sunken 3D frame of the given thickness over an optional solid fill
void RectangleComponent3D::Paint(Graphics &g)
{
    static_assert(COLOR_INVALID==0xffffffff,"guidraw.rs rectangle_3d_paint compares with -1");
    WONDrawRun(g,WONDrawOps([&](EeWonDrawOp* o, size_t n) { return ee_won_gui_draw_rectangle_3d(this->mUp,this->mThickness,
        this->mSolidColor,this->mLightColor,this->mDarkColor,this->Width(),this->Height(),o,n); }),WONDrawSlots());
}
}

// the hosts behind the game's (guisplit.rs ee_won_gui_text_game ..)
[[maybe_unused]] static const bool sSplitHosts = (ee_won_gui_split_register(EeWonGuiSplitHost::Text, ee_won_guicpp_text_call),
    ee_won_gui_split_register(EeWonGuiSplitHost::Input, ee_won_guicpp_input_call),
    ee_won_gui_split_register(EeWonGuiSplitHost::Label, ee_won_guicpp_label_call),
    ee_won_gui_split_register(EeWonGuiSplitHost::Split, ee_won_guicpp_split_call),
    ee_won_gui_split_register(EeWonGuiSplitHost::List, ee_won_guicpp_list_call), true);
// where wrapobj.rs reads its holders' wrapped texts (R6 WON GUI round 40; the text a Rust object since round 41)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
static const EeWonWrapAt kWrapAt = {offsetof(Label, mWrappedText), offsetof(MSButtonBase, mText), offsetof(Font, mRust)};
#pragma clang diagnostic pop
[[maybe_unused]] static const bool sWrapAt = (ee_won_wrap_register(&kWrapAt), true);
// where the Rust area reads a segment's members (gamewin.rs text_host_op, R6 WON GUI round 40)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
static const EeWonSegAt kSegAt = {offsetof(TextSegment, mText), offsetof(TextSegment, mColor), offsetof(TextSegment, mFont),
    offsetof(TextSegment, mLineBreak), offsetof(TextLink, mPressed), offsetof(TextLink, mDownColor),
    offsetof(TextLink, mNormalColor), offsetof(Font, mRust)};
#pragma clang diagnostic pop
[[maybe_unused]] static const bool sSegAt = (ee_won_text_seg_register(&kSegAt), true);
// where gamefwd.rs writes the list's sort function and the text area's fonts and colours and reads a scroll area's
// viewport (R6 WON GUI round 41)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
static const EeWonAreaAt kAreaAt = {offsetof(ListArea, mSortFunc), offsetof(TextArea, mFont), offsetof(TextArea, mLinkFont),
    offsetof(TextArea, mTextColor), offsetof(TextArea, mLinkColor), offsetof(TextArea, mSelColor), offsetof(ScrollArea, mViewport), offsetof(Label, mBackground),
    offsetof(InputBox, mDisabledBackground), offsetof(TabCtrl, mContainerLayout)};
#pragma clang diagnostic pop
[[maybe_unused]] static const bool sAreaAt = (ee_won_area_members_register(&kAreaAt), true);

// ---- TextFormat.cpp until R6 WON GUI round 39
// Formatted text for text areas: expands <n>, <cN>, <fN>, <sN> and <lN> commands
// The format's parse and its run are ee_won's guitext module (rust/crates/ee_won_gui/src/guitext.rs, format_ops and
// ee_won_gui_text_append_formatted: the pending text, the colour and the font; R6 WON GUI round 36); this unit is
// the call (its four calls on the area are guitext.rs's game sink: R6 WON GUI round 41).
namespace WONAPI {
// Appends format text to a text area, expanding <n> line breaks, <cN>/<fN> colour and font changes and
// <sN>/<lN> inserted strings and segments (1-based indices; "<<" is a literal '<')
void LobbyAppendFormattedText(TextArea* area,const GUIString& format,
    const GUIString*const* strings,int numStrings,const int* colors,int numColors,
    Font*const* fonts,int numFonts,TextSegmentBase*const* segments,int numSegments,
    bool lineBreak)
{
    EeWonFormatArgs args = { (const void*const*)strings,numStrings,colors,numColors,(void*const*)fonts,numFonts,(void*const*)segments,numSegments };
    ee_won_gui_text_append_formatted(area,ee_won_gui_text_game_sink(),format.mData->mText,format.length(),&args,lineBreak,area->mTextColor,area->mFont.get());
}
}

// ---- BrowserComponent.cpp until R6 WON GUI round 39
// The lobby's page view (layout type BrowserComponent): the message of the day, the terms of use and the help and
// news tabs. WON's HTML rendering is not ported (docs/RUST_PLAN.md D8): a page is shown as plain text in a TextArea,
// HTML pages with their tags dropped and entities decoded by ee_won (rust/crates/ee_won_net/src/plaintext.rs). There are
// no links, so no history; a web address is shown as text (the WON servers are gone).
// R6 WON round 34: the class is the Rust record's (rust/crates/ee_won_gui/src/browserobj.rs: its data, Go, ShowFiles,
// the page reads, Refresh, Reset, SetPageText, SetScroller, AddedToParent, SetTransparent); this is its host, each op
// one of the original's statements, and the constructor and destructor.
typedef EeWonBrowserOp Bo;

extern "C" intptr_t ee_won_guicpp_browser_call(void* inMe, EeWonBrowserOp op, intptr_t a, intptr_t b)
{
    BrowserComponent* t = (BrowserComponent*)inMe;
    Scroller* s = (Scroller*)a;
    switch (op) {
    case Bo::NewView: t->mHTMLView = new TextArea; return 0;
    case Bo::SameSize: t->Fn2F6140(s, 3, t, 0, 0, 0, 0, 0); return 0; // CLI_SameSize = 3
    case Bo::NewScrollContainer:
        return (intptr_t)static_cast<Scroller*>(static_cast<ScrollbarScroller*>(WONShellNew(EeWonShellKind::MsScrollContainer, (intptr_t)static_cast<ScrollArea*>(t->mHTMLView.get()))));
    case Bo::BaseAddedToParent: t->Container::AddedToParent(); return 0;
    case Bo::ViewNoBackground: t->mHTMLView->SetBackground(Background(-1, -1)); return 0;
    // (the scroller, the references and Invalidate are browserobj.rs's: R6 WON GUI round 40; the view's and the
    // scroller's calls, round 41)
    default: return 0;
    }
    return 0;
}

namespace WONAPI {
// Creates the view: an empty, word-wrapped, read-only text area (the Rust record's constructor)
// where browserobj.rs reads the view (R6 WON GUI round 41)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
[[maybe_unused]] static const bool sViewAt = (ee_won_browser_view_register(offsetof(BrowserComponent, mHTMLView)), true);
#pragma clang diagnostic pop
BrowserComponent::BrowserComponent() { this->mRust = ee_won_browser_new(this, ee_won_guicpp_browser_call, sizeof(wchar_t)); }
BrowserComponent::~BrowserComponent() { ee_won_browser_free(this->mRust); }
}

// ---- GuiShell.cpp until R6 WON GUI round 39
// The host of WONGUI's generic widget shells (ee/wonapi/WONGUIShell.h; rust/crates/ee_won_gui/src/shell.rs): a kind's
// calls the base classes' Rust objects do not answer in the game (the base call, a draw, the wrapped text, a new
// widget, a base class's C++ members and layouts), and the maker of a kind's widget.

typedef EeWonShellHostOp H;

static Component* NewBase(EeWonShellBaseNew k)
{
    switch (k) {
    case EeWonShellBaseNew::InputBox: return new InputBox;
    case EeWonShellBaseNew::ListArea: return new ListArea;
    case EeWonShellBaseNew::MultiListArea: return new MultiListArea;
    case EeWonShellBaseNew::Button: return new Button;
    case EeWonShellBaseNew::WonButton: return new WONButton;
    default: return new Label;
    }
}

extern "C" intptr_t WONAPI::ee_won_guicpp_shell_call(void* inShell, EeWonShellHostOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    WONShell* s = (WONShell*)inShell;
    Component* t = s ? s->mComp : NULL; // (the skin's calls have no shell: skinobj.rs)
    switch (op) {
    case H::Base: return s->BaseCall((EeWonShellVirt)a, b, c, d);
    // (the MS buttons' wrapped texts are wrapobj.rs's in the game: R6 WON GUI round 40)
    case H::SameSize: {
        SmartPtr<ChildLayoutInstructionBase> l = new ChildLayoutInstructionBase(EeWonLayoutKind::SameSize, NULL, NULL, 0, 0, 0, true, true);
        for (intptr_t x : {a, b, c}) l->AddComponent((Component*)x);
        l->Execute(static_cast<Container*>(t));
        return 0;
    }
    case H::CenterLayout: {
        SmartPtr<CenterLayout> l = new CenterLayout(t, (Component*)a, true, false);
        if (b) l->Add((Component*)b);
        if (c) l->Add((Component*)c);
        static_cast<Container*>(t)->AddChildLayout(l.get());
        return 0;
    }
    case H::New: return (intptr_t)WONShellNew((EeWonShellKind)a, b, c, d);
    case H::NewBase: return (intptr_t)NewBase((EeWonShellBaseNew)a);
    case H::SelfVirt: t->Virtual08(); return 0;
    case H::SkinCall: return ee_won_guicpp_skin_call(t, EeWonGuiOp::SkinLay, a, b, c, d);
    case H::Layout: { const int* r = (const int*)d; static_cast<Container*>(t)->Fn2F6140((Component*)a, (int)b, (Component*)c, r[0], r[1], r[2], r[3], r[4]); return 0; }
    // (a combo box's parts are comboobj.rs's in place: R6 WON GUI round 41)
    // (an image's size and duplicate, the named fonts, the WONButtons' calls and a label's text are shell.rs's in the
    // game: R6 WON GUI round 41)
    case H::DrawImages: {
        WONDrawSlots aSlots;
        for (int i = 0; i < 4; i++) aSlots.mImages[i] = ((Image* const*)c)[i];
        WONDrawNoCall aNoCall;
        WONDrawOne(*(Graphics*)a, *(const EeWonDrawOp*)b, aSlots, aNoCall);
        return 0;
    }
    case H::BtnCopy: static_cast<Button*>((Component*)a)->CopyAttributes(static_cast<Button*>((Component*)b)); return 0;
    case H::ClickSound: static_cast<Button*>((Component*)a)->SetClickSound((Sound*)b); return 0;
    case H::VDesiredSize: { int* wh = (int*)b; ((Component*)a)->GetDesiredSize(wh[0], wh[1]); return 0; }
    case H::NewStretchImage: { ImageComponent* i = new ImageComponent; i->mStretch = true; i->SetImage((Image*)a); return (intptr_t)static_cast<Component*>(i); }
    case H::NewMsImage: return (intptr_t)WONNewMsImage((unsigned char)a, (unsigned char)b, (int)c, (int)d);
    // MSMessageBox::Msg's making of the box and its window and its kind op (shell_msgbox.rs ee_won_msgbox_run; the
    // window's calls and the focus are gamewin.rs msgbox_op's since R6 WON GUI round 38)
    case H::MsgBoxNew: return (intptr_t)WONShellNew(EeWonShellKind::MsMessageBox, a);
    case H::MsgWinNew: return (intptr_t)CreateNativeWindow();
    case H::MsgShellOp: return WONShell::Op((Component*)a, (EeWonShellOp)b);
    default: return 0; // the component's, button's and other widgets' calls: their Rust objects (shell.rs Sh)
    }
}

Component* WONAPI::WONShellNew(EeWonShellKind k, intptr_t a, intptr_t b, intptr_t c)
{
    switch (ee_won_shell_base(k)) {
    case EeWonShellBase::Button: return new WONShellButton(k, a, b, c);
    case EeWonShellBase::MsButton: return new WONShellMsButton(k, a, b, c);
    case EeWonShellBase::Input: return new WONShellInput(k, a, b, c);
    case EeWonShellBase::Container: return new WONShellContainer(k, a, b, c);
    case EeWonShellBase::Combo: return new WONShellCombo(k, a, b, c);
    case EeWonShellBase::Scrollbar: return new WONShellBar(k, a, b, c);
    case EeWonShellBase::Scroller: return new WONShellScroller(k, a, b, c);
    case EeWonShellBase::Header: return new WONShellHeader(k, a, b, c);
    case EeWonShellBase::Tab: return new WONShellTab(k, a, b, c);
    case EeWonShellBase::Dialog: return new WONShellDialog(k, a, b, c);
    case EeWonShellBase::WonButton: return new WONButton(k);
    default: return new WONShellComp(k, a, b, c);
    }
}

// MSMessageBox::Msg, PrepareWindow and DoDialog: the box in a dialog window (style 0x59) titled and sized for it, its
// first button focused, run modally, the window closed (shell_msgbox.rs ee_won_msgbox_run since R6 WON GUI round 36)
int WONAPI::MSMessageBoxMsg(Window* inParent, const GUIString& inTitle, const GUIString& inMessage,
                            const GUIString& inButton1, const GUIString& inButton2, const GUIString& inButton3)
{
    static const GUIString kOk("Ok");
    // the texts (shell_msgbox.rs EeWonMsgBoxTexts): the title, the message, the three buttons, the "Ok" for none
    intptr_t aTexts[6] = {(intptr_t)&inTitle, (intptr_t)&inMessage, (intptr_t)&inButton1, (intptr_t)&inButton2, (intptr_t)&inButton3, (intptr_t)&kOk};
    return ee_won_msgbox_run(ee_won_guicpp_shell_call, (intptr_t)inParent, (const EeWonMsgBoxTexts*)aTexts);
}
