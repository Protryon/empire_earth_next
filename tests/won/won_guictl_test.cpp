// WONGUI's compound controls (ComboBox.cpp, TabCtrl.cpp, Splitter.cpp): the original's statements (copied from
// the C++ before the port, the components replaced by the numbers and flags they read, their SetPos / SetSize /
// SetEnabled calls by the values they received) against ee_won's guictl module through its C ABI, over generated
// inputs (a fixed LCG; small, edge and wrapping ints). Prints one line per failed check; the exit code is the
// number of failed checks.
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <vector>
#include "rust/ee_rust.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 1618;
static unsigned int Next() { sSeed = sSeed * 1103515245u + 12345u; return sSeed >> 8; }
static int Small() { return (int)(Next() % 61) - 20; }
static int Any()
{
    switch (Next() % 8) {
    case 0: return (int)Next() - (int)(Next() << 8);
    case 1: return 0x7fffffff - (int)(Next() % 3);
    case 2: return (int)0x80000000 + (int)(Next() % 3);
    default: return (int)(Next() % 400) - 60;
    }
}
static int Val(int n) { return n % 11 == 0 ? Any() : (int)(Next() % 300) - 20; }

static std::vector<bool> sDisabled;
static bool DisabledOf(const void*, int32_t i) { return i < 0 || i >= (int)sDisabled.size() || sDisabled[i]; }
static bool sMatches[8];
static bool MatchOf(const void*, uintptr_t i) { return sMatches[i]; }

static void Combo()
{
    for (int n = 0; n < 400000; n++) {
        // CalcListBoxSize
        int mListBoxWidth = Val(n), mListBoxHeight = Val(n), boxW = Val(n), areaW = Val(n), rp = Small(), lp = Small(),
            totalW = Val(n), comboW = Val(n), totalH = Val(n);
        int width=mListBoxWidth;
        int contentWidth=boxW-areaW+rp+lp+totalW;
        if(width>contentWidth) width=contentWidth;
        if(width<comboW) width=comboW;
        int height=totalH+4;
        if(height>mListBoxHeight) height=mListBoxHeight;
        int rw = -1, rh = -1;
        ee_won_gui_combo_list_size(mListBoxWidth, mListBoxHeight, boxW, areaW, rp, lp, totalW, comboW, totalH, &rw, &rh);
        CHECK(rw == width && rh == height);
        // SetInputBoxToListSelection
        int itemW = Val(n), editW = Val(n);
        CHECK((itemW>editW-10) == ee_won_gui_combo_select_to_end(itemW, editW));
        // KeyDown
        static const int kKeys[] = { 40, 38, 33, 34, 13, 27, 39, 37, 9, 0 };
        int key = kKeys[Next() % 10];
        bool mListBoxVisible = (Next() & 1) != 0;
        int o;
        if(key==40 && !mListBoxVisible) o=1;
        else if(mListBoxVisible && (key==40 || key==38 || key==33 || key==34)) o=2;
        else if(mListBoxVisible && key==13) o=3;
        else if(key==27 && mListBoxVisible) o=4;
        else o=0;
        CHECK(o == ee_won_gui_combo_key(key, mListBoxVisible));
        // ShowList's / SetTextAndSel's search
        int count = (int)(Next() % 8);
        for (int i = 0; i < 8; i++) sMatches[i] = Next() % 4 == 0;
        int i=0;
        while(i<count) { if(sMatches[i]) break; ++i; }
        CHECK((i==count ? -1 : i) == ee_won_gui_first_match(count, MatchOf, NULL));
    }
}

static void Tabs()
{
    for (int n = 0; n < 400000; n++) {
        // PositionTab
        int id = (int)(Next() % 8) - 2, size = (int)(Next() % 6), prevRight = Val(n), mOverlap = Small();
        int x=0;
        if(id>0 && id<=size) x=prevRight-mOverlap;
        CHECK(x == ee_won_gui_tab_x(id, size, prevRight, mOverlap));
        // EnableTab's neighbour
        int oldId = n % 9 == 0 ? Any() : (int)(Next() % 8);
        CHECK((oldId>0 ? oldId-1 : oldId+1) == ee_won_gui_tab_neighbour(oldId));
        // KeepTabButtonInView
        {
            int left = Val(n), width = Val(n), limit = Val(n);
            int right=left+width;
            bool scrolled=false; int amount=0;
            if(left>=0 && width<=limit) {
                if(right>limit) { scrolled=true; amount=limit-right; }
            } else { scrolled=true; amount=-left; }
            int r = 12345;
            bool rs = ee_won_gui_tab_keep_in_view(left, width, limit, &r);
            CHECK(rs == scrolled && (!rs || r == amount));
        }
        // AdjustTabBarScroll
        {
            int oldX = Val(n), inAmount = n % 3 == 0 ? (Next() % 2 ? 10 : -10) : Val(n), width = Val(n), containerWidth = Val(n);
            int xx=oldX+inAmount;
            bool arrows=false, enableLeft=true, enableRight=true;
            if(containerWidth>=width) xx=0;
            else {
                if(xx+width<containerWidth-5) { enableRight=false; xx=containerWidth-width-5; }
                if(xx>=0) { enableLeft=false; xx=0; }
                arrows=true;
            }
            int rx = 0; bool rl = false, rr = false;
            bool ra = ee_won_gui_tab_scroll(oldX, inAmount, width, containerWidth, &rx, &rl, &rr);
            CHECK(rx == xx && ra == arrows && (!ra || (rl == enableLeft && rr == enableRight)));
        }
        // PositionTabContainer
        {
            int mLeftPad = Small(), mRightPad = Small(), mTopPad = Small(), mBottomPad = Small(), top = Val(n), h = Val(n), w = Val(n);
            EeWonRect4 r = ee_won_gui_tab_page_rect(mLeftPad, mRightPad, mTopPad, mBottomPad, top, h, w);
            CHECK(r.x == mLeftPad && r.y == top+mTopPad && r.w == w-mRightPad-mLeftPad && r.h == h-mBottomPad-mTopPad);
        }
        // CalcContainerLayout
        {
            bool onBottom = (Next() & 1) != 0;
            int H = Val(n), barTop = Val(n), barBottom = Val(n), mTabBarOverlap = Small();
            int top=0, bottom=H;
            if(onBottom) bottom=barTop+mTabBarOverlap;
            else top=barBottom-mTabBarOverlap;
            int rh = 0;
            int rt = ee_won_gui_tab_area(onBottom, H, barTop, barBottom, mTabBarOverlap, &rh);
            CHECK(rt == top && rh == bottom-top);
        }
        // PositionArrows
        {
            bool onLeft = (Next() & 1) != 0, onBottom = (Next() & 1) != 0;
            int W = Val(n), rightW = Val(n), leftW = Val(n), leftH = Val(n), aTop = Val(n), aBottom = Val(n);
            int ax=0;
            if(!onLeft) ax=W-rightW-leftW;
            int ay;
            if(onBottom) ay=aBottom; else ay=aTop-leftH;
            int ry = 0;
            int rx = ee_won_gui_tab_arrows(onLeft, onBottom, W, rightW, leftW, leftH, aTop, aBottom, &ry);
            CHECK(rx == ax && ry == ay);
        }
        // VertPositionTabBar
        {
            int mTabBarHeight = Val(n), mTabBarOverlap = Small(), mBottomTabTopOffset = Small(), H = Val(n), containerH = Val(n);
            bool onBottom = (Next() & 1) != 0;
            CHECK(mTabBarHeight-mTabBarOverlap-mBottomTabTopOffset == ee_won_gui_tab_bottom_height(mTabBarHeight, mTabBarOverlap, mBottomTabTopOffset));
            int y=0;
            int offset=mBottomTabTopOffset;
            if(onBottom) { y=H-containerH; offset=-offset; }
            int rb = 0;
            int ry = ee_won_gui_tab_vert(onBottom, H, containerH, mBottomTabTopOffset, &rb);
            CHECK(ry == y && rb == offset+y);
        }
        // HorzPositionTabBar
        {
            bool need = (Next() & 1) != 0, arrowsOnLeft = (Next() & 1) != 0, onRight = (Next() & 1) != 0;
            bool rightThere = Next() % 3 != 0, leftThere = Next() % 3 != 0;
            int rightRight = Val(n), leftLeft = Val(n), W = Val(n), barW = Val(n), off = Small();
            int left=0, right=W;
            if(need) {
                if(arrowsOnLeft) { if(rightThere) left=rightRight; }
                else { if(leftThere) right=leftLeft-1; }
            } else if(onRight) left=W-barW-off;
            int rr = 0;
            int rl = ee_won_gui_tab_horz(need, arrowsOnLeft, rightThere, rightRight, leftThere, leftLeft, onRight, W, barW, off, &rr);
            CHECK(rl == left && rr == right);
        }
        // AddTab's bar height
        {
            int mTabBarHeight = Next() % 2 ? 0 : Val(n), tabH = Val(n);
            int o = mTabBarHeight;
            if(o==0) { if(tabH>0) o=tabH; else o=20; }
            CHECK(o == ee_won_gui_tab_bar_height(mTabBarHeight, tabH));
        }
        // KeyDown's target
        {
            int count = (int)(Next() % 7);
            sDisabled.assign(count, false);
            for (int i = 0; i < count; i++) sDisabled[i] = Next() % 3 == 0;
            int inKey = Next() % 5 == 0 ? 13 : (Next() % 2 ? 39 : 37);
            int old = count ? (int)(Next() % count) : -1;
            if (Next() % 13 == 0) old = (int)(Next() % 10) - 3;
            // without pages the bound is the largest size_t: the walk ran up to the int's wrap (both do; the
            // control always has a page when a tab is selected)
            if (count == 0) old = -2;
            size_t mTabVectorSize = (size_t)count;
            int step = 0; bool used = true;
            if(inKey==39) step=1; else if(inKey==37) step=-1; else used=false;
            if (used) {
                int next=old+step;
                while(DisabledOf(NULL,next) && next>0 && next<mTabVectorSize-1) {
                    if(inKey==39) { ++step; ++next; }
                    else if(inKey==37) { --step; --next; }
                }
            }
            int r = 0;
            bool ru = ee_won_gui_tab_key_target(inKey, old, count, DisabledOf, NULL, &r);
            CHECK(ru == used && (!ru || r == old+step));
        }
        // RemoveTab's next tab
        {
            int theId = (int)(Next() % 6), aCurTabId = (int)(Next() % 7) - 1;
            int o;
            if(theId<aCurTabId) o=aCurTabId-1; else if(theId>aCurTabId) o=aCurTabId; else o=0;
            CHECK(o == ee_won_gui_tab_after_remove(theId, aCurTabId));
        }
    }
}

static int Trunc(double d) { return (int)d; } // the compiled conversion (cvttsd2si on x86)

static void Splitters()
{
    static const int kPositions[] = { 1, 2, 4, 8, 3, 6, 9, 12, 0, 5, 15 };
    static const int kTypes[] = { 5, 10, 0x15, 0x25, 0x4a, 0x8a, 0x10f, 0, 0x30 };
    for (int n = 0; n < 400000; n++) {
        int W = Val(n), H = Val(n), barW = n % 7 == 0 ? Any() : (int)(Next() % 12), barH = n % 7 == 1 ? Any() : (int)(Next() % 12);
        int sx = Val(n), sy = Val(n);
        int pos = kPositions[Next() % 11];
        // MaxQuadrant's split
        {
            bool lr = true, ud = true;
            int x = sx, y = sy;
            if(pos & 1) x = W-barW/2;
            else if(pos & 4) x = barW/2;
            else lr = false;
            if(pos & 2) y = H-barH/2;
            else if(pos & 8) y = barH/2;
            else ud = false;
            int rx = 0, ry = 0; bool rlr = false, rud = false;
            ee_won_gui_split_max(pos, W, H, barW, barH, sx, sy, &rx, &ry, &rlr, &rud);
            CHECK(rx == x && ry == y && rlr == lr && rud == ud);
        }
        // the maximise buttons
        {
            int id = 100000 + (int)(Next() % 24);
            if (n % 17 == 0) id = Any();
            CHECK(id - 100006 + 1 == ee_won_gui_split_button_quadrant(id));
            CHECK(!(id<100006 || id>100017) == ee_won_gui_split_is_max_button(id));
        }
        // SetSplitterPos
        bool lrMax = Next() % 4 == 0, udMax = Next() % 4 == 0, valid = (Next() & 1) != 0;
        int mins[4] = { Small(), Small(), Small(), Small() };
        int x = Val(n), y = Val(n);
        if (n % 5 == 0) { x = sx; y = sy; }
        EeWonSplit cur = { sx, sy, 0, 0, 0, 0 };
        {
            int ox = x, oy = y;
            if(!lrMax) { if(ox < mins[0]) ox = mins[0]; if(ox > W-mins[1]) ox = W-mins[1]; }
            if(!udMax) { if(oy < mins[2]) oy = mins[2]; if(oy > H-mins[3]) oy = H-mins[3]; }
            bool changed = !(valid && ox==sx && oy==sy);
            EeWonSplit r;
            memset(&r, 0, sizeof r);
            bool rc = ee_won_gui_split_set(x, y, lrMax, udMax, mins[0], mins[1], mins[2], mins[3], W, H, valid, sx, sy, barW, barH, &r);
            CHECK(rc == changed);
            if (rc && changed) {
                CHECK(r.x == ox && r.y == oy && r.left == ox - barW/2 && r.right == ox + barW/2 && r.top == oy - barH/2 && r.bottom == oy + barH/2);
                cur = r;
                double pct = 0;
                bool rp = ee_won_gui_split_percent(ox, W, &pct);
                CHECK(rp == (W > 0) && (!rp || pct == ox*100.0/W));
            }
        }
        // Virtual08 and UnMaxQuadrant
        {
            double mLeftPercent = n % 13 == 0 ? (double)Any() : (Next() % 1000) / 7.0, mTopPercent = (Next() % 1000) / 3.0;
            if (n % 97 == 0) mTopPercent = NAN;
            bool have = (Next() & 1) != 0;
            int ox, oy;
            if (have) {
                double aLeftPercent = mLeftPercent, aTopPercent = mTopPercent;
                int xoff = 0, yoff = 0;
                if(pos & 1) { aLeftPercent = 100; xoff = -barW/2; }
                else if(pos & 4) { aLeftPercent = 0; xoff = barW/2; }
                if(pos & 2) { aTopPercent = 100; yoff = -barH/2; }
                else if(pos & 8) { aTopPercent = 0; yoff = barH/2; }
                ox = Trunc(W*aLeftPercent/100 + xoff); oy = Trunc(H*aTopPercent/100 + yoff);
            } else { ox = Trunc(W*mLeftPercent/100); oy = Trunc(H*mTopPercent/100); }
            int ry = 0;
            int rx = ee_won_gui_split_layout(have, pos, mLeftPercent, mTopPercent, barW, barH, W, H, &ry);
            CHECK(rx == ox && ry == oy);
            CHECK(Trunc(W*mLeftPercent/100) == ee_won_gui_split_at(W, mLeftPercent, 0));
        }
        // CalcQuadrants
        {
            int left = 0, right = W, top = 0, bottom = H;
            if(pos & 1) right = cur.left;
            if(pos & 4) left = cur.right;
            if(pos & 2) bottom = cur.top;
            if(pos & 8) top = cur.bottom;
            EeWonRect4 r = ee_won_gui_split_pane(pos, W, H, cur);
            CHECK(r.x == left && r.y == top && r.w == right-left && r.h == bottom-top);
        }
        // CalcSplitterPositions, the cursor
        {
            int type = kTypes[Next() % 9];
            int left = 0, top = 0, right = W, bottom = H;
            if(type & 5) { left = cur.left; right = cur.right; }
            if(type & 10) { top = cur.top; bottom = cur.bottom; }
            switch(type) {
                case 0x15: bottom = cur.top; break;
                case 0x25: top = cur.bottom; break;
                case 0x4a: right = cur.left; break;
                case 0x8a: left = cur.right; break;
            }
            EeWonRect4 r = ee_won_gui_split_bar(type, W, H, cur);
            CHECK(r.x == left && r.y == top && r.w == right-left && r.h == bottom-top);
            int c = 0;
            if(type==0x10f) c = 1; else if(type & 5) c = 2; else if(type & 10) c = 3;
            CHECK(c == ee_won_gui_split_cursor(type));
        }
        // MouseDrag
        {
            int mx = Val(n), my = Val(n), cl = Val(n), ct = Val(n), dx = Small(), dy = Small();
            bool lr = (Next() & 1) != 0, ud = (Next() & 1) != 0;
            int aDeltaX = 0, aDeltaY = 0;
            if(lr) aDeltaX = mx - cl - dx;
            if(ud) aDeltaY = my - ct - dy;
            int ry = 0;
            int rx = ee_won_gui_split_drag(mx, my, cl, ct, dx, dy, lr, ud, sx, sy, &ry);
            CHECK(rx == sx+aDeltaX && ry == sy+aDeltaY);
        }
    }
}

int main()
{
    Combo();
    Tabs();
    Splitters();
    printf("won_guictl_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
