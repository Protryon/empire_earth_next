// Golden driver for UWideString (every member: the constructors, assignment, CopyString, appends, InsertString,
// substrings, concatenation, Find, Tokenize, Export/Import, Load, Save, GetChecksum, sprintf, Clear, Reallocate,
// the default capacity): random sequences on four strings through the original class (uws_orig.h, the unmodified
// UWideString, over us_orig.h's UString). The platform's UTF-8 functions are the real ones (ee_plat_core, linked
// from build/lle4/usrelink), so the multibyte steps are the game's. The heap is zero-filled, as in
// us_store_golden.cpp. The Rust replay is rust/crates/ee_base/src/ustring/golden_wide.rs.
//
//   python -I build/lle4/cc.py build/lle4/uwsg.exe tests/string/uws_golden.cpp -Itests/string <usrelink.lib + libs>
//   build/lle4/uwsg.exe <sequences> [first seed] > golden.txt
// Relinked over the new classes (-DUS_NEW: include/lle/UString.h, UWideString.h and their units' forwarders) it
// must print the same.
//
// Strings are hex bytes ("-" for none). A state is "= <bytes> <characters> <capacity> <the capacity's bytes>".
//   seq <n> | def <length> | leg <0/1>
//   new <k> d | n <characters> | s <text> | c <j>  <state>
//   asg <k> <j> <state> | asgs <k> <text> <state> | add <k> <j> <state> | adds <k> <text> <state>
//   cpy <k> <j> <count> <state> | cpys <k> <text> <count> <state>        CopyString
//   ins <k> <text> <position> <replace> <state> | insw <k> <j> <position> <replace> <state>   InsertString
//   plus <k> <i> <j> <state> | pluss <k> <i> <text> <state> | isub <k> <i> <first> <last> <state>
//   at <k> <index> = <char> | size <k> = <GetBytesUsedToSave> <GetExportSize>
//   find <k> <j> <start> = <r> | finds <k> <text> <start> = <r> | tok <k> <j> = <token state>...
//   exp <k> = <bytes> | imp <k> <data> = <consumed> <state> | sum <k> = <name (hex)> <checksum>
//   clr <k> <state> | real <k> <size> <state>
//   load <k> <mode> <size> <data> = <result> <state> | save <k> <mode> = <result> <written>
//   fmt <k> <text> <number> = <result> <state>      sprintf(s, "%s|%d"), only when it fits
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <new>
#include <vector>

void* operator new[](size_t n) { return calloc(n ? n : 1, 1); }
void operator delete[](void* p) noexcept { free(p); }
void* operator new(size_t n) { return calloc(n ? n : 1, 1); }
void operator delete(void* p) noexcept { free(p); }
void operator delete(void* p, size_t) noexcept { free(p); }

#define LLE_API
#include "platform/PlatString.h"
#ifndef US_NEW
#define LOWLEVELENGINE_EXPORTS
#endif
enum { kErrorOK = 0, kErrorFailed = 9, kErrorScripted = 5 };

// a scripted file
class FSFile {
public:
    int mMode = 1;                          // Load: 0 the size fails, 1 reads, 2 the data fails
    unsigned long mLength = 0;
    std::vector<char> mData;
    int mWriteMode = 0;                     // Save: 0 writes, 1 the size fails, 2 the data fails, 3 short
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

#ifdef US_NEW
#include "lle/UString.h"
#include "lle/UWideString.h"
#else
#include "us_orig.h"
#endif

class ESChecksum {
public:
    void AddChecksum(const UString& inName, unsigned long inChecksum)
    {
        printf(" ");
        for (const char* c = inName; *c; c++)
            printf("%02x", (unsigned char)*c);
        printf(" %lu", inChecksum);
    }
};

#ifdef US_NEW
#define LLE_LOWLEVELENGINE_H   // the units' engine header: this driver supplies FSFile, ESChecksum and the error codes
#include "../../src/Low-Level Engine/Utilities/UString.cpp"
#include "../../src/Low-Level Engine/Utilities/UWideString.cpp"
#else
#include "uws_orig.h"
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
static void State(const UWideString& s)
{
    printf(" = %lu %lu %lu", s.GetTCHARLength(), s.GetLength(), s.GetAllocatedLength());
    Hex(s, s.GetAllocatedLength());
}
static unsigned long Characters(const char* s) { return (unsigned long)PlatMbsLen((const unsigned char*)s); }
// whether the string's length is its text's (a GetIndexedSubstring result's is the whole tail's: Find and Tokenize
// would walk past its terminator)
static bool Fresh(const UWideString& s) { return s.GetLength() == Characters(s); }

// a C string over a small alphabet (é, €, a stray Windows-1252 byte)
static std::vector<char> RandomText(uint32_t inMax, bool inNul = false)
{
    static const char* kPieces[] = {"a", "b", " ", ",", "(", "\xc3\xa9", "\xe2\x82\xac", "\xe9", "A", "\0"};
    std::vector<char> theText;
    uint32_t n = Below(inMax + 1);
    for (uint32_t i = 0; i < n; i++) {
        uint32_t p = Below(inNul ? 10 : 9);
        if (p == 9)
            theText.push_back(0);
        else
            theText.insert(theText.end(), kPieces[p], kPieces[p] + strlen(kPieces[p]));
    }
    theText.push_back(0);
    return theText;
}

static UWideString* sSlots[4];

static void Replace(int k, UWideString* inString)
{
    delete sSlots[k];
    sSlots[k] = inString;
    State(*inString);
    printf("\n");
}

static void New(int k)
{
    switch (Below(4)) {
    case 0:
        printf("new %d d", k);
        Replace(k, new UWideString());
        break;
    case 1: {
        unsigned long theCharacters = Below(Below(4) ? 6 : 140);
        printf("new %d n %lu", k, theCharacters);
        Replace(k, new UWideString(theCharacters));
        break;
    }
    case 2: {
        std::vector<char> theText = RandomText(Below(3) ? 8 : 40);
        printf("new %d s", k);
        HexText(theText.data());
        Replace(k, new UWideString(theText.data()));
        break;
    }
    default: {
        int j = Below(4);
        if (!sSlots[j]) {
            printf("new %d d", k);
            Replace(k, new UWideString());
        } else {
            printf("new %d c %d", k, j);
            Replace(k, new UWideString(*sSlots[j]));
        }
        break;
    }
    }
}

static void Op()
{
    int k = Below(4), j = Below(4), i = Below(4);
    UWideString& s = *sSlots[k];
    std::vector<char> t = RandomText(Below(4) ? 6 : 30);
    const char* text = t.data();
    switch (Below(28)) {
    case 0:
        New(k);
        break;
    case 1: {
        static const unsigned long kDefaults[] = {4, 8, 16, 32, 256};
        unsigned long d = kDefaults[Below(5)];
        printf("def %lu\n", d);
        UWideString::SetDefaultAllocationLength(d);
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
    case 5: {
        unsigned long theCount = Below(Characters(*sSlots[j]) + 1);
        printf("cpy %d %d %lu", k, j, theCount);
        s.CopyString(*sSlots[j], theCount);
        State(s);
        printf("\n");
        break;
    }
    case 6: {
        unsigned long theCount = Below(Characters(text) + 1);
        printf("cpys %d", k);
        HexText(text);
        printf(" %lu", theCount);
        s.CopyString(text, theCount);
        State(s);
        printf("\n");
        break;
    }
    case 7:
        if (j == k)   // strcat onto itself
            break;
        printf("add %d %d", k, j);
        s += *sSlots[j];
        State(s);
        printf("\n");
        break;
    case 8:
        printf("adds %d", k);
        HexText(text);
        s += text;
        State(s);
        printf("\n");
        break;
    case 9:
    case 10: {
        unsigned long thePosition = Below(Characters(s) + 1);
        bool theReplace = Below(2);
        if (Below(2)) {
            printf("ins %d", k);
            HexText(text);
            printf(" %lu %d", thePosition, (int)theReplace);
            s.InsertString(text, thePosition, theReplace);
        } else {
            printf("insw %d %d %lu %d", k, j, thePosition, (int)theReplace);
            s.InsertString(*sSlots[j], thePosition, theReplace);
        }
        State(s);
        printf("\n");
        break;
    }
    case 11:
        printf("plus %d %d %d", k, i, j);
        Replace(k, new UWideString(*sSlots[i] + *sSlots[j]));
        break;
    case 12:
        printf("pluss %d %d", k, i);
        HexText(text);
        Replace(k, new UWideString(*sSlots[i] + text));
        break;
    case 13: {
        unsigned long theFirst = Below(Characters(*sSlots[i]) + 3);
        unsigned long theLast = theFirst - 1 + Below(10);
        printf("isub %d %d %lu %lu", k, i, theFirst, theLast);
        Replace(k, new UWideString(sSlots[i]->GetIndexedSubstring(theFirst, theLast)));
        break;
    }
    case 14: {
        int theIndex = (int)Below(s.GetLength() + 5) - 2;
        printf("at %d %d = %d\n", k, theIndex, (int)s[theIndex]);
        printf("size %d = %lu %lu\n", k, s.GetBytesUsedToSave(), s.GetExportSize());
        break;
    }
    case 15:
    case 16: {
        if (!Fresh(s) || !Fresh(*sSlots[j]))
            break;
        unsigned long theStart = Below(s.GetLength() + 1);
        unsigned long r;
        if (Below(2)) {
            printf("finds %d", k);
            HexText(text);
            r = s.Find(text, theStart);
        } else {
            printf("find %d %d", k, j);
            r = s.Find(*sSlots[j], theStart);
        }
        printf(" %lu = %lu\n", theStart, r);
        break;
    }
    case 17:
    case 18: {
        if (!Fresh(s) || !Fresh(*sSlots[j]))
            break;
        std::vector<UWideString> theTokens;
        printf("tok %d %d =", k, j);
        s.Tokenize(*sSlots[j], theTokens);
        for (size_t n = 0; n < theTokens.size(); n++)
            State(theTokens[n]);
        printf("\n");
        break;
    }
    case 19: {
        std::vector<unsigned char> theData(sSlots[j]->GetExportSize());
        sSlots[j]->Export(theData.data());
        printf("imp %d", k);
        Hex((const char*)theData.data(), theData.size());
        unsigned long r = s.Import(theData.data());
        printf(" = %lu", r);
        State(s);
        printf("\n");
        break;
    }
    case 20: {
        std::vector<unsigned char> theData(s.GetExportSize() + 3, 0xee);
        s.Export(theData.data());
        printf("exp %d =", k);
        Hex((const char*)theData.data(), theData.size());
        printf("\n");
        break;
    }
    case 21:
        if (strlen(s) > 200)   // the name is a 256-byte UString
            break;
        printf("sum %d =", k);
        {
            ESChecksum theChecksum;
            s.GetChecksum(theChecksum);
        }
        printf("\n");
        break;
    case 22:
        if (Below(2)) {
            printf("clr %d", k);
            s.Clear();
        } else {
            unsigned long theSize = Below(20);
            printf("real %d %lu", k, theSize);
            s.Reallocate(theSize);
        }
        State(s);
        printf("\n");
        break;
    case 23:
    case 24: {
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
    case 25: {
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
        sRandom = 0x7f4a7c15u ^ (uint32_t)n * 2654435761u;
        if (sRandom == 0)
            sRandom = 1;
        printf("seq %d\n", n);
        static const unsigned long kDefaults[] = {4, 8, 16, 256};
        unsigned long d = kDefaults[Below(4)];
        printf("def %lu\n", d);
        UWideString::SetDefaultAllocationLength(d);
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
