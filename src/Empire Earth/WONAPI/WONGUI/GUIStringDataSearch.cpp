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
namespace WONAPI {
// FUNCTION: WON 0x00728410
// Returns the last position of a character at or before start (-1 = from the end), or -1
int GUIStringData::rFind(int character,int start) const
{
    if(start==-1) start=this->mLength-1;
    for(int i=start;i>=0;--i)
        if(this->mText[i]==character) return i;
    return -1;
}
}

#include <string.h>
namespace WONAPI {
// Returns the first position of any of theStr's characters at or after theStartPos, or -1 (not identified in the original exe)
int GUIStringData::findOneOf(const GUIStringData *theStr, int theStartPos) const
{
	int aFirstFound = -1;

	for (int i=0; i<theStr->mLength; i++)
	{
		int aFound = this->find(theStr->mText[i], theStartPos);
		if (aFound!=-1)
			if (aFirstFound==-1 || aFound<aFirstFound)
				aFirstFound = aFound;
	}

	return aFirstFound;
}

// Returns the last position of a substring at or before theStartPos, or -1 (not identified in the original exe)
int GUIStringData::rFind(const GUIStringData* theStr, int theStartPos) const
{
	std::wstring sSrc = this->GetWString();
	std::wstring sFind = theStr->GetWString();

	return sSrc.rfind(sFind, theStartPos);
}
}
