// Image filters: crop, scale, tint, transparent colour, plus filter chains and transparent-border trimming
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
namespace WONAPI {
static inline void NewRaw(SmartPtr<RawImage8>& image,int width,int height) { image=new RawImage8(width,height,NULL); }
static inline void NewRaw(SmartPtr<RawImage32>& image,int width,int height) { image=new RawImage32(width,height); }
static inline void CopyInfo(RawImage8* dst,RawImage8* src) { dst->mPalette=src->mPalette;dst->mTransparentColor=src->mTransparentColor; }
static inline void CopyInfo(RawImage32* dst,RawImage32* src) { dst->mDoTransparency=src->mDoTransparency; }

// FUNCTION: WON 0x00752220
// FUNCTION: WON 0x00752380
// Returns a new image holding a rectangle copied row by row from a raw image, with its palette or
// transparency settings (instances: 0x00752220 8-bit, 0x00752380 32-bit)
template<class ImageType,class PixelType>
static SmartPtr<ImageType> CropPixels(ImageType* src,int x,int y,int width,int height)
{
    SmartPtr<ImageType> dst=new ImageType(width,height);
    CopyInfo(dst.get(),src);
    for(int row=0;row<height;++row)
    {
        PixelType* from=src->StartRowTraversal(row+y)+x;
        PixelType* to=dst->StartRowTraversal(row);
        memcpy(to,from,sizeof(PixelType)*width);
    }
    return dst;
}

// FUNCTION: WON 0x00748a60
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

// FUNCTION: WON 0x00748ba0
// Resolves the target size (percent of the image when set, at least 1x1); true if it equals the image size
bool ScaleImageFilter::CheckSize(Image* image)
{
    if(this->mIsPercent)
    {
        this->mWidth=image->mWidth*this->mPercentWidth/100;
        this->mHeight=image->mHeight*this->mPercentHeight/100;
    }
    if(this->mWidth<=0) this->mWidth=1;
    if(this->mHeight<=0) this->mHeight=1;
    if(this->mWidth==image->mWidth && this->mHeight==image->mHeight)
        return true;

    return false;
}

// FUNCTION: WON 0x00752690
// FUNCTION: WON 0x00752820
// Returns a nearest-neighbour scaled copy of a raw image, with its palette or transparency settings
// (instances: 0x00752690 8-bit, 0x00752820 32-bit)
template<class ImageType,class PixelType>
static SmartPtr<ImageType> ScalePixels(ImageType* src,int width,int height)
{
    SmartPtr<ImageType> dst=new ImageType(width,height);
    CopyInfo(dst.get(),src);
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

static inline bool CurrentBit(RawImage1* image) { return (*image->mRowPtr&image->mRowMask)!=0; }
static inline void NextBit(RawImage1* image)
{
    image->mRowMask>>=1;
    if(image->mRowMask==0) { image->mRowMask=0x80;++image->mRowPtr; }
}
static inline void PutBit(RawImage1* image,bool bit)
{
    if(bit) *image->mRowPtr|=image->mRowMask;
    else *image->mRowPtr&=~image->mRowMask;
    NextBit(image);
}

// FUNCTION: WON 0x007524b0
// Returns a nearest-neighbour scaled copy of a 1-bit image
static SmartPtr<RawImage1> ScaleBits(RawImage1* src,int width,int height)
{
    SmartPtr<RawImage1> dst=new RawImage1(width,height);
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

// FUNCTION: WON 0x00748c30
// Returns the image scaled to the filter's size; the image itself when the size already matches or
// the type is unknown
SmartPtr<RawImage> ScaleImageFilter::Filter(RawImage* image)
{
    if(this->CheckSize(image)) return image;
    switch(image->mType)
    {
    case 0:
        return ScaleBits(static_cast<RawImage1*>(image),this->mWidth,this->mHeight).get();
    case 1:return ScalePixels<RawImage8,unsigned char>(static_cast<RawImage8*>(image),this->mWidth,this->mHeight).get();
    case 2:return ScalePixels<RawImage32,DWORD>(static_cast<RawImage32*>(image),this->mWidth,this->mHeight).get();
    default:return image;
    }
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

// FUNCTION: WON 0x00748dd0
// Tints the image in place by per-channel percentages (8-bit: on a duplicated palette; 32-bit: every
// pixel, alpha kept) and returns it
SmartPtr<RawImage> TintImageFilter::Filter(RawImage* image)
{
    if(image->mType==0) return image;
    if(image->mType==1)

    {
        RawImage8* raw=static_cast<RawImage8*>(image);
        SmartPtr<Palette> palette=raw->mPalette->Duplicate();
        for(int i=0;i<palette->mNumColors;++i)
            palette->mColors[i]=TintColor(palette->mColors[i],this->mRed,this->mGreen,this->mBlue);
        raw->mPalette=palette;
        return image;
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
                raw->SetRowPixel((color&0xff000000)|TintColor(color,this->mRed,this->mGreen,this->mBlue));
                raw->NextRowPixel();
            }
            raw->NextRow();
        }
        return image;
    }
    return image;
}

// FUNCTION: WON 0x00749110
// Makes one colour transparent (fixed, or sampled from a corner pixel) and returns the image; 8-bit
// images get a transparent palette index, 32-bit pixels get alpha 0 or opaque
SmartPtr<RawImage> TransparentImageFilter::Filter(RawImage* image)
{
    if(image->mType==0) return image;
    DWORD color=this->mTransparentColor;
    if(this->mUseSample)
    {
        if(image->mHeight<=0 || image->mWidth<=0) return image;
        int x=0,y=0;
        switch(this->mPixelSample)
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
            return image;
        }
        if(image->mType==2) color=static_cast<RawImage32*>(image)->GetPixel(x,y);
    }
    if(image->mType==1)
    {
        RawImage8* raw=static_cast<RawImage8*>(image);
        Palette* palette=raw->mPalette.get();
        for(int i=0;i<palette->mNumColors;++i)
            if(palette->mColors[i]==color) { raw->mTransparentColor=i;break; }
        return image;
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
        return image;
    }
    return image;
}
}


namespace WONAPI {
// FUNCTION: WON 0x00748b60
// Sets the scale target as a percentage of the source image size
void ScaleImageFilter::SetPercentSize(int thePercentWidth, int thePercentHeight)
{
	this->mPercentWidth = thePercentWidth;
	this->mPercentHeight = thePercentHeight;
	this->mIsPercent = true;
}

// FUNCTION: WON 0x00748b80
// Sets the scale target to a fixed size in pixels
void ScaleImageFilter::SetAbsSize(int theWidth, int theHeight)
{
	this->mWidth = theWidth;
	this->mHeight = theHeight;
	this->mIsPercent = false;
}
}


// Filter chains and transparent-border trimming (not identified in the original exe)
namespace WONAPI {
MultiImageFilter* MultiImageFilter::Add(ImageFilter *theFilter)
{
	this->mFilterList.push_back(theFilter);
	return this;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////

static bool TrimFilter_IsRowEmpty(unsigned char *pixels, unsigned char transparentPixel, int width)
{
	for(int i=0; i<width; i++)
		if(*pixels++!=transparentPixel)
			return false;

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static bool TrimFilter_IsRowEmpty(DWORD *pixels, int width)
{
	for(int i=0; i<width; i++)
		if((*pixels++)&0xff000000)
			return false;

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static bool TrimFilter_IsColEmpty(unsigned char *pixels, unsigned char transparentPixel, int height, int rowInc)
{
	for(int i=0; i<height; i++, pixels+=rowInc)
	{
		if(*pixels!=transparentPixel)
			return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static bool TrimFilter_IsColEmpty(DWORD *pixels, int height, int rowInc)
{
	for(int i=0; i<height; i++, pixels+=rowInc)
	{
		if((*pixels)&0xff000000)
			return false;
	}

	return true;
}



///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SmartPtr<RawImage> TrimImageFilter::Filter(RawImage *theImage)
{
	this->mx = this->my = 0;
	this->mWidth = theImage->mWidth;
	this->mHeight = theImage->mHeight;

	if(!theImage->HasTransparency())
		return theImage;

	int x,y;
	int left = 0,top = 0,right = this->mWidth-1, bottom = this->mHeight-1;
	if(theImage->mType==1)
	{
		RawImage8 *anImage = static_cast<RawImage8*>(theImage);
		unsigned char transparentPixel = anImage->mTransparentColor;
		unsigned char *pixels = anImage->mImageData;
		int bytesPerRow = anImage->mBytesPerRow;

		// Find Top
		for(y=0; y<this->mHeight; y++, pixels+=bytesPerRow)
		{
			if(!TrimFilter_IsRowEmpty(pixels,transparentPixel,this->mWidth))
				break;
		}
		if(y>=this->mHeight)
			return NULL;

		top = y;

		// Find Bottom
		pixels = anImage->mImageData + (this->mHeight-1)*bytesPerRow;
		for(y=bottom; y>top; y--, pixels-=bytesPerRow)
		{
			if(!TrimFilter_IsRowEmpty(pixels,transparentPixel,this->mWidth))
				break;
		}
		bottom = y;
		this->mHeight = bottom-top+1;

		// Find Left
		pixels = anImage->mImageData + top*bytesPerRow;
		for(x=0; x<this->mWidth; x++, pixels++)
		{
			if(!TrimFilter_IsColEmpty(pixels,transparentPixel,this->mHeight,bytesPerRow))
				break;
		}
		if(x>=this->mWidth)
			return NULL;

		left = x;
		
		// Find Right
		pixels = anImage->mImageData + top*bytesPerRow + (this->mWidth-1);
		for(x=right; x>left; x--, pixels--)
		{
			if(!TrimFilter_IsColEmpty(pixels,transparentPixel,this->mHeight,bytesPerRow))
				break;
		}
		right = x;
		this->mWidth = right-left+1;
	}
	else if(theImage->mType==2)
	{
		RawImage32 *anImage = static_cast<RawImage32*>(theImage);
		DWORD *pixels = anImage->mImageData;
		int bytesPerRow = this->mWidth;

		// Find Top
		for(y=0; y<this->mHeight; y++, pixels+=bytesPerRow)
		{
			if(!TrimFilter_IsRowEmpty(pixels,this->mWidth))
				break;
		}
		if(y>=this->mHeight)
			return NULL;

		top = y;

		// Find Bottom
		pixels = anImage->mImageData + (this->mHeight-1)*bytesPerRow;
		for(y=bottom; y>top; y--, pixels-=bytesPerRow)
		{
			if(!TrimFilter_IsRowEmpty(pixels,this->mWidth))
				break;
		}
		bottom = y;
		this->mHeight = bottom-top+1;

		// Find Left
		pixels = anImage->mImageData + top*bytesPerRow;
		for(x=0; x<this->mWidth; x++, pixels++)
		{
			if(!TrimFilter_IsColEmpty(pixels,this->mHeight,bytesPerRow))
				break;
		}
		if(x>=this->mWidth)
			return NULL;

		left = x;
		
		// Find Right
		pixels = anImage->mImageData + top*bytesPerRow + (this->mWidth-1);
		for(x=right; x>left; x--, pixels--)
		{
			if(!TrimFilter_IsColEmpty(pixels,this->mHeight,bytesPerRow))
				break;
		}
		right = x;
		this->mWidth = right-left+1;
	}
	else
		return NULL;

	this->mx = left; this->my = top;
	this->mWidth = right-left+1;
	this->mHeight = bottom-top+1;
	return CropImageFilter::Filter(theImage);
}
///////////////////////////////////////////////////////////////////////////////
}
