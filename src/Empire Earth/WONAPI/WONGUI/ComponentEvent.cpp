// Component events and input/popup event delivery (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// FUNCTION: WON 0x0072b530
// Creates an event of the given type for a component (not a broadcast)
ComponentEvent::ComponentEvent(Component* c, int type)
 : mComponentRef(c), mType(type), mComponent(c) { this->mBroadcast=false; }
// FUNCTION: WON 0x0072b580
// (??_GComponentEvent@WONAPI@@UAEPAXI@Z)

// FUNCTION: WON 0x0072b5a0
// Retargets the event to another component
void ComponentEvent::SetComponent(Component* c) { this->mComponent=c; this->mComponentRef=c; }
// FUNCTION: WON 0x0072b600
// Returns the control id of the event's component
int ComponentEvent::GetControlId() const { return this->mComponent->mControlId; }
// FUNCTION: WON 0x0072b610
// Delivers the event: broadcasts it through the window manager or bubbles it up from the component
void ComponentEvent::Deliver() {
 if (this->mBroadcast) this->mWindow->mManager->BroadcastEvent(this);
 else this->mComponent->BubbleEvent(this);
}
}

#include "ee/wonapi/WONLobbyPopupEvent.h"
namespace WONAPI {
// FUNCTION: WON 0x0072b520
// Passes the input event to its window
void InputEvent::Deliver() { this->mWindow->HandleInputEvent(this); }
// FUNCTION: WON 0x0072b630
// Passes the mouse event to its window
void MouseEvent::Deliver() { this->mWindow->HandleMouseEvent(this); }
// FUNCTION: WON 0x0072b640
// Passes the key event to its window
void KeyEvent::Deliver() { this->mWindow->HandleKeyEvent(this); }
// FUNCTION: WON 0x0072b650
// Opens the popup described by the event in its window
void DoPopupEvent::Deliver() { this->mWindow->OpenPopup(this->mParams); }
}
