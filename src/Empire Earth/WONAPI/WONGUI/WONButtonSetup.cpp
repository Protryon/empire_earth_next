// Skinned button constructor (file name inferred)
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
#include "ee/wonapi/WONLobbyWONButton.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/ee_won.h"
extern "C" intptr_t ee_won_guicpp_wonbtn_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
namespace WONAPI {
// FUNCTION: WON 0x00717c70
// Creates a skinned button (the Rust button holds 8/4 text padding, centred text, scaled images, grey disabled text
// and a focus rectangle) that wants focus
WONButton::WONButton()
{
    this->mWb=ee_won_wonbtn_new(this,ee_won_guicpp_wonbtn_call);
    this->SetButtonFlags(1,true);
    this->SetComponentFlags(ComponentFlag_WantFocus,true);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00717df0
// (public: virtual void * __thiscall WONAPI::WONButton::`scalar deleting destructor'(unsigned int))
