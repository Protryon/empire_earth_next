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
namespace WONAPI {
// FUNCTION: WON 0x00717c70
// Creates a skinned button: 8/4 text padding, centred text, scaled images, grey disabled text, a focus
// rectangle, and wants focus
WONButton::WONButton()
{
    this->mLeftTextPad=this->mRightTextPad=8;
    this->mTopTextPad=this->mBottomTextPad=4;
    this->mLeftFocusPad=this->mRightFocusPad=4;
    this->mTopFocusPad=3;
    this->mBottomFocusPad=4;
    this->mPushDX=this->mPushDY=1;
    this->mHasCheck=false;
    this->mScaleImage=true;
    this->mAdjustHeightToFitText=false;
    this->mWrapText=false;
    this->mDrawGrayText=true;
    this->mIsTransparent=false;
    this->mHAlign=HorzAlign_Center;
    this->mVAlign=VertAlign_Center;
    this->mFocusStyle=1;
    this->SetButtonFlags(1,true);
    this->SetComponentFlags(ComponentFlag_WantFocus,true);
    this->mDefaultTextColor=0x80000005;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00717e90
// (public: __thiscall std::map<class WONAPI::Font *,class WONAPI::SmartPtr<class WONAPI::WrappedText>,struct std::less<class WONAPI::Font *>,class std::a)
// TEMPLATE: WON 0x00717df0
// (public: virtual void * __thiscall WONAPI::WONButton::`scalar deleting destructor'(unsigned int))
// TEMPLATE: WON 0x00717dd0
// (public: __thiscall WONAPI::WONButton::ButtonState::ButtonState(void))
