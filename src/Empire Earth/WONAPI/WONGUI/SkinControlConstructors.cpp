// Constructors of the skinned header button, scrollbar and tab control (file name inferred)
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
#include "ee/wonapi/WONLobbySkinFrame.h"
#include "ee/wonapi/WONLobbySkinListControls.h"
#include "ee/wonapi/WONLobbyWONListConfig.h"
#include "ee/wonapi/WONLobbySkinTab.h"
#include "ee/wonapi/WONComponentFlags.h"
namespace WONAPI {
// Creates a skinned list header button: left-aligned text, 4-pixel padding, never takes focus (emitted at 0x007191a0)
WONHeaderButton::WONHeaderButton()
{
    this->SetHorzAlign(HorzAlign_Left);
    this->SetTextPadding(4,4,4,4);
    this->SetComponentFlags(ComponentFlag_WantFocus,false);
}
// Creates an opaque skinned scrollbar, vertical or horizontal (emitted at 0x00719540)
WONScrollbar::WONScrollbar(bool vertical) : Scrollbar(vertical)
{
    this->mIsTransparent=false;
}
// Creates an opaque skinned tab control using the given skin (emitted at 0x0071a830)
WONTabCtrl::WONTabCtrl(WONSkin* skin)
{
    this->mSkin=skin;
    this->mIsTransparent=false;
}
}
