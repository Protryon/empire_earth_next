// Skinned input box: a framed input box, its duplicate and its painting (file name inferred)
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
#include "ee/wonapi/WONLobbyWONInputBox.h"
namespace WONAPI {
// Creates a skinned input box with an empty frame
WONInputBox::WONInputBox()
{
}
// Returns a new skinned input box with a copy of this one's frame (other settings stay default)
WONInputBox* WONInputBox::Duplicate()
{
    WONInputBox* input=new WONInputBox;
    input->SetFrame(this->mFrame);
    return input;
}
// Paints the input box, then its frame image over its full size
void WONInputBox::Paint(Graphics& graphics)
{
    InputBox::Paint(graphics);
    this->mFrame.Paint(graphics,this);
}
}
