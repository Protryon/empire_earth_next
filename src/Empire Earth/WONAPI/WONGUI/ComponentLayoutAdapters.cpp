// Container layout helpers (file name inferred)
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
namespace WONAPI {
// FUNCTION: WON 0x006f6100
// Adds a layout point, owned by this container, to the child layout
void Container::AddLayoutPoint(LayoutPoint* point)
{
    point->mParent=this;
    this->mChildLayout.AddLayoutPoint(point);
}
// FUNCTION: WON 0x006f6120
// Enables or disables the child layout instructions with the given id
void Container::EnableLayoutInstructions(int id,bool enabled)
{
    this->mChildLayout.EnableInstructions(id,enabled);
}
// FUNCTION: WON 0x006f6290
// Re-runs the size-changed layout and repaints the container
void Container::Relayout()
{
    this->Virtual08();
    this->Invalidate();
}
}
