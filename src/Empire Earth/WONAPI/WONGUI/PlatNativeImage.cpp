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
#include <ctype.h>
#include <math.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPlatform.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyImageDecoder.h"
#include "lle/GETextureManager.h"

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

// Copies part of the image into the context's surface, within its clip (BitBlt SRCCOPY); with a transparency mask
// the destination is first ANDed with the mask (set bits: white, kept) and then ORed with the image (SRCAND,
// SRCPAINT): a masked pixel keeps the destination when the image is black there, as the raw images make it.
// Everything is in logical pixels; between surfaces of different pixel scales the image is resampled (DrawScaled).
void PlatNativeImage::Draw(DisplayContext* inContext, int x, int y, int left, int top, int width, int height)
{
    if (inContext == NULL || inContext->mSurface == NULL)
        return;
    SurfaceBase* theDest = inContext->mSurface;
    SurfaceBase* theSource = this->mSurface.get();
    // the source rectangle inside the image (GDI reads nothing outside a bitmap)
    if (left < 0) { x -= left; width += left; left = 0; }
    if (top < 0) { y -= top; height += top; top = 0; }
    if (left + width > (int)this->GetWidth())
        width = (int)this->GetWidth() - left;
    if (top + height > (int)this->GetHeight())
        height = (int)this->GetHeight() - top;
    if (width <= 0 || height <= 0 || theDest->GetPixelsBase() == NULL || theSource->GetPixelsBase() == NULL)
        return;
    float theDestScale = inContext->mPixelScale > 0.0f ? inContext->mPixelScale : 1.0f;
    if (theDestScale != 1.0f || this->mScale != 1.0f) {
        this->DrawScaled(theDest, theDestScale, x, y, left, top, width, height);
        return;
    }
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

// One axis of a scaled draw for one destination pixel: the logical source pixel it lies in (the mask's), the two
// source pixels it is filtered from with the second's weight (0..256), and their logical pixels
struct PlatScaleTap {
    int mLogical;
    int mPixel0, mPixel1;
    int mWeight1;
    int mLogical0, mLogical1;
};

// The taps of destination pixels inFrom..inTo-1 drawing logical source cells inStart..inStart+inCount-1 placed at
// logical inAt, at destination scale inDest and source scale inSource. Equal scales with equal pixel extents copy
// pixel for pixel; otherwise bilinear, clamped to the source rectangle's pixels.
static void PlatScaleTaps(std::vector<PlatScaleTap>& outTaps, int inFrom, int inTo, int inAt, int inStart, int inCount,
                          float inDest, float inSource)
{
    outTaps.resize(inTo > inFrom ? (size_t)(inTo - inFrom) : 0);
    int theDestStart = PlatGUIPixels(inAt, inDest), theDestEnd = PlatGUIPixels(inAt + inCount, inDest);
    int theFirst = PlatGUIPixels(inStart, inSource), theLast = PlatGUIPixels(inStart + inCount, inSource) - 1;
    if (theLast < theFirst)
        theLast = theFirst;
    bool theExact = inDest == inSource && theDestEnd - theDestStart == theLast - theFirst + 1;
    for (int p = inFrom; p < inTo; p++) {
        PlatScaleTap& t = outTaps[(size_t)(p - inFrom)];
        float theLogical = ((float)p + 0.5f) / inDest - (float)inAt + (float)inStart;   // in the image
        if (theExact) {
            t.mPixel0 = t.mPixel1 = theFirst + (p - theDestStart);
            t.mWeight1 = 0;
        } else {
            float theCentre = theLogical * inSource - 0.5f;
            if (theCentre < (float)theFirst)
                theCentre = (float)theFirst;
            if (theCentre > (float)theLast)
                theCentre = (float)theLast;
            int theBase = (int)floorf(theCentre);
            t.mPixel0 = theBase;
            t.mPixel1 = theBase + 1 <= theLast ? theBase + 1 : theBase;
            t.mWeight1 = (int)((theCentre - (float)theBase) * 256.0f + 0.5f);
        }
        if (t.mPixel0 < theFirst) t.mPixel0 = theFirst;
        if (t.mPixel0 > theLast) t.mPixel0 = theLast;
        if (t.mPixel1 < theFirst) t.mPixel1 = theFirst;
        if (t.mPixel1 > theLast) t.mPixel1 = theLast;
        int theCell = (int)floorf(theLogical);
        t.mLogical = theCell < inStart ? inStart : theCell >= inStart + inCount ? inStart + inCount - 1 : theCell;
        t.mLogical0 = (int)floorf(((float)t.mPixel0 + 0.5f) / inSource);
        t.mLogical1 = (int)floorf(((float)t.mPixel1 + 0.5f) / inSource);
    }
}

// Draw between pixel scales: the logical rectangle's destination pixels (within the destination's clip) are
// filtered from the source rectangle's pixels; with a mask, each destination pixel is transparent exactly where its
// logical source pixel is, and an opaque one is filtered from opaque source pixels only (no dark fringe of the
// transparent black around a masked shape)
void PlatNativeImage::DrawScaled(SurfaceBase* inDest, float inDestScale, int x, int y, int left, int top, int width,
                                 int height)
{
    int theClipX1, theClipY1, theClipX2, theClipY2;
    inDest->GetClipRect(theClipX1, theClipY1, theClipX2, theClipY2);
    int theX1 = (std::max)(PlatGUIPixels(x, inDestScale), theClipX1);
    int theY1 = (std::max)(PlatGUIPixels(y, inDestScale), theClipY1);
    int theX2 = (std::min)(PlatGUIPixels(x + width, inDestScale), theClipX2 + 1);
    int theY2 = (std::min)(PlatGUIPixels(y + height, inDestScale), theClipY2 + 1);
    if (theX2 <= theX1 || theY2 <= theY1)
        return;
    std::vector<PlatScaleTap> theColumns, theRows;
    PlatScaleTaps(theColumns, theX1, theX2, x, left, width, inDestScale, this->mScale);
    PlatScaleTaps(theRows, theY1, theY2, y, top, height, inDestScale, this->mScale);
    const uint32_t* theSource = (const uint32_t*)this->mSurface->GetPixelsBase();
    int theSourcePitch = this->mSurface->GetPixelsPerRow();
    uint32_t* theDestPixels = (uint32_t*)inDest->GetPixelsBase();
    int theDestPitch = inDest->GetPixelsPerRow();
    PlatNativeMask* theMask = static_cast<PlatNativeMask*>(this->mTransparentMask.get());
    struct Local {
        static bool Transparent(const PlatNativeMask* m, int lx, int ly)
        { return m != NULL && lx < m->mWidth && ly < m->mHeight && m->IsTransparent(lx, ly); }
    };
    for (int j = theY1; j < theY2; j++) {
        const PlatScaleTap& r = theRows[(size_t)(j - theY1)];
        const uint32_t* theRow0 = theSource + (size_t)r.mPixel0 * theSourcePitch;
        const uint32_t* theRow1 = theSource + (size_t)r.mPixel1 * theSourcePitch;
        uint32_t* theTo = theDestPixels + (size_t)j * theDestPitch;
        for (int i = theX1; i < theX2; i++) {
            const PlatScaleTap& c = theColumns[(size_t)(i - theX1)];
            if (Local::Transparent(theMask, c.mLogical, r.mLogical)) {
                theTo[i] |= theRow0[c.mPixel0] & 0x00FFFFFF;
                continue;
            }
            if (c.mWeight1 == 0 && r.mWeight1 == 0) {
                theTo[i] = theRow0[c.mPixel0] & 0x00FFFFFF;
                continue;
            }
            uint32_t thePixels[4] = {theRow0[c.mPixel0], theRow0[c.mPixel1], theRow1[c.mPixel0], theRow1[c.mPixel1]};
            int theWeights[4] = {(256 - c.mWeight1) * (256 - r.mWeight1), c.mWeight1 * (256 - r.mWeight1),
                                 (256 - c.mWeight1) * r.mWeight1, c.mWeight1 * r.mWeight1};
            if (theMask != NULL) {
                if (Local::Transparent(theMask, c.mLogical0, r.mLogical0)) theWeights[0] = 0;
                if (Local::Transparent(theMask, c.mLogical1, r.mLogical0)) theWeights[1] = 0;
                if (Local::Transparent(theMask, c.mLogical0, r.mLogical1)) theWeights[2] = 0;
                if (Local::Transparent(theMask, c.mLogical1, r.mLogical1)) theWeights[3] = 0;
            }
            int theTotal = theWeights[0] + theWeights[1] + theWeights[2] + theWeights[3];
            if (theTotal <= 0) {
                // every tap is masked though the pixel is not: the nearest source pixel of its own logical pixel
                int theX = PlatGUIPixels(c.mLogical, this->mScale), theY = PlatGUIPixels(r.mLogical, this->mScale);
                theTo[i] = theSource[(size_t)theY * theSourcePitch + theX] & 0x00FFFFFF;
                continue;
            }
            unsigned theRed = 0, theGreen = 0, theBlue = 0;
            for (int k = 0; k < 4; k++) {
                theRed += ((thePixels[k] >> 16) & 0xff) * (unsigned)theWeights[k];
                theGreen += ((thePixels[k] >> 8) & 0xff) * (unsigned)theWeights[k];
                theBlue += (thePixels[k] & 0xff) * (unsigned)theWeights[k];
            }
            unsigned theHalf = (unsigned)theTotal / 2;
            theTo[i] = (((theRed + theHalf) / theTotal) << 16) | (((theGreen + theHalf) / theTotal) << 8) |
                       ((theBlue + theHalf) / theTotal);
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
    int thePixelWidth = this->GetPixelWidth(), thePixelHeight = this->GetPixelHeight();
    for (int y = 0; y < theHeight; y++) {
        DWORD* theRow = theImage->mImageData + (size_t)y * theWidth;
        // a scaled image: the pixel at each logical pixel's centre
        int thePixelY = this->mScale == 1.0f ? y : (std::min)((int)floorf((y + 0.5f) * this->mScale), thePixelHeight - 1);
        for (int x = 0; x < theWidth; x++) {
            int thePixelX = this->mScale == 1.0f ? x
                                                 : (std::min)((int)floorf((x + 0.5f) * this->mScale), thePixelWidth - 1);
            DWORD thePixel = thePixels[(size_t)thePixelY * thePitch + thePixelX] & 0x00FFFFFF;
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
    SmartPtr<PlatNativeImage> theCopy =
        new PlatNativeImage(this->mDisplayContext.get(), this->mWidth, this->mHeight, this->mScale);
    memcpy(theCopy->mSurface->GetPixelsBase(), this->mSurface->GetPixelsBase(),
           (size_t)this->mSurface->GetBytesPerRow() * this->mSurface->GetHeight());
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

// Creates a blank (black) image of the given size for this display context, at the lobby surface's pixel scale (the
// windows' offscreen images and what the GUI composes for them)
SmartPtr<NativeImage> DisplayContext::CreateImage(int width, int height)
{
    PlatWindowManager* theManager = dynamic_cast<PlatWindowManager*>(WindowManager::GetDefaultWindowManager());
    return new PlatNativeImage(this, width, height, theManager != NULL ? theManager->GetPixelScale() : 1.0f);
}

// ---- the texture pack's copies of the lobby's images

// "...WONLobby Resources/<rest>" -> <rest> (either separator, any case); false for another path
static bool PlatPackRelativePath(const char* inPath, std::string& outRest)
{
    static const char kFolder[] = "wonlobby resources";
    const size_t theLength = sizeof(kFolder) - 1;
    for (const char* p = inPath; *p != 0; p++) {
        size_t i = 0;
        while (i < theLength && p[i] != 0 && tolower((unsigned char)p[i]) == kFolder[i])
            i++;
        if (i == theLength && (p[i] == '/' || p[i] == '\\') && (p == inPath || p[-1] == '/' || p[-1] == '\\')) {
            outRest = p + i + 1;
            for (size_t j = 0; j < outRest.size(); j++)
                if (outRest[j] == '\\')
                    outRest[j] = '/';
            return !outRest.empty();
        }
    }
    return false;
}

SmartPtr<NativeImage> PlatGUIPackImage(DisplayContext* inContext, NativeImage* inImage, const char* inPath)
{
    long theShift = GETextureManager::GetTexturePackLogicalShift();
    std::string theRest;
    if (inImage == NULL || inPath == NULL || theShift <= 0 || inImage->GetWidth() == 0 || inImage->GetHeight() == 0 ||
        !PlatPackRelativePath(inPath, theRest))
        return inImage;
    WindowManager* theManager = WindowManager::GetDefaultWindowManager();
    if (theManager == NULL)
        return inImage;
    std::string theName = "Data/Textures-4x/WONLobby Resources/" + theRest;
    std::string theCandidates[2] = {theName, theName};
    size_t theDot = theName.find_last_of('.');
    if (theDot != std::string::npos && theName.find('/', theDot) == std::string::npos)
        theCandidates[1] = theName.substr(0, theDot) + ".png";
    SmartPtr<RawImage> theRaw;
    for (int i = 0; i < 2 && theRaw.get() == NULL; i++) {
        if (i == 1 && theCandidates[1] == theCandidates[0])
            break;
        FileReader theReader;
        if (!theReader.Open(theCandidates[i].c_str()))
            continue;
        MultiImageDecoder* theDecoder = &theManager->mImageDecoder;
        theDecoder->SetFileName(theCandidates[i].c_str());
        try {
            theRaw = theDecoder->ImageDecoder::DecodeRaw(theReader);
        } catch (FileReaderException&) {
            theRaw = NULL;
        }
    }
    if (theRaw.get() == NULL)
        return inImage;
    // the pack's copy: an integer multiple of the original in both directions
    int theWidth = inImage->GetWidth(), theHeight = inImage->GetHeight();
    int theFactor = theRaw->GetWidth() / theWidth;
    if (theFactor < 2 || theRaw->GetWidth() != theWidth * theFactor || theRaw->GetHeight() != theHeight * theFactor)
        return inImage;
    SmartPtr<NativeImage> theNative = theRaw->GetNative(inContext);
    PlatNativeImage* thePack = dynamic_cast<PlatNativeImage*>(theNative.get());
    if (thePack == NULL || thePack->GetPixelWidth() != theWidth * theFactor)
        return inImage;
    // the 2x setting: the 4x file reduced by 2x2 boxes as often as the setting's shift is below the file's
    int theScale = theFactor, theBox = 1;
    while (theScale > (1 << theShift) && theScale % 2 == 0) {
        theScale /= 2;
        theBox *= 2;
    }
    SmartPtr<PlatNativeImage> theImage = new PlatNativeImage(inContext, theWidth, theHeight, (float)theScale);
    uint32_t* theTo = theImage->GetPixels();
    int theToPitch = theImage->GetPitchPixels();
    const uint32_t* theFrom = thePack->GetPixels();
    int theFromPitch = thePack->GetPitchPixels();
    for (int y = 0; y < theHeight * theScale; y++)
        for (int x = 0; x < theWidth * theScale; x++) {
            unsigned theRed = 0, theGreen = 0, theBlue = 0;
            for (int j = 0; j < theBox; j++)
                for (int i = 0; i < theBox; i++) {
                    uint32_t p = theFrom[(size_t)(y * theBox + j) * theFromPitch + x * theBox + i];
                    theRed += (p >> 16) & 0xff;
                    theGreen += (p >> 8) & 0xff;
                    theBlue += p & 0xff;
                }
            unsigned n = (unsigned)(theBox * theBox);
            theTo[(size_t)y * theToPitch + x] = (((theRed + n / 2) / n) << 16) | (((theGreen + n / 2) / n) << 8) |
                                                ((theBlue + n / 2) / n);
        }
    // the original's transparency, exactly: its mask, and black under it (the mask draw ORs the image in there)
    theImage->mTransparentMask = inImage->mTransparentMask;
    if (PlatNativeMask* theMask = static_cast<PlatNativeMask*>(theImage->mTransparentMask.get()))
        for (int y = 0; y < theHeight * theScale; y++)
            for (int x = 0; x < theWidth * theScale; x++) {
                int lx = x / theScale, ly = y / theScale;
                if (lx < theMask->mWidth && ly < theMask->mHeight && theMask->IsTransparent(lx, ly))
                    theTo[(size_t)y * theToPitch + x] = 0;
            }
    return theImage.get();
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
