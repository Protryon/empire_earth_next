// the members only the drivers use, cut from ee/wonapi/WONLobbyModel.h's LobbyGameList (R6 WON GUI round 40):
// included in its class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
    bool RemoveStaleGames(long inMaxAge) { return this->M(LobbyMC::GameListRemoveStale, inMaxAge, (intptr_t)time(NULL)) != 0; }
