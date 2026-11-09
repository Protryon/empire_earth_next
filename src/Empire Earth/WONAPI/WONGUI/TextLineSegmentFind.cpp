// Text search inside one rich text line segment, continuing a match across segments
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <cwctype>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTextSegment.h"
namespace WONAPI {
// FUNCTION: WON 0x0070A3F0
// Searches the segment's part of the search range for the find text (optionally ignoring case),
// recording a full or trailing partial match; returns whether the whole text has been found
bool TextLineSegment::FindStr(ElementFindInfo &theInfo)
{ 
	if(this->mStartSelIndex<theInfo.mStartSelSegment || this->mStartSelIndex>theInfo.mEndSelSegment)
		return false;

	int aStartPos = 0;
	int anEndPos = this->mText.length();
	if(theInfo.mStartSelSegment==this->mStartSelIndex)
		aStartPos = theInfo.mStartSelSegmentPos;
	if(theInfo.mEndSelSegment==this->mStartSelIndex)
		anEndPos = theInfo.mEndSelSegmentPos;

	if(theInfo.mFoundStartSegment>=0)
	{
		if(this->FindPartial(theInfo,aStartPos,anEndPos))
			return theInfo.mCurFoundLength == theInfo.mFindText.length();

		theInfo.mFoundStartSegment = -1;
	}

	const GUIString &aText = theInfo.mFindText;
	int p1,p2;
	int cp = aStartPos;
	int len2 = aText.length();
	const bool caseSensitive = theInfo.mCaseSensitive;
	while(cp < anEndPos)
	{
		p1 = cp;
		p2 = 0;
		while(p1<anEndPos && p2<len2)
		{
			if(caseSensitive)
			{
				if(this->mText[p1]!=aText[p2])
					break;
			}
			else if(towupper(this->mText[p1])!=towupper(aText[p2]))
				break;

			p1++; p2++;
		}
		if(p1==anEndPos || p2==len2)
			break;

		cp++;
	}

	if(p2>0)
	{
		theInfo.mFoundX = this->mAbsX;
		theInfo.mFoundY = this->mAbsY;
		theInfo.mCurFoundLength = p2;
		theInfo.mFoundStartSegment = theInfo.mFoundEndSegment = this->mStartSelIndex;
		theInfo.mFoundStartChar = cp;
		theInfo.mFoundEndChar = p1;
 
		return theInfo.mCurFoundLength==theInfo.mFindText.length();
	}
	else
		return false; 
}

}
