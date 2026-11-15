// Drawing of the Windows-skin images (frames, buttons, arrows, check boxes, scrollbar parts)
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
#include "ee/wonapi/WONLobbyMSSkinImage.h"
#include "ee/wonapi/WONGUIDraw.h"
namespace WONAPI {
// the types and flags mspaint.rs names
static_assert(WONMSSkin::MSImageType_CloseButton==17 && WONMSSkin::MSImageType_ListHeader==13 && WONMSSkin::MSImageFlag_Checked==8,"mspaint.rs");

// FUNCTION: WON 0x007172B0
// Paints the skin image according to its type (frames, arrows, buttons, tab, check box, radio, scrollbar gutter and
// corner, combo popup frame; the push button, arrow and close button faces were DrawPushButton 0x00717110,
// DrawArrowButton 0x007171E0 and DrawCloseButton 0x00717250); unknown types draw nothing. The Rust plan
// (mspaint.rs SkinImage).
void WONMSSkin::MSImage::Paint(Graphics &g)
{
    WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::SkinImage,this->mWidth,this->mHeight,0,this->mImageType,this->mImageFlags));
}
}
