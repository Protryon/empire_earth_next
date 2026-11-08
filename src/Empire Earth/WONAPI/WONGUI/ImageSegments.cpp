// Rich text image selection, link state and image-map hit testing.
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
#include "ee/wonapi/WONLobbyImageSegment.h"
namespace WONAPI {

SmartPtr<Image> LineSegment::mSelectImageMask;

// FUNCTION: WON 0x00709570
// Fills the segment's line area with the highlight colour when the whole segment is selected; returns
// whether it is selected
bool LineSegment::DrawSelection1(Graphics &g, int x, int y, ElementPaintInfo &theInfo)
{
	if(this->mStartSelIndex<theInfo.mStartSelSegment || this->mStartSelIndex>theInfo.mEndSelSegment)
		return false;

	if(this->mStartSelIndex==theInfo.mStartSelSegment && theInfo.mStartSelSegmentPos>0)
		return false;

	if(this->mStartSelIndex==theInfo.mEndSelSegment && theInfo.mEndSelSegmentPos<1)
		return false;

	g.SetSysColor(StandardColor_Hilight);
	g.FillRect(x,theInfo.mLineY,this->GetWidth(),theInfo.mLineHeight);
	return true;
}

// FUNCTION: WON 0x007095E0
// Overlays a checkerboard selection mask on the segment (the mask image is built once and shared)
void LineSegment::DrawSelection2(Graphics &g, int x, int y)
{
	const int MASK_WIDTH = 100;
	const int MASK_HEIGHT = 100;
	if(LineSegment::mSelectImageMask.get()==NULL)
	{
		SmartPtr<Palette> aPalette = new Palette(2);
		aPalette->SetColor(0,0x000000);
		aPalette->SetColor(1,0x000080);
		SmartPtr<RawImage8> anImage = new RawImage8(MASK_WIDTH,MASK_HEIGHT,aPalette.get());
		anImage->mTransparentColor=0;

		for(int i=0; i<MASK_WIDTH; i++)
		{
			int pixel = i%2;
			anImage->StartRowTraversal(i);
			for(int j=0; j<MASK_HEIGHT; j++)
			{
				anImage->SetRowPixel(pixel);
				anImage->NextRowPixel();
				pixel = 1-pixel;
			}
		}
		LineSegment::mSelectImageMask = anImage->GetNative(g.mDisplayContext.get()).get();
	}

	int aWidth = this->GetWidth();
	int aHeight = this->GetHeight();
	g.PushClipRect(x,y,aWidth,aHeight);
	for(int i=0; i<aWidth; i+=MASK_WIDTH)
		for(int j=0; j<aHeight; j+=MASK_HEIGHT)
			g.DrawImage(LineSegment::mSelectImageMask.get(),i+x,j+y);

	g.PopClipRect();
}

// FUNCTION: WON 0x0070B390
// Draws the image segment: selection background, border rectangles, the image, then the selection mask
void ImageLineSegment::Paint(Graphics &g, int x, int y, ElementPaintInfo &theInfo)
{
	bool selected = this->DrawSelection1(g,x,y,theInfo);
	if(this->mBorder)
	{
		int aWidth = this->mImage->mWidth+2*this->mBorder;
		int aHeight = this->mImage->mHeight+2*this->mBorder;
		g.SetColor(this->mBorderColor);
		for(int i=0; i<this->mBorder; i++)
			g.DrawRect(x+this->mHSpace+i,y+this->mVSpace+i,aWidth-2*i,aHeight-2*i);
	}

	g.DrawImage(this->mImage.get(),x+this->mHSpace+this->mBorder,y+this->mVSpace+this->mBorder);
	if(selected)
		this->DrawSelection2(g,x,y);
}

// FUNCTION: WON 0x0070AB50
// Marks the link as pressed and shows every segment of the link pressed
void TextLinkParam::LinkDown()
{
	this->mLinkIsDown = true;
	for(std::list<LineSegment*>::iterator anItr = this->mLinkList.begin(); anItr!=this->mLinkList.end(); ++anItr)
		(*anItr)->LinkDown();
}

// FUNCTION: WON 0x0070AB80
// Releases a pressed link and its segments; returns whether it was pressed
bool TextLinkParam::LinkUp()
{
	if(this->mLinkIsDown)
	{
		this->mLinkIsDown = false;
		for(std::list<LineSegment*>::iterator anItr = this->mLinkList.begin(); anItr!=this->mLinkList.end(); ++anItr)
			(*anItr)->LinkUp();

		return true;
	}
	else
		return false;
}

// FUNCTION: WON 0x0070AED0
// Returns the link of the first image-map rectangle containing the point; NULL if none
TextLinkParam* ImageMap::GetLink(int x, int y)
{
	std::list<ShapeLink>::iterator anItr = this->mShapeList.begin();
	if(anItr==this->mShapeList.end())
		return NULL;
	while(true)
	{
		ShapeLink &aLink = *anItr;
		if(x>=aLink.mShape.x && x<aLink.mShape.x+aLink.mShape.width && y>=aLink.mShape.y && y<aLink.mShape.y+aLink.mShape.height)
			return aLink.mLinkParam.get();
		++anItr;
		if(anItr==this->mShapeList.end())
			break;
	}
	
	return NULL;
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00709860
// (public: virtual void * __thiscall WONAPI::Palette::`scalar deleting destructor'(unsigned int)    [??_GPalette@WONAPI@@UAEPAXI@Z])
// SYNTHETIC: WON 0x7094d0
// SYNTHETIC: WON 0x7094e0
// SYNTHETIC: WON 0x7094f0
// SYNTHETIC: WON 0x709500
