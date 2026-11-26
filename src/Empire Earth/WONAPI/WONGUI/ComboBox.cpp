// The containers' controls' hosts (R6 WON GUI round 37: ComboBox.cpp with DialogBehavior.cpp, TabCtrl.cpp,
// SkinnedContainers.cpp and ChildLayouts.cpp).
// ComboBox (R6 WON round 28): the Rust ComboBox's calls out (rust/crates/ee_won_gui/src/comboobj.rs, EeWonGuiOp's combo
// ops; the container's fall back to Container.cpp's), its constructor, destructor and list box setter. The original's
// ComboBox.cpp (the list box's size and popup, the selection, the keys, the events, no-typing, enabling) is the Rust
// object; each op here is a run of its C++ statements with no decision between them.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/won_gui/comboobj.h"
#include "rust/won_gui/guihost.h"
#include <set>
#include "rust/won_gui/dialogobj.h"
#include "rust/won_gui/guictl.h"
#include "rust/won_gui/tabobj.h"
#include "ee/wonapi/WONGUIDraw.h"
#include "ee/wonapi/WONGUIShell.h"
#include "ee/wonapi/WONSkinLayout.h"
#include <string.h>
#include "rust/won_gui/layoutobj.h"
using namespace WONAPI;

typedef EeWonGuiOp Op;

static Component* Comp(intptr_t a) { return (Component*)a; }
static TextListItem* Item(intptr_t a) { return (TextListItem*)a; }

// the ops on a child component (`a`) that the combo box's, the dialog's, the tab controls' and the layouts'
// hosts share, and their own base's; any other the container's (none for a layout, `t` NULL)
static intptr_t CtlOp(Container* t, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    switch (op) {
    // (a child's place, size, visibility, control id and references are gamewin.rs common_op's in the game)
    case Op::ChildSetEnabled: Comp(a)->SetEnabled(b != 0); return 0;
    case Op::ButtonSetFlags: static_cast<Button*>(Comp(a))->SetButtonFlags((int)b, c != 0); return 0;
    case Op::ButtonSetCheck: static_cast<Button*>(Comp(a))->SetCheck(b != 0, false); return 0;
    case Op::DlgClick: static_cast<Button*>(Comp(a))->Fn2F3440(); return 0;
    case Op::ComponentHandleEvent: t->Component::HandleComponentEvent((ComponentEvent*)a); return 0;
    default: return t != NULL ? ContainerHostCall(t, op, a, b, c, d) : 0;
    }
}

extern "C" intptr_t ee_won_guicpp_combo_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    ComboBox* t = static_cast<ComboBox*>((Component*)inComp);
    switch (op) {
    // (the list area's methods and an item's width are comboobj.rs game_list_op's in the game: R6 WON GUI round 38)
    case Op::ComboPart:
        switch (a) {
        case 0: return (intptr_t)static_cast<Component*>(t->mEdit.get());
        case 1: return (intptr_t)static_cast<Component*>(t->mButton.get());
        case 2: return (intptr_t)static_cast<Component*>(t->mListBox.get());
        default: return (intptr_t)static_cast<Component*>(t->mListArea.get());
        }
    case Op::ComboInsertString: return (intptr_t)t->mListArea->InsertString(*(const GUIString*)a, (int)b);
    case Op::ComboDropItem: { SmartPtr<TextListItem> held = Item(a); return 0; }
    case Op::ComboSetListBox: t->mListBox = static_cast<Scroller*>(Comp(a)); return 0;
    case Op::ComboBoxArea: return (intptr_t)static_cast<Component*>(static_cast<Scroller*>(Comp(a))->GetScrollArea());
    case Op::ComboEditClear: t->mEdit->SetText(GUIString(""), a != 0); return 0;
    case Op::ComboEditItemText: t->mEdit->SetText(Item(a)->mText, true); return 0;
    case Op::ComboEditSetText: t->mEdit->SetText(*(const GUIString*)a, b != 0); return 0;
    case Op::ComboEditUndo: t->mEdit->SetText(t->mUndoStr, true); return 0;
    case Op::ComboUndoSave: t->mUndoStr = t->mEdit->GetText(); return 0;
    case Op::ComboEditSelect: t->mEdit->SelectRange((int)a, (int)b); return 0;
    case Op::ComboEditAllowSelection: t->mEdit->SetAllowSelection(a != 0); return 0;
    case Op::ComboItemTextLength: return Item(a)->mText.length();
    case Op::ComboDeliverSel: t->DeliverEvent(ComboSelChangedEvent(t, Item(a), b != 0), false); return 0;
    case Op::ComboItemTextIs: return Item(a)->mText == (b == 0 ? t->mEdit->GetText() : *(const GUIString*)b) ? 1 : 0; // (b 0: the edit's)
    case Op::ComboOpenPopup: {
        Window* window = t->GetRoot();
        int x = 0, y = t->Height();
        t->LocalToRoot(x, y);
        PopupParams params(t->mListBox.get(), 0x2347, t, x, y);
        params.mInputComponent = t; // input component (+0x08)
        params.mAvoidRect.y = y - t->Height();
        params.mAvoidRect.height = t->Height();
        window->OpenPopup(params);
        return 0;
    }
    case Op::ComboRelayout: t->Relayout(); return 0;
    case Op::ComboUnlisten: t->mListBox->AddListener(NULL); return 0;
    case Op::ClosePopup: t->GetRoot()->ClosePopup(Comp(a)); return 0;
    case Op::ListenTo: Comp(a)->AddListener(new ComponentListenerComp(t)); return 0;
    default: return CtlOp(t, op, a, b, c, d);
    }
}

namespace WONAPI {
// Creates a combo box that takes focus and passes input to its children (the Rust combo box: the list hidden, a
// 200 x 150 list box)
ComboBox::ComboBox() : mCombo(ee_won_combo_make(static_cast<Component*>(this),ee_won_guicpp_combo_call,this->mCt)) {}
// Destroys the combo box, first detaching it from its list box's listeners (the Rust combo box)
ComboBox::~ComboBox() { ee_won_combo_free(this->mCombo); }
// Sets the popup list box and attaches the list area to it if it is not already its scroll area
void ComboBox::SetListBox(Scroller* inListBox)
{
    this->BCall(EeWonComboCall::SetListBox,(intptr_t)static_cast<Component*>(inListBox));
}
}

// ---- DialogBehavior.cpp until R6 WON GUI round 37
// Dialog (R6 WON round 26): the Rust Dialog's calls out (rust/crates/ee_won_gui/src/dialogobj.rs, EeWonGuiOp's dialog
// ops; the container's fall back to Container.cpp's), its constructor, destructor and default button. The original's
// DialogBehavior.cpp (dragging, focus, Escape and Enter, the default button) is the Rust object.

extern "C" intptr_t ee_won_guicpp_btn_call(void*, EeWonGuiOp, intptr_t, intptr_t, intptr_t, intptr_t); // Button.cpp
extern "C" intptr_t ee_won_guicpp_dlg_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    Dialog* t = static_cast<Dialog*>((Component*)inComp);
    switch (op) {
    // (Button::SetDialogDefaultButton and UnSetDialogDefaultButton, 0x2f31c0 and 0x2f3230: the Rust button's statics)
    case Op::DlgDefaultButton: ee_won_btn_static(ee_won_guicpp_btn_call, a ? EeWonBtnStatic::SetDialogDefault : EeWonBtnStatic::UnSetDialogDefault, b); return 0;
    // (the original's BringToTop compared the child's parent with `this` before reading itself; the Rust asks only
    // with a next sibling, so a parent exists, and the guard keeps a NULL parent unread: round 30's hazard)
    case Op::DlgBringToTop: if (t->mParent != NULL) t->mParent->BringToTop(t); return 0;
    case Op::DlgInvalidateRect: t->InvalidateRect(*(WONRectangle*)a); return 0;
    case Op::DlgEndDialog: t->GetRoot()->EndDialog((int)a, (Component*)b); return 0;
    case Op::DlgIsModal: return t->GetRoot()->mManager->GetModalComponent() == t ? 1 : 0;
    case Op::DlgIsDefault: return ((Button*)(Component*)a)->IsDefaultButton() ? 1 : 0;
    default: return CtlOp(t, op, a, b, c, d);
    }
}

namespace WONAPI {
// Creates a focusable, draggable dialog that grabs its background, stays inside the root window and ignores Escape
// (the Rust dialog: no drag, drag allowed and kept in the root, the Escape result -1, no default button)
Dialog::Dialog() : mDlg(ee_won_dlg_make(static_cast<Component*>(this),ee_won_guicpp_dlg_call,this->mCt)) {}
// Destroys the dialog: the Rust dialog gives the default button back (the implicit destructor in the original)
Dialog::~Dialog() { ee_won_dlg_free(this->mDlg); }
// Sets the button that Enter activates (held by reference)
void Dialog::SetDefaultButton(Button* button) { this->DCall(EeWonDlgCall::SetDefaultButton,(intptr_t)static_cast<Component*>(button)); }
}

// ---- TabCtrl.cpp until R6 WON GUI round 37
// TabBar and TabCtrl (R6 WON round 27): the Rust objects' calls out (rust/crates/ee_won_gui/src/tabobj.rs, EeWonGuiOp's
// tab ops; the container's fall back to Container.cpp's), the constructors and destructors. The original's
// TabCtrl.cpp (the tabs' places and selection, the bar's scroll and arrows, the page area, focus and keys) is the
// Rust objects; each op here is a run of its C++ statements with no decision between them.

extern "C" intptr_t ee_won_guicpp_tab_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    Container* t = static_cast<Container*>((Component*)inComp);
    switch (op) {
    case Op::TabTextIs: return static_cast<Button*>(Comp(a))->GetText() == *(const GUIString*)b ? 1 : 0;
    case Op::TabAddLayoutPoint: t->AddLayoutPoint(static_cast<LayoutPoint*>(Comp(a))); return 0;
    case Op::TabNewContainer: return (intptr_t)static_cast<Component*>(new Container);
    case Op::TabNewBar: return (intptr_t)static_cast<Component*>(new TabBar((EeWonTabCtrl*)a));
    case Op::TabBarOf: return (intptr_t)static_cast<TabBar*>(Comp(a))->mBar;
    case Op::TabNewPoint: return (intptr_t)static_cast<Component*>((static_cast<TabCtrl*>(t)->mContainerLayout = new LayoutPoint, static_cast<TabCtrl*>(t)->mContainerLayout.get()));
    default: return CtlOp(t, op, a, b, c, d);
    }
}

namespace WONAPI {
// Creates a tab bar for its tab control (the Rust bar: no tab, none selected, a 4-pixel overlap)
TabBar::TabBar(EeWonTabCtrl* inCtrl) : mBar(ee_won_tabbar_new(static_cast<Component*>(this),ee_won_guicpp_tab_call,this->mCt,inCtrl)) {}
// Destroys the tab bar: the Rust bar gives the buttons back
TabBar::~TabBar() { ee_won_tabbar_free(this->mBar); }
// Creates a tab control with 5-pixel padding and no tabs: the Rust control makes its tab bars and their containers
// (the TabNew* ops), holds them, sets them up and adds them with the page area's layout point (tabobj.rs make_parts)
TabCtrl::TabCtrl() { ee_won_tab_make(static_cast<Component*>(this),ee_won_guicpp_tab_call,this->mCt,&this->mTab); }
// Destroys the tab control: the Rust control gives the arrows back (its members go after)
TabCtrl::~TabCtrl() { ee_won_tab_free(this->mTab); }
}

// ---- SkinnedContainers.cpp until R6 WON GUI round 37
// The host of the skins' layouts (R6 WON round 37: rust/crates/ee_won_gui/src/skinlay.rs lays out MSComboBox,
// MSScrollContainer, MSListCtrl, WONComboBox, WONScrollContainer and WONListCtrl and sizes WONComboBox,
// MSEtchedFrameGroup and WONSimpleButton). R6 WONLobby round 23: the WON skin's widgets are shell kinds
// (shell_won.rs) that answer their own fields, header and images; this answers the combo boxes' and scroll
// containers' parts and the calls on other components.
// ---- the skins' layouts' host (skinlay.rs EeWonSkinLay): the receiver's parts and fields by the layout's kind, and the
// calls each layout makes
namespace WONAPI {
typedef EeWonSkinLay SL;
typedef EeWonSkinLayKind SK;
typedef EeWonSkinPart SP;
typedef EeWonSkinField SF;
template <class C> static intptr_t SkinScrollPart(C* t, intptr_t which)
{
    switch ((SP)which) {
    case SP::ScrollArea: return (intptr_t)static_cast<Component*>(t->GetScrollArea());
    case SP::VertBar: return (intptr_t)static_cast<Component*>(t->GetScrollbar(true));
    case SP::HorzBar: return (intptr_t)static_cast<Component*>(t->GetScrollbar(false));
    default: return 0;
    }
}
// the parts the kinds do not hold (a combo box's, a scroll container's: their base classes'); fields are the kinds'
static intptr_t SkinPart(Component* c, SK k, intptr_t which)
{
    switch (k) {
    case SK::MsCombo: case SK::WonCombo: case SK::WonComboSize: {
        ComboBox* t = static_cast<ComboBox*>(c);
        return (SP)which == SP::Button ? (intptr_t)static_cast<Component*>(t->mButton.get())
             : (SP)which == SP::Edit ? (intptr_t)static_cast<Component*>(t->mEdit.get()) : (intptr_t)static_cast<Component*>(t->mListArea.get());
    }
    case SK::MsScroll: case SK::MsList: case SK::WonScroll: case SK::WonList: case SK::WonListTextHeader: case SK::WonListHeader:
        return SkinScrollPart(static_cast<ScrollbarScroller*>(c), which);
    default: return 0;
    }
}
}

extern "C" intptr_t ee_won_guicpp_skin_call(void* inMe, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    Component* m = (Component*)inMe;
    Component* x = (Component*)b; // (the bounds and visibility the layouts read are compmem.rs's block)
    if (op != EeWonGuiOp::SkinLay) return 0;
    switch ((SL)a) {
    case SL::Part: return SkinPart(m, (SK)b, c);
    case SL::SetPosSize: x->SetPosSize(((const int*)c)[0], ((const int*)c)[1], ((const int*)c)[2], ((const int*)c)[3]); return 0;
    case SL::DesiredSize: x->GetDesiredSize(((int*)c)[0], ((int*)c)[1]); return 0;
    case SL::SetLeftPad: static_cast<InputBox*>(x)->SetLeftPad((int)c); return 0;
    case SL::ListCount: return static_cast<ListArea*>(x)->GetNumItems();
    case SL::ListTotalWidth: return static_cast<ListArea*>(x)->GetTotalWidth();
    case SL::SetColumnWidth: static_cast<MultiListArea*>(static_cast<ScrollbarScroller*>(m)->GetScrollArea())->SetColumnWidth((int)b, (int)c, false); return 0;
    default: return 0;
    }
}

// ---- ChildLayouts.cpp until R6 WON GUI round 37
// The child layouts (R6 WON round 31): the Rust layout manager's and instructions' calls out
// (rust/crates/ee_won_gui/src/layoutobj.rs, EeWonGuiOp's layout ops), the instruction shell's Make and destructor, and
// CenterLayout::Center (R6 WON GUI round 35: the manager is the Rust container's, the instruction classes one shell). The original's
// ChildLayouts.cpp, ChildLayoutsAdvanced.cpp's Execute bodies and DoCalc, and CenterLayoutExecute.cpp are the Rust
// objects.

// every op a layout object makes is on a component or an instruction it is given (never on its own shell)
// (SetSizeAtLeast and the instructions' references are gamewin.rs common_op's in the game: R6 WON GUI round 38)
extern "C" intptr_t ee_won_guicpp_layout_call(void*, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t)
{
    return CtlOp(NULL, op, a, b, c, 0);
}

namespace WONAPI {
// Destroys an instruction: its Rust instruction (the list borrows its components)
ChildLayoutInstructionBase::~ChildLayoutInstructionBase() { ee_won_layout_free(this->mLay); }
// the record's numbers are layoutobj.rs's (R6 WON round 33)
void ChildLayoutInstructionBase::Make(EeWonLayoutKind k, Component* r0, Component* r1, int i0, int i1, int i2, bool h,
                                      bool v)
{
    this->mLay = ee_won_layout_new_basic(this, ee_won_guicpp_layout_call, k, r0, r1, i0, i1, i2, h, v);
}
void ChildLayoutInstructionBase::Make(const EeWonLayoutInit& init)
{
    this->mLay = ee_won_layout_new(this, ee_won_guicpp_layout_call, &init);
}

// Centres a child in its parent once, horizontally and/or vertically (the original's temporary centre layout is the
// Rust's)
void CenterLayout::Center(Component* parent, Component* child, bool horz, bool vert)
{
    ee_won_layout_center(ee_won_guicpp_layout_call, (intptr_t)parent, (intptr_t)child, horz, vert);
}
}
