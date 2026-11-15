// Skin factory for multi-column list controls (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyMSSkinImage.h"
#include "ee/wonapi/WONLobbySkinListControls.h"
#include "ee/wonapi/WONLobbyWONListConfig.h"
namespace WONAPI {
// FUNCTION: WON 0x00715b20
// Returns a new list control from the skin template with a skinned header, a multi-column list area and scrollbars
WONListCtrl* WONSkin::GetListCtrl()
{
    WONListCtrl* list=this->mListCtrl->Duplicate();
    list->CopyAttributes(this->mScrollContainer.get());
    list->SetHeader(new WONHeaderControl(this));
    MultiListArea* area=new MultiListArea;
    list->SetScrollArea(area);
    list->SetScrollbar(this->GetScrollbar(true));
    list->SetScrollbar(this->GetScrollbar(false));
    list->GetHeader()->SetMultiListArea(area);
    return list;
}
}
