// Skin-file instructions of the WON scrollbar, header control and list control configs (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyWONListConfig.h"
namespace WONAPI {
// FUNCTION: WON 0x007986d0
// Handles TRANSPARENT (true gives the scrollbar a plain button as its scroll rectangle); passes other keys on
bool WONScrollbarConfig::Virtual24(const std::string& key)
{
    if(key=="TRANSPARENT") {
        if(this->ReadBool()) {
            Button* rect=new Button;
            this->mScrollbar->mScrollRect=rect;
        }
    } else return ScrollbarConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x00798b20
// Lets the list header config handle the key first, then handles SETSTANDARDCOLUMN (column, title)
bool WONHeaderControlConfig::Virtual24(const std::string& key)
{
    if(MultiListHeaderConfig::Virtual24(key)) return true;
    if(key=="SETSTANDARDCOLUMN") {
        int column=this->ReadInt(); this->EnsureComma();
        GUIString title=this->ReadString(false);
        this->mHeader->SetColumn(column,title,NULL);
    } else return false;
    return true;
}
// FUNCTION: WON 0x00798c00
// Handles the list control keys HEADERHEIGHT, NUMCOLUMNS, DYNAMICSIZECOLUMN, LISTFLAGS, SETSTANDARDCOLUMN
// and SETCOLUMN; passes other keys to the scroll container config
bool WONListCtrlConfig::Virtual24(const std::string& key)
{
    if(key=="HEADERHEIGHT") {
        int height=this->ReadInt(); this->mListCtrl->mHeaderHeight=height;
    } else if(key=="NUMCOLUMNS") {
        MultiListArea* area=static_cast<MultiListArea*>(this->mListCtrl->mScrollArea.get());
        int count=this->ReadInt(); area->SetNumColumns(count);
    } else if(key=="DYNAMICSIZECOLUMN") {
        MultiListArea* area=static_cast<MultiListArea*>(this->mListCtrl->mScrollArea.get());
        int column=this->ReadInt(); area->mDynamicSizeColumn=column;
    } else if(key=="LISTFLAGS") {
        ListAreaConfig::ReadListFlags(this,static_cast<MultiListArea*>(this->mListCtrl->mScrollArea.get()));
    } else if(key=="SETSTANDARDCOLUMN") {
        int column=this->ReadInt(); this->EnsureComma();
        GUIString title=this->ReadString(false); this->EnsureComma();
        int width=this->ReadInt(); this->mListCtrl->SetStandarColumnHeader(column,title,width);
    } else if(key=="SETCOLUMN") {
        int column=this->ReadInt(); this->EnsureComma();
        Component* component=this->ReadComponent(); this->EnsureComma();
        int width=this->ReadInt(); this->mListCtrl->SetColumnHeader(column,component,width);
    } else return WONScrollContainerConfig::Virtual24(key);
    return true;
}
}
