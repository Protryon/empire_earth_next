// RoutingLogic: internet play over WON routing servers (rooms, games, friends, chat, moderation)
#ifndef EE_WONAPI_WONLOBBYROUTINGLOGIC_H
#define EE_WONAPI_WONLOBBYROUTINGLOGIC_H
#include <time.h>
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyEvents.h"
class RoutingOpCompletion;
namespace WONAPI {
class AcceptClientEvent;
class ClientActionEvent;
class FriendActionEvent;
class JumpToRoomEvent;
class CreateRoomEvent;
class JoinRoomEvent;
class JoinFriendsRoomEvent;
class QuickPlayEvent;
class LobbyGameList;
class AddFriendDialog;
class AdminActionDialog;
class BadUserDialog;
class CreateRoomDialog;
class RoomDialog;
class PasswordDialog;
// RTTI: RoutingLogic > NetLogic > LobbyLogic > RefCount; vtable 0x855a60.
// Members run from 0x40 to the last SmartPtr at 0x88; 0x8c bytes.
class RoutingLogic : public NetLogic {
public:
    virtual void InitComponent(ComponentConfig* inConfig);
    RoutingLogic();
    virtual ~RoutingLogic();
    virtual bool HostGame(LobbyGame* inGame);
    virtual bool JoinGame(LobbyGame* inGame);
    void ClearPendingServer();
    int GetRoomSpec(unsigned short inGroupId);
    int GetRoomSpec(LobbyGroup* inGroup, LobbyServer* inServer);
    LobbyClientList* GetClientList();
    bool IsConnected();
    RoutingConnection* GetConnection();
    LobbyGroup* GetGroup(int inRoomSpec);
    LobbyServer* GetServer(const IPAddr& inAddress);
    LobbyServer* GetServer(RoutingOp* inOp);
    LobbyServer* GetDefaultServer();
    bool IsConnecting();
    virtual LobbyClient* GetMyClient();
    virtual void SendToCaptain(const ByteBuffer* inData);
    virtual void BroadcastToGame(const ByteBuffer* inData);
    virtual void SendToClient(LobbyClient* inClient, const ByteBuffer* inData);
    virtual void AdvertiseGame(LobbyGame* inGame);
    virtual void RemoveGameAdvertisement(LobbyGame*);
    virtual void KickClient(LobbyClient* inClient, bool inBan);
    virtual void SetGameInProgress(bool inClose);
    void MemberFlagsChanged(RoutingOp* inBaseOp, LobbyServer* inServer);
    void ClientFlagsChanged(RoutingOp* inBaseOp, LobbyServer* inServer);
    bool ShouldLocateFriend(LobbyFriend* inFriend, unsigned long inNow);
    bool AnnounceClientLocation(LobbyClient* inClient, bool inForce, int inRoomSpec, bool inFindByUserId);
    void PeerData(RoutingOp* inBaseOp, LobbyServer* inServer);
    void ClientJoinedGroup(RoutingOp* inBaseOp, LobbyServer* inServer);
    void ClientLeftGroup(RoutingOp* inBaseOp, LobbyServer* inServer);
    void GroupCreated(RoutingOp* inBaseOp, LobbyServer* inServer);
    void GroupDeleted(RoutingOp* inBaseOp, LobbyServer* inServer);
    void UpdateFriend(LobbyClient* inClient);
    void AnnounceFriendLocation(LobbyFriend* inFriend, int inRoomSpec);
    void ClientJoinedServer(RoutingOp* inBaseOp, LobbyServer* inServer);
    void ClientLeftServer(RoutingOp* inBaseOp, LobbyServer* inServer);
    void GroupInvitation(RoutingOp* inBaseOp, LobbyServer* inServer);
    void GroupJoinAttempt(RoutingOp* inBaseOp, LobbyServer* inServer);
    virtual void Reset();
    void PeerChat(RoutingOp* inBaseOp, LobbyServer* inServer);
    void ServerAlert(RoutingOp* inBaseOp, LobbyServer*);
    void ServerShutdownStarted(RoutingOp* inBaseOp, LobbyServer*);
    void ServerShutdownAborted(RoutingOp* inBaseOp, LobbyServer*);
    void YouWereMuted(RoutingOp* inBaseOp, LobbyServer* inServer);
    virtual void LeaveGame(LobbyGame*);
    virtual void Suspend();
    virtual void Resume();
    void LeaveServer();
    void GroupCaptainChanged(RoutingOp* inBaseOp, LobbyServer* inServer);
    void DataObjectDeleted(RoutingOp* inBaseOp, LobbyServer* inServer);
    void DataObjectCreated(RoutingOp* inBaseOp, LobbyServer* inServer);
    void YouWereBanned(RoutingOp* inBaseOp, LobbyServer* inServer);
    bool JoinDefaultRoom(bool inShowStatus);
    void RefreshGameList();
    void JoinedGroup(unsigned short inGroupId, bool inGameGroup);
    void ReadGameListComplete(RoutingOp* inBaseOp, LobbyServer* inServer);
    void AcceptClient(AcceptClientEvent* inEvent);
    void RemoveFriend(LobbyFriend* inFriend);
    void FriendAction(FriendActionEvent* inEvent);
    LobbyGame* SelectGame(int inGameType, unsigned short inPingLimit);
    void JumpToRoom(JumpToRoomEvent* inEvent);
    void QuickPlay(QuickPlayEvent* inEvent);
    void ClientAction(ClientActionEvent* inEvent);
    bool InviteClient(LobbyClient* inClient, bool inInvite, const GUIString& inComment);
    void ChatInput(Component* inTarget);
    void ConnectionClosed(AsyncOp* inBaseOp);
    void CreateRoom(CreateRoomEvent* inEvent);
    bool JoinChatRoom(LobbyServer* inServer, unsigned short inGroupId);
    void JoinRoom(JoinRoomEvent* inEvent);
    void JoinFriendsRoom(JoinFriendsRoomEvent* inEvent);
    bool CanAddFriend(); // 0x7ec1e0
    void QueryServerGroupsDone(AsyncOp*); // 0x7e9e60; group query completion
    void HandleQueryServerGroups(QueryServerGroupsEvent*); // 0x7ed4c0; starts a server group query
    void AddFriend(); // 0x7ec210; add-friend dialog and request
    void EnterInternetScreen(bool); // 0x7ed430; enters the internet screen
    void RefreshServerList(); // 0x7e9f60; refreshes the server list from the directory
    void DoRoomDialog(); // 0x7e9a90
    void EditBadUserList(bool, bool, int); // 0x7e9ab0; muted/banned user list dialog
    void DoAdminAction(int, int, LobbyClient*); // 0x7e9440; admin action dialog
    void ShowClientOptions(LobbyClient*, int);
    void ShowFriendOptions(LobbyFriend*, int);
    void GetUserStatusComplete(AsyncOp*); // 0x7e7460; user status query completion
    void LocateFriend(LobbyFriend*, int); // locates one friend
    void LocateFriends(); // locates every friend
    bool ConnectToServer(LobbyServer*, bool); // 0x7edd70
    bool CreateGroup(LobbyServer*, const GUIString&, const GUIString&,
                     int, unsigned short&); // 0x7ed670
    bool JoinGroup(LobbyServer*, int); // 0x7ed8b0

    virtual void Logout();    // 0x7eeed0
    void HandleAnonymousModeChange(); // 0x7eb2f0; updates the anonymous flag
    void HandleDirtyWordFilterChange(); // 0x7eb320; updates the dirty-word filter flag
    virtual void HandleControlEvent(int); // 0x7ea310
    virtual void HandleComponentEvent(ComponentEvent*); // 0x7ea3a0
    time_t mLastServerRefresh; // 0x40, time(); compared signed at 0x7e9fb2; zeroed by the constructor
    std::string mGameDataType; // 0x44, "G" from the constructor
    SmartPtr<LobbyServerList> mServerList; // 0x54
    SmartPtr<LobbyGameList> mGameList; // 0x58
    bool mSuppressIdle; // 0x5c
    bool mIdle; // 0x5d
    bool mHaveGameList; // 0x5e
    bool mWantToBeModerator; // 0x5f; set by ChatInput (0x7eb688) on a moderator request
    SmartPtr<AsyncOp> mGameListWaitOp; // 0x60, an AsyncOp (0x44 bytes) allocated at 0x7ecf30
    SmartPtr<LobbyServer> mCurrentServer; // 0x64
    SmartPtr<LobbyServer> mPendingServer; // 0x68
    SmartPtr<LobbyGroup> mChatGroup; // 0x6c
    SmartPtr<LobbyGroup> mGameGroup; // 0x70
    SmartPtr<AddFriendDialog> mAddFriendDialog; // 0x74; type descriptor 0x8e1608
    SmartPtr<AdminActionDialog> mAdminActionDialog; // 0x78; type descriptor 0x8e1630
    SmartPtr<BadUserDialog> mBadUserDialog; // 0x7c; type descriptor 0x8e1658
    SmartPtr<CreateRoomDialog> mCreateRoomDialog; // 0x80; type descriptor 0x8e1680
    SmartPtr<RoomDialog> mRoomDialog; // 0x84; type descriptor 0x8e16a8
    SmartPtr<PasswordDialog> mPasswordDialog; // 0x88; type descriptor 0x8e15e0
};
}
#endif
