// DX7Surface (RECONSTRUCTION): a GESurface over an GXSurface. Layout from the ctor 0x1000eced and the
// inline getters (emitted by DX7HRDisplay.cpp, 0x10001503..): sizeof 0x60 (copy ctor 0x10001563). The ctor is private
// (??0DX7Surface@@AAE..): DX7Screen::CreateSurface builds them. Member names provisional where no getter names them.
#ifndef DX7_DX7SURFACE_H
#define DX7_DX7SURFACE_H

#include <vector>

#include "dx7/DX7Common.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"

class DX7_API DX7Surface : public GESurface {
    friend class DX7Screen;
public:
    operator GXSurface*() { return this->mSurface; }

    virtual long AttachSurface(GESurface* inSurface);
    virtual long Clear(GEColor& inColor, float inAlpha);
    virtual long Clear(GERectangle& inRectangle, GEColor& inColor, float inAlpha);
    virtual long CopyTo(GESurface* inTarget, GERectangle& inSource, GERectangle& inDestination, bool inTransparent);
    virtual long CopyWithFilter(GESurface* inTarget, GERectangle& inSource, GERectangle& inDestination);
    virtual long DetachSurface(unsigned long inIndex);
    virtual long Flip(bool inWait);
    virtual unsigned long GetAllocatedSize();
    virtual GESurface* GetAttachedSurface(unsigned long inIndex) { return this->mAttachedSurfaces[inIndex]; }
    virtual unsigned long GetAttachedSurfaceCount() { return this->mAttachedSurfaces.size(); }
    virtual unsigned long GetBytesPerRow() const { return this->mBytesPerRow; }
    virtual unsigned long GetHeight() const { return this->mDescription.mHeight; }
    virtual bool GetIs3DRenderable() const { return this->mDescription.mIs3DRenderable; }
    virtual bool GetIsTextLocked() const { return this->mIsTextLocked; }
    virtual bool GetIsTransparencyEnabled() const { return this->mDescription.mIsTransparencyEnabled; }
    virtual unsigned char* GetMemoryAddress() const { return this->mMemoryAddress; }
    virtual GEScreen* GetParentScreen() const { return this->mParentScreen; }
    virtual unsigned long GetPixelFormat() const { return this->mDescription.mPixelFormat; }
    virtual unsigned long GetPixelDepth() const { return this->mDescription.mPixelDepth; }
    virtual unsigned long GetSurfaceVolatility() const { return this->mDescription.mSurfaceVolatility; }
    virtual const GESurfaceDescription& GetSurfaceDescription() const { return this->mDescription; }
    virtual const GEColor& GetTransparencyColor() const { return this->mTransparencyColor; }
    virtual unsigned long GetType() const { return this->mDescription.mType; }
    virtual unsigned long GetWidth() const { return this->mDescription.mWidth; }
    virtual long Lock();
    virtual long RestoreSurface();
    virtual void SetTransparencyColor(GEColor& inColor);
    virtual long TextLock();
    virtual long TextUnlock();
    virtual long Unlock();

protected:
    virtual ~DX7Surface();
private:
    DX7Surface(GXSurface* inSurface, const GESurfaceDescription& inDescription, GEScreen* inParentScreen);
    long Copy16BitWithFilter(DX7Surface* inTarget, GERectangle& inSource, GERectangle& inDestination);
    long Copy32BitWithFilter(DX7Surface* inTarget, GERectangle& inSource, GERectangle& inDestination);

    GESurfaceDescription mDescription;      // +0x08
    std::vector<GESurface*> mAttachedSurfaces;   // +0x2c
    GEColor mTransparencyColor;             // +0x3c
    GXSurface* mSurface;          // +0x48
    GEScreen* mParentScreen;                // +0x4c
    unsigned char* mMemoryAddress;          // +0x50 (Lock: GXSURFACEDESC::lpSurface)
    void* mTextDC;                          // +0x54 (TextLock's GDI DC in the original; unused)
    unsigned long mBytesPerRow;             // +0x58 (Lock: lPitch)
    bool mIsLocked;                         // +0x5c
    bool mIsTextLocked;                     // +0x5d
};

#endif
