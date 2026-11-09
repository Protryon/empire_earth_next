// Text area anchors, links and the anchor instruction (not identified in the original exe)
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
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
namespace WONAPI {
void TextArea::AddAnchor(const GUIString &theName, int theLineNum)
{
	this->mAnchorMap[theName] = theLineNum;
}

bool TextArea::GotoAnchor(const GUIString &theName)
{
	if(this->GetNumLines()<=0)
		return false;

	AnchorMap::iterator anItr = this->mAnchorMap.find(theName);
	if(anItr==this->mAnchorMap.end())
		return false;

	int aLineNum = anItr->second;
	if(aLineNum>=this->mLineVector.size())
		aLineNum = this->mLineVector.size()-1;

	if(aLineNum<0)
		aLineNum = 0;

	TextLine *aLine = this->mLineVector[aLineNum];
	this->mVertOffset = aLine->my;
	this->AdjustPosition(false);
	return true;
}

void TextArea::AddLink(const GUIString &theText, const GUIString &theLinkParam, bool lineBreak)
{
	SmartPtr<TextLink> aSeg = new TextLink;
	aSeg->mText = theText;
	aSeg->mLinkParam = theLinkParam;
	aSeg->mLineBreak = lineBreak;
	aSeg->mColor = this->mLinkColor;
	aSeg->mFont = this->mLinkFont;
	this->AddSegment(aSeg);
}

void TextAnchorInstruction::PerformInstructions(TextArea *theTextArea)
{
	theTextArea->AddAnchor(this->mAnchorName, theTextArea->GetNumLines()-1);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00712b40
// (public: bool __thiscall WONAPI::GUIString::operator<(class WONAPI::GUIString const &)const     [??MGUIString@WONAPI@@QBE_NABV01@@Z])
