// Image filters: crop, scale, tint, transparent colour, plus filter chains and transparent-border trimming (R6 WON
// round 36: the pixel loops are rust/crates/ee_won_gui/src/imgfilter.rs; the C++ keeps the filters, the new images and
// the dispatch on the image's type)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <cstring>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyConfigImages.h"
#include "rust/won_gui/imgfilter.h"
namespace WONAPI {
static_assert(sizeof(DWORD)==4,"the pixels and colours are 32-bit");
static_assert(TransparentImageFilter::UpperRight==1 && TransparentImageFilter::LowerRight==3,"imgfilter.rs sample_place");

// a raw image's pixels as imgfilter.rs walks them
static EeWonRawPixels Px(RawImage* image)
{
    EeWonRawPixels p;
    p.kind=image->mType; p.width=image->mWidth; p.height=image->mHeight;
    if(image->mType==0) { p.data=static_cast<RawImage1*>(image)->mImageData; p.pitch=static_cast<RawImage1*>(image)->mBytesPerRow; }
    else if(image->mType==1) { p.data=static_cast<RawImage8*>(image)->mImageData; p.pitch=static_cast<RawImage8*>(image)->mBytesPerRow; }
    else { p.data=(uint8_t*)static_cast<RawImage32*>(image)->mImageData; p.pitch=image->mWidth*4; }
    return p;
}
static inline void CopyInfo(RawImage1*,RawImage1*) {}
static inline void CopyInfo(RawImage8* dst,RawImage8* src) { dst->mPalette=src->mPalette;dst->mTransparentColor=src->mTransparentColor; }
static inline void CopyInfo(RawImage32* dst,RawImage32* src) { dst->mDoTransparency=src->mDoTransparency; }

// Returns a new image holding a rectangle copied row by row from a raw image, with its palette or transparency
// settings (instances: 0x00752220 8-bit, 0x00752380 32-bit)
template<class ImageType,class PixelType>
static SmartPtr<ImageType> CropPixels(ImageType* src,int x,int y,int width,int height)
{
    SmartPtr<ImageType> dst=new ImageType(width,height);
    CopyInfo(dst.get(),src);
    EeWonRawPixels s=Px(src),d=Px(dst.get());
    ee_won_imgf_crop(&s,&d,x*(int)sizeof(PixelType),y,width*(int)sizeof(PixelType),height);
    return dst;
}

// Returns the filter's rectangle cut out of an 8- or 32-bit image; NULL for other image types
SmartPtr<RawImage> CropImageFilter::Filter(RawImage* image)
{
    switch(image->mType)
    {
    case 1:return CropPixels<RawImage8,unsigned char>(static_cast<RawImage8*>(image),this->mx,this->my,this->mWidth,this->mHeight).get();
    case 2:return CropPixels<RawImage32,DWORD>(static_cast<RawImage32*>(image),this->mx,this->my,this->mWidth,this->mHeight).get();
    }
    return NULL;
}

// Resolves the target size (percent of the image when set, at least 1x1); true if it equals the image size
bool ScaleImageFilter::CheckSize(Image* image)
{
    return ee_won_imgf_check_size(this->mIsPercent,this->mPercentWidth,this->mPercentHeight,&this->mWidth,&this->mHeight,
                                  image->mWidth,image->mHeight);
}

// Returns a nearest-neighbour scaled copy of a raw image, with its palette or transparency settings (instances:
// 0x007524b0 1-bit, 0x00752690 8-bit, 0x00752820 32-bit)
template<class ImageType>
static SmartPtr<ImageType> ScalePixels(ImageType* src,int width,int height)
{
    SmartPtr<ImageType> dst=new ImageType(width,height);
    CopyInfo(dst.get(),src);
    EeWonRawPixels s=Px(src),d=Px(dst.get());
    ee_won_imgf_scale(&s,&d);
    return dst;
}

// Returns the image scaled to the filter's size; the image itself when the size already matches or the type is
// unknown
SmartPtr<RawImage> ScaleImageFilter::Filter(RawImage* image)
{
    if(this->CheckSize(image)) return image;
    switch(image->mType)
    {
    case 0:return ScalePixels<RawImage1>(static_cast<RawImage1*>(image),this->mWidth,this->mHeight).get();
    case 1:return ScalePixels<RawImage8>(static_cast<RawImage8*>(image),this->mWidth,this->mHeight).get();
    case 2:return ScalePixels<RawImage32>(static_cast<RawImage32*>(image),this->mWidth,this->mHeight).get();
    default:return image;
    }
}

// Tints the image in place by per-channel percentages (8-bit: on a duplicated palette; 32-bit: every pixel, alpha
// kept) and returns it
SmartPtr<RawImage> TintImageFilter::Filter(RawImage* image)
{
    if(image->mType==1)
    {
        RawImage8* raw=static_cast<RawImage8*>(image);
        SmartPtr<Palette> palette=raw->mPalette->Duplicate();
        ee_won_imgf_tint((uint32_t*)palette->mColors,palette->mNumColors>0?(size_t)palette->mNumColors:0,this->mRed,this->mGreen,this->mBlue,false);
        raw->mPalette=palette;
    }
    else if(image->mType==2)
        ee_won_imgf_tint((uint32_t*)static_cast<RawImage32*>(image)->mImageData,(size_t)image->mWidth*image->mHeight,this->mRed,this->mGreen,this->mBlue,true);
    return image;
}

// Makes one colour transparent (fixed, or sampled from a corner pixel) and returns the image; 8-bit images get a
// transparent palette index, 32-bit pixels get alpha 0 or opaque
SmartPtr<RawImage> TransparentImageFilter::Filter(RawImage* image)
{
    if(image->mType==0) return image;
    DWORD color=this->mTransparentColor;
    if(this->mUseSample)
    {
        if(image->mHeight<=0 || image->mWidth<=0) return image;
        int x,y;
        ee_won_imgf_sample((int)this->mPixelSample,image->mWidth,image->mHeight,&x,&y);
        if(image->mType==1)
        {
            RawImage8* raw=static_cast<RawImage8*>(image);
            raw->mTransparentColor=raw->GetPixel(x,y);
            return image;
        }
        if(image->mType==2) color=static_cast<RawImage32*>(image)->GetPixel(x,y);
    }
    if(image->mType==1)
    {
        RawImage8* raw=static_cast<RawImage8*>(image);
        Palette* palette=raw->mPalette.get();
        int i=ee_won_imgf_palette_find((const uint32_t*)palette->mColors,palette->mNumColors>0?(size_t)palette->mNumColors:0,color);
        if(i>=0) raw->mTransparentColor=i;
    }
    else if(image->mType==2)
    {
        static_cast<RawImage32*>(image)->mDoTransparency=true;
        ee_won_imgf_transparent32((uint32_t*)static_cast<RawImage32*>(image)->mImageData,(size_t)image->mWidth*image->mHeight,color);
    }
    return image;
}

// Sets the scale target as a percentage of the source image size
void ScaleImageFilter::SetPercentSize(int thePercentWidth, int thePercentHeight)
{
	this->mPercentWidth = thePercentWidth;
	this->mPercentHeight = thePercentHeight;
	this->mIsPercent = true;
}

// Sets the scale target to a fixed size in pixels
void ScaleImageFilter::SetAbsSize(int theWidth, int theHeight)
{
	this->mWidth = theWidth;
	this->mHeight = theHeight;
	this->mIsPercent = false;
}

// Filter chains and transparent-border trimming (not identified in the original exe)
MultiImageFilter* MultiImageFilter::Add(ImageFilter *theFilter)
{
	this->mFilterList.push_back(theFilter);
	return this;
}

SmartPtr<RawImage> MultiImageFilter::Filter(RawImage *theImage)
{
	FilterList::iterator anItr = this->mFilterList.begin();
	SmartPtr<RawImage> anImage = theImage;
	while(anItr!=this->mFilterList.end())
	{
		ImageFilter *aFilter = *anItr;
		anImage = aFilter->Filter(anImage);
		++anItr;
	}
	return anImage;
}

// Crops the image to the smallest rectangle holding every pixel that is not transparent (the search is
// imgfilter.rs trim); NULL when every pixel is transparent or the type has no transparency to trim
SmartPtr<RawImage> TrimImageFilter::Filter(RawImage *theImage)
{
	this->mx = this->my = 0;
	this->mWidth = theImage->mWidth;
	this->mHeight = theImage->mHeight;

	if(!theImage->HasTransparency())
		return theImage;
	if(theImage->mType!=1 && theImage->mType!=2)
		return NULL;
	EeWonRawPixels p=Px(theImage);
	unsigned char transparentPixel=theImage->mType==1 ? (unsigned char)static_cast<RawImage8*>(theImage)->mTransparentColor : 0;
	int rect[4];
	if(!ee_won_imgf_trim(&p,transparentPixel,rect))
		return NULL;
	this->mx = rect[0]; this->my = rect[1];
	this->mWidth = rect[2]; this->mHeight = rect[3];
	return CropImageFilter::Filter(theImage);
}
}
