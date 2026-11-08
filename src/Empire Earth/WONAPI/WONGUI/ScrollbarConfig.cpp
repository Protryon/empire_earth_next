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
namespace WONAPI {
// FUNCTION: WON 0x00794920
// Handles VERTICAL, RESIZETHUMB and MINTHUMBSIZE for a scrollbar; else defers to the container parser
bool ScrollbarConfig::Virtual24(const std::string& instruction)
{
    if(instruction=="VERTICAL") this->mScrollbar->mVertical=this->ReadBool();
    else if(instruction=="RESIZETHUMB") this->mScrollbar->mResizeThumb=this->ReadBool();
    else if(instruction=="MINTHUMBSIZE") this->mScrollbar->mMinThumbSize=(unsigned char)this->ReadInt();
    else return ContainerConfig::Virtual24(instruction);
    return true;
}

// FUNCTION: WON 0x007949c0
// Returns the scrollbar's THUMB, UPARROW, DOWNARROW or SCROLLRECT control with a new config for it; else defers
bool ScrollbarConfig::GetPredefinedComponent(const std::string& name,ComponentConfig*& config,Component*& component)
{
    if(name=="THUMB")
    {
        config=new ComponentConfig;
        component=this->mScrollbar->mThumb.get();
    }
    else if(name=="UPARROW")
    {
        config=new ButtonConfig;
        component=this->mScrollbar->mUpArrow.get();
    }
    else if(name=="DOWNARROW")
    {
        config=new ButtonConfig;
        component=this->mScrollbar->mDownArrow.get();
    }
    else if(name=="SCROLLRECT")
    {
        config=new ButtonConfig;
        component=this->mScrollbar->mScrollRect.get();
    }
    else return ContainerConfig::GetPredefinedComponent(name,config,component);
    return true;
}
}
