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

// Binds the config to its screen container (calls the component-level binder, so the container pointer stays unset)
void ScreenContainerConfig::SetComponent(Component *theComponent)
{
	ComponentConfig::SetComponent(theComponent);
	this->mScreenContainer = static_cast<ScreenContainer*>(theComponent);
}

// Creates a new rectangle component
Component* RectangleComponentConfig::CompFactory()
{
    return new RectangleComponent;
}

// Creates a new 3D rectangle component
Component* RectangleComponent3DConfig::CompFactory()
{
    return new RectangleComponent3D;
}

// Creates a new image component
Component* ImageComponentConfig::CompFactory()
{
    return new ImageComponent;
}

// Creates a new screen container
Component* ScreenContainerConfig::CompFactory()
{
    return new ScreenContainer;
}
}
