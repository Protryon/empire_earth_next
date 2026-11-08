// The WONAPI GUI's images on the platform layer (WONLobbyPlatform.h): native images in software surfaces, 1-bit
// masks, icons, cursors, DisplayContext::CreateImage and the raw images' platform conversions. Replaces the GDI
// backend's MSNativeImage.cpp, MSRawImage.cpp and DisplayContext.cpp; GDI's results are kept (a mask draws as
// SRCAND then SRCPAINT, a new bitmap is black, the alpha byte of a device bitmap reads back as 0).
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

// A new image of the given size, black (CreateCompatibleBitmap), with its own display context on its surface
PlatNativeImage::PlatNativeImage(DisplayContext* inContext, int inWidth, int inHeight) : NativeImage(inContext)
{
    this->mSurface = NewSurface(inWidth, inHeight);
    this->mWidth = (unsigned short)inWidth;
    this->mHeight = (unsigned short)inHeight;
    this->mDisplayContext = new DisplayContext(this->mSurface.get());
}

PlatNativeImage::~PlatNativeImage()
{
    delete this->mGraphics;
    this->mGraphics = NULL;
}

uint32_t* PlatNativeImage::GetPixels() { return (uint32_t*)this->mSurface->GetPixelsBase(); }
int PlatNativeImage::GetPitchPixels() { return this->mSurface->GetPixelsPerRow(); }

Graphics* PlatNativeImage::PlatformGetGraphics() { return new PlatGraphics; }

// Copies part of the image into the context's surface, within its clip (BitBlt SRCCOPY); with a transparency mask
// the destination is first ANDed with the mask (set bits: white, kept) and then ORed with the image (SRCAND,
// SRCPAINT): a masked pixel keeps the destination when the image is black there, as the raw images make it.
void PlatNativeImage::Draw(DisplayContext* inContext, int x, int y, int left, int top, int width, int height)
{
    if (inContext == NULL || inContext->mSurface == NULL)
        return;
    SurfaceBase* theDest = inContext->mSurface;
    SurfaceBase* theSource = this->mSurface.get();
    // the source rectangle inside the image (GDI reads nothing outside a bitmap)
    if (left < 0) { x -= left; width += left; left = 0; }
    if (top < 0) { y -= top; height += top; top = 0; }
    if (left + width > theSource->GetWidth())
        width = theSource->GetWidth() - left;
    if (top + height > theSource->GetHeight())
        height = theSource->GetHeight() - top;
    // the destination clip
    int theClipX1, theClipY1, theClipX2, theClipY2;
    theDest->GetClipRect(theClipX1, theClipY1, theClipX2, theClipY2);
    if (x < theClipX1) { left += theClipX1 - x; width -= theClipX1 - x; x = theClipX1; }
    if (y < theClipY1) { top += theClipY1 - y; height -= theClipY1 - y; y = theClipY1; }
    if (x + width - 1 > theClipX2)
        width = theClipX2 - x + 1;
    if (y + height - 1 > theClipY2)
        height = theClipY2 - y + 1;
    if (width <= 0 || height <= 0)
        return;
    const uint32_t* theSourcePixels = (const uint32_t*)theSource->GetPixelsBase();
    int theSourcePitch = theSource->GetPixelsPerRow();
    uint32_t* theDestPixels = (uint32_t*)theDest->GetPixelsBase();
    int theDestPitch = theDest->GetPixelsPerRow();
    PlatNativeMask* theMask = static_cast<PlatNativeMask*>(this->mTransparentMask.get());
    for (int j = 0; j < height; j++) {
        const uint32_t* theFrom = theSourcePixels + (size_t)(top + j) * theSourcePitch + left;
        uint32_t* theTo = theDestPixels + (size_t)(y + j) * theDestPitch + x;
        if (theMask == NULL || top + j >= theMask->mHeight) {
            memcpy(theTo, theFrom, (size_t)width * 4);
            continue;
        }
        for (int i = 0; i < width; i++) {
            int theX = left + i;
            bool theTransparent = theX < theMask->mWidth && theMask->IsTransparent(theX, top + j);
            theTo[i] = (theTransparent ? theTo[i] : 0) | theFrom[i];
        }
    }
}

// A 32-bit raw copy (GetDIBits: the alpha byte reads 0); with a mask the raw image is transparent and its opaque
// pixels get alpha 0xff
SmartPtr<RawImage32> PlatNativeImage::GetRaw()
{
    int theWidth = this->GetWidth(), theHeight = this->GetHeight();
    SmartPtr<RawImage32> theImage = new RawImage32(theWidth, theHeight);
    const uint32_t* thePixels = this->GetPixels();
    int thePitch = this->GetPitchPixels();
    PlatNativeMask* theMask = static_cast<PlatNativeMask*>(this->mTransparentMask.get());
    if (theMask != NULL)
        theImage->mDoTransparency = true;
    for (int y = 0; y < theHeight; y++) {
        DWORD* theRow = theImage->mImageData + (size_t)y * theWidth;
        for (int x = 0; x < theWidth; x++) {
            DWORD thePixel = thePixels[(size_t)y * thePitch + x] & 0x00FFFFFF;
            if (theMask != NULL && !(x < theMask->mWidth && y < theMask->mHeight && theMask->IsTransparent(x, y)))
                thePixel |= 0xff000000;
            theRow[x] = thePixel;
        }
    }
    return theImage;
}

// A copy of the pixels sharing the transparency mask
SmartPtr<Image> PlatNativeImage::Duplicate()
{
    SmartPtr<PlatNativeImage> theCopy = new PlatNativeImage(this->mDisplayContext.get(), this->mWidth, this->mHeight);
    memcpy(theCopy->mSurface->GetPixelsBase(), this->mSurface->GetPixelsBase(),
           (size_t)this->mSurface->GetBytesPerRow() * this->mHeight);
    theCopy->mTransparentMask = this->mTransparentMask;
    return theCopy.get();
}

// An icon of the image and its mask (the lobby surface has no window icons to show it in)
SmartPtr<NativeIcon> PlatNativeImage::GetIcon()
{
    return new PlatNativeIcon(this->mDisplayContext.get(), this);
}

// A cursor from the image and its mask; as the original, the hot spot is (0, 0) whatever is asked for
SmartPtr<Cursor> PlatNativeImage::GetCursor(int, int)
{
    SmartPtr<RawImage32> theRaw = this->GetRaw();
    int theCount = this->GetWidth() * this->GetHeight();
    std::vector<uint32_t> thePixels(theRaw->mImageData, theRaw->mImageData + theCount);
    if (!theRaw->mDoTransparency)
        for (int i = 0; i < theCount; i++)
            thePixels[i] |= 0xff000000;
    return new PlatCursor(thePixels.data(), this->GetWidth(), this->GetHeight(), 0, 0);
}

// Creates a blank (black) image of the given size for this display context
SmartPtr<NativeImage> DisplayContext::CreateImage(int width, int height)
{
    return new PlatNativeImage(this, width, height);
}

// ---- raw image conversions (the platform parts of RawImage32/8/1)

// A native image of the 32-bit pixels (CreateDIBitmap: the alpha byte is not kept)
SmartPtr<NativeImage> RawImage32::CreateNativeImage(DisplayContext* context)
{
    SmartPtr<PlatNativeImage> theImage = new PlatNativeImage(context, this->GetWidth(), this->GetHeight());
    uint32_t* thePixels = theImage->GetPixels();
    int thePitch = theImage->GetPitchPixels();
    for (int y = 0; y < this->mHeight; y++)
        for (int x = 0; x < this->mWidth; x++)
            thePixels[(size_t)y * thePitch + x] = this->mImageData[(size_t)y * this->mWidth + x] & 0x00FFFFFF;
    return theImage.get();
}

// Sets the row stride to the width rounded up to a multiple of 4 bytes
void RawImage8::CalcBytesPerRow()
{
    this->mBytesPerRow = this->mWidth;
    if (this->mBytesPerRow % 4 != 0)
        this->mBytesPerRow += 4 - this->mBytesPerRow % 4;
}

// A native image of the 8-bit pixels through the palette; the transparent index, if any, is black
SmartPtr<NativeImage> RawImage8::CreateNativeImage(DisplayContext* context)
{
    SmartPtr<PlatNativeImage> theImage = new PlatNativeImage(context, this->mWidth, this->mHeight);
    uint32_t* thePixels = theImage->GetPixels();
    int thePitch = theImage->GetPitchPixels();
    int theNumColors = this->mPalette.get() != NULL ? this->mPalette->GetNumColors() : 0;
    for (int y = 0; y < this->mHeight; y++) {
        const unsigned char* theRow = this->mImageData + (size_t)y * this->mBytesPerRow;
        for (int x = 0; x < this->mWidth; x++) {
            int theIndex = theRow[x];
            uint32_t theColor = 0;
            if (theIndex < theNumColors && theIndex != this->mTransparentColor)
                theColor = this->mPalette->mColors[theIndex] & 0x00FFFFFF;
            thePixels[(size_t)y * thePitch + x] = theColor;
        }
    }
    return theImage.get();
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

// A native image showing the mask: set bits white, clear bits black (a monochrome bitmap)
SmartPtr<NativeImage> RawImage1::GetNative(DisplayContext* context)
{
    SmartPtr<PlatNativeMask> theMask = static_cast<PlatNativeMask*>(this->GetMask(context).get());
    SmartPtr<PlatNativeImage> theImage = new PlatNativeImage(context, this->mWidth, this->mHeight);
    uint32_t* thePixels = theImage->GetPixels();
    int thePitch = theImage->GetPitchPixels();
    for (int y = 0; y < this->mHeight; y++)
        for (int x = 0; x < this->mWidth; x++)
            thePixels[(size_t)y * thePitch + x] = theMask->IsTransparent(x, y) ? 0x00FFFFFF : 0;
    return theImage.get();
}

// ---- cursors

static PlatCursor* gAppliedCursor = NULL;   // the cursor last made the pointer's shape

PlatCursor::PlatCursor(PlatSystemCursor inSystem)
    : mIsSystem(true), mSystem(inSystem), mWidth(0), mHeight(0), mHotX(0), mHotY(0), mHandle(NULL), mFailed(false) {}

PlatCursor::PlatCursor(const uint32_t* inPixels, int inWidth, int inHeight, int inHotX, int inHotY)
    : mIsSystem(false), mSystem(PLAT_CURSOR_ARROW), mPixels(inPixels, inPixels + inWidth * inHeight), mWidth(inWidth),
      mHeight(inHeight), mHotX(inHotX), mHotY(inHotY), mHandle(NULL), mFailed(false) {}

PlatCursor::~PlatCursor()
{
    if (gAppliedCursor == this) {
        PlatCursorSet(NULL);
        gAppliedCursor = NULL;
    }
    if (this->mHandle != NULL)
        PlatCursorDestroy(this->mHandle);
}

void PlatCursor::Apply()
{
    if (!this->mIsSystem && this->mHandle == NULL && !this->mFailed) {
        this->mHandle = PlatCursorCreate(this->mPixels.data(), this->mWidth, this->mHeight, this->mHotX, this->mHotY);
        this->mFailed = this->mHandle == NULL;
    }
    if (this->mHandle != NULL)
        PlatCursorSet(this->mHandle);
    else
        PlatCursorSetSystem(this->mIsSystem ? this->mSystem : PLAT_CURSOR_HAND);
    gAppliedCursor = this;
}

} // namespace WONAPI
