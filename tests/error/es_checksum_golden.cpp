// Golden driver for the Error System's checksum sets (ESChecksum, ESChecksumEntry): random call sequences over the
// original classes (es_checksum_orig.h) on three sets, one trace line per call with its results. The Rust replay
// (rust/crates/ee_error/src/golden_checksum.rs) makes the same calls on the Rust sets and compares every line.
//
//   python -I build/lle1/cc.py build/lle2/esc.exe tests/error/es_checksum_golden.cpp -Itests/error
//   build/lle2/esc.exe <sequences> [first seed] > golden.txt
//
// Lines (fields separated by one space; descriptions and error texts as hex bytes, "-" for empty):
//   seq <n> <verbose 0/1 x3>                 three fresh sets
//   add <i> <description> <value>            AddChecksum(description, value) (value: 32-bit)
//   addv <i> <value>                         the protected AddChecksum(value)
//   copy <i> <j>                             set i = set j (the implicit operator=)
//   new <i> <verbose>                        set i = a fresh set
//   total <i> = <total> <verbose 0/1>
//   dump <i> = <count> [<description>:<value> ...]
//   cmp <i> <j> = <result 0/1> <error count> [<error> ...]   set i Compare(set j)
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <string>
#include <vector>

#define LLE_API
struct UString {
    std::string s;
    UString() {}
    UString(const char* inText) : s(inText) {}
    bool operator!=(const UString& inOther) const { return s != inOther.s; }
    operator const char*() const { return s.c_str(); }
};
static long sprintf(UString& outString, const char* inFormat, ...)
{
    char theText[4096];
    va_list theArguments;
    va_start(theArguments, inFormat);
    long theResult = vsnprintf(theText, sizeof(theText), inFormat, theArguments);
    va_end(theArguments);
    outString.s = theText;
    return theResult;
}

#define protected public
#include "es_checksum_orig.h"
#undef protected

static uint32_t sRandom;
static uint32_t Next()
{
    sRandom ^= sRandom << 13;
    sRandom ^= sRandom >> 17;
    sRandom ^= sRandom << 5;
    return sRandom;
}
static uint32_t Below(uint32_t n) { return Next() % n; }

static void Hex(const std::string& s)
{
    if (s.empty())
        printf("-");
    for (unsigned char c : s)
        printf("%02x", c);
}

static const char* kDescriptions[] = {"", "EEMap", "EEPlayer", "Unit 4", "EEPChecksum", "Tech", "x%sy", "EEMap "};

static uint32_t Value()
{
    switch (Below(4)) {
    case 0: return Next();
    case 1: return 0xffffffffu - Below(10);
    default: return Below(1000);
    }
}

static void Sequence(int inIndex)
{
    int v[3] = {(int)Below(2), (int)Below(2), (int)Below(2)};
    std::vector<ESChecksum*> theSets;
    for (int i = 0; i < 3; i++)
        theSets.push_back(new ESChecksum(v[i] != 0));
    printf("seq %d %d %d %d\n", inIndex, v[0], v[1], v[2]);
    for (uint32_t k = 20 + Below(60); k > 0; k--) {
        uint32_t theOp = Below(100);
        int i = Below(3), j = Below(3);
        ESChecksum& c = *theSets[i];
        if (theOp < 35) {
            const char* d = kDescriptions[Below(8)];
            uint32_t theValue = Value();
            printf("add %d ", i);
            Hex(d);
            printf(" %u\n", theValue);
            c.AddChecksum(UString(d), theValue);
        } else if (theOp < 50) {
            uint32_t theValue = Value();
            printf("addv %d %u\n", i, theValue);
            c.AddChecksum((unsigned long)theValue);
        } else if (theOp < 56) {
            printf("copy %d %d\n", i, j);
            c = *theSets[j];
        } else if (theOp < 60) {
            int theVerbose = Below(2);
            printf("new %d %d\n", i, theVerbose);
            delete theSets[i];
            theSets[i] = new ESChecksum(theVerbose != 0);
        } else if (theOp < 72) {
            printf("total %d = %u %d\n", i, (uint32_t)c.GetTotalChecksum(), c.GetIsVerboseMode() ? 1 : 0);
        } else if (theOp < 82) {
            printf("dump %d = %zu", i, c.mEntries.size());
            for (const ESChecksumEntry& e : c.mEntries) {
                printf(" ");
                Hex(e.GetDescription().s);
                printf(":%u", (uint32_t)e.GetChecksum());
            }
            printf("\n");
        } else {
            std::vector<UString> theErrors;
            bool theResult = c.Compare(*theSets[j], theErrors);
            printf("cmp %d %d = %d %zu", i, j, theResult ? 1 : 0, theErrors.size());
            for (const UString& e : theErrors) {
                printf(" ");
                Hex(e.s);
            }
            printf("\n");
        }
    }
    for (ESChecksum* c : theSets)
        delete c;
}

int main(int argc, char** argv)
{
    int theCount = argc > 1 ? atoi(argv[1]) : 100;
    int theFirst = argc > 2 ? atoi(argv[2]) : 1;
    for (int i = 0; i < theCount; i++) {
        sRandom = (uint32_t)(theFirst + i) * 2654435761u + 1;
        Next();
        Sequence(theFirst + i);
    }
    return 0;
}
