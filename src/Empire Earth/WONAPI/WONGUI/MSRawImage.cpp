// Conversion of raw 32-bit, 8-bit and 1-bit images to Win32 bitmaps
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <memory>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
using namespace WONAPI;

// FUNCTION: WON 0x007d0410
// Returns a new native image holding a top-down 32-bit device bitmap made from the pixel data
SmartPtr<NativeImage> RawImage32::CreateNativeImage(DisplayContext* context) {
    BITMAPINFO info;
    BITMAPINFOHEADER& header=info.bmiHeader;
    header.biSize=sizeof(BITMAPINFOHEADER);
    header.biWidth=this->GetWidth();
    header.biHeight=-this->GetHeight();
    header.biPlanes=1;
    header.biBitCount=32;
    header.biCompression=BI_RGB;
    HBITMAP bitmap=CreateDIBitmap(context->mDC,&header,CBM_INIT,this->mImageData,&info,DIB_RGB_COLORS);
    return new MSNativeImage(context,bitmap);
}

// FUNCTION: WON 0x007d04f0
// Sets the row stride to the width rounded up to a multiple of 4 bytes
void RawImage8::CalcBytesPerRow() {
    this->mBytesPerRow=this->mWidth;
    if(this->mBytesPerRow%4!=0) this->mBytesPerRow+=4-this->mBytesPerRow%4;
}

// FUNCTION: WON 0x007d0520
// Returns a new native image from the 8-bit pixels and palette; the transparent index, if any, maps to
// black
SmartPtr<NativeImage> RawImage8::CreateNativeImage(DisplayContext* context) {
    int aNumColors=this->mPalette->mNumColors;
    char* buffer=new char[sizeof(BITMAPINFO)+aNumColors*4];
    std::auto_ptr<char> releaseBuffer(buffer);
    BITMAPINFO* info=reinterpret_cast<BITMAPINFO*>(buffer);
    BITMAPINFOHEADER& header=info->bmiHeader;
    header.biSize=sizeof(BITMAPINFOHEADER);
    header.biWidth=this->mWidth;
    header.biHeight=-this->mHeight;
    header.biPlanes=1;
    header.biBitCount=8;
    header.biCompression=BI_RGB;
    header.biSizeImage=0;
    header.biXPelsPerMeter=0;
    header.biYPelsPerMeter=0;
    header.biClrUsed=this->mPalette->GetNumColors();
    header.biClrImportant=0;
    for(int i=0;i<this->mPalette->GetNumColors();++i) {
        DWORD rgb=this->mPalette->mColors[i];
        RGBQUAD color;
        color.rgbRed=(rgb>>16)&0xff;
        color.rgbGreen=(rgb>>8)&0xff;
        color.rgbBlue=rgb&0xff;
        color.rgbReserved=0;
        info->bmiColors[i]=color;
    }
    if(this->mTransparentColor!=-1 && this->mTransparentColor<this->mPalette->mNumColors) {
        RGBQUAD black={0,0,0,0};
        info->bmiColors[this->mTransparentColor]=black;
    }
    HBITMAP bitmap=CreateDIBitmap(context->mDC,&header,CBM_INIT,this->mImageData,info,DIB_RGB_COLORS);
    return new MSNativeImage(context,bitmap);
}

// FUNCTION: WON 0x007d0690
// Sets the row stride to the unpadded row size rounded up to a multiple of 4 bytes
void RawImage1::CalcBytesPerRow() {
    this->mBytesPerRow=this->mBytesPerRowMinusPad;
    if(this->mBytesPerRow%4!=0) this->mBytesPerRow+=4-this->mBytesPerRow%4;
}

// FUNCTION: WON 0x007d06b0
// Returns a new native mask holding a black-and-white device bitmap made from the 1-bit pixels
SmartPtr<NativeMask> RawImage1::GetMask(DisplayContext* context) {
    char* buffer=new char[sizeof(BITMAPINFO)+8];
    std::auto_ptr<char> releaseBuffer(buffer);
    BITMAPINFO* info=reinterpret_cast<BITMAPINFO*>(buffer);
    BITMAPINFOHEADER& header=info->bmiHeader;
    header.biSize=sizeof(BITMAPINFOHEADER);
    header.biWidth=this->mWidth;
    header.biHeight=-this->mHeight;
    header.biPlanes=1;
    header.biBitCount=1;
    header.biCompression=BI_RGB;
    header.biSizeImage=0;
    header.biXPelsPerMeter=0;
    header.biYPelsPerMeter=0;
    header.biClrUsed=2;
    header.biClrImportant=0;
    RGBQUAD black={0,0,0,0},white={0xff,0xff,0xff,0};
    info->bmiColors[0]=black;
    info->bmiColors[1]=white;
    HBITMAP bitmap=CreateDIBitmap(context->mDC,&header,CBM_INIT,this->mImageData,info,DIB_RGB_COLORS);
    return new MSNativeMask(context,bitmap);
}

// FUNCTION: WON 0x007d07e0
// Returns a native image showing the mask bitmap; the temporary mask then tries to delete that bitmap,
// which fails because the new image has it selected
SmartPtr<NativeImage> RawImage1::GetNative(DisplayContext* context) {
    return new MSNativeImage(context,static_cast<MSNativeMask*>(this->GetMask(context).get())->GetMask());
}
