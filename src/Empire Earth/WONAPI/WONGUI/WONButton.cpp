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
#include "rust/ee_won.h"
#include "ee/wonapi/WONGUIDraw.h"
// GetButtonState's state and fallback order are ee_won's guifocus module (rust/crates/ee_won/src/guifocus.rs).
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
// The paint's inputs: the button's size, flags and pads, its state image and text (guidraw.rs EeWonButtonPaint)
static EeWonButtonPaint WONButtonPaintOf(WONButton* b, Image* theImage, WrappedText* theText, int theColor)
{
	EeWonButtonPaint p;
	p.width = b->Width();
	p.height = b->Height();
	p.transparent = b->mIsTransparent;
	p.scale_image = b->mScaleImage;
	p.has_image = theImage!=NULL;
	p.has_text = theText!=NULL;
	p.pressed = b->IsPressed();
	p.disabled = b->IsDisabled();
	p.draw_gray_text = b->mDrawGrayText;
	p.focus_shown = b->mFocusStyle && b->HasFocus() && b->mParent->HasUsedDialogKeys();
	p.h_align = b->mHAlign;
	p.v_align = b->mVAlign;
	p.image_width = theImage ? theImage->mWidth : 0;
	p.image_height = theImage ? theImage->mHeight : 0;
	p.text_width = theText ? theText->GetWidth() : 0;
	p.text_height = theText ? theText->GetHeight() : 0;
	p.color = theColor;
	p.left_text_pad = b->mLeftTextPad;
	p.top_text_pad = b->mTopTextPad;
	p.right_text_pad = b->mRightTextPad;
	p.bottom_text_pad = b->mBottomTextPad;
	p.left_focus_pad = b->mLeftFocusPad;
	p.top_focus_pad = b->mTopFocusPad;
	p.right_focus_pad = b->mRightFocusPad;
	p.bottom_focus_pad = b->mBottomFocusPad;
	p.push_dx = b->mPushDX;
	p.push_dy = b->mPushDY;
	return p;
}

void WONButton::DrawButtonText(Graphics &g, WrappedText *theText, int theColor)
{
	if(theText==NULL)
		return;

	// guidraw.rs button_text: the place by the alignment and push, the grey emboss, the focus rectangle
	EeWonButtonPaint p = WONButtonPaintOf(this,NULL,theText,theColor);
	WONDrawSlots aSlots;
	aSlots.mText = theText;
	WONDrawOps anOps([&](EeWonDrawOp* o, size_t n) { return ee_won_gui_draw_button_text(&p,o,n); });
	WONDrawRun(g,anOps,aSlots);
}

// FUNCTION: WON 0x00718F90
// Picks the current state (disabled, pressed, mouse-over, default, normal; checked or not) and returns its
// colour, text and image, falling back to the normal state and the default text colour
void WONButton::GetButtonState(Graphics &g, int *theColor, WrappedText **theText, Image **theImage)
{
	int aState = 0;
	int aCheckedIndex = ee_won_gui_button_state(this->IsDisabled(),(uint32_t)this->GetButtonFlags(),this->IsDefaultButton(),
		this->mHasCheck,&aState);

	// the state's own cell, then the normal state's in the same column, then (checked) the normal unchecked one
	Image *anImage = NULL;
	int aColor = -1;
	WrappedText *aText = NULL;
	int s = 0, c = 0;
	for(int k = 0; ee_won_gui_button_state_cell(aState,aCheckedIndex,k,&s,&c); k++)
	{
		ButtonState &aCell = this->mStates[s][c];
		if(anImage==NULL) anImage = aCell.mImage.get();
		if(aColor==-1) aColor = aCell.mColor;
		if(aText==NULL) aText = aCell.mText.get();
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

	Image *anImage;
	int aColor;
	WrappedText *aText;
	this->GetButtonState(g,&aColor,&aText,&anImage);

	// guidraw.rs button: the face, the state image scaled or aligned, then the text
	EeWonButtonPaint p = WONButtonPaintOf(this,anImage,aText,aColor);
	WONDrawSlots aSlots;
	aSlots.mImages[0] = anImage;
	aSlots.mText = aText;
	WONDrawOps anOps([&](EeWonDrawOp* o, size_t n) { return ee_won_gui_draw_button(&p,o,n); });
	WONDrawRun(g,anOps,aSlots);
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
