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
    if(key=="SOLIDCOLOR") { int color=this->ReadColor(); this->mRectangleComponent->SetSolidColor(color); }
    else if(key=="OUTLINECOLOR") { int color=this->ReadColor(); this->mRectangleComponent->SetOutlineColor(color); }
    else return ComponentConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x00794c10
// Reads a 3D rectangle's SOLIDCOLOR, LIGHTCOLOR, DARKCOLOR, UP and THICKNESS keys; other keys go to the base config
bool RectangleComponent3DConfig::Virtual24(const std::string& key)
{
    if(key=="SOLIDCOLOR") { int color=this->ReadColor(); this->mRectangleComponent3D->SetSolidColor(color); }
    else if(key=="LIGHTCOLOR") { int color=this->ReadColor(); this->mRectangleComponent3D->SetLightColor(color); }
    else if(key=="DARKCOLOR") { int color=this->ReadColor(); this->mRectangleComponent3D->SetDarkColor(color); }
    else if(key=="UP") { bool up=this->ReadBool(); this->mRectangleComponent3D->SetIsUp(up); }
    else if(key=="THICKNESS") { int thickness=this->ReadInt(); this->mRectangleComponent3D->SetThickness(thickness); }
    else return ComponentConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x00794cf0
// Reads an image component's IMAGE, TILE and STRETCH keys; other keys go to the base component config
bool ImageComponentConfig::Virtual24(const std::string& key)
{
    if(key=="IMAGE") this->mImageComponent->SetImage(this->ReadImage(false));
    else if(key=="TILE") { bool tile=this->ReadBool(); this->mImageComponent->mTile=tile; }
    else if(key=="STRETCH") { bool stretch=this->ReadBool(); this->mImageComponent->mStretch=stretch; }
    else return ComponentConfig::Virtual24(key);
    return true;
}
// FUNCTION: WON 0x00794e50
// Reads a screen container's ADDSCREEN and SHOWSCREEN keys; other keys go to the container config
bool ScreenContainerConfig::Virtual24(const std::string& key)
{
    if(key=="ADDSCREEN") this->mScreenContainer->AddScreen(this->ReadComponent());
    else if(key=="SHOWSCREEN") this->mScreenContainer->ShowScreen(this->ReadComponent(),true);
    else return ContainerConfig::Virtual24(key);
    return true;
}
}


#include "ee/wonapi/WONLobbyAnimationComponent.h"
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

void AnimationComponentConfig::SetComponent(Component *theComponent)
{
	ComponentConfig::SetComponent(theComponent);
	this->mAnimationComponent = static_cast<AnimationComponent*>(theComponent);
}

// FUNCTION: WON 0x00794e30
// Binds the config to its screen container (calls the component-level binder, so the container pointer stays unset)
void ScreenContainerConfig::SetComponent(Component *theComponent)
{
	ComponentConfig::SetComponent(theComponent);
	this->mScreenContainer = static_cast<ScreenContainer*>(theComponent);
}

// FUNCTION: WON 0x00794e10
// Passes every animation component key to the base component config (no keys of its own)
bool AnimationComponentConfig::Virtual24(const std::string &theInstruction)
{
	if(ComponentConfig::Virtual24(theInstruction))
		return true;

	return false;
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

// FUNCTION: WON 0x0079b240
// Creates a new animation component
Component* AnimationComponentConfig::CompFactory()
{
    return new AnimationComponent;
}

// FUNCTION: WON 0x0079b2f0
// Creates a new screen container
Component* ScreenContainerConfig::CompFactory()
{
    return new ScreenContainer;
}
}
