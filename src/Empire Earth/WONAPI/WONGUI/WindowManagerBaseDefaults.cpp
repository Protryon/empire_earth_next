// Base (empty) platform hooks of the window manager (file name inferred)
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
// FUNCTION: WON 0x0072e7c0
// Waits for events: the base version only sleeps 20 ms
void WindowManager::GetNextEvent() { Sleep(20); }
// FUNCTION: WON 0x0072c380
// Base screen-mode change: changes nothing and returns false
bool WindowManager::SetScreenResolutionHook(int,int) { return false; }
// Empty screen-resolution query (the linker folds this body into the identical one at 0x00727440)
void WindowManager::GetScreenResolutionHook(int&,int&) {}
// Empty event-loop wake-up (the linker folds this body into the identical one at 0x00726b60)
void WindowManager::Signal() {}
// Empty screen-rectangle query (the linker folds this body into the identical one at 0x0070a800)
void WindowManager::GetScreenRect(WONRectangle&) {}
// Empty system colour scheme hook (the linker folds this body into the identical one at 0x00726b60)
void WindowManager::GetSystemColorScheme() {}
}
