// C:\EEX\Low-Level Engine\Graphics Engine\GESurface.h (RECONSTRUCTION). Abstract in the original (the renderer plug-ins
// implemented it: DX7HRDisplay DX7Surface, vtable 0x104c4); the game links one renderer, so it is DX7Surface itself.
#ifndef LLE_GESURFACE_H
#define LLE_GESURFACE_H
#include "rust/render/gestate.h"
#include "rust/render/ge.h"
#include "rust/render/dx7.h"

#include "lle/LLEForward.h"
class GXSurface;
struct HDC__;

// How a surface is created (GEScreen::CreateSurface). Fields from GETextureManager::LoadTexture/RefreshAllTextures
// (0x33cd4, 0x341d9), which fill it from a GETextureDescription or from an existing surface's getters, and from
// GETexture::Load (0x32ec4: type, size, mip levels). Names are provisional; sizeof 0x24 (copied with rep movsd 9).
struct GESurfaceDescription : EeGeSurfaceDescription {   // the record is Rust's (ee_render ge texobj.rs)
    GESurfaceDescription() { this->mNoAlpha = false; }  // the only store before the fields are filled in
};

// The game's one surface (LLE round 13: DX7Surface folded in, its virtuals gone): every member is one call of the Rust
// object (rust/crates/ee_render/src/dx7/surfobj.rs: the description, the attached surfaces, the transparency colour,
// the GX surface, the lock state); the class keeps the reference count and the logical-size members. The model store
// gate (tools/eemodel/storegate) links its own memory surfaces as the ee_dx7sf_* functions.
class LLE_API GESurface : public UReferenceCounter {   // sizeof 8 (vector deleting dtor 0xdf58)
public:
    // Wraps a GX surface with its description and parent screen, unlocked and with no attachments (DX7Surface's ctor
    // 0x1000eced; DX7Rasterizer.cpp: the host is there)
    GESurface(GXSurface* inSurface, const GESurfaceDescription& inDescription, GEScreen* inParentScreen);
    operator GXSurface*() { return static_cast<GXSurface*>(ee_dx7sf_gx(this->mDX7)); }

    long Clear(GEColor& inColor, float inAlpha) { return ee_dx7sf_clear(this->mDX7, NULL, reinterpret_cast<const float*>(&inColor), inAlpha); }
    long Clear(GERectangle& inRectangle, GEColor& inColor, float inAlpha)
        { return ee_dx7sf_clear(this->mDX7, reinterpret_cast<const float*>(&inRectangle), reinterpret_cast<const float*>(&inColor), inAlpha); }
    long CopyTo(GESurface* inTarget, GERectangle& inSource, GERectangle& inDestination, bool inTransparent)
        { return ee_dx7sf_copy_to(this->mDX7, inTarget, reinterpret_cast<const float*>(&inSource), reinterpret_cast<const float*>(&inDestination), inTransparent); }
    long CopyWithFilter(GESurface* inTarget, GERectangle& inSource, GERectangle& inDestination)
        { return ee_dx7sf_copy_with_filter(this->mDX7, inTarget, reinterpret_cast<const float*>(&inSource), reinterpret_cast<const float*>(&inDestination)); }
    long Flip(bool inWait) { return ee_dx7sf_flip(this->mDX7, inWait); }
    unsigned long GetAllocatedSize() { return ee_dx7sf_allocated_size(this->mDX7); }
    GESurface* GetAttachedSurface(unsigned long inIndex) { return static_cast<GESurface*>(ee_dx7sf_attached(this->mDX7, inIndex)); }
    unsigned long GetAttachedSurfaceCount() { return ee_dx7sf_attached_count(this->mDX7); }
    unsigned long GetBytesPerRow() const { return ee_dx7sf_bytes_per_row(this->mDX7); }
    unsigned long GetHeight() const { return ee_dx7sf_logical_size(this->mDX7, this->mLogicalHeight, this->mLogicalShift, true); }
    bool GetIs3DRenderable() const { return this->GetSurfaceDescription().mIs3DRenderable; }
    bool GetIsTransparencyEnabled() const { return this->GetSurfaceDescription().mIsTransparencyEnabled; }
    unsigned char* GetMemoryAddress() const { return ee_dx7sf_memory(this->mDX7); }
    GEScreen* GetParentScreen() const { return static_cast<GEScreen*>(ee_dx7sf_parent(this->mDX7)); }
    unsigned long GetPixelFormat() const { return this->GetSurfaceDescription().mPixelFormat; }
    unsigned long GetPixelDepth() const { return this->GetSurfaceDescription().mPixelDepth; }
    unsigned long GetSurfaceVolatility() const { return this->GetSurfaceDescription().mSurfaceVolatility; }
    const GESurfaceDescription& GetSurfaceDescription() const { return *reinterpret_cast<const GESurfaceDescription*>(ee_dx7sf_description(this->mDX7)); }
    unsigned long GetType() const { return this->GetSurfaceDescription().mType; }
    unsigned long GetWidth() const { return ee_dx7sf_logical_size(this->mDX7, this->mLogicalWidth, this->mLogicalShift, false); }
    long Lock() { return ee_dx7sf_lock(this->mDX7); }
    void SetTransparencyColor(GEColor& inColor) { ee_dx7sf_set_transparency_color(this->mDX7, reinterpret_cast<const float*>(&inColor)); }
    long TextUnlock() { return ee_dx7sf_text_lock(this->mDX7, false); }
    long Unlock() { return ee_dx7sf_unlock(this->mDX7); }
    // port: the pixels the GPU copy of a scaled back buffer holds per logical pixel (the render scale: the GX
    // renderer draws the game's logical pixels into a larger texture); 1 for every other surface
    float GetRenderScale() const { return ee_dx7sf_render_scale(this->mDX7); }

    // Image files (inType: GEImageFileType): Rust (ee_openformats surface_file.rs), GETexture.cpp forwards
    long Load(FSFileSpec& inFile, unsigned char inType);
    long Save(FSFileSpec& inFile, unsigned char inType);
    static long GetImageFileAttributes(FSFileSpec& inFile, unsigned char inType, unsigned long& outWidth,
                                       unsigned long& outHeight, unsigned long& outDepth);

    // port: a surface's two sizes (docs/DATA_FORMATS.md "Texture packs"). GetWidth/GetHeight are its LOGICAL size,
    // the one every layout, source rectangle, UV and hot spot of the game is computed in; its PIXEL size (what its
    // memory and GPU copy hold) is that times 1 << the logical shift: 0 for every surface but a texture pack's (2:
    // 4x pixels, 1: 2x). The surface converts at the bottom: CopyTo, CopyWithFilter and Clear take logical
    // rectangles; Lock/GetMemoryAddress/GetBytesPerRow address pixels (CPU code reads GetPixelWidth/Height).
    unsigned long GetLogicalShift() const { return this->mLogicalShift; }
    void SetLogicalShift(unsigned long inShift) { this->mLogicalShift = inShift; }
    // An explicit logical size for a surface whose pixels are not a power of two times it (the WON lobby's frame:
    // 1024x768 logical at the window's pixels); 0, 0 goes back to the logical shift
    void SetLogicalSize(unsigned long inWidth, unsigned long inHeight) { this->mLogicalWidth = inWidth; this->mLogicalHeight = inHeight; }
    unsigned long GetLogicalWidthSet() const { return this->mLogicalWidth; }
    unsigned long GetLogicalHeightSet() const { return this->mLogicalHeight; }
    unsigned long GetPixelWidth() const { return this->GetSurfaceDescription().mWidth; }
    unsigned long GetPixelHeight() const { return this->GetSurfaceDescription().mHeight; }

    EeDx7Surface* mDX7;                     // the surface (ee_render dx7 surfobj.rs)

    virtual ~GESurface() { ee_dx7sf_destroy(this->mDX7); }   // (exported protected: ??1GESurface@@MAE@XZ)
    unsigned long mLogicalShift = 0;        // port: see GetLogicalShift
    unsigned long mLogicalWidth = 0;        // port: see SetLogicalSize (0: none)
    unsigned long mLogicalHeight = 0;
};

#endif
