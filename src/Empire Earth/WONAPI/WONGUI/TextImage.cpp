// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// Inline image segment of a text area: painting, size and copy text (not identified in the original exe)
#include <windows.h>
#include <string>
#include <list>
#include <deque>
#include <vector>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTextImage.h"
namespace WONAPI {
void TextImage::Paint(Graphics &g, TextArea *theTextArea)
{
	if(theTextArea->mSelStartChar<=this->mAbsStartChar && theTextArea->mSelEndChar>=this->mAbsEndChar)
	{
		g.SetColor(theTextArea->mSelColor.GetBackgroundColor(g));
		g.FillRect(this->mx,0,this->mWidth,this->mLine->mHeight);
	}

	g.DrawImage(this->mImage,this->mx,this->my);
}

void TextImage::CalcDimensions()
{
	this->mWidth = this->mImage->mWidth;
	this->mHeight = this->mImage->mHeight;
	this->mAscent = this->mHeight;
}

void TextImage::GetSelText(TextArea *theTextArea, GUIString &theStr)
{
	if(!(theTextArea->mSelStartChar<=this->mAbsStartChar) || !(theTextArea->mSelEndChar>=this->mAbsEndChar))
		return;
	theStr.append(this->mText);
	if(this->mLineBreak && theTextArea->mSelEndChar>this->mAbsEndChar)
		theStr.append("\r\n");
}
}
