// C:\EEX\Empire Earth\WONAPI\WONCommon\StringUtil.cpp
// Narrow/wide string conversion and case helpers (file grouping inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyConfig.h"
namespace WONAPI {
// RVA 565ef0: cached default code page, 0 until first computed
static int gStringUtilDefaultCodePage=0;
std::string StringToUpperCase(const std::string& theStr)
{
	std::string aNewStr;
	aNewStr.reserve(theStr.length());
	for(int i=0; i<theStr.length(); i++)
		aNewStr+=toupper(theStr[i]);

	return aNewStr;
}
// FUNCTION: WON 0x006a63e0
// Returns the code page named by the CRT LC_CTYPE locale's suffix (not GetACP), 0 when there is none
static int StringUtilLocaleCodePage()
{
    const char* locale=setlocale(LC_CTYPE,NULL);
    if(locale==NULL) return 0;
    const char* dot=strchr(locale,'.');
    if(dot==NULL) return 0;
    return atoi(dot+1);
}
// 0x6a6570: caches the locale code page; a 0 result is retried on the next call
static int StringUtilDefaultCodePage()
{
    if(gStringUtilDefaultCodePage==0) gStringUtilDefaultCodePage=StringUtilLocaleCodePage();
    return gStringUtilDefaultCodePage;
}
// 0x6a6250: converts at most count characters with the ANSI code page into a fixed buffer
static wchar_t* StringUtilToWideBuffer(wchar_t* out,const char* in,int count)
{
    if(out==NULL) return NULL;
    *out=0;
    if(in!=NULL) MultiByteToWideChar(0,0,in,count,out,count);
    return out;
}
// 0x6a63a0: converts at most count characters to the ANSI code page into a fixed buffer
static char* StringUtilToNarrowBuffer(char* out,const wchar_t* in,int count)
{
    if(out==NULL) return NULL;
    *out=0;
    if(in!=NULL) WideCharToMultiByte(0,0,in,count,out,count,NULL,NULL);
    return out;
}
// FUNCTION: WON 0x006a6130
// Converts a string to wide with the ANSI code page, stopping at the first NUL
// The raw buffer is not guarded while the result string is built
std::wstring StringToWString(const std::string& in)
{
    int length=in.length();
    if(length==0) return std::wstring();
    wchar_t* buffer=new wchar_t[length+1];
    StringUtilToWideBuffer(buffer,in.c_str(),length);
    buffer[length]=0;
    std::wstring result(buffer);
    delete [] buffer;
    return result;
}
// FUNCTION: WON 0x006a6280
// Converts a wide string to narrow with the ANSI code page into a buffer of one byte per character
// The fixed-size conversion can fail or stop part way; the original ignores the result
std::string WStringToString(const std::wstring& in)
{
    unsigned int length=in.length();
    if(length==0) return std::string();
    char* buffer=new char[length+1];
    StringUtilToNarrowBuffer(buffer,in.c_str(),length);
    buffer[length]=0;
    std::string result(buffer);
    delete [] buffer;
    return result;
}
// FUNCTION: WON 0x006a6410
// Converts a wide string to narrow with the given code page (negative: the locale's), keeping embedded NULs
std::string WStringToString(const std::wstring& in,int codePage)
{
    if(codePage<0) codePage=StringUtilDefaultCodePage();
    int count=WideCharToMultiByte(codePage,0,in.c_str(),in.length(),NULL,0,NULL,NULL);
    std::string result;
    result.reserve(count+1);
    result.resize(count);
    WideCharToMultiByte(codePage,0,in.c_str(),in.length(),const_cast<char*>(result.data()),result.length(),NULL,NULL);
    return result;
}
// FUNCTION: WON 0x006a6590
// Converts a string to wide with the given code page (negative: the locale's), keeping embedded NULs
// Both API calls happen even when the output length is 0
std::wstring StringToWString(const std::string& in,int codePage)
{
    if(codePage<0) codePage=StringUtilDefaultCodePage();
    int count=MultiByteToWideChar(codePage,0,in.c_str(),in.length(),NULL,0);
    std::wstring result;
    result.reserve(count+1);
    result.resize(count);
    MultiByteToWideChar(codePage,0,in.c_str(),in.length(),const_cast<wchar_t*>(result.data()),result.length());
    return result;
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
