// InputBox as a whole object (R6 WON round 22): the original InputBox.cpp's methods and SetAllowSelection
// (ScreenSelectionState.cpp), the statements copied from the C++ before the port with every call on a C++ object
// (the base Component, the font, the icon, the filter, the clipboard, the events, the Graphics) replaced by the
// host op that makes it (GuiHost.cpp runs the same calls in the same order), against ee_won's Rust InputBox
// (inputobj.rs) over one scripted host. The original's shown and real text are its own members; the op TextStore
// hands both to the host after each change (the C++ shell keeps them for GetText()'s readers and the font), at
// the same point on both sides.
//
// Each of 20,000 sequences creates the box and makes 40 random inward calls (texts typed, pasted, set and
// replaced, every key the box handles and some it does not, the history's keys and its two setters, the password
// character, selection on and off, ranges, the mouse and its drag with the timer's scroll, the blink, focus,
// size and icon changes, the limits, paints); the host answers from the sequence's seeded stream and keeps the
// size, the icon, the clipboard, the filter and the mirrors. A fired event runs a scripted handler that may set
// the text, clear it into the history (as the chat line's return does) or select all; the numeric filter reads
// the box's selection start back; SetSize calls the box's size change back. The traces (every op with its
// arguments and answer, the stored texts, the draws) must be the same.
//
// The first 40 sequences and every later one that makes an op or a call none before made are
// rust/crates/ee_won/src/golden_inputobj.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <set>
#include "rust/ee_won.h"
#include "ee/wonapi/WONEventTypes.h"

typedef EeWonGuiOp Op;
typedef EeWonGuiField Fd;
typedef std::vector<uint32_t> Units;

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

// ---- the scripted host

static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }

static int sWidth, sHeight, sIcon, sFontHeight, sKern, sFilter;
static Units sShown, sReal, sClipboard;
static std::vector<std::string> sTrace;
static std::set<int> sOpsMet;
static int sFired;

// the side running (to call the box back)
struct Side {
    void (*mSizeChanged)();
    void (*mSetText)(const Units&, bool);
    void (*mAddHistory)(const Units&, bool);
    void (*mSelectRange)(int, int);
    int (*mSelStart)();
};
static Side sSide;

static int CharWidth(uint32_t u) { return u == ' ' ? 2 : u > 0x7f ? 9 : 3 + (int)(u % 5); }
static int StringWidth(const Units& s, int inStart, int inLength)
{
    int w = 0;
    for (int i = inStart; i < inStart + inLength && i < (int)s.size(); i++)
        if (i >= 0) w += CharWidth(s[i]);
    return inLength > 0 ? w + sKern : w;
}
static std::string Hex(const Units& u)
{
    std::string r = "'";
    char b[16];
    for (size_t i = 0; i < u.size(); i++) { snprintf(b, sizeof(b), "%s%x", i ? "." : "", (unsigned)u[i]); r += b; }
    return r + "'";
}
static Units FromU32(intptr_t p, intptr_t n)
{
    Units u;
    for (intptr_t i = 0; i < n; i++) u.push_back(((const uint32_t*)p)[i]);
    return u;
}
static void Sink(intptr_t inSink, const Units& u, bool inRust)
{
    if (inRust) { if (!u.empty()) ee_won_gui_sink_units((void*)inSink, &u[0], u.size(), sizeof(uint32_t)); }
    else *(Units*)inSink = u;
}
static bool sRustSide;
static Units RandomText(unsigned inMax)
{
    Units u;
    unsigned n = R(inMax + 1);
    unsigned aKind = R(4);
    for (unsigned i = 0; i < n; i++) {
        if (aKind == 0) u.push_back('0' + R(10));
        else if (R(6) == 0) u.push_back(' ');
        else if (R(30) == 0) u.push_back(R(2) ? 0x263a : 9); // a unit past ASCII, or a control character
        else u.push_back('a' + R(26));
    }
    if (aKind == 0 && R(3) == 0 && !u.empty()) u[0] = '-';
    return u;
}

static void Line(const std::string& s) { sTrace.push_back(s); }

static intptr_t ScriptedHost(void* inComp, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)inComp; (void)d;
    sOpsMet.insert((int)inOp);
    intptr_t r = 0;
    char aLine[400];
    int n = (int)inOp;
    switch (inOp) {
    case Op::Get: return (Fd)a == Fd::Width ? sWidth : (Fd)a == Fd::Height ? sHeight : 0; // pure reads: not traced
    case Op::SetSize: {
        bool aChanged = a != sWidth || b != sHeight;
        sWidth = (int)a; sHeight = (int)b;
        snprintf(aLine, sizeof(aLine), "%d set-size %lld %lld -> %d", n, (long long)a, (long long)b, aChanged ? 1 : 0);
        Line(aLine);
        if (aChanged) sSide.mSizeChanged(); // Component::SetSize's Virtual08
        return 0;
    }
    case Op::GetMousePos: {
        int* x = (int*)a; int* y = (int*)b;
        *x = (int)R(sWidth + 80) - 40;
        *y = R(3) == 0 ? (int)R(sHeight + 40) - 20 : sHeight / 2;
        snprintf(aLine, sizeof(aLine), "%d mouse %d %d", n, *x, *y);
        Line(aLine);
        return 0;
    }
    case Op::KeyMod: r = R(8); break;
    case Op::FontHeight: r = sFontHeight; break;
    case Op::FontCharWidth: r = CharWidth((uint32_t)a); break;
    case Op::TextWidth: r = StringWidth(sShown, (unsigned short)a, (unsigned short)b); break;
    case Op::TextWidthAll: r = StringWidth(sShown, 0, (int)sShown.size()); break;
    case Op::TextCharWidth: r = a >= 0 && a < (intptr_t)sShown.size() ? CharWidth(sShown[a]) : 0; break;
    case Op::FontStringM: r = CharWidth('M') + sKern; break;
    case Op::IconWidth: r = sIcon; break;
    case Op::TextStore: {
        Units u = FromU32(b, c);
        (a == 0 ? sShown : sReal) = u;
        snprintf(aLine, sizeof(aLine), "%d store %lld ", n, (long long)a);
        Line(aLine + Hex(u));
        return 0;
    }
    case Op::Filter: {
        Units u = FromU32(a, b);
        std::string aHead = std::to_string(n) + " filter " + Hex(u);
        if (sFilter == 0) { Line(aHead + " -> 2"); return 2; }
        if (sFilter == 1) {
            // InputBox::NumericFilter, as the original (it reads the box's selection start)
            int aPos = 0;
            r = 1;
            uint32_t aFirst = u.empty() ? 0 : u[0];
            if (aFirst == '-') { if (sSide.mSelStart() > 0) r = 0; aPos++; }
            for (; r && aPos < (int)u.size(); aPos++)
                if (!(u[aPos] >= '0' && u[aPos] <= '9')) r = 0;
        } else {
            // a filter that changes the text (upper case, and drops 'q')
            Units v;
            for (size_t i = 0; i < u.size(); i++)
                if (u[i] != 'q') v.push_back(u[i] >= 'a' && u[i] <= 'z' ? u[i] - 32 : u[i]);
            u = v;
            r = R(5) != 0;
        }
        Sink(c, u, sRustSide);
        Line(aHead + " -> " + std::to_string((long long)r) + " " + Hex(u));
        return r;
    }
    case Op::ClipboardSet:
        sClipboard = FromU32(a, b);
        Line(std::to_string(n) + " clip-set " + Hex(sClipboard));
        return 0;
    case Op::ClipboardTake:
        if (R(4) == 0) sClipboard = RandomText(12); // another program's copy
        Sink(a, sClipboard, sRustSide);
        Line(std::to_string(n) + " clip-take " + Hex(sClipboard));
        return 0;
    case Op::FireEvent: {
        // the type listeners receive: the original's enum (a negative code here), the Rust's through
        // ee_won_gui_event_type, the call GuiHost.cpp's host makes (round 30: traced as the type)
        long long aType = a < 0 ? -(long long)a : ee_won_gui_event_type((EeWonGuiEvent)a);
        int anAction = (int)R(6);
        snprintf(aLine, sizeof(aLine), "%d fire %lld -> %d", n, aType, anAction);
        Line(aLine);
        sFired++;
        if (anAction == 1) { Units u; u.push_back('o'); u.push_back('k'); sSide.mSetText(u, false); }
        else if (anAction == 2 && aType == WONAPI::ComponentEvent_InputReturn) { Units u = sReal; sSide.mAddHistory(u, true); sSide.mSetText(Units(), false); }
        else if (anAction == 3) sSide.mSelectRange(0, -1);
        return 0;
    }
    case Op::PaintInputs: {
        EeWonInputPaint* p = (EeWonInputPaint*)a;
        p->disabled_look = R(4) == 0;
        p->has_icon = sIcon >= 0;
        p->has_focus = R(3) != 0;
        p->text_color = 0x101010;
        p->disabled_text_color = 0x808080;
        p->icon_height = sIcon >= 0 ? 12 : 0;
        p->font_height = sFontHeight;
        p->sel.background = 0x3366cc; p->sel.foreground = 0xffffff; p->sel.mode = (int)R(3);
        snprintf(aLine, sizeof(aLine), "%d paint-inputs %d %d %d %d %d %d %d %d", n, p->disabled_look, p->has_icon,
                 p->has_focus, p->text_color, p->disabled_text_color, p->icon_height, p->font_height, p->sel.mode);
        Line(aLine);
        return 0;
    }
    case Op::Draw: {
        const EeWonDrawOp* o = (const EeWonDrawOp*)b;
        snprintf(aLine, sizeof(aLine), "%d draw %llx %d %d %d %d %d %d %d %d %d", n, (long long)a, o->kind, o->a, o->b,
                 o->c, o->d, o->e, o->f, o->g, o->h);
        Line(aLine);
        return 0;
    }
    default: break;
    }
    snprintf(aLine, sizeof(aLine), "%d %lld %lld %lld -> %lld", n, (long long)a, (long long)b, (long long)c, (long long)r);
    Line(aLine);
    return r;
}

static intptr_t H(Op inOp, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0)
{
    return ScriptedHost((void*)0x52, inOp, a, b, c, 0);
}

// ---- the original (InputBox.cpp; the base Component, the font, icon, filter, clipboard, events and Graphics
// through the host)

namespace orig {

static const intptr_t kG = 0x6; // the Graphics
using WONAPI::ComponentEvent_InputReturn;
using WONAPI::ComponentEvent_InputTextChanged;

static int SmallerOf(int a, int b) { return a > b ? b : a; }

struct InputBox {
    Units mText, mPasswordText;
    int mPassword;
    std::deque<Units> mInputHistory;
    int mInputHistoryPos, mMaxInputHistory;
    bool mHaveTraversedHistory, mBackwardsSel;
    int mSelStartChar, mSelEndChar, mHorzOffset;
    bool mSelecting;
    int mLeftPad, mTopPad, mRightPad, mBottomPad;
    int mCursorDelay, mCursorOnTime, mCursorOffTime;
    bool mCursorOn, mUnderlineCursor, mAllowSelection, mSelectOnFocus;
    int mMaxChars;
    struct { int x, y, width, height; } mTextRect;

    InputBox()
    {
        this->mSelStartChar = this->mSelEndChar = 0;
        this->mBackwardsSel = false;
        this->mSelecting = false;
        this->mHorzOffset = 0;
        this->mCursorOn = false;
        this->mUnderlineCursor = false;
        this->mSelectOnFocus = false;
        this->mCursorOffTime = this->mCursorOnTime = 500;
        this->mPassword = 0;
        this->mAllowSelection = true;
        this->mMaxChars = 0;
        this->mRightPad = this->mLeftPad = 4;
        this->mBottomPad = this->mTopPad = 3;
        this->mInputHistoryPos = 0;
        this->mMaxInputHistory = 100;
        this->mHaveTraversedHistory = false;
        this->mCursorDelay = 0;
        this->mTextRect.x = this->mTextRect.y = this->mTextRect.width = this->mTextRect.height = 0;
    }
    int Width() { return sWidth; }
    int Height() { return sHeight; }
    void Invalidate() { H(Op::Invalidate); }
    int GetKeyMod() { return (int)H(Op::KeyMod); }
    void Fn2F5380(int e, int) { H(Op::FireEvent, -e); } // the original's enum: see the host's FireEvent
    const Units& GetText() const { return this->mPassword == 0 ? this->mText : this->mPasswordText; }
    int TextWidth(int o, int l) { return (int)H(Op::TextWidth, o, l); }

    void AdjustTextRect()
    {
        int aLeftPad = this->mLeftPad;
        int aRightPad = this->mRightPad;
        int aTopPad = this->mTopPad;
        int aBottomPad = this->mBottomPad;
        int anIcon = (int)H(Op::IconWidth);
        if (anIcon >= 0)
            aLeftPad += anIcon + 2;
        this->mTextRect.x = aLeftPad;
        this->mTextRect.width = this->Width() - aLeftPad - aRightPad;
        this->mTextRect.y = aTopPad;
        this->mTextRect.height = this->Height() - aTopPad - aBottomPad;
    }
    void SetIcon() // after the icon is set
    {
        this->AdjustTextRect();
        this->AdjustPosition();
        this->Invalidate();
    }
    void Virtual08()
    {
        H(Op::BaseSizeChanged);
        this->AdjustTextRect();
    }
    void GetDesiredSize(int& width, int& height)
    {
        width = 100;
        height = (int)H(Op::FontHeight) + 6;
    }
    void SetCharWidth(int theEm)
    {
        int w = (int)H(Op::FontCharWidth, 'M') * theEm + 8;
        H(Op::SetSize, w, this->Height());
    }
    void GotFocus()
    {
        H(Op::BaseGotFocus);
        this->mCursorDelay = 0;
        this->mCursorOn = true;
        H(Op::RequestTimer, 1);
        if (this->mSelectOnFocus)
            this->SelectRange(0, -1);
        this->Invalidate();
    }
    void LostFocus()
    {
        H(Op::BaseLostFocus);
        this->mCursorOn = false;
        H(Op::RequestTimer, 0);
        this->Invalidate();
    }
    void CheckSel(int x, int y, bool newStart)
    {
        (void)y;
        if (!this->mAllowSelection)
            return;
        int aContentX = ee_won_gui_sel_x(x, this->Width(), this->mHorzOffset, this->mTextRect.x);
        int aSelChar = this->mBackwardsSel ? this->mSelStartChar : this->mSelEndChar;
        int aWidth = this->TextWidth(0, aSelChar);
        int aLength = (int)this->mText.size();
        std::vector<int> aWidths(aLength, 0);
        if (aWidth >= aContentX) {
            for (int i = 0; i < aSelChar && i < aLength; i++)
                aWidths[i] = (int)H(Op::TextCharWidth, i);
        } else {
            for (int i = aSelChar < 0 ? 0 : aSelChar; i < aLength; i++)
                aWidths[i] = (int)H(Op::TextCharWidth, i);
        }
        EeWonSel aSel = {this->mSelStartChar, this->mSelEndChar, this->mBackwardsSel};
        ee_won_gui_check_sel(x, this->Width(), this->mHorzOffset, this->mTextRect.x, aWidth, aSelChar,
                             aWidths.empty() ? NULL : &aWidths[0], aWidths.size(), newStart, &aSel);
        this->mSelStartChar = aSel.start;
        this->mSelEndChar = aSel.end;
        this->mBackwardsSel = aSel.backwards;
        this->Invalidate();
    }
    void MouseDown(int x, int y, int theButton)
    {
        if (theButton != 0)
            return;
        this->CheckSel(x, y, (this->GetKeyMod() & 1) ? false : true);
        if (this->mAllowSelection)
            this->mSelecting = true;
        H(Op::RequestFocus);
    }
    void MouseUp(int, int, int theButton)
    {
        if (theButton == 0)
            this->mSelecting = false;
    }
    void MouseExit()
    {
        H(Op::BaseMouseExit);
        this->mSelecting = false;
    }
    void MouseDrag(int x, int y) { this->CheckSel(x, y, false); }
    void SetCursorBlinkRate(int theOnTime, int theOffTime)
    {
        this->mCursorOnTime = theOnTime;
        this->mCursorOffTime = ee_won_gui_blink_off_time(theOnTime, theOffTime);
    }
    bool TimerEvent(int theDelta)
    {
        EeWonBlink aBlink = ee_won_gui_blink(this->mCursorDelay, theDelta, this->mCursorOn, this->mCursorOnTime,
                                             this->mCursorOffTime);
        this->mCursorDelay = aBlink.delay;
        this->mCursorOn = aBlink.on;
        if (aBlink.changed)
            this->Invalidate();
        if (!this->mSelecting)
            return true;
        int x, y;
        H(Op::GetMousePos, (intptr_t)&x, (intptr_t)&y);
        int aCharPos = this->mBackwardsSel ? this->mSelStartChar : this->mSelEndChar;
        int anOffset = this->mHorzOffset;
        if (!ee_won_gui_drag_scroll(x, y, this->Width(), this->Height(), aCharPos, (int)this->mText.size(),
                                    this->mHorzOffset, &anOffset))
            return true;
        this->mHorzOffset = anOffset;
        this->CheckSel(x, y, false);
        aCharPos = this->mBackwardsSel ? this->mSelStartChar : this->mSelEndChar;
        int aTextWidth = 0, anM = 0;
        if (aCharPos == (int)this->mText.size()) {
            aTextWidth = (int)H(Op::TextWidthAll);
            if (aTextWidth + this->mHorzOffset > this->mTextRect.width)
                anM = (int)H(Op::FontStringM);
        }
        this->mHorzOffset = ee_won_gui_drag_scroll_end(aCharPos, (int)this->mText.size(), aTextWidth, anM,
                                                       this->mTextRect.width, this->mHorzOffset);
        return true;
    }
    void AdjustPosition()
    {
        int aCursorPos = this->mBackwardsSel ? this->mSelStartChar : this->mSelEndChar;
        int aPreWidth = this->TextWidth(0, aCursorPos);
        int anM = (int)H(Op::FontCharWidth, 'M');
        this->mHorzOffset = ee_won_gui_cursor_scroll(aPreWidth, anM, this->mHorzOffset, this->mTextRect.width);
    }
    void SelectRange(int theStartPos, int theEndPos)
    {
        EeWonSel aSel = ee_won_gui_select_range(theStartPos, theEndPos, (int)this->mText.size());
        this->mBackwardsSel = aSel.backwards;
        this->mSelStartChar = aSel.start;
        this->mSelEndChar = aSel.end;
        this->AdjustPosition();
        this->Invalidate();
    }
    void SetSelText(const Units& theText, bool sendEvent)
    {
        const Units& aText = this->GetText();
        Units aPasteStr = theText;
        int aCut = ee_won_gui_control_cut(aPasteStr.empty() ? NULL : &aPasteStr[0], (uint16_t)aPasteStr.size(), 4);
        if (aCut < (int)aPasteStr.size())
            aPasteStr.resize(aCut);
        Units aFiltered;
        intptr_t aFilter = H(Op::Filter, aPasteStr.empty() ? 0 : (intptr_t)&aPasteStr[0], aPasteStr.size(), (intptr_t)&aFiltered);
        if (aFilter != 2) { // mInputFilter && !mInputFilter(this,aPasteStr)
            aPasteStr = aFiltered;
            if (aFilter == 0)
                return;
        }
        int aNumCharsAllowed = ee_won_gui_paste_allowed((int)aText.size(), (int)aPasteStr.size(), this->mSelStartChar,
                                                        this->mSelEndChar, this->mMaxChars);
        if (aNumCharsAllowed >= 0)
            aPasteStr.resize(aNumCharsAllowed);
        if (aPasteStr.size() > 0 || this->mSelStartChar != this->mSelEndChar) {
            int aSelStartChar = this->mSelStartChar, aSelEndChar = this->mSelEndChar;
            this->mSelStartChar += (int)aPasteStr.size();
            this->mSelEndChar = this->mSelStartChar;
            Units aNewStr(aText.begin(), aText.begin() + aSelStartChar);
            aNewStr.insert(aNewStr.end(), aPasteStr.begin(), aPasteStr.end());
            aNewStr.insert(aNewStr.end(), aText.begin() + aSelEndChar, aText.end());
            this->SetTextPrv(aNewStr, sendEvent);
        }
        this->mCursorDelay = 0;
        this->mCursorOn = true;
        this->AdjustPosition();
        this->Invalidate();
    }
    void AddInputHistory(const Units& theText, bool clearTraversalState)
    {
        if (theText.size() == 0)
            return;
        int aHistoryPos = this->mInputHistoryPos;
        while (this->mInputHistory.size() >= (size_t)(intptr_t)this->mMaxInputHistory) {
            if (this->mInputHistory.empty()) break; // (the driver never sets a limit of 0)
            this->mInputHistory.pop_front();
            aHistoryPos--;
        }
        if (theText.size() > 0 && this->mMaxInputHistory > 0) {
            if (this->mInputHistory.empty() || this->mInputHistory.back() != theText)
                this->mInputHistory.push_back(theText);
        }
        if (aHistoryPos >= 0 && (size_t)aHistoryPos < this->mInputHistory.size() && this->mInputHistory[aHistoryPos] == theText)
            this->mInputHistoryPos = aHistoryPos;
        else
            this->mInputHistoryPos = (int)this->mInputHistory.size();
        if (clearTraversalState)
            this->mHaveTraversedHistory = false;
    }
    void SetInputHistory(const Units& theText, bool clearTraversalState)
    {
        if (theText.size() == 0)
            return;
        int aPos = this->mInputHistoryPos;
        if (aPos >= 0 && (size_t)aPos < this->mInputHistory.size())
            this->mInputHistory[aPos] = theText;
        else {
            this->AddInputHistory(theText, clearTraversalState);
            this->mInputHistoryPos = aPos;
        }
        if (clearTraversalState)
            this->mHaveTraversedHistory = false;
    }
    bool KeyDown(int theKey)
    {
        const Units& aText = this->GetText();
        int aKeyMod = this->GetKeyMod();
        bool shift = (aKeyMod & 1) ? true : false;
        bool ctrl = (aKeyMod & 4) ? true : false;
        if (!this->mAllowSelection)
            shift = false;
        switch (theKey) {
        case 13:
            if (aText.size() > 0)
                this->Fn2F5380(ComponentEvent_InputReturn, 0);
            return true;
        case 38:
        case 40: {
            int aSize = SmallerOf(this->mMaxInputHistory, (int)this->mInputHistory.size());
            if (aSize > 0) {
                if (!this->mHaveTraversedHistory && this->mInputHistoryPos >= 0 &&
                    (size_t)this->mInputHistoryPos < this->mInputHistory.size())
                    this->mHaveTraversedHistory = true;
                else {
                    this->mHaveTraversedHistory = true;
                    Units aCur = this->GetText();
                    this->SetInputHistory(aCur, false);
                    if (theKey == 38)
                        this->mInputHistoryPos--;
                    else
                        this->mInputHistoryPos++;
                }
                if (this->mInputHistoryPos < 0)
                    this->mInputHistoryPos = 0;
                if (this->mInputHistoryPos >= aSize) {
                    this->SetText(Units(), true);
                    this->mInputHistoryPos = (int)this->mInputHistory.size();
                } else {
                    Units anEntry = this->mInputHistory[this->mInputHistoryPos];
                    this->SetText(anEntry, true);
                    this->SelectRange((int)this->GetText().size(), -1);
                }
            }
        } break;
        case 8:
        case 46: {
            Units left, right;
            EeWonDeleteParts aParts = ee_won_gui_delete_parts(theKey == 8, this->mSelStartChar, this->mSelEndChar,
                                                              (int)aText.size());
            if (aParts.left_count >= 0)
                left.assign(aText.begin(), aText.begin() + aParts.left_count);
            if (aParts.right_start >= 0)
                right.assign(aText.begin() + aParts.right_start, aText.end());
            this->mSelStartChar = this->mSelEndChar = (int)left.size();
            Units total = left;
            total.insert(total.end(), right.begin(), right.end());
            this->SetTextPrv(total, true);
        } break;
        case 36:
        case 35:
        case 37:
        case 39: {
            const Units& aWalk = (theKey == 36 || theKey == 35) ? aText : this->mText;
            EeWonSel aSel = {this->mSelStartChar, this->mSelEndChar, this->mBackwardsSel};
            ee_won_gui_key_selection(theKey, shift, ctrl, &aSel, aWalk.empty() ? NULL : &aWalk[0],
                                     (uint16_t)aWalk.size(), 4);
            this->mSelStartChar = aSel.start;
            this->mSelEndChar = aSel.end;
            this->mBackwardsSel = aSel.backwards;
        } break;
        default:
            return false;
        }
        this->mCursorDelay = 0;
        this->mCursorOn = true;
        this->AdjustPosition();
        this->Invalidate();
        return true;
    }
    void SetPasswordChar(int theChar)
    {
        this->mPassword = theChar;
        Units aText = this->mText;
        this->SetText(aText, false);
    }
    void SetText(const Units& theText, bool sendEvent)
    {
        this->SetTextPrv(theText, sendEvent);
        this->mSelStartChar = this->mSelEndChar = 0;
        this->AdjustPosition();
        this->Invalidate();
    }
    void CopySelection()
    {
        Units aPart(this->mText.begin() + this->mSelStartChar, this->mText.begin() + this->mSelEndChar);
        H(Op::ClipboardSet, aPart.empty() ? 0 : (intptr_t)&aPart[0], aPart.size());
    }
    bool KeyChar(int theKey)
    {
        Units aPasteStr;
        if (theKey == 22)
            H(Op::ClipboardTake, (intptr_t)&aPasteStr);
        else if (theKey == 3) {
            this->CopySelection();
            return true;
        } else if (theKey == 24) {
            this->CopySelection();
        } else if (theKey > 31)
            aPasteStr.assign(1, (uint32_t)(wchar_t)theKey); // Fill(theKey,0,1)
        else
            return true;
        this->SetSelText(aPasteStr, true);
        return true;
    }
    void SetTextPrv(const Units& theText, bool sendEvent)
    {
        if (this->mPassword == 0)
            this->mText = theText;
        else {
            this->mPasswordText = theText;
            this->mText.assign(theText.size(), (uint32_t)(wchar_t)this->mPassword); // Fill(mPassword,0,length)
        }
        H(Op::TextStore, 0, this->mText.empty() ? 0 : (intptr_t)&this->mText[0], this->mText.size());
        const Units& aReal = this->GetText();
        H(Op::TextStore, 1, aReal.empty() ? 0 : (intptr_t)&aReal[0], aReal.size());
        if (sendEvent)
            this->Fn2F5380(ComponentEvent_InputTextChanged, 0);
    }
    void SetAllowSelection(bool allowSelection)
    {
        if (allowSelection == this->mAllowSelection)
            return;
        this->mAllowSelection = allowSelection;
        H(Op::SetDefaultCursor, allowSelection ? (intptr_t)EeWonGuiCursor::IBeam : (intptr_t)EeWonGuiCursor::None);
        this->mSelecting = false;
        if (!this->mAllowSelection && this->mSelStartChar != this->mSelEndChar) {
            this->mSelStartChar = this->mSelEndChar = 0;
            this->Invalidate();
        }
    }
    static int32_t MeasureString(void*, int32_t o, int32_t l) { return (int32_t)H(Op::TextWidth, o, l); }
    static int32_t MeasureChar(void*, int32_t i) { return (int32_t)(i < 0 ? H(Op::FontCharWidth, 'M') : H(Op::TextCharWidth, i)); }
    void Paint()
    {
        H(Op::BasePaint, kG);
        EeWonInputPaint b;
        memset(&b, 0, sizeof(b));
        H(Op::PaintInputs, (intptr_t)&b);
        b.cursor_on = this->mCursorOn;
        b.underline_cursor = this->mUnderlineCursor;
        b.backwards_sel = this->mBackwardsSel;
        b.width = this->Width();
        b.height = this->Height();
        b.horz_offset = this->mHorzOffset;
        b.text_x = this->mTextRect.x;
        b.text_y = this->mTextRect.y;
        b.text_width = this->mTextRect.width;
        b.text_height = this->mTextRect.height;
        b.sel_start = this->mSelStartChar;
        b.sel_end = this->mSelEndChar;
        b.length = (int)this->mText.size();
        EeWonTextMeasure m;
        m.ctx = this;
        m.string_width = &MeasureString;
        m.char_width = &MeasureChar;
        EeWonDrawOp anOps[48];
        size_t aCount = ee_won_gui_draw_input_box(&b, &m, anOps, 48);
        for (size_t i = 0; i < aCount && i < 48; i++)
            H(Op::Draw, kG, (intptr_t)&anOps[i]);
    }
};

} // namespace orig

// ---- the two sides

static orig::InputBox* sOrig;
static const EeWonInput* sRust;

static std::vector<wchar_t> Wide(const Units& u)
{
    std::vector<wchar_t> w;
    for (size_t i = 0; i < u.size(); i++) w.push_back((wchar_t)u[i]);
    w.push_back(0);
    return w;
}
static intptr_t RustText(EeWonInputCall c, const Units& u, intptr_t inFlag)
{
    std::vector<wchar_t> w = Wide(u);
    return ee_won_input_call(sRust, c, (intptr_t)&w[0], (intptr_t)u.size(), inFlag);
}
static void OrigSizeChanged() { sOrig->Virtual08(); }
static void OrigSetText(const Units& u, bool s) { sOrig->SetText(u, s); }
static void OrigAddHistory(const Units& u, bool c) { sOrig->AddInputHistory(u, c); }
static void OrigSelectRange(int a, int b) { sOrig->SelectRange(a, b); }
static int OrigSelStart() { return sOrig->mSelStartChar; }
static void RustSizeChanged() { ee_won_input_call(sRust, EeWonInputCall::SizeChanged, 0, 0, 0); }
static void RustSetText(const Units& u, bool s) { RustText(EeWonInputCall::SetText, u, s); }
static void RustAddHistory(const Units& u, bool c) { RustText(EeWonInputCall::AddInputHistory, u, c); }
static void RustSelectRange(int a, int b) { ee_won_input_call(sRust, EeWonInputCall::SelectRange, a, b, 0); }
static int RustSelStart() { return (int)ee_won_input_call(sRust, EeWonInputCall::SelStart, 0, 0, 0); }

enum {
    kSize, kDesired, kGotFocus, kLostFocus, kDown, kUp, kExit, kDrag, kTimer, kKeyChar, kKeyDown, kPaint, kSetText,
    kPassword, kAllow, kSelect, kIcon, kCharWidth, kBlink, kAdjust, kCopy, kSelText, kAddHistory, kSetHistory,
    kMaxChars, kSelectOnFocus, kMaxHistory, kLeftPad, kSelStart, kFilter, kCalls
};
static const char* kCallNames[kCalls] = {"size", "desired", "got-focus", "lost-focus", "down", "up", "exit", "drag",
                                         "timer", "key-char", "key-down", "paint", "set-text", "password", "allow",
                                         "select", "icon", "char-width", "blink", "adjust", "copy", "sel-text",
                                         "add-history", "set-history", "max-chars", "select-on-focus", "max-history",
                                         "left-pad", "sel-start", "filter"};
struct Call {
    int mKind;
    int a, b, c;
    Units mText;
};

static unsigned sDriverSeed;
static unsigned DR(unsigned n) { sDriverSeed = sDriverSeed * 69069u + 1u; return n ? (sDriverSeed >> 16) % n : 0; }

static Call MakeCall()
{
    static const int kWeights[kCalls] = {2, 1, 2, 2, 4, 3, 1, 3, 5, 14, 14, 3, 3, 1, 1, 3, 1, 1, 1, 1, 1, 3, 3, 2, 1, 1,
                                         1, 1, 1, 1};
    int aTotal = 0;
    for (int i = 0; i < kCalls; i++) aTotal += kWeights[i];
    int r = (int)DR(aTotal), k = 0;
    while (r >= kWeights[k]) r -= kWeights[k++];
    Call c;
    c.mKind = k; c.a = c.b = c.c = 0;
    int w = sWidth + 60, h = sHeight + 20;
    switch (k) {
    case kSize: c.a = 20 + (int)DR(200); c.b = 10 + (int)DR(30); break;
    case kDown: case kUp: c.a = (int)DR(w) - 30; c.b = (int)DR(h) - 10; c.c = DR(5) == 0 ? 1 : 0; break;
    case kDrag: c.a = (int)DR(w) - 30; c.b = (int)DR(h) - 10; break;
    case kTimer: c.a = DR(3) == 0 ? 600 + (int)DR(600) : (int)DR(100); break;
    case kKeyChar: {
        static const int kKeys[] = {22, 3, 24, 'a', 'z', ' ', '5', '-', 0x263a, 9, 13, 127};
        c.a = DR(2) ? 'a' + (int)DR(26) : kKeys[DR(12)];
        break;
    }
    case kKeyDown: {
        static const int kKeys[] = {13, 38, 40, 8, 46, 36, 35, 37, 39, 37, 39, 8, 65, 33};
        c.a = kKeys[DR(14)];
        break;
    }
    case kSetText: case kSelText: case kAddHistory: case kSetHistory: {
        unsigned n = DR(4) == 0 ? 30 : 8;
        for (unsigned i = 0, l = DR(n + 1); i < l; i++) {
            unsigned x = DR(20);
            c.mText.push_back(x == 0 ? ' ' : x == 1 ? '0' + DR(10) : x == 2 && DR(4) == 0 ? (DR(2) ? 10 : 0x263a) : 'a' + DR(26));
        }
        c.a = DR(2);
        break;
    }
    case kPassword: { static const int kChars[] = {0, '*', '#', -3}; c.a = kChars[DR(4)]; break; }
    case kAllow: c.a = DR(3) != 0; break;
    case kSelect: c.a = (int)DR(14) - 2; c.b = (int)DR(14) - 2; break;
    case kIcon: c.a = DR(2) ? -1 : 8 + (int)DR(10); break;
    case kCharWidth: c.a = (int)DR(20); break;
    case kBlink: c.a = 50 + (int)DR(500); c.b = (int)DR(500) - 100; break;
    case kMaxChars: c.a = DR(3) == 0 ? 0 : (int)DR(20); break;
    case kSelectOnFocus: c.a = DR(2); break;
    case kMaxHistory: { static const int kMax[] = {-1, 1, 2, 3, 5, 100}; c.a = kMax[DR(6)]; break; }
    case kLeftPad: c.a = (int)DR(10); break;
    case kFilter: c.a = (int)DR(3); break;
    default: break;
    }
    return c;
}

static void Run(bool inRust, const Call& c)
{
    char aLine[100];
    snprintf(aLine, sizeof(aLine), "call %s %d %d %d ", kCallNames[c.mKind], c.a, c.b, c.c);
    sTrace.push_back(aLine + Hex(c.mText));
    size_t aCallLine = sTrace.size() - 1;
    intptr_t r = 0;
    if (c.mKind == kSize) { sWidth = c.a; sHeight = c.b; }
    if (c.mKind == kIcon) sIcon = c.a;
    if (c.mKind == kFilter) { sFilter = c.a; sTrace[aCallLine] += " -> 0"; return; }
    if (inRust) {
        typedef EeWonInputCall C;
        const EeWonInput* t = sRust;
        switch (c.mKind) {
        case kSize: ee_won_input_call(t, C::SizeChanged, 0, 0, 0); break;
        case kDesired: { int w = 0, h = 0; ee_won_input_call(t, C::DesiredSize, (intptr_t)&w, (intptr_t)&h, 0); r = w * 1000 + h; break; }
        case kGotFocus: ee_won_input_call(t, C::GotFocus, 0, 0, 0); break;
        case kLostFocus: ee_won_input_call(t, C::LostFocus, 0, 0, 0); break;
        case kDown: ee_won_input_call(t, C::MouseDown, c.a, c.b, c.c); break;
        case kUp: ee_won_input_call(t, C::MouseUp, c.a, c.b, c.c); break;
        case kExit: ee_won_input_call(t, C::MouseExit, 0, 0, 0); break;
        case kDrag: ee_won_input_call(t, C::MouseDrag, c.a, c.b, 0); break;
        case kTimer: r = ee_won_input_call(t, C::TimerEvent, c.a, 0, 0); break;
        case kKeyChar: r = ee_won_input_call(t, C::KeyChar, c.a, 0, 0); break;
        case kKeyDown: r = ee_won_input_call(t, C::KeyDown, c.a, 0, 0); break;
        case kPaint: ee_won_input_call(t, C::Paint, orig::kG, 0, 0); break;
        case kSetText: RustText(C::SetText, c.mText, c.a); break;
        case kPassword: ee_won_input_call(t, C::SetPasswordChar, (intptr_t)(signed char)c.a, 0, 0); break;
        case kAllow: ee_won_input_call(t, C::SetAllowSelection, c.a, 0, 0); break;
        case kSelect: ee_won_input_call(t, C::SelectRange, c.a, c.b, 0); break;
        case kIcon: ee_won_input_call(t, C::IconChanged, 0, 0, 0); break;
        case kCharWidth: ee_won_input_call(t, C::SetCharWidth, c.a, 0, 0); break;
        case kBlink: ee_won_input_call(t, C::SetCursorBlinkRate, c.a, c.b, 0); break;
        case kAdjust: ee_won_input_call(t, C::AdjustPosition, 0, 0, 0); break;
        case kCopy: ee_won_input_call(t, C::CopySelection, 0, 0, 0); break;
        case kSelText: RustText(C::SetSelText, c.mText, c.a); break;
        case kAddHistory: RustText(C::AddInputHistory, c.mText, c.a); break;
        case kSetHistory: RustText(C::SetInputHistory, c.mText, c.a); break;
        case kMaxChars: ee_won_input_call(t, C::SetMaxChars, c.a, 0, 0); break;
        case kSelectOnFocus: ee_won_input_call(t, C::SetSelectOnFocus, c.a, 0, 0); break;
        case kMaxHistory: ee_won_input_call(t, C::SetMaxInputHistory, c.a, 0, 0); break;
        case kLeftPad: ee_won_input_call(t, C::SetLeftPad, c.a, 0, 0); break;
        case kSelStart: r = ee_won_input_call(t, C::SelStart, 0, 0, 0); break;
        }
    } else {
        orig::InputBox* t = sOrig;
        switch (c.mKind) {
        case kSize: t->Virtual08(); break;
        case kDesired: { int w = 0, h = 0; t->GetDesiredSize(w, h); r = w * 1000 + h; break; }
        case kGotFocus: t->GotFocus(); break;
        case kLostFocus: t->LostFocus(); break;
        case kDown: t->MouseDown(c.a, c.b, c.c); break;
        case kUp: t->MouseUp(c.a, c.b, c.c); break;
        case kExit: t->MouseExit(); break;
        case kDrag: t->MouseDrag(c.a, c.b); break;
        case kTimer: r = t->TimerEvent(c.a); break;
        case kKeyChar: r = t->KeyChar(c.a); break;
        case kKeyDown: r = t->KeyDown(c.a); break;
        case kPaint: t->Paint(); break;
        case kSetText: t->SetText(c.mText, c.a != 0); break;
        case kPassword: t->SetPasswordChar((signed char)c.a); break;
        case kAllow: t->SetAllowSelection(c.a != 0); break;
        case kSelect: t->SelectRange(c.a, c.b); break;
        case kIcon: t->SetIcon(); break;
        case kCharWidth: t->SetCharWidth(c.a); break;
        case kBlink: t->SetCursorBlinkRate(c.a, c.b); break;
        case kAdjust: t->AdjustPosition(); break;
        case kCopy: t->CopySelection(); break;
        case kSelText: t->SetSelText(c.mText, c.a != 0); break;
        case kAddHistory: t->AddInputHistory(c.mText, c.a != 0); break;
        case kSetHistory: t->SetInputHistory(c.mText, c.a != 0); break;
        case kMaxChars: t->mMaxChars = c.a; break;
        case kSelectOnFocus: t->mSelectOnFocus = c.a != 0; break;
        case kMaxHistory: t->mMaxInputHistory = c.a; break;
        case kLeftPad: t->mLeftPad = c.a; break;
        case kSelStart: r = t->mSelStartChar; break;
        }
    }
    sTrace[aCallLine] += " -> " + std::to_string((long long)r);
}

static std::vector<std::string> RunSide(bool inRust, unsigned inSeq, int inCalls)
{
    sTrace.clear();
    sSeed = inSeq * 2654435761u + 23u;
    sDriverSeed = inSeq * 40503u + 7u;
    sWidth = 60 + (int)R(200);
    sHeight = 14 + (int)R(14);
    sIcon = R(4) == 0 ? 10 : -1;
    sFontHeight = 10 + (int)R(6);
    sKern = (int)R(3);
    sFilter = (int)R(3) == 0 ? 1 : 0;
    sShown.clear(); sReal.clear();
    sClipboard.clear();
    sClipboard.push_back('h'); sClipboard.push_back('i');
    char aLine[100];
    snprintf(aLine, sizeof(aLine), "start %d %d %d %d %d %d", sWidth, sHeight, sIcon, sFontHeight, sKern, sFilter);
    sTrace.push_back(aLine);
    sRustSide = inRust;
    if (inRust) {
        sSide.mSizeChanged = RustSizeChanged; sSide.mSetText = RustSetText; sSide.mAddHistory = RustAddHistory;
        sSide.mSelectRange = RustSelectRange; sSide.mSelStart = RustSelStart;
        EeWonInput* t = ee_won_input_new((void*)0x52, ScriptedHost);
        sRust = t;
        for (int i = 0; i < inCalls; i++) Run(true, MakeCall());
        sTrace.push_back("free");
        ee_won_input_free(t);
        sRust = NULL;
    } else {
        sSide.mSizeChanged = OrigSizeChanged; sSide.mSetText = OrigSetText; sSide.mAddHistory = OrigAddHistory;
        sSide.mSelectRange = OrigSelectRange; sSide.mSelStart = OrigSelStart;
        sOrig = new orig::InputBox;
        for (int i = 0; i < inCalls; i++) Run(false, MakeCall());
        sTrace.push_back("free");
        delete sOrig;
        sOrig = NULL;
    }
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    const int kSequences = 20000;
    const int kCalls = aPrint ? 16 : 40; // the golden's sequences are shorter
    size_t aLines = 0;
    std::set<std::string> aSeen;
    int aDiffering = 0;
    for (int seq = 0; seq < kSequences; seq++) {
        sOpsMet.clear();
        std::vector<std::string> a = RunSide(false, (unsigned)seq, kCalls);
        std::vector<std::string> b = RunSide(true, (unsigned)seq, kCalls);
        aLines += a.size();
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
        printf("won_inputobj_test: %d sequences, %u trace lines, %u ops and calls met, %d events fired, %d differing, "
               "%d checks, %d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), sFired / 2,
               aDiffering, sChecks, sFailures);
    return sFailures;
}
