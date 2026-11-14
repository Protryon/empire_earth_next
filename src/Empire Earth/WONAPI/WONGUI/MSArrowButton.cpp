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
// FUNCTION: WON 0x006ff150
// Draws the arrow button: face, frame and arrow, shifted one pixel while pressed and greyed when disabled
void MSArrowButton::Paint(Graphics& g) {
    Component::Paint(g);
    WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::ArrowButton,this->Width(),this->Height(),
                              (this->IsPressed()?EE_WON_MSPAINT_PRESSED:0)|(this->IsDisabled()?EE_WON_MSPAINT_DISABLED:0),this->mDirection));
}
