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
#include "ee/wonapi/WONGUIDraw.h"



using namespace WONAPI;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Creates a background drawn by a component
Background::Background(Component *theComponent)
{
	this->Init();
	this->mComponent = theComponent;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Sets the fill colour (-1 for none)
void Background::SetColor(int theColor)
{
	this->mColor = theColor;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Paints a rectangle: stretched image alone, or fill, border, tiled image (scrolled by the offsets) and component
void Background::Paint(Graphics &g, int xpos, int ypos, int width, int height, int xoff, int yoff)
{
	// guidraw.rs background: the stretched image, or the fill, border, tiles and component
	EeWonBackground b;
	b.has_image = this->mImage.get()!=NULL;
	b.stretch_image = this->mStretchImage;
	b.use_offsets = this->mUseOffsets;
	b.has_component = this->mComponent.get()!=NULL;
	b.image_width = b.has_image ? this->mImage->mWidth : 0;
	b.image_height = b.has_image ? this->mImage->mHeight : 0;
	b.color = this->mColor;
	b.border_color = this->mBorderColor;
	b.x = xpos;
	b.y = ypos;
	b.width = width;
	b.height = height;
	b.x_offset = xoff;
	b.y_offset = yoff;
	WONDrawSlots aSlots;
	aSlots.mImages[0] = this->mImage.get();
	WONDrawOps anOps([&](EeWonDrawOp* o, size_t n) { return ee_won_gui_draw_background(&b,o,n); });
	WONDrawRun(g,anOps,aSlots,[&](const EeWonDrawOp& o) {
		this->mComponent->SetSize(o.b,o.c);
		this->mComponent->Invalidate();
		this->mComponent->Paint(g);
	});
}
