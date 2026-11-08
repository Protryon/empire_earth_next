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
namespace WONAPI {

// FUNCTION: WON 0x0078cd60
// Handles the input box configuration keys (text, password char, selection, max chars, char width, backgrounds,
// icon, colours, select-on-focus, history size, cursor blink rate); passes other keys to the base configuration
bool InputBoxConfig::Virtual24(const std::string& inInstruction)
{
    if(inInstruction=="TEXT") this->mInputBox->SetText(this->ReadString(false),true);
    else if(inInstruction=="PASSWORDCHAR") this->mInputBox->SetPasswordChar(this->ReadChar());
    else if(inInstruction=="ALLOWSELECTION") this->mInputBox->SetAllowSelection(this->ReadBool());
    else if(inInstruction=="MAXCHARS") this->mInputBox->mMaxChars=this->ReadInt();
    else if(inInstruction=="CHARWIDTH") this->mInputBox->SetCharWidth(this->ReadInt());
    else if(inInstruction=="BACKGROUND") this->mInputBox->SetBackground(this->ReadBackground());
    else if(inInstruction=="DISABLEDBACKGROUND") this->mInputBox->SetDisabledBackground(this->ReadBackground());
    else if(inInstruction=="ICON") this->mInputBox->SetIcon(this->ReadImage(false).get());
    else if(inInstruction=="TEXTCOLOR") this->mInputBox->mTextColor=this->ReadColor();
    else if(inInstruction=="DISABLEDTEXTCOLOR") this->mInputBox->mDisabledTextColor=this->ReadColor();
    else if(inInstruction=="SELCOLOR") this->mInputBox->mSelColor=this->ReadSelectionColor();
    else if(inInstruction=="SELONFOCUS") this->mInputBox->mSelectOnFocus=this->ReadBool();
    else if(inInstruction=="MAXINPUTHISTORY") this->mInputBox->mMaxInputHistory=this->ReadInt();
    else if(inInstruction=="USEDISABLEDCOLORS") this->mInputBox->mUseDisabledColors=this->ReadBool();
    else if(inInstruction=="CURSORBLINKRATE")
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
