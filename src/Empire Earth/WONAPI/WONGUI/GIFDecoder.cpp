// GIF image decoder (not identified in the original exe)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <cstring>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyAnimation.h"
#include "ee/wonapi/WONLobbyImageDecoder.h"
#include "ee/wonapi/WONLobbyGIFDecoder.h"
#include <memory>
using namespace std;

using namespace WONAPI;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GIFDecoder::GIFDecoder()
{
	this->mGlobalColorMap = NULL;
	this->mNumLoops = 0;
}
	
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GIFDecoder::Add(MultiImageDecoder *theDecoder)
{
	GIFDecoder *aDecoder = new GIFDecoder;
	theDecoder->AddDecoder(aDecoder,"gif");
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GIFDecoder::InitStringTable()
{
	for(int i=0; i<256; i++)
	{
		this->mStringTable[i].mData = i;
		this->mStringTable[i].mRootData = i;
		this->mStringTable[i].mLen = 1;
		this->mStringTable[i].mPrev = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static unsigned long GetBits(unsigned long &theVal, unsigned char theNumBits)
{
	unsigned long aVal = theVal & ((1<<theNumBits)-1);
	theVal>>=theNumBits;
	return aVal;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool GIFDecoder::ReadSignature()
{
	this->mSignature.resize(6);
	this->mFileReader->ReadBytes((void*)this->mSignature.data(),this->mSignature.size());
	string aSub = this->mSignature.substr(3);

	return StringCompareNoCase(aSub,"GIF")!=0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GIFDecoder::ReadScreenDescriptor()
{
	this->mScreenWidth = this->mFileReader->ReadShort();
	this->mScreenHeight = this->mFileReader->ReadShort();

	unsigned long aVal = this->mFileReader->ReadByte();
	this->mNumBitsPerPixel = GetBits(aVal,3)+1;
	GetBits(aVal,1); // should be zero
	this->mNumBitsOfColorResolution = GetBits(aVal,3)+1;
	this->mHaveGlobalColorMap = GetBits(aVal,1)?true:false;

	this->mBackgroundColorIndex = this->mFileReader->ReadByte();
	aVal = this->mFileReader->ReadByte(); // should be zero
	
	if(this->mHaveGlobalColorMap)
		this->mGlobalColorMap = this->ReadColorMap(1<<this->mNumBitsPerPixel);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SmartPtr<Palette> GIFDecoder::ReadColorMap(int theNumColors)
{
	Palette* aColorMap = new Palette(theNumColors);
	for(int i=0; i<theNumColors; i++)
	{
		unsigned char r = this->mFileReader->ReadByte();
		unsigned char g = this->mFileReader->ReadByte();
		unsigned char b = this->mFileReader->ReadByte();
		aColorMap->SetColor(i, (r<<16) | (g<<8) | b);
	}

	return aColorMap;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GIFDecoder::ReadExtension()
{
	unsigned char aFunctionCode = this->mFileReader->ReadByte();
	unsigned char aByteCount = this->mFileReader->ReadByte();
	switch(aFunctionCode)
	{
		case 0xf9: // graphics control extension
		{
			unsigned long aVal = this->mFileReader->ReadByte();
			bool transparentColor = GetBits(aVal,1)?true:false;
			bool userInput = GetBits(aVal,1)?true:false;
			unsigned char aDisposalMethod = GetBits(aVal,3); // 0 - Not Specified, 1 - Leave, 2 - Restore BG Color, 3 - Restore what was there before
			unsigned short aDelayTime = this->mFileReader->ReadShort();
			unsigned char aTransparentColorIndex = this->mFileReader->ReadByte();

			if(aDisposalMethod < AnimationFrame::DisposalMethod_Max)
				this->mCurFrame->mDisposalMethod = (AnimationFrame::DisposalMethod)aDisposalMethod;
			else
				this->mCurFrame->mDisposalMethod = AnimationFrame::DisposalMethod_None;

			if(aDelayTime==0)
				this->mNumLoops = 1;

			this->mCurFrame->mDelayTime = aDelayTime*10;
			if(transparentColor)
				this->mCurImageTransparentColor = aTransparentColorIndex;
			else
				this->mCurImageTransparentColor = -1;

			aByteCount = this->mFileReader->ReadByte();
		}
		break;

		case 0xff: // application extension
		{
			string anIdentifier;
			anIdentifier.resize(11);
			this->mFileReader->ReadBytes((void*)anIdentifier.data(),anIdentifier.size());
			aByteCount = this->mFileReader->ReadByte();
			if(anIdentifier=="NETSCAPE2.0")
			{
				unsigned char aByte = this->mFileReader->ReadByte();
				unsigned short aNumLoops = this->mFileReader->ReadShort();
				aByteCount = this->mFileReader->ReadByte();

				this->mNumLoops = aNumLoops;
			}
		}
		break;
	}
	
	while(aByteCount!=0)
	{
		this->mFileReader->SkipBytes(aByteCount);
		aByteCount = this->mFileReader->ReadByte();
	}
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GIFDecoder::ReadImageData()
{
	unsigned char aCodeSize = this->mFileReader->ReadByte();
	unsigned char aByteCount = this->mFileReader->ReadByte();

	unsigned char aCompressionSize = aCodeSize+1;
	int aCompressionMask = (1<<aCompressionSize)-1;

	unsigned short aClearCode = 1<<aCodeSize;
	unsigned short anEndCode = aClearCode+1;
	unsigned short nextCode = aClearCode+2;
	
	SmartPtr<RawImage8> anImage = new RawImage8(this->mCurImageWidth,this->mCurImageHeight,this->mCurImagePalette.get());
	int anImageSize = this->mCurImageWidth*this->mCurImageHeight;
	int aRowPad = anImage->mBytesPerRow - this->mCurImageWidth;

	unsigned long anImagePos = 0;

	unsigned short code = 0;
	int oldCode = -1;
	LZWNode *oldString = NULL, *newString = NULL;
	int aBitCount = 0;
	int aData = 0;
	int aBytesLeft = aByteCount;

	int aDebugCount = 0;
	while(aByteCount!=0)
	{
		if(aCompressionSize > aBitCount)
		{
			if(aBytesLeft==0)
			{
				aByteCount = aBytesLeft = this->mFileReader->ReadByte();
				continue;
			}

			aData |= (this->mFileReader->ReadByte()<<aBitCount);
			aBitCount+=8;
			aBytesLeft--;
			continue;
		}

		code = aData&aCompressionMask;
		aData>>=aCompressionSize;
		aBitCount-=aCompressionSize;

		if(code>nextCode)
			break;

		if(code==aClearCode)
		{
			oldCode = -1;
			aCompressionSize = aCodeSize+1;
			aCompressionMask = (1<<aCompressionSize)-1;
			nextCode = aClearCode+2;
		}
		else if(code==anEndCode)
			break; 
		else 
		{
			if(code<nextCode)
				newString = this->mStringTable + code;
			else
			{
				newString = this->mStringTable + nextCode;
				newString->Init(oldString, oldString->mRootData);
			}

			int aLen = newString->mLen;
			LZWNode *aNode = newString;

			anImagePos += newString->mLen-1;
			if(anImagePos >= anImageSize) // keep from crashing with corrupt image
				break;

			int aRow = anImagePos/this->mCurImageWidth;
			int aCol = anImagePos%this->mCurImageWidth;
			unsigned char *anImageData = anImage->StartRowTraversal(aRow)+aCol;
			while(aLen--)
			{
				*anImageData = aNode->mData;
				aNode = aNode->mPrev;
				aCol--;
				anImageData--;
				if(aCol<0)
				{
					anImageData-=aRowPad;
					aCol = this->mCurImageWidth-1;
				}
			}
			anImagePos++;


			if(oldCode!=-1)
			{
				this->mStringTable[nextCode++].Init(oldString,newString->mRootData);
				if(nextCode > aCompressionMask && nextCode<4096)
				{
					aCompressionSize++;
					aCompressionMask = (1<<aCompressionSize)-1;
				}
			}

			oldCode = code;
			oldString = newString;
		}
	}
	
	unsigned long aVal = this->mFileReader->ReadByte(); // should be zero

	// Handle interlaced image
	if(this->mCurImageIsInterlaced)
	{
		SmartPtr<RawImage8> anInterlacedImage = anImage;
		anImage = new RawImage8(this->mCurImageWidth,this->mCurImageHeight,this->mCurImagePalette.get());
		
		static const int dy[] = {8, 8, 4, 2};
		static const int yoff[] = {0, 4, 2, 1};
		int aWidth = anImage->mBytesPerRow;

		int y_interlaced = 0;
		for(int i = 0; i<4; i++)
		{
			int y = yoff[i];
			while(y < this->mCurImageHeight)
			{
				memcpy(anImage->mImageData + y*aWidth, anInterlacedImage->mImageData + y_interlaced*aWidth, aWidth);
				y+=dy[i];
				y_interlaced++;
			}
		}
	}
		
	anImage->mTransparentColor = this->mCurImageTransparentColor;
	if(this->mIsRawDecode)
		this->mCurFrame->mImage = anImage;
	else
		this->mCurFrame->mImage = anImage->GetNative(this->mContext);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GIFDecoder::ReadImage()
{
	this->mCurFrame->mLeft = this->mFileReader->ReadShort();
	this->mCurFrame->mTop = this->mFileReader->ReadShort();
	this->mCurImageWidth = this->mFileReader->ReadShort();
	this->mCurImageHeight = this->mFileReader->ReadShort();
	if(this->mCurImageWidth*this->mCurImageHeight>3000*3000)
		throw FileReaderException("Image too big.");
	else if(this->mCurImageWidth<=0 || this->mCurImageHeight<=0)
		throw FileReaderException("Image too small.");

	unsigned long aVal = this->mFileReader->ReadByte();
	int aNumBitsPerPixel =  GetBits(aVal,3)+1;
	GetBits(aVal,3); // should be 0
	this->mCurImageIsInterlaced = GetBits(aVal,1)?true:false;

	bool haveLocalColorMap = GetBits(aVal,1)?true:false;
	if(haveLocalColorMap)
		this->mCurImagePalette = this->ReadColorMap(1<<aNumBitsPerPixel);
	else
		this->mCurImagePalette = this->mGlobalColorMap;

	this->ReadImageData();

	Image *anImage = (Image*)this->mCurFrame->mImage;
	if(anImage->mWidth!=this->mScreenWidth || anImage->mHeight!=this->mScreenHeight)
		this->mNeedAccumulate = true;

	this->mFrameList.push_back(this->mCurFrame);
	this->NextImage();
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GIFDecoder::NextImage()
{
	this->mCurImageWidth = this->mCurImageHeight = 0;
	this->mCurImageTransparentColor = -1;
	this->mCurImageIsInterlaced = false;
	this->mCurImagePalette = NULL;
	this->mCurFrame = new AnimationFrame;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool GIFDecoder::DoGetAttrib(ImageAttributes &theAttrib)
{
	if(!this->ReadSignature())
		return false;

	this->ReadScreenDescriptor();
	theAttrib.mWidth = this->mScreenWidth;
	theAttrib.mHeight = this->mScreenHeight;
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SmartPtr<Image> GIFDecoder::BaseDecode()
{
	this->InitStringTable();

	bool success = false;
	this->mImage = NULL;
	this->mAnimation = NULL;

	this->mNeedAccumulate = false;

	this->mFrameList.clear();

	try
	{
		this->ReadSignature();
		this->ReadScreenDescriptor();

		this->NextImage();
		bool done = false;
		while(!success)
		{
			char aChar = this->mFileReader->ReadByte();
			switch(aChar)
			{
				case '!': 
					this->ReadExtension(); 
					break;

				case ',': 
					this->ReadImage(); 
					break;

				case ';': 
					success = true; 
					break;
			}
		} 
	}
	catch(WONAPI::FileReaderException&)
	{
	}

	if(!this->mFrameList.empty())
	{
		SmartPtr<Image> anImage = this->mFrameList.front()->mImage.get();

		this->mAnimation = new Animation(this->mFrameList, this->mScreenWidth, this->mScreenHeight);
		this->mAnimation->SetNumLoops(this->mNumLoops);
		this->mAnimation->SetNeedAccumulate(this->mNeedAccumulate);
		if(this->mIsRawDecode)
		{
			RawImage8 *aRawImage = (RawImage8*)anImage.get();
			this->mAnimation->SetCopyFirstImageToAccumulate(aRawImage->mTransparentColor>=0);
		}
		else
		{
			NativeImage *aNativeImage = (NativeImage*)anImage.get();
			this->mAnimation->SetCopyFirstImageToAccumulate((aNativeImage->mTransparentMask.get()!=NULL));
			this->mAnimation->RealizeNative(this->mContext);
		}				

		return anImage;
	}

	return NULL;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SmartPtr<NativeImage> GIFDecoder::DoDecode(DisplayContext *theContext)
{
	this->mIsRawDecode = false;
	this->mContext = theContext;
	this->mImage = (NativeImage*)this->BaseDecode().get();
	return this->mImage;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SmartPtr<RawImage> GIFDecoder::DoDecodeRaw()
{
	this->mIsRawDecode = true;
	return (RawImage*)this->BaseDecode().get();
}
