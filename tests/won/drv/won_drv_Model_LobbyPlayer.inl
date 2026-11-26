// the members only the drivers use, cut from ee/wonapi/WONLobbyModelObj.h's LobbyPlayer (R6 WON GUI round 40):
// included in its class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
    bool IsSameTeam(LobbyPlayer* inOther) { return this->M(LobbyMC::PlayerIsSameTeam, (intptr_t)inOther) != 0; }
