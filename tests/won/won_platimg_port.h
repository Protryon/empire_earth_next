#include "rust/won_gui/imgfilter.h"
#include "rust/won_gui/platimg.h"
// PlatNativeImage.cpp's statements after the port (R6 WON round 40: the forwarders to ee_won's platimg.rs), copied from
// the working tree by build/r40/gen_platimg.py; won_platimg_test.cpp compiles them over its stand-ins.

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

uint32_t* PlatNativeImage::GetPixels() { return (uint32_t*)this->mSurface->GetPixelsBase(); }

int PlatNativeImage::GetPitchPixels() { return this->mSurface->GetPixelsPerRow(); }

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

static void HandCursor(std::vector<uint32_t>& outHand)
{
    std::vector<uint32_t> theHand(32 * 32);
    ee_won_plat_hand_cursor(theHand.data());   // the WON hand (32x32, hot spot 12, 4; platimg.rs)
    outHand = theHand;
}
