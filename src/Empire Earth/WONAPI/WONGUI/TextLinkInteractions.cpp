// Mouse handling of hyperlink line segments in rich text tables
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyTextLinkSegment.h"
namespace WONAPI {
// FUNCTION: WON 0x0070abc0
// Shows the link in its pressed colour and repaints the table
void TextLinkLineSegment::LinkDown()
{
	this->mColor = this->mDownColor;
	LineSegment::mTableContext->Invalidate();
}


// FUNCTION: WON 0x0070abe0
// Restores the link's normal colour and repaints the table if it was changed
void TextLinkLineSegment::LinkUp()
{
	if(this->mColor!=this->mNormalColor)
	{
		this->mColor = this->mNormalColor;
		LineSegment::mTableContext->Invalidate();
	}
}


// FUNCTION: WON 0x70ac00
// Shows the hand cursor and sends a hyperlink-over event for this link
void TextLinkLineSegment::MouseEnter(int x, int y)
{
	LineSegment::mTableContext->SetCursor(Cursor::GetStandardCursor(StandardCursor_Hand));
	LineSegment::mTableContext->DeliverEvent(new HyperLinkEvent(LineSegment::mTableContext,ComponentEvent_HyperLinkOver,this->mLinkParam.get()),false);
}


// FUNCTION: WON 0x0070ad70
// Presses the link on a left-button press
void TextLinkLineSegment::MouseDown(int x, int y, MouseButton theButton)
{
	if(theButton==MouseButton_Left)
		this->mLinkParam->LinkDown();
}


// FUNCTION: WON 0x70ad90
// On a left-button release, releases the link and sends a hyperlink-activated event if it was pressed
void TextLinkLineSegment::MouseUp(int x, int y, MouseButton theButton)
{
	if(theButton==MouseButton_Left)
	{
		if(this->mLinkParam->LinkUp())
			LineSegment::mTableContext->DeliverEvent(new HyperLinkEvent(LineSegment::mTableContext,ComponentEvent_HyperLinkActivated,this->mLinkParam.get()),false);
	}
}


// FUNCTION: WON 0x70ae40
// Releases the link and sends a hyperlink-over event with no link
void TextLinkLineSegment::MouseExit()
{
	this->mLinkParam->LinkUp();
	LineSegment::mTableContext->DeliverEvent(new HyperLinkEvent(LineSegment::mTableContext,ComponentEvent_HyperLinkOver,NULL),false);
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0070acc0
// (public: virtual void * __thiscall WONAPI::HyperLinkEvent::`scalar deleting destructor'(unsigned int)    [??_GHyperLinkEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x0070ace0
// (public: virtual __thiscall WONAPI::HyperLinkEvent::~HyperLinkEvent(void)    [??1HyperLinkEvent@WONAPI@@UAE@XZ])
