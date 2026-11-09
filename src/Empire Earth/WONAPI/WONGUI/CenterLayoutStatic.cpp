// One-shot centring helper of the centre layout (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyLayout.h"
namespace WONAPI {
// FUNCTION: WON 0x00737690
// Centres a child in its parent once, horizontally and/or vertically, with a temporary centre layout
void CenterLayout::Center(Component* parent,Component* child,bool horz,bool vert)
{
    SmartPtr<CenterLayout> layout=new CenterLayout(parent,child,horz,vert);
    layout->Execute(parent);
}
}
