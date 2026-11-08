// Mouse and link-state handling of a linked HTML image (file name inferred)
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
#include "ee/wonapi/WONLobbyHTMLImageSegments.h"
namespace WONAPI {
// FUNCTION: WON 0x0070af30
// Shows the linked image's border in the pressed colour and repaints the table
void ImageLinkLineSegment::LinkDown()
{
	this->mBorderColor = this->mDownColor;
	LineSegment::mTableContext->Invalidate();
}

// FUNCTION: WON 0x0070af50
// Restores the linked image's normal border colour, repainting only when it changed
void ImageLinkLineSegment::LinkUp()
{
	if(this->mBorderColor!=this->mNormalColor)
	{
		this->mBorderColor = this->mNormalColor;
		LineSegment::mTableContext->Invalidate();
	}
}

// FUNCTION: WON 0x0070af70
// On mouse entry, hit-tests the image map if there is one; otherwise shows the hand cursor and sends a
// link-over event for the image's link
void ImageLinkLineSegment::MouseEnter(int x, int y)
{
	if(this->mImageMap.get()!=NULL)
	{
		this->mCurLinkParam = NULL;
		this->MouseMove(x,y);
		return;
	}

	LineSegment::mTableContext->SetCursor(Cursor::GetStandardCursor(StandardCursor_Hand));
	LineSegment::mTableContext->DeliverEvent(new HyperLinkEvent(LineSegment::mTableContext,ComponentEvent_HyperLinkOver,this->mCurLinkParam.get()),false);
}

// FUNCTION: WON 0x0070b080
// Tracks the image-map area under the mouse (falling back to the anchor link); on a change, updates the
// cursor and sends a link-over event
void ImageLinkLineSegment::MouseMove(int x, int y)
{
	if(this->mImageMap.get()==NULL)
		return;

	TextLinkParam *aParam = this->mImageMap->GetLink(x,y);
	if(aParam==NULL)
		aParam = this->mAnchorLinkParam.get();

	if(aParam==this->mCurLinkParam.get())
		return;
	this->mCurLinkParam = aParam;
	LineSegment::mTableContext->SetCursor(aParam!=NULL ? Cursor::GetStandardCursor(StandardCursor_Hand) : NULL);
	LineSegment::mTableContext->DeliverEvent(new HyperLinkEvent(LineSegment::mTableContext,ComponentEvent_HyperLinkOver,this->mCurLinkParam.get()),false);

}

// FUNCTION: WON 0x0070b1b0
// Presses the current link on a left-button press
void ImageLinkLineSegment::MouseDown(int x, int y, MouseButton theButton)
{
	if(this->mCurLinkParam.get()==NULL)
		return;

	if(theButton==MouseButton_Left)
		this->mCurLinkParam->LinkDown();
}

// FUNCTION: WON 0x0070b1d0
// Releases the current link on a left-button release and sends a link-activated event if it was pressed
void ImageLinkLineSegment::MouseUp(int x, int y, MouseButton theButton)
{
	if(this->mCurLinkParam.get()==NULL)
		return;

	if(theButton==MouseButton_Left)
	{
		if(this->mCurLinkParam->LinkUp())
			LineSegment::mTableContext->DeliverEvent(new HyperLinkEvent(LineSegment::mTableContext,ComponentEvent_HyperLinkActivated,this->mCurLinkParam.get()),false);
	}
}

// FUNCTION: WON 0x0070b280
// On mouse exit, releases the current link and sends a link-over event with no link
void ImageLinkLineSegment::MouseExit()
{
	if(this->mCurLinkParam.get()==NULL)
		return;

	this->mCurLinkParam->LinkUp();
	LineSegment::mTableContext->DeliverEvent(new HyperLinkEvent(LineSegment::mTableContext,ComponentEvent_HyperLinkOver,NULL),false);
}
// A linked image never starts a text selection; the linker folds this body into the identical one
// at 0x0070a7b0
bool ImageLinkLineSegment::CanStartSelection() { return false; }
}
