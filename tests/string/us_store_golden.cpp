// Golden driver for UString's storage (every member: the constructors, assignment, appends, Copy, substrings,
// concatenation, comparisons, searches, tokenizers, Load, Save, sprintf, Compact, the default capacity, the legacy
// text load): random sequences on four strings through the original class (us_orig.h, the unmodified UString). The
// Rust replay (rust/crates/ee_base/src/ustring/golden_store.rs) makes the same calls on Rust records and regenerates
// every line. The heap is zero-filled (operator new[] below) so the bytes past a terminator, which the original
// copies and prints here, are what the Rust buffer holds.
//
//   python -I build/lle4/cc.py build/lle4/ussg.exe tests/string/us_store_golden.cpp -Itests/string
//   build/lle4/ussg.exe <sequences> [first seed] > golden.txt
// Relinked over the new class (-DUS_NEW: include/lle/UString.h and UString.cpp's forwarders, with the Rust crates
// ee_ustring and ee_plat_core as a static library: build/lle4/usrelink) it must print the same.
//
// Strings are hex bytes ("-" for none). A state is "= <length> <capacity> <the capacity's bytes>". Lines:
//   seq <n>                                         a sequence starts (the four strings are made by `new` lines)
//   def <length> | leg <0/1>                        SetDefaultAllocationLength, USetLegacyTextLoad
//   new <k> d | n <capacity> | s <text> | c <j>  <state>       the four constructors (slot k replaced)
//   asg <k> <j> <state> | asgs <k> <text> <state>  operator=
//   add <k> <j> <state> | adds <k> <text> <state>  operator+=
//   plus <k> <i> <j> <state> | pluss <k> <i> <text> <state>   slot k = new UString(slot i + ...)
//   sub <k> <i> <start> <length> <state> | isub <k> <i> <first> <last> <state>   (Get/GetIndexed)Substring
//   copy <k> <j> <start> <count> <dest> <resize> <state> | copys <k> <text> ...   Copy
//   compact <k> <state> | at <k> <index> = <char> | size <k> = <SizeOf> <GetBytesUsedToSave>
//   find|back <k> <j> <start> = <r> | finds|backs <k> <text> <start> = <r>
//   cmp <i> <j> <count> = <bits> | cmps <i> <text> <count> = <bits>   == != < <= > >=, Equals, Equals(count)
//   tok <k> <j> = <token state>... | nest <k> <j> <brackets> = <token state>...
//   load <k> <mode> <length> <data> = <result> <state>    mode 0: the length's read fails, 1: reads, 2: the data's
//   save <k> <mode> = <result> <written>            mode 0: writes, 1: the length fails, 2: the data fails, 3: short
//   fmt <k> <text> <number> = <result> <state>      sprintf(s, "%s|%d"), only when it fits (the original wrote past)
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <new>
#include <vector>

void* operator new[](size_t n) { return calloc(n ? n : 1, 1); }
void operator delete[](void* p) noexcept { free(p); }

#define LLE_API
#ifdef US_NEW
#include "platform/PlatString.h"
#else
#define PLAT_PRINTF_FORMAT(a, b)
#define LOWLEVELENGINE_EXPORTS
#endif
enum { kErrorOK = 0, kErrorFailed = 9, kErrorScripted = 5 };

// a scripted file
class FSFile {
public:
    int mMode = 1;                          // Load: 0 the length fails, 1 reads, 2 the data fails
    unsigned long mLength = 0;
    std::vector<char> mData;
    int mWriteMode = 0;                     // Save: 0 writes, 1 the length fails, 2 the data fails, 3 short
    std::vector<unsigned char> mWritten;
    long Read(unsigned long& outValue)
    {
        if (mMode == 0)
            return kErrorScripted;
        outValue = mLength;
        return kErrorOK;
    }
    long Read(char* outData, unsigned long inLength, unsigned long& outRead)
    {
        if (mMode == 2) {
            outRead = 0;
            return kErrorScripted;
        }
        outRead = inLength < mData.size() ? inLength : (unsigned long)mData.size();
        memcpy(outData, mData.data(), outRead);
        return kErrorOK;
    }
    long Write(unsigned long inValue)
    {
        if (mWriteMode == 1)
            return kErrorScripted;
        for (int i = 0; i < 4; i++)
            mWritten.push_back((unsigned char)(inValue >> (8 * i)));
        return kErrorOK;
    }
    long Write(const char* inData, unsigned long inLength, unsigned long& outWritten)
    {
        if (mWriteMode == 2) {
            outWritten = 0;
            return kErrorScripted;
        }
        outWritten = mWriteMode == 3 ? inLength - 1 : inLength;
        mWritten.insert(mWritten.end(), inData, inData + outWritten);
        return kErrorOK;
    }
};
#ifndef US_NEW
static int PlatStrICmp(const char* a, const char* b) { return _stricmp(a, b); }
static int PlatStrNICmp(const char* a, const char* b, size_t n) { return _strnicmp(a, b, n); }
static void PlatMbsLwr(unsigned char*) {}
static void PlatMbsUpr(unsigned char*) {}
// well-formed UTF-8 (Rust's str::from_utf8)
static int PlatUtf8IsValid(const char* inText, size_t inLength)
{
    const unsigned char* s = (const unsigned char*)inText;
    size_t i = 0;
    while (i < inLength) {
        unsigned c = s[i];
        int n;
        unsigned lo = 0x80, hi = 0xBF;
        if (c < 0x80) { i++; continue; }
        else if (c >= 0xC2 && c <= 0xDF) n = 1;
        else if (c >= 0xE0 && c <= 0xEF) { n = 2; if (c == 0xE0) lo = 0xA0; if (c == 0xED) hi = 0x9F; }
        else if (c >= 0xF0 && c <= 0xF4) { n = 3; if (c == 0xF0) lo = 0x90; if (c == 0xF4) hi = 0x8F; }
        else return 0;
        for (int k = 1; k <= n; k++) {
            if (i + k >= inLength) return 0;
            unsigned d = s[i + k];
            if (k == 1 ? (d < lo || d > hi) : (d < 0x80 || d > 0xBF)) return 0;
        }
        i += n + 1;
    }
    return 1;
}
static const unsigned short kCp1252High[32] = {
    0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
    0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E,
    0x0178};
static uint32_t PlatCp1252ToUtf8(const char* inText, size_t inLength, char* outText, size_t inCapacity)
{
    std::vector<char> theOut;
    for (size_t i = 0; i < inLength; i++) {
        unsigned b = (unsigned char)inText[i];
        unsigned cp = b >= 0x80 && b < 0xA0 && kCp1252High[b - 0x80] ? kCp1252High[b - 0x80] : b;
        if (cp < 0x80)
            theOut.push_back((char)cp);
        else if (cp < 0x800) {
            theOut.push_back((char)(0xC0 | cp >> 6));
            theOut.push_back((char)(0x80 | (cp & 0x3F)));
        } else {
            theOut.push_back((char)(0xE0 | cp >> 12));
            theOut.push_back((char)(0x80 | (cp >> 6 & 0x3F)));
            theOut.push_back((char)(0x80 | (cp & 0x3F)));
        }
    }
    if (outText) {
        memcpy(outText, theOut.data(), theOut.size());
        outText[theOut.size()] = 0;
    }
    (void)inCapacity;
    return (uint32_t)theOut.size();
}

#endif

#ifdef US_NEW
#include "lle/UString.h"
#define LLE_LOWLEVELENGINE_H   // the unit's engine header: this driver supplies FSFile and the error codes
#include "../../src/Low-Level Engine/Utilities/UString.cpp"
#else
#include "us_orig.h"
#endif

static uint32_t sRandom;
static uint32_t Next()
{
    sRandom ^= sRandom << 13;
    sRandom ^= sRandom >> 17;
    sRandom ^= sRandom << 5;
    return sRandom;
}
static uint32_t Below(uint32_t n) { return Next() % n; }

static void Hex(const char* s, size_t n)
{
    if (!n)
        printf(" -");
    else {
        printf(" ");
        for (size_t i = 0; i < n; i++)
            printf("%02x", (unsigned char)s[i]);
    }
}
static void HexText(const char* s) { Hex(s, strlen(s)); }
static void State(const UString& s)
{
    printf(" = %lu %lu", s.GetLength(), s.GetAllocatedLength());
    Hex(s, s.GetAllocatedLength());
}

// a C string over a small alphabet (é as UTF-8 and as Windows-1252, a C1 byte)
static std::vector<char> RandomText(uint32_t inMax, bool inNul = false)
{
    static const char* kPieces[] = {"a", "b", " ", ",", "(", ")", "\"", "\xc3\xa9", "\xe9", "\x80", "A", "\0"};
    std::vector<char> theText;
    uint32_t n = Below(inMax + 1);
    for (uint32_t i = 0; i < n; i++) {
        uint32_t p = Below(inNul ? 12 : 11);
        if (p == 11)
            theText.push_back(0);
        else
            theText.insert(theText.end(), kPieces[p], kPieces[p] + strlen(kPieces[p]));
    }
    theText.push_back(0);
    return theText;
}

static UString* sSlots[4];

static void New(int k)
{
    UString* theString;
    switch (Below(4)) {
    case 0:
        printf("new %d d", k);
        theString = new UString();
        break;
    case 1: {
        unsigned long theCapacity = 1 + Below(Below(4) ? 12 : 300);
        printf("new %d n %lu", k, theCapacity);
        theString = new UString(theCapacity);
        break;
    }
    case 2: {
        std::vector<char> theText = RandomText(Below(3) ? 8 : 40);
        printf("new %d s", k);
        HexText(theText.data());
        theString = new UString(theText.data());
        break;
    }
    default: {
        int j = Below(4);
        if (!sSlots[j])
            j = k;
        if (!sSlots[j]) {
            printf("new %d d", k);
            theString = new UString();
        } else {
            printf("new %d c %d", k, j);
            theString = new UString(*sSlots[j]);
        }
        break;
    }
    }
    delete sSlots[k];
    sSlots[k] = theString;
    State(*theString);
    printf("\n");
}

static void Replace(int k, UString* inString)
{
    delete sSlots[k];
    sSlots[k] = inString;
    State(*inString);
    printf("\n");
}

static void Op()
{
    int k = Below(4), j = Below(4), i = Below(4);
    UString& s = *sSlots[k];
    std::vector<char> t = RandomText(Below(4) ? 6 : 30);
    const char* text = t.data();
    switch (Below(26)) {
    case 0:
        New(k);
        break;
    case 1: {
        static const unsigned long kDefaults[] = {1, 2, 3, 5, 8, 16, 40, 256};
        unsigned long d = kDefaults[Below(8)];
        printf("def %lu\n", d);
        UString::SetDefaultAllocationLength(d);
        break;
    }
    case 2: {
        int theLegacy = Below(2);
        printf("leg %d\n", theLegacy);
        USetLegacyTextLoad(theLegacy != 0);
        break;
    }
    case 3:
        printf("asg %d %d", k, j);
        s = *sSlots[j];
        State(s);
        printf("\n");
        break;
    case 4:
        printf("asgs %d", k);
        HexText(text);
        s = text;
        State(s);
        printf("\n");
        break;
    case 5:
        printf("add %d %d", k, j);
        s += *sSlots[j];
        State(s);
        printf("\n");
        break;
    case 6:
        printf("adds %d", k);
        HexText(text);
        s += text;
        State(s);
        printf("\n");
        break;
    case 7:
        printf("plus %d %d %d", k, i, j);
        Replace(k, new UString(*sSlots[i] + *sSlots[j]));
        break;
    case 8:
        printf("pluss %d %d", k, i);
        HexText(text);
        Replace(k, new UString(*sSlots[i] + text));
        break;
    case 9: {
        unsigned long theStart = Below(sSlots[i]->GetAllocatedLength() + 3), theLength = Below(20);
        printf("sub %d %d %lu %lu", k, i, theStart, theLength);
        Replace(k, new UString(sSlots[i]->GetSubstring(theStart, theLength)));
        break;
    }
    case 10: {
        unsigned long theFirst = Below(sSlots[i]->GetLength() + 3);
        unsigned long theLast = theFirst - 1 + Below(20);
        printf("isub %d %d %lu %lu", k, i, theFirst, theLast);
        Replace(k, new UString(sSlots[i]->GetIndexedSubstring(theFirst, theLast)));
        break;
    }
    case 11:
    case 12: {
        unsigned long theStart = Below(20), theCount = Below(20), theDestination = Below(20);
        bool theResize = Below(4) == 0;
        // from itself with a resize: the original reads the fresh buffer, past it when the start is past the count
        if (j == k && theResize && theStart > theCount)
            theStart = theCount;
        printf("copy %d %d %lu %lu %lu %d", k, j, theStart, theCount, theDestination, (int)theResize);
        s.Copy(*sSlots[j], theStart, theCount, theDestination, theResize);
        State(s);
        printf("\n");
        break;
    }
    case 13: {
        unsigned long theStart = Below(12), theCount = Below(20), theDestination = Below(20);
        bool theResize = Below(4) == 0;
        printf("copys %d", k);
        HexText(text);
        printf(" %lu %lu %lu %d", theStart, theCount, theDestination, (int)theResize);
        s.Copy(text, theStart, theCount, theDestination, theResize);
        State(s);
        printf("\n");
        break;
    }
    case 14:
        printf("compact %d", k);
        s.Compact();
        State(s);
        printf("\n");
        break;
    case 15: {
        int theIndex = (int)Below(s.GetLength() + 5) - 2;
        printf("at %d %d = %d\n", k, theIndex, (int)s[theIndex]);
        printf("size %d = %lu %lu\n", k, s.SizeOf(), s.GetBytesUsedToSave());
        break;
    }
    case 16:
    case 17: {
        bool theBack = Below(2);
        bool theText = Below(2);
        unsigned long theLength = s.GetLength();
        unsigned long theNeedle = theText ? (unsigned long)strlen(text) : sSlots[j]->GetLength();
        unsigned long theStart = Below(theLength + 4);
        if (theStart > theLength && theNeedle > theLength)
            theStart = theLength;   // the original reads past the string there
        if (theBack && Below(3) == 0)
            theStart = Below(4) ? 0xFFFFFFFFu : theLength + 1 + Below(100);
        unsigned long r;
        if (theText) {
            printf("%s %d", theBack ? "backs" : "finds", k);
            HexText(text);
            r = theBack ? s.FindBackwards(text, theStart) : s.Find(text, theStart);
        } else {
            printf("%s %d %d", theBack ? "back" : "find", k, j);
            r = theBack ? s.FindBackwards(*sSlots[j], theStart) : s.Find(*sSlots[j], theStart);
        }
        printf(" %lu = %lu\n", theStart, r);
        break;
    }
    case 18:
    case 19: {
        unsigned long theCount = Below(8);
        bool theText = Below(2);
        // the other text: often a prefix of the string, with or without a byte more
        if (theText && Below(2)) {
            const char* a = *sSlots[i];
            t.assign(a, a + sSlots[i]->GetLength());
            t.resize(Below((uint32_t)t.size() + 1));
            if (Below(2))
                t.push_back('a');
            t.push_back(0);
            text = t.data();
        }
        const UString& a = *sSlots[i];
        bool r[8];
        if (theText) {
            printf("cmps %d", i);
            HexText(text);
            bool v[8] = {a == text, a != text, a < text, a <= text, a > text, a >= text, a.Equals(text),
                         a.Equals(text, theCount)};
            memcpy(r, v, sizeof(v));
        } else {
            printf("cmp %d %d", i, j);
            const UString& b = *sSlots[j];
            bool v[8] = {a == b, a != b, a < b, a <= b, a > b, a >= b, a.Equals(b), a.Equals(b, theCount)};
            memcpy(r, v, sizeof(v));
        }
        printf(" %lu = ", theCount);
        for (int n = 0; n < 8; n++)
            printf("%d", (int)r[n]);
        printf("\n");
        break;
    }
    case 20:
    case 21: {
        std::vector<UString> theTokens;
        if (Below(2)) {
            printf("tok %d %d =", k, j);
            s.Tokenize(*sSlots[j], theTokens);
        } else {
            static const char* kBrackets[] = {"()", "\"\"", "((", ")(", "a "};
            const char* theBrackets = kBrackets[Below(5)];
            // same-character brackets: a text starting with a lone quote makes the original read out of bounds
            if (theBrackets[0] == theBrackets[1] && s[0] == theBrackets[0])
                break;
            printf("nest %d %d", k, j);
            HexText(theBrackets);
            printf(" =");
            s.NestedTokenize(*sSlots[j], UString(theBrackets), theTokens);
        }
        for (size_t n = 0; n < theTokens.size(); n++)
            State(theTokens[n]);
        printf("\n");
        break;
    }
    case 22:
    case 23: {
        FSFile theFile;
        std::vector<char> theData = RandomText(Below(3) ? 8 : 30, true);
        theData.pop_back();
        theFile.mMode = Below(8) == 0 ? (int)Below(3) : 1;
        switch (Below(6)) {
        case 0: theFile.mLength = 0; break;
        case 1: theFile.mLength = (unsigned long)theData.size() + 1; break;
        case 2: theFile.mLength = theData.empty() ? 0 : (unsigned long)theData.size() - 1; break;
        default: theFile.mLength = (unsigned long)theData.size(); break;
        }
        theFile.mData = theData;
        printf("load %d %d %lu", k, theFile.mMode, theFile.mLength);
        Hex(theData.data(), theData.size());
        long r = s.Load(theFile);
        printf(" = %ld", r);
        State(s);
        printf("\n");
        break;
    }
    case 24: {
        FSFile theFile;
        theFile.mWriteMode = Below(6) == 0 ? 1 + (int)Below(3) : 0;
        long r = s.Save(theFile);
        printf("save %d %d = %ld", k, theFile.mWriteMode, r);
        Hex((const char*)theFile.mWritten.data(), theFile.mWritten.size());
        printf("\n");
        break;
    }
    default: {
        int theNumber = (int)Below(2001) - 1000;
        int theNeeded = snprintf(NULL, 0, "%s|%d", text, theNumber);
        if ((unsigned long)theNeeded + 1 > s.GetAllocatedLength())
            break;
        printf("fmt %d", k);
        HexText(text);
        long r = sprintf(s, "%s|%d", text, theNumber);
        printf(" %d = %ld", theNumber, r);
        State(s);
        printf("\n");
        break;
    }
    }
}

int main(int argc, char** argv)
{
    int theCount = argc > 1 ? atoi(argv[1]) : 100;
    int theFirst = argc > 2 ? atoi(argv[2]) : 1;
    for (int n = theFirst; n < theFirst + theCount; n++) {
        sRandom = 0x2545f491u ^ (uint32_t)n * 2654435761u;
        if (sRandom == 0)
            sRandom = 1;
        printf("seq %d\n", n);
        static const unsigned long kDefaults[] = {1, 3, 8, 16, 40, 256};
        unsigned long d = kDefaults[Below(6)];
        printf("def %lu\n", d);
        UString::SetDefaultAllocationLength(d);
        printf("leg 0\n");
        USetLegacyTextLoad(false);
        for (int k = 0; k < 4; k++)
            New(k);
        int theOps = 10 + Below(40);
        for (int o = 0; o < theOps; o++)
            Op();
        for (int k = 0; k < 4; k++) {
            delete sSlots[k];
            sSlots[k] = NULL;
        }
    }
    return 0;
}
