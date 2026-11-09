// Lobby events: constructors, destructors and flag tests of the events the lobby screens and logic exchange
// (file name inferred: no __FILE__)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyEvents.h"
namespace WONAPI {
// FUNCTION: WON 0x007593a0
// Creates a change event of the given type that reports an add, delete or modify
LobbyChangeEvent::LobbyChangeEvent(int inType, int inChangeKind)
    : LobbyEvent(inType), mChangeKind(inChangeKind) {}
// FUNCTION: WON 0x00759450
// Returns true when the change is an addition
bool LobbyChangeEvent::IsAdd() { return this->mChangeKind == 0; }
// FUNCTION: WON 0x00759460
// Returns true when the change is a removal
bool LobbyChangeEvent::IsDelete() { return this->mChangeKind == 1; }
// FUNCTION: WON 0x00759470
// Returns true when the change is a modification
bool LobbyChangeEvent::IsModify() { return this->mChangeKind == 2; }
// FUNCTION: WON 0x00759480
// Creates the captain's accept or decline answer for a player asking to join
AcceptClientEvent::AcceptClientEvent(LobbyClient* inClient, bool inAccepted)
    : LobbyEvent(LobbyEvent_AcceptClient), mClient(inClient) { this->mAccepted = inAccepted; }
// FUNCTION: WON 0x00759580
// Creates a server alert event (alert, shutdown started or aborted) with its text
AlertEvent::AlertEvent(int inKind, int inValue, int inSeverity, const GUIString& inText)
    : LobbyEvent(LobbyEvent_Alert), mKind(0)
{
    this->mKind = inKind;
    this->mValue = inValue;
    this->mSeverity = inSeverity;
    this->mText = inText;
}
// FUNCTION: WON 0x00759710
// Creates the chat notice that tells which chat and game rooms a player is in
AnnounceClientLocationEvent::AnnounceClientLocationEvent(LobbyClient* inClient,
    const GUIString& inServer, const GUIString& inRoom, int inRoomId)
    : LobbyEvent(LobbyEvent_AnnounceClientLocation), mClient(inClient), mServer(inServer), mRoom(inRoom),
      mRoomId(inRoomId) { this->mHandled = false; }
// FUNCTION: WON 0x00759890
// Creates the chat notice for a friend coming online, leaving or being located
AnnounceFriendEvent::AnnounceFriendEvent(LobbyFriend* inFriend, int inLocation, int inChangeKind)
    : LobbyChangeEvent(LobbyEvent_AnnounceFriend, inChangeKind), mFriend(inFriend), mLocation(inLocation) { this->mHandled = false; }
// FUNCTION: WON 0x00759ea0
// Creates a friends-list action request for one friend
FriendActionEvent::FriendActionEvent(int inAction, LobbyFriend* inFriend, int inValue)
    : LobbyEvent(LobbyEvent_FriendAction), mFriend(inFriend)
{ this->mAction = inAction; this->mValue = inValue; }
// FUNCTION: WON 0x00759fa0
// Creates the notice that a friend was added, removed or changed
FriendChangedEvent::FriendChangedEvent(LobbyFriend* inFriend, int inChangeKind)
    : LobbyChangeEvent(LobbyEvent_FriendChanged, inChangeKind), mFriend(inFriend) {}
// FUNCTION: WON 0x0075a0a0
// Creates a game-list action request for one game
GameActionEvent::GameActionEvent(int inAction, LobbyGame* inGame)
    : LobbyEvent(LobbyEvent_GameAction), mGame(inGame) { this->mAction = inAction; }
// FUNCTION: WON 0x0075a1a0
// Creates the notice that a game was added to, removed from or changed in the list
GameChangedEvent::GameChangedEvent(LobbyGame* inGame, int inChangeKind)
    : LobbyChangeEvent(LobbyEvent_GameChanged, inChangeKind), mGame(inGame) {}
// FUNCTION: WON 0x0075a7b0
// Creates the request to join the room a friend is in
JoinFriendsRoomEvent::JoinFriendsRoomEvent(LobbyFriend* inFriend)
    : LobbyEvent(LobbyEvent_JoinFriendsRoom), mFriend(inFriend) {}
// FUNCTION: WON 0x0075a8b0
// Creates the request to join a game
JoinGameEvent::JoinGameEvent(LobbyGame* inGame)
    : LobbyEvent(LobbyEvent_JoinGame), mGame(inGame) {}
// FUNCTION: WON 0x0075ab30
// Creates the request to ping a game's host
PingGameEvent::PingGameEvent(LobbyGame* inGame, bool inFlag)
    : LobbyEvent(LobbyEvent_PingGame), mGame(inGame) { this->mFlag = inFlag; }
// FUNCTION: WON 0x0075ac30
// Creates the notice that a staging player was added, removed or changed
PlayerChangedEvent::PlayerChangedEvent(LobbyClient* inClient, int inChangeKind)
    : LobbyChangeEvent(LobbyEvent_PlayerChanged, inChangeKind), mClient(inClient) {}
// FUNCTION: WON 0x0075ad30
// Creates the notice that a staging player's team changed
PlayerTeamChangedEvent::PlayerTeamChangedEvent(LobbyClient* inClient, int inChangeKind)
    : LobbyChangeEvent(LobbyEvent_PlayerTeamChanged, inChangeKind), mClient(inClient) {}
// FUNCTION: WON 0x0075aff0
// Creates the notice that a lobby server was added, removed or changed
ServerChangedEvent::ServerChangedEvent(LobbyServer* inServer, int inChangeKind)
    : LobbyChangeEvent(LobbyEvent_ServerChanged, inChangeKind), mServer(inServer) {}
// FUNCTION: WON 0x0075a2a0
// Creates the notice that an HTTP download finished, holding its operation
GotHTTPDocumentEvent::GotHTTPDocumentEvent(GetHTTPDocumentOp* inOp)
    : LobbyEvent(LobbyEvent_GotHTTPDocument), mOp(inOp) {}
// FUNCTION: WON 0x00759990
// Creates the notice that a Direct Connect game was added, removed or changed
ChangeDirectConnectEvent::ChangeDirectConnectEvent(int inChangeKind, LobbyGame* inGame)
    : LobbyChangeEvent(LobbyEvent_ChangeDirectConnect, inChangeKind), mGame(inGame) {}
// FUNCTION: WON 0x00759a90
// Creates a player-list action request for one player in a room
ClientActionEvent::ClientActionEvent(int inAction, const int& inRoomSpec, LobbyClient* inClient)
    : LobbyEvent(LobbyEvent_ClientAction), mRoomSpec(inRoomSpec), mClient(inClient)
{ this->mAction = inAction; }
// FUNCTION: WON 0x00759b90
// Creates the notice that a player joined, left or changed in a room
ClientChangedEvent::ClientChangedEvent(LobbyClient* inClient, const int& inRoomSpec, int inChangeKind)
    : LobbyChangeEvent(LobbyEvent_ClientChanged, inChangeKind), mClient(inClient), mRoomSpec(inRoomSpec) {}
// FUNCTION: WON 0x00759c90
// Creates the notice that a player was muted, kicked or banned, with room, duration and comment
ClientModeratedEvent::ClientModeratedEvent(LobbyClient* inClient, int inRoomSpec, const GUIString& inRoomName,
    int inModerationKind, unsigned long inDuration, bool inFlag, const GUIString& inComment)
    : LobbyEvent(LobbyEvent_ClientModerated), mClient(inClient), mRoomSpec(0), mRoomName(inRoomName)
{
    this->mRoomSpec = inRoomSpec;
    this->mDuration = inDuration;
    this->mModerationKind = inModerationKind;
    this->mFlag = inFlag;
    this->mComment = inComment;
}
// FUNCTION: WON 0x0075a3a0
// Creates the notice that a room on a server was added, removed or changed
GroupChangedEvent::GroupChangedEvent(LobbyGroup* inGroup, LobbyServer* inServer, int inChangeKind)
    : LobbyChangeEvent(LobbyEvent_GroupChanged, inChangeKind), mServer(inServer), mGroup(inGroup) {}
// FUNCTION: WON 0x0075a4e0
// Creates the notice of a game invitation sent or received, with its comment
GroupInvitationEvent::GroupInvitationEvent(int inKind, const GUIString& inGameName, unsigned short inGameId,
    LobbyClient* inClient, bool inInvite, const GUIString& inComment, bool inFlag)
    : LobbyEvent(LobbyEvent_GroupInvitation), mKind(inKind), mGameName(inGameName), mClient(inClient), mComment(inComment)
{ this->mGameId = inGameId; this->mInvite = inInvite; this->mFlag = inFlag; }
// FUNCTION: WON 0x0075a670
// Creates the notice that a player asks to join the hosted game
GroupJoinAttemptEvent::GroupJoinAttemptEvent(int inKind, LobbyClient* inClient, const GUIString& inComment)
    : LobbyEvent(LobbyEvent_GroupJoinAttempt), mKind(inKind), mClient(inClient), mComment(inComment) {}
// FUNCTION: WON 0x0075a9b0
// Creates the notice that the local player joined a room, with its player list
JoinedRoomEvent::JoinedRoomEvent(const GUIString& inServerName, const GUIString& inGroupName,
    const int& inRoomSpec, unsigned short inClientId, LobbyClientList* inClients)
    : LobbyEvent(LobbyEvent_JoinedRoom), mClients(inClients), mServerName(inServerName), mGroupName(inGroupName), mClientId(inClientId), mRoomSpec(inRoomSpec)
{}
// FUNCTION: WON 0x0075ae30
// Creates a received chat line with its sender, flags and whisper target
RecvChatEvent::RecvChatEvent(const int& inRoomSpec, unsigned long inFlags, LobbyClient* inSender,
    const GUIString& inText, LobbyClient* inTarget)
    : LobbyEvent(LobbyEvent_RecvChat), mSender(inSender), mTarget(inTarget), mRoomSpec(inRoomSpec), mText(inText)
{ this->mFlags = inFlags; }
// FUNCTION: WON 0x0075afb0
// Returns true for a whisper (chat flag 4)
bool RecvChatEvent::IsWhisper() { return (this->mFlags & 4) ? true : false; }
// FUNCTION: WON 0x0075afc0
// Returns true for an emote line (chat flag 2)
bool RecvChatEvent::IsEmote() { return (this->mFlags & 2) ? true : false; }
// FUNCTION: WON 0x0075afd0
// Returns true for a moderator warning
bool RecvChatEvent::IsWarning() { return (this->mFlags & 16) ? true : false; }
// FUNCTION: WON 0x0075afe0
// Returns true for a team chat line
bool RecvChatEvent::IsTeam() { if (this->mFlags & 256) return true; return false; }
// FUNCTION: WON 0x007594f0
// Destroys the accept answer, releasing what it holds
AcceptClientEvent::~AcceptClientEvent() {}
// FUNCTION: WON 0x00759680
// Destroys the alert, releasing what it holds
AlertEvent::~AlertEvent() {}
// FUNCTION: WON 0x007597b0
// Destroys the location notice, releasing what it holds
AnnounceClientLocationEvent::~AnnounceClientLocationEvent() {}
// FUNCTION: WON 0x00759900
// Destroys the friend notice, releasing what it holds
AnnounceFriendEvent::~AnnounceFriendEvent() {}
// FUNCTION: WON 0x00759a00
// Destroys the Direct Connect notice, releasing what it holds
ChangeDirectConnectEvent::~ChangeDirectConnectEvent() {}
// FUNCTION: WON 0x00759b00
// Destroys the player action, releasing what it holds
ClientActionEvent::~ClientActionEvent() {}
// FUNCTION: WON 0x00759c00
// Destroys the player notice, releasing what it holds
ClientChangedEvent::~ClientChangedEvent() {}
// FUNCTION: WON 0x00759dc0
// Destroys the moderation notice, releasing what it holds
ClientModeratedEvent::~ClientModeratedEvent() {}
// FUNCTION: WON 0x00759f10
// Destroys the friend action, releasing what it holds
FriendActionEvent::~FriendActionEvent() {}
// FUNCTION: WON 0x0075a010
// Destroys the friend notice, releasing what it holds
FriendChangedEvent::~FriendChangedEvent() {}
// FUNCTION: WON 0x0075a110
// Destroys the game action, releasing what it holds
GameActionEvent::~GameActionEvent() {}
// FUNCTION: WON 0x0075a210
// Destroys the game notice, releasing what it holds
GameChangedEvent::~GameChangedEvent() {}
// FUNCTION: WON 0x0075a310
// Destroys the download notice, releasing what it holds
GotHTTPDocumentEvent::~GotHTTPDocumentEvent() {}
// FUNCTION: WON 0x0075a430
// Destroys the room notice, releasing what it holds
GroupChangedEvent::~GroupChangedEvent() {}
// FUNCTION: WON 0x0075a590
// Destroys the invitation notice, releasing what it holds
GroupInvitationEvent::~GroupInvitationEvent() {}
// FUNCTION: WON 0x0075a700
// Destroys the join request, releasing what it holds
GroupJoinAttemptEvent::~GroupJoinAttemptEvent() {}
// FUNCTION: WON 0x0075a820
// Destroys the join request, releasing what it holds
JoinFriendsRoomEvent::~JoinFriendsRoomEvent() {}
// FUNCTION: WON 0x0075a920
// Destroys the join request, releasing what it holds
JoinGameEvent::~JoinGameEvent() {}
// FUNCTION: WON 0x0075aa50
// Destroys the joined-room notice, releasing what it holds
JoinedRoomEvent::~JoinedRoomEvent() {}
// FUNCTION: WON 0x0075aba0
// Destroys the ping request, releasing what it holds
PingGameEvent::~PingGameEvent() {}
// FUNCTION: WON 0x0075aca0
// Destroys the player notice, releasing what it holds
PlayerChangedEvent::~PlayerChangedEvent() {}
// FUNCTION: WON 0x0075ada0
// Destroys the team notice, releasing what it holds
PlayerTeamChangedEvent::~PlayerTeamChangedEvent() {}
// FUNCTION: WON 0x0075aed0
// Destroys the chat line, releasing what it holds
RecvChatEvent::~RecvChatEvent() {}
// FUNCTION: WON 0x0075b060
// Destroys the server notice, releasing what it holds
ServerChangedEvent::~ServerChangedEvent() {}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007594d0
// (public: virtual void * __thiscall WONAPI::AcceptClientEvent::`scalar deleting destructor'(unsigned int)    [??_GAcceptClientEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00759660
// (public: virtual void * __thiscall WONAPI::AlertEvent::`scalar deleting destructor'(unsigned int)    [??_GAlertEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00759790
// (public: virtual void * __thiscall WONAPI::AnnounceClientLocationEvent::`scalar deleting destructor'(unsigned int)    [??_GAnnounceClientLocationEvent@)
// TEMPLATE: WON 0x007598e0
// (public: virtual void * __thiscall WONAPI::AnnounceFriendEvent::`scalar deleting destructor'(unsigned int)    [??_GAnnounceFriendEvent@WONAPI@@UAEPAXI@)
// TEMPLATE: WON 0x007599e0
// (public: virtual void * __thiscall WONAPI::ChangeDirectConnectEvent::`scalar deleting destructor'(unsigned int)    [??_GChangeDirectConnectEvent@WONAPI)
// TEMPLATE: WON 0x00759ae0
// (public: virtual void * __thiscall WONAPI::ClientActionEvent::`scalar deleting destructor'(unsigned int)    [??_GClientActionEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00759be0
// (public: virtual void * __thiscall WONAPI::ClientChangedEvent::`scalar deleting destructor'(unsigned int)    [??_GClientChangedEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00759da0
// (public: virtual void * __thiscall WONAPI::ClientModeratedEvent::`scalar deleting destructor'(unsigned int)    [??_GClientModeratedEvent@WONAPI@@UAEPAX)
// TEMPLATE: WON 0x00759ef0
// (public: virtual void * __thiscall WONAPI::FriendActionEvent::`scalar deleting destructor'(unsigned int)    [??_GFriendActionEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00759ff0
// (public: virtual void * __thiscall WONAPI::FriendChangedEvent::`scalar deleting destructor'(unsigned int)    [??_GFriendChangedEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x0075a0f0
// (public: virtual void * __thiscall WONAPI::GameActionEvent::`scalar deleting destructor'(unsigned int)    [??_GGameActionEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x0075a1f0
// (public: virtual void * __thiscall WONAPI::GameChangedEvent::`scalar deleting destructor'(unsigned int)    [??_GGameChangedEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x0075a2f0
// (public: virtual void * __thiscall WONAPI::GotHTTPDocumentEvent::`scalar deleting destructor'(unsigned int)    [??_GGotHTTPDocumentEvent@WONAPI@@UAEPAX)
// TEMPLATE: WON 0x0075a410
// (public: virtual void * __thiscall WONAPI::GroupChangedEvent::`scalar deleting destructor'(unsigned int)    [??_GGroupChangedEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x0075a570
// (public: virtual void * __thiscall WONAPI::GroupInvitationEvent::`scalar deleting destructor'(unsigned int)    [??_GGroupInvitationEvent@WONAPI@@UAEPAX)
// TEMPLATE: WON 0x0075a6e0
// (public: virtual void * __thiscall WONAPI::GroupJoinAttemptEvent::`scalar deleting destructor'(unsigned int)    [??_GGroupJoinAttemptEvent@WONAPI@@UAEP)
// TEMPLATE: WON 0x0075a800
// (public: virtual void * __thiscall WONAPI::JoinFriendsRoomEvent::`scalar deleting destructor'(unsigned int)    [??_GJoinFriendsRoomEvent@WONAPI@@UAEPAX)
// TEMPLATE: WON 0x0075a900
// (public: virtual void * __thiscall WONAPI::JoinGameEvent::`scalar deleting destructor'(unsigned int)    [??_GJoinGameEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x0075aa30
// (public: virtual void * __thiscall WONAPI::JoinedRoomEvent::`scalar deleting destructor'(unsigned int)    [??_GJoinedRoomEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x0075ab80
// (public: virtual void * __thiscall WONAPI::PingGameEvent::`scalar deleting destructor'(unsigned int)    [??_GPingGameEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x0075ac80
// (public: virtual void * __thiscall WONAPI::PlayerChangedEvent::`scalar deleting destructor'(unsigned int)    [??_GPlayerChangedEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x0075ad80
// (public: virtual void * __thiscall WONAPI::PlayerTeamChangedEvent::`scalar deleting destructor'(unsigned int)    [??_GPlayerTeamChangedEvent@WONAPI@@UA)
// TEMPLATE: WON 0x0075aeb0
// (public: virtual void * __thiscall WONAPI::RecvChatEvent::`scalar deleting destructor'(unsigned int)    [??_GRecvChatEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x0075b040
// (public: virtual void * __thiscall WONAPI::ServerChangedEvent::`scalar deleting destructor'(unsigned int)    [??_GServerChangedEvent@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007593d0
// (public: virtual void * __thiscall WONAPI::LobbyChangeEvent::`scalar deleting destructor'(unsigned int)    [??_GLobbyChangeEvent@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007593f0
// (public: virtual __thiscall WONAPI::LobbyChangeEvent::~LobbyChangeEvent(void)    [??1LobbyChangeEvent@WONAPI@@UAE@XZ])
