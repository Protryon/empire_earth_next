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
    if(key=="LISTBOXWIDTH") {
        int width=this->ReadInt(); this->mComboBox->SetListBoxWidth(width);
    } else if(key=="LISTBOXHEIGHT") {
        int height=this->ReadInt(); this->mComboBox->SetListBoxHeight(height);
    } else if(key=="LISTBOXSIZE") {
        int width=this->ReadInt();
        int height=this->ReadInt();
        this->mComboBox->SetListBoxDimensions(width,height);
    } else if(key=="TEXT") {
        this->mComboBox->SetText(this->ReadString(false),true);
    } else if(key=="TEXTITEM") {
        while(!this->EndOfString()) {
            this->mComboBox->AddString(this->ReadString(false),-1);
            if(!this->EndOfString()) this->EnsureComma();
        }
    } else if(key=="SELITEM") {
        this->mComboBox->SetSelItem(this->ReadInt(),true);
    } else if(key=="NOTYPE") {
        this->mComboBox->SetNoType(this->ReadBool());
    } else if(key=="MAXCHARS") {
        InputBox* input=this->mComboBox->mEdit.get();
        input->mMaxChars=this->ReadInt();
    } else if(key=="SHOWBUTTONWHENDISABLED") {
        bool show=this->ReadBool(); this->mComboBox->mShowButtonWhenDisabled=show;
    } else return ContainerConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x00782070
// Returns the configuration and component for the predefined LISTAREA and INPUTBOX children; passes other
// names to the container configuration
bool ComboBoxConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    if(name=="LISTAREA") {
        config=ListAreaConfig::CfgFactory();
        component=this->mComboBox->mListArea.get();
    } else if(name=="INPUTBOX") {
        config=InputBoxConfig::CfgFactory();
        component=this->mComboBox->mEdit.get();
    } else return ContainerConfig::GetPredefinedComponent(name,config,component);
    return true;
}

}
