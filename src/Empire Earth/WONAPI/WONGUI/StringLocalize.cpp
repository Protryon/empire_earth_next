// Localized string substitution (file name inferred). R6 WON round 34: StringLocalize::GetStr (each <sN> tag replaced
// by substitution N, << a literal <) is Rust's (rust/crates/ee_won_gui/src/localize.rs) and the class is gone; these
// are the Rust's readers of a GUIString and its maker of the result, and the lobby's formatting functions over it.
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
#include <set>
#include "ee/wonapi/WONLobbyGUI.h"
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
// the lobby's formatting functions (StringLocalizeCalls.cpp until R6 WON GUI round 37)
// Returns the format string with <s1> replaced by the given string
GUIString LobbyFormatString(const GUIString& format,const GUIString& first) { return WONLocalize(format,&first,NULL,NULL,1); }
// Returns the format string with <s1> and <s2> replaced by the given strings
GUIString LobbyFormatString2(const GUIString& format,const GUIString& first,const GUIString& second) { return WONLocalize(format,&first,&second,NULL,2); }
// Returns the format string with <s1>, <s2> and <s3> replaced by the given strings
GUIString LobbyFormatString3(const GUIString& format,const GUIString& first,const GUIString& second,const GUIString& third)
{
    return WONLocalize(format,&first,&second,&third,3);
}
}
