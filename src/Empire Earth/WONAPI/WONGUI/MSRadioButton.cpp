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
using namespace WONAPI;
// Radio circle size (13 x 13) and the replaceable radio painter
int MSRadioButton::mRadioWidth=13;
int MSRadioButton::mRadioHeight=13;
MSRadioButton::RadioPainterFunc MSRadioButton::RadioPainter=MSRadioButton::DefaultRadioPainterFunc;
// FUNCTION: WON 0x6ff5c0
// Draws the standard radio circle, pressed (flag 0x20) and checked (flag 4) as the button's flags say
void MSRadioButton::DefaultRadioPainterFunc(Graphics& g,MSRadioButton* button) {
    MSGraphicsUtil::DrawRadio(g,0,0,MSRadioButton::mRadioWidth,MSRadioButton::mRadioHeight,(button->mButtonFlags&0x20)!=0,(button->mButtonFlags&4)!=0);
}
// FUNCTION: WON 0x6ff600
// Paints the radio button: face background unless transparent, the radio circle aligned vertically
// between the text pads, then the button text
void MSRadioButton::Paint(Graphics& g) {
    Component::Paint(g);
    g.SetSysColor(StandardColor_3DFace);
    if(!this->mIsTransparent) g.FillRect(0,0,this->Width(),this->Height());
    int height=MSRadioButton::mRadioHeight;
    int y=0;
    if(this->mVAlign==VertAlign_Center) y=(this->Height()-height-this->mTopTextPad-this->mBottomTextPad)/2;
    else if(this->mVAlign==VertAlign_Bottom) y=this->Height()-height-this->mTopTextPad-this->mBottomTextPad;
    g.Translate(0,this->mTopTextPad+y);
    MSRadioButton::RadioPainter(g,this);
    g.Translate(0,-y-this->mTopTextPad);
    this->DrawButtonText(g,0,0);
}
