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
// Check box size and the replaceable function that paints the box
int MSCheckbox::mCheckboxWidth=13;
int MSCheckbox::mCheckboxHeight=13;
MSCheckbox::CheckboxPainterFunc MSCheckbox::CheckboxPainter=MSCheckbox::DefaultCheckboxPainterFunc;
// FUNCTION: WON 0x6ff430
// Paints the default check box square, pressed and checked as the button's flags say
void MSCheckbox::DefaultCheckboxPainterFunc(Graphics& g,MSCheckbox* button) {
    MSGraphicsUtil::DrawCheck(g,0,0,MSCheckbox::mCheckboxWidth,MSCheckbox::mCheckboxHeight,(button->mButtonFlags&0x20)!=0,(button->mButtonFlags&4)!=0);
}
// FUNCTION: WON 0x6ff470
// Paints the check box: face, the box aligned vertically by the painter function, then the text
void MSCheckbox::Paint(Graphics& g) {
    Component::Paint(g);
    g.SetSysColor(StandardColor_3DFace);
    if(!this->mIsTransparent) g.FillRect(0,0,this->Width(),this->Height());
    int height=MSCheckbox::mCheckboxHeight;
    int y=0;
    if(this->mVAlign==VertAlign_Center) y=(this->Height()-height-this->mTopTextPad-this->mBottomTextPad)/2;
    else if(this->mVAlign==VertAlign_Bottom) y=this->Height()-height-this->mTopTextPad-this->mBottomTextPad;
    g.Translate(0,this->mTopTextPad+y);
    MSCheckbox::CheckboxPainter(g,this);
    g.Translate(0,-y-this->mTopTextPad);
    this->DrawButtonText(g,0,0);
}
