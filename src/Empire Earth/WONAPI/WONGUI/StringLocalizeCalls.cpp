// Lobby string formatting with one, two or three substitutions (file name inferred)
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
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONStringLocalize.h"
namespace WONAPI {
// The SDK's StringLocalize::GetStr overloads for one to three substitutions, as global functions
// Returns the format string with <s1> replaced by the given string
GUIString LobbyFormatString(const GUIString& format,const GUIString& first)
{
    const GUIString* subs[]={&first};
    return StringLocalize::GetStr(format,subs,1);
}
// Returns the format string with <s1> and <s2> replaced by the given strings
GUIString LobbyFormatString2(const GUIString& format,const GUIString& first,const GUIString& second)
{
    const GUIString* subs[]={&first,&second};
    return StringLocalize::GetStr(format,subs,2);
}
// Returns the format string with <s1>, <s2> and <s3> replaced by the given strings
GUIString LobbyFormatString3(const GUIString& format,const GUIString& first,const GUIString& second,const GUIString& third)
{
    const GUIString* subs[]={&first,&second,&third};
    return StringLocalize::GetStr(format,subs,3);
}
}
