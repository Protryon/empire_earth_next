// Window::RunDialog (file name inferred)
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
// Window::DoDialog in the WON SDK
// FUNCTION: WON 0x0072b080
// Runs the component as a modal dialog over this window through the window manager; returns the dialog result
int Window::RunDialog(Component* component)
{
    return this->mManager->DoDialog(this,component);
}
}
