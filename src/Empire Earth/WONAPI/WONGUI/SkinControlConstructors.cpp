// Constructors of the skinned header button, scrollbar and tab control (file name inferred)
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
#include "ee/wonapi/WONLobbySkinFrame.h"
#include "ee/wonapi/WONLobbySkinListControls.h"
#include "ee/wonapi/WONLobbyWONListConfig.h"
#include "ee/wonapi/WONLobbySkinTab.h"
#include "ee/wonapi/WONComponentFlags.h"
namespace WONAPI {
// Creates a skinned list header button: left-aligned text, 4-pixel padding, never takes focus (emitted at 0x007191a0)
WONHeaderButton::WONHeaderButton()
{
    this->SetHorzAlign(HorzAlign_Left);
    this->mLeftTextPad=this->mRightTextPad=4;
    this->mTopTextPad=this->mBottomTextPad=4;
    this->SetComponentFlags(ComponentFlag_WantFocus,false);
}
// Creates an opaque skinned scrollbar, vertical or horizontal (emitted at 0x00719540)
WONScrollbar::WONScrollbar(bool vertical) : Scrollbar(vertical)
{
    this->mIsTransparent=false;
}
// Creates an opaque skinned tab control using the given skin (emitted at 0x0071a830)
WONTabCtrl::WONTabCtrl(WONSkin* skin)
{
    this->mSkin=skin;
    this->mIsTransparent=false;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007191a0
// (public: __thiscall WONAPI::WONHeaderButton::WONHeaderButton(void)    [??0WONHeaderButton@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x00719540
// (public: __thiscall WONAPI::WONScrollbar::WONScrollbar(bool)    [??0WONScrollbar@WONAPI@@QAE@_N@Z])
// TEMPLATE: WON 0x0071a830
// (public: __thiscall WONAPI::WONTabCtrl::WONTabCtrl(class WONAPI::WONSkin *)    [??0WONTabCtrl@WONAPI@@QAE@PAVWONSkin@1@@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00719220
// (public: virtual void * __thiscall WONAPI::WONHeaderButton::`scalar deleting destructor'(unsigned int)    [??_GWONHeaderButton@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x0071a8f0
// (public: virtual void * __thiscall WONAPI::WONTabCtrl::`scalar deleting destructor'(unsigned int)    [??_GWONTabCtrl@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00719240
// (public: virtual __thiscall WONAPI::WONHeaderButton::~WONHeaderButton(void))
// TEMPLATE: WON 0x0071a910
// (public: virtual __thiscall WONAPI::WONTabCtrl::~WONTabCtrl(void))
// TEMPLATE: WON 0x00722a10
// (protected: void __thiscall std::_Tree<class WONAPI::Font *,struct std::pair<class WONAPI::Font * const,class WONAPI::SmartPtr<class WONAPI::WrappedTex)
// TEMPLATE: WON 0x00717e10
// (public: __thiscall WONAPI::WONButton::ButtonState::~ButtonState(void))
// TEMPLATE: WON 0x00717f50
// (public: virtual __thiscall WONAPI::WONButton::~WONButton(void))
