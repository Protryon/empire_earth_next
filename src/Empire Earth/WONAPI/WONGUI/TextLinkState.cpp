// Text link segments: construction and pressed state
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyRichText.h"
namespace WONAPI {
// Creates an unpressed link that turns red while pressed (emitted at 0x007114f0)
TextLink::TextLink()
{
    this->mDownColor=0xff0000;
    this->mPressed=false;
}
// FUNCTION: WON 0x00711560
// Returns the parameter delivered when the link is activated
const GUIString& TextLink::GetLinkParam() { return this->mLinkParam; }
// FUNCTION: WON 0x00711640
// Marks the link pressed and switches it to its pressed colour, remembering the normal colour
void TextLink::Pressed()
{
    this->mPressed=true;
    this->mNormalColor=this->mColor;
    this->mColor=this->mDownColor;
}
// FUNCTION: WON 0x00711660
// Restores the normal colour of a pressed link; returns whether it was pressed
bool TextLink::Released()
{
    if(this->mPressed) {
        this->mColor=this->mNormalColor;
        this->mPressed=false;
        return true;
    }
    return false;
}
// Returns true: a text link is a link (the linker folds this body into the identical one at
// 0x006f30d0)
bool TextLink::IsLink() { return true; }
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007114f0
// (public: __thiscall WONAPI::TextLink::TextLink(void)    [??0TextLink@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x00711590
// (public: virtual __thiscall WONAPI::TextLink::~TextLink(void)    [??1TextLink@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00711570
// (public: virtual void * __thiscall WONAPI::TextLink::`scalar deleting destructor'(unsigned int)    [??_GTextLink@WONAPI@@UAEPAXI@Z])
