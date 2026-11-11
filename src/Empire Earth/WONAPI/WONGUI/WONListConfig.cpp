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
#include "rust/ee_rust.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
// FUNCTION: WON 0x007986d0
// Handles TRANSPARENT (true gives the scrollbar a plain button as its scroll rectangle); passes other keys on
bool WONScrollbarConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONSCROLLBARCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) {
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
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONHEADERCONTROLCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(MultiListHeaderConfig::Virtual24(key)) return true;
    if(aKeyIdx==0) {
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
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONLISTCTRLCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) {
        int height=this->ReadInt(); this->mListCtrl->mHeaderHeight=height;
    } else if(aKeyIdx==1) {
        MultiListArea* area=static_cast<MultiListArea*>(this->mListCtrl->mScrollArea.get());
        int count=this->ReadInt(); area->SetNumColumns(count);
    } else if(aKeyIdx==2) {
        MultiListArea* area=static_cast<MultiListArea*>(this->mListCtrl->mScrollArea.get());
        int column=this->ReadInt(); area->mDynamicSizeColumn=column;
    } else if(aKeyIdx==3) {
        ListAreaConfig::ReadListFlags(this,static_cast<MultiListArea*>(this->mListCtrl->mScrollArea.get()));
    } else if(aKeyIdx==4) {
        int column=this->ReadInt(); this->EnsureComma();
        GUIString title=this->ReadString(false); this->EnsureComma();
        int width=this->ReadInt(); this->mListCtrl->SetStandarColumnHeader(column,title,width);
    } else if(aKeyIdx==5) {
        int column=this->ReadInt(); this->EnsureComma();
        Component* component=this->ReadComponent(); this->EnsureComma();
        int width=this->ReadInt(); this->mListCtrl->SetColumnHeader(column,component,width);
    } else return WONScrollContainerConfig::Virtual24(key);
    return true;
}
}
