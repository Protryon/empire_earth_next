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
#include "ee/wonapi/WONGUIDraw.h"

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
// (guidraw.rs image_component_paint)
void ImageComponent::Paint(Graphics& g)
{
    Component::Paint(g);
    Image* anImage = this->mImage.get();
    WONDrawSlots aSlots;
    aSlots.mImages[0] = anImage;
    WONDrawRun(g,WONDrawOps([&](EeWonDrawOp* o, size_t n) { return ee_won_gui_draw_image_component(anImage!=NULL,this->mStretch,
        this->mTile,anImage ? anImage->mWidth : 0,anImage ? anImage->mHeight : 0,this->Width(),this->Height(),o,n); }),aSlots);
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
// Fills the component with the solid colour and draws the outline, each only when set (guidraw.rs rectangle_paint)
void RectangleComponent::Paint(Graphics &g)
{
	Component::Paint(g);
	WONDrawRun(g,WONDrawOps([&](EeWonDrawOp* o, size_t n) {
		return ee_won_gui_draw_rectangle(this->mSolidColor,this->mOutlineColor,this->Width(),this->Height(),o,n); }),WONDrawSlots());
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
// optional solid fill (guidraw.rs rectangle_3d_paint)
void RectangleComponent3D::Paint(Graphics &g)
{
	static_assert(COLOR_INVALID==0xffffffff,"guidraw.rs rectangle_3d_paint compares with -1");
	WONDrawRun(g,WONDrawOps([&](EeWonDrawOp* o, size_t n) { return ee_won_gui_draw_rectangle_3d(this->mUp,this->mThickness,
		this->mSolidColor,this->mLightColor,this->mDarkColor,this->Width(),this->Height(),o,n); }),WONDrawSlots());
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
