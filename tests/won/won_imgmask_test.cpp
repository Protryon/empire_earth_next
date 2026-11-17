// The raw images' transparency masks (R6 WON round 37): the original Image.cpp's RawImage32::GetNative transparency
// pass and RawImage8::GetNative mask loop, copied from the C++ before the port over stand-in raw images with the
// original's row walks (Image.cpp's StartRowTraversal, WONLobbyGUI.h's row pixel walks, RawImage1's constructor and
// PlatNativeImage.cpp's CalcBytesPerRow), against ee_won's imgfilter.rs mask32 and mask8 over copies.
//
// Each of 200,000 images (32-bit or 8-bit, 0..40 x 0..20; pixels with alpha 0 or the transparent index now and then;
// the transparent index -2..300; masks filled with random bytes first, as `new` leaves them) is masked on both sides;
// the pixels and every mask byte are compared. The first 300 are rust/crates/ee_won_gui/src/golden_imgmask.txt (the
// inputs' seeds and the results' hashes), which the Rust tests replay; `--print` prints them.
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

// ---- the raw images (the original's members and walks)
struct RawImage1 {
    int mWidth, mHeight;
    unsigned char* mImageData;
    unsigned char *mImageEnd, *mRowPtr, *mRowEnd;
    int mRowMask, mRowPad, mEndRowMask, mBytesPerRow, mBytesPerRowMinusPad, mCompleteBytesPerRow, mWidthMod8;
    void CalcBytesPerRow()
    {
        this->mBytesPerRow = this->mBytesPerRowMinusPad;
        if (this->mBytesPerRow % 4 != 0)
            this->mBytesPerRow += 4 - this->mBytesPerRow % 4;
    }
    RawImage1(int width,int height) {
        this->mWidth=width;
        this->mHeight=height;
        this->mBytesPerRowMinusPad=this->mWidth/8;
        this->mCompleteBytesPerRow=this->mBytesPerRowMinusPad;
        this->mWidthMod8=this->mWidth%8;
        if(this->mWidthMod8!=0) ++this->mBytesPerRowMinusPad;
        this->CalcBytesPerRow();
        this->mRowPad=this->mBytesPerRow-this->mBytesPerRowMinusPad;
        if(this->mWidthMod8!=0) ++this->mRowPad;
        this->mEndRowMask=1<<(8-this->mWidthMod8);
        this->mImageData=new unsigned char[this->mBytesPerRow*this->mHeight+1];
        this->mImageEnd=this->mImageData+this->mBytesPerRow*this->mHeight;
    }
    ~RawImage1() { delete[] mImageData; }
    unsigned char* StartRowTraversal(int row) {
        this->mRowMask=128;
        this->mRowPtr=this->mImageData+row*this->mBytesPerRow;
        this->mRowEnd=this->mRowPtr+this->mCompleteBytesPerRow;
        return this->mRowPtr;
    }
    void NextRowPixel() { this->mRowMask>>=1; if(this->mRowMask==0) { this->mRowPtr++; this->mRowMask = 128; } }
    void SetRowPixel(bool on) { if(on) *this->mRowPtr |= this->mRowMask; else *this->mRowPtr &= ~mRowMask; }
    size_t Bytes() { return (size_t)(mBytesPerRow * mHeight); }
};
struct RawImage32 {
    int mWidth, mHeight;
    DWORD* mImageData;
    DWORD *mRowPtr, *mRowEnd;
    RawImage32(int w, int h) : mWidth(w), mHeight(h) { mImageData = new DWORD[(size_t)(w * h + 1)]; }
    ~RawImage32() { delete[] mImageData; }
    int GetWidth() { return mWidth; }
    int GetHeight() { return mHeight; }
    DWORD* StartRowTraversal(int row) {
        this->mRowPtr=this->mImageData+row*this->mWidth;
        this->mRowEnd=this->mRowPtr+this->mWidth;
        return this->mRowPtr;
    }
};
struct RawImage8 {
    int mWidth, mHeight, mBytesPerRow, mTransparentColor;
    unsigned char* mImageData;
    unsigned char *mRowPtr, *mRowEnd;
    RawImage8(int w, int h) : mWidth(w), mHeight(h)
    {
        mBytesPerRow = w;
        if (mBytesPerRow % 4 != 0) mBytesPerRow += 4 - mBytesPerRow % 4;
        mImageData = new unsigned char[(size_t)(mBytesPerRow * h + 1)];
    }
    ~RawImage8() { delete[] mImageData; }
    unsigned char* StartRowTraversal(int row) {
        this->mRowPtr=this->mImageData+row*this->mBytesPerRow;
        this->mRowEnd=this->mRowPtr+this->mWidth;
        return this->mRowPtr;
    }
    unsigned char GetRowPixel() { return *this->mRowPtr; }
    void NextRowPixel() { ++this->mRowPtr; }
};

// ---- the original statements (Image.cpp's GetNative loops before the port; `this` the image)
namespace orig {
static void Mask32(RawImage32* t, RawImage1* mask)
{
#define this t
        int width=this->GetWidth(),height=this->GetHeight();
        for(int y=0;y<height;++y) {
            mask->StartRowTraversal(y);
            this->StartRowTraversal(y);
            while(this->mRowPtr<this->mRowEnd) {
                if((*this->mRowPtr&0xff000000)==0) {
                    *this->mRowPtr=0;
                    mask->SetRowPixel(true);
                } else mask->SetRowPixel(false);
                ++this->mRowPtr;
                mask->NextRowPixel();
            }
        }
#undef this
}
static void Mask8(RawImage8* t, RawImage1* mask)
{
#define this t
        for(int y=0;y<this->mHeight;++y) {
            this->StartRowTraversal(y);
            mask->StartRowTraversal(y);
            while(this->mRowPtr<this->mRowEnd) {
                mask->SetRowPixel(this->GetRowPixel()==this->mTransparentColor);
                mask->NextRowPixel();
                this->NextRowPixel();
            }
        }
#undef this
}
}

static uint64_t Fnv(const unsigned char* p, size_t n, uint64_t h = 1469598103934665603ull)
{
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    int aFailures = 0;
    const int kImages = 200000;
    for (int n = 0; n < kImages; n++) {
        sSeed = 4049u * (unsigned)n + 3u;
        bool wide = R(2) != 0;
        int w = (int)R(41), h = (int)R(21);
        int transparent = R(4) == 0 ? (int)R(303) - 2 : (int)R(4);
        unsigned seed = Rand() * 65536u + Rand();
        RawImage1 ma(w, h), mb(w, h);
        unsigned s = seed;
        for (size_t i = 0; i < ma.Bytes() + 1; i++) { s = s * 1103515245u + 12345u; ma.mImageData[i] = mb.mImageData[i] = (unsigned char)(s >> 16); }
        bool ok;
        uint64_t hash;
        if (wide) {
            RawImage32 a(w, h), b(w, h);
            for (int i = 0; i < w * h; i++) {
                s = s * 1103515245u + 12345u;
                DWORD v = s ^ (s << 11);
                if ((s >> 7) % 3 == 0) v &= 0x00ffffff;
                a.mImageData[i] = b.mImageData[i] = v;
            }
            orig::Mask32(&a, &ma);
            ee_won_imgf_mask32(b.mImageData, w, h, mb.mImageData, mb.mBytesPerRow);
            ok = memcmp(a.mImageData, b.mImageData, (size_t)(w * h) * 4) == 0 && memcmp(ma.mImageData, mb.mImageData, ma.Bytes()) == 0;
            hash = Fnv(ma.mImageData, ma.Bytes(), Fnv((const unsigned char*)a.mImageData, (size_t)(w * h) * 4));
        } else {
            RawImage8 a(w, h), b(w, h);
            a.mTransparentColor = b.mTransparentColor = transparent;
            for (int i = 0; i < a.mBytesPerRow * h; i++) {
                s = s * 1103515245u + 12345u;
                a.mImageData[i] = b.mImageData[i] = (unsigned char)((s >> 9) % 3 == 0 ? (unsigned)transparent : s >> 16);
            }
            orig::Mask8(&a, &ma);
            ee_won_imgf_mask8(b.mImageData, w, h, b.mBytesPerRow, transparent, mb.mImageData, mb.mBytesPerRow);
            ok = memcmp(ma.mImageData, mb.mImageData, ma.Bytes()) == 0;
            hash = Fnv(ma.mImageData, ma.Bytes());
        }
        if (!ok && ++aFailures < 20) printf("FAIL %d %d %d %d %d\n", n, wide ? 32 : 8, w, h, transparent);
        if (aPrint && n < 300) printf("%d %d %d %d %u = %016llx\n", wide ? 32 : 8, w, h, transparent, seed, (unsigned long long)hash);
    }
    if (aPrint) return 0;
    printf("won_imgmask_test: %d images, %d failures\n", kImages, aFailures);
    return aFailures ? 1 : 0;
}
