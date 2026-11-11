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
#include "ee/wonapi/WONGUIDraw.h"
namespace WONAPI {
void TextImage::Paint(Graphics &g, TextArea *theTextArea)
{
	// the selection background when the selection covers the image, then the image (guidraw.rs text_image)
	WONDrawSlots aSlots;
	aSlots.mImages[0] = this->mImage;
	WONDrawOps anOps([&](EeWonDrawOp* o, size_t n) {
		return ee_won_gui_draw_text_image(theTextArea->mSelStartChar,theTextArea->mSelEndChar,this->mAbsStartChar,
			this->mAbsEndChar,this->mx,this->my,this->mWidth,this->mLine->mHeight,
			(uint32_t)theTextArea->mSelColor.GetBackgroundColor(g),o,n); });
	WONDrawRun(g,anOps,aSlots);
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
