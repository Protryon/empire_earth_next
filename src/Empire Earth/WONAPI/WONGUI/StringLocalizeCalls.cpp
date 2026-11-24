// Lobby string formatting with one, two or three substitutions (file name inferred). R6 WON round 34: the SDK's
// StringLocalize::GetStr (the <s1>..<s9> tags filled) is Rust's (rust/crates/ee_won_gui/src/localize.rs, called from
// StringLocalize.cpp).
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// StringLocalize.cpp's: the text with its tags filled from up to three substitutions (NULL ones dropped)
GUIString WONLocalize(const GUIString& inFormat, const GUIString* a, const GUIString* b, const GUIString* c, int n);
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
