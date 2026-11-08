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

using namespace WONAPI;

// FUNCTION: WON 0x00706b10
// Creates an image component showing the given image (tiled or not), sized to the image, stretching off
ImageComponent::ImageComponent(Image* image,bool tile)
{
    this->SetImageAndTile(image,tile);
    this->mStretch=false;
}


// FUNCTION: WON 0x00706c10
// Sets the shown image and resizes the component to it (a null image keeps the size)
void ImageComponent::SetImage(Image* image)
{
    this->mImage=image;
    if(image!=NULL)
        this->SetSize(image->mWidth,image->mHeight);
}

// FUNCTION: WON 0x00706c80
// Sets the shown image and the tiling flag and resizes the component to the image
void ImageComponent::SetImageAndTile(Image* image,bool tile)
{
    this->mImage=image;
    this->mTile=tile;
    if(image!=NULL)
        this->SetSize(image->mWidth,image->mHeight);
}

// FUNCTION: WON 0x00706cf0
// Returns the image's size, or 10 x 10 without an image
void ImageComponent::GetDesiredSize(int& width,int& height)
{
    if(this->mImage.get()!=NULL)
    {
        width=this->mImage->mWidth;
        height=this->mImage->mHeight;
    }
    else
        width=height=10;
}

// FUNCTION: WON 0x00706d30
// Draws the image stretched to the component, once at the origin, or tiled over the component clipped to it
void ImageComponent::Paint(Graphics& g)
{
    Component::Paint(g);
    if(this->mImage.get()==NULL)
        return;
    if(this->mStretch)
    {
        if(this->mImage->mWidth!=this->Width() || this->mImage->mHeight!=this->Height())
            this->mImage->SetSize(this->Width(),this->Height());
        g.DrawImage(this->mImage.get(),0,0);
    }
    else if(!this->mTile)
        g.DrawImage(this->mImage.get(),0,0);
    else
    {
        g.PushClipRect(0,0,this->Width(),this->Height());
        for(int x=0;x<this->Width();x+=this->mImage->mWidth)
            for(int y=0;y<this->Height();y+=this->mImage->mHeight)
                g.DrawImage(this->mImage.get(),x,y);
        g.PopClipRect();
    }
}


#include "ee/wonapi/WONLobbySimpleConfig.h"
namespace WONAPI {
RectangleComponent::RectangleComponent()
{
	this->mSolidColor = -1;
	this->mOutlineColor = -1;
}

// FUNCTION: WON 0x00706870
// Sets the rectangle's fill colour (-1 = no fill)
void RectangleComponent::SetSolidColor(int theSolidColor)
{
	this->mSolidColor = theSolidColor;
}

// FUNCTION: WON 0x00706880
// Sets the rectangle's outline colour (-1 = no outline)
void RectangleComponent::SetOutlineColor(int theOutlineColor)
{
	this->mOutlineColor = theOutlineColor;
}

// FUNCTION: WON 0x00706890
// Fills the component with the solid colour and draws the outline, each only when set
void RectangleComponent::Paint(Graphics &g)
{
	Component::Paint(g);
	if(this->mSolidColor!=-1)
	{
		g.SetColor(this->mSolidColor);
		g.FillRect(0,0,this->Width(),this->Height());
	}

	if(this->mOutlineColor!=-1)
	{
		g.SetColor(this->mOutlineColor);
		g.DrawRect(0,0,this->Width(),this->Height());
	}
}
}


namespace WONAPI {
RectangleComponent3D::RectangleComponent3D(bool up, int theThickness)
{
	this->mUp = up;
	this->mThickness = theThickness;

	this->mLightColor = ColorScheme::GetColorRef(StandardColor_3DHilight);
	this->mDarkColor = ColorScheme::GetColorRef(StandardColor_3DShadow);
	this->mSolidColor = -1;
}

// FUNCTION: WON 0x00706930
// Draws a raised or sunken 3D frame of the given thickness (light/dark edges swapped by mUp), over an
// optional solid fill
void RectangleComponent3D::Paint(Graphics &g)
{
	int width = this->Width()-1;
	int height = this->Height()-1;
	bool up = this->mUp;
	if(this->mSolidColor!=COLOR_INVALID)
	{
		g.SetColor(this->mSolidColor);
		g.FillRect(0,0,this->Width(),this->Height());
	}

	for(int i=0; i<this->mThickness; i++)
	{
		if(up)
			g.SetColor(this->mLightColor);
		else
			g.SetColor(this->mDarkColor);

		g.DrawLine(i,i,width-i,i);
		g.DrawLine(i,i,i,height-i);
		if(up)
			g.SetColor(this->mDarkColor);
		else
			g.SetColor(this->mLightColor);

		g.DrawLine(width-i,i,width-i,height);
		g.DrawLine(i,height-i,width,height-i);
	}
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00706ba0
// (public: virtual __thiscall WONAPI::ImageComponent::~ImageComponent(void)    [??1ImageComponent@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00706b80
// (public: virtual void * __thiscall WONAPI::ImageComponent::`scalar deleting destructor'(unsigned int)    [??_GImageComponent@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00706840
// (public: __thiscall WONAPI::RectangleComponent::RectangleComponent(void)    [??0RectangleComponent@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x007068f0
// (public: __thiscall WONAPI::RectangleComponent3D::RectangleComponent3D(bool,int)    [??0RectangleComponent3D@WONAPI@@QAE@_NH@Z])
