#ifndef EE_WONAPI_WONLOBBYCONFIGIMAGES_H
#define EE_WONAPI_WONLOBBYCONFIGIMAGES_H
#include "ee/wonapi/WONLobbyStretchImage.h"
namespace WONAPI {

// Image filters; the fields agree with the original's config allocations (0x1c/0x14/0x18/0x14
// bytes) and their writes; virtual slots 0 and 4.
class MultiImageFilter : public ImageFilter {
protected:
    typedef std::list<SmartPtr<ImageFilter> > FilterList;
    FilterList mFilterList;
public:
    MultiImageFilter() {}
    MultiImageFilter* Add(ImageFilter*);
    virtual SmartPtr<RawImage> Filter(RawImage*);
};

class ScaleImageFilter : public ImageFilter {
public:
    int mWidth,mHeight; //08/0c
    int mPercentWidth,mPercentHeight; //10/14
    bool mIsPercent; //18 naturalpad, total1c
    ScaleImageFilter(int percent=100) { this->SetPercentSize(percent,percent); }
    ScaleImageFilter(int w,int h) { this->SetAbsSize(w,h); }
    void SetAbsSize(int,int); //348b80
    void SetPercentSize(int,int); //348b60
    bool CheckSize(Image*); // at 0x00748ba0
    virtual SmartPtr<RawImage> Filter(RawImage*); // at 0x00748c30
};
class TintImageFilter : public ImageFilter {
public:
    int mRed,mGreen,mBlue; //08/0c/10 total14
    TintImageFilter(int r,int g,int b) : mRed(r),mGreen(g),mBlue(b) {}
    virtual SmartPtr<RawImage> Filter(RawImage*); // at 0x00748dd0
};
class CropImageFilter : public ImageFilter {
public:
    int mx,my,mWidth,mHeight; //08/0c/10/14 total18
    CropImageFilter(int x=0,int y=0,int w=0,int h=0) : mx(x),my(y),mWidth(w),mHeight(h) {}
    void SetPos(int x,int y) { this->mx=x;this->my=y; }
    void SetSize(int w,int h) { this->mWidth=w;this->mHeight=h; }
    int GetX() { return this->mx; }
    int GetY() { return this->my; }
    int GetWidth() { return this->mWidth; }
    int GetHeight() { return this->mHeight; }
    virtual SmartPtr<RawImage> Filter(RawImage*); // at 0x00748a60
};
class TrimImageFilter : public CropImageFilter {
public:
    TrimImageFilter() {}
    virtual SmartPtr<RawImage> Filter(RawImage*);
};

class TransparentImageFilter : public ImageFilter {
public:
    enum PixelSample { UpperLeft,UpperRight,LowerLeft,LowerRight };
    DWORD mTransparentColor; //08
    PixelSample mPixelSample; //0c
    bool mUseSample; //10 naturalpad,total14
    TransparentImageFilter(DWORD color) { this->mUseSample=false;this->mTransparentColor=color; }
    TransparentImageFilter(PixelSample sample) { this->mUseSample=true;this->mPixelSample=sample; }
    void SetPixelSample(PixelSample theSample) { this->mUseSample = true; this->mPixelSample = theSample; }
    void SetTransparentColor(DWORD theColor) { this->mUseSample = false; this->mTransparentColor = theColor; }
    virtual SmartPtr<RawImage> Filter(RawImage*); // at 0x00749110
};

// Image configuration classes. ImageFilterConfig is constructed at 0x0078c260 and destroyed at
// 0x0078c2b0: ImageConfig (0x7c) + raw image pointer at +0x7c + bool at +0x80 = 0x84 bytes.
class ImageConfig : public ComponentConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078b130
    static Component* CompFactory(); // at 0x006c7b10
    ImageConfig(); // at 0x0078b2b0
    // Implicit virtual destructor: releases the owned members, then the base.
    virtual bool PostParse(ConfigParser*); // at 0x007099f0
    virtual bool Virtual24(const std::string&); //38b3e0
    virtual bool GetNamedValue(const std::string&,int&); //38b370
    virtual Image* GetImage() { return this->mImage.get(); } // slot 0x38, at 0x0078b2d0
    virtual RawImage* GetRawImage() { return NULL; } // slot 0x3c (base behaviour), at 0x006c7b10
    SmartPtr<Image> mImage; //78, total7c
};
// 0x80-byte configs; the raw pointer at +0x7c aliases the inherited image pointer at +0x78.
class StretchImageConfig : public ImageConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078b190
    StretchImageConfig(); //38b4e0
    // Implicit virtual destructor: releases the owned members, then the base.
    virtual bool Virtual24(const std::string&); //38b630
    StretchImage* mStretchImage; // offset 0x7c, raw alias; class size 0x80
};
class StretchCenterImageConfig : public ImageConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078b1f0
    StretchCenterImageConfig(); //38bf90
    // Implicit virtual destructor: releases the owned members, then the base.
    virtual bool Virtual24(const std::string&); //38c0e0
    StretchCenterImage* mStretchCenterImage; // offset 0x7c, raw alias; class size 0x80
};
class ImageFilterConfig : public ImageConfig {
public:
    static ComponentConfig* CfgFactory(); // at 0x0078b250
    ImageFilterConfig(); // at 0x0078c260
    // Implicit virtual destructor: releases the owned members, then the base.
    virtual bool Virtual24(const std::string&); //38c780
    virtual Image* GetImage(); //38cc80 slot38
    virtual RawImage* GetRawImage() { return this->mRawImage.get(); } // slot 0x3c, at 0x0078c280
    SmartPtr<RawImage> HandleFilter(); //38c340
    void SetRawImage(RawImage*); //38c6e0
    SmartPtr<RawImage> mRawImage; //7c
    bool mImageIsRawImage; //80 naturalpad, total84
};
}
#endif
