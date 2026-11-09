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
using namespace WONAPI;
// FUNCTION: WON 0x007007b0
// Draws the frame raised or sunken in one of three styles (3D, flat lobby frame, three-line); other
// types draw nothing
void MS3DFrame::Paint(Graphics& g) {
    Component::Paint(g);
    switch(this->mType) {
    case 1:LobbyDraw3DFrame(g,0,0,this->Width(),this->Height(),!this->mIsUp);break;
    case 2:LobbyDrawFrame(g,0,0,this->Width(),this->Height(),!this->mIsUp);break;
    case 3:MSGraphicsUtil::DrawFrame3(g,0,0,this->Width(),this->Height(),!this->mIsUp);break;
    }
}
