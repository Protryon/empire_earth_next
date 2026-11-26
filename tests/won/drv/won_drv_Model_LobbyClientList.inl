// the members only the drivers use, cut from ee/wonapi/WONLobbyModelObj.h's LobbyClientList (R6 WON GUI round 40):
// included in its class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
    void UpdateIgnoreStatus() { this->M(LobbyMC::ListUpdateIgnore); }
    void ClearInvited() { this->M(LobbyMC::ListClearInvited); }
