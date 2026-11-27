// the members only the drivers use, cut from ee/wonapi/WONLobbyModel.h's LobbyGroup (R6 WON GUI round 40):
// included in its class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
    LobbyClient* GetCaptain() { return (LobbyClient*)this->M(LobbyMC::GroupCaptain); }
    bool IsPasswordProtected() { return (this->M(LobbyMC::GroupKind) & 1) != 0; }
    bool IsGameGroup() { return (this->M(LobbyMC::GroupKind) & 8) == 0; }
    unsigned long GetNumClients() const { return (unsigned long)this->MGet(LobbyMF::GroupNumClients); }
