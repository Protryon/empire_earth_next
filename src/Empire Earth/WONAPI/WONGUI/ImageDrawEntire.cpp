// Image::DrawEntire (file name inferred)
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
namespace WONAPI {
// FUNCTION: WON 0x007285a0
// Draws the whole image with its top-left corner at (x, y)
void Image::DrawEntire(DisplayContext* context,int x,int y)
{
    this->Draw(context,x,y,0,0,this->mWidth,this->mHeight);
}
}
