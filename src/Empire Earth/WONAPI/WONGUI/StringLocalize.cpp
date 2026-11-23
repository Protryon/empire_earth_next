// Localized string substitution (file name inferred). R6 WON round 34: StringLocalize::GetStr (each <sN> tag replaced
// by substitution N, << a literal <) is Rust's (rust/crates/ee_won_gui/src/localize.rs) and the class is gone; these
// are the Rust's readers of a GUIString and its maker of the result, and the call the lobby's formatting functions
// (StringLocalizeCalls.cpp) make.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "rust/won_gui/localize.h"
namespace WONAPI {
static void LocalizeUnits(const void* inText, EeWonLocalizeText* outText)
{
    *outText = EeWonLocalizeText{static_cast<const GUIString*>(inText)->mData->mText, static_cast<const GUIString*>(inText)->length()};
}
static void* LocalizeNew(const void* inUnits, size_t inLength) { return new GUIStringData(std::wstring((const wchar_t*)inUnits, inLength)); }
GUIString WONLocalize(const GUIString& inFormat, const GUIString* a, const GUIString* b, const GUIString* c, int n)
{
    return GUIString((GUIStringData*)ee_won_gui_localize(&inFormat, a, b, c, n, LocalizeUnits, LocalizeNew, sizeof(wchar_t)));
}
}
