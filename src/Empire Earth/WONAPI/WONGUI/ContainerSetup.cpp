// Container constructor and the mouse-wheel routing default (file name inferred)
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
#include "ee/wonapi/WONComponentFlags.h"
namespace WONAPI {
bool Container::mMouseWheelGoesToFocusChildDef=false;
// FUNCTION: WON 0x006f5b70
// Creates an empty, focusable container with no mouse or focus child and no modal state
Container::Container()
{
    this->mMouseChild=NULL;
    this->mFocusChild=NULL;
    this->mCurrentCursor=NULL;
    this->mChildNeedsTimer=false;
    this->mMouseWheelGoesToFocusChild=Container::mMouseWheelGoesToFocusChildDef;
    this->mOnlyChildInput=false;
    this->mIsolateTabFocus=false;
    this->mRemovingAllChildren=false;
    this->mHasUsedDialogKeys=false;
    this->mModalCount=0;
    this->mNextFocusId=0;
    this->SetComponentFlags(ComponentFlag_WantFocus,true);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071de50
// (protected: void __thiscall std::_Tree<class WONAPI::SmartPtr<class WONAPI::Component>,class WONAPI::SmartPtr<class WONAPI::Component>,struct std::_Set)
// TEMPLATE: WON 0x0074d4a0
// (protected: struct std::_Tree<class WONAPI::SmartPtr<class WONAPI::Component>,class WONAPI::SmartPtr<class WONAPI::Component>,struct std::_Setk<class W)
// TEMPLATE: WON 0x006f5e60
// (public: virtual void * __thiscall WONAPI::ChildLayoutManager::`scalar deleting destructor'(unsigned int))
