// the functions only the drivers use, cut from ee/wonapi/WONBuffer.h (R6 WON round 38): included in its
// class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
// the number of UTF-16 units (the wire's character count) of a wide string
inline unsigned long WideUtf16Length(const std::wstring& inText)
{
return static_cast<unsigned long>(ee_won_utf16le_from_wide(inText.data(),sizeof(wchar_t),inText.length(),NULL,0)/2);
}
