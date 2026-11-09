// GIFDecoder (optional; not identified in the original exe).
#ifndef EE_WONAPI_WONLOBBYGIFDECODER_H
#define EE_WONAPI_WONLOBBYGIFDECODER_H
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyAnimation.h"
#include "ee/wonapi/WONLobbyImageDecoder.h"
namespace WONAPI {
class GIFDecoder : public ImageDecoder
{
private:
	DisplayContext *mContext;

	std::string mSignature;
	
	unsigned short mScreenWidth;
	unsigned short mScreenHeight;
	unsigned char mNumBitsPerPixel;
	unsigned char mNumBitsOfColorResolution;
	bool mHaveGlobalColorMap;
	unsigned char mBackgroundColorIndex;
	SmartPtr<Palette> mGlobalColorMap;
	
	AnimationFrameList mFrameList;
	unsigned short mNumLoops;

	int mCurImageWidth;
	int mCurImageHeight;
	int mCurImageTransparentColor;
	bool mCurImageIsInterlaced;
	SmartPtr<Palette> mCurImagePalette;
	SmartPtr<AnimationFrame> mCurFrame;

	bool mNeedAccumulate;
	bool mIsRawDecode;

	SmartPtr<NativeImage> mImage;
	SmartPtr<Animation> mAnimation;

private:
	struct LZWNode
	{
		LZWNode *mPrev;
		unsigned char mRootData;
		unsigned char mData;
		unsigned short mLen;

		void Init(LZWNode *thePrev, unsigned char theData)
		{
			this->mData = theData;
			this->mPrev = thePrev;
			this->mLen = thePrev->mLen+1;
			this->mRootData = thePrev->mRootData;
		}
	};

	LZWNode mStringTable[4096];

	bool ReadSignature();
	void ReadScreenDescriptor();
	SmartPtr<Palette> ReadColorMap(int theNumColors);

	void InitStringTable();
	void ReadExtension();

	void ReadImageData();
	void ReadImage();
	void NextImage();

	SmartPtr<Image> BaseDecode();
	SmartPtr<NativeImage> DoDecode(DisplayContext *theContext);
	SmartPtr<RawImage> DoDecodeRaw();
	virtual bool DoGetAttrib(ImageAttributes &theAttrib);


public:
	GIFDecoder();
	static void Add(MultiImageDecoder *theDecoder);

	NativeImage* GetImage() { return this->mImage; }
	virtual SmartPtr<Animation> GetAnimation() { return this->mAnimation; }

	virtual ImageDecoder* Duplicate() { return new GIFDecoder; }

};
}
#endif
