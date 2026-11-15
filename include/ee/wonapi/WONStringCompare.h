#ifndef EE_WONAPI_WONSTRINGCOMPARE_H
#define EE_WONAPI_WONSTRINGCOMPARE_H
#include <string>
namespace WONAPI {
int WStringCompareNoCase(const std::wstring& inLeft, const std::wstring& inRight); // 0x2a6b60
struct WStringLessNoCase {
    bool operator()(const std::wstring& inLeft, const std::wstring& inRight) const
    { return WStringCompareNoCase(inLeft, inRight) < 0; }
};
// Original comparator helper 0x2a6ab0: case-insensitive comparison of std::string values.
int StringCompareNoCase(const std::string& inLeft, const std::string& inRight);
struct StringLessNoCase {
    bool operator()(const std::string& inLeft, const std::string& inRight) const
    { return StringCompareNoCase(inLeft, inRight) < 0; }
};

}
#endif
