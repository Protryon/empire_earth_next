// Component defaults: desired size, event firing and double-click test
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
// FUNCTION: WON 0x006f4e70
// Returns the default desired size of 20 x 20
void Component::GetDesiredSize(int& width,int& height) { width=20; height=20; }
// FUNCTION: WON 0x006f4f20
// Sets the component's height to its desired height plus the padding, keeping its width
void Component::SetDesiredHeight(int padding)
{
    int width=0,height=0;
    this->GetDesiredSize(width,height);
    this->SetSize(this->Width(),height+padding);
}
// FUNCTION: WON 0x006f5380
// Creates an event of the given type from this component and delivers it now or queues it
void Component::FireEvent(int type,bool sendNow)
{
    this->DeliverEvent(new ComponentEvent(this,type),sendNow);
}
// FUNCTION: WON 0x006f5550
// Passes a child's event on up to the parent (default handling)
void Component::HandleComponentEvent(ComponentEvent* event) { this->BubbleEvent(event); }
// FUNCTION: WON 0x006f57b0
// Returns whether the root window saw the last press of the mouse button as a double click
bool Component::CheckDoubleClick(int button)
{
    Window* aWindow=this->GetRoot();
    return aWindow->CheckDoubleClick(static_cast<MouseButton>(button));
}
// FUNCTION: WON 0x0072b110
// Returns whether the last press of the mouse button was a double click
bool Window::CheckDoubleClick(MouseButton button) { return this->mButtonDoubleClick[button]; }
}
