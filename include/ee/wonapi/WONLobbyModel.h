// WONLobby data model: friends, groups (rooms and games), servers and game lists. Shells over their Rust objects
// (R6 WONLobby round 10; rust/crates/ee_won/src/modelobj.rs, as WONLobbyModelObj.h's player, client and game); the
// methods over the routing server's and directory's records are WONLobby/LobbyModelHost.cpp's.
#ifndef EE_WONAPI_WONLOBBYMODEL_H
#define EE_WONAPI_WONLOBBYMODEL_H
#include <set>
#include <time.h>
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {

// LobbyFriend: a friend list entry; 0x50 bytes, the destructor is its only virtual function.
class LobbyFriend : public LobbyModelObject {
public:
    LobbyFriend(const GUIString& inName, unsigned long inUserId) : LobbyModelObject(EeWonModelKind::Friend)
    {
        this->MSetText(LobbyMF::FriendName, inName);
        this->MSet(LobbyMF::FriendUserId, (intptr_t)inUserId);
    }
    void SetLocation(const IPAddr& inAddress, const GUIString& inLocation, int inFlags)
    {
        this->M(LobbyMC::FriendSetLocation, (intptr_t)inAddress.GetValue(), (intptr_t)inLocation.mData.get(), inFlags);
    }
    void ClearLocation() { this->M(LobbyMC::FriendClearLocation); }
    void SetStatusFromClient(LobbyClient* inClient) { this->M(LobbyMC::FriendSetStatusFromClient, (intptr_t)inClient); }
    void SetAnonymous(bool inAnonymous) { this->MSet(LobbyMF::FriendAnonymous, inAnonymous); }
    void StartLocate(unsigned long inTime) { this->M(LobbyMC::FriendStartLocate, (intptr_t)inTime); }
    void EndLocate(unsigned long inTime) { this->M(LobbyMC::FriendEndLocate, (intptr_t)inTime); }
    unsigned long GetUserId() { return (unsigned long)this->MGet(LobbyMF::FriendUserId); }
    void SetLastLocate(unsigned long inTime) { this->MSet(LobbyMF::FriendLastLocate, (intptr_t)inTime); }
    unsigned long GetSearchStart() const { return (unsigned long)this->MGet(LobbyMF::FriendSearchStart); }
    bool GetAnonymous() const { return this->MGet(LobbyMF::FriendAnonymous) != 0; }
    GUIString GetName() const { return this->MText(LobbyMF::FriendName); }
    unsigned char GetFlags() const { return (unsigned char)this->MGet(LobbyMF::FriendFlags); }
    IPAddr GetAddress() const { return this->MAddr(LobbyMF::FriendAddress); }
    GUIString GetLocation() const { return this->MText(LobbyMF::FriendLocation); }
    unsigned long GetLastLocate() const { return (unsigned long)this->MGet(LobbyMF::FriendLastLocate); }
    unsigned long GetLastSearch() const { return (unsigned long)this->MGet(LobbyMF::FriendLastSearch); }
    void SetLastSearch(unsigned long inCount) { this->MSet(LobbyMF::FriendLastSearch, (intptr_t)inCount); }
};

// LobbyGroup: a routing group, i.e. a chat room or a game. RTTI: LobbyGroup : RefCount; vtable 0x852c4c; 0x20 bytes.
class LobbyGroup : public LobbyModelObject {
public:
    LobbyGroup() : LobbyModelObject(EeWonModelKind::Group) {}
    void ReadRoutingInfo(RoutingGroupInfo*, LobbyClientList*);
    void SetMembers(const RoutingMemberMap&, LobbyClientList*);
    void AddClient(LobbyClient* inClient, unsigned char inFlags) { this->M(LobbyMC::GroupAdd, (intptr_t)inClient, inFlags); }
    SmartPtr<LobbyClient> RemoveClient(unsigned short inId) { return LobbyModelHeld<LobbyClient>(this->M(LobbyMC::GroupRemove, inId)); }
    LobbyClient* SetCaptain(unsigned short inId, LobbyClient** outOld) { return (LobbyClient*)this->M(LobbyMC::GroupSetCaptain, inId, (intptr_t)outOld); }
    LobbyClient* GetCaptain() { return (LobbyClient*)this->M(LobbyMC::GroupCaptain); }
    LobbyClient* FindClient(unsigned short inId) { return (LobbyClient*)this->M(LobbyMC::GroupFind, inId); }
    bool IsPasswordProtected() { return (this->M(LobbyMC::GroupKind) & 1) != 0; }
    bool IsAskToJoin() { return (this->M(LobbyMC::GroupKind) & 2) != 0; }
    bool IsInviteOnly() { return (this->M(LobbyMC::GroupKind) & 4) != 0; }
    bool IsChatGroup() { return (this->M(LobbyMC::GroupKind) & 8) != 0; }
    bool IsGameGroup() { return (this->M(LobbyMC::GroupKind) & 8) == 0; }
    bool IsPublicChatGroup() { return (this->M(LobbyMC::GroupKind) & 0x10) != 0; }
    unsigned short GetId() { return (unsigned short)this->MGet(LobbyMF::GroupId); }
    GUIString GetName() const { return this->MText(LobbyMF::GroupName); }
    unsigned short GetCaptainId() const { return (unsigned short)this->MGet(LobbyMF::GroupCaptainId); }
    unsigned long GetNumClients() const { return (unsigned long)this->MGet(LobbyMF::GroupNumClients); }
    unsigned long GetFlags() const { return (unsigned long)this->MGet(LobbyMF::GroupFlags); }
    SmartPtr<LobbyClientList> GetClients() const { return SmartPtr<LobbyClientList>((LobbyClientList*)this->MGet(LobbyMF::GroupClients)); }
};

LobbyFriend* LobbyFindFriend(unsigned long inUserId); // looks the user up in the persistent friend list

// LobbyServer: a WON routing server with its groups and clients; 0x68 bytes, RTTI LobbyServer : RefCount.
class LobbyServer : public LobbyModelObject {
public:
    LobbyServer() : LobbyModelObject(EeWonModelKind::Server) {}
    void Disconnect() { this->M(LobbyMC::ServerDisconnect, (intptr_t)time(NULL)); }
    bool HasAddress(const std::string& inName) { return this->M(LobbyMC::ServerHasAddress, (intptr_t)inName.data(), (intptr_t)inName.length()) != 0; }
    void SetConnection(RoutingConnection* inConnection) { this->MSet(LobbyMF::ServerConnection, (intptr_t)inConnection); }
    void ReadDirEntity(DirEntity* inEntity);
    void SyncClients(const RoutingClientMap& inClients);
    void UpdateFriends() { this->M(LobbyMC::ServerUpdateFriends); }
    void ClearFriendLocations() { this->M(LobbyMC::ServerClearFriendLocations, (intptr_t)time(NULL)); }
    void SyncGroups(const RoutingGroupMap& inGroups, bool inFlag);
    LobbyClient* AddClient(RoutingClientInfo* inInfo) { return (LobbyClient*)this->M(LobbyMC::ServerAddClient, (intptr_t)inInfo); }
    void AddClientToGroup(LobbyClient* inClient, unsigned char inFlags, LobbyGroup* inGroup)
    {
        this->M(LobbyMC::ServerAddClientToGroup, (intptr_t)inClient, inFlags, (intptr_t)inGroup);
    }
    LobbyGroup* AddGroup(RoutingGroupInfo* inInfo) { return (LobbyGroup*)this->M(LobbyMC::ServerAddGroup, (intptr_t)inInfo); }
    SmartPtr<LobbyGroup> RemoveGroup(unsigned short inId) { return LobbyModelHeld<LobbyGroup>(this->M(LobbyMC::ServerRemoveGroup, inId)); }
    SmartPtr<LobbyClient> RemoveClient(unsigned short inId) { return LobbyModelHeld<LobbyClient>(this->M(LobbyMC::ServerRemoveClient, inId)); }
    SmartPtr<LobbyClient> RemoveClientFromGroup(unsigned short inId, LobbyGroup* inGroup)
    {
        return LobbyModelHeld<LobbyClient>(this->M(LobbyMC::ServerRemoveClientFromGroup, inId, (intptr_t)inGroup));
    }
    // 0x7aa370 (int: EELobbyGame at 0x692c7e zero-extends the id it passes)
    LobbyClient* GetClient(int inId) { return (LobbyClient*)this->M(LobbyMC::ServerGetClient, inId); }
    LobbyClient* FindClientByWONId(unsigned long inId) { return (LobbyClient*)this->M(LobbyMC::ServerFindByWonId, (intptr_t)inId); }
    LobbyClient* GetMyClient() { return (LobbyClient*)this->M(LobbyMC::ServerMyClient); }
    LobbyGroup* GetGroup(int inId) { return (LobbyGroup*)this->M(LobbyMC::ServerGroup, inId); }
    int GetNumClients() { return (int)this->M(LobbyMC::ServerNumClientsNow); }
    bool IsConnected() { return this->M(LobbyMC::ServerConnected) != 0; }
    void ClearGroups() { this->M(LobbyMC::ServerClearGroups); }
    size_t GetGroupCount() const { return (size_t)this->M(LobbyMC::ServerGroupCount); }
    LobbyGroup* GroupAt(size_t i) const { return (LobbyGroup*)this->M(LobbyMC::ServerGroupAt, (intptr_t)i); } // in id order

    unsigned long GetLastPing() const { return (unsigned long)this->MGet(LobbyMF::ServerLastPing); }
    unsigned short GetMyClientId() const { return (unsigned short)this->MGet(LobbyMF::ServerMyClientId); }
    GUIString GetName() const { return this->MText(LobbyMF::ServerName); }
    IPAddr GetAddress() const { return this->MAddr(LobbyMF::ServerAddress); }
    SmartPtr<LobbyClientList> GetClients() const { return SmartPtr<LobbyClientList>((LobbyClientList*)this->MGet(LobbyMF::ServerClients)); }
    SmartPtr<RoutingConnection> GetConnection() const { return SmartPtr<RoutingConnection>((RoutingConnection*)this->MGet(LobbyMF::ServerConnection)); }
    void SetLastPing(unsigned long inTime) { this->MSet(LobbyMF::ServerLastPing, (intptr_t)inTime); }
    void SetMyClientId(unsigned short inId) { this->MSet(LobbyMF::ServerMyClientId, inId); }
    bool GetQueryingGroups() const { return this->MGet(LobbyMF::ServerQuerying) != 0; }
    void SetQueryingGroups(bool inValue) { this->MSet(LobbyMF::ServerQuerying, inValue); }
};

// the servers, in address order
class LobbyServerList : public LobbyModelObject {
public:
    LobbyServerList() : LobbyModelObject(EeWonModelKind::ServerList) {}
    void UpdateFromDirectory(const DirEntityList& inEntities);
    LobbyServer* FindServer(const IPAddr& inAddress) { return (LobbyServer*)this->M(LobbyMC::ServerListFind, (intptr_t)inAddress.GetValue()); }
    size_t GetCount() const { return (size_t)this->M(LobbyMC::ServerListCount); }
    bool IsEmpty() const { return this->GetCount() == 0; }
    LobbyServer* ServerAt(size_t i) const { return (LobbyServer*)this->M(LobbyMC::ServerListAt, (intptr_t)i); }
    IPAddr AddressAt(size_t i) const
    {
        return IPAddr::FromValue(ee_won_ipaddr_clone((const EeWonIpAddr*)this->M(LobbyMC::ServerListAddressAt, (intptr_t)i)));
    }
};

// Vtable 0x852c44, RTTI locator 0x87aca0, type descriptor 0x8ded08; 0x2c bytes (allocated at 0x7e6e80).
class LobbyGameList : public LobbyModelObject {
public:
    LobbyGameList(int inType) : LobbyModelObject(EeWonModelKind::GameList) { this->MSet(LobbyMF::GameListType, inType); }
    void Clear() { this->M(LobbyMC::GameListClear); }
    void AddGame(LobbyGame* inGame) { this->M(LobbyMC::GameListAdd, (intptr_t)inGame); }
    LobbyGame* GetOrCreateGame(unsigned short inId, bool* outExisted) { return (LobbyGame*)this->M(LobbyMC::GameListGetOrCreateId, inId, (intptr_t)outExisted); }
    LobbyGame* GetOrCreateGame(const IPAddr& inAddr, bool* outExisted)
    {
        return (LobbyGame*)this->M(LobbyMC::GameListGetOrCreateAddr, (intptr_t)inAddr.GetValue(), (intptr_t)outExisted);
    }
    LobbyGame* GetGame(unsigned short inId) { return (LobbyGame*)this->M(LobbyMC::GameListGetId, inId); }
    LobbyGame* GetGame(const IPAddr& inAddr) { return (LobbyGame*)this->M(LobbyMC::GameListGetAddr, (intptr_t)inAddr.GetValue()); }
    SmartPtr<LobbyGame> RemoveGame(unsigned short inId) { return LobbyModelHeld<LobbyGame>(this->M(LobbyMC::GameListRemoveId, inId)); }
    SmartPtr<LobbyGame> RemoveGame(const IPAddr& inAddr) { return LobbyModelHeld<LobbyGame>(this->M(LobbyMC::GameListRemoveAddr, (intptr_t)inAddr.GetValue())); }
    void Rewind() { this->M(LobbyMC::GameListRewind); }
    bool HasMore() { return this->M(LobbyMC::GameListHasMore) != 0; }
    LobbyGame* Next() { return (LobbyGame*)this->M(LobbyMC::GameListNext); } // NULL at the end, where the original read past it
    bool IsEmpty() { return this->M(LobbyMC::GameListIsEmpty) != 0; }
    bool RemoveStaleGames(long inMaxAge) { return this->M(LobbyMC::GameListRemoveStale, inMaxAge, (intptr_t)time(NULL)) != 0; }
    long GetType() const { return (long)this->MGet(LobbyMF::GameListType); }
};

}
#endif
