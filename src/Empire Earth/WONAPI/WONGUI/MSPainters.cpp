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
#include "ee/wonapi/WONGUIDraw.h"
namespace WONAPI {
// FUNCTION: WON 0x00700450
// Paints a popup menu background in the menu colour with a raised 3D border (highlight top and left, dark
// shadow and shadow on the right and bottom)
void MSPopup::Paint(Graphics& g)
{
    Component::Paint(g);
    WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::Popup,this->Width(),this->Height()));
}
// FUNCTION: WON 0x00700520
// Fills the whole component with the system 3D face colour (dialog background)
void MSDlgBack::Paint(Graphics& g)
{
    Component::Paint(g);
    WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::DlgBack,this->Width(),this->Height()));
}
}
