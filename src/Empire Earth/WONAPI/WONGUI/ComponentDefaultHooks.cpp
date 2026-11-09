// Component default virtual hooks (file name inferred)
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
// Does nothing when the position changes (the linker folds this body into the identical one at 0x008142a0)
void Component::PositionChanged() {}
// FUNCTION: WON 0x006f5050
// Reports a hit when the component is visible and does not ignore input (flag 0x800)
bool Component::HitTest(int,int)
{
    return this->IsVisible() && !this->ComponentFlagSet(ComponentFlag_NoInput);
}
// Ignores mouse drags (the linker folds this body into the identical one at 0x004ac9c8)
void Component::MouseDrag(int,int) {}
// Reports timer events as handled (the linker folds this body into the identical one at 0x00620a18)
bool Component::TimerEvent(int) { return true; }
// Allows the window to close (the linker folds this body into the identical one at 0x00418aa9)
bool Component::CanClose() { return true; }
// Does nothing when the enabled state changes (the linker folds this body into the identical one at 0x00556dd8)
void Component::EnabledChanged(bool) {}
// FUNCTION: WON 0x006f4c40
// Passes the cursor request up to the parent
void Component::SetCursor(Cursor* cursor)
{
    if(this->mParent!=NULL) this->mParent->SetCursor(cursor);
}
// FUNCTION: WON 0x006f52c0
// Handles a broadcast event like any component event
void Component::HandleBroadcastEvent(ComponentEvent* event)
{
    this->HandleComponentEvent(event);
}
// Returns no mouse child (the linker folds this body into the identical one at 0x005ad67e)
Component* Component::GetMouseChild() { return NULL; }
// FUNCTION: WON 0x006f52a0
// Marks the event as a broadcast and delivers it, now or queued
void Component::FireBroadcastEvent(ComponentEvent* event,bool sendNow)
{
    event->mBroadcast=true;
    this->DeliverEvent(event,sendNow);
}
// FUNCTION: WON 0x006f4c20
// Asks the parent to stack this component above the sibling
void Component::EnsureOnTopOf(Component* sibling)
{
    if(this->mParent!=NULL) this->mParent->EnsureOnTopOf(sibling,this);
}
// FUNCTION: WON 0x006f4fb0
// Makes the component translucent with the given color and level, through its background grabber
void Component::SetTranslucentColor(int color,int level)
{
    this->SetComponentFlags(ComponentFlag_Translucent,true);
    if(!this->CheckGrabBG()) return;
    this->mExtraStuff->mGrabBG->SetTranslucentColor(color,level);
}

}
