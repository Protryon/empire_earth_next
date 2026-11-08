// Windows-style popup and dialog background painting
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyMSPainters.h"
namespace WONAPI {
// FUNCTION: WON 0x00700450
// Paints a popup menu background in the menu colour with a raised 3D border (highlight top and left, dark
// shadow and shadow on the right and bottom)
void MSPopup::Paint(Graphics& g)
{
    Component::Paint(g);
    g.SetSysColor(StandardColor_MenuBack);
    g.FillRect(0,0,this->Width(),this->Height());
    g.SetSysColor(StandardColor_3DHilight);
    g.DrawLine(1,1,this->Width()-2,1);
    g.DrawLine(1,1,1,this->Height()-2);
    g.SetSysColor(StandardColor_3DDarkShadow);
    g.DrawLine(this->Width()-1,0,this->Width()-1,this->Height()-1);
    g.DrawLine(1,this->Height()-1,this->Width()-1,this->Height()-1);
    g.SetSysColor(StandardColor_3DShadow);
    g.DrawLine(this->Width()-2,0,this->Width()-2,this->Height()-2);
    g.DrawLine(2,this->Height()-2,this->Width()-2,this->Height()-2);
}
// FUNCTION: WON 0x00700520
// Fills the whole component with the system 3D face colour (dialog background)
void MSDlgBack::Paint(Graphics& g)
{
    Component::Paint(g);
    g.SetSysColor(StandardColor_3DFace);
    g.FillRect(0,0,this->Width(),this->Height());
}
}
