// TextArea as a whole object (R6 WON round 21): the original TextArea.cpp's methods and its segments' layout
// virtuals (TextSegments.cpp, TextAreaLines.cpp, TextAreaInstructions.cpp, TextSegmentBaseDefaults.cpp,
// TextSegmentPieceState.cpp: the statements copied from the C++ before the port, every call on a C++ object, a field
// of the ScrollArea base or a segment's C++ data replaced by the host op that makes it: GuiHost.cpp runs the same
// calls in the same order) against ee_won's Rust TextArea (textobj.rs) over one scripted host. Each of 20,000
// sequences creates the area and makes 40 random inward calls (text segments of every length with links among
// them, line breaks, alignments, size changes, visibility, the mouse over links and text, drags, the timer, the
// keys, the copy, the flags, both limits, resets, paints); the host answers from the sequence's seeded stream and
// keeps the base class's fields, the segments' link state and their references. The base class's size change
// calls the area's AdjustPosition back, and a delivered link event runs a scripted handler that may reset the
// area, add a link or disable the clicked one, as the chat's handler does. The traces (every op with its
// arguments and answer, the draws' operations, the copied text) must be the same, and every reference taken
// must be given back when the area goes.
//
// Two departures from the original, the same on both sides: a text segment's line break is read once when it is
// added (SegLineBreak; nothing changes it later), and when a trim removes the segment a hovered link piece belongs
// to, the link is dropped (the original kept the piece of a freed segment). The lines' references to whole
// segments are not counted: they never outlived the list's.
//
// The first 100 sequences and every later one that makes an op or a call none before made are
// rust/crates/ee_won/src/golden_textobj.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <deque>
#include <map>
#include <set>
#include "rust/ee_won.h"

typedef EeWonGuiOp Op;
typedef EeWonGuiField Fd;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

// ---- the scripted host

static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }

struct HostSeg {
    std::vector<uint32_t> mText;
    bool mLineBreak;
    bool mLink;      // a TextLink (or a chat link) ...
    bool mEnabled;   // ... still clickable (ChatLink::IsLink)
    bool mPressed;
    int mColor, mNormalColor, mDownColor;
    int mFontHeight, mFontAscent, mFontKern;
    int mRefs;
};
static std::map<intptr_t, HostSeg> sSegs;
static intptr_t sNextSeg;
static int sFields[16];
static std::vector<std::string> sTrace;
static std::set<int> sOpsMet;

// the side running (to call the area back)
struct Side {
    void (*mAdjust)(bool);
    void (*mReset)(bool);
    void (*mAddSegment)(intptr_t);
    bool mRust;
};
static Side sSide;

static intptr_t NewSeg(bool inLink)
{
    HostSeg s;
    unsigned aLength = R(8) == 0 ? R(90) : R(30);
    bool aLongWord = R(6) == 0;
    for (unsigned i = 0; i < aLength; i++)
        s.mText.push_back(!aLongWord && R(5) == 0 ? ' ' : (uint32_t)('a' + R(26)));
    if (R(12) == 0) s.mText.push_back(0x263a); // a unit past ASCII
    s.mLineBreak = R(3) == 0;
    s.mLink = inLink;
    s.mEnabled = true;
    s.mPressed = false;
    s.mColor = (int)R(0x1000000);
    s.mNormalColor = 0;
    s.mDownColor = 0xff0000;
    s.mFontHeight = 10 + (int)R(8);
    s.mFontAscent = s.mFontHeight - 2 - (int)R(3);
    s.mFontKern = (int)R(3);
    s.mRefs = 1; // the creator's
    intptr_t anId = 0x1000 + (++sNextSeg);
    sSegs[anId] = s;
    return anId;
}
static void DropCreatorRef(intptr_t inSeg) { sSegs[inSeg].mRefs--; }

static int CharWidth(uint32_t inUnit) { return inUnit == ' ' ? 2 : inUnit > 0x7f ? 9 : 3 + (int)(inUnit % 5); }
static int StringWidth(const HostSeg& s, int inStart, int inLength)
{
    int w = 0;
    for (int i = inStart; i < inStart + inLength && i < (int)s.mText.size(); i++)
        if (i >= 0) w += CharWidth(s.mText[i]);
    return inLength > 0 ? w + s.mFontKern : w;
}

static std::string Units(const HostSeg& s, int inOffset, int inLength)
{
    std::string r;
    for (int i = inOffset; i < inOffset + inLength && i < (int)s.mText.size(); i++)
        r += s.mText[i] < 0x80 ? (char)s.mText[i] : '?';
    return r;
}

static void Trace(const char* inFormat, long long a, long long b, long long c, long long d, long long r)
{
    char aLine[200];
    snprintf(aLine, sizeof(aLine), inFormat, a, b, c, d, r);
    sTrace.push_back(aLine);
}

static int sLinksActivated;
static void TraceFields()
{
    char aLine[200];
    snprintf(aLine, sizeof(aLine), "fields");
    sTrace.push_back(aLine);
    for (int i = 0; i < 12; i++) { snprintf(aLine, sizeof(aLine), " %d", sFields[i]); sTrace.back() += aLine; }
}

static intptr_t ScriptedHost(void* inComp, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)inComp;
    sOpsMet.insert((int)inOp);
    intptr_t r = 0;
    char aLine[300];
    switch (inOp) {
    case Op::Get: return sFields[a];  // pure reads: not traced (their order is free)
    case Op::Set: sFields[a] = (int)b; break;
    case Op::BaseSizeChanged:
        Trace("%lld base-size", (int)inOp, 0, 0, 0, 0);
        sFields[(int)Fd::ScrollWidth] = R(4) == 0 ? 0 : 20 + (int)R(280);
        sFields[(int)Fd::ScrollHeight] = 10 + (int)R(200);
        sFields[(int)Fd::Width] = sFields[(int)Fd::ScrollWidth] + (int)R(12);
        sFields[(int)Fd::Height] = sFields[(int)Fd::ScrollHeight] + (int)R(12);
        if (R(3) == 0) sFields[(int)Fd::VertOffset] += (int)R(40) - 20;
        TraceFields();
        sSide.mAdjust(false); // ScrollArea::Virtual08 calls the virtual AdjustPosition
        return 0;
    case Op::BaseAdjustPosition: {
        if (a) sFields[(int)Fd::UserHasScrolled] = 1;
        int aMaxV = sFields[(int)Fd::TotalHeight] + sFields[(int)Fd::ExtraHeight] - sFields[(int)Fd::ScrollHeight];
        int aMaxH = sFields[(int)Fd::TotalWidth] - sFields[(int)Fd::ScrollWidth];
        int& v = sFields[(int)Fd::VertOffset];
        int& h = sFields[(int)Fd::HorzOffset];
        if (v > aMaxV) v = aMaxV;
        if (v < 0) v = 0;
        if (h > aMaxH) h = aMaxH;
        if (h < 0) h = 0;
        Trace("%lld base-adjust %lld -> %lld %lld %lld", (int)inOp, (long long)a, v, h, sFields[(int)Fd::UserHasScrolled]);
        return 0;
    }
    case Op::BaseKeyDown: case Op::BaseKeyChar: r = R(8) == 0; break;
    case Op::ScrollTranslate: {
        int* x = (int*)a; int* y = (int*)b;
        long long ix = *x, iy = *y;
        *x += sFields[(int)Fd::HorzOffset] - 3;
        *y += sFields[(int)Fd::VertOffset] - 2;
        Trace("%lld translate %lld %lld -> %lld %lld", (int)inOp, ix, iy, *x, *y);
        return 0;
    }
    case Op::GetMousePos: {
        int* x = (int*)a; int* y = (int*)b;
        *x = (int)R(sFields[(int)Fd::Width] + 60) - 30;
        *y = (int)R(sFields[(int)Fd::Height] + 60) - 30;
        Trace("%lld mouse %lld %lld", (int)inOp, *x, *y, 0, 0);
        return 0;
    }
    case Op::KeyMod: r = R(4); break;
    case Op::SelColor: {
        EeWonSelColor* s = (EeWonSelColor*)a;
        s->background = 0x3366cc; s->foreground = 0xffffff; s->mode = (int)(sNextSeg % 3);
        Trace("%lld sel-color %lld", (int)inOp, s->mode, 0, 0, 0);
        return 0;
    }
    case Op::LinkActivated: {
        unsigned anAction = R(5);
        Trace("%lld link-activated %llx action %lld", (int)inOp, (long long)a, anAction, 0, 0);
        sLinksActivated++;
        if (anAction == 1) sSide.mReset(false);
        else if (anAction == 2) { intptr_t s = NewSeg(true); sSide.mAddSegment(s); DropCreatorRef(s); }
        else if (anAction == 3) { HostSeg& s = sSegs[a]; s.mEnabled = false; s.mColor = 0x808080; }
        return 0;
    }
    case Op::SelTextAppend:
        snprintf(aLine, sizeof(aLine), "%u sel-append %llx %d %d '%s'", (unsigned)inOp, (long long)a, (int)b, (int)c,
                 Units(sSegs[a], (int)b, (int)c).c_str());
        sTrace.push_back(aLine);
        return 0;
    case Op::SegHold: sSegs[a].mRefs++; break;
    case Op::SegRelease: sSegs[a].mRefs--; break;
    case Op::SegLineBreak: r = sSegs[a].mLineBreak; break;
    case Op::SegLength: r = (intptr_t)sSegs[a].mText.size(); break;
    case Op::SegMeasure: {
        const HostSeg& s = sSegs[a];
        int* m = (int*)b;
        m[0] = StringWidth(s, 0, (int)s.mText.size());
        m[1] = s.mFontHeight;
        m[2] = s.mFontAscent;
        Trace("%lld measure %llx -> %lld %lld %lld", (int)inOp, (long long)a, m[0], m[1], m[2]);
        return 0;
    }
    case Op::SegStringWidth: r = StringWidth(sSegs[a], (int)b, (int)c); break;
    case Op::SegCharWidth: {
        const HostSeg& s = sSegs[a];
        r = b >= 0 && b < (intptr_t)s.mText.size() ? CharWidth(s.mText[b]) : 0;
        break;
    }
    case Op::SegTextTake: {
        const HostSeg& s = sSegs[a];
        if (sSide.mRust) { if (!s.mText.empty()) ee_won_gui_sink_units((void*)b, &s.mText[0], s.mText.size(), sizeof(uint32_t)); }
        else *(std::vector<uint32_t>*)b = s.mText; // the original read the text itself
        snprintf(aLine, sizeof(aLine), "%u take %llx ", (unsigned)inOp, (long long)a);
        sTrace.push_back(aLine);
        for (size_t i = 0; i < s.mText.size(); i++) { snprintf(aLine, sizeof(aLine), "%x.", (unsigned)s.mText[i]); sTrace.back() += aLine; }
        return 0;
    }
    case Op::SegIsLink: r = sSegs[a].mLink && sSegs[a].mEnabled; break;
    case Op::SegPressed: {
        HostSeg& s = sSegs[a];
        if (s.mLink) { s.mPressed = true; s.mNormalColor = s.mColor; s.mColor = s.mDownColor; }
        break;
    }
    case Op::SegReleased: {
        HostSeg& s = sSegs[a];
        if (s.mLink && s.mPressed) { s.mColor = s.mNormalColor; s.mPressed = false; r = 1; }
        break;
    }
    case Op::SegColor: r = sSegs[a].mColor; break;
    case Op::Draw: {
        const EeWonDrawOp* o = (const EeWonDrawOp*)b;
        snprintf(aLine, sizeof(aLine), "%u draw %llx %d %d %d %d %d %d %d %d %d %llx", (unsigned)inOp, (long long)a,
                 o->kind, o->a, o->b, o->c, o->d, o->e, o->f, o->g, o->h, (long long)c);
        sTrace.push_back(aLine);
        return 0;
    }
    default: break;
    }
    Trace("%lld %lld %lld %lld %lld", (int)inOp, (long long)a, (long long)b, (long long)c, 0);
    sTrace.back() += " -> " + std::to_string((long long)r);
    (void)d;
    return r;
}

static intptr_t H(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0)
{
    return ScriptedHost((void*)0x51, inOp, a, b, c, d);
}
static int G(Fd inField) { return (int)H(Op::Get, (intptr_t)inField); }
static void S(Fd inField, int inValue) { H(Op::Set, (intptr_t)inField, inValue); }

// ---- the original (TextArea.cpp and its segments; ScrollArea's fields through G and S, the C++ segments' calls
// through the host)

namespace orig {

enum { TextFlag_WordWrap = 0x01, TextFlag_AllowSelection = 0x02, TextFlag_ScrollOnBottom = 0x04,
       TextFlag_ShowPartialLines = 0x08, TextFlag_ScrollFullLine = 0x10, TextFlag_DelayWordWrap = 0x20 };
enum { HorzAlign_Left = 0 };
static const intptr_t kG = 0x6; // the Graphics

struct TextArea;
struct TextLine;
static std::vector<struct TextSegmentBase*> sSegArena;
static std::vector<TextLine*> sLineArena;

struct TextSegmentBase {
    TextSegmentBase() : mLine(NULL), mAbsStartChar(0), mAbsEndChar(0), mWidth(0), mHeight(0), mAscent(0), mx(0),
                        my(0), mLineBreak(false), mCpp(0) { sSegArena.push_back(this); }
    virtual ~TextSegmentBase() {}
    virtual int GetLength() { return 1; }
    virtual void CalcDimensions() {}
    virtual void PerformInstructions(TextArea*) {}
    virtual void GetSelText(TextArea*);
    virtual void Paint(TextArea*) {}
    virtual int CharAt(int x) { return ee_won_gui_text_base_char_at(x, this->mx, this->mWidth, this->mAbsStartChar, this->mAbsEndChar); }
    virtual void Wrap(TextArea*, TextLine*);
    virtual bool IsLink() { return false; }
    virtual void Pressed() {}
    virtual bool Released() { return false; }
    virtual TextSegmentBase* GetActual() { return this; }
    TextLine* mLine;
    int mAbsStartChar, mAbsEndChar;
    int mWidth, mHeight, mAscent, mx, my;
    bool mLineBreak;
    intptr_t mCpp; // a text segment's C++ object
};
struct TextAlignInstruction : TextSegmentBase {
    virtual void PerformInstructions(TextArea*);
    int mAlign;
};
// TextSegment and TextLink: the C++ object's data and link virtuals through the host
struct TextSegment : TextSegmentBase {
    virtual int GetLength() { return (int)H(Op::SegLength, this->mCpp); }
    virtual void CalcDimensions();
    virtual void GetSelText(TextArea*);
    void Paint(TextArea*, int, int, int, int, int);
    virtual void Paint(TextArea*);
    int CharAt(int, int, int);
    virtual int CharAt(int);
    virtual void Wrap(TextArea*, TextLine*);
    virtual bool IsLink() { return H(Op::SegIsLink, this->mCpp) != 0; }
    virtual void Pressed() { H(Op::SegPressed, this->mCpp); }
    virtual bool Released() { return H(Op::SegReleased, this->mCpp) != 0; }
    int TextLength() { return (int)H(Op::SegLength, this->mCpp); } // mText.length()
};
struct TextSegmentPiece : TextSegmentBase {
    TextSegmentPiece(TextSegment* segment, int start, int end, int width)
    {
        this->mSeg = segment;
        this->mWidth = width;
        this->mHeight = segment->mHeight;
        this->mAscent = segment->mAscent;
        this->mStartChar = start;
        this->mEndChar = end;
    }
    virtual void Paint(TextArea*);
    virtual int CharAt(int);
    virtual bool IsLink() { return this->mSeg->IsLink(); }
    virtual void Pressed() { this->mSeg->Pressed(); }
    virtual bool Released() { return this->mSeg->Released(); }
    virtual TextSegmentBase* GetActual() { return this->mSeg; }
    TextSegment* mSeg;
    int mStartChar, mEndChar;
};
struct TextLine {
    TextLine() : mx(0), my(0), mWidth(0), mHeight(0), mBaseline(0), mDescent(0) { sLineArena.push_back(this); }
    int mx, my, mWidth, mHeight, mBaseline, mDescent;
    typedef std::list<TextSegmentBase*> SegmentList;
    SegmentList mSegmentList;
};

struct TextArea {
    typedef std::list<TextSegmentBase*> SegmentList;
    typedef std::deque<TextLine*> LineVector;
    SegmentList mSegmentList;
    LineVector mLineVector;
    int mTopLineVectorPos, mMaxChars, mMaxLines, mNumLines;
    bool mNeedRecalcWordWrap;
    int mDelayWordWrapTimeAccum;
    TextSegmentBase *mCurLink, *mLastFindSeg;
    bool mSelecting, mBackwardsSel, mHasBeenVisible;
    int mSelStartChar, mSelEndChar;
    int mTextFlags;
    int mWrapIndentSize;
    int mLineAlign;
    bool TextFlagSet(int f) { return (this->mTextFlags & f) != 0; }

    // the constructor's own state (the fonts, colours and padding are the C++ shell's), then Reset(false)
    TextArea()
    {
        this->mTextFlags = TextFlag_WordWrap | TextFlag_AllowSelection | TextFlag_ShowPartialLines | TextFlag_ScrollOnBottom;
        this->mHasBeenVisible = false;
        this->mMaxChars = 80 * 2000;
        this->mMaxLines = 0;
        this->mWrapIndentSize = 0;
        this->mLineAlign = HorzAlign_Left;
    }
    ~TextArea()
    {
        // the segment list's references (the C++ destructor's list clear)
        for (SegmentList::iterator i = this->mSegmentList.begin(); i != this->mSegmentList.end(); ++i)
            if ((*i)->mCpp) H(Op::SegRelease, (*i)->mCpp);
    }
    void ReleaseList(SegmentList::iterator from, SegmentList::iterator to)
    {
        for (SegmentList::iterator i = from; i != to; ++i)
            if ((*i)->mCpp) H(Op::SegRelease, (*i)->mCpp);
        this->mSegmentList.erase(from, to);
    }
    void Invalidate() { H(Op::Invalidate); }
    void ScrollTranslate(int& x, int& y) { H(Op::ScrollTranslate, (intptr_t)&x, (intptr_t)&y); }
    int Width() { return G(Fd::Width); }
    int Height() { return G(Fd::Height); }

    void Reset(bool justDisplay)
    {
        this->mLineVector.clear();
        if (!justDisplay) {
            this->ReleaseList(this->mSegmentList.begin(), this->mSegmentList.end());
            this->mSelecting = false;
            this->mSelStartChar = this->mSelEndChar = 0;
            this->mBackwardsSel = false;
            S(Fd::HorzOffset, 0); S(Fd::VertOffset, 0);   // mVertOffset=mHorzOffset=0
            S(Fd::TotalHeight, 0); S(Fd::TotalWidth, 0);  // mTotalWidth=mTotalHeight=0
            this->AdjustPosition(false);
        }
        this->mNumLines = 0;
        this->mTopLineVectorPos = 0;
        S(Fd::TotalWidth, 0); S(Fd::TotalHeight, 0);      // mTotalHeight=mTotalWidth=0
        this->mCurLink = NULL;
        this->mLastFindSeg = NULL;
        this->mNeedRecalcWordWrap = false;
        this->mDelayWordWrapTimeAccum = 0;
    }

    void RecalcWordWrap(bool wasDelayed)
    {
        this->Reset(true);
        SegmentList::iterator itr = this->mSegmentList.begin();
        while (itr != this->mSegmentList.end()) {
            this->AddSegmentPrv(*itr);
            ++itr;
        }
        if (this->ShouldScrollOnBottom()) S(Fd::VertOffset, G(Fd::TotalHeight));
        if (wasDelayed) this->AdjustPosition(false);
    }

    void Virtual08()
    {
        int oldWidth = G(Fd::ScrollWidth);
        bool bottom = this->ShouldScrollOnBottom();
        H(Op::BaseSizeChanged);
        if (this->TextFlagSet(TextFlag_WordWrap) && !this->mSegmentList.empty() && G(Fd::ScrollWidth) != oldWidth &&
            this->mHasBeenVisible) {
            if (ee_won_gui_text_delay_wrap(this->mTextFlags, this->mSegmentList.back()->mAbsEndChar)) {
                this->mDelayWordWrapTimeAccum = 0;
                this->mNeedRecalcWordWrap = true;
                H(Op::RequestTimer, 1);
            } else this->RecalcWordWrap(false);
        }
        if (bottom) S(Fd::VertOffset, G(Fd::TotalHeight) + G(Fd::ExtraHeight) - G(Fd::ScrollHeight));
        this->AdjustPosition(false);
    }

    void AdjustPosition(bool becauseOfScrollbar)
    {
        S(Fd::ExtraHeight, 0);
        if (this->TextFlagSet(TextFlag_ScrollFullLine) && G(Fd::TotalHeight) > G(Fd::ScrollHeight)) {
            LineVector::iterator itr = this->FindLineAt(G(Fd::TotalHeight) - G(Fd::ScrollHeight));
            if (itr != this->mLineVector.end()) {
                TextLine* line = *itr;
                S(Fd::ExtraHeight, ee_won_gui_list_full_line_extra(G(Fd::TotalHeight), G(Fd::ScrollHeight), line->my, line->mHeight));
            }
        }
        H(Op::BaseAdjustPosition, becauseOfScrollbar);
        this->FindTopLine();
        if (!this->TextFlagSet(TextFlag_ScrollFullLine))
            return;
        if (!((size_t)this->mTopLineVectorPos < this->mLineVector.size()))
            return;
        TextLine* line = this->mLineVector[this->mTopLineVectorPos];
        S(Fd::VertOffset, line->my);
        S(Fd::DownScrollAmount, line->mHeight);
        if (this->mTopLineVectorPos > 0) {
            line = this->mLineVector[this->mTopLineVectorPos - 1];
            S(Fd::UpScrollAmount, line->mHeight);
        }
        H(Op::BaseAdjustPosition, 1);
    }

    bool ShouldScrollOnBottom()
    {
        EeWonTextLineY aLast = {0, 0}, aPrev = {0, 0};
        size_t aCount = this->mLineVector.size();
        if (aCount > 0) { aLast.y = this->mLineVector.back()->my; aLast.height = this->mLineVector.back()->mHeight; }
        if (aCount > 1) { aPrev.y = this->mLineVector[aCount - 2]->my; aPrev.height = this->mLineVector[aCount - 2]->mHeight; }
        return ee_won_gui_text_scroll_on_bottom(this->mTextFlags, G(Fd::UserHasScrolled) != 0, aCount, aLast, aPrev,
                                                G(Fd::VertOffset), G(Fd::ScrollHeight));
    }

    void NotifyRootVisibility(bool visible)
    {
        H(Op::BaseNotifyRootVisibility, visible);
        if (visible && !this->mHasBeenVisible) {
            this->mHasBeenVisible = true;
            if (this->TextFlagSet(TextFlag_WordWrap) && !this->mSegmentList.empty()) this->RecalcWordWrap(true);
        }
    }

    static EeWonRowPlace TextLinePlaceOf(const void* ctx, size_t index)
    {
        const LineVector& lines = *(const LineVector*)ctx;
        EeWonRowPlace r;
        r.x = lines[index]->mx;
        r.top = lines[index]->my;
        r.height = lines[index]->mHeight;
        return r;
    }
    void ScrollPaint()
    {
        EeWonDrawOp ops[512];
        size_t n = ee_won_gui_draw_text_lines((size_t)this->mTopLineVectorPos, this->mLineVector.size(), G(Fd::VertOffset),
                                              G(Fd::ScrollHeight), this->TextFlagSet(TextFlag_ShowPartialLines),
                                              TextLinePlaceOf, &this->mLineVector, ops, 512);
        for (size_t i = 0; i < n && i < 512; i++) {
            const EeWonDrawOp& o = ops[i];
            if (o.kind == EE_WON_DRAW_CALL) {
                TextLine* line = this->mLineVector[o.b];
                for (TextLine::SegmentList::iterator segItr = line->mSegmentList.begin(); segItr != line->mSegmentList.end(); ++segItr)
                    (*segItr)->Paint(this);
            } else H(Op::Draw, kG, (intptr_t)&o, 0);
        }
    }

    static int32_t TextLineTopOf(const void* inLines, uintptr_t i) { return (*static_cast<const LineVector*>(inLines))[i]->my; }
    LineVector::iterator FindLineAt(int y)
    {
        return this->mLineVector.begin() + ee_won_gui_list_line_at(this->mLineVector.size(), y, TextLineTopOf, &this->mLineVector);
    }
    void FindTopLine()
    {
        LineVector::iterator itr = this->FindLineAt(G(Fd::VertOffset));
        this->mTopLineVectorPos = (int)(itr - this->mLineVector.begin());
    }

    TextSegmentBase* FindSegment(int x, int y, bool lenient)
    {
        if (this->mLineVector.empty()) return NULL;
        if (lenient) ee_won_gui_text_find_clamp(&x, &y, G(Fd::TotalWidth), G(Fd::TotalHeight));
        if (this->mLastFindSeg != NULL) {
            TextLine* line = this->mLastFindSeg->mLine;
            if (ee_won_gui_text_find_cached(x, y, line->mx, line->my, line->mHeight, this->mLastFindSeg->mx, this->mLastFindSeg->mWidth))
                return this->mLastFindSeg;
        }
        LineVector::iterator lineItr = this->FindLineAt(y);
        TextLine* line = *lineItr;
        std::vector<int> aXs, aWidths;
        std::vector<TextSegmentBase*> aSegs;
        for (TextLine::SegmentList::iterator segItr = line->mSegmentList.begin(); segItr != line->mSegmentList.end(); ++segItr) {
            aXs.push_back((*segItr)->mx);
            aWidths.push_back((*segItr)->mWidth);
            aSegs.push_back(*segItr);
        }
        int64_t aPick = ee_won_gui_text_find_pick(x - line->mx, y, line->my, line->mHeight, aXs.empty() ? NULL : &aXs[0],
                                                  aWidths.empty() ? NULL : &aWidths[0], aXs.size(), lenient);
        if (aPick == -2) return NULL;
        TextSegmentBase* seg = aPick < 0 ? NULL : aSegs[(size_t)aPick];
        this->mLastFindSeg = seg;
        return seg;
    }

    void CheckSel(int x, int y, bool newStart)
    {
        TextSegmentBase* seg = this->FindSegment(x, y, true);
        if (seg == NULL) return;
        int selection = seg->CharAt(x - seg->mLine->mx);
        ee_won_gui_text_check_sel(&this->mSelStartChar, &this->mSelEndChar, &this->mBackwardsSel, selection, newStart);
        this->Invalidate();
    }

    bool TimerEvent()
    {
        if (this->mNeedRecalcWordWrap) {
            this->mDelayWordWrapTimeAccum++;
            int chars = 0;
            if (!this->mSegmentList.empty()) chars = this->mSegmentList.back()->mAbsEndChar;
            if (ee_won_gui_text_wrap_due(this->mDelayWordWrapTimeAccum, chars)) this->RecalcWordWrap(true);
        }
        if (!this->mSelecting) return this->mNeedRecalcWordWrap;
        int x, y; H(Op::GetMousePos, (intptr_t)&x, (intptr_t)&y);
        int v = G(Fd::VertOffset), h = G(Fd::HorzOffset);
        if (!ee_won_gui_text_drag_scroll(x, y, this->Width(), this->Height(), this->TextFlagSet(TextFlag_ScrollFullLine),
                                         G(Fd::DownScrollAmount), G(Fd::UpScrollAmount), &v, &h))
            return true;
        S(Fd::VertOffset, v); S(Fd::HorzOffset, h);
        this->AdjustPosition(false);
        this->ScrollTranslate(x, y);
        this->CheckSel(x, y, false);
        return true;
    }

    void MouseDown(int x, int y, int button)
    {
        H(Op::BaseMouseDown, x, y, button);
        if (button != 0) return;
        this->ScrollTranslate(x, y);
        H(Op::RequestFocus);
        if (this->mCurLink != NULL) {
            this->mCurLink->Pressed();
            this->Invalidate();
            this->mSelecting = false;
            return;
        }
        if (!this->TextFlagSet(TextFlag_AllowSelection))
            return;
        this->mSelecting = true;
        H(Op::RequestTimer, 1);
        this->CheckSel(x, y, (H(Op::KeyMod) & 1) == 0);
    }

    intptr_t SelectCursor() { return this->TextFlagSet(TextFlag_AllowSelection) ? (intptr_t)EeWonGuiCursor::IBeam : (intptr_t)EeWonGuiCursor::None; }

    void MouseExit()
    {
        H(Op::BaseMouseExit);
        if (this->mCurLink != NULL) {
            this->mCurLink->Released();
            this->mCurLink = NULL;
            this->Invalidate();
        }
        this->mSelecting = false;
    }

    void MouseMove(int x, int y)
    {
        H(Op::BaseMouseMove, x, y);
        this->ScrollTranslate(x, y);
        TextSegmentBase* seg = this->FindSegment(x, y, false);
        if (seg == this->mCurLink) return;
        if (seg != NULL && seg->IsLink()) {
            this->mCurLink = seg;
            H(Op::SetParentCursor, (intptr_t)EeWonGuiCursor::Hand);
        } else {
            this->mCurLink = NULL;
            H(Op::SetParentCursor, this->SelectCursor());
        }
    }

    void MouseEnter(int x, int y)
    {
        H(Op::BaseMouseEnter, x, y);
        H(Op::SetParentCursor, this->SelectCursor());
        this->MouseMove(x, y);
    }

    void MouseDrag(int x, int y)
    {
        H(Op::BaseMouseDrag, x, y);
        this->ScrollTranslate(x, y);
        if (this->mSelecting) this->CheckSel(x, y, false);
    }

    bool KeyDown(int key)
    {
        if (H(Op::BaseKeyDown, key)) return true;
        int v = G(Fd::VertOffset), h = G(Fd::HorzOffset);
        if (!ee_won_gui_text_key_scroll(key, &v, &h, G(Fd::DownScrollAmount), G(Fd::UpScrollAmount), G(Fd::ScrollHeight)))
            return false;
        S(Fd::VertOffset, v); S(Fd::HorzOffset, h);
        S(Fd::UserHasScrolled, 1);
        this->AdjustPosition(false);
        return true;
    }

    void CopySelection()
    {
        SegmentList::iterator itr = this->mSegmentList.begin();
        while (itr != this->mSegmentList.end()) {
            (*itr)->GetSelText(this);
            ++itr;
        }
        H(Op::SelTextCopy);
    }

    bool KeyChar(int key)
    {
        if (H(Op::BaseKeyChar, key)) return true;
        if (key == 3) { this->CopySelection(); return true; }
        return false;
    }

    void SetTextFlags(int flags, bool on)
    {
        bool wordWrap = this->TextFlagSet(TextFlag_WordWrap);
        if (on) this->mTextFlags |= flags; else this->mTextFlags &= ~flags;
        if (!wordWrap && this->TextFlagSet(TextFlag_WordWrap)) this->Virtual08();
        if (this->TextFlagSet(TextFlag_AllowSelection) || this->mSelStartChar == this->mSelEndChar)
            return;
        this->mSelecting = false;
        this->mSelStartChar = this->mSelEndChar = 0;
        this->Invalidate();
    }

    void SetMaxChars(int chars) { this->mMaxChars = chars; this->EnforceMaxChars(); this->AdjustPosition(false); }
    void SetMaxLines(int lines) { this->mMaxLines = lines; this->EnforceMaxLines(); this->AdjustPosition(false); }

    void AddSeg(TextSegmentBase* segment)
    {
        TextLine* line = this->mLineVector.back();
        EeWonTextLineM aMetrics = {line->mHeight, line->mBaseline, line->mDescent};
        int aTotal = G(Fd::TotalHeight), aTotal0 = aTotal;
        bool adjust = ee_won_gui_text_add_seg(&aMetrics, segment->mHeight, segment->mAscent, &aTotal);
        line->mHeight = aMetrics.height;
        line->mBaseline = aMetrics.baseline;
        line->mDescent = aMetrics.descent;
        if (adjust) {
            TextLine::SegmentList::iterator itr = line->mSegmentList.begin();
            while (itr != line->mSegmentList.end()) {
                TextSegmentBase* seg = *itr;
                seg->my = line->mBaseline - seg->mAscent;
                ++itr;
            }
        }
        segment->mx = line->mWidth;
        segment->my = line->mBaseline - segment->mAscent;
        segment->mLine = line;
        line->mSegmentList.push_back(segment);
        line->mWidth += segment->mWidth;
        if (aTotal != aTotal0) S(Fd::TotalHeight, aTotal); // the original wrote mTotalHeight as it grew
        this->AlignLine(line, this->mLineAlign);
    }
    void AlignLine(TextLine* line, int align) { line->mx = ee_won_gui_text_align_x(G(Fd::ScrollWidth), line->mWidth, align); }

    void AddSegment(TextSegmentBase* segment)
    {
        if (segment->mCpp) segment->mLineBreak = H(Op::SegLineBreak, segment->mCpp) != 0; // read once (see above)
        if (!this->mSegmentList.empty()) segment->mAbsStartChar = this->mSegmentList.back()->mAbsEndChar;
        segment->mAbsEndChar = segment->mAbsStartChar + segment->GetLength();
        bool bottom = this->ShouldScrollOnBottom();
        this->mSegmentList.push_back(segment);
        if (segment->mCpp) H(Op::SegHold, segment->mCpp);
        segment->CalcDimensions();
        this->AddSegmentPrv(segment);
        this->EnforceMaxChars();
        this->EnforceMaxLines();
        if (bottom) S(Fd::VertOffset, G(Fd::TotalHeight) + G(Fd::ExtraHeight) - G(Fd::ScrollHeight));
        this->AdjustPosition(false);
    }

    void EnforceInvariants()
    {
        S(Fd::TotalWidth, 0);
        S(Fd::TotalHeight, 0);
        this->mNumLines = 0;
        if (this->mLineVector.empty() || this->mSegmentList.empty()) return;
        std::vector<int> aWidths, aHeights, aYs, aStarts, aEnds;
        std::vector<char> aBreaks;
        for (LineVector::iterator lineItr = this->mLineVector.begin(); lineItr != this->mLineVector.end(); ++lineItr) {
            aWidths.push_back((*lineItr)->mWidth);
            aHeights.push_back((*lineItr)->mHeight);
            aYs.push_back((*lineItr)->my);
        }
        for (SegmentList::iterator itr = this->mSegmentList.begin(); itr != this->mSegmentList.end(); ++itr) {
            aStarts.push_back((*itr)->mAbsStartChar);
            aEnds.push_back((*itr)->mAbsEndChar);
            aBreaks.push_back((*itr)->mLineBreak);
        }
        int aTotals[3];
        int aVert = G(Fd::VertOffset);
        ee_won_gui_text_renumber(&aWidths[0], &aHeights[0], &aYs[0], aYs.size(), &aStarts[0], &aEnds[0], (const bool*)&aBreaks[0],
                                 aStarts.size(), &this->mSelStartChar, &this->mSelEndChar, &aVert, aTotals);
        S(Fd::VertOffset, aVert);
        size_t i = 0;
        for (LineVector::iterator lineItr = this->mLineVector.begin(); lineItr != this->mLineVector.end(); ++lineItr, ++i)
            (*lineItr)->my = aYs[i];
        i = 0;
        for (SegmentList::iterator itr = this->mSegmentList.begin(); itr != this->mSegmentList.end(); ++itr, ++i) {
            (*itr)->mAbsStartChar = aStarts[i];
            (*itr)->mAbsEndChar = aEnds[i];
        }
        S(Fd::TotalWidth, aTotals[0]);
        S(Fd::TotalHeight, aTotals[1]);
        this->mNumLines = aTotals[2];
    }

    struct TextRemoveSearch { const LineVector* mLines; TextLine* mFindLine; TextSegmentBase* mSegment; };
    static uint32_t TextRemoveFlagsOf(const void* inSearch, uintptr_t i)
    {
        const TextRemoveSearch* aSearch = static_cast<const TextRemoveSearch*>(inSearch);
        TextLine* aLine = (*aSearch->mLines)[i];
        uint32_t aFlags = 0;
        if (aLine == aSearch->mFindLine) aFlags |= 1;
        if (!aLine->mSegmentList.empty()) {
            aFlags |= 2;
            if (aLine->mSegmentList.back()->GetActual() == aSearch->mSegment) aFlags |= 4;
        }
        return aFlags;
    }
    void RemoveSegmentsUpTo(TextSegmentBase* segment)
    {
        TextLine* findLine = segment->mLine;
        this->mLastFindSeg = NULL;
        TextRemoveSearch aSearch = {&this->mLineVector, findLine, segment};
        LineVector::iterator lineItr = this->mLineVector.begin() +
            ee_won_gui_text_remove_stop(this->mLineVector.size(), TextRemoveFlagsOf, &aSearch);
        if (lineItr != this->mLineVector.end()) segment->mLine = *lineItr;
        this->mLineVector.erase(this->mLineVector.begin(), lineItr);
        TextSegmentBase* stop = NULL;
        if (!this->mLineVector.empty()) {
            TextLine* line = this->mLineVector.front();
            if (!line->mSegmentList.empty()) stop = line->mSegmentList.front()->GetActual();
        }
        SegmentList::iterator itr = this->mSegmentList.begin();
        while (itr != this->mSegmentList.end()) {
            TextSegmentBase* seg = *itr;
            if (seg == stop) break;
            if (seg == this->mCurLink) this->mCurLink = NULL;
            ++itr;
        }
        // the deliberate difference: a hovered piece of a removed segment goes with it
        if (this->mCurLink != NULL)
            for (SegmentList::iterator j = this->mSegmentList.begin(); j != itr; ++j)
                if (*j == this->mCurLink->GetActual()) { this->mCurLink = NULL; break; }
        this->ReleaseList(this->mSegmentList.begin(), itr);
        this->EnforceInvariants();
        this->AdjustPosition(false);
    }

    void EnforceMaxChars()
    {
        if (this->mSegmentList.empty()) return;
        int toRemove = ee_won_gui_text_chars_excess(this->mMaxChars, this->mSegmentList.back()->mAbsEndChar);
        if (toRemove <= 0) return;
        std::vector<int> aEnds;
        std::vector<TextSegmentBase*> aSegs;
        for (SegmentList::iterator itr = this->mSegmentList.begin(); itr != this->mSegmentList.end(); ++itr) {
            aEnds.push_back((*itr)->mAbsEndChar);
            aSegs.push_back(*itr);
        }
        this->RemoveSegmentsUpTo(aSegs[ee_won_gui_text_chars_cut(&aEnds[0], aEnds.size(), toRemove)]);
    }
    void EnforceMaxLines()
    {
        if (this->mSegmentList.empty()) return;
        int toRemove = ee_won_gui_text_lines_excess(this->mMaxLines, this->mNumLines);
        if (toRemove <= 0) return;
        std::vector<char> aBreaks;
        std::vector<TextSegmentBase*> aSegs;
        for (SegmentList::iterator itr = this->mSegmentList.begin(); itr != this->mSegmentList.end(); ++itr) {
            aBreaks.push_back((*itr)->mLineBreak);
            aSegs.push_back(*itr);
        }
        this->RemoveSegmentsUpTo(aSegs[ee_won_gui_text_lines_cut((const bool*)&aBreaks[0], aBreaks.size(), toRemove)]);
    }

    void MouseUp(int x, int y, int button)
    {
        H(Op::BaseMouseUp, x, y, button);
        if (button != 0) return;
        this->ScrollTranslate(x, y);
        TextSegmentBase* seg = this->FindSegment(x, y, false);
        bool pressed = false;
        if (this->mCurLink != NULL) pressed = this->mCurLink->Released();
        if (seg == this->mCurLink && pressed)
            H(Op::LinkActivated, this->mCurLink->GetActual()->mCpp);
        if (this->mCurLink != NULL) {
            if (seg == NULL || !seg->IsLink()) {
                this->mCurLink = NULL;
                H(Op::SetParentCursor, this->SelectCursor());
            } else this->mCurLink = seg;
            this->Invalidate();
        }
        this->mSelecting = false;
    }

    TextLine* AddLine(int height = 0, int baseline = 0)
    {
        TextLine* line = new TextLine;
        line->mHeight = height;
        line->mBaseline = baseline;
        if (!this->mLineVector.empty()) {
            TextLine* previous = this->mLineVector.back();
            line->my = previous->my + previous->mHeight;
        }
        this->mLineVector.push_back(line);
        S(Fd::TotalHeight, G(Fd::TotalHeight) + height);
        return line;
    }

    void LineBreak(int height)
    {
        TextSegmentBase* segment = new TextSegmentBase;
        segment->mHeight = segment->mAscent = height;
        segment->mLineBreak = true;
        this->AddSegment(segment);
    }
    void SetLineAlignment(int alignment)
    {
        if (alignment == this->mLineAlign) return;
        TextAlignInstruction* instruction = new TextAlignInstruction;
        instruction->mAlign = alignment;
        this->AddSegment(instruction);
    }

    void AddSegmentPrv(TextSegmentBase* segment)
    {
        if (this->mLineVector.empty()) this->AddLine();
        TextLine* line = this->mLineVector.back();
        segment->mLine = line;
        segment->PerformInstructions(this);
        if (ee_won_gui_text_seg_fits(this->Width(), line->mWidth, segment->mWidth, G(Fd::ScrollWidth),
                                     this->TextFlagSet(TextFlag_WordWrap), this->mHasBeenVisible)) {
            this->AddSeg(segment);
            if (line->mWidth > G(Fd::TotalWidth) && !this->TextFlagSet(TextFlag_WordWrap)) S(Fd::TotalWidth, line->mWidth);
        } else segment->Wrap(this, line);
        if (segment->mLineBreak) {
            this->mNumLines++;
            this->AddLine();
        }
    }
};

void TextAlignInstruction::PerformInstructions(TextArea* area) { area->mLineAlign = this->mAlign; }

void TextSegmentBase::Wrap(TextArea* theTextArea, TextLine* theLine)
{
    if (!theLine->mSegmentList.empty())
        theLine = theTextArea->AddLine();
    theLine->mWidth = theTextArea->mWrapIndentSize;
    theTextArea->AddSeg(this);
}

void TextSegmentBase::GetSelText(TextArea* theTextArea)
{
    if (ee_won_gui_text_sel_break(false, this->mLineBreak, theTextArea->mSelStartChar, theTextArea->mSelEndChar,
                                  this->mAbsStartChar, this->mAbsEndChar))
        H(Op::SelTextBreak);
}

void TextSegment::CalcDimensions()
{
    int m[3];
    H(Op::SegMeasure, this->mCpp, (intptr_t)m);
    this->mWidth = m[0];
    this->mHeight = m[1];
    this->mAscent = m[2];
}

void TextSegment::GetSelText(TextArea* theTextArea)
{
    int anOffset, aLength;
    if (!ee_won_gui_text_sel_part(theTextArea->mSelStartChar, theTextArea->mSelEndChar, this->mAbsStartChar,
                                  this->mAbsEndChar, &anOffset, &aLength))
        return;
    H(Op::SelTextAppend, this->mCpp, anOffset, aLength);
    if (ee_won_gui_text_sel_break(true, this->mLineBreak, theTextArea->mSelStartChar, theTextArea->mSelEndChar,
                                  this->mAbsStartChar, this->mAbsEndChar))
        H(Op::SelTextBreak);
}

struct TextSegmentMeasure { TextSegment* mSeg; int mLength; }; // the font and the text (its length read once)
static int32_t TextRestWidthOf(const void* inMeasure, int32_t inStart)
{
    const TextSegmentMeasure* m = (const TextSegmentMeasure*)inMeasure;
    return (int32_t)H(Op::SegStringWidth, m->mSeg->mCpp, inStart, m->mLength - inStart);
}

void TextSegment::Wrap(TextArea* theTextArea, TextLine* theLine)
{
    this->mLine = NULL;
    int aLength = this->TextLength();
    std::vector<int> aWidths(aLength);
    for (int i = 0; i < aLength; i++)
        aWidths[i] = (int)H(Op::SegCharWidth, this->mCpp, i);
    std::vector<uint32_t> aText;
    H(Op::SegTextTake, this->mCpp, (intptr_t)&aText);
    aText.resize(aLength);
    std::vector<EeWonTextWrapOp> anOps(3 * aLength + 8);
    TextSegmentMeasure aMeasure = {this, aLength};
    bool aLeft = theTextArea->mLineAlign == HorzAlign_Left;
    size_t aCount = ee_won_gui_text_wrap(aLength ? &aText[0] : NULL, (uint16_t)aLength, sizeof(uint32_t),
                                         aLength ? &aWidths[0] : NULL, G(Fd::ScrollWidth), theLine->mWidth,
                                         theLine->mSegmentList.empty(), aLeft, theTextArea->mWrapIndentSize,
                                         TextRestWidthOf, &aMeasure, &anOps[0], anOps.size());
    TextLine* aLine = theLine;
    for (size_t i = 0; i < aCount && i < anOps.size(); i++) {
        const EeWonTextWrapOp& anOp = anOps[i];
        if (anOp.kind == 1) {
            aLine = theTextArea->AddLine(this->mHeight, this->mAscent);
            if (aLeft)
                aLine->mWidth = anOp.width;
            continue;
        }
        theTextArea->AddSeg(new TextSegmentPiece(this, anOp.start, anOp.end, anOp.width));
        if (this->mLine == NULL)
            this->mLine = aLine;
    }
}

struct Measure { TextSegment* mSeg; };
static int32_t MeasureString(void* ctx, int32_t offset, int32_t length)
{
    return (int32_t)H(Op::SegStringWidth, ((Measure*)ctx)->mSeg->mCpp, offset, length);
}
static int32_t MeasureChar(void* ctx, int32_t index)
{
    return (int32_t)H(Op::SegCharWidth, ((Measure*)ctx)->mSeg->mCpp, index);
}

void TextSegment::Paint(TextArea* theTextArea, int theStartChar, int theEndChar, int xpos, int ypos, int lineHeight)
{
    EeWonSegmentPaint s;
    s.area_sel_start = theTextArea->mSelStartChar;
    s.area_sel_end = theTextArea->mSelEndChar;
    s.abs_start = this->mAbsStartChar;
    s.start = theStartChar;
    s.end = theEndChar;
    s.x = xpos;
    s.y = ypos;
    s.line_height = lineHeight;
    s.color = (uint32_t)H(Op::SegColor, this->mCpp);
    H(Op::SelColor, (intptr_t)&s.sel);
    Measure aMeasure = {this};
    EeWonTextMeasure m;
    m.ctx = &aMeasure;
    m.string_width = MeasureString;
    m.char_width = MeasureChar;
    EeWonDrawOp ops[48];
    size_t n = ee_won_gui_draw_segment(&s, &m, ops, 48);
    for (size_t i = 0; i < n && i < 48; i++)
        H(Op::Draw, kG, (intptr_t)&ops[i], this->mCpp);
}
void TextSegment::Paint(TextArea* theTextArea)
{
    this->Paint(theTextArea, 0, this->TextLength(), this->mx, this->my, this->mLine->mHeight);
}
int TextSegment::CharAt(int theWidth, int theStartChar, int theEndChar)
{
    Measure aMeasure = {this};
    return ee_won_gui_text_char_at(theWidth, theStartChar, theEndChar, this->mAbsStartChar,
                                   [](const void* ctx, int32_t pos) -> int32_t { return MeasureChar((void*)ctx, pos); },
                                   &aMeasure);
}
int TextSegment::CharAt(int x) { return this->CharAt(x - this->mx, 0, this->TextLength()); }
void TextSegmentPiece::Paint(TextArea* theTextArea)
{
    this->mSeg->Paint(theTextArea, this->mStartChar, this->mEndChar, this->mx, this->my, this->mLine->mHeight);
}
int TextSegmentPiece::CharAt(int x) { return this->mSeg->CharAt(x - this->mx, this->mStartChar, this->mEndChar); }

static void FreeArena()
{
    for (size_t i = 0; i < sSegArena.size(); i++) delete sSegArena[i];
    for (size_t i = 0; i < sLineArena.size(); i++) delete sLineArena[i];
    sSegArena.clear();
    sLineArena.clear();
}

} // namespace orig

// ---- the two sides

static orig::TextArea* sOrig;
static const EeWonText* sRust;

static void OrigAdjust(bool b) { sOrig->AdjustPosition(b); }
static void OrigReset(bool b) { sOrig->Reset(b); }
static void OrigAddSegment(intptr_t inSeg)
{
    orig::TextSegment* s = new orig::TextSegment;
    s->mCpp = inSeg;
    sOrig->AddSegment(s);
}
static void RustAdjust(bool b) { ee_won_text_call(sRust, EeWonTextCall::AdjustPosition, b, 0, 0); }
static void RustReset(bool b) { ee_won_text_call(sRust, EeWonTextCall::Reset, b, 0, 0); }
static void RustAddSegment(intptr_t inSeg) { ee_won_text_call(sRust, EeWonTextCall::AddSegment, inSeg, 0, 0); }

// one inward call, drawn from the driver's stream (the same on both sides)
struct Call {
    int mKind;
    int a, b, c;
    intptr_t mSeg;
};
enum {
    kAddText, kAddLink, kLineBreak, kAlign, kSize, kVisible, kAdjust, kPaint, kEnter, kExit, kDown, kUp, kMove,
    kDrag, kTimer, kKeyChar, kKeyDown, kFlags, kMaxChars, kMaxLines, kIndent, kCheckSel, kReset, kNumLines, kCalls
};
static const char* kCallNames[kCalls] = {"add-text", "add-link", "line-break", "align", "size", "visible", "adjust",
                                         "paint", "enter", "exit", "down", "up", "move", "drag", "timer", "key-char",
                                         "key-down", "flags", "max-chars", "max-lines", "indent", "check-sel", "reset",
                                         "num-lines"};

static unsigned sDriverSeed;
static int sLastX, sLastY; // the last mouse point (reset with each sequence)
static unsigned DR(unsigned n) { sDriverSeed = sDriverSeed * 69069u + 1u; return n ? (sDriverSeed >> 16) % n : 0; }

static Call MakeCall()
{
    static const int kWeights[kCalls] = {14, 10, 4, 2, 3, 2, 2, 3, 2, 2, 4, 4, 5, 3, 3, 2, 3, 2, 2, 2, 1, 2, 1, 1};
    int aTotal = 0;
    for (int i = 0; i < kCalls; i++) aTotal += kWeights[i];
    int r = (int)DR(aTotal), k = 0;
    while (r >= kWeights[k]) r -= kWeights[k++];
    Call c = {k, 0, 0, 0, 0};
    int w = sFields[(int)Fd::Width] + 40, h = sFields[(int)Fd::Height] + 40;
    bool aSame = DR(2) == 0;
    switch (k) {
    case kLineBreak: c.a = DR(3) == 0 ? 0 : (int)DR(14); break;
    case kAlign: c.a = (int)DR(3); break;
    case kVisible: c.a = DR(4) != 0; break;
    case kAdjust: c.a = DR(4) == 0; break;
    case kEnter: case kMove: case kDrag: case kDown: case kUp:
        if (aSame) { c.a = sLastX; c.b = sLastY; } else if (DR(2)) { c.a = (int)DR(80); c.b = (int)DR(50); } else { c.a = (int)DR(w) - 20; c.b = (int)DR(h) - 20; }
        sLastX = c.a; sLastY = c.b;
        if (k == kDown || k == kUp) c.c = DR(5) == 0 ? 1 : 0;
        break;
    case kKeyChar: c.a = DR(2) ? 3 : 'a'; break;
    case kKeyDown: { static const int kKeys[] = {0x21, 0x22, 0x25, 0x26, 0x27, 0x28, 0x41}; c.a = kKeys[DR(7)]; break; }
    case kFlags: c.a = 1 << DR(6); c.b = DR(2); break;
    case kMaxChars: c.a = DR(3) == 0 ? 0 : (int)DR(300); break;
    case kMaxLines: c.a = DR(3) == 0 ? 0 : (int)DR(10); break;
    case kIndent: c.a = (int)DR(24); break;
    case kCheckSel: c.a = (int)DR(w) - 20; c.b = (int)DR(h) - 20; c.c = DR(2); break;
    case kReset: c.a = DR(3) == 0; break;
    default: break;
    }
    return c;
}

static void Run(bool inRust, const Call& c)
{
    char aLine[100];
    snprintf(aLine, sizeof(aLine), "call %s %d %d %d", kCallNames[c.mKind], c.a, c.b, c.c);
    sTrace.push_back(aLine);
    intptr_t r = 0;
    if (c.mKind == kAddText || c.mKind == kAddLink) {
        intptr_t s = NewSeg(c.mKind == kAddLink);
        if (inRust) RustAddSegment(s); else OrigAddSegment(s);
        DropCreatorRef(s);
        return;
    }
    if (inRust) {
        const EeWonText* t = sRust;
        switch (c.mKind) {
        case kLineBreak: ee_won_text_call(t, EeWonTextCall::LineBreak, c.a, 0, 0); break;
        case kAlign: ee_won_text_call(t, EeWonTextCall::SetLineAlignment, c.a, 0, 0); break;
        case kSize: ee_won_text_call(t, EeWonTextCall::SizeChanged, 0, 0, 0); break;
        case kVisible: ee_won_text_call(t, EeWonTextCall::NotifyRootVisibility, c.a, 0, 0); break;
        case kAdjust: ee_won_text_call(t, EeWonTextCall::AdjustPosition, c.a, 0, 0); break;
        case kPaint: ee_won_text_call(t, EeWonTextCall::ScrollPaint, orig::kG, 0, 0); break;
        case kEnter: ee_won_text_call(t, EeWonTextCall::MouseEnter, c.a, c.b, 0); break;
        case kExit: ee_won_text_call(t, EeWonTextCall::MouseExit, 0, 0, 0); break;
        case kDown: ee_won_text_call(t, EeWonTextCall::MouseDown, c.a, c.b, c.c); break;
        case kUp: ee_won_text_call(t, EeWonTextCall::MouseUp, c.a, c.b, c.c); break;
        case kMove: ee_won_text_call(t, EeWonTextCall::MouseMove, c.a, c.b, 0); break;
        case kDrag: ee_won_text_call(t, EeWonTextCall::MouseDrag, c.a, c.b, 0); break;
        case kTimer: r = ee_won_text_call(t, EeWonTextCall::TimerEvent, 0, 0, 0); break;
        case kKeyChar: r = ee_won_text_call(t, EeWonTextCall::KeyChar, c.a, 0, 0); break;
        case kKeyDown: r = ee_won_text_call(t, EeWonTextCall::KeyDown, c.a, 0, 0); break;
        case kFlags: ee_won_text_call(t, EeWonTextCall::SetTextFlags, c.a, c.b, 0); break;
        case kMaxChars: ee_won_text_call(t, EeWonTextCall::SetMaxChars, c.a, 0, 0); break;
        case kMaxLines: ee_won_text_call(t, EeWonTextCall::SetMaxLines, c.a, 0, 0); break;
        case kIndent: ee_won_text_call(t, EeWonTextCall::SetWrapIndentSize, c.a, 0, 0); break;
        case kCheckSel: ee_won_text_call(t, EeWonTextCall::CheckSel, c.a, c.b, c.c); break;
        case kReset: ee_won_text_call(t, EeWonTextCall::Reset, c.a, 0, 0); break;
        case kNumLines: r = ee_won_text_call(t, EeWonTextCall::GetNumLines, 0, 0, 0); break;
        }
    } else {
        orig::TextArea* t = sOrig;
        switch (c.mKind) {
        case kLineBreak: t->LineBreak(c.a); break;
        case kAlign: t->SetLineAlignment(c.a); break;
        case kSize: t->Virtual08(); break;
        case kVisible: t->NotifyRootVisibility(c.a != 0); break;
        case kAdjust: t->AdjustPosition(c.a != 0); break;
        case kPaint: t->ScrollPaint(); break;
        case kEnter: t->MouseEnter(c.a, c.b); break;
        case kExit: t->MouseExit(); break;
        case kDown: t->MouseDown(c.a, c.b, c.c); break;
        case kUp: t->MouseUp(c.a, c.b, c.c); break;
        case kMove: t->MouseMove(c.a, c.b); break;
        case kDrag: t->MouseDrag(c.a, c.b); break;
        case kTimer: r = t->TimerEvent(); break;
        case kKeyChar: r = t->KeyChar(c.a); break;
        case kKeyDown: r = t->KeyDown(c.a); break;
        case kFlags: t->SetTextFlags(c.a, c.b != 0); break;
        case kMaxChars: t->SetMaxChars(c.a); break;
        case kMaxLines: t->SetMaxLines(c.a); break;
        case kIndent: t->mWrapIndentSize = c.a; break;
        case kCheckSel: t->CheckSel(c.a, c.b, c.c != 0); break;
        case kReset: t->Reset(c.a != 0); break;
        case kNumLines: r = (intptr_t)t->mLineVector.size(); break;
        }
    }
    sTrace.back() += " -> " + std::to_string((long long)r);
}

static void ResetHost(unsigned inSeed)
{
    sSeed = inSeed;
    sSegs.clear();
    sNextSeg = 0;
    memset(sFields, 0, sizeof(sFields));
    sFields[(int)Fd::ScrollWidth] = 30 + (int)R(260);
    sFields[(int)Fd::ScrollHeight] = 20 + (int)R(180);
    sFields[(int)Fd::Width] = sFields[(int)Fd::ScrollWidth] + 10;
    sFields[(int)Fd::Height] = sFields[(int)Fd::ScrollHeight] + 10;
    sFields[(int)Fd::UpScrollAmount] = sFields[(int)Fd::DownScrollAmount] = 10;
}

static bool RefsBack()
{
    for (std::map<intptr_t, HostSeg>::const_iterator i = sSegs.begin(); i != sSegs.end(); ++i)
        if (i->second.mRefs != 0) return false;
    return true;
}

// one sequence on one side; its trace
static std::vector<std::string> RunSide(bool inRust, unsigned inSeq, int inCalls)
{
    sTrace.clear();
    ResetHost(inSeq * 2654435761u + 17u);
    sDriverSeed = inSeq * 40503u + 5u;
    TraceFields();
    sLastX = sLastY = 0;
    if (inRust) {
        sSide.mAdjust = RustAdjust; sSide.mReset = RustReset; sSide.mAddSegment = RustAddSegment; sSide.mRust = true;
        EeWonText* t = ee_won_text_new((void*)0x51, ScriptedHost);
        sRust = t;
        ee_won_text_call(t, EeWonTextCall::Reset, 0, 0, 0);
        for (int i = 0; i < inCalls; i++) Run(true, MakeCall());
        sTrace.push_back("free");
        ee_won_text_free(t);
        sRust = NULL;
    } else {
        sSide.mAdjust = OrigAdjust; sSide.mReset = OrigReset; sSide.mAddSegment = OrigAddSegment; sSide.mRust = false;
        sOrig = new orig::TextArea;
        sOrig->Reset(false);
        for (int i = 0; i < inCalls; i++) Run(false, MakeCall());
        sTrace.push_back("free");
        delete sOrig;
        sOrig = NULL;
        orig::FreeArena();
    }
    char aLine[40];
    snprintf(aLine, sizeof(aLine), "refs %s", RefsBack() ? "back" : "LEAKED");
    sTrace.push_back(aLine);
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    const int kSequences = 20000;
    const int kCalls = aPrint ? 16 : 40; // the golden's sequences are shorter
    size_t aLines = 0;
    std::set<std::string> aSeen;
    std::set<int> aCallsMet;
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
        // a new op or call kind makes the sequence part of the golden
        bool aNew = seq < 40;
        for (std::set<int>::const_iterator i = sOpsMet.begin(); i != sOpsMet.end(); ++i) {
            std::string k = "op" + std::to_string(*i);
            if (aSeen.insert(k).second) aNew = true;
        }
        for (size_t i = 0; i < a.size(); i++)
            if (a[i].compare(0, 5, "call ") == 0) {
                std::string k = a[i].substr(0, a[i].find(' ', 5));
                if (aSeen.insert(k).second) aNew = true;
            }
        if (aPrint && aNew) {
            printf("seq %d\n", seq);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (!aPrint)
        printf("won_textobj_test: %d sequences, %u trace lines, %u ops and calls met, %d links activated, %d differing, "
               "%d checks, %d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), sLinksActivated / 2,
               aDiffering, sChecks, sFailures);
    return sFailures;
}
