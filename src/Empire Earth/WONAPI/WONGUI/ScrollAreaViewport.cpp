// Scroll area scroller hookup and the viewport container that holds the scrolled content
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
// FUNCTION: WON 0x00705010
// Attaches a scroller to the scroll area and points the scroller back at it
void ScrollArea::SetScroller(Scroller* scroller)
{
    this->mScroller=scroller;
    if(scroller!=NULL) scroller->SetScrollArea(this);
}
// FUNCTION: WON 0x00705180
// Handles a component event with the plain component behaviour
void ScrollArea::HandleComponentEvent(ComponentEvent* event)
{
    Component::HandleComponentEvent(event);
}
// FUNCTION: WON 0x00705c10
// Repaints the scroll area if a rectangle, translated up through nested viewports, is in view
void ScrollAreaViewport::InvalidateIfRectVisible(WONRectangle& rect)
{
    if(this->mParent==this->mScrollArea) {
        if(this->mScrollArea->IsRectVisible(rect)) this->mScrollArea->Invalidate();
    } else {
        Container* parent=this->mParent;
        if(parent!=NULL) {
            rect.x+=this->Left();
            rect.y+=this->Top();
            static_cast<ScrollAreaViewport*>(parent)->InvalidateIfRectVisible(rect);
        }
    }
}
// FUNCTION: WON 0x00705c70
// Drops every child, timer child and mouse/focus child and ends any popup it owns
void ScrollAreaViewport::Reset()
{
    this->mMouseChild=this->mFocusChild=NULL;
    this->mChildTimerSet.clear();
    this->mChildList.clear();
    WindowManager* manager=WONAPI::WindowManager::GetDefaultWindowManager();
    if(manager!=NULL) manager->CheckEndPopup(this);
}
// FUNCTION: WON 0x00705db0
// Sets the focused child (container behaviour)
void ScrollAreaViewport::SetFocusChild(Component* child) { Container::SetFocusChild(child); }
// FUNCTION: WON 0x00705dc0
// Registers a child for timer events (container behaviour)
void ScrollAreaViewport::AddTimerChild(Component* child) { Container::AddTimerChild(child); }
// FUNCTION: WON 0x00705dd0
// Passes a timer tick to the timer children (container behaviour)
bool ScrollAreaViewport::TimerEvent(int delta) { return Container::TimerEvent(delta); }
// FUNCTION: WON 0x00705de0
// Sets the cursor on the owning scroll area instead of the viewport
void ScrollAreaViewport::SetCursor(Cursor* cursor) { this->mScrollArea->SetCursor(cursor); }
// FUNCTION: WON 0x00705e00
// Paints the children (container behaviour)
void ScrollAreaViewport::Paint(Graphics& graphics) { Container::Paint(graphics); }
}

namespace WONAPI {
// FUNCTION: WON 0x00705d70
// Repaints the scroll area when an invalidated child is in view; returns false
bool ScrollAreaViewport::ChildInvalidated(Component* child)
{
    WONRectangle bounds=child->GetBounds();
    this->InvalidateIfRectVisible(bounds);
    return false;
}
}

namespace WONAPI {
// Ignores child state changes (the linker folds this empty body into the identical one at 0x006fa500)
void ScrollAreaViewport::ChildStateChanged(Component*) {}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x0071b2a0
// (public: class std::_Tree<class WONAPI::SmartPtr<class WONAPI::Component>,class WONAPI::SmartPtr<class WONAPI::Component>,struct std::_Setk<class WONAP)
