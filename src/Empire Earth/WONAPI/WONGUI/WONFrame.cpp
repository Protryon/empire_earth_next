// Skin frame painting and the frame component's image and border accessors (file name inferred)
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
namespace WONAPI {

// Copies another frame's borders and a private duplicate of its image (none when it has no image)
void WONFrame::Copy(const WONFrame &from)
{
	if(from.mImage.get()==NULL)
		this->mImage = NULL;
	else
		this->mImage = from.mImage->Duplicate();

	this->mLeft = from.mLeft;
	this->mTop = from.mTop;
	this->mRight = from.mRight;
	this->mBottom = from.mBottom;
}

// Stretches the frame image to the component's full size when needed and draws it at the top-left corner
void WONFrame::Paint(Graphics &g, Component *theComponent)
{
	this->Paint(g,theComponent,0,0,0,0);
}

// Stretches the frame image to the component's size less the given paddings and draws it inside them
void WONFrame::Paint(Graphics &g, Component *theComponent, int lp, int tp, int rp, int bp)
{
	// guidraw.rs frame_paint
	EeWonFrame aFrame = WONDrawFrameOf(*this,theComponent,lp,tp,rp,bp);
	WONDrawSlots aSlots;
	aSlots.mImages[0] = this->mImage.get();
	WONDrawOps anOps([&](EeWonDrawOp* o, size_t n) { return ee_won_gui_draw_frame(&aFrame,o,n); });
	WONDrawRun(g,anOps,aSlots);
}
}

namespace WONAPI {
// Sets the frame image and its four border widths
void WONFrameComponent::SetFrameImageAndBorders(Image* image,int left,int top,int right,int bottom) { this->mFrame.SetImage(image,left,top,right,bottom); }
// Sets the frame image, keeping the borders
void WONFrameComponent::SetFrameImage(Image* image) { this->mFrame.SetImage(image); }
// Sets the frame's four border widths
void WONFrameComponent::SetFrameBorders(int left,int top,int right,int bottom) { this->mFrame.mLeft=left; this->mFrame.mTop=top; this->mFrame.mRight=right; this->mFrame.mBottom=bottom; }
// Sets the frame's left border width
void WONFrameComponent::SetFrameLeft(int value) { this->mFrame.mLeft=value; }
// Sets the frame's top border width
void WONFrameComponent::SetFrameTop(int value) { this->mFrame.mTop=value; }
// Sets the frame's right border width
void WONFrameComponent::SetFrameRight(int value) { this->mFrame.mRight=value; }
// Sets the frame's bottom border width
void WONFrameComponent::SetFrameBottom(int value) { this->mFrame.mBottom=value; }
// Returns the frame's left border width (the linker folds this body into the identical one at 0x0071a670)
int WONFrameComponent::GetFrameLeft() { return this->mFrame.mLeft; }
// Returns the frame's top border width
int WONFrameComponent::GetFrameTop() { return this->mFrame.mTop; }
// Returns the frame's right border width
int WONFrameComponent::GetFrameRight() { return this->mFrame.mRight; }
// Returns the frame's bottom border width
int WONFrameComponent::GetFrameBottom() { return this->mFrame.mBottom; }
// Paints the component, then its frame image over its full size
void WONFrameComponent::Paint(Graphics& g) { Component::Paint(g); this->mFrame.Paint(g,this); }
// The linker folds this body into the identical StretchImage::SetImage at 0x00749ab0
// Sets the frame image, keeping the borders
void WONFrame::SetImage(Image* image) { this->mImage=image; }
// Sets the frame image and its four border widths
void WONFrame::SetImage(Image* image,int left,int top,int right,int bottom)
{
    this->mImage=image; this->mLeft=left; this->mTop=top; this->mRight=right; this->mBottom=bottom;
}
}
