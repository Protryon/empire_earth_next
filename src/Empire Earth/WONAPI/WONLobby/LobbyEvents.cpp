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
// Creates a change event of the given type that reports an add, delete or modify
LobbyChangeEvent::LobbyChangeEvent(int inType, int inChangeKind)
    : LobbyEvent(inType), mChangeKind(inChangeKind) {}
// Returns true when the change is an addition
bool LobbyChangeEvent::IsAdd() { return this->mChangeKind == 0; }
// Returns true when the change is a removal
bool LobbyChangeEvent::IsDelete() { return this->mChangeKind == 1; }
// Returns true when the change is a modification
bool LobbyChangeEvent::IsModify() { return this->mChangeKind == 2; }
// Creates the captain's accept or decline answer for a player asking to join
AcceptClientEvent::AcceptClientEvent(LobbyClient* inClient, bool inAccepted)
    : LobbyEvent(LobbyEvent_AcceptClient), mClient(inClient) { this->mAccepted = inAccepted; }
// Creates a server alert event (alert, shutdown started or aborted) with its text
AlertEvent::AlertEvent(int inKind, int inValue, int inSeverity, const GUIString& inText)
    : LobbyEvent(LobbyEvent_Alert), mKind(0)
{
    this->mKind = inKind;
    this->mValue = inValue;
    this->mSeverity = inSeverity;
    this->mText = inText;
}
// Creates the chat notice that tells which chat and game rooms a player is in
AnnounceClientLocationEvent::AnnounceClientLocationEvent(LobbyClient* inClient,
    const GUIString& inServer, const GUIString& inRoom, int inRoomId)
    : LobbyEvent(LobbyEvent_AnnounceClientLocation), mClient(inClient), mServer(inServer), mRoom(inRoom),
      mRoomId(inRoomId) { this->mHandled = false; }
// Creates the chat notice for a friend coming online, leaving or being located
AnnounceFriendEvent::AnnounceFriendEvent(LobbyFriend* inFriend, int inLocation, int inChangeKind)
    : LobbyChangeEvent(LobbyEvent_AnnounceFriend, inChangeKind), mFriend(inFriend), mLocation(inLocation) { this->mHandled = false; }
// Creates a friends-list action request for one friend
FriendActionEvent::FriendActionEvent(int inAction, LobbyFriend* inFriend, int inValue)
    : LobbyEvent(LobbyEvent_FriendAction), mFriend(inFriend)
{ this->mAction = inAction; this->mValue = inValue; }
// Creates the notice that a friend was added, removed or changed
FriendChangedEvent::FriendChangedEvent(LobbyFriend* inFriend, int inChangeKind)
    : LobbyChangeEvent(LobbyEvent_FriendChanged, inChangeKind), mFriend(inFriend) {}
// Creates a game-list action request for one game
GameActionEvent::GameActionEvent(int inAction, LobbyGame* inGame)
    : LobbyEvent(LobbyEvent_GameAction), mGame(inGame) { this->mAction = inAction; }
// Creates the notice that a game was added to, removed from or changed in the list
GameChangedEvent::GameChangedEvent(LobbyGame* inGame, int inChangeKind)
    : LobbyChangeEvent(LobbyEvent_GameChanged, inChangeKind), mGame(inGame) {}
// Creates the request to join the room a friend is in
JoinFriendsRoomEvent::JoinFriendsRoomEvent(LobbyFriend* inFriend)
    : LobbyEvent(LobbyEvent_JoinFriendsRoom), mFriend(inFriend) {}
// Creates the request to join a game
JoinGameEvent::JoinGameEvent(LobbyGame* inGame)
    : LobbyEvent(LobbyEvent_JoinGame), mGame(inGame) {}
// Creates the request to ping a game's host
PingGameEvent::PingGameEvent(LobbyGame* inGame, bool inFlag)
    : LobbyEvent(LobbyEvent_PingGame), mGame(inGame) { this->mFlag = inFlag; }
// Creates the notice that a staging player was added, removed or changed
PlayerChangedEvent::PlayerChangedEvent(LobbyClient* inClient, int inChangeKind)
    : LobbyChangeEvent(LobbyEvent_PlayerChanged, inChangeKind), mClient(inClient) {}
// Creates the notice that a staging player's team changed
PlayerTeamChangedEvent::PlayerTeamChangedEvent(LobbyClient* inClient, int inChangeKind)
    : LobbyChangeEvent(LobbyEvent_PlayerTeamChanged, inChangeKind), mClient(inClient) {}
// Creates the notice that a lobby server was added, removed or changed
ServerChangedEvent::ServerChangedEvent(LobbyServer* inServer, int inChangeKind)
    : LobbyChangeEvent(LobbyEvent_ServerChanged, inChangeKind), mServer(inServer) {}
// Creates the notice that an HTTP download finished, holding its operation
GotHTTPDocumentEvent::GotHTTPDocumentEvent(RustServerOp* inOp)
    : LobbyEvent(LobbyEvent_GotHTTPDocument), mOp(inOp) {}
// Creates the notice that a Direct Connect game was added, removed or changed
ChangeDirectConnectEvent::ChangeDirectConnectEvent(int inChangeKind, LobbyGame* inGame)
    : LobbyChangeEvent(LobbyEvent_ChangeDirectConnect, inChangeKind), mGame(inGame) {}
// Creates a player-list action request for one player in a room
ClientActionEvent::ClientActionEvent(int inAction, const int& inRoomSpec, LobbyClient* inClient)
    : LobbyEvent(LobbyEvent_ClientAction), mRoomSpec(inRoomSpec), mClient(inClient)
{ this->mAction = inAction; }
// Creates the notice that a player joined, left or changed in a room
ClientChangedEvent::ClientChangedEvent(LobbyClient* inClient, const int& inRoomSpec, int inChangeKind)
    : LobbyChangeEvent(LobbyEvent_ClientChanged, inChangeKind), mClient(inClient), mRoomSpec(inRoomSpec) {}
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
// Creates the notice that a room on a server was added, removed or changed
GroupChangedEvent::GroupChangedEvent(LobbyGroup* inGroup, LobbyServer* inServer, int inChangeKind)
    : LobbyChangeEvent(LobbyEvent_GroupChanged, inChangeKind), mServer(inServer), mGroup(inGroup) {}
// Creates the notice of a game invitation sent or received, with its comment
GroupInvitationEvent::GroupInvitationEvent(int inKind, const GUIString& inGameName, unsigned short inGameId,
    LobbyClient* inClient, bool inInvite, const GUIString& inComment, bool inFlag)
    : LobbyEvent(LobbyEvent_GroupInvitation), mKind(inKind), mGameName(inGameName), mClient(inClient), mComment(inComment)
{ this->mGameId = inGameId; this->mInvite = inInvite; this->mFlag = inFlag; }
// Creates the notice that a player asks to join the hosted game
GroupJoinAttemptEvent::GroupJoinAttemptEvent(int inKind, LobbyClient* inClient, const GUIString& inComment)
    : LobbyEvent(LobbyEvent_GroupJoinAttempt), mKind(inKind), mClient(inClient), mComment(inComment) {}
// Creates the notice that the local player joined a room, with its player list
JoinedRoomEvent::JoinedRoomEvent(const GUIString& inServerName, const GUIString& inGroupName,
    const int& inRoomSpec, unsigned short inClientId, LobbyClientList* inClients)
    : LobbyEvent(LobbyEvent_JoinedRoom), mClients(inClients), mServerName(inServerName), mGroupName(inGroupName), mClientId(inClientId), mRoomSpec(inRoomSpec)
{}
// Creates a received chat line with its sender, flags and whisper target
RecvChatEvent::RecvChatEvent(const int& inRoomSpec, unsigned long inFlags, LobbyClient* inSender,
    const GUIString& inText, LobbyClient* inTarget)
    : LobbyEvent(LobbyEvent_RecvChat), mSender(inSender), mTarget(inTarget), mRoomSpec(inRoomSpec), mText(inText)
{ this->mFlags = inFlags; }
// Returns true for a whisper (chat flag 4)
bool RecvChatEvent::IsWhisper() { return (this->mFlags & 4) ? true : false; }
// Returns true for an emote line (chat flag 2)
bool RecvChatEvent::IsEmote() { return (this->mFlags & 2) ? true : false; }
// Returns true for a moderator warning
bool RecvChatEvent::IsWarning() { return (this->mFlags & 16) ? true : false; }
// Returns true for a team chat line
bool RecvChatEvent::IsTeam() { if (this->mFlags & 256) return true; return false; }
// Destroys the accept answer, releasing what it holds
AcceptClientEvent::~AcceptClientEvent() {}
// Destroys the alert, releasing what it holds
AlertEvent::~AlertEvent() {}
// Destroys the location notice, releasing what it holds
AnnounceClientLocationEvent::~AnnounceClientLocationEvent() {}
// Destroys the friend notice, releasing what it holds
AnnounceFriendEvent::~AnnounceFriendEvent() {}
// Destroys the Direct Connect notice, releasing what it holds
ChangeDirectConnectEvent::~ChangeDirectConnectEvent() {}
// Destroys the player action, releasing what it holds
ClientActionEvent::~ClientActionEvent() {}
// Destroys the player notice, releasing what it holds
ClientChangedEvent::~ClientChangedEvent() {}
// Destroys the moderation notice, releasing what it holds
ClientModeratedEvent::~ClientModeratedEvent() {}
// Destroys the friend action, releasing what it holds
FriendActionEvent::~FriendActionEvent() {}
// Destroys the friend notice, releasing what it holds
FriendChangedEvent::~FriendChangedEvent() {}
// Destroys the game action, releasing what it holds
GameActionEvent::~GameActionEvent() {}
// Destroys the game notice, releasing what it holds
GameChangedEvent::~GameChangedEvent() {}
// Destroys the download notice, releasing what it holds
GotHTTPDocumentEvent::~GotHTTPDocumentEvent() {}
// Destroys the room notice, releasing what it holds
GroupChangedEvent::~GroupChangedEvent() {}
// Destroys the invitation notice, releasing what it holds
GroupInvitationEvent::~GroupInvitationEvent() {}
// Destroys the join request, releasing what it holds
GroupJoinAttemptEvent::~GroupJoinAttemptEvent() {}
// Destroys the join request, releasing what it holds
JoinFriendsRoomEvent::~JoinFriendsRoomEvent() {}
// Destroys the join request, releasing what it holds
JoinGameEvent::~JoinGameEvent() {}
// Destroys the joined-room notice, releasing what it holds
JoinedRoomEvent::~JoinedRoomEvent() {}
// Destroys the ping request, releasing what it holds
PingGameEvent::~PingGameEvent() {}
// Destroys the player notice, releasing what it holds
PlayerChangedEvent::~PlayerChangedEvent() {}
// Destroys the team notice, releasing what it holds
PlayerTeamChangedEvent::~PlayerTeamChangedEvent() {}
// Destroys the chat line, releasing what it holds
RecvChatEvent::~RecvChatEvent() {}
// Destroys the server notice, releasing what it holds
ServerChangedEvent::~ServerChangedEvent() {}
}
