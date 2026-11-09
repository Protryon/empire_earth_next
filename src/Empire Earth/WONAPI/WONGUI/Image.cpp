// Image base class, program-drawn images, image pieces, native images and masks, and the 32/8/1-bit raw
// images (file name inferred)
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
#include "ee/wonapi/WONLobbyImageHelpers.h"

// Image, NativeImage and RawImage are one original object: the base destructor ~Image is defined first, so
// VC6 knows it cannot throw and the derived destructors call it without the -1 EH state store.
namespace WONAPI {
// Destroys the image (empty; the linker folds this body into the identical one at 0x00728590)
Image::~Image() {}
// FUNCTION: WON 0x007285d0
// Draws nothing (the base image has no pixels of its own)
void Image::Draw(DisplayContext*,int,int,int,int,int,int) {}
// Ignores a resize (empty; the linker folds this body into the identical one at 0x006fa2a0)
void Image::SetSize(int,int) {}
// Returns no copy (the linker folds this body into the identical one at 0x0072b9d0)
SmartPtr<Image> Image::Duplicate() { return NULL; }
}

namespace WONAPI {
// FUNCTION: WON 0x007285e0
// Creates a native image of the same size on the display context and draws this image into it
SmartPtr<NativeImage> Image::GetNative(DisplayContext* context)
{
    SmartPtr<NativeImage> image=context->CreateImage(this->mWidth,this->mHeight);
    image->GetGraphics().DrawImage(this,0,0);
    return image;
}
// FUNCTION: WON 0x007286d0
// Returns a new program image with the same size and paint function
SmartPtr<Image> ProgramImage::Duplicate()
{
    return new ProgramImage(this->mWidth,this->mHeight,this->mPaintFunc);
}
ProgramImage::ProgramImage(int width,int height,PaintFunc paint)
{
    this->mWidth=(unsigned short)width;
    this->mHeight=(unsigned short)height;
    this->mPaintFunc=paint;
}
// FUNCTION: WON 0x007287c0
// Calls the paint function with the image's size, if one is set
void ProgramImage::Paint(Graphics& g)
{
    if(this->mPaintFunc!=NULL) this->mPaintFunc(g,this->mWidth,this->mHeight);
}
// FUNCTION: WON 0x007287f0
// Paints the image at (x, y) through the context's graphics, clipped to the source rectangle when it
// is not the whole image, then restores the translation
void ProgramImage::Draw(DisplayContext* context,int x,int y,int left,int top,int width,int height)
{
    Graphics* g=context->mGraphics;
    if(g==NULL) return;
    int tx=g->mTransX, ty=g->mTransY;
    g->Translate(x-tx,y-ty);
    bool clipped=false;
    if(left!=0 || top!=0 || width!=this->mWidth || height!=this->mHeight)
    {
        clipped=true;
        g->PushClipRect(left,top,width,height);
    }
    this->Paint(*g);
    if(clipped) g->PopClipRect();
    g->Translate(tx-x,ty-y);
}
}


// Image pieces and the size-less program image constructor (not identified in the original exe)
namespace WONAPI {
ImagePiece::ImagePiece(Image *theImage, int left, int top, int width, int height) : mImage(theImage)
{
	this->mLeft = left;
	this->mTop = top;
	this->mWidth = width;
	this->mHeight = height;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ImagePiece::~ImagePiece()
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void ImagePiece::Draw(DisplayContext *theContext, int x, int y, int left, int top, int width, int height)
{
	this->mImage->Draw(theContext,x,y,left+this->mLeft,top+this->mTop,width,height);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ProgramImage::ProgramImage(PaintFunc thePaintFunc)
{
	this->mWidth = this->mHeight = 0;
	this->mPaintFunc = thePaintFunc;
}

///////////////////////////////////////////////////////////////////////////////
}

using namespace WONAPI;

// FUNCTION: WON 0x007288b0
// Creates a native mask bound to the display context
NativeMask::NativeMask(DisplayContext* context) : mDisplayContext(context) {}
// FUNCTION: WON 0x00728920
// Destroys the native mask (members only)
NativeMask::~NativeMask() {}

// FUNCTION: WON 0x00728a70
// Creates a native image on the display context, with no graphics attached yet
NativeImage::NativeImage(DisplayContext* context) {
    this->mGraphics=NULL;
    this->mDisplayContext=context;
}

// FUNCTION: WON 0x00728b20
// Destroys the native image and its graphics object
NativeImage::~NativeImage() {
    delete this->mGraphics;
    this->mGraphics=NULL;
}

// FUNCTION: WON 0x00728bf0
// Returns this image (it is already native)
SmartPtr<NativeImage> NativeImage::GetNative(DisplayContext*) { return this; }

// FUNCTION: WON 0x00728c20
// Returns the image's graphics, creating one attached to its display context on first use
Graphics& NativeImage::GetGraphics() {
    if(this->mGraphics==NULL) {
        this->mGraphics=this->PlatformGetGraphics();
        this->mGraphics->Attach(this->mDisplayContext.get());
    }
    return *this->mGraphics;
}


// FUNCTION: WON 0x00728c50
// Draws the raw image by converting it to a native image for the context and drawing that
void RawImage::Draw(DisplayContext* context,int x,int y,int left,int top,int width,int height) {
    SmartPtr<NativeImage> native=this->GetNative(context);
    native->Draw(context,x,y,left,top,width,height);
}

// FUNCTION: WON 0x00728cf0
// Creates a 32-bit raw image of the given size with uninitialised pixels and transparency off
RawImage32::RawImage32(int width,int height) : RawImage(2) {
    this->mWidth=width;
    this->mHeight=height;
    this->mDoTransparency=false;
    this->mImageData=new DWORD[width*height];
    this->mImageEnd=this->mImageData+width*height;
}

// FUNCTION: WON 0x00728da0
// Destroys the 32-bit raw image and its pixel buffer
RawImage32::~RawImage32() { delete [] this->mImageData; }

// FUNCTION: WON 0x00728df0
// Returns a copy of the 32-bit image, pixels and transparency flag included
SmartPtr<Image> RawImage32::Duplicate() {
    SmartPtr<RawImage32> image=new RawImage32(this->mWidth,this->mHeight);
    image->mDoTransparency=this->mDoTransparency;
    memcpy(image->mImageData,this->mImageData,this->mWidth*this->mHeight*4);
    return image.get();
}

// FUNCTION: WON 0x00728ee0
// Returns the pixel at (x, y)
DWORD RawImage32::GetPixel(int x,int y) { return this->mImageData[y*this->mWidth+x]; }

// FUNCTION: WON 0x00728f00
// Starts a walk over the pixels of the given row and returns its first pixel
DWORD* RawImage32::StartRowTraversal(int row) {
    this->mRowPtr=this->mImageData+row*this->mWidth;
    this->mRowEnd=this->mRowPtr+this->mWidth;
    return this->mRowPtr;
}

// FUNCTION: WON 0x00729170
// Creates an 8-bit palettised raw image of the given size with padded rows and no transparent colour
RawImage8::RawImage8(int width,int height,Palette* palette) : RawImage(1) {
    this->mWidth=width;
    this->mHeight=height;
    this->mPalette=palette;
    this->mTransparentColor=-1;
    this->CalcBytesPerRow();
    this->mRowPad=this->mBytesPerRow-this->mWidth;
    this->mImageData=new unsigned char[height*this->mBytesPerRow];
    this->mImageEnd=this->mImageData+this->mBytesPerRow*height;
}

// FUNCTION: WON 0x00729280
// Destroys the 8-bit raw image and its pixel buffer (scalar delete of an array)
RawImage8::~RawImage8() { delete this->mImageData; }

// FUNCTION: WON 0x00729300
// Returns a copy of the 8-bit image with the same palette, transparent colour and pixels
SmartPtr<Image> RawImage8::Duplicate() {
    SmartPtr<RawImage8> image=new RawImage8(this->mWidth,this->mHeight,this->mPalette.get());
    image->mPalette=this->mPalette;
    image->mTransparentColor=this->mTransparentColor;
    memcpy(image->mImageData,this->mImageData,this->mBytesPerRow*this->mHeight);
    return image.get();
}

// FUNCTION: WON 0x00729420
// Returns the palette index at (x, y)
unsigned char RawImage8::GetPixel(int x,int y) { return this->mImageData[y*this->mBytesPerRow+x]; }

// FUNCTION: WON 0x00729440
// Starts a walk over the pixels of the given row and returns its first pixel
unsigned char* RawImage8::StartRowTraversal(int row) {
    this->mRowPtr=this->mImageData+row*this->mBytesPerRow;
    this->mRowEnd=this->mRowPtr+this->mWidth;
    return this->mRowPtr;
}

// FUNCTION: WON 0x00729690
// Creates a 1-bit raw image (mask) of the given size, computing its row padding and end-of-row bit mask
RawImage1::RawImage1(int width,int height) : RawImage(0) {
    this->mWidth=width;
    this->mHeight=height;
    this->mBytesPerRowMinusPad=this->mWidth/8;
    this->mCompleteBytesPerRow=this->mBytesPerRowMinusPad;
    this->mWidthMod8=this->mWidth%8;
    if(this->mWidthMod8!=0) ++this->mBytesPerRowMinusPad;
    this->CalcBytesPerRow();
    this->mRowPad=this->mBytesPerRow-this->mBytesPerRowMinusPad;
    if(this->mWidthMod8!=0) ++this->mRowPad;
    this->mEndRowMask=1<<(8-this->mWidthMod8);
    this->mImageData=new unsigned char[this->mBytesPerRow*this->mHeight];
    this->mImageEnd=this->mImageData+this->mBytesPerRow*this->mHeight;
}

// FUNCTION: WON 0x00729790
// Destroys the 1-bit raw image and its pixel buffer (scalar delete of an array)
RawImage1::~RawImage1() { delete this->mImageData; }

// FUNCTION: WON 0x007297e0
// Starts a bit walk at the first pixel of the given row and returns its first byte
unsigned char* RawImage1::StartRowTraversal(int row) {
    this->mRowMask=128;
    this->mRowPtr=this->mImageData+row*this->mBytesPerRow;
    this->mRowEnd=this->mRowPtr+this->mCompleteBytesPerRow;
    return this->mRowPtr;
}


// FUNCTION: WON 0x00728f30
// Creates a native image; with transparency on, first clears every pixel whose alpha is 0 and attaches
// a mask of those pixels
SmartPtr<NativeImage> RawImage32::GetNative(DisplayContext* context) {
    if(this->mDoTransparency) {
        int width=this->GetWidth(),height=this->GetHeight();
        SmartPtr<RawImage1> mask=new RawImage1(width,height);
        for(int y=0;y<height;++y) {
            mask->StartRowTraversal(y);
            this->StartRowTraversal(y);
            while(this->mRowPtr<this->mRowEnd) {
                if((*this->mRowPtr&0xff000000)==0) {
                    *this->mRowPtr=0;
                    mask->SetRowPixel(true);
                } else mask->SetRowPixel(false);
                ++this->mRowPtr;
                mask->NextRowPixel();
            }
        }
        SmartPtr<NativeImage> native=this->CreateNativeImage(context);
        native->SetTransparentMask(mask->GetMask(context).get());
        return native;
    }
    return this->CreateNativeImage(context);
}

// FUNCTION: WON 0x00729460
// Creates a native image and, when a transparent colour is set, attaches a mask of the pixels of that colour
SmartPtr<NativeImage> RawImage8::GetNative(DisplayContext* context) {
    SmartPtr<NativeImage> native=this->CreateNativeImage(context);
    if(this->mTransparentColor!=-1) {
        SmartPtr<RawImage1> mask=new RawImage1(this->mWidth,this->mHeight);
        for(int y=0;y<this->mHeight;++y) {
            this->StartRowTraversal(y);
            mask->StartRowTraversal(y);
            while(this->mRowPtr<this->mRowEnd) {
                mask->SetRowPixel(this->GetRowPixel()==this->mTransparentColor);
                mask->NextRowPixel();
                this->NextRowPixel();
            }
        }
        native->SetTransparentMask(mask->GetMask(context).get());
    }
    return native;
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00728790
// (public: __thiscall WONAPI::ProgramImage::ProgramImage(int,int,void (__cdecl*)(class WONAPI::Graphics &,int,int))    [??0ProgramImage@WONAPI@@QAE@HHP6A)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00728900
// (public: virtual void * __thiscall WONAPI::NativeMask::`scalar deleting destructor'(unsigned int)    [??_GNativeMask@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00728b00
// (public: virtual void * __thiscall WONAPI::NativeImage::`scalar deleting destructor'(unsigned int)    [??_GNativeImage@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00728d80
// (public: virtual void * __thiscall WONAPI::RawImage32::`scalar deleting destructor'(unsigned int)    [??_GRawImage32@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00729260
// (public: virtual void * __thiscall WONAPI::RawImage8::`scalar deleting destructor'(unsigned int)    [??_GRawImage8@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00729770
// (public: virtual void * __thiscall WONAPI::RawImage1::`scalar deleting destructor'(unsigned int)    [??_GRawImage1@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00728d70
// (public: virtual bool __thiscall WONAPI::RawImage32::HasTransparency(void))
// TEMPLATE: WON 0x00729250
// (public: virtual bool __thiscall WONAPI::RawImage8::HasTransparency(void))
