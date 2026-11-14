// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONGUIDraw.h"
using namespace WONAPI;
// FUNCTION: WON 0x007007b0
// Draws the frame raised or sunken in one of three styles (3D, flat lobby frame, three-line); other
// types draw nothing
void MS3DFrame::Paint(Graphics& g) {
    Component::Paint(g);
    WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::Frame3DComponent,this->Width(),this->Height(),this->mIsUp?0:EE_WON_MSPAINT_PRESSED,this->mType));
}
