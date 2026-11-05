// C:\EEX\Low-Level Engine\Graphics Engine\GETextureManager.h (RECONSTRUCTION). GETextureManager.cpp (__FILE__
// anchor, ~0x10033bef) holds the real code; the getters and the implicit copy members are inline.
#ifndef LLE_GETEXTUREMANAGER_H
#define LLE_GETEXTUREMANAGER_H

#include <map>

class FSFileSpec;
class FSPath;
class GEScreen;
class GESurface;
class GETexture;
class TSCriticalSection;
class UString;

// A picture the caller loads as a Targa (GESurface::Load, kImageFileTGA): its .png in place of the .tga when the path
// has one (GEOpenFormatsFile.cpp), else FSPath::FindFile
long GEFindTargaFile(FSPath* inPath, const UString& inName, FSFileSpec*& outFile);

// How a texture file is loaded (sizeof 0x14, copied with `rep movsd` x5 by the User Interface backgrounds,
// UITextureBackground 0x1005c84e). Fields from GETextureManager::LoadTexture (0x33cd4), which turns it into a
// GESurfaceDescription; (provisional names)
struct GETextureDescription {
    unsigned long mSurfaceVolatility;       // +0x00 -> GESurfaceDescription::mSurfaceVolatility
    unsigned long mFrameFormat;             // +0x04 GETexture::Load's frame format
    unsigned long mPixelDepth;              // +0x08 16 or 32
    long mNumMipLevelsToDrop;               // +0x0c
    bool mIs3DRenderable;                   // +0x10
    bool mIsTransparencyEnabled;            // +0x11
    bool mHasAlpha;                         // +0x12
    bool mCreateMIPMaps;                    // +0x13
};

class LLE_API GETextureManager {            // sizeof 0x1c (implicit copy ctor 0xdfd3, operator= 0xe013)
public:
    GETextureManager(FSPath* inTexturePath);
    ~GETextureManager();
    void DumpAllLoadedMedia(FSFileSpec& inFile);
    unsigned long GetMemoryFootprint() { return this->mMemoryFootprint; }
    GEScreen* GetScreen() const { return this->mScreen; }
    unsigned long GetTextureCount() { return this->mTextures.size(); }
    FSPath* GetTexturePath() { return this->mTexturePath; }
    GETexture* LoadTexture(const UString& inName, const GETextureDescription& inDescription);
    GETexture* MakeTexture(GESurface* inSurface);
    long RefreshAllTextures();
    long ReloadTexture(const UString& inName, const GETextureDescription& inDescription);
    long RemoveAllTextures();
    long RemoveTexture(GETexture* inTexture);
    void SetScreen(GEScreen* inScreen) { this->mScreen = inScreen; }
    void SetTexturePath(FSPath* inTexturePath);
    void UpdateMIPFilterMode(unsigned long inMode);
private:
    friend class GETexture;
    long PromoteTexture(GETexture* inTexture);

    std::map<UString, GETexture*> mTextures;   // +0x00 (node 0x24: key +0xc, value +0x1c, color +0x20, nil +0x21)
    TSCriticalSection* mCriticalSection;    // +0x0c
    FSPath* mTexturePath;                   // +0x10
    GEScreen* mScreen;                      // +0x14
    unsigned long mMemoryFootprint;         // +0x18
};

#endif
