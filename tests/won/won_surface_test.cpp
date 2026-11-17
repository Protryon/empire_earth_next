// The software raster surface as a whole Rust object (R6 WON round 37): the WON SDK's WONSurface.h and Surface.cpp,
// copied unmodified from the C++ before the port (their includes left out; a RefCount and SmartPtr stand in for
// EEWonNet.h's), against ee_won's Rust surface (surface.rs) called as the C++ shell (WONSurface.h) calls it.
//
// Each of 20,000 sequences makes three surface slots of random formats (32-bit, 555, 565) and sizes (0..40, now and
// then 1 or 64), clears the original's pixels as the port's only creator does (PlatNativeImage's NewSurface; the
// Rust's are 0), and runs 60 random operations on both: colours, clip rectangles (set, cleared), filled and framed
// rectangles, horizontal, vertical and free lines (inside, across and outside the clip), circles and filled circles,
// ellipses and filled ellipses, arcs (start -400..800, arc -50..500), pixels put and set, pixels read, copies from
// another slot of the same format or a 32-bit into a 16-bit one (with and without a transparency mask, the source
// rectangle inside the source as the original needs), copies within one surface, duplicates, re-creates and frees.
// After every operation each slot's fields (size, pitch, clip, colour, whether it has pixels) and every pixel byte
// are compared. The first 150 sequences are rust/crates/ee_won_gui/src/golden_surface.txt (each operation with the
// FNV-1a hash of its slot's pixels after it), which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <string>
#include <vector>
#include <algorithm>
#include "rust/ee_won.h"

typedef uint32_t DWORD;
namespace WONAPI {
class RefCount {
public:
    RefCount() : mRefCount(0) {}
    virtual ~RefCount() {}
    void AddRef() { mRefCount++; }
    void Release() { if (--mRefCount <= 0) delete this; }
    int mRefCount;
};
template <class T> class SmartPtr {
public:
    SmartPtr(T* p = NULL) : mPtr(p) { if (mPtr) mPtr->AddRef(); }
    SmartPtr(const SmartPtr& o) : mPtr(o.mPtr) { if (mPtr) mPtr->AddRef(); }
    template <class U> SmartPtr(const SmartPtr<U>& o) : mPtr(o.get()) { if (mPtr) mPtr->AddRef(); }
    ~SmartPtr() { if (mPtr) mPtr->Release(); }
    SmartPtr& operator=(T* p) { if (p) p->AddRef(); if (mPtr) mPtr->Release(); mPtr = p; return *this; }
    SmartPtr& operator=(const SmartPtr& o) { return *this = o.mPtr; }
    T* get() const { return mPtr; }
    T* operator->() const { return mPtr; }
    T* mPtr;
};
}

// ---- WONSurface.h (the original, its includes left out)
// Software surface classes (optional; not identified in the original exe).
// The SDK's WIN32_NOT_XBOX conditional is kept.
//
namespace WONAPI
{
	
//class SFont;
//class SNativeMask;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
enum SurfaceFormat
{
	SurfaceFormat_32,
	SurfaceFormat_555,
	SurfaceFormat_565
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class SurfaceBase;
typedef SmartPtr<SurfaceBase> SurfaceBasePtr;
class SurfaceBase : public RefCount
{
protected:
	SurfaceFormat mFormat;
	void *mImageBase;
	int mWidth, mHeight;
	int mBytesPerRow;
	int mPixelsPerRow;

#ifndef WIN32_NOT_XBOX
	struct BITMAPINFO
	{
		DWORD unused;
	};
#endif

	char mBitmapInfoBuffer[sizeof(BITMAPINFO) + sizeof(DWORD)*2];
	BITMAPINFO &mBitmapInfo;
	int mClipX1, mClipY1, mClipX2, mClipY2;

	inline void DoClipRect(int &x, int &y, int &width, int &height);
	inline void DoClipRect(int &x, int &y, int &left, int &top, int &width, int &height);

	inline int CalcClipCode(int x, int y);

	SurfaceBase& operator=(const SurfaceBase&) { assert(false); return *this; }
	virtual ~SurfaceBase();

public:
	virtual void CopySurface(SurfaceBase *theSurface, int x, int y, int left, int top, int width, int height)=0;
	virtual void CopySurface(SurfaceBase *theSurface, const char *theMaskData, int x, int y, int left, int top, int width, int height)=0;
//	virtual void DrawString(SFont *theFont, const GUIString &theStr, unsigned short theOffset, unsigned short theLength, int x, int y)=0;

	virtual DWORD GetPixel(int x, int y) = 0;
	virtual void PutPixel(int x, int y) = 0;
	virtual void SetPixel(int x, int y, DWORD theColor) = 0;
	virtual void DrawCircle(int x, int y, int r) = 0;
	virtual void FillCircle(int x, int y, int r) = 0;
	virtual void DrawEllipse(int x, int y, int width, int height) = 0;
	virtual void FillEllipse(int x, int y, int width, int height) = 0;
	virtual void DrawArc(int x, int y, int width, int height, int startAngle, int arcAngle) = 0;

	virtual void DrawHorzLine(int x1, int x2, int y) = 0;
	virtual void DrawVertLine(int x, int y1, int y2) = 0;
	virtual void DrawLine(int x1, int y1, int x2, int y2) = 0;

	virtual void DrawRect(int x, int y, int width, int height) = 0;
	virtual void FillRect(int x, int y, int width, int height) = 0;
	virtual void SetColor(DWORD theColor) = 0;

public:
	SurfaceBase();

	virtual void Create(int theWidth, int theHeight);
	virtual void Free();
	virtual int GetPixelSize()=0;
	SurfaceFormat GetSurfaceFormat() { return this->mFormat; }
	int GetBytesPerRow() { return this->mBytesPerRow; }
	int GetPixelsPerRow() { return this->mPixelsPerRow; }

#ifdef WIN32_NOT_XBOX
	void BlitToDC(HDC theDC, int x, int y, int left, int top, int width, int height);
	void BlitToDC(HDC theDC);
#endif

	int GetWidth() { return this->mWidth; }
	int GetHeight() { return this->mHeight; }

	void SetClipRect(int x, int y, int width, int height);
	void ClearClipRect();

	void* GetPixelsBase() { return this->mImageBase; }

	virtual SurfaceBasePtr Duplicate()=0;

	static void SetDefaultSurfaceFormat(SurfaceFormat theFormat);
	static SurfaceBasePtr GetNewSurface(SurfaceFormat theFormat);
	static SurfaceBasePtr GetNewDefaultSurface();

	int GetClipX1() { return this->mClipX1; }
	int GetClipX2() { return this->mClipX2; }
	int GetClipY1() { return this->mClipY1; }
	int GetClipY2() { return this->mClipY2; }
	void GetClipRect(int &cx1, int &cy1, int &cx2, int &cy2) { cx1 = this->mClipX1; cx2 = this->mClipX2; cy1 = this->mClipY1; cy2 = this->mClipY2; }
	inline bool NotClipped(int x, int y) { 	return x>=this->mClipX1 && y>=this->mClipY1 && x<=this->mClipX2 && y<=this->mClipY2; }
};
typedef SmartPtr<SurfaceBase> SurfaceBasePtr;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
class Surface : public SurfaceBase
{
protected:
//	typedef unsigned short PixelType;
//	typedef DWORD PixelType;

	PixelType *mImage;
	PixelType mColor;
	
	void DrawLineClip(int x1, int y1, int x2, int y2);
	virtual ~Surface();

public:
	Surface(SurfaceFormat theFormat);

	virtual void Create(int theWidth, int theHeight);
	virtual void Free();
	virtual int GetPixelSize() { return sizeof(PixelType); }
	
	virtual void CopySurface(SurfaceBase *theSurface, int x, int y, int left, int top, int width, int height);
	virtual void CopySurface(SurfaceBase *theSurface, const char *theMaskData, int x, int y, int left, int top, int width, int height);
//	virtual void DrawString(SFont *theFont, const GUIString &theStr, unsigned short theOffset, unsigned short theLength, int x, int y);

	DWORD GetPixel(int x, int y);
	void PutPixel(int x, int y);
	void SetPixel(int x, int y, DWORD theColor);
	void DrawCircle(int x, int y, int r);
	void FillCircle(int x, int y, int r);
	void DrawEllipse(int x, int y, int width, int height);
	void FillEllipse(int x, int y, int width, int height);
	void DrawArc(int x, int y, int width, int height, int startAngle, int arcAngle);

	void DrawHorzLine(int x1, int x2, int y);
	void DrawVertLine(int x, int y1, int y2);
	void DrawLine(int x1, int y1, int x2, int y2);

	PixelType* GetPixels() { return this->mImage; }
	PixelType GetColor() { return this->mColor; }

	void DrawRect(int x, int y, int width, int height);
	void FillRect(int x, int y, int width, int height);
	void SetColor(DWORD theColor);

	inline void PutPixel(int x, int y, PixelType *thePtr)
	{
		if(this->NotClipped(x,y))
			*thePtr = this->mColor;
	}

	SurfaceBasePtr Duplicate();
};
typedef Surface<unsigned short> Surface16;
typedef Surface<DWORD> Surface32;

typedef SmartPtr<Surface16> Surface16Ptr;
typedef SmartPtr<Surface32> Surface32Ptr;

#define SURFACE_32_TO_555(x) ((((x)>>3)&0x1f) | ((((x)>>11)&0x1f)<<5) | ((((x)>>19)&0x1f)<<10))
#define SURFACE_555_TO_32(x) ((((x)&0x1f)<<3) | ((((x)>>5)&0x1f)<<11) | ((((x)>>10)&0x1f)<<19))

#define SURFACE_32_TO_565(x) ((((x)>>3)&0x1f) | ((((x)>>10)&0x3f)<<5) | ((((x)>>19)&0x1f)<<11))
#define SURFACE_565_TO_32(x) ((((x)&0x1f)<<3) | ((((x)>>5)&0x3f)<<10) | ((((x)>>11)&0x1f)<<19))


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
class Surface32 : public Surface<DWORD>
{
public:
	Surface32();
};
typedef SmartPtr<Surface32> Surface32Ptr;
*/

} // namespace WONAPI;


// ---- Surface.cpp (the original, its includes left out)
// Software raster surface: pixel, line, circle, rectangle and image drawing into a pixel buffer
// (not identified in the original exe)



using namespace WONAPI;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SurfaceBase::SurfaceBase() : mBitmapInfo(*(BITMAPINFO*)mBitmapInfoBuffer)
{
	this->mImageBase = NULL;
	this->mWidth = this->mHeight = 0;
	this->ClearClipRect();

	memset(&this->mBitmapInfo,0,sizeof(this->mBitmapInfo));

#ifdef WIN32_NOT_XBOX
	BITMAPINFOHEADER &aHeader = this->mBitmapInfo.bmiHeader;
	aHeader.biSize = sizeof(aHeader);
#endif
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SurfaceBase::~SurfaceBase()
{
	this->Free();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static SurfaceFormat gDefaultSurfaceFormat = SurfaceFormat_32;
void SurfaceBase::SetDefaultSurfaceFormat(SurfaceFormat theFormat)
{
	gDefaultSurfaceFormat = theFormat;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SurfaceBasePtr SurfaceBase::GetNewSurface(SurfaceFormat theFormat)
{
	switch(theFormat)
	{
		case SurfaceFormat_32: return new Surface32(theFormat); break;
		case SurfaceFormat_555: return new Surface16(theFormat); break;
		case SurfaceFormat_565: return new Surface16(theFormat); break;
		default: return NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SurfaceBasePtr SurfaceBase::GetNewDefaultSurface()
{
	return SurfaceBase::GetNewSurface(gDefaultSurfaceFormat);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SurfaceBase::Create(int theWidth, int theHeight)
{
	this->Free();

#ifdef WIN32_NOT_XBOX
	this->mBitmapInfo.bmiHeader.biWidth = theWidth;
	this->mBitmapInfo.bmiHeader.biHeight = -theHeight;
#endif

	this->mBytesPerRow = theWidth*this->GetPixelSize();
	if(this->mBytesPerRow%4!=0)
		this->mBytesPerRow += 4-this->mBytesPerRow%4;

	this->mPixelsPerRow = this->mBytesPerRow/this->GetPixelSize();

	this->mImageBase = new char[this->mBytesPerRow*theHeight];

	this->mWidth = theWidth;
	this->mHeight = theHeight;

	this->ClearClipRect();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SurfaceBase::Free()
{
	delete [] (char*)this->mImageBase;
	this->mImageBase = NULL;
	this->mWidth = this->mHeight = 0;
	this->mBytesPerRow = 0;
	this->ClearClipRect();
}

#ifdef WIN32_NOT_XBOX
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SurfaceBase::BlitToDC(HDC theDC)
{
	SetDIBitsToDevice(theDC,0,0,this->mWidth,this->mHeight,0,0,0,this->mHeight,this->mImageBase,&this->mBitmapInfo,DIB_RGB_COLORS);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SurfaceBase::BlitToDC(HDC theDC, int x, int y, int left, int top, int width, int height)
{
	SetDIBitsToDevice(theDC,x,y,width,height,left,top,top,height,this->mImageBase,&this->mBitmapInfo,DIB_RGB_COLORS);
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SurfaceBase::SetClipRect(int x, int y, int width, int height)
{
	this->mClipX1 = x;
	this->mClipY1 = y;
	this->mClipX2 = x+width-1;
	this->mClipY2 = y+height-1;

	if(this->mClipX1<0)
		this->mClipX1 = 0;
	if(this->mClipY1<0)
		this->mClipY1 = 0;
	if(this->mClipX2>=this->mWidth)
		this->mClipX2 = this->mWidth-1;
	if(this->mClipY2>=this->mHeight)
		this->mClipY2 = this->mHeight-1;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SurfaceBase::ClearClipRect()
{
	this->mClipX1 = this->mClipY1 = 0;
	this->mClipX2 = this->mWidth-1;
	this->mClipY2 = this->mHeight-1;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SurfaceBase::DoClipRect(int &x, int &y, int &width, int &height)
{
	int x2 = x+width-1, y2 = y+height-1;

	if(x<this->mClipX1) x = this->mClipX1; 
	if(x2>this->mClipX2) x2 = this->mClipX2;
	if(y<this->mClipY1) y = this->mClipY1; 
	if(y2>this->mClipY2) y2 = this->mClipY2;

	width = x2-x+1;
	height = y2-y+1;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SurfaceBase::DoClipRect(int &x, int &y, int &left, int &top, int &width, int &height)
{
	int x2 = x+width-1, y2 = y+height-1;

	if(x<this->mClipX1) { left += this->mClipX1-x; x = this->mClipX1; }
	if(x2>this->mClipX2) x2 = this->mClipX2;
	if(y<this->mClipY1) { top += this->mClipY1-y; y = this->mClipY1; }
	if(y2>this->mClipY2) y2 = this->mClipY2;

	width = x2-x+1;
	height = y2-y+1;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
inline int SurfaceBase::CalcClipCode(int x, int y)
{
	int aCode = 0;
	if(x<this->mClipX1)
		aCode |= 1;
	if(y<this->mClipY1)
		aCode |= 2;
	if(x>this->mClipX2)
		aCode |= 4;
	if(y>this->mClipY2)
		aCode |= 8;

	return aCode;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
Surface<PixelType>::Surface(SurfaceFormat theFormat)
{
	this->mImage = NULL;
	this->mColor = 0;
	this->mFormat = theFormat;

#ifdef WIN32_NOT_XBOX
	BITMAPINFOHEADER &aHeader = this->mBitmapInfo.bmiHeader;
	aHeader.biPlanes = 1;

	switch(this->mFormat)
	{
		case SurfaceFormat_555:
		case SurfaceFormat_565:
			aHeader.biBitCount = 16; 
			if(this->mFormat==SurfaceFormat_565)
				aHeader.biCompression = BI_BITFIELDS;
			else
				aHeader.biCompression = BI_RGB;

			*(DWORD*)&this->mBitmapInfo.bmiColors[0] = 0xf800;
			*(DWORD*)&this->mBitmapInfo.bmiColors[1] = 0x7e0;
			*(DWORD*)&this->mBitmapInfo.bmiColors[2] = 0x1f;
			break;
		
		case SurfaceFormat_32: 
			aHeader.biBitCount = 32; 
			aHeader.biCompression = BI_RGB;
			this->mFormat = SurfaceFormat_32; 
			break;
	}
#endif

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
Surface<PixelType>::~Surface()
{
	this->Free();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
void Surface<PixelType>::Create(int theWidth, int theHeight)
{
	SurfaceBase::Create(theWidth, theHeight);
	this->mImage = (PixelType*)this->mImageBase;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
void Surface<PixelType>::Free()
{
	SurfaceBase::Free();
	this->mImage = NULL;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
SurfaceBasePtr Surface<PixelType>::Duplicate()
{
	SmartPtr<Surface<PixelType> > aSurface = new Surface<PixelType>(this->mFormat);
	aSurface->Create(this->mWidth,this->mHeight);
	memcpy(aSurface->mImage,this->mImage,this->mBytesPerRow*this->mHeight);
	return aSurface.get();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
void Surface<PixelType>::CopySurface(SurfaceBase *theSurface, int x, int y, int left, int top, int width, int height)
{
	this->DoClipRect(x,y,left,top,width,height);
	if(width<=0 || height<=0)
		return;

	int srcPixelsPerRow = theSurface->GetPixelsPerRow();
	PixelType *srcRow = ((PixelType*)theSurface->GetPixelsBase()) + srcPixelsPerRow*top + left;
	PixelType *destRow = this->mImage + this->mPixelsPerRow*y + x;
	PixelType *src, *dest;

	for(int i=0; i<height; i++)
	{
		src = srcRow;
		dest = destRow;
		for(int j=0; j<width; j++)
			*dest++ = *src++;

		srcRow += srcPixelsPerRow;
		destRow += this->mPixelsPerRow;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
void Surface<PixelType>::CopySurface(SurfaceBase *theSurface, const char *theMaskData, int x, int y, int left, int top, int width, int height)
{
	this->DoClipRect(x,y,left,top,width,height);
	if(width<=0 || height<=0)
		return;

	int srcWidth = theSurface->GetWidth();
	int srcPixlesPerRow = theSurface->GetPixelsPerRow();
	PixelType *srcRow = ((PixelType*)theSurface->GetPixelsBase()) + srcPixlesPerRow*top + left;
	PixelType *destRow = this->mImage + this->mPixelsPerRow*y + x;

	int aMaskBytesPerRow = srcWidth/8;
	if(srcWidth%8!=0)
		aMaskBytesPerRow++;

	int aMaskStartVal = 1<<(7-(left%8));
	unsigned char aMask;
	const unsigned char *aMaskRow = reinterpret_cast<const unsigned char*>(theMaskData) + aMaskBytesPerRow*top + left/8;

	PixelType *src, *dest;
	const unsigned char *aMaskPtr;

	for(int i=0; i<height; i++)
	{
		src = srcRow;
		dest = destRow;
		aMaskPtr = aMaskRow;
		aMask = aMaskStartVal;

		for(int j=0; j<width; j++)
		{
			if((*aMaskPtr) & aMask)
			{
				dest++;
				src++;
			}
			else
				*dest++ = *src++;

			aMask>>=1;
			if(aMask==0)
			{
				aMask = 128;
				aMaskPtr++;
			}
		}

		srcRow += srcPixlesPerRow;
		destRow += this->mPixelsPerRow;
		aMaskRow += aMaskBytesPerRow;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
template <class PixelType>
void Surface<PixelType>::DrawString(SFont *theFont, const GUIString &theStr, unsigned short theOffset, unsigned short theLength, int x, int y)
{
	if(x>mClipX2 || y>mClipY2 || y+theFont->GetHeight()<=mClipY1)
		return;

	ReadBuffer aBuf;
	
	int aHeight = theFont->GetHeight();
	PixelType *anUpperLeftCorner = mImage + y*mPixelsPerRow + x;
	PixelType *aRow, *aPixel;
	for(int i=0; i<theLength; i++)
	{
		int aChar = theStr.at(theOffset+i);
		int aWidth;
		if(x>=mClipX1)
		{
			aBuf.SetData(theFont->GetCharData(aChar, &aWidth),1000000);

			aRow = anUpperLeftCorner; 
			for(int i=0; i<aHeight; i++)
			{
				aPixel = aRow;
				for(int j=0; j<aWidth; j++)
				{
					if(aBuf.ReadBit())
						PutPixel(x+j,y+i,aPixel);

					aPixel++;
				}
				aRow+=mPixelsPerRow;
			}
		}
		else
			aWidth=theFont->GetCharWidth(aChar);

		x+=aWidth;
		anUpperLeftCorner+=aWidth;

		if(x>mClipX2)
			return;
	}
}
*/

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static inline DWORD SurfaceColorFrom32(DWORD theColor, SurfaceFormat theFormat)
{
	switch(theFormat)
	{
		case SurfaceFormat_555: return SURFACE_32_TO_555(theColor); break;
		case SurfaceFormat_565: return SURFACE_32_TO_565(theColor); break;

		case SurfaceFormat_32:	
		default: 
			return theColor; 
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
void Surface<PixelType>::SetColor(DWORD theColor)
{
	this->mColor = SurfaceColorFrom32(theColor,this->mFormat);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
void Surface<PixelType>::FillRect(int x, int y, int width, int height)
{
	this->DoClipRect(x,y,width,height);
	if(width<=0 || height<=0)
		return;

	int i,j;
	PixelType *aRow = this->mImage + y*this->mPixelsPerRow + x;
	PixelType *aPtr;
	for(i=0; i<height; i++)
	{
		aPtr = aRow;   
		for(j=0; j<width; j++)
			*aPtr++ = this->mColor;

		aRow+=this->mPixelsPerRow;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
void Surface<PixelType>::DrawRect(int x, int y, int width, int height)
{
	if(width==1)
	{
		// vert line special case
		this->DrawVertLine(x,y,y+height-1);
		return;
	}
	if(height==1)
	{
		// horz line special case
		this->DrawHorzLine(x,x+width-1,y);
		return;
	}

	this->DrawHorzLine(x,x+width-1,y);
	this->DrawHorzLine(x,x+width-1,y+height-1);
	this->DrawVertLine(x,y,y+height-1);
	this->DrawVertLine(x+width-1,y,y+height-1);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
void Surface<PixelType>::DrawHorzLine(int x1, int x2, int y)
{
	if(y<this->mClipY1 || y>this->mClipY2)
		return;

	if(x2<x1)
		std::swap(x2,x1);

	if(x1<this->mClipX1) x1 = this->mClipX1;
	if(x2>this->mClipX2) x2 = this->mClipX2;
	if(x2<x1)
		return;

	PixelType *aPtr = this->mImage + y*this->mPixelsPerRow + x1;   
	const int distance = x2-x1+1;
	for(int i=0; i<distance; i++)
		*aPtr++ = this->mColor;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
void Surface<PixelType>::DrawVertLine(int x, int y1, int y2)
{
	if(x<this->mClipX1 || x>this->mClipX2)
		return;

	if(y2<y1)
		std::swap(y1,y2);

	if(y1<this->mClipY1) y1 = this->mClipY1;
	if(y2>this->mClipY2) y2 = this->mClipY2;
	if(y2<y1)
		return;

	PixelType *aPtr = this->mImage + y1*this->mPixelsPerRow + x;   
	const int distance = y2-y1+1;
	for(int i=0; i<distance; i++)
	{
		*aPtr = this->mColor;
		aPtr += this->mPixelsPerRow;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Midpoint line algorithm

// Line defined by y = (dy/dx)x + b, dx = (x1-x0), dy = (y1-y0)
// Let F(x,y) = dy*x - dx*y + c, c = dx*b
// Points on the line satisfy F(x,y) = 0.

// For each point x, y on the line we must determine whether the next point is (x+1, y) or (x+1,y+1)
// (for a line with a slope between 0 and 1.)  To decide, we evaluate
// F(x+1,y+1/2) and see whether it's above the line or below the line, i.e. positive or negative.

// Notice that
// F(x+1,y) = F(x,y) + dy  and
// F(x+1,y+1) = F(x,y) + dy - dx.
// So the next decision variable can be computed in terms of the previous decision variable

// Note then that F(x0,y0) = 0 since (x0,y0) is a point on the line.
// So F(x0,y0+1/2) = F(x0,y0) - dx/2 = -dx/2.

// For all values to be integers make the decision variable 2*F(x+1,y+1/2).
// Note that I don't use the midpoint line algorithm
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
void Surface<PixelType>::DrawLine(int x1, int y1, int x2, int y2)
{
	if(x1==x2) 
	{
		this->DrawVertLine(x1,y1,y2);
		return;
	}
	
	if(y1==y2)
	{
		this->DrawHorzLine(x1,x2,y1);
		return;
	}

	int clipCode1 = this->CalcClipCode(x1,y1);
	int clipCode2 = this->CalcClipCode(x2,y2);
	if(clipCode1!=0 || clipCode2!=0) // Need clipping
	{
		if((clipCode1&clipCode2)==0)
			this->DrawLineClip(x1,y1,x2,y2);
		// else line is completely outside clipping rectangle

		return;
	}


	int i;
	int accum = 0;

	int width = x2-x1;
	int height = y2-y1;
	const int xinc = width>0?1:-1;
	const int yinc = height>0?this->mPixelsPerRow:-this->mPixelsPerRow;
	if(width<0)
		width = -width;
	if(height<0)
		height = -height;

	PixelType *aPtr = this->mImage + y1*this->mPixelsPerRow + x1;
	if(width<height)
	{
		// up/down
		for(i=0; i<height; i++)
		{
			*aPtr = this->mColor;
			aPtr += yinc;
			accum += width;
			if(accum >= height)
			{
				aPtr += xinc;
				accum -= height;
			}
		}

	}
	else
	{
		// left/right
		for(i=0; i<width; i++)
		{
			*aPtr = this->mColor;
			aPtr += xinc;
			accum += height;
			if(accum >= width)
			{
				aPtr += yinc;
				accum -= width;
			}
		}
	}

	// Draw last point
	*aPtr = this->mColor;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
inline int my_round(float theNum) 
{
	return floorf(theNum + 0.5f);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
void Surface<PixelType>::DrawLineClip(int x1, int y1, int x2, int y2)
{
	int i;
	int accum = 0;

	int width = x2-x1;
	int height = y2-y1;
	const int xCoordInc = width>0?1:-1;
	const int yCoordInc = height>0?1:-1;
	const int xinc = xCoordInc;
	const int yinc = yCoordInc*this->mPixelsPerRow;

	if(width<0)
		width = -width;
	if(height<0)
		height = -height;

	PixelType *aPtr = this->mImage + y1*this->mPixelsPerRow + x1;
	int x=x1, y=y1;
	if(width<height)
	{
		// up/down
		for(i=0; i<height; i++)
		{
			if(this->NotClipped(x,y))
				*aPtr = this->mColor;

			aPtr += yinc;
			y += yCoordInc;
			accum += width;
			if(accum < height)
				continue;
			aPtr += xinc;
			x += xCoordInc;
			accum -= height;
		}

	}
	else
	{
		// left/right
		for(i=0; i<width; i++)
		{
			if(this->NotClipped(x,y))
				*aPtr = this->mColor;

			aPtr += xinc;
			x += xCoordInc;
			accum += height;
			if(accum < width)
				continue;
			aPtr += yinc;
			y += yCoordInc;
			accum -= width;
		}
	}

	// Draw last point
	if(this->NotClipped(x,y))
		*aPtr = this->mColor;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Midpoint circle algorithm

// Consider the circle of radius r at the origin defined by
// x^2 + y^2 = r^2
// Let F(x,y) = x^2 + y^2 - r^2.
// So F(x,y) = 0 if (x,y) is on the circle.
// F(x,y)>0 is outside the circle, and F(x,y)<0 is inside the circle.

// To draw the circle from 0 to 45 degrees, do the following.
// Given a point (x,y) on the circle the next point is either (x,y+1) or (x-1,y+1).
// To decide which consider F(x-1/2,y+1) to see which is closer.  If this is negative,
// then (x,y+1) is closer, if positive then (x-1,y+1) is closer.

// Notice that F(x,y+1) = x^2 + y^2 + 2y + 1 - r^2 = F(x,y) + 2y + 1.
// and F(x-1,y+1) = x^2 - 2x + 1 + y^2 + 2y + 1 - r^2 = F(x,y) + (2y - 2x) + 2.

// Start out F(x,y) as F(r,0) = 0.
// Then F(r-1/2, 1) = r^2 - r + 1/4 + 1 - r^2
// 5/4 - r
// So use D(x,y) = 4*F(x,y) as the decision variable to avoid fractions.
// -> D(r-1/2,1) = 5 - 4r.

// Notice that  D(x,y+1) = 4*F(x,y+1) = 4*F(x,y) + 8y + 4 = D(x,y) + 8y + 4
// and D(x+1,y+1) = 4*F(x+1,y+1) = 4*F(x,y) + (8y - 8x) + 8

template <class PixelType>
void Surface<PixelType>::DrawCircle(int x, int y, int r)
{
	int x1 = r, y1 = 0;
	int d = 5 - 4*r;

	while(x1>=y1)
	{
		this->PutPixel(x + x1,y - y1); // (0-45)
		this->PutPixel(x + y1,y - x1); // (45-90)
		this->PutPixel(x - y1,y - x1); // (90-135)
		this->PutPixel(x - x1,y - y1); // (135,180)
		this->PutPixel(x - x1,y + y1); // (180,225)
		this->PutPixel(x - y1,y + x1); // (225,270)
		this->PutPixel(x + y1,y + x1); // (270,315)
		this->PutPixel(x + x1,y + y1); // (315,360);

		if(d>0) // Evalue D(x-1,y+1)
		{
			d = d + 8*(y1-x1) + 4;
			x1--;
			y1++;
		}
		else // Evalue D(x,y+1)
		{
			d = d + 8*y1 + 4;
			y1++;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
void Surface<PixelType>::FillCircle(int x, int y, int r)
{
	int x1 = r, y1 = 0;
	int d = 5 - 4*r;

	while(x1>y1)
	{
		this->DrawHorzLine(x-x1,x+x1,y-y1); // (0-45, 135-180)
		this->DrawHorzLine(x-x1,x+x1,y+y1); // (180-225, 315-360)
		this->DrawHorzLine(x-y1,x+y1,y-x1); // (45-90, 90-135)
		this->DrawHorzLine(x-y1,x+y1,y+x1); // (225-270,270-315)

		if(d>0) // Evalue D(x-1,y+1)
		{
			d = d + 8*(y1-x1) + 4;
			x1--;
			y1++;
		}
		else // Evalue D(x,y+1)
		{
			d = d + 8*y1 + 4;
			y1++;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// Midpoint ellipse algorithm

// Consider the ellipse with horizontal radius, a, and vertical radius, b, at the origin defined by
// (x/a)^2 + (y/b)^2 = 1 --> (bx)^2 + (ay)^2 = (ab)^2
// Let F(x,y) = (bx)^2 + (ay)^2 - (ab)^2
// So F(x,y) = 0 if (x,y) is on the ellipse.
// F(x,y)>0 is outside the ellipse, and F(x,y)<0 is inside the ellipse.

// To draw the ellipse with slope<-1, do the following.
// Given a point (x,y) on the circle the next point is either (x,y+1) or (x-1,y+1).
// To decide which consider F(x-1/2,y+1) to see which is closer.  If this is negative,
// then (x,y+1) is closer, if positive then (x-1,y+1) is closer.

// Notice that F(x,y+1) = (bx)^2 + a^2(y^2 + 2y + 1) - (ab)^2 = F(x,y) + 2(a^2)y + a^2
// and F(x-1,y+1) = b^2(x^2 - 2x + 1) + a^2(y^2 + 2y + 1) - (ab)^2 = F(x,y) + 2((a^2)y - (b^2)x) + a^2 + b^2
// and F(x-1,y) = b^2(x^2 - 2x + 1) + (ay)^2 - (ab^2) = F(x,y) - 2(b^2)x + b^2

// Start out F(x,y) as F(a,0) = 0.
// Then F(a-1/2, 1) = b^2(a^2 - a + 1/4) + a^2 - (ab)^2 = a^2 - (b^2)a + (b^2)*1/4

// Again use D(x,y) = 4*F(x,y) as the decision variable.
// -> D(a-1/2,1) = 4a^2 - 4(b^2)a + b^2.

// Need to calculate point where slope is greater than -1.
// -> gradiant of F(x,y) has i and j components equal.
// grad(F(x,y)) = df/dx i + df/dy j = 2(b^2)x i + 2(b^2)y j

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
void Surface<PixelType>::DrawEllipse(int x, int y, int width, int height)
{
	typedef double IntType;
	IntType a = (IntType)(width-1)/2;
	IntType b = (IntType)(height-1)/2;
	IntType a2 = a*a;
	IntType b2 = b*b;
	IntType cx = x+a;
	IntType cy = y+b;

	IntType x1 = a, y1 = 0;
	IntType d = 4*a2 - 4*b2*a + b2;


	while(a2*y1 < b2*x1)
	{
//		double d2 = 4*(b2*(x1-0.5)*(x1-0.5) + a2*(y1+1)*(y1+1) - a2*b2);

		this->PutPixel(cx + x1,cy - y1); // (0-45)
		this->PutPixel(cx - x1,cy - y1); // (135,180)
		this->PutPixel(cx - x1,cy + y1); // (180,225)
		this->PutPixel(cx + x1,cy + y1); // (315,360);

		if(d>0) // Evalue D(x-1,y+1)
		{
			x1--;
			y1++;
			d = d + 8*(a2*y1 - b2*x1) + 4*a2;
		}
		else // Evalue D(x,y+1)
		{
			y1++;
			d = d + 8*a2*y1 + 4*a2;
		}
	}

	// Know D(x-1/2,y+1)
	// Want D(x-1,y+1/2)

	d = d + 4*(-a2*y1 - b2*x1 - a2 + b2) + a2 - b2;
	while(x1>=0)
	{
//		double d2 = 4*(b2*(x1-1)*(x1-1) + a2*(y1+0.5)*(y1+0.5) - a2*b2);
		
		this->PutPixel(cx + x1,cy - y1); // (45-90)
		this->PutPixel(cx - x1,cy - y1); // (90,135)
		this->PutPixel(cx - x1,cy + y1); // (225,270)
		this->PutPixel(cx + x1,cy + y1); // (270,315);

		if(d<0) // Evalue D(x-1,y+1)
		{
			x1--;
			y1++;
			d = d + 8*(a2*y1 - b2*x1) + 4*b2;
		}
		else // Evalue D(x-1,y)
		{
			x1--;
			d = d - 8*b2*x1 + 4*b2;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
void Surface<PixelType>::FillEllipse(int x, int y, int width, int height)
{
	typedef double IntType;
	IntType a = (IntType)(width-1)/2;
	IntType b = (IntType)(height-1)/2;
	IntType a2 = a*a;
	IntType b2 = b*b;
	IntType cx = x+a;
	IntType cy = y+b;

	IntType x1 = a, y1 = 0;
	IntType d = 4*a2 - 4*b2*a + b2;


	while(a2*y1 < b2*x1)
	{
//		double d2 = 4*(b2*(x1-0.5)*(x1-0.5) + a2*(y1+1)*(y1+1) - a2*b2);

		this->DrawHorzLine(cx - x1, cx + x1, cy-y1);
		this->DrawHorzLine(cx - x1, cx + x1, cy+y1);

		if(d>0) // Evalue D(x-1,y+1)
		{
			x1--;
			y1++;
			d = d + 8*(a2*y1 - b2*x1) + 4*a2;
		}
		else // Evalue D(x,y+1)
		{
			y1++;
			d = d + 8*a2*y1 + 4*a2;
		}
	}

	d = d + 4*(-a2*y1 - b2*x1 - a2 + b2) + a2 - b2;
	while(x1>=0)
	{
//		double d2 = 4*(b2*(x1-1)*(x1-1) + a2*(y1+0.5)*(y1+0.5) - a2*b2);

		this->DrawHorzLine(cx - x1, cx + x1, cy-y1);
		this->DrawHorzLine(cx - x1, cx + x1, cy+y1);

		if(d<0) // Evalue D(x-1,y+1)
		{
			x1--;
			y1++;
			d = d + 8*(a2*y1 - b2*x1) + 4*b2;
		}
		else // Evalue D(x-1,y)
		{
			x1--;
			d = d - 8*b2*x1 + 4*b2;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
struct OctantEndPoint
{
	typedef double IntType;

	IntType x1, y1;
	IntType x2, y2;
	bool c1, c2;
	int mOctant;
	bool mInverseArc;

	enum { BIG_VAL = 100000 };

	void SetOctant(int theOctant, bool inverseArc) { this->mOctant = theOctant; this->mInverseArc = inverseArc; }
	void SetEnd1(IntType x, IntType y) { this->x1 = x; this->y1 = y; }
	void SetEnd2(IntType x, IntType y) { this->x2 = x; this->y2 = y; }

	void InvertLimits() { this->x1*=-1; this->y1*=-1; this->x2*=-1; this->y2*=-1; }
	void InitMax1() { this->x1 = this->y2 = BIG_VAL; this->x2 = this->y1 = -BIG_VAL; }
	void InitMax2() { this->x1 = this->y2 = -BIG_VAL; this->x2 = this->y1 = BIG_VAL; }

	void IncludeAll() 
	{ 
		switch(this->mOctant)
		{
			case 0: this->y1 = -BIG_VAL; this->y2 = BIG_VAL; break;
			case 1: this->x1 = BIG_VAL; this->x2 = -BIG_VAL; break;
			case 2: this->x1 = BIG_VAL; this->x2= -BIG_VAL; break;
			case 3: this->y1 = BIG_VAL; this->y2 = -BIG_VAL; break;
			case 4: this->y1 = BIG_VAL; this->y2 = -BIG_VAL; break;
			case 5: this->x1 = -BIG_VAL; this->x2 = BIG_VAL; break;
			case 6: this->x1 = -BIG_VAL; this->x2 = BIG_VAL; break;
			case 7: this->y1 = -BIG_VAL; this->y2 = BIG_VAL; break;
		}
	}

	void IncludeNone() 
	{
		this->IncludeAll();
		this->InvertLimits();
	}

	bool IncludePointPrv(IntType x, IntType y)
	{
		switch(this->mOctant)
		{
			case 0: return y>=this->y1 && y<=this->y2;
			case 1: return x<=this->x1 && x>=this->x2;
			case 2: return x<=this->x1 && x>=this->x2;
			case 3: return y<=this->y1 && y>=this->y2;
			case 4: return y<=this->y1 && y>=this->y2;
			case 5: return x>=this->x1 && x<=this->x2;
			case 6: return x>=this->x1 && x<=this->x2;
			case 7: return y>=this->y1 && y<=this->y2;
			default: return false;
		}
	}

	bool IncludePoint(IntType x, IntType y)
	{
		bool aVal = this->IncludePointPrv(x,y);
		if(this->mInverseArc)
			return !aVal;
		else
			return aVal;
	}
};


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
void Surface<PixelType>::DrawArc(int x, int y, int width, int height, int startAngle, int arcAngle)
{
	if(arcAngle>=360)
	{
		this->DrawEllipse(x,y,width,height);
		return;
	}

	static const double PI = 3.14159;

	if(startAngle<0)
		startAngle = (startAngle+360)%360;
	else if(startAngle>360)
		startAngle%=360;

	bool inverseArc = false;
	int endAngle = startAngle + arcAngle;
	if(endAngle>360)
	{
		endAngle -= 360;
		std::swap(startAngle,endAngle);
		inverseArc = true;
	}
	
	typedef double IntType;
	IntType a = (IntType)(width-1)/2;
	IntType b = (IntType)(height-1)/2;
	IntType a2 = a*a;
	IntType b2 = b*b;
	IntType cx = x+a;
	IntType cy = y+b;

	IntType xEnd1 = a*cos(PI*startAngle/180);
	IntType yEnd1 = b*sin(PI*startAngle/180);
	IntType xEnd2 = a*cos(PI*endAngle/180);
	IntType yEnd2 = b*sin(PI*endAngle/180);

	OctantEndPoint oep[8];
	int curStartAngle = 0, curEndAngle = 45;
	for(int i=0; i<8; i++)
	{
		oep[i].SetOctant(i, inverseArc);
		if(curEndAngle<startAngle || curStartAngle>endAngle)
			oep[i].IncludeNone();
		else
		{
			oep[i].IncludeAll();
			if(curStartAngle<startAngle)
				oep[i].SetEnd1(xEnd1,yEnd1);
			if(curEndAngle>endAngle)
				oep[i].SetEnd2(xEnd2,yEnd2);
		}
		curStartAngle+=45;
		curEndAngle+=45;
	}

	IntType x1 = a, y1 = 0;
	IntType d = 4*a2 - 4*b2*a + b2;

	while(a2*y1 < b2*x1)
	{
		if(oep[0].IncludePoint(x1,y1)) this->PutPixel(cx + x1,cy - y1); // (0-45)
		if(oep[3].IncludePoint(-x1,y1)) this->PutPixel(cx - x1,cy - y1); // (135,180)
		if(oep[4].IncludePoint(-x1,-y1)) this->PutPixel(cx - x1,cy + y1); // (180,225)
		if(oep[7].IncludePoint(x1,-y1)) this->PutPixel(cx + x1,cy + y1); // (315,360);

		if(d>0) // Evalue D(x-1,y+1)
		{
			x1--;
			y1++;
			d = d + 8*(a2*y1 - b2*x1) + 4*a2;
		}
		else // Evalue D(x,y+1)
		{
			y1++;
			d = d + 8*a2*y1 + 4*a2;
		}
	}

	d = d + 4*(-a2*y1 - b2*x1 - a2 + b2) + a2 - b2;
	while(x1>=0)
	{
		if(oep[1].IncludePoint(x1,y1)) this->PutPixel(cx + x1,cy - y1); // (45-90)
		if(oep[2].IncludePoint(-x1,y1)) this->PutPixel(cx - x1,cy - y1); // (90,135)
		if(oep[5].IncludePoint(-x1,-y1)) this->PutPixel(cx - x1,cy + y1); // (225,270)
		if(oep[6].IncludePoint(x1,-y1)) this->PutPixel(cx + x1,cy + y1); // (270,315);

		if(d<0) // Evalue D(x-1,y+1)
		{
			x1--;
			y1++;
			d = d + 8*(a2*y1 - b2*x1) + 4*b2;
		}
		else // Evalue D(x-1,y)
		{
			x1--;
			d = d - 8*b2*x1 + 4*b2;
		}
	}
};


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
void Surface<PixelType>::PutPixel(int x, int y)
{
	if(this->NotClipped(x,y))
		*(this->mImage + y*this->mPixelsPerRow + x) = this->mColor;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
/*
template <class PixelType>
void Surface<PixelType>::PutPixel(int x, int y, PixelType *thePtr)
{
	if(NotClipped(x,y))
		*thePtr = mColor;
}
*/
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
void Surface<PixelType>::SetPixel(int x, int y, DWORD theColor)
{
	if(this->NotClipped(x,y))
		*(this->mImage + y*this->mPixelsPerRow + x) = SurfaceColorFrom32(theColor,this->mFormat);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <class PixelType>
DWORD Surface<PixelType>::GetPixel(int x, int y)
{
	if(x<0 || y<0 || x>=this->mWidth || y>=this->mHeight)
		return 0;

	return *(this->mImage + y*this->mPixelsPerRow + x);
}

// ---- the driver
using namespace WONAPI;

static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }
static int RI(int lo, int hi) { return lo + (int)R((unsigned)(hi - lo + 1)); }

struct Slot {
    SmartPtr<SurfaceBase> o;
    EeWonSurface* r;
    int fmt;
};
static Slot sSlot[3];

static DWORD OrigColor(Slot& s)
{
    return s.fmt == SurfaceFormat_32 ? ((Surface32*)s.o.get())->GetColor() : ((Surface16*)s.o.get())->GetColor();
}
static uint64_t Fnv(const unsigned char* p, size_t n)
{
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}
static std::string Fields(int w, int h, int bpr, int ppr, int c1, int c2, int c3, int c4, DWORD color, bool pixels, uint64_t hash)
{
    char aLine[200];
    snprintf(aLine, sizeof(aLine), "= %d %d %d %d %d %d %d %d %x %d %016llx", w, h, bpr, ppr, c1, c2, c3, c4, (unsigned)color,
             pixels ? 1 : 0, (unsigned long long)hash);
    return aLine;
}
static std::string OrigState(Slot& s)
{
    SurfaceBase* o = s.o.get();
    size_t n = o->GetPixelsBase() ? (size_t)o->GetBytesPerRow() * o->GetHeight() : 0;
    return Fields(o->GetWidth(), o->GetHeight(), o->GetBytesPerRow(), o->GetPixelsPerRow(), o->GetClipX1(), o->GetClipY1(),
                  o->GetClipX2(), o->GetClipY2(), OrigColor(s), o->GetPixelsBase() != NULL,
                  Fnv((const unsigned char*)o->GetPixelsBase(), n));
}
static int G(EeWonSurface* r, EeWonSurfaceField f) { return ee_won_surface_get(r, f); }
static std::string RustState(Slot& s)
{
    EeWonSurface* r = s.r;
    void* p = ee_won_surface_pixels(r);
    size_t n = p ? (size_t)G(r, EeWonSurfaceField::BytesPerRow) * G(r, EeWonSurfaceField::Height) : 0;
    return Fields(G(r, EeWonSurfaceField::Width), G(r, EeWonSurfaceField::Height), G(r, EeWonSurfaceField::BytesPerRow),
                  G(r, EeWonSurfaceField::PixelsPerRow), G(r, EeWonSurfaceField::ClipX1), G(r, EeWonSurfaceField::ClipY1),
                  G(r, EeWonSurfaceField::ClipX2), G(r, EeWonSurfaceField::ClipY2), (DWORD)G(r, EeWonSurfaceField::Color),
                  p != NULL, Fnv((const unsigned char*)p, n));
}

static int Size()
{
    switch (R(20)) {
    case 0: return 0;
    case 1: return 1;
    case 2: return 64;
    default: return (int)R(41);
    }
}
static void Create(Slot& s, int w, int h)
{
    s.o->Create(w, h);
    if (s.o->GetPixelsBase() != NULL) memset(s.o->GetPixelsBase(), 0, (size_t)s.o->GetBytesPerRow() * h);
    ee_won_surface_create(s.r, w, h);
}
static void New(Slot& s, int fmt)
{
    s.o = SurfaceBase::GetNewSurface((SurfaceFormat)fmt);
    if (s.r) ee_won_surface_free(s.r);
    s.r = ee_won_surface_new(fmt);
    s.fmt = fmt;
}
static std::vector<unsigned char> Mask(int srcW, int srcH, int salt)
{
    int bpr = srcW / 8 + (srcW % 8 != 0 ? 1 : 0);
    std::vector<unsigned char> m((size_t)(bpr * srcH > 0 ? bpr * srcH : 0) + 1);
    for (size_t k = 0; k < m.size(); k++) {
        unsigned v = (unsigned)(k * 37 + salt * 11 + (k >> 3) * 5) & 0xff;
        m[k] = (unsigned char)((salt & 3) == 0 ? v & (v >> 1) : (salt & 3) == 1 ? v | (v << 3) : v);
    }
    return m;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    long long aChecks = 0;
    int aFailures = 0;
    char aLine[300];
    const int kSequences = 20000;
    for (int seq = 0; seq < kSequences; seq++) {
        sSeed = 7919u * (unsigned)seq + 17u;
        bool aShow = aPrint && seq < 150;
        if (aShow) printf("S %d\n", seq);
        for (int k = 0; k < 3; k++) {
            int fmt = (int)R(3), w = Size(), h = Size();
            New(sSlot[k], fmt);
            Create(sSlot[k], w, h);
            snprintf(aLine, sizeof(aLine), "N %d %d %d %d", k, fmt, w, h);
            std::string a = OrigState(sSlot[k]), b = RustState(sSlot[k]);
            aChecks++;
            if (a != b && ++aFailures < 20) printf("FAIL %d %s\n orig %s\n rust %s\n", seq, aLine, a.c_str(), b.c_str());
            if (aShow) printf("%s %s\n", aLine, a.c_str());
        }
        for (int op = 0; op < 60; op++) {
            int k = (int)R(3);
            Slot& s = sSlot[k];
            SurfaceBase* o = s.o.get();
            int W = o->GetWidth(), H = o->GetHeight();
            int a = RI(-20, W + 20), b = RI(-20, H + 20), c = RI(-20, W + 20), d = RI(-20, H + 20);
            std::string extra;
            unsigned which = R(100);
            if (which < 6) {
                DWORD col = (DWORD)(R(0x8000) | (R(0x8000) << 15) | (R(4) << 30));
                o->SetColor(col);
                ee_won_surface_draw(s.r, EeWonSurfaceDraw::SetColor, (int)col, 0, 0, 0, 0, 0);
                snprintf(aLine, sizeof(aLine), "D %d %d %d", k, (int)EeWonSurfaceDraw::SetColor, (int)col);
            } else if (which < 12) {
                int cw = RI(-5, W + 10), ch = RI(-5, H + 10);
                o->SetClipRect(a, b, cw, ch);
                ee_won_surface_draw(s.r, EeWonSurfaceDraw::SetClipRect, a, b, cw, ch, 0, 0);
                snprintf(aLine, sizeof(aLine), "D %d %d %d %d %d %d", k, (int)EeWonSurfaceDraw::SetClipRect, a, b, cw, ch);
            } else if (which < 14) {
                o->ClearClipRect();
                ee_won_surface_draw(s.r, EeWonSurfaceDraw::ClearClipRect, 0, 0, 0, 0, 0, 0);
                snprintf(aLine, sizeof(aLine), "D %d %d", k, (int)EeWonSurfaceDraw::ClearClipRect);
            } else if (which < 20) {
                int w = RI(-5, W + 10), h = RI(-5, H + 10);
                o->FillRect(a, b, w, h);
                ee_won_surface_draw(s.r, EeWonSurfaceDraw::FillRect, a, b, w, h, 0, 0);
                snprintf(aLine, sizeof(aLine), "D %d %d %d %d %d %d", k, (int)EeWonSurfaceDraw::FillRect, a, b, w, h);
            } else if (which < 26) {
                int w = R(4) == 0 ? (int)R(3) : RI(-5, W + 10), h = R(4) == 0 ? (int)R(3) : RI(-5, H + 10);
                o->DrawRect(a, b, w, h);
                ee_won_surface_draw(s.r, EeWonSurfaceDraw::DrawRect, a, b, w, h, 0, 0);
                snprintf(aLine, sizeof(aLine), "D %d %d %d %d %d %d", k, (int)EeWonSurfaceDraw::DrawRect, a, b, w, h);
            } else if (which < 30) {
                o->DrawHorzLine(a, c, b);
                ee_won_surface_draw(s.r, EeWonSurfaceDraw::HorzLine, a, c, b, 0, 0, 0);
                snprintf(aLine, sizeof(aLine), "D %d %d %d %d %d", k, (int)EeWonSurfaceDraw::HorzLine, a, c, b);
            } else if (which < 34) {
                o->DrawVertLine(a, b, d);
                ee_won_surface_draw(s.r, EeWonSurfaceDraw::VertLine, a, b, d, 0, 0, 0);
                snprintf(aLine, sizeof(aLine), "D %d %d %d %d %d", k, (int)EeWonSurfaceDraw::VertLine, a, b, d);
            } else if (which < 46) {
                if (R(4) == 0) c = a;
                else if (R(4) == 0) d = b;
                o->DrawLine(a, b, c, d);
                ee_won_surface_draw(s.r, EeWonSurfaceDraw::Line, a, b, c, d, 0, 0);
                snprintf(aLine, sizeof(aLine), "D %d %d %d %d %d %d", k, (int)EeWonSurfaceDraw::Line, a, b, c, d);
            } else if (which < 52) {
                int r = RI(-5, 40);
                bool fill = R(2);
                if (fill) o->FillCircle(a, b, r); else o->DrawCircle(a, b, r);
                EeWonSurfaceDraw dk = fill ? EeWonSurfaceDraw::FillCircle : EeWonSurfaceDraw::Circle;
                ee_won_surface_draw(s.r, dk, a, b, r, 0, 0, 0);
                snprintf(aLine, sizeof(aLine), "D %d %d %d %d %d", k, (int)dk, a, b, r);
            } else if (which < 60) {
                int w = RI(-5, 60), h = RI(-5, 60);
                bool fill = R(2);
                if (fill) o->FillEllipse(a, b, w, h); else o->DrawEllipse(a, b, w, h);
                EeWonSurfaceDraw dk = fill ? EeWonSurfaceDraw::FillEllipse : EeWonSurfaceDraw::Ellipse;
                ee_won_surface_draw(s.r, dk, a, b, w, h, 0, 0);
                snprintf(aLine, sizeof(aLine), "D %d %d %d %d %d %d", k, (int)dk, a, b, w, h);
            } else if (which < 68) {
                int w = RI(-5, 60), h = RI(-5, 60), st = RI(-400, 800), ar = RI(-50, 500);
                if (R(3) == 0) st = 45 * RI(-2, 9);
                if (R(3) == 0) ar = 45 * RI(0, 9);
                o->DrawArc(a, b, w, h, st, ar);
                ee_won_surface_draw(s.r, EeWonSurfaceDraw::Arc, a, b, w, h, st, ar);
                snprintf(aLine, sizeof(aLine), "D %d %d %d %d %d %d %d %d", k, (int)EeWonSurfaceDraw::Arc, a, b, w, h, st, ar);
            } else if (which < 72) {
                o->PutPixel(a, b);
                ee_won_surface_draw(s.r, EeWonSurfaceDraw::PutPixel, a, b, 0, 0, 0, 0);
                snprintf(aLine, sizeof(aLine), "D %d %d %d %d", k, (int)EeWonSurfaceDraw::PutPixel, a, b);
            } else if (which < 76) {
                DWORD col = (DWORD)(R(0x8000) | (R(0x8000) << 15) | (R(4) << 30));
                o->SetPixel(a, b, col);
                ee_won_surface_draw(s.r, EeWonSurfaceDraw::SetPixel, a, b, (int)col, 0, 0, 0);
                snprintf(aLine, sizeof(aLine), "D %d %d %d %d %d", k, (int)EeWonSurfaceDraw::SetPixel, a, b, (int)col);
            } else if (which < 82) {
                DWORD va = o->GetPixel(a, b), vb = ee_won_surface_pixel(s.r, a, b);
                aChecks++;
                if (va != vb && ++aFailures < 20) printf("FAIL %d pixel %d %d: %x %x\n", seq, a, b, (unsigned)va, (unsigned)vb);
                snprintf(aLine, sizeof(aLine), "P %d %d %d %x", k, a, b, (unsigned)va);
            } else if (which < 92) {
                // a copy from another slot of the same format (or a 32-bit one into a 16-bit one), or within this one
                int from = (int)R(3);
                Slot& f = sSlot[from];
                bool ok = from == k || f.fmt == s.fmt || (f.fmt == SurfaceFormat_32 && s.fmt != SurfaceFormat_32);
                int SW = f.o->GetWidth(), SH = f.o->GetHeight();
                if (!ok || SW <= 0 || SH <= 0 || f.o->GetPixelsBase() == NULL || o->GetPixelsBase() == NULL) {
                    op--;
                    continue;
                }
                int l = (int)R(SW), t = (int)R(SH), w = RI(-2, SW - l), h = RI(-2, SH - t);
                bool masked = R(2);
                int salt = (int)R(256);
                std::vector<unsigned char> m = Mask(SW, SH, salt);
                if (masked) o->CopySurface(f.o.get(), (const char*)&m[0], a, b, l, t, w, h);
                else o->CopySurface(f.o.get(), a, b, l, t, w, h);
                ee_won_surface_copy(s.r, f.r, masked ? &m[0] : NULL, a, b, l, t, w, h);
                snprintf(aLine, sizeof(aLine), "K %d %d %d %d %d %d %d %d %d %d", k, from, masked ? 1 : 0, salt, a, b, l, t, w, h);
            } else if (which < 95) {
                int from = (int)R(3);
                if (from == k) { op--; continue; }
                SurfaceBasePtr dup = sSlot[from].o->Duplicate();
                s.o = dup;
                if (s.r) ee_won_surface_free(s.r);
                s.r = ee_won_surface_duplicate(sSlot[from].r);
                s.fmt = sSlot[from].fmt;
                snprintf(aLine, sizeof(aLine), "U %d %d", k, from);
            } else if (which < 98) {
                int w = Size(), h = Size();
                Create(s, w, h);
                snprintf(aLine, sizeof(aLine), "C %d %d %d", k, w, h);
            } else {
                o->Free();
                ee_won_surface_release(s.r);
                snprintf(aLine, sizeof(aLine), "F %d", k);
            }
            std::string sa = OrigState(s), sb = RustState(s);
            aChecks++;
            if (sa != sb && ++aFailures < 20) printf("FAIL %d %s\n orig %s\n rust %s\n", seq, aLine, sa.c_str(), sb.c_str());
            if (aShow) printf("%s %s\n", aLine, sa.c_str());
        }
    }
    for (int k = 0; k < 3; k++) { sSlot[k].o = NULL; ee_won_surface_free(sSlot[k].r); sSlot[k].r = NULL; }
    if (aPrint) return 0;
    printf("won_surface_test: %lld checks, %d failures\n", aChecks, aFailures);
    return aFailures ? 1 : 0;
}
