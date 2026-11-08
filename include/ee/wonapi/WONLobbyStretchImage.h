// ImageFilter (8 bytes), StretchImageFilter (0x24 bytes) and StretchImage (0x30 bytes).
#ifndef EE_WONAPI_WONLOBBYSTRETCHIMAGE_H
#define EE_WONAPI_WONLOBBYSTRETCHIMAGE_H
#include "ee/wonapi/WONLobbyImageHelpers.h"
namespace WONAPI {
class ImageFilter : public RefCount {
public:
    virtual SmartPtr<RawImage> Filter(RawImage *theImage) { return theImage; } // returns the image unchanged (slot 4)
};
class StretchImageFilter : public ImageFilter {
public:
    int mWidth,mHeight; //08,0c
    int mLeftWidth,mRightWidth,mTopHeight,mBottomHeight; //10..1c
    bool mTileHorz,mTileVert,mSkipCenter; // offsets 0x20..0x22; class size 0x24
    StretchImageFilter(); //349540
    virtual SmartPtr<RawImage> Filter(RawImage*); // pixel algorithm at 0x00749570
    void SetSize(int w,int h) { this->mWidth=w; this->mHeight=h; }
    void SetBorders(int l,int t,int r,int b) { this->mLeftWidth=l; this->mRightWidth=r; this->mTopHeight=t; this->mBottomHeight=b; }
    void SetTile(bool h,bool v) { this->mTileHorz=h; this->mTileVert=v; }
    void SetSkipCenter(bool b) { this->mSkipCenter=b; }
};
class StretchImage : public ProgramImage {
private:
    SmartPtr<RawImage> mImage; //10
    SmartPtr<NativeImage> mCachedImage; //14
    bool mNeedRecalc; //18
    int mLeftWidth,mRightWidth,mTopHeight,mBottomHeight; //1c..28
    bool mTileHorz,mTileVert,mSkipCenter; // offsets 0x2c..0x2e; class size 0x30
    void CalcImage(DisplayContext*); //349810
protected:
    virtual ~StretchImage(); //349740
public:
    StretchImage(RawImage*,int,int,int,int); //349650
    int GetLeft() { return this->mLeftWidth; } // inline, member at +0x1c
    int GetRight() { return this->mRightWidth; } // inline, member at +0x20
    int GetTop() { return this->mTopHeight; } // inline, member at +0x24
    int GetBottom() { return this->mBottomHeight; } // inline, member at +0x28
    virtual void Paint(Graphics&); //3499a0
    virtual SmartPtr<Image> Duplicate(); //349b80
    virtual SmartPtr<NativeImage> GetNative(DisplayContext*); //349c60
    virtual void SetSize(int,int); //3497e0
    void SetImage(RawImage*); //349ab0
    void SetBorders(int,int,int,int); //349b10
    void CopyAttributes(StretchImage*); //349b30
    void SetTile(bool h,bool v) { this->mTileHorz=h; this->mTileVert=v; }
    void SetSkipCenter(bool b) { this->mSkipCenter=b; }
};
class StretchCenterImage : public ProgramImage {
protected:
    SmartPtr<Image> mStretchImage,mCenterImage; // offsets 0x10, 0x14; class size 0x18
public:
    StretchCenterImage(Image* stretch=NULL,Image* center=NULL) : mStretchImage(stretch),mCenterImage(center) {}
    void SetStretchImage(Image* image) { this->mStretchImage=image; } // inline (used at 0x0078c13a)
    void SetCenterImage(Image* image) { this->mCenterImage=image; } // inline (used at 0x0078c1dd)
    virtual void Paint(Graphics&); //349cb0
    virtual SmartPtr<Image> Duplicate(); //349d40
    virtual void SetSize(int,int); //349d20
};
}
#endif
