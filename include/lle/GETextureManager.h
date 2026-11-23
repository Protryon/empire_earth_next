// C:\EEX\Low-Level Engine\Graphics Engine\GETextureManager.h (RECONSTRUCTION). GETextureManager.cpp (__FILE__
// anchor, ~0x10033bef) holds the real code; the getters and the implicit copy members are inline.
#ifndef LLE_GETEXTUREMANAGER_H
#define LLE_GETEXTUREMANAGER_H

#include <map>   // (the original's cache; kept for the units that rely on it)
#include <stdint.h>
#include "rust/render/ge.h"   // the manager (ee_ge texobj.rs), its host's EeGeManOp
#include "lle/UString.h"   // LoadTexture's name

class FSFileSpec;
class FSPath;
class GEScreen;
class GESurface;
class GETexture;

// How a texture file is loaded (the original's sizeof 0x14, copied with `rep movsd` x5 by the User Interface
// backgrounds, UITextureBackground 0x1005c84e; 0x18 with the port's mUseTexturePack). Fields from
// GETextureManager::LoadTexture (0x33cd4), which turns it into a GESurfaceDescription; (provisional names)
struct GETextureDescription {
    unsigned long mSurfaceVolatility;       // +0x00 -> GESurfaceDescription::mSurfaceVolatility
    unsigned long mFrameFormat;             // +0x04 GETexture::Load's frame format
    unsigned long mPixelDepth;              // +0x08 16 or 32
    long mNumMipLevelsToDrop;               // +0x0c
    bool mIs3DRenderable;                   // +0x10
    bool mIsTransparencyEnabled;            // +0x11
    bool mHasAlpha;                         // +0x12
    bool mCreateMIPMaps;                    // +0x13
    // port: the texture may come from the texture pack (GETextureManager::SetTexturePack). True unless a caller
    // clears it: a pack texture keeps the original's (logical) size for the game's code (GESurface::GetLogicalShift),
    // so the 2D/UI code lays out as before; a caller that reads a texture's pixels as data would clear it.
    bool mUseTexturePack = true;            // +0x14
};

// port (R4.3, seventh part): a whole Rust object (ee_ge texobj.rs); this class is its shell.
class LLE_API GETextureManager {            // sizeof 0x1c in the original (implicit copy ctor 0xdfd3, operator= 0xe013)
public:
    // a texture manager on a search path (referenced), with no textures and its own lock
    GETextureManager(FSPath* inTexturePath) { this->mRust = ee_ge_texman_new(&GETextureManager::Host, this, inTexturePath); }
    // removes every unreferenced texture, releases the path and the lock
    ~GETextureManager() { ee_ge_texman_free(this->mRust); }
    GETextureManager(const GETextureManager&) = delete;   // port: one Rust object per manager
    GETextureManager& operator=(const GETextureManager&) = delete;
    // Prints every loaded texture file and its reference count to a dump file
    void DumpAllLoadedMedia(FSFileSpec& inFile) { ee_ge_texman_dump(this->mRust, &inFile); }
    // the sum of the cached textures' GetAllocatedSize()
    unsigned long GetMemoryFootprint() { return ee_ge_texman_footprint(this->mRust); }
    GEScreen* GetScreen() const { return (GEScreen*)ee_ge_texman_screen(this->mRust); }
    unsigned long GetTextureCount() { return ee_ge_texman_count(this->mRust); }
    // port: the cache's textures in name order (the store gate)
    GETexture* GetTextureAt(unsigned long inIndex) { return (GETexture*)ee_ge_texman_at(this->mRust, inIndex); }
    FSPath* GetTexturePath() { return (FSPath*)ee_ge_texman_path(this->mRust); }
    GETexture* LoadTexture(const UString& inName, const GETextureDescription& inDescription)
    {
        return (GETexture*)ee_ge_texman_load(this->mRust, inName, (const EeGeTextureDescription*)&inDescription);
    }
    // Wraps a surface in a new managed texture, named by a running counter, with modulate blending
    GETexture* MakeTexture(GESurface* inSurface) { return (GETexture*)ee_ge_texman_make(this->mRust, inSurface); }
    // Reloads every cached texture from its files (or recreates its surface when they are gone),
    // then reapplies the frames' saved stage settings to the whole texture; returns the last result
    long RefreshAllTextures() { return ee_ge_texman_refresh_all(this->mRust); }
    long ReloadTexture(const UString& inName, const GETextureDescription& inDescription)
        { return ee_ge_texman_reload(this->mRust, inName, (const EeGeTextureDescription*)&inDescription); }
    // Removes and releases every texture nobody else references; returns 5 if any are still in use
    long RemoveAllTextures() { return ee_ge_texman_remove_all(this->mRust); }
    // Removes and releases one texture if nobody else references it; 5 if still in use, 3 if not managed here
    long RemoveTexture(GETexture* inTexture) { return ee_ge_texman_remove(this->mRust, inTexture); }
    void SetScreen(GEScreen* inScreen) { ee_ge_texman_set_screen(this->mRust, inScreen); }
    // Replaces the texture search path, moving the reference
    void SetTexturePath(FSPath* inTexturePath) { ee_ge_texman_set_path(this->mRust, inTexturePath); }
    // Sets the MIP filter mode of every cached texture that uses MIP filtering
    void UpdateMIPFilterMode(unsigned long inMode) { ee_ge_texman_update_mip_filter(this->mRust, inMode); }
    // port: the optional texture pack (docs/DATA_FORMATS.md "Texture packs"). inDirectory (e.g. "textures-4x")
    // stands in for a texture's "textures" directory and holds files 1 << inScaleShift times the originals' size: a
    // description with mUseTexturePack loads the pack's file of the same name when the path has one (else the
    // original), dropping inMipLevelsToDrop more MIP levels (each halves the size; 2x box filtered). Its surfaces
    // keep the original's size as their logical size (GESurface::GetLogicalShift: inScaleShift - inMipLevelsToDrop)
    // and get MIP levels. NULL or "" turns the pack off. Every texture manager; textures already cached keep the file
    // they were loaded from until they are released.
    static void SetTexturePack(const char* inDirectory, long inScaleShift, long inMipLevelsToDrop)
        { ee_ge_texpack_set(inDirectory, inScaleShift, inMipLevelsToDrop); }
    // the pack's name of a "textures\..." file ("textures\x" -> "textures-4x\x"; false for a name outside
    // "textures\" or with no pack directory, for a picture read as data and while the pack is off), and the logical
    // shift and extra MIP drop of its files: for pictures loaded around the texture manager (RTSMouse's pointers)
    static bool GetTexturePackName(const UString& inName, UString& outName) { return ee_ge_texpack_name(inName, &outName); }
    static long GetTexturePackLogicalShift() { return ee_ge_texpack_logical_shift(); }
    static long GetTexturePackMipLevelsToDrop() { return ee_ge_texpack_mip_levels_to_drop(); }
private:
    friend class GETexture;
    // Marks the texture as promoted; always succeeds
    long PromoteTexture(GETexture* inTexture) { return ee_ge_texman_promote(this->mRust, inTexture); }
    // the calls the Rust object makes on other objects (GETexture.cpp)
    static intptr_t Host(void* inOwner, EeGeManOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c);

    EeGeTextureManager* mRust;              // the cache (name -> texture), the lock, the path, the screen, the footprint
};

#endif
