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
    MSGraphicsUtil::DrawRadio(g,0,0,MSRadioButton::mRadioWidth,MSRadioButton::mRadioHeight,button->IsPressed(),button->IsChecked());
}
// FUNCTION: WON 0x6ff600
// Paints the radio button: face background unless transparent, the radio circle aligned vertically between the
// text pads and drawn by the painter function, then the button text (the Rust button's box paint, msbtnobj.rs)
void MSRadioButton::Paint(Graphics& g) {
    this->MCall(EeWonMsBtnCall::BoxPaint,(intptr_t)EeWonMsBox::Radio,(intptr_t)&g,MSRadioButton::mRadioHeight);
}
