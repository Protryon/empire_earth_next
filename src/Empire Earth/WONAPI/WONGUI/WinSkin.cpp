// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// Windows-style skin: builds the WON skin's buttons, arrows, frames and backgrounds from a skin .ini file
// and its bitmaps (not identified in the original exe)
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyWinSkin.h"
#include "ee/wonapi/WONLobbyConfigImages.h"
#include "ee/wonapi/WONLobbyStretchImage.h"
#include "ee/wonapi/WONLobbySkinTab.h"
#include "ee/wonapi/WONLobbySkinListControls.h"
#include "ee/wonapi/WONLobbySkinnedContainers.h"
using namespace std;
namespace WONAPI {
WinSkin::WinSkin()
{
	this->mSkin = new WONMSSkin;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
std::string WinSkin::GetStrParam(const char *theSection, const char *theKey, const char *theDefault)
{
	char aStr[512];
	GetPrivateProfileString(theSection,theKey,theDefault,aStr,512,this->mFileName.c_str());
	return aStr;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
std::string WinSkin::GetPathParam(const char *theSection, const char *theKey)
{
	char aStr[512];
	GetPrivateProfileString(theSection,theKey,"",aStr,512,this->mFileName.c_str());
	return this->mFilePath + aStr;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int WinSkin::GetIntParam(const char *theSection, const char *theKey, int theDefault)
{
	char aStr[512];
	GetPrivateProfileString(theSection,theKey,"",aStr,512,this->mFileName.c_str());
	if(aStr[0]=='\0')
		return theDefault;
	else
		return atoi(aStr);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int WinSkin::GetColor(int theIndex)
{
	ColorMap::iterator anItr = this->mColorMap.insert(ColorMap::value_type(theIndex,-1)).first;
	if(anItr->second==-1)
	{
		char aSection[50];
		sprintf(aSection,"Colour%d",theIndex);
		int r = this->GetIntParam(aSection,"R");
		int g = this->GetIntParam(aSection,"G");
		int b = this->GetIntParam(aSection,"B");
		anItr->second = ((r&0xff)<<16) + ((g&0xff)<<8) + (b&0xff);
	}

	return anItr->second;

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
Font* WinSkin::GetFont(int theIndex)
{
	if(theIndex==255)
		return this->mWindow->mManager->GetNamedFont("MSButton");

	FontMap::iterator anItr = this->mFontMap.insert(FontMap::value_type(theIndex,NULL)).first;
	if(anItr->second==NULL)
	{
		char aSection[50];
		sprintf(aSection,"Font%d",theIndex);

		string aName = this->GetStrParam(aSection,"FontName");
		int aHeight = this->GetIntParam(aSection,"FontHeight")*72/96;
		int aWeight = this->GetIntParam(aSection,"FontWeight");
		bool italics = this->GetIntParam(aSection,"Italics")?true:false;
		bool underline = this->GetIntParam(aSection,"Underline")?true:false;
		bool antialias = this->GetIntParam(aSection,"AntiAlias")?true:false;

		int aStyle = 0;
		if(italics) aStyle |= FontStyle_Italic;
		if(underline) aStyle |= FontStyle_Underline;
		if(aWeight>=600) aStyle |= FontStyle_Bold;

		anItr->second = this->mWindow->GetFont(FontDescriptor(aName,aStyle,aHeight));
	}

	return anItr->second;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int WinSkin::GetColorSchemeColor(const char *theKey)
{
	char aStr[512];
	GetPrivateProfileString("Colours",theKey,"",aStr,512,this->mFileName.c_str());
	int r,g,b;
	sscanf(aStr,"%d%d%d",&r,&g,&b);

	return ((r&0xff)<<16) | ((g&0xff)<<8) | (b&0xff);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int WinSkin::GetColor(const char *theSection, const char *theKey)
{
	int anIndex = this->GetIntParam(theSection,theKey,-1);
	if(anIndex<0)
		return -1;
	else
		return this->GetColor(anIndex);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
Font* WinSkin::GetFont(const char *theSection, const char *theKey)
{
	int anIndex = this->GetIntParam(theSection,theKey,-1);
	if(anIndex<0)
		return NULL;
	else
		return this->GetFont(anIndex);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SmartPtr<RawImage> WinSkin::GetRawImage(const char *theSection, const char *theKey, bool checkTrans)
{
	SmartPtr<RawImage> anImage = this->mWindow->DecodeImageRaw(this->GetPathParam(theSection,theKey).c_str());
	if(anImage.get()==NULL)
		return NULL;

	if(!checkTrans)
		return anImage;

	int trans = this->GetIntParam(theSection,"trans");
	if(!trans)
		return anImage;

	SmartPtr<TransparentImageFilter> aTrans;
	if(trans==2) 
		aTrans = new TransparentImageFilter(TransparentImageFilter::UpperLeft);
	else 
		aTrans = new TransparentImageFilter(0xff00ff);

	anImage = aTrans->Filter(anImage);
	return anImage;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WinSkin::CalcColorScheme()
{
	SmartPtr<ColorScheme> aColorScheme = WindowManager::GetDefaultWindowManager()->mColorScheme;
	
	aColorScheme->SetStandardColor(StandardColor_Scrollbar,this->GetColorSchemeColor("Scrollbar"));
	aColorScheme->SetStandardColor(StandardColor_Back,this->GetColorSchemeColor("Window"));
	aColorScheme->SetStandardColor(StandardColor_Text,this->GetColorSchemeColor("WindowText"));
	aColorScheme->SetStandardColor(StandardColor_MenuBack,this->GetColorSchemeColor("Menu"));
	aColorScheme->SetStandardColor(StandardColor_MenuText,this->GetColorSchemeColor("MenuText"));
	aColorScheme->SetStandardColor(StandardColor_Hilight,this->GetColorSchemeColor("Hilight"));
	aColorScheme->SetStandardColor(StandardColor_HilightText,this->GetColorSchemeColor("HilightText"));
	aColorScheme->SetStandardColor(StandardColor_3DFace,this->GetColorSchemeColor("ButtonFace"));
	aColorScheme->SetStandardColor(StandardColor_3DShadow,this->GetColorSchemeColor("ButtonShadow"));
	aColorScheme->SetStandardColor(StandardColor_GrayText,this->GetColorSchemeColor("GrayText"));
	aColorScheme->SetStandardColor(StandardColor_ButtonText,this->GetColorSchemeColor("ButtonText"));
	aColorScheme->SetStandardColor(StandardColor_3DHilight,this->GetColorSchemeColor("ButtonHilight"));
	aColorScheme->SetStandardColor(StandardColor_3DDarkShadow,this->GetColorSchemeColor("ButtonDkShadow"));
	aColorScheme->SetStandardColor(StandardColor_ToolTipText,this->GetColorSchemeColor("InfoText"));
	aColorScheme->SetStandardColor(StandardColor_ToolTipBack,this->GetColorSchemeColor("InfoWindow"));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SmartPtr<WONButton> WinSkin::CalcButton(const char *theSection, const char *theBitmapName, WONButton *orig)
{
	SmartPtr<RawImage> aButtonMasterImage = this->GetRawImage(theSection,theBitmapName);
	if(aButtonMasterImage.get()==NULL)
		return orig;

	bool isTab = false;
	bool isHeader = false;

	if(stricmp(theSection,"HeaderBar")==0)
	{
		isHeader = true;
		this->mSkin->mListCtrl->mHeaderHeight=aButtonMasterImage->mHeight;
	}
	else if(stricmp(theSection,"Tabs")==0)
	{
		this->mSkin->mTabCtrl->mTabBarHeight=aButtonMasterImage->mHeight;
		isTab = true;
	}

	SmartPtr<WONButton> aButton;
	if(isHeader)
		aButton = new WONHeaderButton;
	else
		aButton = new WONButton;

	int leftWidth = this->GetIntParam(theSection,"LeftWidth");
	int topHeight = this->GetIntParam(theSection,"TopHeight");
	int rightWidth = this->GetIntParam(theSection,"RightWidth");
	int bottomHeight = this->GetIntParam(theSection,"BottomHeight");
	int tile = this->GetIntParam(theSection,"Tile");
	bool mouseOver = this->GetIntParam(theSection,"MouseOver")?true:false;
	aButton->SetButtonFlags(0x0001,mouseOver);
//	aButton->SetTextPaddingAtLeast(leftWidth+2,topHeight+2,rightWidth+2,bottomHeight+2);

	int aNumFrames = this->GetIntParam(theSection,"FrameCount");
	if(aNumFrames<5)
		aNumFrames = 5;

	int aWidth = aButtonMasterImage->mWidth/aNumFrames;

	const char *colornames[] = { "NormalColour", "PressedColour", "DisabledColour", "FocusColour", "DefaultColour" };
	const char *fontnames[] = { "NormalFont", "PressedFont", "DisabledFont", "FocusFont", "DefaultFont" };

	SmartPtr<CropImageFilter> aCrop = new CropImageFilter;
	aCrop->SetSize(aWidth,aButtonMasterImage->mHeight);
	for(int i=0; i<5; i++)
	{
		SmartPtr<StretchImage> aStretchImage = NULL;
		if(i<5)
		{
			aCrop->SetPos(i*aWidth,0);
			SmartPtr<RawImage> aRawImage = aCrop->Filter(aButtonMasterImage);

			aStretchImage = new StretchImage(aRawImage,leftWidth,topHeight,rightWidth,bottomHeight);
			switch(tile)
			{
				case 0: aStretchImage->SetTile(false,false); break;
				case 1: aStretchImage->SetTile(true,true); break;
				case 2: aStretchImage->SetTile(true,false); break;
				case 3: aStretchImage->SetTile(false,true); break;
			}
		}

		int aColor = this->GetColor(theSection,colornames[i]);
		SmartPtr<Font> aFont = this->GetFont(theSection,fontnames[i]);
		if(i==0)
		{
			if(aFont.get()==NULL)
				aFont = this->mWindow->mManager->GetNamedFont("MSButton");
			if(aColor==-1)
				aColor = 0;
		}

		if(i==1 && isTab)
		{
			aButton->SetHasCheck(true);
			aButton->SetButtonState(0,aStretchImage,aColor,aFont,true);
		}
		else
			aButton->SetButtonState(i,aStretchImage,aColor,aFont);
	}

	aButton->SetTransparent(aButtonMasterImage->HasTransparency());
	return aButton;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
SmartPtr<WONButton> WinSkin::CalcCheckButton(const char *theBitmapName, WONButton *orig)
{
	SmartPtr<WONButton> aCheckbox = new WONButton;
	aCheckbox->SetPushOffsets(0,0);
	aCheckbox->SetHasCheck(true);
	aCheckbox->SetScaleImage(false);

	SmartPtr<RawImage> aButtonMasterImage = this->GetRawImage("Buttons",theBitmapName);
	int aNumPictures = this->GetIntParam("Buttons","EnhancedMode")?5:4;
	if(aButtonMasterImage.get()==NULL)
		return orig;

	int aWidth = aButtonMasterImage->mWidth/aNumPictures;
	aCheckbox->SetTextPadding(aWidth + 4,2,2,2);

	int aColor = this->GetColor("Buttons","NormalColour");
	SmartPtr<Font> aFont = this->GetFont("Buttons","NormalFont");
	if(aFont.get()==NULL)
		aFont = this->mWindow->mManager->GetNamedFont("MSButton");
	if(aColor==-1)
		aColor = 0;

	SmartPtr<CropImageFilter> aCrop = new CropImageFilter;
	aCrop->SetSize(aWidth,aButtonMasterImage->mHeight);
	for(int i=0; i<4; i++)
	{
		aCrop->SetPos(i*aWidth,0);
		SmartPtr<RawImage> aRawImage = aCrop->Filter(aButtonMasterImage);
		SmartPtr<NativeImage> aNativeImage = aRawImage->GetNative(this->mWindow->GetDisplayContext());

		bool checked = i&1?true:false;
		int anIndex = i>=2?2:0;

		aCheckbox->SetButtonState(anIndex,aNativeImage,aColor,aFont,checked);
		if(anIndex==0)
		{
			SmartPtr<RawImage> aRawImage2 = (RawImage*)aRawImage->Duplicate().get();
			SmartPtr<TintImageFilter> aTint = new TintImageFilter(100,100,150);
			aTint->Filter(aRawImage);
			aNativeImage = aRawImage->GetNative(this->mWindow->GetDisplayContext());

			aCheckbox->SetButtonState(3,aNativeImage,aColor,aFont,checked);

			aTint = new TintImageFilter(100,150,100);
			aTint->Filter(aRawImage2);
			aNativeImage = aRawImage2->GetNative(this->mWindow->GetDisplayContext());

			aCheckbox->SetButtonState(1,aNativeImage,aColor,aFont,checked);
		}
	}
	return aCheckbox;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WinSkin::CalcArrows()
{

	SmartPtr<RawImage> aScrollMasterImage = this->GetRawImage("Scrollbar","Image");
	if(aScrollMasterImage.get()==NULL)
		return;

	int aWidth = aScrollMasterImage->mWidth/23;
	bool mouseOver = this->GetIntParam("Scrollbar","MouseOver")?true:false;

	SmartPtr<CropImageFilter> aCrop = new CropImageFilter;
	aCrop->SetSize(aWidth,aScrollMasterImage->mHeight);

	int i,j;

	int curX = 0;
	for(i=0; i<4; i++)
	{
		Direction aDirection;
		SmartPtr<WONSimpleButton> aButton = new WONSimpleButton;
		switch(i)
		{
			case 0: aDirection = Direction_Left; break;
			case 1: aDirection = Direction_Right; break;
			case 2: aDirection = Direction_Up; break;
			case 3: aDirection = Direction_Down; break;
		}

		for(j=0; j<4; j++)
		{
			aCrop->SetPos(curX,0);
			curX += aWidth;
			SmartPtr<RawImage> aRawImage = aCrop->Filter(aScrollMasterImage);
			SmartPtr<NativeImage> aNativeImage = aRawImage->GetNative(this->mWindow->GetDisplayContext());
			aButton->SetImage(j,aNativeImage);
		}
		
		this->mSkin->mArrowButton[aDirection] = aButton;
	}

	const char *thumbName[] = { "HorzScrollThumb","VertScrollThumb","HorzScroll","VertScroll" };
	for(i=0; i<4; i++)
	{
		SmartPtr<RawImage> aThumbMasterImage = this->GetRawImage(thumbName[i],"Image");
		int leftWidth = this->GetIntParam(thumbName[i],"LeftWidth");
		int topHeight = this->GetIntParam(thumbName[i],"TopHeight");
		int rightWidth = this->GetIntParam(thumbName[i],"RightWidth");
		int bottomHeight = this->GetIntParam(thumbName[i],"BottomHeight");
		int tile = this->GetIntParam(thumbName[i],"tile");
		bool showDot = this->GetIntParam(thumbName[i],"ShowDot")?true:false;
		int aFrameCount = this->GetIntParam(thumbName[i],"FrameCount");
		int aNumImages = i<2?3:4;
		if(aFrameCount<aNumImages)
			aFrameCount = aNumImages;

		int aThumbWidth = aThumbMasterImage->mWidth/aFrameCount;
		SmartPtr<CropImageFilter> aThumbCrop = new CropImageFilter;
		aThumbCrop->SetSize(aThumbWidth,aThumbMasterImage->mHeight);
		
		SmartPtr<WONSimpleButton> aThumb;
		SmartPtr<WONSimpleButton> aGutter;
		
		if(i<2)
		{
			aThumb = new WONScrollThumb;
			aThumb->SetScaleImage(true);
		}
		else
		{
			aGutter = new WONSimpleButton;
			aGutter->SetScaleImage(true);
		}


		for(j=0; j<aNumImages; j++)
		{
			aThumbCrop->SetPos(j*aThumbWidth,0);
			SmartPtr<RawImage> aRawImage = aThumbCrop->Filter(aThumbMasterImage);
			SmartPtr<StretchImage> aStretchImage = new StretchImage(aRawImage,leftWidth,topHeight,rightWidth,bottomHeight);
			switch(tile)
			{
				case 0: aStretchImage->SetTile(false,false); break;
				case 1: aStretchImage->SetTile(true,true); break;
				case 2: aStretchImage->SetTile(true,false); break;
				case 3: aStretchImage->SetTile(false,true); break;
			}

			if(j!=aNumImages-1 || mouseOver)
			{
				if(i<2)
					;//aThumb->SetThumb(j,aStretchImage,false);
				else
					aGutter->SetImage(j,aStretchImage);
			}
			
			if(i<2)
			{
				aCrop->SetPos(curX,0);
				curX += aWidth;
				aRawImage = aCrop->Filter(aScrollMasterImage);

				if(j!=2 || mouseOver)
				{
					int index = j; if(index==2) index=3;
					if(showDot)
						aThumb->SetImage(index,new StretchCenterImage(aStretchImage,aRawImage->GetNative(this->mWindow->GetDisplayContext())));
					else
						aThumb->SetImage(index,aStretchImage);
				}
			}

			if(i==2 && j==0) // corner piece
			{
				aCrop->SetPos(curX,0);
				aRawImage = aCrop->Filter(aScrollMasterImage);
				this->mSkin->mScrollContainer->SetCorner(aRawImage->GetNative(this->mWindow->GetDisplayContext()));
			}
		}

		if(i<2)
			this->mSkin->mScrollThumb[i] = aThumb;
		else
			this->mSkin->mScrollGutter[i-2] = aGutter;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WinSkin::CalcComboButton()
{
	SmartPtr<RawImage> aMasterImage = this->GetRawImage("ComboButton","Image");
	SmartPtr<RawImage> anArrowImage = this->GetRawImage("ExtraImages","Image");
	if(aMasterImage.get()==NULL)
		return;

	int leftWidth = this->GetIntParam("ComboButton","LeftWidth");
	int topHeight = this->GetIntParam("ComboButton","TopHeight");
	int rightWidth = this->GetIntParam("ComboButton","RightWidth");
	int bottomHeight = this->GetIntParam("ComboButton","BottomHeight");
	int tile = this->GetIntParam("ComboButton","tile");
	bool mouseOver = this->GetIntParam("ComboButton","MouseOver")?true:false;

	SmartPtr<CropImageFilter> aCrop = new CropImageFilter;
	int aWidth = aMasterImage->mWidth/4;
	aCrop->SetSize(aWidth,aMasterImage->mHeight);
	
	SmartPtr<WONSimpleButton> aButton = new WONSimpleButton;
	aButton->SetScaleImage(true);

	int curX = 0;
	for(int i=0; i<4; i++)
	{
		aCrop->SetPos(curX,0);
		curX += aWidth;

		SmartPtr<RawImage> aRawImage = aCrop->Filter(aMasterImage);
		SmartPtr<StretchImage> aStretchImage = new StretchImage(aRawImage,leftWidth,topHeight,rightWidth,bottomHeight);
		switch(tile)
		{
			case 0: aStretchImage->SetTile(false,false); break;
			case 1: aStretchImage->SetTile(true,true); break;
			case 2: aStretchImage->SetTile(true,false); break;
			case 3: aStretchImage->SetTile(false,true); break;
		}
		SmartPtr<Image> anImage;


		if(i!=3 || mouseOver)
		{
			if(anArrowImage.get()!=NULL)
			{
				SmartPtr<CropImageFilter> aCrop = new CropImageFilter;
				aCrop->SetSize(anArrowImage->mWidth/5,anArrowImage->mHeight);
				aCrop->SetPos(anArrowImage->mWidth*i/5,0);
				anImage = new StretchCenterImage(aStretchImage,aCrop->Filter(anArrowImage)->GetNative(this->mWindow->GetDisplayContext()));
			}
			else
				anImage = aStretchImage;

			anImage->SetSize(aWidth,aMasterImage->mHeight);
			aButton->SetImage(i,anImage);
		}
	}
	this->mSkin->mComboButton = aButton;
	this->mSkin->mComboBox->mButtonInFrame=false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WinSkin::CalcTabCtrlFrame()
{
	SmartPtr<RawImage> aRawImage = this->GetRawImage("Tabs","Border",false);
	if(aRawImage.get()==NULL)
		return;

	int leftWidth = this->GetIntParam("Tabs","LeftWidth");
	int topHeight = this->GetIntParam("Tabs","TopHeight");
	int rightWidth = this->GetIntParam("Tabs","RightWidth");
	int bottomHeight = this->GetIntParam("Tabs","BottomHeight");
	int tile = this->GetIntParam("Tabs","tile");
	int tabBarOverlap = this->GetIntParam("Tabs","TabBarOverlap");
	if(tabBarOverlap==0)
		tabBarOverlap = 3;


	if(leftWidth + rightWidth >= aRawImage->mWidth)
		return;
	if(topHeight + bottomHeight >= aRawImage->mHeight)
		return;

	SmartPtr<StretchImage> aStretchImage = new StretchImage(aRawImage,leftWidth,topHeight,rightWidth,bottomHeight);
	switch(tile)
	{
		case 0: aStretchImage->SetTile(false,false); break;
		case 1: aStretchImage->SetTile(true,true); break;
		case 2: aStretchImage->SetTile(true,false); break;
		case 3: aStretchImage->SetTile(false,true); break;
	}

	aStretchImage->SetSkipCenter(true);

	this->mSkin->mTabCtrl->mBottomTabBar->mOverlap=0;
	this->mSkin->mTabCtrl->mBottomTabTopOffset=0;
	this->mSkin->mTabCtrl->mBottomTabLeftOffset=0;
	this->mSkin->mTabCtrl->mTabBarOverlap=tabBarOverlap;

	this->mSkin->mTabCtrl->SetFrame(aStretchImage.get());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WinSkin::CalcSunkEdge()
{
	SmartPtr<RawImage> aMasterImage = this->GetRawImage("SunkEdge","Image",false);
	if(aMasterImage.get()==NULL)
		return;

	int leftWidth = this->GetIntParam("SunkEdge","LeftWidth");
	int topHeight = this->GetIntParam("SunkEdge","TopHeight");
	int rightWidth = this->GetIntParam("SunkEdge","RightWidth");
	int bottomHeight = this->GetIntParam("SunkEdge","BottomHeight");
	int tile = this->GetIntParam("SunkEdge","tile");
	bool mouseOver = this->GetIntParam("SunkEdge","MouseOver")?true:false;

	SmartPtr<CropImageFilter> aCrop = new CropImageFilter;
	int aWidth = aMasterImage->mWidth/4;
	aCrop->SetSize(aWidth,aMasterImage->mHeight);
	
	int curX = 0;
	for(int i=0; i<4; i++)
	{
		aCrop->SetPos(curX,0);
		curX += aWidth;

		SmartPtr<RawImage> aRawImage = aCrop->Filter(aMasterImage);
		SmartPtr<StretchImage> aStretchImage = new StretchImage(aRawImage,leftWidth,topHeight,rightWidth,bottomHeight);
		switch(tile)
		{
			case 0: aStretchImage->SetTile(false,false); break;
			case 1: aStretchImage->SetTile(true,true); break;
			case 2: aStretchImage->SetTile(true,false); break;
			case 3: aStretchImage->SetTile(false,true); break;
		}

		aStretchImage->SetSkipCenter(true);
		if(i==0)
		{
			this->mSkin->mComboBox->SetFrame(aStretchImage.get());
//			mSkin->mInputBox->SetFrame(aStretchImage.get());
//			mSkin->mScrollContainer->SetFrame(aStretchImage.get());
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WinSkin::CalcPopupBack()
{
	SmartPtr<RawImage> aRawImage = this->GetRawImage("MenuBackground","Image");
	if(aRawImage.get()==NULL)
		return;

	int leftWidth = this->GetIntParam("MenuBackground","LeftWidth");
	int topHeight = this->GetIntParam("MenuBackground","TopHeight");
	int rightWidth = this->GetIntParam("MenuBackground","RightWidth");
	int bottomHeight = this->GetIntParam("MenuBackground","BottomHeight");
	int tile = this->GetIntParam("MenuBackground","tile");

	SmartPtr<StretchImage> aStretchImage = new StretchImage(aRawImage,leftWidth,topHeight,rightWidth,bottomHeight);
	switch(tile)
	{
		case 0: aStretchImage->SetTile(false,false); break;
		case 1: aStretchImage->SetTile(true,true); break;
		case 2: aStretchImage->SetTile(true,false); break;
		case 3: aStretchImage->SetTile(false,true); break;
	}

	this->mSkin->mPopupBack->SetFrameImage(aStretchImage);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WinSkin::CalcDialogBack()
{
	SmartPtr<RawImage> aRawImage = this->GetRawImage("MenuBackground","Image");
	if(aRawImage.get()==NULL)
		return;

	int leftWidth = this->GetIntParam("MenuBackground","LeftWidth");
	int topHeight = this->GetIntParam("MenuBackground","TopHeight");
	int rightWidth = this->GetIntParam("MenuBackground","RightWidth");
	int bottomHeight = this->GetIntParam("MenuBackground","BottomHeight");
	int tile = this->GetIntParam("MenuBackground","tile");

	SmartPtr<StretchImage> aStretchImage = new StretchImage(aRawImage,leftWidth,topHeight,rightWidth,bottomHeight);
	switch(tile)
	{
		case 0: aStretchImage->SetTile(false,false); break;
		case 1: aStretchImage->SetTile(true,true); break;
		case 2: aStretchImage->SetTile(true,false); break;
		case 3: aStretchImage->SetTile(false,true); break;
	}

	this->mSkin->mDialogBack->SetFrameImage(aStretchImage);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void WinSkin::CalcCloseButton()
{
	char section[256];
	int i;
	for(i=0; i<50; i++)
	{
		sprintf(section,"Button%d",i);
		if(this->GetIntParam(section,"Action",-1)==0)
			break;
	}
	if(i==50)
		return;

	SmartPtr<RawImage> aMasterImage = this->GetRawImage(section,"ButtonImage");
	if(aMasterImage.get()==NULL)
		return;

	int leftWidth = this->GetIntParam(section,"LeftWidth");
	int topHeight = this->GetIntParam(section,"TopHeight");
	int rightWidth = this->GetIntParam(section,"RightWidth");
	int bottomHeight = this->GetIntParam(section,"BottomHeight");
	int tile = this->GetIntParam(section,"tile");
	int numImages = this->GetIntParam("Personality","TripleImages")?6:3;
	bool mouseOver = this->GetIntParam(section,"MouseOver")?true:false;

	SmartPtr<CropImageFilter> aCrop = new CropImageFilter;
	int aWidth = aMasterImage->mWidth/numImages;
	aCrop->SetSize(aWidth,aMasterImage->mHeight);
	
	SmartPtr<WONSimpleButton> aButton = new WONSimpleButton;

	int curX = 0;
	for(i=0; i<3; i++)
	{
		aCrop->SetPos(curX,0);
		curX += aWidth;

		SmartPtr<RawImage> aRawImage = aCrop->Filter(aMasterImage);
		int index = i; if(i==2) index = 3;
		aButton->SetImage(index,aRawImage->GetNative(this->mWindow->GetDisplayContext()));
	}

	this->mSkin->mCloseButton = aButton;
}


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
WONSkin* WinSkin::Load(const char *theFilePath)
{
	this->mFontMap.clear();
	this->mColorMap.clear();

	WONFile aFile(theFilePath);

	this->mFileName = theFilePath;
	this->mFilePath = aFile.GetFileDirectory() + "\\";


	this->mWindow = Window::GetDefaultWindow();

//	mSkin->mInputBox = new WONInputBox;
//	mSkin->mInputBox->SetFrame(new ProgramImage(0,0,DrawFrame));
	
//	mSkin->mComboBox = new WONComboBox;
//	mSkin->mComboBox->SetFrame(new ProgramImage(0,0,DrawFrame));

//	mSkin->mScrollContainer = new WONScrollContainer;
//	mSkin->mScrollContainer->SetFrame(new ProgramImage(0,0,DrawFrame));

	this->CalcColorScheme();
	
	this->mSkin->mPushButton = this->CalcButton("Buttons","Bitmap",this->mSkin->mPushButton);
	this->mSkin->mHeaderButton = (WONHeaderButton*)this->CalcButton("HeaderBar","Image",this->mSkin->mHeaderButton).get();
	this->mSkin->mTabButton = this->CalcButton("Tabs","Image",this->mSkin->mTabButton);
	this->mSkin->mCheckbox = this->CalcCheckButton("CheckButton",this->mSkin->mCheckbox);
	this->mSkin->mRadioButton = this->CalcCheckButton("RadioButton",this->mSkin->mRadioButton);

	this->mSkin->mPushButton->SetButtonFlags(0x0400,true);
	this->mSkin->mRadioButton->SetButtonFlags(0x0008,true);

	this->CalcArrows();
	this->CalcComboButton();
	this->CalcTabCtrlFrame();
	this->CalcSunkEdge();
	this->CalcPopupBack();
	this->CalcDialogBack();
	this->CalcCloseButton();

	return this->mSkin;
}



}
