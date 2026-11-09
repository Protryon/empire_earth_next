// Win32 bitmap images, masks and icons: ownership, drawing and conversion
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
// FUNCTION: WON 0x007cfe50
// Returns a 32-bit raw copy of the bitmap; with a transparency mask, opaque pixels get alpha 0xff
SmartPtr<RawImage32> MSNativeImage::GetRaw() {
    SmartPtr<RawImage32> image=new RawImage32(this->GetWidth(),this->GetHeight());
    this->SelectBitmap(false);
    char infoBuffer[sizeof(BITMAPINFOHEADER)+12];
    BITMAPINFO& info=*reinterpret_cast<BITMAPINFO*>(infoBuffer);
    BITMAPINFOHEADER& header=info.bmiHeader;
    header.biSize=sizeof(BITMAPINFOHEADER);
    header.biWidth=this->GetWidth();
    header.biHeight=-this->GetHeight();
    header.biPlanes=1;
    header.biBitCount=32;
    header.biCompression=BI_RGB;
    GetDIBits(this->mDC,this->mBitmap,0,this->GetHeight(),image->GetImageData(),&info,DIB_RGB_COLORS);
    if(this->mTransparentMask.get()!=NULL) {
        image->mDoTransparency=true;
        SmartPtr<RawImage1> mask=new RawImage1(this->GetWidth(),this->GetHeight());
        header.biBitCount=1;
        GetDIBits(this->mDC,((MSNativeMask*)this->mTransparentMask.get())->GetMask(),0,this->GetHeight(),
            mask->GetImageData(),&info,DIB_RGB_COLORS);
        mask->StartRowTraversal(0);
        image->StartRowTraversal(0);
        for(int row=0;row<this->mHeight;++row) {
            while(image->HasMoreRowPixels()) {
                bool transparent=mask->GetRowPixel();
                if(!transparent) image->SetRowPixel(image->GetRowPixel()|0xff000000);
                image->NextRowPixel();
                mask->NextRowPixel();
            }
            image->NextRow();
            mask->NextRow();
        }
    }
    this->SelectBitmap(true);
    return image.get();
}

// FUNCTION: WON 0x007cf9b0
// Destroys the mask, deleting its bitmap
MSNativeMask::~MSNativeMask()
{
	DeleteObject(this->mMask); 
}

// FUNCTION: WON 0x007cf9f0
// Destroys the icon, releasing its Win32 icon
MSNativeIcon::~MSNativeIcon()
{
	DestroyIcon(this->mIcon);
}

// FUNCTION: WON 0x007cfa10
// Selects the bitmap into the image's memory DC or restores the previous bitmap, if not already so
void MSNativeImage::SelectBitmap(bool select)
{
	if(this->mBitmapIsSelected==select)
		return;

	if(select)
		this->mOldBitmap = (HBITMAP)SelectObject(this->mDC, this->mBitmap);
	else
		SelectObject(this->mDC,this->mOldBitmap);

	this->mBitmapIsSelected = select;
}

// FUNCTION: WON 0x007cfa50
// Takes ownership of the bitmap and mask, creates a memory DC compatible with the context, selects the
// bitmap and reads its size
void MSNativeImage::Init(DisplayContext *theContext, HBITMAP theBitmap, MSNativeMask *theTransparentMask)
{	
	this->mBitmap = theBitmap;
	this->mTransparentMask = theTransparentMask;
	this->mBitmapIsSelected = false;

	this->mDC = CreateCompatibleDC(theContext->mDC);
	this->mDisplayContext = new DisplayContext(this->mDC);

	this->SelectBitmap(true);

	BITMAP     bm;
	POINT      size;
	GetObject(this->mBitmap, sizeof(BITMAP), (LPSTR)&bm);
	
	size.x = bm.bmWidth;            // Get width of bitmap
	size.y = bm.bmHeight;           // Get height of bitmap
	DPtoLP(this->mDC, &size, 1);			// Convert from
	this->mWidth = size.x;
	this->mHeight = size.y;
}

// FUNCTION: WON 0x007cfb70
// Creates a native image owning the given bitmap and optional transparency mask
MSNativeImage::MSNativeImage(DisplayContext *theContext, HBITMAP theBitmap, MSNativeMask *theTransparentMask) : NativeImage(theContext)
{
	this->Init(theContext, theBitmap, theTransparentMask);
}

// FUNCTION: WON 0x007cfbf0
// Creates a native image with a new bitmap of the given size, compatible with the context
MSNativeImage::MSNativeImage(DisplayContext* context,int width,int height) : NativeImage(context) {
    this->Init(context,CreateCompatibleBitmap(context->mDC,width,height),NULL);
}

// FUNCTION: WON 0x007cfc60
// Destroys the image: its graphics, memory DC and bitmap
MSNativeImage::~MSNativeImage()
{
	delete this->mGraphics;
	this->mGraphics = NULL;
	DeleteDC(this->mDC);
	DeleteObject(this->mBitmap);
}

// FUNCTION: WON 0x007cfce0
// Returns a new GDI graphics context for drawing into the image
Graphics* MSNativeImage::PlatformGetGraphics()
{
	return new MSGraphics;
}

// FUNCTION: WON 0x007cfd40
// Copies part of the image to the context; with a mask, masks the destination first so masked pixels stay
// transparent
void MSNativeImage::Draw(DisplayContext *theContext, int x, int y, int left, int top, int width, int height)
{
	if(!this->mBitmapIsSelected)
		this->SelectBitmap(true);

	HDC theDC = theContext->mDC;
	if(this->mTransparentMask.get()!=NULL)
	{
		HDC aMaskDC = CreateCompatibleDC(theDC);
		SelectObject(aMaskDC,((MSNativeMask*)this->mTransparentMask.get())->mMask);

		DWORD aTextColor = SetTextColor(theDC,0x000000); //off/non-transparent pixels -> black
		DWORD aBackColor = SetBkColor(theDC,0xFFFFFF);	// on/transparent pixels -> white
		
		BitBlt(theDC,x,y,width,height,aMaskDC,left,top,SRCAND);
		BitBlt(theDC,x,y,width,height,this->mDC,left,top,SRCPAINT);

		SetBkColor(theDC, aBackColor);
		SetTextColor(theDC, aTextColor);

		DeleteDC(aMaskDC);
	}
	else
		BitBlt(theDC, x, y, width, height, this->mDC, left, top, SRCCOPY);
}

// FUNCTION: WON 0x007d0130
// Returns a copy of the image with a new bitmap, sharing the same transparency mask
SmartPtr<Image> MSNativeImage::Duplicate()
{
	this->SelectBitmap(true);
	HDC aCopyDC = CreateCompatibleDC(this->mDC);
	HBITMAP aBitmap = CreateCompatibleBitmap(this->mDC,this->mWidth,this->mHeight);
	SelectObject(aCopyDC,aBitmap);
	BitBlt(aCopyDC,0,0,this->mWidth,this->mHeight,this->mDC,0,0,SRCCOPY);
	DeleteDC(aCopyDC);

	return new MSNativeImage(this->mDisplayContext.get(), aBitmap, (MSNativeMask*)this->mTransparentMask.get());
}

// FUNCTION: WON 0x007d0350
// Returns a new icon made from the bitmap and its mask
SmartPtr<NativeIcon> MSNativeImage::GetIcon()
{
	ICONINFO anInfo;
	anInfo.fIcon = TRUE;
	anInfo.xHotspot = 0; 
	anInfo.yHotspot = 0; 
	anInfo.hbmMask = NULL;
	if(this->mTransparentMask.get()!=NULL)
		anInfo.hbmMask = ((MSNativeMask*)this->mTransparentMask.get())->GetMask();
   
	anInfo.hbmColor = this->mBitmap; 

	HICON anIcon = CreateIconIndirect(&anInfo);
	return new MSNativeIcon(this->mDisplayContext.get(), anIcon);
}

// FUNCTION: WON 0x007d0230
// Returns a new cursor made from the bitmap and its mask; the hotspot is always (0, 0), the requested
// one is ignored
SmartPtr<Cursor> MSNativeImage::GetCursor(int hotX,int hotY) {
    ICONINFO info;
    info.fIcon=FALSE;
    info.xHotspot=0;
    info.yHotspot=0;
    info.hbmMask=NULL;
    if(this->mTransparentMask.get()!=NULL)
        info.hbmMask=((MSNativeMask*)this->mTransparentMask.get())->GetMask();
    info.hbmColor=this->mBitmap;
    return new MSCursor((HCURSOR)CreateIconIndirect(&info),true);
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007cf9d0
// (public: virtual void * __thiscall WONAPI::MSNativeMask::`scalar deleting destructor'(unsigned int)    [??_GMSNativeMask@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007cfbd0
// (public: virtual void * __thiscall WONAPI::MSNativeImage::`scalar deleting destructor'(unsigned int)    [??_GMSNativeImage@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007d0300
// (protected: virtual void * __thiscall WONAPI::MSCursor::`scalar deleting destructor'(unsigned int)    [??_GMSCursor@WONAPI@@MAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007d02f0
// (public: virtual class WONAPI::Cursor * __thiscall WONAPI::MSCursor::GetNative(void))
