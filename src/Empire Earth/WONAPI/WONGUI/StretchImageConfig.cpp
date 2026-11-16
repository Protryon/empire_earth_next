// Config handlers for stretch images and stretch-center images (file name inferred)
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
#include "rust/ee_won.h"
// R6 WON round 41: StretchImageConfig's instructions (SRC, TILE, BORDERS, SKIPCENTER and the three exports) are
// programs of ee_won's table reader (cfgtab.rs, cfgimg.rs) over WONCfgTable.inl.
namespace WONAPI {
// Creates a stretch image config holding a new empty stretch image
StretchImageConfig::StretchImageConfig()
{
    this->mStretchImage=new StretchImage(NULL,0,0,0,0);
    this->mImage=this->mStretchImage;
}

// Creates a stretch-center image config holding a new empty stretch-center image
StretchCenterImageConfig::StretchCenterImageConfig()
{
    this->mStretchCenterImage=new StretchCenterImage;
    this->mImage=this->mStretchCenterImage;
}

}
