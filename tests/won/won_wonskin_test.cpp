// The WON skin's widgets and the skin as Rust (R6 WONLobby round 23): the deleted classes' statements (WONFrame.cpp,
// WONFrameOwnerCompletion.cpp, WONSimpleButton.cpp, WONInputBox.cpp, SkinnedContainers.cpp's paints and layouts,
// SkinListControls.cpp, SkinTab.cpp, WONControlDuplicates.cpp, WONControlSetup.cpp, WONListCtrlSetup.cpp,
// SimpleSkinControlConstructors.cpp, SkinControlConstructors.cpp, ControlSetupCompletion.cpp, WONSkinCloneProviders.cpp,
// SkinSelectionAdapters.cpp, WONSkinComposition.cpp, WONSkinListComposition.cpp, WONSkinDefaultGetters.cpp,
// WONSkinCopies.cpp, WONSkinConfig.cpp's click sound and WONMSSkinComposition.cpp, and the classes' inline members),
// copied from the C++ before the port with every call on a C++ object (the base classes, another widget, an image,
// the WONButton, the Graphics) replaced by the shell host op that makes it, against ee_won_gui's kinds
// (shell_won.rs) and skin (skinobj.rs) over the same scripted world.
//
// The kinds: 40,000 sequences each make one widget of a random kind (a tab control and a header control on a new
// skin) and make 14 random calls (Paint, Virtual08, GetDesiredSize, the classes' own methods with random frames,
// images and arguments, world changes). The skin: 8,000 sequences make the Windows look's skin and make 20 random
// calls (its getters, its slots and type maps set with keys in mixed case, the type copies, the click sound). The
// traces of the two sides must be the same, reference for reference.
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
#include "rust/won_gui/comboobj.h"
#include "rust/won_gui/listobj.h"
#include "rust/won_gui/multiobj.h"
#include "rust/won_gui/scrollerobj.h"
#include "rust/won_gui/tabobj.h"
#include "rust/won_gui/compmem.h"
#include "rust/won_gui/compobj.h"
#include "rust/won_gui/guidraw.h"
#include "rust/won_gui/guihost.h"
#include "rust/won_gui/shell.h"
#include "rust/won_gui/shell_won.h"
#include "rust/won_gui/skinlay.h"
#include "rust/won_gui/skinobj.h"
#include "rust/won_gui/wonbtnobj.h"

using namespace WONAPI;

// the game defines it (EEWonNet.cpp): the empty wide string GUIString shares
wchar_t gEmptyWideString[2];

typedef EeWonShellHostOp H;
typedef EeWonShellKind K;
typedef EeWonShellVirt V;
typedef EeWonShellOp O;
typedef EeWonCompCall Cc;
typedef EeWonBtnCall Bc;
typedef EeWonWbCall Wb;
typedef EeWonWbField Wf;
typedef EeWonWbImage Im;
typedef EeWonSkinLay SL;

static int sFailures, sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }

// ---- the world
static struct World {
    int w, h;
    bool disabled, over;
    unsigned flags, mouseDown;
    int sort, color, baseW, baseH;
    intptr_t text;
    unsigned salt;
} sW;
static std::vector<std::string> sTrace;
static void T(const char* f, ...)
{
    char b[240];
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
    unsigned h = (unsigned)c * 2654435761u + sW.salt;
    sBlock.bounds.x = (int)(h % 17);
    sBlock.bounds.y = (int)((h >> 5) % 13);
    sBlock.bounds.width = (h >> 9) % 4 == 0 ? 0 : (int)((h >> 11) % 40);
    sBlock.bounds.height = (h >> 13) % 5 == 0 ? 0 : (int)((h >> 15) % 30);
    sBlock.bits = (h >> 17) % 3 == 0 ? 1 : 0; // invisible
    sBlock.parent = (h >> 21) % 3 == 0 ? kComp : 0;
    return &sBlock;
}
static int sNext; // the world's new objects
static intptr_t Answer(intptr_t a, intptr_t b, intptr_t c) { return 0x3000 + (intptr_t)(((unsigned)(a * 31 + b * 7 + c) + sW.salt) & 0xfff); }
// the two sides' skins (their pointers traced as one token)
static intptr_t sSkinA, sSkinB;
static long Sk(intptr_t p) { return p != 0 && (p == sSkinA || p == sSkinB) ? 0x5c1 : (long)p; }
static std::string Fr(const EeWonFrameIn* f)
{
    char b[96];
    snprintf(b, sizeof b, "frame(%d,%d,%d,%d,%lx)", f->left, f->top, f->right, f->bottom, (long)f->image);
    return b;
}
static int Len(intptr_t s) { return s ? (int)((const GUIString*)s)->length() : -1; }

enum { GButton = 3, GCombo = 5, GList = 6, GCt = 9, GWonButton = 13, GScroller = 16, GBar = 17, GMHead = 18, GTab = 19, GShell = 20 };

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
        T("draw %ld %d %d %d %d %d %d", (long)a, o->kind, o->a, o->b, o->c, o->d, o->e);
        return 0;
    }
    case H::DrawImages: {
        const EeWonDrawOp* o = (const EeWonDrawOp*)b;
        const intptr_t* im = (const intptr_t*)c;
        T("drawimg %ld %d %d %d %d %d [%lx %lx %lx %lx]", (long)a, o->kind, o->a, o->b, o->c, o->d, (long)im[0], (long)im[1], (long)im[2], (long)im[3]);
        return 0;
    }
    case H::Comp: T("comp %d %ld %ld", (int)a, (long)b, (long)c); return 0;
    case H::Btn:
        T("btn %d %ld %ld", (int)a, (long)b, (long)c);
        if ((Bc)a == Bc::GetField && b == (intptr_t)EeWonBtnField::Flags) return sW.flags;
        if ((Bc)a == Bc::GetField && b == (intptr_t)EeWonBtnField::MouseDownState) return sW.mouseDown;
        if ((Bc)a == Bc::IsMouseOver) return sW.over;
        return 0;
    case H::Call: {
        int g = (int)(b >> 16), k = (int)(b & 0xffff);
        if (g == GShell) {
            switch ((O)k) {
            case O::SetFrame: T("call %lx shell SetFrame %s", (long)a, Fr((const EeWonFrameIn*)c).c_str()); return 0;
            case O::GetFrame: {
                EeWonFrameIn* f = (EeWonFrameIn*)c;
                f->left = (int)(Answer(a, 1, 0) & 7); f->top = (int)(Answer(a, 2, 0) & 7);
                f->right = (int)(Answer(a, 3, 0) & 7); f->bottom = (int)(Answer(a, 4, 0) & 7);
                f->image = (Answer(a, 5, 0) & 3) == 0 ? 0 : Answer(a, 6, 0);
                T("call %lx shell GetFrame", (long)a);
                return 0;
            }
            case O::WonSetColumn: {
                const intptr_t* x = (const intptr_t*)d;
                T("call %lx shell WonSetColumn %ld text%d %d", (long)a, (long)c, Len(x[0]), x[1] != 0);
                if (x[1]) *(int*)x[1] = (int)(Answer(a, c, 9) & 31) - 4;
                return 0;
            }
            case O::Duplicate: case O::DuplicateScroll:
                T("call %lx shell dup %d", (long)a, k);
                return 0x1000 + sNext++;
            case O::SetSkin: T("call %lx shell SetSkin %lx", (long)a, Sk(c)); return 0;
            default: break;
            }
        }
        if (g == GButton && k == (int)Bc::SetText) { T("call %lx btn SetText text%d %ld", (long)a, Len(c), (long)d); return 0; }
        if (g == GTab && k == (int)EeWonTabCall::CopyAttributes) { T("call %lx tab CopyAttributes %lx", (long)a, (long)c); return 0; }
        T("call %lx %d/%d %ld %ld", (long)a, g, k, (long)c, (long)d);
        return Answer(a, b, c);
    }
    case H::New:
        if ((K)a == K::WonFrameComponent && b != 0) T("new %d %s", (int)a, Fr((const EeWonFrameIn*)b).c_str());
        else T("new %d %lx %lx %ld", (int)a, Sk(b), Sk(c), (long)d);
        return 0x1000 + sNext++;
    case H::NewBase: T("newbase %d", (int)a); return 0x2000 + sNext++;
    case H::Ref: T("ref %lx %ld", (long)a, (long)b); return 0;
    case H::RefObj: T("refobj %lx %ld", (long)a, (long)b); return 0;
    case H::SelfVirt: T("self %d", (int)a); return 0;
    case H::Img:
        T("img %d %lx", (int)a, (long)b);
        if ((Im)a == Im::Width) return (b * 7 + sW.salt) % 50;
        if ((Im)a == Im::Height) return (b * 11 + sW.salt) % 40;
        if ((Im)a == Im::Duplicate) return 0x4000 + sNext++;
        return 0;
    case H::WonBtn: {
        const intptr_t* x = (const intptr_t*)c;
        if ((Wb)b == Wb::GetButtonState) {
            T("wb %lx GetButtonState %ld", (long)a, (long)x[0]);
            if (x[1]) *(int*)x[1] = sW.color;
            if (x[2]) *(intptr_t*)x[2] = sW.text;
            return 0;
        }
        T("wb %lx %d %ld %ld", (long)a, (int)b, (long)x[0], (long)x[1]);
        return (Wb)b == Wb::Get ? Answer(a, x[0], 3) & 15 : 0;
    }
    case H::WonBtnState: {
        const intptr_t* x = (const intptr_t*)b;
        T("wbstate %lx %ld %lx %ld %lx %ld", (long)a, (long)x[0], (long)x[1], (long)x[2], (long)x[3], (long)x[4]);
        return 0;
    }
    case H::WonBtnCopy: T("wbcopy %lx %lx", (long)a, (long)b); return 0;
    case H::BtnCopy: T("btncopy %lx %lx", (long)a, (long)b); return 0;
    case H::ClickSound: T("click %lx %lx", (long)a, (long)b); return 0;
    case H::TextWidth: T("textwidth %lx", (long)a); return Answer(a, 0, 0) & 63;
    case H::ComboGet: T("combo get %ld", (long)a); return Answer(a, 8, 8);
    case H::ComboPart: T("part %ld %lx", (long)a, (long)b); return 0;
    case H::ComboPartOn: T("parton %lx %ld %lx", (long)a, (long)b, (long)c); return 0;
    case H::VDesiredSize: { int* wh = (int*)b; wh[0] = (int)(a & 15); wh[1] = (int)((a >> 4) & 31); T("vdesired %lx", (long)a); return 0; }
    case H::NewStretchImage: T("newstretch %lx", (long)a); return 0x2000 + sNext++;
    case H::NewMsImage: T("newms %ld %ld %ld %ld", (long)a, (long)b, (long)c, (long)d); return 0x6000 + sNext++;
    case H::GetNamedFont: T("font %s", (const char*)a); return 0x7f00;
    case H::SortType: T("sort"); return sW.sort;
    case H::TabLayoutPoint: T("tablayout"); return 0x5000;
    case H::Layout: {
        const int* r = (const int*)d;
        T("layout %lx %ld %lx %d %d %d %d %d", (long)a, (long)b, (long)c, r[0], r[1], r[2], r[3], r[4]);
        return 0;
    }
    case H::SkinCall:
        if (a == (intptr_t)SL::SetPosSize) { const int* r = (const int*)c; T("skin setpos %lx %d %d %d %d", (long)b, r[0], r[1], r[2], r[3]); }
        else if (a == (intptr_t)SL::DesiredSize) { T("skin desired %lx", (long)b); ((int*)c)[0] = (int)(b & 63); ((int*)c)[1] = (int)((b >> 3) & 31); }
        else if (a == (intptr_t)SL::Part || a == (intptr_t)SL::Field || a == (intptr_t)SL::ListCount || a == (intptr_t)SL::ListTotalWidth || a == (intptr_t)SL::SetColumnWidth || a == (intptr_t)SL::SetLeftPad)
            T("skin %ld %ld %lx", (long)a, (long)b, (long)c);
        else T("skin other %ld", (long)a);
        return a == (intptr_t)SL::Part ? Answer(b, c, 1) : (a & 3);
    default: T("op %d", (int)op); return 0;
    }
}
static intptr_t Host(void*, H op, intptr_t a, intptr_t b, intptr_t c, intptr_t d) { return World(op, a, b, c, d); }

// ---- the original classes' calls (the inline members of the headers, as host ops)
static intptr_t On(int g, intptr_t p, int k, intptr_t a = 0, intptr_t b = 0) { return World(H::Call, p, ((intptr_t)g << 16) | k, a, b); }
static intptr_t NewKind(K k, intptr_t a = 0, intptr_t b = 0) { return World(H::New, (intptr_t)k, a, b, 0); }
static void Ref(intptr_t p, int d) { World(H::Ref, p, d); }
static intptr_t KCall(Cc k, intptr_t a = 0, intptr_t b = 0) { return World(H::Comp, (intptr_t)k, a, b); }
static intptr_t BCall(Bc k, intptr_t a = 0, intptr_t b = 0) { return World(H::Btn, (intptr_t)k, a, b); }
static intptr_t WCall(intptr_t p, Wb k, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0)
{
    intptr_t x[4] = {a, b, c, d};
    return World(H::WonBtn, p, (intptr_t)k, (intptr_t)x);
}
static void WSet(intptr_t p, Wf f, int v) { WCall(p, Wb::Set, (intptr_t)f, v); }
static intptr_t ImgOp(Im w, intptr_t i) { return World(H::Img, (intptr_t)w, i); }
static void DrawRun(intptr_t g, const std::vector<EeWonDrawOp>& ops, intptr_t i0, intptr_t i1 = 0)
{
    intptr_t im[4] = {i0, i1, 0, 0};
    for (const EeWonDrawOp& o : ops) World(H::DrawImages, g, (intptr_t)&o, (intptr_t)im);
}
template <class Plan> static std::vector<EeWonDrawOp> Ops(Plan plan)
{
    std::vector<EeWonDrawOp> v(64);
    v.resize(plan(&v[0], v.size()));
    return v;
}
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
// SmartPtr<Image> / SmartPtr<Component> assignment: the old released, then the new held
static void SetImg(intptr_t& cell, intptr_t v) { if (cell == v) return; if (cell) ImgOp(Im::Release, cell); cell = v; if (v) ImgOp(Im::Hold, v); }
static void SetComp(intptr_t& cell, intptr_t v) { if (cell == v) return; if (cell) Ref(cell, -1); cell = v; if (v) Ref(v, 1); }

// WONFrame
struct OFrame {
    int mLeft, mTop, mRight, mBottom;
    intptr_t mImage;
    EeWonFrameIn In() const { EeWonFrameIn f; f.left = mLeft; f.top = mTop; f.right = mRight; f.bottom = mBottom; f.image = mImage; return f; }
    // WONFrame::Copy: mImage = from.mImage->Duplicate() (the temporary's reference) or NULL, then the borders
    void Copy(const EeWonFrameIn& from)
    {
        if (from.image == 0) SetImg(this->mImage, 0);
        else { intptr_t d = ImgOp(Im::Duplicate, from.image); intptr_t o = this->mImage; this->mImage = d; if (o && o != d) ImgOp(Im::Release, o); }
        this->mLeft = from.left; this->mTop = from.top; this->mRight = from.right; this->mBottom = from.bottom;
    }
    EeWonFrame Of(int w, int h) const
    {
        EeWonFrame r;
        r.has_image = this->mImage != 0;
        r.image_width = r.has_image ? (int)ImgOp(Im::Width, this->mImage) : 0;
        r.image_height = r.has_image ? (int)ImgOp(Im::Height, this->mImage) : 0;
        r.width = w; r.height = h; r.left = r.top = r.right = r.bottom = 0;
        return r;
    }
    // WONFrame::Paint(g, c)
    void Paint(intptr_t g)
    {
        EeWonFrame f = this->Of(sW.w, sW.h);
        DrawRun(g, Ops([&](EeWonDrawOp* o, size_t n) { return ee_won_gui_draw_frame(&f, o, n); }), this->mImage);
    }
    intptr_t Field(intptr_t which) const
    {
        switch ((EeWonSkinField)which) {
        case EeWonSkinField::FrameLeft: return mLeft;
        case EeWonSkinField::FrameTop: return mTop;
        case EeWonSkinField::FrameRight: return mRight;
        case EeWonSkinField::FrameBottom: return mBottom;
        default: return 0;
        }
    }
};

// the skin's type maps' order (WONLobbyGUI.h)
struct GUIStringLessNoCase {
    bool operator()(const GUIString& l, const GUIString& r) const { return l.CompareNoCase(r) < 0; }
};
struct OSkin;
// ---- the deleted widget classes
struct OW {
    K kind;
    // WONSimpleButton
    intptr_t mImages[4]; bool mScaleImage, mCheckMouseButtonDown;
    // WONComboBox
    bool mDrawFrame, mButtonInFrame;
    // WONScrollContainer, WONListCtrl
    int mHeaderHeight; bool mTransparent; intptr_t mCorner, mHeader;
    // WONHeaderControl, WONTabCtrl
    int mTextLeftPad; OSkin* mSkin; intptr_t mBack; bool mIsTransparent;
    OFrame mFrame;
    void BasePaint(intptr_t g) { World(H::Base, (intptr_t)V::Paint, g); }
    void BaseSize() { World(H::Base, (intptr_t)V::SizeChanged, 0); }
    bool IsPressed() { return (BCall(Bc::GetField, (intptr_t)EeWonBtnField::Flags) & 0x20) != 0; }
    void Skin(EeWonSkinLayKind k, int a = 0, intptr_t b = 0, int c = 0, int* w = NULL, int* h = NULL);

    void New(intptr_t a, intptr_t b)
    {
        switch (kind) {
        case K::WonSimpleButton: case K::WonScrollThumb: // SimpleSkinControlConstructors.cpp
            this->mScaleImage = false;
            this->mCheckMouseButtonDown = false;
            BCall(Bc::SetButtonFlags, 1, true);
            if (kind == K::WonScrollThumb) { this->mCheckMouseButtonDown = true; this->mScaleImage = true; }
            break;
        case K::WonHeaderButton: // SkinControlConstructors.cpp
            WCall(kComp, Wb::SetHorzAlign, 0);
            WSet(kComp, Wf::LeftTextPad, 4); WSet(kComp, Wf::TopTextPad, 4); WSet(kComp, Wf::RightTextPad, 4); WSet(kComp, Wf::BottomTextPad, 4);
            KCall(Cc::SetFlags, 0x8, false);
            break;
        case K::WonScrollbar: break; // mIsTransparent=false
        case K::WonComboBox: { // WONControlSetup.cpp
            this->mDrawFrame = true;
            this->mButtonInFrame = true;
            intptr_t e = World(H::NewBase, (intptr_t)EeWonShellBaseNew::InputBox);
            World(H::ComboPart, (intptr_t)EeWonShellComboPart::Edit, e);
            intptr_t l = World(H::NewBase, (intptr_t)EeWonShellBaseNew::ListArea);
            World(H::ComboPart, (intptr_t)EeWonShellComboPart::ListArea, l);
            On(GList, l, (int)EeWonListCall::SetListFlags, 2, 1);
            break;
        }
        case K::WonScrollContainer: case K::WonListCtrl: // ControlSetupCompletion.cpp, WONListCtrlSetup.cpp
            this->mHeaderHeight = 0; this->mTransparent = false;
            if (kind == K::WonListCtrl) this->mHeaderHeight = 0;
            break;
        case K::WonHeaderControl: this->mSkin = (OSkin*)b; this->mTextLeftPad = 0; break;
        case K::WonTabCtrl: this->mSkin = (OSkin*)a; this->mIsTransparent = false; break;
        case K::WonFrameComponent: if (a) this->mFrame.Copy(*(const EeWonFrameIn*)a); break; // (the default: SetFrameImageAndBorders(NULL,0,0,0,0))
        default: break;
        }
    }
    void Paint(intptr_t g);
    void SizeChanged()
    {
        if (kind == K::WonComboBox) { BaseSize(); Skin(EeWonSkinLayKind::WonCombo); }
        else if (kind == K::WonScrollContainer) { BaseSize(); Skin(EeWonSkinLayKind::WonScroll); }
        else if (kind == K::WonListCtrl) { BaseSize(); Skin(EeWonSkinLayKind::WonList); }
    }
    bool DesiredSize(int* w, int* h)
    {
        if (kind == K::WonSimpleButton || kind == K::WonScrollThumb) { Skin(EeWonSkinLayKind::SimpleButtonSize, 0, 0, 0, w, h); return true; }
        if (kind == K::WonComboBox) { Skin(EeWonSkinLayKind::WonComboSize, 0, 0, 0, w, h); return true; }
        return false;
    }
    intptr_t Op(O op, intptr_t a, intptr_t b);
    void Free()
    {
        switch (kind) {
        case K::WonSimpleButton: case K::WonScrollThumb: for (int i = 3; i >= 0; i--) SetImg(this->mImages[i], 0); break;
        case K::WonInputBox: case K::WonComboBox: case K::WonFrameComponent: SetImg(this->mFrame.mImage, 0); break;
        case K::WonScrollContainer: case K::WonListCtrl: SetComp(this->mHeader, 0); SetImg(this->mCorner, 0); SetImg(this->mFrame.mImage, 0); break;
        case K::WonTabCtrl: SetComp(this->mBack, 0); SetImg(this->mFrame.mImage, 0); break;
        default: break;
        }
    }
};

// WONSkin, WONMSSkin
struct OSkin {
    intptr_t mPushButton, mCheckbox, mRadioButton, mHeaderButton, mTabButton, mCloseButton, mComboButton;
    intptr_t mScrollThumb[2], mScrollGutter[2], mArrowButton[4], mScrollbar[2];
    intptr_t mInputBox, mComboBox, mScrollContainer, mListCtrl, mTabCtrl, mPopupBack, mDialogBack;
    typedef std::map<GUIString, intptr_t, GUIStringLessNoCase> TypeMap;
    TypeMap mButtonTypeMap, mSimpleButtonTypeMap;
    intptr_t* Slot(int s)
    {
        intptr_t* all[] = {&mPushButton, &mCheckbox, &mRadioButton, &mHeaderButton, &mTabButton, &mCloseButton, &mComboButton,
            &mScrollThumb[0], &mScrollThumb[1], &mScrollGutter[0], &mScrollGutter[1], &mArrowButton[0], &mArrowButton[1],
            &mArrowButton[2], &mArrowButton[3], &mScrollbar[0], &mScrollbar[1], &mInputBox, &mComboBox, &mScrollContainer,
            &mListCtrl, &mTabCtrl, &mPopupBack, &mDialogBack};
        return all[s];
    }
    // WONButton::Duplicate (WONSkinCloneProviders.cpp): new WONButton; CopyAttributes(this)
    static intptr_t WonDup(intptr_t p) { intptr_t n = World(H::NewBase, (intptr_t)EeWonShellBaseNew::WonButton); World(H::WonBtnCopy, n, p); return n; }
    static intptr_t Dup(intptr_t p) { return On(GShell, p, (int)O::Duplicate); }
    intptr_t GetTabButton() { return WonDup(this->mTabButton); }
    intptr_t GetPushButton() { return WonDup(this->mPushButton); }
    intptr_t GetCheckbox() { return WonDup(this->mCheckbox); }
    intptr_t GetRadioButton() { return WonDup(this->mRadioButton); }
    intptr_t GetHeaderButton() { return Dup(this->mHeaderButton); }
    intptr_t GetCloseButton() { return Dup(this->mCloseButton); }
    intptr_t GetInputBox() { return Dup(this->mInputBox); }
    intptr_t GetBack(intptr_t back) { EeWonFrameIn f; On(GShell, back, (int)O::GetFrame, (intptr_t)&f); return NewKind(K::WonFrameComponent, (intptr_t)&f); }
    intptr_t GetArrowButton(int d) { intptr_t b = Dup(this->mArrowButton[d & 3]); On(0, b, (int)Cc::SetDesiredSize, 0, 0); return b; }
    intptr_t GetScrollThumb(bool v) { return Dup(this->mScrollThumb[v ? 1 : 0]); }
    intptr_t GetScrollGutter(bool v) { return Dup(this->mScrollGutter[v ? 1 : 0]); }
    intptr_t GetScrollbar(bool v)
    {
        intptr_t bar = NewKind(K::WonScrollbar, v);
        On(GBar, bar, (int)EeWonBarCall::CopyAttributes, this->mScrollbar[v ? 1 : 0]);
        On(GBar, bar, (int)EeWonBarCall::SetPart, 3, this->GetScrollThumb(v));
        On(GBar, bar, (int)EeWonBarCall::SetPart, 2, this->GetScrollGutter(v));
        if (v) {
            On(GBar, bar, (int)EeWonBarCall::SetPart, 0, this->GetArrowButton(2));
            On(GBar, bar, (int)EeWonBarCall::SetPart, 1, this->GetArrowButton(3));
            intptr_t up = On(GBar, bar, (int)EeWonBarCall::GetPart, 0);
            On(0, bar, (int)Cc::SetSize, W(up), Hh(bar));
        } else {
            On(GBar, bar, (int)EeWonBarCall::SetPart, 0, this->GetArrowButton(0));
            On(GBar, bar, (int)EeWonBarCall::SetPart, 1, this->GetArrowButton(1));
            intptr_t up = On(GBar, bar, (int)EeWonBarCall::GetPart, 0);
            On(0, bar, (int)Cc::SetSize, W(bar), Hh(up));
        }
        return bar;
    }
    // SetFrame(WONFrame(image, n)): the temporary holds the image over the call
    static void FrameOf(intptr_t c, intptr_t image, int n)
    {
        World(H::RefObj, image, 1);
        EeWonFrameIn f; f.left = f.top = f.right = f.bottom = n; f.image = image;
        On(GShell, c, (int)O::SetFrame, (intptr_t)&f);
        World(H::RefObj, image, -1);
    }
    static intptr_t MsImg(int t, int f = 0, int w = 0, int h = 0) { return World(H::NewMsImage, t, f, w, h); }
    intptr_t GetComboBox()
    {
        intptr_t combo = Dup(this->mComboBox);
        intptr_t arrow = Dup(this->mComboButton);
        On(0, arrow, (int)Cc::SetDesiredSize, 0, 0);
        On(GButton, arrow, (int)Bc::SetButtonFlags, 2, 0);
        intptr_t list = this->GetScrollContainer();
        On(GScroller, list, (int)EeWonScrollerCall::SetScrollbarConditions, 0, 2);
        FrameOf(list, MsImg(16), 0);
        World(H::ComboPartOn, combo, (intptr_t)EeWonShellComboPart::Button, arrow);
        On(GCombo, combo, (int)EeWonComboCall::SetListBox, list);
        return combo;
    }
    intptr_t GetScrollContainer()
    {
        intptr_t c = On(GShell, this->mScrollContainer, (int)O::DuplicateScroll);
        On(GScroller, c, (int)EeWonScrollerCall::SetScrollbar, this->GetScrollbar(true));
        On(GScroller, c, (int)EeWonScrollerCall::SetScrollbar, this->GetScrollbar(false));
        return c;
    }
    intptr_t GetTabCtrl()
    {
        intptr_t tab = Dup(this->mTabCtrl);
        On(GShell, tab, (int)O::SetSkin, (intptr_t)this);
        intptr_t l = this->GetArrowButton(0);
        intptr_t r = this->GetArrowButton(1);
        On(GTab, tab, (int)EeWonTabCall::SetTabArrows, l, r);
        return tab;
    }
    intptr_t GetListCtrl()
    {
        intptr_t list = Dup(this->mListCtrl);
        On(GShell, list, (int)O::CopyScrollAttributes, this->mScrollContainer);
        intptr_t header = NewKind(K::WonHeaderControl, 0, (intptr_t)this);
        On(GShell, list, (int)O::SetHeader, header);
        intptr_t area = World(H::NewBase, (intptr_t)EeWonShellBaseNew::MultiListArea);
        On(GScroller, list, (int)EeWonScrollerCall::SetScrollArea, area);
        On(GScroller, list, (int)EeWonScrollerCall::SetScrollbar, this->GetScrollbar(true));
        On(GScroller, list, (int)EeWonScrollerCall::SetScrollbar, this->GetScrollbar(false));
        intptr_t h = On(GShell, list, (int)O::GetHeader);
        On(GMHead, h, (int)EeWonMHeadCall::SetMultiListArea, area);
        return list;
    }
    intptr_t Get(EeWonSkinGet g, intptr_t a)
    {
        switch (g) {
        case EeWonSkinGet::PushButton: return GetPushButton();
        case EeWonSkinGet::Checkbox: return GetCheckbox();
        case EeWonSkinGet::RadioButton: return GetRadioButton();
        case EeWonSkinGet::TabButton: return GetTabButton();
        case EeWonSkinGet::HeaderButton: return GetHeaderButton();
        case EeWonSkinGet::CloseButton: return GetCloseButton();
        case EeWonSkinGet::InputBox: return GetInputBox();
        case EeWonSkinGet::PopupBack: return GetBack(this->mPopupBack);
        case EeWonSkinGet::DialogBack: return GetBack(this->mDialogBack);
        case EeWonSkinGet::ArrowButton: return GetArrowButton((int)a);
        case EeWonSkinGet::ScrollThumb: return GetScrollThumb(a != 0);
        case EeWonSkinGet::ScrollGutter: return GetScrollGutter(a != 0);
        case EeWonSkinGet::Scrollbar: return GetScrollbar(a != 0);
        case EeWonSkinGet::ComboBox: return GetComboBox();
        case EeWonSkinGet::ScrollContainer: return GetScrollContainer();
        case EeWonSkinGet::ListCtrl: return GetListCtrl();
        default: return GetTabCtrl();
        }
    }
    // WONSkinCopies.cpp
    bool CopyType(bool simple, const GUIString& t, intptr_t to)
    {
        TypeMap& m = simple ? mSimpleButtonTypeMap : mButtonTypeMap;
        TypeMap::iterator it = m.find(t);
        if (it == m.end()) return false;
        if (simple) World(H::BtnCopy, to, it->second);
        else World(H::WonBtnCopy, to, it->second);
        return true;
    }
    void SetType(bool simple, const GUIString& k, intptr_t c) { SetComp((simple ? mSimpleButtonTypeMap : mButtonTypeMap)[k], c); }
    // WONSkinConfig::HandleButtonClickSound
    void ClickSound(intptr_t s)
    {
        intptr_t all[] = {mPushButton, mCheckbox, mRadioButton, mHeaderButton, mTabButton, mCloseButton, mComboButton};
        for (intptr_t b : all) if (b) World(H::ClickSound, b, s);
        for (TypeMap::iterator it = mButtonTypeMap.begin(); it != mButtonTypeMap.end(); ++it) if (it->second) World(H::ClickSound, it->second, s);
    }
    // WONMSSkin::WONMSSkin (WONMSSkinComposition.cpp)
    void New()
    {
        enum { Down = 1, Default = 2, Disabled = 4, Checked = 8 };
        intptr_t aFont = World(H::GetNamedFont, (intptr_t)"MSButton");
        auto NewBtn = [](intptr_t& cell) { SetComp(cell, World(H::NewBase, (intptr_t)EeWonShellBaseNew::WonButton)); return cell; };
        auto State = [](intptr_t b, int s, intptr_t i, int c, intptr_t f, bool ch = false) {
            intptr_t x[5] = {s, i, c, f, ch};
            World(H::WonBtnState, b, (intptr_t)x);
        };
        auto Pads = [](intptr_t b, int l, int t, int r, int bo) { WSet(b, Wf::LeftTextPad, l); WSet(b, Wf::TopTextPad, t); WSet(b, Wf::RightTextPad, r); WSet(b, Wf::BottomTextPad, bo); };
        auto Push = [](intptr_t b, int x, int y) { WSet(b, Wf::PushDX, x); WSet(b, Wf::PushDY, y); };
        // Push Button
        intptr_t b = NewBtn(mPushButton);
        On(GButton, b, (int)Bc::SetButtonFlags, 0x0001, 0);
        On(GButton, b, (int)Bc::SetButtonFlags, 0x0400, 1);
        WSet(b, Wf::DrawGrayText, 1);
        State(b, 0, MsImg(7, 0), -1, aFont);
        State(b, 1, MsImg(7, Down), -1, 0);
        State(b, 4, MsImg(7, Default), -1, 0);
        // Checkbox
        b = NewBtn(mCheckbox);
        Push(b, 0, 0);
        WSet(b, Wf::HasCheck, 1);
        WSet(b, Wf::ScaleImage, 0);
        Pads(b, 13 + 4, 2, 2, 2);
        State(b, 0, MsImg(8, 0, 13, 13), -1, aFont);
        State(b, 1, MsImg(8, Down, 13, 13), -1, 0);
        State(b, 0, MsImg(8, Checked, 13, 13), -1, 0, true);
        State(b, 1, MsImg(8, Checked | Down, 13, 13), -1, 0, true);
        // RadioButton
        b = NewBtn(mRadioButton);
        On(GButton, b, (int)Bc::SetButtonFlags, 0x0008, 1);
        Push(b, 0, 0);
        WSet(b, Wf::HasCheck, 1);
        WSet(b, Wf::ScaleImage, 0);
        Pads(b, 13 + 4, 2, 2, 2);
        State(b, 0, MsImg(9, 0, 13, 13), -1, aFont);
        State(b, 1, MsImg(9, Down, 13, 13), -1, 0);
        State(b, 0, MsImg(9, Checked, 13, 13), -1, 0, true);
        State(b, 1, MsImg(9, Checked | Down, 13, 13), -1, 0, true);
        // HeaderButton
        SetComp(mHeaderButton, NewKind(K::WonHeaderButton)); b = mHeaderButton;
        WSet(b, Wf::ScaleImage, 1);
        State(b, 0, MsImg(13, 0), -1, aFont);
        State(b, 1, MsImg(13, Down), -1, aFont);
        // Tab Button
        b = NewBtn(mTabButton);
        WSet(b, Wf::ScaleImage, 1);
        WSet(b, Wf::HasCheck, 1);
        Push(b, 0, 0);
        State(b, 0, MsImg(14, 0), -1, aFont);
        State(b, 0, MsImg(14, Checked), -1, aFont);
        // Close Button
        SetComp(mCloseButton, NewKind(K::WonSimpleButton)); b = mCloseButton;
        On(GShell, b, (int)O::SetImage, 0, MsImg(17, 0, 16, 14));
        On(GShell, b, (int)O::SetImage, 1, MsImg(17, Down, 16, 14));
        // Arrow Buttons
        for (int i = 0; i < 4; i++) {
            SetComp(mArrowButton[i], NewKind(K::WonSimpleButton)); b = mArrowButton[i];
            On(GShell, b, (int)O::SetImage, 0, MsImg(3 + i, 0, 15, 15));
            On(GShell, b, (int)O::SetImage, 1, MsImg(3 + i, Down, 15, 15));
            On(GShell, b, (int)O::SetImage, 2, MsImg(3 + i, Disabled, 15, 15));
        }
        // Scrollbar, ScrollThumb, and ScrollGutter
        for (int i = 0; i < 2; i++) {
            SetComp(mScrollbar[i], NewKind(K::WonScrollbar, 1));
            SetComp(mScrollThumb[i], NewKind(K::WonScrollThumb));
            On(GShell, mScrollThumb[i], (int)O::SetImage, 0, MsImg(10, 0));
            On(GShell, mScrollThumb[i], (int)O::SetScaleImage, 1);
            SetComp(mScrollGutter[i], NewKind(K::WonSimpleButton));
            On(GShell, mScrollGutter[i], (int)O::SetScaleImage, 1);
            On(GShell, mScrollGutter[i], (int)O::SetImage, 0, MsImg(11, 0));
        }
        // ComboBox Button
        SetComp(mComboButton, NewKind(K::WonSimpleButton)); b = mComboButton;
        On(GShell, b, (int)O::SetScaleImage, 1);
        On(GShell, b, (int)O::SetImage, 0, MsImg(6, 0, 16, 18));
        On(GShell, b, (int)O::SetImage, 1, MsImg(6, Down, 16, 18));
        On(GShell, b, (int)O::SetImage, 2, MsImg(6, Disabled, 16, 18));
        SetComp(mPopupBack, NewKind(K::WonFrameComponent));
        On(GShell, mPopupBack, (int)O::SetFrameImage, MsImg(10, 0));
        SetComp(mDialogBack, NewKind(K::WonFrameComponent));
        On(GShell, mDialogBack, (int)O::SetFrameImage, MsImg(10, 0));
        SetComp(mInputBox, NewKind(K::WonInputBox));
        SetComp(mComboBox, NewKind(K::WonComboBox));
        SetComp(mScrollContainer, NewKind(K::WonScrollContainer));
        SetComp(mListCtrl, NewKind(K::WonListCtrl));
        SetComp(mTabCtrl, NewKind(K::WonTabCtrl));
        FrameOf(mTabCtrl, MsImg(15), 2);
        FrameOf(mInputBox, MsImg(2, Down), 2);
        FrameOf(mComboBox, MsImg(2, Down), 2);
        FrameOf(mScrollContainer, MsImg(2, Down), 2);
        On(GShell, mScrollContainer, (int)O::SetCorner, MsImg(12, 0));
        On(GShell, mListCtrl, (int)O::CopyScrollAttributes, mScrollContainer);
        World(H::RefObj, aFont, -1);
    }
    // ~WONSkin: the maps (the simple one first), then the templates in reverse
    void Free()
    {
        for (TypeMap* m : {&mSimpleButtonTypeMap, &mButtonTypeMap}) {
            std::vector<intptr_t> all;
            for (TypeMap::reverse_iterator it = m->rbegin(); it != m->rend(); ++it) all.push_back(it->second);
            m->clear();
            for (intptr_t c : all) if (c) Ref(c, -1);
        }
        for (int s = 23; s >= 0; s--) SetComp(*Slot(s), 0);
    }
};

// the original skin host's answers (SkinnedContainers.cpp's SkinPartOrField and the classes' own cases)
static OW* sOW;
static intptr_t OrigSkinHost(void*, EeWonGuiOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    OW* t = sOW;
    bool part = a == (intptr_t)SL::Part, field = a == (intptr_t)SL::Field;
    if (t->kind == K::WonComboBox && field)
        return c == (intptr_t)EeWonSkinField::DrawFrame ? t->mDrawFrame : c == (intptr_t)EeWonSkinField::ButtonInFrame ? t->mButtonInFrame : t->mFrame.Field(c);
    if (t->kind == K::WonScrollContainer || t->kind == K::WonListCtrl) {
        bool list = t->kind == K::WonListCtrl;
        if (part && list && c == (intptr_t)EeWonSkinPart::Header) return t->mHeader;
        if (field) return c == (intptr_t)EeWonSkinField::HeaderHeight ? t->mHeaderHeight : t->mFrame.Field(c);
        if (a == (intptr_t)SL::SetHeaderHeight) { t->mHeaderHeight = (int)b; return 0; }
        if (a == (intptr_t)SL::SetColumn && list) { intptr_t x[2] = {c, d}; On(GShell, t->mHeader, (int)O::WonSetColumn, b, (intptr_t)x); return 0; }
        if (a == (intptr_t)SL::SetColumnItem && list) { On(GMHead, t->mHeader, (int)EeWonMHeadCall::SetColumnItem, b, c); return 0; }
    }
    if ((t->kind == K::WonSimpleButton || t->kind == K::WonScrollThumb) && a == (intptr_t)SL::Image) {
        intptr_t i = b >= 0 && b < 4 ? t->mImages[b] : 0;
        if (i == 0) return 0;
        int iw = (int)ImgOp(Im::Width, i), ih = (int)ImgOp(Im::Height, i);
        ((int*)c)[0] = iw; ((int*)c)[1] = ih;
        return 1;
    }
    return World(H::SkinCall, a, b, c, d);
}
void OW::Skin(EeWonSkinLayKind k, int a, intptr_t b, int c, int* w, int* h)
{
    int ww = w ? *w : 0, hh = h ? *h : 0;
    sOW = this;
    ee_won_skin_layout(k, (void*)kComp, OrigSkinHost, a, b, c, &ww, &hh);
    if (w) *w = ww;
    if (h) *h = hh;
}
void OW::Paint(intptr_t g)
{
    switch (kind) {
    case K::WonSimpleButton: case K::WonScrollThumb: {
        BasePaint(g);
        EeWonSimpleButtonPaint p;
        p.disabled = sW.disabled;
        p.check_mouse_down = this->mCheckMouseButtonDown;
        p.pressed = this->IsPressed();
        p.mouse_over = BCall(Bc::IsMouseOver) != 0;
        p.scale_image = this->mScaleImage;
        p.mouse_down_state = (uint8_t)BCall(Bc::GetField, (intptr_t)EeWonBtnField::MouseDownState);
        p.width = sW.w;
        p.height = sW.h;
        intptr_t im[4];
        for (int i = 0; i < 4; i++) {
            intptr_t anImage = this->mImages[i];
            im[i] = anImage;
            p.has_image[i] = anImage != 0;
            p.image_width[i] = anImage ? (int)ImgOp(Im::Width, anImage) : 0;
            p.image_height[i] = anImage ? (int)ImgOp(Im::Height, anImage) : 0;
        }
        std::vector<EeWonDrawOp> ops = Ops([&](EeWonDrawOp* o, size_t n) { return ee_won_gui_draw_simple_button(&p, o, n); });
        for (const EeWonDrawOp& o : ops) World(H::DrawImages, g, (intptr_t)&o, (intptr_t)im);
        break;
    }
    case K::WonHeaderButton: {
        BasePaint(g);
        int aSortType = (int)World(H::SortType, 0);
        if (aSortType == 2) return;
        int aColor = 0; intptr_t aText = 0;
        WCall(kComp, Wb::GetButtonState, g, (intptr_t)&aColor, (intptr_t)&aText);
        int tw = aText ? (int)World(H::TextWidth, aText) : 0;
        int pad = (int)WCall(kComp, Wb::Get, (intptr_t)Wf::LeftTextPad);
        bool pressed = this->IsPressed();
        std::vector<EeWonDrawOp> ops = Ops([&](EeWonDrawOp* o, size_t n) {
            return ee_won_gui_draw_header_arrow(aSortType, aColor, aText != 0, tw, pad, sW.w, sW.h, pressed, o, n); });
        for (const EeWonDrawOp& o : ops) World(H::Draw, g, (intptr_t)&o);
        break;
    }
    case K::WonInputBox: case K::WonFrameComponent: BasePaint(g); this->mFrame.Paint(g); break;
    case K::WonComboBox: {
        BasePaint(g);
        EeWonFrame aFrame = this->mFrame.Of(sW.w, sW.h);
        bool df = this->mDrawFrame, inf = this->mButtonInFrame;
        bool vis = df && !inf ? Vis(World(H::ComboGet, (intptr_t)EeWonShellComboPart::Button)) : true;
        int bw = df && !inf && vis ? W(World(H::ComboGet, (intptr_t)EeWonShellComboPart::Button)) : 0;
        DrawRun(g, Ops([&](EeWonDrawOp* o, size_t n) { return ee_won_gui_draw_combo_frame(df, inf, vis, bw, &aFrame, o, n); }), this->mFrame.mImage);
        break;
    }
    case K::WonScrollContainer: case K::WonListCtrl: {
        BasePaint(g);
        EeWonFrame aFrame = this->mFrame.Of(sW.w, sW.h);
        bool aVert = Vis(On(GScroller, kComp, (int)EeWonScrollerCall::GetScrollbar, 1));
        bool aHorz = aVert && Vis(On(GScroller, kComp, (int)EeWonScrollerCall::GetScrollbar, 0));
        int l = Lf(On(GScroller, kComp, (int)EeWonScrollerCall::GetScrollbar, 1));
        int tp = Tp(On(GScroller, kComp, (int)EeWonScrollerCall::GetScrollbar, 1));
        int hh = Hh(On(GScroller, kComp, (int)EeWonScrollerCall::GetScrollbar, 1));
        DrawRun(g, Ops([&](EeWonDrawOp* o, size_t n) {
            return ee_won_gui_draw_scroll_container(aVert, aHorz, this->mTransparent, this->mCorner != 0, l, tp + hh, &aFrame, o, n); }),
            this->mFrame.mImage, this->mCorner);
        break;
    }
    default: break;
    }
}
intptr_t OW::Op(O op, intptr_t a, intptr_t b)
{
    bool simple = kind == K::WonSimpleButton || kind == K::WonScrollThumb;
    bool scroll = kind == K::WonScrollContainer || kind == K::WonListCtrl, list = kind == K::WonListCtrl;
    bool framed = kind == K::WonInputBox || kind == K::WonComboBox || scroll || kind == K::WonTabCtrl || kind == K::WonFrameComponent;
    const intptr_t* x = (const intptr_t*)b;
    switch (op) {
    case O::SetImage: if (simple && a >= 0 && a < 4) SetImg(this->mImages[a], b); return 0;
    case O::SetScaleImage: if (simple) this->mScaleImage = a != 0; return 0;
    case O::SetCheckMouseButtonDown: if (simple) this->mCheckMouseButtonDown = a != 0; return 0;
    case O::GetFrame: if (framed) *(EeWonFrameIn*)a = this->mFrame.In(); return 0;
    case O::SetFrame:
        if (kind == K::WonTabCtrl) { // SkinTab.cpp WONTabCtrl::SetFrame
            this->mFrame.Copy(*(const EeWonFrameIn*)a);
            intptr_t aFrame = World(H::NewStretchImage, this->mFrame.mImage);
            Ref(aFrame, 1);
            intptr_t page = World(H::TabLayoutPoint, 0);
            Lay(aFrame, 0x5003, page, 0, 0, 0, 0, 0);
            On(GCt, kComp, (int)EeWonCtCall::AddChild, aFrame);
            if (!this->mIsTransparent) {
                SetComp(this->mBack, NewKind(K::MsDlgBack));
                Lay(this->mBack, 0x5003, aFrame, this->mFrame.mLeft, this->mFrame.mTop, -this->mFrame.mRight - this->mFrame.mLeft, -this->mFrame.mTop - this->mFrame.mBottom, 0);
                On(GCt, kComp, (int)EeWonCtCall::AddChild, this->mBack);
            }
            Ref(aFrame, -1);
        } else if (kind == K::WonInputBox || kind == K::WonComboBox || scroll) this->mFrame.Copy(*(const EeWonFrameIn*)a);
        return 0;
    case O::SetDrawFrameField: if (kind == K::WonComboBox) this->mDrawFrame = a != 0; return 0;
    case O::SetButtonInFrame: if (kind == K::WonComboBox) this->mButtonInFrame = a != 0; return 0;
    case O::SetTransparentField: if (scroll) this->mTransparent = a != 0; return 0;
    case O::SetHeaderHeightField: if (scroll) this->mHeaderHeight = (int)a; return 0;
    case O::SetCorner: if (scroll) SetImg(this->mCorner, a); return 0;
    case O::GetCorner: return scroll ? this->mCorner : 0;
    case O::CopyScrollAttributes: // ControlSetupCompletion.cpp: mFrame=from->mFrame; SetCorner(from->mCorner)
        if (scroll) {
            EeWonFrameIn f;
            On(GShell, a, (int)O::GetFrame, (intptr_t)&f);
            this->mFrame.Copy(f);
            SetImg(this->mCorner, On(GShell, a, (int)O::GetCorner));
        }
        return 0;
    case O::DuplicateScroll: case O::Duplicate:
        switch (kind) {
        case K::WonSimpleButton: case K::WonScrollThumb: if (op != O::Duplicate) return 0; { // WONControlDuplicates.cpp
            intptr_t n = NewKind(K::WonSimpleButton);
            for (int i = 0; i < 4; i++) On(GShell, n, (int)O::SetImage, i, this->mImages[i]);
            On(GShell, n, (int)O::SetScaleImage, this->mScaleImage);
            On(GShell, n, (int)O::SetCheckMouseButtonDown, this->mCheckMouseButtonDown);
            World(H::BtnCopy, n, kComp);
            On(0, n, (int)Cc::SetSize, sW.w, sW.h);
            return n;
        }
        case K::WonHeaderButton: if (op != O::Duplicate) return 0; { intptr_t n = NewKind(K::WonHeaderButton); World(H::WonBtnCopy, n, kComp); return n; }
        case K::WonInputBox: if (op != O::Duplicate) return 0; { intptr_t n = NewKind(K::WonInputBox); EeWonFrameIn f = this->mFrame.In(); On(GShell, n, (int)O::SetFrame, (intptr_t)&f); return n; }
        case K::WonComboBox: if (op != O::Duplicate) return 0; {
            intptr_t n = NewKind(K::WonComboBox);
            EeWonFrameIn f = this->mFrame.In();
            On(GShell, n, (int)O::SetFrame, (intptr_t)&f);
            On(GShell, n, (int)O::SetDrawFrameField, this->mDrawFrame);
            On(GShell, n, (int)O::SetButtonInFrame, this->mButtonInFrame);
            return n;
        }
        case K::WonScrollContainer: case K::WonListCtrl:
            if (op == O::Duplicate && list) { intptr_t n = NewKind(K::WonListCtrl); On(GShell, n, (int)O::SetHeaderHeightField, this->mHeaderHeight); return n; }
            { intptr_t n = NewKind(K::WonScrollContainer); On(GShell, n, (int)O::CopyScrollAttributes, kComp); return n; }
        case K::WonTabCtrl: if (op != O::Duplicate) return 0; {
            intptr_t n = NewKind(K::WonTabCtrl, (intptr_t)this->mSkin);
            On(GTab, n, (int)EeWonTabCall::CopyAttributes, kComp);
            On(GShell, n, (int)O::SetTransparent, this->mIsTransparent);
            EeWonFrameIn f = this->mFrame.In();
            On(GShell, n, (int)O::SetFrame, (intptr_t)&f);
            return n;
        }
        default: return 0;
        }
    case O::SetHeader: if (list) { SetComp(this->mHeader, a); On(GCt, kComp, (int)EeWonCtCall::AddChild, a); } return 0;
    case O::GetHeader: return list ? this->mHeader : 0;
    case O::SetStandardColumnHeader: if (list) Skin(EeWonSkinLayKind::WonListTextHeader, (int)a, x[0], (int)x[1]); return 0;
    case O::SetColumnHeader: if (list) Skin(EeWonSkinLayKind::WonListHeader, (int)a, x[0], (int)x[1]); return 0;
    case O::WonSetColumn: // SkinListControls.cpp WONHeaderControl::SetColumn
        if (kind == K::WonHeaderControl && this->mSkin) {
            intptr_t button = this->mSkin->GetHeaderButton();
            Ref(button, 1);
            On(GButton, button, (int)Bc::SetText, x[0], 1);
            if (x[1]) { int wh[2] = {0, 0}; World(H::VDesiredSize, button, (intptr_t)wh); *(int*)x[1] = wh[1]; }
            On(GMHead, kComp, (int)EeWonMHeadCall::SetColumnItem, a, button);
            Ref(button, -1);
        }
        return 0;
    case O::SetSkin: if (kind == K::WonHeaderControl || kind == K::WonTabCtrl) this->mSkin = (OSkin*)a; return 0;
    case O::SetTransparent: // SkinTab.cpp: removes the back; never stores the flag
        if (kind == K::WonTabCtrl && this->mBack != 0 && a != 0) On(GCt, kComp, (int)EeWonCtCall::RemoveChild, this->mBack);
        return 0;
    case O::AddStandardTab:
        if (kind == K::WonTabCtrl && this->mSkin) {
            intptr_t aButton = this->mSkin->GetTabButton();
            Ref(aButton, 1);
            On(GButton, aButton, (int)Bc::SetText, a, 1);
            On(0, aButton, (int)Cc::SetDesiredWidth, 0);
            On(GTab, kComp, (int)EeWonTabCall::AddTab, aButton, b);
            Ref(aButton, -1);
        }
        return 0;
    case O::SetFrameImage: if (kind == K::WonFrameComponent) SetImg(this->mFrame.mImage, a); return 0;
    case O::SetFrameBorder:
        if (kind == K::WonFrameComponent) { int* s[] = {&mFrame.mLeft, &mFrame.mTop, &mFrame.mRight, &mFrame.mBottom}; *s[a & 3] = (int)b; }
        return 0;
    case O::GetFrameBorder:
        if (kind == K::WonFrameComponent) { int s[] = {mFrame.mLeft, mFrame.mTop, mFrame.mRight, mFrame.mBottom}; return s[a & 3]; }
        return 0;
    default: return 0;
    }
}

static void RandomWorld()
{
    sW.w = (int)R(60) - 2; sW.h = (int)R(40) - 2;
    sW.disabled = R(3) == 0; sW.over = R(2) != 0;
    sW.flags = R(0x800); sW.mouseDown = R(3);
    sW.sort = (int)R(3); sW.color = (int)R(0x1000);
    sW.text = R(3) ? 0x8800 + (intptr_t)R(64) : 0;
    sW.baseW = (int)R(50); sW.baseH = (int)R(30);
    sW.salt = R(0x7fff);
}

static const K kKinds[] = {K::WonSimpleButton, K::WonScrollThumb, K::WonHeaderButton, K::WonInputBox, K::WonScrollbar,
    K::WonComboBox, K::WonScrollContainer, K::WonListCtrl, K::WonHeaderControl, K::WonTabCtrl, K::WonFrameComponent};
static const char* const kNames[] = {"WONSimpleButton", "WONScrollThumb", "WONHeaderButton", "WONInputBox", "WONScrollbar",
    "WONComboBox", "WONScrollContainer", "WONListCtrl", "WONHeaderControl", "WONTabCtrl", "WONFrameComponent"};
static const O kOps[] = {O::SetImage, O::SetScaleImage, O::SetCheckMouseButtonDown, O::SetFrame, O::GetFrame,
    O::SetDrawFrameField, O::SetButtonInFrame, O::SetTransparentField, O::SetHeaderHeightField, O::SetCorner, O::GetCorner,
    O::CopyScrollAttributes, O::Duplicate, O::DuplicateScroll, O::SetHeader, O::GetHeader, O::SetStandardColumnHeader,
    O::SetColumnHeader, O::WonSetColumn, O::SetTransparent, O::AddStandardTab, O::SetFrameImage, O::SetFrameBorder,
    O::GetFrameBorder};

static EeWonFrameIn RandFrame()
{
    EeWonFrameIn f;
    f.left = (int)R(9) - 1; f.top = (int)R(9) - 1; f.right = (int)R(9) - 1; f.bottom = (int)R(9) - 1;
    f.image = R(4) ? 0x9000 + (intptr_t)R(32) : 0;
    return f;
}
static std::string FrStr(const EeWonFrameIn& f) { return Fr(&f); }

static std::vector<std::string> Run(bool rust, unsigned seed, int ki)
{
    K kind = kKinds[ki];
    sSeed = seed;
    sTrace.clear();
    sNext = 0;
    RandomWorld();
    // a skin for the kinds that hold one (made the same on both sides)
    bool withSkin = kind == K::WonHeaderControl || kind == K::WonTabCtrl;
    OSkin* os = NULL;
    EeWonSkin* rs = NULL;
    if (withSkin) {
        if (rust) { rs = ee_won_skin_new(Host); sSkinB = (intptr_t)rs; }
        else { os = new OSkin(); os->New(); sSkinA = (intptr_t)os; }
    }
    intptr_t skinArg = rust ? (intptr_t)rs : (intptr_t)os;
    EeWonFrameIn initFrame = RandFrame();
    intptr_t a0 = 0, b0 = 0;
    if (kind == K::WonTabCtrl) a0 = R(4) ? skinArg : 0;
    if (kind == K::WonHeaderControl) b0 = skinArg;
    if (kind == K::WonFrameComponent && R(2)) a0 = (intptr_t)&initFrame;
    if (kind == K::WonScrollbar) a0 = R(2);
    OW o; memset(&o, 0, sizeof o); o.kind = kind;
    EeWonShell* s = NULL;
    if (rust) {
        s = ee_won_shell_new(kind, (void*)kComp, NULL, Host);
        ee_won_shell_init(s, a0, b0, 0);
    } else o.New(a0, b0);
    uint64_t over = rust ? ee_won_shell_overrides(s) : 0;
    for (int step = 0; step < 14; step++) {
        unsigned what = R(8);
        intptr_t g = 0x100 + step;
        std::wstring w;
        for (unsigned i = R(3); i > 0; i--) w.push_back((wchar_t)('a' + R(26)));
        GUIString text(w);
        if (what == 0 || what == 1) { // Paint
            if (rust) { if ((over >> (int)V::Paint) & 1) ee_won_shell_virtual(s, V::Paint, g, 0, 0); }
            else o.Paint(g);
        } else if (what == 2) {
            if (rust) { if ((over >> (int)V::SizeChanged) & 1) ee_won_shell_virtual(s, V::SizeChanged, 0, 0, 0); }
            else o.SizeChanged();
        } else if (what == 3) {
            int ww = 1, hh = 2;
            bool done;
            if (rust) { done = ((over >> (int)V::DesiredSize) & 1) != 0; if (done) ee_won_shell_virtual(s, V::DesiredSize, (intptr_t)&ww, (intptr_t)&hh, 0); }
            else done = o.DesiredSize(&ww, &hh);
            T("desired %d %d %d", done, ww, hh);
        } else if (what <= 6) { // the classes' own methods
            O op = kOps[R(sizeof kOps / sizeof kOps[0])];
            EeWonFrameIn f = RandFrame(), out = {0, 0, 0, 0, 0};
            intptr_t comp = R(3) ? 0x900 + (intptr_t)R(16) : 0;
            intptr_t img = R(3) ? 0xa000 + (intptr_t)R(16) : 0;
            int v = (int)R(6) - 1;
            int desired = 0;
            intptr_t pr[2] = {(intptr_t)&text, v};
            intptr_t a = v, b = 0;
            switch (op) {
            case O::SetImage: a = (intptr_t)R(5) - 1; b = img; break;
            case O::SetFrame: a = (intptr_t)&f; break;
            case O::GetFrame: a = (intptr_t)&out; break;
            case O::SetCorner: case O::SetFrameImage: a = img; break;
            case O::CopyScrollAttributes: case O::SetHeader: a = comp; break;
            case O::SetStandardColumnHeader: b = (intptr_t)pr; break;
            case O::SetColumnHeader: pr[0] = comp; b = (intptr_t)pr; break;
            case O::WonSetColumn: pr[1] = R(2) ? (intptr_t)&desired : 0; b = (intptr_t)pr; break;
            case O::AddStandardTab: a = (intptr_t)&text; b = comp; break;
            case O::SetFrameBorder: a = R(4); b = v; break;
            case O::GetFrameBorder: a = R(4); break;
            default: break;
            }
            intptr_t r = rust ? ee_won_shell_op(s, op, a, b) : o.Op(op, a, b);
            T("op %d -> %lx out %s desired %d", (int)op, (long)r, FrStr(out).c_str(), desired);
        } else RandomWorld();
    }
    if (rust) {
        T("name %s", ee_won_shell_name(s));
        ee_won_shell_free(s);
        if (rs) ee_won_skin_free(rs);
    } else {
        T("name %s", kNames[ki]);
        o.Free();
        if (os) { os->Free(); delete os; }
    }
    return sTrace;
}

static std::vector<std::string> RunSkin(bool rust, unsigned seed)
{
    sSeed = seed;
    sTrace.clear();
    sNext = 0;
    RandomWorld();
    OSkin* os = NULL;
    EeWonSkin* rs = NULL;
    if (rust) { rs = ee_won_skin_new(Host); sSkinB = (intptr_t)rs; }
    else { os = new OSkin(); os->New(); sSkinA = (intptr_t)os; }
    static const char* const kKeys[] = {"push", "PUSH", "Push", "menu", "Menu", "option", "OPTION", "close", "b", "B", ""};
    for (int step = 0; step < 20; step++) {
        unsigned what = R(6);
        GUIString key(kKeys[R(sizeof kKeys / sizeof kKeys[0])]);
        intptr_t comp = R(4) ? 0xb00 + (intptr_t)R(24) : 0;
        if (what == 0) { // a getter
            EeWonSkinGet g = (EeWonSkinGet)(1 + R(17));
            intptr_t a = (intptr_t)R(4);
            intptr_t r = rust ? (intptr_t)ee_won_skin_get(rs, g, a) : os->Get(g, a);
            T("get %d %ld -> %lx", (int)g, (long)a, (long)r);
        } else if (what == 1) { // a slot
            int sl = (int)R(24);
            if (rust) ee_won_skin_set(rs, (EeWonSkinSlot)sl, 0, (void*)comp);
            else SetComp(*os->Slot(sl), comp);
        } else if (what == 2) { // a type
            bool simple = R(2) != 0;
            if (rust) ee_won_skin_set_type(rs, simple, &key, (void*)comp);
            else os->SetType(simple, key, comp);
        } else if (what == 3) {
            bool simple = R(2) != 0;
            intptr_t to = 0xc00 + (intptr_t)R(8);
            bool r = rust ? ee_won_skin_copy_type(rs, simple, &key, (void*)to) : os->CopyType(simple, key, to);
            T("copy %d -> %d", simple, r);
        } else if (what == 4) {
            intptr_t snd = R(3) ? 0xd00 + (intptr_t)R(8) : 0;
            if (rust) ee_won_skin_click_sound(rs, (void*)snd);
            else os->ClickSound(snd);
        } else RandomWorld();
    }
    if (rust) ee_won_skin_free(rs);
    else { os->Free(); delete os; }
    return sTrace;
}

int main(int argc, char** argv)
{
    bool print = argc > 1 && strcmp(argv[1], "--print") == 0;
    ee_won_comp_members_resolver(Resolve);
    const int nk = (int)(sizeof kKinds / sizeof kKinds[0]);
    std::vector<int> met(nk, 0);
    for (unsigned seq = 0; seq < 40000; seq++) {
        int ki = (int)(seq % nk);
        unsigned seed = 0x5c11u + seq * 7919u;
        std::vector<std::string> a = Run(false, seed, ki), b = Run(true, seed, ki);
        CHECK(a == b);
        if (a != b && sFailures < 6) {
            printf("sequence %u (%s) differs:\n", seq, kNames[ki]);
            for (size_t i = 0; i < a.size() || i < b.size(); i++)
                if (i >= a.size() || i >= b.size() || a[i] != b[i] || sFailures < 2)
                    printf("  %4zu %-62s | %s\n", i, i < a.size() ? a[i].c_str() : "", i < b.size() ? b[i].c_str() : "");
        }
        met[ki] += (int)a.size();
        if (print && seq < 22) {
            printf("# sequence %u %s\n", seq, kNames[ki]);
            for (const std::string& l : a) printf("%s\n", l.c_str());
        }
    }
    for (int k = 0; k < nk; k++) CHECK(met[k] > 0);
    for (unsigned seq = 0; seq < 8000; seq++) {
        unsigned seed = 0x51c1u + seq * 104729u;
        std::vector<std::string> a = RunSkin(false, seed), b = RunSkin(true, seed);
        CHECK(a == b);
        if (a != b && sFailures < 6) {
            printf("skin sequence %u differs:\n", seq);
            for (size_t i = 0; i < a.size() || i < b.size(); i++)
                if (i >= a.size() || i >= b.size() || a[i] != b[i])
                    printf("  %4zu %-62s | %s\n", i, i < a.size() ? a[i].c_str() : "", i < b.size() ? b[i].c_str() : "");
        }
    }
    printf("won_wonskin_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
