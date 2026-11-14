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
    MSGraphicsUtil::DrawCheck(g,0,0,MSCheckbox::mCheckboxWidth,MSCheckbox::mCheckboxHeight,button->IsPressed(),button->IsChecked());
}
// FUNCTION: WON 0x6ff470
// Paints the check box: face, the box aligned vertically by the painter function, then the text
void MSCheckbox::Paint(Graphics& g) {
    Component::Paint(g);
    g.SetSysColor(StandardColor_3DFace);
    if(!this->IsTransparent()) g.FillRect(0,0,this->Width(),this->Height());
    int height=MSCheckbox::mCheckboxHeight;
    int y=0;
    if(this->MsField(EeWonMsBtnField::VAlign)==VertAlign_Center) y=(this->Height()-height-this->MsField(EeWonMsBtnField::TopTextPad)-this->MsField(EeWonMsBtnField::BottomTextPad))/2;
    else if(this->MsField(EeWonMsBtnField::VAlign)==VertAlign_Bottom) y=this->Height()-height-this->MsField(EeWonMsBtnField::TopTextPad)-this->MsField(EeWonMsBtnField::BottomTextPad);
    g.Translate(0,this->MsField(EeWonMsBtnField::TopTextPad)+y);
    MSCheckbox::CheckboxPainter(g,this);
    g.Translate(0,-y-this->MsField(EeWonMsBtnField::TopTextPad));
    this->DrawButtonText(g,0,0);
}
