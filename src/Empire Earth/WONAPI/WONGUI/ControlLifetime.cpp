// Constructors of the scroller, scrollbar-scroller, scrollbar and screen container (file name inferred)
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
// Creates a scroller with no scroll area (emitted at 0x00705540)
Scroller::Scroller() :mScrollArea(NULL) {}
// Creates a scroller whose scrollbars are both enabled (emitted at 0x00705700)
ScrollbarScroller::ScrollbarScroller()
{
    this->mVertCondition=this->mHorzCondition=ScrollbarCondition_Enable;
    this->mSettingScrollbarConditions=false;
}
// Creates an empty vertical or horizontal scrollbar with default line, page and thumb sizes; the drag
// coordinates stay uninitialised (emitted at 0x00705e10)
Scrollbar::Scrollbar(bool vertical)
{
    this->mPosition=0;
    this->mPageSize=0;
    this->mTotalSize=0;
    this->mLineUpScrollSize=this->mLineDownScrollSize=10;
    this->mPageScrollSize=50;
    this->mResizeThumb=true;
    this->mVertical=vertical;
    this->mMinThumbSize=10;
}
// Destroys the scrollbar: thumb, track and the two arrow buttons, then the container (emitted at 0x00705eb0)
Scrollbar::~Scrollbar() {}
// Creates a screen container showing no screen (emitted at 0x00706a20)
ScreenContainer::ScreenContainer() { this->mCurScreen=NULL; }
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00705540
// (public: __thiscall WONAPI::Scroller::Scroller(void)    [??0Scroller@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x00705560
// (public: virtual void * __thiscall WONAPI::Scroller::`scalar deleting destructor'(unsigned int)    [??_GScroller@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00705700
// (public: __thiscall WONAPI::ScrollbarScroller::ScrollbarScroller(void)    [??0ScrollbarScroller@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x00705740
// (public: virtual void * __thiscall WONAPI::ScrollbarScroller::`scalar deleting destructor'(unsigned int)    [??_GScrollbarScroller@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00705e10
// (public: __thiscall WONAPI::Scrollbar::Scrollbar(bool)    [??0Scrollbar@WONAPI@@QAE@_N@Z])
// TEMPLATE: WON 0x00705e90
// (public: virtual void * __thiscall WONAPI::Scrollbar::`scalar deleting destructor'(unsigned int)    [??_GScrollbar@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00705eb0
// (public: virtual __thiscall WONAPI::Scrollbar::~Scrollbar(void)    [??1Scrollbar@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x00706a20
// (public: __thiscall WONAPI::ScreenContainer::ScreenContainer(void)    [??0ScreenContainer@WONAPI@@QAE@XZ])
