// Drawing of the Windows-skin images (frames, buttons, arrows, check boxes, scrollbar parts)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyMSSkinImage.h"
namespace WONAPI {

// FUNCTION: WON 0x00717110
// Paints a push button face and frame; a default or pressed button (when checked) gets a dark outline
// first, and pressed always uses the flat frame
void WONMSSkin::MSImage::DrawPushButton(Graphics &g, bool useFrame2, bool checkDefault)
{

	bool down = this->FlagSet(MSImageFlag_Down);

	int x = 0, y = 0, width = this->mWidth, height = this->mHeight;

	g.SetSysColor(StandardColor_3DFace);
	g.FillRect(0,0,width,height);

	if(checkDefault && (this->FlagSet(MSImageFlag_Default) || down))
	{
		g.SetSysColor(StandardColor_3DDarkShadow);
		g.DrawRect(0,0,width,height);
		x++;
		y++;
		width-=2;
		height-=2;
	
		if(down)
			useFrame2 = true;
	}

	if(useFrame2)	
		LobbyDrawFrame(g,x,y,width,height,down);
	else
		LobbyDraw3DFrame(g,x,y,width,height,down);
}

// FUNCTION: WON 0x007171E0
// Paints an arrow button: a push button face with the arrow, greyed when disabled
void WONMSSkin::MSImage::DrawArrowButton(Graphics &g, Direction theDirection)
{
	this->DrawPushButton(g,true,false);
	if(this->FlagSet(MSImageFlag_Down))
		MSGraphicsUtil::DrawArrow(g,theDirection,1,1,this->mWidth,this->mHeight,this->FlagSet(MSImageFlag_Disabled));
	else
		MSGraphicsUtil::DrawArrow(g,theDirection,0,0,this->mWidth,this->mHeight,this->FlagSet(MSImageFlag_Disabled));
}

// FUNCTION: WON 0x00717250
// Paints a close button: a push button face with the X, shifted by one pixel while pressed
void WONMSSkin::MSImage::DrawCloseButton(Graphics &g)
{
	this->DrawPushButton(g,false,false);
	if(this->FlagSet(MSImageFlag_Down))
		MSGraphicsUtil::DrawCloseX(g,1,1,this->mWidth,this->mHeight);
	else
		MSGraphicsUtil::DrawCloseX(g,0,0,this->mWidth,this->mHeight);
}

// FUNCTION: WON 0x007172B0
// Paints the skin image according to its type (frames, arrows, buttons, tab, check box, radio, scrollbar
// gutter and corner, combo popup frame); unknown types draw nothing
void WONMSSkin::MSImage::Paint(Graphics &g)
{
	switch(this->mImageType)
	{
		case MSImageType_Frame1:		LobbyDraw3DFrame(g,0,0,this->mWidth,this->mHeight,this->FlagSet(MSImageFlag_Down)); return;
		case MSImageType_Frame2:		LobbyDrawFrame(g,0,0,this->mWidth,this->mHeight,this->FlagSet(MSImageFlag_Down)); return;
		case MSImageType_Frame3:		MSGraphicsUtil::DrawFrame3(g,0,0,this->mWidth,this->mHeight,this->FlagSet(MSImageFlag_Down)); return;

		case MSImageType_LeftArrow:		this->DrawArrowButton(g,Direction_Left);	return;
		case MSImageType_UpArrow:		this->DrawArrowButton(g,Direction_Up);	return;
		case MSImageType_RightArrow:	this->DrawArrowButton(g,Direction_Right);	return;
		case MSImageType_DownArrow:		this->DrawArrowButton(g,Direction_Down);	return;
		
		case MSImageType_PushButton:	this->DrawPushButton(g,false,true);	return;
		case MSImageType_TabCtrlFrame:	this->DrawPushButton(g,false,false); return;
		case MSImageType_ListHeader:	this->DrawPushButton(g,true,false);   return;
		case MSImageType_ScrollThumb:	this->DrawPushButton(g,true,false); return;
		case MSImageType_CloseButton:	this->DrawCloseButton(g); return;	

		case MSImageType_TabButton:		MSGraphicsUtil::DrawTab(g,0,0,this->mWidth,this->mHeight); return;
		case MSImageType_Checkbox:		MSGraphicsUtil::DrawCheck(g,0,0,this->mWidth,this->mHeight,this->FlagSet(MSImageFlag_Down),this->FlagSet(MSImageFlag_Checked)); return;
		case MSImageType_RadioButton:	MSGraphicsUtil::DrawRadio(g,0,0,this->mWidth,this->mHeight,this->FlagSet(MSImageFlag_Down),this->FlagSet(MSImageFlag_Checked)); return;

		case MSImageType_ScrollGutter:	g.SetSysColor(StandardColor_Scrollbar); g.FillRect(0,0,this->mWidth,this->mHeight); return;
		case MSImageType_ScrollCorner:  g.SetSysColor(StandardColor_3DFace); g.FillRect(0,0,this->mWidth,this->mHeight); return;
		
		case MSImageType_ComboPopupFrame: g.SetSysColor(StandardColor_3DDarkShadow); g.DrawRect(0,0,this->mWidth,this->mHeight); return;
	}

}
}
