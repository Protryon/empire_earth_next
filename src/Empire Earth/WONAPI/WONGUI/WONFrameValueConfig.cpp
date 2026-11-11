// Frame values in skin files and the WON combo box, tab control and scroll container instructions (file name inferred)
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
#include "ee/wonapi/WONLobbyStretchImage.h"
#include "ee/wonapi/WONLobbyWONFrameValueConfig.h"
#include "rust/ee_rust.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
using namespace WONAPI;
// FUNCTION: WON 0x007980a0
// Reads a frame value: a stretch image alone (its own borders), or an image plus one border width or four
static WONFrame ReadFrame(ComponentConfig* config)
{
    SmartPtr<Image> image=config->ReadImage(false);
    StretchImage* stretch=dynamic_cast<StretchImage*>(image.get());
    if(stretch!=NULL) return stretch;
    config->EnsureComma();
    int left=config->ReadInt();
    if(config->EndOfString()) return WONFrame(image.get(),left);
    config->EnsureComma();
    int top=config->ReadInt(); config->EnsureComma();
    int right=config->ReadInt(); config->EnsureComma();
    int bottom=config->ReadInt();
    return WONFrame(image.get(),left,top,right,bottom);
}
namespace WONAPI {
// Out-of-line copy constructor (not identified separately in the original exe)
WONFrame::WONFrame(const WONFrame& from)
{
    this->Copy(from);
}
// FUNCTION: WON 0x00797f70
// Handles the WON combo box skin keys DRAWFRAME, BUTTONINFRAME and FRAME; passes other keys to the combo box config
bool WONComboBoxConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONCOMBOBOXCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) { bool on=this->ReadBool(); this->mComboBox->mDrawFrame=on; }
    else if(aKeyIdx==1) { bool on=this->ReadBool(); this->mComboBox->mButtonInFrame=on; }
    else if(aKeyIdx==2) this->mComboBox->SetFrame(ReadFrame(this));
    else return ComboBoxConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x007981f0
// Handles the WON tab control skin keys TRANSPARENT, ADDSTANDARDTAB (name, container) and FRAME; passes other keys on
bool WONTabCtrlConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONTABCTRLCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) this->mTabCtrl->SetTransparent(this->ReadBool());
    else if(aKeyIdx==1) {
        GUIString name=this->ReadString(false); this->EnsureComma();
        Container* tab=SafeConfigGetComponent<Container*>(this->ReadComponent(),"Container");
        this->mTabCtrl->AddStandardTab(name,tab);
    } else if(aKeyIdx==2) this->mTabCtrl->SetFrame(ReadFrame(this));
    else return TabCtrlConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x007987e0
// Handles DRAWFRAME (false clears the frame, true does nothing), TRANSPARENT, FRAME and CORNER; passes other keys on
bool WONScrollContainerConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONSCROLLCONTAINERCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) {
        if(!this->ReadBool()) this->mScrollContainer->SetFrame(WONFrame());
    } else if(aKeyIdx==1) { bool on=this->ReadBool(); this->mScrollContainer->mTransparent=on; }
    else if(aKeyIdx==2) this->mScrollContainer->SetFrame(ReadFrame(this));
    else if(aKeyIdx==3) this->mScrollContainer->SetCorner(this->ReadImage(false));
    else return ScrollbarScrollerConfig::Virtual24(key);
    return true;
}
}
