// Resource-file instructions for the other Windows-style controls (file name inferred)
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
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
namespace WONAPI {
// FUNCTION: WON 0x0078f810
// Handles DRAWFRAME for a combo box; other keys go to the combo box config
bool MSComboBoxConfig::Virtual24(const std::string& key)
{
    if(key=="DRAWFRAME") { bool on=this->ReadBool(); this->mMSComboBox->SetDrawFrame(on); }
    else return ComboBoxConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x0078f880
// Handles TRANSPARENT for a scrollbar; other keys go to the scrollbar config
bool MSScrollbarConfig::Virtual24(const std::string& key)
{
    if(key=="TRANSPARENT") this->mMSScrollbar->SetTransparent(this->ReadBool());
    else return ScrollbarConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x0078fa40
// Handles TITLE and CONTAINEDCOMPONENT for an etched frame group; other keys go to the container config
bool MSEtchedFrameGroupConfig::Virtual24(const std::string& key)
{
    if(key=="TITLE") this->mFrame->SetLabelText(this->ReadString(false));
    else if(key=="CONTAINEDCOMPONENT") this->mFrame->AddContainedComponent(this->ReadComponent());
    else return ContainerConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x0078fb30
// Handles TRANSPARENT and ADDSTANDARDTAB (title, container) for a tab control; other keys go to the
// tab control config
bool MSTabCtrlConfig::Virtual24(const std::string& key)
{
    if(key=="TRANSPARENT") this->mMSTabCtrl->SetTransparent(this->ReadBool());
    else if(key=="ADDSTANDARDTAB") {
        GUIString text=this->ReadString(false); this->EnsureComma();
        Container* container=SafeConfigGetComponent<Container*>(this->ReadComponent(),"Container");
        this->mMSTabCtrl->AddStandardTab(text,container);
    } else return TabCtrlConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x0078fd30
// Handles DRAWFRAME, DRAW3DFRAME and TRANSPARENT for a scroll container; other keys go to the
// scroller config
bool MSScrollContainerConfig::Virtual24(const std::string& key)
{
    if(key=="DRAWFRAME") this->mScrollContainer->SetDrawFrame(this->ReadBool());
    else if(key=="DRAW3DFRAME") this->mScrollContainer->SetDraw3DFrame(this->ReadBool());
    else if(key=="TRANSPARENT") { bool on=this->ReadBool(); this->mScrollContainer->SetTransparent(on); }
    else return ScrollbarScrollerConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x0078ff00
// Handles SETSTANDARDCOLUMN (column, title) for a list header; other keys go to the header config
bool MSHeaderControlConfig::Virtual24(const std::string& key)
{
    if(key=="SETSTANDARDCOLUMN") {
        int column=this->ReadInt(); this->EnsureComma(); GUIString title=this->ReadString(false);
        this->mMSHeader->SetColumn(column,title);
    } else return MultiListHeaderConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x0078ffd0
// Handles HEADERHEIGHT for a list control; other keys go to the scroll container config
bool MSListCtrlConfig::Virtual24(const std::string& key)
{
    if(key=="HEADERHEIGHT") { int height=this->ReadInt(); this->mListCtrl->SetHeaderHeight(height); }
    else return MSScrollContainerConfig::Virtual24(key);
    return true;
}
}
