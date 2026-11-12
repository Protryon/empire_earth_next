// C:\EEX\Low-Level Engine\Graphics Engine\DX7 Display Plug-ins\Common\DX7Surface.cpp (file name inferred: class
// name from the exports; units link alphabetically and this one is last)
// Unit flags (hypothesis, docs/CALIBRATION.md "dx7hr"): -O1 -G6 -MD -GR
//
// 0x1000eced..0x1000fc91: ctor, dtor, then the members in alphabetical order; the vector<GESurface*>::erase/copy
// COMDATs it is the first to use close it (0x1000fc0c, 0x1000fc6d). No init chains. DirectDraw calls retry while the
// blitter is busy (GXERR_WASSTILLDRAWING); a lost surface is reported as 0xd, any other failure as kErrorNativeError.
#include <windows.h>
#include <stdint.h>

#include "lle/LowLevelEngine.h"
#include "lle/Graphics Engine.h"
#include "dx7/DX7Surface.h"
#include "rust/ee_dx7.h"

// The surface algorithms are in Rust (rust/crates/ee_dx7/src/surface.rs); this class keeps the fields (the harness reads
// mSurface), the attached-surface vector and the reference counts. Each method forwards to its ee_dx7s_* function.

// The logical-size members and pixel size Rust converts a rectangle with
static EeDx7LogicalMetrics DX7Metrics(const GESurface* inSurface)
{
    EeDx7LogicalMetrics theMetrics;
    theMetrics.logical_width = inSurface->GetLogicalWidthSet();
    theMetrics.logical_height = inSurface->GetLogicalHeightSet();
    theMetrics.pixel_width = inSurface->GetPixelWidth();
    theMetrics.pixel_height = inSurface->GetPixelHeight();
    theMetrics.logical_shift = inSurface->GetLogicalShift();
    return theMetrics;
}

static void DX7RectFloats(const GERectangle& inRectangle, float outRect[4])
{
    outRect[0] = inRectangle.GetLeft();
    outRect[1] = inRectangle.GetTop();
    outRect[2] = inRectangle.GetRight();
    outRect[3] = inRectangle.GetBottom();
}

// FUNCTION: DX7HR 0x1000eced
// Wraps a DirectDraw surface with its description and parent screen, unlocked and with no attachments
DX7Surface::DX7Surface(GXSurface* inSurface, const GESurfaceDescription& inDescription,
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
    DX7Surface* theSurface = static_cast<DX7Surface*>(inSurface);
    long theResult = ee_dx7s_attach(this->mSurface, theSurface->mSurface);
    if (theResult == kErrorOK) {
        theSurface->Reference();
        this->mAttachedSurfaces.push_back(theSurface);
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000edfe
// Fills the whole surface with a colour and alpha, retrying while the blitter is busy; 0xd when the surface is lost
long DX7Surface::Clear(GEColor& inColor, float inAlpha)
{
    return ee_dx7s_clear(this->mSurface, GEPackColor(this->mDescription.mPixelFormat, inColor, inAlpha));
}

// FUNCTION: DX7HR 0x1000ee8c
// Fills a rectangle of the surface with a colour and alpha, retrying while the blitter is busy; 0xd when lost
long DX7Surface::Clear(GERectangle& inLogicalRectangle, GEColor& inColor, float inAlpha)
{
    float theRect[4];
    DX7RectFloats(inLogicalRectangle, theRect);
    EeDx7LogicalMetrics theMetrics = DX7Metrics(this);
    return ee_dx7s_clear_rect(this->mSurface, theRect, &theMetrics, GEPackColor(this->mDescription.mPixelFormat, inColor, inAlpha));
}

// FUNCTION: DX7HR 0x1000ef4b
// Blits a source rectangle onto a target surface's rectangle, optionally colour-keyed; 0xd when lost or busy
long DX7Surface::CopyTo(GESurface* inTarget, GERectangle& inLogicalSource, GERectangle& inLogicalDestination,
                        bool inTransparent)
{
    float theSource[4];
    float theDestination[4];
    DX7RectFloats(inLogicalSource, theSource);
    DX7RectFloats(inLogicalDestination, theDestination);
    EeDx7LogicalMetrics theSourceMetrics = DX7Metrics(this);
    EeDx7LogicalMetrics theTargetMetrics = DX7Metrics(inTarget);
    return ee_dx7s_copy_to(this->mSurface, static_cast<DX7Surface*>(inTarget)->mSurface, theSource, &theSourceMetrics,
                           theDestination, &theTargetMetrics, inTransparent);
}

// FUNCTION: DX7HR 0x1000f01d
// Copies a rectangle to the target with bilinear scaling, choosing the 32-bit or 16-bit path by this surface's depth
long DX7Surface::CopyWithFilter(GESurface* inTarget, GERectangle& inLogicalSource, GERectangle& inLogicalDestination)
{
    // port: logical rectangles -> each surface's pixels
    float theLogical[4];
    float thePixels[4];
    DX7RectFloats(inLogicalSource, theLogical);
    EeDx7LogicalMetrics theMetrics = DX7Metrics(this);
    ee_dx7s_pixel_rect(theLogical, &theMetrics, thePixels);
    GERectangle inSource(thePixels[0], thePixels[1], thePixels[2], thePixels[3]);
    DX7RectFloats(inLogicalDestination, theLogical);
    theMetrics = DX7Metrics(inTarget);
    ee_dx7s_pixel_rect(theLogical, &theMetrics, thePixels);
    GERectangle inDestination(thePixels[0], thePixels[1], thePixels[2], thePixels[3]);
    if (this->mDescription.mPixelDepth == 32)
        return this->Copy32BitWithFilter(static_cast<DX7Surface*>(inTarget), inSource, inDestination);
    return this->Copy16BitWithFilter(static_cast<DX7Surface*>(inTarget), inSource, inDestination);
}

// FUNCTION: DX7HR 0x1000f03f
// Copies a rectangle to the target with bilinear scaling, 16-bit pixels; returns this surface's Unlock result,
// so a failed target lock is not reported
long DX7Surface::Copy16BitWithFilter(DX7Surface* inTarget, GERectangle& inSource, GERectangle& inDestination)
{
    long theResult = this->Lock();
    if (theResult != kErrorOK)
        return theResult;
    theResult = inTarget->Lock();
    if (theResult == kErrorOK) {
        const float theSourceRect[4] = { inSource.GetLeft(), inSource.GetTop(), inSource.GetRight(), inSource.GetBottom() };
        const float theDestinationRect[4] = { inDestination.GetLeft(), inDestination.GetTop(), inDestination.GetRight(),
                                              inDestination.GetBottom() };
        ee_dx7_copy_with_filter_16(this->mDescription.mPixelFormat, reinterpret_cast<unsigned short*>(this->mMemoryAddress), this->mBytesPerRow / 2,
           reinterpret_cast<unsigned short*>(inTarget->mMemoryAddress), inTarget->mBytesPerRow / 2, theSourceRect,
           theDestinationRect);
        inTarget->Unlock();
    }
    theResult = this->Unlock();
    return theResult;
}

// FUNCTION: DX7HR 0x1000f4d0
// Copies a rectangle to the target with bilinear scaling, 32-bit pixels (as the 16-bit copy)
long DX7Surface::Copy32BitWithFilter(DX7Surface* inTarget, GERectangle& inSource, GERectangle& inDestination)
{
    long theResult = this->Lock();
    if (theResult != kErrorOK)
        return theResult;
    theResult = inTarget->Lock();
    if (theResult == kErrorOK) {
        const float theSourceRect[4] = { inSource.GetLeft(), inSource.GetTop(), inSource.GetRight(), inSource.GetBottom() };
        const float theDestinationRect[4] = { inDestination.GetLeft(), inDestination.GetTop(), inDestination.GetRight(),
                                              inDestination.GetBottom() };
        ee_dx7_copy_with_filter_32(this->mDescription.mPixelFormat, reinterpret_cast<uint32_t*>(this->mMemoryAddress), this->mBytesPerRow / 4,
           reinterpret_cast<uint32_t*>(inTarget->mMemoryAddress), inTarget->mBytesPerRow / 4, theSourceRect,
           theDestinationRect);
        inTarget->Unlock();
    }
    theResult = this->Unlock();
    return theResult;
}

// FUNCTION: DX7HR 0x1000f960
// Detaches the attached surface at an index and drops its reference; native error on failure
long DX7Surface::DetachSurface(unsigned long inIndex)
{
    GESurface* theSurface = this->mAttachedSurfaces[inIndex];
    long theResult = ee_dx7s_detach(this->mSurface, static_cast<DX7Surface*>(theSurface)->mSurface);
    if (theResult == kErrorOK) {
        this->mAttachedSurfaces.erase(this->mAttachedSurfaces.begin() + inIndex);
        theSurface->Dereference();
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000f9d9
// Flips the flipping chain, waiting for vertical sync only when asked; 0xd when the surface is lost
long DX7Surface::Flip(bool inWait)
{
    return ee_dx7s_flip(this->mSurface, inWait);
}

// FUNCTION: DX7HR 0x1000fa2e
// Returns the memory size of the surface plus every attached surface, in bytes
unsigned long DX7Surface::GetAllocatedSize()
{
    unsigned long theSize = ee_dx7s_own_size(this->mDescription.mPixelDepth, this->mDescription.mHeight, this->mDescription.mWidth);
    for (unsigned long i = 0; i < this->mAttachedSurfaces.size(); i++)
        theSize += this->mAttachedSurfaces[i]->GetAllocatedSize();
    return theSize;
}

// FUNCTION: DX7HR 0x1000fa69
// Locks the whole surface and records its memory address and row pitch; 0xd when the surface is lost
long DX7Surface::Lock()
{
    EeDx7LockOutcome theOutcome;
    ee_dx7s_lock(this->mSurface, &theOutcome);
    if (theOutcome.locked) {
        this->mMemoryAddress = (unsigned char*)theOutcome.memory;
        this->mIsLocked = true;
        this->mBytesPerRow = theOutcome.pitch;
    }
    return theOutcome.result;
}

// FUNCTION: DX7HR 0x1000faed
// Restores a lost surface's memory; native error on failure
long DX7Surface::RestoreSurface()
{
    return ee_dx7s_restore(this->mSurface);
}

// FUNCTION: DX7HR 0x1000fb0c
// Sets the source colour key used for transparent blits
void DX7Surface::SetTransparencyColor(GEColor& inColor)
{
    this->mTransparencyColor = inColor;
    ee_dx7s_set_transparency_color(this->mSurface, GEPackColor(this->mDescription.mPixelFormat, inColor, 1.0f));
}

// FUNCTION: DX7HR 0x1000fb55
// Marks the surface locked for text drawing (the original took a GDI device context on it; text now draws through
// GXSurface::DrawText into its CPU copy while it is text-locked); already-exists error when it is
long DX7Surface::TextLock()
{
    return ee_dx7s_text_lock(&this->mIsTextLocked);
}

// FUNCTION: DX7HR 0x1000fba9
// Ends a text lock; already-exists error when there is none
long DX7Surface::TextUnlock()
{
    return ee_dx7s_text_unlock(&this->mIsTextLocked);
}

// FUNCTION: DX7HR 0x1000fbd8
// Unlocks the surface (an unlocked surface is not an error) and clears the locked flag
long DX7Surface::Unlock()
{
    bool theCleared = false;
    long theResult = ee_dx7s_unlock(this->mSurface, &theCleared);
    if (theCleared)
        this->mIsLocked = false;
    return theResult;
}

// port: the GPU copy's pixels per logical pixel (a scaled back buffer's render scale)
float DX7Surface::GetRenderScale() const
{
    return this->mSurface != NULL ? this->mSurface->GetRenderScale() : 1.0f;
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
