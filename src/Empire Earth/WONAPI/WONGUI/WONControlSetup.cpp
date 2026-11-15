// Skinned combo box constructor (file name inferred)
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
#include "ee/wonapi/WONLobbySkinnedContainers.h"
namespace WONAPI {
// Creates a skinned combo box with a framed button, a new input box and a new list area (emitted at
// 0x00719800)
WONComboBox::WONComboBox()
{
    this->mDrawFrame=true;
    this->mButtonInFrame=true;
    this->mEdit=new InputBox;
    this->mListArea=new ListArea;
    this->mListArea->SetListFlags(2,true);
}
}
