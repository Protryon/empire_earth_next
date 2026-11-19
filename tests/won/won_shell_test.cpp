// The Windows skin's concrete widgets as shell kinds (R6 WONLobby round 20): the deleted classes' statements
// (MSControlConstructors.cpp's constructors and paints, MSRemainingPainters.cpp, MSPainters.cpp, MSBorderPainters.cpp,
// MenuSeparator.cpp's MSSeperator, CrossPromotionButton.cpp's MaxSplitterButton, the classes' inline members in
// WONLobbyGUI.h, WONLobbyRoomServer.h and WONLobbyMSRemainingPainters.h), copied from the C++ before the port with
// every call on a C++ object (the base Component, Button and MSButtonBase, the wrapped text, the Graphics, the base
// class's virtual) replaced by the shell host op that makes it, against ee_won_gui's kinds (shell_ms.rs) over the
// same scripted world. WONMsPaint is WONGUIDraw.h's (the mspaint.rs plan run, its text call the button base's
// DrawButtonText).
//
// Each of 30,000 sequences makes one widget of a random kind with random constructor arguments and makes 12 random
// calls (Paint, GetDesiredSize where the class overrode it, the classes' own setters, world changes). The traces of
// the two must be the same; so must the kinds' override masks and names (the deleted classes' declarations).
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "rust/won_gui/btnobj.h"
#include "rust/won_gui/barobj.h"
#include "rust/won_gui/ctobj.h"
#include "rust/won_gui/labelobj.h"
#include "rust/won_gui/listobj.h"
#include "rust/won_gui/multiobj.h"
#include "rust/won_gui/scrollerobj.h"
#include "rust/won_gui/tabobj.h"
#include "rust/won_gui/compmem.h"
#include "rust/won_gui/compobj.h"
#include "rust/won_gui/guidraw.h"
#include "rust/won_gui/msbtnobj.h"
#include "rust/won_gui/mspaint.h"
#include "rust/won_gui/shell.h"
#include "rust/won_gui/skinlay.h"

using namespace WONAPI;

// the game defines it (EEWonNet.cpp): the empty wide string GUIString shares
wchar_t gEmptyWideString[2];

typedef EeWonShellHostOp H;
typedef EeWonShellKind K;
typedef EeWonShellVirt V;
typedef EeWonMsBtnCall M;
typedef EeWonMsBtnField Mf;
typedef EeWonBtnCall Bc;
typedef EeWonCompCall Cc;

static int sFailures, sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }

// ---- the world: the widget's size and disabled bit (compmem.rs's block), the button's flags and default state,
// the MS button base's transparency and left pad, the wrapped text's width, the header's sort, the base's size
static struct World {
    int w, h;
    bool disabled;
    unsigned flags;
    bool transparent, isDefault;
    int leftPad, textWidth, sort, baseW, baseH;
    unsigned salt;
} sW;
static std::vector<std::string> sTrace;
static void T(const char* f, ...)
{
    char b[200];
    va_list a;
    va_start(a, f);
    vsnprintf(b, sizeof b, f, a);
    va_end(a);
    sTrace.push_back(b);
}
static const intptr_t kComp = 0x5e7700;
static EeWonCompMembers sBlock;
static EeWonCompMembers* Resolve(intptr_t c)
{
    memset(&sBlock, 0, sizeof sBlock);
    if (c == kComp) {
        sBlock.bounds.width = sW.w;
        sBlock.bounds.height = sW.h;
        sBlock.bits = sW.disabled ? 0x80 : 0;
        return &sBlock;
    }
    // another component (a part the world answered): its members a function of its id and the world's salt
    unsigned h = (unsigned)c * 2654435761u + sW.salt;
    sBlock.bounds.x = (int)(h % 17);
    sBlock.bounds.y = (int)((h >> 5) % 13);
    sBlock.bounds.width = (h >> 9) % 4 == 0 ? 0 : (int)((h >> 11) % 40);
    sBlock.bounds.height = (h >> 13) % 5 == 0 ? 0 : (int)((h >> 15) % 30);
    sBlock.bits = (h >> 17) % 3 == 0 ? 1 : 0; // invisible
    sBlock.invalid = (char)((h >> 19) % 2);
    sBlock.parent = (h >> 21) % 3 == 0 ? kComp : 0;
    return &sBlock;
}
static int sNext; // the world's new widgets
static intptr_t Answer(intptr_t a, intptr_t b, intptr_t c) { return 0x3000 + (intptr_t)(((unsigned)(a * 31 + b * 7 + c) + sW.salt) & 0xfff); }

// one call on a C++ object, the same for both sides: the world's answer
static intptr_t World(H op, intptr_t a, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0)
{
    switch (op) {
    case H::Base:
        T("base %d %ld", (int)a, (long)b);
        if ((V)a == V::DesiredSize) { *(int*)b = sW.baseW; *(int*)c = sW.baseH; }
        return 0;
    case H::Draw: {
        const EeWonDrawOp* o = (const EeWonDrawOp*)b;
        T("draw %ld %d %d %d %d %d %d %d", (long)a, o->kind, o->a, o->b, o->c, o->d, o->e, o->f);
        return 0;
    }
    case H::Comp: T("comp %d %ld %ld", (int)a, (long)b, (long)c); return 0;
    case H::Btn:
        T("btn %d %ld %ld", (int)a, (long)b, (long)c);
        if ((Bc)a == Bc::GetField && b == (intptr_t)EeWonBtnField::Flags) return sW.flags;
        if ((Bc)a == Bc::IsDefaultButton) return sW.isDefault;
        return 0;
    case H::MsBtn:
        T("msb %d %ld %ld %ld", (int)a, (long)b, (long)c, (long)d);
        if ((M)a == M::Get && b == (intptr_t)Mf::Transparent) return sW.transparent;
        if ((M)a == M::Get && b == (intptr_t)Mf::LeftTextPad) return sW.leftPad;
        return 0;
    case H::WrapSetText: T("wraptext %d", (int)((const GUIString*)a)->length()); return 0;
    case H::WrapWidth: T("wrapwidth"); return sW.textWidth;
    case H::SortType: T("sort"); return sW.sort;
    case H::Call: T("call %lx %lx %ld %ld", (long)a, (long)b, (long)c, (long)d); return Answer(a, b, c);
    case H::New: // (a text argument by its length: the two sides' texts are two objects)
        if (((K)a == K::MsTabButton || (K)a == K::MsHeaderButton) && b != 0) T("new %d text%d %ld %ld", (int)a, (int)((const GUIString*)b)->length(), (long)c, (long)d);
        else T("new %d %ld %ld %ld", (int)a, (long)b, (long)c, (long)d);
        return 0x1000 + sNext++;
    case H::NewBase: T("newbase %d", (int)a); return 0x2000 + sNext++;
    case H::Ref: T("ref %lx %ld", (long)a, (long)b); return 0;
    case H::SelfVirt: T("self %d", (int)a); return 0;
    case H::SkinCall:
        // (an out or rectangle argument by its contents: the two sides' are two objects)
        if (a == (intptr_t)EeWonSkinLay::SetPosSize) { const int* r = (const int*)c; T("skin setpos %lx %d %d %d %d", (long)b, r[0], r[1], r[2], r[3]); }
        else if (a == (intptr_t)EeWonSkinLay::DesiredSize) { T("skin desired %lx", (long)b); ((int*)c)[0] = (int)(b & 63); ((int*)c)[1] = (int)((b >> 3) & 31); }
        else T("skin %ld %ld %lx", (long)a, (long)b, (long)c);
        return a == (intptr_t)EeWonSkinLay::Part ? Answer(b, c, 1) : (a & 3);
    case H::Layout: {
        const int* r = (const int*)d;
        T("layout %lx %ld %lx %d %d %d %d %d", (long)a, (long)b, (long)c, r[0], r[1], r[2], r[3], r[4]);
        return 0;
    }
    case H::TabLayoutPoint: T("tablayout"); return 0x5000;
    case H::LabelSetText: T("labeltext %lx %d", (long)a, (int)((const GUIString*)b)->length()); return 0;
    case H::NamedFont: T("font %lx %ld", (long)a, (long)b); return 0;
    case H::ComboPart: T("part %ld %lx", (long)a, (long)b); return 0;
    default: T("op %d", (int)op); return 0;
    }
}
static intptr_t Host(void*, H op, intptr_t a, intptr_t b, intptr_t c, intptr_t d) { return World(op, a, b, c, d); }

// ---- the original classes' calls on their bases (the inline members of WONLobbyGUI.h)
struct Graphics;
static intptr_t KCall(Cc k, intptr_t a = 0, intptr_t b = 0) { return World(H::Comp, (intptr_t)k, a, b); }
static intptr_t BCall(Bc k, intptr_t a = 0, intptr_t b = 0) { return World(H::Btn, (intptr_t)k, a, b); }
static intptr_t MCall(M k, intptr_t a = 0, intptr_t b = 0, intptr_t d = 0) { return World(H::MsBtn, (intptr_t)k, a, b, d); }
static void MSet(Mf f, int v) { MCall(M::Set, (intptr_t)f, v); }
enum { ComponentFlag_WantFocus = 0x8, ComponentFlag_NoInput = 0x800 };
enum { HorzAlign_Left = 0, VertAlign_Center = 1 };
enum { StandardColor_3DHilight = 2, StandardColor_3DShadow = 3, StandardColor_ButtonText = 5, StandardColor_Text = 14 };

struct Orig {
    K kind;
    bool mUseFrame2; int mLowerAmount; int mDirection; int mType; bool mIsUp;
    // Component
    int Width() { return sW.w; }
    int Height() { return sW.h; }
    bool IsDisabled() { return sW.disabled; }
    void SetComponentFlags(int f, bool on) { KCall(Cc::SetFlags, f, on); }
    void SetDesiredSize(int w, int h) { KCall(Cc::SetDesiredSize, w, h); }
    void SetSize(int w, int h) { KCall(Cc::SetSize, w, h); }
    // Button
    unsigned long GetButtonFlags() { return (unsigned long)BCall(Bc::GetField, (intptr_t)EeWonBtnField::Flags); }
    bool IsChecked() { return (GetButtonFlags() & 0x04) ? true : false; }
    bool IsPressed() { return (GetButtonFlags() & 0x20) != 0; }
    void SetButtonFlags(unsigned long f, bool s) { BCall(Bc::SetButtonFlags, (intptr_t)f, s); }
    bool IsDefaultButton() { return BCall(Bc::IsDefaultButton) != 0; }
    void SetText(const GUIString& t, bool inv) { BCall(Bc::SetText, (intptr_t)&t, inv); }
    // MSButtonBase
    struct Text {
        void SetText(const GUIString& t) { World(H::WrapSetText, (intptr_t)&t); }
        int GetWidth() { return (int)World(H::WrapWidth, 0); }
    } mTextObj, *mText = &mTextObj;
    void SetTransparentField(bool t) { MSet(Mf::Transparent, t); }
    bool IsTransparent() { return MCall(M::Get, (intptr_t)Mf::Transparent) != 0; }
    int GetLeftTextPad() { return (int)MCall(M::Get, (intptr_t)Mf::LeftTextPad); }
    void SetStandardTextColor(int c) { MCall(M::SetStandardTextColor, (intptr_t)c); }
    void SetHorzAlign(int h) { MCall(M::SetHorzAlign, (intptr_t)h); }
    void SetHorzAlignField(int h) { MSet(Mf::HAlign, h); }
    void SetVertAlign(int v) { MSet(Mf::VAlign, v); }
    void SetHorzTextPad(int l, int r) { MSet(Mf::LeftTextPad, l); MSet(Mf::RightTextPad, r); }
    void SetVertTextPad(int t, int b) { MSet(Mf::TopTextPad, t); MSet(Mf::BottomTextPad, b); }
    void IncrementHorzTextPad(int l, int r) { MCall(M::IncrementHorzTextPad, l, r); }
    void SetUseFrame2(bool useFrame2) { this->mUseFrame2 = useFrame2; }
    // the base class's virtuals (Component::Paint, MSButtonBase::GetDesiredSize ...)
    void BasePaint(intptr_t g) { World(H::Base, (intptr_t)V::Paint, g); }
    void BaseDesiredSize(int& w, int& h) { World(H::Base, (intptr_t)V::DesiredSize, (intptr_t)&w, (intptr_t)&h); }
    // the Graphics
    void Draw(intptr_t g, int k, int a = 0, int b = 0, int c = 0, int d = 0)
    {
        EeWonDrawOp o; memset(&o, 0, sizeof o);
        o.kind = k; o.a = a; o.b = b; o.c = c; o.d = d;
        World(H::Draw, g, (intptr_t)&o);
    }
    // WONGUIDraw.h's WONMsPaint: the plan run, its text call the button base's DrawButtonText
    void WONMsPaint(intptr_t g, const EeWonMsPaint& p, bool text = false)
    {
        EeWonDrawOp ops[64];
        size_t n = ee_won_mspaint_plan(&p, ops, 64);
        for (size_t i = 0; i < n && i < 64; i++) {
            if (ops[i].kind == EE_WON_DRAW_CALL) {
                if (text && ops[i].a == EE_WON_MSPAINT_CALL_TEXT) MCall(M::DrawButtonText, g, ops[i].b, ops[i].c);
            } else World(H::Draw, g, (intptr_t)&ops[i]);
        }
    }
    static EeWonMsPaint WONMsPaintOf(EeWonMsPaintKind k, int w, int h, uint32_t flags = 0, int a = 0, int b = 0, int c = 0)
    {
        EeWonMsPaint p; p.kind = k; p.flags = flags; p.width = w; p.height = h; p.a = a; p.b = b; p.c = c; p.d = 0; p.e = 0;
        return p;
    }

    // ---- MSControlConstructors.cpp
    // MSButton::Init
    void MSButton_Init()
    {
        this->SetHorzTextPad(8,8);
        this->SetVertTextPad(4,4);
        this->SetComponentFlags(ComponentFlag_WantFocus,true);
        this->SetButtonFlags(0x400,true);
        this->mUseFrame2=false;
    }
    // MSButton() / MSButton(text)
    void MSButton_New(const GUIString* text)
    {
        this->MSButton_Init();
        if (text) {
            this->mText->SetText(*text);
            this->SetDesiredSize(0,0);
        }
    }
    // MSHeaderButton(text) (over MSButton())
    void MSHeaderButton_New(const GUIString& text)
    {
        this->MSButton_Init();
        this->SetComponentFlags(ComponentFlag_WantFocus,false);
        this->SetUseFrame2(true);
        this->IncrementHorzTextPad(2,0);
        this->SetHorzAlign(HorzAlign_Left);
        if(text.length()!=0) {
            this->mText->SetText(text);
            this->SetDesiredSize(0,0);
        }
    }
    // MSTabButton(text, fillWhole, lower)
    void MSTabButton_New(const GUIString& text,bool fillWhole,int lower)
    {
        this->mLowerAmount=lower;
        if(text.length()!=0) {
            this->mText->SetText(text);
            this->SetDesiredSize(0,0);
        }
        this->SetTransparentField(!fillWhole);
        this->SetButtonFlags(0x48,true);
        this->SetVertAlign(VertAlign_Center);
    }
    // MSArrowButton(direction)
    void MSArrowButton_New(int direction)
    {
        this->mDirection=direction;
        this->SetButtonFlags(2,true);
    }
    // MaxSplitterButton(direction) : MSArrowButton(direction) (CrossPromotionButton.cpp)
    void MaxSplitterButton_New(int direction)
    {
        this->MSArrowButton_New(direction);
        this->SetSize(15,15);
        this->SetButtonFlags(2,false);
    }
    // MS3DFrame(type, up)
    void MS3DFrame_New(int type,bool up)
    {
        this->mType=type;
        this->mIsUp=up;
        this->SetComponentFlags(ComponentFlag_NoInput,true);
    }
    // MSSeperator(height)
    void MSSeperator_New(int height) { this->SetSize(this->Width(),height); }
    // MSCheckbox::Init, MSRadioButton::Init
    void MSCheckbox_New() { MCall(M::BoxInit,(intptr_t)EeWonMsBox::Checkbox,13); }
    void MSRadioButton_New() { MCall(M::BoxInit,(intptr_t)EeWonMsBox::Radio,13); }
    // MSLabel() / MSLabel(text)
    void MSLabel_New()
    {
        this->SetStandardTextColor(StandardColor_Text);
        this->SetHorzTextPad(0,0);
        this->SetVertTextPad(0,0);
        this->SetHorzAlignField(HorzAlign_Left);
        this->SetButtonFlags(0x200,true);
    }
    void MSLabel_New(const GUIString& text)
    {
        this->SetStandardTextColor(StandardColor_Text);
        this->SetHorzTextPad(0,0);
        this->SetVertTextPad(0,0);
        this->SetButtonFlags(0x200,true);
        this->SetText(text,true);
        this->SetDesiredSize(0,0);
    }
    // MSCloseButton(), MSEtchedFrame()
    void MSCloseButton_New() { this->SetSize(16,14); }
    void MSEtchedFrame_New() { this->SetComponentFlags(ComponentFlag_NoInput,true); }

    // ---- the paints
    void MS3DFrame_Paint(intptr_t g) {
        this->BasePaint(g);
        WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::Frame3DComponent,this->Width(),this->Height(),this->mIsUp?0:EE_WON_MSPAINT_PRESSED,this->mType));
    }
    void MSArrowButton_Paint(intptr_t g) {
        this->BasePaint(g);
        uint32_t f=this->IsPressed()?EE_WON_MSPAINT_PRESSED:0;
        f|=this->IsDisabled()?EE_WON_MSPAINT_DISABLED:0;
        WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::ArrowButton,this->Width(),this->Height(),f,this->mDirection));
    }
    void MSButton_Paint(intptr_t g) {
        this->BasePaint(g);
        uint32_t f=this->IsPressed()?EE_WON_MSPAINT_PRESSED:0;
        f|=this->IsTransparent()?EE_WON_MSPAINT_TRANSPARENT:0;
        f|=this->IsDefaultButton()?EE_WON_MSPAINT_DEFAULT:0;
        f|=this->mUseFrame2?EE_WON_MSPAINT_FRAME2:0;
        WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::Button,this->Width(),this->Height(),f),true);
    }
    void MSTabButton_GetDesiredSize(int& width,int& height) {
        this->BaseDesiredSize(width,height);
        width+=16;
        height+=5+this->mLowerAmount;
    }
    void MSTabButton_Paint(intptr_t g) {
        this->BasePaint(g);
        WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::TabButton,this->Width(),this->Height()),true);
    }
    void MSCheckbox_Paint(intptr_t g) { MCall(M::BoxPaint,(intptr_t)EeWonMsBox::Checkbox,g,13); }
    void MSRadioButton_Paint(intptr_t g) { MCall(M::BoxPaint,(intptr_t)EeWonMsBox::Radio,g,13); }
    // MSRemainingPainters.cpp
    void MSHeaderButton_Paint(intptr_t g)
    {
        this->MSButton_Paint(g);
        uint32_t f=this->IsPressed()?EE_WON_MSPAINT_PRESSED:0;
        int sort=this->Sort();
        int width=this->mText->GetWidth();
        int pad=this->GetLeftTextPad();
        WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::HeaderSort,this->Width(),this->Height(),f,sort,width,pad));
    }
    int Sort() { return (int)World(H::SortType, 0); } // MultiListHeader::GetSortTypeForButton(this)
    void MSCloseButton_Paint(intptr_t g)
    {
        this->BasePaint(g);
        WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::CloseButton,this->Width(),this->Height(),this->IsPressed()?EE_WON_MSPAINT_PRESSED:0));
    }
    void MSScrollThumb_Paint(intptr_t g)
    {
        this->BasePaint(g);
        WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::ScrollThumb,this->Width(),this->Height()));
    }
    void MSLabel_Paint(intptr_t g)
    {
        this->BasePaint(g);
        WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::Label,this->Width(),this->Height(),this->IsTransparent()?EE_WON_MSPAINT_TRANSPARENT:0),true);
    }
    // MSPainters.cpp
    void MSPopup_Paint(intptr_t g) { this->BasePaint(g); WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::Popup,this->Width(),this->Height())); }
    void MSDlgBack_Paint(intptr_t g) { this->BasePaint(g); WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::DlgBack,this->Width(),this->Height())); }
    // MSBorderPainters.cpp
    void MSEtchedFrame_Paint(intptr_t g) { this->BasePaint(g); WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::EtchedFrame,this->Width(),this->Height())); }
    void MSInputBox_Paint(intptr_t g)
    {
        this->BasePaint(g);
        // MSGraphicsUtil::DrawFrame3(g,0,0,this->Width(),this->Height(),true): MsDraw(g,Frame3,x,y,w,h,false)
        WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::Frame3,this->Width(),this->Height(),0,0,0));
    }
    // MenuSeparator.cpp
    void MSSeperator_Paint(intptr_t g)
    {
        this->BasePaint(g);
        int y=this->Height()/2-1;
        if(y<0) y=0;
        Draw(g,EE_WON_DRAW_SET_SYS_COLOR,StandardColor_3DShadow);
        Draw(g,EE_WON_DRAW_LINE,0,y,this->Width()-1,y);
        ++y;
        Draw(g,EE_WON_DRAW_SET_SYS_COLOR,StandardColor_3DHilight);
        Draw(g,EE_WON_DRAW_LINE,0,y,this->Width()-1,y);
    }
    // CrossPromotionButton.cpp
    void MaxSplitterButton_Paint(intptr_t g)
    {
        this->MSArrowButton_Paint(g);
        if(!this->IsChecked())
            return;
        int x=this->Width()/2-3,y=this->Height()/2-2;
        if(this->IsPressed()) {
            ++x;
            ++y;
        }
        Draw(g,EE_WON_DRAW_SET_SYS_COLOR,StandardColor_ButtonText);
        Draw(g,EE_WON_DRAW_FILL_RECT,x,y,7,4);
    }
    // the inline GetDesiredSize overrides (MSCloseButton 16 x 14, MSSeperator 100 x 6, MSInputBox 100 x 22)
    void FixedSize(int& width,int& height,int w,int h) { width=w; height=h; }

    // ---- the virtual and op dispatch the classes' vtables made
    bool Paint(intptr_t g)
    {
        switch (kind) {
        case K::MsButton: MSButton_Paint(g); return true;
        case K::MsHeaderButton: MSHeaderButton_Paint(g); return true;
        case K::MsCheckbox: MSCheckbox_Paint(g); return true;
        case K::MsRadioButton: MSRadioButton_Paint(g); return true;
        case K::MsLabel: MSLabel_Paint(g); return true;
        case K::MsTabButton: MSTabButton_Paint(g); return true;
        case K::MsArrowButton: MSArrowButton_Paint(g); return true;
        case K::MaxSplitterButton: MaxSplitterButton_Paint(g); return true;
        case K::MsCloseButton: MSCloseButton_Paint(g); return true;
        case K::Ms3DFrame: MS3DFrame_Paint(g); return true;
        case K::MsSeperator: MSSeperator_Paint(g); return true;
        case K::MsEtchedFrame: MSEtchedFrame_Paint(g); return true;
        case K::MsScrollThumb: MSScrollThumb_Paint(g); return true;
        case K::MsPopup: MSPopup_Paint(g); return true;
        case K::MsDlgBack: MSDlgBack_Paint(g); return true;
        case K::MsInputBox: MSInputBox_Paint(g); return true;
        }
        return false;
    }
    bool DesiredSize(int& w, int& h)
    {
        switch (kind) {
        case K::MsTabButton: MSTabButton_GetDesiredSize(w, h); return true;
        case K::MsCloseButton: FixedSize(w, h, 16, 14); return true;
        case K::MsSeperator: FixedSize(w, h, 100, 6); return true;
        case K::MsInputBox: FixedSize(w, h, 100, 22); return true;
        default: return false;
        }
    }
    void Op(EeWonShellOp op, int a)
    {
        bool button = kind == K::MsButton || kind == K::MsHeaderButton;
        bool arrow = kind == K::MsArrowButton || kind == K::MaxSplitterButton;
        switch (op) {
        case EeWonShellOp::SetUseFrame2: if (button) this->SetUseFrame2(a != 0); break;
        case EeWonShellOp::SetDirection: if (arrow) this->mDirection = a; break;     // MSArrowButton::SetDirection
        case EeWonShellOp::SetLowerAmount: if (kind == K::MsTabButton) this->mLowerAmount = a; break;
        case EeWonShellOp::SetFrameType: if (kind == K::Ms3DFrame) this->mType = a; break;
        case EeWonShellOp::SetIsUp: if (kind == K::Ms3DFrame) this->mIsUp = a != 0; break;
        }
    }
};

static const char* const kNames[] = {"", "MSButton", "MSHeaderButton", "MSCheckbox", "MSRadioButton", "MSLabel",
    "MSTabButton", "MSArrowButton", "MaxSplitterButton", "MSCloseButton", "MS3DFrame", "MSSeperator", "MSEtchedFrame",
    "MSScrollThumb", "MSPopup", "MSDlgBack", "MSInputBox"};
static const int kKinds = 16;

static void RandomWorld()
{
    sW.w = (int)R(60) - 2; sW.h = (int)R(40) - 2;
    sW.disabled = R(3) == 0;
    sW.flags = R(0x800);
    sW.transparent = R(2) != 0; sW.isDefault = R(3) == 0;
    sW.leftPad = (int)R(12); sW.textWidth = (int)R(80); sW.sort = (int)R(3);
    sW.baseW = (int)R(90); sW.baseH = (int)R(30);
    sW.salt = R(0x7fff);
}

static std::vector<std::string> Run(bool rust, unsigned seed, K kind)
{
    sSeed = seed;
    sTrace.clear();
    RandomWorld();
    // the constructor's arguments
    std::wstring w;
    unsigned n = R(4);
    for (unsigned i = 0; i < n; i++) w.push_back((wchar_t)('a' + R(26)));
    GUIString text(w);
    bool withText = R(2) != 0;
    int i0 = (int)R(4), i1 = (int)R(2), i2 = (int)R(7);
    intptr_t a[3] = {0, 0, 0};
    Orig o; memset(&o, 0, sizeof o); o.mText = &o.mTextObj; o.kind = kind;
    EeWonShell* s = NULL;
    if (rust) {
        switch (kind) {
        case K::MsButton: case K::MsLabel: a[0] = withText ? (intptr_t)&text : 0; break;
        case K::MsHeaderButton: a[0] = (intptr_t)&text; break;
        case K::MsTabButton: a[0] = withText ? (intptr_t)&text : 0; a[1] = i1; a[2] = i2; break;
        case K::MsArrowButton: case K::MaxSplitterButton: a[0] = i0; break;
        case K::Ms3DFrame: a[0] = i0; a[1] = i1; break;
        case K::MsSeperator: a[0] = i2; break;
        default: break;
        }
        s = ee_won_shell_new(kind, (void*)kComp, NULL, Host);
        ee_won_shell_init(s, a[0], a[1], a[2]);
    } else {
        static const GUIString kEmpty;
        switch (kind) {
        case K::MsButton: o.MSButton_New(withText ? &text : NULL); break;
        case K::MsHeaderButton: o.MSHeaderButton_New(text); break;
        case K::MsCheckbox: o.MSCheckbox_New(); break;
        case K::MsRadioButton: o.MSRadioButton_New(); break;
        case K::MsLabel: if (withText) o.MSLabel_New(text); else o.MSLabel_New(); break;
        case K::MsTabButton: o.MSTabButton_New(withText ? text : kEmpty, i1 != 0, i2); break;
        case K::MsArrowButton: o.MSArrowButton_New(i0); break;
        case K::MaxSplitterButton: o.MaxSplitterButton_New(i0); break;
        case K::MsCloseButton: o.MSCloseButton_New(); break;
        case K::Ms3DFrame: o.MS3DFrame_New(i0, i1 != 0); break;
        case K::MsSeperator: o.MSSeperator_New(i2); break;
        case K::MsEtchedFrame: o.MSEtchedFrame_New(); break;
        default: break;
        }
    }
    uint64_t over = rust ? ee_won_shell_overrides(s) : 0;
    for (int step = 0; step < 12; step++) {
        switch (R(5)) {
        case 0: case 1: {
            intptr_t g = 0x100 + step;
            if (rust) { if ((over >> (int)V::Paint) & 1) ee_won_shell_virtual(s, V::Paint, g, 0, 0); }
            else o.Paint(g);
            break;
        }
        case 2: {
            int ww = 1, hh = 2;
            bool done = rust ? ((over >> (int)V::DesiredSize) & 1) != 0 : o.DesiredSize(ww, hh);
            if (rust && done) ee_won_shell_virtual(s, V::DesiredSize, (intptr_t)&ww, (intptr_t)&hh, 0);
            T("desired %d %d %d", done, ww, hh);
            break;
        }
        case 3: {
            EeWonShellOp op = (EeWonShellOp)(1 + R(5));
            int v = (int)R(6) - 1;
            if (rust) ee_won_shell_op(s, op, v, 0); else o.Op(op, v);
            break;
        }
        default: RandomWorld(); break;
        }
    }
    if (rust) {
        T("name %s", ee_won_shell_name(s));
        ee_won_shell_free(s);
    } else T("name %s", kNames[(int)kind]);
    return sTrace;
}

// ---- the composite kinds (MSComboComposition.cpp, MSScrollComposition.cpp, MSControlPainters.cpp,
// MSTabComposition.cpp, MSEtchedFrameGroup.cpp, MSControlConstructors.cpp). A call on another component is the
// host's Call op, its code the registry's object kind << 16 | the call (ee_won_gui registry.rs EeWonGuiKind: Label 2,
// List 6, Container 9, Scroller 16, Bar 17, MHead 18, Tab 19, Shell 20; a component's own call 0). A new widget is
// the host's New (a shell kind) or NewBase; a SmartPtr local or temporary is a Ref pair around its use, except the
// factories' temporaries assigned straight to a member (an AddRef and Release with nothing between: dropped).
enum { GLabel = 2, GList = 6, GCt = 9, GScroller = 16, GBar = 17, GMHead = 18, GTab = 19, GShell = 20 };
typedef EeWonScrollerCall Sc;
typedef EeWonBarCall Ba;
static intptr_t On(int g, intptr_t p, int k, intptr_t a = 0, intptr_t b = 0) { return World(H::Call, p, ((intptr_t)g << 16) | k, a, b); }
static intptr_t NewKind(K k, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0) { return World(H::New, (intptr_t)k, a, b, c); }
static intptr_t NewBase(EeWonShellBaseNew k) { return World(H::NewBase, (intptr_t)k); }
static void Ref(intptr_t p, int d) { World(H::Ref, p, d); }
static int W(intptr_t p) { return Resolve(p)->bounds.width; }
static int Hh(intptr_t p) { return Resolve(p)->bounds.height; }
static int Lf(intptr_t p) { return Resolve(p)->bounds.x; }
static int Tp(intptr_t p) { return Resolve(p)->bounds.y; }
static bool Vis(intptr_t p) { return (Resolve(p)->bits & 1) == 0; }
static void Lay(intptr_t child, int flags, intptr_t ref, int a, int b, int c, int d, int e)
{
    int r[5] = {a, b, c, d, e};
    World(H::Layout, child, flags, ref, (intptr_t)r);
}
struct OrigC;
static OrigC* sOrigC;
static intptr_t OrigSkinHost(void*, EeWonGuiOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
struct OrigC {
    K kind;
    bool mDrawFrame, mIsTransparent, mDraw3DFrame, mTransparent;
    int mHeaderHeight;
    intptr_t mHeader, mLabel, mComponent;
    int Width() { return sW.w; }
    int Height() { return sW.h; }
    void SetSize(int w, int h) { KCall(Cc::SetSize, w, h); }
    void BasePaint(intptr_t g) { World(H::Base, (intptr_t)V::Paint, g); }
    void BaseCall(V v) { World(H::Base, (intptr_t)v); }
    void Skin(EeWonSkinLayKind k, int* w = NULL, int* h = NULL) { sOrigC = this; ee_won_skin_layout(k, (void*)kComp, OrigSkinHost, 0, 0, 0, w, h); }
    void MsPaint(intptr_t g, EeWonMsPaintKind k, int w, int h, uint32_t f, int a = 0, int b = 0, int c = 0, int d = 0, int e = 0)
    {
        EeWonMsPaint p; p.kind = k; p.flags = f; p.width = w; p.height = h; p.a = a; p.b = b; p.c = c; p.d = d; p.e = e;
        Orig o; memset(&o, 0, sizeof o);
        o.WONMsPaint(g, p);
    }
    // MSComboBox::MSComboBox
    void MSComboBox_New()
    {
        this->mDrawFrame=true;
        intptr_t button=NewKind(K::MsArrowButton,3); // this->mButton=MSComboBox::ArrowButtonFactory(Direction_Down);
        World(H::ComboPart,(intptr_t)EeWonShellComboPart::Button,button);
        if(W(button)==0) On(0,button,(int)Cc::SetSize,16,Hh(button));
        intptr_t edit=NewBase(EeWonShellBaseNew::InputBox); World(H::ComboPart,(intptr_t)EeWonShellComboPart::Edit,edit);
        intptr_t list=NewBase(EeWonShellBaseNew::ListArea); World(H::ComboPart,(intptr_t)EeWonShellComboPart::ListArea,list);
        World(H::NamedFont,list,(intptr_t)EeWonShellBaseNew::ListArea); // mListArea->SetFont(GetNamedFont("MSButton"))
        On(GList,list,(int)EeWonListCall::SetListFlags,2,true);
        World(H::NamedFont,edit,(intptr_t)EeWonShellBaseNew::InputBox);
        intptr_t listBox=NewKind(K::MsScrollContainer,list); Ref(listBox,1); // SmartPtr<MSScrollContainer> listBox
        On(GScroller,listBox,(int)Sc::SetScrollbarConditions,0,2);
        On(GShell,listBox,(int)EeWonShellOp::SetDraw3DFrame,false);
        World(H::ComboPart,(intptr_t)EeWonShellComboPart::ListBox,listBox);
        On(3,button,(int)Bc::SetButtonFlags,2,false); // mButton->SetButtonFlags(2,false) (Button 3)
        Ref(listBox,-1);
    }
    void MSComboBox_Paint(intptr_t g) { this->BasePaint(g); MsPaint(g,EeWonMsPaintKind::ComboBox,this->Width(),this->Height(),this->mDrawFrame?EE_WON_MSPAINT_DRAW_FRAME:0); }
    void MSComboBox_Virtual08() { this->BaseCall(V::SizeChanged); Skin(EeWonSkinLayKind::MsCombo); }
    // MSScrollbar::MSScrollbar(vertical)
    void MSScrollbar_New(bool vertical)
    {
        this->mIsTransparent=false;
        int up=vertical?2:0, down=vertical?3:1;
        intptr_t b=NewKind(K::MsArrowButton,up); Ref(b,1); On(GBar,kComp,(int)Ba::SetPart,0,b); Ref(b,-1); // SetUpArrow(F(..))
        b=NewKind(K::MsArrowButton,down); Ref(b,1); On(GBar,kComp,(int)Ba::SetPart,1,b); Ref(b,-1);
        if(W(On(GBar,kComp,(int)Ba::GetPart,0))==0 || Hh(On(GBar,kComp,(int)Ba::GetPart,0))==0)
            On(0,On(GBar,kComp,(int)Ba::GetPart,0),(int)Cc::SetSizeAtLeast,15,15);
        if(W(On(GBar,kComp,(int)Ba::GetPart,1))==0 || Hh(On(GBar,kComp,(int)Ba::GetPart,1))==0)
            On(0,On(GBar,kComp,(int)Ba::GetPart,1),(int)Cc::SetSizeAtLeast,15,15);
        On(GBar,kComp,(int)Ba::IsVertical); // SetThumb(ThumbFactory(ThumbType(IsVertical())))
        b=NewKind(K::MsScrollThumb); Ref(b,1); On(GBar,kComp,(int)Ba::SetPart,3,b); Ref(b,-1);
        On(GBar,kComp,(int)Ba::SetPart,2,NewBase(EeWonShellBaseNew::Button)); // SetScrollRect(new Button)
        int w=W(On(GBar,kComp,(int)Ba::GetPart,0));
        this->SetSize(w,Hh(On(GBar,kComp,(int)Ba::GetPart,0)));
    }
    void MSScrollbar_Paint(intptr_t g)
    {
        uint32_t f=Resolve(On(GBar,kComp,(int)Ba::GetPart,2))->invalid!=0?EE_WON_MSPAINT_INVALID:0;
        f|=this->mIsTransparent?EE_WON_MSPAINT_TRANSPARENT:0;
        int l=Lf(On(GBar,kComp,(int)Ba::GetPart,2)), t=Tp(On(GBar,kComp,(int)Ba::GetPart,2));
        int w=W(On(GBar,kComp,(int)Ba::GetPart,2)), h=Hh(On(GBar,kComp,(int)Ba::GetPart,2));
        MsPaint(g,EeWonMsPaintKind::Scrollbar,0,0,f,l,t,w,h);
        this->BasePaint(g);
    }
    // MSScrollContainer::MSScrollContainer(area); MSListCtrl::MSListCtrl(area)
    void MSScrollContainer_New(intptr_t area)
    {
        On(GScroller,kComp,(int)Sc::SetBar,true,NewKind(K::MsScrollbar,true));
        On(GScroller,kComp,(int)Sc::SetBar,false,NewKind(K::MsScrollbar,false));
        On(GScroller,kComp,(int)Sc::SetScrollArea,area);
        this->mHeaderHeight=0; this->mDraw3DFrame=true; this->mDrawFrame=true; this->mTransparent=false;
    }
    void MSListCtrl_New(intptr_t area)
    {
        this->MSScrollContainer_New(area?area:NewBase(EeWonShellBaseNew::MultiListArea));
        this->mHeader=NewKind(K::MsHeaderControl,On(GScroller,kComp,(int)Sc::GetScrollArea)); Ref(this->mHeader,1);
        On(GCt,kComp,(int)EeWonCtCall::AddChild,this->mHeader);
        this->mHeaderHeight=15;
    }
    void SetFlag(bool& f,bool on) { if(f==on) return; f=on; World(H::SelfVirt,(intptr_t)V::SizeChanged); } // this->Virtual08()
    void MSScrollContainer_Virtual08() { this->BaseCall(V::SizeChanged); Skin(kind==K::MsListCtrl?EeWonSkinLayKind::MsList:EeWonSkinLayKind::MsScroll); }
    void MSScrollContainer_Paint(intptr_t g)
    {
        this->BasePaint(g);
        intptr_t v=On(GScroller,kComp,(int)Sc::GetScrollbar,true), h=On(GScroller,kComp,(int)Sc::GetScrollbar,false);
        uint32_t f=(Vis(v)?EE_WON_MSPAINT_VERT:0)|(Vis(h)?EE_WON_MSPAINT_HORZ:0)|(this->mTransparent?EE_WON_MSPAINT_TRANSPARENT:0)|
                   (this->mDrawFrame?EE_WON_MSPAINT_DRAW_FRAME:0)|(this->mDraw3DFrame?EE_WON_MSPAINT_DRAW_3D:0);
        MsPaint(g,EeWonMsPaintKind::ScrollContainer,this->Width(),this->Height(),f,Lf(v),Tp(v),W(v),Hh(v),Hh(h));
    }
    // MSHeaderControl::SetColumn
    void MSHeaderControl_SetColumn(int column,const GUIString& title)
    {
        intptr_t button=NewKind(K::MsHeaderButton,(intptr_t)&title); Ref(button,1);
        On(GMHead,kComp,(int)EeWonMHeadCall::SetColumnItem,column,button);
        Ref(button,-1);
    }
    // MSTabCtrl::MSTabCtrl
    void MSTabCtrl_New()
    {
        this->mIsTransparent=false;
        intptr_t frame=NewKind(K::Ms3DFrame,1,true); Ref(frame,1);
        intptr_t back=NewKind(K::MsDlgBack); Ref(back,1);
        Lay(back,0x5003,World(H::TabLayoutPoint,0),0,0,0,0,0);
        Lay(frame,0x5003,World(H::TabLayoutPoint,0),0,0,0,0,0);
        On(GCt,kComp,(int)EeWonCtCall::AddChild,back);
        On(GCt,kComp,(int)EeWonCtCall::AddChild,frame);
        intptr_t left=NewKind(K::MsArrowButton,0); Ref(left,1);
        intptr_t right=NewKind(K::MsArrowButton,1); Ref(right,1);
        On(0,left,(int)Cc::SetSize,15,15);
        On(0,right,(int)Cc::SetSize,15,15);
        On(GTab,kComp,(int)EeWonTabCall::SetTabArrows,left,right);
        Ref(right,-1); Ref(left,-1); Ref(back,-1); Ref(frame,-1);
    }
    void MSTabCtrl_AddStandardTab(const GUIString& text,intptr_t container) { On(GTab,kComp,(int)EeWonTabCall::AddTab,NewKind(K::MsTabButton,(intptr_t)&text,true,3),container); }
    // MSEtchedFrameGroup
    void MSEtchedFrameGroup_New()
    {
        this->mLabel=NewBase(EeWonShellBaseNew::Label); Ref(this->mLabel,1);
        On(GLabel,this->mLabel,(int)EeWonLabelCall::SetHorzAlign,1);
    }
    void MSEtchedFrameGroup_AddedToParent()
    {
        if(Resolve(this->mLabel)->parent!=kComp) {
            On(GCt,kComp,(int)EeWonCtCall::AddChild,this->mLabel);
            Lay(this->mLabel,0x1000,kComp,5,0,0,0,0);
            if(this->mComponent!=0) {
                On(GCt,kComp,(int)EeWonCtCall::AddChild,this->mComponent);
                Lay(this->mComponent,0x1001,kComp,10,0,-20,0,0);
                Lay(this->mComponent,0x200,this->mLabel,0,10,0,0,0);
                Lay(this->mComponent,0x80000,kComp,0,0,0,-10,0);
            }
        }
        this->BaseCall(V::AddedToParent);
    }
    void MSEtchedFrameGroup_SetLabelText(const GUIString& text) { World(H::LabelSetText,this->mLabel,(intptr_t)&text); On(0,this->mLabel,(int)Cc::SetDesiredSize,10,0); }
    void MSEtchedFrameGroup_AddContainedComponent(intptr_t c) { if(c) Ref(c,1); intptr_t o=this->mComponent; this->mComponent=c; if(o) Ref(o,-1); }
    void MSEtchedFrameGroup_Paint(intptr_t g)
    {
        this->BasePaint(g);
        MsPaint(g,EeWonMsPaintKind::EtchedGroup,this->Width(),this->Height(),0,Hh(this->mLabel),Lf(this->mLabel),Lf(this->mLabel)+W(this->mLabel));
    }
    void Free()
    {
        if (kind == K::MsListCtrl && this->mHeader) Ref(this->mHeader,-1);
        if (kind == K::MsEtchedFrameGroup) { if (this->mComponent) Ref(this->mComponent,-1); Ref(this->mLabel,-1); }
    }
};
// the original skin host's answers for these classes (SkinnedContainers.cpp's SkinPartOrField): a part or field the
// class held; the base class's members (a combo box's button and edit, a scroller's area and bars) as the game's C++
// host answers them, through the world
static intptr_t OrigSkinHost(void*, EeWonGuiOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    OrigC* t = sOrigC;
    bool part = a == (intptr_t)EeWonSkinLay::Part, field = a == (intptr_t)EeWonSkinLay::Field;
    if (field && t->kind == K::MsComboBox) return t->mDrawFrame ? 1 : 0;
    if (t->kind == K::MsScrollContainer || t->kind == K::MsListCtrl) {
        if (part && c == (intptr_t)EeWonSkinPart::Header) return t->mHeader;
        if (field) return c == (intptr_t)EeWonSkinField::DrawFrame ? t->mDrawFrame : c == (intptr_t)EeWonSkinField::Draw3DFrame ? t->mDraw3DFrame : t->mHeaderHeight;
    }
    if (t->kind == K::MsEtchedFrameGroup && (part || field))
        return !part ? 0 : c == (intptr_t)EeWonSkinPart::Label ? t->mLabel : t->mComponent;
    return World(H::SkinCall, a, b, c, d);
}

static const char* const kContNames[] = {"MSComboBox", "MSScrollbar", "MSScrollContainer", "MSListCtrl",
    "MSHeaderControl", "MSTabCtrl", "MSEtchedFrameGroup"};
static std::vector<std::string> RunC(bool rust, unsigned seed, K kind)
{
    sSeed = seed;
    sTrace.clear();
    sNext = 0;
    RandomWorld();
    OrigC o; memset(&o, 0, sizeof o); o.kind = kind;
    intptr_t arg = R(2) ? 0x700 + (intptr_t)R(64) : 0;
    EeWonShell* s = NULL;
    if (rust) {
        s = ee_won_shell_new(kind, (void*)kComp, NULL, Host);
        ee_won_shell_init(s, kind == K::MsScrollbar ? (intptr_t)(arg != 0) : arg, 0, 0);
    } else {
        switch (kind) {
        case K::MsComboBox: o.MSComboBox_New(); break;
        case K::MsScrollbar: o.MSScrollbar_New(arg != 0); break;
        case K::MsScrollContainer: o.MSScrollContainer_New(arg); break;
        case K::MsListCtrl: o.MSListCtrl_New(arg); break;
        case K::MsTabCtrl: o.MSTabCtrl_New(); break;
        case K::MsEtchedFrameGroup: o.MSEtchedFrameGroup_New(); break;
        default: break;
        }
    }
    uint64_t over = rust ? ee_won_shell_overrides(s) : 0;
    for (int step = 0; step < 14; step++) {
        unsigned what = R(7);
        intptr_t g = 0x100 + step;
        std::wstring w;
        for (unsigned i = R(3); i > 0; i--) w.push_back((wchar_t)('a' + R(26)));
        GUIString text(w);
        int v = (int)R(4) - 1;
        intptr_t comp = R(3) ? 0x900 + (intptr_t)R(16) : 0;
        if (what == 0 || what == 1) { // Paint
            if (rust) { if ((over >> (int)V::Paint) & 1) ee_won_shell_virtual(s, V::Paint, g, 0, 0); }
            else if (kind == K::MsComboBox) o.MSComboBox_Paint(g);
            else if (kind == K::MsScrollbar) o.MSScrollbar_Paint(g);
            else if (kind == K::MsScrollContainer || kind == K::MsListCtrl) o.MSScrollContainer_Paint(g);
            else if (kind == K::MsEtchedFrameGroup) o.MSEtchedFrameGroup_Paint(g);
        } else if (what == 2) { // Virtual08, AddedToParent
            if (rust) {
                if ((over >> (int)V::SizeChanged) & 1) ee_won_shell_virtual(s, V::SizeChanged, 0, 0, 0);
                if ((over >> (int)V::AddedToParent) & 1) ee_won_shell_virtual(s, V::AddedToParent, 0, 0, 0);
            } else if (kind == K::MsComboBox) o.MSComboBox_Virtual08();
            else if (kind == K::MsScrollContainer || kind == K::MsListCtrl) o.MSScrollContainer_Virtual08();
            else if (kind == K::MsEtchedFrameGroup) o.MSEtchedFrameGroup_AddedToParent();
        } else if (what == 3) { // GetDesiredSize
            int ww = 1, hh = 2;
            bool done = rust ? ((over >> (int)V::DesiredSize) & 1) != 0 : (kind == K::MsComboBox || kind == K::MsEtchedFrameGroup);
            if (rust && done) ee_won_shell_virtual(s, V::DesiredSize, (intptr_t)&ww, (intptr_t)&hh, 0);
            else if (done && kind == K::MsComboBox) { ww = 100; hh = 22; }
            else if (done) o.Skin(EeWonSkinLayKind::EtchedSize, &ww, &hh);
            T("desired %d %d %d", done, ww, hh);
        } else if (what == 4 || what == 5) { // the classes' own methods
            EeWonShellOp op = (EeWonShellOp)(6 + R(10));
            if (rust) T("op -> %lx", (long)ee_won_shell_op(s, op, op == EeWonShellOp::SetColumn ? v : op == EeWonShellOp::AddStandardTab ||
                op == EeWonShellOp::SetLabelText ? (intptr_t)&text : op == EeWonShellOp::AddContainedComponent ? comp : v,
                op == EeWonShellOp::SetColumn ? (intptr_t)&text : comp));
            else {
                intptr_t r = 0;
                bool scroll = kind == K::MsScrollContainer || kind == K::MsListCtrl;
                switch (op) {
                case EeWonShellOp::SetDrawFrame: if (kind == K::MsComboBox) o.mDrawFrame = v != 0; else if (scroll) o.SetFlag(o.mDrawFrame, v != 0); break;
                case EeWonShellOp::SetDraw3DFrame: if (scroll) o.SetFlag(o.mDraw3DFrame, v != 0); break;
                case EeWonShellOp::SetTransparent: if (kind == K::MsScrollbar || kind == K::MsTabCtrl) o.mIsTransparent = v != 0; else if (scroll) o.mTransparent = v != 0; break;
                case EeWonShellOp::SetHeaderHeight: if (kind == K::MsListCtrl) o.mHeaderHeight = v; break;
                case EeWonShellOp::GetHeader: if (kind == K::MsListCtrl) r = o.mHeader; break;
                case EeWonShellOp::SetColumn: if (kind == K::MsHeaderControl) o.MSHeaderControl_SetColumn(v, text); break;
                case EeWonShellOp::AddStandardTab: if (kind == K::MsTabCtrl) o.MSTabCtrl_AddStandardTab(text, comp); break;
                case EeWonShellOp::SetLabelText: if (kind == K::MsEtchedFrameGroup) o.MSEtchedFrameGroup_SetLabelText(text); break;
                case EeWonShellOp::AddContainedComponent: if (kind == K::MsEtchedFrameGroup) o.MSEtchedFrameGroup_AddContainedComponent(comp); break;
                case EeWonShellOp::GetLabel: if (kind == K::MsEtchedFrameGroup) r = o.mLabel; break;
                default: break;
                }
                T("op -> %lx", (long)r);
            }
        } else RandomWorld();
    }
    if (rust) {
        T("name %s", ee_won_shell_name(s));
        ee_won_shell_free(s);
    } else {
        T("name %s", kContNames[(int)kind - (int)K::MsComboBox]);
        o.Free();
    }
    return sTrace;
}

int main(int argc, char** argv)
{
    bool print = argc > 1 && strcmp(argv[1], "--print") == 0;
    ee_won_comp_members_resolver(Resolve);
    // the deleted classes' overrides (their declarations): Paint for all; GetDesiredSize for MSTabButton,
    // MSCloseButton, MSSeperator and MSInputBox
    for (int k = 1; k <= kKinds; k++) {
        EeWonShell* s = ee_won_shell_new((K)k, (void*)kComp, NULL, Host);
        uint64_t want = 1ull << (int)V::Paint;
        if ((K)k == K::MsTabButton || (K)k == K::MsCloseButton || (K)k == K::MsSeperator || (K)k == K::MsInputBox)
            want |= 1ull << (int)V::DesiredSize;
        CHECK(ee_won_shell_overrides(s) == want);
        CHECK(strcmp(ee_won_shell_name(s), kNames[k]) == 0);
        ee_won_shell_free(s);
    }
    std::vector<int> met(kKinds + 1, 0);
    for (unsigned seq = 0; seq < 30000; seq++) {
        K kind = (K)(1 + seq % kKinds);
        unsigned seed = 0x5e11u + seq * 7919u;
        std::vector<std::string> a = Run(false, seed, kind), b = Run(true, seed, kind);
        CHECK(a == b);
        if (a != b && sFailures < 6) {
            printf("sequence %u (%s) differs:\n", seq, kNames[(int)kind]);
            for (size_t i = 0; i < a.size() || i < b.size(); i++)
                printf("  %-50s | %s\n", i < a.size() ? a[i].c_str() : "", i < b.size() ? b[i].c_str() : "");
        }
        met[(int)kind] += (int)a.size();
        if (print && seq < 32) {
            printf("# sequence %u %s\n", seq, kNames[(int)kind]);
            for (const std::string& l : a) printf("%s\n", l.c_str());
        }
    }
    for (int k = 1; k <= kKinds; k++) CHECK(met[k] > 0);
    // the composite kinds
    for (unsigned seq = 0; seq < 21000; seq++) {
        K kind = (K)((int)K::MsComboBox + seq % 7);
        unsigned seed = 0xc0417u + seq * 104729u;
        std::vector<std::string> a = RunC(false, seed, kind), b = RunC(true, seed, kind);
        CHECK(a == b);
        if (a != b && sFailures < 6) {
            printf("sequence %u (%s) differs:\n", seq, kContNames[seq % 7]);
            for (size_t i = 0; i < a.size() || i < b.size(); i++)
                printf("  %-60s | %s\n", i < a.size() ? a[i].c_str() : "", i < b.size() ? b[i].c_str() : "");
        }
    }
    printf("won_shell_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
