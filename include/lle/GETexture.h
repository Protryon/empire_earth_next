// C:\EEX\Low-Level Engine\Graphics Engine\GETexture.h (RECONSTRUCTION). GETexture.cpp (~0x10032cfa) holds the
// ctors, dtor and the heavy members; the accessors are inline (GEBitmap.cpp emits them first, 0x1000e040..).
#ifndef LLE_GETEXTURE_H
#define LLE_GETEXTURE_H

class FSFileSpec;
class GERasterizer;
class GEScreen;
class GESurface;
class GETexture;
class GETextureManager;
struct GESurfaceDescription;
struct EeOfTexture;                        // a texture decoded by Rust (rust/ee_rust.h)

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

    void* GetData() { return this->mFrameTexture->mData; }
    bool GetCreateMIPMaps() const { return this->mCreateMIPMaps; }
    float GetFrameHeight() const { return this->GetFrameSurface()->GetHeight() * this->mFrameUVSize; }
    GESurface* GetFrameSurface() const { return this->mFrameTexture->mSurface; }
    GETexture* GetFrameTexture(unsigned long inFrame) const
        { return this->mFrameFormat == 0 ? this->mFrames[inFrame] : this->mFrames[0]; }
    const GETextureCoordinate& GetFrameTopLeftCorner() const { return this->mFrameTopLeftCorner; }
    float GetFrameWidth() const { return this->GetFrameSurface()->GetWidth() * this->mFrameUVSize; }
    unsigned long GetHeight() const { return this->mSurface->GetHeight(); }   // logical (GESurface::GetLogicalShift)
    bool GetIsCompressed() const { return this->mFrameTexture->mIsCompressed; }
    bool GetIsPromoted() const { return this->mFrameTexture->mIsPromoted; }
    unsigned long GetPixelFormat() const { return this->mFrameTexture->mSurface->GetPixelFormat(); }
    GETexture* GetStageTexture(unsigned long inStage) const { return this->mStage.mTexture; }
    unsigned long GetWidth() const { return this->mSurface->GetWidth(); }
    bool HasChanged() const { return this->mHasChanged; }
    long Promote()
    {
        if (this->mFrameTexture->mIsPromoted)
            return 0;
        return this->mManager->PromoteTexture(this->mFrameTexture);
    }
    void SetData(void* inData) { this->mFrameTexture->mData = inData; }
    void SetHasChanged() { this->mHasChanged = true; }
    void ValidateChanges() { this->mHasChanged = false; }

    unsigned long GetFrame() const { return this->mFrame; }
    unsigned long GetFrameCount() const { return this->mFrameCount; }
    float GetFrameUVSize() const { return this->mFrameUVSize; }
    unsigned long GetMipLevelCount() const { return this->mMipLevelCount; }
    unsigned long GetNumMipLevelsToDrop() const { return this->mNumMipLevelsToDrop; }
    GEScreen* GetScreen() const { return this->mScreen; }
    const GETStage* GetStage() const { return &this->mStage; }
    GESurface* GetSurface() const { return this->mSurface; }

    long Demote();
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
    void SetStageTexture(GETexture* inTexture, unsigned long inStage);
    long SetSurface(GESurface* inSurface);
    long Unlock();
protected:
    virtual ~GETexture();                   // ??1GETexture@@MAE@XZ (protected: released through Dereference)
private:
    friend class GETextureManager;
    friend struct GEStoreDigest;            // tools/eemodel/storegate: reads what the loaders stored
    GETexture(GETextureManager* inManager);                         // ??0GETexture@@AAE@PAVGETextureManager@@@Z
    GETexture(GESurface* inSurface, GETextureManager* inManager);   // private too (AAE)
    unsigned long GetFrameFormat() const { return this->mFrameFormat; }  // ?GetFrameFormat@GETexture@@ABEKXZ
    // port (D9): the frames of a texture's PNG files, decoded by Rust (GEFindTextureFile), in place of a texture file
    long Load(const EeOfTexture* inTexture, GESurfaceDescription& inDescription, GEScreen* inScreen,
              bool inCreateMIPMaps, long inNumMipLevelsToDrop, unsigned long inFrameFormat);
    void SetIsPromoted(bool inIsPromoted) { this->mIsPromoted = inIsPromoted; }
    // port: the logical shift of every frame's surface (GESurface::GetLogicalShift; a texture pack's texture)
    void SetLogicalShift(unsigned long inShift);

    bool mIsManaged;                        // +0x08 set by GETextureManager (Dereference then calls RemoveTexture)
    GETStage mStage;                        // +0x0c
    GETextureCoordinate mFrameTopLeftCorner;   // +0x2c
    GEScreen* mScreen;                      // +0x34
    GESurface* mSurface;                    // +0x38
    GETexture* mFrameTexture;               // +0x3c the current frame's texture (this when unframed)
    GETexture** mFrames;                    // +0x40
    GETextureManager* mManager;             // +0x44
    void* mData;                            // +0x48
    float mFrameUVSize;                     // +0x4c
    unsigned long mFrameFormat;             // +0x50 0: one texture per frame
    unsigned long mFrame;                   // +0x54
    unsigned long mFrameCount;              // +0x58
    unsigned long mFramesPerRow;            // +0x5c frame format 1: frames per row/column of the grid (SetFrame)
    long mMipLevelCount;                    // +0x60 signed (Load: jle/jge)
    unsigned long mNumMipLevelsToDrop;      // +0x64
    bool mIsCompressed;                     // +0x68
    bool mIsPromoted;                       // +0x69
    bool mHasChanged;                       // +0x6a
    bool mCreateMIPMaps;                    // +0x6b
    bool mIsFromTexturePack;                // port: loaded from the texture pack (GETextureManager::SetTexturePack)
    long mTexturePackMipLevels;             // port: the MIP levels the pack dropped on top of mNumMipLevelsToDrop

    static GERasterizer* sRasterizer;       // exported ?sRasterizer@GETexture@@0PAVGERasterizer@@A
};

#endif
