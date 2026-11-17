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
    GETexture* GetFrameTexture(unsigned long inFrame) const
        { return (GETexture*)ee_ge_tex_frame_texture(this->mRust, inFrame); }
    const GETextureCoordinate& GetFrameTopLeftCorner() const
        { return *(const GETextureCoordinate*)ee_ge_tex_corner(this->mRust); }
    float GetFrameWidth() const { return this->GetFrameSurface()->GetWidth() * this->GetFrameUVSize(); }
    unsigned long GetHeight() const { return this->GetSurface()->GetHeight(); }   // logical (GESurface::GetLogicalShift)
    bool GetIsCompressed() const { return ee_ge_tex_get(this->mRust, EeGeTexVal::FrameIsCompressed) != 0; }
    bool GetIsPromoted() const { return ee_ge_tex_get(this->mRust, EeGeTexVal::FrameIsPromoted) != 0; }
    unsigned long GetPixelFormat() const { return this->GetFrameSurface()->GetPixelFormat(); }
    GETexture* GetStageTexture(unsigned long inStage) const { return this->GetStage()->mTexture; }
    unsigned long GetWidth() const { return this->GetSurface()->GetWidth(); }
    bool HasChanged() const { return ee_ge_tex_get(this->mRust, EeGeTexVal::HasChanged) != 0; }
    long Promote() { return ee_ge_tex_promote(this->mRust); }
    void SetData(void* inData) { ee_ge_tex_set(this->mRust, EeGeTexVal::FrameData, (intptr_t)inData); }
    void SetHasChanged() { ee_ge_tex_set(this->mRust, EeGeTexVal::HasChanged, 1); }
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

    void Dereference();
    unsigned long GetAllocatedSize();
    long Lock(unsigned char*& outMemory, unsigned long& outBytesPerRow);
    long Recreate(GEScreen* inScreen);
    // (Save 0x10033749 wrote a texture file (.sst): retired under D9)
    void SetFrame(unsigned long inFrame);
    void SetStageAlphaMode(unsigned long inMode, unsigned long inStage);
    void SetStageColorMode(unsigned long inMode, unsigned long inStage);
    void SetStageMagFilterMode(unsigned long inMode, unsigned long inStage);
    void SetStageMinFilterMode(unsigned long inMode, unsigned long inStage);
    void SetStageMipFilterMode(unsigned long inMode, unsigned long inStage);
    void SetStageMipMapBias(float inBias, unsigned long inStage);
    void SetStageTextureAddressingMode(unsigned long inMode, unsigned long inStage);
    long SetSurface(GESurface* inSurface);
    long Unlock();
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
    void SetIsPromoted(bool inIsPromoted) { ee_ge_tex_set(this->mRust, EeGeTexVal::IsPromoted, inIsPromoted); }
    // port: the members the friends read and write (the manager's host, the store gate's digest), this texture's own
    // (GetData, GetIsCompressed and GetIsPromoted read the current frame's)
    bool GetIsManaged() const { return ee_ge_tex_get(this->mRust, EeGeTexVal::IsManaged) != 0; }
    void SetIsManaged(bool inIsManaged) { ee_ge_tex_set(this->mRust, EeGeTexVal::IsManaged, inIsManaged); }
    GETexture* GetCurrentFrameTexture() const
        { return (GETexture*)ee_ge_tex_get(this->mRust, EeGeTexVal::FrameTexture); }
    GETexture* const* GetFrames() const { return (GETexture* const*)ee_ge_tex_frames(this->mRust); }
    unsigned long GetFramesPerRow() const
        { return (unsigned long)ee_ge_tex_get(this->mRust, EeGeTexVal::FramesPerRow); }
    void* GetOwnData() const { return (void*)ee_ge_tex_get(this->mRust, EeGeTexVal::Data); }
    bool GetOwnIsCompressed() const { return ee_ge_tex_get(this->mRust, EeGeTexVal::IsCompressed) != 0; }
    bool GetOwnIsPromoted() const { return ee_ge_tex_get(this->mRust, EeGeTexVal::IsPromoted) != 0; }
    void SetNumMipLevelsToDrop(unsigned long inCount)
        { ee_ge_tex_set(this->mRust, EeGeTexVal::NumMipLevelsToDrop, (intptr_t)inCount); }
    bool GetIsFromTexturePack() const { return ee_ge_tex_get(this->mRust, EeGeTexVal::FromPack) != 0; }
    void SetIsFromTexturePack(bool inIsFromTexturePack)
        { ee_ge_tex_set(this->mRust, EeGeTexVal::FromPack, inIsFromTexturePack); }
    long GetTexturePackMipLevels() const { return (long)ee_ge_tex_get(this->mRust, EeGeTexVal::PackMipLevels); }
    void SetTexturePackMipLevels(long inLevels) { ee_ge_tex_set(this->mRust, EeGeTexVal::PackMipLevels, inLevels); }
    // port: the logical shift of every frame's surface (GESurface::GetLogicalShift; a texture pack's texture)
    void SetLogicalShift(unsigned long inShift);
    // port: the calls the Rust texture makes on other objects (EeGeTexOp)
    static intptr_t Host(void* inOwner, EeGeTexOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c);

    EeGeTexture* mRust;                     // port: the texture (ee_ge texobj_tex.rs)

    static GERasterizer* sRasterizer;       // exported ?sRasterizer@GETexture@@0PAVGERasterizer@@A
};

#endif
