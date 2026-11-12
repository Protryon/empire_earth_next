// WONGUI's rich text area (TextArea.cpp, TextSegments.cpp, TextFormat.cpp, TextAreaInstructions.cpp): the
// original's statements (copied from the C++ before the port, the lines and segments replaced by plain structs,
// the font by a width per unit, the area's calls by a trace) against ee_won's guitext module (and the guilist
// functions the area shares) through their C ABI, over generated inputs (a fixed LCG). Prints one line per failed
// check; the exit code is the number of failed checks.
#include <stdio.h>
#include <ctype.h>
#include <deque>
#include <list>
#include <string>
#include <vector>
#include <algorithm>
#include "rust/ee_won.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 31415;
static unsigned int Next() { sSeed = sSeed * 1103515245u + 12345u; return sSeed >> 8; }
static int Small() { return (int)(Next() % 41) - 10; }
static int Any()
{
    switch (Next() % 8) {
    case 0: return (int)Next() - (int)(Next() << 8);
    case 1: return 0x7fffffff - (int)(Next() % 3);
    case 2: return (int)0x80000000 + (int)(Next() % 3);
    default: return (int)(Next() % 300) - 50;
    }
}

struct Line { int mx, my, mWidth, mHeight, mBaseline, mDescent; int mSegs; };

// ---- AddSeg's metrics, AlignLine, AddSegmentPrv's test

static bool OrigAddSeg(Line* line, int segHeight, int segAscent, int& mTotalHeight)
{
    bool adjust=false;
    int baseline=segAscent;
    if(baseline>line->mBaseline) { line->mBaseline=baseline;adjust=true; }
    int descent=segHeight-segAscent;
    if(descent>line->mDescent) line->mDescent=descent;
    if(segHeight>line->mHeight) {
        mTotalHeight-=line->mHeight;
        line->mHeight=segHeight;
        mTotalHeight+=line->mHeight;
        adjust=true;
    }
    if(line->mDescent+line->mBaseline>line->mHeight) {
        mTotalHeight-=line->mHeight;
        line->mHeight=line->mBaseline+line->mDescent;
        mTotalHeight+=line->mHeight;
        adjust=true;
    }
    return adjust;
}

static int OrigAlign(int width, int lineWidth, int align)
{
    int x=0;
    switch(align) {
    case 1:x=(width-lineWidth)/2;break;
    case 2:x=width-lineWidth;break;
    }
    if(x<=0) x=0;
    return x;
}

static void Metrics()
{
    for (int n = 0; n < 300000; n++) {
        Line l = { 0, 0, 0, Small(), Small(), Small(), 0 };
        if (n % 7 == 0) { l.mHeight = Any(); l.mBaseline = Any(); l.mDescent = Any(); }
        int h = n % 5 == 0 ? Any() : (int)(Next() % 30), a = n % 5 == 1 ? Any() : (int)(Next() % 30) - 3;
        int total = Small() * 5;
        int rTotal = total;
        EeWonTextLineM m = { l.mHeight, l.mBaseline, l.mDescent };
        bool o = OrigAddSeg(&l, h, a, total);
        bool r = ee_won_gui_text_add_seg(&m, h, a, &rTotal);
        CHECK(o == r && total == rTotal && m.height == l.mHeight && m.baseline == l.mBaseline && m.descent == l.mDescent);

        int vw = n % 9 == 0 ? Any() : (int)(Next() % 200), lw = n % 11 == 0 ? Any() : (int)(Next() % 250);
        int al = (int)(Next() % 4);
        CHECK(OrigAlign(vw, lw, al) == ee_won_gui_text_align_x(vw, lw, al));

        int areaW = Small() * 10, segW = Any();
        bool wrap = (n & 1) != 0, vis = (n & 2) != 0;
        bool fits = areaW<=0 || lw+segW<=vw || !wrap || !vis;
        CHECK(fits == ee_won_gui_text_seg_fits(areaW, lw, segW, vw, wrap, vis));
    }
}

// ---- FindLineAt, IsLineVisible, ShouldScrollOnBottom, AdjustPosition's extra, the delayed wrap

struct LineYLess { bool operator()(const Line& a, const Line& b) const { return a.my < b.my; } };
static int32_t DequeTop(const void* p, uintptr_t i) { return (*(const std::deque<Line>*)p)[i].my; }

static bool OrigVisible(const Line* line, int flags, int mVertOffset, int height)
{
    if(line->my+line->mHeight<mVertOffset) return false;
    int y=line->my;
    if(!(flags&0x08)) y+=line->mHeight;
    return y<=mVertOffset+height;
}

static void Lines()
{
    for (int n = 0; n < 200000; n++) {
        std::deque<Line> lines;
        int count = (int)(Next() % 12);
        int y = Small();
        for (int i = 0; i < count; i++) {
            Line l = { 0, y, 0, (int)(Next() % 20), 0, 0, 0 };
            if (n % 17 == 0) l.mHeight = Any();
            lines.push_back(l);
            y += Next() % 4 == 0 ? 0 : l.mHeight;
            if (n % 13 == 0) y = Any();
        }
        int at = n % 3 == 0 ? Any() : Small() * 4;
        Line search = { 0, at, 0, 0, 0, 0, 0 };
        std::deque<Line>::iterator itr = std::lower_bound(lines.begin(), lines.end(), search, LineYLess());
        if(itr==lines.end() || itr->my>at) {
            if(itr!=lines.begin()) --itr;
        }
        CHECK((size_t)(itr - lines.begin()) == ee_won_gui_list_line_at(lines.size(), at, DequeTop, &lines));

        int flags = (int)(Next() % 64), offset = n % 5 == 0 ? Any() : Small() * 5, vh = (int)(Next() % 120);
        bool user = (Next() & 1) != 0;
        for (size_t i = 0; i < lines.size(); i++)
            CHECK(OrigVisible(&lines[i], flags, offset, vh) ==
                  ee_won_gui_list_visible(lines[i].my, lines[i].mHeight, offset, vh, (flags & 0x08) != 0));
        // ShouldScrollOnBottom
        bool o;
        if(!(flags&0x04)) o=false;
        else if(!user) o=true;
        else {
            o=false;
            if(!lines.empty()) {
                const Line* line=&lines.back();
                if(line->mHeight==0 && lines.size()>1) line=&lines[lines.size()-2];
                if(OrigVisible(line,flags,offset,vh)) o=true;
            }
        }
        EeWonTextLineY last = { 0, 0 }, prev = { 0, 0 };
        if (!lines.empty()) { last.y = lines.back().my; last.height = lines.back().mHeight; }
        if (lines.size() > 1) { prev.y = lines[lines.size() - 2].my; prev.height = lines[lines.size() - 2].mHeight; }
        CHECK(o == ee_won_gui_text_scroll_on_bottom(flags, user, lines.size(), last, prev, offset, vh));

        // AdjustPosition's extra height
        int total = n % 7 == 0 ? Any() : (int)(Next() % 200);
        if (!lines.empty()) {
            const Line& l = lines[Next() % lines.size()];
            int extra=l.my+l.mHeight+vh-total;
            if(extra<0) extra=0;
            CHECK(extra == ee_won_gui_list_full_line_extra(total, vh, l.my, l.mHeight));
        }

        // Virtual08's wait and TimerEvent's due tick
        int chars = n % 4 == 0 ? Any() : (int)(Next() % 400000);
        bool wait = (flags&0x20) && chars>5000;
        CHECK(wait == ee_won_gui_text_delay_wrap(flags, chars));
        int ticks = (int)(Next() % 14);
        bool due = false;
        if(ticks>1) { if(ticks>=10 || ticks>=chars/16000) due=true; }
        CHECK(due == ee_won_gui_text_wrap_due(ticks, chars));
    }
}

// ---- the limits, RemoveSegmentsUpTo's search, EnforceInvariants

struct Seg { int mAbsStartChar, mAbsEndChar; bool mLineBreak; int mx, mWidth; Seg* mActual; };
struct FLine { int mx, my, mWidth, mHeight; std::list<Seg*> mSegmentList; };
struct FlagCtx { std::deque<FLine*>* lines; FLine* findLine; Seg* segment; };
static uint32_t FlagsOf(const void* p, uintptr_t i)
{
    const FlagCtx* c = (const FlagCtx*)p;
    FLine* line = (*c->lines)[i];
    uint32_t f = 0;
    if (line == c->findLine) f |= 1;
    if (!line->mSegmentList.empty()) { f |= 2; if (line->mSegmentList.back()->mActual == c->segment) f |= 4; }
    return f;
}

static void Limits()
{
    for (int n = 0; n < 200000; n++) {
        int count = (int)(Next() % 10) + 1;
        std::vector<Seg> segs(count);
        int c = n % 9 == 0 ? Any() : (int)(Next() % 50);
        for (int i = 0; i < count; i++) {
            segs[i].mAbsStartChar = c;
            c += n % 31 == 0 ? Any() : (int)(Next() % 20);
            segs[i].mAbsEndChar = c;
            segs[i].mLineBreak = Next() % 3 == 0;
            segs[i].mActual = &segs[i];
        }
        std::list<Seg*> list;
        for (int i = 0; i < count; i++) list.push_back(&segs[i]);
        // EnforceMaxChars
        int mMaxChars = n % 5 == 0 ? 0 : (n % 11 == 0 ? Any() : (int)(Next() % 150));
        {
            Seg* seg=list.back();
            int toRemove=seg->mAbsEndChar-mMaxChars;
            Seg* o = NULL;
            if(!(mMaxChars<=0) && toRemove>0) {
                std::list<Seg*>::iterator itr=list.begin();
                while(itr!=list.end()) { seg=*itr; if(seg->mAbsEndChar+1>=toRemove) break; ++itr; }
                o = seg;
            }
            int excess = ee_won_gui_text_chars_excess(mMaxChars, list.back()->mAbsEndChar);
            Seg* r = NULL;
            if (excess > 0) {
                std::vector<int> ends;
                for (std::list<Seg*>::iterator i = list.begin(); i != list.end(); ++i) ends.push_back((*i)->mAbsEndChar);
                r = &segs[ee_won_gui_text_chars_cut(&ends[0], ends.size(), excess)];
            }
            CHECK(o == r);
        }
        // EnforceMaxLines
        int mMaxLines = n % 5 == 0 ? 0 : (int)(Next() % 8) - 1, mNumLines = (int)(Next() % 12);
        if (n % 23 == 0) mNumLines = Any();
        {
            Seg* seg=list.back();
            int toRemove=mNumLines-mMaxLines;
            Seg* o = NULL;
            if(!(mMaxLines<=0) && toRemove>0) {
                int k=0;
                std::list<Seg*>::iterator itr=list.begin();
                while(itr!=list.end()) { seg=*itr; if(seg->mLineBreak) k++; if(k>=toRemove) break; ++itr; }
                o = seg;
            }
            int excess = ee_won_gui_text_lines_excess(mMaxLines, mNumLines);
            Seg* r = NULL;
            if (excess > 0) {
                std::vector<char> breaks;
                for (std::list<Seg*>::iterator i = list.begin(); i != list.end(); ++i) breaks.push_back((*i)->mLineBreak);
                r = &segs[ee_won_gui_text_lines_cut((const bool*)&breaks[0], breaks.size(), excess)];
            }
            CHECK(o == r);
        }
        // RemoveSegmentsUpTo's line search: the segments over lines (some pieces standing for one segment)
        std::vector<Seg> pieces(count * 2);
        std::vector<FLine> flines((size_t)(Next() % 6) + 1);
        std::deque<FLine*> lines;
        for (size_t i = 0; i < flines.size(); i++) lines.push_back(&flines[i]);
        for (int i = 0; i < count; i++) {
            FLine* l = lines[Next() % lines.size()];
            l->mSegmentList.push_back(&segs[i]);
            if (Next() % 3 == 0) { pieces[i] = segs[i]; pieces[i].mActual = &segs[i]; lines[Next() % lines.size()]->mSegmentList.push_back(&pieces[i]); }
        }
        Seg* segment = &segs[Next() % count];
        FLine* findLine = Next() % 5 == 0 ? NULL : lines[Next() % lines.size()];
        {
            bool found=false;
            std::deque<FLine*>::iterator lineItr=lines.begin();
            while(lineItr!=lines.end()) {
                FLine* line=*lineItr;
                if(line==findLine) found=true;
                if(found && !line->mSegmentList.empty()) {
                    Seg* s=line->mSegmentList.back();
                    if(s->mActual!=segment) break;
                }
                ++lineItr;
            }
            FlagCtx ctx = { &lines, findLine, segment };
            CHECK((size_t)(lineItr - lines.begin()) == ee_won_gui_text_remove_stop(lines.size(), FlagsOf, &ctx));
        }
        // EnforceInvariants
        {
            for (size_t i = 0; i < flines.size(); i++) {
                flines[i].mWidth = n % 19 == 0 ? Any() : (int)(Next() % 300);
                flines[i].mHeight = n % 19 == 1 ? Any() : (int)(Next() % 30);
                flines[i].my = n % 19 == 2 ? Any() : (int)(Next() % 500);
            }
            bool noLines = Next() % 9 == 0, noSegs = Next() % 9 == 0;
            int sel0 = Small() * 7, sel1 = Small() * 7, off0 = n % 7 == 0 ? Any() : Small() * 20;
            std::vector<FLine> oLines = flines;
            std::vector<Seg> oSegs = segs;
            int mTotalWidth=0, mTotalHeight=0, mNumLines=0, mSelStartChar=sel0, mSelEndChar=sel1, mVertOffset=off0;
            if(!(noLines || noSegs)) {
                int deltaPos=oLines.front().my;
                for (size_t i = 0; i < oLines.size(); i++) {
                    FLine* line=&oLines[i];
                    if(line->mWidth>mTotalWidth) mTotalWidth=line->mWidth;
                    line->my-=deltaPos;
                    mTotalHeight+=line->mHeight;
                }
                int deltaChar=oSegs.front().mAbsStartChar;
                for (size_t i = 0; i < oSegs.size(); i++) {
                    Seg* seg=&oSegs[i];
                    seg->mAbsStartChar-=deltaChar;
                    seg->mAbsEndChar-=deltaChar;
                    if(seg->mLineBreak) mNumLines++;
                }
                mSelStartChar-=deltaChar;
                mSelEndChar-=deltaChar;
                if(mSelStartChar<0) mSelStartChar=0;
                if(mSelEndChar<0) mSelEndChar=0;
                mVertOffset-=deltaPos;
            }
            std::vector<int> lw, lh, ly, ss, se;
            std::vector<char> sb;
            for (size_t i = 0; i < flines.size(); i++) { lw.push_back(flines[i].mWidth); lh.push_back(flines[i].mHeight); ly.push_back(flines[i].my); }
            for (size_t i = 0; i < segs.size(); i++) { ss.push_back(segs[i].mAbsStartChar); se.push_back(segs[i].mAbsEndChar); sb.push_back(segs[i].mLineBreak); }
            int rs = sel0, re = sel1, ro = off0, out[3] = { -1, -1, -1 };
            size_t nl = noLines ? 0 : lw.size(), ns = noSegs ? 0 : ss.size();
            ee_won_gui_text_renumber(&lw[0], &lh[0], &ly[0], nl, &ss[0], &se[0], (const bool*)&sb[0], ns, &rs, &re, &ro, out);
            bool same = out[0] == mTotalWidth && out[1] == mTotalHeight && out[2] == mNumLines;
            if (!(noLines || noSegs)) {
                same = same && rs == mSelStartChar && re == mSelEndChar && ro == mVertOffset;
                for (size_t i = 0; i < flines.size(); i++) same = same && ly[i] == oLines[i].my;
                for (size_t i = 0; i < segs.size(); i++) same = same && ss[i] == oSegs[i].mAbsStartChar && se[i] == oSegs[i].mAbsEndChar;
            } else {
                same = same && rs == sel0 && re == sel1 && ro == off0;
            }
            CHECK(same);
        }
    }
}

// ---- FindSegment, CharAt, CheckSel, the drag and key scrolls, GetSelText, GotoAnchor

struct Area {
    std::deque<FLine*> mLineVector;
    int mTotalWidth, mTotalHeight;
    Seg* mLastFindSeg;
    std::vector<Seg*> owner; // the line of each segment
};
static FLine* LineOf(Area& a, Seg* s, std::vector<Seg>& segs) { return (FLine*)a.owner[s - &segs[0]]; }

static Seg* OrigFind(Area& a, std::vector<Seg>& segs, int x, int y, bool lenient)
{
    if(a.mLineVector.empty()) return NULL;
    if(lenient) {
        if(x>=a.mTotalWidth && a.mTotalWidth!=0) x=a.mTotalWidth-1;
        if(x<0) x=0;
        if(y>=a.mTotalHeight) y=a.mTotalHeight-1;
        if(y<0) y=0;
    }
    if(a.mLastFindSeg!=NULL) {
        FLine* line=LineOf(a,a.mLastFindSeg,segs);
        int xpos=x-line->mx;
        if(line->my<=y && line->my+line->mHeight>=y) {
            if(xpos>=a.mLastFindSeg->mx && xpos<=a.mLastFindSeg->mx+a.mLastFindSeg->mWidth) return a.mLastFindSeg;
        }
    }
    // FindLineAt
    FLine search; search.my=y;
    struct L { bool operator()(const FLine* p, const FLine* q) const { return p->my<q->my; } };
    std::deque<FLine*>::iterator lineItr=std::lower_bound(a.mLineVector.begin(),a.mLineVector.end(),&search,L());
    if(lineItr==a.mLineVector.end() || (*lineItr)->my>y) { if(lineItr!=a.mLineVector.begin()) --lineItr; }
    FLine* line=*lineItr;
    if(!lenient && line->my+line->mHeight<y) return NULL;
    x-=line->mx;
    std::list<Seg*>::iterator segItr=line->mSegmentList.begin();
    Seg* seg=NULL;
    while(segItr!=line->mSegmentList.end()) {
        seg=*segItr;
        if(x<=seg->mx+seg->mWidth) break;
        ++segItr;
    }
    if(seg!=NULL && !lenient && x<seg->mx) seg=NULL;
    a.mLastFindSeg=seg;
    return seg;
}

static int32_t FLineTop(const void* p, uintptr_t i) { return (*(const std::deque<FLine*>*)p)[i]->my; }

// the port's FindSegment (TextArea.cpp after the port)
static Seg* PortFind(Area& a, std::vector<Seg>& segs, int x, int y, bool lenient)
{
    if(a.mLineVector.empty()) return NULL;
    if(lenient) ee_won_gui_text_find_clamp(&x,&y,a.mTotalWidth,a.mTotalHeight);
    if(a.mLastFindSeg!=NULL) {
        FLine* line=LineOf(a,a.mLastFindSeg,segs);
        if(ee_won_gui_text_find_cached(x,y,line->mx,line->my,line->mHeight,a.mLastFindSeg->mx,a.mLastFindSeg->mWidth))
            return a.mLastFindSeg;
    }
    FLine* line=a.mLineVector[ee_won_gui_list_line_at(a.mLineVector.size(),y,FLineTop,&a.mLineVector)];
    std::vector<int> xs, ws;
    std::vector<Seg*> ps;
    for (std::list<Seg*>::iterator i=line->mSegmentList.begin(); i!=line->mSegmentList.end(); ++i) {
        xs.push_back((*i)->mx); ws.push_back((*i)->mWidth); ps.push_back(*i);
    }
    long long k=ee_won_gui_text_find_pick(x-line->mx,y,line->my,line->mHeight,xs.empty()?NULL:&xs[0],
                                          ws.empty()?NULL:&ws[0],xs.size(),lenient);
    if(k==-2) return NULL;
    Seg* seg=k<0?NULL:ps[(size_t)k];
    a.mLastFindSeg=seg;
    return seg;
}

static std::vector<int> sWidths;
static int32_t WidthOf(const void*, int32_t i) { return sWidths[(size_t)i]; }

static void Find()
{
    for (int n = 0; n < 60000; n++) {
        int nlines = (int)(Next() % 6);
        std::vector<FLine> flines(nlines);
        std::vector<Seg> segs(nlines * 4 + 1);
        Area a, b;
        a.owner.resize(segs.size(), NULL);
        int y = 0, k = 0;
        for (int i = 0; i < nlines; i++) {
            flines[i].mx = (int)(Next() % 20);
            flines[i].my = y;
            flines[i].mHeight = (int)(Next() % 16);
            y += Next() % 5 == 0 ? 0 : flines[i].mHeight;
            int ns = (int)(Next() % 4), x = (int)(Next() % 5);
            for (int j = 0; j < ns; j++, k++) {
                segs[k].mx = x;
                segs[k].mWidth = (int)(Next() % 30);
                x += segs[k].mWidth + (Next() % 3 == 0 ? (int)(Next() % 5) : 0);
                flines[i].mSegmentList.push_back(&segs[k]);
                a.owner[k] = (Seg*)&flines[i];
            }
            a.mLineVector.push_back(&flines[i]);
        }
        a.mTotalHeight = y;
        a.mTotalWidth = n % 4 == 0 ? 0 : (int)(Next() % 120);
        a.mLastFindSeg = NULL;
        b.mLineVector = a.mLineVector; b.mTotalHeight = a.mTotalHeight; b.mTotalWidth = a.mTotalWidth;
        b.mLastFindSeg = NULL; b.owner = a.owner;
        for (int q = 0; q < 12; q++) {
            int px = (int)(Next() % 160) - 20, py = (int)(Next() % 120) - 20;
            bool lenient = (Next() & 1) != 0;
            Seg* o = OrigFind(a, segs, px, py, lenient);
            Seg* r = PortFind(b, segs, px, py, lenient);
            CHECK(o == r && a.mLastFindSeg == b.mLastFindSeg);
        }
    }
    for (int n = 0; n < 300000; n++) {
        // TextSegment::CharAt
        int len = (int)(Next() % 12);
        sWidths.assign(len, 0);
        for (int i = 0; i < len; i++) sWidths[i] = n % 13 == 0 ? (int)(Next() % 40) - 5 : (int)(Next() % 12);
        int theStartChar = len ? (int)(Next() % (len + 1)) : 0, theEndChar = len;
        if (Next() % 3 == 0 && len) theEndChar = theStartChar + (int)(Next() % (len - theStartChar + 1));
        int theWidth = n % 9 == 0 ? Any() : (int)(Next() % 120) - 10, abs = Small() * 3;
        int aPos=theStartChar, aWidth=0, aCharWidth=0;
        while(aPos<theEndChar && aWidth<theWidth) { aCharWidth=sWidths[aPos]; aWidth+=aCharWidth; aPos++; }
        if(theWidth>=0 && theWidth<aWidth-aCharWidth/2) aPos--;
        CHECK(aPos + abs == ee_won_gui_text_char_at(theWidth, theStartChar, theEndChar, abs, WidthOf, NULL));
        // TextSegmentBase::CharAt
        int x = Any(), mx = Small(), mw = n % 7 == 0 ? Any() : (int)(Next() % 40), s0 = Small(), s1 = Small();
        CHECK((x<mx+mw/2 ? s0 : s1) == ee_won_gui_text_base_char_at(x, mx, mw, s0, s1));

        // CheckSel
        int mSelStartChar = Small(), mSelEndChar = Small(), sel = Small();
        bool mBackwardsSel = (Next() & 1) != 0, newStart = Next() % 4 == 0;
        int rs = mSelStartChar, re = mSelEndChar; bool rb = mBackwardsSel;
        if(newStart) { mSelStartChar=mSelEndChar=sel; mBackwardsSel=false; }
        else {
            bool backwards=mBackwardsSel;
            if(backwards) mSelStartChar=sel; else mSelEndChar=sel;
            if(mSelStartChar>mSelEndChar) { mBackwardsSel=!backwards; int old=mSelStartChar;mSelStartChar=mSelEndChar;mSelEndChar=old; }
        }
        ee_won_gui_text_check_sel(&rs, &re, &rb, sel, newStart);
        CHECK(rs == mSelStartChar && re == mSelEndChar && rb == mBackwardsSel);

        // TimerEvent's drag scroll
        int w = (int)(Next() % 100), h = (int)(Next() % 100), mx2 = (int)(Next() % 160) - 30, my2 = (int)(Next() % 160) - 30;
        if (n % 17 == 0) { mx2 = Any(); my2 = Any(); }
        bool full = (Next() & 1) != 0;
        int down = (int)(Next() % 30), up = (int)(Next() % 30), mVertOffset = Small() * 9, mHorzOffset = Small() * 9;
        int rv = mVertOffset, rh = mHorzOffset;
        bool inside = mx2>=0 && mx2<=w && my2>=0 && my2<=h;
        if (!inside) {
            if(my2>h) { int delta=my2-h; if(full && delta<down) delta=down; mVertOffset+=delta; }
            else if(my2<0) { int delta=-my2; if(full && delta<up) delta=up; mVertOffset-=delta; }
            if(mx2>w) mHorzOffset+=mx2-w;
            else if(mx2<0) mHorzOffset+=mx2;
        }
        bool moved = ee_won_gui_text_drag_scroll(mx2, my2, w, h, full, down, up, &rv, &rh);
        CHECK(moved == !inside && rv == mVertOffset && rh == mHorzOffset);

        // KeyDown
        static const int kKeys[] = { 0x27, 0x25, 0x28, 0x26, 0x22, 0x21, 0x41, 0x24, 0x23, 0x0d };
        int key = kKeys[Next() % 10];
        int kv = Small() * 5, kh = Small() * 5, ov = kv, oh = kh, vh = (int)(Next() % 90);
        bool used = true;
        switch(key) {
        case 0x27:oh+=10;break;
        case 0x25:oh-=10;break;
        case 0x28:ov+=down;break;
        case 0x26:ov-=up;break;
        case 0x22:ov+=vh;break;
        case 0x21:ov-=vh;break;
        default:used=false;
        }
        CHECK(used == ee_won_gui_text_key_scroll(key, &kv, &kh, down, up, vh) && kv == ov && kh == oh);

        // GetSelText: a text segment's part and line break, a plain segment's line break
        int ss = Small(), se = Small(), as = Small(), ae = as + (int)(Next() % 8);
        bool lb = (Next() & 1) != 0;
        {
            int aSelStartChar=ss, aSelEndChar=se;
            if(as>aSelStartChar) aSelStartChar=as;
            if(ae<aSelEndChar) aSelEndChar=ae;
            bool any = !(aSelStartChar>=aSelEndChar);
            int off = aSelStartChar-as, cnt = (aSelEndChar-as)-off;
            bool brk = any && lb && se>ae;
            int ro = -1, rl = -1;
            bool rany = ee_won_gui_text_sel_part(ss, se, as, ae, &ro, &rl);
            CHECK(rany == any && (!any || (ro == off && rl == cnt)));
            CHECK(brk == ee_won_gui_text_sel_break(true, lb, ss, se, as, ae));
            CHECK((lb && ss<=as && se>ae) == ee_won_gui_text_sel_break(false, lb, ss, se, as, ae));
        }

        // GotoAnchor's clamp (only with lines)
        size_t size = (size_t)(Next() % 6) + 1;
        int aLineNum = n % 5 == 0 ? Any() : Small();
        int o2 = aLineNum;
        if(o2>=size) o2=size-1;
        if(o2<0) o2=0;
        CHECK(o2 == ee_won_gui_text_anchor_line(aLineNum, size));
    }
}

// ---- TextSegment::Wrap

struct WLine { int mWidth; int mSegs; };
struct WOp { int kind, start, end, width; };
static std::vector<WLine*> sLines;
static std::vector<WOp> sOps;
static WLine* AddLine() { WLine* l = new WLine(); l->mWidth = 0; l->mSegs = 0; sLines.push_back(l); WOp o = { 1, 0, 0, 0 }; sOps.push_back(o); return l; }
static void AddPiece(WLine* l, int s, int e, int w) { l->mWidth += w; l->mSegs++; WOp o = { 0, s, e, w }; sOps.push_back(o); }
static std::vector<wchar_t> sText;
static int StringWidth(int from, int len) { int w = 0; for (int i = from; i < from + len; i++) w += sWidths[i] + (sText[i] == L'W' ? 1 : 0); return w; }
static int32_t RestOf(const void*, int32_t from) { return StringWidth(from, (int)sText.size() - from); }

static void OrigWrap(WLine* theLine, int aTextBoxWidth, bool left, int indent)
{
    int aCurPos = 0;
    int len = (int)sText.size();
    WLine *aLine = theLine;
    while(aCurPos < len)
    {
        int aWidth = aLine->mWidth;
        int aBreakWidth = aLine->mWidth;
        int aStartPos = aCurPos;
        int aBreakPos = aStartPos-1;
        while(aCurPos < len)
        {
            int aChar = sText[aCurPos];
            aWidth += sWidths[aCurPos];
            if(aChar==' ') { aBreakPos = aCurPos; aBreakWidth = aWidth; }
            if(aWidth > aTextBoxWidth) break;
            aCurPos++;
        }
        bool forceBreak = false;
        if(aBreakPos < aStartPos)
        {
            if(aWidth < aTextBoxWidth) aBreakPos = len;
            else if(aLine->mSegs==0)
            {
                aBreakPos = aCurPos;
                aBreakWidth = aTextBoxWidth;
                if(aBreakPos==aStartPos) aBreakPos++;
                forceBreak = true;
            }
            else aCurPos = 0;
        }
        if(aBreakPos >= aStartPos)
        {
            if(aBreakPos!=aStartPos) AddPiece(aLine, aStartPos, aBreakPos, aBreakWidth);
            aCurPos = aBreakPos;
            if(!forceBreak) aCurPos++;
        }
        if(aCurPos>=len) break;
        aLine = AddLine();
        if(left) { aLine->mWidth = indent; sOps.back().width = indent; }
        aWidth = StringWidth(aCurPos, len-aCurPos);
        if(!(aWidth <= aTextBoxWidth - aLine->mWidth)) continue;
        AddPiece(aLine, aCurPos, len, aWidth);
        break;
    }
}

static void Wrap()
{
    static const wchar_t kUnits[] = { L' ', L' ', L'a', L'b', L'W', L'c', 0xe9, 0x4e2d };
    for (int n = 0; n < 200000; n++) {
        int len = (int)(Next() % 24);
        if (n % 100 == 0) len = (int)(Next() % 300);
        sText.clear(); sWidths.clear();
        for (int i = 0; i < len; i++) {
            sText.push_back(kUnits[Next() % 8]);
            sWidths.push_back(Next() % 11 == 0 ? (int)(Next() % 60) : (int)(Next() % 9));
        }
        int box = (int)(Next() % 90) - 3;
        if (n % 37 == 0) box = Any();
        WLine first = { (int)(Next() % 40), (int)(Next() % 2) };
        if (n % 29 == 0) first.mWidth = Any();
        bool left = (Next() & 1) != 0;
        int indent = (int)(Next() % 20);
        sOps.clear();
        WLine copy = first;
        OrigWrap(&copy, box, left, indent);
        for (size_t i = 0; i < sLines.size(); i++) delete sLines[i];
        sLines.clear();
        std::vector<EeWonTextWrapOp> r(3 * len + 8);
        size_t k = ee_won_gui_text_wrap(len ? &sText[0] : NULL, (uint16_t)len, sizeof(wchar_t), len ? &sWidths[0] : NULL,
                                        box, first.mWidth, first.mSegs == 0, left, indent, RestOf, NULL, &r[0], r.size());
        CHECK(k == sOps.size());
        bool same = k == sOps.size();
        for (size_t i = 0; same && i < k; i++)
            same = r[i].kind == sOps[i].kind && r[i].width == sOps[i].width &&
                   (r[i].kind == 1 || (r[i].start == sOps[i].start && r[i].end == sOps[i].end));
        CHECK(same);
    }
}

// ---- AddFormatedText, LobbyAppendFormattedText

static std::vector<std::wstring> sTrace;
static void Rec(const std::wstring& s) { sTrace.push_back(s); }
static std::wstring Num(int i) { char b[32]; snprintf(b, sizeof b, "%d", i); std::wstring w; for (char* p = b; *p; p++) w += (wchar_t)*p; return w; }

static void Split()
{
    static const wchar_t kUnits[] = { L'\r', L'\n', L'a', L'b', L' ', 0x4e2d };
    for (int n = 0; n < 200000; n++) {
        int len = (int)(Next() % 16);
        std::wstring text;
        for (int i = 0; i < len; i++) text += kUnits[Next() % 6];
        bool lineBreak = (Next() & 1) != 0;
        // AddFormatedText
        sTrace.clear();
        int start=0,next=0;
        while(start<(int)text.length()) {
            int end=start;
            while(end<(int)text.length()) {
                int ch=text[end];
                if(ch=='\r') { if(end+1<(int)text.length() && text[end+1]=='\n') next=end+2; else next=end+1; break; }
                else if(ch=='\n') { next=end+1;break; }
                end++;
            }
            bool foundReturn=true;
            if(end>=(int)text.length()) { next=(int)text.length();foundReturn=lineBreak; }
            Rec(text.substr(start,end-start) + (foundReturn ? L"|b" : L"|"));
            start=next;
        }
        std::vector<std::wstring> orig = sTrace;
        sTrace.clear();
        std::vector<EeWonTextPart> parts(len + 1);
        size_t k = ee_won_gui_text_split(len ? text.c_str() : NULL, (uint16_t)len, sizeof(wchar_t), lineBreak, &parts[0], parts.size());
        for (size_t i = 0; i < k && i < parts.size(); i++)
            Rec(text.substr(parts[i].start, parts[i].len) + (parts[i].line_break ? L"|b" : L"|"));
        CHECK(orig == sTrace);
    }
}

static void Format()
{
    static const wchar_t kUnits[] = { L'<', L'<', L'>', L'n', L'N', L'c', L'C', L'f', L's', L'l', L'L', L'1', L'2', L'3', L'0', L'x', L' ', 0xe9 };
    for (int n = 0; n < 300000; n++) {
        int len = (int)(Next() % 18);
        std::vector<wchar_t> format;
        for (int i = 0; i < len; i++) format.push_back(kUnits[Next() % (sizeof(kUnits) / sizeof(kUnits[0]))]);
        format.push_back(0);
        int numStrings = (int)(Next() % 4) - 1, numColors = (int)(Next() % 4) - 1, numFonts = (int)(Next() % 4) - 1,
            numSegments = (int)(Next() % 4) - 1;
        // the NULL entries: index 1 of the strings, fonts and segments
        bool lineBreak = (Next() & 1) != 0;
        // the original over a trace (format[i] reads the terminator at the end, as the GUIString did)
        sTrace.clear();
        {
            int color=-1, font=-1;
            std::wstring text;
            int start=0;
            while(true)
            {
                int open=-1;
                for (int i = start; i < len; i++) if (format[i] == '<') { open = i; break; }
                if(open<0) break;
                text.append(&format[start], open-start);
                int next=open+1;
                int command=format[next];
                if(command=='<') { text += L'<'; start=open+2; continue; }
                if(next<len)
                {
                    if(!text.empty()) { Rec(L"seg " + text + L" " + Num(color) + L" " + Num(font)); text.clear(); }
                    int index=format[open+2]-'1';
                    switch(command < 256 ? tolower(command) : command)
                    {
                    case 'n': Rec(L"break"); break;
                    case 'c': if(index>=0 && index<numColors) color=index; break;
                    case 'f': if(index>=0 && index<numFonts && index!=1) font=index; break;
                    case 's': if(index>=0 && index<numStrings && index!=1) Rec(L"str " + Num(index) + L" " + Num(color) + L" " + Num(font)); break;
                    case 'l': if(index>=0 && index<numSegments && index!=1) Rec(L"lseg " + Num(index)); break;
                    }
                }
                start=open+1;
                int close=-1;
                for (int i = start; i < len; i++) if (format[i] == '>') { close = i; break; }
                if(close<0) break;
                start=close+1;
            }
            if(start<len) text.append(&format[start], len-start);
            if(!text.empty() || lineBreak) Rec(L"last " + text + L" " + Num(color) + L" " + Num(font) + (lineBreak ? L" b" : L""));
        }
        std::vector<std::wstring> orig = sTrace;
        // the port (TextFormat.cpp after the port) over the same trace
        sTrace.clear();
        {
            int color=-1, font=-1;
            std::wstring text;
            std::vector<EeWonFormatOp> ops(2 * len + 2);
            size_t k = ee_won_gui_text_format(&format[0], (uint16_t)len, sizeof(wchar_t), numStrings, numColors, numFonts,
                                              numSegments, lineBreak, &ops[0], ops.size());
            CHECK(k <= ops.size());
            for (size_t i = 0; i < k && i < ops.size(); i++) {
                const EeWonFormatOp& o = ops[i];
                switch (o.kind) {
                case 0: text.append(&format[o.a], o.b); break;
                case 1: text += L'<'; break;
                case 2: Rec(L"seg " + text + L" " + Num(color) + L" " + Num(font)); text.clear(); break;
                case 3: Rec(L"break"); break;
                case 4: color = o.a; break;
                case 5: if (o.a != 1) font = o.a; break;
                case 6: if (o.a != 1) Rec(L"str " + Num(o.a) + L" " + Num(color) + L" " + Num(font)); break;
                case 7: if (o.a != 1) Rec(L"lseg " + Num(o.a)); break;
                case 8: Rec(L"last " + text + L" " + Num(color) + L" " + Num(font) + (o.b ? L" b" : L"")); break;
                }
            }
        }
        CHECK(orig == sTrace);
    }
}

int main()
{
    Metrics();
    Lines();
    Limits();
    Find();
    Wrap();
    Split();
    Format();
    printf("won_guitext_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
