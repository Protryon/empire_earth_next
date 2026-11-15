// Config handlers and factories for the simple components: rectangles, images, animations and screen containers
// (file name inferred)
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
#include "ee/wonapi/WONLobbySimpleConfig.h"
namespace WONAPI {
// FUNCTION: WON 0x00794ba0
// Reads a rectangle's SOLIDCOLOR and OUTLINECOLOR keys; other keys go to the base component config
bool RectangleComponentConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_RECTANGLECOMPONENTCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) { int color=this->ReadColor(); this->mRectangleComponent->SetSolidColor(color); }
    else if(aKeyIdx==1) { int color=this->ReadColor(); this->mRectangleComponent->SetOutlineColor(color); }
    else return ComponentConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x00794c10
// Reads a 3D rectangle's SOLIDCOLOR, LIGHTCOLOR, DARKCOLOR, UP and THICKNESS keys; other keys go to the base config
bool RectangleComponent3DConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_RECTANGLECOMPONENT3DCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) { int color=this->ReadColor(); this->mRectangleComponent3D->SetSolidColor(color); }
    else if(aKeyIdx==1) { int color=this->ReadColor(); this->mRectangleComponent3D->SetLightColor(color); }
    else if(aKeyIdx==2) { int color=this->ReadColor(); this->mRectangleComponent3D->SetDarkColor(color); }
    else if(aKeyIdx==3) { bool up=this->ReadBool(); this->mRectangleComponent3D->SetIsUp(up); }
    else if(aKeyIdx==4) { int thickness=this->ReadInt(); this->mRectangleComponent3D->SetThickness(thickness); }
    else return ComponentConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x00794cf0
// Reads an image component's IMAGE, TILE and STRETCH keys; other keys go to the base component config
bool ImageComponentConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_IMAGECOMPONENTCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) this->mImageComponent->SetImage(this->ReadImage(false));
    else if(aKeyIdx==1) { bool tile=this->ReadBool(); this->mImageComponent->mTile=tile; }
    else if(aKeyIdx==2) { bool stretch=this->ReadBool(); this->mImageComponent->mStretch=stretch; }
    else return ComponentConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x00794e50
// Reads a screen container's ADDSCREEN and SHOWSCREEN keys; other keys go to the container config
bool ScreenContainerConfig::Virtual24(const std::string& key)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_SCREENCONTAINERCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0) this->mScreenContainer->AddScreen(this->ReadComponent());
    else if(aKeyIdx==1) this->mScreenContainer->ShowScreen(this->ReadComponent(),true);
    else return ContainerConfig::Virtual24(key);
    return true;
}
}


#include "rust/ee_won.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {
void RectangleComponentConfig::SetComponent(Component *theComponent)
{
	ComponentConfig::SetComponent(theComponent);
	this->mRectangleComponent = static_cast<RectangleComponent*>(theComponent);
}

void RectangleComponent3DConfig::SetComponent(Component *theComponent)
{
	ComponentConfig::SetComponent(theComponent);
	this->mRectangleComponent3D = static_cast<RectangleComponent3D*>(theComponent);
}

void ImageComponentConfig::SetComponent(Component *theComponent)
{
	ComponentConfig::SetComponent(theComponent);
	this->mImageComponent = static_cast<ImageComponent*>(theComponent);
}

// FUNCTION: WON 0x00794e30
// Binds the config to its screen container (calls the component-level binder, so the container pointer stays unset)
void ScreenContainerConfig::SetComponent(Component *theComponent)
{
	ComponentConfig::SetComponent(theComponent);
	this->mScreenContainer = static_cast<ScreenContainer*>(theComponent);
}

// FUNCTION: WON 0x0079b030
// Creates a new rectangle component
Component* RectangleComponentConfig::CompFactory()
{
    return new RectangleComponent;
}

// FUNCTION: WON 0x0079b0e0
// Creates a new 3D rectangle component
Component* RectangleComponent3DConfig::CompFactory()
{
    return new RectangleComponent3D;
}

// FUNCTION: WON 0x0079b190
// Creates a new image component
Component* ImageComponentConfig::CompFactory()
{
    return new ImageComponent;
}

// FUNCTION: WON 0x0079b2f0
// Creates a new screen container
Component* ScreenContainerConfig::CompFactory()
{
    return new ScreenContainer;
}
}
