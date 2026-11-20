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
#include "ee/wonapi/WONLobbyConfigImages.h"
#include "rust/won_gui/guiconf.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won_gui/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
namespace WONAPI {

// Returns the image's WIDTH or HEIGHT for a named-value lookup; false for any other name
bool ImageConfig::GetNamedValue(const std::string& inName,int& outValue)
{
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_IMAGECONFIG_GETNAMEDVALUE_INNAME,inName.data(),inName.size());
    if(aKeyIdx==0) outValue=this->mImage->GetWidth();
    else if(aKeyIdx==1) outValue=this->mImage->GetHeight();
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
