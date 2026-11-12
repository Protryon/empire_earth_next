// WONGUI's component objects in Rust (R6 WON round 21): the one outward call table, ee_won_guicpp_call
// (rust/crates/ee_won/src/guihost.rs EeWonGuiOp), and the C++ left of the classes that moved: the TextArea's
// constructor and destructor and the segments it makes (the Rust TextArea is textobj.rs). Each op is a run of the
// original's C++ statements with no decision between them.
#include <windows.h>
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
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/ee_won.h"
#include "ee/wonapi/WONGUIDraw.h"
using namespace WONAPI;

typedef EeWonGuiOp Op;
typedef EeWonGuiField Fd;

static int* FieldOf(TextArea* t, intptr_t inField)
{
    switch ((Fd)inField) {
    case Fd::ScrollWidth: return &t->mScrollArea.width;
    case Fd::ScrollHeight: return &t->mScrollArea.height;
    case Fd::VertOffset: return &t->mVertOffset;
    case Fd::HorzOffset: return &t->mHorzOffset;
    case Fd::TotalWidth: return &t->mTotalWidth;
    case Fd::TotalHeight: return &t->mTotalHeight;
    case Fd::ExtraHeight: return &t->mExtraHeight;
    case Fd::UpScrollAmount: return &t->mUpScrollAmount;
    case Fd::DownScrollAmount: return &t->mDownScrollAmount;
    default: return NULL;
    }
}

static Cursor* CursorOf(intptr_t inCursor)
{
    switch ((EeWonGuiCursor)inCursor) {
    case EeWonGuiCursor::Hand: return Cursor::GetStandardCursor(StandardCursor_Hand);
    case EeWonGuiCursor::IBeam: return Cursor::GetStandardCursor(StandardCursor_IBeam);
    default: return NULL;
    }
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
        if ((Fd)a == Fd::UserHasScrolled) return t->mUserHasScrolled ? 1 : 0;
        if ((Fd)a == Fd::Width) return t->Width();
        if ((Fd)a == Fd::Height) return t->Height();
        return FieldOf(t, a) ? *FieldOf(t, a) : 0;
    case Op::Set:
        if ((Fd)a == Fd::UserHasScrolled) t->mUserHasScrolled = b != 0;
        else if (FieldOf(t, a)) *FieldOf(t, a) = (int)b;
        return 0;
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
    this->mTopPad=this->mLeftPad=this->mRightPad=this->mBottomPad=5;
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
