// Localized string substitution: fills <s1>..<s9> placeholders in a GUI string (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <ctype.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONStringLocalize.h"
namespace WONAPI {
// FUNCTION: WON 0x00749f10
// Returns the string with each <sN> tag replaced by substitution N (missing ones dropped); << gives a literal <
GUIString StringLocalize::GetStr(const GUIString &theStr, const GUIString **subs, int numSubs)
{
	GUIString aNewStr;
	int aStartPos = 0;
	while(true)
	{
		int aBracketPos = theStr.find('<',aStartPos);
		if(aBracketPos<0)
			break;

		aNewStr.append(theStr,aStartPos,aBracketPos-aStartPos);
		int aNextChar = theStr[aBracketPos+1];
		if(aNextChar=='<') // literal '<' character
		{
			aNewStr.append('<');
			aStartPos = aBracketPos+2;
			continue;
		}
		else if(aBracketPos+1<theStr.length()) // substitution
		{
			int aNextNextChar = theStr[aBracketPos+2];
			int aSubId = aNextNextChar - '0' - 1;
			if(tolower(aNextChar)=='s' && aSubId>=0 && aSubId<numSubs && subs[aSubId]!=NULL)
				aNewStr.append(*(subs[aSubId]));
		}

		aStartPos = aBracketPos+1;
		aBracketPos = theStr.find('>',aStartPos); // find ending bracket
		if(aBracketPos<0)
			break;
			
		aStartPos = aBracketPos+1;
	}

	if(aStartPos < theStr.length())
		aNewStr.append(theStr,aStartPos);

	return aNewStr;
}

}
