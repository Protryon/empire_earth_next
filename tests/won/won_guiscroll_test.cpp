// WONGUI's scrolling arithmetic: the original's statements (copied from the C++ before the port, the scrollbar's
// controls and the scroll area replaced by the numbers they read, their SetPos / SetSize / SetVisible calls by
// the values they received) against ee_won's guiscroll module through its C ABI: Scrollbar::LayoutThumb,
// MouseDrag's thumb and position, HandleComponentEvent's line and page steps, ScrollArea::AdjustPosition's clamp,
// OnBottom, Virtual08's view and bottom pin, GetDesiredSize, SetSizeEqualToTotal, ScrollbarScroller's
// EnableScrollbar and NotifyScrollChange numbers, ButtonScroller's NotifyScrollChange and EnableScrollButtons,
// over generated inputs (a fixed LCG; small and edge values). Prints one line per failed check; the exit code is
// the number of failed checks.
#include <stdio.h>
#include <limits.h>
#include "rust/ee_won.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 777;
static unsigned int Next() { sSeed = sSeed * 1103515245u + 12345u; return sSeed >> 8; }
static int Small() { return (int)(Next() % 400) - 60; }
static int Pos() { return (int)(Next() % 400); }
static int Big()
{
    static const int kEdges[] = { 0, 1, -1, 65535, 100000, -100000, INT_MAX / 2, INT_MIN / 2 };
    return Next() % 8 == 0 ? kEdges[Next() % 8] : Small();
}

// ---- the original's statements

// Scrollbar::LayoutThumb along the bar's axis: the thumb's visibility, top and size as SetVisible, S_SetPosSize
// and S_SetSize left them
struct Thumb { bool mHidden; bool mSetPos; int mPos, mLen; };
static Thumb OrigLayoutThumb(int mPageSize, int mTotalSize, int mPosition, int aTrackTop, int aScrollHeight,
                             int aThumbTop, int aThumbLen, bool mResizeThumb, int mMinThumbSize, bool changePos)
{
    Thumb t = { false, false, aThumbTop, aThumbLen };
    if(mPageSize >= mTotalSize)
    {
        t.mHidden = true;
        return t;
    }
    int aThumbSize = aThumbLen;
    if(mResizeThumb)
    {
        aThumbSize = aScrollHeight*mPageSize/mTotalSize;
        if(aThumbSize<mMinThumbSize)
            aThumbSize = mMinThumbSize;
    }
    int aThumbPos = aThumbTop;
    if(changePos)
        aThumbPos = aTrackTop+(aScrollHeight-aThumbSize)*mPosition/(mTotalSize-mPageSize);
    if(aThumbPos<aTrackTop)
    {
        aThumbPos = aTrackTop;
        changePos = true;
    }
    if(aThumbPos+aThumbSize >= aTrackTop+aScrollHeight)
    {
        aThumbPos = aTrackTop+aScrollHeight-aThumbSize;
        changePos = true;
    }
    t.mSetPos = changePos;
    t.mPos = changePos ? aThumbPos : aThumbTop;
    t.mLen = aThumbSize;
    return t;
}

// Scrollbar::MouseDrag on the thumb (vertical): the new top and position
static void OrigDrag(int y, int aThumbTop, int mDragY, int aTrackTop, int aTrackLen, int aThumbLen, int mTotalSize,
                     int mPageSize, int& outTop, int& outPosition)
{
    int dy = (y-aThumbTop)-mDragY;
    int aNewY = aThumbTop + dy;
    if(aNewY<aTrackTop)
        aNewY = aTrackTop;
    if(aNewY+aThumbLen > aTrackTop+aTrackLen)
        aNewY = aTrackTop+aTrackLen-aThumbLen;
    int aHeightDiff = aTrackLen - aThumbLen;
    if(aHeightDiff<=0)
        outPosition = 0;
    else
        outPosition = (mTotalSize-mPageSize)*(aNewY-aTrackTop)/aHeightDiff;
    outTop = aNewY;
}

// Scrollbar::HandleComponentEvent's position (0 up arrow, 1 down arrow, 2 track)
static int OrigStep(int which, int aMouseY, int aThumbTop, int aThumbLen, int mPosition, int mLineUpScrollSize,
                    int mLineDownScrollSize, int mPageScrollSize, int mTotalSize, int mPageSize)
{
    if(which==0)
        mPosition-=mLineUpScrollSize;
    else if(which==1)
        mPosition+=mLineDownScrollSize;
    else
    {
        if(aMouseY < aThumbTop)
            mPosition-=mPageScrollSize;
        else if(aMouseY >= aThumbTop+aThumbLen)
            mPosition+=mPageScrollSize;
    }
    int aMaxPos = mTotalSize - mPageSize;
    if(mPosition>aMaxPos)
        mPosition = aMaxPos;
    if(mPosition<0)
        mPosition = 0;
    return mPosition;
}

struct Area {
    int mVertOffset, mHorzOffset, mTotalWidth, mTotalHeight, mExtraHeight;
    int x, y, width, height; // mScrollArea
};
static EeWonScrollArea A(const Area& a)
{
    EeWonScrollArea r = { a.mVertOffset, a.mHorzOffset, a.mTotalWidth, a.mTotalHeight, a.mExtraHeight, a.x, a.y,
                          a.width, a.height };
    return r;
}
static Area RandArea()
{
    Area a = { Small(), Small(), Pos(), Pos(), Next() % 3 == 0 ? Pos() / 4 : 0, Small() / 10, Small() / 10, Pos(),
               Pos() };
    if (Next() % 9 == 0) { a.mVertOffset = Big(); a.mTotalHeight = Big(); a.height = Big(); }
    if (Next() % 7 == 0) a.mVertOffset = a.mTotalHeight - a.height + (int)(Next() % 3) - 1;
    return a;
}

static void OrigAdjust(Area& a)
{
    if(a.mVertOffset+a.height>a.mTotalHeight+a.mExtraHeight)
        a.mVertOffset=a.mTotalHeight+a.mExtraHeight-a.height;
    if(a.mVertOffset<0) a.mVertOffset=0;
    if(a.mHorzOffset+a.width>a.mTotalWidth) a.mHorzOffset=a.mTotalWidth-a.width;
    if(a.mHorzOffset<0) a.mHorzOffset=0;
}
static bool OrigOnBottom(const Area& a)
{
    return a.mVertOffset>0 && a.mVertOffset+a.height>=a.mTotalHeight;
}
static void OrigVirtual08(Area& a, int aWidth, int aHeight, int mLeftPad, int mTopPad, int mRightPad, int mBottomPad)
{
    bool bottom=OrigOnBottom(a);
    a.x=mLeftPad;
    a.y=mTopPad;
    a.width=aWidth-mRightPad-mLeftPad;
    a.height=aHeight-mBottomPad-mTopPad;
    if(a.width<0) a.width=0;
    if(a.height<0) a.height=0;
    if(bottom) a.mVertOffset=a.mTotalHeight+a.mExtraHeight-a.height;
}

// ScrollbarScroller::EnableScrollbar: the visibility the bar is left with, and whether it changed
static bool OrigEnableScrollbar(bool& visibleNow, int condition, bool mSettingScrollbarConditions, bool vertical,
                                const Area& a)
{
    bool visible=visibleNow;
    if(mSettingScrollbarConditions && condition==2 && visible) return false;
    switch(condition) {
    case 0:visibleNow=false;break;
    case 1:visibleNow=true;break;
    case 2:
        visibleNow=vertical ? a.mTotalHeight>a.height : a.mTotalWidth>a.width;
        break;
    }
    return visible!=visibleNow;
}

// ButtonScroller::EnableScrollButtons
static bool OrigEnableButtons(bool& leftVisible, bool& rightVisible, bool leftDisabled, bool rightDisabled,
                              int theCondition, bool mSettingScrollButtonConditions)
{
    bool showLeft = leftVisible, showRight = rightVisible;
    switch(theCondition)
    {
        case 0: showLeft = false; showRight = false; break;
        case 1: showLeft = true; showRight = true; break;
        case 2: showLeft = showRight = !(leftDisabled && rightDisabled); break;
        case 3: showLeft = !leftDisabled; showRight = !rightDisabled; break;
    }
    if(mSettingScrollButtonConditions)
    {
        if(!showLeft && leftVisible)
            return false;
        if(!showRight && rightVisible)
            return false;
    }
    bool changed = showLeft!=leftVisible || showRight!=rightVisible;
    leftVisible = showLeft;
    rightVisible = showRight;
    return changed;
}

// ---- the checks

static void TestThumb()
{
    for (int n = 0; n < 400000; n++) {
        EeWonThumbIn t;
        t.page = Pos();
        t.total = Pos() + (n % 3 == 0 ? 400 : 0);
        if (t.total <= 0) t.total = 1; // a zero total with a negative page divides by zero in both
        if (t.page < 0) t.page = 0;
        t.position = n % 5 == 0 ? Small() : (int)(Next() % (unsigned)(t.total + 1));
        t.track_top = Small() / 10;
        t.track_len = Pos();
        t.thumb_top = Small();
        t.thumb_len = Pos() / 4;
        t.resize_thumb = (n & 1) != 0;
        t.min_thumb = (int)(Next() % 20);
        t.change_pos = (n & 2) != 0;
        if (n % 11 == 0) { t.track_len = Big(); t.position = Big(); }
        Thumb o = OrigLayoutThumb(t.page, t.total, t.position, t.track_top, t.track_len, t.thumb_top, t.thumb_len,
                                  t.resize_thumb, t.min_thumb, t.change_pos);
        EeWonThumbOut r = ee_won_gui_thumb_layout(t);
        CHECK(o.mHidden == r.hidden);
        if (!o.mHidden && !r.hidden) {
            CHECK(o.mSetPos == r.set_pos && o.mLen == r.len);
            if (o.mSetPos && r.set_pos) CHECK(o.mPos == r.pos);
        }

        // MouseDrag: the port's caller passes the thumb's top plus the drag
        int y = Small(), aThumbTop = Small(), aDrag = Small() / 4;
        int ot = 0, op = 0;
        OrigDrag(y, aThumbTop, aDrag, t.track_top, t.track_len, t.thumb_len, t.total, t.page, ot, op);
        int rt = 0;
        int rp = ee_won_gui_thumb_drag(aThumbTop + ((y - aThumbTop) - aDrag), t.track_top, t.track_len, t.thumb_len,
                                       t.total, t.page, &rt);
        CHECK(ot == rt && op == rp);

        // the steps
        int which = (int)(Next() % 3);
        int aMouse = Small(), aLen = Pos() / 4, aPos = Small(), up = Pos() / 8, down = Pos() / 8, page = Pos() / 2;
        int o2 = OrigStep(which, aMouse, aThumbTop, aLen, aPos, up, down, page, t.total, t.page);
        int r2 = ee_won_gui_scroll_step(which == 0 ? EeWonScrollPress::LineUp
                                        : which == 1 ? EeWonScrollPress::LineDown : EeWonScrollPress::Track,
                                        aMouse, aThumbTop, aLen, aPos, up, down, page, t.total, t.page);
        CHECK(o2 == r2);
    }
}

static void TestArea()
{
    for (int n = 0; n < 400000; n++) {
        Area a = RandArea();
        Area o = a;
        OrigAdjust(o);
        EeWonScrollArea r = A(a);
        ee_won_gui_scroll_clamp(&r);
        CHECK(o.mVertOffset == r.vert_offset && o.mHorzOffset == r.horz_offset);
        CHECK(OrigOnBottom(a) == ee_won_gui_scroll_on_bottom(A(a)));

        int w = Pos(), h = Pos(), lp = Small() / 20, tp = Small() / 20, rp = Small() / 20, bp = Small() / 20;
        if (n % 13 == 0) { w = Big(); h = Big(); }
        Area v = a;
        OrigVirtual08(v, w, h, lp, tp, rp, bp);
        EeWonScrollArea rv = A(a);
        ee_won_gui_scroll_view(&rv, w, h, lp, tp, rp, bp);
        CHECK(v.x == rv.view_x && v.y == rv.view_y && v.width == rv.view_width && v.height == rv.view_height &&
              v.mVertOffset == rv.vert_offset && v.mHorzOffset == rv.horz_offset);

        // GetDesiredSize and SetSizeEqualToTotal
        int dw = 0, dh = 0;
        ee_won_gui_scroll_size(0, A(a), 0, 0, 0, 0, 0, 0, &dw, &dh);
        CHECK(dw == (a.mTotalWidth < 300 ? a.mTotalWidth : 300) && dh == (a.mTotalHeight < 300 ? a.mTotalHeight : 300));
        int mw = Small(), mh = Small();
        int width = a.mTotalWidth, height = a.mTotalHeight;
        if(mw>width) width=mw;
        if(mh>height) height=mh;
        ee_won_gui_scroll_size(1, A(a), mw, mh, lp, tp, rp, bp, &dw, &dh);
        CHECK(dw == width+lp+rp && dh == height+tp+bp);

        // EnableScrollbar
        for (int c = 0; c < 4; c++)
            for (int s = 0; s < 8; s++) {
                bool setting = (s & 1) != 0, visible = (s & 2) != 0, vertical = (s & 4) != 0;
                bool ov = visible;
                bool oc = OrigEnableScrollbar(ov, c, setting, vertical, a);
                int k = ee_won_gui_scrollbar_condition(c, setting, visible, vertical, A(a));
                bool rv2 = visible;
                if (k == 0 || k == 1) rv2 = k == 1;
                bool rc = k >= 0 && rv2 != visible;
                CHECK(oc == rc && ov == rv2);
            }

        // NotifyScrollChange's numbers
        int tt = 0, pp = 0;
        bool e = ee_won_gui_scrollbar_sync(A(a), true, &tt, &pp);
        CHECK(tt == a.mTotalHeight + a.mExtraHeight && pp == a.height && e == (a.mTotalHeight + a.mExtraHeight > a.height));
        e = ee_won_gui_scrollbar_sync(A(a), false, &tt, &pp);
        CHECK(tt == a.mTotalWidth && pp == a.width && e == (a.mTotalWidth > a.width));

        // ButtonScroller
        unsigned int b = ee_won_gui_scroll_buttons(A(a));
        CHECK(((b & 1) != 0) == (a.mHorzOffset>0));
        CHECK(((b & 2) != 0) == (a.mVertOffset>0));
        CHECK(((b & 4) != 0) == (a.mHorzOffset + a.width < a.mTotalWidth));
        CHECK(((b & 8) != 0) == (a.mVertOffset + a.height < (a.mTotalHeight+a.mExtraHeight)));
    }
    for (int c = 0; c < 5; c++)
        for (int s = 0; s < 32; s++) {
            bool lv = (s & 1) != 0, rv = (s & 2) != 0, ld = (s & 4) != 0, rd = (s & 8) != 0, set = (s & 16) != 0;
            bool olv = lv, orv = rv;
            bool oc = OrigEnableButtons(olv, orv, ld, rd, c, set);
            int k = ee_won_gui_scroll_button_condition(c, set, lv, rv, ld, rd);
            bool rlv = lv, rrv = rv;
            if (k >= 0) { rlv = (k & 1) != 0; rrv = (k & 2) != 0; }
            bool rc = k >= 0 && (rlv != lv || rrv != rv);
            CHECK(oc == rc && olv == rlv && orv == rrv);
        }
}

int main()
{
    TestThumb();
    TestArea();
    printf("won_guiscroll_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
