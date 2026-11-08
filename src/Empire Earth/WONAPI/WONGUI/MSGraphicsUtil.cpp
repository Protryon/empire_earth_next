// Windows-style drawing helpers: 3D frames, arrows, check boxes, radio circles, tabs and the close X
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
namespace WONAPI {
// FUNCTION: WON 0x006fdcd0
// Draws a two-pixel 3D frame, raised or sunken (down), at the given position
void LobbyDraw3DFrame(Graphics &g, int x, int y, int width, int height, bool down)
{
	g.Translate(x,y);

	g.SetSysColor(StandardColor_3DHilight);
	g.DrawRect(0,0,width,height);
	g.SetSysColor(StandardColor_3DDarkShadow);

	if(down)
	{
		g.DrawLine(0,0,width-1,0);
		g.DrawLine(0,0,0,height-1);

		g.SetSysColor(StandardColor_3DFace);
		g.DrawLine(width-2,1,width-2,height-2);
		g.DrawLine(width-2,height-2,2,height-2);

		g.SetSysColor(StandardColor_3DShadow);
		g.DrawLine(1,1,width-2,1);
		g.DrawLine(1,1,1,height-2);		
	}
	else
	{
		g.DrawLine(width-1,0,width-1,height-1);
		g.DrawLine(0,height-1,width-1,height-1);

		g.SetSysColor(StandardColor_3DShadow);
		g.DrawLine(1,height-2,width-2,height-2);
		g.DrawLine(width-2,height-2,width-2,1);
	}	

	g.Translate(-x,-y);
}

// FUNCTION: WON 0x006fddf0
// Draws a raised button frame, or a flat shadow-coloured rectangle when down
void LobbyDrawFrame(Graphics &g, int x, int y, int width, int height, bool down)
{
	g.Translate(x,y);

	if(!down)
	{
		g.SetSysColor(StandardColor_3DDarkShadow);
		g.DrawLine(width-1,0,width-1,height-1);
		g.DrawLine(0,height-1,width-1,height-1);

		g.SetSysColor(StandardColor_3DHilight);
		g.DrawLine(1,1,1,height-2);
		g.DrawLine(1,1,width-2,1);

		g.SetSysColor(StandardColor_3DShadow);
		g.DrawLine(1,height-2,width-2,height-2);
		g.DrawLine(width-2,height-2,width-2,1);
	}
	else
	{
		g.SetSysColor(StandardColor_3DShadow);
		g.DrawRect(0,0,width,height);
	}

	g.Translate(-x,-y);
}

// FUNCTION: WON 0x006fdee0
// Draws a sunken frame inside a dark outline (the down flag is ignored)
void MSGraphicsUtil::DrawFrame3(Graphics &g, int x, int y, int width, int height, bool down)
{
	g.Translate(x,y);

	g.SetSysColor(StandardColor_3DDarkShadow);
	g.DrawRect(0,0,width,height);
	g.SetSysColor(StandardColor_3DHilight);
	g.DrawLine(width-1,0,width-1,height-1);
	g.DrawLine(0,height-1,width-1,height-1);
	g.SetSysColor(StandardColor_3DShadow);
	g.DrawLine(1,1,width-2,1);
	g.DrawLine(1,1,1,height-2);		
	g.SetSysColor(StandardColor_3DFace);
	g.DrawLine(width-2,1,width-2,height-2);
	g.DrawLine(1,height-2,width-2,height-2);		

	g.Translate(-x,-y);
}

// FUNCTION: WON 0x006fdfb0
// Draws a small four-stroke arrow centred in width x height, grey with a highlight when disabled; the x
// and y offsets are ignored (shadowed by the centre locals)
void MSGraphicsUtil::DrawArrow(Graphics &g, Direction theDirection, int x, int y, int width, int height, bool disabled)
{
	int i = 0;

	if(disabled)
		g.SetSysColor(StandardColor_GrayText);
	else
		g.SetSysColor(StandardColor_ButtonText);

	// Always four strokes, even for small or negative sizes
	int arrow_size = 4;

	int arrow_size2 = arrow_size/2;


	if(theDirection==Direction_Left)
	{
		int x = width/2-arrow_size2, y = height/2;
		for(i=0; i<arrow_size; i++)
			g.DrawLine(x+i, y-i, x+i, y+i);	

		if(disabled)
		{
			g.SetSysColor(StandardColor_3DHilight);
			g.DrawLine(x+i,y-i+arrow_size2,x+i,y+i);
		}
	}
	else if(theDirection==Direction_Right)
	{
		int x = width/2-arrow_size2, y = height/2;
		for(i=0; i<arrow_size; i++)
		{
			g.DrawLine(x+(3-i), y-i, x+(3-i), y+i);	
			if(!disabled || i>=3)
				continue;
			g.SetSysColor(StandardColor_3DHilight);
			g.DrawLine(x+(arrow_size-i-1),y+i+1,x+(arrow_size-i-1),y+i+2);
			g.SetSysColor(StandardColor_GrayText);
		}

		if(disabled)
			g.SetPixel(x+4,y+1,g.GetStandardColor(StandardColor_3DHilight));
	}
	else if(theDirection==Direction_Up)
	{
		int x = width/2, y = height/2-arrow_size2;
		for(i=0; i<arrow_size; i++)
			g.DrawLine(x-i, y+i, x+i, y+i);	

		if(disabled)
		{
			g.SetSysColor(StandardColor_3DHilight);
			g.DrawLine(x-i+2, y+i, x+i, y+i);
		}
	}
	else if(theDirection==Direction_Down)
	{
		int x = width/2, y = height/2-arrow_size2;
		for(i=0; i<arrow_size; i++)
		{
			g.DrawLine(x-(arrow_size-i-1), y+i, x+(arrow_size-i-1), y+i);	
			if(!disabled || i<=0)
				continue;
			g.SetSysColor(StandardColor_3DHilight);
			g.DrawLine(x+(arrow_size-i-1)+1,y+i,x+(arrow_size-i-1)+2,y+i);
			g.SetSysColor(StandardColor_GrayText);
		}

		if(disabled)
			g.SetPixel(x+1,y+arrow_size,g.GetStandardColor(StandardColor_3DHilight));
	}
}

// FUNCTION: WON 0x006fe250
// Draws a sunken check box (face colour while pressed, window colour otherwise) with a tick if checked
void MSGraphicsUtil::DrawCheck(Graphics &g, int x, int y, int width, int height, bool down, bool checked)
{
	g.Translate(x,y);

	if(down)
		g.SetSysColor(StandardColor_3DFace);
	else
		g.SetSysColor(StandardColor_Back);

	g.FillRect(0,0,width,height);

	g.SetSysColor(StandardColor_3DShadow);
	g.DrawLine(0,0,width-1,0);
	g.DrawLine(0,0,0,height-1);

	g.SetSysColor(StandardColor_3DHilight);
	g.DrawLine(width-1,0,width-1,height-1);
	g.DrawLine(0,height-1,width-1,height-1);

	g.SetSysColor(StandardColor_3DDarkShadow);
	g.DrawLine(1,1,width-2,1);
	g.DrawLine(1,1,1,height-2);

	g.SetSysColor(StandardColor_3DFace);
	g.DrawLine(width-2,1,width-2,height-2);
	g.DrawLine(1,height-2,width-2,height-2);

	if(checked)
	{
		g.SetSysColor(StandardColor_Text);
		int x = 3;
		int y = height/2 - 1;
		int i;
		for(i=0; i<3; i++, x++, y++)
			g.DrawLine(x,y,x,y+2);

		y-=2;
		for(i=0; i<4; i++, x++, y--)
			g.DrawLine(x,y,x,y+2);
	}
	g.Translate(-x,-y);
}

// FUNCTION: WON 0x006fe3b0
// Draws a sunken radio circle (face colour while pressed, window colour otherwise) with a dot if checked
void MSGraphicsUtil::DrawRadio(Graphics &g, int x, int y, int width, int height, bool down, bool checked)
{
	g.Translate(x,y);

	if(down)
		g.SetSysColor(StandardColor_3DFace);
	else
		g.SetSysColor(StandardColor_Back);

	g.FillEllipse(0,0,width,height);

	g.SetSysColor(StandardColor_3DShadow);
	g.DrawArc(0,0,width,height,45,180);
	g.SetSysColor(StandardColor_3DDarkShadow);
	g.DrawArc(1,1,width-2,height-2,45,180);
	
	g.SetSysColor(StandardColor_3DHilight);
	g.DrawArc(0,0,width,height,235,180);
	g.SetSysColor(StandardColor_3DFace);
	g.DrawArc(1,1,width-2,height-2,235,180);

	if(checked)
	{
		g.SetSysColor(StandardColor_Text);
		g.FillEllipse(width/2-2,height/2-2,4,4);
	}

	g.Translate(-x,-y);
}

// FUNCTION: WON 0x006fe4d0
// Draws a tab with rounded top corners, highlight on the left and top and shadows on the right
void MSGraphicsUtil::DrawTab(Graphics &g, int x, int y, int width, int height)
{
	g.SetSysColor(StandardColor_3DFace);
	g.FillRect(x,y+2,width-x,height-y-2);
	g.DrawLine(x+2,y+1,width-1,y+1);
	g.DrawLine(x+1,y+2,width-1,y+2);

	g.SetSysColor(StandardColor_3DHilight);
	g.DrawLine(x,y+2,x,height-1);
	g.SetPixel(x+1,y+1,g.GetStandardColor(StandardColor_3DHilight));
	g.DrawLine(x+2,y,width-2,y);

	g.SetSysColor(StandardColor_3DShadow);
	g.DrawLine(width-2,y+2,width-2,height-1);
	g.SetSysColor(StandardColor_3DDarkShadow);
	g.SetPixel(width-2,y+1,g.GetStandardColor(StandardColor_3DDarkShadow));
	g.DrawLine(width-1,y+2,width-1,height-1);
}

// FUNCTION: WON 0x006fe5e0
// Draws the 8 x 8 close X centred in the area, offset by x and y
void MSGraphicsUtil::DrawCloseX(Graphics &g, int x, int y, int width, int height)
{
	g.Translate(x,y);
	g.SetSysColor(StandardColor_ButtonText);
	int hmid = width/2;
	int vmid = height/2;
	int left = hmid-4;
	int right = hmid+3;
	int top = vmid-4;
	int bottom = vmid+3;
	for(int i=0; i<4; i++)
	{
		g.DrawLine(left+i,top+i,left+i+1,top+i);
		g.DrawLine(right-i,top+i,right-1-i,top+i);
		g.DrawLine(left+i,bottom-i,left+i+1,bottom-i);
		g.DrawLine(right-i,bottom-i,right-i-1,bottom-i);
	}
	g.Translate(-x,-y);
}
}
