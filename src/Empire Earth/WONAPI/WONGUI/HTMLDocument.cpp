// HTML document: tag list, body bounds, list numbering, image loading and image maps (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <cstdio>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyHTMLTag.h"
#include "ee/wonapi/WONLobbyAnimation.h"
#include "ee/wonapi/WONLobbyImageDecoder.h"
#include "ee/wonapi/WONLobbyImageHelpers.h"
#include "ee/wonapi/WONLobbyResourceConfig.h"
#include "ee/wonapi/WONLobbyConfigImages.h"

namespace WONAPI {
inline HTMLDocument::LoadedImageStruct::LoadedImageStruct(NativeImage* image,Animation* animation) : mImage(image),mAnimation(animation)
{
    this->mHaveUsedAnimation=false;
}
// FUNCTION: WON 0x0073c780
// Deleting destructor of an HTML document (compiler-generated)
// (??_GHTMLDocument@WONAPI@@UAEPAXI@Z)

// FUNCTION: WON 0x0073c610
// Creates an empty HTML document on the default window, delaying image loads, with an optional base path
HTMLDocument::HTMLDocument(const GUIString &theBasePath)
{
	this->Clear();

	this->mDecodingImages = false;
	this->mDelayImages = true;
	this->mWindow = Window::GetDefaultWindow();

	if(!(theBasePath.length()==0))
		this->SetBasePath(theBasePath);
}

///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x0073cbd0
// Empties the document: tags, body bounds, loaded and scaled images, pending image tags, lists and image maps
void HTMLDocument::Clear()
{
	this->mBodyStart = this->mBodyEnd = this->mTagList.end();
	this->mImageTagMap.clear();
	this->mLoadedImageMap.clear();
	this->mScaledImageMap.clear();
	this->mImageTagList.clear();
	this->mTagList.clear();
	this->mListStack.clear();
	this->mCurImageMap = NULL;
	this->mImageMapMap.clear();
}
// FUNCTION: WON 0x0073ccf0
// Sets the folder relative image paths resolve against: the given path up to its last slash, or empty
void HTMLDocument::SetBasePath(const GUIString& path)
{
    int pos=path.rFind('\\');
    if(pos==-1)
        pos=path.rFind('/',pos);
    if(pos==-1)
        this->mBasePath.erase();
    else
        this->mBasePath=path.substr(0,pos+1);
}
// FUNCTION: WON 0x0073d350
// Destroys a loaded image record, releasing its images and animation
// (??1LoadedImageStruct@HTMLDocument@WONAPI@@QAE@XZ)

// FUNCTION: WON 0x0073d400
// Destroys a path/loaded image map entry
// (??1?$pair@$$CBVGUIString@WONAPI@@ULoadedImageStruct@HTMLDocument@2@@std@@QAE@XZ)

// FUNCTION: WON 0x0073d4e0
// Destroys a path/tag map entry
// (??1?$pair@$$CBVGUIString@WONAPI@@V?$SmartPtr@VHTMLTag@WONAPI@@@2@@std@@QAE@XZ)

// FUNCTION: WON 0x0073d9a0
// Creates an image filter with no next filter
// (??0ImageFilter@WONAPI@@QAE@XZ)

// FUNCTION: WON 0x0073de80
// Destroys a name/image map entry
// (??1?$pair@$$CBVGUIString@WONAPI@@V?$SmartPtr@VImageMap@WONAPI@@@2@@std@@QAE@XZ)

// FUNCTION: WON 0x0073df20
// Destroys an image map, releasing its links
// (??1ImageMap@WONAPI@@UAE@XZ)

// FUNCTION: WON 0x0073df00
// Deleting destructor of an image map (compiler-generated)
// (??_GImageMap@WONAPI@@UAEPAXI@Z)

// FUNCTION: WON 0x0073eb80
// Appends the body tags of another document to this one (ignored when NULL)
void HTMLDocument::AddDocument(HTMLDocument* document)
{
    if(document==NULL) return;
    HTMLTagList::iterator begin=document->GetBodyStart(),end=document->GetBodyEnd();
    for(HTMLTagList::iterator i=begin;i!=end;++i) this->AddTag(i->get());
}
// FUNCTION: WON 0x0073ebd0
// Appends a tag and processes it: body bounds, images, inputs, image maps and areas, lists and list items
void HTMLDocument::AddTag(HTMLTag* tag)
{
    this->mTagList.push_back(tag);
    switch(tag->GetType())
    {
    case HTMLTagType_Body: this->HandleBody(static_cast<HTMLBodyTag*>(tag)); break;
    case HTMLTagType_Image: this->HandleImage(static_cast<HTMLImageTag*>(tag)); break;
    case HTMLTagType_Input: this->HandleInput(static_cast<HTMLInputTag*>(tag)); break;
    case HTMLTagType_ImageMap: this->HandleImageMap(static_cast<HTMLImageMapTag*>(tag)); break;
    case HTMLTagType_Area: this->HandleArea(static_cast<HTMLAreaTag*>(tag)); break;
    case HTMLTagType_OrderedList:
    case HTMLTagType_UnorderedList: this->HandleList(static_cast<HTMLListTag*>(tag)); break;
    case HTMLTagType_ListItem: this->HandleListItem(static_cast<HTMLListItemTag*>(tag)); break;
    }
}
// FUNCTION: WON 0x0073ef70
// Returns the first body tag, or the first tag when the document has no <BODY>
HTMLTagList::iterator HTMLDocument::GetBodyStart()
{
    if(this->mBodyStart!=this->mTagList.end())
        return this->mBodyStart;
    return this->mTagList.begin();
}
// FUNCTION: WON 0x0073efa0
// Returns the position just past the last body tag (the list end when the body is open)
HTMLTagList::iterator HTMLDocument::GetBodyEnd()
{
    HTMLTagList::iterator end=this->mBodyEnd;
    if(end!=this->mTagList.end()) ++end;
    return end;
}
// FUNCTION: WON 0x0073e980
// Writes a list number as letters (a, b, ... z, aa, ...) followed by a dot
static void GetLetterItem(int theCount, char *theBuf, bool upper)
{
	int aPos = 0;
	while(theCount>0)
	{
		theCount--;
		theBuf[aPos++] = (upper?'A':'a') + theCount%26;
		theCount/=26;
	} 


	theBuf[aPos] = '.';
	theBuf[aPos+1] = 0;

	// Reverse String
	int i = 0;
	aPos--;
	while(i<aPos)
	{
		char aTemp = theBuf[aPos]; theBuf[aPos] = theBuf[i]; theBuf[i] = aTemp;
		i++; aPos--;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x0073ea90
// Writes one decimal digit of a Roman numeral using the digit's high, middle and low letters
static void GetRomanNumeralHelp(char *theBuf, int &thePos, int &theNumber, int hiVal, char hiChar, char midChar, char lowChar)
{
	int lowVal = hiVal/10;
	int midVal = hiVal>>1;

	while(theNumber>=hiVal)
	{
		theBuf[thePos++] = hiChar;
		theNumber-=hiVal;
	}

	if(theNumber>=hiVal-lowVal)
	{
		theBuf[thePos++] = lowChar;
		theBuf[thePos++] = hiChar;
		theNumber-=hiVal-lowVal;
	}

	if(theNumber>=midVal)
	{
		theBuf[thePos++] = midChar;
		theNumber-=midVal;
	}

	if(theNumber>=midVal-lowVal)
	{
		theBuf[thePos++] = lowChar;
		theBuf[thePos++] = midChar;
		theNumber-=midVal-lowVal;
	}

	while(theNumber>=lowVal)
	{
		theBuf[thePos++] = lowChar;
		theNumber-=lowVal;
	}
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x0073e9f0
// Writes a list number as a Roman numeral with a dot; numbers from 4000 up are written as digits
static void GetRomanNumeralItem(int theCount, char *theBuf, bool upper)
{
	static char lower_letters[] = { 'm', 'd', 'c', 'l', 'x', 'v', 'i' };
	static char upper_letters[] = { 'M', 'D', 'C', 'L', 'X', 'V', 'I' };
	const char* letters = upper?upper_letters:lower_letters;

	if(theCount>=4000)
	{
		sprintf(theBuf,"%d.",theCount);
		return;
	}

	int aPos = 0;
	int aHiVal = 1000;
	int aLetterPos = 0;
	for(int i=0; i<3; i++)
	{
		GetRomanNumeralHelp(theBuf,aPos,theCount,aHiVal,letters[aLetterPos],letters[aLetterPos+1],letters[aLetterPos+2]);
		aHiVal/=10;
		aLetterPos+=2;
	}
	theBuf[aPos++] = '.';
	theBuf[aPos] = '\0';
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// FUNCTION: WON 0x0073e680
// Opens a list level (bullet style by depth for <UL> unless given, numbering style and start for <OL>); end pops
void HTMLDocument::HandleList(HTMLListTag *theTag)
{	
	if(theTag->IsEnd())
	{
		if(!this->mListStack.empty())
			this->mListStack.pop_back();

		return;
	}

	this->mListStack.insert(this->mListStack.end(),ListInfo());

	ListInfo &anInfo = this->mListStack.back();
	if(theTag->GetType()==HTMLTagType_UnorderedList)
	{
		anInfo.mOrdered = false;
		if(theTag->mListType==HTMLListType_None)
		{
			anInfo.mType = (HTMLListType)(HTMLListType_Disc + this->mListStack.size() - 1);

		}
		else
			anInfo.mType = theTag->mListType;

		if(anInfo.mType > HTMLListType_Square)
			anInfo.mType = HTMLListType_Square;
		else if(anInfo.mType < HTMLListType_Disc)
			anInfo.mType = HTMLListType_Disc;
	}
	else
	{
		anInfo.mOrdered = true;
		anInfo.mType = theTag->mListType;
	}

	anInfo.mCount = theTag->mStart;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x0073e7b0
// Gives an <LI> its list's bullet style and, in an ordered list, its number text, then counts the item
void HTMLDocument::HandleListItem(HTMLListItemTag *theTag)
{
	if(theTag->IsEnd())
		return;

	if(this->mListStack.empty())
	{
		theTag->mListType = HTMLListType_Disc;
		return;
	}

	ListInfo &anInfo = this->mListStack.back();
	theTag->mListType = anInfo.mType;

	if(anInfo.mOrdered)
	{
		char aBuf[100];
		aBuf[0] = 0;
		switch(anInfo.mType)
		{
			case HTMLListType_LowerLetter: GetLetterItem(anInfo.mCount,aBuf,false); break;
			case HTMLListType_UpperLetter: GetLetterItem(anInfo.mCount,aBuf,true); break;
			case HTMLListType_LowerRoman: GetRomanNumeralItem(anInfo.mCount,aBuf,false); break;
			case HTMLListType_UpperRoman: GetRomanNumeralItem(anInfo.mCount,aBuf,true); break;

			case HTMLListType_Digit: 
			default:
				sprintf(aBuf,"%d.",anInfo.mCount); break;
		}
		theTag->mBulletText = aBuf;
	}

	anInfo.mCount++;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// FUNCTION: WON 0x0074b600
// FUNCTION: WON 0x0073ce10
// Decodes an image file into a loaded image record (native image, raw image, animation); false on failure
// (0x0074b600 above: the loaded image record's copy constructor, compiler-generated and emitted here)
bool HTMLDocument::DoDecodeImage(LoadedImageStruct *theStruct, Window *theWindow, const std::string &theImagePath)
{
	if(theWindow==NULL)
		return false;

	MultiImageDecoder *aDecoder = &theWindow->GetWindowManager()->mImageDecoder;
	SmartPtr<RawImage> aRawImage = aDecoder->DecodeRaw(theImagePath.c_str());
	if(aRawImage.get()==NULL)
		return false;

	SmartPtr<NativeImage> anImage = aRawImage->GetNative(theWindow->mDisplayContext.get());
	
	SmartPtr<Animation> anAnimation = aDecoder->GetAnimation(false);
	if(anAnimation.get()!=NULL)
		anAnimation->RealizeNative(this->mWindow->mDisplayContext.get());

	theStruct->mImage = anImage;
	theStruct->mRawImage = aRawImage;
	theStruct->mAnimation = anAnimation;
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x0073d010
// Returns the loaded image for a path, decoding it once; while images are delayed it queues the tag instead
HTMLDocument::LoadedImageStruct* HTMLDocument::GetImage(const GUIString &thePath, HTMLTag *theTag)
{
	if((thePath.empty()))
		return NULL;

	std::pair<LoadedImageMap::iterator,bool> aRet;
	aRet = this->mLoadedImageMap.insert(LoadedImageMap::value_type(thePath,LoadedImageStruct()));
	LoadedImageStruct &aStruct = aRet.first->second;

	if(aStruct.mImage.get()!=NULL)
		return &aStruct;

	if(this->mDelayImages && !this->mDecodingImages)
	{
		this->mImageTagMap.insert(ImageTagMap::value_type(thePath,theTag));
		return NULL;
	}

	if(!aRet.second) // already failed to decode
			return NULL;

	GUIString aPath = this->mBasePath;
	aPath.append(thePath);

	if(this->DoDecodeImage(&aStruct,this->mWindow,aPath))
		return &aStruct;
	else
		return NULL;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// FUNCTION: WON 0x0073dfa0
// Loads the image of an image <INPUT> and stores it on the tag
void HTMLDocument::HandleInput(HTMLInputTag *theTag)
{
	if(theTag->IsEnd())
		return;

	if((theTag->mImageSrc.length()==0))
		return;

	LoadedImageStruct *aStruct = this->GetImage(theTag->mImageSrc,theTag);
	if(aStruct==NULL)
		return;

	theTag->mImage = aStruct->mImage.get();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// FUNCTION: WON 0x0073edb0
// Loads every queued image and updates its tags; unless delays continue, stops delaying and frees the caches
void HTMLDocument::DecodeImages(bool continueDelay)
{
	if(!continueDelay)
		this->mDelayImages = false;

	this->mDecodingImages = true;
	ImageTagMap::iterator anItr = this->mImageTagMap.begin();

	while(anItr!=this->mImageTagMap.end())
	{
		HTMLTag *aTag = anItr->second.get();
		switch(aTag->GetType())
		{
			case HTMLTagType_Body: this->HandleBody(static_cast<HTMLBodyTag*>(aTag)); break;
			case HTMLTagType_Image: this->HandleImage(static_cast<HTMLImageTag*>(aTag)); break;
			case HTMLTagType_Input: this->HandleInput(static_cast<HTMLInputTag*>(aTag)); break;
		}

		++anItr;

	}
	this->mDecodingImages = false;

	if(continueDelay)
		return;
	this->mImageTagMap.clear();
	this->mLoadedImageMap.clear();
	this->mScaledImageMap.clear();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x0073d560
// Returns the image scaled to the given size, cached per path and size; nonpositive sizes keep the image's own
NativeImage* HTMLDocument::CheckScaleImage(LoadedImageStruct* image,const GUIString& path,int width,int height)
{
    NativeImage* native=image->mImage.get();
    // Each nonpositive dimension defaults on its own; the aspect ratio is not kept
    if(width<=0) width=native->GetWidth();
    if(height<=0) height=native->GetHeight();
    if(width!=native->GetWidth() || height!=native->GetHeight())
    {
        char buffer[50];
        sprintf(buffer,"_%d_%d",width,height);
        GUIString scaleName=path;
        scaleName.append(GUIString(buffer));
        LoadedImageMap::iterator i=this->mScaledImageMap.insert(LoadedImageMap::value_type(scaleName,LoadedImageStruct())).first;
        LoadedImageStruct& scaled=i->second;
        if(scaled.mImage.get()==NULL)
        {
            if(image->mRawImage.get()==NULL) image->mRawImage=native->GetRaw().get();
            SmartPtr<ScaleImageFilter> filter=new ScaleImageFilter(width,height);
            scaled.mImage=filter->Filter(image->mRawImage.get())->GetNative(this->mWindow->mDisplayContext.get());
        }
        native=scaled.mImage.get();
    }
    return native;
}
// FUNCTION: WON 0x0073dcd0
// Paints the placeholder for an image still loading: a sunken frame with a small red-cross icon
static void ImagePaint(Graphics &g, int width, int height)
{
	g.SetColor(0x848484);
	g.DrawRect(0,0,width,height);
	g.SetColor(0xd6d6d6);
	g.DrawLine(width-1,0,width-1,height-1);
	g.DrawLine(0,height-1,width-1,height-1);

	int w = 14, h = 16;
	int ox = 7, oy = 7;

	bool clipped = false;
	if(width<w+ox || height<h+oy)
	{
		if(width<8 || height<8)
			return;

		g.PushClipRect(0,0,width-5,height-5);
		clipped = true;
	}

	g.Translate(ox,oy);
	
	g.SetColor(0xffffff);
	g.FillRect(0,0,w,h);
	g.SetColor(0x848484);
	g.DrawLine(0,0,w-1,0);
	g.DrawLine(0,0,0,h-2);
	g.SetColor(0x000000);
	g.DrawLine(0,h-1,w-1,h-1);
	g.DrawLine(w-1,1,w-1,h-1);
	g.SetColor(0xa0a0a0);
	g.DrawLine(w-2,1,w-2,h-2);
	g.DrawLine(1,h-2,w-2,h-2);

	g.SetColor(0xff0000);


	int offset = 0;
	int hmid = w/2;
	int vmid = h/2;
	int left = hmid-4+offset;
	int right = hmid+3+offset;
	int top = vmid-4+offset;
	int bottom = vmid+3+offset;
	for(int i=0; i<4; i++)
	{
		g.DrawLine(left+i,top+i,left+i+1,top+i);
		g.DrawLine(right-i,top+i,right-1-i,top+i);
		g.DrawLine(left+i,bottom-i,left+i+1,bottom-i);
		g.DrawLine(right-i,bottom-i,right-i-1,bottom-i);
	}

	g.Translate(-ox,-oy);

	if(clipped)
		g.PopClipRect();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x0073d9e0
// Gives an <IMG> its image (scaled), animation or loading placeholder, and its image map for USEMAP
void HTMLDocument::HandleImage(HTMLImageTag *theTag)
{
	LoadedImageStruct *aStruct = this->GetImage(theTag->mSrc,theTag);
	if(aStruct==NULL)
	{		
		if(this->mDelayImages && theTag->mWidth>0 && theTag->mHeight>0)
			theTag->mImage = new ProgramImage(theTag->mWidth, theTag->mHeight,ImagePaint);

		return;
	}


	if(aStruct->mAnimation.get()!=NULL)
	{
		if(aStruct->mHaveUsedAnimation)
			theTag->mAnimation = aStruct->mAnimation->Duplicate();
		else
		{
			theTag->mAnimation = aStruct->mAnimation;
			aStruct->mHaveUsedAnimation = true;
		}

		return;
	}
	
	NativeImage *anImage = this->CheckScaleImage(aStruct,theTag->mSrc,theTag->mWidth,theTag->mHeight);
	theTag->mImage = anImage;	

	if(!(theTag->mUseMap.length()==0))
	{
		ImageMapMap::iterator anItr = this->mImageMapMap.insert(ImageMapMap::value_type(theTag->mUseMap,NULL)).first;
		if(anItr->second.get()==NULL)
			anItr->second = new ImageMap;

		theTag->mImageMap = anItr->second;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x0073e020
// Opens the named image map (created on first use) for the following <AREA> tags; the end tag closes it
void HTMLDocument::HandleImageMap(HTMLImageMapTag* theTag)
{
	if(theTag->IsEnd())
	{
		this->mCurImageMap = NULL;
		return;
	}

	if((theTag->mName.empty()))
		return;

	ImageMapMap::iterator anItr = this->mImageMapMap.insert(ImageMapMap::value_type(theTag->mName,SmartPtr<ImageMap>())).first;

	if(anItr->second.get()==NULL)
		anItr->second = new ImageMap;

	this->mCurImageMap = anItr->second;				
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x0073e270
// Adds a valid <AREA>'s rectangle and link to the open image map
void HTMLDocument::HandleArea(HTMLAreaTag *theTag)
{
	if(theTag->IsEnd())
		return;

	if(this->mCurImageMap.get()==NULL)
		return;

	if(!theTag->mIsValid)
		return;

	WONRectangle aRect(theTag->mLeft,theTag->mTop,theTag->mRight-theTag->mLeft,theTag->mBottom-theTag->mTop);
	this->mCurImageMap->AddLink(new TextLinkParam(theTag->mHRef, theTag->mLaunchBrowser), aRect);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// FUNCTION: WON 0x0073e410
// Records the body bounds (dropping repeated <BODY> tags) and loads the background, pre-tiled to 50 pixels
void HTMLDocument::HandleBody(HTMLBodyTag *theTag)
{
	if(!this->mDecodingImages)
	{
		if(theTag->IsEnd())
		{
			if(this->mBodyEnd!=this->mTagList.end())
			{
				this->mTagList.erase(this->mBodyEnd);
				this->mBodyEnd = this->mTagList.end();
			}

			--this->mBodyEnd; // point to last added tag

			return;
		}

		if(this->mBodyStart==this->mTagList.end())
			--this->mBodyStart; // point to last added tag
		else // get rid of this body tag because we already have one.
		{
			if(this->mBodyStart->get()!=theTag)
				this->mTagList.pop_back();

			return;
		}
	}

	LoadedImageStruct *aStruct = this->GetImage(theTag->mBGImageSrc,theTag);
	if(aStruct==NULL)
		return;

	NativeImage *anImage = aStruct->mImage.get();

	// Pre-Tile the image a little if it's too small
	int aWidth = anImage->GetWidth();
	int aHeight = anImage->GetHeight();
	int anExtraWidth = 0;
	int anExtraHeight = 0;
	if(aWidth < 50)
	{
		if(50%aWidth==0)
			anExtraWidth = 50-aWidth;
		else
			anExtraWidth = (50/aWidth)*aWidth;
	}
	if(aHeight < 50)
	{
		if(50%aHeight==0)
			anExtraHeight = 50-aHeight;
		else
			anExtraHeight = (50/aHeight)*aHeight;
	}

	if(anExtraWidth>0 || anExtraHeight>0)
	{
		aWidth+=anExtraWidth;
		aHeight+=anExtraHeight;
		SmartPtr<NativeImage> aBigImage = this->mWindow->CreateImage(aWidth,aHeight);
		Graphics &g = aBigImage->GetGraphics();
		for(int x=0; x<aWidth; x+=anImage->GetWidth())
			for(int y=0; y<aHeight; y+=anImage->GetHeight())
				g.DrawImage(anImage,x,y);

		theTag->mBGImage = aBigImage.get();
	}
	else
		theTag->mBGImage = anImage;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0073c7a0
// (public: __thiscall std::map<class WONAPI::GUIString,struct WONAPI::HTMLDocument::LoadedImageStruct,struct WONAPI::GUIStringLessNoCase,class std::alloc)
// TEMPLATE: WON 0x0073c860
// (public: __thiscall std::map<class WONAPI::GUIString,class WONAPI::SmartPtr<class WONAPI::ImageMap>,struct WONAPI::GUIStringLessNoCase,class std::alloc)
// TEMPLATE: WON 0x0073c920
// (public: __thiscall std::multimap<class WONAPI::GUIString,class WONAPI::SmartPtr<class WONAPI::HTMLTag>,struct WONAPI::GUIStringLessNoCase,class std::a)
// TEMPLATE: WON 0x0073d9b0
// (public: virtual class WONAPI::SmartPtr<class WONAPI::RawImage> __thiscall WONAPI::ImageFilter::Filter(class WONAPI::RawImage *))
// TEMPLATE: WON 0x0074b030
// (public: __thiscall std::list<struct WONAPI::ImageMap::ShapeLink,class std::allocator<struct WONAPI::ImageMap::ShapeLink> >::list<struct WONAPI::ImageM)
// TEMPLATE: WON 0x0074be80
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::HTMLDocument::LoadedImageStruct>,struc)
// TEMPLATE: WON 0x0074bf30
// (protected: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::HTMLDocument::LoadedImageStruct>,st)
// TEMPLATE: WON 0x0074c230
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::SmartPtr<class WONAPI::ImageMap> >,stru)
// TEMPLATE: WON 0x0074c2e0
// (protected: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::SmartPtr<class WONAPI::ImageMap> >,s)
// TEMPLATE: WON 0x0074c5e0
// (public: class std::_Ptrit<struct WONAPI::HTMLDocument::ListInfo,int,struct WONAPI::HTMLDocument::ListInfo *,struct WONAPI::HTMLDocument::ListInfo &,st)
// TEMPLATE: WON 0x0074c650
// (public: class std::_Ptrit<struct WONAPI::HTMLDocument::ListInfo,int,struct WONAPI::HTMLDocument::ListInfo *,struct WONAPI::HTMLDocument::ListInfo &,st)
// TEMPLATE: WON 0x0074c6c0
// (public: class std::_Ptrit<struct WONAPI::HTMLDocument::ListInfo,int,struct WONAPI::HTMLDocument::ListInfo *,struct WONAPI::HTMLDocument::ListInfo &,st)
// TEMPLATE: WON 0x0074c710
// (protected: bool __thiscall std::vector<struct WONAPI::HTMLDocument::ListInfo,class std::allocator<struct WONAPI::HTMLDocument::ListInfo> >::_Buy(unsig)
// TEMPLATE: WON 0x0074c750
// (public: class std::_Ptrit<struct WONAPI::HTMLDocument::ListInfo,int,struct WONAPI::HTMLDocument::ListInfo *,struct WONAPI::HTMLDocument::ListInfo &,st)
// TEMPLATE: WON 0x0074c770
// (public: struct std::pair<class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::SmartPtr<class WONAPI:)
// TEMPLATE: WON 0x0074c7f0
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::SmartPtr<class WONAPI::HTMLTag> >,struc)
// TEMPLATE: WON 0x0074dd70
// (protected: void __thiscall std::list<struct WONAPI::ImageMap::ShapeLink,class std::allocator<struct WONAPI::ImageMap::ShapeLink> >::_Incsize(unsigned )
// TEMPLATE: WON 0x0074e840
// (protected: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::HTMLTag>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::HTMLTag> )
// TEMPLATE: WON 0x0074e990
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::HTMLDocument::LoadedImageStruct>,struc)
// TEMPLATE: WON 0x0074eee0
// (protected: void __thiscall std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,struct WONAPI::HTMLDocument::LoadedImage)
// TEMPLATE: WON 0x0074ef90
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::SmartPtr<class WONAPI::ImageMap> >,stru)
// TEMPLATE: WON 0x0074f4b0
// (protected: void __thiscall std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::SmartPtr<class WONAPI::Ima)
// TEMPLATE: WON 0x0074f4f0
// (public: void __thiscall std::vector<struct WONAPI::HTMLDocument::ListInfo,class std::allocator<struct WONAPI::HTMLDocument::ListInfo> >::insert(class )
// TEMPLATE: WON 0x0074f870
// (public: class std::_Ptrit<struct WONAPI::HTMLDocument::ListInfo,int,struct WONAPI::HTMLDocument::ListInfo *,struct WONAPI::HTMLDocument::ListInfo &,st)
// TEMPLATE: WON 0x0074f890
// (public: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::SmartPtr<class WONAPI::HTMLTag> >,struc)
// TEMPLATE: WON 0x0074fdb0
// (protected: void __thiscall std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::SmartPtr<class WONAPI::HTM)
// TEMPLATE: WON 0x0074fdf0
// (protected: class std::_Tree<class WONAPI::GUIString,struct std::pair<class WONAPI::GUIString const ,class WONAPI::SmartPtr<class WONAPI::HTMLTag> >,st)
// TEMPLATE: WON 0x00751fa0
// (protected: void __thiscall std::vector<struct WONAPI::HTMLDocument::ListInfo,class std::allocator<struct WONAPI::HTMLDocument::ListInfo> >::_Xlen(void)
// TEMPLATE: WON 0x00754330
// (class std::_Ptrit<struct WONAPI::HTMLDocument::ListInfo,int,struct WONAPI::HTMLDocument::ListInfo *,struct WONAPI::HTMLDocument::ListInfo &,struct WON)
// TEMPLATE: WON 0x00754380
// (struct WONAPI::HTMLDocument::ListInfo * __cdecl std::copy(class std::_Ptrit<struct WONAPI::HTMLDocument::ListInfo,int,struct WONAPI::HTMLDocument::Lis)
// TEMPLATE: WON 0x007546d0
// (void __cdecl std::_Construct(struct WONAPI::ImageMap::ShapeLink *,struct WONAPI::ImageMap::ShapeLink const &))
// TEMPLATE: WON 0x00754720
// (void __cdecl std::_Construct(struct std::pair<class WONAPI::GUIString const ,struct WONAPI::HTMLDocument::LoadedImageStruct> *,struct std::pair<class )
// TEMPLATE: WON 0x007547a0
// (protected: struct WONAPI::HTMLDocument::ListInfo * __thiscall std::vector<struct WONAPI::HTMLDocument::ListInfo,class std::allocator<struct WONAPI::HT)
// TEMPLATE: WON 0x007547f0
// (void __cdecl std::fill(class std::_Ptrit<struct WONAPI::HTMLDocument::ListInfo,int,struct WONAPI::HTMLDocument::ListInfo *,struct WONAPI::HTMLDocument)
// TEMPLATE: WON 0x00754830
// (class std::_Ptrit<struct WONAPI::HTMLDocument::ListInfo,int,struct WONAPI::HTMLDocument::ListInfo *,struct WONAPI::HTMLDocument::ListInfo &,struct WON)
// TEMPLATE: WON 0x00754e20
// (void __cdecl std::_Construct(struct WONAPI::HTMLDocument::ListInfo *,struct WONAPI::HTMLDocument::ListInfo const &))
// TEMPLATE: WON 0x00755000
// (public: void * __thiscall std::pair<class WONAPI::GUIString const ,struct WONAPI::HTMLDocument::LoadedImageStruct>::`scalar deleting destructor'(unsig)
// TEMPLATE: WON 0x00755020
// (public: void * __thiscall std::pair<class WONAPI::GUIString const ,class WONAPI::SmartPtr<class WONAPI::ImageMap> >::`scalar deleting destructor'(unsi)
// TEMPLATE: WON 0x00755040
// (public: void * __thiscall std::pair<class WONAPI::GUIString const ,class WONAPI::SmartPtr<class WONAPI::HTMLTag> >::`scalar deleting destructor'(unsig)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00702390
// (public: void __thiscall WONAPI::GUIString::append(class WONAPI::GUIString const &,unsigned short,unsigned short))
