// WONGUI's list lines and selection (ListArea.cpp): the original's statements (copied from the C++ before the port,
// the items replaced by their tops, heights, widths and selectable flags, the STL calls kept) against ee_won's
// guilist module through its C ABI: FindLineAt (std::lower_bound over sorted and unsorted tops), FindItemAt's test,
// IsItemVisible, EnsureVisible, EnforceInvariants, AdjustPosition's full-line extra height, FindSelectableItem
// (without the wrap cycle the original never returned from), SelectRange's bounds, KeyDown's target and scroll,
// InsertItem's, RefreshItem's and DeleteItemPrv's scrolls, the batch insert's top, RefreshItem's new place,
// DeleteRange's bounds and MouseMove's tool tip, over generated lists (a fixed LCG). Prints one line per failed
// check; the exit code is the number of failed checks.
#include <stdio.h>
#include <limits.h>
#include <algorithm>
#include <vector>
#include "rust/ee_won.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 99;
static unsigned int Next() { sSeed = sSeed * 1103515245u + 12345u; return sSeed >> 8; }
static int Small() { return (int)(Next() % 400) - 60; }

struct Item { int mTop, mHeight, mWidth; bool mSelectable; };
typedef std::vector<Item> Items;

static Items RandList(bool sorted)
{
    Items v;
    int n = (int)(Next() % 12);
    if (Next() % 20 == 0) n = (int)(Next() % 200);
    int y = 0;
    for (int i = 0; i < n; i++) {
        Item it;
        it.mHeight = (int)(Next() % 25) + (Next() % 9 == 0 ? 0 : 1);
        it.mWidth = (int)(Next() % 300);
        it.mSelectable = Next() % 3 != 0;
        it.mTop = sorted ? y : Small();
        y += it.mHeight;
        v.push_back(it);
    }
    return v;
}

extern "C" int TopOf(const void* ctx, size_t i) { return (*(const Items*)ctx)[i].mTop; }
extern "C" bool SelectableOf(const void* ctx, size_t i) { return (*(const Items*)ctx)[i].mSelectable; }

// ---- the original's statements

struct TopLess { bool operator()(const Item& a, const Item& b) const { return a.mTop<b.mTop; } };
static size_t OrigFindLineAt(const Items& mItems, int y)
{
    Item aSearch = { y, 0, 0, false };
    Items::const_iterator first=std::lower_bound(mItems.begin(),mItems.end(),aSearch,TopLess());
    if(first==mItems.end() || first->mTop>y)
    {
        if(first!=mItems.begin()) --first;
    }
    return first-mItems.begin();
}
static bool OrigIsItemVisible(const Item& item, int mVertOffset, int aViewHeight, bool partial)
{
    if(item.mTop+item.mHeight<mVertOffset) return false;
    int last=item.mTop;
    if(!partial) last+=item.mHeight;
    return last<=mVertOffset+aViewHeight;
}
static int OrigEnsureVisible(const Item& item, int mVertOffset, int aViewHeight)
{
    int offset=mVertOffset;
    if(item.mTop+item.mHeight>mVertOffset+aViewHeight)
        offset=item.mTop-aViewHeight+item.mHeight;
    if(offset>item.mTop) offset=item.mTop;
    return offset!=mVertOffset ? offset : mVertOffset;
}
static void OrigEnforce(Items& mItems, int& mTotalHeight, int& mTotalWidth)
{
    mTotalHeight=mTotalWidth=0;
    for(size_t i=0;i<mItems.size();++i) {
        mItems[i].mTop=mTotalHeight;
        mTotalHeight+=mItems[i].mHeight;
        if(mItems[i].mWidth>mTotalWidth) mTotalWidth=mItems[i].mWidth;
    }
}
// FindSelectableItem with a depth guard: -2 when the original would have recursed without end
static long long OrigFindSelectable(const Items& mItems, int start, bool up, bool opposite, bool wrap, int depth)
{
    if (depth > 8) return -2;
    int step=up ? -1 : 1;
    int i=start;
    for(;i>=0 && (unsigned int)i<mItems.size();i+=step) // GetItem(i) != NULL
        if(mItems[i].mSelectable) return i;
    if(wrap) {
        if(up) return OrigFindSelectable(mItems,(int)mItems.size()-1,false,false,wrap,depth+1);
        else return OrigFindSelectable(mItems,0,true,false,wrap,depth+1);
    }
    if(opposite) return OrigFindSelectable(mItems,start,!up,false,wrap,depth+1);
    return -1;
}
static bool OrigSelectRange(int start, int end, int aSize, int& outStart, int& outEnd, int& outStep)
{
    if(start>=aSize) start=aSize-1;
    if(end>=aSize) end=aSize-1;
    if(start<0) start=0;
    if(end<0) end=0;
    if(start>=aSize || end>=aSize) return false;
    outStart=start; outEnd=end; outStep=start<=end ? 1 : -1;
    return true;
}
static bool OrigKeyTarget(int theKey, int y, int aHeight, int aViewHeight, int mTotalHeight, bool wrap, int& outY,
                          bool& outUp)
{
    int height = aHeight+1;
    int anItemPos = -1;
    switch(theKey)
    {
        case 40: anItemPos = y+height; break;
        case 38: anItemPos = y-1; break;
        case 34: anItemPos = y+aViewHeight; break;
        case 33: anItemPos = y+height-aViewHeight; break;
        case 39: case 37: break; // the horizontal scroll
        default: return false;
    }
    bool up = anItemPos<y;
    if(wrap)
    {
        if(anItemPos<0)
        {
            anItemPos = mTotalHeight;
            up = true;
        }
        else if(anItemPos>=mTotalHeight)
        {
            anItemPos = 0;
            up = false;
        }
    }
    outY = anItemPos; outUp = up;
    return true;
}

// ---- the checks

static void TestLines()
{
    for (int n = 0; n < 100000; n++) {
        Items v = RandList(n % 4 != 0);
        for (int k = 0; k < 6; k++) {
            int y = k == 0 ? INT_MIN : k == 1 ? INT_MAX : Small();
            size_t o = OrigFindLineAt(v, y);
            CHECK(o == ee_won_gui_list_line_at(v.size(), y, TopOf, &v));
            if (o < v.size()) CHECK((v[o].mTop+v[o].mHeight<y ? false : true) == ee_won_gui_list_covers(v[o].mTop, v[o].mHeight, y));
        }
        int anOffset = Small(), aView = (int)(Next() % 200);
        for (size_t i = 0; i < v.size(); i++) {
            for (int p = 0; p < 2; p++)
                CHECK(OrigIsItemVisible(v[i], anOffset, aView, p != 0) ==
                      ee_won_gui_list_visible(v[i].mTop, v[i].mHeight, anOffset, aView, p != 0));
            CHECK(OrigEnsureVisible(v[i], anOffset, aView) ==
                  ee_won_gui_list_ensure_visible(v[i].mTop, v[i].mHeight, anOffset, aView));
        }
        Items e = v;
        int th = 0, tw = 0;
        OrigEnforce(e, th, tw);
        std::vector<int> hs, ws, ts(v.size() + 1, -9);
        for (size_t i = 0; i < v.size(); i++) { hs.push_back(v[i].mHeight); ws.push_back(v[i].mWidth); }
        int rh = -1, rw = -1;
        ee_won_gui_list_stack(hs.empty() ? NULL : &hs[0], ws.empty() ? NULL : &ws[0], &ts[0], v.size(), &rh, &rw);
        CHECK(rh == th && rw == tw);
        for (size_t i = 0; i < v.size(); i++) CHECK(ts[i] == e[i].mTop);
        CHECK(ts[v.size()] == -9);

        // AdjustPosition's full-line extra height on the stacked list
        if (th > aView) {
            size_t i = OrigFindLineAt(e, th-aView+1);
            if (i != e.size()) {
                int x = e[i].mTop+e[i].mHeight+aView-th;
                if (x < 0) x = 0;
                CHECK(x == ee_won_gui_list_full_line_extra(th, aView, e[i].mTop, e[i].mHeight));
            }
        }

        // FindSelectableItem
        for (int s = -1; s <= (int)v.size(); s++)
            for (int f = 0; f < 8; f++) {
                bool up = (f & 1) != 0, opp = (f & 2) != 0, wrap = (f & 4) != 0;
                long long o = OrigFindSelectable(v, s, up, opp, wrap, 0);
                long long r = ee_won_gui_list_find_selectable(v.size(), s, up, opp, wrap, SelectableOf, &v);
                CHECK(o == r || (o == -2 && r == -1));
            }

        // SelectRange
        int a = Small() / 10, b = Small() / 10, os = 0, oe = 0, ost = 0, rs = 0, re = 0, rst = 0;
        bool ok = OrigSelectRange(a, b, (int)v.size(), os, oe, ost);
        bool rk = ee_won_gui_list_select_range(a, b, (int)v.size(), &rs, &re, &rst);
        CHECK(ok == rk);
        if (ok && rk) CHECK(os == rs && oe == re && ost == rst);

        // KeyDown
        static const int kKeys[] = { 33, 34, 37, 38, 39, 40, 13 };
        for (int k = 0; k < 7; k++)
            for (int w = 0; w < 2; w++) {
                int y = Small(), h = (int)(Next() % 30), oy = 0, ry = 0;
                bool ou = false, ru = false;
                bool o = OrigKeyTarget(kKeys[k], y, h, aView, th, w != 0, oy, ou);
                bool r = ee_won_gui_list_key_target(kKeys[k], y, h, aView, th, w != 0, &ry, &ru);
                CHECK(o == r);
                if (o && r) CHECK(oy == ry && ou == ru);
            }
    }
}

static void TestScrolls()
{
    for (int n = 0; n < 500000; n++) {
        int top = Small(), h = (int)(Next() % 30), off = Small(), view = (int)(Next() % 200), extra = (int)(Next() % 20);
        int total = Small() * 2, oldh = (int)(Next() % 30), topLine = (int)(Next() % 10), pos = (int)(Next() % 10);
        bool fixed = (n & 1) != 0, bottom = (n & 2) != 0, end = (n & 4) != 0;

        int o = off; // KeyDown's scroll to the new item
        if(top+h > o+view) o = top+h-view + extra;
        if(top < o) o = top;
        CHECK(o == ee_won_gui_list_key_scroll(top, h, off, view, extra));

        o = off; // InsertItem
        if(end) { if(!fixed && bottom) o+=h; }
        else if(!fixed && (topLine>=pos || bottom) && total>view) o+=h;
        CHECK(o == ee_won_gui_list_insert_scroll(end, fixed, bottom, topLine, pos, total, view, off, h));

        int aPrevTop = Small(), aNextTop = Small(); // the batch insert's top
        for (int p = 0; p < 2; p++) {
            int prev=p==0 ? 0 : aPrevTop;
            int y=prev+1;
            if(y>aNextTop) y=aNextTop;
            CHECK(y == ee_won_gui_list_batch_top(p != 0, aPrevTop, aNextTop));
        }

        o = off; // RefreshItem
        if(!fixed && (o>=top || bottom) && total>view) o+=h-oldh;
        CHECK(o == ee_won_gui_list_refresh_scroll(fixed, bottom, top, off, total, view, h, oldh));

        o = off; // DeleteItemPrv
        if(!fixed && o>=top) o-=h;
        CHECK(o == ee_won_gui_list_delete_scroll(fixed, top, off, h));

        // RefreshItem's place
        std::vector<int> aItems((size_t)(Next() % 8));
        int position = (int)(Next() % 9) - 1, insert = (int)(Next() % 11) - 2;
        int i = insert;
        if(position<i) --i;
        if(i>=aItems.size()) i=aItems.size()-1;
        if(i<0) i=0;
        CHECK(i == ee_won_gui_list_refresh_place(position, insert, aItems.size()));

        // DeleteRange
        int start = (int)(Next() % 12) - 2, endp = (int)(Next() % 14) - 3, rs = -5, ra = -5;
        int e2 = endp;
        bool aDo = true;
        if(e2<0 || (unsigned int)e2>aItems.size()) e2=aItems.size();
        if(start<0 || e2<=start) aDo = false;
        int amount=e2-start;
        if(aDo && (unsigned int)(start+amount)>aItems.size()) amount=aItems.size()-start;
        bool rk = ee_won_gui_list_delete_range(start, endp, aItems.size(), &rs, &ra);
        CHECK(aDo == rk);
        if (aDo && rk) CHECK(rs == start && ra == amount);

        // MouseMove's tool tip
        int w = (int)(Next() % 300), vw = (int)(Next() % 300);
        bool show=false;
        if(w>vw) {
            int half=h>>1;
            if(top+half>off && top+half<off+view) show=true;
        }
        CHECK(show == ee_won_gui_list_item_tip(w, h, top, vw, off, view));
    }
}

int main()
{
    TestLines();
    TestScrolls();
    printf("won_guilist_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
