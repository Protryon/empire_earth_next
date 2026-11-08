// GUIString substrings
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
namespace WONAPI {
// FUNCTION: WON 0x006f8b90
// Returns a new string with count characters from offset
GUIString GUIString::substr(unsigned short offset,unsigned short count) const
{
    return GUIString(new GUIStringData(this->mData.get(),offset,count));
}
// FUNCTION: WON 0x00739c80
// Returns a new string with the characters from offset to the end
GUIString GUIString::substr(unsigned short offset) const
{
    return GUIString(new GUIStringData(this->mData.get(),offset,this->length()-offset));
}
}
