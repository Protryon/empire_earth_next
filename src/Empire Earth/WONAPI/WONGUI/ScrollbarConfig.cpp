// Scrollbar resource config parser
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
// FUNCTION: WON 0x00794920
// Handles VERTICAL, RESIZETHUMB and MINTHUMBSIZE for a scrollbar; else defers to the container parser
bool ScrollbarConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_SCROLLBARCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    if(aKeyIdx==0) this->mScrollbar->SetVertical(this->ReadBool());
    else if(aKeyIdx==1) this->mScrollbar->SetResizeThumb(this->ReadBool());
    else if(aKeyIdx==2) this->mScrollbar->SetMinThumbSize((unsigned char)this->ReadInt());
    else return ContainerConfig::Virtual24(instruction);
    return true;
}

// FUNCTION: WON 0x007949c0
// Returns the scrollbar's THUMB, UPARROW, DOWNARROW or SCROLLRECT control with a new config for it; else defers
bool ScrollbarConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_SCROLLBARCONFIG_GETPREDEFINEDCOMPONENT_NAME,name.data(),name.size());
    if(aKeyIdx==0)
    {
        config=new ComponentConfig;
        component=this->mScrollbar->GetThumb();
    }
    else if(aKeyIdx==1)
    {
        config=new ButtonConfig;
        component=this->mScrollbar->GetUpArrow();
    }
    else if(aKeyIdx==2)
    {
        config=new ButtonConfig;
        component=this->mScrollbar->GetDownArrow();
    }
    else if(aKeyIdx==3)
    {
        config=new ButtonConfig;
        component=this->mScrollbar->GetScrollRect();
    }
    else return ContainerConfig::GetPredefinedComponent(name,config,component);
    return true;
}
}
