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

// Paints a Windows push button: face, a dark outline when it is the default button, a raised or pressed frame, and
// the text shifted one pixel while pressed (mspaint.rs Button)
void MSButton::Paint(Graphics& g) {
    Component::Paint(g);
    uint32_t f=(this->IsPressed()?EE_WON_MSPAINT_PRESSED:0)|(this->IsTransparent()?EE_WON_MSPAINT_TRANSPARENT:0)|
               (this->IsDefaultButton()?EE_WON_MSPAINT_DEFAULT:0)|(this->mUseFrame2?EE_WON_MSPAINT_FRAME2:0);
    WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::Button,this->Width(),this->Height(),f),this);
}
