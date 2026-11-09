// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONComponentFlags.h"
using namespace WONAPI;

// FUNCTION: WON 0x006fe850
// Returns the button's text
const GUIString& MSButtonBase::GetText() { return this->mText->mText; }

// FUNCTION: WON 0x006fe880
// Sets the button's text and refits the height to it if asked to
void MSButtonBase::SetTextHook(const GUIString& text) {
    this->mText->SetText(text);
    this->AdjustHeightToFitText();
}

// FUNCTION: WON 0x006fe8a0
// Sets whether the face is left unpainted; a transparent button grabs the background behind it
void MSButtonBase::SetTransparent(bool transparent) {
    this->mIsTransparent=transparent;
    if(transparent) this->SetComponentFlags(ComponentFlag_GrabBG,true);
}

// FUNCTION: WON 0x006fe8c0
// Returns the text size plus the text padding on each side
void MSButtonBase::GetDesiredSize(int& width,int& height) {
    width=this->mText->GetWidth()+this->mLeftTextPad+this->mRightTextPad;
    height=this->mText->GetHeight()+this->mTopTextPad+this->mBottomTextPad;
}

// FUNCTION: WON 0x006fe980
// Resizes the button's height to the text plus padding, when height fitting is on
void MSButtonBase::AdjustHeightToFitText() {
    if(!this->mAdjustHeightToFitText) return;
    if(this->Height()-this->mTopTextPad-this->mBottomTextPad!=this->mText->GetHeight())
        this->SetSize(this->Width(),this->mText->GetHeight()+this->mTopTextPad+this->mBottomTextPad);
}

// FUNCTION: WON 0x006fe9f0
// Turns height fitting on or off, refitting at once when it changes
void MSButtonBase::SetAdjustHeightToFitText(bool adjust) {
    if(adjust==this->mAdjustHeightToFitText) return;
    this->mAdjustHeightToFitText=adjust;
    this->AdjustHeightToFitText();
}

// FUNCTION: WON 0x006fea10
// Relays out the button: wraps the text to the width inside the padding and refits the height
void MSButtonBase::Virtual08() {
    Component::Virtual08();
    this->mText->SetMaxWidth(this->Width()-this->mLeftTextPad-this->mRightTextPad);
    this->AdjustHeightToFitText();
}

// FUNCTION: WON 0x006fea40
// Draws the aligned text (embossed gray when disabled) and, once dialog keys were used, the focus
// rectangle around the window or the text
void MSButtonBase::DrawButtonText(Graphics& g,int xoff,int yoff) {
    int textWidth=this->mText->GetWidth();
    int textHeight=this->mText->GetHeight();
    int width=this->Width()-this->mLeftTextPad-this->mRightTextPad;
    int height=this->Height()-this->mTopTextPad-this->mBottomTextPad;
    int x,y;
    switch(this->mHAlign) {
    case HorzAlign_Left: x=0;break;
    case HorzAlign_Center: x=(width-textWidth)/2;break;
    case HorzAlign_Right: x=width-textWidth;break;
    default:x=0;break;
    }
    switch(this->mVAlign) {
    case VertAlign_Top:y=0;break;
    case VertAlign_Center:y=(height-textHeight)/2;break;
    case VertAlign_Bottom:y=height-textHeight;break;
    default:y=0;break;
    }
    x+=xoff+this->mLeftTextPad;
    y+=yoff+this->mTopTextPad;
    if(this->IsDisabled()) {
        g.SetSysColor(StandardColor_3DHilight);
        this->mText->Paint(g,x+1,y+1);
        g.SetSysColor(StandardColor_GrayText);
    } else if(this->mTextColor<0) g.SetSysColor(this->mStandardTextColor);
    else g.SetColor(this->mTextColor);
    this->mText->Paint(g,x,y);
    if(!this->HasFocus())
        return;
    Container* parent=this->mParent;
    if(parent==NULL || !parent->mHasUsedDialogKeys)
        return;
    if(this->mTextColor<0) g.SetSysColor(this->mStandardTextColor);
    else g.SetColor(this->mTextColor);
    switch(this->mFocusStyle) {
    case FocusStyle_AroundWindow:g.DrawDottedRect(4,3,this->Width()-8,this->Height()-7);break;
    case FocusStyle_AroundText:g.DrawDottedRect(x-1,y-1,textWidth+2,textHeight+2);break;
    }
}

// FUNCTION: WON 0x006fe6e0
// Creates a button base with unwrapped centred text in the MSButton font and 2-pixel padding
MSButtonBase::MSButtonBase() {
    this->mText=new WrappedText;
    this->mText->SetWrap(false);
    this->mText->SetFont(this->GetNamedFont("MSButton"));
    this->mStandardTextColor=StandardColor_ButtonText;
    this->mTextColor=-1;
    this->mHAlign=HorzAlign_Center;
    this->mVAlign=VertAlign_Center;
    this->mFocusStyle=FocusStyle_AroundWindow;
    this->mTopTextPad=this->mBottomTextPad=this->mLeftTextPad=this->mRightTextPad=2;
    this->mIsTransparent=false;
    this->mAdjustHeightToFitText=false;
}
// FUNCTION: WON 0x006fe910
// Destroys the button base (members only)
MSButtonBase::~MSButtonBase() {}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006fe860
// (public: virtual void * __thiscall WONAPI::MSButtonBase::`scalar deleting destructor'(unsigned int)    [??_GMSButtonBase@WONAPI@@UAEPAXI@Z])
