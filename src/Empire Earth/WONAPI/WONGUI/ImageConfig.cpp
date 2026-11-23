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
#include "ee/wonapi/WONLayoutConfigs.h" // CfgBodyCall
// The named values' and the lazy values' bodies are ee_won_gui cfgbody.rs's (R6 WON round 34) over WONCfgBody.inl.
namespace WONAPI {

// The image's WIDTH or HEIGHT as a named value; false for any other name (cfgbody.rs)
bool ImageConfig::GetNamedValue(const std::string& n, int& v) { return CfgBodyCall(this, EeWonCfgBody::ImageNamed, (intptr_t)n.data(), (intptr_t)n.size(), (intptr_t)&v) != 0; }


// Sets the raw image (also as the current image) and marks the native image as not yet built
void ImageFilterConfig::SetRawImage(RawImage* inImage)
{
    this->mRawImage=inImage;
    this->mImage=inImage;
    this->mImageIsRawImage=true;
}

// R6 WON round 41: ImageFilterConfig's instructions (SRC, FILTER and HandleFilter's filters, DUPLICATE,
// MULTIHORZCROP, EXPORT) are programs of ee_won's table reader (cfgtab.rs, cfgimg.rs) over WONCfgTable.inl.

// The image, first converting a pending raw image to a native one on the default window (cfgbody.rs)
Image* ImageFilterConfig::GetImage() { return (Image*)CfgBodyCall(this, EeWonCfgBody::FilterImage); }

}
