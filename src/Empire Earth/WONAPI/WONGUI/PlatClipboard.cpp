// The WONAPI GUI's clipboard on the platform layer (Clipboard.cpp's GDI-era version used the Win32 clipboard):
// Windows-1252 text through PlatClipboardGetText/SetText (other characters become '?').
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
    uint32_t aCount = PlatCp1252ToWide(aText, PLAT_TEXT_TERMINATED, NULL, 0);
    std::wstring aWide(aCount, L'\0');
    if (aCount != 0)
        PlatCp1252ToWide(aText, PLAT_TEXT_TERMINATED, &aWide[0], aCount);
    free(aText);
    aStr = GUIString(aWide);
    return aStr;
}

// Replaces the clipboard's contents with the string
void Clipboard::Set(const GUIString& theStr)
{
    std::wstring aWide = theStr;
    std::string aText;
    uint32_t aCount = PlatWideToCp1252(aWide.c_str(), (uint32_t)aWide.size(), NULL, 0);
    aText.resize(aCount);
    if (aCount != 0)
        PlatWideToCp1252(aWide.c_str(), (uint32_t)aWide.size(), &aText[0], aCount);
    PlatClipboardSetText(aText.c_str());
}

} // namespace WONAPI
