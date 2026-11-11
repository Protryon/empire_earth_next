// WONGUI's window manager's popup and modality rules and the popup menus' timing (WindowManager.cpp,
// MenuBehavior.cpp): the original's statements (copied from the C++ before the port, the popup a plain rectangle
// whose SetPos calls are recorded, the windows and dialogs flags) against ee_won's guiwin module through its C ABI,
// over generated inputs (a fixed LCG). Prints one line per failed check; the exit code is the number of failed
// checks.
#include <stdio.h>
#include <vector>
#include "rust/ee_rust.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 777;
static unsigned int Next() { sSeed = sSeed * 1103515245u + 12345u; return sSeed >> 8; }
static int Any()
{
    switch (Next() % 8) {
    case 0: return (int)Next() - (int)(Next() << 8);
    case 1: return 0x7fffffff - (int)(Next() % 3);
    case 2: return (int)0x80000000 + (int)(Next() % 3);
    default: return (int)(Next() % 900) - 100;
    }
}
static int Val(int n) { return n % 17 == 0 ? Any() : (int)(Next() % 900) - 100; }

struct Rect {
    int x, y, width, height;
    int Left() const { return x; } int Top() const { return y; }
    int Right() const { return x + width; } int Bottom() const { return y + height; }
    int Width() const { return width; } int Height() const { return height; }
};
struct Popup : Rect {
    std::vector<int> moves;
    void SetPos(int ax, int ay) { x = ax; y = ay; moves.push_back(ax); moves.push_back(ay); }
};

static void OrigAdjust(Popup* theComponent, const Rect& aBoundingRect, const Rect& theAvoidRect)
{
    Rect anAvoidRect = theAvoidRect;
    int dx = 0, dy = 0;
    if(theComponent->Right() > aBoundingRect.Right())
    {
        dx = aBoundingRect.Right() - theComponent->Right();
        theComponent->SetPos(theComponent->Left() + dx, theComponent->Top());
        if(anAvoidRect.Width()>0 && theComponent->Left()<=theAvoidRect.Right() && theComponent->Right()>=theAvoidRect.x)
            theComponent->SetPos(anAvoidRect.Left()-theComponent->Width(),theComponent->Top());
    }
    else if(theComponent->Left() < aBoundingRect.x)
    {
        dx = aBoundingRect.Left() - theComponent->Left();
        theComponent->SetPos(theComponent->Left() + dx,theComponent->Top());
        if(anAvoidRect.Width()>0 && theComponent->Left()<=theAvoidRect.Right() && theComponent->Right()>=theAvoidRect.x)
            theComponent->SetPos(anAvoidRect.Right(),theComponent->Top());
    }
    if(theComponent->Bottom() > aBoundingRect.Bottom())
    {
        dy = aBoundingRect.Bottom() - theComponent->Bottom();
        theComponent->SetPos(theComponent->Left(), theComponent->Top() + dy);
        if(anAvoidRect.Height()>0 && theComponent->Top()<=theAvoidRect.Bottom() && theComponent->Bottom()>=theAvoidRect.y)
            theComponent->SetPos(theComponent->Left(), anAvoidRect.Top()-theComponent->Height());
    }
    else if(theComponent->Top() < aBoundingRect.y)
    {
        dy = aBoundingRect.Top() - theComponent->Top();
        theComponent->SetPos(theComponent->Left(), theComponent->Top() + dy);
        if(anAvoidRect.Height()>0 && theComponent->Top()<=theAvoidRect.Bottom() && theComponent->Bottom()>=theAvoidRect.y)
            theComponent->SetPos(theComponent->Left(), anAvoidRect.Bottom());
    }
}

static EeWonRect4 R4(const Rect& r) { EeWonRect4 o = { r.x, r.y, r.width, r.height }; return o; }
static Rect Gen(int n) { Rect r = { Val(n), Val(n), n % 11 == 0 ? Any() : (int)(Next() % 400), n % 13 == 0 ? Any() : (int)(Next() % 300) }; return r; }

int main()
{
    for (int n = 0; n < 500000; n++) {
        // AdjustPopupPosition
        {
            Popup p; Rect pr = Gen(n); p.x = pr.x; p.y = pr.y; p.width = pr.width; p.height = pr.height;
            Rect bounds = Gen(n), avoid = Gen(n);
            if (Next() % 3 == 0) avoid.width = 0;
            if (Next() % 3 == 0) avoid.height = 0;
            EeWonRect4 popup4 = R4(p);
            OrigAdjust(&p, bounds, avoid);
            int out[8];
            size_t k = ee_won_gui_popup_adjust(popup4, R4(bounds), R4(avoid), out);
            bool same = k * 2 == p.moves.size();
            for (size_t i = 0; same && i < 2 * k; i++) same = out[i] == p.moves[i];
            CHECK(same);
        }
        int flags = (int)(Next() % 0x20000);
        // DoPopup
        {
            bool open = (Next() & 1) != 0;
            CHECK(((flags&0x1000) && open) == ee_won_gui_popup_additional(flags, open));
            int x = Next() % 3 ? -1 : Val(n), y = Next() % 3 ? -1 : Val(n), mx = Val(n), my = Val(n);
            int ox = x, oy = y;
            if(ox==-1) ox = mx;
            if(oy==-1) oy = my;
            int ry = 0;
            int rx = ee_won_gui_popup_place(x, y, mx, my, &ry);
            CHECK(rx == ox && ry == oy);
            bool mBA5 = (Next() & 1) != 0;
            bool inOwnWindow = mBA5;
            if(((flags & (0x10000))?true:false)) inOwnWindow = true;
            else if(((flags & (0x8000))?true:false)) inOwnWindow = false;
            CHECK(inOwnWindow == ee_won_gui_popup_own_window(flags, mBA5));
        }
        // FindMouseWindow
        {
            int count = (int)(Next() % 5) + 1;
            std::vector<Rect> rects;
            std::vector<EeWonRect4> r4;
            for (int i = 0; i < count; i++) { rects.push_back(Gen(n)); r4.push_back(R4(rects.back())); }
            int px = Val(n), py = Val(n);
            int found = -1;
            int i = count;
            do {
                --i;
                const Rect& r = rects[i];
                if(px>=r.x && px<r.x+r.width && py>=r.y && py<r.y+r.height) { found = i; break; }
            } while(i!=0);
            CHECK(found == ee_won_gui_popup_window_at(&r4[0], count, px, py));
            const Rect& r = rects[0];
            CHECK((px>=r.x && px<r.x+r.width && py>=r.y && py<r.y+r.height) == ee_won_gui_rect_contains(r4[0], px, py));
        }
        // CheckPopupClick's test
        {
            int button = (int)(Next() % 3);
            CHECK(((flags & (1<<button))?true:false) == ee_won_gui_popup_click_ends(flags, button));
        }
        // CalcModality, EndDialog
        {
            bool popupsOpen = (Next() & 1) != 0, hasDialog = (Next() & 1) != 0;
            bool modal = false, setDialog = false;
            bool mPopupIsUp = false;
            if(popupsOpen) { mPopupIsUp = true; if(((flags & 0x100)?true:false)) modal = true; }
            if(hasDialog) { if(!modal) setDialog = true; modal = true; }
            unsigned int bits = ee_won_gui_modality(popupsOpen, flags, hasDialog);
            CHECK((bits & 1) == (modal ? 1u : 0u) && ((bits & 2) != 0) == setDialog);
            bool isDialogWindow = (Next() & 1) != 0, dialogIsRoot = (Next() & 1) != 0;
            bool o;
            if(isDialogWindow && !mPopupIsUp && dialogIsRoot) o = false; else o = modal;
            CHECK(o == ee_won_gui_window_modal(isDialogWindow, mPopupIsUp, dialogIsRoot, modal));
            bool wg = (Next() & 1) != 0, wm = (Next() & 1) != 0, dg = (Next() & 1) != 0, dm = (Next() & 1) != 0;
            bool ends = true;
            if(wg && !wm) ends = false;
            else if(dg && !dm) ends = false;
            CHECK(ends == ee_won_gui_ends_dialog(wg, wm, dg, dm));
        }
        // PopupMenu::TimerEvent, MouseUp, MouseMove; MenuItem::CalcDimensions; CheckSubMenu's place
        {
            bool mKeyboardMode = Next() % 4 == 0;
            int mTimerAcc = (int)(Next() % 400), delta = n % 7 == 0 ? Any() : (int)(Next() % 120);
            bool check = false, keep;
            int acc = mTimerAcc;
            if(mKeyboardMode) keep = false;
            else { acc += delta; if(acc>=350) { check = true; keep = false; } else keep = true; }
            int racc = mTimerAcc; bool rcheck = true;
            bool rkeep = ee_won_gui_menu_timer(mKeyboardMode, &racc, delta, &rcheck);
            CHECK(rkeep == keep && racc == acc && rcheck == check);

            int button = (int)(Next() % 3), mMouseUpCount = (int)(Next() % 4) - 1;
            int c = mMouseUpCount; bool goes = true;
            if(button==0) { ++c; if(c<2) goes = false; }
            int rc = mMouseUpCount;
            CHECK(ee_won_gui_menu_mouse_up(button, &rc) == goes && rc == c);

            int x = (int)(Next() % 4), y = (int)(Next() % 4), lx = (int)(Next() % 4), ly = (int)(Next() % 4);
            int mv;
            if(mKeyboardMode) { if(x==lx && y==ly) mv = -1; else mv = 1; } else mv = 0;
            CHECK(mv == ee_won_gui_menu_mouse_move(mKeyboardMode, x, y, lx, ly));

            int w = Val(n), h = Val(n), rw = w, rh = h;
            ee_won_gui_menu_item_size(&rw, &rh);
            CHECK(rw == w + 32 && rh == h + 4);

            int vw = Val(n), top = Val(n), sy = 0;
            int sx = ee_won_gui_submenu_place(vw, top, &sy);
            CHECK(sx == vw-2 && sy == top);
            int rootX = Val(n);
            EeWonRect4 a = ee_won_gui_submenu_avoid(rootX, vw);
            CHECK(a.x == rootX-vw+2 && a.y == 0 && a.w == vw-2 && a.h == 0);
        }
    }
    printf("won_guiwin_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
