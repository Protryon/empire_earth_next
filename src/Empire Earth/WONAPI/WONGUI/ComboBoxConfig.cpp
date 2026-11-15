// Combo box configuration: resource-file keys and the predefined list area and input box children
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
#include "ee/wonapi/WONLobbyComboBoxConfig.h"
#include "ee/wonapi/WONLobbyResourceConfig.h"
#include "rust/ee_won.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
// FUNCTION: WON 0x00782050
// Creates a combo box configuration (base members only)
ComboBoxConfig::ComboBoxConfig()
{
}

// FUNCTION: WON 0x007821b0
// Handles a combo box configuration key (list box size, text, items, selection, no-typing, max chars,
// button-when-disabled); passes other keys to the container configuration
bool ComboBoxConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_COMBOBOXCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) {
        int width=this->ReadInt(); this->mComboBox->SetListBoxWidth(width);
    } else if(aKeyIdx==1) {
        int height=this->ReadInt(); this->mComboBox->SetListBoxHeight(height);
    } else if(aKeyIdx==2) {
        int width=this->ReadInt();
        int height=this->ReadInt();
        this->mComboBox->SetListBoxDimensions(width,height);
    } else if(aKeyIdx==3) {
        this->mComboBox->SetText(this->ReadString(false),true);
    } else if(aKeyIdx==4) {
        while(!this->EndOfString()) {
            this->mComboBox->AddString(this->ReadString(false),-1);
            if(!this->EndOfString()) this->EnsureComma();
        }
    } else if(aKeyIdx==5) {
        this->mComboBox->SetSelItem(this->ReadInt(),true);
    } else if(aKeyIdx==6) {
        this->mComboBox->SetNoType(this->ReadBool());
    } else if(aKeyIdx==7) {
        InputBox* input=this->mComboBox->mEdit.get();
        input->SetMaxChars(this->ReadInt());
    } else if(aKeyIdx==8) {
        bool show=this->ReadBool(); this->mComboBox->SetShowButtonWhenDisabled(show);
    } else return ContainerConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x00782070
// Returns the configuration and component for the predefined LISTAREA and INPUTBOX children; passes other
// names to the container configuration
bool ComboBoxConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_COMBOBOXCONFIG_GETPREDEFINEDCOMPONENT_NAME,name.data(),name.size());
    if(aKeyIdx==0) {
        config=ListAreaConfig::CfgFactory();
        component=this->mComboBox->mListArea.get();
    } else if(aKeyIdx==1) {
        config=InputBoxConfig::CfgFactory();
        component=this->mComboBox->mEdit.get();
    } else return ContainerConfig::GetPredefinedComponent(name,config,component);
    return true;
}

}
