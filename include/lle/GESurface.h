// C:\EEX\Low-Level Engine\Graphics Engine\GESurface.h (RECONSTRUCTION). Abstract: the renderer plug-ins implement it
// (DX7HRDisplay DX7Surface); slot order and signatures from DX7Surface's vtable (0x104c4) and exports.
#ifndef LLE_GESURFACE_H
#define LLE_GESURFACE_H

class FSFileSpec;
class GEColor;
class GERectangle;
class GEScreen;
struct HDC__;

// How a surface is created (GEScreen::CreateSurface). Fields from GETextureManager::LoadTexture/RefreshAllTextures
// (0x33cd4, 0x341d9), which fill it from a GETextureDescription or from an existing surface's getters, and from
// GETexture::Load (0x32ec4: type, size, mip levels). Names are provisional; sizeof 0x24 (copied with rep movsd 9).
struct GESurfaceDescription {
    GESurfaceDescription() { this->mNoAlpha = false; }  // the only store before the fields are filled in

    unsigned long mType;                    // +0x00 GETexture::Load: 5 texture, 6 texture with a mip chain
    unsigned long mPixelFormat;             // +0x04 GetPixelFormat
    unsigned long mSurfaceVolatility;       // +0x08 GetSurfaceVolatility
    unsigned long mWidth;                   // +0x0c GETexture::Load (file header width)
    unsigned long mHeight;                  // +0x10
    unsigned long m14;                      // +0x14 GETexture::Load stores 0
    long mMipLevelCount;                    // +0x18 signed (GETexture::Load's mip loop: jl)
    unsigned long mPixelDepth;              // +0x1c GetPixelDepth
    bool mIs3DRenderable;                   // +0x20 GetIs3DRenderable
    bool mIsTransparencyEnabled;            // +0x21 GetIsTransparencyEnabled
    bool mNoAlpha;                          // +0x22 32-bit surfaces only: !GETextureDescription::mHasAlpha
};

class LLE_API GESurface : public UReferenceCounter {   // sizeof 8 (vector deleting dtor 0xdf58)
protected:
    GESurface() {}                          // 0xdf33
    virtual ~GESurface() {}                 // slot 0 (exported protected: ??1GESurface@@MAE@XZ)
public:
    virtual long AttachSurface(GESurface* inSurface) = 0;
    // overloaded virtuals take vtable slots in reverse declaration order (0x08 rectangle form, 0x0c whole surface)
    virtual long Clear(GEColor& inColor, float inZ) = 0;
    virtual long Clear(GERectangle& inRectangle, GEColor& inColor, float inZ) = 0;
    virtual long CopyTo(GESurface* inTarget, GERectangle& inSource, GERectangle& inDestination, bool inTransparent) = 0;
    virtual long CopyWithFilter(GESurface* inTarget, GERectangle& inSource, GERectangle& inDestination) = 0;
    virtual long DetachSurface(unsigned long inIndex) = 0;
    virtual long Flip(bool inWait) = 0;
    virtual unsigned long GetAllocatedSize() = 0;
    virtual GESurface* GetAttachedSurface(unsigned long inIndex) = 0;
    virtual unsigned long GetAttachedSurfaceCount() = 0;
    virtual unsigned long GetBytesPerRow() const = 0;
    virtual unsigned long GetHeight() const = 0;           // slot 0x30
    virtual bool GetIs3DRenderable() const = 0;
    virtual bool GetIsTextLocked() const = 0;
    virtual bool GetIsTransparencyEnabled() const = 0;
    virtual unsigned char* GetMemoryAddress() const = 0;
    virtual GEScreen* GetParentScreen() const = 0;
    virtual unsigned long GetPixelFormat() const = 0;      // slot 0x48
    virtual unsigned long GetPixelDepth() const = 0;
    virtual unsigned long GetSurfaceVolatility() const = 0;
    virtual const GESurfaceDescription& GetSurfaceDescription() const = 0;
    virtual const GEColor& GetTransparencyColor() const = 0;
    virtual unsigned long GetType() const = 0;
    virtual unsigned long GetWidth() const = 0;            // slot 0x64
    virtual long Lock() = 0;
    virtual long RestoreSurface() = 0;
    virtual void SetTransparencyColor(GEColor& inColor) = 0;
    virtual long TextLock() = 0;
    virtual long TextUnlock() = 0;
    virtual long Unlock() = 0;
    // port: the pixels the GPU copy of a scaled back buffer holds per logical pixel (the render scale: the GX
    // renderer draws the game's logical pixels into a larger texture); 1 for every other surface
    virtual float GetRenderScale() const { return 1.0f; }

    // Image files (GESurface.cpp); inType: GEImageFileType (the switches in Load/Save)
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
    void SetLogicalSize(unsigned long inWidth, unsigned long inHeight)
        { this->mLogicalWidth = inWidth; this->mLogicalHeight = inHeight; }
    unsigned long GetLogicalWidthSet() const { return this->mLogicalWidth; }
    unsigned long GetLogicalHeightSet() const { return this->mLogicalHeight; }
    unsigned long GetPixelWidth() const { return this->GetSurfaceDescription().mWidth; }
    unsigned long GetPixelHeight() const { return this->GetSurfaceDescription().mHeight; }

protected:  // the per-format members are exported protected (?LoadBMP@GESurface@@IAE.., ..@@KAJ..)
    long LoadBMP(FSFileSpec& inFile);
    long LoadJPG(FSFileSpec& inFile);
    long LoadJPG2(FSFileSpec& inFile);
    long LoadTGA(FSFileSpec& inFile);       // only the texture manager's .sst/.tga frames (GETexture.cpp)
    long LoadPNG(FSFileSpec& inFile);       // port: the pictures read outside the texture manager
    long SaveAsBMP(FSFileSpec& inFile);
    long SaveAsJPG(FSFileSpec& inFile);
    long SaveAsJPG2(FSFileSpec& inFile);
    static long GetBMPFileAttributes(FSFileSpec& inFile, unsigned long& outWidth, unsigned long& outHeight,
                                     unsigned long& outDepth);
    static long GetJPGFileAttributes(FSFileSpec& inFile, unsigned long& outWidth, unsigned long& outHeight,
                                     unsigned long& outDepth);
    static long GetJPG2FileAttributes(FSFileSpec& inFile, unsigned long& outWidth, unsigned long& outHeight,
                                      unsigned long& outDepth);
    static long GetPNGFileAttributes(FSFileSpec& inFile, unsigned long& outWidth, unsigned long& outHeight,
                                     unsigned long& outDepth);   // port (GetTGAFileAttributes: removed)

    unsigned long mLogicalShift = 0;        // port: see GetLogicalShift
    unsigned long mLogicalWidth = 0;        // port: see SetLogicalSize (0: none)
    unsigned long mLogicalHeight = 0;
};

// GESurface::Load/Save/GetImageFileAttributes inType: the per-format exported member each case calls
enum GEImageFileType {
    kImageFileTGA = 0,  // Load -> LoadTGA, only for the texture manager's frames; Save/attributes: removed (RUST_PLAN D9)
    kImageFileBMP = 1,  // GESurface switch case -> exported LoadBMP/SaveAsBMP/GetBMPFileAttributes
    kImageFileJPG = 2,  // GESurface switch case -> exported LoadJPG/SaveAsJPG/GetJPGFileAttributes
    kImageFileJPG2 = 3,  // GESurface switch case -> exported LoadJPG2/SaveAsJPG2/GetJPG2FileAttributes
    kImageFilePNG = 4,  // port: LoadPNG/GetPNGFileAttributes (no saver)
};

// GETGAHeader::mDescriptor (TGA header byte 17, the image descriptor): Truevision TGA spec bits 4 and 5 give the pixel order (GESurface::LoadTGA)
enum GETGADescriptorFlag {
    kTGADescriptorRightToLeft = 0x10,  // TGA spec: image descriptor bit 4 = pixels right to left (LoadTGA: theLeftToRight when clear)
    kTGADescriptorTopToBottom = 0x20,  // TGA spec: image descriptor bit 5 = rows top to bottom (LoadTGA: theTopDown)
};

#endif
