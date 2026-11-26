// the members only the drivers use, cut from ee/wonapi/WONLobbyModelObj.h's LobbyClient (R6 WON GUI round 40):
// included in its class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
    void ClearGroup(unsigned short inId, bool inGame) { this->M(LobbyMC::ClientClearGroup, inId, inGame); }
    bool IsInChatGroup() { return this->M(LobbyMC::ClientInChat) != 0; }
    bool IsInGameGroup() { return this->M(LobbyMC::ClientInGame) != 0; }
    void UpdateIgnoreStatus() { this->M(LobbyMC::ClientUpdateIgnore); }
    bool IsAnonymous() { return this->M(LobbyMC::ClientAnonymous) != 0; }
    bool IsMuted(bool inGame) { return this->M(LobbyMC::ClientMuted, inGame) != 0; }
    bool IsAway() { return this->M(LobbyMC::ClientAway) != 0; }
    bool IsAdmin() { return this->M(LobbyMC::ClientAdmin) != 0; }
    bool IsModerator() { return this->M(LobbyMC::ClientModerator) != 0; }
    bool IsCaptain(bool inGame) { return this->M(LobbyMC::ClientIsCaptain, inGame) != 0; }
    bool HasPlayer() { return this->M(LobbyMC::ClientHasPlayer) != 0; }
    void SetMemberFlags(unsigned char inFlags, bool inGame) { this->M(LobbyMC::ClientSetMemberFlags, inFlags, inGame); }
    void SetClientId(unsigned short inId) { this->MSet(LobbyMF::ClientId, inId); }
    void SetClientFlags(unsigned long inFlags) { this->MSet(LobbyMF::ClientFlags, (intptr_t)inFlags); }
