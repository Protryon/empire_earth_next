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
// Returns the default desired size of 20 x 20
void Component::GetDesiredSize(int& width,int& height) { width=20; height=20; }
// Sets the component's height to its desired height plus the padding, keeping its width
void Component::SetDesiredHeight(int padding)
{
    int width=0,height=0;
    this->GetDesiredSize(width,height);
    this->SetSize(this->Width(),height+padding);
}
// Creates an event of the given type from this component and delivers it now or queues it
void Component::FireEvent(int type,bool sendNow)
{
    this->DeliverEvent(new ComponentEvent(this,type),sendNow);
}
// Passes a child's event on up to the parent (default handling)
void Component::HandleComponentEvent(ComponentEvent* event) { this->BubbleEvent(event); }
// Returns whether the root window saw the last press of the mouse button as a double click
bool Component::CheckDoubleClick(int button)
{
    Window* aWindow=this->GetRoot();
    return aWindow->CheckDoubleClick(static_cast<MouseButton>(button));
}
// Returns whether the last press of the mouse button was a double click
bool Window::CheckDoubleClick(MouseButton button) { return this->WCall(EeWonWinCall::CheckDoubleClick, (intptr_t)button) != 0; }
}
