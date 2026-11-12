// WONGUI's word wrap (WrappedText.cpp): the original's CalcWrap loop and SetMaxWidth (copied from the C++ before
// the port, the font replaced by a width per unit and the break list by a vector) against ee_won's guiwrap module
// through its C ABI, over generated texts (spaces, CR, LF, letters, wide units; widths from 0 up, some wider than
// the line) and line widths (a fixed LCG). Prints one line per failed check; the exit code is the number of failed
// checks.
#include <stdio.h>
#include <vector>
#include "rust/ee_won.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 2718;
static unsigned int Next() { sSeed = sSeed * 1103515245u + 12345u; return sSeed >> 8; }

struct Break { int mBreakPos, mNextStartPos, mWidth; };

// CalcWrap's loop (mText holds the units and the terminator)
static int OrigWrap(const std::vector<wchar_t>& mText, const std::vector<int>& widths, int mMaxWidth,
                    std::vector<Break>& mBreakList)
{
    int aLength = (int)mText.size() - 1;
    int aBreakPos = -1, aBreakWidth = 0;
    int aPos = 0, aWidth = 0, aMaxWidth = 0;
    while(aPos<aLength)
    {
        int aChar = mText[aPos];
        int aCharWidth = widths[aPos]; // GetCharacterWidth(aChar)
        if(aChar==' ')
        {
            aBreakPos = aPos;
            aBreakWidth = aWidth;
        }
        else if(aChar=='\r' || aChar=='\n')
        {
            aBreakPos = aPos;
            aBreakWidth = aWidth;
            if(aChar=='\r' && mText[aPos+1]=='\n')
                aPos++;
            aPos++;
            Break b = { aBreakPos, aPos, aBreakWidth };
            mBreakList.push_back(b);
            if(aBreakWidth > aMaxWidth)
                aMaxWidth = aBreakWidth;
            aBreakPos = -1;
            aWidth = 0;
            aBreakWidth = 0;
            continue;
        }
        if(aWidth+aCharWidth>mMaxWidth)
        {
            if(aBreakPos!=-1)
            {
                Break b = { aBreakPos, aBreakPos+1, aBreakWidth };
                mBreakList.push_back(b);
                aPos = aBreakPos;
                if(aBreakWidth > aMaxWidth)
                    aMaxWidth = aBreakWidth;
            }
            else
            {
                Break b = { aPos, aPos, aWidth };
                mBreakList.push_back(b);
                if(aWidth > aMaxWidth)
                    aMaxWidth = aWidth;
                if(aWidth>0)
                    aPos--;
            }
            aBreakPos = -1;
            aWidth = 0;
            aBreakWidth = 0;
        }
        else
            aWidth += aCharWidth;
        aPos++;
    }
    if(aWidth>aMaxWidth)
        aMaxWidth = aWidth;
    return aMaxWidth==0 ? aWidth : aMaxWidth;
}

int main()
{
    static const wchar_t kUnits[] = { L' ', L' ', L' ', L'a', L'b', L'c', L'd', L'\r', L'\n', 0xe9, 0x4e2d, L'-' };
    for (int n = 0; n < 400000; n++) {
        int aLength = (int)(Next() % 30);
        if (n % 100 == 0) aLength = (int)(Next() % 400);
        std::vector<wchar_t> aText;
        std::vector<int> aWidths;
        for (int i = 0; i < aLength; i++) {
            aText.push_back(kUnits[Next() % (sizeof(kUnits) / sizeof(kUnits[0]))]);
            aWidths.push_back(Next() % 13 == 0 ? (int)(Next() % 80) : (int)(Next() % 9));
        }
        aText.push_back(0);
        int aMax = (int)(Next() % 120) - 5;
        std::vector<Break> o;
        int ow = OrigWrap(aText, aWidths, aMax, o);
        std::vector<EeWonBreak> r(2 * aLength + 1);
        int rw = -1;
        size_t k = ee_won_gui_wrap(&aText[0], (uint16_t)aLength, sizeof(wchar_t), aLength ? &aWidths[0] : NULL, aMax,
                                   &r[0], r.size(), &rw);
        CHECK(ow == rw);
        CHECK(k == o.size());
        for (size_t i = 0; i < k && i < o.size(); i++)
            CHECK(r[i].pos == o[i].mBreakPos && r[i].next == o[i].mNextStartPos && r[i].width == o[i].mWidth);

        // SetMaxWidth
        int aNew = (int)(Next() % 60) - 10, anOld = (int)(Next() % 50);
        bool aWrap = (n & 1) != 0, aBreaks = (n & 2) != 0;
        int theMaxWidth = aNew, mMaxWidth = anOld;
        bool changed = true, calc = false;
        if(theMaxWidth<0) theMaxWidth = 0;
        if(theMaxWidth==mMaxWidth) changed = false;
        else {
            int anOldMax = mMaxWidth;
            mMaxWidth = theMaxWidth;
            if(aWrap)
                calc = anOldMax==0 || aBreaks || mMaxWidth<anOldMax;
        }
        int w = -1;
        bool c = false;
        bool rk = ee_won_gui_max_width(aNew, anOld, aWrap, aBreaks, &w, &c);
        CHECK(rk == changed);
        if (rk && changed) CHECK(w == mMaxWidth && c == calc);
    }
    printf("won_guiwrap_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
