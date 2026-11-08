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
namespace WONAPI {

// FUNCTION: WON 0x007177C0
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

// FUNCTION: WON 0x007178C0
// Stretches the frame image to the component's full size when needed and draws it at the top-left corner
void WONFrame::Paint(Graphics &g, Component *theComponent)
{
	if(this->mImage.get()==NULL)
		return;

	if(this->mImage->mWidth!=theComponent->Width() || this->mImage->mHeight!=theComponent->Height())
		this->mImage->SetSize(theComponent->Width(),theComponent->Height());

	g.DrawImage(this->mImage.get(),0,0);
}

// FUNCTION: WON 0x00717910
// Stretches the frame image to the component's size less the given paddings and draws it inside them
void WONFrame::Paint(Graphics &g, Component *theComponent, int lp, int tp, int rp, int bp)
{
	if(this->mImage.get()==NULL)
		return;

	if(this->mImage->mWidth!=theComponent->Width()-lp-rp || this->mImage->mHeight!=theComponent->Height()-tp-bp)
		this->mImage->SetSize(theComponent->Width()-lp-rp,theComponent->Height()-tp-bp);

	g.DrawImage(this->mImage.get(),lp,tp);
}
}

namespace WONAPI {
// FUNCTION: WON 0x0071a7c0
// Sets the frame image and its four border widths
void WONFrameComponent::SetFrameImageAndBorders(Image* image,int left,int top,int right,int bottom) { this->mFrame.SetImage(image,left,top,right,bottom); }
// FUNCTION: WON 0x0071a7b0
// Sets the frame image, keeping the borders
void WONFrameComponent::SetFrameImage(Image* image) { this->mFrame.SetImage(image); }
// FUNCTION: WON 0x0071a7f0
// Sets the frame's four border widths
void WONFrameComponent::SetFrameBorders(int left,int top,int right,int bottom) { this->mFrame.mLeft=left; this->mFrame.mTop=top; this->mFrame.mRight=right; this->mFrame.mBottom=bottom; }
// FUNCTION: WON 0x0071a630
// Sets the frame's left border width
void WONFrameComponent::SetFrameLeft(int value) { this->mFrame.mLeft=value; }
// FUNCTION: WON 0x0071a640
// Sets the frame's top border width
void WONFrameComponent::SetFrameTop(int value) { this->mFrame.mTop=value; }
// FUNCTION: WON 0x0071a650
// Sets the frame's right border width
void WONFrameComponent::SetFrameRight(int value) { this->mFrame.mRight=value; }
// FUNCTION: WON 0x0071a660
// Sets the frame's bottom border width
void WONFrameComponent::SetFrameBottom(int value) { this->mFrame.mBottom=value; }
// Returns the frame's left border width (the linker folds this body into the identical one at 0x0071a670)
int WONFrameComponent::GetFrameLeft() { return this->mFrame.mLeft; }
// FUNCTION: WON 0x0071a680
// Returns the frame's top border width
int WONFrameComponent::GetFrameTop() { return this->mFrame.mTop; }
// FUNCTION: WON 0x0071a690
// Returns the frame's right border width
int WONFrameComponent::GetFrameRight() { return this->mFrame.mRight; }
// FUNCTION: WON 0x0071a6a0
// Returns the frame's bottom border width
int WONFrameComponent::GetFrameBottom() { return this->mFrame.mBottom; }
// FUNCTION: WON 0x0071a810
// Paints the component, then its frame image over its full size
void WONFrameComponent::Paint(Graphics& g) { Component::Paint(g); this->mFrame.Paint(g,this); }
// The linker folds this body into the identical StretchImage::SetImage at 0x00749ab0
// Sets the frame image, keeping the borders
void WONFrame::SetImage(Image* image) { this->mImage=image; }
// FUNCTION: WON 0x00717730
// Sets the frame image and its four border widths
void WONFrame::SetImage(Image* image,int left,int top,int right,int bottom)
{
    this->mImage=image; this->mLeft=left; this->mTop=top; this->mRight=right; this->mBottom=bottom;
}
}
