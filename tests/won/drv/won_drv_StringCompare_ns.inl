// WONStringCompare.h's functions only the drivers use (R6 WON round 39; WStringLessNoCase had no user and went)
inline void StringToUpperCaseInPlace(std::string& ioText)
{
    if (!ioText.empty()) ee_won_case_bytes(reinterpret_cast<uint8_t*>(&ioText[0]), ioText.length(), true);
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
