// GUIStringData searches
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
#include "rust/ee_won.h"
// The searches are ee_won's guistring module (rust/crates/ee_won/src/guistring.rs)
namespace WONAPI {
// FUNCTION: WON 0x00728410
// Returns the last position of a character at or before start (-1 = from the end), or -1
int GUIStringData::rFind(int character,int start) const
{
    return ee_won_guistring_find_char(this->mText,this->mLength,character,start,true,sizeof(wchar_t));
}
}

namespace WONAPI {
// Returns the first position of any of theStr's characters at or after theStartPos, or -1 (not identified in the original exe)
int GUIStringData::findOneOf(const GUIStringData *theStr, int theStartPos) const
{
	return ee_won_guistring_op(EeWonGuiStringOp::FindOneOf,this->mText,this->mLength,theStr->mText,theStr->mLength,
	                           theStartPos,sizeof(wchar_t));
}

// Returns the last position of a substring at or before theStartPos, or -1 (not identified in the original exe)
int GUIStringData::rFind(const GUIStringData* theStr, int theStartPos) const
{
	return ee_won_guistring_op(EeWonGuiStringOp::RFind,this->mText,this->mLength,theStr->mText,theStr->mLength,
	                           theStartPos,sizeof(wchar_t));
}
}
