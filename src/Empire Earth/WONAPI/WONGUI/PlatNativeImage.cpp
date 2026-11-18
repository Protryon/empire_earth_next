// The WONAPI GUI's images on the platform layer (WONLobbyPlatform.h): native images in software surfaces, 1-bit
// masks, icons, cursors, DisplayContext::CreateImage and the raw images' platform conversions. Replaces the GDI
// backend's MSNativeImage.cpp, MSRawImage.cpp and DisplayContext.cpp; GDI's results are kept (a mask draws as
// SRCAND then SRCPAINT, a new bitmap is black, the alpha byte of a device bitmap reads back as 0). An image has a
// logical size (Image::mWidth/mHeight: what the GUI lays out, draws and clips in) and a pixel scale (its surface holds
// PlatGUIPixels(size, scale) pixels; WONLobbyPlatform.h "Pixel scale").
#include <windows.h>
#include <string>
#include <string.h>
#include <list>
#include <vector>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPlatform.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyImageDecoder.h"
#include "lle/GETextureManager.h"
#include "rust/won_gui/imgfilter.h"
#include "rust/won_gui/platcursor.h"
#include "rust/won_gui/platimg.h"

namespace WONAPI {

// ---- masks and icons

PlatNativeMask::PlatNativeMask(DisplayContext* inContext, int inWidth, int inHeight, int inBytesPerRow,
                               const unsigned char* inBits)
    : NativeMask(inContext), mWidth(inWidth), mHeight(inHeight), mBytesPerRow(inBytesPerRow)
{
    this->mBits.assign(inBits, inBits + (size_t)inBytesPerRow * (inHeight > 0 ? inHeight : 0));
}

PlatNativeMask::~PlatNativeMask() {}

PlatNativeIcon::~PlatNativeIcon() {}

// ---- native images

static SmartPtr<SurfaceBase> NewSurface(int inWidth, int inHeight)
{
    SmartPtr<SurfaceBase> theSurface = SurfaceBase::GetNewSurface(SurfaceFormat_32);
    if (inWidth < 0)
        inWidth = 0;
    if (inHeight < 0)
        inHeight = 0;
    theSurface->Create(inWidth, inHeight);
    if (theSurface->GetPixelsBase() != NULL)
        memset(theSurface->GetPixelsBase(), 0, (size_t)theSurface->GetBytesPerRow() * inHeight);
    return theSurface;
}

// A new image of the given logical size, black (CreateCompatibleBitmap), with its own display context on its surface
// of inScale times as many pixels
PlatNativeImage::PlatNativeImage(DisplayContext* inContext, int inWidth, int inHeight, float inScale)
    : NativeImage(inContext), mScale(inScale >= 1.0f ? inScale : 1.0f)
{
    this->mSurface = NewSurface(PlatGUIPixels(inWidth, this->mScale), PlatGUIPixels(inHeight, this->mScale));
    this->mWidth = (unsigned short)inWidth;
    this->mHeight = (unsigned short)inHeight;
    this->mDisplayContext = new DisplayContext(this->mSurface.get());
    this->mDisplayContext->mPixelScale = this->mScale;
}

PlatNativeImage::~PlatNativeImage()
{
    delete this->mGraphics;
    this->mGraphics = NULL;
}

uint32_t* PlatNativeImage::GetPixels() { return (uint32_t*)this->mSurface->GetPixelsBase(); }
int PlatNativeImage::GetPitchPixels() { return this->mSurface->GetPixelsPerRow(); }

Graphics* PlatNativeImage::PlatformGetGraphics() { return new PlatGraphics; }

// Copies part of the image into the context's surface, within its clip (BitBlt SRCCOPY; with a transparency mask
// SRCAND then SRCPAINT), resampled between surfaces of different pixel scales: ee_won's platimg.rs draw
void PlatNativeImage::Draw(DisplayContext* inContext, int x, int y, int left, int top, int width, int height)
{
    if (inContext == NULL || inContext->mSurface == NULL)
        return;
    PlatNativeMask* theMask = static_cast<PlatNativeMask*>(this->mTransparentMask.get());
    EeWonPlatMask theBits = theMask != NULL ? theMask->Rust() : EeWonPlatMask();
    ee_won_plat_img_draw(inContext->mSurface->GetRust(), inContext->mPixelScale, this->mSurface->GetRust(), this->mScale,
                         (int)this->GetWidth(), (int)this->GetHeight(), theMask != NULL ? &theBits : NULL, x, y, left, top,
                         width, height);
}

// A 32-bit raw copy (GetDIBits: the alpha byte reads 0); with a mask the raw image is transparent and its opaque
// pixels get alpha 0xff (platimg.rs raw)
SmartPtr<RawImage32> PlatNativeImage::GetRaw()
{
    SmartPtr<RawImage32> theImage = new RawImage32(this->GetWidth(), this->GetHeight());
    PlatNativeMask* theMask = static_cast<PlatNativeMask*>(this->mTransparentMask.get());
    EeWonPlatMask theBits = theMask != NULL ? theMask->Rust() : EeWonPlatMask();
    if (theMask != NULL)
        theImage->mDoTransparency = true;
    ee_won_plat_img_raw(this->mSurface->GetRust(), this->mScale, this->GetWidth(), this->GetHeight(),
                        theMask != NULL ? &theBits : NULL, (uint32_t*)theImage->mImageData);
    return theImage;
}

// A copy of the pixels sharing the transparency mask
SmartPtr<Image> PlatNativeImage::Duplicate()
{
    SmartPtr<PlatNativeImage> theCopy =
        new PlatNativeImage(this->mDisplayContext.get(), this->mWidth, this->mHeight, this->mScale);
    memcpy(theCopy->mSurface->GetPixelsBase(), this->mSurface->GetPixelsBase(),
           (size_t)this->mSurface->GetBytesPerRow() * this->mSurface->GetHeight());
    theCopy->mTransparentMask = this->mTransparentMask;
    return theCopy.get();
}

// Creates a blank (black) image of the given size for this display context, at the lobby surface's pixel scale (the
// windows' offscreen images and what the GUI composes for them)
SmartPtr<NativeImage> DisplayContext::CreateImage(int width, int height)
{
    PlatWindowManager* theManager = dynamic_cast<PlatWindowManager*>(WindowManager::GetDefaultWindowManager());
    return new PlatNativeImage(this, width, height, theManager != NULL ? theManager->GetPixelScale() : 1.0f);
}

// ---- the texture pack's copies of the lobby's images (the names, fit and reduction: platimg.rs)

SmartPtr<NativeImage> PlatGUIPackImage(DisplayContext* inContext, NativeImage* inImage, const char* inPath)
{
    long theShift = GETextureManager::GetTexturePackLogicalShift();
    if (inImage == NULL || inPath == NULL || theShift <= 0 || inImage->GetWidth() == 0 || inImage->GetHeight() == 0)
        return inImage;
    WindowManager* theManager = WindowManager::GetDefaultWindowManager();
    if (theManager == NULL)
        return inImage;
    SmartPtr<RawImage> theRaw;
    char theName[4096];
    for (int i = 0; i < 2 && theRaw.get() == NULL; i++) {
        if (ee_won_plat_pack_name((const uint8_t*)inPath, i, (uint8_t*)theName, sizeof(theName)) < 0)
            break;
        FileReader theReader;
        if (!theReader.Open(theName))
            continue;
        MultiImageDecoder* theDecoder = &theManager->mImageDecoder;
        theDecoder->SetFileName(theName);
        try {
            theRaw = theDecoder->ImageDecoder::DecodeRaw(theReader);
        } catch (FileReaderException&) {
            theRaw = NULL;
        }
    }
    if (theRaw.get() == NULL)
        return inImage;
    int theWidth = inImage->GetWidth(), theHeight = inImage->GetHeight(), theBox = 1;
    int theScale = ee_won_plat_pack_fit(theRaw->GetWidth(), theRaw->GetHeight(), theWidth, theHeight, (int)theShift, &theBox);
    if (theScale == 0)
        return inImage;
    SmartPtr<NativeImage> theNative = theRaw->GetNative(inContext);
    PlatNativeImage* thePack = dynamic_cast<PlatNativeImage*>(theNative.get());
    if (thePack == NULL || thePack->GetPixelWidth() != theRaw->GetWidth())
        return inImage;
    SmartPtr<PlatNativeImage> theImage = new PlatNativeImage(inContext, theWidth, theHeight, (float)theScale);
    // the original's transparency, exactly: its mask, and black under it (the mask draw ORs the image in there)
    theImage->mTransparentMask = inImage->mTransparentMask;
    PlatNativeMask* theMask = static_cast<PlatNativeMask*>(theImage->mTransparentMask.get());
    EeWonPlatMask theBits = theMask != NULL ? theMask->Rust() : EeWonPlatMask();
    ee_won_plat_pack_reduce(thePack->mSurface->GetRust(), theImage->mSurface->GetRust(), theWidth, theHeight, theScale, theBox,
                            theMask != NULL ? &theBits : NULL);
    return theImage.get();
}

// ---- raw image conversions (the platform parts of RawImage32/8/1)

// A native image of a raw image's pixels (platimg.rs from_raw): 32-bit without the alpha byte (CreateDIBitmap),
// 8-bit through the palette with the transparent index black, 1-bit white where a bit is set (a monochrome bitmap)
static SmartPtr<NativeImage> NativeOfRaw(DisplayContext* inContext, int inKind, const void* inData, int inPitch, int inWidth,
                                         int inHeight, Palette* inPalette, int inTransparent)
{
    SmartPtr<PlatNativeImage> theImage = new PlatNativeImage(inContext, inWidth, inHeight);
    EeWonRawPixels thePixels = {inKind, (uint8_t*)inData, inWidth, inHeight, inPitch};
    ee_won_plat_img_from_raw(&thePixels, inPalette != NULL ? (const uint32_t*)inPalette->mColors : NULL,
                             inPalette != NULL ? inPalette->GetNumColors() : 0, inTransparent, theImage->mSurface->GetRust());
    return theImage.get();
}

SmartPtr<NativeImage> RawImage32::CreateNativeImage(DisplayContext* context)
{
    return NativeOfRaw(context, 2, this->mImageData, this->mWidth * 4, this->GetWidth(), this->GetHeight(), NULL, -1);
}

// Sets the row stride to the width rounded up to a multiple of 4 bytes
void RawImage8::CalcBytesPerRow()
{
    this->mBytesPerRow = this->mWidth;
    if (this->mBytesPerRow % 4 != 0)
        this->mBytesPerRow += 4 - this->mBytesPerRow % 4;
}

SmartPtr<NativeImage> RawImage8::CreateNativeImage(DisplayContext* context)
{
    return NativeOfRaw(context, 1, this->mImageData, this->mBytesPerRow, this->mWidth, this->mHeight, this->mPalette.get(),
                       this->mTransparentColor);
}

// Sets the row stride to the unpadded row size rounded up to a multiple of 4 bytes
void RawImage1::CalcBytesPerRow()
{
    this->mBytesPerRow = this->mBytesPerRowMinusPad;
    if (this->mBytesPerRow % 4 != 0)
        this->mBytesPerRow += 4 - this->mBytesPerRow % 4;
}

// A native mask of the 1-bit pixels
SmartPtr<NativeMask> RawImage1::GetMask(DisplayContext* context)
{
    return new PlatNativeMask(context, this->mWidth, this->mHeight, this->mBytesPerRow, this->mImageData);
}

SmartPtr<NativeImage> RawImage1::GetNative(DisplayContext* context)
{
    return NativeOfRaw(context, 0, this->mImageData, this->mBytesPerRow, this->mWidth, this->mHeight, NULL, -1);
}

const EeWonPlatCursorFns PlatCursor::kFns = {(void (*)(uint32_t))PlatCursorSetSystem, PlatCursorCreate, PlatCursorSet,
                                             PlatCursorDestroy};

} // namespace WONAPI
