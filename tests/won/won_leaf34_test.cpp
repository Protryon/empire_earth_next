// R6 WON round 34's whole-class moves of WONGUI's leaf classes, against the deleted C++ as it was at ab1b33bcb
// (won_leaf34_orig.h): BrowserComponent (ee_won_gui browserobj.rs: the constructor's view, SetScroller,
// AddedToParent's default scroller, SetTransparent, SetFontSize, SetPageText, Reset, Go, ShowFiles, Refresh and the
// page reads), ListAreaToolTip (tipobj.rs: Paint, DoTip, EndTip) and StringLocalize::GetStr (localize.rs, through the
// lobby's LobbyFormatString, 2 and 3). The new side is the classes and their hosts' one-statement cases as they are
// in the tree (won_leaf34_new.h, written by won_leaf34_gen.py), over the same stand-in world (won_leaf34_world.h)
// that logs every call; the texts are real GUIStrings and the page files real files read through PlatFOpen.
//
// Go, ShowFiles, Refresh and the page reads are not reached by a LAN game (R6 WON round 33), nor is the tooltip
// (the harness moves no pointer over a cut-off list item): hence this golden. Each browser and tip sequence makes
// random calls on worlds made alike for the two sides; after every call the traces, the objects' members and the
// reference counts must be the same. The formatting cases compare the texts. Then each host op in turn is mutated
// (an effect skipped, an existence answered false, a number answered one more, a maker or a reference run twice) and the
// formatting host's reads and maker: every mutation must make a mismatch. Exit code: the mismatches plus the
// mutations not caught.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <filesystem>
#include <list>
#include <map>
#include <set>
#include <ctype.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONURL.h"
#include "platform/PlatFile.h"
#include "rust/won_gui/browserobj.h"
#include "rust/won_gui/tipobj.h"
#include "rust/won_gui/localize.h"
#include "rust/won_gui/guidraw.h"
#include "rust/won_net/ffi.h"
wchar_t gEmptyWideString[2]; // EEWonNet.cpp's (GetEmptyGUIStringData)
#include "won_leaf34_world.h"
#include "won_leaf34_orig.h"
#include "won_leaf34_new.h"

using namespace Lf;

// ---- the mutations: an op of the hosts (browser ops from 1, tip ops from 100, the formatting host 200 and 201)
static int sMutate = -1;
namespace LfNew {
static bool IsBrowserRead(EeWonBrowserOp o) { return o == Bo::ScrollerHasView || o == Bo::HasScroller; }
static intptr_t MutBrowser(void* me, EeWonBrowserOp op, intptr_t a, intptr_t b)
{
    if ((int)op != sMutate) return HostBrowser(me, op, a, b);
    if (op == Bo::NewScrollContainer || op == Bo::NewView || op == Bo::Ref) { // (a skipped Ref frees a held scroller)
        HostBrowser(me, op, a, b);
        return HostBrowser(me, op, a, b);
    }
    if (IsBrowserRead(op)) return HostBrowser(me, op, a, b) ? 0 : 1;
    return 0;
}
static bool IsTipExists(EeWonTipOp o) { return o == To::ItemIs || o == To::HasItem || o == To::HasParent || o == To::HasOwnScheme; }
static bool IsTipNumber(EeWonTipOp o) { return (o >= To::ItemTop && o <= To::ItemHeight) || (o >= To::ListLeftPad && o <= To::Height); }
static intptr_t MutTip(void* me, EeWonTipOp op, intptr_t a, intptr_t b)
{
    if ((int)op + 100 != sMutate) return HostTip(me, op, a, b);
    if (IsTipExists(op)) return 0;
    if (IsTipNumber(op)) return HostTip(me, op, a, b) + 1;
    if (op == To::Scheme || op == To::ListScheme) return 0;
    if (op == To::ListLocalToRoot) {
        HostTip(me, op, a, b);
        ((int*)a)[0]++;
        return 0;
    }
    return 0;
}
static void MutUnits(const void* inText, EeWonLocalizeText* outText)
{
    LocalizeUnits(inText, outText);
    if (sMutate == 200 && outText->length > 0) outText->length--;
}
static void* MutNew(const void* inUnits, size_t inLength)
{
    return LocalizeNew(inUnits, sMutate == 201 && inLength > 0 ? inLength - 1 : inLength);
}
} // namespace LfNew

// ---- random numbers (one generator, reseeded per sequence so the two sides draw alike)
static uint32_t sSeed = 1;
static uint32_t Rand()
{
    sSeed = sSeed * 1103515245u + 12345u;
    return (sSeed >> 8) & 0xffffff;
}
static int Pick(int n) { return (int)(Rand() % (uint32_t)n); }

// ---- the page files
static std::string sDir;
static std::vector<std::string> sLocations; // narrow (UTF-8) locations
static std::vector<std::wstring> sWideLocations;
static std::vector<std::string> sPaths; // ShowFiles' paths
static void WriteFile(const std::string& inName, const std::string& inBytes)
{
    FILE* f = fopen((sDir + inName).c_str(), "wb");
    fwrite(inBytes.data(), 1, inBytes.size(), f);
    fclose(f);
}
static void MakePages()
{
    std::error_code e;
    std::filesystem::path p = std::filesystem::temp_directory_path(e) / ("ee_leaf34_" + std::to_string((unsigned)GetCurrentProcessId()));
    std::filesystem::create_directories(p, e);
    sDir = p.string();
    if (!sDir.empty() && sDir.back() != '\\' && sDir.back() != '/') sDir += "\\";
    WriteFile("motd.txt", "1Welcome to the lobby\r\nSecond line\r\n");
    WriteFile("utf8.txt", "\xc3\xa9t\xc3\xa9 \xe4\xb8\xad\xe6\x96\x87 \xf0\x9f\x98\x80 caf\xc3\xa9");
    WriteFile("cp1252.txt", "na\xefve \x93quotes\x94 \x80 end");
    WriteFile("page.html", "<html><head><title>T</title></head><body><p>Hello &amp; welcome</p><br>Line&nbsp;two &lt;x&gt;</body></html>");
    WriteFile("empty.txt", "");
    WriteFile("big.txt", std::string(9000, 'x') + "\nend");
    const char* kNames[] = {"motd.txt", "utf8.txt", "cp1252.txt", "page.html", "empty.txt", "big.txt", "missing.txt"};
    for (const char* n : kNames) sPaths.push_back(sDir + n);
    sPaths.push_back("");
    for (const char* n : kNames) sLocations.push_back(sDir + n);
    sLocations.push_back("");
    sLocations.push_back("http://www.won.net/news.html");
    sLocations.push_back("HTTP://Upper.case/path?x=1");
    sLocations.push_back("ftp://ftp.example/pub");
    sLocations.push_back("gopher://hole/x");
    sLocations.push_back("file://" + sDir + "motd.txt");
    sLocations.push_back("file:///" + sDir + "page.html");
    sLocations.push_back("relative/missing/page.html");
    sLocations.push_back(sDir + "motd.txt" + std::string(1, '\0') + "tail");
    for (const std::string& s : sLocations) sWideLocations.push_back(std::wstring(GUIString(s)));
    sWideLocations.push_back(L"C:\\caf\u00e9\\\u4e2d\u6587.html");
    sWideLocations.push_back(std::wstring(L"http://x/") + wchar_t(0xd800) + L"lone");
}

// ---- browser sequences
template <class B> static intptr_t FontOf(B& b);
template <> intptr_t FontOf(LfOrig::Browser& b) { return b.mFontSize; }
template <> intptr_t FontOf(LfNew::BrowserComponent& b) { return ee_won_browser_call(b.mRust, EeWonBrowserCall::FontSize, 0, 0, 0, 0, 0); }
template <class B> static intptr_t BgOf(B& b);
template <> intptr_t BgOf(LfOrig::Browser& b) { return b.mHaveBackground; }
template <> intptr_t BgOf(LfNew::BrowserComponent& b) { return ee_won_browser_call(b.mRust, EeWonBrowserCall::HaveBackground, 0, 0, 0, 0, 0); }

template <class B> static void DumpBrowser(B& b, const std::vector<SmartPtr<ScrollbarScroller>>& outside)
{
    std::string s = "state font " + N(FontOf(b)) + " bg " + N(BgOf(b)) + " view " + Id(b.GetHTMLView()) + " scroller " +
                    Id(b.mScroller.get()) + " status " + Text(b.GetCurStatus()) + " children";
    for (const auto& c : b.mChildren) s += " " + Id(c.get()) + "/" + N(c->mRefCount);
    if (b.mScroller.get() != NULL) s += " scroller refs " + N(b.mScroller->mRefCount) + " area " + Id(b.mScroller->GetScrollArea());
    s += " view refs " + N(b.GetHTMLView()->mRefCount);
    for (const auto& o : outside) s += " outside " + Id(o.get()) + "/" + N(o->mRefCount);
    Log(s);
}

template <class B> static std::vector<std::string> BrowserRun(uint32_t inSeed, int inCalls)
{
    sTrace.clear();
    sObjects.clear();
    sSeed = inSeed;
    std::vector<SmartPtr<ScrollbarScroller>> outside;
    {
        SmartPtr<B> b = new B;
        DumpBrowser(*b.get(), outside);
        for (int i = 0; i < inCalls; i++) {
            const int k = Pick(9);
            switch (k) {
            case 0: Log("call AddedToParent"); b->AddedToParent(); break;
            case 1: {
                SmartPtr<ScrollbarScroller> s = new ScrollbarScroller;
                if (Pick(2)) s->mArea = b->GetHTMLView();
                outside.push_back(s);
                Log("call SetScroller " + Id(s.get()));
                b->SetScroller(s.get());
                break;
            }
            case 2: { const bool t = Pick(2) != 0; Log("call SetTransparent " + N(t)); b->SetTransparent(t); break; }
            case 3: { const int f = Pick(5); Log("call SetFontSize " + N(f)); b->SetFontSize((HTMLFontSize)f); break; }
            case 4: Log("call Reset"); b->Reset(); break;
            case 5: Log("call Refresh"); b->Refresh(); break;
            case 6: case 7: {
                const std::wstring& w = sWideLocations[Pick((int)sWideLocations.size())];
                Log("call Go");
                b->Go(GUIString(w), Pick(2) != 0, Pick(2) != 0, Pick(3) - 1);
                break;
            }
            default: {
                const std::string& f = sPaths[Pick((int)sPaths.size())];
                const std::string& s = sPaths[Pick((int)sPaths.size())];
                const bool skip = Pick(2) != 0;
                Log("call ShowFiles " + N(f.size()) + " " + N(s.size()) + " " + N(skip));
                b->ShowFiles(f, s, skip);
                break;
            }
            }
            DumpBrowser(*b.get(), outside);
        }
        Log("destroy");
    }
    for (const auto& o : outside) Log("after " + Id(o.get()) + "/" + N(o->mRefCount));
    std::vector<std::string> t;
    t.swap(sTrace);
    return t;
}

// ---- tip sequences
struct TipWorld {
    Window mWindow;
    ListArea mList;
    ColorScheme mSchemes[3] = {{1}, {2}, {3}};
    Graphics mGraphics;
    std::vector<SmartPtr<ListItem>> mItems;
    TipWorld()
    {
        mList.AddRef(); // held by the world
        mList.mRoot = &mWindow;
        mList.mLeftPad = Pick(6);
        mList.mRightPad = Pick(6);
        mList.mTopPad = Pick(6);
        mList.mVertOffset = Pick(40);
        mList.mWidth = 50 + Pick(200);
        mList.mRootX = Pick(300);
        mList.mRootY = Pick(300);
        mList.mScheme = Pick(4) ? &mSchemes[0] : NULL;
        for (int i = 0; i < 3; i++) {
            SmartPtr<ListItem> it = new ListItem;
            it->mTop = Pick(200) - 20;
            it->mWidth = Pick(300);
            it->mHeight = 8 + Pick(20);
            mItems.push_back(it);
        }
        mGraphics.mColorScheme = Pick(2) ? &mSchemes[1] : NULL;
    }
};
template <class T> static void DumpTip(T& t, TipWorld& w)
{
    std::string s = "state parent " + Id(t.mParent) + " item " + Id(t.mItem.get()) + " size " + N(t.Width()) + " " +
                    N(t.Height()) + " g scheme " + Id(w.mGraphics.mColorScheme) + " items";
    for (const auto& i : w.mItems) s += " " + N(i->mRefCount);
    Log(s);
}
template <class T> static std::vector<std::string> TipRun(uint32_t inSeed, int inCalls)
{
    sTrace.clear();
    sObjects.clear();
    sSeed = inSeed;
    {
        TipWorld w;
        Id(&w.mWindow);
        Id(&w.mList);
        SmartPtr<T> t = new T(&w.mList);
        if (Pick(3) == 0) t->mOwnScheme = &w.mSchemes[2];
        DumpTip(*t.get(), w);
        for (int i = 0; i < inCalls; i++) {
            const int k = Pick(8);
            if (k <= 2) {
                Log("call Paint");
                t->Paint(w.mGraphics);
            } else if (k <= 5) {
                const int n = Pick(4);
                ListItem* it = n < 3 ? w.mItems[n].get() : NULL;
                Log("call DoTip " + Id(it));
                t->DoTip(it);
            } else if (k == 6) {
                Log("call EndTip");
                t->EndTip();
            } else {
                // the world changes: the popup closed from outside, the tip's scheme, the list's pads
                const int c = Pick(3);
                if (c == 0) t->mParent = NULL;
                if (c == 1) t->mOwnScheme = t->mOwnScheme ? NULL : &w.mSchemes[2];
                if (c == 2) w.mList.mLeftPad = Pick(9);
                Log("world " + N(c));
            }
            DumpTip(*t.get(), w);
        }
        t->mItem = NULL;
        DumpTip(*t.get(), w);
    }
    std::vector<std::string> r;
    r.swap(sTrace);
    return r;
}

// ---- formatting cases
static GUIString RandomText(int inMax)
{
    static const wchar_t kUnits[] = {L'<', L'<', L'<', L'>', L'>', L's', L's', L'S', L'1', L'2', L'3', L'4', L'0', L'9',
                                     L'a', L' ', 0xe9, 0x4e2d, L'x', L'<'};
    std::wstring w;
    const int n = Pick(inMax + 1);
    for (int i = 0; i < n; i++) w += kUnits[Pick((int)(sizeof(kUnits) / sizeof(kUnits[0])))];
    return GUIString(w);
}
static std::string FormatOrig(int k, const GUIString& f, const GUIString& a, const GUIString& b, const GUIString& c)
{
    const GUIString* subs[3] = {&a, &b, &c};
    const GUIString* withNull[3] = {&a, NULL, &c};
    return Text(LfOrig::GetStr(f, k == 3 ? withNull : subs, k == 3 ? 3 : k + 1));
}
static std::string FormatNew(int k, const GUIString& f, const GUIString& a, const GUIString& b, const GUIString& c)
{
    switch (k) {
    case 0: return Text(LfNew::LobbyFormatString(f, a));
    case 1: return Text(LfNew::LobbyFormatString2(f, a, b));
    case 2: return Text(LfNew::LobbyFormatString3(f, a, b, c));
    default: return Text(LfNew::WONLocalize(f, &a, NULL, &c, 3));
    }
}

// ---- the runs
static const int kBrowserRuns = 4000, kTipRuns = 6000, kFormatRuns = 60000;
static int RunAll(bool inShow, long long* outSteps)
{
    int mismatches = 0;
    long long steps = 0;
    auto report = [&](const char* inWhat, uint32_t inSeed, const std::vector<std::string>& a, const std::vector<std::string>& b) {
        if (a == b) return;
        if (inShow && mismatches < 4) {
            printf("MISMATCH %s seed %u\n", inWhat, inSeed);
            for (size_t i = 0; i < a.size() || i < b.size(); i++)
                if (i >= a.size() || i >= b.size() || a[i] != b[i])
                    printf("  %zu: %s\n   | %s\n", i, i < a.size() ? a[i].c_str() : "", i < b.size() ? b[i].c_str() : "");
        }
        mismatches++;
    };
    for (int i = 0; i < kBrowserRuns; i++) {
        const uint32_t seed = 1000u + (uint32_t)i;
        const int calls = 4 + i % 9;
        report("browser", seed, BrowserRun<LfOrig::Browser>(seed, calls), BrowserRun<LfNew::BrowserComponent>(seed, calls));
        steps += calls;
    }
    for (int i = 0; i < kTipRuns; i++) {
        const uint32_t seed = 900000u + (uint32_t)i;
        const int calls = 3 + i % 12;
        report("tip", seed, TipRun<LfOrig::Tip>(seed, calls), TipRun<LfNew::ListAreaToolTip>(seed, calls));
        steps += calls;
    }
    for (int i = 0; i < kFormatRuns; i++) {
        sSeed = 77u + (uint32_t)i;
        const GUIString f = RandomText(14), a = RandomText(4), b = RandomText(4), c = RandomText(4);
        const int k = i % 4;
        const std::vector<std::string> x = {FormatOrig(k, f, a, b, c)}, y = {FormatNew(k, f, a, b, c)};
        report("format", (uint32_t)i, x, y);
        steps++;
    }
    if (outSteps) *outSteps = steps;
    return mismatches;
}

int main()
{
    setvbuf(stdout, NULL, _IONBF, 0);
    MakePages();
    long long steps = 0;
    const int base = RunAll(true, &steps);
    printf("won_leaf34_test: %d browser, %d tip and %d formatting cases (%lld calls), %d mismatches\n", kBrowserRuns,
           kTipRuns, kFormatRuns, steps, base);
    std::vector<int> ops;
    for (int o = (int)EeWonBrowserOp::NewView; o <= (int)EeWonBrowserOp::Invalidate; o++) ops.push_back(o);
    for (int o = (int)EeWonTipOp::BasePaint; o <= (int)EeWonTipOp::ClosePopup; o++) ops.push_back(o + 100);
    ops.push_back(200);
    ops.push_back(201);
    int missed = 0, caught = 0;
    for (int op : ops) {
        sMutate = op;
        const int m = RunAll(false, NULL);
        if (m == 0) {
            printf("mutation of op %d not caught\n", op);
            missed++;
        } else {
            caught++;
        }
    }
    sMutate = -1;
    printf("mutations: %d of %d caught\n", caught, caught + missed);
    std::error_code e;
    std::filesystem::remove_all(sDir, e);
    return base + missed;
}
