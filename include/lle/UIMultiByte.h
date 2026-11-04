// GetMultiByteChar, shared by lle/UIControl.h and lle/User Interface.h (every caller sees the inline definition).
// Requires <mbstring.h>.
#ifndef LLE_UIMULTIBYTE_H
#define LLE_UIMULTIBYTE_H

// The (possibly double-byte) character at inString, as _ismbcdigit/_ismbcspace take it. Header-inline (provisional
// name and home): the COMDAT sits after its first user, UIEditBox::AddChar (0x1006a9a5), and UITextLabel.cpp
// and User Interface.cpp call the same copy.
inline int GetMultiByteChar(const char* inString)
{
    int theChar = 0;
    if (_ismbblead(*reinterpret_cast<const unsigned char*>(inString))) {
        theChar = *inString << 8;
        inString++;
    }
    return *inString + theChar;
}

#endif
