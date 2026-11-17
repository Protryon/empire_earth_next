// C:\EEX\Empire Earth\WONAPI\WONCommon\StringUtil.cpp
// Narrow/wide string conversion and case helpers (file grouping inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include <stdlib.h>
#include <string.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "platform/PlatString.h"
#include "rust/won_net/ffi.h"
#include "rust/won_net/strcase.h"
#include <wchar.h>
namespace WONAPI {
// The case maps and compares are ee_won's strcase module (rust/crates/ee_won_net/src/strcase.rs): under D6 game text is
// UTF-8 and the C runtime's character classes the "C" locale's, so only ASCII letters change case, as the original's
// toupper / tolower / towupper / towlower did in the game.
static void StringUtilCaseInPlace(std::string& ioText, bool inUpper)
{
    if (!ioText.empty())
        ee_won_case_bytes(reinterpret_cast<uint8_t*>(&ioText[0]), ioText.length(), inUpper);
}
static void StringUtilCaseInPlace(std::wstring& ioText, bool inUpper)
{
    if (ioText.empty())
        return;
#if WCHAR_MAX > 0xffff
    ee_won_case_units32(reinterpret_cast<uint32_t*>(&ioText[0]), ioText.length(), inUpper);
#else
    ee_won_case_units16(reinterpret_cast<uint16_t*>(&ioText[0]), ioText.length(), inUpper);
#endif
}
std::string StringToUpperCase(const std::string& theStr)
{
    std::string aNewStr(theStr);
    StringUtilCaseInPlace(aNewStr, true);
    return aNewStr;
}
// The original converted with the ANSI code page (MultiByteToWideChar/WideCharToMultiByte), or with the code page
// of the CRT locale's LC_CTYPE suffix (0x006a63e0/0x6a6570: none in the "C" locale, so the ANSI one again). The
// game's narrow text is UTF-8 on every system (decision D6) and WON's wide text UTF-16 units (UTF-32 code points
// where wchar_t is 4 bytes): this is where the two meet, and the conversions are ee_won's text module
// (rust/crates/ee_won_net/src/text.rs). A stray byte or a lone surrogate becomes U+FFFD. Embedded NULs are kept.
static void StringUtilWideSink(void* inContext, const void* inText, size_t inCount)
{
    std::wstring* theOut = static_cast<std::wstring*>(inContext);
    theOut->assign(static_cast<const wchar_t*>(inText), inCount);
}
static void StringUtilByteSink(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    std::string* theOut = static_cast<std::string*>(inContext);
    theOut->assign(reinterpret_cast<const char*>(inBytes), inLength);
}
static std::wstring StringUtilToWide(const char* in, size_t count)
{
    std::wstring result;
    if(in!=NULL && count!=0)
        ee_won_wide_from_utf8(reinterpret_cast<const uint8_t*>(in),count,sizeof(wchar_t),StringUtilWideSink,&result);
    return result;
}
static std::string StringUtilToNarrow(const wchar_t* in, size_t count)
{
    std::string result;
    if(in!=NULL && count!=0)
        ee_won_utf8_from_wide(in,sizeof(wchar_t),count,StringUtilByteSink,&result);
    return result;
}
// Converts a string to wide, stopping at the first NUL
std::wstring StringToWString(const std::string& in)
{
    return StringUtilToWide(in.c_str(),strlen(in.c_str()));
}
// Converts a wide string to narrow, stopping at the first NUL
std::string WStringToString(const std::wstring& in)
{
    size_t length=0;
    while(length<in.length() && in[length]!=0) length++;
    return StringUtilToNarrow(in.c_str(),length);
}
// Converts a wide string to narrow (any code page: UTF-8), keeping embedded NULs
std::string WStringToString(const std::wstring& in,int codePage)
{
    return StringUtilToNarrow(in.data(),in.length());
}
// Converts a string to wide (any code page: UTF-8), keeping embedded NULs
std::wstring StringToWString(const std::string& in,int codePage)
{
    return StringUtilToWide(in.data(),in.length());
}
}

#include <ctype.h>
#include <wctype.h>
namespace WONAPI {
// tolower gets the signed char; the loop runs over the counted length, not to a NUL
// Returns a lower-case copy of the string
std::string StringToLowerCase(const std::string& text)
{
    std::string result(text);
    StringUtilCaseInPlace(result, false);
    return result;
}
// Returns an upper-case copy of the wide string
std::wstring WStringToUpperCase(const std::wstring& text)
{
    std::wstring result(text);
    StringUtilCaseInPlace(result, true);
    return result;
}
// Both indexed accesses are non-const (each unshares the copy-on-write string)
// Upper-cases the string in place
void StringToUpperCaseInPlace(std::string& text)
{
    StringUtilCaseInPlace(text, true);
}
// The mismatch branch calls toupper again
// Compares two strings ignoring case; returns -1, 0 or 1, the shorter string first on a common prefix
int StringCompareNoCase(const std::string& first,const std::string& second)
{
    return ee_won_compare_no_case(reinterpret_cast<const uint8_t*>(first.data()), first.length(),
                                  reinterpret_cast<const uint8_t*>(second.data()), second.length());
}
// towupper results are compared as 16-bit values
// Compares two wide strings ignoring case; returns -1, 0 or 1, the shorter string first on a common prefix
int WStringCompareNoCase(const std::wstring& first,const std::wstring& second)
{
#if WCHAR_MAX > 0xffff
    return ee_won_compare_no_case32(reinterpret_cast<const int32_t*>(first.data()), first.length(),
                                    reinterpret_cast<const int32_t*>(second.data()), second.length());
#else
    return ee_won_compare_no_case16(reinterpret_cast<const uint16_t*>(first.data()), first.length(),
                                    reinterpret_cast<const uint16_t*>(second.data()), second.length());
#endif
}
}

namespace WONAPI {
// WON SDK GetEmptyStr (emitted from the list below)
const std::string& GetEmptyStr()
{
    static std::string anEmptyStr;
    return anEmptyStr;
}
}

namespace WONAPI {
// Returns a shared empty wide string
const std::wstring& GetEmptyWStr()
{
	static std::wstring anEmptyWStr;
	return anEmptyWStr;
}
}

namespace WONAPI {
// No original address identified: WON SDK case conversion loops

std::wstring WStringToLowerCase(const std::wstring& theStr)
{
    std::wstring aNewStr(theStr);
    StringUtilCaseInPlace(aNewStr, false);
    return aNewStr;
}

void StringToLowerCaseInPlace(std::string &theStr)
{
    StringUtilCaseInPlace(theStr, false);
}

void WStringToUpperCaseInPlace(std::wstring &theStr)
{
    StringUtilCaseInPlace(theStr, true);
}

void WStringToLowerCaseInPlace(std::wstring &theStr)
{
    StringUtilCaseInPlace(theStr, false);
}
}
