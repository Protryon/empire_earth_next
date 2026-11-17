// C:\EEX\Low-Level Engine\Graphics Engine\GETextureManager.h (RECONSTRUCTION). GETextureManager.cpp (__FILE__
// anchor, ~0x10033bef) holds the real code; the getters and the implicit copy members are inline.
#ifndef LLE_GETEXTUREMANAGER_H
#define LLE_GETEXTUREMANAGER_H

#include <map>   // (the original's cache; kept for the units that rely on it)
#include <stdint.h>
#include "rust/render/ge.h"   // the manager (ee_ge texobj.rs), its host's EeGeManOp

class FSFileSpec;
class FSPath;
class GEScreen;
class GESurface;
class GETexture;
class UString;

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
    GETextureManager(FSPath* inTexturePath);
    ~GETextureManager();
    GETextureManager(const GETextureManager&) = delete;   // port: one Rust object per manager
    GETextureManager& operator=(const GETextureManager&) = delete;
    void DumpAllLoadedMedia(FSFileSpec& inFile);
    unsigned long GetMemoryFootprint();     // the sum of the cached textures' GetAllocatedSize()
    GEScreen* GetScreen() const;
    unsigned long GetTextureCount();
    GETexture* GetTextureAt(unsigned long inIndex);   // port: the cache's textures in name order (the store gate)
    FSPath* GetTexturePath();
    GETexture* LoadTexture(const UString& inName, const GETextureDescription& inDescription);
    GETexture* MakeTexture(GESurface* inSurface);
    long RefreshAllTextures();
    long ReloadTexture(const UString& inName, const GETextureDescription& inDescription);
    long RemoveAllTextures();
    long RemoveTexture(GETexture* inTexture);
    void SetScreen(GEScreen* inScreen);
    void SetTexturePath(FSPath* inTexturePath);
    void UpdateMIPFilterMode(unsigned long inMode);
    // port: the optional texture pack (docs/DATA_FORMATS.md "Texture packs"). inDirectory (e.g. "textures-4x")
    // stands in for a texture's "textures" directory and holds files 1 << inScaleShift times the originals' size: a
    // description with mUseTexturePack loads the pack's file of the same name when the path has one (else the
    // original), dropping inMipLevelsToDrop more MIP levels (each halves the size; 2x box filtered). Its surfaces
    // keep the original's size as their logical size (GESurface::GetLogicalShift: inScaleShift - inMipLevelsToDrop)
    // and get MIP levels. NULL or "" turns the pack off. Every texture manager; textures already cached keep the file
    // they were loaded from until they are released.
    static void SetTexturePack(const char* inDirectory, long inScaleShift, long inMipLevelsToDrop);
    // the pack's name of a "textures\..." file (false with no pack, or for a picture read as data), and the logical
    // shift and extra MIP drop of its files: for pictures loaded around the texture manager (RTSMouse's pointers)
    static bool GetTexturePackName(const UString& inName, UString& outName);
    static long GetTexturePackLogicalShift();
    static long GetTexturePackMipLevelsToDrop();
private:
    friend class GETexture;
    long PromoteTexture(GETexture* inTexture);
    // the calls the Rust object makes on other objects (GETexture.cpp)
    static intptr_t Host(void* inOwner, EeGeManOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c);

    EeGeTextureManager* mRust;              // the cache (name -> texture), the lock, the path, the screen, the footprint
};

#endif
