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
// FUNCTION: WON 0x006feed0
// Returns the button's text size plus 16 pixels of width and 5 plus the lowered amount of height
void MSTabButton::GetDesiredSize(int& width,int& height) {
    MSButtonBase::GetDesiredSize(width,height);
    width+=16;
    height+=5+this->mLowerAmount;
}
// FUNCTION: WON 0x006fef00
// Paints a tab: the tab outline, then the button text
void MSTabButton::Paint(Graphics& g) {
    Component::Paint(g);
    MSGraphicsUtil::DrawTab(g,0,0,this->Width(),this->Height());
    this->DrawButtonText(g,0,0);
}
