// WONGUI's single-line text editing (InputBox.cpp): the original's statements (copied from the C++ before the port,
// the box replaced by its selection, text units, offsets and the font's widths) against ee_won's guiedit module
// through its C ABI: MoveCursor, SelectRange, KeyDown's Home / End / Left / Right selection and Backspace / Delete
// parts, SetSelText's control cut and limit, TimerEvent's blink and drag scroll, SetCursorBlinkRate, CheckSel
// (the character under the mouse and the selection it extends) and AdjustPosition, over generated texts (spaces,
// letters, control and wide units) and positions (a fixed LCG). Prints one line per failed check; the exit code
// is the number of failed checks.
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <algorithm>
#include <vector>
#include "rust/ee_won.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 4242;
static unsigned int Next() { sSeed = sSeed * 1103515245u + 12345u; return sSeed >> 8; }
static int Small() { return (int)(Next() % 300) - 50; }

typedef std::vector<wchar_t> Text;
static Text RandText(int maxLen)
{
    static const wchar_t kUnits[] = { L' ', L' ', L'a', L'b', L'Z', L'1', L'-', L'\t', 1, 31, 32, 127, 0xe9, 0x4e2d,
                                      0xd83d, 0xfffd };
    int n = (int)(Next() % (unsigned)(maxLen + 1));
    Text t;
    for (int i = 0; i < n; i++) t.push_back(kUnits[Next() % (sizeof(kUnits) / sizeof(kUnits[0]))]);
    return t;
}
static const void* P(const Text& t) { return t.empty() ? NULL : &t[0]; }

// ---- the original's statements

static int OrigMoveCursor(bool left, bool wholeWord, const Text& theStr, int thePos)
{
    int dx = left ? -1 : 1;
    int aLength = (int)theStr.size();
    thePos+=dx;
    if(wholeWord)
    {
        while(thePos>=0 && thePos<aLength)
        {
            if((theStr[thePos]!=' '))
                thePos+=dx;
            else
                break;
        }
        while(thePos>=0 && thePos<aLength)
        {
            if((theStr[thePos]==' '))
                thePos+=dx;
            else
                break;
        }
        if(left)
            thePos++;
    }
    if(thePos<0)
        thePos = 0;
    if(thePos>aLength)
        thePos = aLength;
    return thePos;
}

struct Sel { int mSelStartChar, mSelEndChar; bool mBackwardsSel; };
static void OrigSelectRange(Sel& s, int theStartPos, int theEndPos, int aLength)
{
    if(theStartPos<0) theStartPos = aLength;
    if(theEndPos<0) theEndPos = aLength;
    if(theStartPos>aLength) theStartPos = aLength;
    if(theEndPos>aLength) theEndPos = aLength;
    s.mBackwardsSel = theStartPos>theEndPos;
    if(s.mBackwardsSel)
        std::swap(theStartPos,theEndPos);
    s.mSelStartChar = theStartPos;
    s.mSelEndChar = theEndPos;
}

// KeyDown's 36, 35, 37, 39; false for another key
static bool OrigKeySel(Sel& s, int theKey, bool shift, bool ctrl, const Text& mText)
{
    int aLength = (int)mText.size();
    switch(theKey)
    {
        case 36:
            if(!shift)
                s.mSelEndChar = 0;
            else if(!s.mBackwardsSel)
                s.mSelEndChar = s.mSelStartChar;
            s.mSelStartChar = 0;
            s.mBackwardsSel =  s.mSelStartChar!=s.mSelEndChar;
            break;
        case 35:
            if(!shift)
                s.mSelStartChar = aLength;
            else if(s.mBackwardsSel)
                s.mSelStartChar = s.mSelEndChar;
            s.mBackwardsSel = false;
            s.mSelEndChar = aLength;
            break;
        case 37:
        case 39:
            if(!shift && s.mSelStartChar!=s.mSelEndChar)
            {
                if(theKey==37)
                    s.mSelEndChar = s.mSelStartChar;
                else
                    s.mSelStartChar = s.mSelEndChar;
            }
            else if(s.mBackwardsSel)
            {
                s.mSelStartChar = OrigMoveCursor(theKey==37,ctrl,mText,s.mSelStartChar);
                if(!shift)
                    s.mSelEndChar = s.mSelStartChar;
            }
            else
            {
                s.mSelEndChar = OrigMoveCursor(theKey==37,ctrl,mText,s.mSelEndChar);
                if(!shift)
                    s.mSelStartChar = s.mSelEndChar;
            }
            if(s.mSelStartChar>s.mSelEndChar)
            {
                s.mBackwardsSel = !s.mBackwardsSel;
                std::swap(s.mSelStartChar, s.mSelEndChar);
            }
            else if(s.mSelStartChar==s.mSelEndChar)
                s.mBackwardsSel = false;
            break;
        default:
            return false;
    }
    return true;
}

// KeyDown's 8 / 46: which assigns ran (left: (0, count); right: (start)) as -1 for none
static void OrigDelete(int theKey, int mSelStartChar, int mSelEndChar, int aLength, int& leftCount, int& rightStart)
{
    leftCount = -1; rightStart = -1;
    if(mSelStartChar!=mSelEndChar)
    {
        leftCount = mSelStartChar;
        rightStart = mSelEndChar;
    }
    else
    {
        if(theKey==8)
        {
            if(mSelStartChar>0)
                leftCount = mSelStartChar-1;
            rightStart = mSelEndChar;
        }
        else
        {
            leftCount = mSelStartChar;
            if(mSelEndChar<aLength)
                rightStart = mSelEndChar+1;
        }
    }
}

// SetSelText's cut and limit: the pasted length it keeps (-1: the text is not pasted at all is the caller's)
static int OrigPasteLength(const Text& theText, int aTextLength, int mSelStartChar, int mSelEndChar, int mMaxChars,
                           int& outCut)
{
    int aPasteLength = (int)theText.size();
    for(int i=0; i<aPasteLength; i++)
    {
        int aChar = theText[i];
        if(aChar<32)
        {
            aPasteLength = i;
            break;
        }
    }
    outCut = aPasteLength;
    if(mMaxChars>0 && aTextLength+aPasteLength-(mSelEndChar-mSelStartChar)>mMaxChars)
    {
        int aNumCharsAllowed = mMaxChars-aTextLength+(mSelEndChar-mSelStartChar);
        if(aNumCharsAllowed<0)
            aNumCharsAllowed = 0;
        aPasteLength = aNumCharsAllowed;
    }
    return aPasteLength;
}

struct Blink { int mCursorDelay; bool mCursorOn; bool mInvalidated; };
static void OrigBlink(Blink& b, int theDelta, int mCursorOnTime, int mCursorOffTime)
{
    b.mInvalidated = false;
    b.mCursorDelay+=theDelta;
    if(b.mCursorOn && b.mCursorDelay>mCursorOnTime)
    {
        b.mCursorDelay%=mCursorOnTime;
        b.mCursorOn = false;
        b.mInvalidated = true;
    }
    else if(!b.mCursorOn && b.mCursorDelay>mCursorOffTime)
    {
        b.mCursorDelay%=mCursorOffTime;
        b.mCursorOn = true;
        b.mInvalidated = true;
    }
}

// CheckSel with the font's widths: widths[i] is the width of character i, the prefix width the sum of the first n
static void OrigCheckSel(Sel& s, int x, bool newStart, int aBoxWidth, int mHorzOffset, int aTextLeft,
                         const std::vector<int>& widths)
{
    int aLength = (int)widths.size();
    if(x<0) x = 0;
    else if(x>aBoxWidth) x = aBoxWidth;
    x += mHorzOffset - aTextLeft;
    int aSelChar = s.mBackwardsSel?s.mSelStartChar:s.mSelEndChar;
    int aWidth = 0;
    for (int i = 0; i < aSelChar && i < aLength; i++) aWidth += widths[i]; // GetStringWidth(mText, 0, aSelChar)
    if(aWidth >= x)
    {
        aSelChar--;
        while(aSelChar>=0 && aWidth>=x)
        {
            aWidth-=widths[aSelChar];
            if(aWidth>=x)
                aSelChar--;
        }
        aSelChar++;
    }
    else
    {
        while(aSelChar<aLength && aWidth<x)
        {
            aWidth+=widths[aSelChar];
                aSelChar++;
        }
    }
    if(aSelChar<0)
        aSelChar=0;
    if(aSelChar>aLength)
        aSelChar = aLength;
    if(newStart)
    {
        s.mSelStartChar = s.mSelEndChar = aSelChar;
        s.mBackwardsSel = false;
    }
    else
    {
        if(s.mBackwardsSel)
            s.mSelStartChar = aSelChar;
        else
            s.mSelEndChar = aSelChar;
        if(s.mSelStartChar > s.mSelEndChar)
        {
            s.mBackwardsSel = !s.mBackwardsSel;
            std::swap(s.mSelStartChar,s.mSelEndChar);
        }
    }
}

static int OrigAdjustPosition(int aPreWidth, int anM, int mHorzOffset, int aRectWidth)
{
    if(aPreWidth - mHorzOffset < 0)
    {
        mHorzOffset = aPreWidth - 10*anM;
        if(mHorzOffset<0)
            mHorzOffset = 0;
    }
    if(aPreWidth - mHorzOffset >= aRectWidth)
    {
        if(aRectWidth!=0 && aPreWidth-mHorzOffset!=0)
            mHorzOffset = aPreWidth - aRectWidth + anM;
    }
    return mHorzOffset;
}

// TimerEvent's drag scroll around CheckSel: false while the mouse is in the box
static bool OrigDragPre(int x, int y, int aWidth, int aHeight, int aCharPos, int aLength, int& mHorzOffset)
{
    if((x>=0 && x<=aWidth && y>=0 && y<=aHeight))
        return false;
    if(x>aWidth && aCharPos!=aLength)
    {
        mHorzOffset += x-aWidth;
        if(mHorzOffset<0)
            mHorzOffset = 0;
    }
    else if(x<0 && aCharPos!=0)
        mHorzOffset += x;
    return true;
}
static int OrigDragPost(int aCharPos, int aLength, int aTextWidth, int anM, int aRectWidth, int mHorzOffset)
{
    if(aCharPos==aLength)
    {
        if(aTextWidth + mHorzOffset > aRectWidth)
            mHorzOffset = aTextWidth-aRectWidth+anM;
    }
    if(aCharPos==0)
        mHorzOffset = 0;
    if(mHorzOffset<0)
        mHorzOffset = 0;
    return mHorzOffset;
}

// ---- the checks

static Sel RandSel(int aLength)
{
    Sel s;
    s.mSelStartChar = (int)(Next() % (unsigned)(aLength + 1));
    s.mSelEndChar = (int)(Next() % (unsigned)(aLength + 1));
    if (s.mSelStartChar > s.mSelEndChar) std::swap(s.mSelStartChar, s.mSelEndChar);
    if (Next() % 3 == 0) s.mSelEndChar = s.mSelStartChar;
    s.mBackwardsSel = Next() % 2 == 0 && s.mSelStartChar != s.mSelEndChar;
    return s;
}
static bool Same(const Sel& a, const EeWonSel& b)
{
    return a.mSelStartChar == b.start && a.mSelEndChar == b.end && a.mBackwardsSel == b.backwards;
}
static EeWonSel R(const Sel& s) { EeWonSel r = { s.mSelStartChar, s.mSelEndChar, s.mBackwardsSel }; return r; }

static void TestCursor()
{
    for (int n = 0; n < 60000; n++) {
        Text t = RandText(n % 10 == 0 ? 60 : 12);
        int aLength = (int)t.size();
        for (int pos = -2; pos <= aLength + 2; pos++)
            for (int k = 0; k < 4; k++) {
                bool left = (k & 1) != 0, word = (k & 2) != 0;
                CHECK(OrigMoveCursor(left, word, t, pos) ==
                      ee_won_gui_move_cursor(left, word, P(t), (uint16_t)aLength, sizeof(wchar_t), pos));
            }
        Sel s;
        int a = Small() / 10, b = Small() / 10;
        OrigSelectRange(s, a, b, aLength);
        CHECK(Same(s, ee_won_gui_select_range(a, b, aLength)));

        static const int kKeys[] = { 35, 36, 37, 39, 8, 13, 38, 40, 46 };
        for (int k = 0; k < 9; k++)
            for (int m = 0; m < 4; m++) {
                Sel o = RandSel(aLength);
                EeWonSel r = R(o);
                bool shift = (m & 1) != 0, ctrl = (m & 2) != 0;
                bool ok = OrigKeySel(o, kKeys[k], shift, ctrl, t);
                bool rk = ee_won_gui_key_selection(kKeys[k], shift, ctrl, &r, P(t), (uint16_t)aLength, sizeof(wchar_t));
                CHECK(ok == rk);
                CHECK(Same(o, r));
            }

        Sel d = RandSel(aLength);
        for (int k = 0; k < 2; k++) {
            int lc, rs;
            OrigDelete(k == 0 ? 8 : 46, d.mSelStartChar, d.mSelEndChar, aLength, lc, rs);
            EeWonDeleteParts p = ee_won_gui_delete_parts(k == 0, d.mSelStartChar, d.mSelEndChar, aLength);
            CHECK(p.left_count == lc && p.right_start == rs);
        }

        Text aPaste = RandText(8);
        int aMax = (int)(Next() % 20) - 2, aCut = 0;
        int o = OrigPasteLength(aPaste, aLength, d.mSelStartChar, d.mSelEndChar, aMax, aCut);
        int rc = ee_won_gui_control_cut(P(aPaste), (uint16_t)aPaste.size(), sizeof(wchar_t));
        CHECK(rc == aCut);
        int ra = ee_won_gui_paste_allowed(aLength, rc, d.mSelStartChar, d.mSelEndChar, aMax);
        CHECK((ra < 0 ? rc : ra) == o);
    }
}

static void TestTimers()
{
    for (int n = 0; n < 300000; n++) {
        Blink b = { Small() * 3, (n & 1) != 0, false };
        int on = (int)(Next() % 900) + 1, off = (int)(Next() % 900) + 1, delta = (int)(Next() % 700);
        if (n % 9 == 0) off = on;
        EeWonBlink r = ee_won_gui_blink(b.mCursorDelay, delta, b.mCursorOn, on, off);
        OrigBlink(b, delta, on, off);
        CHECK(b.mCursorDelay == r.delay && b.mCursorOn == r.on && b.mInvalidated == r.changed);
        int aRate = Small();
        CHECK(ee_won_gui_blink_off_time(on, aRate) == (aRate <= 0 ? on : aRate));
    }
}

static void TestMouse()
{
    for (int n = 0; n < 200000; n++) {
        int aLength = (int)(Next() % 20);
        std::vector<int> w;
        for (int i = 0; i < aLength; i++) w.push_back((int)(Next() % 12) + (n % 17 == 0 ? 0 : 1));
        Sel o = RandSel(aLength);
        EeWonSel r = R(o);
        int x = Small(), aBox = (int)(Next() % 200), aOff = Small() / 3, aLeft = (int)(Next() % 10);
        bool aNew = (n & 1) != 0;
        int aSelChar = o.mBackwardsSel ? o.mSelStartChar : o.mSelEndChar, aPrefix = 0;
        for (int i = 0; i < aSelChar; i++) aPrefix += w[i];
        OrigCheckSel(o, x, aNew, aBox, aOff, aLeft, w);
        ee_won_gui_check_sel(x, aBox, aOff, aLeft, aPrefix, aSelChar, w.empty() ? NULL : &w[0], w.size(), aNew, &r);
        CHECK(Same(o, r));

        int aPre = Small() * 2, anM = (int)(Next() % 12), aHorz = Small(), aRect = (int)(Next() % 200);
        if (n % 7 == 0) aRect = 0;
        if (n % 11 == 0) aHorz = aPre;
        CHECK(OrigAdjustPosition(aPre, anM, aHorz, aRect) == ee_won_gui_cursor_scroll(aPre, anM, aHorz, aRect));

        int y = Small(), h = (int)(Next() % 50), pos = (int)(Next() % (unsigned)(aLength + 1)), oh = aHorz, rh = -77;
        bool od = OrigDragPre(x, y, aBox, h, pos, aLength, oh);
        bool rd = ee_won_gui_drag_scroll(x, y, aBox, h, pos, aLength, aHorz, &rh);
        CHECK(od == rd);
        if (od && rd) CHECK(oh == rh);
        int tw = Small() * 3;
        CHECK(OrigDragPost(pos, aLength, tw, anM, aRect, aHorz) ==
              ee_won_gui_drag_scroll_end(pos, aLength, tw, anM, aRect, aHorz));
    }
}

int main()
{
    TestCursor();
    TestTimers();
    TestMouse();
    printf("won_guiedit_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
