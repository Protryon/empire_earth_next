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
// FUNCTION: WON 0x006ff150
// Draws the arrow button: face, frame and arrow, shifted one pixel while pressed and greyed when disabled
void MSArrowButton::Paint(Graphics& g) {
    Component::Paint(g);
    bool down=(mButtonFlags&0x20)!=0;
    g.SetSysColor(StandardColor_3DFace);
    g.FillRect(0,0,this->Width(),this->Height());
    LobbyDrawFrame(g,0,0,this->Width(),this->Height(),down);
    if(down) MSGraphicsUtil::DrawArrow(g,this->mDirection,1,1,this->Width(),this->Height(),this->IsDisabled());
    else MSGraphicsUtil::DrawArrow(g,this->mDirection,0,0,this->Width(),this->Height(),this->IsDisabled());
}
