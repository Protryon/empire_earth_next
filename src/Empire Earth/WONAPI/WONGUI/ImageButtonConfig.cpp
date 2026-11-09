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
    ImageButtonType type;
    if(instruction=="NORMALIMAGE") type=ImageButtonType_Normal;
    else if(instruction=="OVERIMAGE") type=ImageButtonType_Over;
    else if(instruction=="DOWNIMAGE") type=ImageButtonType_Down;
    else if(instruction=="DISABLEDIMAGE") type=ImageButtonType_Disabled;
    else if(instruction=="CHECKEDIMAGE") type=ImageButtonType_Checked;
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
