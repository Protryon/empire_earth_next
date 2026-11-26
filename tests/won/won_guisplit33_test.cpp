// The GUI hosts' cases R6 WON round 33 composed in Rust (ee_won_gui guisplit.rs: the input box's ClipboardTake, the
// text area's LinkActivated, the list's PointerOutside, TipShow, TipHide, QueueSubMenu and QueueMouseUp, the window
// manager's WmNewPopupWindow, WmDefaultWindowFont, WmResolution and the size events' Size and SetSize) against the
// deleted cases as they were (won_guisplit33_orig.h), over one stand-in world that logs every call
// (won_guisplit33_world.h). The Rust's calls out are answered by the hosts' new one-statement cases as they are
// (won_guisplit33_new.h), registered behind the game hosts (ee_won_gui_split_register) the way GuiHost.cpp and
// WindowManager.cpp register theirs; the game hosts (ee_won_gui_input_game ..) are the entry, as for the game's
// components. The LAN game reaches none of these cases (the round's reach probe), hence this golden.
//
// Each case runs for every combination of the world's answers (the window, the parent, the containment, the
// tooltip, the font, the resolution's answer, the clipboard's text) and several argument sets, on worlds made alike
// for the two sides; the traces, results and the objects' members after the call must be the same. Then each new
// op in turn is mutated (its statement skipped, its answer off by one): every mutation must make a mismatch. Exit
// code: the number of mismatches plus the mutations not caught.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <vector>
#include "rust/won_gui/guihost.h"
#include "rust/won_gui/guisplit.h"
#include "rust/won_gui/guictl.h"
#include "rust/won_gui/wmobj.h"
#include "rust/won_gui/winobj.h"
#include "won_guisplit33_world.h"
#include "won_guisplit33_orig.h"
#include "won_guisplit33_new.h"

using namespace GsWorld;
typedef EeWonGuiOp Op;

// the new cases behind the game hosts, with the mutation: an op making a pointer runs twice (its answer the
// second's), one reading a pointer answers NULL, one answering a number answers one more, any other is skipped
static bool IsPointerOp(Op op) { return op == Op::ListRoot || op == Op::WmNativeWindowNew; }
static bool IsReadOp(Op op) { return op == Op::ListParent || op == Op::WmHostFont; }
static bool IsNumberOp(Op op)
{
    return op == Op::WinMouseX || op == Op::WinMouseY || op == Op::ListContains || op == Op::TipExists ||
           op == Op::WmScreenResSet;
}
static intptr_t Mutated(intptr_t (*inHost)(void*, Op, intptr_t, intptr_t, intptr_t, intptr_t), void* inComp, Op op,
                        intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    if ((int)op != GsNew::sMutate) return inHost(inComp, op, a, b, c, d);
    if (IsPointerOp(op)) {
        inHost(inComp, op, a, b, c, d);
        return inHost(inComp, op, a, b, c, d);
    }
    if (IsReadOp(op)) return 0;
    // an input event's record: another record (its fields 0, the writes lost)
    if (op == Op::WmInputRec) { static EeWonWinEvent sOther; sOther = EeWonWinEvent(); return (intptr_t)&sOther; }
    if (IsNumberOp(op)) return inHost(inComp, op, a, b, c, d) + 1;
    return 0;
}
extern "C" intptr_t NewInput(void* p, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d) { return Mutated(GsNew::Input, p, op, a, b, c, d); }
extern "C" intptr_t NewText(void* p, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d) { return Mutated(GsNew::Text, p, op, a, b, c, d); }
extern "C" intptr_t NewList(void* p, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d) { return Mutated(GsNew::List, p, op, a, b, c, d); }
extern "C" intptr_t NewWm(void* p, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d) { return Mutated(GsNew::Wm, p, op, a, b, c, d); }

// ---- one world: the objects of a side, made alike for both
enum { K_WINDOW, K_PARENT, K_CONTAINS, K_TIP, K_FONT, K_SETOK, K_CLIP, K_COUNT };
struct Side {
    Window mWindow, mNewWindow, mParentWindow, mDefault;
    Container mParent;
    ListArea mList;
    TextArea mText;
    Component mInput;
    TextSegment mSeg;
    PopupMenu mMenu;
    ListItem mItem;
    Font mFont;
    WindowManager mWm;
    SizeEvent mSize;
};
static void Make(Side& s, unsigned k, int v)
{
    sTrace.clear();
    sObjects.clear();
    // name the objects in one order on both sides
    for (const void* p : {(const void*)&s.mWindow, (const void*)&s.mNewWindow, (const void*)&s.mParentWindow,
                          (const void*)&s.mDefault, (const void*)&s.mParent, (const void*)&s.mList, (const void*)&s.mText,
                          (const void*)&s.mInput, (const void*)&s.mSeg, (const void*)&s.mMenu, (const void*)&s.mItem,
                          (const void*)&s.mFont, (const void*)&s.mWm, (const void*)&s.mSize})
        Id(p);
    s.mWindow.mMouseX = 100 + v;
    s.mWindow.mMouseY = 200 - v;
    s.mWindow.mKeyMod = v & 7;
    s.mParent.mDx = 3 + v;
    s.mParent.mDy = 5;
    s.mList.mRoot = (k >> K_WINDOW & 1) ? &s.mWindow : NULL;
    s.mList.mParent = (k >> K_PARENT & 1) ? &s.mParent : NULL;
    s.mList.mContains = (k >> K_CONTAINS & 1) != 0;
    s.mList.mLocalDx = 7;
    s.mList.mLocalDy = -2 * v;
    if (k >> K_TIP & 1) s.mList.mItemToolTip = new ListAreaToolTip(&s.mList);
    s.mSeg.mParam = 40 + v;
    sFont = (k >> K_FONT & 1) ? &s.mFont : NULL;
    s.mWm.mDefault = &s.mDefault;
    s.mWm.mSetOk = (k >> K_SETOK & 1) != 0;
    sNewWindow = &s.mNewWindow;
    sClipboard = (k >> K_CLIP & 1) ? (v & 1 ? L"pasted text" : L"x") : L"";
    s.mSize.mx = v;
    s.mSize.my = 2 * v;
    s.mSize.mWidth = 640 + v;
    s.mSize.mHeight = 480 - v;
    sTrace.clear();
}
static void Dump(Side& s, intptr_t r, const int* ints, int n)
{
    std::string res = N(r);
    for (size_t i = 0; i < sObjects.size(); i++)
        if ((intptr_t)sObjects[i] == r && r != 0) res = Id(sObjects[i]);
    Log("result " + res);
    std::string o = "ints";
    for (int i = 0; i < n; i++) o += " " + N(ints[i]);
    Log(o);
    Log("text " + N(s.mText.mLinkParam) + " tip " + Id(s.mList.mItemToolTip.get()) + " size " + N(s.mSize.mx) + "," +
        N(s.mSize.my) + "," + N(s.mSize.mWidth) + "," + N(s.mSize.mHeight) + " refs " + N(s.mNewWindow.mRefs) + " " +
        N(s.mFont.mRefs) + " host text " + N(GsNew::sHostText.length()) + " " + N(s.mList.mItemToolTip.get() != NULL ? s.mList.mItemToolTip->mRefs : 0));
}

struct Case {
    const char* mName;
    int mHost; // 0 input, 1 text, 2 list, 3 window manager
    Op mOp;
};
static const Case kCases[] = {
    {"ClipboardTake", 0, Op::ClipboardTake},   {"LinkActivated", 1, Op::LinkActivated},
    {"PointerOutside", 2, Op::PointerOutside}, {"TipShow", 2, Op::TipShow},
    {"TipHide", 2, Op::TipHide},               {"QueueSubMenu", 2, Op::QueueSubMenu},
    {"QueueMouseUp", 2, Op::QueueMouseUp},     {"WmNewPopupWindow", 3, Op::WmNewPopupWindow},
    {"WmDefaultWindowFont", 3, Op::WmDefaultWindowFont}, {"WmResolutionGet", 3, Op::WmResolution},
    {"WmResolutionSet", 3, Op::WmResolution},  {"SizeGet", 3, Op::WmEvent},
    {"SetSize", 3, Op::WmEvent},
};

// one case on one side; the trace
static std::vector<std::string> Run(const Case& c, unsigned k, int v, bool inNew)
{
    Side s;
    Make(s, k, v);
    int ints[4] = {v, -v, 9, 9};
    EeWonRect4 q = {v + 3, v + 1, 30 + v, 40};
    intptr_t a = 0, b = 0, cc = 0, d = 0;
    void* comp = NULL;
    switch (c.mHost) {
    case 0: comp = &s.mInput; a = 0x5000 + v; break;
    case 1: comp = &s.mText; a = (intptr_t)&s.mSeg; break;
    case 2: comp = &s.mList; break;
    default: comp = &s.mWm; break;
    }
    const std::string n = c.mName;
    if (n == "TipShow") a = (v & 1) ? (intptr_t)&s.mItem : 0;
    if (n == "QueueSubMenu") { a = (intptr_t)&s.mMenu; b = (intptr_t)ints; cc = (intptr_t)&q; d = 0x23e7 + v; }
    if (n == "QueueMouseUp") { a = 11 * v; b = -v; cc = v & 3; }
    if (n == "WmNewPopupWindow") { a = (intptr_t)&s.mParentWindow; b = (intptr_t)&q; }
    if (n == "WmResolutionGet") { a = 0; b = (intptr_t)&ints[0]; cc = (intptr_t)&ints[1]; }
    if (n == "WmResolutionSet") { a = 1; b = 800 + v; cc = 600; }
    if (n == "SizeGet") { a = (intptr_t)EeWonWmEvent::Size; b = (intptr_t)&s.mSize; cc = (intptr_t)ints; ints[2] = ints[3] = 0; }
    if (n == "SetSize") { a = (intptr_t)EeWonWmEvent::SetSize; b = (intptr_t)&s.mSize; cc = (intptr_t)&q; }
    intptr_t r;
    if (!inNew) {
        switch (c.mHost) {
        case 0: r = GsOrig::Input((Component*)comp, c.mOp, a, b); break;
        case 1: r = GsOrig::Text((TextArea*)comp, c.mOp, a); break;
        case 2: r = GsOrig::List((ListArea*)comp, c.mOp, a, b, cc, d); break;
        default: r = GsOrig::Wm((WindowManager*)comp, c.mOp, a, b, cc); break;
        }
    } else {
        switch (c.mHost) {
        case 0: r = ee_won_gui_input_game(comp, c.mOp, a, b, cc, d); break;
        case 1: r = ee_won_gui_text_game(comp, c.mOp, a, b, cc, d); break;
        case 2: r = ee_won_gui_list_game(comp, c.mOp, a, b, cc, d); break;
        default: r = ee_won_gui_wm_game(comp, c.mOp, a, b, cc, d); break;
        }
    }
    Dump(s, r, ints, 4);
    std::vector<std::string> t;
    t.swap(sTrace);
    return t;
}

// all cases over all worlds: the mismatches (the first few printed when inShow)
static int RunAll(bool inShow, int* outRuns)
{
    int mismatches = 0, runs = 0;
    for (const Case& c : kCases)
        for (unsigned k = 0; k < (1u << K_COUNT); k++)
            for (int v = 0; v < 4; v++) {
                // the two queues read the list's window unchecked (the original's): only with one
                const std::string n = c.mName;
                if ((n == "QueueSubMenu" || n == "QueueMouseUp") && !(k >> K_WINDOW & 1)) continue;
                std::vector<std::string> a = Run(c, k, v, false), b = Run(c, k, v, true);
                runs++;
                if (a == b) continue;
                if (inShow && mismatches < 6) {
                    printf("MISMATCH %s world %#x v %d\n", c.mName, k, v);
                    for (size_t i = 0; i < a.size() || i < b.size(); i++)
                        printf("  %-58s | %s\n", i < a.size() ? a[i].c_str() : "", i < b.size() ? b[i].c_str() : "");
                }
                mismatches++;
            }
    if (outRuns) *outRuns = runs;
    return mismatches;
}

int main()
{
    ee_won_gui_split_register(EeWonGuiSplitHost::Input, NewInput);
    ee_won_gui_split_register(EeWonGuiSplitHost::Text, NewText);
    ee_won_gui_split_register(EeWonGuiSplitHost::List, NewList);
    ee_won_gui_split_register(EeWonGuiSplitHost::Wm, NewWm);
    int runs = 0;
    const int base = RunAll(true, &runs);
    printf("won_guisplit33_test: %d cases x worlds, %d mismatches\n", runs, base);
    // the mutations: each new op of the round (EeWonGuiOp from HostTextClipboard), and the older ops they call
    const Op kOps[] = {Op::HostTextClipboard, Op::HostTextSink, Op::HostTextClear, Op::TextSetLinkParam,
                       Op::TextDeliverLinkEvent, Op::ListRoot, Op::WinMouseX, Op::WinMouseY, Op::ListParent,
                       Op::ParentRootToChild, Op::ListContains, Op::TipExists, Op::TipNew, Op::TipDo, Op::ParamsMake,
                       Op::ParamsAvoid, Op::QueueDoPopup, Op::ListLocalToRoot, Op::WinQueueMouseUp, Op::WmWin,
                       Op::WmFontHold, Op::WmNativeWindowNew, Op::WinSetOneComponent, Op::WinSetStyle,
                       Op::WinSetParent, Op::WinCreate, Op::WmFontKeep, Op::WmHostFont, Op::WmHostFontClear,
                       Op::WmScreenResGet, Op::WmScreenResSet, Op::WmInputRec};
    int missed = 0, caught = 0;
    for (Op op : kOps) {
        GsNew::sMutate = (int)op;
        const int m = RunAll(false, NULL);
        if (m == 0) {
            printf("mutation of op %d not caught\n", (int)op);
            missed++;
        } else {
            caught++;
        }
    }
    GsNew::sMutate = -1;
    printf("mutations: %d of %d caught\n", caught, caught + missed);
    return base + missed;
}
