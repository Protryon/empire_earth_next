// GetMultiByteChar, shared by lle/UIControl.h and lle/User Interface.h (every caller sees the inline definition).
#ifndef LLE_UIMULTIBYTE_H
#define LLE_UIMULTIBYTE_H
#include "platform/PlatString.h"

// The character at inString as PlatIsMbcDigit/PlatIsMbcSpace take it: its code point (game text is UTF-8, decision
// D6; the original read a lead byte and its trail byte as one double-byte value). Header-inline (provisional name
// and home): the COMDAT sits after its first user, UIEditBox::AddChar (0x1006a9a5), and UITextLabel.cpp and
// User Interface.cpp call the same copy.
inline int GetMultiByteChar(const char* inString)
{
    return static_cast<int>(PlatUtf8Decode(inString, NULL));
}

#endif
