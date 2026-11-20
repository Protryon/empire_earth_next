// The image classes as they were before R6 WONLobby round 24 moved them into ee_won_gui's imgobj.rs, for
// won_imgobj_test.cpp: the statements of the deleted units, copied (Image.cpp's Image, ProgramImage, NativeMask,
// NativeImage and the 32-, 8- and 1-bit raw images; Palette.cpp and PaletteLifetime.cpp; DelayLoadImage.cpp;
// MSSkinImage.cpp and MSSkinImageClone.cpp; ImageFilters.cpp's crop, scale, tint and transparent filters;
// GuiHost.cpp's StretchImageFilter, StretchImage and StretchCenterImage with WONLobbyStretchImage.h's inline methods;
// PlatNativeImage.cpp's native images, masks, texture pack copies and raw conversions; ImageDecoder.cpp's
// multi-decoder with the PNG and JPEG decoders). Over the driver's world (Graphics, DisplayContext, the window
// manager's decode and scale, the texture pack's shift); the changes, each marked "world:", are:
//  - every pixel buffer starts zeroed (the original's `new T[n]` left them unset; imgobj.rs zeroes them);
//  - WONMsPaint / WONDrawRun are the world's RunOps over these images;
//  - DisplayContext::CreateImage, WindowManager::DecodeImage and the texture pack's setting are the world's;
//  - the multi-decoder's map is a C++ copy of what round 41 made Rust (imgdec.rs EeWonImgDec, deleted this round).
// Included inside namespace Orig.

struct NativeImage;
struct RawImage32;
static int sLive; // the images alive

struct Image : RefCount {
    Image() { sLive++; }
    void DrawEntire(DisplayContext* context, int x, int y) { this->Draw(context, x, y, 0, 0, this->mWidth, this->mHeight); }
    virtual ~Image() { sLive--; }
    virtual void Draw(DisplayContext*, int, int, int, int, int, int) {}
    virtual void SetSize(int, int) {}
    virtual SmartPtr<Image> Duplicate() { return NULL; }
    virtual SmartPtr<NativeImage> GetNative(DisplayContext*);
    unsigned short GetWidth() const { return this->mWidth; }
    unsigned short GetHeight() const { return this->mHeight; }
    unsigned short mWidth;
    unsigned short mHeight;
};

// world: the images' draws on the logged Graphics and their sizes in a plan's slots
static void DrawOf(void* i, DisplayContext* c, int x, int y) { ((Image*)i)->DrawEntire(c, x, y); }
static void SizeOf(void* i, int w, int h) { ((Image*)i)->SetSize(w, h); }
static void Run(Graphics& g, const EeWonDrawOp* ops, size_t n, Image* s0 = NULL, Image* s1 = NULL)
{
    void* slots[4] = {s0, s1, NULL, NULL};
    RunOps(g, ops, n, slots, SizeOf);
}

struct ProgramImage : Image {
    typedef void (*PaintFunc)(Graphics&, int, int);
    PaintFunc mPaintFunc;
    virtual void Paint(Graphics& g)
    {
        if (this->mPaintFunc != NULL) this->mPaintFunc(g, this->mWidth, this->mHeight);
    }
    ProgramImage(int width = 0, int height = 0, PaintFunc paint = NULL)
    {
        this->mWidth = (unsigned short)width;
        this->mHeight = (unsigned short)height;
        this->mPaintFunc = paint;
    }
    virtual SmartPtr<Image> Duplicate() { return new ProgramImage(this->mWidth, this->mHeight, this->mPaintFunc); }
    // Paints the image at (x, y) through the context's graphics, clipped to the source rectangle when it
    // is not the whole image, then restores the translation
    virtual void Draw(DisplayContext* context, int x, int y, int left, int top, int width, int height)
    {
        Graphics* g = context->mGraphics;
        if (g == NULL) return;
        int tx = g->mTransX, ty = g->mTransY;
        g->Translate(x - tx, y - ty);
        bool clipped = false;
        if (left != 0 || top != 0 || width != this->mWidth || height != this->mHeight) {
            clipped = true;
            g->PushClipRect(left, top, width, height);
        }
        this->Paint(*g);
        if (clipped) g->PopClipRect();
        g->Translate(tx - x, ty - y);
    }
    virtual void SetSize(int w, int h) { this->mWidth = (unsigned short)w; this->mHeight = (unsigned short)h; }
};

struct NativeMask : RefCount {
    NativeMask(DisplayContext* context) : mDisplayContext(context) {}
    virtual ~NativeMask() {}
    SmartPtr<DisplayContext> mDisplayContext;
};
struct PlatNativeMask : NativeMask {
    PlatNativeMask(DisplayContext* inContext, int inWidth, int inHeight, int inBytesPerRow, const unsigned char* inBits)
        : NativeMask(inContext), mWidth(inWidth), mHeight(inHeight), mBytesPerRow(inBytesPerRow)
    {
        this->mBits.assign(inBits, inBits + (size_t)inBytesPerRow * (inHeight > 0 ? inHeight : 0));
    }
    EeWonPlatMask Rust() const
    {
        EeWonPlatMask m = {this->mBits.data(), this->mWidth, this->mHeight, this->mBytesPerRow};
        return m;
    }
    int mWidth, mHeight, mBytesPerRow;
    std::vector<unsigned char> mBits;
};

struct NativeImage : Image {
    NativeImage(DisplayContext* context)
    {
        this->mGraphics = NULL;
        this->mDisplayContext = context;
    }
    virtual ~NativeImage()
    {
        delete this->mGraphics;
        this->mGraphics = NULL;
    }
    Graphics& GetGraphics()
    {
        if (this->mGraphics == NULL) {
            this->mGraphics = this->PlatformGetGraphics();
            this->mGraphics->Attach(this->mDisplayContext.get());
        }
        return *this->mGraphics;
    }
    void SetTransparentMask(NativeMask* mask) { this->mTransparentMask = mask; }
    virtual SmartPtr<NativeImage> GetNative(DisplayContext*) { return this; }
    virtual Graphics* PlatformGetGraphics() = 0;
    virtual SmartPtr<RawImage32> GetRaw() { return NULL; }
    Graphics* mGraphics;
    SmartPtr<DisplayContext> mDisplayContext;
    SmartPtr<NativeMask> mTransparentMask;
};

struct Palette : RefCount {
    Palette(int n) : mNumColors(n) { this->mColors = new DWORD[n](); } // world: zeroed
    SmartPtr<Palette> Duplicate()
    {
        Palette* aPalette = new Palette(this->mNumColors);
        memcpy(aPalette->mColors, this->mColors, this->mNumColors * sizeof(DWORD));
        return aPalette;
    }
    virtual ~Palette() { delete[] this->mColors; }
    DWORD* mColors;
    int mNumColors;
};

struct RawImage1;
struct RawImage : Image {
    RawImage(int type) : mType(type) {}
    virtual void Draw(DisplayContext* context, int x, int y, int left, int top, int width, int height)
    {
        SmartPtr<NativeImage> native = this->GetNative(context);
        native->Draw(context, x, y, left, top, width, height);
    }
    virtual SmartPtr<NativeImage> GetNative(DisplayContext*) = 0;
    virtual bool HasTransparency() { return false; }
    int mType;
};
struct RawImage32 : RawImage {
    RawImage32(int width, int height) : RawImage(2)
    {
        this->mWidth = width;
        this->mHeight = height;
        this->mDoTransparency = false;
        this->mImageData = new DWORD[width * height](); // world: zeroed
        this->mImageEnd = this->mImageData + width * height;
    }
    virtual ~RawImage32() { delete[] this->mImageData; }
    virtual SmartPtr<Image> Duplicate()
    {
        SmartPtr<RawImage32> image = new RawImage32(this->mWidth, this->mHeight);
        image->mDoTransparency = this->mDoTransparency;
        memcpy(image->mImageData, this->mImageData, this->mWidth * this->mHeight * 4);
        return image.get();
    }
    virtual SmartPtr<NativeImage> GetNative(DisplayContext*);
    virtual bool HasTransparency() { return this->mDoTransparency; }
    SmartPtr<NativeImage> CreateNativeImage(DisplayContext*);
    DWORD GetPixel(int x, int y) { return this->mImageData[y * this->mWidth + x]; }
    DWORD* mImageData;
    DWORD* mImageEnd;
    bool mDoTransparency;
};
struct RawImage8 : RawImage {
    RawImage8(int width, int height, Palette* palette = NULL) : RawImage(1)
    {
        this->mWidth = width;
        this->mHeight = height;
        this->mPalette = palette;
        this->mTransparentColor = -1;
        this->CalcBytesPerRow();
        this->mRowPad = this->mBytesPerRow - this->mWidth;
        this->mImageData = new unsigned char[height * this->mBytesPerRow](); // world: zeroed
        this->mImageEnd = this->mImageData + this->mBytesPerRow * height;
    }
    virtual ~RawImage8() { delete[] this->mImageData; }
    virtual SmartPtr<Image> Duplicate()
    {
        SmartPtr<RawImage8> image = new RawImage8(this->mWidth, this->mHeight, this->mPalette.get());
        image->mPalette = this->mPalette;
        image->mTransparentColor = this->mTransparentColor;
        memcpy(image->mImageData, this->mImageData, this->mBytesPerRow * this->mHeight);
        return image.get();
    }
    virtual SmartPtr<NativeImage> GetNative(DisplayContext*);
    virtual bool HasTransparency() { return this->mTransparentColor >= 0; }
    void CalcBytesPerRow()
    {
        this->mBytesPerRow = this->mWidth;
        if (this->mBytesPerRow % 4 != 0)
            this->mBytesPerRow += 4 - this->mBytesPerRow % 4;
    }
    SmartPtr<NativeImage> CreateNativeImage(DisplayContext*);
    unsigned char GetPixel(int x, int y) { return this->mImageData[y * this->mBytesPerRow + x]; }
    unsigned char* mImageData;
    unsigned char* mImageEnd;
    int mRowPad;
    int mBytesPerRow;
    SmartPtr<Palette> mPalette;
    int mTransparentColor;
};
struct RawImage1 : RawImage {
    RawImage1(int width, int height) : RawImage(0)
    {
        this->mWidth = width;
        this->mHeight = height;
        this->mBytesPerRowMinusPad = this->mWidth / 8;
        this->mCompleteBytesPerRow = this->mBytesPerRowMinusPad;
        this->mWidthMod8 = this->mWidth % 8;
        if (this->mWidthMod8 != 0) ++this->mBytesPerRowMinusPad;
        this->CalcBytesPerRow();
        this->mRowPad = this->mBytesPerRow - this->mBytesPerRowMinusPad;
        if (this->mWidthMod8 != 0) ++this->mRowPad;
        this->mEndRowMask = 1 << (8 - this->mWidthMod8);
        this->mImageData = new unsigned char[this->mBytesPerRow * this->mHeight](); // world: zeroed
        this->mImageEnd = this->mImageData + this->mBytesPerRow * this->mHeight;
    }
    virtual ~RawImage1() { delete[] this->mImageData; }
    virtual SmartPtr<NativeImage> GetNative(DisplayContext*);
    void CalcBytesPerRow()
    {
        this->mBytesPerRow = this->mBytesPerRowMinusPad;
        if (this->mBytesPerRow % 4 != 0)
            this->mBytesPerRow += 4 - this->mBytesPerRow % 4;
    }
    SmartPtr<NativeMask> GetMask(DisplayContext* context)
    {
        return new PlatNativeMask(context, this->mWidth, this->mHeight, this->mBytesPerRow, this->mImageData);
    }
    unsigned char* mImageData;
    unsigned char* mImageEnd;
    int mRowPad, mEndRowMask, mBytesPerRow, mBytesPerRowMinusPad, mCompleteBytesPerRow, mWidthMod8;
};

// ---- PlatNativeImage.cpp

static SmartPtr<SurfaceBase> NewSurface(int inWidth, int inHeight)
{
    SmartPtr<SurfaceBase> theSurface = SurfaceBase::GetNewSurface(SurfaceFormat_32);
    if (inWidth < 0)
        inWidth = 0;
    if (inHeight < 0)
        inHeight = 0;
    theSurface->Create(inWidth, inHeight);
    if (theSurface->GetPixelsBase() != NULL)
        memset(theSurface->GetPixelsBase(), 0, (size_t)theSurface->GetBytesPerRow() * inHeight);
    return theSurface;
}

struct PlatNativeImage : NativeImage {
    PlatNativeImage(DisplayContext* inContext, int inWidth, int inHeight, float inScale = 1.0f)
        : NativeImage(inContext), mScale(inScale >= 1.0f ? inScale : 1.0f)
    {
        this->mSurface = NewSurface(PlatGUIPixels(inWidth, this->mScale), PlatGUIPixels(inHeight, this->mScale));
        this->mWidth = (unsigned short)inWidth;
        this->mHeight = (unsigned short)inHeight;
        this->mDisplayContext = new DisplayContext(this->mSurface.get());
        this->mDisplayContext->mPixelScale = this->mScale;
    }
    virtual ~PlatNativeImage()
    {
        delete this->mGraphics;
        this->mGraphics = NULL;
    }
    virtual Graphics* PlatformGetGraphics() { return new Graphics(DrawOf); } // world: the logged Graphics
    virtual void Draw(DisplayContext* inContext, int x, int y, int left, int top, int width, int height)
    {
        if (inContext == NULL || inContext->mSurface == NULL)
            return;
        PlatNativeMask* theMask = static_cast<PlatNativeMask*>(this->mTransparentMask.get());
        EeWonPlatMask theBits = theMask != NULL ? theMask->Rust() : EeWonPlatMask();
        ee_won_plat_img_draw(inContext->mSurface->GetRust(), inContext->mPixelScale, this->mSurface->GetRust(), this->mScale,
                             (int)this->GetWidth(), (int)this->GetHeight(), theMask != NULL ? &theBits : NULL, x, y, left, top,
                             width, height);
    }
    virtual SmartPtr<RawImage32> GetRaw()
    {
        SmartPtr<RawImage32> theImage = new RawImage32(this->GetWidth(), this->GetHeight());
        PlatNativeMask* theMask = static_cast<PlatNativeMask*>(this->mTransparentMask.get());
        EeWonPlatMask theBits = theMask != NULL ? theMask->Rust() : EeWonPlatMask();
        if (theMask != NULL)
            theImage->mDoTransparency = true;
        ee_won_plat_img_raw(this->mSurface->GetRust(), this->mScale, this->GetWidth(), this->GetHeight(),
                            theMask != NULL ? &theBits : NULL, (uint32_t*)theImage->mImageData);
        return theImage;
    }
    virtual SmartPtr<Image> Duplicate()
    {
        SmartPtr<PlatNativeImage> theCopy =
            new PlatNativeImage(this->mDisplayContext.get(), this->mWidth, this->mHeight, this->mScale);
        memcpy(theCopy->mSurface->GetPixelsBase(), this->mSurface->GetPixelsBase(),
               (size_t)this->mSurface->GetBytesPerRow() * this->mSurface->GetHeight());
        theCopy->mTransparentMask = this->mTransparentMask;
        return theCopy.get();
    }
    int GetPixelWidth() { return this->mSurface->GetWidth(); }
    SmartPtr<SurfaceBase> mSurface;
    float mScale;
};

// world: DisplayContext::CreateImage (the window manager's pixel scale)
static SmartPtr<NativeImage> CreateImage(DisplayContext* context, int width, int height)
{
    return new PlatNativeImage(context, width, height, sWmScale);
}

SmartPtr<NativeImage> Image::GetNative(DisplayContext* context)
{
    SmartPtr<NativeImage> image = CreateImage(context, this->mWidth, this->mHeight);
    image->GetGraphics().DrawImage(this, 0, 0);
    return image;
}

static SmartPtr<NativeImage> NativeOfRaw(DisplayContext* inContext, int inKind, const void* inData, int inPitch, int inWidth,
                                         int inHeight, Palette* inPalette, int inTransparent)
{
    SmartPtr<PlatNativeImage> theImage = new PlatNativeImage(inContext, inWidth, inHeight);
    EeWonRawPixels thePixels = {inKind, (uint8_t*)inData, inWidth, inHeight, inPitch};
    ee_won_plat_img_from_raw(&thePixels, inPalette != NULL ? (const uint32_t*)inPalette->mColors : NULL,
                             inPalette != NULL ? inPalette->mNumColors : 0, inTransparent, theImage->mSurface->GetRust());
    return theImage.get();
}
SmartPtr<NativeImage> RawImage32::CreateNativeImage(DisplayContext* context)
{
    return NativeOfRaw(context, 2, this->mImageData, this->mWidth * 4, this->GetWidth(), this->GetHeight(), NULL, -1);
}
SmartPtr<NativeImage> RawImage8::CreateNativeImage(DisplayContext* context)
{
    return NativeOfRaw(context, 1, this->mImageData, this->mBytesPerRow, this->mWidth, this->mHeight, this->mPalette.get(),
                       this->mTransparentColor);
}
SmartPtr<NativeImage> RawImage1::GetNative(DisplayContext* context)
{
    return NativeOfRaw(context, 0, this->mImageData, this->mBytesPerRow, this->mWidth, this->mHeight, NULL, -1);
}
SmartPtr<NativeImage> RawImage32::GetNative(DisplayContext* context)
{
    if (this->mDoTransparency) {
        int width = this->GetWidth(), height = this->GetHeight();
        SmartPtr<RawImage1> mask = new RawImage1(width, height);
        ee_won_imgf_mask32((uint32_t*)this->mImageData, width, height, mask->mImageData, mask->mBytesPerRow);
        SmartPtr<NativeImage> native = this->CreateNativeImage(context);
        native->SetTransparentMask(mask->GetMask(context).get());
        return native;
    }
    return this->CreateNativeImage(context);
}
SmartPtr<NativeImage> RawImage8::GetNative(DisplayContext* context)
{
    SmartPtr<NativeImage> native = this->CreateNativeImage(context);
    if (this->mTransparentColor != -1) {
        SmartPtr<RawImage1> mask = new RawImage1(this->mWidth, this->mHeight);
        ee_won_imgf_mask8(this->mImageData, this->mWidth, this->mHeight, this->mBytesPerRow, this->mTransparentColor, mask->mImageData,
                          mask->mBytesPerRow);
        native->SetTransparentMask(mask->GetMask(context).get());
    }
    return native;
}

// ---- DelayLoadImage.cpp

struct DelayLoadImage : Image {
    std::string mImagePath;
    bool mHaveLoaded;
    SmartPtr<NativeImage> mImage;
    DelayLoadImage(const std::string& path, int width, int height) : mImagePath(path)
    {
        this->mHaveLoaded = false;
        this->mWidth = (unsigned short)width;
        this->mHeight = (unsigned short)height;
    }
    virtual void Draw(DisplayContext* context, int x, int y, int left, int top, int width, int height)
    {
        if (!this->mHaveLoaded) {
            this->mHaveLoaded = true;
            this->mImage = WmDecodeImage(this->mImagePath.c_str()); // world: the window manager's decode
        }
        if (this->mImage.get() != NULL) this->mImage->Draw(context, x, y, left, top, width, height);
    }
    virtual SmartPtr<NativeImage> GetNative(DisplayContext*)
    {
        if (!this->mHaveLoaded) {
            this->mHaveLoaded = true;
            this->mImage = WmDecodeImage(this->mImagePath.c_str());
        }
        return this->mImage;
    }
};

// ---- MSSkinImage.cpp, MSSkinImageClone.cpp

struct MSImage : ProgramImage {
    unsigned char mImageType, mImageFlags;
    MSImage(unsigned char t, unsigned char f = 0, int w = 0, int h = 0) : ProgramImage(w, h), mImageType(t), mImageFlags(f) {}
    virtual SmartPtr<Image> Duplicate() { return new MSImage(this->mImageType, this->mImageFlags, this->mWidth, this->mHeight); }
    virtual void Paint(Graphics& g)
    {
        // world: WONMsPaint(g, WONMsPaintOf(SkinImage, ...)) is the plan run on the Graphics
        EeWonMsPaint p;
        p.kind = EeWonMsPaintKind::SkinImage; p.flags = 0; p.width = this->mWidth; p.height = this->mHeight;
        p.a = this->mImageType; p.b = this->mImageFlags; p.c = p.d = p.e = 0;
        std::vector<EeWonDrawOp> o(ee_won_mspaint_plan(&p, NULL, 0));
        ee_won_mspaint_plan(&p, o.data(), o.size());
        Run(g, o.data(), o.size());
    }
};

// ---- ImageFilters.cpp

struct ImageFilter : RefCount {
    virtual SmartPtr<RawImage> Filter(RawImage* theImage) { return theImage; }
};
static EeWonRawPixels Px(RawImage* image)
{
    EeWonRawPixels p;
    p.kind = image->mType; p.width = image->mWidth; p.height = image->mHeight;
    if (image->mType == 0) { p.data = static_cast<RawImage1*>(image)->mImageData; p.pitch = static_cast<RawImage1*>(image)->mBytesPerRow; }
    else if (image->mType == 1) { p.data = static_cast<RawImage8*>(image)->mImageData; p.pitch = static_cast<RawImage8*>(image)->mBytesPerRow; }
    else { p.data = (uint8_t*)static_cast<RawImage32*>(image)->mImageData; p.pitch = image->mWidth * 4; }
    return p;
}
static inline void CopyInfo(RawImage1*, RawImage1*) {}
static inline void CopyInfo(RawImage8* dst, RawImage8* src) { dst->mPalette = src->mPalette; dst->mTransparentColor = src->mTransparentColor; }
static inline void CopyInfo(RawImage32* dst, RawImage32* src) { dst->mDoTransparency = src->mDoTransparency; }
template <class ImageType, class PixelType>
static SmartPtr<ImageType> CropPixels(ImageType* src, int x, int y, int width, int height)
{
    SmartPtr<ImageType> dst = new ImageType(width, height);
    CopyInfo(dst.get(), src);
    EeWonRawPixels s = Px(src), d = Px(dst.get());
    ee_won_imgf_crop(&s, &d, x * (int)sizeof(PixelType), y, width * (int)sizeof(PixelType), height);
    return dst;
}
struct CropImageFilter : ImageFilter {
    int mx, my, mWidth, mHeight;
    CropImageFilter(int x = 0, int y = 0, int w = 0, int h = 0) : mx(x), my(y), mWidth(w), mHeight(h) {}
    virtual SmartPtr<RawImage> Filter(RawImage* image)
    {
        switch (image->mType) {
            case 1: return CropPixels<RawImage8, unsigned char>(static_cast<RawImage8*>(image), this->mx, this->my, this->mWidth, this->mHeight).get();
            case 2: return CropPixels<RawImage32, DWORD>(static_cast<RawImage32*>(image), this->mx, this->my, this->mWidth, this->mHeight).get();
        }
        return NULL;
    }
};
template <class ImageType>
static SmartPtr<ImageType> ScalePixels(ImageType* src, int width, int height)
{
    SmartPtr<ImageType> dst = new ImageType(width, height);
    CopyInfo(dst.get(), src);
    EeWonRawPixels s = Px(src), d = Px(dst.get());
    ee_won_imgf_scale(&s, &d);
    return dst;
}
struct ScaleImageFilter : ImageFilter {
    int mWidth, mHeight;
    int mPercentWidth, mPercentHeight;
    bool mIsPercent;
    ScaleImageFilter(int percent = 100) { this->SetPercentSize(percent, percent); }
    ScaleImageFilter(int w, int h) { this->SetAbsSize(w, h); }
    void SetPercentSize(int thePercentWidth, int thePercentHeight)
    {
        this->mPercentWidth = thePercentWidth;
        this->mPercentHeight = thePercentHeight;
        this->mIsPercent = true;
    }
    void SetAbsSize(int theWidth, int theHeight)
    {
        this->mWidth = theWidth;
        this->mHeight = theHeight;
        this->mIsPercent = false;
    }
    bool CheckSize(Image* image)
    {
        return ee_won_imgf_check_size(this->mIsPercent, this->mPercentWidth, this->mPercentHeight, &this->mWidth, &this->mHeight,
                                      image->mWidth, image->mHeight);
    }
    virtual SmartPtr<RawImage> Filter(RawImage* image)
    {
        if (this->CheckSize(image)) return image;
        switch (image->mType) {
            case 0: return ScalePixels<RawImage1>(static_cast<RawImage1*>(image), this->mWidth, this->mHeight).get();
            case 1: return ScalePixels<RawImage8>(static_cast<RawImage8*>(image), this->mWidth, this->mHeight).get();
            case 2: return ScalePixels<RawImage32>(static_cast<RawImage32*>(image), this->mWidth, this->mHeight).get();
            default: return image;
        }
    }
};
struct TintImageFilter : ImageFilter {
    int mRed, mGreen, mBlue;
    TintImageFilter(int r, int g, int b) : mRed(r), mGreen(g), mBlue(b) {}
    virtual SmartPtr<RawImage> Filter(RawImage* image)
    {
        if (image->mType == 1) {
            RawImage8* raw = static_cast<RawImage8*>(image);
            SmartPtr<Palette> palette = raw->mPalette->Duplicate();
            ee_won_imgf_tint((uint32_t*)palette->mColors, palette->mNumColors > 0 ? (size_t)palette->mNumColors : 0, this->mRed, this->mGreen, this->mBlue, false);
            raw->mPalette = palette;
        } else if (image->mType == 2)
            ee_won_imgf_tint((uint32_t*)static_cast<RawImage32*>(image)->mImageData, (size_t)image->mWidth * image->mHeight, this->mRed, this->mGreen, this->mBlue, true);
        return image;
    }
};
struct TransparentImageFilter : ImageFilter {
    DWORD mTransparentColor;
    TransparentImageFilter(DWORD color) { this->mTransparentColor = color; }
    // (the sampled colour's constructor and branch: no caller)
    virtual SmartPtr<RawImage> Filter(RawImage* image)
    {
        if (image->mType == 0) return image;
        DWORD color = this->mTransparentColor;
        if (image->mType == 1) {
            RawImage8* raw = static_cast<RawImage8*>(image);
            Palette* palette = raw->mPalette.get();
            int i = ee_won_imgf_palette_find((const uint32_t*)palette->mColors, palette->mNumColors > 0 ? (size_t)palette->mNumColors : 0, color);
            if (i >= 0) raw->mTransparentColor = i;
        } else if (image->mType == 2) {
            static_cast<RawImage32*>(image)->mDoTransparency = true;
            ee_won_imgf_transparent32((uint32_t*)static_cast<RawImage32*>(image)->mImageData, (size_t)image->mWidth * image->mHeight, color);
        }
        return image;
    }
};

// ---- GuiHost.cpp's stretch images and WONLobbyStretchImage.h

struct StretchImageFilter : ImageFilter {
    int mWidth, mHeight;
    int mLeftWidth, mRightWidth, mTopHeight, mBottomHeight;
    bool mTileHorz, mTileVert, mSkipCenter;
    StretchImageFilter() : mWidth(0), mHeight(0), mLeftWidth(0), mRightWidth(0), mTopHeight(0), mBottomHeight(0),
        mTileHorz(true), mTileVert(true), mSkipCenter(false) {}
    virtual SmartPtr<RawImage> Filter(RawImage*);
    void SetSize(int w, int h) { this->mWidth = w; this->mHeight = h; }
    void SetBorders(int l, int t, int r, int b) { this->mLeftWidth = l; this->mRightWidth = r; this->mTopHeight = t; this->mBottomHeight = b; }
    void SetTile(bool h, bool v) { this->mTileHorz = h; this->mTileVert = v; }
    void SetSkipCenter(bool b) { this->mSkipCenter = b; }
};
static EeWonRawPixels StretchPx(RawImage8* i) { EeWonRawPixels p = {1, i->mImageData, i->mWidth, i->mHeight, i->mBytesPerRow}; return p; }
static EeWonRawPixels StretchPx(RawImage32* i) { EeWonRawPixels p = {2, (uint8_t*)i->mImageData, i->mWidth, i->mHeight, i->mWidth * 4}; return p; }
static void CopyRawImageInfo(RawImage8* dst, RawImage8* src) { dst->mPalette = src->mPalette.get(); dst->mTransparentColor = src->mTransparentColor; }
static void CopyRawImageInfo(RawImage32* dst, RawImage32* src) { dst->mDoTransparency = src->mDoTransparency; }
template <class ImageType> static SmartPtr<ImageType> DoStretchFilter(ImageType* src, StretchImageFilter* f)
{
    EeWonStretchFilter aFilter = {f->mWidth, f->mHeight, f->mLeftWidth, f->mRightWidth, f->mTopHeight, f->mBottomHeight,
                                  src->GetWidth(), src->GetHeight(), f->mTileHorz, f->mTileVert, f->mSkipCenter};
    int aWidth = 0, aHeight = 0;
    std::vector<EeWonDrawOp> anOps(ee_won_gui_draw_stretch_filter(&aFilter, &aWidth, &aHeight, NULL, 0));
    ee_won_gui_draw_stretch_filter(&aFilter, &aWidth, &aHeight, anOps.data(), anOps.size());
    SmartPtr<ImageType> newImage = new ImageType(aWidth, aHeight);
    CopyRawImageInfo(newImage.get(), src);
    EeWonRawPixels aSrc = StretchPx(src), aDst = StretchPx(newImage.get());
    ee_won_imgf_stretch_run(&aSrc, &aDst, anOps.data(), anOps.size());
    return newImage;
}
SmartPtr<RawImage> StretchImageFilter::Filter(RawImage* theImage)
{
    switch (theImage->mType) {
        case 1: return DoStretchFilter(static_cast<RawImage8*>(theImage), this).get();
        case 2: return DoStretchFilter(static_cast<RawImage32*>(theImage), this).get();
        default: return NULL;
    }
}

struct StretchImage : ProgramImage {
    SmartPtr<RawImage> mImage;
    SmartPtr<NativeImage> mCachedImage;
    EeWonStretchImg* mObj;
    StretchImage(RawImage* i, EeWonStretchImg* o) : mImage(i), mObj(o) { this->mWidth = this->mHeight = 0; }
    int Get(EeWonStretchField f) { return ee_won_stretch_get(this->mObj, f); }
    virtual ~StretchImage() { ee_won_stretch_free(this->mObj); }
    StretchImage(RawImage* i, int l, int t, int r, int b) : mImage(i), mObj(ee_won_stretch_new(l, t, r, b)) { this->mWidth = this->mHeight = 0; }
    int GetLeft() { return this->Get(EeWonStretchField::Left); }
    int GetRight() { return this->Get(EeWonStretchField::Right); }
    int GetTop() { return this->Get(EeWonStretchField::Top); }
    int GetBottom() { return this->Get(EeWonStretchField::Bottom); }
    void CalcImage(DisplayContext* context)
    {
        SmartPtr<StretchImageFilter> aFilter = new StretchImageFilter;
        aFilter->SetBorders(this->GetLeft(), this->GetTop(), this->GetRight(), this->GetBottom());
        aFilter->SetSize(this->GetWidth(), this->GetHeight());
        aFilter->SetTile(this->Get(EeWonStretchField::TileHorz) != 0, this->Get(EeWonStretchField::TileVert) != 0);
        aFilter->SetSkipCenter(this->Get(EeWonStretchField::SkipCenter) != 0);
        this->mCachedImage = aFilter->Filter(this->mImage.get())->GetNative(context);
        ee_won_stretch_built(this->mObj);
    }
    virtual void Paint(Graphics& g)
    {
        if (this->Get(EeWonStretchField::NeedRecalc))
            this->CalcImage(g.mDisplayContext);
        std::vector<EeWonDrawOp> o(ee_won_stretch_paint(this->mObj, this->mWidth, this->mHeight, NULL, 0));
        ee_won_stretch_paint(this->mObj, this->mWidth, this->mHeight, o.data(), o.size());
        Run(g, o.data(), o.size(), this->mCachedImage.get());
    }
    virtual SmartPtr<Image> Duplicate() { return new StretchImage(this->mImage.get(), ee_won_stretch_duplicate(this->mObj)); }
    virtual SmartPtr<NativeImage> GetNative(DisplayContext* c) { if (this->Get(EeWonStretchField::NeedRecalc)) this->CalcImage(c); return this->mCachedImage; }
    virtual void SetSize(int w, int h) { if (ee_won_stretch_set_size(this->mObj, this->mWidth, this->mHeight, w, h)) { this->mWidth = w; this->mHeight = h; } }
    void SetImage(RawImage* i) { this->mImage = i; }
    void SetBorders(int l, int t, int r, int b) { ee_won_stretch_set_borders(this->mObj, l, t, r, b); }
    void CopyAttributes(StretchImage* f) { ee_won_stretch_copy(this->mObj, f->mObj); this->mWidth = f->mWidth; this->mHeight = f->mHeight; }
    void SetTile(bool h, bool v) { ee_won_stretch_set_tile(this->mObj, h, v); }
    void SetSkipCenter(bool b) { ee_won_stretch_set_skip(this->mObj, b); }
};
struct StretchCenterImage : ProgramImage {
    SmartPtr<Image> mStretchImage, mCenterImage;
    StretchCenterImage(Image* stretch = NULL, Image* center = NULL) : mStretchImage(stretch), mCenterImage(center) {}
    void SetStretchImage(Image* image) { this->mStretchImage = image; }
    void SetCenterImage(Image* image) { this->mCenterImage = image; }
    virtual void Paint(Graphics& g)
    {
        std::vector<EeWonDrawOp> o(ee_won_gui_draw_stretch_center(this->mWidth, this->mHeight, this->mCenterImage->mWidth,
                                                                  this->mCenterImage->mHeight, NULL, 0));
        ee_won_gui_draw_stretch_center(this->mWidth, this->mHeight, this->mCenterImage->mWidth, this->mCenterImage->mHeight,
                                       o.data(), o.size());
        Run(g, o.data(), o.size(), this->mStretchImage.get(), this->mCenterImage.get());
    }
    virtual SmartPtr<Image> Duplicate() { return new StretchCenterImage(this->mStretchImage->Duplicate().get(), this->mCenterImage.get()); }
    virtual void SetSize(int w, int h) { this->mWidth = w; this->mHeight = h; this->mStretchImage->SetSize(w, h); }
};

// ---- ImageDecoder.cpp and PlatNativeImage.cpp's texture pack copies

struct ImageAttributes {
    int mWidth, mHeight;
    ImageAttributes() { this->mWidth = this->mHeight = 0; }
};
struct ImageDecoder {
    FileReader* mFileReader;
    ImageDecoder() { this->mFileReader = NULL; }
    virtual ~ImageDecoder() {}
    virtual SmartPtr<NativeImage> DoDecode(DisplayContext* context)
    {
        SmartPtr<RawImage> image = this->DoDecodeRaw();
        if (image.get() != NULL) return image->GetNative(context);
        return NULL;
    }
    virtual SmartPtr<RawImage> DoDecodeRaw() = 0;
    virtual bool DoGetAttrib(ImageAttributes&) = 0;
    SmartPtr<NativeImage> Decode(DisplayContext* context, const char* path);
    SmartPtr<NativeImage> Decode(DisplayContext* context, FileReader& reader)
    {
        if (!reader.IsOpen()) return NULL;
        this->mFileReader = &reader;
        return this->DoDecode(context);
    }
    SmartPtr<RawImage> DecodeRaw(const char* path)
    {
        FileReader reader(path);
        return this->DecodeRaw(reader);
    }
    SmartPtr<RawImage> DecodeRaw(FileReader& reader)
    {
        if (!reader.IsOpen()) return NULL;
        this->mFileReader = &reader;
        return this->DoDecodeRaw();
    }
    bool GetImageAttributes(const char* path, ImageAttributes& attributes)
    {
        FileReader reader(path);
        return this->GetImageAttributes(reader, attributes);
    }
    bool GetImageAttributes(FileReader& reader, ImageAttributes& attributes)
    {
        if (!reader.IsOpen()) return false;
        this->mFileReader = &reader;
        try { return this->DoGetAttrib(attributes); }
        catch (FileReaderException&) {}
        return false;
    }
    bool RustAttrib(int inKind, ImageAttributes& outAttributes)
    {
        int32_t wh[2], thrown = 0;
        bool ok = ee_won_img_attrib((EeWonImgKind)inKind, &this->mFileReader->mCore, &wh, &thrown);
        this->mFileReader->Check(thrown);
        if (ok) { outAttributes.mWidth = wh[0]; outAttributes.mHeight = wh[1]; }
        return ok;
    }
    SmartPtr<RawImage> RustDecodeRaw(int inKind)
    {
        EeWonImgRaw raw = {0, 0, false, NULL};
        int32_t thrown = 0;
        bool ok = ee_won_img_decode((EeWonImgKind)inKind, &this->mFileReader->mCore, &raw, &thrown);
        this->mFileReader->Check(thrown);
        if (!ok) return NULL;
        SmartPtr<RawImage32> image = new RawImage32(raw.width, raw.height);
        image->mDoTransparency = raw.transparent;
        ee_won_img_take(&raw, (uint32_t*)image->mImageData);
        return image.get();
    }
};
// world: round 41's Rust map (imgdec.rs EeWonImgDec): ordered without case, the first spelling of a key stays
struct MultiImageDecoder : ImageDecoder {
    std::vector<std::pair<std::string, ImageDecoder*> > mMap;
    std::set<ImageDecoder*> mOwned;
    std::string mCur;
    ImageDecoder* Find()
    {
        for (size_t i = 0; i < mMap.size(); i++)
            if (StringCompareNoCase(mMap[i].first, mCur) == 0) return mMap[i].second;
        return NULL;
    }
    ~MultiImageDecoder()
    {
        for (std::set<ImageDecoder*>::iterator i = mOwned.begin(); i != mOwned.end(); ++i) delete *i;
    }
    void AddDecoder(ImageDecoder* decoder, const char* extension)
    {
        mOwned.insert(decoder);
        for (size_t i = 0; i < mMap.size(); i++)
            if (StringCompareNoCase(mMap[i].first, extension) == 0) { mMap[i].second = decoder; return; }
        size_t i = 0;
        while (i < mMap.size() && StringCompareNoCase(mMap[i].first, extension) < 0) i++;
        mMap.insert(mMap.begin() + (ptrdiff_t)i, std::make_pair(std::string(extension), decoder));
    }
    void SetFileName(const char* path)
    {
        WONAPI::WONFile file(path);
        this->mCur = file.GetFileExtension();
    }
    SmartPtr<NativeImage> Decode(DisplayContext* context, const char* path)
    {
        this->SetFileName(path);
        return ImageDecoder::Decode(context, path);
    }
    SmartPtr<RawImage> DecodeRaw(const char* path)
    {
        this->SetFileName(path);
        return ImageDecoder::DecodeRaw(path);
    }
    bool GetImageAttributes(const char* path, ImageAttributes& attributes)
    {
        this->SetFileName(path);
        return ImageDecoder::GetImageAttributes(path, attributes);
    }
    virtual SmartPtr<NativeImage> DoDecode(DisplayContext* context)
    {
        ImageDecoder* decoder = this->Find();
        return decoder != NULL ? decoder->Decode(context, *this->mFileReader) : NULL;
    }
    virtual SmartPtr<RawImage> DoDecodeRaw()
    {
        ImageDecoder* decoder = this->Find();
        return decoder != NULL ? decoder->DecodeRaw(*this->mFileReader) : NULL;
    }
    virtual bool DoGetAttrib(ImageAttributes& attributes)
    {
        ImageDecoder* decoder = this->Find();
        return decoder != NULL && decoder->GetImageAttributes(*this->mFileReader, attributes);
    }
};
struct PNGDecoder : ImageDecoder {
    static void Add(MultiImageDecoder* decoder) { decoder->AddDecoder(new PNGDecoder, "png"); }
    virtual bool DoGetAttrib(ImageAttributes& attributes) { return this->RustAttrib((int)EeWonImgKind::Png, attributes); }
    virtual SmartPtr<RawImage> DoDecodeRaw() { return this->RustDecodeRaw((int)EeWonImgKind::Png); }
};
struct JPGDecoder : ImageDecoder {
    static void Add(MultiImageDecoder* decoder)
    {
        JPGDecoder* jpg = new JPGDecoder;
        decoder->AddDecoder(jpg, "jpg");
        decoder->AddDecoder(jpg, "jpe");
        decoder->AddDecoder(jpg, "jpeg");
        decoder->AddDecoder(jpg, "pjpeg");
    }
    virtual bool DoGetAttrib(ImageAttributes& attributes) { return this->RustAttrib((int)EeWonImgKind::Jpg, attributes); }
    virtual SmartPtr<RawImage> DoDecodeRaw() { return this->RustDecodeRaw((int)EeWonImgKind::Jpg); }
};
static MultiImageDecoder* sWmDecoder; // world: the window manager's decoder (NULL: no window manager)

static SmartPtr<NativeImage> PlatGUIPackImage(DisplayContext* inContext, NativeImage* inImage, const char* inPath)
{
    long theShift = sShift; // world: GETextureManager::GetTexturePackLogicalShift()
    if (inImage == NULL || inPath == NULL || theShift <= 0 || inImage->GetWidth() == 0 || inImage->GetHeight() == 0)
        return inImage;
    if (sWmDecoder == NULL)
        return inImage;
    SmartPtr<RawImage> theRaw;
    char theName[4096];
    for (int i = 0; i < 2 && theRaw.get() == NULL; i++) {
        if (ee_won_plat_pack_name((const uint8_t*)inPath, i, (uint8_t*)theName, sizeof(theName)) < 0)
            break;
        FileReader theReader;
        if (!theReader.Open(theName))
            continue;
        MultiImageDecoder* theDecoder = sWmDecoder;
        theDecoder->SetFileName(theName);
        try {
            theRaw = theDecoder->ImageDecoder::DecodeRaw(theReader);
        } catch (FileReaderException&) {
            theRaw = NULL;
        }
    }
    if (theRaw.get() == NULL)
        return inImage;
    int theWidth = inImage->GetWidth(), theHeight = inImage->GetHeight(), theBox = 1;
    int theScale = ee_won_plat_pack_fit(theRaw->GetWidth(), theRaw->GetHeight(), theWidth, theHeight, (int)theShift, &theBox);
    if (theScale == 0)
        return inImage;
    SmartPtr<NativeImage> theNative = theRaw->GetNative(inContext);
    PlatNativeImage* thePack = dynamic_cast<PlatNativeImage*>(theNative.get());
    if (thePack == NULL || thePack->GetPixelWidth() != theRaw->GetWidth())
        return inImage;
    SmartPtr<PlatNativeImage> theImage = new PlatNativeImage(inContext, theWidth, theHeight, (float)theScale);
    theImage->mTransparentMask = inImage->mTransparentMask;
    PlatNativeMask* theMask = static_cast<PlatNativeMask*>(theImage->mTransparentMask.get());
    EeWonPlatMask theBits = theMask != NULL ? theMask->Rust() : EeWonPlatMask();
    ee_won_plat_pack_reduce(thePack->mSurface->GetRust(), theImage->mSurface->GetRust(), theWidth, theHeight, theScale, theBox,
                            theMask != NULL ? &theBits : NULL);
    return theImage.get();
}
SmartPtr<NativeImage> ImageDecoder::Decode(DisplayContext* context, const char* path)
{
    FileReader reader(path);
    SmartPtr<NativeImage> image = this->Decode(context, reader);
    if (image.get() != NULL) image = PlatGUIPackImage(context, image.get(), path);
    return image;
}
