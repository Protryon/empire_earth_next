#ifndef EE_WONAPI_WONSTRINGCOMPARE_H
#define EE_WONAPI_WONSTRINGCOMPARE_H
// WONAPI's string utilities (StringUtil.cpp in the original), each an inline forward to ee_won_net's strcase.rs and
// text.rs: under D6 game text is UTF-8 and the C runtime's character classes the "C" locale's, so only ASCII
// letters change case; narrow text is UTF-8 and wide text UTF-16 units (UTF-32 code points where wchar_t is 4
// bytes). A stray byte or a lone surrogate becomes U+FFFD; the counted conversions keep embedded NULs.
#include <string>
#include <string.h>
#include <wchar.h>
#include "rust/won_net/ffi.h"
#include "rust/won_net/strcase.h"
namespace WONAPI {
inline void StringToUpperCaseInPlace(std::string& ioText)
{
    if (!ioText.empty()) ee_won_case_bytes(reinterpret_cast<uint8_t*>(&ioText[0]), ioText.length(), true);
}
// 0x2a6ab0: -1, 0 or 1 ignoring case, the shorter string first on a common prefix
inline int StringCompareNoCase(const std::string& inLeft, const std::string& inRight)
{
    return ee_won_compare_no_case(reinterpret_cast<const uint8_t*>(inLeft.data()), inLeft.length(),
                                  reinterpret_cast<const uint8_t*>(inRight.data()), inRight.length());
}
inline int WStringCompareNoCase(const std::wstring& inLeft, const std::wstring& inRight)
{
#if WCHAR_MAX > 0xffff
    return ee_won_compare_no_case32(reinterpret_cast<const int32_t*>(inLeft.data()), inLeft.length(),
                                    reinterpret_cast<const int32_t*>(inRight.data()), inRight.length());
#else
    return ee_won_compare_no_case16(reinterpret_cast<const uint16_t*>(inLeft.data()), inLeft.length(),
                                    reinterpret_cast<const uint16_t*>(inRight.data()), inRight.length());
#endif
}
struct WStringLessNoCase {
    bool operator()(const std::wstring& inLeft, const std::wstring& inRight) const
    { return WStringCompareNoCase(inLeft, inRight) < 0; }
};
struct StringLessNoCase {
    bool operator()(const std::string& inLeft, const std::string& inRight) const
    { return StringCompareNoCase(inLeft, inRight) < 0; }
};
inline void StringUtilWideSink(void* ioText, const void* inText, size_t inCount)
{
    static_cast<std::wstring*>(ioText)->assign(static_cast<const wchar_t*>(inText), inCount);
}
inline void StringUtilByteSink(void* ioText, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(ioText)->assign(reinterpret_cast<const char*>(inBytes), inLength);
}
// 0x2a6590 (with a code page: any is UTF-8 now): the counted text widened, embedded NULs kept
inline std::wstring StringToWString(const std::string& inText, int)
{
    std::wstring aResult;
    if (!inText.empty())
        ee_won_wide_from_utf8(reinterpret_cast<const uint8_t*>(inText.data()), inText.length(), sizeof(wchar_t),
                              StringUtilWideSink, &aResult);
    return aResult;
}
// the text to its first NUL widened
inline std::wstring StringToWString(const std::string& inText)
{
    return StringToWString(std::string(inText.c_str(), strlen(inText.c_str())), -1);
}
// 0x2a6410 (with a code page: any is UTF-8 now): the counted text narrowed, embedded NULs kept
inline std::string WStringToString(const std::wstring& inText, int)
{
    std::string aResult;
    if (!inText.empty())
        ee_won_utf8_from_wide(inText.data(), sizeof(wchar_t), inText.length(), StringUtilByteSink, &aResult);
    return aResult;
}
// 0x2a6280: the text to its first NUL narrowed
inline std::string WStringToString(const std::wstring& inText)
{
    return WStringToString(std::wstring(inText.c_str(), wcslen(inText.c_str())), -1);
}
}
#endif
