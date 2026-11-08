// Skinned button: per-state images, colours and fonts, text layout and painting
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyWONButton.h"
#include "ee/wonapi/WONComponentFlags.h"
namespace WONAPI {

// FUNCTION: WON 0x00718060
// Sets the button text in every font's wrapped text and refits the height
void WONButton::SetTextHook(const GUIString &theText)
{
	FontTextMap::iterator anItr = this->mFontTextMap.begin();
	while(anItr!=this->mFontTextMap.end())
	{
		WrappedText *aText = anItr->second.get();
		aText->SetText(theText);
		++anItr;
	}

	this->AdjustHeightToFitText(); 
}

// FUNCTION: WON 0x00718100
// After the base handling, re-wraps every font's text to the inner width (when wrapping) and refits the height
void WONButton::Virtual08()
{
	Button::Virtual08();
	if(!this->mWrapText)
		return;

	FontTextMap::iterator anItr = this->mFontTextMap.begin();
	while(anItr!=this->mFontTextMap.end())
	{
		WrappedText *aText = anItr->second.get();
		aText->SetMaxWidth(this->Width()-this->mLeftTextPad-this->mRightTextPad);
		++anItr;
	}

	this->AdjustHeightToFitText();
}

// FUNCTION: WON 0x007181A0
// Resizes the button to the text height plus padding, when height fitting is on
void WONButton::AdjustHeightToFitText()
{
	if(!this->mAdjustHeightToFitText)
		return;

	int width,height;
	this->GetTextSize(width,height);

	if(this->Height()-this->mTopTextPad-this->mBottomTextPad!=height)
		this->SetSize(this->Width(),height+this->mTopTextPad+this->mBottomTextPad);
}

// FUNCTION: WON 0x00718220
// Turns text wrapping on or off for every font's wrapped text
void WONButton::SetWrapText(bool wrap)
{
	if(this->mWrapText==wrap)
		return;

	this->mWrapText = wrap;
	FontTextMap::iterator anItr = this->mFontTextMap.begin();
	while(anItr!=this->mFontTextMap.end())
	{
		WrappedText *aText = anItr->second.get();
		aText->SetWrap(wrap);

		++anItr;
	}
}

// FUNCTION: WON 0x007182C0
// Sets the image, text colour and font of one button state (normal or checked); the first state without
// a font gets the default font; one wrapped text is shared per font
void WONButton::SetButtonState(int theState, Image *theImage, int theColor, Font *theFont, bool checked)
{
	int checkedIndex = checked?1:0;
	ButtonState &aState = this->mStates[theState][checkedIndex];
	if(checkedIndex)
		this->mHasCheck = true;

	SmartPtr<Font> aFont;
	if(this->mFontTextMap.empty() && theFont==NULL)
		theFont = aFont = WONAPI::WindowManager::GetDefaultWindowManager()->GetDefaultFont();

	if(theFont!=NULL)
	{
		FontTextMap::iterator anItr = this->mFontTextMap.insert(FontTextMap::value_type(theFont,NULL)).first;
		if(anItr->second.get()==NULL)
		{
			SmartPtr<WrappedText> aText = new WrappedText;
			aText->SetCentered(this->mHAlign==HorzAlign_Center);
			aText->SetWrap(false);
			aText->SetFont(theFont);
			anItr->second = aText;
		}

		aState.mText = anItr->second;
	}

	aState.mImage = theImage;
	aState.mColor = theColor;

}

// FUNCTION: WON 0x00718580
// Returns the largest width and height of the button text over all fonts
void WONButton::GetTextSize(int &width, int &height)
{
	width = 0; height = 0;
	FontTextMap::iterator anItr = this->mFontTextMap.begin();
	while(anItr!=this->mFontTextMap.end())
	{
		WrappedText *aText = anItr->second.get();
		if(aText->GetWidth() > width)
			width = aText->GetWidth();

		if(aText->GetHeight() > height)
			height = aText->GetHeight();

		++anItr;
	}
}

// FUNCTION: WON 0x00718640
// Returns the text size, grown to the largest state image unless images are scaled, plus the text padding
void WONButton::GetDesiredSize(int &width, int &height)
{
	this->GetTextSize(width,height);
	for(int i=0; i<5; i++)
	{
		for(int j=0; j<2; j++)
		{
			Image *anImage = this->mStates[i][j].mImage.get();
			if(anImage==NULL)
				continue;
			if(!this->mScaleImage && anImage->mWidth>width)
				width = anImage->mWidth;

			if(!this->mScaleImage && anImage->mHeight > height)
				height = anImage->mHeight;
		}
	}


	width += this->mLeftTextPad + this->mRightTextPad;
	height += this->mTopTextPad + this->mBottomTextPad;
}

// FUNCTION: WON 0x007186E0
// Sets the horizontal text alignment; only switches to or from centred (left <-> right is ignored)
void WONButton::SetHorzAlign(HorzAlign theHorzAlign)
{
	if(theHorzAlign==this->mHAlign)
		return;

	if(this->mHAlign!=HorzAlign_Center && theHorzAlign!=HorzAlign_Center)
		return;

	this->mHAlign = theHorzAlign;

	FontTextMap::iterator anItr = this->mFontTextMap.begin();
	while(anItr!=this->mFontTextMap.end())
	{
		WrappedText *aText = anItr->second.get();
		aText->SetCentered(this->mHAlign==HorzAlign_Center); 
		++anItr;
	}
}

// FUNCTION: WON 0x00718790
// Sets the text colour of every state; with scaled images only of the normal and default states
void WONButton::SetTextColor(int theColor)
{
	if(!this->mScaleImage)
	{
		for(int i=0; i<5; i++)
			for(int j=0; j<2; j++)
				this->mStates[i][j].mColor = theColor;
	}
	else
	{
		this->mStates[0][0].mColor = theColor;
		this->mStates[0][1].mColor = theColor;
		this->mStates[4][0].mColor = theColor;
		this->mStates[4][1].mColor = theColor;
	}
}

// FUNCTION: WON 0x007187E0
// Makes every state use the normal state's wrapped text, set to the given font
void WONButton::SetFont(Font *theFont)
{
	this->mFontTextMap.clear();
	SmartPtr<WrappedText> aText = this->mStates[0][0].mText.get();
	aText->SetFont(theFont);
	this->mFontTextMap[theFont] = aText;
	
	for(int i=0; i<5; i++)
		for(int j=0; j<2; j++)
			this->mStates[i][j].mText = aText;
}

// FUNCTION: WON 0x00718970
// Copies another button's padding, alignment, flags, colours and state images (duplicated when scaled),
// with fresh wrapped texts per font
void WONButton::CopyAttributes(WONButton *from)
{
	Button::CopyAttributes(from);
	this->mLeftTextPad		= from->mLeftTextPad;
	this->mTopTextPad			= from->mTopTextPad;
	this->mRightTextPad		= from->mRightTextPad;
	this->mBottomTextPad		= from->mBottomTextPad;
	this->mLeftFocusPad		= from->mLeftFocusPad;
	this->mTopFocusPad		= from->mTopFocusPad;
	this->mRightFocusPad		= from->mRightFocusPad;
	this->mBottomFocusPad		= from->mBottomFocusPad;
	this->mPushDX				= from->mPushDX;
	this->mPushDY				= from->mPushDY;
	this->mHasCheck			= from->mHasCheck;
	this->mScaleImage			= from->mScaleImage;
	this->mHAlign				= from->mHAlign;
	this->mVAlign				= from->mVAlign;
	this->mDrawGrayText		= from->mDrawGrayText;
	this->mIsTransparent		= from->mIsTransparent;
	this->mFocusStyle			= from->mFocusStyle;
	this->mDefaultTextColor	= from->mDefaultTextColor;
	if(from->ComponentFlagSet(ComponentFlag_GrabBG))
		this->SetComponentFlags(ComponentFlag_GrabBG,true);
	if(from->ComponentFlagSet(ComponentFlag_Translucent))
		this->SetComponentFlags(ComponentFlag_Translucent,true);

	this->mFontTextMap = from->mFontTextMap;
	FontTextMap::iterator anItr = this->mFontTextMap.begin();
	while(anItr!=this->mFontTextMap.end())
	{
		anItr->second = new WrappedText;
		anItr->second->SetCentered(this->mHAlign==HorzAlign_Center);
		anItr->second->SetWrap(this->mWrapText);
		anItr->second->SetFont(anItr->first);
		++anItr;
	}


	for(int i=0; i<5; i++)
	{
		for(int j=0; j<2; j++)
		{
			ButtonState &src = from->mStates[i][j];
			ButtonState &dst = this->mStates[i][j];

			dst.mColor = src.mColor;

			if(src.mImage.get()!=NULL)
			{
				if(this->mScaleImage)
					dst.mImage = src.mImage->Duplicate();
				else
					dst.mImage = src.mImage;
			}

			if(src.mText.get()!=NULL)
				dst.mText = this->mFontTextMap[src.mText->mFont.get()];
		}
	}
}

// FUNCTION: WON 0x00718DB0
// Draws the button text aligned and pushed when pressed, embossed grey when disabled, with a dotted focus
// rectangle after dialog-key use
void WONButton::DrawButtonText(Graphics &g, WrappedText *theText, int theColor)
{
	if(theText==NULL)
		return;

	int x,y;
	if(this->mScaleImage)
	{
		switch(this->mHAlign)
		{
			case HorzAlign_Left: x = this->mLeftTextPad; break;
			case HorzAlign_Center: x = (this->Width() - theText->GetWidth() - this->mLeftTextPad - this->mRightTextPad)/2 + this->mLeftTextPad; break;
			case HorzAlign_Right: x = this->Width() - theText->GetWidth() - this->mRightTextPad; break;

		}
		y = (this->Height() - theText->GetHeight())/2;
		if(this->IsPressed())
		{
			x+=this->mPushDX;
			y+=this->mPushDY;
		}
	}
	else
	{
		x = this->mLeftTextPad;
		y = (this->Height() - theText->GetHeight())/2;
	}

	if(this->IsDisabled() && this->mDrawGrayText)
	{
		g.SetSysColor(StandardColor_3DHilight);
		theText->Paint(g,x+1,y+1);
		g.SetSysColor(StandardColor_GrayText);
		theText->Paint(g,x,y);
	}
	else
	{
		g.SetColor(theColor);
		theText->Paint(g,x,y);
		if(this->mFocusStyle && this->HasFocus() && this->mParent->mHasUsedDialogKeys)
		{
			if(this->mScaleImage)
				g.DrawDottedRect(this->mLeftFocusPad,this->mTopFocusPad,this->Width()-this->mLeftFocusPad-this->mRightFocusPad,this->Height()-this->mTopFocusPad-this->mBottomFocusPad);
			else
				g.DrawDottedRect(this->mLeftTextPad-1,y-1,theText->GetWidth()+2,theText->GetHeight()+2);
		}
		
	}
}

// FUNCTION: WON 0x00718F90
// Picks the current state (disabled, pressed, mouse-over, default, normal; checked or not) and returns its
// colour, text and image, falling back to the normal state and the default text colour
void WONButton::GetButtonState(Graphics &g, int *theColor, WrappedText **theText, Image **theImage)
{
	int aState = 0;
	if(this->IsDisabled())
		aState = 2;
	else if(this->IsPressed())
		aState = 1;
	else if(this->IsMouseOver() && ((mButtonFlags&1)!=0))
		aState = 3;
	else if(this->IsDefaultButton())
		aState = 4;

	int aCheckedIndex = (this->mHasCheck && this->IsChecked())?1:0;

	ButtonState &aStateStruct = this->mStates[aState][aCheckedIndex];

	Image *anImage = aStateStruct.mImage.get();
	int aColor = aStateStruct.mColor;
	WrappedText *aText = aStateStruct.mText.get();

	if(anImage==NULL) anImage = this->mStates[0][aCheckedIndex].mImage.get();
	if(aColor==-1) aColor = this->mStates[0][aCheckedIndex].mColor;
	if(aText==NULL) aText = this->mStates[0][aCheckedIndex].mText.get();

	if(aCheckedIndex==1)
	{
		if(anImage==NULL) anImage = this->mStates[0][0].mImage.get();
		if(aColor==-1) aColor = this->mStates[0][0].mColor;
		if(aText==NULL) aText = this->mStates[0][0].mText.get();
	}

	if(aColor==-1)
		aColor = this->mDefaultTextColor;

	if(theText) *theText = aText;
	if(theColor) *theColor = aColor;
	if(theImage) *theImage = anImage;
}

// FUNCTION: WON 0x00719080
// Paints the button: face colour unless transparent, the state image (scaled or vertically aligned), then the text
void WONButton::Paint(Graphics &g)
{
	Button::Paint(g);
	if(!this->mIsTransparent)
	{
		g.SetSysColor(StandardColor_3DFace);
		g.FillRect(0,0,this->Width(),this->Height());
	}

	Image *anImage; 
	int aColor; 
	WrappedText *aText; 
	this->GetButtonState(g,&aColor,&aText,&anImage);

	if(this->mScaleImage)
	{
		if(anImage!=NULL)
		{
			if(anImage->mWidth!=this->Width() || anImage->mHeight!=this->Height())
				anImage->SetSize(this->Width(),this->Height());

			g.DrawImage(anImage,0,0);
		}
		this->DrawButtonText(g,aText,aColor);
	}
	else
	{
		int y = 0;

		if(anImage!=NULL)
		{
			int aHeight = anImage->mHeight;
			if(this->mVAlign==HorzAlign_Center) y = (this->Height() - aHeight - this->mTopTextPad - this->mBottomTextPad)/2;
			else if(this->mVAlign==VertAlign_Bottom) y = this->Height() - aHeight - this->mTopTextPad - this->mBottomTextPad;

			g.DrawImage(anImage,0,y + this->mTopTextPad);
		}
		this->DrawButtonText(g,aText,aColor);
	}
}


// FUNCTION: WON 0x00718200
// Turns height fitting on or off and refits the height when it changed
void WONButton::SetAdjustHeightToFitText(bool adjust)
{
	if(adjust==this->mAdjustHeightToFitText)
		return;

	this->mAdjustHeightToFitText = adjust;
	this->AdjustHeightToFitText();
}


// FUNCTION: WON 0x007182a0
// Sets whether the face is left unpainted; transparent buttons grab the background
void WONButton::SetTransparent(bool isTransparent)
{
	this->mIsTransparent = isTransparent;
	if(isTransparent)
		this->SetComponentFlags(ComponentFlag_GrabBG,true);
}
}

namespace WONAPI {
// FUNCTION: WON 0x007180e0
// Returns the button text, or an empty string when the button has no font yet
const GUIString& WONButton::GetText()
{
    if(this->mFontTextMap.empty()) return gEmptyGUIString;
    return this->mFontTextMap.begin()->second->GetText();
}
}

namespace WONAPI {
// FUNCTION: WON 0x00718780
// Sets the vertical alignment of the button image
void WONButton::SetVertAlign(VertAlign align)
{
    this->mVAlign=align;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00722490
// (public: class std::_Tree<class WONAPI::Font *,struct std::pair<class WONAPI::Font * const,class WONAPI::SmartPtr<class WONAPI::WrappedText> >,struct s)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071ba60
// (public: struct std::pair<class std::_Tree<class WONAPI::Font *,struct std::pair<class WONAPI::Font * const,class WONAPI::SmartPtr<class WONAPI::Wrappe)
// TEMPLATE: WON 0x0071b8e0
// (public: class std::_Tree<class WONAPI::Font *,struct std::pair<class WONAPI::Font * const,class WONAPI::SmartPtr<class WONAPI::WrappedText> >,struct s)
// TEMPLATE: WON 0x0071d8f0
// (public: class std::_Tree<class WONAPI::Font *,struct std::pair<class WONAPI::Font * const,class WONAPI::SmartPtr<class WONAPI::WrappedText> >,struct s)
// TEMPLATE: WON 0x0071d9a0
// (protected: class std::_Tree<class WONAPI::Font *,struct std::pair<class WONAPI::Font * const,class WONAPI::SmartPtr<class WONAPI::WrappedText> >,struc)
// TEMPLATE: WON 0x00722980
// (protected: struct std::_Tree<class WONAPI::Font *,struct std::pair<class WONAPI::Font * const,class WONAPI::SmartPtr<class WONAPI::WrappedText> >,stru)
