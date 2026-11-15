// Text area configuration: binds the text area and reads its config keys
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
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
#include "rust/ee_won.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
// FUNCTION: WON 0x0078f860
// Binds the config to its scroll area and keeps the component as the text area to configure
void TextAreaConfig::SetComponent(Component* theComponent)
{
    ScrollAreaConfig::SetComponent(theComponent);
    this->mTextArea = static_cast<TextArea*>(theComponent);
}
// FUNCTION: WON 0x00795990
// Handles one text area config key (text flags, text, font, colours, limits, alignment); passes
// unknown keys to the scroll area config and throws on an unknown text flag
bool TextAreaConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_TEXTAREACONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) {
        while(true) {
            bool add=true;
            GUIString flag=this->ReadFlag(&add);
            if(flag.length()==0) break;
            int mask;
            const int aNameIdx_flag=ee_won_gui_cfg_name(EE_WON_CFG_NAMES_TEXTAREACONFIG_VIRTUAL24_FLAG,flag.mData->mText,flag.mData->mLength,sizeof(wchar_t));
            if(aNameIdx_flag==0) mask=1;
            else if(aNameIdx_flag==1) mask=2;
            else if(aNameIdx_flag==2) mask=4;
            else if(aNameIdx_flag==3) mask=8;
            else if(aNameIdx_flag==4) mask=0x10;
            else if(aNameIdx_flag==5) mask=0x20;
            else throw ConfigObjectException("Unknown text flag: "+(std::string)flag);
            this->mTextArea->SetTextFlags(mask,add);
        }
    } else if(aKeyIdx==1) {
        this->mTextArea->AddSegment(this->ReadString(false),false);
    } else if(aKeyIdx==2) {
        this->mTextArea->AddFormatedText(this->ReadString(false),false);
    } else if(aKeyIdx==3) {
        this->mTextArea->SetFont(this->ReadFont());
    } else if(aKeyIdx==4) {
        this->mTextArea->mTextColor=this->ReadColor();
    } else if(aKeyIdx==5) {
        this->mTextArea->mSelColor=this->ReadSelectionColor();
    } else if(aKeyIdx==6) {
        this->mTextArea->SetLinkFont(this->ReadFont());
    } else if(aKeyIdx==7) {
        this->mTextArea->SetMaxChars(this->ReadInt());
    } else if(aKeyIdx==8) {
        this->mTextArea->SetMaxLines(this->ReadInt());
    } else if(aKeyIdx==9) {
        this->mTextArea->SetWrapIndentSize(this->ReadInt());
    } else if(aKeyIdx==10) {
        this->mTextArea->SetLineAlignment(this->ReadHorzAlign());
    } else return ScrollAreaConfig::Virtual24(key);
    return true;
}
}
