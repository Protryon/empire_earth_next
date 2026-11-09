// Component backgrounds: fill colour, border colour, tiled or stretched image, or a painted component
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
#include "ee/wonapi/WONLobbyBackground.h"



using namespace WONAPI;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00735cb0
// Resets the background to no colour, no border, tiled image with offsets, no grab
void Background::Init()
{
	this->mColor = -1;
	this->mBorderColor = -1;
	this->mUseOffsets = true;
	this->mStretchImage = false;
	this->mWantGrabBG = false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
Background::Background()
{
	this->Init();
	this->mColor = (int)0x8000000d;
}

// FUNCTION: WON 0x00735cd0
// Creates a default background filled with the system window colour (constructor body above)
// (??0Background@WONAPI@@QAE@XZ)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00735cf0
// Creates a background that tiles an image
Background::Background(Image *theImage)
{
	this->Init();
	this->mImage = theImage;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
Background::Background(int theColor, int theBorderColor)
{
	this->Init();
	this->mColor = theColor;
	this->mBorderColor = theBorderColor;
}

// FUNCTION: WON 0x00735d80
// Creates a background with a fill colour and a border colour (constructor body above)
// (??0Background@WONAPI@@QAE@HH@Z)

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00735db0
// Creates a background drawn by a component
Background::Background(Component *theComponent)
{
	this->Init();
	this->mComponent = theComponent;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00735e50
// Sets the fill colour (-1 for none)
void Background::SetColor(int theColor)
{
	this->mColor = theColor;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x00735e60
// Paints a rectangle: stretched image alone, or fill, border, tiled image (scrolled by the offsets) and component
void Background::Paint(Graphics &g, int xpos, int ypos, int width, int height, int xoff, int yoff)
{
	if(this->mStretchImage && this->mImage.get()!=NULL)
	{
		if(this->mImage->mWidth!=width || this->mImage->mHeight!=height)
			this->mImage->SetSize(width,height);

		g.DrawImage(this->mImage.get(),xpos,ypos);
		return;
	}

	g.Translate(xpos,ypos);

	if(this->mColor!=-1)
	{
		g.SetColor(this->mColor);
		g.FillRect(0,0,width,height);
	}

	if(this->mBorderColor!=-1)
	{
		g.SetColor(this->mBorderColor);
		g.DrawRect(0,0,width,height);
	}
	
	if(this->mImage.get()!=NULL)
	{
		g.PushClipRect(0,0,width,height);
		int startX = this->mUseOffsets?-(xoff%this->mImage->mWidth):0;
		int startY = this->mUseOffsets?-(yoff%this->mImage->mHeight):0;
		for(int y = startY; y<height; y+=this->mImage->mHeight)
			for(int x = startX; x<width; x+=this->mImage->mWidth)
				g.DrawImage(this->mImage.get(), x, y);

		g.PopClipRect();
	}

	if(this->mComponent.get()!=NULL)
	{
		this->mComponent->SetSize(width,height);
		this->mComponent->Invalidate();
		this->mComponent->Paint(g);
	}
	
	g.Translate(-xpos,-ypos);
}
