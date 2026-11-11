// StringUtil's case functions (WONCommon/StringUtil.cpp): the original's loops (copied from the C++ before the port,
// run with the C runtime in the "C" LC_CTYPE, as the game sets it: decision D6) against the port's functions, which
// are ee_won's strcase module, over every byte and every 16-bit unit (and, where wchar_t is 32 bits, a sample past
// them), and over every pair of short texts drawn from a mixed alphabet (ASCII letters of both cases, digits, a
// space, UTF-8 sequences, Latin-1 and astral units). Links StringUtil.cpp. Prints one line per failed check; the exit
// code is the number of failed checks.
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <wctype.h>
#include <string>
#include <vector>
#include "rust/ee_rust.h"

namespace WONAPI {
std::string StringToUpperCase(const std::string& theStr);
std::string StringToLowerCase(const std::string& text);
std::wstring WStringToUpperCase(const std::wstring& text);
std::wstring WStringToLowerCase(const std::wstring& theStr);
void StringToUpperCaseInPlace(std::string& text);
void StringToLowerCaseInPlace(std::string& theStr);
void WStringToUpperCaseInPlace(std::wstring& theStr);
void WStringToLowerCaseInPlace(std::wstring& theStr);
int StringCompareNoCase(const std::string& first, const std::string& second);
int WStringCompareNoCase(const std::wstring& first, const std::wstring& second);
}

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

// ---- the original's statements

static std::string OrigUpper(const std::string& theStr)
{
    std::string aNewStr;
    aNewStr.reserve(theStr.length());
    for (int i = 0; i < (int)theStr.length(); i++)
        aNewStr += toupper(theStr[i]);
    return aNewStr;
}
static std::string OrigLower(const std::string& text)
{
    std::string result;
    result.reserve(text.length());
    for (std::string::size_type i = 0; i < text.length(); ++i)
        result += static_cast<char>(tolower(text[i]));
    return result;
}
static std::wstring OrigWUpper(const std::wstring& text)
{
    std::wstring result;
    result.reserve(text.length());
    for (std::wstring::size_type i = 0; i < text.length(); ++i)
        result += static_cast<wchar_t>(towupper(text[i]));
    return result;
}
static std::wstring OrigWLower(const std::wstring& theStr)
{
    std::wstring aNewStr;
    aNewStr.reserve(theStr.length());
    for (int i = 0; i < (int)theStr.length(); i++)
        aNewStr += towlower(theStr[i]);
    return aNewStr;
}
static int OrigCompare(const std::string& first, const std::string& second)
{
    std::string::const_iterator a = first.begin();
    std::string::const_iterator b = second.begin();
    while (a != first.end() && b != second.end()) {
        if (toupper(*a) != toupper(*b))
            return toupper(*a) < toupper(*b) ? -1 : 1;
        ++a;
        ++b;
    }
    if (first.length() == second.length())
        return 0;
    return first.length() < second.length() ? -1 : 1;
}
static int OrigWCompare(const std::wstring& first, const std::wstring& second)
{
    std::wstring::const_iterator a = first.begin();
    std::wstring::const_iterator b = second.begin();
    while (a != first.end() && b != second.end()) {
        if (static_cast<wchar_t>(towupper(*a)) != static_cast<wchar_t>(towupper(*b)))
            return static_cast<wchar_t>(towupper(*a)) < static_cast<wchar_t>(towupper(*b)) ? -1 : 1;
        ++a;
        ++b;
    }
    if (first.length() == second.length())
        return 0;
    return first.length() < second.length() ? -1 : 1;
}

static void CheckNarrow(const std::string& s)
{
    CHECK(WONAPI::StringToUpperCase(s) == OrigUpper(s));
    CHECK(WONAPI::StringToLowerCase(s) == OrigLower(s));
    std::string a = s, b = s;
    WONAPI::StringToUpperCaseInPlace(a);
    CHECK(a == OrigUpper(s));
    WONAPI::StringToLowerCaseInPlace(b);
    CHECK(b == OrigLower(s));
}

static void CheckWide(const std::wstring& s)
{
    CHECK(WONAPI::WStringToUpperCase(s) == OrigWUpper(s));
    CHECK(WONAPI::WStringToLowerCase(s) == OrigWLower(s));
    std::wstring a = s, b = s;
    WONAPI::WStringToUpperCaseInPlace(a);
    CHECK(a == OrigWUpper(s));
    WONAPI::WStringToLowerCaseInPlace(b);
    CHECK(b == OrigWLower(s));
}

int main()
{
    setlocale(LC_CTYPE, "C");
    for (int c = 0; c < 256; c++) CheckNarrow(std::string(1, (char)c));
    for (int c = 0; c < 0x10000; c++) CheckWide(std::wstring(1, (wchar_t)c));
    if (sizeof(wchar_t) == 4)
        for (unsigned c = 0x10000; c < 0x110000; c += 97) CheckWide(std::wstring(1, (wchar_t)c));
    const char* aNarrow[] = { "a", "A", "z", "Z", "0", " ", "\xc3\xa9", "\xc3\x89", "\xe2\x98\xba", "\x80", "\xff", "_",
                              "[", "`", "{" };
    size_t aNarrowCount = sizeof(aNarrow) / sizeof(aNarrow[0]);
    std::vector<std::string> aTexts(1, std::string());
    for (size_t i = 0; i < aNarrowCount; i++) aTexts.push_back(aNarrow[i]);
    for (size_t i = 0; i < aNarrowCount; i++)
        for (size_t j = 0; j < aNarrowCount; j++) aTexts.push_back(std::string(aNarrow[i]) + aNarrow[j]);
    for (size_t i = 0; i < aTexts.size(); i++) {
        CheckNarrow(aTexts[i]);
        for (size_t j = 0; j < aTexts.size(); j++)
            CHECK(WONAPI::StringCompareNoCase(aTexts[i], aTexts[j]) == OrigCompare(aTexts[i], aTexts[j]));
    }
    std::vector<wchar_t> aUnits;
    const unsigned aWide[] = { 0x61, 0x41, 0x7a, 0x5a, 0x30, 0x20, 0xe9, 0xc9, 0xff, 0x178, 0x263a, 0xd83d, 0xde00,
                               0xffff, 0x5b, 0x60, 0x7b };
    std::vector<std::wstring> aWTexts(1, std::wstring());
    size_t aWideCount = sizeof(aWide) / sizeof(aWide[0]);
    for (size_t i = 0; i < aWideCount; i++) aWTexts.push_back(std::wstring(1, (wchar_t)aWide[i]));
    for (size_t i = 0; i < aWideCount; i++)
        for (size_t j = 0; j < aWideCount; j++) {
            std::wstring w(1, (wchar_t)aWide[i]);
            w += (wchar_t)aWide[j];
            aWTexts.push_back(w);
        }
    if (sizeof(wchar_t) == 4) aWTexts.push_back(std::wstring(1, (wchar_t)0x1f600));
    for (size_t i = 0; i < aWTexts.size(); i++) {
        CheckWide(aWTexts[i]);
        for (size_t j = 0; j < aWTexts.size(); j++)
            CHECK(WONAPI::WStringCompareNoCase(aWTexts[i], aWTexts[j]) == OrigWCompare(aWTexts[i], aWTexts[j]));
    }
    printf("won_strcase_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
