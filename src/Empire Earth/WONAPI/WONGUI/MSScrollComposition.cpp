// Windows-style scrollbar and scroll container construction
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
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
namespace WONAPI {
// FUNCTION: WON 0x006ff820
// Creates an opaque scrollbar with arrow buttons from the factory (up/down or left/right, at least
// 15 x 15 when sizeless), a thumb from the thumb factory and a scroll area button
MSScrollbar::MSScrollbar(bool vertical) : Scrollbar(vertical)
{
    this->mIsTransparent=false;
    if(vertical) {
        this->mUpArrow=MSScrollbar::ArrowButtonFactory(Direction_Up);
        this->mDownArrow=MSScrollbar::ArrowButtonFactory(Direction_Down);
    } else {
        this->mUpArrow=MSScrollbar::ArrowButtonFactory(Direction_Left);
        this->mDownArrow=MSScrollbar::ArrowButtonFactory(Direction_Right);
    }
    if(this->mUpArrow->Width()==0 || this->mUpArrow->Height()==0)
        this->mUpArrow->SetSizeAtLeast(15,15);
    if(this->mDownArrow->Width()==0 || this->mDownArrow->Height()==0)
        this->mDownArrow->SetSizeAtLeast(15,15);
    this->mThumb=MSScrollbar::ThumbFactory(ThumbType(this->mVertical));
    this->mScrollRect=new Button;
    this->SetSize(this->mUpArrow->Width(),this->mUpArrow->Height());
}
// FUNCTION: WON 0x006ffeb0
// Creates a scroll container around the scroll area with Windows-style scrollbars, no header and both
// frames on
MSScrollContainer::MSScrollContainer(ScrollArea* scrollArea)
{
    this->mVertScrollbar=new MSScrollbar(true);
    this->mHorzScrollbar=new MSScrollbar(false);
    this->mScrollArea=scrollArea;
    this->mHeaderHeight=0;
    this->mDraw3DFrame=true;
    this->mDrawFrame=true;
    this->mTransparent=false;
}
// FUNCTION: WON 0x006ffa60
// Sets whether the scrollbar paints its background
void MSScrollbar::SetTransparent(bool transparent)
{
    this->mIsTransparent=transparent;
}
// FUNCTION: WON 0x00700250
// Turns the 3D frame on or off and refreshes the container when it changes
void MSScrollContainer::SetDraw3DFrame(bool on)
{
    if(this->mDraw3DFrame==on) return;
    this->mDraw3DFrame=on;
    this->Virtual08();
}
// FUNCTION: WON 0x00700270
// Turns the frame on or off and refreshes the container when it changes
void MSScrollContainer::SetDrawFrame(bool on)
{
    if(this->mDrawFrame==on) return;
    this->mDrawFrame=on;
    this->Virtual08();
}
}

namespace WONAPI {
// Default arrow button factory: returns a new Windows-style arrow button (body at 0x006fdc40)
SmartPtr<Button> DefaultMSArrowButtonFactory(Direction direction)
{
    return new MSArrowButton(direction);
}

MSArrowButtonFactory MSScrollbar::ArrowButtonFactory=DefaultMSArrowButtonFactory;
MSScrollbar::ThumbFactoryFunc MSScrollbar::ThumbFactory=MSScrollbar::DefaultThumbFactory;
MSArrowButtonFactory MSComboBox::ArrowButtonFactory=DefaultMSArrowButtonFactory;
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006fccb0
// (public: static class WONAPI::SmartPtr<class WONAPI::Component> __cdecl WONAPI::MSScrollbar::DefaultThumbFactory(bool)    [?DefaultThumbFactory@MSScrol)
// TEMPLATE: WON 0x006fdc40
// (class WONAPI::SmartPtr<class WONAPI::Button> __cdecl WONAPI::DefaultMSArrowButtonFactory(enum WONAPI::Direction)    [?DefaultMSArrowButtonFactory@WONA)
// TEMPLATE: WON 0x006ffa50
// (public: virtual __thiscall WONAPI::MSScrollbar::~MSScrollbar(void)    [??1MSScrollbar@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007001e0
// (public: virtual __thiscall WONAPI::Scroller::~Scroller(void)    [??1Scroller@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00700040
// (public: virtual void * __thiscall WONAPI::MSScrollContainer::`scalar deleting destructor'(unsigned int)    [??_GMSScrollContainer@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00700060
// (public: virtual __thiscall WONAPI::ScrollbarScroller::~ScrollbarScroller(void)    [??1ScrollbarScroller@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x00719560
// (public: virtual void * __thiscall WONAPI::MSScrollbar::`scalar deleting destructor'(unsigned int)    [??_GMSScrollbar@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00700120
// (public: virtual __thiscall WONAPI::MSScrollContainer::~MSScrollContainer(void)    [??1MSScrollContainer@WONAPI@@UAE@XZ])
