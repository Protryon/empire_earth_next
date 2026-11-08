// Resource configuration of buttons: flags, text, check state, click sound, fit to text (file grouping inferred)
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
#include "ee/wonapi/EELobbySound.h"
namespace WONAPI {

// FUNCTION: WON 0x00781880
// Reads a list of named button flags (each optionally negated) and sets or clears them; throws on an
// unknown name (WantDefault is tested twice, and the message has no separator before the name)
void ButtonConfig::HandleButtonFlags()
{
    while(true)
    {
        bool on=true;
        GUIString flag=this->ReadFlag(&on);
        if(flag.length()==0) break;
        int value;
        if(flag.CompareNoCase("InvalidateOnMouseOver")==0) value=1;
        else if(flag.CompareNoCase("RepeatFire")==0) value=2;
        else if(flag.CompareNoCase("Checked")==0) value=4;
        else if(flag.CompareNoCase("Radio")==0) value=8;
        else if(flag.CompareNoCase("ActivateDown")==0) value=0x40;
        else if(flag.CompareNoCase("WantDefault")==0) value=0x400;
        else if(flag.CompareNoCase("EndDialog")==0) value=0x100;
        else if(flag.CompareNoCase("NoActivate")==0) value=0x200;
        else if(flag.CompareNoCase("WantDefault")==0) value=0x400;
        else throw ConfigObjectException("Unknown button flag"+(std::string)flag);
        this->mButton->SetButtonFlags(value,on);
    }
}

// FUNCTION: WON 0x00781d00
// Grows the button to fit the largest of a comma-separated list of texts, keeping its current text
void ButtonConfig::HandleFitText()
{
    int width=this->mButton->Width();
    int height=this->mButton->Height();
    GUIString oldText=this->mButton->GetText();
    for(;;)
    {
        GUIString text=this->ReadString(false);
        this->mButton->SetText(text,true);
        int desiredWidth=0,desiredHeight=0;
        this->mButton->GetDesiredSize(desiredWidth,desiredHeight);
        if(desiredWidth>width) width=desiredWidth;
        if(desiredHeight>height) height=desiredHeight;
        if(this->EndOfString()) break;
        this->EnsureComma();
    }
    this->mButton->SetText(oldText,true);
    this->mButton->SetSize(width,height);
}

// FUNCTION: WON 0x00781e80
// Handles the BUTTONFLAGS, TEXT, SETCHECK, CLICKSOUND and FITTEXT instructions; others go to the base config
bool ButtonConfig::Virtual24(const std::string& inInstruction)
{
    if(inInstruction=="BUTTONFLAGS") this->HandleButtonFlags();
    else if(inInstruction=="TEXT") this->mButton->SetText(this->ReadString(false),true);
    else if(inInstruction=="SETCHECK") this->mButton->SetCheck(this->ReadBool(),false);
    else if(inInstruction=="CLICKSOUND") this->mButton->SetClickSound(this->ReadSound().get());
    else if(inInstruction=="FITTEXT") this->HandleFitText();
    else return ComponentConfig::Virtual24(inInstruction);
    return true;
}

}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00781b10
// (public: virtual __thiscall WONAPI::ConfigObjectException::~ConfigObjectException(void)    [??1ConfigObjectException@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00781af0
// (public: virtual void * __thiscall WONAPI::ConfigObjectException::`scalar deleting destructor'(unsigned int)    [??_GConfigObjectException@WONAPI@@UAEP)
// TEMPLATE: WON 0x00781b80
