// WON list control: constructor, header and duplicate (file name inferred)
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
#include "ee/wonapi/WONLobbySkinListControls.h"
namespace WONAPI {
// Creates a WON list control with a header height of 0 (the original's body at 0x0071a270)
WONListCtrl::WONListCtrl() { this->mHeaderHeight=0; }
// The destructor (0x0071a2b0) is implicit: compiler-generated from the header pointer and the bases
// Sets the list's header control and adds it as a child
void WONListCtrl::SetHeader(WONHeaderControl* header)
{
    this->mHeader=header;
    this->AddChild(header);
}
// Returns a new WON list control with the same header height (nothing else is copied)
WONListCtrl* WONListCtrl::Duplicate()
{
    WONListCtrl* result=new WONListCtrl;
    result->mHeaderHeight=this->mHeaderHeight;
    return result;
}
}
