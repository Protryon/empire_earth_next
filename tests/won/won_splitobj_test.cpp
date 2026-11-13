// Splitter as a whole object (R6 WON round 22): the original Splitter.cpp's methods, the statements copied from the
// C++ before the port with every call on a C++ object (the base Container, the panes, bars, drag handles and
// buttons, the events) replaced by the host op that makes it (GuiHost.cpp runs the same calls in the same order),
// against ee_won's Rust Splitter (splitobj.rs) over one scripted host. The host keeps the components (sizes,
// positions, visibility, enabled state, flags, default cursors, control ids, checks, references), the splitter's
// mouse child and its grandchild, and the events.
//
// Each of 20,000 sequences creates the splitter and makes 40 random inward calls (panes, bars of every type with
// and without their own drag handles, invisible bars, maximise buttons, minimums, percentages, maximise and
// restore, positions with and without remembering, size changes, the mouse on bars and panes, button events and
// other events). The traces (every op with its arguments and answer) must be the same, and every reference the
// splitter takes must be given back when it goes. The first 40 sequences and every later one that makes an op or a
// call none before made are rust/crates/ee_won/src/golden_splitobj.txt, which the Rust tests replay; `--print`
// prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include "rust/ee_won.h"

typedef EeWonGuiOp Op;
typedef EeWonGuiField Fd;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }

struct HostComp {
    int w, h, x, y;
    bool visible, enabled, hasCursor, checked;
    int flags, controlId, held;
    intptr_t mouseChild;
};
struct HostEvent {
    bool press;
    int controlId;
    intptr_t button;
};
static std::map<intptr_t, HostComp> sComps;
static std::map<intptr_t, HostEvent> sEvents;
static intptr_t sNextId;
static int sWidth, sHeight;
static intptr_t sMouseChild;
static std::vector<std::string> sTrace;
static std::set<int> sOpsMet;

static intptr_t NewComp()
{
    HostComp c;
    c.w = 2 + (int)R(12); c.h = 2 + (int)R(12); c.x = c.y = 0;
    c.visible = R(6) != 0; c.enabled = true; c.hasCursor = R(4) == 0; c.checked = false;
    c.flags = R(3) == 0 ? 0x20 : 0; c.controlId = 0; c.held = 0; c.mouseChild = 0;
    intptr_t id = 0x2000 + (++sNextId);
    sComps[id] = c;
    return id;
}

static void Line(const char* f, long long a, long long b, long long c, long long d, long long e)
{
    char aLine[300];
    snprintf(aLine, sizeof(aLine), f, a, b, c, d, e);
    sTrace.push_back(aLine);
}

static intptr_t ScriptedHost(void* inComp, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)inComp; (void)d;
    sOpsMet.insert((int)inOp);
    intptr_t r = 0;
    int n = (int)inOp;
    switch (inOp) {
    case Op::Get: return (Fd)a == Fd::Width ? sWidth : (Fd)a == Fd::Height ? sHeight : 0; // pure reads: not traced
    case Op::ChildSetPosSize: {
        const EeWonRect4* q = (const EeWonRect4*)b;
        HostComp& k = sComps[a];
        k.x = q->x; k.y = q->y; k.w = q->w; k.h = q->h;
        char aLine[200];
        snprintf(aLine, sizeof(aLine), "%d pos-size %llx %d %d %d %d", n, (long long)a, q->x, q->y, q->w, q->h);
        sTrace.push_back(aLine);
        return 0;
    }
    case Op::ChildSetEnabled: sComps[a].enabled = b != 0; break;
    case Op::ChildSetVisible: sComps[a].visible = b != 0; break;
    case Op::ChildIsVisible: r = sComps[a].visible; break;
    case Op::ChildWidth: r = sComps[a].w; break;
    case Op::ChildHeight: r = sComps[a].h; break;
    case Op::ChildLeft: r = sComps[a].x; break;
    case Op::ChildTop: r = sComps[a].y; break;
    case Op::ChildHold: sComps[a].held++; break;
    case Op::ChildRelease: sComps[a].held--; break;
    case Op::ChildFlagSet: r = (sComps[a].flags & b) != 0; break;
    case Op::ChildSetFlags: if (c) sComps[a].flags |= (int)b; else sComps[a].flags &= ~(int)b; break;
    case Op::ChildHasDefaultCursor: r = sComps[a].hasCursor; break;
    case Op::ChildSetDefaultCursor: sComps[a].hasCursor = b != 0; break;
    case Op::ChildMouseChild: r = sComps[a].mouseChild; break;
    case Op::AddChild: break;
    case Op::MouseChild: r = sMouseChild; break;
    case Op::ButtonControlId: r = sComps[a].controlId; break;
    case Op::ButtonSetControlId: sComps[a].controlId = (int)b; break;
    case Op::ButtonSetCheck: sComps[a].checked = b != 0; break;
    case Op::ButtonIsChecked: r = sComps[a].checked; break;
    case Op::NewDragComponent: r = NewComp(); sComps[r].flags = 0; sComps[r].hasCursor = false; break;
    case Op::NewSizedComponent: r = NewComp(); sComps[r].w = sComps[r].h = (int)a; sComps[r].flags = 0; sComps[r].hasCursor = false; sComps[r].held = 1; break;
    case Op::EventIsButtonPress: r = sEvents[a].press; break;
    case Op::EventControlId: r = sEvents[a].controlId; break;
    case Op::EventButton: r = sEvents[a].button; break;
    default: break;
    }
    Line("%lld %llx %llx %lld -> %llx", n, (long long)a, (long long)b, (long long)c, (long long)r);
    return r;
}

static intptr_t H(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0)
{
    return ScriptedHost((void*)0x54, inOp, a, b, c, 0);
}

// ---- the original (Splitter.cpp; the base Container, its children and the events through the host)

namespace orig {
enum { Splitter_LeftRight = 5, Splitter_UpDown = 10, ComponentFlag_ParentDrag = 0x20 };
static EeWonSplit SplitOf(int x, int y, int left, int right, int top, int bottom)
{
    EeWonSplit aSplit = {x, y, left, right, top, bottom};
    return aSplit;
}
struct Splitter {
    struct Quadrant { intptr_t mComponent; int mPosition; Quadrant(intptr_t c, int p) : mComponent(c), mPosition(p) {} };
    struct SplitterParam { intptr_t mComponent; int mType; intptr_t mDragComponent; SplitterParam(intptr_t c, int t) : mComponent(c), mType(t), mDragComponent(0) {} };
    std::list<Quadrant> mQuadrantList;
    std::list<SplitterParam> mSplitterList;
    std::list<intptr_t> mButtonList;
    bool mLeftRightDrag, mUpDownDrag;
    int mDragX, mDragY;
    int mMinLeft, mMinRight, mMinTop, mMinBottom;
    bool mUpDownMax, mLeftRightMax;
    double mLeftPercent, mTopPercent;
    bool mSplitCoordsValid;
    int mSplitX, mSplitY, mSplitLeft, mSplitRight, mSplitTop, mSplitBottom;
    int mLeftRightSplitterWidth, mUpDownSplitterHeight;
    bool mHaveMaxQuadrant;
    int mCurMaxQuadrant;

    Splitter()
    {
        this->mSplitLeft = 0; this->mSplitRight = 0; this->mSplitTop = 0; this->mSplitBottom = 0;
        this->mUpDownSplitterHeight = 0; this->mLeftRightSplitterWidth = 0;
        this->mHaveMaxQuadrant = false;
        this->mMinLeft = this->mMinRight = this->mMinTop = this->mMinBottom = 0;
        this->mLeftPercent = this->mTopPercent = 50;
        this->mUpDownMax = false; this->mLeftRightMax = false;
        this->mLeftRightDrag = false; this->mUpDownDrag = false;
        this->mSplitX = 0; this->mSplitY = 0;
        this->mSplitCoordsValid = false;
        this->mDragX = this->mDragY = 0;
        this->mCurMaxQuadrant = 0;
    }
    ~Splitter()
    {
        for (std::list<intptr_t>::iterator i = this->mButtonList.begin(); i != this->mButtonList.end(); ++i) H(Op::ChildRelease, *i);
        for (std::list<SplitterParam>::iterator i = this->mSplitterList.begin(); i != this->mSplitterList.end(); ++i)
            if (i->mDragComponent) H(Op::ChildRelease, i->mDragComponent);
    }
    int Width() { return sWidth; }
    int Height() { return sHeight; }
    void SetPosSize(intptr_t c, const EeWonRect4& r) { H(Op::ChildSetPosSize, c, (intptr_t)&r); }

    void RegisterMaxButton(intptr_t theButton, int thePosition)
    {
        H(Op::ButtonSetControlId, theButton, thePosition + 100005);
        H(Op::ChildHold, theButton); // mButtonList.push_back(SmartPtr)
        this->mButtonList.push_back(theButton);
    }
    void AddQuadrant(intptr_t theComponent, int thePosition)
    {
        this->mQuadrantList.push_back(Quadrant(theComponent, thePosition));
        H(Op::AddChild, theComponent);
    }
    void AddSplitterPrv(intptr_t theSplitter, int theType, intptr_t theDragComponent)
    {
        this->mSplitterList.push_back(SplitterParam(theSplitter, theType));
        SplitterParam& aParam = this->mSplitterList.back();
        H(Op::ChildSetFlags, theDragComponent, ComponentFlag_ParentDrag, 1);
        if (!H(Op::ChildHasDefaultCursor, theDragComponent)) {
            switch (ee_won_gui_split_cursor(theType)) {
            case 1: H(Op::ChildSetDefaultCursor, theDragComponent, (intptr_t)EeWonGuiCursor::NorthSouthEastWest); break;
            case 2: H(Op::ChildSetDefaultCursor, theDragComponent, (intptr_t)EeWonGuiCursor::EastWest); break;
            case 3: H(Op::ChildSetDefaultCursor, theDragComponent, (intptr_t)EeWonGuiCursor::NorthSouth); break;
            }
        }
        H(Op::ChildHold, theDragComponent);
        aParam.mDragComponent = theDragComponent;
        H(Op::AddChild, theSplitter);
        if (this->mLeftRightSplitterWidth == 0 && (theType & Splitter_LeftRight))
            this->mLeftRightSplitterWidth = (int)H(Op::ChildWidth, theSplitter);
        if (this->mUpDownSplitterHeight == 0 && (theType & Splitter_UpDown))
            this->mUpDownSplitterHeight = (int)H(Op::ChildHeight, theSplitter);
    }
    void AddSplitter(intptr_t c, int t) { this->AddSplitterPrv(c, t, c); }
    void AddComplexSplitter(intptr_t theContainer, int theType, intptr_t theDragComponent)
    {
        if (theDragComponent == 0)
            theDragComponent = H(Op::NewDragComponent, theContainer);
        this->AddSplitterPrv(theContainer, theType, theDragComponent);
    }
    void AddInvisibleSplitter(int theType, int theSize)
    {
        intptr_t aComp = H(Op::NewSizedComponent, theSize);
        this->AddSplitter(aComp, theType);
        H(Op::ChildRelease, aComp);
    }
    void MaxQuadrant(int thePosition)
    {
        this->mHaveMaxQuadrant = true;
        this->mCurMaxQuadrant = thePosition;
        int x, y;
        ee_won_gui_split_max(thePosition, this->Width(), this->Height(), this->mLeftRightSplitterWidth,
                             this->mUpDownSplitterHeight, this->mSplitX, this->mSplitY, &x, &y, &this->mLeftRightMax,
                             &this->mUpDownMax);
        for (std::list<intptr_t>::iterator i = this->mButtonList.begin(); i != this->mButtonList.end(); ++i) {
            int aPosition = ee_won_gui_split_button_quadrant((int)H(Op::ButtonControlId, *i));
            H(Op::ButtonSetCheck, *i, aPosition == thePosition);
        }
        for (std::list<SplitterParam>::iterator i = this->mSplitterList.begin(); i != this->mSplitterList.end(); ++i) {
            if (i->mType & thePosition) H(Op::ChildSetEnabled, i->mDragComponent, 0);
            else H(Op::ChildSetEnabled, i->mDragComponent, 1);
        }
        for (std::list<Quadrant>::iterator i = this->mQuadrantList.begin(); i != this->mQuadrantList.end(); ++i) {
            if (i->mPosition & thePosition) H(Op::ChildSetVisible, i->mComponent, 1);
            else H(Op::ChildSetVisible, i->mComponent, 0);
        }
        this->SetSplitterPos(x, y, false);
    }
    void UnMaxQuadrant()
    {
        this->mHaveMaxQuadrant = false;
        this->mUpDownMax = this->mLeftRightMax = false;
        for (std::list<SplitterParam>::iterator i = this->mSplitterList.begin(); i != this->mSplitterList.end(); ++i)
            H(Op::ChildSetEnabled, i->mDragComponent, 1);
        for (std::list<Quadrant>::iterator i = this->mQuadrantList.begin(); i != this->mQuadrantList.end(); ++i)
            H(Op::ChildSetVisible, i->mComponent, 1);
        int x = ee_won_gui_split_at(this->Width(), this->mLeftPercent, 0);
        int y = ee_won_gui_split_at(this->Height(), this->mTopPercent, 0);
        this->SetSplitterPos(x, y, false);
    }
    void CalcQuadrants()
    {
        for (std::list<Quadrant>::iterator i = this->mQuadrantList.begin(); i != this->mQuadrantList.end(); ++i) {
            EeWonRect4 r = ee_won_gui_split_pane(i->mPosition, this->Width(), this->Height(),
                                                 SplitOf(this->mSplitX, this->mSplitY, this->mSplitLeft, this->mSplitRight,
                                                         this->mSplitTop, this->mSplitBottom));
            this->SetPosSize(i->mComponent, r);
        }
    }
    void CalcSplitterPositions()
    {
        for (std::list<SplitterParam>::iterator i = this->mSplitterList.begin(); i != this->mSplitterList.end(); ++i) {
            if (H(Op::ChildIsVisible, i->mComponent)) {
                EeWonRect4 r = ee_won_gui_split_bar(i->mType, this->Width(), this->Height(),
                                                    SplitOf(this->mSplitX, this->mSplitY, this->mSplitLeft,
                                                            this->mSplitRight, this->mSplitTop, this->mSplitBottom));
                this->SetPosSize(i->mComponent, r);
            }
        }
    }
    void SetSplitterPos(int x, int y, bool rememberPercentage)
    {
        EeWonSplit aSplit;
        if (!ee_won_gui_split_set(x, y, this->mLeftRightMax, this->mUpDownMax, this->mMinLeft, this->mMinRight,
                                  this->mMinTop, this->mMinBottom, this->Width(), this->Height(), this->mSplitCoordsValid,
                                  this->mSplitX, this->mSplitY, this->mLeftRightSplitterWidth, this->mUpDownSplitterHeight,
                                  &aSplit))
            return;
        this->mSplitCoordsValid = true;
        this->mSplitX = aSplit.x; this->mSplitY = aSplit.y;
        this->mSplitLeft = aSplit.left; this->mSplitRight = aSplit.right;
        this->mSplitTop = aSplit.top; this->mSplitBottom = aSplit.bottom;
        if (rememberPercentage) {
            ee_won_gui_split_percent(aSplit.x, this->Width(), &this->mLeftPercent);
            ee_won_gui_split_percent(aSplit.y, this->Height(), &this->mTopPercent);
        }
        this->CalcSplitterPositions();
        this->CalcQuadrants();
        H(Op::Invalidate);
    }
    void Virtual08()
    {
        H(Op::BaseSizeChanged);
        this->mSplitCoordsValid = false;
        int y;
        int x = ee_won_gui_split_layout(this->mHaveMaxQuadrant, this->mCurMaxQuadrant, this->mLeftPercent,
                                        this->mTopPercent, this->mLeftRightSplitterWidth, this->mUpDownSplitterHeight,
                                        this->Width(), this->Height(), &y);
        this->SetSplitterPos(x, y, false);
    }
    void MouseDown(int x, int y, int theButton)
    {
        H(Op::BaseMouseDown, x, y, theButton);
        intptr_t aMouseChild = H(Op::MouseChild);
        if (theButton != 0 || aMouseChild == 0)
            return;
        this->mLeftRightDrag = this->mUpDownDrag = false;
        this->mDragX = x - (int)H(Op::ChildLeft, aMouseChild);
        this->mDragY = y - (int)H(Op::ChildTop, aMouseChild);
        intptr_t aGrandChild = H(Op::ChildMouseChild, aMouseChild);
        if (aGrandChild == 0) {
            if (!H(Op::ChildFlagSet, aMouseChild, ComponentFlag_ParentDrag))
                return;
        } else if (!H(Op::ChildFlagSet, aGrandChild, ComponentFlag_ParentDrag))
            return;
        for (std::list<SplitterParam>::iterator i = this->mSplitterList.begin(); i != this->mSplitterList.end(); ++i) {
            if (i->mComponent == aMouseChild) {
                if (i->mType & Splitter_UpDown) this->mUpDownDrag = true;
                if (i->mType & Splitter_LeftRight) this->mLeftRightDrag = true;
                break;
            }
        }
    }
    void MouseUp(int x, int y, int theButton)
    {
        H(Op::BaseMouseUp, x, y, theButton);
        if (theButton != 0 || H(Op::MouseChild) == 0)
            return;
        this->mLeftRightDrag = this->mUpDownDrag = false;
    }
    void MouseDrag(int x, int y)
    {
        H(Op::BaseMouseDrag, x, y);
        intptr_t aMouseChild = H(Op::MouseChild);
        if (aMouseChild == 0)
            return;
        int aLeft = (int)H(Op::ChildLeft, aMouseChild);
        int aTop = (int)H(Op::ChildTop, aMouseChild);
        int aNewY;
        int aNewX = ee_won_gui_split_drag(x, y, aLeft, aTop, this->mDragX, this->mDragY,
                                          this->mLeftRightDrag && !this->mLeftRightMax,
                                          this->mUpDownDrag && !this->mUpDownMax, this->mSplitX, this->mSplitY, &aNewY);
        this->SetSplitterPos(aNewX, aNewY, true);
    }
    bool CheckMaxButton(intptr_t theEvent)
    {
        if (!H(Op::EventIsButtonPress, theEvent))
            return false;
        if (!ee_won_gui_split_is_max_button((int)H(Op::EventControlId, theEvent)))
            return false;
        intptr_t aButton = H(Op::EventButton, theEvent);
        if (aButton == 0)
            return false;
        if (!H(Op::ButtonIsChecked, aButton)) {
            this->UnMaxQuadrant();
            return true;
        }
        int aType = ee_won_gui_split_button_quadrant((int)H(Op::EventControlId, theEvent));
        this->MaxQuadrant(aType);
        return true;
    }
    void HandleComponentEvent(intptr_t theEvent)
    {
        if (this->CheckMaxButton(theEvent))
            return;
        else
            H(Op::BaseHandleEvent, theEvent);
    }
};
} // namespace orig

static orig::Splitter* sOrig;
static const EeWonSplitter* sRust;

enum { kQuadrant, kSplitter, kComplex, kInvisible, kButton, kMinLR, kMinTB, kPercent, kMax, kUnMax, kSetPos, kSize,
       kDown, kUp, kDrag, kEvent, kCheckMax, kCalls };
static const char* kCallNames[kCalls] = {"quadrant", "splitter", "complex", "invisible", "button", "min-lr", "min-tb",
                                         "percent", "max", "unmax", "set-pos", "size", "down", "up", "drag", "event",
                                         "check-max"};
static unsigned sDriverSeed;
static unsigned DR(unsigned n) { sDriverSeed = sDriverSeed * 69069u + 1u; return n ? (sDriverSeed >> 16) % n : 0; }
static const int kPositions[] = {1, 2, 4, 8, 3, 6, 9, 12};
static const int kTypes[] = {5, 10, 0x15, 0x25, 0x4a, 0x8a, 0x10f};
static std::vector<intptr_t> sBars, sButtons; // the host's view, for mouse children and events

static void Run(bool inRust)
{
    static const int kWeights[kCalls] = {3, 3, 2, 1, 2, 1, 1, 1, 2, 2, 3, 3, 4, 2, 5, 3, 1};
    int aTotal = 0;
    for (int i = 0; i < kCalls; i++) aTotal += kWeights[i];
    int r0 = (int)DR(aTotal), k = 0;
    while (r0 >= kWeights[k]) r0 -= kWeights[k++];
    long long a = 0, b = 0, c = 0;
    // the host's side of the call (the same on both sides: the driver's stream)
    switch (k) {
    case kQuadrant: a = NewComp(); b = kPositions[DR(8)]; break;
    case kSplitter: a = NewComp(); b = kTypes[DR(7)]; sBars.push_back((intptr_t)a); break;
    case kComplex: a = NewComp(); b = kTypes[DR(7)]; c = DR(2) ? NewComp() : 0; sBars.push_back((intptr_t)a); break;
    case kInvisible: a = kTypes[DR(7)]; b = 1 + DR(12); break;
    case kButton: a = NewComp(); b = kPositions[DR(8)]; sComps[(intptr_t)a].checked = DR(2); sButtons.push_back((intptr_t)a); break;
    case kMinLR: case kMinTB: a = DR(80); b = DR(80); break;
    case kPercent: a = DR(101); b = DR(101); break;
    case kMax: a = kPositions[DR(8)]; break;
    case kSetPos: a = (int)DR(sWidth + 40) - 20; b = (int)DR(sHeight + 40) - 20; c = DR(2); break;
    case kSize: a = 20 + DR(400); b = 20 + DR(300); sWidth = (int)a; sHeight = (int)b; break;
    case kDown: case kUp: case kDrag: {
        a = (int)DR(sWidth + 20) - 10; b = (int)DR(sHeight + 20) - 10; c = DR(4) == 0;
        unsigned w = DR(4);
        sMouseChild = w == 0 || sComps.empty() ? 0 : w == 1 && !sBars.empty() ? sBars[DR((unsigned)sBars.size())] : 0x2001 + (intptr_t)DR((unsigned)sComps.size());
        if (sMouseChild) sComps[sMouseChild].mouseChild = DR(3) == 0 ? 0x2001 + (intptr_t)DR((unsigned)sComps.size()) : 0;
        if (k == kDrag) c = 0;
        break;
    }
    case kEvent: case kCheckMax: {
        HostEvent e;
        e.press = DR(5) != 0;
        e.button = !sButtons.empty() && DR(4) != 0 ? sButtons[DR((unsigned)sButtons.size())] : 0;
        e.controlId = e.button && DR(3) ? sComps[e.button].controlId : 100000 + (int)DR(20);
        if (e.button && DR(2)) sComps[e.button].checked = !sComps[e.button].checked;
        a = 0x9000 + (long long)sEvents.size();
        sEvents[(intptr_t)a] = e;
        break;
    }
    default: break;
    }
    char aLine[120];
    snprintf(aLine, sizeof(aLine), "call %s %llx %llx %llx", kCallNames[k], a, b, c);
    sTrace.push_back(aLine);
    size_t aCallLine = sTrace.size() - 1;
    intptr_t r = 0;
    if (inRust) {
        typedef EeWonSplitCall C;
        const EeWonSplitter* t = sRust;
        switch (k) {
        case kQuadrant: ee_won_split_call(t, C::AddQuadrant, a, b, 0); break;
        case kSplitter: ee_won_split_call(t, C::AddSplitter, a, b, 0); break;
        case kComplex: ee_won_split_call(t, C::AddComplexSplitter, a, b, c); break;
        case kInvisible: ee_won_split_call(t, C::AddInvisibleSplitter, a, b, 0); break;
        case kButton: ee_won_split_call(t, C::RegisterMaxButton, a, b, 0); break;
        case kMinLR: ee_won_split_call(t, C::SetMinLeftRight, a, b, 0); break;
        case kMinTB: ee_won_split_call(t, C::SetMinTopBottom, a, b, 0); break;
        case kPercent: ee_won_split_call(t, C::SetPercentages, a, b, 0); break;
        case kMax: ee_won_split_call(t, C::MaxQuadrant, a, 0, 0); break;
        case kUnMax: ee_won_split_call(t, C::UnMaxQuadrant, 0, 0, 0); break;
        case kSetPos: ee_won_split_call(t, C::SetSplitterPos, a, b, c); break;
        case kSize: ee_won_split_call(t, C::SizeChanged, 0, 0, 0); break;
        case kDown: ee_won_split_call(t, C::MouseDown, a, b, c); break;
        case kUp: ee_won_split_call(t, C::MouseUp, a, b, c); break;
        case kDrag: ee_won_split_call(t, C::MouseDrag, a, b, 0); break;
        case kEvent: ee_won_split_call(t, C::HandleEvent, a, 0, 0); break;
        case kCheckMax: r = ee_won_split_call(t, C::CheckMaxButton, a, 0, 0); break;
        }
    } else {
        orig::Splitter* t = sOrig;
        switch (k) {
        case kQuadrant: t->AddQuadrant(a, (int)b); break;
        case kSplitter: t->AddSplitter(a, (int)b); break;
        case kComplex: t->AddComplexSplitter(a, (int)b, c); break;
        case kInvisible: t->AddInvisibleSplitter((int)a, (int)b); break;
        case kButton: t->RegisterMaxButton(a, (int)b); break;
        case kMinLR: t->mMinLeft = (int)a; t->mMinRight = (int)b; break;
        case kMinTB: t->mMinTop = (int)a; t->mMinBottom = (int)b; break;
        case kPercent: t->mLeftPercent = (int)a; t->mTopPercent = (int)b; break;
        case kMax: t->MaxQuadrant((int)a); break;
        case kUnMax: t->UnMaxQuadrant(); break;
        case kSetPos: t->SetSplitterPos((int)a, (int)b, c != 0); break;
        case kSize: t->Virtual08(); break;
        case kDown: t->MouseDown((int)a, (int)b, (int)c); break;
        case kUp: t->MouseUp((int)a, (int)b, (int)c); break;
        case kDrag: t->MouseDrag((int)a, (int)b); break;
        case kEvent: t->HandleComponentEvent(a); break;
        case kCheckMax: r = t->CheckMaxButton(a); break;
        }
    }
    sTrace[aCallLine] += " -> " + std::to_string((long long)r);
}

static std::vector<std::string> RunSide(bool inRust, unsigned inSeq, int inCalls)
{
    sTrace.clear();
    sComps.clear(); sEvents.clear(); sBars.clear(); sButtons.clear();
    sNextId = 0;
    sSeed = inSeq * 2654435761u + 31u;
    sDriverSeed = inSeq * 40503u + 13u;
    sWidth = 100 + (int)R(400);
    sHeight = 80 + (int)R(300);
    sMouseChild = 0;
    char aLine[100];
    snprintf(aLine, sizeof(aLine), "start %d %d", sWidth, sHeight);
    sTrace.push_back(aLine);
    if (inRust) {
        EeWonSplitter* t = ee_won_split_new((void*)0x54, ScriptedHost);
        sRust = t;
        for (int i = 0; i < inCalls; i++) Run(true);
        sTrace.push_back("free");
        ee_won_split_free(t);
        sRust = NULL;
    } else {
        sOrig = new orig::Splitter;
        for (int i = 0; i < inCalls; i++) Run(false);
        sTrace.push_back("free");
        delete sOrig;
        sOrig = NULL;
    }
    bool aBack = true;
    for (std::map<intptr_t, HostComp>::const_iterator i = sComps.begin(); i != sComps.end(); ++i)
        if (i->second.held != 0) aBack = false;
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
    if (!aPrint)
        printf("won_splitobj_test: %d sequences, %u trace lines, %u ops and calls met, %d differing, %d checks, "
               "%d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), aDiffering, sChecks, sFailures);
    return sFailures;
}
