// Software surface classes (optional; not identified in the original exe).
// R6 WON round 37: the surface is ee_won's Rust object (rust/crates/ee_won_gui/src/surface.rs: the format, size, row
// pitch, pixels, clip rectangle and colour, and every drawing method of the SDK's Surface.cpp); SurfaceBase is its
// reference-counted shell, each method a forwarder. The SDK's Surface<PixelType> (Surface16, Surface32) is the one
// class, its pixel size from the format. The pixels stay where callers write them (GetPixelsBase). The SDK's
// WIN32_NOT_XBOX conditional (BlitToDC's GDI blit) is kept.
#ifndef EE_WONAPI_WONSURFACE_H
#define EE_WONAPI_WONSURFACE_H
#include <windows.h>
#include <string.h>
#include "ee/EEWonNet.h"
#include "rust/won_gui/surface.h"

namespace WONAPI
{

enum SurfaceFormat
{
	SurfaceFormat_32,
	SurfaceFormat_555,
	SurfaceFormat_565
};

class SurfaceBase;
typedef SmartPtr<SurfaceBase> SurfaceBasePtr;
class SurfaceBase : public RefCount
{
	EeWonSurface* mRust;

	typedef EeWonSurfaceDraw D;
	void Draw(D op, int a=0, int b=0, int c=0, int d=0, int e=0, int f=0) { ee_won_surface_draw(this->mRust,op,a,b,c,d,e,f); }
	int Get(EeWonSurfaceField f) { return ee_won_surface_get(this->mRust,f); }
	explicit SurfaceBase(EeWonSurface* theRust) : mRust(theRust) {}
	SurfaceBase(const SurfaceBase&);
	SurfaceBase& operator=(const SurfaceBase&);
#ifdef WIN32_NOT_XBOX
	void Blit(HDC theDC, int x, int y, int width, int height, int left, int top, int start, int lines)
	{
		struct { BITMAPINFOHEADER mHeader; DWORD mMasks[3]; } anInfo;
		int v[8];
		ee_won_surface_bitmap_info(this->mRust,v);
		memset(&anInfo,0,sizeof(anInfo));
		anInfo.mHeader.biSize = sizeof(anInfo.mHeader);
		anInfo.mHeader.biWidth = v[0]; anInfo.mHeader.biHeight = v[1]; anInfo.mHeader.biPlanes = (WORD)v[2];
		anInfo.mHeader.biBitCount = (WORD)v[3]; anInfo.mHeader.biCompression = (DWORD)v[4];
		anInfo.mMasks[0] = v[5]; anInfo.mMasks[1] = v[6]; anInfo.mMasks[2] = v[7];
		SetDIBitsToDevice(theDC,x,y,width,height,left,top,start,lines,this->GetPixelsBase(),(BITMAPINFO*)&anInfo,DIB_RGB_COLORS);
	}
#endif

protected:
	virtual ~SurfaceBase() { ee_won_surface_free(this->mRust); }

public:
	EeWonSurface* GetRust() { return this->mRust; }
	void CopySurface(SurfaceBase *theSurface, int x, int y, int left, int top, int width, int height)
	{ ee_won_surface_copy(this->mRust,theSurface->mRust,NULL,x,y,left,top,width,height); }
	void CopySurface(SurfaceBase *theSurface, const char *theMaskData, int x, int y, int left, int top, int width, int height)
	{ ee_won_surface_copy(this->mRust,theSurface->mRust,(const uint8_t*)theMaskData,x,y,left,top,width,height); }

	DWORD GetPixel(int x, int y) { return ee_won_surface_pixel(this->mRust,x,y); }
	void PutPixel(int x, int y) { this->Draw(D::PutPixel,x,y); }
	void SetPixel(int x, int y, DWORD theColor) { this->Draw(D::SetPixel,x,y,(int)theColor); }
	void DrawCircle(int x, int y, int r) { this->Draw(D::Circle,x,y,r); }
	void FillCircle(int x, int y, int r) { this->Draw(D::FillCircle,x,y,r); }
	void DrawEllipse(int x, int y, int width, int height) { this->Draw(D::Ellipse,x,y,width,height); }
	void FillEllipse(int x, int y, int width, int height) { this->Draw(D::FillEllipse,x,y,width,height); }
	void DrawArc(int x, int y, int width, int height, int startAngle, int arcAngle) { this->Draw(D::Arc,x,y,width,height,startAngle,arcAngle); }

	void DrawHorzLine(int x1, int x2, int y) { this->Draw(D::HorzLine,x1,x2,y); }
	void DrawVertLine(int x, int y1, int y2) { this->Draw(D::VertLine,x,y1,y2); }
	void DrawLine(int x1, int y1, int x2, int y2) { this->Draw(D::Line,x1,y1,x2,y2); }

	void DrawRect(int x, int y, int width, int height) { this->Draw(D::DrawRect,x,y,width,height); }
	void FillRect(int x, int y, int width, int height) { this->Draw(D::FillRect,x,y,width,height); }
	void SetColor(DWORD theColor) { this->Draw(D::SetColor,(int)theColor); }
	DWORD GetColor() { return (DWORD)this->Get(EeWonSurfaceField::Color); }

	void Create(int theWidth, int theHeight) { ee_won_surface_create(this->mRust,theWidth,theHeight); }
	void Free() { ee_won_surface_release(this->mRust); }
	int GetPixelSize() { return this->Get(EeWonSurfaceField::PixelSize); }
	SurfaceFormat GetSurfaceFormat() { return (SurfaceFormat)this->Get(EeWonSurfaceField::Format); }
	int GetBytesPerRow() { return this->Get(EeWonSurfaceField::BytesPerRow); }
	int GetPixelsPerRow() { return this->Get(EeWonSurfaceField::PixelsPerRow); }

#ifdef WIN32_NOT_XBOX
	void BlitToDC(HDC theDC, int x, int y, int left, int top, int width, int height) { this->Blit(theDC,x,y,width,height,left,top,top,height); }
	void BlitToDC(HDC theDC) { this->Blit(theDC,0,0,this->GetWidth(),this->GetHeight(),0,0,0,this->GetHeight()); }
#endif

	int GetWidth() { return this->Get(EeWonSurfaceField::Width); }
	int GetHeight() { return this->Get(EeWonSurfaceField::Height); }

	void SetClipRect(int x, int y, int width, int height) { this->Draw(D::SetClipRect,x,y,width,height); }
	void ClearClipRect() { this->Draw(D::ClearClipRect); }

	void* GetPixelsBase() { return ee_won_surface_pixels(this->mRust); }
	void* GetPixels() { return ee_won_surface_pixels(this->mRust); }

	SurfaceBasePtr Duplicate() { return new SurfaceBase(ee_won_surface_duplicate(this->mRust)); }

	static void SetDefaultSurfaceFormat(SurfaceFormat theFormat) { ee_won_surface_set_default_format((int)theFormat); }
	static SurfaceBasePtr GetNewSurface(SurfaceFormat theFormat)
	{ EeWonSurface* aRust = ee_won_surface_new((int)theFormat); return aRust!=NULL ? new SurfaceBase(aRust) : NULL; }
	static SurfaceBasePtr GetNewDefaultSurface() { return GetNewSurfaceOf(ee_won_surface_new_default()); }
	static SurfaceBasePtr GetNewSurfaceOf(EeWonSurface* theRust) { return theRust!=NULL ? new SurfaceBase(theRust) : NULL; }

	int GetClipX1() { return this->Get(EeWonSurfaceField::ClipX1); }
	int GetClipX2() { return this->Get(EeWonSurfaceField::ClipX2); }
	int GetClipY1() { return this->Get(EeWonSurfaceField::ClipY1); }
	int GetClipY2() { return this->Get(EeWonSurfaceField::ClipY2); }
	void GetClipRect(int &cx1, int &cy1, int &cx2, int &cy2) { cx1 = this->GetClipX1(); cx2 = this->GetClipX2(); cy1 = this->GetClipY1(); cy2 = this->GetClipY2(); }
	bool NotClipped(int x, int y) { return x>=this->GetClipX1() && y>=this->GetClipY1() && x<=this->GetClipX2() && y<=this->GetClipY2(); }

	template <class PixelType> void PutPixel(int x, int y, PixelType *thePtr)
	{
		if(this->NotClipped(x,y))
			*thePtr = (PixelType)this->GetColor();
	}
};
typedef SurfaceBase Surface16;
typedef SurfaceBase Surface32;
typedef SmartPtr<Surface16> Surface16Ptr;
typedef SmartPtr<Surface32> Surface32Ptr;

#define SURFACE_32_TO_555(x) ee_won_surface_color_from32((x),SurfaceFormat_555)
#define SURFACE_555_TO_32(x) ee_won_surface_color_to32((x),SurfaceFormat_555)

#define SURFACE_32_TO_565(x) ee_won_surface_color_from32((x),SurfaceFormat_565)
#define SURFACE_565_TO_32(x) ee_won_surface_color_to32((x),SurfaceFormat_565)

} // namespace WONAPI;

#endif
