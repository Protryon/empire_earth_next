// WONGUI's component objects in Rust (R6 WON rounds 21-22): the one outward call table, ee_won_guicpp_call
// (rust/crates/ee_won/src/guihost.rs EeWonGuiOp), and the C++ left of the classes that moved: the TextArea's
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
#include "ee/wonapi/WONLobbySplitterConfig.h"
#include "ee/wonapi/WONLobbyComponentListItem.h"
#include "ee/wonapi/WONLobbyMSConfig.h"
#include <algorithm>
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/ee_won.h"
#include "ee/wonapi/WONGUIDraw.h"
#include "ee/wonapi/WONLobbyPopupEvent.h"
#include "ee/wonapi/WONLobbyMSPainters.h"
using namespace WONAPI;

typedef EeWonGuiOp Op;
typedef EeWonGuiField Fd;

static Cursor* CursorOf(intptr_t inCursor)
{
    switch ((EeWonGuiCursor)inCursor) {
    case EeWonGuiCursor::Hand: return Cursor::GetStandardCursor(StandardCursor_Hand);
    case EeWonGuiCursor::IBeam: return Cursor::GetStandardCursor(StandardCursor_IBeam);
    case EeWonGuiCursor::NorthSouthEastWest: return Cursor::GetStandardCursor(StandardCursor_NorthSouthEastWest);
    case EeWonGuiCursor::EastWest: return Cursor::GetStandardCursor(StandardCursor_EastWest);
    case EeWonGuiCursor::NorthSouth: return Cursor::GetStandardCursor(StandardCursor_NorthSouth);
    default: return NULL;
    }
}

// a text the Rust object hands out (u32 units)
static GUIString UnitsText(intptr_t inUnits, intptr_t inCount)
{
    std::wstring w;
    for (intptr_t i = 0; i < inCount; i++) w.push_back((wchar_t)((const uint32_t*)inUnits)[i]);
    return GUIString(w);
}

// a component's services, the same for every component object
static intptr_t ComponentOp(Component* c, Op op, intptr_t a, intptr_t b)
{
    switch (op) {
    case Op::Invalidate: c->Invalidate(); return 0;
    case Op::RequestTimer: c->RequestTimer(a != 0); return 0;
    case Op::RequestFocus: c->RequestFocus(); return 0;
    case Op::GetMousePos: c->GetMousePos(*(int*)a, *(int*)b); return 0;
    case Op::KeyMod: return c->GetKeyMod();
    case Op::SetParentCursor: c->mParent->SetCursor(CursorOf(a)); return 0;
    case Op::SetSize: c->SetSize((int)a, (int)b); return 0;
    case Op::SetDefaultCursor: c->SetDefaultCursor(CursorOf(a)); return 0;
    case Op::FireEvent:
        // the type is Rust's map, which the goldens check against the enum (round 23's lists fired ListSelChanged
        // through a switch here as InputTextChanged: the LAN lobby's Join Game stayed disabled)
        c->Fn2F5380(ee_won_gui_event_type((EeWonGuiEvent)a), 0);
        return 0;
    case Op::ClipboardSet: Clipboard::Set(UnitsText(a, b)); return 0;
    case Op::ClipboardTake: {
        GUIString aText = Clipboard::Get();
        ee_won_gui_sink_units((void*)a, aText.mData->mText, aText.length(), sizeof(wchar_t));
        return 0;
    }
    default: return 0;
    }
}

static TextSegment* Seg(intptr_t a) { return (TextSegment*)a; }

extern "C" intptr_t ee_won_guicpp_text_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)d;
    TextArea* t = (TextArea*)inComp;
    switch (op) {
    case Op::Get:
        if ((Fd)a == Fd::Width) return t->Width();
        if ((Fd)a == Fd::Height) return t->Height();
        return t->GetField((Fd)a);
    case Op::Set: t->SetField((Fd)a, (int)b); return 0;
    case Op::BaseSizeChanged: t->ScrollArea::Virtual08(); return 0;
    case Op::BaseAdjustPosition: t->ScrollArea::AdjustPosition(a != 0); return 0;
    case Op::BaseNotifyRootVisibility: t->ScrollArea::NotifyRootVisibility(a != 0); return 0;
    case Op::BaseMouseDown: t->ScrollArea::MouseDown((int)a, (int)b, (int)c); return 0;
    case Op::BaseMouseUp: t->ScrollArea::MouseUp((int)a, (int)b, (int)c); return 0;
    case Op::BaseMouseExit: t->ScrollArea::MouseExit(); return 0;
    case Op::BaseMouseMove: t->ScrollArea::MouseMove((int)a, (int)b); return 0;
    case Op::BaseMouseEnter: t->ScrollArea::MouseEnter((int)a, (int)b); return 0;
    case Op::BaseMouseDrag: t->ScrollArea::MouseDrag((int)a, (int)b); return 0;
    case Op::BaseKeyDown: return t->ScrollArea::KeyDown((int)a) ? 1 : 0;
    case Op::BaseKeyChar: return t->ScrollArea::KeyChar((int)a) ? 1 : 0;
    case Op::ScrollTranslate: t->ScrollTranslate(*(int*)a, *(int*)b); return 0;
    case Op::SelColor: *(EeWonSelColor*)a = WONDrawSelOf(t->mSelColor); return 0;
    case Op::LinkActivated:
        t->mLinkParam = Seg(a)->GetLinkParam();
        t->DeliverEvent(new TextLinkActivatedEvent(t, Seg(a)), false);
        return 0;
    case Op::SelTextAppend: t->mSelText.append(Seg(a)->mText, (unsigned short)b, (unsigned short)c); return 0;
    case Op::SelTextBreak: t->mSelText.append("\r\n"); return 0;
    case Op::SelTextCopy: Clipboard::Set(t->mSelText); t->mSelText.erase(); return 0;
    case Op::SegHold: Seg(a)->AddRef(); return 0;
    case Op::SegRelease: Seg(a)->Release(); return 0;
    case Op::SegLineBreak: return Seg(a)->mLineBreak ? 1 : 0;
    case Op::SegLength: return Seg(a)->mText.length();
    case Op::SegMeasure: {
        TextSegment* s = Seg(a);
        int* m = (int*)b;
        m[0] = s->mFont->GetStringWidth(s->mText);
        m[1] = s->mFont->GetHeight();
        m[2] = s->mFont->GetAscent();
        return 0;
    }
    case Op::SegStringWidth: return Seg(a)->mFont->GetStringWidth(Seg(a)->mText, (unsigned short)b, (unsigned short)c);
    case Op::SegCharWidth: return Seg(a)->mFont->GetCharacterWidth(Seg(a)->mText[(int)b]);
    case Op::SegTextTake:
        ee_won_gui_sink_units((void*)b, Seg(a)->mText.mData->mText, Seg(a)->mText.length(), sizeof(wchar_t));
        return 0;
    case Op::SegIsLink: return Seg(a)->IsLink() ? 1 : 0;
    case Op::SegPressed: Seg(a)->Pressed(); return 0;
    case Op::SegReleased: return Seg(a)->Released() ? 1 : 0;
    case Op::SegColor: return Seg(a)->mColor;
    case Op::Draw: {
        WONDrawSlots aSlots;
        if (c != 0) {
            aSlots.mFont = Seg(c)->mFont.get();
            aSlots.mString = &Seg(c)->mText;
        }
        WONDrawNoCall aNoCall;
        WONDrawOne(*(Graphics*)a, *(const EeWonDrawOp*)b, aSlots, aNoCall);
        return 0;
    }
    default: return ComponentOp(t, op, a, b);
    }
}

namespace WONAPI {
// FUNCTION: WON 0x007117c0
// Creates an empty text area with word wrap and selection on, the named TextArea/TextAreaLink fonts
TextArea::TextArea()
{
    this->mRust=ee_won_text_new(this,ee_won_guicpp_text_call); // the flags, limits, indent and alignment
    this->mTextColor=0x8000000e;
    this->SetFont(this->GetNamedFont("TextArea"));
    this->mLinkColor=0x0000ff;
    this->SetLinkFont(this->GetNamedFont("TextAreaLink"));
    if(this->mLinkFont.get()==this->GetRoot()->GetWindowManager()->GetDefaultFont())
    {
        FontDescriptor descriptor=this->mFont->GetDescriptor();
        descriptor.mStyle|=FontStyle_Underline;
        this->SetLinkFont(this->GetFont(descriptor));
    }
    this->SetComponentFlags(ComponentFlag_WantFocus,true);
    this->SetPads(5,5,5,5);
    this->Reset(false);
}
// FUNCTION: WON 0x00711be0
// Destroys the text area: the Rust area gives its segments back
TextArea::~TextArea() { ee_won_text_free(this->mRust); }

// FUNCTION: WON 0x00713c60
// FUNCTION: WON 0x00713c80
// FUNCTION: WON 0x00713da0
// Appends a text segment in the area's text colour (0x00713c60); the overloads below take a colour
void TextArea::AddSegment(const GUIString& text,bool lineBreak)
{
    this->AddSegment(text,this->mTextColor,lineBreak);
}
// Appends a text segment in the given colour and the area's font (the body at 0x00713c80)
void TextArea::AddSegment(const GUIString& text,int color,bool lineBreak)
{
    SmartPtr<TextSegment> seg=new TextSegment;
    seg->mText.mData=text.mData;
    seg->mColor=color;
    seg->mFont=this->mFont;
    seg->mLineBreak=lineBreak;
    this->AddSegment(seg.get());
}
// Appends a text segment in the given colour and font (the body at 0x00713da0)
void TextArea::AddSegment(const GUIString& text,int color,Font* font,bool lineBreak)
{
    SmartPtr<TextSegment> seg=new TextSegment;
    seg->mText.mData=text.mData;
    seg->mColor=color;
    seg->mFont=font;
    seg->mLineBreak=lineBreak;
    this->AddSegment(seg.get());
}

// FUNCTION: WON 0x00713ec0
// Appends text split at CR, LF and CRLF into one line-broken segment per line; the last line breaks
// only when lineBreak is set
void TextArea::AddFormatedText(const GUIString& text,bool lineBreak) {
    std::vector<EeWonTextPart> aParts(text.length()+1);
    size_t aCount=ee_won_gui_text_split(text.mData->mText,text.length(),sizeof(wchar_t),lineBreak,&aParts[0],aParts.size());
    for(size_t i=0;i<aCount && i<aParts.size();i++)
        this->AddSegment(text.substr((unsigned short)aParts[i].start,(unsigned short)aParts[i].len),aParts[i].line_break);
}
}

// ---- the input box

extern "C" intptr_t ee_won_guicpp_input_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)d;
    InputBox* t = (InputBox*)inComp;
    switch (op) {
    case Op::Get:
        if ((Fd)a == Fd::Width) return t->Width();
        if ((Fd)a == Fd::Height) return t->Height();
        return 0;
    case Op::BaseSizeChanged: t->Component::Virtual08(); return 0;
    case Op::BaseGotFocus: t->Component::GotFocus(); return 0;
    case Op::BaseLostFocus: t->Component::LostFocus(); return 0;
    case Op::BaseMouseExit: t->Component::MouseExit(); return 0;
    case Op::BasePaint: t->Component::Paint(*(Graphics*)a); return 0;
    case Op::FontHeight: return t->mFont->GetHeight();
    case Op::FontCharWidth: return t->mFont->GetCharacterWidth((int)a);
    case Op::TextWidth: return t->mFont->GetStringWidth(t->mText, (unsigned short)a, (unsigned short)b);
    case Op::TextWidthAll: return t->mFont->GetStringWidth(t->mText);
    case Op::TextCharWidth: return t->mFont->GetCharacterWidth(t->mText[(int)a]);
    case Op::FontStringM: return t->mFont->GetStringWidth(GUIString("M"));
    case Op::TextStore: (a == 0 ? t->mText : t->mRealText) = UnitsText(b, c); return 0;
    case Op::Filter: {
        if (!t->mInputFilter) return 2;
        GUIString aText = UnitsText(a, b);
        bool r = t->mInputFilter(t, aText);
        ee_won_gui_sink_units((void*)c, aText.mData->mText, aText.length(), sizeof(wchar_t));
        return r ? 1 : 0;
    }
    case Op::IconWidth: return t->mIcon.get() != NULL ? t->mIcon->mWidth : -1;
    case Op::PaintInputs: {
        EeWonInputPaint* p = (EeWonInputPaint*)a;
        p->disabled_look = t->IsDisabled() && t->mUseDisabledColors;
        p->has_icon = t->mIcon.get() != NULL;
        p->has_focus = t->HasFocus();
        p->text_color = (int)t->mTextColor;
        p->disabled_text_color = t->mDisabledTextColor;
        p->icon_height = p->has_icon ? t->mIcon->mHeight : 0;
        p->font_height = t->mFont->GetHeight();
        p->sel = WONDrawSelOf(t->mSelColor);
        return 0;
    }
    case Op::Draw: {
        WONDrawSlots aSlots;
        aSlots.mImages[0] = t->mIcon.get();
        aSlots.mFont = t->mFont.get();
        aSlots.mString = &t->mText;
        Graphics& g = *(Graphics*)a;
        const EeWonDrawOp& o = *(const EeWonDrawOp*)b;
        auto aBackground = [&](const EeWonDrawOp& call) {
            Background& aBack = call.b ? t->mDisabledBackground : t->mBackground;
            aBack.Paint(g, 0, 0, call.c, call.d, call.e, 0);
        };
        WONDrawOne(g, o, aSlots, aBackground);
        return 0;
    }
    default: return ComponentOp(t, op, a, b);
    }
}

namespace WONAPI {
// FUNCTION: WON 0x006f7f70
// Input filter that accepts only digits, with a leading minus sign allowed at the start of the box
bool InputBox::NumericFilter(InputBox *theInputBox, GUIString &theStr)
{
    int aPos = 0;
    if(theStr[0]=='-')
    {
        if(theInputBox->GetSelStart()>0)
            return false;
        aPos++;
    }
    for(;aPos<theStr.length();aPos++)
        if(!isdigit(theStr[aPos]))
            return false;
    return true;
}

// FUNCTION: WON 0x006f7fd0
// Creates an empty input box with the InputBox font, system colours and the I-beam cursor (the Rust box holds the
// rest: the 500 ms blink, the 100-entry input history, the padding)
InputBox::InputBox()
{
    this->mRust=ee_won_input_new(this,ee_won_guicpp_input_call);
    this->mFont=this->GetNamedFont("InputBox");
    this->SetComponentFlags(0x4008,true);
    this->mTextColor=0x8000000e;
    this->mDisabledTextColor=(int)0x80000006;
    this->mDisabledBackground.SetColor((int)0x80000001);
    this->mUseDisabledColors=true;
    this->SetDefaultCursor(Cursor::GetStandardCursor(StandardCursor_IBeam));
    this->mInputFilter=NULL;
}
// FUNCTION: WON 0x006f8270
// Destroys the input box: the Rust box goes
InputBox::~InputBox() { ee_won_input_free(this->mRust); }
}

// ---- the label

extern "C" intptr_t ee_won_guicpp_label_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)d;
    Label* t = (Label*)inComp;
    switch (op) {
    case Op::Get:
        if ((Fd)a == Fd::Width) return t->Width();
        if ((Fd)a == Fd::Height) return t->Height();
        return 0;
    case Op::BaseSizeChanged: t->Component::Virtual08(); return 0;
    case Op::BasePaint: t->Component::Paint(*(Graphics*)a); return 0;
    case Op::WrapSetMaxWidth: t->mWrappedText->SetMaxWidth((int)a); return 0;
    case Op::WrapSetCentered: t->mWrappedText->SetCentered(a != 0); return 0;
    case Op::WrapSetWrap: t->mWrappedText->SetWrap(a != 0); return 0;
    case Op::WrapWidth: return t->mWrappedText->GetWidth();
    case Op::WrapHeight: return t->mWrappedText->GetHeight();
    case Op::PaintBackground: t->mBackground.Paint(*(Graphics*)a, 0, 0, (int)b, (int)c); return 0;
    case Op::Draw: {
        WONDrawSlots aSlots;
        aSlots.mText = t->mWrappedText.get();
        WONDrawNoCall aNoCall;
        WONDrawOne(*(Graphics*)a, *(const EeWonDrawOp*)b, aSlots, aNoCall);
        return 0;
    }
    default: return ComponentOp(t, op, a, b);
    }
}

namespace WONAPI {
// FUNCTION: WON 0x006f9d80
// Creates an empty, clipped label with the Label font, no wrapping and no background colour (the Rust label holds
// the colour, the top-left alignment and the zero padding)
Label::Label()
{
    this->mRust=ee_won_label_new(this,ee_won_guicpp_label_call);
    this->mWrappedText=new WrappedText;
    this->mWrappedText->SetFont(this->GetNamedFont("Label"));
    this->mWrappedText->SetWrap(false);
    this->SetComponentFlags(ComponentFlag_Clip,true);
    this->mBackground.SetColor(-1);
}
// FUNCTION: WON 0x006f9f50
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
    case Op::ChildSetEnabled: Comp(a)->SetEnabled(b != 0); return 0;
    case Op::ChildSetVisible: Comp(a)->SetVisible(b != 0); return 0;
    case Op::ChildSetPosSize: {
        const EeWonRect4* r = (const EeWonRect4*)b;
        Comp(a)->SetPosSize(r->x, r->y, r->w, r->h);
        return 0;
    }
    case Op::ChildSetPos: Comp(a)->SetPos((int)b, (int)c); return 0;
    case Op::ChildSetSize: Comp(a)->SetSize((int)b, (int)c); return 0;
    case Op::ChildInvalidate: Comp(a)->Invalidate(); return 0;
    case Op::ChildIsVisible: return Comp(a)->IsVisible() ? 1 : 0;
    case Op::ChildIsMine: return Comp(a)->mParent == t ? 1 : 0;
    case Op::ChildWidth: return Comp(a)->Width();
    case Op::ChildHeight: return Comp(a)->Height();
    case Op::ChildLeft: return Comp(a)->Left();
    case Op::ChildTop: return Comp(a)->Top();
    case Op::ChildHold: Comp(a)->AddRef(); return 0;
    case Op::ChildRelease: Comp(a)->Release(); return 0;
    case Op::ChildFlagSet: return Comp(a)->ComponentFlagSet((int)b) ? 1 : 0;
    case Op::ChildSetFlags: Comp(a)->SetComponentFlags((int)b, c != 0); return 0;
    case Op::ChildHasDefaultCursor: return Comp(a)->GetDefaultCursor() != NULL ? 1 : 0;
    case Op::ChildSetDefaultCursor: Comp(a)->SetDefaultCursor(CursorOf(b)); return 0;
    case Op::ChildMouseChild: return (intptr_t)Comp(a)->GetMouseChild();
    case Op::AddChild: t->AddChild(Comp(a)); return 0;
    case Op::MouseChild: return (intptr_t)t->GetMouseChild();
    case Op::IsDisabled: return t->IsDisabled() ? 1 : 0;
    case Op::EventType: return ((ComponentEvent*)a)->mType;
    case Op::EventComponent: return (intptr_t)((ComponentEvent*)a)->mComponent;
    default: return ComponentOp(t, op, a, b);
    }
}

namespace WONAPI {
intptr_t SplitterHostCall(Splitter* t, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c)
{
    switch (op) {
    case Op::Get:
        if ((Fd)a == Fd::Width) return t->Width();
        if ((Fd)a == Fd::Height) return t->Height();
        return 0;
    case Op::BaseSizeChanged: t->Container::Virtual08(); return 0;
    case Op::BaseMouseDown: t->Container::MouseDown((int)a, (int)b, (int)c); return 0;
    case Op::BaseMouseUp: t->Container::MouseUp((int)a, (int)b, (int)c); return 0;
    case Op::BaseMouseDrag: t->Container::MouseDrag((int)a, (int)b); return 0;
    case Op::BaseHandleEvent: t->Container::HandleComponentEvent((ComponentEvent*)a); return 0;
    case Op::ButtonControlId: return Btn(a)->mControlId;
    case Op::ButtonSetControlId: Btn(a)->mControlId = (int)b; return 0;
    case Op::ButtonSetCheck: Btn(a)->SetCheck(b != 0, false); return 0;
    case Op::ButtonIsChecked: return Btn(a)->IsChecked() ? 1 : 0;
    case Op::NewDragComponent: {
        Container* aContainer = (Container*)Comp(a);
        SmartPtr<Component> aDragComponent = new Component;
        aContainer->Fn2F6140(aDragComponent.get(), 3, aContainer, 0, 0, 0, 0, 0);
        aContainer->AddChildEx(aDragComponent.get(), 0);
        return (intptr_t)aDragComponent.get();
    }
    case Op::NewSizedComponent: {
        Component* aComp = new Component;
        aComp->AddRef(); // the creator's SmartPtr, given back with ChildRelease
        aComp->SetSize((int)a, (int)a);
        return (intptr_t)aComp;
    }
    case Op::EventIsButtonPress: return ((ComponentEvent*)a)->mType == ComponentEvent_ButtonPressed ? 1 : 0;
    case Op::EventControlId: return ((ComponentEvent*)a)->GetControlId();
    case Op::EventButton: return (intptr_t)dynamic_cast<Button*>(((ComponentEvent*)a)->mComponent);
    default: return ContainerOp(t, op, a, b, c);
    }
}

// FUNCTION: WON 0x00707060
// Creates an empty clipping splitter (the Rust splitter holds the split at 50% / 50% and no minimum pane sizes)
Splitter::Splitter()
{
    this->mRust=ee_won_split_new(this,ee_won_guicpp_split_call);
    this->SetComponentFlags(ComponentFlag_Clip,true);
}
// FUNCTION: WON 0x007071e0
// Destroys the splitter: the Rust splitter gives its buttons and drag handles back
Splitter::~Splitter() { ee_won_split_free(this->mRust); }
}

extern "C" intptr_t ee_won_guicpp_split_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)d;
    return SplitterHostCall((Splitter*)inComp, op, a, b, c);
}

// ---- the scroll bar

extern "C" intptr_t ee_won_guicpp_bar_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)d;
    Scrollbar* t = (Scrollbar*)inComp;
    switch (op) {
    case Op::Get:
        if ((Fd)a == Fd::Width) return t->Width();
        if ((Fd)a == Fd::Height) return t->Height();
        return 0;
    case Op::BaseSizeChanged: t->Container::Virtual08(); return 0;
    case Op::BaseMouseDown: t->Container::MouseDown((int)a, (int)b, (int)c); return 0;
    case Op::BaseMouseDrag: t->Container::MouseDrag((int)a, (int)b); return 0;
    case Op::BaseAddedToParent: t->Container::AddedToParent(); return 0;
    case Op::BaseHandleEvent: t->Container::HandleComponentEvent((ComponentEvent*)a); return 0;
    case Op::BaseSetEnabled: t->Component::SetEnabled(a != 0); return 0;
    case Op::NewButton: return (intptr_t)static_cast<Component*>(new Button);
    case Op::ButtonSetFlags: static_cast<Button*>(Comp(a))->SetButtonFlags((int)b, c != 0); return 0;
    default: return ContainerOp(t, op, a, b, c);
    }
}

namespace WONAPI {
// FUNCTION: WON 0x00705e10
// Creates an empty vertical or horizontal scrollbar (the Rust bar holds the line steps 10, the page step 50 and a
// resizing thumb of at least 10)
Scrollbar::Scrollbar(bool vertical) { this->mRust=ee_won_bar_new(this,ee_won_guicpp_bar_call,vertical); }
// FUNCTION: WON 0x00705eb0
// Destroys the scrollbar: the Rust bar gives the thumb, the track and the arrows back
Scrollbar::~Scrollbar() { ee_won_bar_free(this->mRust); }
}

// ---- the scroller (`a` the scroll area or a bar)

extern "C" intptr_t ee_won_guicpp_scroller_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)d;
    Scroller* t = (Scroller*)inComp;
    ScrollArea* area = static_cast<ScrollArea*>(Comp(a));
    switch (op) {
    case Op::Get:
        if ((Fd)a == Fd::Width) return t->Width();
        if ((Fd)a == Fd::Height) return t->Height();
        return 0;
    case Op::BaseMouseDown: t->Container::MouseDown((int)a, (int)b, (int)c); return 0;
    case Op::BaseMouseWheel: return t->Container::MouseWheel((int)a) ? 1 : 0;
    case Op::BaseAddedToParent: t->Container::AddedToParent(); return 0;
    case Op::ComponentHandleEvent: t->Component::HandleComponentEvent((ComponentEvent*)a); return 0;
    case Op::VirtSizeChanged: t->Virtual08(); return 0;
    case Op::VirtNotifyScrollChange: t->NotifyScrollChange(a != 0); return 0;
    case Op::HasFocusChild: return t->GetFocusChild() != NULL ? 1 : 0;
    case Op::BarOf: return (intptr_t)static_cast<Scrollbar*>(Comp(a))->mRust;
    case Op::ListenTo: Comp(a)->AddListener(new ComponentListenerComp(t)); return 0;
    case Op::AreaGet: return area->GetField((Fd)b);
    case Op::AreaSet: area->SetField((Fd)b, (int)c); return 0;
    case Op::AreaAdjustPosition: area->AdjustPosition(b != 0); return 0;
    case Op::AreaRequestFocus: area->RequestFocus(); return 0;
    case Op::AreaMouseWheel: return area->MouseWheel((int)b) ? 1 : 0;
    case Op::AreaSetScroller: area->SetScroller(t); return 0;
    default: return ContainerOp(t, op, a, b, c);
    }
}

namespace WONAPI {
// FUNCTION: WON 0x00705540
// Creates a scroller with no scroll area (the Rust scroller; a ScrollbarScroller's bars start without, both
// conditions Enable)
Scroller::Scroller() { this->mRust=ee_won_scroller_new(this,ee_won_guicpp_scroller_call); }
// Destroys the scroller: the Rust scroller gives the bars and the scroll area back (the implicit destructors
// 0x003001e0 and 0x00300060)
Scroller::~Scroller() { ee_won_scroller_free(this->mRust); }
}

// ---- the scroll area

extern "C" intptr_t ee_won_guicpp_pane_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)d;
    ScrollArea* t = (ScrollArea*)inComp;
    switch (op) {
    case Op::Get:
        if ((Fd)a == Fd::Width) return t->Width();
        if ((Fd)a == Fd::Height) return t->Height();
        return 0;
    case Op::BaseSizeChanged: t->Container::Virtual08(); return 0;
    case Op::BaseMouseWheel: return t->Container::MouseWheel((int)a) ? 1 : 0;
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
    case Op::Draw: {
        WONDrawSlots aSlots;
        WONDrawNoCall aNoCall;
        WONDrawOne(*(Graphics*)a, *(const EeWonDrawOp*)b, aSlots, aNoCall);
        return 0;
    }
    default: return ContainerOp(t, op, a, b, c);
    }
}

namespace WONAPI {
// FUNCTION: WON 0x00704df0
// Creates an empty scroll area with its viewport as its child (the Rust area holds the scroll amounts 10 and every
// other number 0, no scroller)
ScrollArea::ScrollArea()
{
    this->mPane=ee_won_pane_new(this,ee_won_guicpp_pane_call);
    this->mViewport=new ScrollAreaViewport(this);
    this->AddChild(this->mViewport.get());
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
    case Op::Get:
        if ((Fd)a == Fd::Width) return t->Width();
        if ((Fd)a == Fd::Height) return t->Height();
        return t->GetField((Fd)a);
    case Op::Set: t->SetField((Fd)a, (int)b); return 0;
    case Op::BaseSizeChanged: t->ScrollArea::Virtual08(); return 0;
    case Op::BaseAdjustPosition: t->ScrollArea::AdjustPosition(a != 0); return 0;
    case Op::BaseMouseDown: t->ScrollArea::MouseDown((int)a, (int)b, (int)c); return 0;
    case Op::BaseMouseUp: t->ScrollArea::MouseUp((int)a, (int)b, (int)c); return 0;
    case Op::BaseMouseExit: t->ScrollArea::MouseExit(); return 0;
    case Op::BaseMouseMove: t->ScrollArea::MouseMove((int)a, (int)b); return 0;
    case Op::BaseMouseEnter: t->ScrollArea::MouseEnter((int)a, (int)b); return 0;
    case Op::BaseMouseDrag: t->ScrollArea::MouseDrag((int)a, (int)b); return 0;
    case Op::BaseKeyDown: return t->ScrollArea::KeyDown((int)a) ? 1 : 0;
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
    case Op::PointerOutside: {
        Window* window = t->GetRoot();
        if (window == NULL) return 0;
        int x = window->mMouseX, y = window->mMouseY;
        Container* parent = t->mParent;
        if (parent == NULL) return 0;
        parent->RootToChild(x, y);
        return t->Contains(x, y) ? 0 : 1;
    }
    case Op::DeliverItemClicked: {
        const EeWonItemClick* k = (const EeWonItemClick*)b;
        t->DeliverEvent(new ListItemClickedEvent(t, Item(a), static_cast<MouseButton>(k->button),
                                                 t->CheckDoubleClick(k->button), k->up, k->x, k->y), false);
        return 0;
    }
    case Op::TipShow:
        if (t->mItemToolTip.get() == NULL) t->mItemToolTip = new ListAreaToolTip(t);
        t->mItemToolTip->DoTip(Item(a));
        return 0;
    case Op::TipHide: if (t->mItemToolTip.get() != NULL) t->mItemToolTip->DoTip(NULL); return 0;
    case Op::TipEnd: if (t->mItemToolTip.get() != NULL) t->mItemToolTip->EndTip(); return 0;
    case Op::SortItems: {
        ListItem** v = (ListItem**)a;
        std::sort(v, v + b, ListArea::LineSortPred(t, t->GetSortFunc(), (int)c));
        return 0;
    }
    case Op::LowerBound: {
        ListItem** v = (ListItem**)a;
        return std::lower_bound(v, v + b, Item(c), ListArea::LineSortPred(t, t->GetSortFunc(), (int)d)) - v;
    }
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
    case Op::NewColumnViewport: {
        ScrollAreaViewport* v = new ScrollAreaViewport(t);
        v->AddRef(); // the column's SmartPtr, given back with ChildRelease
        t->GetViewport()->AddChild(v);
        return (intptr_t)v;
    }
    case Op::ChildSetPos: Comp(a)->SetPos((int)b, (int)c); return 0;
    case Op::ChildRelease: Comp(a)->Release(); return 0;
    case Op::HeaderNotify: ((MultiListHeader*)a)->NotifyColumnChange(); return 0;
    case Op::HeaderSetSortColumn: ((MultiListHeader*)a)->SetSortColumn((int)b, (ListSortType)c); return 0;
    case Op::RowCell: return (intptr_t)((MultiListItem*)a)->GetItem((int)b);
    case Op::RowSetCell: ((MultiListItem*)a)->SetItem(Item(b), (int)c); return 0;
    // the tree list and its rows (treeobj.rs)
    case Op::NodeOf: return (intptr_t)((TreeItem*)a)->mNode;
    case Op::NewTreeItem: {
        TreeItem* n = new TreeItem;
        n->AddRef(); // the creator's SmartPtr, given back with ItemRelease
        return (intptr_t)n;
    }
    case Op::ItemSetSelectable: Item(a)->mSelectable = b != 0; return 0;
    case Op::ItemSetPosXY: Item(a)->SetPos((int)b, (int)c); return 0;
    case Op::EventType: return ((ComponentEvent*)a)->mType;
    case Op::EventCompControlId: return ((ComponentEvent*)a)->mComponent->mControlId;
    case Op::EventComponentTop: return ((ComponentEvent*)a)->mComponent->Top();
    case Op::CheckDoubleClick: return t->CheckDoubleClick((int)a) ? 1 : 0;
    case Op::DeliverTreeOpened: t->DeliverEvent(new TreeItemOpenedEvent(t, (TreeItem*)a), false); return 0;
    case Op::BaseHandleEvent: t->ScrollArea::HandleComponentEvent((ComponentEvent*)a); return 0;
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
    case Op::QueueSubMenu: {
        const int* at = (const int*)b;
        const EeWonRect4* q = (const EeWonRect4*)c;
        PopupParams params((PopupMenu*)a, (int)d, t, at[0], at[1]);
        params.mAvoidRect = WONRectangle(q->x, q->y, q->w, q->h);
        t->GetRoot()->QueueEvent(new DoPopupEvent(params));
        return 0;
    }
    case Op::QueueMouseUp: {
        Window* window = t->GetRoot();
        int x = (int)a, y = (int)b;
        t->LocalToRoot(x, y);
        window->QueueEvent(new MouseEvent(InputEvent_MouseUp, x, y, window->GetKeyMod(), (int)c));
        return 0;
    }
    case Op::MenuTryKeyDown: return ((PopupMenu*)a)->TryKeyDown((int)b) ? 1 : 0;
    case Op::Draw: {
        WONDrawSlots aSlots;
        WONDrawNoCall aNoCall;
        WONDrawOne(*(Graphics*)a, *(const EeWonDrawOp*)b, aSlots, aNoCall);
        return 0;
    }
    default: return ComponentOp(t, op, a, b);
    }
}
}

extern "C" intptr_t ee_won_guicpp_list_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    return ListAreaHostCall((ListArea*)inComp, op, a, b, c, d);
}

namespace WONAPI {
// FUNCTION: WON 0x006fa280
// Creates an unselected, selectable list item with zero position and size
ListItem::ListItem()
{
    this->mTop=this->mWidth=this->mHeight=0;
    this->mSelected=false;
    this->mSelectable=true;
}
// FUNCTION: WON 0x006fa2a0
// Draws nothing
void ListItem::Paint(Graphics&,ListArea*) {}
// FUNCTION: WON 0x006fa2b0
// Orders two items by their addresses (true when this one comes first in memory)
int ListItem::Compare(const ListItem* other,ListArea*) const { return this<other; }

// FUNCTION: WON 0x006fa570
// Creates an empty, unsorted list box with the ListArea font, 2-pixel padding and focus (the Rust list holds the
// default flags: highlighted icons, full-row select, partial lines shown)
ListArea::ListArea()
{
    this->mRust=ee_won_list_new(this,ee_won_guicpp_list_call);
    this->mTextColor=(int)0x8000000e;
    this->mFont=this->GetNamedFont("ListArea");
    this->SetComponentFlags(ComponentFlag_WantFocus,true);
    this->SetPads(2,2,2,2);
    this->mSortFunc=NULL;
}
// FUNCTION: WON 0x006fa860
// Destroys the list box, closing an open item tooltip first; the Rust list gives its items back
ListArea::~ListArea()
{
    ListAreaHostCall(this, Op::TipEnd, 0, 0, 0, 0);
    ee_won_list_free(this->mRust);
}

// FUNCTION: WON 0x006fa550
// Returns whether the clicked item is the list box's current selection
bool ListItemClickedEvent::IsSelectedItem()
{
    return this->mItem.get()==static_cast<ListArea*>(this->mComponent)->GetSelItem();
}

// FUNCTION: WON 0x006fd8b0
// Adds a separator line to the popup menu and resizes the menu to fit (at least 100 pixels wide)
void PopupMenu::AddSeperator()
{
    this->InsertItem(new ComponentListItem(new MSSeperator(6),true,false));
    this->SetSizeEqualToTotal(100,-1);
}
}

// ---- the multi-column list, its rows and header (multiobj.rs)

// a row's own calls out (its SetPos and its cells' references)
extern "C" intptr_t ee_won_guicpp_mrow_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)inComp; (void)c; (void)d;
    switch (op) {
    case Op::ItemSetTop: Item(a)->mTop = (int)b; return 0;
    case Op::ItemSetPos: Item(a)->SetPos(0, (int)b); return 0;
    case Op::ItemHold: Item(a)->AddRef(); return 0;
    case Op::ItemRelease: Item(a)->Release(); return 0;
    default: return 0;
    }
}

extern "C" intptr_t ee_won_guicpp_mhead_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)d;
    MultiListHeader* t = (MultiListHeader*)inComp;
    switch (op) {
    case Op::Get:
        if ((Fd)a == Fd::Width) return t->Width();
        if ((Fd)a == Fd::Height) return t->Height();
        return 0;
    case Op::BaseSizeChanged: t->Container::Virtual08(); return 0;
    case Op::BaseMouseDown: t->Container::MouseDown((int)a, (int)b, (int)c); return 0;
    case Op::BaseMouseUp: t->Container::MouseUp((int)a, (int)b, (int)c); return 0;
    case Op::BaseMouseMove: t->Container::MouseMove((int)a, (int)b); return 0;
    case Op::BaseMouseDrag: t->Container::MouseDrag((int)a, (int)b); return 0;
    case Op::BaseMouseExit: t->Container::MouseExit(); return 0;
    case Op::BaseHandleEvent: t->Container::HandleComponentEvent((ComponentEvent*)a); return 0;
    case Op::MultiOf: return (intptr_t)((MultiListArea*)a)->mMulti;
    case Op::RemoveChild: t->RemoveChild(Comp(a)); return 0;
    case Op::AddChild: t->AddChild(Comp(a)); return 0;
    case Op::ChildSetControlId: Comp(a)->mControlId = (int)b; return 0;
    case Op::ChildControlId: return Comp(a)->mControlId;
    case Op::ChildSetFlags: Comp(a)->SetComponentFlags((int)b, c != 0); return 0;
    case Op::ChildDesiredSize: { int* wh = (int*)b; Comp(a)->GetDesiredSize(wh[0], wh[1]); return 0; }
    case Op::ChildSetEnabled: Comp(a)->SetEnabled(b != 0); return 0;
    case Op::ChildSetVisible: Comp(a)->SetVisible(b != 0); return 0;
    case Op::ChildSetPosSize: {
        const EeWonRect4* r = (const EeWonRect4*)b;
        Comp(a)->SetPosSize(r->x, r->y, r->w, r->h);
        return 0;
    }
    case Op::ChildLeft: return Comp(a)->Left();
    case Op::ChildRight: return Comp(a)->Right();
    case Op::ChildWidth: return Comp(a)->Width();
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
    default: return ComponentOp(t, op, a, b);
    }
}

namespace WONAPI {
// FUNCTION: WON 0x007033a0
// Creates a multi-column list area with the given column count and no sort (the Rust list holds the 5-pixel column
// border, no dynamic column, no width cap and auto grow off; the original reserved room for 11000 items)
MultiListArea::MultiListArea(int columns)
{
    this->mMulti=ee_won_multi_new(this,ee_won_guicpp_list_call,this->mRust);
    this->SetNumColumns(columns);
}
// FUNCTION: WON 0x00703540
// Destroys the list: the Rust list gives its columns' viewports back (the ListArea base then its rows)
MultiListArea::~MultiListArea() { ee_won_multi_free(this->mMulti); }
// FUNCTION: WON 0x00703f60
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

// FUNCTION: WON 0x00702a90
// Creates a clipped list header with sort arrows on, no sort column, attached to a list area
MultiListHeader::MultiListHeader(MultiListArea* area)
{
    this->mHead=ee_won_mhead_new(this,ee_won_guicpp_mhead_call); // no drag, no sort column, sort arrows on
    this->SetComponentFlags(ComponentFlag_Clip,true);
    this->SetMultiListArea(area);
}
// FUNCTION: WON 0x00702b50
// Destroys the header: the Rust header goes (its buttons are the container's children)
MultiListHeader::~MultiListHeader() { ee_won_mhead_free(this->mHead); }
// Paints the header's children (the linker folds this body into the identical ScrollAreaViewport::Paint at 0x00705e00)
void MultiListHeader::Paint(Graphics& graphics) { Container::Paint(graphics); }
// Returns the child under a point (the linker folds this body into the identical RootContainer::FindChildAt
// at 0x00704710)
Component* MultiListHeader::FindChildAt(int x,int y) { return Container::FindChildAt(x,y); }
// FUNCTION: WON 0x00703050
// Returns the sort arrow a header button should draw, found through its enclosing header (none without one)
ListSortType MultiListHeader::GetSortTypeForButton(Component* button)
{
    while(true) {
        Container* parent=button->mParent;
        if(parent==NULL) return ListSortType_None;
        MultiListHeader* header=dynamic_cast<MultiListHeader*>(parent);
        if(header==NULL) {
            button=parent;
            continue;
        }
        if(header->DrawSortArrow()) return header->GetSortType(header->GetColumnIndex(button));
        return ListSortType_None;
    }
}
}

// ---- the tree list and its rows (treeobj.rs)

// a row's own calls out (its SetPos, its walks' other rows, its references)
extern "C" intptr_t ee_won_guicpp_tnode_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)inComp; (void)d;
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
// FUNCTION: WON 0x007140f0
// Creates a closed row at indent 0 with no parent and no index (the Rust row)
TreeItem::TreeItem() { this->mNode=ee_won_tnode_new(this,ee_won_guicpp_tnode_call); }
// FUNCTION: WON 0x00714170
// Destroys the row: the Rust row gives its children and its wrapped item back
TreeItem::~TreeItem() { ee_won_tnode_free(this->mNode); }

// FUNCTION: WON 0x00714ea0
// Creates an empty tree with an open root row, list flags 0x30 off and 0x200 on, and tree flags 3 (the Rust tree)
TreeArea::TreeArea()
{
    this->mTree=ee_won_tree_new(this,ee_won_guicpp_list_call,this->mRust);
    this->TCall(EeWonTreeCall::Init);
}
// FUNCTION: WON 0x00714fc0
// Destroys the tree: the Rust tree gives its root back (and so every row's)
TreeArea::~TreeArea() { ee_won_tree_free(this->mTree); }
// Wraps a text item (the tree's colour and font, an optional icon) in a new row under a parent (not identified in
// the original exe)
TreeItem* TreeArea::InsertString(TreeItem* theParent,const GUIString& theStr,Image* theIcon,int thePos)
{
    SmartPtr<TextListItem> anItem=new TextListItem;
    anItem->mText=theStr;
    anItem->mColor=this->mTextColor;
    anItem->mFont=this->mFont;
    anItem->mImage=theIcon;
    return this->InsertTreeItem(theParent,anItem.get(),thePos);
}
}

// ---- the popup menu (menuobj.rs)

namespace WONAPI {
// FUNCTION: WON 0x006fd150
// Creates a popup menu with the menu font and colors, a popup background and 4-pixel padding (the Rust menu holds
// no submenu, the timer at 0 and mouse mode)
PopupMenu::PopupMenu()
{
    this->mMenu=ee_won_menu_new(this,ee_won_guicpp_list_call,this->mRust);
    this->SetFont(this->GetNamedFont("Menu"));
    this->SetListFlags(0x806,true);
    this->SetBackground(new MSPopup);
    this->mTextColor=0x8000000c;
    this->SetBorderPadding(4,4,4,4);
}
// FUNCTION: WON 0x006fd340
// Destroys the menu: the Rust menu gives the submenu's item and the submenu back
PopupMenu::~PopupMenu() { ee_won_menu_free(this->mMenu); }
}
