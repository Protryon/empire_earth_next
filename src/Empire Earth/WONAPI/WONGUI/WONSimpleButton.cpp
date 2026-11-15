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
#include "ee/wonapi/WONGUIDraw.h"
#include "ee/wonapi/WONSkinLayout.h"
namespace WONAPI {

// Returns the largest width and height among the button's four state images (skinlay.rs)
void WONSimpleButton::GetDesiredSize(int &width, int &height)
{
	WONSkinLayout(this,EeWonSkinLayKind::SimpleButtonSize,0,0,0,&width,&height);
}

// Draws the disabled, pressed or mouse-over image (the normal one when that state has none), scaled if set
void WONSimpleButton::Paint(Graphics &g)
{
	Button::Paint(g);
	// the state's image (disabled, pressed, mouse-over, else the normal one), scaled when set (guidraw.rs
	// simple_button)
	EeWonSimpleButtonPaint p;
	p.disabled = this->IsDisabled();
	p.check_mouse_down = this->mCheckMouseButtonDown;
	p.pressed = this->IsPressed();
	p.mouse_over = this->IsMouseOver();
	p.scale_image = this->mScaleImage;
	p.mouse_down_state = this->GetMouseDownState();
	p.width = this->Width();
	p.height = this->Height();
	WONDrawSlots aSlots;
	for(int i=0; i<4; i++)
	{
		Image* anImage = this->mImages[i].get();
		aSlots.mImages[i] = anImage;
		p.has_image[i] = anImage!=NULL;
		p.image_width[i] = anImage ? anImage->mWidth : 0;
		p.image_height[i] = anImage ? anImage->mHeight : 0;
	}
	WONDrawOps anOps([&](EeWonDrawOp* o, size_t n) { return ee_won_gui_draw_simple_button(&p,o,n); });
	WONDrawRun(g,anOps,aSlots);
}
}
