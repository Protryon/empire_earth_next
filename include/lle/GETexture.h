// C:\EEX\Low-Level Engine\Graphics Engine\GETexture.h (RECONSTRUCTION). GETexture.cpp (~0x10032cfa) holds the
// ctors, dtor and the heavy members; the accessors are inline (GEBitmap.cpp emits them first, 0x1000e040..).
//
// port (R4.3, eighth part): the texture is a whole Rust object (ee_ge texobj_tex.rs): its stage, frames, surface,
// screen, sizes and flags with every method. The class's only member is that object; the accessors below forward
// to it, and GETexture.cpp is the forwarders and the host (the calls the texture makes on other objects).
#ifndef LLE_GETEXTURE_H
#define LLE_GETEXTURE_H

#include <stdint.h>
#include "rust/base/png.h"   // the texture (ee_ge texobj_tex.rs), its host's EeGeTexOp
#include "rust/render/ge.h"

class FSFileSpec;
class GERasterizer;
class GEScreen;
class GESurface;
class GETexture;
class GETextureManager;
struct GESurfaceDescription;
struct EeOfTexture;                        // a texture decoded by Rust (rust/ee_openformats.h)

struct GETStage {                           // sizeof 0x20 (copied with rep movsd in GETexture's copy ctor)
    GETexture* mTexture;                    // +0x00 (GetStageTexture)
    unsigned long mColorMode;               // +0x04 (SetStageColorMode)
    unsigned long mAlphaMode;               // +0x08 (SetStageAlphaMode)
    unsigned long mTextureAddressingMode;   // +0x0c
    unsigned long mMinFilterMode;           // +0x10
    unsigned long mMagFilterMode;           // +0x14
    unsigned long mMipFilterMode;           // +0x18
    float mMipMapBias;                      // +0x1c
};

class LLE_API GETexture : public UReferenceCounter {   // sizeof 0x6c (vector deleting dtor 0xe255)
public:

    void* GetData() { return (void*)ee_ge_tex_get(this->mRust, EeGeTexVal::FrameData); }
    bool GetCreateMIPMaps() const { return ee_ge_tex_get(this->mRust, EeGeTexVal::CreateMipMaps) != 0; }
    float GetFrameHeight() const { return this->GetFrameSurface()->GetHeight() * this->GetFrameUVSize(); }
    GESurface* GetFrameSurface() const { return (GESurface*)ee_ge_tex_get(this->mRust, EeGeTexVal::FrameSurface); }
    const GETextureCoordinate& GetFrameTopLeftCorner() const
        { return *(const GETextureCoordinate*)ee_ge_tex_corner(this->mRust); }
    float GetFrameWidth() const { return this->GetFrameSurface()->GetWidth() * this->GetFrameUVSize(); }
    unsigned long GetHeight() const { return this->GetSurface()->GetHeight(); }   // logical (GESurface::GetLogicalShift)
    unsigned long GetPixelFormat() const { return this->GetFrameSurface()->GetPixelFormat(); }
    GETexture* GetStageTexture(unsigned long inStage) const { return this->GetStage()->mTexture; }
    unsigned long GetWidth() const { return this->GetSurface()->GetWidth(); }
    bool HasChanged() const { return ee_ge_tex_get(this->mRust, EeGeTexVal::HasChanged) != 0; }
    long Promote() { return ee_ge_tex_promote(this->mRust); }
    void SetData(void* inData) { ee_ge_tex_set(this->mRust, EeGeTexVal::FrameData, (intptr_t)inData); }
    void ValidateChanges() { ee_ge_tex_set(this->mRust, EeGeTexVal::HasChanged, 0); }

    unsigned long GetFrame() const { return (unsigned long)ee_ge_tex_get(this->mRust, EeGeTexVal::Frame); }
    unsigned long GetFrameCount() const { return (unsigned long)ee_ge_tex_get(this->mRust, EeGeTexVal::FrameCount); }
    float GetFrameUVSize() const { return ee_ge_tex_frame_uv_size(this->mRust); }
    unsigned long GetMipLevelCount() const
        { return (unsigned long)ee_ge_tex_get(this->mRust, EeGeTexVal::MipLevelCount); }
    unsigned long GetNumMipLevelsToDrop() const
        { return (unsigned long)ee_ge_tex_get(this->mRust, EeGeTexVal::NumMipLevelsToDrop); }
    GEScreen* GetScreen() const { return (GEScreen*)ee_ge_tex_get(this->mRust, EeGeTexVal::Screen); }
    const GETStage* GetStage() const { return (const GETStage*)ee_ge_tex_stage(this->mRust); }
    GESurface* GetSurface() const { return (GESurface*)ee_ge_tex_get(this->mRust, EeGeTexVal::Surface); }

    // Drops a reference; at the last outside reference a managed texture removes itself from its manager
    void Dereference() { ee_ge_tex_dereference(this->mRust); }
    // Returns the surface's allocated size, times the frame count when each frame has its own texture
    unsigned long GetAllocatedSize() { return ee_ge_tex_allocated_size(this->mRust); }
    long Lock(unsigned char*& outMemory, unsigned long& outBytesPerRow);
    // (Save 0x10033749 wrote a texture file (.sst): retired under D9)
    // Shows the given animation frame: picks the frame texture, or moves the UV corner to its grid cell
    void SetFrame(unsigned long inFrame) { ee_ge_tex_set_frame(this->mRust, inFrame); }
    void SetStageAlphaMode(unsigned long inMode, unsigned long inStage) { ee_ge_tex_set_stage(this->mRust, EeGeTexStageField::AlphaMode, inMode); }
    void SetStageColorMode(unsigned long inMode, unsigned long inStage) { ee_ge_tex_set_stage(this->mRust, EeGeTexStageField::ColorMode, inMode); }
    void SetStageMagFilterMode(unsigned long inMode, unsigned long inStage) { ee_ge_tex_set_stage(this->mRust, EeGeTexStageField::MagFilterMode, inMode); }
    void SetStageMinFilterMode(unsigned long inMode, unsigned long inStage) { ee_ge_tex_set_stage(this->mRust, EeGeTexStageField::MinFilterMode, inMode); }
    // (a texture pack's texture keeps MIP filtering where none is asked for: texobj_tex.rs)
    void SetStageMipFilterMode(unsigned long inMode, unsigned long inStage) { ee_ge_tex_set_stage(this->mRust, EeGeTexStageField::MipFilterMode, inMode); }
    void SetStageMipMapBias(float inBias, unsigned long inStage) { ee_ge_tex_set_stage_bias(this->mRust, inBias); }
    void SetStageTextureAddressingMode(unsigned long inMode, unsigned long inStage)
    {
        ee_ge_tex_set_stage(this->mRust, EeGeTexStageField::TextureAddressingMode, inMode);
    }
    // Replaces the texture's surface (new rasterizer handle, promotion cleared); already-exists error if unchanged
    long SetSurface(GESurface* inSurface) { return ee_ge_tex_set_surface(this->mRust, inSurface); }
    // Unlocks the texture's surface
    long Unlock() { return ee_ge_tex_unlock(this->mRust); }
protected:
    virtual ~GETexture();                   // ??1GETexture@@MAE@XZ (protected: released through Dereference)
private:
    friend class GETextureManager;
    friend struct GEStoreDigest;            // tools/eemodel/storegate: reads what the loaders stored
    GETexture(GETextureManager* inManager);                         // ??0GETexture@@AAE@PAVGETextureManager@@@Z
    GETexture(GESurface* inSurface, GETextureManager* inManager);   // private too (AAE)
    GETexture(const GETexture&) = delete;
    GETexture& operator=(const GETexture&) = delete;
    unsigned long GetFrameFormat() const   // ?GetFrameFormat@GETexture@@ABEKXZ
        { return (unsigned long)ee_ge_tex_get(this->mRust, EeGeTexVal::FrameFormat); }
    // port (D9): the frames of a texture's PNG files, decoded by Rust (GEFindTextureFile), in place of a texture file
    long Load(const EeOfTexture* inTexture, GESurfaceDescription& inDescription, GEScreen* inScreen,
              bool inCreateMIPMaps, long inNumMipLevelsToDrop, unsigned long inFrameFormat);
    // port: the members the friends read and write (the manager's host, the store gate's digest), this texture's own
    // (GetData, GetIsCompressed and GetIsPromoted read the current frame's)
    bool GetIsManaged() const { return ee_ge_tex_get(this->mRust, EeGeTexVal::IsManaged) != 0; }
    GETexture* GetCurrentFrameTexture() const
        { return (GETexture*)ee_ge_tex_get(this->mRust, EeGeTexVal::FrameTexture); }
    GETexture* const* GetFrames() const { return (GETexture* const*)ee_ge_tex_frames(this->mRust); }
    unsigned long GetFramesPerRow() const
        { return (unsigned long)ee_ge_tex_get(this->mRust, EeGeTexVal::FramesPerRow); }
    void* GetOwnData() const { return (void*)ee_ge_tex_get(this->mRust, EeGeTexVal::Data); }
    bool GetOwnIsCompressed() const { return ee_ge_tex_get(this->mRust, EeGeTexVal::IsCompressed) != 0; }
    bool GetOwnIsPromoted() const { return ee_ge_tex_get(this->mRust, EeGeTexVal::IsPromoted) != 0; }
    bool GetIsFromTexturePack() const { return ee_ge_tex_get(this->mRust, EeGeTexVal::FromPack) != 0; }
    long GetTexturePackMipLevels() const { return (long)ee_ge_tex_get(this->mRust, EeGeTexVal::PackMipLevels); }
    // port: the logical shift of every frame's surface (GESurface::GetLogicalShift; a texture pack's texture)
    void SetLogicalShift(unsigned long inShift) { ee_ge_tex_set_logical_shift(this->mRust, inShift); }
    // port: the calls the Rust texture makes on other objects (EeGeTexOp)
    static intptr_t Host(void* inOwner, EeGeTexOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c);

    EeGeTexture* mRust;                     // port: the texture (ee_ge texobj_tex.rs)

    static GERasterizer* sRasterizer;       // exported ?sRasterizer@GETexture@@0PAVGERasterizer@@A
};

#endif
