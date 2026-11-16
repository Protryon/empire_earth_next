// The stand-in world of won_platimg_test.cpp, included once in each of its two namespaces (Orig: PlatNativeImage.cpp's
// statements as they were, won_platimg_orig.h; Port: the port's statements, won_platimg_port.h): the images' classes
// with the game's members over the real surface (WONSurface.h), the texture pack's files, the decoder and the
// setting. Every file opened is appended to the namespace's trace.
using WONAPI::RefCount;
using WONAPI::SmartPtr;
using WONAPI::SurfaceBase;
using WONAPI::SurfaceFormat_32;
static std::string sTrace;
struct NativeImage;
struct Graphics { virtual ~Graphics() {} };
struct PlatGraphics : Graphics {};
struct DisplayContext : RefCount {
    DisplayContext(SurfaceBase* s = NULL) : mSurface(s), mGraphics(NULL), mPixelScale(1.0f) {}
    SurfaceBase* mSurface;
    Graphics* mGraphics;
    float mPixelScale;
};
struct NativeMask : RefCount {
    NativeMask(DisplayContext* c) : mDisplayContext(c) {}
    SmartPtr<DisplayContext> mDisplayContext;
};
struct PlatNativeMask : NativeMask {
    PlatNativeMask(DisplayContext* inContext, int inWidth, int inHeight, int inBytesPerRow, const unsigned char* inBits)
        : NativeMask(inContext), mWidth(inWidth), mHeight(inHeight), mBytesPerRow(inBytesPerRow)
    { this->mBits.assign(inBits, inBits + (size_t)inBytesPerRow * (inHeight > 0 ? inHeight : 0)); }
    bool IsTransparent(int x, int y) const
    { return (this->mBits[(size_t)y * this->mBytesPerRow + (x >> 3)] & (0x80 >> (x & 7))) != 0; }
    EeWonPlatMask Rust() const
    {
        EeWonPlatMask m = {this->mBits.data(), this->mWidth, this->mHeight, this->mBytesPerRow};
        return m;
    }
    int mWidth, mHeight, mBytesPerRow;
    std::vector<unsigned char> mBits;
};
struct Image : RefCount {
    unsigned short GetWidth() const { return this->mWidth; }
    unsigned short GetHeight() const { return this->mHeight; }
    unsigned short mWidth, mHeight;
};
struct RawImage32;
struct NativeImage : Image {
    NativeImage(DisplayContext*) : mGraphics(NULL) {}
    virtual ~NativeImage() {}
    Graphics* mGraphics;
    SmartPtr<DisplayContext> mDisplayContext;
    SmartPtr<NativeMask> mTransparentMask;
};
struct PlatNativeImage : NativeImage {
    PlatNativeImage(DisplayContext* inContext, int inWidth, int inHeight, float inScale = 1.0f);
    void Draw(DisplayContext* inContext, int x, int y, int left, int top, int width, int height);
    void DrawScaled(SurfaceBase* inDest, float inDestScale, int x, int y, int left, int top, int width, int height);
    SmartPtr<RawImage32> GetRaw();
    uint32_t* GetPixels();
    int GetPitchPixels();
    int GetPixelWidth() { return this->mSurface->GetWidth(); }
    int GetPixelHeight() { return this->mSurface->GetHeight(); }
    float GetScale() const { return this->mScale; }
    SmartPtr<SurfaceBase> mSurface;
    float mScale;
};
inline int PlatGUIPixels(int inLogical, float inScale)
{
    return inScale == 1.0f ? inLogical : (int)floorf((float)inLogical * inScale + 0.5f);
}
struct Palette : RefCount {
    Palette(int n) : mNumColors(n) { this->mColors = new DWORD[n > 0 ? n : 1](); }
    ~Palette() { delete[] this->mColors; }
    int GetNumColors() { return this->mNumColors; }
    DWORD* mColors;
    int mNumColors;
};
struct RawImage : Image {
    RawImage(int t) : mType(t) {}
    virtual SmartPtr<NativeImage> GetNative(DisplayContext*) = 0;
    int mType;
};
struct RawImage32 : RawImage {
    RawImage32(int width, int height) : RawImage(2)
    {
        this->mWidth = width; this->mHeight = height; this->mDoTransparency = false;
        this->mImageData = new DWORD[width * height > 0 ? width * height : 1]();
    }
    ~RawImage32() { delete[] this->mImageData; }
    SmartPtr<NativeImage> CreateNativeImage(DisplayContext* context);
    virtual SmartPtr<NativeImage> GetNative(DisplayContext* c) { return this->CreateNativeImage(c); }
    DWORD* mImageData;
    bool mDoTransparency;
};
struct RawImage8 : RawImage {
    RawImage8(int width, int height, Palette* p) : RawImage(1)
    {
        this->mWidth = width; this->mHeight = height; this->mPalette = p; this->mTransparentColor = -1;
        this->CalcBytesPerRow();
        this->mImageData = new unsigned char[height * this->mBytesPerRow > 0 ? height * this->mBytesPerRow : 1]();
    }
    ~RawImage8() { delete[] this->mImageData; }
    void CalcBytesPerRow();
    SmartPtr<NativeImage> CreateNativeImage(DisplayContext* context);
    virtual SmartPtr<NativeImage> GetNative(DisplayContext* c) { return this->CreateNativeImage(c); }
    unsigned char* mImageData;
    int mBytesPerRow;
    SmartPtr<Palette> mPalette;
    int mTransparentColor;
};
struct RawImage1 : RawImage {
    RawImage1(int width, int height) : RawImage(0)
    {
        this->mWidth = width; this->mHeight = height;
        this->mBytesPerRowMinusPad = width / 8 + (width % 8 != 0 ? 1 : 0);
        this->CalcBytesPerRow();
        this->mImageData = new unsigned char[height * this->mBytesPerRow > 0 ? height * this->mBytesPerRow : 1]();
    }
    ~RawImage1() { delete[] this->mImageData; }
    void CalcBytesPerRow();
    SmartPtr<NativeMask> GetMask(DisplayContext* context);
    virtual SmartPtr<NativeImage> GetNative(DisplayContext* context);
    unsigned char* mImageData;
    int mBytesPerRow, mBytesPerRowMinusPad;
};
// the texture pack's files: name -> the decoded image (NULL: the decoder throws)
struct PackFile { bool mThrows; SmartPtr<RawImage> mImage; };
static std::map<std::string, PackFile> sFiles;
static long sShift;
struct FileReaderException {};
struct FileReader {
    std::string mName;
    bool Open(const char* inName)
    {
        sTrace += std::string("open ") + inName + (sFiles.count(inName) ? " found\n" : " missing\n");
        if (!sFiles.count(inName)) return false;
        mName = inName;
        return true;
    }
};
struct ImageDecoder {
    std::string mFile;
    SmartPtr<RawImage> DecodeRaw(FileReader& r)
    {
        sTrace += "decode " + mFile + "\n";
        PackFile& f = sFiles[r.mName];
        if (f.mThrows) throw FileReaderException();
        return f.mImage;
    }
};
struct MultiImageDecoder : ImageDecoder {
    void SetFileName(const char* n) { this->mFile = n; }
};
struct WindowManager {
    MultiImageDecoder mImageDecoder;
    static WindowManager*& Default() { static WindowManager* w; return w; }
    static WindowManager* GetDefaultWindowManager() { return Default(); }
};
struct GETextureManager {
    static long GetTexturePackLogicalShift() { return sShift; }
};
