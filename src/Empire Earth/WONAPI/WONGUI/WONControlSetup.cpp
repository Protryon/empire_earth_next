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

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00703520
// (public: virtual void * __thiscall WONAPI::MultiListArea::`scalar deleting destructor'(unsigned int)    [??_GMultiListArea@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x0070be20
// (public: __thiscall WONAPI::WONFrame::~WONFrame(void)    [??1WONFrame@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x00719800
// (public: __thiscall WONAPI::WONComboBox::WONComboBox(void)    [??0WONComboBox@WONAPI@@QAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007033a0
// (public: __thiscall WONAPI::MultiListArea::MultiListArea(int)    [??0MultiListArea@WONAPI@@QAE@H@Z])
// TEMPLATE: WON 0x00703540
// (public: virtual __thiscall WONAPI::MultiListArea::~MultiListArea(void)    [??1MultiListArea@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00719970
// (public: virtual __thiscall WONAPI::WONComboBox::~WONComboBox(void))
// TEMPLATE: WON 0x00719950
// (public: virtual void * __thiscall WONAPI::WONComboBox::`scalar deleting destructor'(unsigned int))
