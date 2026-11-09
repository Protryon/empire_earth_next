// WONLobby data model: friends, groups (rooms and games), servers and game lists
#ifndef EE_WONAPI_WONLOBBYMODEL_H
#define EE_WONAPI_WONLOBBYMODEL_H
#include <set>
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {

// LobbyFriend: a friend list entry; 0x50 bytes, the destructor is its only virtual function.
class LobbyFriend : public RefCount {
public:
    LobbyFriend(const GUIString&, unsigned long);
    // Implicit destructor (0x7a6380): it does not write its own vtable pointer.
    void SetLocation(const IPAddr&, const GUIString&, int);
    void ClearLocation();
    void SetStatusFromClient(LobbyClient*);
    void SetAnonymous(bool);
    void StartLocate(unsigned long);
    void EndLocate(unsigned long);
    GUIString mName;             // 0x08
    unsigned long GetUserId() { return this->mUserId; }
    unsigned long mUserId;       // 0x0c
    IPAddr mAddress;             // 0x10
    GUIString mLocation;         // 0x38
    unsigned long mLastSearch;   // 0x3c (meaning inferred): zeroed by the constructor, kept on reset
    unsigned char mFlags;       // 0x40
    void SetLastLocate(unsigned long inTime) { this->mLastLocate = inTime; }
    unsigned long mLastLocate;   // 0x44
    unsigned long mSearchStart;  // 0x48
    bool mAnonymous;             // 0x4c
public:
    unsigned long GetSearchStart() const { return this->mSearchStart; }
    bool GetAnonymous() const { return this->mAnonymous; }
public:
    const GUIString& GetName() const { return this->mName; }
    unsigned char GetFlags() const { return this->mFlags; }
};

// LobbyGroup: a routing group, i.e. a chat room or a game (member meanings inferred).
// RTTI: LobbyGroup : RefCount; vtable 0x852c4c (destructor only); 0x20 bytes, allocated at 0x7a9ca5.
class LobbyGroup : public RefCount {
public:
    LobbyGroup();
    virtual ~LobbyGroup();
    void ReadRoutingInfo(RoutingGroupInfo*, LobbyClientList*);
    void SetMembers(const RoutingMemberMap&, LobbyClientList*);
    void AddClient(LobbyClient*, unsigned char);
    SmartPtr<LobbyClient> RemoveClient(unsigned short);
    LobbyClient* SetCaptain(unsigned short, LobbyClient**);
    LobbyClient* GetCaptain();
    LobbyClient* FindClient(unsigned short);
    bool IsPasswordProtected();
    bool IsAskToJoin();
    bool IsInviteOnly();
    bool IsChatGroup();
    bool IsGameGroup();
    bool IsPublicChatGroup();
    GUIString mName;                   // 0x08
    unsigned short GetId() { return this->mId; }
    unsigned short mId;               // 0x0c
    unsigned short mCaptainId;        // 0x0e
    unsigned long mNumClients;        // 0x10
    unsigned long mMaxClients;        // 0x14
    unsigned long mFlags;             // 0x18
    SmartPtr<LobbyClientList> mClients;  // 0x1c
};

typedef std::set<std::string, StringLessNoCase> LobbyServerAddressSet;
typedef std::map<unsigned short, SmartPtr<LobbyGroup> > LobbyGroupMap;
LobbyFriend* LobbyFindFriend(unsigned long inUserId); // looks the user up in the persistent friend list

// LobbyServer: a WON routing server with its groups and clients; 0x68 bytes, RTTI LobbyServer : RefCount.
class LobbyServer : public RefCount {
public:
    LobbyServer();                         // 0x7a91c0
    void Disconnect(); // 0x7a96a0
    virtual ~LobbyServer();                // 0x7a9500
    bool HasAddress(const std::string& inName);
    void SetConnection(RoutingConnection* inConnection);
    void ReadDirEntity(DirEntity* inEntity);
    void SyncClients(const RoutingClientMap& inClients);
    void UpdateFriends();
    void ClearFriendLocations();
    void SyncGroups(const RoutingGroupMap& inGroups, bool inFlag);
    LobbyClient* AddClient(RoutingClientInfo* inInfo);
    void AddClientToGroup(LobbyClient* inClient, unsigned char inFlags, LobbyGroup* inGroup);
    LobbyGroup* AddGroup(RoutingGroupInfo* inInfo);
    SmartPtr<LobbyGroup> RemoveGroup(unsigned short inId);
    SmartPtr<LobbyClient> RemoveClient(unsigned short inId);
    SmartPtr<LobbyClient> RemoveClientFromGroup(unsigned short inId, LobbyGroup* inGroup);
    LobbyClient* GetClient(int inId);       // 0x7aa370 (int: EELobbyGame at 0x692c7e zero-extends the id it passes)
    LobbyClient* FindClientByWONId(unsigned long inId);
    LobbyClient* GetMyClient();
    LobbyGroup* GetGroup(int inId);
    int GetNumClients();
    bool IsConnected();
    void ClearGroups();

    GUIString mName;                       // +0x08
    LobbyServerAddressSet mAddresses;      // +0x0c
    IPAddr mAddress;                       // +0x18
    int mNumChatClients;                   // +0x40
    int mNumClients;                       // +0x44
    unsigned long mLastPing;               // +0x48
    bool mQueryingGroups;                  // +0x4c
    LobbyGroupMap mGroups;                 // +0x50
    SmartPtr<LobbyClientList> mClients;    // +0x5c
    SmartPtr<RoutingConnection> mConnection; // +0x60
    unsigned short mMyClientId;            // +0x64
public:
    unsigned long GetLastPing() const { return this->mLastPing; }
    unsigned short GetMyClientId() const { return this->mMyClientId; }
};

typedef std::map<IPAddr, SmartPtr<LobbyServer> > LobbyServerMap;
class LobbyServerList : public RefCount {
public:
    LobbyServerList();
    virtual ~LobbyServerList();
    void UpdateFromDirectory(const DirEntityList& inEntities);
    LobbyServer* FindServer(const IPAddr& inAddress);
    LobbyServerMap mServers;               // +0x08; allocation 0x14, node 0x3c
};
typedef std::map<unsigned short,SmartPtr<LobbyGame> > LobbyIdGameMap;
typedef std::map<IPAddr,SmartPtr<LobbyGame> > LobbyAddrGameMap;
// Vtable 0x852c44, RTTI locator 0x87aca0, type descriptor 0x8ded08; 0x2c bytes (allocated at 0x7e6e80).
class LobbyGameList : public RefCount {
public:
    LobbyGameList(int inType);
    virtual ~LobbyGameList();
    void Clear();
    void AddGame(LobbyGame*);
    LobbyGame* GetOrCreateGame(unsigned short,bool*);
    LobbyGame* GetOrCreateGame(const IPAddr&,bool*);
    LobbyGame* GetGame(unsigned short);
    LobbyGame* GetGame(const IPAddr&);
    SmartPtr<LobbyGame> RemoveGame(unsigned short);
    SmartPtr<LobbyGame> RemoveGame(const IPAddr&);
    void Rewind();
    bool HasMore();
    LobbyGame* Next();
    bool IsEmpty();
    bool RemoveStaleGames(long);
    long GetType() const { return this->mType; }
    long mType;                            // 0x08
    LobbyIdGameMap mIdGames;                // 0x0c
    LobbyAddrGameMap mAddrGames;            // 0x18
    LobbyIdGameMap::iterator mIdItr;        // 0x24
    LobbyAddrGameMap::iterator mAddrItr;    // 0x28
};


}
#endif
