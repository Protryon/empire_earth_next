// the members only the drivers use, cut from ee/wonapi/WONLobbyModelObj.h's LobbyGame (R6 WON GUI round 40):
// included in its class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
    bool IsSameGame(LobbyGame* inOther) { return this->M(LobbyMC::GameIsSameGame, (intptr_t)inOther) != 0; }
    void ReadGroup(LobbyGroup* inGroup) { this->M(LobbyMC::GameReadGroup, (intptr_t)inGroup); }
    void HandleHostGame(LobbyClient* inClient) { this->M(LobbyMC::GameHandleHost, (intptr_t)inClient); }
    short GetJoinStatus() { return (short)this->M(LobbyMC::GameJoinStatus); }
    void SetMaxPlayers(long inSlots) { this->MSet(LobbyMF::GameOpenSlots, inSlots); }
