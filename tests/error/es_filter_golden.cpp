// Golden driver for the Error System's debug-output filter (Filter.txt, ESFilterDebugOutput): random Filter.txt
// contents and messages over the original function (es_filter_orig.h). The Rust replay
// (rust/crates/ee_base/src/error/golden.rs) loads the same text and filters the same messages.
//
//   python -I build/lle1/cc.py build/lle2/esf.exe tests/error/es_filter_golden.cpp -Itests/error
//   build/lle2/esf.exe <sequences> [first seed] > golden.txt
//
// Lines (fields separated by one space; text as hex bytes, "-" for none):
//   seq <n> <startup path set 0/1> <file present 0/1> <file text as the filter reads it (text mode)>
//   msg <text> = <passes 0/1>
// The file is written in binary and read back in text mode (CR LF -> LF): the trace holds what text mode reads, the
// bytes the Rust side gets from C++.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

enum { kErrorOK = 0, kErrorNotFound = 3 };
struct UString {
    std::string s;
    UString() {}
    UString(const char* inText) : s(inText) {}
    UString(unsigned long) {}                     // the capacity constructor: an empty string
    bool operator!=(const UString& inOther) const { return s != inOther.s; }
    operator const char*() const { return s.c_str(); }
    unsigned long GetLength() const { return (unsigned long)s.size(); }
};
static const char* sPath = "es_filter_golden.tmp";
static bool sPresent;
struct FSFileSpec {
    FILE* mStream = NULL;
    void SetVolume(const UString&) {}
    void SetDirectory(const UString&) {}
    void SetFile(const UString&) {}
    long FOpen(const char* inMode)
    {
        if (!sPresent)
            return kErrorNotFound;
        this->mStream = fopen(sPath, inMode);
        return this->mStream ? kErrorOK : kErrorNotFound;
    }
    operator FILE*() const { return this->mStream; }
    ~FSFileSpec()
    {
        if (this->mStream)
            fclose(this->mStream);
    }
};
static UString gStartupVolume, gStartupDirectory;

#include "es_filter_orig.h"

static uint32_t sRandom;
static uint32_t Next()
{
    sRandom ^= sRandom << 13;
    sRandom ^= sRandom >> 17;
    sRandom ^= sRandom << 5;
    return sRandom;
}
static uint32_t Below(uint32_t n) { return Next() % n; }

static const char* kWords[] = {"-", "+", "OutputToDebugger", ";", "net", "ai", "Unit", "!", "x", "", "error",
                               "net ai", "-x", "+ai", " ", "Unit 4"};

static std::string Word()
{
    switch (Below(8)) {
    case 0: {
        std::string s;
        for (uint32_t i = Below(6); i > 0; i--)
            s += (char)(Below(5) == 0 ? 1 + Below(255) : "aeinrtuxUN !;+-"[Below(15)]);
        return s;
    }
    case 1: return std::string(1000 + Below(100), 'a' + Below(3));   // past fgets' 1023 bytes
    default: return kWords[Below(16)];
    }
}

static void Hex(const std::string& s)
{
    if (s.empty())
        printf("-");
    for (unsigned char c : s)
        printf("%02x", c);
}

static void Sequence(int inIndex)
{
    for (UString* f : gFilters)
        delete f;
    gFilters.clear();
    gFilterMode = 1;
    gOutputToDebugger = false;
    gErrorSystemFiltersInitialized = false;
    bool thePath = Below(10) != 0;
    gStartupVolume = thePath ? "C:" : "";
    gStartupDirectory = thePath ? "\\Game\\" : "";
    sPresent = Below(8) != 0;
    std::string theText;
    for (uint32_t i = Below(8); i > 0; i--) {
        theText += Word();
        static const char* kEnds[] = {"\n", "\r\n", "\n\n", "", "\r", "\n"};
        theText += kEnds[Below(6)];
    }
    if (Below(10) == 0)
        theText += std::string("ai\0net\n", 7);
    FILE* f = fopen(sPath, "wb");
    fwrite(theText.data(), 1, theText.size(), f);
    fclose(f);
    std::string theRead;   // what text mode reads
    f = fopen(sPath, "r");
    for (int c; (c = fgetc(f)) != EOF;)
        theRead += (char)c;
    fclose(f);
    printf("seq %d %d %d ", inIndex, thePath ? 1 : 0, sPresent ? 1 : 0);
    Hex(theRead);
    printf("\n");
    for (uint32_t i = 3 + Below(12); i > 0; i--) {
        std::string theMessage;
        for (uint32_t j = Below(4); j > 0; j--)
            theMessage += (Below(2) ? " " : "") + Word();
        size_t theNul = theMessage.find('\0');
        if (theNul != std::string::npos)
            theMessage.resize(theNul);
        if (theMessage.size() > 1000)
            theMessage.resize(1000);
        bool thePasses = ESFilterDebugOutput(theMessage.c_str());
        printf("msg ");
        Hex(theMessage);
        printf(" = %d\n", thePasses ? 1 : 0);
    }
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
    remove(sPath);
    return 0;
}
