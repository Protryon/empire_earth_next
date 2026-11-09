// Resource-file instructions for the Windows-style button, arrow button, tab button and 3D frame (file name inferred)
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
#include "ee/wonapi/WONLobbyMSConfig.h"
namespace WONAPI {
// FUNCTION: WON 0x0078f3e0
// Handles a button config key (FONT, TEXTCOLOR, alignment, TRANSPARENT, ADJUSTHEIGHTTOFITTEXT, WRAP,
// text padding increments); other keys go to the button config
bool MSButtonConfig::Virtual24(const std::string& key)
{
    if(key=="FONT") {
        this->mMSButton->SetFont(this->ReadFont());
    } else if(key=="TEXTCOLOR") {
        int color=this->ReadColor();
        this->mMSButton->SetTextColor(color);
    } else if(key=="VERTALIGN") {
        VertAlign align=this->ReadVertAlign();
        this->mMSButton->SetVertAlign(align);
    } else if(key=="HORZALIGN") {
        HorzAlign align=this->ReadHorzAlign();
        this->mMSButton->SetHorzAlign(align);
    } else if(key=="TRANSPARENT") {
        this->mMSButton->SetTransparent(this->ReadBool());
    } else if(key=="ADJUSTHEIGHTTOFITTEXT") {
        this->mMSButton->SetAdjustHeightToFitText(this->ReadBool());
    } else if(key=="WRAP") {
        this->mMSButton->SetWrap(this->ReadBool());
    } else if(key=="INCREMENTHORZTEXTPAD") {
        int left=this->ReadInt(); this->EnsureComma();
        int right=this->ReadInt();
        this->mMSButton->IncrementHorzTextPad(left,right);
    } else if(key=="INCREMENTVERTEXTPAD") {
        int top=this->ReadInt(); this->EnsureComma();
        int bottom=this->ReadInt();
        this->mMSButton->IncrementVertTextPad(top,bottom);
    } else return ButtonConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x0078f690
// Handles DIRECTION (Up, Down, Left or Right, any case; other words are ignored); other keys go to the button config
bool MSArrowButtonConfig::Virtual24(const std::string& key)
{
    if(key=="DIRECTION") {
        GUIString direction=this->ReadString(false);
        if(direction.CompareNoCase("Up")==0) this->mMSArrowButton->SetDirection(Direction_Up);
        else if(direction.CompareNoCase("Down")==0) this->mMSArrowButton->SetDirection(Direction_Down);
        else if(direction.CompareNoCase("Left")==0) this->mMSArrowButton->SetDirection(Direction_Left);
        else if(direction.CompareNoCase("Right")==0) this->mMSArrowButton->SetDirection(Direction_Right);
    } else return ButtonConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x0078f7d0
// Handles LOWERAMOUNT for a tab button; other keys go to the MS button config
bool MSTabButtonConfig::Virtual24(const std::string& key)
{
    if(key=="LOWERAMOUNT")
        this->mMSTabButton->SetLowerAmount(this->ReadInt());
    else return MSButtonConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x0078f8f0
// Handles FRAMETYPE and UP for a 3D frame; other keys go to the component config
bool MS3DFrameConfig::Virtual24(const std::string& key)
{
    if(key=="FRAMETYPE") {
        int type=this->ReadInt();
        this->mMS3DFrame->SetFrameType(type);
    } else if(key=="UP") {
        bool up=this->ReadBool();
        this->mMS3DFrame->SetIsUp(up);
    } else return ComponentConfig::Virtual24(key);
    return true;
}
}

namespace WONAPI {
// FUNCTION: WON 0x0078f3c0
// Binds the config to the arrow button it configures
void MSArrowButtonConfig::SetComponent(Component* component)
{
    ButtonConfig::SetComponent(component);
    this->mMSArrowButton=static_cast<MSArrowButton*>(component);
}
}

namespace WONAPI {
// Binds the config to the 3D frame it configures (the original keeps this body at 0x0078f8d0)
void MS3DFrameConfig::SetComponent(Component* component)
{
    ComponentConfig::SetComponent(component);
    this->mMS3DFrame=static_cast<MS3DFrame*>(component);
}
}

namespace WONAPI {
// Creates the button config (emitted at 0x0078f3a0, listed at the end of the file)
MSButtonConfig::MSButtonConfig()
{
}
}

namespace WONAPI {
// Binds the config to the button it configures (the linker folds this body into the identical one at 0x0078f3c0)
void MSButtonConfig::SetComponent(Component* component)
{
    ButtonConfig::SetComponent(component);
    this->mMSButton=static_cast<MSButtonBase*>(component);
}
// FUNCTION: WON 0x0078f7b0
// Binds the config to the tab button it configures
void MSTabButtonConfig::SetComponent(Component* component)
{
    MSButtonConfig::SetComponent(component);
    this->mMSTabButton=static_cast<MSTabButton*>(component);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0078f3a0
// (public: __thiscall WONAPI::MSButtonConfig::MSButtonConfig(void)    [??0MSButtonConfig@WONAPI@@QAE@XZ])
