// the members only the drivers use, cut from ee/wonapi/WONLobbyModel.h's LobbyFriend (R6 WON GUI round 40):
// included in its class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
    void ClearLocation() { this->M(LobbyMC::FriendClearLocation); }
    void SetStatusFromClient(LobbyClient* inClient) { this->M(LobbyMC::FriendSetStatusFromClient, (intptr_t)inClient); }
    void StartLocate(unsigned long inTime) { this->M(LobbyMC::FriendStartLocate, (intptr_t)inTime); }
    void EndLocate(unsigned long inTime) { this->M(LobbyMC::FriendEndLocate, (intptr_t)inTime); }
