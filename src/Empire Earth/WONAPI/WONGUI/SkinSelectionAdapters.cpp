// Skin accessors that hand out copies of the skin's tab, arrow and scrollbar buttons (file name inferred)
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
#include "ee/wonapi/WONLobbyMSSkinImage.h"
namespace WONAPI {
// FUNCTION: WON 0x00715530
// Returns a new copy of the skin's tab button
WONButton* WONSkin::GetTabButton()
{
    return this->mTabButton->Duplicate();
}
// FUNCTION: WON 0x00715550
// Returns a new copy of the skin's arrow button for a direction, with its desired size cleared
WONSimpleButton* WONSkin::GetArrowButton(Direction direction)
{
    WONSimpleButton* button=this->mArrowButton[direction]->Duplicate();
    button->SetDesiredSize(0,0);
    return button;
}
// FUNCTION: WON 0x00715660
// Returns a new copy of the skin's vertical or horizontal scroll thumb
WONSimpleButton* WONSkin::GetScrollThumb(bool vertical)
{
    return this->mScrollThumb[vertical?1:0]->Duplicate();
}
// FUNCTION: WON 0x00715680
// Returns a new copy of the skin's vertical or horizontal scroll gutter
WONSimpleButton* WONSkin::GetScrollGutter(bool vertical)
{
    return this->mScrollGutter[vertical?1:0]->Duplicate();
}
}
