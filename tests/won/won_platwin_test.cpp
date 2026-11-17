// PlatGUIWindow as a whole Rust object (R6 WON round 41): PlatGUIWindow.cpp's statements copied verbatim before the
// port (won_platwin_orig.h) against the port's shell over ee_won's platwin.rs (won_platwin_port.h, copied from the
// working tree), both over the same stand-in world (won_platwin_world.h: the Window base, its root and events, the
// platform manager, the cursors, the platform's pointer and minimize).
//
// 20,000 worlds from seeds: 1..4 windows (parents, every style bit, popups, open or closing, the pointer inside or
// not, an invalid root or not, with a manager or none), the manager's surface, pointer, hit window, popup and its
// flags, capture, focus and platform window; then 50 random operations each: Create at any rectangle, moves and
// resizes (with -1 sizes), GetRect and GetClientRect, show, hide, close, focus, capture, every cursor kind, the system
// pointer, minimize, top-most, timers, paints, mouse events, the surface followed, the manager's fields, the title,
// registration, and changes to the world. Then every window's fields, and the windows destroyed. The two traces must
// be identical. Exit code: the number of mismatching worlds.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <algorithm>
#include "rust/won_gui/platwin.h"
#include "rust/won_gui/platwm.h"

static unsigned sSeed;
static unsigned R(unsigned n) { sSeed = sSeed * 1103515245u + 12345u; return ((sSeed >> 8) & 0xffffff) % n; }

#define EE_PW_DECL \
    virtual bool IdleUpdate(); \
    virtual void TimerEvent(DWORD inElapsed); \
    virtual void PlatformSetTitle(); \
    virtual void PlatformSetIcon(); \
    virtual void PlatformSetCursor(Cursor* inCursor); \
    virtual void PlatformShowNativeCursor(bool inShow); \
    virtual void PlatformShow(bool inShow); \
    virtual void PlatformMinimize(bool inMinimize); \
    virtual void PlatformSetTopMost(bool inTopMost); \
    virtual void PlatformClose(); \
    virtual void Create(const WONRectangle& inRect); \
    virtual void SetPosSize(int inX, int inY, int inWidth, int inHeight); \
    virtual void GetRect(WONRectangle& outRect, bool inClientArea); \
    virtual void PlatformSetFocus(); \
    virtual void PlatformSetMouseCapture(bool inOn); \
    virtual bool PlatformIsVisible(); \
    bool FillsSurface() const { return this->GetParent() == NULL; } \
    bool HasFrame() const { return this->GetParent() != NULL && !(this->GetStyle() & 0x20); } \
    void GetClientRect(WONRectangle& outRect); \
    void GetInfo(EeWonPlatWinInfo& outInfo); \
    void FollowSurface(); \
    void OnPaint(); \
    void QueueMouse(int inType, int inSurfaceX, int inSurfaceY, int inMod, int inButtonOrAmount); \
    PlatWindowManager* GetPlatManager();

// One world from a seed: the trace of every call out and the windows' fields after each operation.
#define EE_PW_SCENARIO \
static std::string Id(PlatGUIWindow* w) { return w ? "w" + I(w->mId) : "null"; } \
static std::string Scenario(unsigned seed) \
{ \
    sTrace.clear(); sSeed = seed; \
    PlatWindowManager* mgr = new PlatWindowManager; \
    PlatCursor platCursor(70); Cursor otherCursor(71); \
    int n = 1 + (int)R(4); \
    std::vector<PlatGUIWindow*> ws; \
    static const int kStyles[] = {0x01, 0x02, 0x20, 0x40, 0x80}; \
    for (int i = 0; i < n; i++) { \
        PlatGUIWindow* w = new PlatGUIWindow; \
        w->mId = i + 1; w->mManager = R(10) ? mgr : NULL; \
        w->mParent = (i && R(3)) ? ws[R((unsigned)i)] : NULL; \
        for (int k : kStyles) if (R(3) == 0) w->mStyle |= k; \
        w->mPopup = R(4) == 0; w->mMouseIn = R(2) != 0; w->mRoot->mInvalidState = (int)R(2); \
        ws.push_back(w); \
    } \
    auto pick = [&]() -> PlatGUIWindow* { return R(5) ? ws[R((unsigned)n)] : NULL; }; \
    for (int step = 0; step < 50; step++) { \
        PlatGUIWindow* w = ws[R((unsigned)n)]; \
        WONRectangle r; \
        EeWonPlatWinInfo info; \
        tr("-- " + I(step) + " " + Id(w)); \
        switch (R(22)) { \
        case 0: w->Create(WONRectangle((int)R(300) - 20, (int)R(200) - 20, (int)R(400), (int)R(300))); break; \
        case 1: w->SetPosSize((int)R(300) - 20, (int)R(200) - 20, R(4) ? (int)R(400) : -1, R(4) ? (int)R(300) : -1); break; \
        case 2: { bool c = R(2) != 0; w->GetRect(r, c); tr("rect " + I(c) + " " + I(r.x) + "," + I(r.y) + "," + I(r.width) + "," + I(r.height)); break; } \
        case 3: w->GetClientRect(r); tr("client " + I(r.x) + "," + I(r.y) + "," + I(r.width) + "," + I(r.height)); break; \
        case 4: w->PlatformShow(R(2) != 0); break; \
        case 5: w->PlatformClose(); break; \
        case 6: w->PlatformSetFocus(); break; \
        case 7: w->PlatformSetMouseCapture(R(2) != 0); break; \
        case 8: { unsigned k = R(3); w->PlatformSetCursor(k == 0 ? (Cursor*)&platCursor : k == 1 ? &otherCursor : NULL); break; } \
        case 9: w->PlatformShowNativeCursor(R(2) != 0); break; \
        case 10: w->PlatformMinimize(R(3) != 0); break; \
        case 11: w->PlatformSetTopMost(R(2) != 0); break; \
        case 12: w->TimerEvent(R(100)); break; \
        case 13: w->OnPaint(); break; \
        case 14: w->QueueMouse((int)R(8), (int)R(900) - 50, (int)R(700) - 50, (int)R(16), (int)R(5) - 2); break; \
        case 15: w->FollowSurface(); break; \
        case 16: tr("visible " + I(w->PlatformIsVisible())); break; \
        case 17: w->PlatformSetTitle(); break; \
        case 18: SetRegistered(w, R(2) != 0); break; \
        case 19: { \
            unsigned k = R(8); \
            if (k == 0) { mgr->mSurfW = 200 + (int)R(900); mgr->mSurfH = 150 + (int)R(700); } \
            else if (k == 1) { mgr->mMouseOk = R(4) != 0; mgr->mMx = (int)R(900) - 50; mgr->mMy = (int)R(700) - 50; } \
            else if (k == 2) mgr->mHit = pick(); \
            else if (k == 3) { mgr->mPopup = pick(); mgr->mPopupFlags = R(2) ? 0x4000 : 0x1; } \
            else if (k == 4) mgr->mPlat = R(2) ? (PlatWindow*)(intptr_t)0x9000 : NULL; \
            else if (k == 5) mgr->mCapture = pick(); \
            else if (k == 6) mgr->mFocus = pick(); \
            else { mgr->mHit = w; mgr->mMouseOk = true; mgr->mMx = (int)R(500); mgr->mMy = (int)R(400); } \
            break; \
        } \
        case 20: { \
            unsigned k = R(5); \
            if (k == 0) w->mState = (int)R(3); \
            else if (k == 1) w->mMouseIn = R(2) != 0; \
            else if (k == 2) w->mStyle ^= kStyles[R(5)]; \
            else if (k == 3) w->mPopup = !w->mPopup; \
            else w->mRoot->mInvalidState = (int)R(2); \
            break; \
        } \
        default: w->GetInfo(info); \
            tr("info " + I(info.x) + "," + I(info.y) + "," + I(info.width) + "," + I(info.height) + " v" + I(info.visible) + " t" \
               + I(info.top_most) + " r" + I(info.registered) + " p" + I(info.popup) + " f" + I(info.fills) + " fr" + I(info.frame) \
               + " o" + I(info.open) + " m" + I(info.minimize_on_alt_tab) + " k" + I(info.forward_keys)); \
            break; \
        } \
    } \
    for (int i = 0; i < n; i++) { \
        EeWonPlatWinInfo info; \
        if (R(2)) SetAltTab(ws[i], R(2) != 0); \
        ws[i]->GetInfo(info); \
        tr("end " + Id(ws[i]) + " " + I(info.x) + "," + I(info.y) + "," + I(info.width) + "," + I(info.height) + " v" + I(info.visible) \
           + " t" + I(info.top_most) + " r" + I(info.registered) + " m" + I(info.minimize_on_alt_tab)); \
    } \
    while (!ws.empty()) { size_t k = R((unsigned)ws.size()); delete ws[k]; ws.erase(ws.begin() + k); } \
    delete mgr; \
    return sTrace; \
}

namespace Orig {
#include "won_platwin_world.h"
struct PlatGUIWindow : Window {
    PlatGUIWindow();
    virtual ~PlatGUIWindow();
    EE_PW_DECL
    void SetMinimizeOnAltTab(bool inMinimize) { this->mMinimizeOnAltTab = inMinimize; }
    int mX, mY, mWidth, mHeight;
    bool mVisible;
    bool mTopMost;
    bool mRegistered;
    bool mMinimizeOnAltTab;
    bool mForwardKeyMsgToParent;
};
#include "won_platwin_orig.h"
static void SetRegistered(PlatGUIWindow* w, bool b) { w->mRegistered = b; }
static void SetAltTab(PlatGUIWindow* w, bool b) { w->SetMinimizeOnAltTab(b); }
EE_PW_SCENARIO
}

namespace Port {
#include "won_platwin_world.h"
struct PlatGUIWindow : Window {
    PlatGUIWindow();
    virtual ~PlatGUIWindow();
    EE_PW_DECL
    void SetMinimizeOnAltTab(bool inMinimize) { this->Rust(EeWonPlatWinCall::SetMinimizeOnAltTab, inMinimize); }
    EeWonPlatWin* mRust;
    intptr_t Rust(EeWonPlatWinCall inCall, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0);
};
#include "won_platwin_port.h"
static void SetRegistered(PlatGUIWindow* w, bool b) { w->Rust(EeWonPlatWinCall::SetRegistered, b); }
static void SetAltTab(PlatGUIWindow* w, bool b) { w->SetMinimizeOnAltTab(b); }
EE_PW_SCENARIO
}

int main(int argc, char** argv)
{
    int worlds = argc > 1 ? atoi(argv[1]) : 20000, bad = 0;
    long long lines = 0;
    for (int s = 0; s < worlds; s++) {
        std::string a = Orig::Scenario((unsigned)s * 2654435761u + 1), b = Port::Scenario((unsigned)s * 2654435761u + 1);
        lines += std::count(a.begin(), a.end(), '\n');
        if (a != b) {
            bad++;
            if (bad < 4) {
                size_t i = 0;
                while (i < a.size() && i < b.size() && a[i] == b[i]) i++;
                size_t from = a.rfind("-- ", i);
                from = from == std::string::npos ? 0 : from;
                printf("MISMATCH world %d\n--- original (from the operation)\n%s\n--- port\n%s\n", s, a.substr(from, 900).c_str(),
                       b.substr(from, 900).c_str());
            }
        }
    }
    printf("won_platwin_test: %d worlds, %lld trace lines, %d mismatching\n", worlds, lines, bad);
    return bad;
}
