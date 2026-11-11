// Skin-file instructions and named padding values of the frame component config (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyWONFrameConfig.h"
#include "rust/ee_rust.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
// FUNCTION: WON 0x00798440
// Handles the frame component skin keys BORDERS (left, top, right, bottom) and IMAGE; passes other keys on
bool WONFrameComponentConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONFRAMECOMPONENTCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) {
        this->mFrameComponent->SetFrameLeft(this->ReadInt()); this->EnsureComma();
        this->mFrameComponent->SetFrameTop(this->ReadInt()); this->EnsureComma();
        this->mFrameComponent->SetFrameRight(this->ReadInt()); this->EnsureComma();
        this->mFrameComponent->SetFrameBottom(this->ReadInt());
    } else if(aKeyIdx==1) {
        this->mFrameComponent->SetFrameImage(this->ReadImage(false).get());
    } else return ComponentConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x00798590
// Returns the frame paddings LEFTPAD, RIGHTPAD, TOPPAD, BOTTOMPAD, HORZPAD and VERTPAD; asks the base config otherwise
bool WONFrameComponentConfig::GetNamedValue(const std::string& key,int& value)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_WONFRAMECOMPONENTCONFIG_GETNAMEDVALUE_KEY,key.data(),key.size());
    if(aKeyIdx==0) value=this->mFrameComponent->GetFrameLeft();
    else if(aKeyIdx==1) value=this->mFrameComponent->GetFrameRight();
    else if(aKeyIdx==2) value=this->mFrameComponent->GetFrameTop();
    else if(aKeyIdx==3) value=this->mFrameComponent->GetFrameBottom();
    else if(aKeyIdx==4) {
        value=this->mFrameComponent->GetFrameRight()+this->mFrameComponent->GetFrameLeft();
    } else if(aKeyIdx==5) {
        value=this->mFrameComponent->GetFrameBottom()+this->mFrameComponent->GetFrameTop();
    } else return ComponentConfig::GetNamedValue(key,value);
    return true;
}
}

namespace WONAPI {
// FUNCTION: WON 0x00796ba0
// Binds the config to the frame component it configures
void WONFrameComponentConfig::SetComponent(Component* component) { ComponentConfig::SetComponent(component); this->mFrameComponent=static_cast<WONFrameComponent*>(component); }
}
