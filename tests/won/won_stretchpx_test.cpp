// StretchImageFilter's pixel work (R6 WON round 37): the original StretchImage.cpp's CopyRawImage and RawStretch
// templates and DoFilter's loop over guidraw.rs's plan, copied from the C++ before the port over stand-in raw images
// with the original's row walks (Image.cpp's StartRowTraversal; an 8-bit image's rows padded to four bytes as
// PlatNativeImage.cpp's CalcBytesPerRow pads them), against ee_won's imgfilter.rs stretch_run over the same plan and
// copies of the same images.
//
// Each of 100,000 filters takes a random 8-bit or 32-bit source (1..24 x 1..24, random pixels), borders inside it
// (left + right <= its width, top + bottom <= its height, as the lobby's stretch images are), a target size 0..60
// and the tile and skip-centre flags; the new images' every byte is compared. The first 300 are
// rust/crates/ee_won/src/golden_stretchpx.txt (the inputs and the new image's hash), which the Rust tests replay;
// `--print` prints them.
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
static int RI(int lo, int hi) { return lo + (int)R((unsigned)(hi - lo + 1)); }

// ---- the raw images (Image.cpp's row walks)
struct RawImage8 {
    int mWidth, mHeight, mBytesPerRow;
    unsigned char* mImageData;
    unsigned char *mRowPtr, *mRowEnd;
    RawImage8(int w, int h) : mWidth(w), mHeight(h)
    {
        mBytesPerRow = w;
        if (mBytesPerRow % 4 != 0) mBytesPerRow += 4 - mBytesPerRow % 4;
        mImageData = new unsigned char[(size_t)(mBytesPerRow * h > 0 ? mBytesPerRow * h : 1)];
        memset(mImageData, 0, (size_t)(mBytesPerRow * h > 0 ? mBytesPerRow * h : 1));
    }
    ~RawImage8() { delete[] mImageData; }
    int GetWidth() { return mWidth; }
    int GetHeight() { return mHeight; }
    unsigned char* StartRowTraversal(int row)
    {
        this->mRowPtr=this->mImageData+row*this->mBytesPerRow;
        this->mRowEnd=this->mRowPtr+this->mWidth;
        return this->mRowPtr;
    }
    size_t Bytes() { return (size_t)(mBytesPerRow * mHeight); }
    unsigned char* Data() { return mImageData; }
    EeWonRawPixels Px() { EeWonRawPixels p = {1, mImageData, mWidth, mHeight, mBytesPerRow}; return p; }
};
struct RawImage32 {
    int mWidth, mHeight;
    DWORD* mImageData;
    DWORD *mRowPtr, *mRowEnd;
    RawImage32(int w, int h) : mWidth(w), mHeight(h)
    {
        mImageData = new DWORD[(size_t)(w * h > 0 ? w * h : 1)];
        memset(mImageData, 0, sizeof(DWORD) * (size_t)(w * h > 0 ? w * h : 1));
    }
    ~RawImage32() { delete[] mImageData; }
    int GetWidth() { return mWidth; }
    int GetHeight() { return mHeight; }
    DWORD* StartRowTraversal(int row)
    {
        this->mRowPtr=this->mImageData+row*this->mWidth;
        this->mRowEnd=this->mRowPtr+this->mWidth;
        return this->mRowPtr;
    }
    size_t Bytes() { return (size_t)(mWidth * mHeight) * 4; }
    unsigned char* Data() { return (unsigned char*)mImageData; }
    EeWonRawPixels Px() { EeWonRawPixels p = {2, (uint8_t*)mImageData, mWidth, mHeight, mWidth * 4}; return p; }
};

// ---- the original statements (StretchImage.cpp before the port)
namespace orig {
template <class ImageType, class PixelType>
static void CopyRawImage(ImageType *src, ImageType *dst, int destx, int desty, int x, int y, int width, int height)
{
	for(int i = 0; i<height; i++)
	{
		PixelType *aFromPixel = src->StartRowTraversal(y+i) + x;
		PixelType *aToPixel = dst->StartRowTraversal(desty+i) + destx;
		memcpy(aToPixel,aFromPixel,sizeof(PixelType)*width);
	}
}

template <class ImageType, class PixelType>
static void RawStretch(ImageType *src, ImageType *dst, int x1, int x2, int y1, int y2, int tileLeft, int tileTop, int tileWidth, int tileHeight)
{
	int theNewWidth = x2 - x1;
	int theNewHeight = y2 - y1;
	if(theNewWidth<=0 || theNewHeight<=0)
		return;


	int cy = 0, oy = tileTop;
	for(int y=0; y<theNewHeight; y++)
	{
		src->StartRowTraversal(oy);
		src->mRowPtr += tileLeft;
		dst->StartRowTraversal(y+y1);
		dst->mRowPtr += x1;

		PixelType curVal = *src->mRowPtr;

		int cx = 0;
		for(int x=0; x<theNewWidth; x++)
		{
			*dst->mRowPtr = curVal;
			++dst->mRowPtr;
			cx += tileWidth;
			if(cx >= theNewWidth)
			{
				src->mRowPtr += cx/theNewWidth;
				cx%=theNewWidth;
				curVal = *src->mRowPtr;
			}
		}

		cy += tileHeight;
		if(cy >= theNewHeight)
		{
			oy += cy/theNewHeight;
			cy%=theNewHeight;
		}
	}
}

// DoFilter's loop over the plan
template <class ImageType, class PixelType>
static void Run(ImageType* src, ImageType* dst, const std::vector<EeWonDrawOp>& ops)
{
	for(size_t i = 0; i < ops.size(); i++)
	{
		const EeWonDrawOp* o = &ops[i];
		if(o->kind==EE_WON_DRAW_COPY_PIXELS)
			CopyRawImage<ImageType,PixelType>(o->a ? dst : src,dst,o->b,o->c,o->d,o->e,o->f,o->g);
		else if(o->kind==EE_WON_DRAW_STRETCH_PIXELS)
			RawStretch<ImageType,PixelType>(src,dst,o->a,o->b,o->c,o->d,o->e,o->f,o->g,o->h);
	}
}
}

static uint64_t Fnv(const unsigned char* p, size_t n)
{
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

template <class ImageType, class PixelType>
static bool One(int kind, int sw, int sh, EeWonStretchFilter f, unsigned pixSeed, std::string& out)
{
    ImageType src(sw, sh);
    unsigned s = pixSeed;
    for (size_t i = 0; i < src.Bytes(); i++) { s = s * 1103515245u + 12345u; src.Data()[i] = (unsigned char)(s >> 16); }
    int w = 0, h = 0;
    std::vector<EeWonDrawOp> ops(64);
    size_t n = ee_won_gui_draw_stretch_filter(&f, &w, &h, &ops[0], ops.size());
    if (n > ops.size()) { ops.resize(n); n = ee_won_gui_draw_stretch_filter(&f, &w, &h, &ops[0], ops.size()); }
    ops.resize(n);
    ImageType a(w, h), b(w, h);
    orig::Run<ImageType, PixelType>(&src, &a, ops);
    EeWonRawPixels ps = src.Px(), pb = b.Px();
    ee_won_imgf_stretch_run(&ps, &pb, ops.empty() ? NULL : &ops[0], ops.size());
    char aLine[300];
    snprintf(aLine, sizeof(aLine), "%d %d %d %u %d %d %d %d %d %d %d %d %d = %d %d %zu %016llx", kind, sw, sh, pixSeed, f.width,
             f.height, f.left, f.right, f.top, f.bottom, f.tile_horz ? 1 : 0, f.tile_vert ? 1 : 0, f.skip_center ? 1 : 0, w, h,
             ops.size(), (unsigned long long)Fnv(a.Data(), a.Bytes()));
    out = aLine;
    return a.Bytes() == b.Bytes() && memcmp(a.Data(), b.Data(), a.Bytes()) == 0;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    int aFailures = 0;
    const int kFilters = 100000;
    for (int n = 0; n < kFilters; n++) {
        sSeed = 7907u * (unsigned)n + 5u;
        int kind = R(2) ? 1 : 2;
        int sw = RI(1, 24), sh = RI(1, 24);
        EeWonStretchFilter f;
        memset(&f, 0, sizeof(f));
        f.left = (int)R((unsigned)sw + 1); f.right = (int)R((unsigned)(sw - f.left) + 1);
        f.top = (int)R((unsigned)sh + 1); f.bottom = (int)R((unsigned)(sh - f.top) + 1);
        if (R(4) == 0) f.left = f.right = 0;
        if (R(4) == 0) f.top = f.bottom = 0;
        f.width = RI(0, 60); f.height = RI(0, 60);
        f.src_width = sw; f.src_height = sh;
        f.tile_horz = R(2) != 0; f.tile_vert = R(2) != 0; f.skip_center = R(5) == 0;
        unsigned pixSeed = Rand() * 65536u + Rand();
        std::string line;
        bool ok = kind == 1 ? One<RawImage8, unsigned char>(kind, sw, sh, f, pixSeed, line)
                            : One<RawImage32, DWORD>(kind, sw, sh, f, pixSeed, line);
        if (!ok && ++aFailures < 20) printf("FAIL %d %s\n", n, line.c_str());
        if (aPrint && n < 300) printf("%s\n", line.c_str());
    }
    if (aPrint) return 0;
    printf("won_stretchpx_test: %d filters, %d failures\n", kFilters, aFailures);
    return aFailures ? 1 : 0;
}
