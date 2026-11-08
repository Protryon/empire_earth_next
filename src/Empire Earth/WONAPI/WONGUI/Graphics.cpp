// Graphics: translated drawing, colours, fonts and the clip stack
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
using namespace WONAPI;
// FUNCTION: WON 0x00726ad0
// Creates a graphics context with no translation and no colour scheme
Graphics::Graphics()
{
	this->mTransX = this->mTransY = 0;
	this->mColorScheme = NULL;
}

// FUNCTION: WON 0x00726b90
// Destroys the graphics context, detaching it first
Graphics::~Graphics()
{
	this->Detach();
}

void Graphics::Detach()
{
}

// FUNCTION: WON 0x00726c70
// Attaches the graphics context to a display context and links the display context back to it
void Graphics::Attach(DisplayContext *theContext)
{
	this->Detach();
	this->mDisplayContext = theContext;
	this->mDisplayContext->mGraphics = this;
}

// FUNCTION: WON 0x00726ce0
// Sets the drawing colour, resolving colour-scheme references (0x80000000 + 0..16) through the
// scheme; calls the colour hook only when the colour changes
void Graphics::SetColor(DWORD theColor)
{
	if(theColor>=0x80000000 && theColor<0x80000000+17 && this->mColorScheme)
	{
		// Do it this way instead of calling ApplyColorSchemeColor in order to avoid an infinite loop
		// If a colorscheme color is a reference to itself
		DWORD aColor = this->mColorScheme->GetStandardColor((int)(theColor-0x80000000));
		if(aColor!=this->mColor)
		{
			this->mColor = aColor;
			this->SetColorHook();
		}
	}
	else if(this->mColor!=theColor)
	{
		this->mColor = theColor;
		this->SetColorHook();
	}
}

// FUNCTION: WON 0x00726d20
// Returns a colour-scheme colour, or 0 without a scheme
DWORD Graphics::GetStandardColor(int theColorRef)
{
	if(this->mColorScheme)
		return this->mColorScheme->GetStandardColor(theColorRef);
	else
		return 0;
}

// FUNCTION: WON 0x00726d40
// Sets the drawing colour to a colour-scheme colour; does nothing without a scheme
void Graphics::SetSysColor(int theColorRef)
{
	if(this->mColorScheme)
		this->SetColor(this->mColorScheme->GetStandardColor(theColorRef));
}

// FUNCTION: WON 0x00726d60
// Moves the drawing origin by the given offset
void Graphics::Translate(int dx, int dy)
{
	this->mTransX += dx;
	this->mTransY += dy;
}

// FUNCTION: WON 0x00726d80
// Applies the drawing origin to a point
void Graphics::DoTranslate(int &x, int &y)
{
	x+=this->mTransX;
	y+=this->mTransY;
}

// FUNCTION: WON 0x00726da0
// Sets one pixel at a point relative to the drawing origin
void Graphics::SetPixel(int x, int y, DWORD color)
{
    this->DoTranslate(x,y);
    this->SetPixelHook(x,y,color);
}

// FUNCTION: WON 0x00726dd0
// Draws a whole image at a point relative to the drawing origin
void Graphics::DrawImage(Image *theImage, int x, int y)
{
	this->DoTranslate(x,y);
	theImage->DrawEntire(this->mDisplayContext.get(),x,y);
}

// FUNCTION: WON 0x00726e00
// Draws part of an image at a point relative to the drawing origin
void Graphics::DrawImage(Image *theImage, int x, int y, int left, int top, int width, int height)
{
	this->DoTranslate(x,y);
	theImage->Draw(this->mDisplayContext.get(), x, y, left, top, width, height);
}

// FUNCTION: WON 0x00726e50
// Draws a line between two points relative to the drawing origin
void Graphics::DrawLine(int x1, int y1, int x2, int y2)
{

	this->DoTranslate(x1,y1);
	this->DoTranslate(x2,y2);

	this->DrawLineHook(x1,y1,x2,y2);	
}

// FUNCTION: WON 0x00726ea0
// Draws a rectangle outline relative to the drawing origin
void Graphics::DrawRect(int x, int y, int width, int height)
{
	this->DoTranslate(x,y);
	this->DrawRectHook(x,y,width,height);
}

// FUNCTION: WON 0x00726ee0
// Fills a rectangle relative to the drawing origin
void Graphics::FillRect(int x, int y, int width, int height)
{
	this->DoTranslate(x,y);
	this->FillRectHook(x,y,width,height);
}

// FUNCTION: WON 0x00726f20
// Fills a rectangle blended by the given amount, relative to the drawing origin
void Graphics::FillRectBlend(int x, int y, int width, int height, int theBlendAmount)
{
	this->DoTranslate(x,y);
	this->FillRectBlendHook(x,y,width,height,theBlendAmount);
}

// FUNCTION: WON 0x00726f60
// Draws a dotted rectangle outline relative to the drawing origin
void Graphics::DrawDottedRect(int x, int y, int width, int height)
{
	this->DoTranslate(x,y);
	this->DrawDottedRectHook(x,y,width,height);
}

// FUNCTION: WON 0x00726fa0
// Draws an ellipse outline relative to the drawing origin
void Graphics::DrawEllipse(int x, int y, int width, int height)
{
	this->DoTranslate(x,y);
	this->DrawEllipseHook(x,y,width,height);
}

// FUNCTION: WON 0x00726fe0
// Fills an ellipse relative to the drawing origin
void Graphics::FillEllipse(int x, int y, int width, int height)
{
	this->DoTranslate(x,y);
	this->FillEllipseHook(x,y,width,height);
}

// FUNCTION: WON 0x00727020
// Draws an arc relative to the drawing origin
void Graphics::DrawArc(int x, int y, int width, int height, int startAngle, int arcAngle)
{
	this->DoTranslate(x,y);
	this->DrawArcHook(x,y,width,height,startAngle,arcAngle);
}

// FUNCTION: WON 0x00727060
// Selects a font into the display context when it differs from the current one
void Graphics::SetFont(Font *theFont)
{
	if(theFont!=this->mFont.get())
	{
		this->mFont = theFont;
		this->mFont->PrepareContext(this->mDisplayContext.get());
	}
}

// FUNCTION: WON 0x007270c0
// Draws a whole string at a point relative to the drawing origin
void Graphics::DrawString(const GUIString &theStr, int x, int y)
{
	this->DoTranslate(x,y);
	this->mFont->DrawString(this->mDisplayContext.get(),theStr,0,theStr.length(),x, y);
}

// FUNCTION: WON 0x00727100
// Draws part of a string at a point relative to the drawing origin
void Graphics::DrawString(const GUIString &theStr, unsigned short theOffset, unsigned short theLength, int x, int y)
{
	this->DoTranslate(x,y);
	this->mFont->DrawString(this->mDisplayContext.get(),theStr,theOffset,theLength,x, y);
}

// FUNCTION: WON 0x00727140
// Recomputes the clip area as the intersection of every rectangle on the clip stack; with an empty
// stack clipping is off
void Graphics::PerformClip()
{
	this->ClearClipRectHook();
	this->mClipRect.x = this->mClipRect.y = -10000000;
	this->mClipRect.width = this->mClipRect.height = 20000001;
	if(this->mClipStack.empty())
		return;

	WONRectangle aRect = this->mClipStack.front();
	ClipStack::iterator anItr = this->mClipStack.begin();
	++anItr;

	while(anItr!=this->mClipStack.end())
	{
		aRect.DoIntersect(*anItr);
		++anItr;
	}

	this->SetClipRectHook(aRect.x, aRect.y, aRect.width, aRect.height);
	this->mClipRect = aRect;
}

// FUNCTION: WON 0x00727200
// Pushes a clip rectangle (relative to the drawing origin) and narrows the clip area to it
void Graphics::PushClipRect(int x, int y, int width, int height)
{
	this->DoTranslate(x,y);
	WONRectangle aRect(x , y, width, height);
	this->mClipStack.push_back(aRect);
	this->PerformClip();
}

// FUNCTION: WON 0x007272b0
// Pops the last clip rectangle, if any, and recomputes the clip area
void Graphics::PopClipRect()
{
	if(!this->mClipStack.empty())
		this->mClipStack.pop_back();

	this->PerformClip();

}

// FUNCTION: WON 0x00727300
// Draws a rectangle outline as four lines, the far edges at x+width and y+height
void Graphics::DrawRectHook(int x, int y, int width, int height)
{
	this->DrawLineHook(x,y,x+width,y);
	this->DrawLineHook(x+width,y,x+width,y+height);
	this->DrawLineHook(x+width,y+height,x,y+height);
	this->DrawLineHook(x,y+height,x,y);
}

// FUNCTION: WON 0x00727360
// Draws a rectangle outline with every other pixel set in the current colour
void Graphics::DrawDottedRectHook(int x, int y, int width, int height)
{
//	DrawRectHook(x,y,width,height);
	int i;
	for(i=0; i<width; i+=2)
	{
		this->SetPixelHook(x+i,y,this->mColor);
		this->SetPixelHook(x+i,y+height-1,this->mColor);
	}

	for(i=0; i<height; i+=2)
	{
		this->SetPixelHook(x,y+i,this->mColor);
		this->SetPixelHook(x+width-1,y+i,this->mColor);
	}
}

// FUNCTION: WON 0x007273f0
// Fills a rectangle with one horizontal line per row
void Graphics::FillRectHook(int x, int y, int width, int height)
{
	for(int i=0; i<height; i++)
		this->DrawLineHook(x,y+i,x+width,y+i);
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00726b70
// (public: virtual void * __thiscall WONAPI::Graphics::`scalar deleting destructor'(unsigned int)    [??_GGraphics@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00726b50
// (public: virtual unsigned long __thiscall WONAPI::Graphics::GetPixelHook(int,int))
// TEMPLATE: WON 0x00727430
// (public: virtual void __thiscall WONAPI::Graphics::FillRectBlendHook(int,int,int,int,int))
// TEMPLATE: WON 0x00727450
// (public: virtual void __thiscall WONAPI::Graphics::DrawArcHook(int,int,int,int,int,int))
// TEMPLATE: WON 0x00731910
// (protected: void __thiscall std::list<class WONAPI::WONRectangle,class std::allocator<class WONAPI::WONRectangle> >::_Incsize(unsigned int))
