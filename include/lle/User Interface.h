// C:\EEX\Low-Level Engine\User Interface\User Interface.h (RECONSTRUCTION; name by analogy with lle/Input System.h).
// The UI subsystem's free functions: User Interface.cpp (0x10082871..0x10082c43) holds UIInitialize and the
// multi-line text helpers. Requires <vector>, lle/UWideString.h.
#ifndef LLE_USER_INTERFACE_H
#define LLE_USER_INTERFACE_H

#include <vector>

class GEFont;

LLE_API long UIInitialize();
LLE_API void UICreateMultilineTextFromString(const UWideString& inString, GEFont* inFont, unsigned long inWidth,
                                             std::vector<UWideString>& outLines);
LLE_API unsigned long UIGetStringCutOffIndex(const UWideString& inString, GEFont* inFont, unsigned long inWidth,
                                             bool inBreakAtSpace, bool inStopAtNewline);

// UIEditBox.cpp (0x1006a9a5; not exported): the multi-byte character at inString
#include "lle/UIMultiByte.h"

#endif
