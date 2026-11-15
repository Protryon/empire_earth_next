// ImageFilter (8 bytes), StretchImageFilter (0x24 bytes) and StretchImage (0x30 bytes).
#ifndef EE_WONAPI_WONLOBBYSTRETCHIMAGE_H
#define EE_WONAPI_WONLOBBYSTRETCHIMAGE_H
#include "ee/wonapi/WONLobbyImageHelpers.h"
#include "rust/ee_won.h"
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
    StretchImageFilter() : mWidth(0),mHeight(0),mLeftWidth(0),mRightWidth(0),mTopHeight(0),mBottomHeight(0),
        mTileHorz(true),mTileVert(true),mSkipCenter(false) {} //349540
    virtual SmartPtr<RawImage> Filter(RawImage*); // pixel algorithm at 0x00749570
    void SetSize(int w,int h) { this->mWidth=w; this->mHeight=h; }
    void SetBorders(int l,int t,int r,int b) { this->mLeftWidth=l; this->mRightWidth=r; this->mTopHeight=t; this->mBottomHeight=b; }
    void SetTile(bool h,bool v) { this->mTileHorz=h; this->mTileVert=v; }
    void SetSkipCenter(bool b) { this->mSkipCenter=b; }
};
// StretchImage (R6 WON round 38): the borders, tiling, skip-centre and rebuild flag are the Rust object's
// (rust/crates/ee_won/src/stretchobj.rs); the C++ keeps the source and cached images and the Image's size.
class StretchImage : public ProgramImage {
private:
    SmartPtr<RawImage> mImage; //10
    SmartPtr<NativeImage> mCachedImage; //14
    EeWonStretchImg* mObj;
    void CalcImage(DisplayContext*); //349810
    StretchImage(RawImage* i,EeWonStretchImg* o) : mImage(i),mObj(o) { this->mWidth=this->mHeight=0; }
    int Get(EeWonStretchField f) { return ee_won_stretch_get(this->mObj,f); }
protected:
    virtual ~StretchImage() { ee_won_stretch_free(this->mObj); } //349740
public:
    StretchImage(RawImage* i,int l,int t,int r,int b) : mImage(i),mObj(ee_won_stretch_new(l,t,r,b)) { this->mWidth=this->mHeight=0; } //349650
    int GetLeft() { return this->Get(EeWonStretchField::Left); } // inline, member at +0x1c
    int GetRight() { return this->Get(EeWonStretchField::Right); } // inline, member at +0x20
    int GetTop() { return this->Get(EeWonStretchField::Top); } // inline, member at +0x24
    int GetBottom() { return this->Get(EeWonStretchField::Bottom); } // inline, member at +0x28
    virtual void Paint(Graphics&); //3499a0
    virtual SmartPtr<Image> Duplicate() { return new StretchImage(this->mImage.get(),ee_won_stretch_duplicate(this->mObj)); } //349b80
    virtual SmartPtr<NativeImage> GetNative(DisplayContext* c) { if(this->Get(EeWonStretchField::NeedRecalc)) this->CalcImage(c); return this->mCachedImage; } //349c60
    virtual void SetSize(int w,int h) { if(ee_won_stretch_set_size(this->mObj,this->mWidth,this->mHeight,w,h)) { this->mWidth=w; this->mHeight=h; } } //3497e0
    void SetImage(RawImage* i) { this->mImage=i; } //349ab0 (the cache not marked)
    void SetBorders(int l,int t,int r,int b) { ee_won_stretch_set_borders(this->mObj,l,t,r,b); } //349b10
    void CopyAttributes(StretchImage* f) { ee_won_stretch_copy(this->mObj,f->mObj); this->mWidth=f->mWidth; this->mHeight=f->mHeight; } //349b30
    void SetTile(bool h,bool v) { ee_won_stretch_set_tile(this->mObj,h,v); }
    void SetSkipCenter(bool b) { ee_won_stretch_set_skip(this->mObj,b); }
};
class StretchCenterImage : public ProgramImage {
protected:
    SmartPtr<Image> mStretchImage,mCenterImage; // offsets 0x10, 0x14; class size 0x18
public:
    StretchCenterImage(Image* stretch=NULL,Image* center=NULL) : mStretchImage(stretch),mCenterImage(center) {}
    void SetStretchImage(Image* image) { this->mStretchImage=image; } // inline (used at 0x0078c13a)
    void SetCenterImage(Image* image) { this->mCenterImage=image; } // inline (used at 0x0078c1dd)
    virtual void Paint(Graphics&); //349cb0
    virtual SmartPtr<Image> Duplicate() { return new StretchCenterImage(this->mStretchImage->Duplicate().get(),this->mCenterImage.get()); } //349d40
    virtual void SetSize(int w,int h) { this->mWidth=w; this->mHeight=h; this->mStretchImage->SetSize(w,h); } //349d20
};
}
#endif
