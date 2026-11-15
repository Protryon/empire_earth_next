// DX7Surface (RECONSTRUCTION): a GESurface over a GXSurface, whole in Rust (rust/crates/ee_dx7/src/surfobj.rs, R4.3
// part 14: the description, the attached surfaces, the transparency colour, the lock state). The class keeps its
// vtable, the GESurface base (reference count, logical size) and one member, the object. The original's layout:
// sizeof 0x60 (ctor 0x1000eced); DX7Screen::CreateSurface builds them.
#ifndef DX7_DX7SURFACE_H
#define DX7_DX7SURFACE_H

#include "dx7/DX7Common.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "rust/ee_dx7.h"

class DX7_API DX7Surface : public GESurface {
public:
    DX7Surface(GXSurface* inSurface, const GESurfaceDescription& inDescription, GEScreen* inParentScreen);
    operator GXSurface*() { return static_cast<GXSurface*>(ee_dx7sf_gx(this->mDX7)); }

    virtual long AttachSurface(GESurface* inSurface) { return ee_dx7sf_attach(this->mDX7, inSurface); }
    virtual long Clear(GEColor& inColor, float inAlpha) { return ee_dx7sf_clear(this->mDX7, NULL, reinterpret_cast<const float*>(&inColor), inAlpha); }
    virtual long Clear(GERectangle& inRectangle, GEColor& inColor, float inAlpha)
        { return ee_dx7sf_clear(this->mDX7, reinterpret_cast<const float*>(&inRectangle), reinterpret_cast<const float*>(&inColor), inAlpha); }
    virtual long CopyTo(GESurface* inTarget, GERectangle& inSource, GERectangle& inDestination, bool inTransparent)
        { return ee_dx7sf_copy_to(this->mDX7, inTarget, reinterpret_cast<const float*>(&inSource), reinterpret_cast<const float*>(&inDestination), inTransparent); }
    virtual long CopyWithFilter(GESurface* inTarget, GERectangle& inSource, GERectangle& inDestination)
        { return ee_dx7sf_copy_with_filter(this->mDX7, inTarget, reinterpret_cast<const float*>(&inSource), reinterpret_cast<const float*>(&inDestination)); }
    virtual long DetachSurface(unsigned long inIndex) { return ee_dx7sf_detach(this->mDX7, inIndex); }
    virtual long Flip(bool inWait) { return ee_dx7sf_flip(this->mDX7, inWait); }
    virtual unsigned long GetAllocatedSize() { return ee_dx7sf_allocated_size(this->mDX7); }
    virtual GESurface* GetAttachedSurface(unsigned long inIndex) { return static_cast<GESurface*>(ee_dx7sf_attached(this->mDX7, inIndex)); }
    virtual unsigned long GetAttachedSurfaceCount() { return ee_dx7sf_attached_count(this->mDX7); }
    virtual unsigned long GetBytesPerRow() const { return ee_dx7sf_bytes_per_row(this->mDX7); }
    virtual unsigned long GetHeight() const { return ee_dx7sf_logical_size(this->mDX7, this->mLogicalHeight, this->mLogicalShift, true); }
    virtual bool GetIs3DRenderable() const { return this->Description().mIs3DRenderable; }
    virtual bool GetIsTextLocked() const { return ee_dx7sf_text_locked(this->mDX7); }
    virtual bool GetIsTransparencyEnabled() const { return this->Description().mIsTransparencyEnabled; }
    virtual unsigned char* GetMemoryAddress() const { return ee_dx7sf_memory(this->mDX7); }
    virtual GEScreen* GetParentScreen() const { return static_cast<GEScreen*>(ee_dx7sf_parent(this->mDX7)); }
    virtual unsigned long GetPixelFormat() const { return this->Description().mPixelFormat; }
    virtual unsigned long GetPixelDepth() const { return this->Description().mPixelDepth; }
    virtual unsigned long GetSurfaceVolatility() const { return this->Description().mSurfaceVolatility; }
    virtual const GESurfaceDescription& GetSurfaceDescription() const { return this->Description(); }
    virtual const GEColor& GetTransparencyColor() const { return *reinterpret_cast<const GEColor*>(ee_dx7sf_transparency(this->mDX7)); }
    virtual unsigned long GetType() const { return this->Description().mType; }
    virtual unsigned long GetWidth() const { return ee_dx7sf_logical_size(this->mDX7, this->mLogicalWidth, this->mLogicalShift, false); }
    virtual long Lock() { return ee_dx7sf_lock(this->mDX7); }
    virtual long RestoreSurface() { return ee_dx7sf_restore(this->mDX7); }
    virtual void SetTransparencyColor(GEColor& inColor) { ee_dx7sf_set_transparency_color(this->mDX7, reinterpret_cast<const float*>(&inColor)); }
    virtual long TextLock() { return ee_dx7sf_text_lock(this->mDX7, true); }
    virtual long TextUnlock() { return ee_dx7sf_text_lock(this->mDX7, false); }
    virtual long Unlock() { return ee_dx7sf_unlock(this->mDX7); }
    virtual float GetRenderScale() const { return ee_dx7sf_render_scale(this->mDX7); }   // port: GXSurface::GetRenderScale

    EeDx7Surface* mDX7;

protected:
    virtual ~DX7Surface() { ee_dx7sf_destroy(this->mDX7); }
    const GESurfaceDescription& Description() const
        { return *reinterpret_cast<const GESurfaceDescription*>(ee_dx7sf_description(this->mDX7)); }
};

#endif
