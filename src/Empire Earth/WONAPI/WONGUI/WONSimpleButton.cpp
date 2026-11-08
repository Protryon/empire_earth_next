// Simple image button: desired size and painting by state (file name inferred)
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
#include "ee/wonapi/WONLobbySkinFrame.h"
namespace WONAPI {

// FUNCTION: WON 0x00717A90
// Returns the largest width and height among the button's four state images
void WONSimpleButton::GetDesiredSize(int &width, int &height)
{
	width = height = 0;
	for(int i=0; i<4; i++)
	{
		Image *anImage = this->mImages[i].get();
		if(anImage==NULL)
			continue;
		if(anImage->mWidth>width)
			width = anImage->mWidth;

		if(anImage->mHeight>height)
			height = anImage->mHeight;
	}
}

// FUNCTION: WON 0x00717BD0
// Draws the disabled, pressed or mouse-over image (the normal one when that state has none), scaled if set
void WONSimpleButton::Paint(Graphics &g)
{
	Button::Paint(g);
	Image *anImage = NULL;
	if(this->IsDisabled())
		anImage = this->mImages[2].get();
	else if((this->mCheckMouseButtonDown && (this->mMouseDownState!=0)) || (!this->mCheckMouseButtonDown && this->IsPressed()))
		anImage = this->mImages[1].get();
	else if(this->IsMouseOver())
		anImage = this->mImages[3].get();
	
	if(anImage==NULL)
		anImage = this->mImages[0].get();

	if(this->mScaleImage && (anImage->mWidth!=this->Width() || anImage->mHeight!=this->Height()))
		anImage->SetSize(this->Width(),this->Height());

	g.DrawImage(anImage,0,0);
}
}
