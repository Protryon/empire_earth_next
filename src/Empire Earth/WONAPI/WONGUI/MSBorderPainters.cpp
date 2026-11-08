// Windows-style frame painting for the etched frame and the input box (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyMSBorderPainters.h"

namespace WONAPI {
// FUNCTION: WON 0x00700860
// Paints an etched frame: a highlight rectangle offset by one pixel under a shadow rectangle
void MSEtchedFrame::Paint(Graphics& g)
{
    Component::Paint(g);
    g.SetSysColor(StandardColor_3DHilight);
    g.DrawRect(0,0,this->Width(),this->Height());
    g.DrawRect(1,1,this->Width()-1,this->Height()-1);
    g.SetSysColor(StandardColor_3DShadow);
    g.DrawRect(0,0,this->Width()-1,this->Height()-1);
}

// FUNCTION: WON 0x006ffad0
// Paints the input box and a sunken 3D frame around it
void MSInputBox::Paint(Graphics& g)
{
    InputBox::Paint(g);
    MSGraphicsUtil::DrawFrame3(g,0,0,this->Width(),this->Height(),true);
}
}

namespace WONAPI {
// FUNCTION: WON 0x006ffb00
// Returns the input box's preferred size, 100 x 22
void MSInputBox::GetDesiredSize(int& width,int& height) { width=100; height=22; }
}
