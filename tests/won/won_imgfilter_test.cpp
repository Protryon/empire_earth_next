// WONGUI's image filters' pixel loops (R6 WON round 36): ImageFilters.cpp's statements copied from the C++ before the
// port (CropPixels, ScaleImageFilter::CheckSize, ScalePixels, ScaleBits, TintColor and TintImageFilter's loops,
// TransparentImageFilter's sample, palette search and 32-bit pass, TrimImageFilter's search) over stand-in raw
// images with the original's members and row walks (Image.cpp's StartRowTraversal, PlatNativeImage.cpp's
// CalcBytesPerRow), against ee_won's imgfilter.rs called as the ported ImageFilters.cpp calls it, over copies of
// the same images. Every allocation is zeroed (row padding), and a 32-bit image has one spare pixel (the
// original's scale read one pixel past each row, the last one past the buffer).
//
// 40,000 random filters of each kind over 1-, 8- and 32-bit images from 0 x 0 to 24 x 24 (sizes, percentages,
// tints, colours, samples and transparency drawn at random); the results (the filter's fields, the new image's
// size and every pixel byte, the palette) must be the same. `--print` writes rust/crates/ee_won/src/
// golden_imgfilter.txt (the first 400 filters, 80 of each kind), which the Rust tests replay.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include "rust/ee_won.h"

typedef uint32_t DWORD;
static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }

// ---- the stand-ins: the original's members and row walks
struct RawImage {
    int mType, mWidth, mHeight;
    explicit RawImage(int t) : mType(t), mWidth(0), mHeight(0) {}
    virtual ~RawImage() {}
    virtual bool HasTransparency() { return false; }
};
struct RawImage32 : RawImage {
    DWORD *mImageData, *mImageEnd, *mRowPtr, *mRowEnd;
    bool mDoTransparency;
    RawImage32(int width, int height) : RawImage(2) {
        mWidth = width; mHeight = height; mDoTransparency = false;
        mImageData = new DWORD[width * height + 1](); // the spare pixel
        mImageEnd = mImageData + width * height;
    }
    ~RawImage32() { delete[] mImageData; }
    virtual bool HasTransparency() { return mDoTransparency; }
    DWORD GetPixel(int x, int y) { return mImageData[y * mWidth + x]; }
    DWORD* StartRowTraversal(int row) { mRowPtr = mImageData + row * mWidth; mRowEnd = mRowPtr + mWidth; return mRowPtr; }
    void SetRowPixel(DWORD value) { *mRowPtr = value; }
    bool HasMoreRows() { return mRowEnd <= mImageEnd; }
    bool HasMoreRowPixels() { return mRowPtr < mRowEnd; }
    DWORD GetRowPixel() { return *mRowPtr; }
    void NextRowPixel() { ++mRowPtr; }
    void NextRowPixel(int n) { mRowPtr += n; }
    void NextRow() { mRowPtr = mRowEnd; mRowEnd += mWidth; }
    int GetWidth() { return mWidth; }
    int GetHeight() { return mHeight; }
    int Bytes() { return mWidth * mHeight * 4; }
    unsigned char* Data() { return (unsigned char*)mImageData; }
};
struct Palette {
    DWORD* mColors; int mNumColors;
    explicit Palette(int n) : mNumColors(n) { mColors = new DWORD[n ? n : 1](); }
    ~Palette() { delete[] mColors; }
};
struct RawImage8 : RawImage {
    unsigned char *mImageData, *mImageEnd, *mRowPtr, *mRowEnd;
    int mRowPad, mBytesPerRow;
    Palette* mPalette;
    int mTransparentColor;
    RawImage8(int width, int height) : RawImage(1) {
        mWidth = width; mHeight = height; mPalette = NULL; mTransparentColor = -1;
        mBytesPerRow = mWidth; if (mBytesPerRow % 4 != 0) mBytesPerRow += 4 - mBytesPerRow % 4;
        mRowPad = mBytesPerRow - mWidth;
        mImageData = new unsigned char[height * mBytesPerRow + 4]();
        mImageEnd = mImageData + mBytesPerRow * height;
    }
    ~RawImage8() { delete[] mImageData; }
    virtual bool HasTransparency() { return mTransparentColor >= 0; }
    unsigned char GetPixel(int x, int y) { return mImageData[y * mBytesPerRow + x]; }
    unsigned char* StartRowTraversal(int row) { mRowPtr = mImageData + row * mBytesPerRow; mRowEnd = mRowPtr + mWidth; return mRowPtr; }
    unsigned char GetRowPixel() { return *mRowPtr; }
    void SetRowPixel(unsigned char v) { *mRowPtr = v; }
    void NextRowPixel() { ++mRowPtr; }
    void NextRowPixel(int n) { mRowPtr += n; }
    int GetWidth() { return mWidth; }
    int GetHeight() { return mHeight; }
    int Bytes() { return mBytesPerRow * mHeight; }
    unsigned char* Data() { return mImageData; }
};
struct RawImage1 : RawImage {
    unsigned char *mImageData, *mImageEnd, *mRowPtr, *mRowEnd;
    int mRowMask, mRowPad, mBytesPerRow, mBytesPerRowMinusPad, mCompleteBytesPerRow, mWidthMod8;
    RawImage1(int width, int height) : RawImage(0) {
        mWidth = width; mHeight = height;
        mBytesPerRowMinusPad = mWidth / 8; mCompleteBytesPerRow = mBytesPerRowMinusPad; mWidthMod8 = mWidth % 8;
        if (mWidthMod8 != 0) ++mBytesPerRowMinusPad;
        mBytesPerRow = mBytesPerRowMinusPad; if (mBytesPerRow % 4 != 0) mBytesPerRow += 4 - mBytesPerRow % 4;
        mRowPad = mBytesPerRow - mBytesPerRowMinusPad;
        mImageData = new unsigned char[height * mBytesPerRow + 4]();
        mImageEnd = mImageData + mBytesPerRow * height;
    }
    ~RawImage1() { delete[] mImageData; }
    unsigned char* StartRowTraversal(int row) { mRowMask = 128; mRowPtr = mImageData + row * mBytesPerRow; mRowEnd = mRowPtr + mCompleteBytesPerRow; return mRowPtr; }
    void NextRowPixel() { mRowMask >>= 1; if (mRowMask == 0) { mRowPtr++; mRowMask = 128; } }
    void NextRowPixel(int n) { for (int i = 0; i < n; i++) NextRowPixel(); }
    void SetRowPixel(bool on) { if (on) *mRowPtr |= mRowMask; else *mRowPtr &= ~mRowMask; }
    bool GetRowPixel() { return (*mRowPtr) & mRowMask ? true : false; }
    int GetWidth() { return mWidth; }
    int GetHeight() { return mHeight; }
    int Bytes() { return mBytesPerRow * mHeight; }
    unsigned char* Data() { return mImageData; }
};
static void CopyInfo(RawImage8* dst, RawImage8* src) { dst->mPalette = src->mPalette; dst->mTransparentColor = src->mTransparentColor; }
static void CopyInfo(RawImage32* dst, RawImage32* src) { dst->mDoTransparency = src->mDoTransparency; }

static EeWonRawPixels Px(RawImage* img)
{
    EeWonRawPixels p;
    p.kind = img->mType; p.width = img->mWidth; p.height = img->mHeight;
    if (img->mType == 0) { p.data = ((RawImage1*)img)->mImageData; p.pitch = ((RawImage1*)img)->mBytesPerRow; }
    else if (img->mType == 1) { p.data = ((RawImage8*)img)->mImageData; p.pitch = ((RawImage8*)img)->mBytesPerRow; }
    else { p.data = (unsigned char*)((RawImage32*)img)->mImageData; p.pitch = img->mWidth * 4; }
    return p;
}

// ---- the original statements (ImageFilters.cpp)
namespace orig {
template<class ImageType,class PixelType>
static ImageType* CropPixels(ImageType* src,int x,int y,int width,int height)
{
    ImageType* dst=new ImageType(width,height);
    CopyInfo(dst,src);
    for(int row=0;row<height;++row)
    {
        PixelType* from=src->StartRowTraversal(row+y)+x;
        PixelType* to=dst->StartRowTraversal(row);
        memcpy(to,from,sizeof(PixelType)*width);
    }
    return dst;
}
struct Scale { int mWidth,mHeight,mPercentWidth,mPercentHeight; bool mIsPercent; };
static bool CheckSize(Scale* f, RawImage* image)
{
    if(f->mIsPercent)
    {
        f->mWidth=image->mWidth*f->mPercentWidth/100;
        f->mHeight=image->mHeight*f->mPercentHeight/100;
    }
    if(f->mWidth<=0) f->mWidth=1;
    if(f->mHeight<=0) f->mHeight=1;
    if(f->mWidth==image->mWidth && f->mHeight==image->mHeight)
        return true;

    return false;
}
template<class ImageType,class PixelType>
static ImageType* ScalePixels(ImageType* src,int width,int height)
{
    ImageType* dst=new ImageType(width,height);
    CopyInfo(dst,src);
    int cy=0,oy=0;
    for(int y=0;y<height;++y)
    {
        src->StartRowTraversal(oy);
        dst->StartRowTraversal(y);
        PixelType value=src->GetRowPixel();
        int cx=0;
        for(int x=0;x<width;++x)
        {
            dst->SetRowPixel(value);
            dst->NextRowPixel();
            cx+=src->GetWidth();
            if(cx<width)
                continue;
            src->NextRowPixel(cx/width);
            cx%=width;
            value=src->GetRowPixel();
        }
        cy+=src->GetHeight();
        if(cy>=height) { oy+=cy/height;cy%=height; }
    }
    return dst;
}
static RawImage1* ScaleBits(RawImage1* src,int width,int height)
{
    RawImage1* dst=new RawImage1(width,height);
    int cy=0,oy=0;
    for(int y=0;y<height;++y)
    {
        src->StartRowTraversal(oy);
        dst->StartRowTraversal(y);
        bool value=src->GetRowPixel();
        int cx=0;
        for(int x=0;x<width;++x)
        {
            dst->SetRowPixel(value);
            dst->NextRowPixel();
            cx+=src->GetWidth();
            if(cx<width)
                continue;
            src->NextRowPixel(cx/width);
            cx%=width;
            value=src->GetRowPixel();
        }
        cy+=src->GetHeight();
        if(cy>=height) { oy+=cy/height;cy%=height; }
    }
    return dst;
}
static inline DWORD TintColor(DWORD color,int redPercent,int greenPercent,int bluePercent)
{
    int red=((color>>16)&0xff)*redPercent/100;
    int green=((color>>8)&0xff)*greenPercent/100;
    int blue=(color&0xff)*bluePercent/100;
    if(red>255) red=255;
    if(green>255) green=255;
    if(blue>255) blue=255;
    return (((red<<8)|green)<<8)|blue;
}
static void Tint(RawImage* image, int mRed, int mGreen, int mBlue)
{
    if(image->mType==1)
    {
        RawImage8* raw=static_cast<RawImage8*>(image);
        Palette* palette=raw->mPalette;
        for(int i=0;i<palette->mNumColors;++i)
            palette->mColors[i]=TintColor(palette->mColors[i],mRed,mGreen,mBlue);
    }
    else if(image->mType==2)
    {
        RawImage32* raw=static_cast<RawImage32*>(image);
        raw->StartRowTraversal(0);
        while(raw->HasMoreRows())
        {
            while(raw->HasMoreRowPixels())
            {
                DWORD color=raw->GetRowPixel();
                raw->SetRowPixel((color&0xff000000)|TintColor(color,mRed,mGreen,mBlue));
                raw->NextRowPixel();
            }
            raw->NextRow();
        }
    }
}
enum PixelSample { UpperLeft,UpperRight,LowerLeft,LowerRight };
static void Transparent(RawImage* image, DWORD mTransparentColor, bool mUseSample, int mPixelSample)
{
    if(image->mType==0) return;
    DWORD color=mTransparentColor;
    if(mUseSample)
    {
        if(image->mHeight<=0 || image->mWidth<=0) return;
        int x=0,y=0;
        switch(mPixelSample)
        {
        case UpperLeft:x=0;y=0;break;
        case UpperRight:x=image->mWidth-1;y=0;break;
        case LowerLeft:x=0;y=image->mHeight-1;break;
        case LowerRight:x=image->mWidth-1;y=image->mHeight-1;break;
        }
        if(image->mType==1)
        {
            RawImage8* raw=static_cast<RawImage8*>(image);
            raw->mTransparentColor=raw->GetPixel(x,y);
            return;
        }
        if(image->mType==2) color=static_cast<RawImage32*>(image)->GetPixel(x,y);
    }
    if(image->mType==1)
    {
        RawImage8* raw=static_cast<RawImage8*>(image);
        Palette* palette=raw->mPalette;
        for(int i=0;i<palette->mNumColors;++i)
            if(palette->mColors[i]==color) { raw->mTransparentColor=i;break; }
        return;
    }
    else if(image->mType==2)
    {
        RawImage32* raw=static_cast<RawImage32*>(image);
        raw->mDoTransparency=true;
        raw->StartRowTraversal(0);
        while(raw->HasMoreRows())
        {
            while(raw->HasMoreRowPixels())
            {
                DWORD aColor=raw->GetRowPixel();
                if(aColor==color) raw->SetRowPixel(0);
                else raw->SetRowPixel(aColor|0xff000000);
                raw->NextRowPixel();
            }
            raw->NextRow();
        }
    }
}
static bool TrimFilter_IsRowEmpty(unsigned char *pixels, unsigned char transparentPixel, int width)
{
	for(int i=0; i<width; i++)
		if(*pixels++!=transparentPixel)
			return false;
	return true;
}
static bool TrimFilter_IsRowEmpty(DWORD *pixels, int width)
{
	for(int i=0; i<width; i++)
		if((*pixels++)&0xff000000)
			return false;
	return true;
}
static bool TrimFilter_IsColEmpty(unsigned char *pixels, unsigned char transparentPixel, int height, int rowInc)
{
	for(int i=0; i<height; i++, pixels+=rowInc)
	{
		if(*pixels!=transparentPixel)
			return false;
	}
	return true;
}
static bool TrimFilter_IsColEmpty(DWORD *pixels, int height, int rowInc)
{
	for(int i=0; i<height; i++, pixels+=rowInc)
	{
		if((*pixels)&0xff000000)
			return false;
	}
	return true;
}
// TrimImageFilter::Filter's search: the crop's fields, false for NULL (the crop itself is CropPixels)
struct Crop { int mx,my,mWidth,mHeight; };
static int Trim(Crop* f, RawImage *theImage)
{
	f->mx = f->my = 0;
	f->mWidth = theImage->mWidth;
	f->mHeight = theImage->mHeight;

	if(!theImage->HasTransparency())
		return 2;

	int x,y;
	int left = 0,top = 0,right = f->mWidth-1, bottom = f->mHeight-1;
	if(theImage->mType==1)
	{
		RawImage8 *anImage = static_cast<RawImage8*>(theImage);
		unsigned char transparentPixel = anImage->mTransparentColor;
		unsigned char *pixels = anImage->mImageData;
		int bytesPerRow = anImage->mBytesPerRow;
		for(y=0; y<f->mHeight; y++, pixels+=bytesPerRow)
		{
			if(!TrimFilter_IsRowEmpty(pixels,transparentPixel,f->mWidth))
				break;
		}
		if(y>=f->mHeight)
			return 0;
		top = y;
		pixels = anImage->mImageData + (f->mHeight-1)*bytesPerRow;
		for(y=bottom; y>top; y--, pixels-=bytesPerRow)
		{
			if(!TrimFilter_IsRowEmpty(pixels,transparentPixel,f->mWidth))
				break;
		}
		bottom = y;
		f->mHeight = bottom-top+1;
		pixels = anImage->mImageData + top*bytesPerRow;
		for(x=0; x<f->mWidth; x++, pixels++)
		{
			if(!TrimFilter_IsColEmpty(pixels,transparentPixel,f->mHeight,bytesPerRow))
				break;
		}
		if(x>=f->mWidth)
			return 0;
		left = x;
		pixels = anImage->mImageData + top*bytesPerRow + (f->mWidth-1);
		for(x=right; x>left; x--, pixels--)
		{
			if(!TrimFilter_IsColEmpty(pixels,transparentPixel,f->mHeight,bytesPerRow))
				break;
		}
		right = x;
		f->mWidth = right-left+1;
	}
	else if(theImage->mType==2)
	{
		RawImage32 *anImage = static_cast<RawImage32*>(theImage);
		DWORD *pixels = anImage->mImageData;
		int bytesPerRow = f->mWidth;
		for(y=0; y<f->mHeight; y++, pixels+=bytesPerRow)
		{
			if(!TrimFilter_IsRowEmpty(pixels,f->mWidth))
				break;
		}
		if(y>=f->mHeight)
			return 0;
		top = y;
		pixels = anImage->mImageData + (f->mHeight-1)*bytesPerRow;
		for(y=bottom; y>top; y--, pixels-=bytesPerRow)
		{
			if(!TrimFilter_IsRowEmpty(pixels,f->mWidth))
				break;
		}
		bottom = y;
		f->mHeight = bottom-top+1;
		pixels = anImage->mImageData + top*bytesPerRow;
		for(x=0; x<f->mWidth; x++, pixels++)
		{
			if(!TrimFilter_IsColEmpty(pixels,f->mHeight,bytesPerRow))
				break;
		}
		if(x>=f->mWidth)
			return 0;
		left = x;
		pixels = anImage->mImageData + top*bytesPerRow + (f->mWidth-1);
		for(x=right; x>left; x--, pixels--)
		{
			if(!TrimFilter_IsColEmpty(pixels,f->mHeight,bytesPerRow))
				break;
		}
		right = x;
		f->mWidth = right-left+1;
	}
	else
		return 0;

	f->mx = left; f->my = top;
	f->mWidth = right-left+1;
	f->mHeight = bottom-top+1;
	return 1;
}
} // namespace orig

// ---- the images, their dumps
static RawImage* NewImage(int kind, int w, int h)
{
    if (kind == 0) return new RawImage1(w, h);
    if (kind == 1) return new RawImage8(w, h);
    return new RawImage32(w, h);
}
static unsigned char* Data(RawImage* i) { return i->mType == 0 ? ((RawImage1*)i)->Data() : i->mType == 1 ? ((RawImage8*)i)->Data() : ((RawImage32*)i)->Data(); }
static int Bytes(RawImage* i) { return i->mType == 0 ? ((RawImage1*)i)->Bytes() : i->mType == 1 ? ((RawImage8*)i)->Bytes() : ((RawImage32*)i)->Bytes(); }
static DWORD Pixel32()
{
    static const DWORD kSome[] = { 0, 0xff000000u, 0x00ffffffu, 0x12345678u, 0x80ff00ffu };
    return R(3) == 0 ? kSome[R(5)] : ((DWORD)Rand() << 17) ^ ((DWORD)Rand() << 2) ^ (DWORD)R(4);
}
static RawImage* RandomImage(int kind, int w, int h, Palette* pal)
{
    RawImage* img = NewImage(kind, w, h);
    unsigned char* d = Data(img);
    int n = Bytes(img);
    int sparse = R(3); // mostly transparent images for the trim
    if (kind == 2) {
        for (int i = 0; i < w * h; i++) ((DWORD*)d)[i] = sparse == 0 && R(8) ? (Pixel32() & 0x00ffffffu) : Pixel32();
    } else {
        for (int i = 0; i < n; i++) d[i] = sparse == 0 && R(8) ? 3 : (unsigned char)R(kind == 1 ? 8 : 256);
    }
    if (kind == 1) { ((RawImage8*)img)->mPalette = pal; ((RawImage8*)img)->mTransparentColor = R(3) ? (int)R(9) - 1 : -1; }
    if (kind == 2) ((RawImage32*)img)->mDoTransparency = R(2);
    return img;
}
static RawImage* Clone(RawImage* src)
{
    RawImage* img = NewImage(src->mType, src->mWidth, src->mHeight);
    memcpy(Data(img), Data(src), Bytes(img));
    if (src->mType == 1) { ((RawImage8*)img)->mPalette = ((RawImage8*)src)->mPalette; ((RawImage8*)img)->mTransparentColor = ((RawImage8*)src)->mTransparentColor; }
    if (src->mType == 2) ((RawImage32*)img)->mDoTransparency = ((RawImage32*)src)->mDoTransparency;
    return img;
}
static bool sHex;
static std::string Dump(RawImage* img)
{
    std::string s;
    char b[64];
    if (img == NULL) return "NULL";
    snprintf(b, sizeof(b), "%d %dx%d", img->mType, img->mWidth, img->mHeight);
    s += b;
    if (img->mType == 1) { snprintf(b, sizeof(b), " t%d", ((RawImage8*)img)->mTransparentColor); s += b; }
    if (img->mType == 2) { snprintf(b, sizeof(b), " d%d", ((RawImage32*)img)->mDoTransparency ? 1 : 0); s += b; }
    s += " ";
    unsigned char* d = Data(img);
    int n = Bytes(img);
    if (!sHex) { s.append((const char*)d, (size_t)n); return s; } // compared raw; hex for the golden and failures
    static const char kHex[] = "0123456789abcdef";
    for (int i = 0; i < n; i++) { s += kHex[d[i] >> 4]; s += kHex[d[i] & 15]; }
    return s;
}
static std::string DumpPal(Palette* p)
{
    std::string s = "pal";
    char b[16];
    for (int i = 0; i < p->mNumColors; i++) { snprintf(b, sizeof(b), " %x", (unsigned)p->mColors[i]); s += b; }
    return s;
}

// ---- the ported calls (as the ported ImageFilters.cpp makes them)
namespace port {
static RawImage* Crop(RawImage* src, int x, int y, int w, int h)
{
    RawImage* dst = NewImage(src->mType, w, h);
    if (src->mType == 1) { ((RawImage8*)dst)->mPalette = ((RawImage8*)src)->mPalette; ((RawImage8*)dst)->mTransparentColor = ((RawImage8*)src)->mTransparentColor; }
    else ((RawImage32*)dst)->mDoTransparency = ((RawImage32*)src)->mDoTransparency;
    int px = src->mType == 1 ? 1 : 4;
    EeWonRawPixels s = Px(src), d = Px(dst);
    ee_won_imgf_crop(&s, &d, x * px, y, w * px, h);
    return dst;
}
static RawImage* ScaleTo(RawImage* src, int w, int h)
{
    RawImage* dst = NewImage(src->mType, w, h);
    if (src->mType == 1) { ((RawImage8*)dst)->mPalette = ((RawImage8*)src)->mPalette; ((RawImage8*)dst)->mTransparentColor = ((RawImage8*)src)->mTransparentColor; }
    if (src->mType == 2) ((RawImage32*)dst)->mDoTransparency = ((RawImage32*)src)->mDoTransparency;
    EeWonRawPixels s = Px(src), d = Px(dst);
    ee_won_imgf_scale(&s, &d);
    return dst;
}
static void Tint(RawImage* image, int r, int g, int b)
{
    if (image->mType == 1) { Palette* p = ((RawImage8*)image)->mPalette; ee_won_imgf_tint(p->mColors, (size_t)p->mNumColors, r, g, b, false); }
    else if (image->mType == 2) ee_won_imgf_tint(((RawImage32*)image)->mImageData, (size_t)image->mWidth * image->mHeight, r, g, b, true);
}
static void Transparent(RawImage* image, DWORD color, bool useSample, int sample)
{
    if (image->mType == 0) return;
    if (useSample) {
        if (image->mHeight <= 0 || image->mWidth <= 0) return;
        int x, y;
        ee_won_imgf_sample(sample, image->mWidth, image->mHeight, &x, &y);
        if (image->mType == 1) { ((RawImage8*)image)->mTransparentColor = ((RawImage8*)image)->GetPixel(x, y); return; }
        if (image->mType == 2) color = ((RawImage32*)image)->GetPixel(x, y);
    }
    if (image->mType == 1) {
        RawImage8* raw = (RawImage8*)image;
        int i = ee_won_imgf_palette_find(raw->mPalette->mColors, (size_t)raw->mPalette->mNumColors, color);
        if (i >= 0) raw->mTransparentColor = i;
    } else if (image->mType == 2) {
        ((RawImage32*)image)->mDoTransparency = true;
        ee_won_imgf_transparent32(((RawImage32*)image)->mImageData, (size_t)image->mWidth * image->mHeight, color);
    }
}
static int Trim(orig::Crop* f, RawImage* img)
{
    f->mx = f->my = 0; f->mWidth = img->mWidth; f->mHeight = img->mHeight;
    if (!img->HasTransparency()) return 2;
    if (img->mType != 1 && img->mType != 2) return 0;
    EeWonRawPixels p = Px(img);
    int rect[4];
    if (!ee_won_imgf_trim(&p, img->mType == 1 ? (unsigned char)((RawImage8*)img)->mTransparentColor : 0, rect)) return 0;
    f->mx = rect[0]; f->my = rect[1]; f->mWidth = rect[2]; f->mHeight = rect[3];
    return 1;
}
}

static int Size() { return (int)R(25); }

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    const int kEach = 40000;
    int aFailures = 0;
    long long aChecks = 0;
    sSeed = 3636;
    for (int i = 0; i < kEach * 5; i++) {
        sHex = aPrint && i < 400;
        int what = i % 5; // 0 crop, 1 scale, 2 tint, 3 transparent, 4 trim
        int kind = what == 0 || what == 4 ? 1 + (int)R(2) : (int)R(3);
        int w = Size(), h = Size();
        // a 32-bit image 0 pixels wide never ends the original's row walk (NextRow adds 0): no such image is tinted or
        // made transparent here
        if (kind == 2 && (what == 2 || what == 3) && w == 0) w = 1;
        Palette* pal = new Palette((int)R(10));
        for (int c = 0; c < pal->mNumColors; c++) pal->mColors[c] = R(3) ? Pixel32() : 0x00ffffffu;
        Palette* pal2 = new Palette(pal->mNumColors);
        memcpy(pal2->mColors, pal->mColors, sizeof(DWORD) * (pal->mNumColors ? pal->mNumColors : 1));
        RawImage* a = RandomImage(kind, w, h, pal);
        RawImage* b = Clone(a);
        if (kind == 1) ((RawImage8*)b)->mPalette = pal2;
        std::string in = Dump(a) + " " + DumpPal(pal), ra, rb, label;
        char aLine[200];
        switch (what) {
        case 0: { // a rectangle inside the image
            if (w == 0 || h == 0) { label = "skip"; break; }
            int x = (int)R(w), y = (int)R(h), cw = (int)R(w - x + 1), ch = (int)R(h - y + 1);
            RawImage* oa = kind == 1 ? (RawImage*)orig::CropPixels<RawImage8, unsigned char>((RawImage8*)a, x, y, cw, ch)
                                     : (RawImage*)orig::CropPixels<RawImage32, DWORD>((RawImage32*)a, x, y, cw, ch);
            RawImage* ob = port::Crop(b, x, y, cw, ch);
            snprintf(aLine, sizeof(aLine), "crop %d %d %d %d", x, y, cw, ch); label = aLine;
            ra = Dump(oa); rb = Dump(ob); delete oa; delete ob; break;
        }
        case 1: {
            orig::Scale fa;
            fa.mIsPercent = R(2); fa.mPercentWidth = (int)R(400) - 50; fa.mPercentHeight = (int)R(400) - 50;
            fa.mWidth = (int)R(40) - 5; fa.mHeight = (int)R(40) - 5;
            orig::Scale fb = fa;
            snprintf(aLine, sizeof(aLine), "scale %d %d %d %d %d", fa.mIsPercent ? 1 : 0, fa.mPercentWidth, fa.mPercentHeight, fa.mWidth, fa.mHeight);
            label = aLine;
            bool sa = orig::CheckSize(&fa, a);
            bool sb = ee_won_imgf_check_size(fb.mIsPercent, fb.mPercentWidth, fb.mPercentHeight, &fb.mWidth, &fb.mHeight, b->mWidth, b->mHeight);
            snprintf(aLine, sizeof(aLine), "size %d %d %d", sa ? 1 : 0, fa.mWidth, fa.mHeight); ra = aLine;
            snprintf(aLine, sizeof(aLine), "size %d %d %d", sb ? 1 : 0, fb.mWidth, fb.mHeight); rb = aLine;
            if (!sa && w > 0 && h > 0) {
                RawImage* oa = kind == 0 ? (RawImage*)orig::ScaleBits((RawImage1*)a, fa.mWidth, fa.mHeight)
                             : kind == 1 ? (RawImage*)orig::ScalePixels<RawImage8, unsigned char>((RawImage8*)a, fa.mWidth, fa.mHeight)
                                         : (RawImage*)orig::ScalePixels<RawImage32, DWORD>((RawImage32*)a, fa.mWidth, fa.mHeight);
                RawImage* ob = sb ? NULL : port::ScaleTo(b, fb.mWidth, fb.mHeight);
                ra += " " + Dump(oa); rb += " " + Dump(ob); delete oa; delete ob;
            }
            break;
        }
        case 2: {
            int r = (int)R(300) - 20, g = (int)R(300) - 20, bl = (int)R(300) - 20;
            snprintf(aLine, sizeof(aLine), "tint %d %d %d", r, g, bl); label = aLine;
            orig::Tint(a, r, g, bl); port::Tint(b, r, g, bl);
            ra = Dump(a) + " " + DumpPal(pal); rb = Dump(b) + " " + DumpPal(pal2); break;
        }
        case 3: {
            bool use = R(2); int sample = (int)R(6) - 1;
            DWORD color = R(2) && kind == 2 && w * h > 0 ? ((RawImage32*)a)->mImageData[R(w * h)]
                        : R(2) && pal->mNumColors ? pal->mColors[R(pal->mNumColors)] : Pixel32();
            snprintf(aLine, sizeof(aLine), "transparent %d %d %x", use ? 1 : 0, sample, (unsigned)color); label = aLine;
            orig::Transparent(a, color, use, sample); port::Transparent(b, color, use, sample);
            ra = Dump(a); rb = Dump(b); break;
        }
        case 4: {
            orig::Crop fa, fb;
            int qa = orig::Trim(&fa, a), qb = port::Trim(&fb, b);
            label = "trim";
            snprintf(aLine, sizeof(aLine), "trim %d %d %d %d %d", qa, fa.mx, fa.my, fa.mWidth, fa.mHeight); ra = aLine;
            snprintf(aLine, sizeof(aLine), "trim %d %d %d %d %d", qb, fb.mx, fb.my, fb.mWidth, fb.mHeight); rb = aLine;
            break;
        }
        }
        aChecks++;
        if (ra != rb) {
            if (++aFailures < 8) printf("FAIL %d %s (rerun with --print to see the pixels)\n", i, label.c_str());
            if (aFailures < 8 && sHex) printf("FAIL %d %s\n in  %s\n orig %s\n port %s\n", i, label.c_str(), in.c_str(), ra.c_str(), rb.c_str());
        }
        if (aPrint && i < 400 && label != "skip") printf("F %s\nI %s\nO %s\n", label.c_str(), in.c_str(), ra.c_str());
        delete a; delete b; delete pal; delete pal2;
    }
    if (aPrint) return 0;
    printf("won_imgfilter_test: %lld filters, %d failures\n", aChecks, aFailures);
    return aFailures ? 1 : 0;
}
