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

// FUNCTION: WON 0x006f9d80
// Creates an empty, clipped, top-left aligned label with the Label font, no wrapping and no background colour
Label::Label() {
    this->mColor=(int)0x80000005;
    this->mAlign38=HorzAlign_Left;
    this->mAlign3C=VertAlign_Top;
    this->mWrappedText=new WrappedText;
    this->mWrappedText->SetFont(this->GetNamedFont("Label"));
    this->mWrappedText->SetWrap(false);
    this->mLeftPad=this->mRightPad=this->mTopPad=this->mBottomPad=0;
    this->mAdjustHeightToFitText=false;
    this->SetComponentFlags(ComponentFlag_Clip,true);
    this->mBackground.SetColor(-1);
}

// FUNCTION: WON 0x006f9f20
// Sets the text wrap width to the label width minus paddings (5 if negative) and refits the height
void Label::SetMaxWidth() {
    int width=this->Width()-this->mLeftPad-this->mRightPad;
    if(width<0) width=5;
    this->mWrappedText->SetMaxWidth(width);
    this->AdjustHeightToFitText();
}

// FUNCTION: WON 0x006f9f50
// Destroys the label (members only)
Label::~Label() {}

// FUNCTION: WON 0x006fa020
// Runs the base component handler, then updates the text wrap width
void Label::Virtual08() {
    Component::Virtual08();
    this->SetMaxWidth();
}

// FUNCTION: WON 0x006fa040
// Sets the horizontal alignment; centred alignment also centres the wrapped lines
void Label::SetHorzAlign(HorzAlign align) {
    this->mAlign38=align;
    this->mWrappedText->SetCentered(align==HorzAlign_Center);
}

// FUNCTION: WON 0x006fa060
// Sets the label text, refits the height and optionally repaints
void Label::SetText(const GUIString& text,bool invalidate) {
    this->mWrappedText->SetText(text);
    this->AdjustHeightToFitText();
    if(invalidate) this->Invalidate();
}

// FUNCTION: WON 0x006fa090
// When fit-to-text is on, resizes the label's height to the wrapped text plus paddings
void Label::AdjustHeightToFitText() {
    if(!this->mAdjustHeightToFitText) return;
    if(this->Height()-this->mTopPad-this->mBottomPad!=this->mWrappedText->GetHeight())
        this->SetSize(this->Width(),this->mWrappedText->GetHeight()+this->mTopPad+this->mBottomPad);
}

// FUNCTION: WON 0x006fa100
// Turns fit-to-text height on or off, refitting when it changes
void Label::SetAdjustHeightToFitText(bool adjust) {
    if(adjust==this->mAdjustHeightToFitText) return;
    this->mAdjustHeightToFitText=adjust;
    this->AdjustHeightToFitText();
}

// FUNCTION: WON 0x006fa120
// Turns word wrapping on or off and refits the height
void Label::SetWrap(bool wrap) {
    this->mWrappedText->SetWrap(wrap);
    this->AdjustHeightToFitText();
}

// FUNCTION: WON 0x006fa140
// Returns the text size plus paddings as the preferred size
void Label::GetDesiredSize(int& width,int& height) {
    width=this->mWrappedText->GetWidth()+this->mLeftPad+this->mRightPad;
    height=this->mWrappedText->GetHeight()+this->mTopPad+this->mBottomPad;
}

// FUNCTION: WON 0x006fa1a0
// Draws the background, then the text aligned inside the padded area in the text colour
void Label::Paint(Graphics& g) {
    Component::Paint(g);
    int x=0,y=0;
    this->mBackground.Paint(g,0,0,this->Width(),this->Height());
    int textWidth=this->mWrappedText->GetWidth();
    int textHeight=this->mWrappedText->GetHeight();
    int width=this->Width()-this->mRightPad-this->mLeftPad;
    int height=this->Height()-this->mBottomPad-this->mTopPad;
    switch(this->mAlign38) {
    case HorzAlign_Left:x=0;break;
    case HorzAlign_Center:x=(width-textWidth)/2;break;
    case HorzAlign_Right:x=width-textWidth;break;
    }
    switch(this->mAlign3C) {
    case VertAlign_Top:y=0;break;
    case VertAlign_Center:y=(height-textHeight)/2;break;
    case VertAlign_Bottom:y=height-textHeight;break;
    }
    x+=this->mLeftPad;
    y+=this->mTopPad;
    g.SetColor(this->mColor);
    this->mWrappedText->Paint(g,x,y);
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006f9f00
// (public: virtual void * __thiscall WONAPI::Label::`scalar deleting destructor'(unsigned int)    [??_GLabel@WONAPI@@UAEPAXI@Z])
