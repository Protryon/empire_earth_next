// Constructors of the skinned simple button and scroll thumb (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
#include "ee/wonapi/WONLobbyWONListConfig.h"
#include "ee/wonapi/WONLobbySkinFrame.h"
namespace WONAPI {
// Creates a skinned simple button: unscaled image, repaints on mouse-over, no mouse-down check (emitted at 0x00717990)
WONSimpleButton::WONSimpleButton()
{
    this->mScaleImage=false;
    this->mCheckMouseButtonDown=false;
    this->SetButtonFlags(1,true); // ButtonFlag_InvalidateOnMouseOver = 1
}
// Creates a skinned scroll thumb that checks the mouse-down state and scales its image (emitted at 0x007194a0)
WONScrollThumb::WONScrollThumb()
{
    this->SetCheckMouseButtonDown(true);
    this->SetScaleImage(true);
}
}
