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
// FUNCTION: WON 0x00729e60
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
// FUNCTION: WON 0x007272f0
// Does nothing on the base window (platform windows move and resize)
void Window::SetPosSize(int,int,int,int) {}
// FUNCTION: WON 0x00727440
// Leaves the rectangle unchanged on the base window
void Window::GetRect(WONRectangle&,bool) {}
// Does nothing on the base window (the linker folds this empty body into the identical one at 0x008142a0)
void Window::PlatformSetFocus() {}
// Ignores the request on the base window (the linker folds this empty body into the identical one at 0x006fa500)
void Window::PlatformSetMouseCapture(bool) {}
// FUNCTION: WON 0x0072b0f0
// Tags the event with this window and posts it to the window manager's queue
void Window::QueueEvent(WindowEventBase* event) { event->mWindow=this; this->mManager->PostEvent(event); }
// FUNCTION: WON 0x00729e80
// Returns whether the window was last shown
bool Window::PlatformIsVisible() { return this->mIsShowing; }
}
