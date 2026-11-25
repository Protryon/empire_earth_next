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
#include "rust/won_gui/guictl.h"
#include "rust/won_gui/guictor.h"
#include "rust/won_gui/guidraw.h"
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
using namespace WONAPI;

typedef EeWonGuiOp Op;
typedef EeWonGuiField Fd;

// the standard cursor a host op names (guihost.rs over cursorobj.rs's table; NULL for none)
static Cursor* CursorOf(intptr_t inCursor) { return (Cursor*)ee_won_gui_cursor(inCursor); }

// a text the Rust object hands out (u32 units)
static GUIString UnitsText(intptr_t inUnits, intptr_t inCount)
{
    std::wstring w;
    for (intptr_t i = 0; i < inCount; i++) w.push_back((wchar_t)((const uint32_t*)inUnits)[i]);
    return GUIString(w);
}

// the text a run of ops shares (guisplit.rs: the original's local GUIString)
static GUIString sHostText;
// QueueSubMenu's local PopupParams (borrowed pointers)
static PopupParams sHostParams(NULL, 0, NULL);

// a component's services, the same for every component object (the members', Component's own calls and the
// container's are gamewin.rs common_op's in the game: R6 WON GUI round 37)
static intptr_t ComponentOp(Component* c, Op op, intptr_t a, intptr_t b)
{
    switch (op) {
    case Op::Draw: WONDrawPlain(*(Graphics*)a, *(const EeWonDrawOp*)b); return 0;
    case Op::Invalidate: c->Invalidate(); return 0;
    case Op::SetParentCursor: c->mParent->SetCursor(CursorOf(a)); return 0;
    case Op::FireEvent:
        // the type is Rust's map, which the goldens check against the enum (round 23's lists fired ListSelChanged
        // through a switch here as InputTextChanged: the LAN lobby's Join Game stayed disabled)
        c->Fn2F5380(ee_won_gui_event_type((EeWonGuiEvent)a), 0);
        return 0;
    case Op::ClipboardSet: Clipboard::Set(UnitsText(a, b)); return 0;
    // ClipboardTake is guisplit.rs's over these (R6 WON round 33)
    case Op::HostTextClipboard: sHostText = Clipboard::Get(); return 0;
    case Op::HostTextSink: ee_won_gui_sink_units((void*)a, sHostText.mData->mText, sHostText.length(), sizeof(wchar_t)); return 0;
    case Op::HostTextClear: sHostText = GUIString(); return 0;
    case Op::HostTextUnits: sHostText = UnitsText(a, b); return 0;
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

extern "C" intptr_t ee_won_guicpp_text_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    TextArea* t = (TextArea*)inComp;
    switch (op) {
    case Op::Get: return (Fd)a == Fd::Width || (Fd)a == Fd::Height ? ComponentOp(t, op, a, b) : t->GetField((Fd)a);
    case Op::Set: t->SetField((Fd)a, (int)b); return 0;
    case Op::ScrollTranslate: t->ScrollTranslate(*(int*)a, *(int*)b); return 0;
    case Op::SelColor: *(EeWonSelColor*)a = WONDrawSelOf(t->mSelColor); return 0;
    // LinkActivated is guisplit.rs's over these two (R6 WON round 33)
    case Op::TextSetLinkParam: t->mLinkParam = Seg(a)->GetLinkParam(); return 0;
    case Op::TextDeliverLinkEvent: t->DeliverEvent(new TextLinkActivatedEvent(t, Seg(a)), false); return 0;
    case Op::SelTextAppend: t->mSelText.append(Seg(a)->mText, (unsigned short)b, (unsigned short)c); return 0;
    case Op::SelTextBreak: t->mSelText.append("\r\n"); return 0;
    case Op::SelTextCopy: Clipboard::Set(t->mSelText); t->mSelText.erase(); return 0;
    case Op::SegHold: Seg(a)->AddRef(); return 0;
    case Op::SegRelease: Seg(a)->Release(); return 0;
    case Op::SegLineBreak: return Seg(a)->mLineBreak ? 1 : 0;
    case Op::SegLength: return Seg(a)->mText.length();
    case Op::SegFontStringWidth: return Seg(a)->mFont->GetStringWidth(Seg(a)->mText);
    case Op::SegFontHeight: return Seg(a)->mFont->GetHeight();
    case Op::SegFontAscent: return Seg(a)->mFont->GetAscent();
    case Op::SegStringWidth: return Seg(a)->mFont->GetStringWidth(Seg(a)->mText, (unsigned short)b, (unsigned short)c);
    case Op::SegCharWidth: return Seg(a)->mFont->GetCharacterWidth(Seg(a)->mText[(int)b]);
    case Op::SegTextTake:
        ee_won_gui_sink_units((void*)b, Seg(a)->mText.mData->mText, Seg(a)->mText.length(), sizeof(wchar_t));
        return 0;
    case Op::SegIsLink: return Seg(a)->IsLink() ? 1 : 0;
    case Op::SegPressed: Seg(a)->Pressed(); return 0;
    case Op::SegReleased: return Seg(a)->Released() ? 1 : 0;
    case Op::SegColor: return Seg(a)->mColor;
    case Op::TextDrawSeg: WONDrawOne(*(Graphics*)a, *(const EeWonDrawOp*)b, WONDrawSlots(Seg(c)->mFont.get(), &Seg(c)->mText), WONDrawNoCall()); return 0;
    // the constructor's statements (guictor.rs)
    case Op::CtorTextColor: t->mTextColor=0x8000000e; return 0;
    case Op::CtorFontNamed: t->SetFont(t->GetNamedFont("TextArea")); return 0;
    case Op::CtorLinkColor: t->mLinkColor=0x0000ff; return 0;
    case Op::CtorLinkFontNamed: t->SetLinkFont(t->GetNamedFont("TextAreaLink")); return 0;
    case Op::CtorLinkFontIsDefault: return t->mLinkFont.get()==t->GetRoot()->GetWindowManager()->GetDefaultFont();
    case Op::CtorLinkFontUnderline: t->SetLinkFont(t->GetFont(Underlined(t->mFont->GetDescriptor()))); return 0;
    case Op::CtorPads: t->SetPads((int)a,(int)a,(int)a,(int)a); return 0;
    case Op::CtorReset: t->Reset(false); return 0;
    case Op::SegNew: return (intptr_t)new TextSegment;
    case Op::SegSetText: Seg(a)->mText.mData=((const GUIString*)b)->mData; return 0;
    case Op::SegSetColor: Seg(a)->mColor=(int)b; return 0;
    case Op::SegSetFont: Seg(a)->mFont=(Font*)b; return 0;
    case Op::SegSetLineBreak: Seg(a)->mLineBreak=b!=0; return 0;
    case Op::AddSegmentOf: t->AddSegment(Seg(a)); return 0;
    case Op::TextAddPart: t->AddSegment(((const GUIString*)a)->substr((unsigned short)b, (unsigned short)c), d != 0); return 0;
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
    ee_won_gui_add_segment(this, ee_won_guicpp_text_call, &text, color, this->mFont.get(), lineBreak);
}
// Appends a text segment in the given colour and font (the body at 0x00713da0)
void TextArea::AddSegment(const GUIString& text,int color,Font* font,bool lineBreak)
{
    ee_won_gui_add_segment(this, ee_won_guicpp_text_call, &text, color, font, lineBreak);
}

// Appends text split at CR, LF and CRLF into one line-broken segment per line; the last line breaks
// only when lineBreak is set
void TextArea::AddFormatedText(const GUIString& text,bool lineBreak)
{
    ee_won_gui_add_formatted(this,ee_won_guicpp_text_call,&text,text.mData->mText,text.length(),sizeof(wchar_t),lineBreak);
}
}

// ---- the input box

extern "C" intptr_t ee_won_guicpp_input_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t /*d*/)
{
    InputBox* t = (InputBox*)inComp;
    switch (op) {
    case Op::FontHeight: return t->mFont->GetHeight();
    case Op::FontCharWidth: return t->mFont->GetCharacterWidth((int)a);
    case Op::TextWidth: return t->mFont->GetStringWidth(t->mText, (unsigned short)a, (unsigned short)b);
    case Op::TextWidthAll: return t->mFont->GetStringWidth(t->mText);
    case Op::TextCharWidth: return t->mFont->GetCharacterWidth(t->mText[(int)a]);
    case Op::FontStringM: return t->mFont->GetStringWidth(GUIString("M"));
    case Op::TextStore: (a == 0 ? t->mText : t->mRealText) = UnitsText(b, c); return 0;
    case Op::InputHasFilter: return t->mInputFilter != NULL ? 1 : 0;
    case Op::InputFilterHostText: return t->mInputFilter(t, sHostText) ? 1 : 0;
    case Op::IconWidth: return t->mIcon.get() != NULL ? t->mIcon->GetWidth() : -1;
    case Op::InputDisabled: return t->IsDisabled() ? 1 : 0;
    case Op::InputUseDisabledColors: return t->mUseDisabledColors ? 1 : 0;
    case Op::InputHasIcon: return t->mIcon.get() != NULL ? 1 : 0;
    case Op::InputHasFocus: return t->HasFocus() ? 1 : 0;
    case Op::InputTextColor: return (int)t->mTextColor;
    case Op::InputDisabledTextColor: return t->mDisabledTextColor;
    case Op::InputIconHeight: return t->mIcon->GetHeight();
    case Op::InputSelColor: *(EeWonSelColor*)a = WONDrawSelOf(t->mSelColor); return 0;
    case Op::InputDrawSlots: WONDrawOne(*(Graphics*)a, *(const EeWonDrawOp*)b, WONDrawSlots(t->mFont.get(), &t->mText, t->mIcon.get()), WONDrawNoCall()); return 0;
    case Op::InputPaintBack: t->mBackground.Paint(*(Graphics*)a, 0, 0, ((const EeWonDrawOp*)b)->c, ((const EeWonDrawOp*)b)->d, ((const EeWonDrawOp*)b)->e, 0); return 0;
    case Op::InputPaintDisabledBack: t->mDisabledBackground.Paint(*(Graphics*)a, 0, 0, ((const EeWonDrawOp*)b)->c, ((const EeWonDrawOp*)b)->d, ((const EeWonDrawOp*)b)->e, 0); return 0;
    // the constructor's statements (guictor.rs)
    case Op::CtorFontNamed: t->mFont=t->GetNamedFont("InputBox"); return 0;
    case Op::CtorTextColor: t->mTextColor=0x8000000e; return 0;
    case Op::CtorDisabledTextColor: t->mDisabledTextColor=(int)0x80000006; return 0;
    case Op::CtorDisabledBackground: t->mDisabledBackground.SetColor((int)0x80000001); return 0;
    case Op::CtorUseDisabledColors: t->mUseDisabledColors=true; return 0;
    case Op::CtorNoFilter: t->mInputFilter=NULL; return 0;
    default: return ComponentOp(t, op, a, b);
    }
}

namespace WONAPI {
// Input filter that accepts only digits, with a leading minus sign allowed at the start of the box
bool InputBox::NumericFilter(InputBox *theInputBox, GUIString &theStr)
{
    return ee_won_input_numeric_filter(theStr.mData->mText, theStr.length(), theInputBox->GetSelStart());
}

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
    case Op::WrapSetMaxWidth: t->mWrappedText->SetMaxWidth((int)a); return 0;
    case Op::WrapSetCentered: t->mWrappedText->SetCentered(a != 0); return 0;
    case Op::WrapSetWrap: t->mWrappedText->SetWrap(a != 0); return 0;
    case Op::WrapWidth: return t->mWrappedText->GetWidth();
    case Op::WrapHeight: return t->mWrappedText->GetHeight();
    case Op::PaintBackground: t->mBackground.Paint(*(Graphics*)a, 0, 0, (int)b, (int)c); return 0;
    case Op::LabelDrawWrapped: WONDrawOne(*(Graphics*)a, *(const EeWonDrawOp*)b, WONDrawSlots(NULL, NULL, NULL, t->mWrappedText.get()), WONDrawNoCall()); return 0;
    // the constructor's statements (guictor.rs)
    case Op::CtorWrapNew: t->mWrappedText=new WrappedText; return 0;
    case Op::CtorFontNamed: t->mWrappedText->SetFont(t->GetNamedFont("Label")); return 0;
    case Op::CtorBackgroundNone: t->mBackground.SetColor(-1); return 0;
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
static Button* Btn(intptr_t a) { return (Button*)a; }

// a container's children (`a` a child `Component*`), the same for the splitter and the scroll bar
static intptr_t ContainerOp(Container* t, Op op, intptr_t a, intptr_t b, intptr_t c)
{
    switch (op) {
    case Op::ChildHold: Comp(a)->AddRef(); return 0;
    case Op::ChildRelease: Comp(a)->Release(); return 0;
    case Op::ChildSetDefaultCursor: Comp(a)->SetDefaultCursor(CursorOf(b)); return 0;
    case Op::AddChild: t->AddChild(Comp(a)); return 0;
    case Op::EventType: return ((ComponentEvent*)a)->mType;
    case Op::EventComponent: return (intptr_t)((ComponentEvent*)a)->mComponent;
    case Op::NewComponent: return (intptr_t)new Component;
    case Op::ChildLayoutDrag: ((Container*)Comp(a))->Fn2F6140(Comp(b), 3, (Container*)Comp(a), 0, 0, 0, 0, 0); return 0;
    case Op::ChildAddEx: ((Container*)Comp(a))->AddChildEx(Comp(b), 0); return 0;
    case Op::ChildSetSize: Comp(a)->SetSize((int)b, (int)c); return 0;
    case Op::ParentOf: return (intptr_t)Comp(a)->mParent;
    case Op::AsHeader: return (intptr_t)dynamic_cast<MultiListHeader*>((Container*)a);
    case Op::HeaderDrawsArrow: return ((MultiListHeader*)a)->DrawSortArrow() ? 1 : 0;
    case Op::HeaderSortTypeOf: return ((MultiListHeader*)a)->GetSortType(((MultiListHeader*)a)->GetColumnIndex(Comp(b)));
    default: return ComponentOp(t, op, a, b);
    }
}

namespace WONAPI {
intptr_t SplitterHostCall(Splitter* t, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c)
{
    switch (op) {
    case Op::ButtonSetCheck: Btn(a)->SetCheck(b != 0, false); return 0;
    case Op::ButtonIsChecked: return Btn(a)->IsChecked() ? 1 : 0;
    case Op::EventIsButtonPress: return ((ComponentEvent*)a)->mType == ComponentEvent_ButtonPressed ? 1 : 0;
    case Op::EventControlId: return ((ComponentEvent*)a)->GetControlId();
    case Op::EventButton: return (intptr_t)dynamic_cast<Button*>(((ComponentEvent*)a)->mComponent);
    default: return ContainerOp(t, op, a, b, c);
    }
}

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
    return SplitterHostCall((Splitter*)inComp, op, a, b, c);
}

// ---- the scroll bar

extern "C" intptr_t ee_won_guicpp_bar_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t /*d*/)
{
    Scrollbar* t = (Scrollbar*)inComp;
    switch (op) {
    case Op::NewButton: return (intptr_t)static_cast<Component*>(new Button);
    case Op::ButtonSetFlags: static_cast<Button*>(Comp(a))->SetButtonFlags((int)b, c != 0); return 0;
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
    case Op::HasFocusChild: return t->GetFocusChild() != NULL ? 1 : 0;
    case Op::BarOf: return (intptr_t)static_cast<Scrollbar*>(Comp(a))->mRust;
    case Op::ListenTo: Comp(a)->AddListener(new ComponentListenerComp(t)); return 0;
    case Op::AreaGet: return area->GetField((Fd)b);
    case Op::AreaSet: area->SetField((Fd)b, (int)c); return 0;
    case Op::AreaAdjustPosition: area->AdjustPosition(b != 0); return 0;
    case Op::AreaMouseWheel: return area->MouseWheel((int)b) ? 1 : 0;
    case Op::AreaSetScroller: area->SetScroller(t); return 0;
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
    case Op::GetViewport: return (intptr_t)static_cast<Component*>(t->GetViewport());
    case Op::PaneBackground: t->mBackground.Paint(*(Graphics*)a, 0, 0, t->Width(), t->Height(), (int)b, (int)c); return 0;
    case Op::ScrollerNotify: static_cast<Scroller*>(Comp(a))->NotifyScrollChange(b != 0); return 0;
    case Op::ScrollerSetArea: static_cast<Scroller*>(Comp(a))->SetScrollArea(t); return 0;
    case Op::ScrollerAsBars:
        return (intptr_t)static_cast<Component*>(dynamic_cast<ScrollbarScroller*>(static_cast<Scroller*>(Comp(a))));
    case Op::NewScrollbarScroller: return (intptr_t)static_cast<Component*>(new ScrollbarScroller);
    case Op::ScrollerSetScrollbar:
        static_cast<ScrollbarScroller*>(Comp(a))->SetScrollbar(static_cast<Scrollbar*>(Comp(b)));
        return 0;
    // the constructor's statements (guictor.rs)
    case Op::CtorViewportNew: t->mViewport=new ScrollAreaViewport(t); return 0;
    case Op::CtorAddViewport: t->AddChild(t->mViewport.get()); return 0;
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
    case Op::Get: return (Fd)a == Fd::Width || (Fd)a == Fd::Height ? ComponentOp(t, op, a, b) : t->GetField((Fd)a);
    case Op::Set: t->SetField((Fd)a, (int)b); return 0;
    case Op::ScrollTranslate: t->ScrollTranslate(*(int*)a, *(int*)b); return 0;
    case Op::VirtAdjustPosition: t->AdjustPosition(a != 0); return 0;
    case Op::VirtSort: return t->Sort() ? 1 : 0;
    case Op::VirtNotifyMouseItemGone: t->NotifyMouseItemGone(); return 0;
    case Op::VirtListSelChangedHook: t->ListSelChangedHook(); return 0;
    case Op::SetVertOffset: t->SetVertOffset((int)a); return 0;
    case Op::OnBottom: return t->OnBottom() ? 1 : 0;
    case Op::FocusChildLost: t->DropFocusChild(); return 0;
    case Op::RootKeyMod: return t->GetRoot()->GetKeyMod();
    case Op::SetCursorDefault: t->SetCursor(t->GetDefaultCursor()); return 0;
    case Op::GetViewport: return (intptr_t)t->GetViewport();
    // PointerOutside, TipShow, TipHide, QueueSubMenu and QueueMouseUp are guisplit.rs's over these (R6 WON round 33)
    case Op::ListRoot: return (intptr_t)t->GetRoot();
    case Op::WinMouseX: return ((Window*)a)->GetMouseX();
    case Op::WinMouseY: return ((Window*)a)->GetMouseY();
    case Op::ListParent: return (intptr_t)(Component*)t->mParent;
    case Op::ParentRootToChild: static_cast<Container*>((Component*)a)->RootToChild(((int*)b)[0], ((int*)b)[1]); return 0;
    case Op::ListContains: return t->Contains((int)a, (int)b) ? 1 : 0;
    case Op::TipNew: t->mItemToolTip = new ListAreaToolTip(t); return 0;
    case Op::TipDo: t->mItemToolTip->DoTip(Item(a)); return 0;
    case Op::ParamsMake: sHostParams = PopupParams((PopupMenu*)a, (int)b, t, (int)c, (int)d); return 0;
    case Op::ParamsAvoid: sHostParams.mAvoidRect = WONRectangle(((const EeWonRect4*)a)->x, ((const EeWonRect4*)a)->y, ((const EeWonRect4*)a)->w, ((const EeWonRect4*)a)->h); return 0;
    case Op::QueueDoPopup: t->GetRoot()->QueueEvent(new DoPopupEvent(sHostParams)); return 0;
    case Op::ListLocalToRoot: t->LocalToRoot(((int*)a)[0], ((int*)a)[1]); return 0;
    case Op::WinQueueMouseUp: ((Window*)a)->QueueEvent(new MouseEvent(InputEvent_MouseUp, (int)b, (int)c, ((Window*)a)->GetKeyMod(), (int)d)); return 0;
    case Op::DeliverItemClicked: {
        const EeWonItemClick* k = (const EeWonItemClick*)b;
        t->DeliverEvent(new ListItemClickedEvent(t, Item(a), static_cast<MouseButton>(k->button),
                                                 t->CheckDoubleClick(k->button), k->up, k->x, k->y), false);
        return 0;
    }
    case Op::TipExists: return t->mItemToolTip.get() != NULL ? 1 : 0;
    case Op::TipEndCall: t->mItemToolTip->EndTip(); return 0;
    case Op::SortItems: std::sort((ListItem**)a, (ListItem**)a + b, ListArea::LineSortPred(t, t->GetSortFunc(), (int)c)); return 0;
    case Op::LowerBound: return std::lower_bound((ListItem**)a, (ListItem**)a + b, Item(c), ListArea::LineSortPred(t, t->GetSortFunc(), (int)d)) - (ListItem**)a;
    case Op::Less: return ListArea::LineSortPred(t, t->GetSortFunc(), (int)c)(Item(a), Item(b)) ? 1 : 0;
    case Op::ItemTop: return Item(a)->mTop;
    case Op::ItemSetTop: Item(a)->mTop = (int)b; return 0;
    case Op::ItemHeight: return Item(a)->mHeight;
    case Op::ItemSetHeight: Item(a)->mHeight = (int)b; return 0;
    case Op::ItemWidth: return Item(a)->mWidth;
    case Op::ItemSelected: return Item(a)->mSelected ? 1 : 0;
    case Op::ItemSetSelected: Item(a)->mSelected = b != 0; return 0;
    case Op::ItemSelectable: return Item(a)->mSelectable ? 1 : 0;
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
    case Op::ItemHold: Item(a)->AddRef(); return 0;
    case Op::ItemRelease: Item(a)->Release(); return 0;
    // the multi-column list and its rows (multiobj.rs)
    case Op::ItemCompare: return Item(a)->Compare(Item(b), t);
    case Op::CallSortFunc: return ((ListArea::SortFunc)a)(t, Item(b), Item(c));
    case Op::ItemSetWidth: Item(a)->mWidth = (int)b; return 0;
    case Op::SetCursor: t->SetCursor(CursorOf(a)); return 0;
    case Op::SelBackground: return (intptr_t)t->GetSelColor().GetBackgroundColor(*(Graphics*)a);
    case Op::NewViewport: return (intptr_t)new ScrollAreaViewport(t);
    case Op::ViewportAddChild: t->GetViewport()->AddChild((ScrollAreaViewport*)a); return 0;
    case Op::ChildRelease: Comp(a)->Release(); return 0;
    case Op::HeaderNotify: ((MultiListHeader*)a)->NotifyColumnChange(); return 0;
    case Op::HeaderSetSortColumn: ((MultiListHeader*)a)->SetSortColumn((int)b, (ListSortType)c); return 0;
    case Op::RowCell: return (intptr_t)((MultiListItem*)a)->GetItem((int)b);
    case Op::RowSetCell: ((MultiListItem*)a)->SetItem(Item(b), (int)c); return 0;
    // the tree list and its rows (treeobj.rs)
    case Op::NodeOf: return (intptr_t)((TreeItem*)a)->mNode;
    case Op::NewTreeItemRaw: return (intptr_t)new TreeItem;
    case Op::ItemSetSelectable: Item(a)->mSelectable = b != 0; return 0;
    case Op::ItemSetPosXY: Item(a)->SetPos((int)b, (int)c); return 0;
    case Op::EventType: return ((ComponentEvent*)a)->mType;
    case Op::EventCompControlId: return ((ComponentEvent*)a)->mComponent->mControlId;
    case Op::EventComponentTop: return ((ComponentEvent*)a)->mComponent->Top();
    case Op::CheckDoubleClick: return t->CheckDoubleClick((int)a) ? 1 : 0;
    case Op::DeliverTreeOpened: t->DeliverEvent(new TreeItemOpenedEvent(t, (TreeItem*)a), false); return 0;
    // the popup menu (menuobj.rs)
    case Op::ChildHold: Comp(a)->AddRef(); return 0;
    case Op::ClosePopup: t->GetRoot()->ClosePopup(Comp(a)); return 0;
    case Op::EventComponent: return (intptr_t)((ComponentEvent*)a)->mComponent;
    case Op::MenuSelItem: return (intptr_t)((PopupMenu*)a)->GetSelItem();
    case Op::MenuSetSelAt: ((PopupMenu*)a)->SetSelItem((int)b); return 0;
    case Op::MenuItemOf: return (intptr_t)dynamic_cast<MenuItem*>(Item(a));
    case Op::MenuItemSub: return (intptr_t)((MenuItem*)a)->mSubMenu.get();
    case Op::MenuItemDisabled: return ((MenuItem*)a)->mDisabled ? 1 : 0;
    case Op::ScrollUntranslate: t->ScrollUntranslate(*(int*)a, *(int*)b); return 0;
    case Op::LocalToRoot: t->LocalToRoot(*(int*)a, *(int*)b); return 0;
    case Op::MenuReset: ((PopupMenu*)a)->Reset(); return 0;
    case Op::MenuTryKeyDown: return ((PopupMenu*)a)->TryKeyDown((int)b) ? 1 : 0;
    // the constructors' statements (guictor.rs: the list area's, the multi-column list's, the menu's)
    case Op::CtorTextColor: t->mTextColor=(int)0x8000000e; return 0;
    case Op::CtorFontNamed: t->mFont=t->GetNamedFont("ListArea"); return 0;
    case Op::CtorPads: t->SetPads((int)a,(int)a,(int)a,(int)a); return 0;
    case Op::CtorNoSortFunc: t->mSortFunc=NULL; return 0;
    case Op::CtorMultiColumns: static_cast<MultiListArea*>(t)->SetNumColumns((int)a); return 0;
    case Op::CtorMenuFont: static_cast<PopupMenu*>(t)->SetFont(t->GetNamedFont("Menu")); return 0;
    case Op::CtorMenuListFlags: static_cast<PopupMenu*>(t)->SetListFlags(0x806,true); return 0;
    case Op::CtorMenuBackground: static_cast<PopupMenu*>(t)->SetBackground(WONShellNew(EeWonShellKind::MsPopup)); return 0;
    case Op::CtorMenuTextColor: t->mTextColor=0x8000000c; return 0;
    case Op::CtorMenuBorderPadding: static_cast<PopupMenu*>(t)->SetBorderPadding(4,4,4,4); return 0;
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

// Returns whether the clicked item is the list box's current selection
bool ListItemClickedEvent::IsSelectedItem()
{
    return this->mItem.get()==static_cast<ListArea*>(this->mComponent)->GetSelItem();
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
    case Op::ItemSetTop: Item(a)->mTop = (int)b; return 0;
    case Op::ItemSetPos: Item(a)->SetPos(0, (int)b); return 0;
    case Op::ItemHold: Item(a)->AddRef(); return 0;
    case Op::ItemRelease: Item(a)->Release(); return 0;
    default: return 0;
    }
}

extern "C" intptr_t ee_won_guicpp_mhead_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t /*d*/)
{
    MultiListHeader* t = (MultiListHeader*)inComp;
    switch (op) {
    case Op::MultiOf: return (intptr_t)((MultiListArea*)a)->mMulti;
    case Op::RemoveChild: t->RemoveChild(Comp(a)); return 0;
    case Op::EventIsButtonPress: return ((ComponentEvent*)a)->mType == ComponentEvent_ButtonPressed ? 1 : 0;
    case Op::EventChildUnder: {
        Component* child = ((ComponentEvent*)a)->mComponent;
        Container* parent = child->mParent;
        while (parent != t && parent != NULL) {
            child = parent;
            parent = child->mParent;
        }
        return parent == NULL ? 0 : (intptr_t)child;
    }
    case Op::SetCursor: t->SetCursor(CursorOf(a)); return 0;
    case Op::CursorIs: return t->GetCurrentCursor() == CursorOf(a) ? 1 : 0;
    case Op::CtorHeaderArea: t->SetMultiListArea((MultiListArea*)a); return 0; // the constructor's (guictor.rs)
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
// Returns the sort arrow a header button should draw, found through its enclosing header (none without one)
ListSortType MultiListHeader::GetSortTypeForButton(Component* button)
{
    return (ListSortType)ee_won_gui_sort_type_for_button(ee_won_guicpp_mhead_call, button);
}
}

// ---- the tree list and its rows (treeobj.rs)

// a row's own calls out (its SetPos, its walks' other rows, its references)
extern "C" intptr_t ee_won_guicpp_tnode_call(void* /*inComp*/, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t /*d*/)
{
    switch (op) {
    case Op::NodeOf: return (intptr_t)((TreeItem*)a)->mNode;
    case Op::ItemTop: return Item(a)->mTop;
    case Op::ItemSetTop: Item(a)->mTop = (int)b; return 0;
    case Op::ItemSetPosXY: Item(a)->SetPos((int)b, (int)c); return 0;
    case Op::ItemContains: return Item(a)->Contains((int)b, (int)c) ? 1 : 0;
    case Op::ItemHold: Item(a)->AddRef(); return 0;
    case Op::ItemRelease: Item(a)->Release(); return 0;
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

// WrappedText (R6 WON round 38): the Rust object (wrapobj.rs) wraps the text it is handed with the font's measure
// (the measure is wrapobj.rs's too, over platfont.rs: R6 WON GUI round 36)
namespace WONAPI {
WrappedText::WrappedText() : mObj(ee_won_wrap_new()) {}
WrappedText::WrappedText(const GUIString& theText) : mText(theText), mObj(ee_won_wrap_new()) {}
// Recomputes the line breaks and the text's width and height for the font and maximum width
void WrappedText::CalcWrap()
{
    ee_won_wrap_calc_font(this->mObj, this->mFont.get() != NULL ? this->mFont->mRust : NULL, this->mText.mData->mText, this->mText.length());
}
// Draws each wrapped line (centred if set) from the given top-left point (wrapobj.rs over guidraw.rs wrapped_text)
void WrappedText::Paint(Graphics& g, int x, int y)
{
    if (this->mFont.get() == NULL)
        return;
    WONDrawRun(g, WONDrawOps([&](EeWonDrawOp* o, size_t n) {
        return ee_won_wrap_paint_font(this->mObj, this->mFont->mRust, this->mText.mData->mText, this->mText.length(), x, y, o, n); }),
        WONDrawSlots(this->mFont.get(), &this->mText));
}
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
