// the members only the drivers use, cut from ee/wonapi/WONLobbyModel.h's LobbyServer (R6 WON GUI round 40):
// included in its class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
    void UpdateFriends() { this->M(LobbyMC::ServerUpdateFriends); }
    void ClearFriendLocations() { this->M(LobbyMC::ServerClearFriendLocations, (intptr_t)time(NULL)); }
    void AddClientToGroup(LobbyClient* inClient, unsigned char inFlags, LobbyGroup* inGroup)
    {
        this->M(LobbyMC::ServerAddClientToGroup, (intptr_t)inClient, inFlags, (intptr_t)inGroup);
    }
    SmartPtr<LobbyClient> RemoveClientFromGroup(unsigned short inId, LobbyGroup* inGroup)
    {
        return LobbyModelHeld<LobbyClient>(this->M(LobbyMC::ServerRemoveClientFromGroup, inId, (intptr_t)inGroup));
    }
    int GetNumClients() { return (int)this->M(LobbyMC::ServerNumClientsNow); }
