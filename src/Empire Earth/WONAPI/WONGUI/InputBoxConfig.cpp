// Input box configuration keys (file name inferred)
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
#include "ee/wonapi/WONLobbyResourceConfig.h"
#include "rust/ee_won.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {

// FUNCTION: WON 0x0078cd60
// Handles the input box configuration keys (text, password char, selection, max chars, char width, backgrounds,
// icon, colours, select-on-focus, history size, cursor blink rate); passes other keys to the base configuration
bool InputBoxConfig::Virtual24(const std::string& inInstruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_INPUTBOXCONFIG_VIRTUAL24_ININSTRUCTION,inInstruction.data(),inInstruction.size());
    if(aKeyIdx==0) this->mInputBox->SetText(this->ReadString(false),true);
    else if(aKeyIdx==1) this->mInputBox->SetPasswordChar(this->ReadChar());
    else if(aKeyIdx==2) this->mInputBox->SetAllowSelection(this->ReadBool());
    else if(aKeyIdx==3) this->mInputBox->SetMaxChars(this->ReadInt());
    else if(aKeyIdx==4) this->mInputBox->SetCharWidth(this->ReadInt());
    else if(aKeyIdx==5) this->mInputBox->SetBackground(this->ReadBackground());
    else if(aKeyIdx==6) this->mInputBox->SetDisabledBackground(this->ReadBackground());
    else if(aKeyIdx==7) this->mInputBox->SetIcon(this->ReadImage(false).get());
    else if(aKeyIdx==8) this->mInputBox->mTextColor=this->ReadColor();
    else if(aKeyIdx==9) this->mInputBox->mDisabledTextColor=this->ReadColor();
    else if(aKeyIdx==10) this->mInputBox->mSelColor=this->ReadSelectionColor();
    else if(aKeyIdx==11) this->mInputBox->SetSelectOnFocus(this->ReadBool());
    else if(aKeyIdx==12) this->mInputBox->SetMaxInputHistory(this->ReadInt());
    else if(aKeyIdx==13) this->mInputBox->mUseDisabledColors=this->ReadBool();
    else if(aKeyIdx==14)
    {
        int on=this->ReadInt();
        int off;
        if(!this->EndOfString())
        {
            this->EnsureComma();
            off=this->ReadInt();
        }
        else
            off=on;
        this->mInputBox->SetCursorBlinkRate(on,off);
    }
    else return ComponentConfig::Virtual24(inInstruction);
    return true;
}

}
