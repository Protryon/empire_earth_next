// The buttons' hosts (R6 WON GUI round 37: Button.cpp with ImageButton.cpp, MSButtonBase.cpp and WONButton.cpp).
// Button (R6 WON round 30): the Rust Button's calls out (rust/crates/ee_won_gui/src/btnobj.rs, EeWonGuiOp's button ops and
// the component ops it shares with the container), its constructor and destructor, the static functions' forwarders,
// the two virtuals subclasses override (HandleActivate, SetTextHook) and the click sound. The original's Button.cpp
// (the flags, the press and release, the keys, the repeat, the radio group, the default buttons) is the Rust object;
// each op here is a run of its C++ statements with no decision between them.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/won_gui/btnobj.h"
#include "rust/won_gui/guihost.h"
#include "ee/wonapi/WONLobbyImageButton.h"
#include "rust/won_gui/leafpaint.h"
#include "rust/won_gui/guidraw.h"
#include "rust/won_gui/msbtnobj.h"
#include "ee/wonapi/WONGUIDraw.h"
#include "ee/wonapi/WONGUIShell.h"
#include "rust/won_gui/wonbtnobj.h"
#include "rust/won_gui/comboobj.h"
#include <set>
#include "rust/won_gui/dialogobj.h"
#include "rust/won_gui/guictl.h"
#include "rust/won_gui/tabobj.h"
#include "ee/wonapi/WONSkinLayout.h"
#include <string.h>
#include "rust/won_gui/layoutobj.h"
#include "rust/won_gui/guiwin.h"
#include "ee/wonapi/WONLobbyBackground.h"
#include <deque>
using namespace WONAPI;

typedef EeWonGuiOp Op;

// t is NULL for the static functions' ops (those on a)
extern "C" intptr_t ee_won_guicpp_btn_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)c; (void)d;
    Button* t = static_cast<Button*>((Component*)inComp);
    switch (op) {
    // (Component's own virtuals, the references, the siblings, the flags, the repaints, the focus, the timer and
    // BtnOf are gamewin.rs common_op's in the game: R6 WON GUI round 38)
    case Op::BtnHandleActivate: return t->HandleActivate() ? 1 : 0;
    case Op::BtnSetTextHook: t->SetTextHook(*(const GUIString*)a); return 0;
    case Op::BtnHasClickSound: return t->mClickSound.get() != NULL ? 1 : 0;
    case Op::BtnPlayClick: t->mClickSound->Play(); return 0;
    default: return 0;
    }
}

namespace WONAPI {
// Creates a button that takes focus (the Rust button: no flags, not pressed)
Button::Button() : mBtn(ee_won_btn_make(static_cast<Component*>(this),ee_won_guicpp_btn_call)) {}
// Destroys the button (the Rust button; the click sound is a member)
Button::~Button() { ee_won_btn_free(this->mBtn); }

// Lets an activation fire its event (always true here)
bool Button::HandleActivate() { return true; }

void Button::SetTextHook(const GUIString&) {}
// Copies another button's flags and click sound
void Button::CopyAttributes(Button* from)
{
    this->BCall(EeWonBtnCall::CopyAttributes,(intptr_t)from->mBtn);
    this->mClickSound=from->mClickSound;
}
// Sets the sound played when the button is pressed
void Button::SetClickSound(Sound* sound) { this->mClickSound=sound; }
}

// ---- ImageButton.cpp until R6 WON GUI round 37
using namespace WONAPI;
// Creates an image button that takes focus but not tab focus, with stretching off
ImageButton::ImageButton() : mStretch(false) { ee_won_btn_image_init(this->mBtn,static_cast<Component*>(this)); }
// Destroys the image button (members only)
ImageButton::~ImageButton() {}
typedef EeWonLeafOp Lo;
// the button's calls for leafpaint.rs (R6 WON round 34: GetDesiredSize and Paint are Rust's), one statement each
extern "C" intptr_t ee_won_guicpp_imgbtn_call(void* inMe, EeWonLeafOp op, intptr_t a, intptr_t b)
{
    ImageButton* t = (ImageButton*)inMe;
    switch (op) {
    // (the images, their sizes and draw and the stretch flag are leafpaint.rs's, read in place: R6 WON GUI round 39)
    case Lo::BtnBasePaint: t->Component::Paint(*(Graphics*)a); return 0;
    default: return 0;
    }
}
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
static_assert(sizeof(SmartPtr<Image>) == sizeof(void*) && offsetof(ImageButton, mStretch) == offsetof(ImageButton, mImages) + 5 * sizeof(void*),
              "leafpaint.rs reads ImageButton's images and stretch flag");
[[maybe_unused]] static const bool sImgBtnAt = (ee_won_leaf_imgbtn_register(offsetof(ImageButton, mImages)), true);
#pragma clang diagnostic pop
// Returns the largest width and height among the button's five state images
void ImageButton::GetDesiredSize(int& outWidth,int& outHeight)
{
    ee_won_leaf_call(this, ee_won_guicpp_imgbtn_call, EeWonLeafCall::ImageButtonSize, (intptr_t)&outWidth, (intptr_t)&outHeight);
}
// Draws the image for the current state (disabled, checked, down, over), falling back to the normal image,
// resizing it to the button first when stretching
void ImageButton::Paint(Graphics& g) { ee_won_leaf_call(this, ee_won_guicpp_imgbtn_call, EeWonLeafCall::ImageButtonPaint, (intptr_t)&g, 0); }

// ---- MSButtonBase.cpp until R6 WON GUI round 37
// MSButtonBase (R6 WON round 35): the Rust MSButtonBase (rust/crates/ee_won_gui/src/msbtnobj.rs) holds the colours,
// alignments, focus style, pads and flags and runs every method; this is its host (its calls out: the base
// Component, the wrapped text, the button's state, the Graphics) and the constructor and destructor.

// the Rust's check box colour is StandardColor_Text (msbtnobj.rs SYS_TEXT)
static_assert(StandardColor_Text==14,"msbtnobj.rs SYS_TEXT");

extern "C" intptr_t ee_won_guicpp_msbtn_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)d;
    MSButtonBase* t = (MSButtonBase*)inComp;
    switch (op) {
    case Op::WrapSetMaxWidth: t->mText->SetMaxWidth((int)a); return 0;
    case Op::WrapSetCentered: t->mText->SetCentered(a != 0); return 0;
    case Op::WrapWidth: return t->mText->GetWidth();
    case Op::WrapHeight: return t->mText->GetHeight();
    case Op::SkinBoxPainter: // the check box's or radio button's default painter function (never replaced), 13 x 13
        WONMsPaint(*(Graphics*)a,WONMsPaintOf((EeWonMsBox)b == EeWonMsBox::Checkbox ? EeWonMsPaintKind::Check : EeWonMsPaintKind::Radio,13,13,(t->IsPressed()?EE_WON_MSPAINT_PRESSED:0)|(t->IsChecked()?EE_WON_MSPAINT_CHECKED:0)));
        return 0;
    case Op::Draw: WONDrawOne(*(Graphics*)a, *(const EeWonDrawOp*)b, WONDrawSlots(NULL, NULL, NULL, t->mText.get()), WONDrawNoCall()); return 0;
    default: return 0; // (the size and the button's state: gamewin.rs common_op's)
    }
}

// Creates a button base with unwrapped centred text in the MSButton font and 2-pixel padding (the Rust button holds
// the colours, alignments, focus style and pads)
MSButtonBase::MSButtonBase() {
    this->mMsb=ee_won_msbtn_new(this,ee_won_guicpp_msbtn_call);
    this->mText=new WrappedText;
    this->mText->SetWrap(false);
    this->mText->SetFont(this->GetNamedFont("MSButton"));
}
// Destroys the button base: the Rust button goes
MSButtonBase::~MSButtonBase() { ee_won_msbtn_free(this->mMsb); }

// ---- WONButton.cpp until R6 WON GUI round 37
// Skinned button (R6 WON round 35): the Rust WONButton (rust/crates/ee_won_gui/src/wonbtnobj.rs) holds the states, the
// wrapped text per font, the pads, colours, flags and alignments and runs every method; this is its host (its calls
// out: the base Button, the wrapped texts, the images, the fonts, the button's state, the Graphics). Since R6
// WONLobby round 24 the button is the shell kind WonButton (WONGUIShell.h, shell_won.rs).

static WrappedText* Txt(intptr_t a) { return (WrappedText*)a; }
static Image* Img(intptr_t a) { return (Image*)a; }

// a wrapped text's method (EeWonWbText; `b` the text)
static intptr_t TextOp(intptr_t which, intptr_t b, intptr_t c)
{
    switch ((EeWonWbText)which) {
    case EeWonWbText::New: { WrappedText* t = new WrappedText; t->AddRef(); return (intptr_t)t; }
    case EeWonWbText::SetText: Txt(b)->SetText(*(const GUIString*)c); return 0;
    case EeWonWbText::SetMaxWidth: Txt(b)->SetMaxWidth((int)c); return 0;
    case EeWonWbText::SetWrap: Txt(b)->SetWrap(c != 0); return 0;
    case EeWonWbText::SetCentered: Txt(b)->SetCentered(c != 0); return 0;
    case EeWonWbText::SetFont: Txt(b)->SetFont((Font*)c); return 0;
    case EeWonWbText::Width: return Txt(b)->GetWidth();
    case EeWonWbText::Height: return Txt(b)->GetHeight();
    case EeWonWbText::Font: return (intptr_t)Txt(b)->mFont.get();
    case EeWonWbText::Text: return (intptr_t)&Txt(b)->GetText();
    default: return 0;
    }
}

extern "C" intptr_t ee_won_guicpp_wonbtn_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    WONButton* t = (WONButton*)inComp;
    switch (op) {
    case Op::WbBaseCopy: t->Button::CopyAttributes((Button*)a); return 0;
    case Op::WbText: return TextOp(a,b,c);
    // (WbImage, WbFont and the texts' references are gamewin.rs wonbtn_op's in the game: R6 WON GUI round 38)
    case Op::Draw: WONDrawOne(*(Graphics*)a, *(const EeWonDrawOp*)b, WONDrawSlots(NULL, NULL, Img(c), Txt(d)), WONDrawNoCall()); return 0;
    default: return 0; // (the size and the button's state: gamewin.rs common_op's)
    }
}

// ---- ComboBox.cpp until R6 WON GUI round 39
// The containers' controls' hosts (R6 WON GUI round 37: ComboBox.cpp with DialogBehavior.cpp, TabCtrl.cpp,
// SkinnedContainers.cpp and ChildLayouts.cpp).
// ComboBox (R6 WON round 28): the Rust ComboBox's calls out (rust/crates/ee_won_gui/src/comboobj.rs, EeWonGuiOp's combo
// ops; the container's fall back to Container.cpp's), its constructor, destructor and list box setter. The original's
// ComboBox.cpp (the list box's size and popup, the selection, the keys, the events, no-typing, enabling) is the Rust
// object; each op here is a run of its C++ statements with no decision between them.

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

// ---- TextListItem.cpp until R6 WON GUI round 39
// The list items' and backgrounds' leaf hosts (R6 WON GUI round 37: TextListItem.cpp with MenuBehavior.cpp,
// ComponentListItemAdapters.cpp, ListControlAdapters.cpp and Background.cpp). Text list items: size, sort order and
// painting (file name inferred). R6 WON round 34: the measure and the paint are Rust's
// (rust/crates/ee_won_gui/src/leafpaint.rs); this is their host, one statement per op (the menu items' host passes
// the others here).
typedef EeWonLeafOp Lo;
// ListItem's members as leafpaint.rs's EeWonListItemRec, read and set in place in the game (R6 WON GUI round 38:
// the item ops' field cases went; gamewin.rs item_op)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
static_assert(offsetof(ListItem, mSelected) == EE_WON_LIST_ITEM_AT && offsetof(ListItem, mSelectable) == EE_WON_LIST_ITEM_AT + 1 &&
              offsetof(ListItem, mTop) == EE_WON_LIST_ITEM_AT + 4 && offsetof(ListItem, mWidth) == EE_WON_LIST_ITEM_AT + 8 &&
              offsetof(ListItem, mHeight) == EE_WON_LIST_ITEM_AT + 12, "leafpaint.rs's EeWonListItemRec");
// where leafpaint.rs reads a text item's, a menu item's and a list's members in the game (R6 WON GUI round 40)
static const EeWonTextItemAt kTextItemAt = {offsetof(TextListItem, mText), offsetof(TextListItem, mColor),
    offsetof(TextListItem, mFont), offsetof(TextListItem, mImage), offsetof(MenuItem, mSubMenu), offsetof(MenuItem, mDisabled),
    offsetof(ListArea, mFont), offsetof(ListArea, mTextColor), offsetof(ListArea, mSelColor), offsetof(ListArea, mRust),
    offsetof(ListArea, mPane), offsetof(Font, mRust)};
#pragma clang diagnostic pop
[[maybe_unused]] static const bool sTextItemAt = (ee_won_leaf_textitem_register(&kTextItemAt), true);
extern "C" intptr_t ee_won_guicpp_textitem_call(void* inMe, EeWonLeafOp op, intptr_t a, intptr_t b)
{
    TextListItem* t = (TextListItem*)inMe;
    switch (op) {
    // (the item's, the menu item's and the list's reads are leafpaint.rs text_item_op's in the game: R6 WON GUI round 40)
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
// (the menu item's own reads are leafpaint.rs text_item_op's in the game: R6 WON GUI round 40)
// Measures the menu item as text plus room for the icon and submenu arrow (32 wider, 4 taller)
void MenuItem::CalcDimensions(ListArea* area) { ee_won_leaf_call(static_cast<TextListItem*>(this), ee_won_guicpp_textitem_call, EeWonLeafCall::MenuItemMeasure, (intptr_t)area, 0); }
// Draws the menu item: icon, selection highlight with an icon box, text (embossed grey when disabled)
// and a submenu arrow; the box's bottom edge runs to x = height-1, not to the box width
void MenuItem::Paint(Graphics& g,ListArea* area)
{
    ee_won_leaf_call(static_cast<TextListItem*>(this), ee_won_guicpp_textitem_call, EeWonLeafCall::MenuItemPaint, (intptr_t)&g, (intptr_t)area);
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
    return ((TextListItem*)left)->mText.ToInt(0)-((TextListItem*)right)->mText.ToInt(0);
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
    // (the item, the members, the sizes, SetSize and LocalToRoot are tipobj.rs's in the game: R6 WON GUI round 39)
    case To::HasOwnScheme: return t->GetOwnColorScheme() != NULL ? 1 : 0;
    case To::ListScheme: return (intptr_t)t->mListArea->GetColorScheme();
    case To::ListLeftPad: return t->mListArea->GetLeftPad();
    case To::ListRightPad: return t->mListArea->GetRightPad();
    case To::ListTopPad: return t->mListArea->GetTopPad();
    case To::ListVertOffset: return t->mListArea->GetVertOffset();
    case To::ScrollPaint: t->mListArea->ScrollPaint(*g); return 0;
    case To::OpenPopup: OpenWith(t->mListArea->GetRoot(), PopupParams(t, EE_WON_TIP_POPUP_FLAGS, t->mListArea, (int)a, (int)b)); return 0;
    case To::ClosePopup: t->mListArea->GetRoot()->ClosePopup(t); return 0;
    default: return 0;
    }
}
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
static_assert(sizeof(SmartPtr<ListItem>) == sizeof(void*) && offsetof(ListAreaToolTip, mItem) == offsetof(ListAreaToolTip, mListArea) + sizeof(void*),
              "tipobj.rs reads the tip's list and item");
[[maybe_unused]] static const bool sTipAt = (ee_won_tip_register(offsetof(ListAreaToolTip, mListArea)), true);
#pragma clang diagnostic pop
