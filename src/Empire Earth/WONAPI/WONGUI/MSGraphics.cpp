// Win32 GDI implementation of the GUI graphics context (pens, brushes, shapes, clipping, alpha fills)
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
#include <math.h>



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007cf0f0
// Creates a graphics context with no device context attached
MSGraphics::MSGraphics()
{
	this->mDC = NULL;
}
	
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007cf130
// Destroys the graphics context, detaching it first
MSGraphics::~MSGraphics()
{
	this->Detach();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007cf180
// Attaches to a display context's DC: creates a hollow brush, a black solid brush and pen, selects them
// (remembering the DC's originals) and turns on transparent text background
void MSGraphics::Attach(DisplayContext *theContext)
{
	Graphics::Attach(theContext);
	this->mDC = theContext->mDC;

	LOGBRUSH aHollowBrush = { BS_NULL,0,0};
	this->mHollowBrush = CreateBrushIndirect(&aHollowBrush);

	this->mColor = 0;
	this->mSolidBrush = CreateSolidBrush(RGB(0,0,0));
	this->mPen = CreatePen(PS_SOLID,0,0);

	this->mOriginalBrush = (HBRUSH)SelectObject(this->mDC,this->mSolidBrush);
	this->mOriginalPen = (HPEN)SelectObject(this->mDC,this->mPen);
	SetBkMode(this->mDC, TRANSPARENT); 	
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007cf210
// Restores the DC's original brush and pen and deletes this context's GDI objects, if attached
void MSGraphics::Detach()
{
	Graphics::Detach();
	if(this->mDC==NULL)
		return;
	SelectObject(this->mDC, this->mOriginalBrush);
	SelectObject(this->mDC, this->mOriginalPen);

	DeleteObject(this->mSolidBrush);
	DeleteObject(this->mHollowBrush);
	DeleteObject(this->mPen);
	this->mDC = NULL;
	this->mDisplayContext = NULL;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007cf290
// Replaces the pen, solid brush and text colour with the current colour, converted from 0xRRGGBB to a
// COLORREF (the top byte is not masked off)
void MSGraphics::SetColorHook()
{
	DWORD aColor = ((this->mColor&0xff)<<16) | (this->mColor&0x00FF00) | (this->mColor>>16);
	HBRUSH aBrush = CreateSolidBrush(aColor);
	HPEN aPen = CreatePen(PS_SOLID,0,aColor);
	SelectObject(this->mDC,aPen);
	SelectObject(this->mDC,aBrush);
	DeleteObject(this->mSolidBrush);
	DeleteObject(this->mPen);
	this->mPen = aPen;
	this->mSolidBrush = aBrush;

	SetTextColor(this->mDC,aColor);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007cf310
// Returns the DC's pixel at the point as a COLORREF (no channel swap back)
DWORD MSGraphics::GetPixelHook(int x, int y)
{
	return ::GetPixel(this->mDC, x, y);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007cf330
// Sets one pixel, converting the colour from 0xRRGGBB to a COLORREF
void MSGraphics::SetPixelHook(int x, int y, DWORD theColor)
{
	DWORD aColor = ((theColor&0xff)<<16) | (theColor&0x00FF00) | (theColor>>16);
	::SetPixel(this->mDC,x,y,aColor);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007cf370
// Draws a line including its end point (a one-pixel extra segment past the end)
void MSGraphics::DrawLineHook(int x1, int y1, int x2, int y2)
{
	POINT points[3] = {x1, y1, x2, y2, x2+1,y2};
	Polyline(this->mDC,points,3);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007cf3c0
// Draws a rectangle outline with the hollow brush
void MSGraphics::DrawRectHook(int x, int y, int width, int height)
{
	SelectObject(this->mDC,this->mHollowBrush);
	Rectangle(this->mDC,x,y,x+width,y+height);
	SelectObject(this->mDC,this->mSolidBrush);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007cf410
// Draws a dotted rectangle with the generic implementation
void MSGraphics::DrawDottedRectHook(int x, int y, int width, int height)
{
    Graphics::DrawDottedRectHook(x,y,width,height);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007cf430
// Fills a rectangle with the current pen and brush
void MSGraphics::FillRectHook(int x, int y, int width, int height)
{
	Rectangle(this->mDC,x,y,x+width,y+height);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

typedef BOOL(WINAPI *AlphaBlendFunc)(HDC,int,int,int,int,HDC,int,int,int,int,BLENDFUNCTION);
static AlphaBlendFunc gAlphaBlendFunc;
struct AutoUnloadLibrary
{
	HMODULE mModule;

	AutoUnloadLibrary() { this->mModule = NULL; }
	~AutoUnloadLibrary() { if(this->mModule!=NULL) FreeLibrary(this->mModule); }

};

// FUNCTION: WON 0x007cf460
// Returns whether AlphaBlend is available, loading msimg32.dll once (freed at exit) to find it
bool MSGraphics::BlendSupported()
{
	static HMODULE aModule = NULL;
	static bool haveLoaded = false;
	static AutoUnloadLibrary anUnload;

	if(!haveLoaded)
	{
		aModule = LoadLibrary("msimg32.dll");
		if(aModule!=NULL)
		{
			anUnload.mModule = aModule;
			gAlphaBlendFunc = (AlphaBlendFunc)GetProcAddress(aModule,"AlphaBlend");
		}

		haveLoaded = true;
	}
	return gAlphaBlendFunc!=NULL;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007cf500
// Blends the current colour over a rectangle at the given opacity, tiling a lazily created 100 x 100
// solid image; does nothing without AlphaBlend
void MSGraphics::FillRectBlendHook(int tx, int ty, int width, int height, int theBlendAmount)
{
	if(!this->BlendSupported())
		return;

	static SmartPtr<NativeImage> anAlphaImage;
	static DWORD *anImageData;
	static int aWidth = 100;
	static int aHeight = 100;
	static int aNumPixels = aWidth*aHeight;
	static int aLastColor = this->mColor+1;
	if(anAlphaImage.get()==NULL)
	{	
		BITMAPINFO anInfo;
		BITMAPINFOHEADER &aHeader = anInfo.bmiHeader;
		aHeader.biSize = sizeof(BITMAPINFOHEADER);
		aHeader.biWidth = aWidth;
		aHeader.biHeight = -aHeight;
		aHeader.biPlanes = 1;
		aHeader.biBitCount = 32;
		aHeader.biCompression = BI_RGB;

		HBITMAP anAlpha = CreateDIBSection(this->mDC,&anInfo,DIB_RGB_COLORS,(void**)&anImageData,NULL,0);
		anAlphaImage = new MSNativeImage(new DisplayContext(this->mDC),anAlpha);
	}

	BLENDFUNCTION aFunc;
	aFunc.AlphaFormat = 0; //AC_SRC_ALPHA;
	aFunc.BlendFlags = 0;
//	aFunc.SourceConstantAlpha = 255;
	aFunc.SourceConstantAlpha = theBlendAmount;
	aFunc.BlendOp = AC_SRC_OVER;
	
	MSGraphics &g = (MSGraphics&)anAlphaImage->GetGraphics();
	g.SetColor(this->mColor);
	g.FillRect(0,0,aWidth,aHeight);


	for(int x=0; x<width; x+=aWidth)
	{
		for(int y=0; y<height; y+=aHeight)
		{
			int useWidth = aWidth, useHeight = aHeight;
			if(x+aWidth > width)
				useWidth = width-x;
			if(y+aHeight > height)
				useHeight = height-y;
	
			gAlphaBlendFunc(this->mDC,x+tx,y+ty,useWidth,useHeight,anAlphaImage->mDisplayContext->mDC,0,0,useWidth,useHeight,aFunc);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007cf790
// Draws a polygon outline with the hollow brush
void MSGraphics::DrawPolygonHook(POINT thePoints[], int nPoints)
{	
	SelectObject(this->mDC,this->mHollowBrush);
	Polygon(this->mDC,thePoints,nPoints);
	SelectObject(this->mDC,this->mSolidBrush);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007cf7d0
// Fills a polygon with the current pen and brush
void MSGraphics::FillPolygonHook(POINT thePoints[], int nPoints)
{
	Polygon(this->mDC,thePoints,nPoints);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007cf7f0
// Draws an ellipse outline with the hollow brush
void MSGraphics::DrawEllipseHook(int x, int y, int width, int height)
{
	SelectObject(this->mDC,this->mHollowBrush);
	Ellipse(this->mDC,x,y,x+width,y+height);
	SelectObject(this->mDC,this->mSolidBrush);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007cf840
// Fills an ellipse with the current pen and brush
void MSGraphics::FillEllipseHook(int x, int y, int width, int height)
{
	Ellipse(this->mDC,x,y,x+width,y+height);
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007cf870
// Draws an elliptical arc from the start angle over the arc angle (degrees, counter-clockwise)
void MSGraphics::DrawArcHook(int x, int y, int width, int height, int startAngle, int arcAngle)
{
	static const double PI = 3.14159;
	static const int MULT = 100;

	SelectObject(this->mDC,this->mHollowBrush);
	int cx = x+width/2;
	int cy = y+height/2;

	Arc(this->mDC,x,y,x+width,y+height,cx+MULT*cos(PI*startAngle/180),cy-MULT*sin(PI*startAngle/180),cx+MULT*cos(PI*(startAngle+arcAngle)/180),cy-MULT*sin(PI*(startAngle+arcAngle)/180));
	SelectObject(this->mDC,this->mSolidBrush);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007cf970
// Removes the DC's clip region
void MSGraphics::ClearClipRectHook()
{
	SelectClipRgn(this->mDC, NULL);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x007cf980
// Narrows the DC's clip region to the rectangle (intersects with the current one)
void MSGraphics::SetClipRectHook(int x, int y, int width, int height)
{
	IntersectClipRect(this->mDC, x, y, x+width, y+height);
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007cf110
// (public: virtual void * __thiscall WONAPI::MSGraphics::`scalar deleting destructor'(unsigned int)    [??_GMSGraphics@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007cf4e0
// TEMPLATE: WON 0x007cf780
// (_$E74)
