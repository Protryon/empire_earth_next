// C:\EEX\Low-Level Engine\Graphics Engine\DX7 Display Plug-ins\Common\DX7Surface.cpp (file name inferred: class
// name from the exports; units link alphabetically and this one is last)
// Unit flags (hypothesis, docs/CALIBRATION.md "dx7hr"): -O1 -G6 -MD -GR
//
// 0x1000eced..0x1000fc91: ctor, dtor, then the members in alphabetical order; the vector<GESurface*>::erase/copy
// COMDATs it is the first to use close it (0x1000fc0c, 0x1000fc6d). No init chains. DirectDraw calls retry while the
// blitter is busy (DDERR_WASSTILLDRAWING); a lost surface is reported as 0xd, any other failure as kErrorNativeError.
#include <windows.h>
#include <ddraw.h>

#include "lle/LowLevelEngine.h"
#include "lle/Graphics Engine.h"
#include "dx7/DX7Surface.h"

// FUNCTION: DX7HR 0x1000eced
// Wraps a DirectDraw surface with its description and parent screen, unlocked and with no attachments
DX7Surface::DX7Surface(IDirectDrawSurface7* inSurface, const GESurfaceDescription& inDescription,
                       GEScreen* inParentScreen)
    : mSurface(inSurface)
{
    this->mDescription = inDescription;
    this->mIsLocked = false;
    this->mIsTextLocked = false;
    this->mParentScreen = inParentScreen;
}

// FUNCTION: DX7HR 0x1000ed5a
// Destroys the surface: ends any text lock, dereferences attached surfaces, releases the DirectDraw surface
DX7Surface::~DX7Surface()
{
    this->TextUnlock();
    for (unsigned long i = 0; i < this->mAttachedSurfaces.size(); i++)
        this->mAttachedSurfaces[i]->Dereference();
    if (this->mDescription.mType != 7)
        this->mSurface->Release();
}

// FUNCTION: DX7HR 0x1000edb9
// Attaches another surface (e.g. a Z buffer) and keeps a reference to it; native error on failure
long DX7Surface::AttachSurface(GESurface* inSurface)
{
    long theResult = kErrorOK;
    DX7Surface* theSurface = static_cast<DX7Surface*>(inSurface);
    HRESULT theError = this->mSurface->AddAttachedSurface(theSurface->mSurface);
    if (theError == DD_OK) {
        theSurface->Reference();
        this->mAttachedSurfaces.push_back(theSurface);
    } else {
        theResult = kErrorNativeError;
        ESRecordNativeError(theError);
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000edfe
// Fills the whole surface with a colour and alpha, retrying while the blitter is busy; 0xd when the surface is lost
long DX7Surface::Clear(GEColor& inColor, float inAlpha)
{
    long theResult = kErrorOK;
    DDBLTFX theEffects;
    memset(&theEffects, 0, sizeof(theEffects));
    theEffects.dwSize = sizeof(theEffects);
    theEffects.dwFillColor = GEPackColor(this->mDescription.mPixelFormat, inColor, inAlpha);
    HRESULT theError;
    do {
        theError = this->mSurface->Blt(NULL, NULL, NULL, DDBLT_COLORFILL, &theEffects);
        if (theError == DDERR_WASSTILLDRAWING)
            Sleep(0);
    } while (theError == DDERR_WASSTILLDRAWING);
    if (theError != DD_OK) {
        if (theError == DDERR_SURFACELOST)
            theResult = 0xd;
        else {
            theResult = kErrorNativeError;
            ESRecordNativeError(theError);
        }
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000ee8c
// Fills a rectangle of the surface with a colour and alpha, retrying while the blitter is busy; 0xd when lost
long DX7Surface::Clear(GERectangle& inRectangle, GEColor& inColor, float inAlpha)
{
    long theResult = kErrorOK;
    RECT theRect;
    theRect.left = (long)inRectangle.GetLeft();
    theRect.right = (long)inRectangle.GetRight();
    theRect.top = (long)inRectangle.GetTop();
    theRect.bottom = (long)inRectangle.GetBottom();
    DDBLTFX theEffects;
    memset(&theEffects, 0, sizeof(theEffects));
    theEffects.dwSize = sizeof(theEffects);
    theEffects.dwFillColor = GEPackColor(this->mDescription.mPixelFormat, inColor, inAlpha);
    HRESULT theError;
    do {
        theError = this->mSurface->Blt(&theRect, NULL, NULL, DDBLT_COLORFILL, &theEffects);
        if (theError == DDERR_WASSTILLDRAWING)
            Sleep(0);
    } while (theError == DDERR_WASSTILLDRAWING);
    if (theError != DD_OK) {
        if (theError == DDERR_SURFACELOST)
            theResult = 0xd;
        else {
            theResult = kErrorNativeError;
            ESRecordNativeError(theError);
        }
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000ef4b
// Blits a source rectangle onto a target surface's rectangle, optionally colour-keyed; 0xd when lost or busy
long DX7Surface::CopyTo(GESurface* inTarget, GERectangle& inSource, GERectangle& inDestination, bool inTransparent)
{
    long theResult = kErrorOK;
    RECT theSource;
    theSource.left = (long)inSource.GetLeft();
    theSource.right = (long)inSource.GetRight();
    theSource.top = (long)inSource.GetTop();
    theSource.bottom = (long)inSource.GetBottom();
    RECT theDestination;
    theDestination.left = (long)inDestination.GetLeft();
    theDestination.right = (long)inDestination.GetRight();
    theDestination.top = (long)inDestination.GetTop();
    theDestination.bottom = (long)inDestination.GetBottom();
    unsigned long theFlags = inTransparent ? DDBLT_DONOTWAIT | DDBLT_KEYSRC : DDBLT_DONOTWAIT;
    HRESULT theError;
    do {
        theError = static_cast<DX7Surface*>(inTarget)->mSurface->Blt(&theDestination, this->mSurface, &theSource, theFlags, NULL);
        if (theError == DDERR_WASSTILLDRAWING)
            Sleep(0);
    } while (theError == DDERR_WASSTILLDRAWING);
    if (theError != DD_OK) {
        if (theError == DDERR_SURFACELOST || theError == DDERR_SURFACEBUSY)
            theResult = 0xd;
        else {
            theResult = kErrorNativeError;
            ESRecordNativeError(theError);
        }
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000f01d
// Copies a rectangle to the target with bilinear scaling, choosing the 32-bit or 16-bit path by this surface's depth
long DX7Surface::CopyWithFilter(GESurface* inTarget, GERectangle& inSource, GERectangle& inDestination)
{
    if (this->mDescription.mPixelDepth == 32)
        return this->Copy32BitWithFilter(static_cast<DX7Surface*>(inTarget), inSource, inDestination);
    return this->Copy16BitWithFilter(static_cast<DX7Surface*>(inTarget), inSource, inDestination);
}

// FUNCTION: DX7HR 0x1000f03f
// Copies a rectangle to the target with bilinear scaling, 16-bit pixels; returns this surface's Unlock result,
// so a failed target lock is not reported
// Pixels go through GEUnpack*/GEPackColor. NEAR: the remaining differences are -G6 scheduling only (the pushes of the
// forward-substituted GEUnpackAlpha calls, the destination pointer step at the loop end).
long DX7Surface::Copy16BitWithFilter(DX7Surface* inTarget, GERectangle& inSource, GERectangle& inDestination)
{
    long theResult;
    unsigned long theTop, theBottom, theLeft, theRight;
    float theXStep, theYStep, theXOffset, theYOffset;
    unsigned short* theSource;
    unsigned long theFormat;
    unsigned long theSourcePitch;
    unsigned short* theDestination;
    unsigned long theDestinationPitch;
    unsigned long x, y;
    float theX, theY;
    long theSourceX, theSourceY;
    float theXFraction, theYFraction, theXInverse, theYInverse;
    long theIndex;
    long theMinX, theMaxX, theMinY, theMaxY, theNextX, theNextY;
    unsigned long thePixel00, thePixel01, thePixel10, thePixel11;
    float theRed1, theGreen1, theBlue1, theAlpha1;
    float theRed2, theGreen2, theBlue2, theAlpha2;
    float theTopRed, theTopGreen, theTopBlue, theTopAlpha;
    float theRed, theGreen, theBlue, theAlpha;

    theTop = (unsigned long)inDestination.GetTop();
    theBottom = (unsigned long)inDestination.GetBottom();
    theLeft = (unsigned long)inDestination.GetLeft();
    theRight = (unsigned long)inDestination.GetRight();
    theXStep = 1.0f / inDestination.GetWidth();
    theYStep = 1.0f / inDestination.GetHeight();
    theXOffset = 1.0f / (2.0f * inDestination.GetWidth());
    theYOffset = 1.0f / (2.0f * inDestination.GetHeight());
    // The taps stay inside the source rectangle (right and bottom are exclusive).
    theMinX = (long)inSource.GetLeft();
    theMaxX = (long)inSource.GetRight() - 1;
    theMinY = (long)inSource.GetTop();
    theMaxY = (long)inSource.GetBottom() - 1;
    theResult = this->Lock();
    if (theResult != kErrorOK)
        return theResult;
    theSource = reinterpret_cast<unsigned short*>(this->mMemoryAddress);
    theFormat = this->mDescription.mPixelFormat;
    theSourcePitch = this->mBytesPerRow / 2;
    theResult = inTarget->Lock();
    if (theResult == kErrorOK) {
        theDestination = reinterpret_cast<unsigned short*>(inTarget->mMemoryAddress);
        theDestinationPitch = inTarget->mBytesPerRow / 2;
        for (y = theTop; y < theBottom; y++) {
            for (x = theLeft; x < theRight; x++) {
                theX = inSource.GetWidth() * ((x - theLeft + 1) * theXStep - theXOffset) + inSource.GetLeft()
                       - 0.5f;
                theY = inSource.GetHeight() * ((y - theTop + 1) * theYStep - theYOffset) + inSource.GetTop()
                       - 0.5f;
                theSourceX = UMFloatToLong(theX);
                theSourceY = UMFloatToLong(theY);
                theXFraction = theX - theSourceX;
                theYFraction = theY - theSourceY;
                // The conversion rounds to nearest: a negative fraction means the sample lies left of (above) the
                // rounded pixel, so step back one pixel.
                if (theXFraction < 0.0f) {
                    theXFraction += 1.0f;
                    theSourceX--;
                }
                if (theYFraction < 0.0f) {
                    theYFraction += 1.0f;
                    theSourceY--;
                }
                if (theSourceX < theMinX) {
                    theSourceX = theMinX;
                    theXFraction = 0.0f;
                }
                if (theSourceX > theMaxX)
                    theSourceX = theMaxX;
                if (theSourceY < theMinY) {
                    theSourceY = theMinY;
                    theYFraction = 0.0f;
                }
                if (theSourceY > theMaxY)
                    theSourceY = theMaxY;
                theNextX = theSourceX < theMaxX ? 1 : 0;
                theNextY = theSourceY < theMaxY ? (long)theSourcePitch : 0;
                theXInverse = 1.0f - theXFraction;
                theYInverse = 1.0f - theYFraction;
                theIndex = theSourceY * theSourcePitch + theSourceX;
                thePixel00 = theSource[theIndex];
                thePixel01 = theSource[theIndex + theNextX];
                thePixel10 = theSource[theIndex + theNextY];
                thePixel11 = theSource[theIndex + theNextY + theNextX];
                theRed1 = GEUnpackRed(theFormat, thePixel00);
                theGreen1 = GEUnpackGreen(theFormat, thePixel00);
                theBlue1 = GEUnpackBlue(theFormat, thePixel00);
                theAlpha1 = GEUnpackAlpha(theFormat, thePixel00);
                theRed2 = GEUnpackRed(theFormat, thePixel01);
                theGreen2 = GEUnpackGreen(theFormat, thePixel01);
                theBlue2 = GEUnpackBlue(theFormat, thePixel01);
                theAlpha2 = GEUnpackAlpha(theFormat, thePixel01);
                theTopRed = theRed1 * theXInverse + theRed2 * theXFraction;
                theTopGreen = theGreen1 * theXInverse + theGreen2 * theXFraction;
                theTopBlue = theBlue1 * theXInverse + theBlue2 * theXFraction;
                theTopAlpha = theAlpha1 * theXInverse + theAlpha2 * theXFraction;
                theRed1 = GEUnpackRed(theFormat, thePixel10);
                theGreen1 = GEUnpackGreen(theFormat, thePixel10);
                theBlue1 = GEUnpackBlue(theFormat, thePixel10);
                theAlpha1 = GEUnpackAlpha(theFormat, thePixel10);
                theRed2 = GEUnpackRed(theFormat, thePixel11);
                theGreen2 = GEUnpackGreen(theFormat, thePixel11);
                theBlue2 = GEUnpackBlue(theFormat, thePixel11);
                theAlpha2 = GEUnpackAlpha(theFormat, thePixel11);
                theRed = theTopRed * theYInverse +
                    (theRed1 * theXInverse + theRed2 * theXFraction) * theYFraction;
                theGreen = theTopGreen * theYInverse +
                    (theGreen1 * theXInverse + theGreen2 * theXFraction) * theYFraction;
                theBlue = theTopBlue * theYInverse +
                    (theBlue1 * theXInverse + theBlue2 * theXFraction) * theYFraction;
                theAlpha = theTopAlpha * theYInverse +
                    (theAlpha1 * theXInverse + theAlpha2 * theXFraction) * theYFraction;
                theDestination[y * theDestinationPitch + x] = (unsigned short)GEPackColor(theFormat,
                    UMFloatToLong(theRed), UMFloatToLong(theGreen), UMFloatToLong(theBlue),
                    UMFloatToLong(theAlpha));
            }
        }
        inTarget->Unlock();
    }
    theResult = this->Unlock();
    return theResult;
}

// FUNCTION: DX7HR 0x1000f4d0
// Copies a rectangle to the target with bilinear scaling, 32-bit pixels (as the 16-bit copy)
// NEAR: the same -G6 scheduling differences as Copy16BitWithFilter.
long DX7Surface::Copy32BitWithFilter(DX7Surface* inTarget, GERectangle& inSource, GERectangle& inDestination)
{
    long theResult;
    unsigned long theTop, theBottom, theLeft, theRight;
    float theXStep, theYStep, theXOffset, theYOffset;
    unsigned long* theSource;
    unsigned long theFormat;
    unsigned long theSourcePitch;
    unsigned long* theDestination;
    unsigned long theDestinationPitch;
    unsigned long x, y;
    float theX, theY;
    long theSourceX, theSourceY;
    float theXFraction, theYFraction, theXInverse, theYInverse;
    long theIndex;
    long theMinX, theMaxX, theMinY, theMaxY, theNextX, theNextY;
    unsigned long thePixel00, thePixel01, thePixel10, thePixel11;
    float theRed1, theGreen1, theBlue1, theAlpha1;
    float theRed2, theGreen2, theBlue2, theAlpha2;
    float theTopRed, theTopGreen, theTopBlue, theTopAlpha;
    float theRed, theGreen, theBlue, theAlpha;

    theTop = (unsigned long)inDestination.GetTop();
    theBottom = (unsigned long)inDestination.GetBottom();
    theLeft = (unsigned long)inDestination.GetLeft();
    theRight = (unsigned long)inDestination.GetRight();
    theXStep = 1.0f / inDestination.GetWidth();
    theYStep = 1.0f / inDestination.GetHeight();
    theXOffset = 1.0f / (2.0f * inDestination.GetWidth());
    theYOffset = 1.0f / (2.0f * inDestination.GetHeight());
    // The taps stay inside the source rectangle (right and bottom are exclusive).
    theMinX = (long)inSource.GetLeft();
    theMaxX = (long)inSource.GetRight() - 1;
    theMinY = (long)inSource.GetTop();
    theMaxY = (long)inSource.GetBottom() - 1;
    theResult = this->Lock();
    if (theResult != kErrorOK)
        return theResult;
    theSource = reinterpret_cast<unsigned long*>(this->mMemoryAddress);
    theFormat = this->mDescription.mPixelFormat;
    theSourcePitch = this->mBytesPerRow / 4;
    theResult = inTarget->Lock();
    if (theResult == kErrorOK) {
        theDestination = reinterpret_cast<unsigned long*>(inTarget->mMemoryAddress);
        theDestinationPitch = inTarget->mBytesPerRow / 4;
        for (y = theTop; y < theBottom; y++) {
            for (x = theLeft; x < theRight; x++) {
                theX = inSource.GetWidth() * ((x - theLeft + 1) * theXStep - theXOffset) + inSource.GetLeft()
                       - 0.5f;
                theY = inSource.GetHeight() * ((y - theTop + 1) * theYStep - theYOffset) + inSource.GetTop()
                       - 0.5f;
                theSourceX = UMFloatToLong(theX);
                theSourceY = UMFloatToLong(theY);
                theXFraction = theX - theSourceX;
                theYFraction = theY - theSourceY;
                // The conversion rounds to nearest: a negative fraction means the sample lies left of (above) the
                // rounded pixel, so step back one pixel.
                if (theXFraction < 0.0f) {
                    theXFraction += 1.0f;
                    theSourceX--;
                }
                if (theYFraction < 0.0f) {
                    theYFraction += 1.0f;
                    theSourceY--;
                }
                if (theSourceX < theMinX) {
                    theSourceX = theMinX;
                    theXFraction = 0.0f;
                }
                if (theSourceX > theMaxX)
                    theSourceX = theMaxX;
                if (theSourceY < theMinY) {
                    theSourceY = theMinY;
                    theYFraction = 0.0f;
                }
                if (theSourceY > theMaxY)
                    theSourceY = theMaxY;
                theNextX = theSourceX < theMaxX ? 1 : 0;
                theNextY = theSourceY < theMaxY ? (long)theSourcePitch : 0;
                theXInverse = 1.0f - theXFraction;
                theYInverse = 1.0f - theYFraction;
                theIndex = theSourceY * theSourcePitch + theSourceX;
                thePixel00 = theSource[theIndex];
                thePixel01 = theSource[theIndex + theNextX];
                thePixel10 = theSource[theIndex + theNextY];
                thePixel11 = theSource[theIndex + theNextY + theNextX];
                theRed1 = GEUnpackRed(theFormat, thePixel00);
                theGreen1 = GEUnpackGreen(theFormat, thePixel00);
                theBlue1 = GEUnpackBlue(theFormat, thePixel00);
                theAlpha1 = GEUnpackAlpha(theFormat, thePixel00);
                theRed2 = GEUnpackRed(theFormat, thePixel01);
                theGreen2 = GEUnpackGreen(theFormat, thePixel01);
                theBlue2 = GEUnpackBlue(theFormat, thePixel01);
                theAlpha2 = GEUnpackAlpha(theFormat, thePixel01);
                theTopRed = theRed1 * theXInverse + theRed2 * theXFraction;
                theTopGreen = theGreen1 * theXInverse + theGreen2 * theXFraction;
                theTopBlue = theBlue1 * theXInverse + theBlue2 * theXFraction;
                theTopAlpha = theAlpha1 * theXInverse + theAlpha2 * theXFraction;
                theRed1 = GEUnpackRed(theFormat, thePixel10);
                theGreen1 = GEUnpackGreen(theFormat, thePixel10);
                theBlue1 = GEUnpackBlue(theFormat, thePixel10);
                theAlpha1 = GEUnpackAlpha(theFormat, thePixel10);
                theRed2 = GEUnpackRed(theFormat, thePixel11);
                theGreen2 = GEUnpackGreen(theFormat, thePixel11);
                theBlue2 = GEUnpackBlue(theFormat, thePixel11);
                theAlpha2 = GEUnpackAlpha(theFormat, thePixel11);
                theRed = theTopRed * theYInverse +
                    (theRed1 * theXInverse + theRed2 * theXFraction) * theYFraction;
                theGreen = theTopGreen * theYInverse +
                    (theGreen1 * theXInverse + theGreen2 * theXFraction) * theYFraction;
                theBlue = theTopBlue * theYInverse +
                    (theBlue1 * theXInverse + theBlue2 * theXFraction) * theYFraction;
                theAlpha = theTopAlpha * theYInverse +
                    (theAlpha1 * theXInverse + theAlpha2 * theXFraction) * theYFraction;
                theDestination[y * theDestinationPitch + x] = GEPackColor(theFormat,
                    UMFloatToLong(theRed), UMFloatToLong(theGreen), UMFloatToLong(theBlue),
                    UMFloatToLong(theAlpha));
            }
        }
        inTarget->Unlock();
    }
    theResult = this->Unlock();
    return theResult;
}

// FUNCTION: DX7HR 0x1000f960
// Detaches the attached surface at an index and drops its reference; native error on failure
long DX7Surface::DetachSurface(unsigned long inIndex)
{
    long theResult = kErrorOK;
    GESurface* theSurface = this->mAttachedSurfaces[inIndex];
    HRESULT theError = this->mSurface->DeleteAttachedSurface(0, static_cast<DX7Surface*>(theSurface)->mSurface);
    if (theError == DD_OK) {
        this->mAttachedSurfaces.erase(this->mAttachedSurfaces.begin() + inIndex);
        theSurface->Dereference();
    } else {
        theResult = kErrorNativeError;
        ESRecordNativeError(theError);
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000f9d9
// Flips the flipping chain, waiting for vertical sync only when asked; 0xd when the surface is lost
long DX7Surface::Flip(bool inWait)
{
    long theResult = kErrorOK;
    unsigned long theFlags = inWait ? DDFLIP_DONOTWAIT : DDFLIP_DONOTWAIT | DDFLIP_NOVSYNC;
    HRESULT theError;
    do {
        theError = this->mSurface->Flip(NULL, theFlags);
        if (theError == DDERR_WASSTILLDRAWING)
            Sleep(0);
    } while (theError == DDERR_WASSTILLDRAWING);
    if (theError != DD_OK) {
        if (theError == DDERR_SURFACELOST)
            theResult = 0xd;
        else {
            theResult = kErrorNativeError;
            ESRecordNativeError(theError);
        }
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000fa2e
// Returns the memory size of the surface plus every attached surface, in bytes
unsigned long DX7Surface::GetAllocatedSize()
{
    unsigned long theSize = this->mDescription.mPixelDepth / 8 * this->mDescription.mHeight * this->mDescription.mWidth;
    for (unsigned long i = 0; i < this->mAttachedSurfaces.size(); i++)
        theSize += this->mAttachedSurfaces[i]->GetAllocatedSize();
    return theSize;
}

// FUNCTION: DX7HR 0x1000fa69
// Locks the whole surface and records its memory address and row pitch; 0xd when the surface is lost
long DX7Surface::Lock()
{
    long theResult = kErrorOK;
    DDSURFACEDESC2 theDescription;
    memset(&theDescription, 0, sizeof(theDescription));
    theDescription.dwSize = sizeof(theDescription);
    HRESULT theError;
    do {
        theError = this->mSurface->Lock(NULL, &theDescription, 0, NULL);
        if (theError == DDERR_WASSTILLDRAWING)
            Sleep(0);
    } while (theError == DDERR_WASSTILLDRAWING);
    if (theError == DD_OK) {
        this->mMemoryAddress = (unsigned char*)theDescription.lpSurface;
        this->mIsLocked = true;
        this->mBytesPerRow = theDescription.lPitch;
    } else if (theError == DDERR_SURFACELOST)
        theResult = 0xd;
    else {
        theResult = kErrorNativeError;
        ESRecordNativeError(theError);
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000faed
// Restores a lost surface's memory; native error on failure
long DX7Surface::RestoreSurface()
{
    long theResult = kErrorOK;
    HRESULT theError = this->mSurface->Restore();
    if (theError != DD_OK) {
        ESRecordNativeError(theError);
        theResult = kErrorNativeError;
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000fb0c
// Sets the source colour key used for transparent blits
void DX7Surface::SetTransparencyColor(GEColor& inColor)
{
    this->mTransparencyColor = inColor;
    DDCOLORKEY theKey;
    unsigned long theColor = GEPackColor(this->mDescription.mPixelFormat, inColor, 1.0f);
    theKey.dwColorSpaceLowValue = theColor;
    theKey.dwColorSpaceHighValue = theColor;
    this->mSurface->SetColorKey(DDCKEY_SRCBLT, &theKey);
}

// FUNCTION: DX7HR 0x1000fb55
// Gets a GDI device context for text drawing; already-exists error when one is held, 0xd when the surface is lost
long DX7Surface::TextLock()
{
    long theResult;
    if (this->mIsTextLocked == true)
        theResult = kErrorAlreadyExists;
    else {
        HRESULT theError;
        do {
            theError = this->mSurface->GetDC(&this->mTextDC);
            if (theError == DDERR_WASSTILLDRAWING)
                Sleep(0);
        } while (theError == DDERR_WASSTILLDRAWING);
        if (theError == DD_OK) {
            this->mIsTextLocked = true;
            theResult = kErrorOK;
        } else if (theError == DDERR_SURFACELOST)
            theResult = 0xd;
        else {
            theResult = kErrorNativeError;
            ESRecordNativeError(theError);
        }
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000fba9
// Releases the GDI device context taken by TextLock; already-exists error when none is held
long DX7Surface::TextUnlock()
{
    if (this->mIsTextLocked) {
        HRESULT theError = this->mSurface->ReleaseDC(this->mTextDC);
        if (theError == DD_OK) {
            this->mIsTextLocked = false;
            return kErrorOK;
        }
        ESRecordNativeError(theError);
        return kErrorNativeError;
    }
    return kErrorAlreadyExists;
}

// FUNCTION: DX7HR 0x1000fbd8
// Unlocks the surface (an unlocked surface is not an error) and clears the locked flag
long DX7Surface::Unlock()
{
    long theResult = kErrorOK;
    HRESULT theError = this->mSurface->Unlock(NULL);
    if (theError != DD_OK && theError != DDERR_NOTLOCKED) {
        theResult = kErrorNativeError;
        ESRecordNativeError(theError);
    } else
        this->mIsLocked = false;
    return theResult;
}

// vector<GESurface*> COMDATs this unit is the first (link order) to instantiate (DetachSurface's erase):
// TEMPLATE: DX7HR 0x1000fc0c
// TEMPLATE: DX7HR 0x1000fc6d

// DX7HRTnLDisplay.dll (the T&L twin builds this unit unchanged): the same functions at their twin addresses.
// FUNCTION: DX7HRTNL 0x1000ed72
// FUNCTION: DX7HRTNL 0x1000eddf
// FUNCTION: DX7HRTNL 0x1000ee3e
// FUNCTION: DX7HRTNL 0x1000ee83
// FUNCTION: DX7HRTNL 0x1000ef11
// FUNCTION: DX7HRTNL 0x1000efd0
// FUNCTION: DX7HRTNL 0x1000f0a2
// FUNCTION: DX7HRTNL 0x1000f0c4
// FUNCTION: DX7HRTNL 0x1000f555
// FUNCTION: DX7HRTNL 0x1000f9e5
// FUNCTION: DX7HRTNL 0x1000fa5e
// FUNCTION: DX7HRTNL 0x1000fab3
// FUNCTION: DX7HRTNL 0x1000faee
// FUNCTION: DX7HRTNL 0x1000fb72
// FUNCTION: DX7HRTNL 0x1000fb91
// FUNCTION: DX7HRTNL 0x1000fbda
// FUNCTION: DX7HRTNL 0x1000fc2e
// FUNCTION: DX7HRTNL 0x1000fc5d
// TEMPLATE: DX7HRTNL 0x1000fc91
// TEMPLATE: DX7HRTNL 0x1000fcf2
