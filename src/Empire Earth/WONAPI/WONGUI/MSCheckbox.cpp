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
// Paints the default check box square, pressed and checked as the button's flags say
void MSCheckbox::DefaultCheckboxPainterFunc(Graphics& g,MSCheckbox* button) {
    MSGraphicsUtil::DrawCheck(g,0,0,MSCheckbox::mCheckboxWidth,MSCheckbox::mCheckboxHeight,button->IsPressed(),button->IsChecked());
}
// Paints the check box: face, the box aligned vertically and drawn by the painter function, then the text (the Rust
// button's box paint, msbtnobj.rs)
void MSCheckbox::Paint(Graphics& g) {
    this->MCall(EeWonMsBtnCall::BoxPaint,(intptr_t)EeWonMsBox::Checkbox,(intptr_t)&g,MSCheckbox::mCheckboxHeight);
}
