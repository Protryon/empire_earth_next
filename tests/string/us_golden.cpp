// Golden driver for UString's searches and tokenizers (Find, FindBackwards, Tokenize, NestedTokenize): random
// strings over a small alphabet through the original class (us_orig.h). The Rust replay
// (rust/crates/ee_base/src/ustring/golden.rs) makes the same searches and splits with the Rust functions and regenerates
// every line.
//
//   python -I build/lle3/cc.py build/lle3/usg.exe tests/string/us_golden.cpp -Itests/string
//   build/lle3/usg.exe <cases> [first seed] > golden.txt
//
// Lines (strings as hex bytes, "-" for empty):
//   find <text> <needle> <start> = <index>          Find(UString), then Find(const char*) (the same result twice)
//   back <text> <needle> <start> = <index>          FindBackwards, both overloads
//   tok <text> <separators> = <token> ...           Tokenize
//   nest <text> <separators> <brackets> = <token> ...   NestedTokenize
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <vector>

#define LLE_API
#define PLAT_PRINTF_FORMAT(a, b)
#define LOWLEVELENGINE_EXPORTS
enum { kErrorOK = 0, kErrorFailed = 9 };
class FSFile {
public:
    long Read(unsigned long&) { return kErrorFailed; }
    long Read(char*, unsigned long, unsigned long&) { return kErrorFailed; }
    long Write(unsigned long) { return kErrorFailed; }
    long Write(const char*, unsigned long, unsigned long&) { return kErrorFailed; }
};
static int PlatStrICmp(const char* a, const char* b) { return _stricmp(a, b); }
static int PlatStrNICmp(const char* a, const char* b, size_t n) { return _strnicmp(a, b, n); }
static void PlatMbsLwr(unsigned char*) {}
static void PlatMbsUpr(unsigned char*) {}
static int PlatUtf8IsValid(const char*, size_t) { return 1; }
static uint32_t PlatCp1252ToUtf8(const char*, size_t, char*, size_t) { return 0; }

#include "us_orig.h"

static uint32_t sRandom;
static uint32_t Next()
{
    sRandom ^= sRandom << 13;
    sRandom ^= sRandom >> 17;
    sRandom ^= sRandom << 5;
    return sRandom;
}
static uint32_t Below(uint32_t n) { return Next() % n; }

static void Hex(const char* s)
{
    if (!*s)
        printf(" -");
    else {
        printf(" ");
        for (; *s; s++)
            printf("%02x", (unsigned char)*s);
    }
}
static std::vector<char> RandomText(const char* inAlphabet, uint32_t inMax)
{
    std::vector<char> theText;
    uint32_t n = Below(inMax + 1);
    size_t theCount = strlen(inAlphabet);
    for (uint32_t i = 0; i < n; i++)
        theText.push_back(inAlphabet[Below((uint32_t)theCount)]);
    theText.push_back(0);
    return theText;
}

int main(int argc, char** argv)
{
    int theCount = argc > 1 ? atoi(argv[1]) : 100;
    int theFirst = argc > 2 ? atoi(argv[2]) : 1;
    static const char* kAlphabets[] = {"ab", "abc ,", "a b,;()\"", "xy[]{} ,\t", "a\xc3\xa9 ,()"};
    static const char* kSeparators[] = {" ", " ,", ",;", "", "()", "\t ", "a", " ,;\"", "\xc3"};
    static const char* kBrackets[] = {"()", "[]", "{}", "\"\"", "((", "(", ")("};
    for (int n = theFirst; n < theFirst + theCount; n++) {
        sRandom = 0x51ed2701u ^ (uint32_t)n * 2654435761u;
        if (sRandom == 0)
            sRandom = 1;
        const char* theAlphabet = kAlphabets[Below(5)];
        std::vector<char> theText = RandomText(theAlphabet, Below(4) == 0 ? 60 : 14);
        UString theString(&theText[0]);
        switch (Below(4)) {
        case 0:
        case 1: {
            std::vector<char> theNeedle = RandomText(theAlphabet, Below(3) == 0 ? 6 : 2);
            unsigned long theLength = theString.GetLength();
            unsigned long theStart = Below((uint32_t)theLength + 4);
            if (theStart > theLength && strlen(&theNeedle[0]) > theLength)
                theStart = theLength;   // the original reads past the string there
            bool theBack = Below(2);
            if (theBack && Below(3) == 0)
                theStart = Below(4) ? 0xFFFFFFFFu : theLength + 1 + Below(100);
            UString theNeedleString(&theNeedle[0]);
            unsigned long a = theBack ? theString.FindBackwards(theNeedleString, theStart)
                                      : theString.Find(theNeedleString, theStart);
            unsigned long b = theBack ? theString.FindBackwards(&theNeedle[0], theStart)
                                      : theString.Find(&theNeedle[0], theStart);
            printf(theBack ? "back" : "find");
            Hex(&theText[0]);
            Hex(&theNeedle[0]);
            printf(" %lu = %lu %lu\n", theStart, a, b);
            break;
        }
        case 2: {
            const char* theSeparators = kSeparators[Below(9)];
            std::vector<UString> theTokens;
            theString.Tokenize(UString(theSeparators), theTokens);
            printf("tok");
            Hex(&theText[0]);
            Hex(theSeparators);
            printf(" =");
            for (size_t k = 0; k < theTokens.size(); k++)
                Hex(theTokens[k]);
            printf("\n");
            break;
        }
        default: {
            const char* theSeparators = kSeparators[Below(9)];
            const char* theBrackets = kBrackets[Below(7)];
            // same-character brackets: a text starting with a lone quote makes the original read out of bounds
            if (theBrackets[0] == theBrackets[1] && theText[0] == theBrackets[0])
                theText[0] = 'a';
            UString theNested(&theText[0]);
            std::vector<UString> theTokens;
            theNested.NestedTokenize(UString(theSeparators), UString(theBrackets), theTokens);
            printf("nest");
            Hex(&theText[0]);
            Hex(theSeparators);
            Hex(theBrackets);
            printf(" =");
            for (size_t k = 0; k < theTokens.size(); k++)
                Hex(theTokens[k]);
            printf("\n");
            break;
        }
        }
    }
    return 0;
}
