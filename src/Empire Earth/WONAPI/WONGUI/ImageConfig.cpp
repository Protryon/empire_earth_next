// Image configurations (Image, StretchImage, StretchCenterImage, ImageFilter) and their registration
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <cstdio>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyResourceConfig.h"
#include "ee/wonapi/WONLobbyConfigImages.h"
#include "rust/ee_won.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won_gui/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {

// Returns the image's WIDTH or HEIGHT for a named-value lookup; false for any other name
bool ImageConfig::GetNamedValue(const std::string& inName,int& outValue)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_IMAGECONFIG_GETNAMEDVALUE_INNAME,inName.data(),inName.size());
    if(aKeyIdx==0) outValue=this->mImage->mWidth;
    else if(aKeyIdx==1) outValue=this->mImage->mHeight;
    else return false;
    return true;
}


// Sets the raw image (also as the current image) and marks the native image as not yet built
void ImageFilterConfig::SetRawImage(RawImage* inImage)
{
    this->mRawImage=inImage;
    this->mImage=inImage;
    this->mImageIsRawImage=true;
}

// R6 WON round 41: ImageFilterConfig's instructions (SRC, FILTER and HandleFilter's filters, DUPLICATE,
// MULTIHORZCROP, EXPORT) are programs of ee_won's table reader (cfgtab.rs, cfgimg.rs) over WONCfgTable.inl.

// Returns the image, first converting a pending raw image to a native one on the default window
Image* ImageFilterConfig::GetImage()
{
    if(this->mImageIsRawImage)
    {
        this->mImage=this->mRawImage->GetNative(Window::GetDefaultWindow()->mDisplayContext.get()).get();
        this->mImageIsRawImage=false;
    }
    return this->mImage.get();
}

}


namespace WONAPI {
// Registers the Image, StretchImage, StretchCenterImage and ImageFilter configuration types
void ImageConfig_Init(ResourceConfigTable* table)
{
    table->AddAllocator("Image",ImageConfig::CfgFactory,ImageConfig::CompFactory);
    table->AddAllocator("StretchImage",StretchImageConfig::CfgFactory,ImageConfig::CompFactory);
    table->AddAllocator("StretchCenterImage",StretchCenterImageConfig::CfgFactory,ImageConfig::CompFactory);
    table->AddAllocator("ImageFilter",ImageFilterConfig::CfgFactory,ImageConfig::CompFactory);
}
}

// Factory callbacks used by the registration above
namespace WONAPI {

// Creates a new image configuration
ComponentConfig* ImageConfig::CfgFactory()
{
    return new ImageConfig;
}

// Creates a new stretch image configuration
ComponentConfig* StretchImageConfig::CfgFactory()
{
    return new StretchImageConfig;
}

// Creates a new stretch-center image configuration
ComponentConfig* StretchCenterImageConfig::CfgFactory()
{
    return new StretchCenterImageConfig;
}

// Creates a new image filter configuration
ComponentConfig* ImageFilterConfig::CfgFactory()
{
    return new ImageFilterConfig;
}

// Returns no component (image configurations create none); the linker folds this body into the
// identical one at 0x006c7b10
Component* ImageConfig::CompFactory()
{
    return NULL;
}

}

namespace WONAPI {
// Accepts the parsed configuration without the base checks; the linker folds this body into the
// identical one at 0x007099f0
bool ImageConfig::PostParse(ConfigParser*)
{
    return true; // skips ComponentConfig::PostParse
}
}
