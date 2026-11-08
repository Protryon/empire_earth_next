// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <time.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/EELobbyGameCode.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "ee/wonapi/WONLobbyRoutingLogic.h"
#include "ee/wonapi/WONLobbyChatCommands.h"
#include "ee/wonapi/WONStatus.h"
#include "ee/wonapi/WONRoutingTypes.h"
extern unsigned short gEmptyWideString[]; // 0x8e60bc, an empty wide string (zero .bss; defined in EEWonNet.cpp)
// Routes one server notification on a routing connection to a RoutingLogic handler for the op's server.
// The original's RTTI names this class in the global namespace, not in WONAPI.
class RoutingOpCompletion : public WONAPI::LogicCompletionBase {
public:
    typedef void (WONAPI::RoutingLogic::*Callback)(WONAPI::RoutingOp*, WONAPI::LobbyServer*);
    RoutingOpCompletion(Callback inCallback, WONAPI::RoutingLogic* inLogic)
        : mCallback(inCallback), mLogic(inLogic) {}
    // The implicit destructor (0x7ee8f0) releases mLogic (+0x10) and stores no vptr of its own.
    // Run (0x7ee8a0) does nothing once the logic or the op is killed, or when the op's server is unknown
    virtual void Run(WONAPI::AsyncOp* inOp)
    {
        if (this->mLogic->mKilled || inOp->mKilled) return;
        WONAPI::RoutingOp* anOp = static_cast<WONAPI::RoutingOp*>(inOp);
        WONAPI::LobbyServer* aServer = this->mLogic->GetServer(anOp);
        if (aServer != NULL) (this->mLogic.get()->*mCallback)(anOp, aServer);
    }
    Callback mCallback; //0c
    WONAPI::SmartPtr<WONAPI::RoutingLogic> mLogic; //10
};

namespace WONAPI {












































































// FUNCTION: WON 0x007e7200
// Looks up the add-friend, admin-action, bad-user, create-room, password and room dialogs from the lobby layout
void RoutingLogic::InitComponent(ComponentConfig* inConfig)
{
    SafeGetComponent(inConfig, this->mAddFriendDialog, "AddFriendDialog", __FILE__, 125);
    SafeGetComponent(inConfig, this->mAdminActionDialog, "AdminActionDialog", __FILE__, 126);
    SafeGetComponent(inConfig, this->mBadUserDialog, "BadUserDialog", __FILE__, 127);
    SafeGetComponent(inConfig, this->mCreateRoomDialog, "CreateRoomDialog", __FILE__, 128);
    SafeGetComponent(inConfig, this->mPasswordDialog, "PasswordDialog", __FILE__, 129);
    SafeGetComponent(inConfig, this->mRoomDialog, "RoomDialog", __FILE__, 130);
}


// FUNCTION: WON 0x007e6e80
// Creates the Internet lobby logic with an empty server list and game list, not idle and not yet a moderator
RoutingLogic::RoutingLogic()
{
    this->mServerList = new LobbyServerList;
    this->mGameList = new LobbyGameList(2);
    this->mGameDataType = "G";
    this->mSuppressIdle = false;
    this->mIdle = false;
    this->mHaveGameList = false;
    this->mWantToBeModerator = false;
    this->mLastServerRefresh = 0;
}

// FUNCTION: WON 0x007e7070
// Destroys the Internet lobby logic (members only)
RoutingLogic::~RoutingLogic() {}

// Clears the pending server on every exit of a connect attempt (destructor at 0x7eae90) (provisional name)
class PendingServerCleanup {
public:
    PendingServerCleanup(RoutingLogic* inLogic) : mLogic(inLogic) {}
    ~PendingServerCleanup() { this->mLogic->ClearPendingServer(); }
    RoutingLogic* mLogic;
};

// FUNCTION: WON 0x007eaea0
// Creates a game room on the current or default server for the hosted game and joins it as captain; false on failure
bool RoutingLogic::HostGame(LobbyGame* inGame)
{
    if (this->IsConnecting() || this->mGameGroup.get() != NULL) return false;
    LobbyServer* aServer = this->mCurrentServer.get();
    if (aServer == NULL) aServer = this->GetDefaultServer();
    if (aServer == NULL) return false;
    PendingServerCleanup aCleanup(this);
    if (!this->ConnectToServer(aServer, false)) return false;
    unsigned long aFlags = 0;
    if (inGame->GetAskToJoinFlag()) aFlags = 0x100;
    if (inGame->mInviteOnly) aFlags |= 0x80;
    unsigned short aGroupId;
    if (!this->CreateGroup(this->mPendingServer.get(), inGame->mName, inGame->mPassword,
                      aFlags, aGroupId)) return false;
    this->JoinedGroup(aGroupId, true);
    if (this->GetConnection() == NULL) return false;
    inGame->mGroupId = aGroupId;
    aServer->mClients->ClearInvited();
    if (this->mGameGroup.get() != NULL) inGame->SetClients(this->mGameGroup->mClients.get());
    return true;
}

// FUNCTION: WON 0x007eafd0
// Joins the room of an Internet game on the current server and attaches its player list; false on failure
bool RoutingLogic::JoinGame(LobbyGame* inGame)
{
    if (this->IsConnecting() || this->mGameGroup.get() != NULL || this->mCurrentServer.get() == NULL ||
        inGame->mGameType != 2) return false;
    PendingServerCleanup aCleanup(this);
    if (!this->ConnectToServer(this->mCurrentServer.get(), false)) return false;
    if (!this->JoinGroup(this->mPendingServer.get(), inGame->GetGroupId())) return false;
    this->JoinedGroup(inGame->mGroupId, true);
    if (this->mGameGroup.get() != NULL) inGame->SetClients(this->mGameGroup->mClients.get());
    return true;
}

// FUNCTION: WON 0x007eeaa0
// Disconnects the server being connected to (unless it is the current one) and forgets it
void RoutingLogic::ClearPendingServer()
{
    if (this->mPendingServer.get() != NULL && this->mPendingServer.get() != this->mCurrentServer.get())
        this->mPendingServer->Disconnect();
    this->mPendingServer = NULL;
}

// FUNCTION: WON 0x007e72b0
// Returns the room flags for a group id: 5 for the joined chat room, 6 for the joined game room, else 0
int RoutingLogic::GetRoomSpec(unsigned short inGroupId)
{
    if (this->mChatGroup.get() != NULL && this->mChatGroup->mId == inGroupId) return 5;
    if (this->mGameGroup.get() != NULL && this->mGameGroup->mId == inGroupId) return 6;
    return 0;
}

// FUNCTION: WON 0x007e72f0
// Returns the room flags for a group on the current server, or 0 for other servers
int RoutingLogic::GetRoomSpec(LobbyGroup* inGroup, LobbyServer* inServer)
{
    if (inServer == this->mCurrentServer.get() && inGroup != NULL)
        return this->GetRoomSpec(inGroup->GetId());
    return 0;
}

// FUNCTION: WON 0x007e9aa0
// Returns the player list of the current server, or NULL when not connected
LobbyClientList* RoutingLogic::GetClientList()
{
    if (this->mCurrentServer.get() != NULL) return this->mCurrentServer->mClients.get();
    return NULL;
}

// FUNCTION: WON 0x007ea290
// Returns true while connected to a lobby server
bool RoutingLogic::IsConnected() { return this->mCurrentServer.get() != NULL; }

// FUNCTION: WON 0x007ea2a0
// Returns the routing connection of the current server, or NULL when not connected
RoutingConnection* RoutingLogic::GetConnection()
{
    if (this->mCurrentServer.get() == NULL) return NULL;
    return this->mCurrentServer->mConnection.get();
}

// FUNCTION: WON 0x007ea2b0
// Returns the joined chat room or game room selected by the room flags, or NULL
LobbyGroup* RoutingLogic::GetGroup(int inRoomSpec)
{
    if (inRoomSpec & 1) return this->mChatGroup.get();
    if (inRoomSpec & 2) return this->mGameGroup.get();
    return NULL;
}

// FUNCTION: WON 0x007ea2d0
// Returns the lobby server with the given address from the server list
LobbyServer* RoutingLogic::GetServer(const IPAddr& inAddress)
{
    return this->mServerList->FindServer(inAddress);
}

// FUNCTION: WON 0x007ea2e0
// Returns the current or pending server whose connection an operation ran on, or NULL
LobbyServer* RoutingLogic::GetServer(RoutingOp* inOp)
{
    if (this->mCurrentServer.get() != NULL &&
        inOp->mConnection.get() == this->mCurrentServer->mConnection.get())
        return this->mCurrentServer.get();
    if (this->mPendingServer.get() != NULL &&
        inOp->mConnection.get() == this->mPendingServer->mConnection.get())
        return this->mPendingServer.get();
    return NULL;
}

// FUNCTION: WON 0x007ed1c0
// Picks the server to join: the last used one, else the busiest one for the lobby language, else the busiest overall
LobbyServer* RoutingLogic::GetDefaultServer()
{
    LobbyConfig* aConfig = GetLobbyConfig();
    if (aConfig == NULL) return NULL;
    LobbyServer* aBest = NULL;
    LobbyServer* aPreferred = NULL;
    const LobbyServerMap& aServers = this->mServerList->mServers;
    LobbyServerMap::const_iterator anItr = aServers.begin();
    while(anItr != aServers.end()) {
        LobbyServer* aServer = anItr->second.get();
        if (aServer->mName == LobbyGetLastServerName()) return aServer;
        if (aBest == NULL || aServer->GetNumClients() > aBest->GetNumClients()) aBest = aServer;
        if (aServer->HasAddress(aConfig->mLanguage)) {
            if (aPreferred == NULL || aServer->GetNumClients() > aPreferred->GetNumClients())
                aPreferred = aServer;
        }
        ++anItr;
    }
    return aPreferred != NULL ? aPreferred : aBest;
}

// FUNCTION: WON 0x007ed660
// Returns true while a server connection is in progress
bool RoutingLogic::IsConnecting() { return this->mPendingServer.get() != NULL; }

// FUNCTION: WON 0x007eeae0
// Returns the local player's client on the current server, or NULL
LobbyClient* RoutingLogic::GetMyClient()
{
    return this->mCurrentServer.get() == NULL ? NULL : this->mCurrentServer->GetMyClient();
}

// FUNCTION: WON 0x007eeaf0
// Sends staging data to the captain of the joined game room
void RoutingLogic::SendToCaptain(const ByteBuffer* inData)
{
    RoutingConnection* aConnection = this->GetConnection();
    if (aConnection != NULL && this->mGameGroup.get() != NULL)
        aConnection->SendData(inData, 0, this->mGameGroup->mCaptainId, NULL);
}

// FUNCTION: WON 0x007eeb20
// Sends staging data to everyone in the joined game room
void RoutingLogic::BroadcastToGame(const ByteBuffer* inData)
{
    RoutingConnection* aConnection = this->GetConnection();
    if (aConnection != NULL && this->mGameGroup.get() != NULL)
        aConnection->SendData(inData, 0, this->mGameGroup->mId, NULL);
}

// FUNCTION: WON 0x007eeb50
// Sends staging data to one player while in a game room
void RoutingLogic::SendToClient(LobbyClient* inClient, const ByteBuffer* inData)
{
    RoutingConnection* aConnection = this->GetConnection();
    if (aConnection != NULL && this->mGameGroup.get() != NULL)
        aConnection->SendData(inData, 0, inClient->mClientId, NULL);
}

// FUNCTION: WON 0x007eeb80
// Publishes the game's summary as a data object on the game room so it appears in other players' game lists
void RoutingLogic::AdvertiseGame(LobbyGame* inGame)
{
    RoutingConnection* aConnection = this->GetConnection();
    if (aConnection == NULL || this->mGameGroup.get() == NULL) return;
    WriteBuffer aBuffer(0, true);
    inGame->WriteSummary(aBuffer);
    aConnection->CreateDataObject(true, this->mGameGroup->mId, this->mGameDataType,
                                  gEmptyWideString, aBuffer.ToByteBuffer(true).get(), NULL);
}

// FUNCTION: WON 0x007eecc0
// Replaces the game room's summary data object with an empty one
void RoutingLogic::RemoveGameAdvertisement(LobbyGame*)
{
    RoutingConnection* aConnection = this->GetConnection();
    if (aConnection == NULL || this->mGameGroup.get() == NULL) return;
    aConnection->ReplaceDataObject(this->mGameGroup->GetId(), this->mGameDataType, gEmptyWideString, NULL);
}

// FUNCTION: WON 0x007eb0c0
// Removes a player from the game room: a permanent ban, or a one-second ban that acts as a kick
void RoutingLogic::KickClient(LobbyClient* inClient, bool inBan)
{
    if (this->mGameGroup.get() == NULL) return;
    RoutingConnection* aConnection = this->GetConnection();
    if (aConnection == NULL) return;
    aConnection->BanClient(this->mGameGroup->GetId(), true, !inBan,
                               gEmptyWideString, inClient->GetClientId(), NULL);
}

// FUNCTION: WON 0x007eb1b0
// Sets or clears the game room's closed flag when the game launches or returns to staging
void RoutingLogic::SetGameInProgress(bool inClose)
{
    RoutingConnection* aConnection = this->GetConnection();
    if (aConnection == NULL || this->mGameGroup.get() == NULL) return;
    aConnection->SetGroupFlags(this->mGameGroup->mId, 2, inClose ? 2 : 0, 0, 0, NULL);
}

// FUNCTION: WON 0x007e7b40
// Applies a player's new room member flags and notifies the joined room's player list
void RoutingLogic::MemberFlagsChanged(RoutingOp* inBaseOp, LobbyServer* inServer)
{
    RoutingMemberFlagsChangedOp* inOp = static_cast<RoutingMemberFlagsChangedOp*>(inBaseOp);
    int aGroupId = inOp->GetGroupId();
    LobbyGroup* aGroup = inServer->GetGroup(aGroupId);
    if (aGroup == NULL) return;
    LobbyClient* aClient = aGroup->FindClient(inOp->mClientId);
    if (aClient == NULL) return;
    aClient->SetMemberFlags(inOp->GetMemberFlags(), aGroup->IsGameGroup());
    if (inServer != this->mCurrentServer.get()) return;
    unsigned int aRoomSpec = this->GetRoomSpec(aGroup, inServer);
    if (aRoomSpec != 0) LobbyBroadcastEvent(new ClientChangedEvent(aClient, aRoomSpec, 2), true);
}

// FUNCTION: WON 0x007e7a30
// Applies a player's new client flags, notifies the lobby and updates a friend's anonymous state
void RoutingLogic::ClientFlagsChanged(RoutingOp* inBaseOp, LobbyServer* inServer)
{
    RoutingClientFlagsChangedOp* inOp = static_cast<RoutingClientFlagsChangedOp*>(inBaseOp);
    LobbyClient* aClient = inServer->GetClient(inOp->GetClientId());
    if (aClient == NULL) return;
    aClient->mClientFlags = inOp->mClientFlags;
    unsigned int aRoomSpec = 4;
    LobbyBroadcastEvent(new ClientChangedEvent(aClient, aRoomSpec, 2), true);
    if (!aClient->IsFriend()) return;
    LobbyFriend* aFriend = LobbyFindFriend(aClient->GetWONUserId());
    if (aFriend == NULL || aFriend->GetAnonymous() == aClient->IsAnonymous()) return;
    aFriend->SetAnonymous(aClient->IsAnonymous());
    LobbyBroadcastEvent(new FriendChangedEvent(aFriend, 2), true);
}

// FUNCTION: WON 0x007ebc90
// Says whether a friend needs a server search: not already searching, not here, other servers, 30 s since the last
bool RoutingLogic::ShouldLocateFriend(LobbyFriend* inFriend, unsigned long inNow)
{
    if (inFriend->mFlags & 4) return false;
    const LobbyServerMap& aServers = this->mServerList->mServers;
    if (this->mCurrentServer.get() != NULL && inFriend->mAddress == this->mCurrentServer->mAddress)
        return false;
    if (aServers.empty()) return false;
    if (aServers.size() == 1 && this->mCurrentServer.get() != NULL) return false;
    return static_cast<long>(inNow - inFriend->mLastLocate) >= 30 ? true : false;
}

// FUNCTION: WON 0x007ebd10
// Announces in chat which chat and game rooms a player is in; false when neither is known and not forced
bool RoutingLogic::AnnounceClientLocation(LobbyClient* inClient, bool inForce,
                                        int inRoomSpec, bool inFindByUserId)
{
    if (this->mCurrentServer.get() == NULL || inClient == NULL) return false;
    GUIString aChatRoom;
    GUIString aGameRoom;
    if (inFindByUserId && inClient->mWONUserId != 0) {
        LobbyClient* aClient = this->mCurrentServer->FindClientByWONId(inClient->mWONUserId);
        if (aClient != NULL) inClient = aClient;
        else goto announce;
    }
    if (inClient->IsInChatGroup()) {
        int aKey = inClient->GetChatGroupId();
        LobbyGroup* aGroup = this->mCurrentServer->GetGroup(aKey);
        if (aGroup != NULL) aChatRoom = aGroup->mName;
    }
    if (inClient->IsInGameGroup()) {
        int aKey = inClient->GetGameGroupId();
        LobbyGroup* aGroup = this->mCurrentServer->GetGroup(aKey);
        if (aGroup != NULL) aGameRoom = aGroup->mName;
    }
announce:
    if (aChatRoom.length() == 0 && aGameRoom.length() == 0 && !inForce) return false;
    int aRoomSpec = inRoomSpec | 4;
    LobbyBroadcastEvent(new AnnounceClientLocationEvent(inClient, aChatRoom,
                                                       aGameRoom, aRoomSpec), true);
    return true;
}

// FUNCTION: WON 0x007e7940
// Passes staging data from another player in the joined game room to the staging handler
void RoutingLogic::PeerData(RoutingOp* inBaseOp, LobbyServer* inServer)
{
    RoutingPeerDataOp* inOp = static_cast<RoutingPeerDataOp*>(inBaseOp);
    if (inServer != this->mCurrentServer.get() || this->mGameGroup.get() == NULL) return;
    LobbyClient* aClient = this->mGameGroup->FindClient(inOp->GetClientId());
    if (aClient == NULL || aClient->GetClientId() == this->mCurrentServer->GetMyClientId()) return;
    LobbyHandleStagingMessage(inOp->GetData().get(), aClient);
}

// FUNCTION: WON 0x007e8030
// Adds a player to a room, notifies the joined room's list and the room list, and updates the friend entry
void RoutingLogic::ClientJoinedGroup(RoutingOp* inBaseOp, LobbyServer* inServer)
{
    RoutingClientJoinedGroupOp* inOp = static_cast<RoutingClientJoinedGroupOp*>(inBaseOp);
    int aKey = inOp->GetGroupId();
    LobbyGroup* aGroup = inServer->GetGroup(aKey);
    if (aGroup == NULL) return;
    LobbyClient* aClient = inServer->GetClient(inOp->GetClientId());
    if (aClient == NULL) return;
    inServer->AddClientToGroup(aClient, inOp->GetMemberFlags(), aGroup);
    unsigned int aRoomSpec = this->GetRoomSpec(aGroup, inServer);
    if (aRoomSpec != 0)
        LobbyBroadcastEvent(new ClientChangedEvent(aClient, aRoomSpec, 0), true);
    LobbyBroadcastEvent(new GroupChangedEvent(aGroup, inServer, 2), true);
    this->UpdateFriend(aClient);
}

// FUNCTION: WON 0x007e8160
// Removes a player from a room, notifies the lists (and the staging screen for the game room), updates the friend
void RoutingLogic::ClientLeftGroup(RoutingOp* inBaseOp, LobbyServer* inServer)
{
    RoutingClientLeftGroupOp* inOp = static_cast<RoutingClientLeftGroupOp*>(inBaseOp);
    LobbyGroup* aGroup = inServer->GetGroup(inOp->GetGroupId());
    if (aGroup == NULL) return;
    SmartPtr<LobbyClient> aClient = inServer->RemoveClientFromGroup(inOp->GetClientId(), aGroup);
    if (aClient.get() == NULL) return;
    unsigned int aRoomSpec = this->GetRoomSpec(aGroup, inServer);
    if (aRoomSpec != 0) {
        LobbyBroadcastEvent(new ClientChangedEvent(aClient.get(), aRoomSpec, 1), true);
        if (aRoomSpec & 2) LobbyClientLeftGame(aClient.get());
    }
    LobbyBroadcastEvent(new GroupChangedEvent(aGroup, inServer, 2), true);
    this->UpdateFriend(aClient.get());
}
// FUNCTION: WON 0x007e8f80
// Adds a newly created room to the server's room list and notifies the lobby
void RoutingLogic::GroupCreated(RoutingOp* inBaseOp, LobbyServer* inServer)
{
    RoutingGroupCreatedOp* inOp = static_cast<RoutingGroupCreatedOp*>(inBaseOp);
    LobbyGroup* aGroup = inServer->AddGroup(inOp->GetGroupInfo().get());
    if (aGroup != NULL)
        LobbyBroadcastEvent(new GroupChangedEvent(aGroup, inServer, 0), true);
}

// FUNCTION: WON 0x007e9040
// Removes a deleted room from the server's room list and notifies the lobby
void RoutingLogic::GroupDeleted(RoutingOp* inBaseOp, LobbyServer* inServer)
{
    RoutingGroupDeletedOp* inOp = static_cast<RoutingGroupDeletedOp*>(inBaseOp);
    SmartPtr<LobbyGroup> aGroup = inServer->RemoveGroup(inOp->GetGroupId());
    if (aGroup.get() != NULL)
        LobbyBroadcastEvent(new GroupChangedEvent(aGroup.get(), inServer, 1), true);
}

// FUNCTION: WON 0x007ef0d0
// Refreshes a friend's location and status from the player's client and notifies the friends list
void RoutingLogic::UpdateFriend(LobbyClient* inClient)
{
    if (!inClient->IsFriend()) return;
    LobbyFriend* aFriend = LobbyFindFriend(inClient->mWONUserId);
    if (aFriend == NULL) return;
    aFriend->SetStatusFromClient(inClient);
    LobbyBroadcastEvent(new FriendChangedEvent(aFriend, 2), true);
}

// FUNCTION: WON 0x007ebf20
// Announces where a friend is: by room when on the current server, else by the friend's last known location
void RoutingLogic::AnnounceFriendLocation(LobbyFriend* inFriend, int inRoomSpec)
{
    if (inFriend == NULL) return;
    if (this->mCurrentServer.get() != NULL && inFriend->mAddress == this->mCurrentServer->mAddress) {
        LobbyClient* aClient = this->mCurrentServer->FindClientByWONId(inFriend->mUserId);
        if (aClient != NULL && this->AnnounceClientLocation(aClient, false, inRoomSpec, false)) return;
    }
    LobbyBroadcastEvent(new AnnounceFriendEvent(inFriend, inRoomSpec, 2), true);
}

// FUNCTION: WON 0x007e7d60
// Adds a player who joined the server; a friend gets a location update, plus a sound and chat notice when visible
void RoutingLogic::ClientJoinedServer(RoutingOp* inBaseOp, LobbyServer* inServer)
{
    RoutingClientJoinedServerOp* inOp = static_cast<RoutingClientJoinedServerOp*>(inBaseOp);
    LobbyClient* aClient = inServer->AddClient(inOp->mClientInfo.get());
    LobbyFriend* aFriend = LobbyFindFriend(aClient->GetWONUserId());
    if (aFriend == NULL) return;
    aClient->mIsFriend = true;
    aFriend->SetLocation(inServer->mAddress, inServer->mName, 0);
    aFriend->SetAnonymous(aClient->IsAnonymous());
    LobbyBroadcastEvent(new FriendChangedEvent(aFriend, 2), true);
    if (inServer != this->mCurrentServer.get() || aClient->IsAnonymous()) return;
    if (LobbyGetChatSoundEffects()) LobbyPlaySound(::LobbyGlobal_FriendJoined_Sound.get());
    LobbyBroadcastEvent(new AnnounceFriendEvent(aFriend, 4, 0), true);
}

// FUNCTION: WON 0x007e7e80
// Removes a player who left the server; a friend loses its location, with a sound and chat notice when visible
void RoutingLogic::ClientLeftServer(RoutingOp* inBaseOp, LobbyServer* inServer)
{
    RoutingClientLeftServerOp* inOp = static_cast<RoutingClientLeftServerOp*>(inBaseOp);
    SmartPtr<LobbyClient> aClient = inServer->RemoveClient(inOp->GetClientId());
    if (aClient.get() != NULL) {
    if (!aClient->IsFriend()) return;
    LobbyFriend* aFriend = LobbyFindFriend(aClient->GetWONUserId());
    if (aFriend == NULL) return;
    aFriend->ClearLocation();
    LobbyBroadcastEvent(new FriendChangedEvent(aFriend, 2), true);
    if (inServer == this->mCurrentServer.get() && !aClient->IsAnonymous()) {
    if (LobbyGetChatSoundEffects()) LobbyPlaySound(::LobbyGlobal_FriendLeft_Sound.get());
    LobbyBroadcastEvent(new AnnounceFriendEvent(aFriend, 4, 1), true);
    }
    return;
    }
}

// FUNCTION: WON 0x007e9100
// Marks a game as (un)invited from a player who is not ignored or blocked, plays the sound and shows the invitation
void RoutingLogic::GroupInvitation(RoutingOp* inBaseOp, LobbyServer* inServer)
{
    RoutingGroupInvitationOp* inOp = static_cast<RoutingGroupInvitationOp*>(inBaseOp);
    if (inServer != this->mCurrentServer.get()) return;
    LobbyClient* aClient = inServer->GetClient(inOp->GetClientId());
    if (aClient == NULL || aClient->IsIgnored() || aClient->mBlocked) return;
    LobbyGame* aGame = this->mGameList->GetGame(inOp->GetGroupId());
    if (aGame == NULL) return;
    aGame->mInvited = inOp->GetInvited();
    LobbyBroadcastEvent(new GameChangedEvent(aGame, 2), true);
    if (LobbyGetStagingSoundEffects()) LobbyPlaySound(::LobbyGlobal_InviteRecv_Sound.get());
    LobbyBroadcastEvent(new GroupInvitationEvent(4, aGame->mName, aGame->GetGroupId(),
        aClient, inOp->GetInvited(), GUIString(new GUIStringData(inOp->mComment)), false), true);
}

// FUNCTION: WON 0x007e92d0
// Shows the captain a player's request to join the hosted game, with the sound, unless the player is ignored/blocked
void RoutingLogic::GroupJoinAttempt(RoutingOp* inBaseOp, LobbyServer* inServer)
{
    RoutingGroupJoinAttemptOp* inOp = static_cast<RoutingGroupJoinAttemptOp*>(inBaseOp);
    if (inServer != this->mCurrentServer.get() || this->mGameGroup.get() == NULL ||
        inOp->mGroupId != this->mGameGroup->mId) return;
    LobbyClient* aClient = inServer->GetClient(inOp->GetClientId());
    if (aClient == NULL || aClient->mIgnored || aClient->mBlocked) return;
    if (LobbyGetStagingSoundEffects()) LobbyPlaySound(::LobbyGlobal_AskToJoinRecv_Sound.get());
    LobbyBroadcastEvent(new GroupJoinAttemptEvent(6, aClient,
        GUIString(new GUIStringData(inOp->mComment))), true);
}

// FUNCTION: WON 0x007e7640
// Sets the away flag after five minutes without input and clears it on new input, unless away was set by hand
void RoutingLogic::Reset()
{
    if (this->mSuppressIdle || !this->IsConnected()) return;
    DWORD aLastInputTick = GetLobbyWindow()->mLastInputTick;
    if (GetTickCount() - aLastInputTick > 300000) {
        if (this->mIdle) return;
        RoutingConnection* aConnection = this->GetConnection();
        if (aConnection != NULL) {
            aConnection->SetClientFlags(RoutingClientFlag_IsAway, RoutingClientFlag_IsAway, 0, 0, NULL);
            this->mIdle = true;
        }
    } else if (this->mIdle) {
        RoutingConnection* aConnection = this->GetConnection();
        if (aConnection != NULL) {
            aConnection->SetClientFlags(RoutingClientFlag_IsAway, 0, 0, 0, NULL);
            this->mIdle = false;
        }
    }
}

// FUNCTION: WON 0x007e76c0
// Shows a received chat line (room, team or whisper) from a player who is not ignored or blocked, with sounds
void RoutingLogic::PeerChat(RoutingOp* inBaseOp, LobbyServer* inServer)
{
    RoutingPeerChatOp* inOp = static_cast<RoutingPeerChatOp*>(inBaseOp);
    if (inServer != this->mCurrentServer.get() ||
        (this->mChatGroup.get() == NULL && this->mGameGroup.get() == NULL) ||
        inOp->GetRecipients().empty()) return;
    int aRecipientId = inOp->GetRecipients().front();
    LobbyClient* aSender = inServer->GetClient(inOp->GetClientId());
    if (aSender == NULL || aSender->IsIgnored() || aSender->IsBlocked()) return;
    LobbyClient* aTarget = NULL;
    unsigned int aRoomSpec = 4;
    if (inOp->GetFlags() & 0x100) {
        if (this->mGameGroup.get() == NULL) return;
        aRoomSpec = 0x16;
    } else if (aRecipientId < 0x4002) {
        aRoomSpec = this->GetRoomSpec(aRecipientId);
        if (aRoomSpec == 0) return;
    } else {
        aTarget = inServer->GetClient(aRecipientId);
        if (aTarget == NULL) return;
    }
    if (inOp->IsWhisper() && aTarget != NULL && aTarget->GetClientId() == inServer->GetMyClientId()) {
        if (LobbyGetChatSoundEffects()) LobbyPlaySound(::LobbyGlobal_WhisperRecv_Sound.get());
        ChatCommandParser* aParser = LobbyGetChatCommandParser();
        if (aParser != NULL) aParser->mInput = aSender->GetName();
    }
    if ((inOp->GetFlags() & 0x10) && LobbyGetLobbySoundEffects())
        LobbyPlaySound(::LobbyGlobal_Error_Sound.get());
    LobbyBroadcastEvent(new RecvChatEvent(aRoomSpec, inOp->GetFlags(), aSender,
        GUIString(inOp->GetText()), aTarget), true);
}

// FUNCTION: WON 0x007e8770
// Shows a server alert message from the administrators, with the error sound
void RoutingLogic::ServerAlert(RoutingOp* inBaseOp, LobbyServer*)
{
    RoutingServerAlertOp* inOp = static_cast<RoutingServerAlertOp*>(inBaseOp);
    if (LobbyGetLobbySoundEffects()) LobbyPlaySound(::LobbyGlobal_Error_Sound.get());
    LobbyBroadcastEvent(new AlertEvent(4, 0, 0,
        GUIString(new GUIStringData(inOp->mText))), true);
}

// FUNCTION: WON 0x007e8890
// Shows the server's shutdown warning with its countdown, with the error sound
void RoutingLogic::ServerShutdownStarted(RoutingOp* inBaseOp, LobbyServer*)
{
    RoutingServerShutdownStartedOp* inOp = static_cast<RoutingServerShutdownStartedOp*>(inBaseOp);
    if (LobbyGetLobbySoundEffects()) LobbyPlaySound(::LobbyGlobal_Error_Sound.get());
    LobbyBroadcastEvent(new AlertEvent(4, 1, inOp->GetSeconds(),
        GUIString(new GUIStringData(inOp->mText))), true);
}

// FUNCTION: WON 0x007e89b0
// Shows that the server shutdown was called off, with the error sound
void RoutingLogic::ServerShutdownAborted(RoutingOp* inBaseOp, LobbyServer*)
{
    RoutingServerShutdownAbortedOp* inOp = static_cast<RoutingServerShutdownAbortedOp*>(inBaseOp);
    if (LobbyGetLobbySoundEffects()) LobbyPlaySound(::LobbyGlobal_Error_Sound.get());
    LobbyBroadcastEvent(new AlertEvent(4, 2, 0,
        GUIString(new GUIStringData(inOp->mText))), true);
}

inline int ModeratedRoomSpec(int inRoomSpec) { return inRoomSpec | 4; }

// FUNCTION: WON 0x007e8580
// Tells the local player they were muted or unmuted on the server, in the game room or in the chat room
void RoutingLogic::YouWereMuted(RoutingOp* inBaseOp, LobbyServer* inServer)
{
    RoutingYouWereMutedOp* inOp = static_cast<RoutingYouWereMutedOp*>(inBaseOp);
    int aKind = inOp->GetIsMuted() ? 2 : 3;
    unsigned short aGroupId = inOp->GetGroupId();
    int aRoomSpec;
    GUIString aRoomName;
    if (aGroupId == 0xffff) {
        aRoomSpec = 0;
        aRoomName = inServer->mName;
    } else if (this->mGameGroup.get() != NULL && aGroupId == this->mGameGroup->GetId()) {
        aRoomSpec = 2;
        aRoomName = this->mGameGroup->mName;
    } else if (this->mChatGroup.get() != NULL && aGroupId == this->mChatGroup->GetId()) {
        aRoomSpec = 1;
        aRoomName = this->mChatGroup->mName;
    } else return;
    LobbyClient* aClient = inServer->GetMyClient();
    LobbyBroadcastEvent(new ClientModeratedEvent(aClient, ModeratedRoomSpec(aRoomSpec),
        aRoomName, aKind, inOp->GetDuration(), true,
        GUIString(inOp->GetComment())), true);
}

// FUNCTION: WON 0x007eb1f0
// Leaves the game room (and the server when not in a chat room) and tells the staging screen
void RoutingLogic::LeaveGame(LobbyGame*)
{
    if (this->mGameGroup.get() == NULL) return;
    if (this->mChatGroup.get() == NULL) {
        this->mGameGroup = NULL;
        this->LeaveServer();
    } else {
        RoutingConnection* aConnection = this->GetConnection();
        if (aConnection != NULL) aConnection->LeaveGroup(this->mGameGroup->GetId(), NULL);
        this->mGameGroup = NULL;
    }
    LobbyBroadcastEvent(new LeftRoomEvent(6, false, true), true);
}

// FUNCTION: WON 0x007eeda0
// Leaves the chat room while a game runs and updates the client flags on the server
void RoutingLogic::Suspend()
{
    RoutingConnection* aConnection = this->GetConnection();
    if (aConnection == NULL) return;
    if (this->mChatGroup.get() != NULL) {
        aConnection->LeaveGroup(this->mChatGroup->GetId(), NULL);
        this->mChatGroup = NULL;
        LobbyBroadcastEvent(new LeftRoomEvent(5, false, true), true);
    }
    aConnection->SetClientFlags(0, 0, ~0UL, 0xfffbffff, NULL);
}

// FUNCTION: WON 0x007eee90
// Returns from a game: marks the player anonymous, leaves the game and server, and rejoins the default room
void RoutingLogic::Resume()
{
    RoutingConnection* aConnection = this->GetConnection();
    if (aConnection != NULL) aConnection->SetClientFlags(0x40, 0x40, 0, 0, NULL);
    LobbyLeaveGame(false);
    this->LeaveServer();
    this->JoinDefaultRoom(true);
}

// FUNCTION: WON 0x007ee950
// Disconnects from the current server, stops pings, clears the rooms and game list, and drops a joined game
void RoutingLogic::LeaveServer()
{
    if (this->mCurrentServer.get() != NULL) {
        this->mCurrentServer->Disconnect();
        LobbyKillPings(NULL);
        LobbyBroadcastEvent(new LeftRoomEvent(4, false, false), true);
    }
    if (this->mGameListWaitOp.get() != NULL) this->mGameListWaitOp->Kill(true);
    this->mHaveGameList = false;
    this->mCurrentServer = NULL;
    this->mChatGroup = NULL;
    this->mGameList->Clear();
    LobbyBroadcastEvent(new SyncGameListEvent(this->mGameList.get()), true);
    if (this->mGameGroup.get() != NULL) {
        this->mGameGroup = NULL;
        LobbyGameDisconnected(true);
    }
}

// FUNCTION: WON 0x007e7c20
// Records a room's new captain, refreshes both captains in the list and drops the game when its captain left
void RoutingLogic::GroupCaptainChanged(RoutingOp* inBaseOp, LobbyServer* inServer)
{
    RoutingGroupCaptainChangedOp* inOp = static_cast<RoutingGroupCaptainChangedOp*>(inBaseOp);
    int aKey = inOp->GetGroupId();
    LobbyGroup* aGroup = inServer->GetGroup(aKey);
    if (aGroup == NULL) return;
    LobbyClient* anOldCaptain = NULL;
    LobbyClient* aNewCaptain = aGroup->SetCaptain(inOp->GetCaptainId(), &anOldCaptain);
    if (inServer != this->mCurrentServer.get()) return;
    unsigned int aRoomSpec = this->GetRoomSpec(aGroup, inServer);
    if (aRoomSpec == 0) return;
    if (anOldCaptain != NULL)
        LobbyBroadcastEvent(new ClientChangedEvent(anOldCaptain, aRoomSpec, 2), true);
    if (aNewCaptain != NULL)
        LobbyBroadcastEvent(new ClientChangedEvent(aNewCaptain, aRoomSpec, 2), true);
    if (aGroup == this->mGameGroup.get() && anOldCaptain != NULL) LobbyGameDisconnected(true);
}

// FUNCTION: WON 0x007e8d10
// Removes a game from the game list when its room's summary is deleted on the current server
void RoutingLogic::DataObjectDeleted(RoutingOp* inBaseOp, LobbyServer* inServer)
{
    RoutingDataObjectDeletedOp* inOp = static_cast<RoutingDataObjectDeletedOp*>(inBaseOp);
    if (inServer != this->mCurrentServer.get() || inOp->mDataType != this->mGameDataType) return;
    SmartPtr<LobbyGame> aGame = this->mGameList->RemoveGame(inOp->GetLinkId());
    if (aGame.get() != NULL) {
        LobbyBroadcastEvent(new GameChangedEvent(aGame.get(), 1), true);
        return;
    }
}

// FUNCTION: WON 0x007e8ad0
// Adds or updates a game in the game list from its room's summary, pinging new games; drops unreadable ones
void RoutingLogic::DataObjectCreated(RoutingOp* inBaseOp, LobbyServer* inServer)
{
    RoutingDataObjectCreatedOp* inOp = static_cast<RoutingDataObjectCreatedOp*>(inBaseOp);
    if (inServer != this->mCurrentServer.get() || inOp->mDataType != this->mGameDataType) return;
    int aLinkId = inOp->GetLinkId();
    LobbyGroup* aGroup = inServer->GetGroup(aLinkId);
    if (aGroup == NULL) return;
    bool wasExisting = false;
    LobbyGame* aGame = this->mGameList->GetOrCreateGame(aGroup->GetId(), &wasExisting);
    if (!aGame->ReadSummaryBuffer(inOp->GetData().get())) {
        SmartPtr<LobbyGame> aHoldGame(aGame);
        this->mGameList->RemoveGame(aGroup->GetId());
        if (wasExisting) LobbyBroadcastEvent(new GameChangedEvent(aGame, 1), true);
        return;
    }
    if (!wasExisting) {
        aGame->ReadGroup(aGroup);
        if (this->mGameGroup.get() != NULL) {
            LobbyGame* aMyGame = GetMyLobbyGame();
            if (aMyGame != NULL && aMyGame->mIsHost && aGame->GetGroupId() == aMyGame->mGroupId)
                aGame->mInvited = true;
        }
        if (LobbyGetAutoPingGames()) LobbyPingGame(aGame, false);
    }
    LobbyBroadcastEvent(new GameChangedEvent(aGame, wasExisting ? 2 : 0), true);
}

// The original tests the game bit with shr + test cl,1 (0x7e836d): a bool helper over an unsigned room spec
inline bool IsGameRoomSpec(unsigned long inRoomSpec) { return (inRoomSpec >> 1) & 1; }
inline bool IsChatRoomSpec(unsigned long inRoomSpec) { return inRoomSpec & 1; }

// FUNCTION: WON 0x007e82b0
// Handles the local player being kicked or banned from the game room, chat room or server and leaves it
void RoutingLogic::YouWereBanned(RoutingOp* inBaseOp, LobbyServer* inServer)
{
    RoutingYouWereBannedOp* inOp = static_cast<RoutingYouWereBannedOp*>(inBaseOp);
    bool aBanned = inOp->mIsBanned;
    unsigned short aGroupId = inOp->GetGroupId();
    if (!aBanned) return;
    int aKind;
    unsigned long aDuration = inOp->GetDuration();
    aKind = aDuration != 0 && aDuration < 10 ? 1 : 0;
    GUIString aRoomName;
    int aRoomSpec;
    if (aGroupId == 0xffff) {
        aRoomSpec = 0;
        aRoomName = inServer->mName;
    } else if (this->mGameGroup.get() != NULL && aGroupId == this->mGameGroup->GetId()) {
        aRoomSpec = 2;
        aRoomName = this->mGameGroup->mName;
    } else if (this->mChatGroup.get() != NULL && aGroupId == this->mChatGroup->GetId()) {
        aRoomSpec = 1;
        aRoomName = this->mChatGroup->mName;
    } else return;
    LobbyClient* aMyClient = inServer->GetMyClient();
    if (IsGameRoomSpec(aRoomSpec)) {
        LobbyClientKicked(aMyClient, !aKind);
        return;
    }
    LobbyBroadcastEvent(new ClientModeratedEvent(aMyClient, aRoomSpec | 4, aRoomName,
        aKind, inOp->GetDuration(), true, GUIString(inOp->mComment)), true);
    if (IsChatRoomSpec(aRoomSpec)) {
        if (this->mGameGroup.get() == NULL) this->LeaveServer();
        else {
            this->mChatGroup = NULL;
            LobbyBroadcastEvent(new LeftRoomEvent(5, false, false), true);
        }
    } else {
        bool hadGameGroup = this->mGameGroup.get() != NULL;
        if (hadGameGroup) LobbyLeaveGame(false);
        this->LeaveServer();
        if (hadGameGroup) LobbyShowLobbyTab();
    }
}

// FUNCTION: WON 0x007ed2f0
// Connects to the default server and joins the last used public chat room, else the busiest one under 50 players
bool RoutingLogic::JoinDefaultRoom(bool inShowStatus)
{
    LobbyServer* aServer = this->GetDefaultServer();
    if (aServer == NULL) return false;
    PendingServerCleanup aCleanup(this);
    if (!this->ConnectToServer(aServer, inShowStatus)) return false;
    LobbyGroup* aBestGroup = NULL;
    LobbyGroupMap::iterator anItr = aServer->mGroups.begin();
    while (anItr != aServer->mGroups.end()) {
        LobbyGroup* aGroup = anItr->second.get();
        if (aGroup->IsPublicChatGroup()) {
            if (aGroup->mName == LobbyGetLastRoomName()) {
                aBestGroup = aGroup;
                break;
            }
            if (aBestGroup == NULL ||
                (static_cast<int>(aGroup->mNumClients) > static_cast<int>(aBestGroup->mNumClients) &&
                 static_cast<int>(aGroup->mNumClients) < 50)) aBestGroup = aGroup;
        }
        anItr++;
    }
    int aGroupId = 0;
    if (aBestGroup != NULL) aGroupId = aBestGroup->GetId();
    if (!this->JoinGroup(this->mPendingServer.get(), aGroupId)) return false;
    this->JoinedGroup(aGroupId, false);
    return true;
}

// FUNCTION: WON 0x007ed160
// Re-pings every unfiltered game in the list while on the Internet screen and refreshes the game list display
void RoutingLogic::RefreshGameList()
{
    if (!LobbyOnInternetScreen()) return;
    this->mGameList->Rewind();
    while (this->mGameList->HasMore()) {
        LobbyGame* aGame = this->mGameList->Next();
        if (!aGame->mIsFiltered) LobbyPingGame(aGame, false);
    }
    LobbyBroadcastLobbyEvent(100058, true);
}

// FUNCTION: WON 0x007eef10
// Makes a joined chat or game room current, switching servers and subscribing to game summaries when needed
void RoutingLogic::JoinedGroup(unsigned short inGroupId, bool inGameGroup)
{
    LobbyGroup* anOldGroup = (inGameGroup ? this->mGameGroup : this->mChatGroup).get();
    if (this->mPendingServer.get() != NULL && this->mPendingServer.get() != this->mCurrentServer.get()) {
        this->LeaveServer();
        this->mIdle = false;
        this->mSuppressIdle = false;
        this->mHaveGameList = false;
        this->mCurrentServer = this->mPendingServer.get();
        RoutingConnection* aConnection = this->GetConnection();
        if (aConnection != NULL)
            aConnection->SubscribeDataObject(0xffff, this->mGameDataType, false,
                new RoutingOpCompletion(&RoutingLogic::ReadGameListComplete, this));
    } else if (anOldGroup != NULL && this->mCurrentServer.get() != NULL &&
               this->mCurrentServer->mConnection.get() != NULL) {
        this->mCurrentServer->mConnection->LeaveGroup(anOldGroup->GetId(), NULL);
    }
    if (this->mCurrentServer.get() == NULL) return;
    LobbyGroup* aGroup = this->mCurrentServer->GetGroup(inGroupId);
    if (inGameGroup) this->mGameGroup = aGroup; else this->mChatGroup = aGroup;
    if (aGroup == NULL) return;
    if (aGroup->IsPublicChatGroup() && this->mCurrentServer.get() != NULL)
        LobbySetLastRoom(this->mCurrentServer->mName, aGroup->mName);
    unsigned int aRoomSpec = (inGameGroup ? 2 : 1) | 4;
    LobbyBroadcastEvent(new JoinedRoomEvent(this->mCurrentServer->mName, aGroup->mName,
        aRoomSpec, this->mCurrentServer->GetMyClientId(), aGroup->mClients.get()), true);
}

// FUNCTION: WON 0x007e8df0
// Rebuilds the game list from the server's game summaries, pings them if auto-ping is on, and ends a waiting op
void RoutingLogic::ReadGameListComplete(RoutingOp* inBaseOp, LobbyServer* inServer)
{
    if (inServer != this->mCurrentServer.get()) return;
    this->mGameList->Clear();
    bool doAlert = LobbyGetAutoPingGames();
    RoutingReadDataObjectOp* inOp = static_cast<RoutingReadDataObjectOp*>(inBaseOp);
    for (RoutingDataObjectList::iterator anItr = inOp->mDataObjects.begin();
         anItr != inOp->mDataObjects.end(); ++anItr) {
        RoutingDataObject* anObject = anItr->get();
        LobbyGroup* aGroup = inServer->GetGroup(anObject->mLinkId);
        if (aGroup == NULL) continue;
        SmartPtr<LobbyGame> aGame = LobbyGame::CreateGame(LobbyGameType_Internet);
        aGame->ReadGroup(aGroup);
        if (!aGame->ReadSummaryBuffer(anObject->mData.get())) continue;
        this->mGameList->AddGame(aGame.get());
        if (doAlert) LobbyPingGame(aGame.get(), false);
    }
    LobbyBroadcastEvent(new SyncGameListEvent(this->mGameList.get()), true);
    this->mHaveGameList = true;
    if (this->mGameListWaitOp.get() != NULL) this->mGameListWaitOp->Finish(WS_Success);
}

// FUNCTION: WON 0x007ea630
// Sends the captain's accept or decline of a player asking to join the hosted game
void RoutingLogic::AcceptClient(AcceptClientEvent* inEvent)
{
    if (this->mCurrentServer.get() == NULL || this->mGameGroup.get() == NULL) return;
    RoutingConnection* aConnection = this->mCurrentServer->mConnection.get();
    if (aConnection == NULL || inEvent->mClient.get() == NULL) return;
    LobbyClient* aClient = this->mCurrentServer->GetClient(inEvent->mClient->GetClientId());
    if (aClient != inEvent->mClient.get()) return;
    aConnection->AcceptClient(this->mGameGroup->GetId(), aClient->GetClientId(),
                              inEvent->mAccepted, gEmptyWideString, NULL);
}

// FUNCTION: WON 0x007ec790
// Removes a friend from the friends list and clears the friend mark on the player if online here
void RoutingLogic::RemoveFriend(LobbyFriend* inFriend)
{
    if (inFriend == NULL) return;
    if (this->mCurrentServer.get() != NULL) {
        LobbyClient* aClient = this->mCurrentServer->FindClientByWONId(inFriend->mUserId);
        if (aClient != NULL) {
            aClient->mIsFriend = false;
            LobbyBroadcastEvent(new ClientChangedEvent(aClient, 4, 2), true);
        }
    }
    LobbyBroadcastEvent(new FriendChangedEvent(inFriend, 1), true);
    LobbyRemoveFriend(inFriend->mUserId);
}

// FUNCTION: WON 0x007ec870
// Runs a friends-list action: options popup, locate one friend or all, or remove the friend
void RoutingLogic::FriendAction(FriendActionEvent* inEvent)
{
    LobbyFriend* aFriend = inEvent->mFriend.get();
    if (aFriend == NULL) return;
    int anAction = inEvent->mAction;
    int aRoomSpec = inEvent->mValue;
    if (aRoomSpec == 0) aRoomSpec = 5;
    switch (anAction) {
    case 5:
        if (this->mCurrentServer.get() != NULL) {
            LobbyClient* aClient = this->mCurrentServer->FindClientByWONId(aFriend->mUserId);
            if (aClient != NULL) {
                this->ShowClientOptions(aClient, aRoomSpec);
                return;
            }
        }
    case 4:
        this->ShowFriendOptions(aFriend, aRoomSpec);
        return;
    case 1:
        this->LocateFriend(aFriend, aRoomSpec);
        return;
    case 2:
        this->LocateFriends();
        return;
    case 3:
        this->RemoveFriend(aFriend);
        return;
    }
}

// FUNCTION: WON 0x007eccc0
// Picks a Quick Play game: open, skill closest to the wanted one, then lowest ping; pings unknown ones
LobbyGame* RoutingLogic::SelectGame(int inGameType, unsigned short inPingLimit)
{
    unsigned short aPingCeiling = 10000;
    LobbyConfig* aConfig = GetLobbyConfig();
    if (aConfig != NULL) {
        aPingCeiling = aConfig->mPingCeiling;
        if (aPingCeiling == 0 || aPingCeiling > 0xf000) aPingCeiling = 0xf000;
    }
    typedef std::multimap<unsigned short, LobbyGame*> GameScoreMap;
    GameScoreMap aScores;
    this->mGameList->Rewind();
    while (this->mGameList->HasMore()) {
        LobbyGame* aGame = this->mGameList->Next();
        if (aGame->GetHasPassword() || aGame->GetAskToJoin() || !aGame->CanJoin()) continue;
        unsigned short aPing = aGame->GetPing();
        if (aPing < aPingCeiling) {
            int aSkill = aGame->GetSkill();
            int aRank;
            if (aSkill == inGameType) aRank = 0;
            else if (aSkill == 0) aRank = 10000;
            else if (aSkill + 1 == inGameType) aRank = 20000;
            else if (aSkill - 1 == inGameType) aRank = 30000;
            else continue;
            if (aPing < inPingLimit) {
                aRank += aPing;
                aScores.insert(GameScoreMap::value_type(aRank, aGame));
            }
        } else if (aPing == 0xfffe) {
            LobbyPingGame(aGame, false);
        }
    }
    GameScoreMap::iterator anItr = aScores.begin();
    if (anItr != aScores.end()) return anItr->second;
    else return NULL;
}

// FUNCTION: WON 0x007ecc30
// Jumps to a room on the current server: joins its game if the name still matches, else joins the chat room
void RoutingLogic::JumpToRoom(JumpToRoomEvent* inEvent)
{
    if (this->IsConnecting() || this->mCurrentServer.get() == NULL ||
        this->mCurrentServer->mAddress != inEvent->mAddress) return;
    LobbyGroup* aGroup = this->mCurrentServer->GetGroup(inEvent->mGroupId);
    if (aGroup == NULL) return;
    if (aGroup->IsGameGroup()) {
        LobbyGame* aGame = this->mGameList->GetGame(inEvent->mGroupId);
        if (aGame != NULL && aGame->mName == inEvent->mRoomName)
            LobbyJoinGame(aGame);
    } else {
        this->JoinChatRoom(this->mCurrentServer.get(), inEvent->mGroupId);
    }
}

// FUNCTION: WON 0x007ecf30
// Runs Quick Play: joins a server, waits for the game list and pings, joins a game or offers to host one
void RoutingLogic::QuickPlay(QuickPlayEvent* inEvent)
{
    if (!LobbyLeaveGame(true)) return;
    if (this->mCurrentServer.get() == NULL) this->JoinDefaultRoom(false);
    if (this->mCurrentServer.get() == NULL) return;
    if (!this->mHaveGameList) {
        this->mGameListWaitOp = new AsyncOp;
        LobbyRunOpWithStatus(this->mGameListWaitOp.get(), ::RoutingLogic_GettingGameListTitle_String,
                      ::RoutingLogic_GettingGameListDetails_String, 1, 0);
        this->mGameListWaitOp = NULL;
        if (!this->mHaveGameList) return;
    }
    while (true) {
        if (this->mCurrentServer.get() == NULL) return;
        LobbyGame* aGame = this->SelectGame(inEvent->mGameType, 1000);
        if (aGame != NULL) {
            LobbyJoinGame(aGame);
            return;
        }
        if (LobbyGetNumPingsToDo() > 0) {
            SmartPtr<AsyncOp> aWaitOp = new AsyncOp;
            LobbyRunOpWithStatus(aWaitOp.get(), ::RoutingLogic_CollectingPingsTitle_String,
                          ::RoutingLogic_CollectingPingsDetails_String, 2, 1000);
            if (aWaitOp->mKilled) return;
        } else {
            if (LobbyDoYesNoDialog(::GameStagingLogic_NoQuickPlayGameTitle_String, ::GameStagingLogic_NoQuickPlayGameDetails_String, true) == 100001)
                LobbyHostGame();
            return;
        }
    }
}

// FUNCTION: WON 0x007ea740
// Runs a player-list action: options, admin actions, friend, ignore, block, invite, away, whisper or locate
void RoutingLogic::ClientAction(ClientActionEvent* inEvent)
{
    if (this->mCurrentServer.get() == NULL) return;
    RoutingConnection* aConnection = this->mCurrentServer->mConnection.get();
    if (aConnection == NULL) return;
    if (!((inEvent->GetRoomSpec() & 4) ? true : false)) return;
    LobbyClient* aClient = inEvent->GetClient();
    if (aClient == NULL) return;
    switch (inEvent->GetAction()) {
    case 18:
        this->ShowClientOptions(aClient, inEvent->GetRoomSpec());
        break;
    case 2: case 7: case 8: case 14: case 15:
        this->DoAdminAction(inEvent->GetAction(), inEvent->GetRoomSpec(), aClient);
        break;
    case 1: {
        if (aClient->GetWONUserId() == 0 || aClient->IsFriend() || !this->CanAddFriend()) return;
        aClient->mIsFriend = true;
        LobbyBroadcastEvent(new ClientChangedEvent(aClient, 4, 2), true);
        LobbyFriend* aFriend = LobbyAddFriend(aClient->mName, aClient->GetWONUserId());
        if (aFriend == NULL) return;
        LobbyClient* aCurrentClient = this->mCurrentServer->FindClientByWONId(aClient->GetWONUserId());
        if (aCurrentClient != NULL) {
            aFriend->SetLocation(this->mCurrentServer->mAddress, this->mCurrentServer->mName, 0);
            aFriend->SetStatusFromClient(aCurrentClient);
        }
        LobbyBroadcastEvent(new FriendChangedEvent(aFriend, 0), true);
        break;
    }
    case 9: {
        if (!aClient->IsFriend()) return;
        aClient->mIsFriend = false;
        LobbyBroadcastEvent(new ClientChangedEvent(aClient, 4, 2), true);
        LobbyFriend* aFriend = LobbyFindFriend(aClient->GetWONUserId());
        if (aFriend != NULL)
            LobbyBroadcastEvent(new FriendChangedEvent(aFriend, 1), true);
        LobbyRemoveFriend(aClient->GetWONUserId());
        break;
    }
    case 3:
        LobbySetClientBlocked(aClient, true, 4);
        break;
    case 4:
        this->mSuppressIdle = false;
        aConnection->SetClientFlags(RoutingClientFlag_IsAway, 0, 0, 0, NULL);
        break;
    case 5:
        LobbySetClientIgnored(aClient, true, 4);
        break;
    case 6:
        this->InviteClient(aClient, true, "");
        break;
    case 13:
        this->InviteClient(aClient, false, "");
        break;
    case 10:
        this->mSuppressIdle = true;
        aConnection->SetClientFlags(RoutingClientFlag_IsAway, RoutingClientFlag_IsAway, 0, 0, NULL);
        break;
    case 11:
        LobbySetClientBlocked(aClient, false, 4);
        break;
    case 12:
        LobbySetClientIgnored(aClient, false, 4);
        break;
    case 16:
        LobbyBroadcastEvent(new SetChatInputEvent(2, aClient->mName,
                                            inEvent->GetRoomSpec(), false), true);
        break;
    case 17: {
        if (aClient->IsFriend()) {
            LobbyFriend* aFriend = LobbyFindFriend(aClient->GetWONUserId());
            if (aFriend != NULL) {
                this->LocateFriend(aFriend, inEvent->GetRoomSpec());
                return;
            }
        }
        this->AnnounceClientLocation(aClient, true, inEvent->GetRoomSpec(), true);
        break;
    }
    }
}

// FUNCTION: WON 0x007ebb40
// Invites or uninvites a player to the hosted game and shows it in chat; false unless the local player hosts
bool RoutingLogic::InviteClient(LobbyClient* inClient, bool inInvite,
                                const GUIString& inComment)
{
    if (inClient == NULL || this->mGameGroup.get() == NULL) return false;
    LobbyGame* aMyGame = GetMyLobbyGame();
    if (aMyGame == NULL || !aMyGame->mIsHost) return false;
    RoutingConnection* aConnection = this->GetConnection();
    if (aConnection == NULL) return false;
    inClient->mInvited = inInvite;
    aConnection->InviteClient(this->mGameGroup->GetId(), inClient->GetClientId(), inInvite,
                               inComment, NULL);
    LobbyBroadcastEvent(new GroupInvitationEvent(4, this->mGameGroup->mName,
        this->mGameGroup->GetId(), inClient, inInvite, inComment, true), true);
    return true;
}

// FUNCTION: WON 0x007eb350
// Carries out a typed chat line or slash command: chat, team chat, whisper, away, invite, mute, ban, admin alerts
void RoutingLogic::ChatInput(Component* inTarget)
{
    if (this->mCurrentServer.get() == NULL) return;
    RoutingConnection* aConnection = this->mCurrentServer->mConnection.get();
    if (aConnection == NULL) return;
    ChatCommandParser* aParser = LobbyGetChatCommandParser();
    if (aParser == NULL || !(aParser->GetRoomFlags() & 4)) return;
    LobbyGroup* aGroup = this->GetGroup(aParser->GetRoomFlags());
    if (aGroup == NULL) return;
    int aGroupId = aGroup->GetId();
    LobbyClient* aTarget = aParser->GetTarget();
    switch (aParser->GetCommand()) {
    case LobbyChatCommand_Broadcast: case LobbyChatCommand_Emote: {
        int aFlags = aParser->GetCommand() == LobbyChatCommand_Emote ? 2 : 0;
        if (aParser->GetRoomFlags() & 0x10) {
            LobbyClient* aMyClient = GetMyLobbyClient();
            if (aMyClient == NULL) return;
            std::list<unsigned short> aRecipients;
            LobbyClientIterator anItr(aParser->GetClients());
            while (anItr.HasMore()) {
                LobbyClient* aClient = anItr.Next();
                if (aMyClient->IsTeammate(aClient))
                    aRecipients.push_back(aClient->GetClientId());
            }
            aConnection->SendChatTo(aParser->GetText(),
                aFlags | 0x100, aRecipients, NULL);
        } else {
            aConnection->SendChat(aParser->GetText(), aFlags, aGroupId, NULL);
        }
        break;
    }
    case LobbyChatCommand_Whisper:
        if (aTarget != NULL)
            aConnection->SendChat(aParser->GetText(), RoutingChatFlag_IsWhisper, aTarget->GetClientId(), NULL);
        break;
    case LobbyChatCommand_Away: {
        LobbyClient* aMyClient = this->mCurrentServer->GetMyClient();
        if (aMyClient == NULL) return;
        this->mSuppressIdle = !aMyClient->IsAway();
        aConnection->SetClientFlags(RoutingClientFlag_IsAway, this->mSuppressIdle ? 8 : 0, 0, 0, NULL);
        break;
    }
    case LobbyChatCommand_Invite: case LobbyChatCommand_Uninvite:
        if (aTarget != NULL && !this->InviteClient(aTarget, aParser->GetCommand() == LobbyChatCommand_Invite, aParser->GetText()))
            LobbySendEventTo(new ChatCommandErrorEvent(::RoutingLogic_NotCaptain_String), inTarget);
        break;
    case LobbyChatCommand_Moderator: {
        LobbyClient* aMyClient = this->mCurrentServer->GetMyClient();
        if (aMyClient == NULL) return;
        // mWantToBeModerator (+0x5f) is the moderator state the player asked for (inferred)
        this->mWantToBeModerator = !aMyClient->IsModerator();
        aConnection->BecomeModerator(this->mWantToBeModerator, NULL);
        break;
    }
    case LobbyChatCommand_Mute:
        if (aTarget != NULL)
            aConnection->MuteClient(aGroupId, true, aParser->GetDuration(),
                aParser->GetText(), aTarget->GetClientId(), NULL);
        break;
    case LobbyChatCommand_ServerMute:
        if (aTarget != NULL)
            aConnection->MuteClient(RoutingId_Global, true, aParser->GetDuration(),
                aParser->GetText(), aTarget->GetClientId(), NULL);
        break;
    case LobbyChatCommand_Ban:
        if (aTarget != NULL)
            aConnection->BanClient(aGroupId, true, aParser->GetDuration(),
                aParser->GetText(), aTarget->GetClientId(), NULL);
        break;
    case LobbyChatCommand_ServerBan:
        if (aTarget != NULL)
            aConnection->BanClient(RoutingId_Global, true, aParser->GetDuration(),
                aParser->GetText(), aTarget->GetClientId(), NULL);
        break;
    case LobbyChatCommand_Unmute:
        if (aTarget != NULL)
            aConnection->MuteClient(aGroupId, false, aParser->GetDuration(),
                aParser->GetText(), aTarget->GetClientId(), NULL);
        else this->EditBadUserList(true, false, aParser->GetRoomFlags());
        break;
    case LobbyChatCommand_ServerUnmute:
        if (aTarget != NULL)
            aConnection->MuteClient(RoutingId_Global, false, aParser->GetDuration(),
                aParser->GetText(), aTarget->GetClientId(), NULL);
        else this->EditBadUserList(true, true, aParser->GetRoomFlags());
        break;
    case LobbyChatCommand_Ignore:
        if (aTarget != NULL) LobbySetClientIgnored(aTarget, !aTarget->mIgnored, 4);
        break;
    case LobbyChatCommand_Block:
        if (aTarget != NULL) LobbySetClientBlocked(aTarget, !aTarget->mBlocked, 4);
        break;
    case LobbyChatCommand_Unban:
        this->EditBadUserList(false, false, aParser->GetRoomFlags());
        break;
    case LobbyChatCommand_ServerUnban:
        this->EditBadUserList(false, true, aParser->GetRoomFlags());
        break;
    case LobbyChatCommand_Alert: {
        LobbyClient* aMyClient = this->mCurrentServer->GetMyClient();
        if (aMyClient != NULL && aMyClient->IsAdmin())
            aConnection->SendServerAlert(aParser->GetText(), NULL);
        break;
    }
    case LobbyChatCommand_StartShutdown: {
        LobbyClient* aMyClient = this->mCurrentServer->GetMyClient();
        if (aMyClient != NULL && aMyClient->IsAdmin())
            aConnection->StartServerShutdown(aParser->GetText(),
                                               aParser->GetDuration(), NULL);
        break;
    }
    case LobbyChatCommand_AbortShutdown: {
        LobbyClient* aMyClient = this->mCurrentServer->GetMyClient();
        if (aMyClient != NULL && aMyClient->IsAdmin())
            aConnection->AbortServerShutdown(NULL);
        break;
    }
    case LobbyChatCommand_Warn:
        if (aTarget != NULL)
            aConnection->SendChat(aParser->GetText(), 0x10, aTarget->GetClientId(), NULL);
        break;
    }
}

// FUNCTION: WON 0x007e7320
// Handles the server dropping the connection: leaves the rooms and server and tells the player
void RoutingLogic::ConnectionClosed(AsyncOp* inBaseOp)
{
    SocketOp* inOp = static_cast<SocketOp*>(inBaseOp);
    // Both RefCount subobjects sit at offset 0, so the socket and connection compare as RefCount pointers
    RefCount* aSocket = inOp->mSocket.get();
    RefCount* aConnection = this->GetConnection();
    if (aSocket != aConnection) return;
    LobbyBroadcastEvent(new LeftRoomEvent(4, true, true), true);
    if (this->mGameGroup.get() != NULL) LobbyGameDisconnected(false);
    this->LeaveServer();
    LobbyMessageBox(::RoutingLogic_DisconnectedTitle_String, ::RoutingLogic_DisconnectedDetails_String);
}

// FUNCTION: WON 0x007eac40
// Asks for a name and password, then creates a chat room on the chosen server and joins it
void RoutingLogic::CreateRoom(CreateRoomEvent* inEvent)
{
    this->mCreateRoomDialog->Reset(false);
    if (LobbyDoDialog(this->mCreateRoomDialog.get()) != 100001 || this->IsConnecting()) return;
    GUIString aName = this->mCreateRoomDialog->GetRoomName();
    GUIString aPassword = this->mCreateRoomDialog->GetPassword();
    if (aName.length() == 0) return;
    LobbyServer* aServer = this->GetServer(inEvent->mAddress);
    if (aServer == NULL) aServer = this->mCurrentServer.get();
    if (aServer == NULL) aServer = this->GetDefaultServer();
    if (aServer == NULL) return;
    PendingServerCleanup aCleanup(this);
    if (!this->ConnectToServer(aServer, false)) return;
    unsigned short aGroupId;
    if (!this->CreateGroup(this->mPendingServer.get(), aName, aPassword, 0x800, aGroupId)) return;
    this->JoinedGroup(aGroupId, false);
    if (this->mRoomDialog->GetParent() != NULL)
        GetLobbyWindow()->EndDialog(-1, NULL, this->mRoomDialog.get());
}

// FUNCTION: WON 0x007ec910
// Connects to a server and joins a chat room there; tells the player when already in that room
bool RoutingLogic::JoinChatRoom(LobbyServer* inServer, unsigned short inGroupId)
{
    if (inServer == NULL) return false;
    PendingServerCleanup aCleanup(this);
    if (!this->ConnectToServer(inServer, false)) return false;
    if (this->mChatGroup.get() != NULL && inServer == this->mCurrentServer.get() &&
        inGroupId == this->mChatGroup->GetId()) {
        LobbyMessageBox(::RoutingLogic_AlreadyJoinedTitle_String, ::RoutingLogic_AlreadyInRoom_String);
        return false;
    }
    if (!this->JoinGroup(this->mPendingServer.get(), inGroupId)) return false;
    this->JoinedGroup(inGroupId, false);
    return true;
}

// FUNCTION: WON 0x007ec9e0
// Joins the chat room picked in the room dialog and closes the dialog on success
void RoutingLogic::JoinRoom(JoinRoomEvent* inEvent)
{
    if (this->IsConnecting()) return;
    LobbyServer* aServer = this->GetServer(inEvent->mAddress);
    if (aServer == NULL) return;
    unsigned long aGroupId = inEvent->mGroupId;
    if (aGroupId == 0xffff) aGroupId = 0;
    if (this->JoinChatRoom(aServer, aGroupId) && this->mRoomDialog->GetParent() != NULL)
        GetLobbyWindow()->EndDialog(-1, NULL, this->mRoomDialog.get());
}

}

namespace WONAPI {
// FUNCTION: WON 0x007eca40
// Connects to a friend's server and joins the chat room the friend is in (or the server's lobby)
void RoutingLogic::JoinFriendsRoom(JoinFriendsRoomEvent* inEvent)
{
    if (this->IsConnecting()) return;
    LobbyFriend* aFriend = inEvent->mFriend.get();
    if (aFriend == NULL) return;
    IPAddr anAddress = aFriend->mAddress;
    if (!anAddress.IsValid()) return;
    LobbyServer* aServer = this->GetServer(anAddress);
    if (aServer == NULL) return;
    PendingServerCleanup aCleanup(this);
    if (!this->ConnectToServer(aServer, false)) return;
    LobbyClient* aClient = aServer->FindClientByWONId(aFriend->GetUserId());
    if (aClient == NULL) return;
    unsigned short aGroupId = 0;
    if (aClient->IsInChatGroup()) aGroupId = aClient->mChatGroupId;
    if (this->mChatGroup.get() != NULL && aServer == this->mCurrentServer.get() &&
        aGroupId == this->mChatGroup->mId) {
        LobbyMessageBox(::RoutingLogic_AlreadyJoinedTitle_String, ::RoutingLogic_AlreadyInRoom_String);
        return;
    }
    if (!this->JoinGroup(aServer, aGroupId)) return;
    this->JoinedGroup(aGroupId, false);
}

// The pending-server cleanup runs before the copied IPAddr is destroyed on every exit

}


namespace WONAPI {
// FUNCTION: WON 0x007ed670
// Creates a room on a server with a status dialog and returns its id; false on failure
bool RoutingLogic::CreateGroup(LobbyServer* inServer,
    const GUIString& inName, const GUIString& inPassword,
    int inRoomFlags, unsigned short& outGroupId)
{
    if (inServer != NULL) {
    SmartPtr<RoutingConnection> aConnection = inServer->mConnection;
    if (aConnection.get() == NULL) return false;
    SmartPtr<RoutingCreateGroupOp> anOp = new RoutingCreateGroupOp(aConnection.get());
    anOp->SetGroupPassword(inPassword);
    anOp->SetGroupName(inName);
    anOp->SetGroupFlags(inRoomFlags);
    anOp->SetJoinFlags(1);
    anOp->SetLeaveGroupOnCancel(true);
    LobbyRunOpWithStatus(anOp.get(), ::RoutingLogic_CreateGroupTitle_String, ::RoutingLogic_CreateGroupDetails_String, 1, 0);
    if (anOp->GetStatus() == WS_Success) {
        outGroupId = anOp->GetGroupId();
        return true;
    }
    }
    return false;
}

// FUNCTION: WON 0x007ed8b0
// Joins a room on a server, asking for its password (again on a wrong one); false on failure or cancel
bool RoutingLogic::JoinGroup(LobbyServer* inServer, int inGroupId)
{
    if (inServer == NULL) return false;
    SmartPtr<RoutingConnection> aConnection = inServer->mConnection;
    if (aConnection.get() == NULL) return false;
    SmartPtr<LobbyGroup> aGroup = inServer->GetGroup(inGroupId);
    if (aGroup.get() == NULL) {
        LobbyDoErrorDialog(::RoutingLogic_JoinGroupTitle_String, (WONStatus)-2520);
        return false;
    }
    GUIString aPassword;
    if (aGroup->IsPasswordProtected()) {
        this->mPasswordDialog->Reset();
        if (LobbyDoDialog(this->mPasswordDialog.get()) != 0x186a1) return false;
        aPassword = this->mPasswordDialog->GetPassword();
    }
    if (!aConnection->IsConnected()) return false;
    SmartPtr<RoutingJoinGroupOp> anOp = new RoutingJoinGroupOp(aConnection.get());
    anOp->SetGroupId(inGroupId);
    anOp->SetGroupPassword(aPassword);
    while (true) {
        if (aGroup.get() != NULL && aGroup->IsAskToJoin())
            LobbyRunOpWithStatus(anOp.get(), ::RoutingLogic_AskingCaptainTitle_String, ::RoutingLogic_AskingCaptainDetails_String, 1, 0);
        else
            LobbyRunOpWithStatus(anOp.get(), ::RoutingLogic_JoinGroupTitle_String, ::RoutingLogic_JoinGroupDetails_String, 1, 0);
        if (anOp->GetStatus() == WS_Success) break;
        if (!aConnection->IsConnected()) return false;
        if (anOp->mKilled) {
            aConnection->CancelJoinGroup(inGroupId, true, NULL);
            return false;
        }
        if (anOp->GetStatus() == (WONStatus)-2524) {
            this->mPasswordDialog->Reset();
            if (LobbyDoDialog(this->mPasswordDialog.get()) != 0x186a1) return false;
            anOp->SetGroupPassword(this->mPasswordDialog->GetPassword());
        } else {
            if (anOp->GetStatus() == (WONStatus)-2504) return true;
            return false;
        }
    }
    return true;
}
}


namespace WONAPI {
}

namespace WONAPI {
// FUNCTION: WON 0x007ea490
// Opens the options popup for a player in the player list
void RoutingLogic::ShowClientOptions(LobbyClient* inClient,int inRoomSpec)
{
    if(this->mCurrentServer.get()==NULL || inClient==NULL) return;
    LobbyClient* myClient=this->mCurrentServer->GetMyClient();
    if(myClient==NULL) return;
    SmartPtr<ClientOptionsPopup> popup=new ClientOptionsPopup;
    popup->Init(inClient,myClient,inRoomSpec);
    LobbyOpenPopup(popup.get(),PopupFlag_StandardPopup,-1,-1);
}
}



namespace WONAPI {
// FUNCTION: WON 0x007e9440
// Kicks, bans, mutes, unmutes or warns a player, asking moderators for details in the admin dialog first
void RoutingLogic::DoAdminAction(int inAction, int inRoomSpec, LobbyClient* inClient)
{
    RoutingConnection* connection = this->GetConnection();
    if (connection == NULL) return;
    LobbyClient* myClient = this->mCurrentServer->GetMyClient();
    if (myClient == NULL) return;
    bool showDialog = myClient->IsModerator() || myClient->IsAdmin();
    if (showDialog) {
        this->mAdminActionDialog->Reset(inAction, inClient);
        if (LobbyDoDialog(this->mAdminActionDialog.get()) != 100001) return;
        connection = this->GetConnection();
        if (connection == NULL) return;
    }
    LobbyClient* currentClient = this->mCurrentServer->FindClientByWONId(inClient->GetWONUserId());
    int groupId = 0xffff;
    if (inRoomSpec & 2) {
        if (this->mGameGroup.get() == NULL) return;
        groupId = this->mGameGroup->GetId();
    } else if (inRoomSpec & 1) {
        if (this->mChatGroup.get() == NULL) return;
        groupId = this->mChatGroup->GetId();
    }
    GUIString explanation;
    unsigned long seconds;
    if (showDialog) {
        if (this->mAdminActionDialog->GetApplyToServer()) groupId = 0xffff;
        else if (groupId == 0xffff && inAction != 15) return;
        explanation = this->mAdminActionDialog->GetExplanation();
        seconds = this->mAdminActionDialog->GetNumSeconds();
    } else {
        if (groupId == 0xffff) return;
        seconds = inAction == 7;
    }
    SmartPtr<RoutingOp> op;
    GUIString title;
    switch (inAction) {
    case 2:
    case 7: {
        if (!showDialog && (inRoomSpec & 2)) {
            LobbyKickClient(inClient, inAction == 2);
            return;
        }
        SmartPtr<RoutingBanClientOp> ban = new RoutingBanClientOp(connection);
        if (inAction == 2) {
            ban->SetBanComment(explanation);
            ban->SetBanTime(seconds);
            title = ::RoutingLogic_BanClientTitle_String;
        } else if (inAction == 7) {
            ban->SetBanTime(1);
            title = ::RoutingLogic_KickClientTitle_String;
        }
        ban->SetGroupId(groupId);
        op = ban.get();
        if (currentClient != NULL) {
            ban->SetClientId(currentClient->GetClientId());
        } else {
            unsigned long id = inClient->GetCDKeyId();
            if (id == 0) id = inClient->GetWONUserId();
            if (id != 0) {
                ban->SetWONId(id,true);
            }
        }
        break;
    }
    case 8:
    case 14: {
        SmartPtr<RoutingMuteClientOp> mute = new RoutingMuteClientOp(connection);
        mute->SetMuteComment(explanation);
        mute->SetMuteTime(seconds);
        mute->SetGroupId(groupId);
        op = mute.get();
        if (currentClient != NULL) {
            mute->SetClientId(currentClient->GetClientId());
        } else {
            unsigned long id = inClient->GetCDKeyId();
            if (id == 0) id = inClient->GetWONUserId();
            if (id != 0) {
                mute->SetWONId(id,true);
            }
        }
        if (inAction == 14) {
            title = ::RoutingLogic_UnmuteClientTitle_String;
            mute->SetIsMuted(false);
        } else {
            title = ::RoutingLogic_MuteClientTitle_String;
            mute->SetIsMuted(true);
        }
        break;
    }
    case 15:
        if (currentClient != NULL)
            connection->SendChat(explanation, 0x10, currentClient->GetClientId(), NULL);
        return;
    }
    if (op.get() == NULL) return;
    if (showDialog) LobbyRunOpWithStatus(op.get(), title, ::RoutingLogic_ModerateDetails_String, 1, 0);
    else this->RunOp(op.get(), NULL, 0);
}
}

namespace WONAPI {
// FUNCTION: WON 0x007e9ab0
// Shows the muted or banned players of a room or the server and lifts the restriction on the ones picked
void RoutingLogic::EditBadUserList(bool inMuted, bool inServerWide, int inRoomSpec)
{
    RoutingConnection* connection = this->GetConnection();
    if (connection == NULL) return;
    LobbyClient* myClient = this->mCurrentServer->GetMyClient();
    if (myClient == NULL) return;
    bool moderator = myClient->IsModerator() || myClient->IsAdmin();
    RoutingBadUserListType listType = inMuted ? RoutingBadUserListType_MutedUsers
                                             : RoutingBadUserListType_BannedUsers;
    unsigned short groupId = 0xffff;
    if (!inServerWide) {
        LobbyGroup* group = this->GetGroup(inRoomSpec);
        if (group == NULL) return;
        groupId = group->mId;
    }
    SmartPtr<RoutingGetBadUserListOp> op =
        new RoutingGetBadUserListOp(connection, listType, groupId, 0x0f);
    LobbyRunOpWithStatus(op.get(), ::RoutingLogic_GetBadUsersTitle_String, ::RoutingLogic_GetBadUsersDetails_String, 1, 0);
    if (op->mStatus != WS_Success) return;
    this->mBadUserDialog->SetUserList(op->mBadUserList, !moderator);
    if (LobbyDoDialog(this->mBadUserDialog.get()) != 100001) return;
    connection = this->GetConnection();
    if (connection == NULL) return;

    std::list<unsigned long> ids;
    this->mBadUserDialog->GetIds(ids);
    if (ids.empty()) return;
    std::list<unsigned long>::iterator i = ids.begin();
    while(i != ids.end()) {
        if (listType == RoutingBadUserListType_BannedUsers)
            connection->BanClientByWONId(groupId, false, 0, std::wstring(gEmptyWideString), *i, NULL);
        else if (listType == RoutingBadUserListType_MutedUsers)
            connection->MuteClientByWONId(groupId, false, 0, std::wstring(gEmptyWideString), *i, NULL);
        ++i;
    }
}
}

namespace WONAPI {
// The iterator advances after RunOp, not before the completion is created
// FUNCTION: WON 0x007ebfd0
// Asks every other server whether a friend is on it, or announces the known location when no search is needed
void RoutingLogic::LocateFriend(LobbyFriend* inFriend,int inRoomSpec)
{
    if(inFriend==NULL) return;
    if(!this->ShouldLocateFriend(inFriend,(unsigned long)time(NULL))) {
        if(!(inFriend->mFlags&4)) this->AnnounceFriendLocation(inFriend,inRoomSpec);
        return;
    }
    UDPManager* manager=LobbyGetUDPManager();
    if(manager==NULL) return;
    int count=0;
    LobbyServerList* servers=this->mServerList.get();
    for(LobbyServerMap::iterator it=servers->mServers.begin();it!=servers->mServers.end();++it) {
        if(this->mCurrentServer.get()!=NULL && this->mCurrentServer->mAddress==it->first) continue;
        SmartPtr<RoutingGetUserStatusOp> op=new RoutingGetUserStatusOp(it->first,manager);
        op->AddWONUserId(inFriend->GetUserId());
        this->RunOp(op.get(),new LogicCompletion<RoutingLogic>(&RoutingLogic::GetUserStatusComplete,this),10000);
        ++count;
    }
    if(count<=0)
        return;
    inFriend->StartLocate(inRoomSpec);
    inFriend->mLastSearch=count;
    LobbyBroadcastEvent(new FriendChangedEvent(inFriend,2),true);
}

// The completion receives an AsyncOp*, not a RoutingOp*
// FUNCTION: WON 0x007e7460
// Records the friends one server reported, ends their search after the last reply and refreshes the friends list
void RoutingLogic::GetUserStatusComplete(AsyncOp* inBaseOp)
{
    RoutingGetUserStatusOp* op=static_cast<RoutingGetUserStatusOp*>(inBaseOp);
    LobbyServer* server=this->mServerList->FindServer(op->mAddress);
    unsigned long now=(unsigned long)time(NULL);
    SmartPtr<LobbyFriend> lastChanged;
    int changed=0;
    for(RoutingGetUserStatusOp::FriendStatusMap::iterator it=op->mFriendStatusMap.begin();
        it!=op->mFriendStatusMap.end();++it) {
        LobbyFriend* friendInfo=LobbyFindFriend(it->first);
        if(friendInfo==NULL || !(friendInfo->mFlags&4)) continue;
        bool found=false;
        if(it->second!=0 && server!=NULL) {
            friendInfo->SetLocation(server->mAddress,server->mName,it->second);
            found=true;
        }
        --friendInfo->mLastSearch;
        int remaining=friendInfo->mLastSearch;
        if(remaining>0 && !found) continue;
        lastChanged=friendInfo;
        ++changed;
        friendInfo->EndLocate(now);
        if(friendInfo->GetSearchStart()!=0)
            LobbyBroadcastEvent(new AnnounceFriendEvent(friendInfo,friendInfo->GetSearchStart(),2),true);
    }
    if(changed>1) LobbyBroadcastLobbyEvent(100056,true);
    else if(changed==1 && lastChanged.get()!=NULL)
        LobbyBroadcastEvent(new FriendChangedEvent(lastChanged.get(),2),true);
}
}

namespace WONAPI {
// FUNCTION: WON 0x007ec4a0
// Asks every other server about all friends that need a search and refreshes the friends list
void RoutingLogic::LocateFriends()
{
    UDPManager* manager = LobbyGetUDPManager();
    if (manager == NULL) return;
    unsigned long now = (unsigned long)time(NULL);
    const LobbyFriendMap& friends = LobbyFn35D270();
    LobbyServerList* servers = this->mServerList.get();
    int count = 0;
    for (LobbyServerMap::iterator scanServer = servers->mServers.begin();
         scanServer != servers->mServers.end(); ++scanServer) {
        if (this->mCurrentServer.get() != NULL && this->mCurrentServer->mAddress == scanServer->first)
            continue;
        ++count;
    }
    if (count == 0) return;
    RoutingGetUserStatusOp::FriendStatusMap requested;
    LobbyFriendMap::const_iterator friendItr = friends.begin();
    while(friendItr != friends.end()) {
        LobbyFriend* friendInfo = friendItr->second.get();
        if (this->ShouldLocateFriend(friendInfo,now)) {
            friendInfo->StartLocate(0);
            friendInfo->mLastSearch = count;
            requested[friendInfo->GetUserId()] = 0;
        }
        ++friendItr;
    }
    if (requested.empty()) return;
    for (LobbyServerMap::iterator queryServer = servers->mServers.begin();
         queryServer != servers->mServers.end(); ++queryServer) {
        if (this->mCurrentServer.get() != NULL && this->mCurrentServer->mAddress == queryServer->first)
            continue;
        SmartPtr<RoutingGetUserStatusOp> op = new RoutingGetUserStatusOp(queryServer->first,manager);
        op->mFriendStatusMap = requested;
        this->RunOp(op.get(),new LogicCompletion<RoutingLogic>(&RoutingLogic::GetUserStatusComplete,this),10000);
    }
    LobbyBroadcastLobbyEvent(100056,true);
}
}

namespace WONAPI {
// FUNCTION: WON 0x007edd70
// Connects and registers with a lobby server, installs the notification handlers and loads its room list
bool RoutingLogic::ConnectToServer(LobbyServer* inServer, bool inFlag)
{
    if (inServer == NULL) return false;
    this->mPendingServer = inServer;
    if (inServer == this->mCurrentServer.get() && inServer->IsConnected()) return true;
    if (this->mGameGroup.get() != NULL && !LobbyLeaveGame(true)) return false;

    SmartPtr<RoutingConnection> connection = new RoutingConnection;
    inServer->SetConnection(connection.get());
    AuthContext* auth = GetLobbyAuthContext();
    if (auth == NULL) return false;
    connection->SetAuth(auth, AUTH_TYPE_PERSISTENT);
    connection->mCompletions[8] = new ::RoutingOpCompletion(&RoutingLogic::ClientJoinedServer, this);
    connection->mCompletions[10] = new ::RoutingOpCompletion(&RoutingLogic::ClientLeftServer, this);
    connection->mCompletions[7] = new ::RoutingOpCompletion(&RoutingLogic::ClientJoinedGroup, this);
    connection->mCompletions[9] = new ::RoutingOpCompletion(&RoutingLogic::ClientLeftGroup, this);
    connection->mCompletions[39] = new ::RoutingOpCompletion(&RoutingLogic::PeerChat, this);
    connection->mCompletions[40] = new ::RoutingOpCompletion(&RoutingLogic::PeerData, this);
    connection->mCompletions[6] = new ::RoutingOpCompletion(&RoutingLogic::ClientFlagsChanged, this);
    connection->mCompletions[36] = new ::RoutingOpCompletion(&RoutingLogic::MemberFlagsChanged, this);
    connection->mCompletions[24] = new ::RoutingOpCompletion(&RoutingLogic::GroupCreated, this);
    connection->mCompletions[25] = new ::RoutingOpCompletion(&RoutingLogic::GroupDeleted, this);
    connection->mCompletions[23] = new ::RoutingOpCompletion(&RoutingLogic::GroupCaptainChanged, this);
    connection->mCompletions[13] = new ::RoutingOpCompletion(&RoutingLogic::DataObjectCreated, this);
    connection->mCompletions[14] = new ::RoutingOpCompletion(&RoutingLogic::DataObjectDeleted, this);
    connection->mCompletions[27] = new ::RoutingOpCompletion(&RoutingLogic::GroupInvitation, this);
    connection->mCompletions[28] = new ::RoutingOpCompletion(&RoutingLogic::GroupJoinAttempt, this);
    connection->mCompletions[58] = new ::RoutingOpCompletion(&RoutingLogic::YouWereBanned, this);
    connection->mCompletions[59] = new ::RoutingOpCompletion(&RoutingLogic::YouWereMuted, this);
    connection->mCompletions[48] = new ::RoutingOpCompletion(&RoutingLogic::ServerAlert, this);
    connection->mCompletions[50] = new ::RoutingOpCompletion(&RoutingLogic::ServerShutdownStarted, this);
    connection->mCompletions[49] = new ::RoutingOpCompletion(&RoutingLogic::ServerShutdownAborted, this);

    SmartPtr<ServerConnectionConnectOp> connectOp =
        new ServerConnectionConnectOp(connection.get(), inServer->mAddress);
    LobbyRunOpWithStatus(connectOp.get(), ::RoutingLogic_ConnectTitle_String, ::RoutingLogic_ConnectDetails_String, 1, 0);
    if (connectOp->GetStatus() != WS_Success) return false;

    unsigned long registerFlags = 0x15;
    if (this->mWantToBeModerator) registerFlags = 0x1d;
    SmartPtr<RoutingRegisterClientOp> registerOp = new RoutingRegisterClientOp(connection.get());
    registerOp->mRegisterFlags = registerFlags;
    registerOp->mReconnectId = LobbyGetReconnectId(inServer->mAddress);
    unsigned long clientFlags = 0;
    if (!LobbyGetFilterDirtyWords()) clientFlags = 4;
    if (LobbyGetAnonymousToFriends() || inFlag) clientFlags |= 0x40;
    registerOp->mClientFlags = clientFlags;
    LobbyRunOpWithStatus(registerOp.get(), ::RoutingLogic_RegisterTitle_String, ::RoutingLogic_RegisterDetails_String, 1, 0);
    if (registerOp->GetStatus() != WS_Success) return false;

    LobbySetReconnectId(inServer->mAddress, registerOp->GetReconnectId());
    inServer->SyncClients(registerOp->GetClientMap());
    inServer->mMyClientId = registerOp->GetClientId();
    connection->SetCloseCompletion(new LogicCompletion<RoutingLogic>(&RoutingLogic::ConnectionClosed, this));
    if (inFlag && !LobbyGetAnonymousToFriends()) connection->SetClientFlags(0x40, 0, 0, 0, NULL);

    SmartPtr<RoutingGetGroupListOp> groupOp = new RoutingGetGroupListOp(connection.get(), 0xff);
    LobbyRunOpWithStatus(groupOp.get(), ::RoutingLogic_QueryGroupsTitle_String, ::RoutingLogic_QueryGroupsDetails_String, 1, 0);
    if (groupOp->GetStatus() != WS_Success) return false;
    inServer->SyncGroups(groupOp->mGroupMap, true);
    LobbyBroadcastEvent(new ServerChangedEvent(inServer, 2), true);
    inServer->UpdateFriends();
    LobbyBroadcastLobbyEvent(100056, true);
    return true;
}
// FUNCTION: WON 0x007ec210
// Asks for a player name, looks up the account and adds it to the friends list, marking the player if online here
void RoutingLogic::AddFriend()
{
    if(!this->CanAddFriend()) return;
    this->mAddFriendDialog->Reset();
    if(LobbyDoDialog(this->mAddFriendDialog.get())!=100001) return;
    ServerContext* servers=LobbyGetAccountServers();
    if(!servers) return;
    SmartPtr<GetUserIdFromNameOp> op=new GetUserIdFromNameOp(servers);
    GUIString name=this->mAddFriendDialog->GetName();
    op->SetUserName(name);
    LobbyRunOpWithStatus(op.get(),::AddFriendDialog_StatusTitle_String,::AddFriendDialog_StatusDetails_String,1,0);
    if(op->GetStatus()!=WS_Success || op->GetUserId()==0) return;
    LobbyFriend* friendEntry=LobbyAddFriend(name,op->GetUserId());
    if(friendEntry) {
    if(this->mCurrentServer.get()) {
        LobbyClient* client=this->mCurrentServer->FindClientByWONId(friendEntry->GetUserId());
        if(client) {
            client->mIsFriend=true;
            friendEntry->SetLocation(this->mCurrentServer->mAddress,this->mCurrentServer->mName,0);
            friendEntry->SetStatusFromClient(client);
            LobbyBroadcastEvent(new ClientChangedEvent(client,4,2),true);
        }
    }
    LobbyBroadcastEvent(new FriendChangedEvent(friendEntry,0),true);
    }
}
}

namespace WONAPI {
// FUNCTION: WON 0x007e9e60
// Stores a server's room list from a background query; on failure allows the next refresh at once
void RoutingLogic::QueryServerGroupsDone(AsyncOp* inBaseOp)
{
    LobbyServerList* servers=LobbyGetServerList();
    if(!servers) return;
    RoutingGetGroupListReqOp* op=static_cast<RoutingGetGroupListReqOp*>(inBaseOp);
    LobbyServer* server=servers->FindServer(op->GetAddr());
    if(!server) return;
    server->mQueryingGroups=false;
    if(op->GetStatus()!=WS_Success) {
        this->mLastServerRefresh=0;
        server->mLastPing=0;
    }
    server->SyncGroups(op->mOp->mGroupMap,false);
    LobbyBroadcastEvent(new ServerChangedEvent(server,2),true);
}
// FUNCTION: WON 0x007ed4c0
// Starts a background room-list query for a server not connected to, at most every 30 seconds
void RoutingLogic::HandleQueryServerGroups(QueryServerGroupsEvent* event)
{
    LobbyServer* server=this->GetServer(event->mAddress);
    if(!server) return;
    time_t now=time(NULL);
    if(server->IsConnected() || server->mQueryingGroups || static_cast<long>(now-server->GetLastPing())<30)
        return;
    server->ClearGroups();
    LobbyBroadcastEvent(new ServerChangedEvent(server,2),true);
    server->mQueryingGroups=true;
    server->mLastPing=now;
    SmartPtr<RoutingGetGroupListReqOp> op=new RoutingGetGroupListReqOp(server->mAddress,0);
    op->mOp->mFlags=0x29;
    this->RunTrackedOp(op.get(),new LogicCompletion<RoutingLogic>(&RoutingLogic::QueryServerGroupsDone,this),0);
}
}

namespace WONAPI {
// FUNCTION: WON 0x007e9f60
// Reloads the server list from the directory every 5 minutes (or when empty) and drops stale room lists
void RoutingLogic::RefreshServerList()
{
    SmartPtr<ServerContext> context=LobbyGetRoomDirServers();
    LobbyConfig* config=GetLobbyConfig();
    if(!context.get() || !config) return;
    time_t refreshTime=time(NULL);
    if(static_cast<long>(static_cast<unsigned long>(refreshTime)-static_cast<unsigned long>(this->mLastServerRefresh))>=300 ||
       this->mServerList->mServers.empty()) {
        SmartPtr<GetDirOp> op=new GetDirOp(context.get());
        op->SetPath(config->mProductDirectory);
        op->SetFlags(0x04000622);
        op->AddDataType("_C");
        op->AddDataType("_L");
        LobbyRunOpWithStatus(op.get(),::LobbyContainer_GettingRoomsTitle_String,::LobbyContainer_GettingRoomsDetails_String,1,0);
        if(op->GetStatus()!=WS_Success) return;
        if(op->GetDirEntityList().empty()) context->NotifyFailed(op->GetCurAddr());
        this->mServerList->UpdateFromDirectory(op->GetDirEntityList());
        this->mLastServerRefresh=refreshTime;
    }
    time_t now=time(NULL);
    LobbyServerList* servers=this->mServerList.get();
    LobbyServerMap::const_iterator it=servers->mServers.begin();
    while(it!=servers->mServers.end()) {
        LobbyServer* server=it->second.get();
        if(!server->IsConnected() && static_cast<long>(static_cast<unsigned long>(now)-server->GetLastPing())>=30)
            server->ClearGroups();
        ++it;
    }
    LobbyBroadcastLobbyEvent(100053,true);
}
// FUNCTION: WON 0x007ed430
// Opens the Internet lobby: refreshes servers, joins the default room or shows the room dialog, plays the sound
void RoutingLogic::EnterInternetScreen(bool joinDefaultRoom)
{
    this->RefreshServerList();
    if(this->mServerList->mServers.empty()) {
        LobbyMessageBox(::RoutingLogic_NoServersTitle_String,::RoutingLogic_NoServersDetails_String);
        return;
    }
    if(joinDefaultRoom) {
        if(!this->JoinDefaultRoom(false)) {
            if(LobbyUserCanceledLastStatusOp()) {
                LobbyLogout(false);
                return;
            }
            this->DoRoomDialog();
        }
    } else this->DoRoomDialog();
    LobbyShowInternetScreen();
    if(LobbyGetLobbySoundEffects() && !LobbyGetLobbyMusic()) LobbyPlaySound(::LobbyGlobal_SierraLogon_Sound.get());
}
}

#include "ee/wonapi/WONLobbyResourceGlobals.h"
namespace WONAPI {
// FUNCTION: WON 0x007e9a90
// Shows the room selection dialog
void RoutingLogic::DoRoomDialog()
{
    LobbyDoDialog(this->mRoomDialog.get());
}

// FUNCTION: WON 0x007ea310
// Handles the Internet lobby buttons: change room, add friend, anonymous and filter checks, refresh lists
void RoutingLogic::HandleControlEvent(int controlId)
{
    switch(controlId) {
    case ID_ChangeRoom: this->RefreshServerList();this->DoRoomDialog();break;
    case ID_AddFriend: this->AddFriend();break;
    case ID_AnonymousToFriendsCheck: this->HandleAnonymousModeChange();break;
    case ID_FilterDirtyWordsCheck: this->HandleDirtyWordFilterChange();break;
    case ID_RefreshFriends: this->LocateFriends();break;
    case ID_RefreshGameList: this->RefreshGameList();break;
    }
}

// FUNCTION: WON 0x007ea3a0
// Dispatches the Internet lobby's component events (accept, actions, rooms, chat, friends, Quick Play)
void RoutingLogic::HandleComponentEvent(ComponentEvent* event)
{
    switch(event->mType) {
    case LobbyEvent_AcceptClient: this->AcceptClient(static_cast<AcceptClientEvent*>(event));return;
    case LobbyEvent_ClientAction: this->ClientAction(static_cast<ClientActionEvent*>(event));return;
    case LobbyEvent_CreateRoom: this->CreateRoom(static_cast<CreateRoomEvent*>(event));return;
    case 100012: this->ChatInput(event->mComponent);return;
    case LobbyEvent_FriendAction: this->FriendAction(static_cast<FriendActionEvent*>(event));return;
    case LobbyEvent_JoinRoom: this->JoinRoom(static_cast<JoinRoomEvent*>(event));return;
    case LobbyEvent_JoinFriendsRoom: this->JoinFriendsRoom(static_cast<JoinFriendsRoomEvent*>(event));return;
    case LobbyEvent_JumpToRoom: this->JumpToRoom(static_cast<JumpToRoomEvent*>(event));return;
    case LobbyEvent_QueryServerGroups: this->HandleQueryServerGroups(static_cast<QueryServerGroupsEvent*>(event));return;
    case LobbyEvent_QuickPlay: this->QuickPlay(static_cast<QuickPlayEvent*>(event));return;
    }
}

// FUNCTION: WON 0x007eb2f0
// Sends the anonymous-to-friends setting to the server
void RoutingLogic::HandleAnonymousModeChange()
{
    RoutingConnection* connection=this->GetConnection();
    if(connection==NULL) return;
    unsigned long flags=0;
    if(LobbyGetAnonymousToFriends()) flags|=0x40;
    connection->SetClientFlags(0x40,flags,0,0,NULL);
}

// FUNCTION: WON 0x007eb320
// Sends the dirty-word filter setting to the server (flag set when the filter is off)
void RoutingLogic::HandleDirtyWordFilterChange()
{
    RoutingConnection* connection=this->GetConnection();
    if(connection==NULL) return;
    unsigned long flags=0;
    if(LobbyGetFilterDirtyWords()!=true) flags|=4;
    connection->SetClientFlags(4,flags,0,0,NULL);
}
}

namespace WONAPI {
// FUNCTION: WON 0x007ec1e0
// Returns false with a message box when the friends list already holds 255 entries
bool RoutingLogic::CanAddFriend()
{
    if(LobbyFn35D270().size()>=255) {
        LobbyMessageBox(::RoutingLogic_TooManyFriendsTitle_String,
                        ::RoutingLogic_TooManyFriendsDetails_String);
        return false;
    }
    return true;
}
// FUNCTION: WON 0x007eeed0
// Logs out of the Internet lobby: stops auth refresh, resets the room dialog and leaves the server
void RoutingLogic::Logout()
{
    AuthContext* context=GetLobbyAuthContext();
    if(context!=NULL) context->SetDoAutoRefresh(false);
    this->mWantToBeModerator=false;
    this->mCreateRoomDialog->Reset(true);
    this->ClearPendingServer();
    this->LeaveServer();
}
}

namespace WONAPI {

// FUNCTION: WON 0x007ea570
// Opens the options popup for a friend in the friends list
void RoutingLogic::ShowFriendOptions(LobbyFriend* friendInfo,int roomSpecFlags)
{
    SmartPtr<FriendOptionsPopup> popup=new FriendOptionsPopup;
    popup->Init(friendInfo,roomSpecFlags);
    LobbyOpenPopup(popup.get(),PopupFlag_StandardPopup,-1,-1);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007e7050
// (public: virtual void * __thiscall WONAPI::RoutingLogic::`scalar deleting destructor'(unsigned int)    [??_GRoutingLogic@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007e79f0
// (public: class WONAPI::SmartPtr<class WONAPI::ByteBuffer> __thiscall WONAPI::RoutingPeerDataOp::GetData(void)const     [?GetData@RoutingPeerDataOp@WONA)
// TEMPLATE: WON 0x007eae90
// (public: __thiscall WONAPI::PendingServerCleanup::~PendingServerCleanup(void)    [??1PendingServerCleanup@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x007ee8a0
// (public: virtual void __thiscall RoutingOpCompletion::Run(class WONAPI::AsyncOp *)    [?Run@RoutingOpCompletion@@UAEXPAVAsyncOp@WONAPI@@@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f1330
// (public: __thiscall std::list<struct WONAPI::RoutingBadUser,class std::allocator<struct WONAPI::RoutingBadUser> >::list<struct WONAPI::RoutingBadUser,c)
// TEMPLATE: WON 0x007f1bb0
// (public: struct std::pair<class std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned char>,struct std::map<unsigned long,unsigned ch)
// TEMPLATE: WON 0x007f30b0
// (protected: struct std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned char>,struct std::map<unsigned long,unsigned char,struct std)
// TEMPLATE: WON 0x007f3e50
// TEMPLATE: WON 0x007f3fd0
// TEMPLATE: WON 0x007f4150
// TEMPLATE: WON 0x007f42d0
// TEMPLATE: WON 0x007f4450

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007e73e0
// (public: virtual void * __thiscall WONAPI::LeftRoomEvent::`scalar deleting destructor'(unsigned int)    [??_GLeftRoomEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007e7400
// (public: virtual __thiscall WONAPI::LeftRoomEvent::~LeftRoomEvent(void)    [??1LeftRoomEvent@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007ee8d0
// (public: virtual void * __thiscall RoutingOpCompletion::`scalar deleting destructor'(unsigned int)    [??_GRoutingOpCompletion@@UAEPAXI@Z])
// TEMPLATE: WON 0x007ee8f0
// (public: virtual __thiscall RoutingOpCompletion::~RoutingOpCompletion(void)    [??1RoutingOpCompletion@@UAE@XZ])
// TEMPLATE: WON 0x007f1490
// (public: virtual void * __thiscall WONAPI::LogicCompletion<class WONAPI::RoutingLogic>::`scalar deleting destructor'(unsigned int)    [??_G?$LogicCompl)
// TEMPLATE: WON 0x007f1630
// (public: virtual __thiscall WONAPI::LogicCompletion<class WONAPI::RoutingLogic>::~LogicCompletion<class WONAPI::RoutingLogic>(void)    [??1?$LogicCompl)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007ece80
// (public: __thiscall std::multimap<unsigned short,class WONAPI::LobbyGame *,struct std::less<unsigned short>,class std::allocator<struct std::pair<unsig)
// TEMPLATE: WON 0x007f1360
// (public: __thiscall std::map<unsigned long,unsigned char,struct std::less<unsigned long>,class std::allocator<struct std::pair<unsigned long const ,uns)
// TEMPLATE: WON 0x007f1ca0
// (public: class std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned char>,struct std::_Mapk<unsigned long,unsigned char,struct std::)
// TEMPLATE: WON 0x007f1d50
// (protected: void __thiscall std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned char>,struct std::_Mapk<unsigned long,unsigned char)
// TEMPLATE: WON 0x007f1e20
// (std::_Tree::insert)
// TEMPLATE: WON 0x007f1ea0
// (std::_Tree::erase)
// TEMPLATE: WON 0x007f27f0
// (protected: struct std::_Tree<unsigned long,struct std::pair<unsigned long const ,unsigned char>,struct std::_Mapk<unsigned long,unsigned char,struct s)
// TEMPLATE: WON 0x007f2880
// (public: class std::list<unsigned long,class std::allocator<unsigned long> >::iterator __thiscall std::list<unsigned long,class std::allocator<unsigned)
// TEMPLATE: WON 0x007f28c0
// (std::_Tree::erase)
// TEMPLATE: WON 0x007f2d70
// (std::_Tree::_Erase)
// TEMPLATE: WON 0x007f2db0
// (std::_Tree::_Insert)
// TEMPLATE: WON 0x006d9f60
// (protected: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::basic_string<char,struc)
// TEMPLATE: WON 0x00769620
// (protected: struct std::list<unsigned long,class std::allocator<unsigned long> >::_Node * __thiscall std::list<unsigned long,class std::allocator<unsig)
// TEMPLATE: WON 0x006d99b0
// (public: struct std::pair<class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::basic_str)
