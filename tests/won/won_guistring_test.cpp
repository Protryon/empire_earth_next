// WONGUI's GUIStringData (the lobby's text: every label, list item, chat line and input): the comparisons, searches,
// case maps, number read and narrowing over a table of strings, one line per case, compared with
// rust/crates/ee_won/src/golden_guistring.txt (recorded over the C++ before its port; the Rust test
// `guistring_golden` replays the C-locale pass). A second pass runs under setlocale(LC_ALL, "English"): the case maps
// and the space test are the C runtime's. `--print` prints the lines instead. The exit code is the number of failures.
#include <windows.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"

using namespace WONAPI;

wchar_t gEmptyWideString[2]; // EEWonNet.cpp's (GetEmptyGUIStringData)

static std::vector<std::string> sLines;

// Shared with guistring.rs's test
static const wchar_t* const kTexts[] = {
    L"", L"a", L"abc", L"ABC", L"abd", L"ab", L"abcabc", L"  -42x", L"12", L"-", L" 7 ", L"\x00e9t\x00e9",
    L"\x00c9T\x00c9", L"\x263a smile", L"a_b", L"a-b c", L"0", L"\t\v99", L"x\x00ffy", L"aBcAbC",
};
static const char* const kNarrow[] = {"", "a", "abc", "ABC", "ab0", "b", "\xe9t", "\xc9T", "abcd", "0"};
static const wchar_t kChars[] = {'a', 'b', 'c', ' ', 0x263a, 0x00e9, 0};

static std::string Units(const GUIString& inText)
{
    std::string anOut;
    char aBuffer[12];
    for (int i = 0; i < inText.length(); i++) {
        unsigned c = (unsigned)inText.at(i);
        if (c > 0x20 && c < 0x7f && c != '%') {
            anOut += (char)c;
        } else {
            snprintf(aBuffer, sizeof(aBuffer), "%%%x;", c);
            anOut += aBuffer;
        }
    }
    return anOut.empty() ? "-" : anOut;
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

static void Run(const char* inPass)
{
    const size_t n = sizeof(kTexts) / sizeof(kTexts[0]);
    for (size_t i = 0; i < n; i++) {
        GUIString a(kTexts[i]);
        char aCase[64];
        snprintf(aCase, sizeof(aCase), "%s t%02d", inPass, (int)i);
        GUIString anUpper = a;
        anUpper.toUpper();
        GUIString aLower = a;
        aLower.toLowerCase();
        std::string aNarrow = a;
        std::string aLine = std::string(aCase) + " " + Units(a) + " upper=" + Units(anUpper) + " lower=" +
                            Units(aLower) + " narrow=" + Hex(aNarrow) + " int=";
        for (int o = 0; o <= a.length(); o++) {
            char b[16];
            snprintf(b, sizeof(b), "%s%d", o ? "," : "", a.ToInt(o));
            aLine += b;
        }
        sLines.push_back(aLine);
        aLine = std::string(aCase) + " find";
        for (size_t c = 0; c < sizeof(kChars) / sizeof(kChars[0]); c++) {
            for (int s = 0; s <= a.length() + 1; s++) {
                char b[32];
                snprintf(b, sizeof(b), " %d", a.find(kChars[c], s));
                aLine += b;
            }
            for (int s = -1; s < a.length() + 1; s++) {
                char b[32];
                snprintf(b, sizeof(b), " r%d", a.rFind(kChars[c], s));
                aLine += b;
            }
            aLine += " |";
        }
        sLines.push_back(aLine);
        for (size_t j = 0; j < n; j++) {
            GUIString b(kTexts[j]);
            std::string l = std::string(aCase) + " u" + std::to_string((int)j) + " cmp=" +
                            std::to_string(a.mData->Compare(b.mData.get())) + " nocase=" +
                            std::to_string(a.CompareNoCase(b)) + " find";
            for (int s = 0; s <= a.length(); s++)
                l += " " + std::to_string(a.find(b, s)) + "/" + std::to_string(a.findNoCase(b, s)) + "/" +
                     std::to_string(a.findOneOf(b, s));
            l += " rfind";
            for (int s = -1; s <= a.length(); s++)
                l += " " + std::to_string(a.rFind(b, s));
            sLines.push_back(l);
        }
        for (size_t j = 0; j < sizeof(kNarrow) / sizeof(kNarrow[0]); j++) {
            std::string l = std::string(aCase) + " n" + std::to_string((int)j);
            for (int o = 0; o <= a.length() + 1; o++)
                l += " " + std::to_string(a.compare(kNarrow[j], o)) + "/" +
                     std::to_string(a.CompareNoCase(kNarrow[j], o));
            sLines.push_back(l);
        }
    }
}

static std::string GoldenPath()
{
    std::string aPath = __FILE__;
    size_t aSlash = aPath.find_last_of("/\\");
    aPath.resize(aSlash == std::string::npos ? 0 : aSlash + 1);
    return aPath + "../../rust/crates/ee_won/src/golden_guistring.txt";
}

int main(int argc, char** argv)
{
    Run("c");
    if (setlocale(LC_ALL, "English") != NULL)
        Run("english");
    else
        sLines.push_back("english: no such locale");
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
    static char aBuffer[65536];
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
        const char* anExpect = i < aWant.size() ? aWant[i].c_str() : "(none)";
        if (strcmp(aGot, anExpect) != 0) {
            if (aFailures++ < 20)
                printf("FAIL line %d\n  got  %s\n  want %s\n", (int)i + 1, aGot, anExpect);
        }
    }
    printf("won_guistring_test: %d lines, %d failures\n", (int)sLines.size(), aFailures);
    return aFailures;
}
