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
#include "rust/won_gui/clipobj.h"

namespace WONAPI {

// R6 WON GUI round 35: both methods are clipobj.rs's (the conversions between the platform's UTF-8 text and the
// GUI's wide text); these are the platform's calls it makes
static const EeWonClipFns kClipFns = {PlatClipboardGetText, PlatClipboardSetText, free};

// Returns the clipboard's text; empty when it holds none
GUIString Clipboard::Get() { return GUIString(std::wstring((const wchar_t*)ee_won_clip_get(&kClipFns))); }

// Replaces the clipboard's contents with the string
void Clipboard::Set(const GUIString& theStr) { ee_won_clip_set(&kClipFns, theStr.mData->mText, theStr.length()); }

} // namespace WONAPI
