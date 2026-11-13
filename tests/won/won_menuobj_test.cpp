// The popup menu as a whole object (R6 WON round 24): the original PopupMenu's methods (MenuBehavior.cpp,
// MenuItemLifetime.cpp's Reset), the statements copied from the C++ before the port with every call on a C++ object
// (the ListArea base, the submenus, the items' fields, the window's popups and queued events, the events) replaced
// by the op that makes it (GuiHost.cpp runs the same calls in the same order), against ee_won's Rust PopupMenu
// (menuobj.rs) over one scripted host. Both sides' ListArea base is the Rust list (listobj.rs), as in the game: its
// selection hook calls the side's ListSelChangedHook back. The submenus are the host's (a selection, Reset, keys
// they take or decline, references).
//
// Each of 20,000 sequences creates the menu and makes 40 random inward calls (items with and without submenus,
// disabled ones; presses, releases, moves on the same spot and elsewhere, the submenu timer, events from the submenu
// and others, submenu checks and closes, keys, resets, selections). The traces must be the same, and every
// reference taken must be given back when the menu goes. The first 40 sequences and every later one that makes an
// op or a call none before made are rust/crates/ee_won/src/golden_menuobj.txt, which the Rust tests replay;
// `--print` prints them.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include "rust/ee_won.h"

typedef EeWonGuiOp Op;
typedef EeWonGuiField Fd;
typedef EeWonListCall LC;
typedef EeWonMenuCall MC;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }

static const intptr_t kMenu = 0x55;

struct HostItem {
    int top, height, width;
    bool selected, selectable, menuItem, disabled;
    intptr_t sub;
    int refs;
};
struct HostSub { intptr_t sel; int refs; };
struct HostEvent { intptr_t comp; int type; };
static std::map<intptr_t, HostItem> sItems;
static std::map<intptr_t, HostSub> sSubs;
static std::map<intptr_t, HostEvent> sEvents;
static intptr_t sNextItem, sNextEvent;
static int sFields[16];
static std::vector<std::string> sTrace;
static std::set<int> sOpsMet;

static void Line(const char* f, long long a, long long b, long long c, long long d, long long e, long long g = 0)
{
    char aLine[300];
    snprintf(aLine, sizeof(aLine), f, a, b, c, d, e, g);
    sTrace.push_back(aLine);
}
static void TraceFields()
{
    std::string s = "fields";
    char b[24];
    for (int i = 0; i < 12; i++) { snprintf(b, sizeof(b), " %d", sFields[i]); s += b; }
    sTrace.push_back(s);
}

struct Side {
    virtual ~Side() {}
    virtual void Hook() = 0; // PopupMenu::ListSelChangedHook
    virtual intptr_t Menu(MC c, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0) = 0;
};
static Side* sSide;
static EeWonList* sList;

static intptr_t ScriptedHost(void* inComp, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)inComp;
    sOpsMet.insert((int)inOp);
    intptr_t r = 0;
    int n = (int)inOp;
    switch (inOp) {
    case Op::Get: return sFields[a]; // pure reads: not traced
    case Op::Set: sFields[a] = (int)b; break;
    case Op::BaseSizeChanged:
        Line("%lld base-size", n, 0, 0, 0, 0);
        sFields[(int)Fd::ScrollWidth] = 20 + (int)R(280);
        sFields[(int)Fd::ScrollHeight] = 10 + (int)R(200);
        sFields[(int)Fd::Width] = sFields[(int)Fd::ScrollWidth] + 4;
        sFields[(int)Fd::Height] = sFields[(int)Fd::ScrollHeight] + 4;
        TraceFields();
        ee_won_list_call(sList, LC::AdjustPosition, 0, 0, 0);
        return 0;
    case Op::BaseAdjustPosition: {
        int aMaxV = sFields[(int)Fd::TotalHeight] + sFields[(int)Fd::ExtraHeight] - sFields[(int)Fd::ScrollHeight];
        int aMaxH = sFields[(int)Fd::TotalWidth] - sFields[(int)Fd::ScrollWidth];
        int& v = sFields[(int)Fd::VertOffset];
        int& h = sFields[(int)Fd::HorzOffset];
        if (v > aMaxV) v = aMaxV;
        if (v < 0) v = 0;
        if (h > aMaxH) h = aMaxH;
        if (h < 0) h = 0;
        Line("%lld base-adjust %lld -> %lld %lld", n, (long long)a, v, h, 0);
        return 0;
    }
    case Op::BaseKeyDown: r = R(8) == 0; break;
    case Op::ScrollTranslate: case Op::ScrollUntranslate: case Op::LocalToRoot: {
        int* x = (int*)a; int* y = (int*)b;
        long long ix = *x, iy = *y;
        int dx = inOp == Op::LocalToRoot ? 100 : 2 - sFields[(int)Fd::HorzOffset];
        int dy = inOp == Op::LocalToRoot ? 50 : 2 - sFields[(int)Fd::VertOffset];
        if (inOp == Op::ScrollTranslate) { dx = -dx; dy = -dy; }
        *x += dx;
        *y += dy;
        Line("%lld translate %lld %lld -> %lld %lld", n, ix, iy, *x, *y);
        return 0;
    }
    case Op::VirtAdjustPosition: Line("%lld virt-adjust %lld", n, (long long)a, 0, 0, 0); ee_won_list_call(sList, LC::AdjustPosition, a, 0, 0); return 0;
    case Op::VirtSort: { Line("%lld virt-sort", n, 0, 0, 0, 0); intptr_t s = ee_won_list_call(sList, LC::Sort, 0, 0, 0); Line("%lld virt-sort -> %lld", n, s, 0, 0, 0); return s; }
    case Op::VirtNotifyMouseItemGone: Line("%lld virt-gone", n, 0, 0, 0, 0); ee_won_list_call(sList, LC::NotifyMouseItemGone, 0, 0, 0); return 0;
    case Op::VirtListSelChangedHook: Line("%lld hook", n, 0, 0, 0, 0); sSide->Hook(); return 0;
    case Op::SetVertOffset: sFields[(int)Fd::VertOffset] = (int)a; break;
    case Op::OnBottom: r = R(2); break;
    case Op::RootKeyMod: r = R(8); break;
    case Op::GetViewport: r = 0x77; break;
    case Op::PointerOutside: r = R(2); break;
    case Op::DeliverItemClicked: {
        const EeWonItemClick* k = (const EeWonItemClick*)b;
        Line("%lld click %llx %lld %lld %lld %lld", n, (long long)a, k->button, k->up, k->x, k->y);
        return 0;
    }
    case Op::Draw: {
        const EeWonDrawOp* o = (const EeWonDrawOp*)b;
        char aLine[200];
        snprintf(aLine, sizeof(aLine), "%d draw %llx %d %d %d %d %d", n, (long long)a, o->kind, o->a, o->b, o->c, o->d);
        sTrace.push_back(aLine);
        return 0;
    }
    case Op::QueueSubMenu: {
        const int* at = (const int*)b;
        const EeWonRect4* q = (const EeWonRect4*)c;
        Line("%lld queue-sub %llx %lld %lld %lld", n, (long long)a, at[0], at[1], (long long)d, 0);
        Line("%lld avoid %lld %lld %lld %lld", n, q->x, q->y, q->w, q->h);
        return 0;
    }
    case Op::ItemTop: r = sItems[a].top; break;
    case Op::ItemSetTop: sItems[a].top = (int)b; break;
    case Op::ItemHeight: r = sItems[a].height; break;
    case Op::ItemSetHeight: sItems[a].height = (int)b; break;
    case Op::ItemWidth: r = sItems[a].width; break;
    case Op::ItemSelected: r = sItems[a].selected; break;
    case Op::ItemSetSelected: sItems[a].selected = b != 0; break;
    case Op::ItemSelectable: r = sItems[a].selectable; break;
    case Op::ItemSetPos: sItems[a].top = (int)b; break;
    case Op::ItemContains: r = c >= sItems[a].top && c < sItems[a].top + sItems[a].height; break;
    case Op::ItemCanSelect: r = sItems[a].selectable; break;
    case Op::ItemCalcDimensions: sItems[a].height = 16; sItems[a].width = 80; break;
    case Op::ItemHold: sItems[a].refs++; break;
    case Op::ItemRelease: sItems[a].refs--; break;
    case Op::MenuItemOf: r = a != 0 && sItems[a].menuItem ? a : 0; break;
    case Op::MenuItemSub: r = sItems[a].sub; break;
    case Op::MenuItemDisabled: r = sItems[a].disabled; break;
    case Op::ChildHold: sSubs[a].refs++; break;
    case Op::ChildRelease: sSubs[a].refs--; break;
    case Op::MenuSelItem: r = sSubs[a].sel; break;
    case Op::MenuSetSelAt: sSubs[a].sel = b >= 0 ? 0x3800 + b : 0; break;
    case Op::MenuReset: sSubs[a].sel = 0; break;
    case Op::MenuTryKeyDown: r = R(3) == 0; break;
    case Op::EventComponent: r = sEvents[a].comp; break;
    case Op::EventType: r = sEvents[a].type; break;
    default: break;
    }
    Line("%lld %llx %llx %llx %llx -> %llx", n, (long long)a, (long long)b, (long long)c, (long long)d, (long long)r);
    return r;
}

static intptr_t HM(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0)
{
    return ScriptedHost((void*)kMenu, inOp, a, b, c, d);
}
static intptr_t Base(LC c, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0) { return ee_won_list_call(sList, c, a, b, cc); }

// ---- the original (MenuBehavior.cpp's PopupMenu, Reset; the ListArea base the Rust list)

namespace orig {
typedef intptr_t Item;
struct PopupMenu {
    Item mCurSubMenu, mCurSubMenuItem;
    int mMouseUpCount, mTimerAcc, mLastX, mLastY;
    bool mKeyboardMode;
    PopupMenu() : mCurSubMenu(0), mCurSubMenuItem(0), mLastX(0), mLastY(0)
    {
        this->mMouseUpCount = 0;
        this->mTimerAcc = 0;
        this->mKeyboardMode = false;
    }
    ~PopupMenu()
    {
        if (this->mCurSubMenuItem) HM(Op::ItemRelease, this->mCurSubMenuItem);
        if (this->mCurSubMenu) HM(Op::ChildRelease, this->mCurSubMenu);
    }
    void SetSub(Item s)
    {
        if (this->mCurSubMenu != s) {
            if (this->mCurSubMenu) HM(Op::ChildRelease, this->mCurSubMenu);
            this->mCurSubMenu = s;
            if (s) HM(Op::ChildHold, s);
        }
    }
    void SetSubItem(Item i)
    {
        if (this->mCurSubMenuItem != i) {
            if (this->mCurSubMenuItem) HM(Op::ItemRelease, this->mCurSubMenuItem);
            this->mCurSubMenuItem = i;
            if (i) HM(Op::ItemHold, i);
        }
    }
    void EndSubMenu()
    {
        if (this->mCurSubMenu != 0) {
            HM(Op::ClosePopup, this->mCurSubMenu);
            if (!this->mKeyboardMode) Base(LC::SetListFlags, 4, 1);
            this->SetSub(0);
            this->SetSubItem(0);
        }
    }
    void HandleComponentEvent(intptr_t event)
    {
        intptr_t comp = HM(Op::EventComponent, event);
        if (ee_won_gui_menu_popup_event(comp == this->mCurSubMenu, (int)HM(Op::EventType, event))) {
            if (HM(Op::MenuSelItem, this->mCurSubMenu) != 0)
                Base(LC::SetSelItemAt, Base(LC::GetItemPos, this->mCurSubMenuItem), 1);
        } else HM(Op::BaseHandleEvent, event);
    }
    bool TimerEvent(int delta)
    {
        bool aCheck;
        bool aKeep = ee_won_gui_menu_timer(this->mKeyboardMode, &this->mTimerAcc, delta, &aCheck);
        if (aCheck) this->CheckSubMenu();
        return aKeep;
    }
    void ListSelChangedHook()
    {
        this->mTimerAcc = 0;
        if (!this->mKeyboardMode) {
            if (ee_won_gui_menu_sel_clears_sub(this->mKeyboardMode, this->mCurSubMenu != 0, Base(LC::SelItem) == this->mCurSubMenuItem))
                HM(Op::MenuSetSelAt, this->mCurSubMenu, -1);
            HM(Op::RequestTimer, 1);
        }
    }
    void MouseMove(int x, int y)
    {
        int aMove = ee_won_gui_menu_mouse_move(this->mKeyboardMode, x, y, this->mLastX, this->mLastY);
        if (aMove < 0) return;
        if (aMove > 0) {
            this->mKeyboardMode = false;
            Base(LC::SetListFlags, 4, this->mCurSubMenu == 0);
        }
        this->mLastX = x;
        this->mLastY = y;
        Base(LC::MouseMove, x, y);
    }
    void MouseUp(int x, int y, int button)
    {
        if (!ee_won_gui_menu_mouse_up(button, &this->mMouseUpCount)) return;
        Base(LC::MouseUp, x, y, button);
    }
    void CheckSubMenu()
    {
        this->mTimerAcc = 0;
        Item item = HM(Op::MenuItemOf, Base(LC::SelItem));
        Item submenu = item != 0 ? HM(Op::MenuItemSub, item) : 0;
        bool disabled = item != 0 && HM(Op::MenuItemDisabled, item) != 0;
        uint8_t aStep = ee_won_gui_menu_check_sub(item != 0, this->mCurSubMenu != 0, item != 0 && this->mCurSubMenu == submenu,
                                                  submenu != 0, disabled);
        if (aStep == 0) return;
        this->EndSubMenu();
        if (aStep == 2) {
            Base(LC::SetListFlags, 4, 0);
            this->SetSub(submenu);
            this->SetSubItem(item);
            int y;
            int x = ee_won_gui_submenu_place((int)HM(Op::Get, (intptr_t)Fd::ScrollWidth), (int)HM(Op::ItemTop, item), &y);
            HM(Op::ScrollUntranslate, (intptr_t)&x, (intptr_t)&y);
            HM(Op::LocalToRoot, (intptr_t)&x, (intptr_t)&y);
            HM(Op::MenuReset, submenu);
            EeWonRect4 anAvoid = ee_won_gui_submenu_avoid(x, (int)HM(Op::Get, (intptr_t)Fd::ScrollWidth));
            int at[2] = {x, y};
            HM(Op::QueueSubMenu, submenu, (intptr_t)at, (intptr_t)&anAvoid, ee_won_gui_menu_submenu_flags());
        }
    }
    void MouseDown(int x, int y, int button)
    {
        if (button == 0) {
            this->mMouseUpCount = 0;
            HM(Op::QueueMouseUp, x, y, button);
            return;
        }
        Base(LC::MouseDown, x, y, button);
    }
    bool TryKeyDown(int theKey)
    {
        this->mKeyboardMode = true;
        bool hasSub = this->mCurSubMenu != 0;
        if (hasSub && HM(Op::MenuTryKeyDown, this->mCurSubMenu, theKey)) return true;
        switch (ee_won_gui_menu_popup_key(hasSub, theKey, hasSub && HM(Op::MenuSelItem, this->mCurSubMenu) != 0)) {
        case 1: this->EndSubMenu(); return true;
        case 2: HM(Op::MenuSetSelAt, this->mCurSubMenu, 0); return true;
        case 3:
            this->CheckSubMenu();
            if (this->mCurSubMenu != 0) { HM(Op::MenuSetSelAt, this->mCurSubMenu, 0); return true; }
            return false;
        case 4: Base(LC::KeyDown, theKey); return true;
        default: return false;
        }
    }
    void Reset()
    {
        this->SetSub(0);
        Base(LC::SetSelItemAt, -1, 1);
    }
};
} // namespace orig

struct OrigSide : Side {
    orig::PopupMenu* mMenu;
    OrigSide() : mMenu(new orig::PopupMenu) {}
    void Hook() override { this->mMenu->ListSelChangedHook(); }
    intptr_t Menu(MC c, intptr_t a, intptr_t b, intptr_t cc) override
    {
        orig::PopupMenu* m = this->mMenu;
        switch (c) {
        case MC::MouseDown: m->MouseDown((int)a, (int)b, (int)cc); return 0;
        case MC::MouseUp: m->MouseUp((int)a, (int)b, (int)cc); return 0;
        case MC::MouseMove: m->MouseMove((int)a, (int)b); return 0;
        case MC::TimerEvent: return m->TimerEvent((int)a);
        case MC::HandleEvent: m->HandleComponentEvent(a); return 0;
        case MC::ListSelChangedHook: m->ListSelChangedHook(); return 0;
        case MC::EndSubMenu: m->EndSubMenu(); return 0;
        case MC::CheckSubMenu: m->CheckSubMenu(); return 0;
        case MC::TryKeyDown: return m->TryKeyDown((int)a);
        case MC::Reset: m->Reset(); return 0;
        case MC::SubMenu: return m->mCurSubMenu;
        case MC::KeyboardMode: return m->mKeyboardMode;
        case MC::TimerAcc: return m->mTimerAcc;
        case MC::MouseUpCount: return m->mMouseUpCount;
        }
        return 0;
    }
    void Free() { delete this->mMenu; }
};

struct RustSide : Side {
    EeWonMenu* mMenu;
    RustSide() { this->mMenu = ee_won_menu_new((void*)kMenu, ScriptedHost, sList); }
    void Hook() override { ee_won_menu_call(this->mMenu, MC::ListSelChangedHook, 0, 0, 0); }
    intptr_t Menu(MC c, intptr_t a, intptr_t b, intptr_t cc) override { return ee_won_menu_call(this->mMenu, c, a, b, cc); }
    void Free() { ee_won_menu_free(this->mMenu); }
};

// ---- the driver

enum { kAdd, kDown, kUp, kMove, kMoveSame, kTimer, kEvent, kEnd, kCheck, kKey, kReset, kSel, kFlags, kSize, kQuery, kCalls };
static const char* kCallNames[kCalls] = {"add", "down", "up", "move", "move-same", "timer", "event", "end", "check", "key",
                                         "reset", "sel", "flags", "size", "query"};
static unsigned sDriverSeed;
static unsigned DR(unsigned n) { sDriverSeed = sDriverSeed * 69069u + 1u; return n ? (sDriverSeed >> 16) % n : 0; }
static int sLastX, sLastY;

static intptr_t NewItem()
{
    HostItem h;
    h.top = 0; h.height = 0; h.width = 0;
    h.selected = false; h.selectable = DR(8) != 0;
    h.menuItem = DR(6) != 0; h.disabled = DR(5) == 0;
    h.sub = 0;
    if (h.menuItem && DR(2)) {
        h.sub = 0x3000 + (intptr_t)DR(4);
        if (sSubs.find(h.sub) == sSubs.end()) { HostSub s = {0, 0}; sSubs[h.sub] = s; }
    }
    h.refs = 1; // the creator's
    intptr_t id = 0x1000 + (++sNextItem);
    sItems[id] = h;
    return id;
}

static void Run()
{
    static const int kWeights[kCalls] = {5, 3, 3, 4, 2, 4, 3, 2, 3, 4, 1, 4, 1, 1, 1};
    int aTotal = 0;
    for (int i = 0; i < kCalls; i++) aTotal += kWeights[i];
    int r0 = (int)DR(aTotal), k = 0;
    while (r0 >= kWeights[k]) r0 -= kWeights[k++];
    long long a = 0, b = 0, c = 0;
    int w = sFields[(int)Fd::ScrollWidth] + 20, h = (int)Base(LC::NumItems) * 16 + 20;
    switch (k) {
    case kAdd: a = NewItem(); break;
    case kDown: case kUp: a = (int)DR(w) - 10; b = (int)DR(h) - 10; c = DR(3) == 0 ? 1 : 0; break;
    case kMove: a = (int)DR(w) - 10; b = (int)DR(h) - 10; break;
    case kMoveSame: a = sLastX; b = sLastY; break;
    case kTimer: a = (int)DR(400); break;
    case kEvent: {
        a = 0x6000 + (++sNextEvent);
        HostEvent e;
        intptr_t cur = sSide->Menu(MC::SubMenu);
        e.comp = DR(3) != 0 && cur != 0 ? cur : 0x4444;
        e.type = DR(4) == 0 ? 15 : 3;
        sEvents[(intptr_t)a] = e;
        break;
    }
    case kKey: { static const int kK[] = {0x25, 0x1b, 0x27, 0x26, 0x28, 0x41}; a = kK[DR(6)]; break; }
    case kSel: a = (int)DR((unsigned)Base(LC::NumItems) + 2) - 1; break;
    case kFlags: { static const int kF[] = {4, 2, 0x10}; a = kF[DR(3)]; b = DR(2); break; }
    case kQuery: a = DR(4); break;
    default: break;
    }
    if (k == kMove || k == kMoveSame) { sLastX = (int)a; sLastY = (int)b; }
    char aLine[160];
    snprintf(aLine, sizeof(aLine), "call %s %llx %llx %llx", kCallNames[k], a, b, c);
    sTrace.push_back(aLine);
    size_t aCallLine = sTrace.size() - 1;
    intptr_t r = 0;
    switch (k) {
    case kAdd: Base(LC::InsertItem, (intptr_t)a, -1); sItems[(intptr_t)a].refs--; r = Base(LC::NumItems); break;
    case kDown: sSide->Menu(MC::MouseDown, (intptr_t)a, (intptr_t)b, (intptr_t)c); r = sSide->Menu(MC::MouseUpCount); break;
    case kUp: sSide->Menu(MC::MouseUp, (intptr_t)a, (intptr_t)b, (intptr_t)c); r = sSide->Menu(MC::MouseUpCount); break;
    case kMove: case kMoveSame: sSide->Menu(MC::MouseMove, (intptr_t)a, (intptr_t)b); r = sSide->Menu(MC::KeyboardMode); break;
    case kTimer: r = sSide->Menu(MC::TimerEvent, (intptr_t)a) * 1000 + sSide->Menu(MC::TimerAcc); break;
    case kEvent: sSide->Menu(MC::HandleEvent, (intptr_t)a); break;
    case kEnd: sSide->Menu(MC::EndSubMenu); r = sSide->Menu(MC::SubMenu); break;
    case kCheck: sSide->Menu(MC::CheckSubMenu); r = sSide->Menu(MC::SubMenu); break;
    case kKey: r = sSide->Menu(MC::TryKeyDown, (intptr_t)a); break;
    case kReset: sSide->Menu(MC::Reset); break;
    case kSel: Base(LC::SetSelItemAt, (intptr_t)a, 1); r = Base(LC::SelItem); break;
    case kFlags: Base(LC::SetListFlags, (intptr_t)a, (intptr_t)b); break;
    case kSize: Base(LC::SizeChanged); break;
    case kQuery: { static const MC kQ[] = {MC::SubMenu, MC::KeyboardMode, MC::TimerAcc, MC::MouseUpCount}; r = sSide->Menu(kQ[a]); break; }
    }
    sTrace[aCallLine] += " -> " + std::to_string((long long)r);
}

static std::vector<std::string> RunSide(bool inRust, unsigned inSeq, int inCalls)
{
    sTrace.clear();
    sItems.clear();
    sSubs.clear();
    sEvents.clear();
    sNextItem = sNextEvent = 0;
    sLastX = sLastY = 0;
    sSeed = inSeq * 2654435761u + 37u;
    sDriverSeed = inSeq * 40503u + 17u;
    memset(sFields, 0, sizeof(sFields));
    sFields[(int)Fd::ScrollWidth] = 40 + (int)R(260);
    sFields[(int)Fd::ScrollHeight] = 20 + (int)R(180);
    sFields[(int)Fd::Width] = sFields[(int)Fd::ScrollWidth] + 4;
    sFields[(int)Fd::Height] = sFields[(int)Fd::ScrollHeight] + 4;
    sFields[(int)Fd::UpScrollAmount] = sFields[(int)Fd::DownScrollAmount] = 10;
    TraceFields();
    sList = ee_won_list_new((void*)kMenu, ScriptedHost);
    ee_won_list_call(sList, LC::SetListFlags, 0x806, 1, 0); // the constructor's list flags
    OrigSide* o = NULL;
    RustSide* rs = NULL;
    if (inRust) sSide = rs = new RustSide;
    else sSide = o = new OrigSide;
    for (int i = 0; i < inCalls; i++) Run();
    sTrace.push_back("free");
    if (inRust) rs->Free(); else o->Free();
    ee_won_list_free(sList);
    delete sSide;
    sSide = NULL;
    sList = NULL;
    bool aBack = true;
    for (std::map<intptr_t, HostItem>::const_iterator i = sItems.begin(); i != sItems.end(); ++i)
        if (i->second.refs != 0) aBack = false;
    for (std::map<intptr_t, HostSub>::const_iterator i = sSubs.begin(); i != sSubs.end(); ++i)
        if (i->second.refs != 0) aBack = false;
    sTrace.push_back(aBack ? "refs back" : "refs LEAKED");
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    const int kSequences = 20000;
    const int kCalls = aPrint ? 16 : 40;
    size_t aLines = 0;
    std::set<std::string> aSeen;
    int aDiffering = 0;
    for (int seq = 0; seq < kSequences; seq++) {
        sOpsMet.clear();
        std::vector<std::string> a = RunSide(false, (unsigned)seq, kCalls);
        std::vector<std::string> b = RunSide(true, (unsigned)seq, kCalls);
        aLines += a.size();
        CHECK(a.back() == "refs back");
        bool same = a == b;
        CHECK(same);
        if (!same) {
            aDiffering++;
            if (aDiffering <= 3) {
                printf("sequence %d differs\n", seq);
                for (size_t i = 0; i < a.size() || i < b.size(); i++) {
                    const char* x = i < a.size() ? a[i].c_str() : "<end>";
                    const char* y = i < b.size() ? b[i].c_str() : "<end>";
                    if (strcmp(x, y) != 0) { printf("  line %u\n  orig: %s\n  rust: %s\n", (unsigned)i, x, y); break; }
                }
            }
        }
        bool aNew = seq < 40;
        for (std::set<int>::const_iterator i = sOpsMet.begin(); i != sOpsMet.end(); ++i)
            if (aSeen.insert("op" + std::to_string(*i)).second) aNew = true;
        for (size_t i = 0; i < a.size(); i++)
            if (a[i].compare(0, 5, "call ") == 0)
                if (aSeen.insert(a[i].substr(0, a[i].find(' ', 5))).second) aNew = true;
        if (aPrint && aNew) {
            printf("seq %d\n", seq);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (!aPrint) {
        printf("ops met:");
        for (std::set<std::string>::const_iterator i = aSeen.begin(); i != aSeen.end(); ++i)
            if (i->compare(0, 2, "op") == 0) printf(" %s", i->c_str() + 2);
        printf("\n");
        printf("won_menuobj_test: %d sequences, %u trace lines, %u ops and calls met, %d differing, %d checks, "
               "%d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), aDiffering, sChecks, sFailures);
    }
    return sFailures;
}
