// Resource configuration of image buttons (file name inferred)
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
#include "ee/wonapi/WONLobbyImageButton.h"
#include "rust/ee_rust.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
// FUNCTION: WON 0x0078ac90
// Creates an image button configuration with no button bound
ImageButtonConfig::ImageButtonConfig()
{
}
// FUNCTION: WON 0x0078acb0
// Handles the NORMALIMAGE, OVERIMAGE, DOWNIMAGE, DISABLEDIMAGE and CHECKEDIMAGE instructions by reading an
// image into that state; others go to the base config
bool ImageButtonConfig::Virtual24(const std::string& instruction)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_IMAGEBUTTONCONFIG_VIRTUAL24_INSTRUCTION,instruction.data(),instruction.size());
    ImageButtonType type;
    if(aKeyIdx==0) type=ImageButtonType_Normal;
    else if(aKeyIdx==1) type=ImageButtonType_Over;
    else if(aKeyIdx==2) type=ImageButtonType_Down;
    else if(aKeyIdx==3) type=ImageButtonType_Disabled;
    else if(aKeyIdx==4) type=ImageButtonType_Checked;
    else return ComponentConfig::Virtual24(instruction);
    this->mImageButton->SetImage(type,this->ReadImage(false).get());
    return true;
}
// FUNCTION: WON 0x0078f8d0
// Binds the configuration to its image button
void ImageButtonConfig::SetComponent(Component* component)
{
    ComponentConfig::SetComponent(component);
    this->mImageButton=static_cast<ImageButton*>(component);
}
}
