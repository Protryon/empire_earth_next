// The WONAPI GUI's clipboard on the platform layer (Clipboard.cpp's GDI-era version used the Win32 clipboard):
// UTF-8 text through PlatClipboardGetText/SetText (decision D6).
#include <windows.h>
#include <stdlib.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "platform/PlatString.h"
#include "platform/PlatWindow.h"

namespace WONAPI {

// Returns the clipboard's text; empty when it holds none
GUIString Clipboard::Get()
{
    GUIString aStr;
    char* aText = PlatClipboardGetText();
    if (aText == NULL)
        return aStr;
    uint32_t aCount = PlatUtf8ToWide(aText, PLAT_TEXT_TERMINATED, NULL, 0);
    std::wstring aWide(aCount, L'\0');
    if (aCount != 0)
        PlatUtf8ToWide(aText, PLAT_TEXT_TERMINATED, &aWide[0], aCount);
    free(aText);
    aStr = GUIString(aWide);
    return aStr;
}

// Replaces the clipboard's contents with the string
void Clipboard::Set(const GUIString& theStr)
{
    std::wstring aWide = theStr;
    std::string aText;
    uint32_t aCount = PlatWideToUtf8(aWide.c_str(), (uint32_t)aWide.size(), NULL, 0);
    aText.resize(aCount);
    if (aCount != 0)
        PlatWideToUtf8(aWide.c_str(), (uint32_t)aWide.size(), &aText[0], aCount);
    PlatClipboardSetText(aText.c_str());
}

} // namespace WONAPI
