// The table setters that call a widget's Rust object (R6 WONLobby round 27; rust/crates/ee_won_gui/src/cfgset.rs):
// the forward golden. WONCfgTable.inl's cases as they were (tests/won/won_cfgset_orig.h, copied before the move)
// run over the game's own classes (WONLobbyGUI.h, WONGUIShell.h, ...; the forwards the move left without a caller
// as they were, tests/won/won_cfgset_fwd.h) whose inline forwards' Rust calls are renamed
// to tracers here, against cfgset.rs's descriptions of the same setters (its trace hook) over the same frame. For
// each setter the calls must be the same: the function, the object (each class's own Rust-object member), the op and
// every argument (a pointer argument to a local array by its contents). The method level (which method, its
// arguments as the statements' expressions had them) is won_cfgtab_test's, over its stand-in world.
//
// Also a census: cfgset.rs answers exactly the setters the move deleted from WONCfgTable.inl, no other.
// Inputs: every moved setter on 2,000 random frames each (ints near zero, at the byte and sign edges, the extremes;
// one to twelve of them; a font or none; one or two components; texts). Exit code: the number of mismatches.

// the forwards' Rust calls, renamed before the headers declare them
#define ee_won_btn_call TR_btn_call
#define ee_won_msbtn_call TR_msbtn_call
#define ee_won_shell_op TR_shell_op
#define ee_won_multi_call TR_multi_call
#define ee_won_mhead_call TR_mhead_call
#define ee_won_bar_call TR_bar_call
#define ee_won_tab_call TR_tab_call
#define ee_won_combo_call TR_combo_call
#define ee_won_dlg_call TR_dlg_call
#define ee_won_label_call TR_label_call
#define ee_won_list_call TR_list_call
#define ee_won_input_call TR_input_call
#define ee_won_text_call TR_text_call
#define ee_won_tree_call TR_tree_call
#define ee_won_wonbtn_call TR_wonbtn_call
#define ee_won_wonbtn_set_state TR_wonbtn_set_state
#define ee_won_split_call TR_split_call
#define ee_won_pane_call TR_pane_call
#define ee_won_scroller_call TR_scroller_call

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include <exception>
#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
// the forwards the move left without a caller are copied into subclasses (won_cfgset_fwd.h): their private call
// helpers are opened
#define _ALLOW_KEYWORD_MACROS
#define private public
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
#include "ee/wonapi/WONGUIShell.h"
#include "rust/won_gui/cfgtab.h"
#include "rust/won_gui/cfgset.h"
#include "rust/won_gui/guiconf.h"
#undef private

using namespace WONAPI;

wchar_t gEmptyWideString[2]; // the game's (EEWonNet.cpp)

// ---- the calls, as the forwards make them and as cfgset.rs describes them
struct Call {
    int fun; uint32_t op; intptr_t args[5];
    bool operator==(const Call& o) const { return fun == o.fun && op == o.op && memcmp(args, o.args, sizeof args) == 0; }
};
static std::vector<Call> sCpp;
static int sBadObject;
typedef EeWonCfgFun F;
static intptr_t Mk(F f) { return 0x10000 + (intptr_t)f * 0x100; }
static void Log(F f, const void* o, uint32_t op, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0, intptr_t e = 0)
{
    // the set-state forward is on the WON button's object too
    if ((intptr_t)o != Mk(f == F::WonBtnState ? F::WonBtn : f) && sBadObject++ < 5) printf("call %d %u on the object %llx\n", (int)f, op, (unsigned long long)(intptr_t)o);
    Call k = {(int)f, op, {a, b, c, d, e}};
    sCpp.push_back(k);
}
// a shell op's array argument (the WON header's and list control's columns): its two values
static bool ShellArray(uint32_t op)
{
    return op == (uint32_t)EeWonShellOp::WonSetColumn || op == (uint32_t)EeWonShellOp::SetStandardColumnHeader
        || op == (uint32_t)EeWonShellOp::SetColumnHeader;
}
extern "C" {
ptrdiff_t TR_btn_call(const EeWonButton* o, EeWonBtnCall c, ptrdiff_t a, ptrdiff_t b, ptrdiff_t d) { Log(F::Btn, o, (uint32_t)c, a, b, d); return 0; }
ptrdiff_t TR_msbtn_call(const EeWonMsButton* o, EeWonMsBtnCall c, ptrdiff_t a, ptrdiff_t b, ptrdiff_t d) { Log(F::MsBtn, o, (uint32_t)c, a, b, d); return 0; }
ptrdiff_t TR_shell_op(const EeWonShell* o, EeWonShellOp c, ptrdiff_t a, ptrdiff_t b) { Log(F::Shell, o, (uint32_t)c, a, b); return 0; }
ptrdiff_t TR_multi_call(const EeWonMulti* o, EeWonMultiCall c, ptrdiff_t a, ptrdiff_t b, ptrdiff_t d) { Log(F::Multi, o, (uint32_t)c, a, b, d); return 0; }
ptrdiff_t TR_mhead_call(const EeWonMHead* o, EeWonMHeadCall c, ptrdiff_t a, ptrdiff_t b, ptrdiff_t d) { Log(F::MHead, o, (uint32_t)c, a, b, d); return 0; }
ptrdiff_t TR_bar_call(const EeWonScrollbar* o, EeWonBarCall c, ptrdiff_t a, ptrdiff_t b, ptrdiff_t d) { Log(F::Bar, o, (uint32_t)c, a, b, d); return 0; }
ptrdiff_t TR_tab_call(const EeWonTabCtrl* o, EeWonTabCall c, ptrdiff_t a, ptrdiff_t b) { Log(F::Tab, o, (uint32_t)c, a, b); return 0; }
ptrdiff_t TR_combo_call(const EeWonComboBox* o, EeWonComboCall c, ptrdiff_t a, ptrdiff_t b, ptrdiff_t d) { Log(F::Combo, o, (uint32_t)c, a, b, d); return 0; }
ptrdiff_t TR_dlg_call(const EeWonDialog* o, EeWonDlgCall c, ptrdiff_t a, ptrdiff_t b, ptrdiff_t d) { Log(F::Dlg, o, (uint32_t)c, a, b, d); return 0; }
ptrdiff_t TR_label_call(const EeWonLabel* o, EeWonLabelCall c, ptrdiff_t a, ptrdiff_t b, ptrdiff_t d) { Log(F::Label, o, (uint32_t)c, a, b, d); return 0; }
ptrdiff_t TR_list_call(const EeWonList* o, EeWonListCall c, ptrdiff_t a, ptrdiff_t b, ptrdiff_t d) { Log(F::List, o, (uint32_t)c, a, b, d); return 0; }
ptrdiff_t TR_input_call(const EeWonInput* o, EeWonInputCall c, ptrdiff_t a, ptrdiff_t b, ptrdiff_t d) { Log(F::Input, o, (uint32_t)c, a, b, d); return 0; }
ptrdiff_t TR_text_call(const EeWonText* o, EeWonTextCall c, ptrdiff_t a, ptrdiff_t b, ptrdiff_t d) { Log(F::Text, o, (uint32_t)c, a, b, d); return 0; }
ptrdiff_t TR_tree_call(const EeWonTree* o, EeWonTreeCall c, ptrdiff_t a, ptrdiff_t b, ptrdiff_t d) { Log(F::Tree, o, (uint32_t)c, a, b, d); return 0; }
ptrdiff_t TR_wonbtn_call(const EeWonWonButton* o, EeWonWbCall c, ptrdiff_t a, ptrdiff_t b, ptrdiff_t d, ptrdiff_t e) { Log(F::WonBtn, o, (uint32_t)c, a, b, d, e); return 0; }
void TR_wonbtn_set_state(const EeWonWonButton* o, int32_t s, void* i, int32_t c, void* f, bool k) { Log(F::WonBtnState, o, 0, s, (intptr_t)i, c, (intptr_t)f, k); }
ptrdiff_t TR_split_call(const EeWonSplitter* o, EeWonSplitCall c, ptrdiff_t a, ptrdiff_t b, ptrdiff_t d) { Log(F::Split, o, (uint32_t)c, a, b, d); return 0; }
ptrdiff_t TR_pane_call(const EeWonScrollPane* o, EeWonPaneCall c, ptrdiff_t a, ptrdiff_t b, ptrdiff_t d)
{
    // SetBorderPadding's argument is the forward's local array of four ints: logged by its values
    if (c == EeWonPaneCall::SetBorderPadding) { const int* p = (const int*)a; Log(F::Pane, o, (uint32_t)c, p[0], p[1], p[2], p[3]); }
    else Log(F::Pane, o, (uint32_t)c, a, b, d);
    return 0;
}
ptrdiff_t TR_scroller_call(const EeWonScroller* o, EeWonScrollerCall c, ptrdiff_t a, ptrdiff_t b, ptrdiff_t d) { Log(F::Scroller, o, (uint32_t)c, a, b, d); return 0; }
}

// ---- the configured component: each class's own storage, its Rust-object members the tracers' markers
static void Mark(Component*) {}
static void Mark(Button* t) { t->mBtn = (EeWonButton*)Mk(F::Btn); }
static void Mark(MSButtonBase* t) { t->mMsb = (EeWonMsButton*)Mk(F::MsBtn); }
static void Mark(MultiListArea* t) { t->mMulti = (EeWonMulti*)Mk(F::Multi); }
static void Mark(MultiListHeader* t) { t->mHead = (EeWonMHead*)Mk(F::MHead); }
static void Mark(Scrollbar* t) { t->mRust = (EeWonScrollbar*)Mk(F::Bar); }
static void Mark(TabCtrl* t) { t->mTab = (EeWonTabCtrl*)Mk(F::Tab); }
static void Mark(ComboBox* t) { t->mCombo = (EeWonComboBox*)Mk(F::Combo); }
static void Mark(Dialog* t) { t->mDlg = (EeWonDialog*)Mk(F::Dlg); }
static void Mark(Label* t) { t->mRust = (EeWonLabel*)Mk(F::Label); }
static void Mark(ListArea* t) { t->mRust = (EeWonList*)Mk(F::List); }
static void Mark(InputBox* t) { t->mRust = (EeWonInput*)Mk(F::Input); }
static void Mark(TextArea* t) { t->mRust = (EeWonText*)Mk(F::Text); }
static void Mark(TreeArea* t) { t->mTree = (EeWonTree*)Mk(F::Tree); }
static void Mark(WONButton* t) { t->mWb = (EeWonWonButton*)Mk(F::WonBtn); }
static void Mark(Splitter* t) { t->mRust = (EeWonSplitter*)Mk(F::Split); }
static void Mark(ScrollArea* t) { t->mPane = (EeWonScrollPane*)Mk(F::Pane); }
static void Mark(Scroller* t) { t->mRust = (EeWonScroller*)Mk(F::Scroller); }
template<class T> static T* Fake()
{
    alignas(16) static unsigned char sStore[4096];
    T* t = reinterpret_cast<T*>(sStore);
    Mark(t);
    return t;
}

// ---- the frame (WONCfgTable.inl's CfgTabFrame members the setters read)
template<class T> struct Ptr { T* p; T* get() const { return p; } };
struct CfgSetFrame {
    int i[12]; int ni; GUIString s[2]; Ptr<Font> f[1]; int nf; Ptr<Image> m[1]; Component* c[2]; int nc;
};
// the class the original statement called: the game's, or its subclass with the forwards the move deleted
template<class T> struct OrigOf { typedef T type; };
#include "won_cfgset_fwd.h"
#define EE_C(T) Fake<OrigOf<T>::type>()
// the shell kinds' own methods: WONShell::Op traced (the configured component a shell of the kind)
namespace OrigNs {
struct WONShell {
    static intptr_t Op(Component*, EeWonShellOp op, intptr_t a = 0, intptr_t b = 0)
    {
        if (ShellArray((uint32_t)op)) { const intptr_t* x = (const intptr_t*)b; Log(F::Shell, (void*)Mk(F::Shell), (uint32_t)op, a, x[0], x[1]); }
        else Log(F::Shell, (void*)Mk(F::Shell), (uint32_t)op, a, b);
        return 0;
    }
};
#include "won_cfgset_orig.h"
}
#undef EE_C

// ---- the Rust side: the host's values over the same frame, the trace hook
static CfgSetFrame* sFrame;
static intptr_t Arg(void*, EeWonCfgArg w, int32_t k)
{
    CfgSetFrame& a = *sFrame;
    switch (w) {
    case EeWonCfgArg::Int: return a.i[k];
    case EeWonCfgArg::IntCount: return a.ni;
    case EeWonCfgArg::StrPtr: return (intptr_t)&a.s[k];
    case EeWonCfgArg::Font: return (intptr_t)a.f[k].get();
    case EeWonCfgArg::FontCount: return a.nf;
    case EeWonCfgArg::Image: return (intptr_t)a.m[k].get();
    case EeWonCfgArg::Comp: return (intptr_t)a.c[k];
    case EeWonCfgArg::Component: return 0x4444;
    case EeWonCfgArg::StrUnits: return (intptr_t)a.s[k].mData->mText;
    case EeWonCfgArg::StrLen: return a.s[k].length();
    case EeWonCfgArg::CompCount: return a.nc;
    }
    return 0;
}
static bool HRead(void*, EeWonCfgRead) { return true; }
static bool HYes(void*) { return true; }
static const void* HText(void*, int32_t, uint16_t* n, uint32_t* u) { *n = 0; *u = 2; return NULL; }
static void HPush(void*, int32_t) {}
static bool HApply(void*, int32_t) { return false; }
static void HFail(void*, const uint8_t*, uintptr_t, int32_t) {}
static void HClear(void*) {}
static int32_t HPeek(void*, int32_t k) { return sFrame->i[k]; }
static std::vector<Call> sRust;
static void Hook(void*, void*, int32_t, const EeWonCfgSetTrace* t)
{
    for (int k = 0; k < t->ncalls; k++) {
        const EeWonCfgSetCall& c = t->calls[k];
        Call r = {(int)c.fun, c.op, {c.args[0], c.args[1], c.args[2], c.args[3], c.args[4]}};
        if (c.fun == F::Shell && ShellArray(c.op)) { const intptr_t* x = (const intptr_t*)c.args[1]; r.args[1] = x[0]; r.args[2] = x[1]; }
        sRust.push_back(r);
    }
}

static unsigned sSeed = 12345;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return Rand() % n; }
static int RandInt()
{
    static const int kEdge[] = {0, 1, -1, 2, 7, 10, 127, 128, 255, 256, 0x1ff, 0x100, 0x103, -128, -129, 0x7fff, 65535, 65536,
                                INT_MAX, INT_MIN, 0x12345678, -0x12345678, 3, 4, 5, 8, 0x40, 0x80};
    switch (R(3)) {
    case 0: return kEdge[R(sizeof kEdge / sizeof kEdge[0])];
    case 1: return (int)R(40) - 10;
    default: return (int)((Rand() << 17) ^ (Rand() << 2) ^ Rand());
    }
}
static std::string Show(const std::vector<Call>& v)
{
    std::string s;
    for (const Call& c : v) {
        char b[200];
        snprintf(b, sizeof b, " [%d %u %lld %lld %lld %lld %lld]", c.fun, c.op, (long long)c.args[0], (long long)c.args[1],
            (long long)c.args[2], (long long)c.args[3], (long long)c.args[4]);
        s += b;
    }
    return s;
}

int main()
{
    static const int kTables[] = {
        OrigNs::kButton, OrigNs::kMSButton, OrigNs::kMSArrow, OrigNs::kMSTabButton, OrigNs::kMS3DFrame, OrigNs::kMSCombo,
        OrigNs::kMSScrollbar, OrigNs::kMSEtched, OrigNs::kMSTabCtrl, OrigNs::kMSScrollCont, OrigNs::kMSHeader, OrigNs::kMSListCtrl,
        OrigNs::kPercent, OrigNs::kScheme, OrigNs::kRect, OrigNs::kRect3D, OrigNs::kImageComp, OrigNs::kScreen, OrigNs::kMultiList,
        OrigNs::kMultiHeader, OrigNs::kScrollbar, OrigNs::kTabCtrl, OrigNs::kBrowser, OrigNs::kCombo, OrigNs::kDialog,
        OrigNs::kImageButton, OrigNs::kLabel, OrigNs::kListArea, OrigNs::kInput, OrigNs::kTextArea, OrigNs::kTree, OrigNs::kSimple,
        OrigNs::kWONButton, OrigNs::kSplitter, OrigNs::kWScrollbar, OrigNs::kWHeader, OrigNs::kWListCtrl, OrigNs::kWFrame,
        OrigNs::kWCombo, OrigNs::kWTabCtrl, OrigNs::kWScrollCont, OrigNs::kScrollArea, OrigNs::kScroller, OrigNs::kSBScroller,
        OrigNs::kStretchCenter, OrigNs::kComponent, OrigNs::kSkin, OrigNs::kFont, OrigNs::kImage, OrigNs::kContainer,
        OrigNs::kStretchImage, OrigNs::kImageFilter};
    EeWonCfgTabHost host = {NULL, HRead, HYes, HYes, HText, HPush, HApply, HFail, HClear, HPeek, Arg};
    ee_won_cfg_set_trace(Hook, NULL);
    CfgSetFrame frame;
    sFrame = &frame;
    memset(frame.i, 0, sizeof frame.i);
    frame.ni = 1; frame.nf = 0; frame.nc = 1; frame.f[0].p = NULL; frame.m[0].p = NULL; frame.c[0] = frame.c[1] = NULL;
    int bad = 0, census = 0, moved = 0, runs = 0;
    // the census: the Rust answers a setter exactly when the original case is one of the moved ones
    std::vector<int> movedIds;
    for (int t : kTables)
        for (int n = 0; n < 64; n++) {
            const int id = t << 8 | n;
            sCpp.clear(); sRust.clear();
            const bool orig = OrigNs::OrigSet(frame, id) || OrigNs::OrigShellSet(frame, id);
            const bool rust = ee_won_cfg_set_apply(&host, id);
            if (orig != rust) { census++; printf("census: setter %d,%d orig %d rust %d\n", t, n, orig, rust); }
            if (orig) movedIds.push_back(id);
        }
    moved = (int)movedIds.size();
    static const char* const kTexts[] = {"", "a", "Hello", "x y z", "LONGER TEXT 12345"};
    for (int id : movedIds)
        for (int k = 0; k < 2000; k++) {
            for (int j = 0; j < 12; j++) frame.i[j] = RandInt();
            frame.ni = 1 + (int)R(12);
            frame.nf = (int)R(2);
            frame.f[0].p = frame.nf ? (Font*)(intptr_t)(0xf000 + R(16)) : NULL;
            frame.m[0].p = R(4) ? (Image*)(intptr_t)(0xe000 + R(16)) : NULL;
            frame.nc = 1 + (int)R(2);
            frame.c[0] = (Component*)(intptr_t)(0xc000 + R(64));
            frame.c[1] = R(3) ? (Component*)(intptr_t)(0xd000 + R(64)) : NULL;
            frame.s[0] = kTexts[R(5)];
            frame.s[1] = kTexts[R(5)];
            sCpp.clear(); sRust.clear();
            OrigNs::OrigSet(frame, id) || OrigNs::OrigShellSet(frame, id);
            ee_won_cfg_set_apply(&host, id);
            runs++;
            if (!(sCpp == sRust)) {
                if (bad++ < 30) printf("setter %d,%d: cpp%s\n    rust%s\n", id >> 8, id & 0xff, Show(sCpp).c_str(), Show(sRust).c_str());
            }
        }
    ee_won_cfg_set_trace(NULL, NULL);
    printf("won_cfgset_test: %d setters moved, %d runs, %d mismatching, %d census mismatches, %d calls on a wrong object\n",
        moved, runs, bad, census, sBadObject);
    return bad + census + sBadObject;
}
