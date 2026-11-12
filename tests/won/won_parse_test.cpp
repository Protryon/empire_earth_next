// WON's StringParser (the lobby's and the config files' scanner): every read and format over a table of narrow and
// wide inputs, printed one line per case and compared with rust/crates/ee_won/src/golden_parse.txt (recorded over
// the C++ StringParser before its port; the Rust tests assert the same file). `--print` prints the lines instead.
// A second pass runs under setlocale(LC_ALL, "English") as the game's UI thread sets it: the scanner's ctype calls
// are the C runtime's, so the code page's letters and spaces count there. The exit code is the number of failures.
#include <windows.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyParser.h"

using namespace WONAPI;

static std::vector<std::string> sLines;

static void Line(const char* inFormat, ...)
{
    char aBuffer[2048];
    va_list anArgs;
    va_start(anArgs, inFormat);
    vsnprintf(aBuffer, sizeof(aBuffer), inFormat, anArgs);
    va_end(anArgs);
    sLines.push_back(aBuffer);
}

static std::string Hex(const std::string& inBytes)
{
    std::string anOut;
    char aBuffer[4];
    for (size_t i = 0; i < inBytes.size(); i++) {
        snprintf(aBuffer, sizeof(aBuffer), "%02x", (unsigned char)inBytes[i]);
        anOut += aBuffer;
    }
    return anOut.empty() ? "-" : anOut;
}

static std::string Hex(const std::wstring& inUnits)
{
    std::string anOut;
    char aBuffer[12];
    for (size_t i = 0; i < inUnits.size(); i++) {
        snprintf(aBuffer, sizeof(aBuffer), "%s%04x", i ? "." : "", (unsigned)inUnits[i]);
        anOut += aBuffer;
    }
    return anOut.empty() ? "-" : anOut;
}

// the inputs as UTF-16 units; the narrow form is each unit's low byte (the cases with units above 255 are wide only)
static const wchar_t* const kInputs[] = {
    L"",
    L"  hello world  ",
    L"\"quoted \\\"x\\\" \\t\\n\\r\\$\\\\ \\x41\\x4142\" rest",
    L"L\"wide\\x263a\\x41 z\" tail",
    L"\"a\" + \"b\"",
    L"\"a\"+ \"b\" , c",
    L"\"a\" \"b\"",
    L"abc, def)",
    L"abc  , def",
    L"  -123 ",
    L"0x1F",
    L"0XaB",
    L"0x",
    L"-",
    L"-0xff",
    L"2147483648",
    L"99999999999",
    L"0xfffffffff",
    L"12abc",
    L"true",
    L"Yes",
    L"off",
    L"N",
    L"maybe",
    L"1",
    L"\"unterminated",
    L"\"back\\q\"",
    L"\"end\\",
    L"\"\\x\"",
    L"\"\\xg\"",
    L"(1,2),(3)",
    L"(1,2",
    L"(1, x)",
    L"(\"a\", b), (c)",
    L"1, 2, x",
    L"1 ,2 , 3",
    L"a\tb\vc",
    L"\xe9t\xe9 \xfc",
    L"tru\xe9",
    L"\xa0 x",
    L"\x263a smile",
    L"L\"\x263a\" x",
    L"ab",
    L"abc",
    L"a",
    L"  ,x",
    L"L x",
    L"\"\"",
    L"\"a\"+",
    L"\xb2\xb3 1\xb9",
    L"0x\xb2",
    L"\"\\x4\xb2\"",
    L"\xe9X, \xff",
};

static bool IsWideOnly(const wchar_t* inText)
{
    for (; *inText; inText++)
        if ((unsigned)*inText > 255)
            return true;
    return false;
}

template <class T>
static void RunInt(const char* inName, StringParser& inParser, const char* inCase)
{
    T aValue = (T)7;
    bool aResult = inParser.ReadValue(aValue);
    Line("%s %s: r=%d pos=%d v=%lld", inCase, inName, aResult, inParser.GetPos(), (long long)aValue);
}

static std::string ListText(const std::list<std::string>& inList)
{
    std::string anOut;
    for (std::list<std::string>::const_iterator i = inList.begin(); i != inList.end(); ++i)
        anOut += (i == inList.begin() ? "" : "|") + Hex(*i);
    return anOut.empty() ? "-" : anOut;
}

static void RunCase(const wchar_t* inWide, const char* inNarrow, const char* inCase)
{
    struct Parser : StringParser {
        Parser(const wchar_t* w, const char* n) { if (n) this->Set(n); else this->Set(w); }
    };
    {
        Parser p(inWide, inNarrow);
        std::string s;
        bool r = p.ReadString(s, false);
        Line("%s str: r=%d pos=%d out=%s", inCase, r, p.GetPos(), Hex(s).c_str());
    }
    {
        Parser p(inWide, inNarrow);
        std::string s;
        bool r = p.ReadString(s, true);
        Line("%s str-ws: r=%d pos=%d out=%s", inCase, r, p.GetPos(), Hex(s).c_str());
    }
    {
        Parser p(inWide, inNarrow);
        std::wstring s;
        bool r = p.ReadString(s, false);
        Line("%s wstr: r=%d pos=%d out=%s", inCase, r, p.GetPos(), Hex(s).c_str());
    }
    {
        Parser p(inWide, inNarrow);
        std::wstring s;
        bool r = p.ReadString(s, true);
        Line("%s wstr-ws: r=%d pos=%d out=%s", inCase, r, p.GetPos(), Hex(s).c_str());
    }
    {
        Parser p(inWide, inNarrow);
        RunInt<int>("int", p, inCase);
    }
    {
        Parser p(inWide, inNarrow);
        RunInt<__int64>("int64", p, inCase);
    }
    {
        Parser p(inWide, inNarrow);
        RunInt<unsigned short>("ushort", p, inCase);
    }
    {
        Parser p(inWide, inNarrow);
        bool b = false;
        bool r = p.ReadValue(b);
        Line("%s bool: r=%d pos=%d v=%d", inCase, r, p.GetPos(), b);
    }
    {
        Parser p(inWide, inNarrow);
        bool r = p.CheckNextChar(',');
        Line("%s comma: r=%d pos=%d", inCase, r, p.GetPos());
    }
    {
        Parser p(inWide, inNarrow);
        p.SkipWhitespace();
        int aSkip = p.GetPos();
        p.SkipNonWhitespace();
        Line("%s skip: ws=%d word=%d end=%d", inCase, aSkip, p.GetPos(), p.EndOfString());
    }
    {
        Parser p(inWide, inNarrow);
        bool r1 = p.StartsWith("ab");
        int aPos = p.GetPos();
        Parser q(inWide, inNarrow);
        bool r2 = q.Is("ab");
        Line("%s starts: r=%d pos=%d is=%d pos=%d", inCase, r1, aPos, r2, q.GetPos());
    }
    {
        Parser p(inWide, inNarrow);
        std::list<int> l;
        bool r = p.ReadValue(l);
        std::string v;
        StringParser::ValueToString(l, v);
        Line("%s ints: r=%d pos=%d n=%d v=%s", inCase, r, p.GetPos(), (int)l.size(), Hex(v).c_str());
    }
    {
        Parser p(inWide, inNarrow);
        std::list<std::string> l;
        bool r = p.ReadValue(l);
        Line("%s strs: r=%d pos=%d v=%s", inCase, r, p.GetPos(), ListText(l).c_str());
    }
    {
        Parser p(inWide, inNarrow);
        std::list<std::wstring> l;
        bool r = p.ReadValue(l);
        std::string v;
        StringParser::ValueToString(l, v);
        Line("%s wstrs: r=%d pos=%d n=%d v=%s", inCase, r, p.GetPos(), (int)l.size(), Hex(v).c_str());
    }
    {
        Parser p(inWide, inNarrow);
        std::list<std::list<int> > l;
        bool r = p.ReadValue(l);
        std::string v;
        StringParser::ValueToString(l, v);
        Line("%s intlists: r=%d pos=%d n=%d v=%s", inCase, r, p.GetPos(), (int)l.size(), Hex(v).c_str());
    }
}

static void RunFormats()
{
    std::string s;
    StringParser::ValueToString(true, s);
    s += ";";
    StringParser::ValueToString(false, s);
    s += ";";
    int anInts[] = {0, -1, 2147483647, (int)0x80000000};
    for (int i = 0; i < 4; i++) {
        StringParser::ValueToString(anInts[i], s);
        s += ";";
    }
    unsigned short aShort = 65535;
    StringParser::ValueToString(aShort, s);
    s += ";";
    short aSigned = -32768;
    StringParser::ValueToString(aSigned, s);
    s += ";";
    char aChar = (char)0xe9;
    StringParser::ValueToString(aChar, s);
    s += ";";
    unsigned char aByte = 0xe9;
    StringParser::ValueToString(aByte, s);
    s += ";";
    long aLong = -5;
    StringParser::ValueToString(aLong, s);
    s += ";";
    unsigned long aULong = 4000000000u;
    StringParser::ValueToString(aULong, s);
    s += ";";
    __int64 aBig = -1234567890123LL;
    StringParser::ValueToString(aBig, s);
    s += ";";
    unsigned __int64 aUBig = 18446744073709551615ULL;
    StringParser::ValueToString(aUBig, s);
    s += ";";
    StringParser::ValueToString(std::wstring(L"w\xe9"), s);
    s += ";";
    std::list<std::string> aList;
    aList.push_back("x");
    aList.push_back("");
    aList.push_back("y");
    StringParser::ValueToString(aList, s);
    Line("formats: %s", Hex(s).c_str());
}

static void RunAll(const char* inPass)
{
    for (size_t i = 0; i < sizeof(kInputs) / sizeof(kInputs[0]); i++) {
        char aCase[32];
        snprintf(aCase, sizeof(aCase), "%s w%02d", inPass, (int)i);
        RunCase(kInputs[i], NULL, aCase);
        if (!IsWideOnly(kInputs[i])) {
            std::string aNarrow;
            for (const wchar_t* w = kInputs[i]; *w; w++)
                aNarrow += (char)(unsigned char)*w;
            snprintf(aCase, sizeof(aCase), "%s n%02d", inPass, (int)i);
            RunCase(NULL, aNarrow.c_str(), aCase);
        }
    }
}

static std::string GoldenPath()
{
    std::string aPath = __FILE__;
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == std::string::npos ? 0 : aSlash + 1);
    return aPath + "../../rust/crates/ee_won/src/golden_parse.txt";
}

int main(int argc, char** argv)
{
    RunAll("c");
    RunFormats();
    if (setlocale(LC_ALL, "English") != NULL)
        RunAll("english");
    else
        Line("english: no such locale");
    setlocale(LC_ALL, "C");
    if (argc > 1 && strcmp(argv[1], "--print") == 0) {
        for (size_t i = 0; i < sLines.size(); i++)
            printf("%s\n", sLines[i].c_str());
        return 0;
    }
    FILE* aFile = fopen(GoldenPath().c_str(), "rb");
    if (aFile == NULL) {
        printf("FAIL: cannot open %s\n", GoldenPath().c_str());
        return 1;
    }
    std::vector<std::string> aWant;
    char aBuffer[4096];
    while (fgets(aBuffer, sizeof(aBuffer), aFile)) {
        size_t n = strlen(aBuffer);
        while (n && (aBuffer[n - 1] == '\n' || aBuffer[n - 1] == '\r'))
            aBuffer[--n] = 0;
        aWant.push_back(aBuffer);
    }
    fclose(aFile);
    int aFailures = 0;
    size_t aCount = aWant.size() > sLines.size() ? aWant.size() : sLines.size();
    for (size_t i = 0; i < aCount; i++) {
        const char* aGot = i < sLines.size() ? sLines[i].c_str() : "(none)";
        const char* aExpect = i < aWant.size() ? aWant[i].c_str() : "(none)";
        if (strcmp(aGot, aExpect) != 0) {
            if (aFailures++ < 20)
                printf("FAIL line %d\n  got  %s\n  want %s\n", (int)i + 1, aGot, aExpect);
        }
    }
    printf("won_parse_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
