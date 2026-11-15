// Default (empty) platform hooks of the base window (file name inferred)
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
// Returns no font (platform windows override this)
SmartPtr<Font> Window::PlatformGetFont(const FontDescriptor&) { return NULL; }
// Does nothing on the base window (the linker folds this empty body into the identical one at 0x008142a0)
void Window::PlatformSetTitle() {}
// Does nothing on the base window (the linker folds this empty body into the identical one at 0x008142a0)
void Window::PlatformSetIcon() {}
// Ignores the cursor on the base window (the linker folds this empty body into the identical one at 0x006fa500)
void Window::PlatformSetCursor(Cursor*) {}
// Ignores the request on the base window (the linker folds this empty body into the identical one at 0x006fa500)
void Window::PlatformShowNativeCursor(bool) {}
// Ignores the request on the base window (the linker folds this empty body into the identical one at 0x006fa500)
void Window::PlatformShow(bool) {}
// Ignores the request on the base window (the linker folds this empty body into the identical one at 0x006fa500)
void Window::PlatformMinimize(bool) {}
// Ignores the request on the base window (the linker folds this empty body into the identical one at 0x006fa500)
void Window::PlatformSetTopMost(bool) {}
// Does nothing on the base window (platform windows move and resize)
void Window::SetPosSize(int,int,int,int) {}
// Leaves the rectangle unchanged on the base window
void Window::GetRect(WONRectangle&,bool) {}
// Does nothing on the base window (the linker folds this empty body into the identical one at 0x008142a0)
void Window::PlatformSetFocus() {}
// Ignores the request on the base window (the linker folds this empty body into the identical one at 0x006fa500)
void Window::PlatformSetMouseCapture(bool) {}
// Tags the event with this window and posts it to the window manager's queue
void Window::QueueEvent(WindowEventBase* event) { event->mWindow=this; this->mManager->PostEvent(event); }
// Returns whether the window was last shown
bool Window::PlatformIsVisible() { return this->IsShowing(); }
}
