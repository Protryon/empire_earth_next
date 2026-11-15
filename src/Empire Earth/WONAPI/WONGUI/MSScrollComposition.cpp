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
// Creates an opaque scrollbar with arrow buttons from the factory (up/down or left/right, at least
// 15 x 15 when sizeless), a thumb from the thumb factory and a scroll area button
MSScrollbar::MSScrollbar(bool vertical) : Scrollbar(vertical)
{
    this->mIsTransparent=false;
    if(vertical) {
        this->SetUpArrow(MSScrollbar::ArrowButtonFactory(Direction_Up));
        this->SetDownArrow(MSScrollbar::ArrowButtonFactory(Direction_Down));
    } else {
        this->SetUpArrow(MSScrollbar::ArrowButtonFactory(Direction_Left));
        this->SetDownArrow(MSScrollbar::ArrowButtonFactory(Direction_Right));
    }
    if(this->GetUpArrow()->Width()==0 || this->GetUpArrow()->Height()==0)
        this->GetUpArrow()->SetSizeAtLeast(15,15);
    if(this->GetDownArrow()->Width()==0 || this->GetDownArrow()->Height()==0)
        this->GetDownArrow()->SetSizeAtLeast(15,15);
    this->SetThumb(MSScrollbar::ThumbFactory(ThumbType(this->IsVertical())));
    this->SetScrollRect(new Button);
    this->SetSize(this->GetUpArrow()->Width(),this->GetUpArrow()->Height());
}
// Creates a scroll container around the scroll area with Windows-style scrollbars, no header and both
// frames on
MSScrollContainer::MSScrollContainer(ScrollArea* scrollArea)
{
    this->SetBar(true,new MSScrollbar(true));
    this->SetBar(false,new MSScrollbar(false));
    this->SetScrollArea(scrollArea);
    this->mHeaderHeight=0;
    this->mDraw3DFrame=true;
    this->mDrawFrame=true;
    this->mTransparent=false;
}
// Sets whether the scrollbar paints its background
void MSScrollbar::SetTransparent(bool transparent)
{
    this->mIsTransparent=transparent;
}
// Turns the 3D frame on or off and refreshes the container when it changes
void MSScrollContainer::SetDraw3DFrame(bool on)
{
    if(this->mDraw3DFrame==on) return;
    this->mDraw3DFrame=on;
    this->Virtual08();
}
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
