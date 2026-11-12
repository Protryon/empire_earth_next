// WONGUI's layout arithmetic: the original's statements (copied from the C++ before the port, with the components
// replaced by the bounds they read and the SetPos / SetSize calls by the values they received) against ee_won's
// guilayout module through its C ABI: WONRectangle's DoIntersect, DoUnion and Intersects, Component::SetSize and
// SetSizeAtLeast, ChildLayoutInstruction::Execute over every flag set of up to three flags and all 20 single ones,
// CenterLayout, SameSizeLayout, ClearRightLayout, SpaceEvenlyLayout, NoOverlapLayout, SizeAtLeastLayout,
// PercentLayoutParam's SetBorders and DoCalc, VerticalFlowLayout, and ContainerConfig::HandleLayout's instruction
// names and offsets, over generated inputs (a fixed LCG; small and edge values, the ints' ends among them). Prints
// one line per failed check; the exit code is the number of failed checks.
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <list>
#include <string>
#include <vector>
#include "rust/ee_won.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 12345;
static unsigned int Next() { sSeed = sSeed * 1103515245u + 12345u; return sSeed >> 8; }
// a coordinate: mostly small (both signs), sometimes an edge value
static int Coord()
{
    static const int kEdges[] = { 0, 1, -1, 2, 100, -100, 640, 480, INT_MAX, INT_MIN, INT_MAX - 1, INT_MIN + 1 };
    unsigned int k = Next() % 16;
    if (k < 2) return kEdges[Next() % (sizeof(kEdges) / sizeof(kEdges[0]))];
    if (k < 4) return (int)(Next() % 7) - 3;
    return (int)(Next() % 1400) - 400;
}
static int Small() { return (int)(Next() % 300) - 50; }

struct Rect { int x, y, width, height; };
static EeWonRect R(const Rect& r) { EeWonRect e = { r.x, r.y, r.width, r.height }; return e; }
static bool Same(const Rect& a, const EeWonRect& b) { return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height; }
static Rect RandRect(bool small)
{
    Rect r;
    if (small) { r.x = Small(); r.y = Small(); r.width = Small(); r.height = Small(); }
    else { r.x = Coord(); r.y = Coord(); r.width = Coord(); r.height = Coord(); }
    return r;
}

// ---- the original's statements

// WONRectangle (Rectangle.cpp)
static void OrigIntersect(Rect& self, const Rect& other)
{
    int right=self.x+self.width;
    int bottom=self.y+self.height;
    int otherRight=other.x+other.width;
    int otherBottom=other.y+other.height;
    if(other.x>self.x) self.x=other.x;
    if(other.y>self.y) self.y=other.y;
    if(otherRight<right) right=otherRight;
    if(otherBottom<bottom) bottom=otherBottom;
    self.width=right-self.x;
    self.height=bottom-self.y;
    if(self.width<0) self.width=0;
    if(self.height<0) self.height=0;
}
static void OrigUnion(Rect& self, const Rect& other)
{
    if(self.width==0 || self.height==0)
    {
        self.x=other.x; self.y=other.y; self.width=other.width; self.height=other.height;
        return;
    }
    int right=self.x+self.width;
    int bottom=self.y+self.height;
    int otherRight=other.x+other.width;
    int otherBottom=other.y+other.height;
    if(other.x<self.x) self.x=other.x;
    if(other.y<self.y) self.y=other.y;
    if(otherRight>right) right=otherRight;
    if(otherBottom>bottom) bottom=otherBottom;
    self.width=right-self.x;
    self.height=bottom-self.y;
}
static bool OrigIntersects(const Rect& self, const Rect& other)
{
    return self.x+self.width>=other.x && self.x<other.x+other.width &&
           self.y+self.height>=other.y && self.y<other.y+other.height;
}

// Component::SetSize / SetSizeAtLeast (Component.cpp): whether Virtual08 ran, and the size it left
static bool OrigSetSize(Rect& b, int width, int height)
{
    if(width < 0) width = 0;
    if(height < 0) height = 0;
    if(width == b.width && height == b.height)
        return false;
    b.height = height;
    b.width = width;
    return true;
}
static bool OrigSetSizeAtLeast(Rect& b, int width, int height)
{
    if(width < b.width) width = b.width;
    if(height < b.height) height = b.height;
    if(width > b.width || height > b.height)
    {
        OrigSetSize(b, width, height);
        return true; // SetSize called
    }
    return false;
}

// ChildLayoutInstruction::Execute (ChildLayouts.cpp)
static Rect OrigChildLayout(const Rect& child, const Rect& reference, bool refIsParent, int mFlags, int mX, int mY,
                            int mWidth, int mHeight)
{
    int left, top;
    if(refIsParent) { left = 0; top = 0; }
    else { left = reference.x; top = reference.y; }
    int referenceWidth = reference.width;
    int referenceHeight = reference.height;
    int right = left + referenceWidth;
    int bottom = top + referenceHeight;
    int x = child.x, y = child.y, width = child.width, height = child.height;
    for(int flag = 1; flag < 0x100000; flag <<= 1)
    {
        if((mFlags & flag) == 0)
            continue;
        switch(flag)
        {
        case 0x00001: width = mWidth + referenceWidth; break;
        case 0x00002: height = mHeight + referenceHeight; break;
        case 0x00100: y = mY - height + top; break;
        case 0x00200: y = mY + bottom; break;
        case 0x00400: x = mX + right; break;
        case 0x00800: x = mX - width + left; break;
        case 0x01000: x = mX + left; break;
        case 0x02000: x = mX - width + right; break;
        case 0x04000: y = mY + top; break;
        case 0x08000: y = mY - height + bottom; break;
        case 0x10000: width = mWidth - x + right; break;
        case 0x20000: width = mWidth - x + left; break;
        case 0x40000: height = mHeight - y + top; break;
        case 0x80000: height = mHeight - y + bottom; break;
        }
    }
    Rect r = { x, y, width, height };
    return r;
}

// CenterLayout::Execute (CenterLayoutExecute.cpp): the components' bounds moved in place; true when it moved them
static bool OrigCenter(std::vector<Rect>& list, const Rect& parent, const Rect* box, bool mHorz, bool mVert)
{
    if(list.empty())
        return false;
    Rect rectangle = { 0, 0, 0, 0 };
    for(size_t i = 0; i < list.size(); i++)
        OrigUnion(rectangle, list[i]);
    int dx=0,dy=0;
    const Rect* center=&parent;
    if(box!=NULL)
    {
        center=box;
        if(mHorz) dx=center->x;
        if(mVert) dy=center->y;
    }
    if(mHorz) dx+=(center->width-rectangle.width)/2-rectangle.x;
    if(mVert) dy+=(center->height-rectangle.height)/2-rectangle.y;
    if(dx==0 && dy==0)
        return false;
    for(size_t i = 0; i < list.size(); i++)
    {
        list[i].x += dx;
        list[i].y += dy;
    }
    return true;
}

// SameSizeLayout::Execute: the sizes each SetSize received
static void OrigSameSize(std::vector<Rect>& list, bool mHorz, bool mVert)
{
    int width=0,height=0;
    for(size_t i = 0; i < list.size(); i++)
    {
        if(mHorz && list[i].width>width) width=list[i].width;
        if(mVert && list[i].height>height) height=list[i].height;
    }
    for(size_t i = 0; i < list.size(); i++)
    {
        if(mHorz && mVert) { list[i].width = width; list[i].height = height; }
        else if(mHorz) list[i].width = width;
        else if(mVert) list[i].height = height;
    }
}

// ClearRightLayout::Execute: the x SetPos received
static int OrigClearRight(const std::vector<Rect>& list, int mPad)
{
    int right=0;
    for(size_t i = 0; i < list.size(); i++)
    {
        int r=list[i].x+list[i].width;
        if(r>right) right=r;
    }
    return right+mPad;
}

// SpaceEvenlyLayout::Execute: the positions SetPos received (the list's size_t as the original's 32-bit one)
static void OrigSpaceEvenly(std::vector<Rect>& list, const Rect& box, bool boxIsParent, bool mIsHorz, int mLeftPad,
                            int mRightPad)
{
    if(list.empty()) return;
    int total=0;
    for(size_t i = 0; i < list.size(); i++)
        total+=mIsHorz?list[i].width:list[i].height;
    int available=(mIsHorz?box.width:box.height)-mRightPad-mLeftPad-total;
    if(available<0) available=0;
    int spacing=0,remainder=0;
    unsigned int aSize = (unsigned int)list.size();
    if(aSize>1)
    {
        spacing=available/(aSize-1);
        remainder=available%(aSize-1);
    }
    int position=mIsHorz?box.x:box.y;
    if(boxIsParent) position=0;
    position+=mLeftPad;
    for(size_t i = 0; i < list.size(); i++)
    {
        Rect& child=list[i];
        if(mIsHorz) child.x = position;
        else child.y = position;
        if(mIsHorz) position+=child.width+spacing;
        else position+=child.height+spacing;
        if(remainder>0) { ++position; --remainder; }
    }
}

// NoOverlapLayout::Execute: (dx, dy), or false for no move
static bool OrigNoOverlap(const Rect& target, const Rect& ref, bool refIsParent, int mType, int mHorzPad, int mVertPad,
                          int& outDx, int& outDy)
{
    int dx=0,dy=0;
    if(refIsParent)
    {
        if(mType&0x800) { dx=mHorzPad-target.x; if(dx<0) dx=0; }
        else if(mType&0x400) { const Rect& b=target; dx=ref.width-b.width+mHorzPad-b.x; if(dx>0) dx=0; }
        if(mType&0x100) { dy=mVertPad-target.y; if(dy<0) dy=0; }
        else if(mType&0x200) { const Rect& b=target; dy=ref.height-b.height-b.y+mVertPad; if(dy>0) dy=0; }
    }
    else
    {
        if(mType&0x800) { const Rect& b=target; dx=ref.x-b.width+mHorzPad-b.x; if(dx>0) dx=0; }
        else if(mType&0x400) { dx=ref.x+ref.width-target.x+mHorzPad; if(dx<0) dx=0; }
        if(mType&0x100) { const Rect& b=target; dy=ref.y-b.height-b.y+mVertPad; if(dy>0) dy=0; }
        else if(mType&0x200) { dy=ref.height-target.y+ref.y+mVertPad; if(dy<0) dy=0; }
    }
    if(dx==0 && dy==0) return false;
    outDx = dx; outDy = dy;
    return true;
}

// SizeAtLeastLayout::Execute: the size SetSizeAtLeast received
static void OrigSizeAtLeastLayout(const std::vector<Rect>& list, int mHorzPad, int mVertPad, int& outW, int& outH)
{
    int width=0,height=0;
    for(size_t i = 0; i < list.size(); i++)
    {
        if(list[i].width>width) width=list[i].width;
        if(list[i].height>height) height=list[i].height;
    }
    outW = width+mHorzPad;
    outH = height+mVertPad;
}

// PercentLayoutParam::SetBorders over four distinct anchors (NULL when not given): the slot each edge took
static void OrigSetBorders(const int* inLeft, const int* inTop, const int* inRight, const int* inBottom,
                           const int*& left, const int*& top, const int*& right, const int*& bottom)
{
    left=inLeft; top=inTop; right=inRight; bottom=inBottom;
    const int* last=NULL;
    if(inLeft==NULL) left=inRight; else last=inLeft;
    if(inRight==NULL) right=left; else last=inRight;
    if(inTop==NULL) top=inBottom; else last=inTop;
    if(inBottom==NULL) bottom=top; else last=inBottom;
    if(left==NULL) left=last;
    if(right==NULL) right=last;
    if(top==NULL) top=last;
    if(bottom==NULL) bottom=last;
}

// PercentLayoutParam::DoCalc with the anchors' edges read
static Rect OrigPercent(const Rect& target, int leftEdge, int rightEdge, int topEdge, int bottomEdge, int px, int py,
                        int pw, int ph, int lp, int tp, int rp, int bp)
{
    int x=target.x, y=target.y, width=target.width, height=target.height;
    if(px>=0 || pw>=0)
    {
        leftEdge+=lp;
        rightEdge-=rp;
        if(px>=0) x=leftEdge+(rightEdge-leftEdge)*px/100;
        if(pw>=0) width=(rightEdge-leftEdge)*pw/100;
    }
    if(py>=0 || ph>=0)
    {
        topEdge+=tp;
        bottomEdge-=bp;
        if(py>=0) y=topEdge+(bottomEdge-topEdge)*py/100;
        if(ph>=0) height=(bottomEdge-topEdge)*ph/100;
    }
    Rect r = { x, y, width, height };
    return r;
}

// VerticalFlowLayout::Execute from its start: the positions SetPos received
struct FlowItem { int mTopPad, mLeftPad; Rect mBounds; };
static void OrigVerticalFlow(std::vector<FlowItem>& list, int aLeft, int aTop)
{
    for(size_t i = 0; i < list.size(); i++)
    {
        aTop += list[i].mTopPad;
        int aLeftPad = list[i].mLeftPad;
        if(aLeftPad>=0)
            list[i].mBounds.x = aLeft+aLeftPad;
        list[i].mBounds.y = aTop;
        aTop += list[i].mBounds.height;
    }
}

// ---- the checks

static void TestRectangles()
{
    for (int n = 0; n < 200000; n++) {
        Rect a = RandRect(n % 2 == 0), b = RandRect(n % 3 == 0);
        if (n % 5 == 0) a.width = 0;
        if (n % 7 == 0) b.height = 0;
        Rect i = a; OrigIntersect(i, b);
        CHECK(Same(i, ee_won_gui_rect_intersect(R(a), R(b))));
        Rect u = a; OrigUnion(u, b);
        CHECK(Same(u, ee_won_gui_rect_union(R(a), R(b))));
        CHECK(OrigIntersects(a, b) == ee_won_gui_rect_intersects(R(a), R(b)));
        int w = Coord(), h = Coord(), ow = -7, oh = -7;
        Rect s = a;
        bool aChanged = OrigSetSize(s, w, h);
        bool aRust = ee_won_gui_set_size(a.width, a.height, w, h, &ow, &oh);
        CHECK(aChanged == aRust);
        if (aChanged && aRust) CHECK(ow == s.width && oh == s.height);
        s = a;
        aChanged = OrigSetSizeAtLeast(s, w, h);
        aRust = ee_won_gui_size_at_least(a.width, a.height, w, h, &ow, &oh);
        CHECK(aChanged == aRust);
        // the port calls SetSize with the size it answered
        if (aRust) { Rect t = a; OrigSetSize(t, ow, oh); CHECK(t.width == s.width && t.height == s.height); }
    }
}

static const int kFlags[] = { 0x1, 0x2, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000, 0x10000, 0x20000,
                              0x40000, 0x80000, 0x4, 0x8, 0x80, 0x100000, 0x200000, (int)0x80000000 };

static void TestChildLayout()
{
    const int kN = sizeof(kFlags) / sizeof(kFlags[0]);
    std::vector<int> aSets;
    aSets.push_back(0);
    for (int i = 0; i < kN; i++) {
        aSets.push_back(kFlags[i]);
        for (int j = i + 1; j < kN; j++) {
            aSets.push_back(kFlags[i] | kFlags[j]);
            for (int k = j + 1; k < kN; k++)
                aSets.push_back(kFlags[i] | kFlags[j] | kFlags[k]);
        }
    }
    for (size_t s = 0; s < aSets.size(); s++)
        for (int n = 0; n < 24; n++) {
            Rect child = RandRect(n % 3 != 0), ref = RandRect(n % 4 != 0);
            bool aParent = (n & 1) != 0;
            int mx = Small(), my = Small(), mw = Small(), mh = Small();
            if (n == 23) { mx = Coord(); my = Coord(); mw = Coord(); mh = Coord(); }
            Rect o = OrigChildLayout(child, ref, aParent, aSets[s], mx, my, mw, mh);
            EeWonChildLayout l = { aSets[s], mx, my, mw, mh };
            CHECK(Same(o, ee_won_gui_child_layout(R(child), R(ref), aParent, l)));
        }
    // the lobby's flag sets: any combination of one x rule, one y rule and the size rules
    for (int n = 0; n < 100000; n++) {
        int f = (int)(Next() & 0xff3ff) & ~0xfc;
        Rect child = RandRect(true), ref = RandRect(true);
        int mx = Small(), my = Small(), mw = Small(), mh = Small();
        bool aParent = (n % 3) == 0;
        Rect o = OrigChildLayout(child, ref, aParent, f, mx, my, mw, mh);
        EeWonChildLayout l = { f, mx, my, mw, mh };
        CHECK(Same(o, ee_won_gui_child_layout(R(child), R(ref), aParent, l)));
    }
}

static void TestLists()
{
    for (int n = 0; n < 60000; n++) {
        int aCount = (int)(Next() % 6);
        if (n % 50 == 0) aCount = (int)(Next() % 40);
        bool aSmall = n % 9 != 0;
        std::vector<Rect> aList;
        for (int i = 0; i < aCount; i++) aList.push_back(RandRect(aSmall));
        if (aCount > 0 && n % 4 == 0) aList[0].width = 0;
        std::vector<EeWonRect> aBounds;
        std::vector<int> aSizes;
        for (int i = 0; i < aCount; i++) {
            aBounds.push_back(R(aList[i]));
            aSizes.push_back(aList[i].width);
            aSizes.push_back(aList[i].height);
        }
        bool aHorz = (n & 1) != 0, aVert = (n & 2) != 0;
        if ((n & 3) == 0) aHorz = aVert = true;
        Rect aParent = RandRect(aSmall), aBox = RandRect(aSmall);
        bool aUseBox = (n % 3) == 0;

        // CenterLayout
        std::vector<Rect> c = aList;
        bool aMoved = OrigCenter(c, aParent, aUseBox ? &aBox : NULL, aHorz, aVert);
        int dx = 0, dy = 0;
        bool aRust = ee_won_gui_center_shift(aBounds.empty() ? NULL : &aBounds[0], aBounds.size(),
                                             R(aUseBox ? aBox : aParent), aUseBox, aHorz, aVert, &dx, &dy);
        CHECK(aMoved == aRust);
        if (aMoved && aRust)
            for (int i = 0; i < aCount; i++) CHECK(c[i].x == aList[i].x + dx && c[i].y == aList[i].y + dy);

        // SameSizeLayout: each component's SetSize as the port's caller makes it
        std::vector<Rect> s = aList;
        OrigSameSize(s, aHorz, aVert);
        int w = 0, h = 0;
        ee_won_gui_max_size(0, aSizes.empty() ? NULL : &aSizes[0], aCount, aHorz, aVert, 0, 0, &w, &h);
        for (int i = 0; i < aCount; i++) {
            Rect t = aList[i];
            if (aHorz && aVert) { t.width = w; t.height = h; }
            else if (aHorz) t.width = w;
            else if (aVert) t.height = h;
            CHECK(t.width == s[i].width && t.height == s[i].height);
        }

        // ClearRightLayout
        std::vector<int> aRights;
        for (int i = 0; i < aCount; i++) aRights.push_back(aList[i].x + aList[i].width);
        int aPad = Small();
        CHECK(OrigClearRight(aList, aPad) == ee_won_gui_clear_right(aRights.empty() ? NULL : &aRights[0], aCount, aPad));

        // SpaceEvenlyLayout
        int aLeftPad = Small() / 4, aRightPad = Small() / 4;
        bool aBoxIsParent = (n % 5) == 0;
        std::vector<Rect> e = aList;
        OrigSpaceEvenly(e, aBox, aBoxIsParent, aHorz, aLeftPad, aRightPad);
        std::vector<int> anExtents, aPos(aCount + 1, -12345);
        for (int i = 0; i < aCount; i++) anExtents.push_back(aHorz ? aList[i].width : aList[i].height);
        ee_won_gui_space_evenly(anExtents.empty() ? NULL : &anExtents[0], aCount, aHorz ? aBox.x : aBox.y,
                                aHorz ? aBox.width : aBox.height, aBoxIsParent, aLeftPad, aRightPad, &aPos[0]);
        for (int i = 0; i < aCount; i++) CHECK((aHorz ? e[i].x : e[i].y) == aPos[i]);
        CHECK(aPos[aCount] == -12345);

        // SizeAtLeastLayout
        int ow = 0, oh = 0, rw = 0, rh = 0;
        int hp = Small(), vp = Small();
        OrigSizeAtLeastLayout(aList, hp, vp, ow, oh);
        ee_won_gui_max_size(1, aSizes.empty() ? NULL : &aSizes[0], aCount, false, false, hp, vp, &rw, &rh);
        CHECK(ow == rw && oh == rh);
    }
}

static void TestNoOverlap()
{
    for (int n = 0; n < 200000; n++) {
        Rect t = RandRect(n % 5 != 0), r = RandRect(n % 6 != 0);
        bool aParent = (n & 1) != 0;
        int aType = (int)(Next() & 0xf00);
        if (n % 11 == 0) aType |= (int)(Next() & 0xff);
        int hp = Small() / 3, vp = Small() / 3;
        int odx = -9, ody = -9, rdx = -9, rdy = -9;
        bool o = OrigNoOverlap(t, r, aParent, aType, hp, vp, odx, ody);
        bool k = ee_won_gui_no_overlap(R(t), R(r), aParent, aType, hp, vp, &rdx, &rdy);
        CHECK(o == k);
        if (o && k) CHECK(odx == rdx && ody == rdy);
    }
}

static void TestPercent()
{
    int anAnchors[4];
    for (int g = 0; g < 16; g++) {
        const int* in[4];
        for (int i = 0; i < 4; i++) in[i] = (g >> i) & 1 ? &anAnchors[i] : NULL;
        const int *l, *t, *r, *b;
        OrigSetBorders(in[0], in[1], in[2], in[3], l, t, r, b);
        int out[4];
        ee_won_gui_percent_borders((uint32_t)g, out);
        const int* got[4];
        for (int i = 0; i < 4; i++) got[i] = out[i] < 0 ? NULL : &anAnchors[out[i]];
        CHECK(got[0] == l && got[1] == t && got[2] == r && got[3] == b);
    }
    for (int n = 0; n < 200000; n++) {
        Rect aTarget = RandRect(n % 4 != 0);
        int e[4] = { Small(), Small(), Small(), Small() };
        if (n % 13 == 0) for (int i = 0; i < 4; i++) e[i] = Coord();
        int p[8];
        for (int i = 0; i < 4; i++) p[i] = (int)(Next() % 160) - 30;
        for (int i = 4; i < 8; i++) p[i] = (int)(Next() % 25) - 1;
        Rect o = OrigPercent(aTarget, e[0], e[1], e[2], e[3], p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
        EeWonPercent pp = { p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7] };
        CHECK(Same(o, ee_won_gui_percent_layout(R(aTarget), e, pp)));
    }
}

static void TestFlow()
{
    for (int n = 0; n < 50000; n++) {
        int aCount = (int)(Next() % 7);
        std::vector<FlowItem> aList;
        std::vector<EeWonFlowItem> anItems;
        for (int i = 0; i < aCount; i++) {
            FlowItem f = { Small() / 5, (int)(Next() % 30) - 10, RandRect(true) };
            if (n % 17 == 0) f.mBounds.height = Coord();
            aList.push_back(f);
            EeWonFlowItem it = { f.mTopPad, f.mLeftPad, f.mBounds.x, f.mBounds.height };
            anItems.push_back(it);
        }
        int aLeft = Small(), aTop = Small();
        OrigVerticalFlow(aList, aLeft, aTop);
        std::vector<int> aPos(2 * aCount + 1, -5);
        ee_won_gui_vertical_flow(anItems.empty() ? NULL : &anItems[0], aCount, aLeft, aTop, &aPos[0]);
        for (int i = 0; i < aCount; i++) CHECK(aList[i].mBounds.x == aPos[2 * i] && aList[i].mBounds.y == aPos[2 * i + 1]);
    }
}

static void TestLayoutRule()
{
    static const char* kNames[] = { "SameWidth", "SameHeight", "Above", "Below", "Right", "Left", "SameLeft", "SameRight",
                                    "SameTop", "SameBottom", "GrowToRight", "GrowToLeft", "GrowToTop", "GrowToBottom",
                                    "SameSize" };
    static const int kBits[] = { 0x1, 0x2, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000, 0x10000,
                                 0x20000, 0x40000, 0x80000, 0x3 };
    for (int i = 0; i < 16; i++) {
        const uint8_t* aName = NULL;
        int aFlag = 0;
        bool ok = ee_won_gui_layout_keyword(i, &aName, &aFlag);
        CHECK(ok == (i < 15));
        if (ok && i < 15) CHECK(strcmp((const char*)aName, kNames[i]) == 0 && aFlag == kBits[i]);
    }
    CHECK(!ee_won_gui_layout_keyword(-1, NULL, NULL));
    for (int c = 0; c < 2; c++)
        for (int r = 0; r < 2; r++)
            for (int i = 0; i <= 4; i++)
                CHECK(ee_won_gui_layout_zero_rest(c != 0, r != 0, i) == ((!c && !r) || i != 0));
}

int main()
{
    TestRectangles();
    TestChildLayout();
    TestLists();
    TestNoOverlap();
    TestPercent();
    TestFlow();
    TestLayoutRule();
    printf("won_guilayout_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
