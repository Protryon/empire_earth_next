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
    if(key=="TEXTFLAGS") {
        while(true) {
            bool add=true;
            GUIString flag=this->ReadFlag(&add);
            if(flag.length()==0) break;
            int mask;
            if(flag.CompareNoCase("WordWrap")==0) mask=1;
            else if(flag.CompareNoCase("AllowSelection")==0) mask=2;
            else if(flag.CompareNoCase("ScrollOnBottom")==0) mask=4;
            else if(flag.CompareNoCase("ShowPartialLines")==0) mask=8;
            else if(flag.CompareNoCase("ScrollFullLine")==0) mask=0x10;
            else if(flag.CompareNoCase("DelayWordWrap")==0) mask=0x20;
            else throw ConfigObjectException("Unknown text flag: "+(std::string)flag);
            this->mTextArea->SetTextFlags(mask,add);
        }
    } else if(key=="TEXT") {
        this->mTextArea->AddSegment(this->ReadString(false),false);
    } else if(key=="FORMATTEDTEXT") {
        this->mTextArea->AddFormatedText(this->ReadString(false),false);
    } else if(key=="FONT") {
        this->mTextArea->SetFont(this->ReadFont());
    } else if(key=="TEXTCOLOR") {
        this->mTextArea->mTextColor=this->ReadColor();
    } else if(key=="SELCOLOR") {
        this->mTextArea->mSelColor=this->ReadSelectionColor();
    } else if(key=="LINKFONT") {
        this->mTextArea->SetLinkFont(this->ReadFont());
    } else if(key=="MAXCHARS") {
        this->mTextArea->SetMaxChars(this->ReadInt());
    } else if(key=="MAXLINES") {
        this->mTextArea->SetMaxLines(this->ReadInt());
    } else if(key=="WRAPINDENTSIZE") {
        this->mTextArea->SetWrapIndentSize(this->ReadInt());
    } else if(key=="HORZALIGN") {
        this->mTextArea->SetLineAlignment(this->ReadHorzAlign());
    } else return ScrollAreaConfig::Virtual24(key);
    return true;
}
}
