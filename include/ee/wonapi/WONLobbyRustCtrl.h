// The lobby screens' controls in Rust (R6 WONLobby round 4): GameBrowserCtrl, GameStagingCtrl, LobbyOptionsCtrl and
// GameUserListCtrl are one ee_won lobbyctl.rs object each (the controls held, the flags, every rule); this shell
// forwards InitComponent and HandleComponentEvent, and the four classes are its subclasses. The host is
// WONLobby/LobbyCtrlHost.cpp.
#ifndef EE_WONAPI_WONLOBBYRUSTCTRL_H
#define EE_WONAPI_WONLOBBYRUSTCTRL_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "rust/won_lobby/lobbyctl.h"
#include "rust/won_lobby/srvlistctl.h"
namespace WONAPI {
class LobbyRustCtrl : public Container {
public:
    explicit LobbyRustCtrl(EeWonLobbyCtlKind inKind);
    virtual ~LobbyRustCtrl(); // the controls released in reverse member order, before the Container base
    virtual void InitComponent(ComponentInit& inInit);
    virtual void HandleComponentEvent(ComponentEvent* inEvent);
    EeWonLobbyCtl* mLobbyCtl;
protected:
    intptr_t CCall(EeWonLobbyCtlCall c, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0) {
        return ee_won_lobbyctl_call(this->mLobbyCtl, c, a, b, cc); }
};
}
#endif
