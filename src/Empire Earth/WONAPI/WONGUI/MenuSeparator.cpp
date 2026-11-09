// Menu separator drawing and component list item painting (file name inferred)
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
#include "ee/wonapi/WONLobbyRoomServer.h"
#include "ee/wonapi/WONLobbyComponentListItem.h"
namespace WONAPI {
// FUNCTION: WON 0x00700720
// Draws an etched horizontal line (shadow over highlight) across the middle of the separator
void MSSeperator::Paint(Graphics& g)
{
    Component::Paint(g);
    int y=this->Height()/2-1;
    if(y<0) y=0;
    g.SetSysColor(StandardColor_3DShadow);
    g.DrawLine(0,y,this->Width()-1,y);
    ++y;
    g.SetSysColor(StandardColor_3DHilight);
    g.DrawLine(0,y,this->Width()-1,y);
}

// FUNCTION: WON 0x006fa510
// Draws the item's component, first stretching it to the list's paint width when the item is sizable
void ComponentListItem::Paint(Graphics& g,ListArea* area)
{
    if(this->mComponent.get()==NULL)
        return;
    if(this->mIsSizable)
        this->mComponent->SetSize(area->mPaintWidth,this->mComponent->Height());
    this->mComponent->MarkInvalid();
    this->mComponent->Paint(g);
}

}

// Compiler-generated and header-inline functions this unit emits:
