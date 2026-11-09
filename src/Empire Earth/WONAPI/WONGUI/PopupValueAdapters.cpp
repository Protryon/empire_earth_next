// Popup parameters and window dialog/popup forwarders (file name inferred)
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
// FUNCTION: WON 0x00728540
// Describes a popup to open: the component, flags, owner component and position (no parent yet)
PopupParams::PopupParams(Component* popup,int flags,Component* owner,int x,int y)
{
    this->mPopup=popup;
    this->mFlags=flags;
    this->mParent=NULL;
    this->mInputComponent=NULL;
    this->mOwnerComponent=owner;
    this->mx=x;
    this->my=y;
}
// FUNCTION: WON 0x0072b0a0
// Asks the window manager to end this window's dialog with a result
void Window::EndDialog(int result,Component* component)
{
    this->mManager->EndDialog(result,this,component);
}
// FUNCTION: WON 0x0072b0c0
// Opens a popup with this window as its parent through the window manager
void Window::OpenPopup(PopupParams& params)
{
    params.mParent=this;
    this->mManager->DoPopup(params);
}
// FUNCTION: WON 0x0072b0e0
// Asks the window manager to close a popup
void Window::ClosePopup(Component* component)
{
    this->mManager->EndPopup(component);
}
}
