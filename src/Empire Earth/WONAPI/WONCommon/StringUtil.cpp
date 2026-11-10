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
#include "rust/ee_rust.h"
namespace WONAPI {
std::string StringToUpperCase(const std::string& theStr)
{
	std::string aNewStr;
	aNewStr.reserve(theStr.length());
	for(int i=0; i<theStr.length(); i++)
		aNewStr+=toupper(theStr[i]);

	return aNewStr;
}
// The original converted with the ANSI code page (MultiByteToWideChar/WideCharToMultiByte), or with the code page
// of the CRT locale's LC_CTYPE suffix (0x006a63e0/0x6a6570: none in the "C" locale, so the ANSI one again). The
// game's narrow text is UTF-8 on every system (decision D6) and WON's wide text UTF-16 units (UTF-32 code points
// where wchar_t is 4 bytes): this is where the two meet, and the conversions are ee_won's text module
// (rust/crates/ee_won/src/text.rs). A stray byte or a lone surrogate becomes U+FFFD. Embedded NULs are kept.
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
// FUNCTION: WON 0x006a6130
// Converts a string to wide, stopping at the first NUL
std::wstring StringToWString(const std::string& in)
{
    return StringUtilToWide(in.c_str(),strlen(in.c_str()));
}
// FUNCTION: WON 0x006a6280
// Converts a wide string to narrow, stopping at the first NUL
std::string WStringToString(const std::wstring& in)
{
    size_t length=0;
    while(length<in.length() && in[length]!=0) length++;
    return StringUtilToNarrow(in.c_str(),length);
}
// FUNCTION: WON 0x006a6410
// Converts a wide string to narrow (any code page: UTF-8), keeping embedded NULs
std::string WStringToString(const std::wstring& in,int codePage)
{
    return StringUtilToNarrow(in.data(),in.length());
}
// FUNCTION: WON 0x006a6590
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
// FUNCTION: WON 0x6a66f0
// Returns a lower-case copy of the string
std::string StringToLowerCase(const std::string& text)
{
    std::string result;
    result.reserve(text.length());
    for(std::string::size_type i=0;i<text.length();++i)
        result+=static_cast<char>(tolower(text[i]));
    return result;
}
// FUNCTION: WON 0x6a6850
// Returns an upper-case copy of the wide string
std::wstring WStringToUpperCase(const std::wstring& text)
{
    std::wstring result;
    result.reserve(text.length());
    for(std::wstring::size_type i=0;i<text.length();++i)
        result+=static_cast<wchar_t>(towupper(text[i]));
    return result;
}
// Both indexed accesses are non-const (each unshares the copy-on-write string)
// FUNCTION: WON 0x6a69a0
// Upper-cases the string in place
void StringToUpperCaseInPlace(std::string& text)
{
    for(std::string::size_type i=0;i<text.length();++i)
        text[i]=static_cast<char>(toupper(text[i]));
}
// The mismatch branch calls toupper again
// FUNCTION: WON 0x6a6ab0
// Compares two strings ignoring case; returns -1, 0 or 1, the shorter string first on a common prefix
int StringCompareNoCase(const std::string& first,const std::string& second)
{
    std::string::const_iterator a=first.begin();
    std::string::const_iterator b=second.begin();
    while(a!=first.end() && b!=second.end()) {
        if(toupper(*a)!=toupper(*b))
            return toupper(*a)<toupper(*b)?-1:1;
        ++a;
        ++b;
    }
    if(first.length()==second.length())
        return 0;
    return first.length()<second.length()?-1:1;
}
// towupper results are compared as 16-bit values
// FUNCTION: WON 0x6a6b60
// Compares two wide strings ignoring case; returns -1, 0 or 1, the shorter string first on a common prefix
int WStringCompareNoCase(const std::wstring& first,const std::wstring& second)
{
    std::wstring::const_iterator a=first.begin();
    std::wstring::const_iterator b=second.begin();
    while(a!=first.end() && b!=second.end()) {
        if(static_cast<wchar_t>(towupper(*a))!=static_cast<wchar_t>(towupper(*b)))
            return static_cast<wchar_t>(towupper(*a))<static_cast<wchar_t>(towupper(*b))?-1:1;
        ++a;
        ++b;
    }
    if(first.length()==second.length())
        return 0;
    return first.length()<second.length()?-1:1;
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
// FUNCTION: WON 0x006a6090
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
	std::wstring aNewStr;
	aNewStr.reserve(theStr.length());
	for(int i=0; i<theStr.length(); i++)
		aNewStr+=towlower(theStr[i]);

	return aNewStr;
}

void StringToLowerCaseInPlace(std::string &theStr)
{
	for(int i=0; i<theStr.length(); i++)
		theStr[i] = tolower(theStr[i]);
}

void WStringToUpperCaseInPlace(std::wstring &theStr)
{
	for(int i=0; i<theStr.length(); i++)
		theStr[i] = towupper(theStr[i]);
}

void WStringToLowerCaseInPlace(std::wstring &theStr)
{
	for(int i=0; i<theStr.length(); i++)
		theStr[i] = towlower(theStr[i]);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a6250
// (unsigned short * __cdecl WONAPI::StringUtilToWideBuffer(unsigned short *,char const *,int)    [?StringUtilToWideBuffer@WONAPI@@YAPAGPAGPBDH@Z])
// TEMPLATE: WON 0x006a63a0
// (char * __cdecl WONAPI::StringUtilToNarrowBuffer(char *,unsigned short const *,int)    [?StringUtilToNarrowBuffer@WONAPI@@YAPADPADPBGH@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a6040
// (_$E73    [_$E73])
// TEMPLATE: WON 0x006a60e0
// (_$E75    [_$E75])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a6570
// (int __cdecl WONAPI::StringUtilDefaultCodePage(void)    [?StringUtilDefaultCodePage@WONAPI@@YAHXZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006a5ff0
// (class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> > const & __cdecl WONAPI::GetEmptyStr(void)    [?GetEmptyStr@WON)
