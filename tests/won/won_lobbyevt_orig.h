// The lobby's event classes as they were at 8d68d2786 (include/ee/wonapi/WONLobbyEvents.h, src/Empire Earth/
// WONAPI/WONLobby/LobbyEvents.cpp and LobbySyncGameEvent.cpp), copied unchanged but for their includes and the
// model's forward declarations, in namespace Orig over won_lobbyevt_test.cpp's world (taken with git show 8d68d2786:<path>).
namespace Orig {
using namespace WONAPI;
// RTTI 0x851af0; LobbyEvent is 0x20 bytes, the change kind is at 0x20, 0x24 bytes in all.
class LobbyChangeEvent : public LobbyEvent {
public:
    LobbyChangeEvent(int inType, int inChangeKind);
    bool IsAdd();
    bool IsDelete();
    bool IsModify();                             // 0x759470; kind 2
    int mChangeKind;
};
class AcceptClientEvent : public LobbyEvent {
public:
    virtual ~AcceptClientEvent(); // explicit: writes its own vtable pointer
    AcceptClientEvent(LobbyClient* inClient, bool inAccepted);
    SmartPtr<LobbyClient> mClient; // 0x20
    bool mAccepted; // 0x24
};
class AlertEvent : public LobbyEvent {
public:
    virtual ~AlertEvent(); // explicit: writes its own vtable pointer
    AlertEvent(int inKind, int inValue, int inSeverity, const GUIString& inText);
    int GetKind() const { return this->mKind; }
    int GetValue() const { return this->mValue; }
    const GUIString& GetText() const { return this->mText; }
    int mKind; // 0x20
    int mValue; // 0x24
    int mSeverity; // 0x28
    GUIString mText; // 0x2c
};
class AnnounceClientLocationEvent : public LobbyEvent {
public:
    virtual ~AnnounceClientLocationEvent(); // explicit: writes its own vtable pointer
    AnnounceClientLocationEvent(LobbyClient* inClient, const GUIString& inServer,
        const GUIString& inRoom, int inRoomId);
    SmartPtr<LobbyClient> mClient; // 0x20
    GUIString mServer; // 0x24
    GUIString mRoom; // 0x28
    int mRoomId; // 0x2c
    bool mHandled; // 0x30
};
class AnnounceFriendEvent : public LobbyChangeEvent {
public:
    virtual ~AnnounceFriendEvent(); // explicit: writes its own vtable pointer
    AnnounceFriendEvent(LobbyFriend* inFriend, int inLocation, int inChangeKind);
    SmartPtr<LobbyFriend> mFriend; // 0x24
    int mLocation; // 0x28
    bool mHandled; // 0x2c
};
class FriendActionEvent : public LobbyEvent {
public:
    virtual ~FriendActionEvent(); // explicit: writes its own vtable pointer
    FriendActionEvent(int inAction, LobbyFriend* inFriend, int inValue);
    int mAction; // 0x20
    SmartPtr<LobbyFriend> mFriend; // 0x24
    int mValue; // 0x28
};
class FriendChangedEvent : public LobbyChangeEvent {
public:
    virtual ~FriendChangedEvent(); // explicit: writes its own vtable pointer
    FriendChangedEvent(LobbyFriend* inFriend, int inChangeKind);
    SmartPtr<LobbyFriend> mFriend; // 0x24
};
class GameActionEvent : public LobbyEvent {
public:
    virtual ~GameActionEvent(); // explicit: writes its own vtable pointer
    GameActionEvent(int inAction, LobbyGame* inGame);
    int mAction; // 0x20
    SmartPtr<LobbyGame> mGame; // 0x24
};
class GameChangedEvent : public LobbyChangeEvent {
public:
    virtual ~GameChangedEvent(); // explicit: writes its own vtable pointer
    GameChangedEvent(LobbyGame* inGame, int inChangeKind);
    SmartPtr<LobbyGame> mGame; // 0x24
};
class JoinFriendsRoomEvent : public LobbyEvent {
public:
    virtual ~JoinFriendsRoomEvent(); // explicit: writes its own vtable pointer
    JoinFriendsRoomEvent(LobbyFriend* inFriend);
    SmartPtr<LobbyFriend> mFriend; // 0x20
};
class JoinGameEvent : public LobbyEvent {
public:
    virtual ~JoinGameEvent(); // explicit: writes its own vtable pointer
    JoinGameEvent(LobbyGame* inGame);
    SmartPtr<LobbyGame> mGame; // 0x20
};
class PingGameEvent : public LobbyEvent {
public:
    virtual ~PingGameEvent(); // explicit: writes its own vtable pointer
    PingGameEvent(LobbyGame* inGame, bool inFlag);
    SmartPtr<LobbyGame> mGame; // 0x20
    bool mFlag; // 0x24
};
class PlayerChangedEvent : public LobbyChangeEvent {
public:
    virtual ~PlayerChangedEvent(); // explicit: writes its own vtable pointer
    PlayerChangedEvent(LobbyClient* inClient, int inChangeKind);
    SmartPtr<LobbyClient> mClient; // 0x24
};
class PlayerTeamChangedEvent : public LobbyChangeEvent {
public:
    virtual ~PlayerTeamChangedEvent(); // explicit: writes its own vtable pointer
    PlayerTeamChangedEvent(LobbyClient* inClient, int inChangeKind);
    SmartPtr<LobbyClient> mClient; // 0x24
};
class ServerChangedEvent : public LobbyChangeEvent {
public:
    virtual ~ServerChangedEvent(); // explicit: writes its own vtable pointer
    ServerChangedEvent(LobbyServer* inServer, int inChangeKind);
    SmartPtr<LobbyServer> mServer; // 0x24
};
class ClientActionEvent : public LobbyEvent {
public:
    virtual ~ClientActionEvent(); // explicit: writes its own vtable pointer
    ClientActionEvent(int inAction, const int& inRoomSpec, LobbyClient* inClient);
    int GetAction() const { return this->mAction; }
    int GetRoomSpec() const { return this->mRoomSpec; }
    LobbyClient* GetClient() const { return this->mClient.get(); }
    int mAction; int mRoomSpec; SmartPtr<LobbyClient> mClient; // 0x20, 0x24, 0x28
};
class GotHTTPDocumentEvent : public LobbyEvent {
public:
    virtual ~GotHTTPDocumentEvent(); // explicit: writes its own vtable pointer
    GotHTTPDocumentEvent(RustServerOp* inOp); // 0x75a2a0; used by InitLogic::HTTPDocumentCompletion (0x7de280)
    SmartPtr<RustServerOp> mOp; // 0x20; 0x24 bytes (a GetHTTPDocumentOp, httpdoc.rs)
};
class ChangeDirectConnectEvent : public LobbyChangeEvent {
public:
    virtual ~ChangeDirectConnectEvent(); // explicit: writes its own vtable pointer
    ChangeDirectConnectEvent(int inChangeKind, LobbyGame* inGame); // 0x759990; created by staging at 0x7f00c3
    SmartPtr<LobbyGame> mGame; // 0x24; 0x28 bytes
};
class ClientChangedEvent : public LobbyChangeEvent {
public:
    int GetRoomSpec() const { return this->mRoomSpec; }
    virtual ~ClientChangedEvent(); // explicit: writes its own vtable pointer
    ClientChangedEvent(LobbyClient* inClient, const int& inRoomSpec, int inChangeKind);
    SmartPtr<LobbyClient> mClient; int mRoomSpec; // 0x24, 0x28
};
class ClientModeratedEvent : public LobbyEvent {
public:
    virtual ~ClientModeratedEvent(); // explicit: writes its own vtable pointer
    ClientModeratedEvent(LobbyClient* inClient, int inRoomSpec, const GUIString& inRoomName,
        int inModerationKind, unsigned long inDuration, bool inFlag, const GUIString& inComment);
    SmartPtr<LobbyClient> mClient; int mRoomSpec; GUIString mRoomName;
    int mModerationKind; unsigned long mDuration; bool mFlag; GUIString mComment; // 0x20, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x38
};
class GroupChangedEvent : public LobbyChangeEvent {
public:
    virtual ~GroupChangedEvent(); // explicit: writes its own vtable pointer
    GroupChangedEvent(LobbyGroup* inGroup, LobbyServer* inServer, int inChangeKind);
    SmartPtr<LobbyServer> mServer; SmartPtr<LobbyGroup> mGroup; // 0x24, 0x28
};
class GroupInvitationEvent : public LobbyEvent {
public:
    virtual ~GroupInvitationEvent(); // explicit: writes its own vtable pointer
    GroupInvitationEvent(int inKind, const GUIString& inGameName, unsigned short inGameId,
        LobbyClient* inClient, bool inInvite, const GUIString& inComment, bool inFlag);
    int mKind; GUIString mGameName; unsigned short mGameId; SmartPtr<LobbyClient> mClient;
    bool mInvite; bool mFlag; GUIString mComment; // 0x20, 0x24, 0x28, 0x2c, 0x30, 0x31, 0x34
};
class GroupJoinAttemptEvent : public LobbyEvent {
public:
    virtual ~GroupJoinAttemptEvent(); // explicit: writes its own vtable pointer
    GroupJoinAttemptEvent(int inKind, LobbyClient* inClient, const GUIString& inComment);
    int mKind; SmartPtr<LobbyClient> mClient; GUIString mComment; // 0x20, 0x24, 0x28
};
class JoinedRoomEvent : public LobbyEvent {
public:
    int GetRoomSpec() const { return this->mRoomSpec; }
    virtual ~JoinedRoomEvent(); // explicit: writes its own vtable pointer
    JoinedRoomEvent(const GUIString& inServerName, const GUIString& inGroupName,
        const int& inRoomSpec, unsigned short inClientId, LobbyClientList* inClients);
    SmartPtr<LobbyClientList> mClients; GUIString mServerName; GUIString mGroupName;
    unsigned short mClientId; int mRoomSpec; // 0x20, 0x24, 0x28, 0x2c, 0x30
};
class RecvChatEvent : public LobbyEvent {
public:
    virtual ~RecvChatEvent(); // explicit: writes its own vtable pointer
    RecvChatEvent(const int& inRoomSpec, unsigned long inFlags, LobbyClient* inSender,
        const GUIString& inText, LobbyClient* inTarget);
    bool IsWhisper(); bool IsEmote(); bool IsWarning(); bool IsTeam();
    SmartPtr<LobbyClient> mSender; SmartPtr<LobbyClient> mTarget; int mRoomSpec;
    unsigned long mFlags; GUIString mText; // 0x20, 0x24, 0x28, 0x2c, 0x30
};
// Inline events below: their implicit destructors are emitted by the units that create them.
class SetRoomSpecFlagsEvent : public LobbyEvent {
public:
    SetRoomSpecFlagsEvent(int inFlags) : LobbyEvent(LobbyEvent_SetRoomSpecFlags), mFlags(inFlags) {}
    int mFlags; // 0x20; 0x24 bytes
};
class LoginInitStatusEvent : public LobbyEvent {
public:
    LoginInitStatusEvent(const GUIString& inStatus, const GUIString& inDescription,
        bool inAllowIgnore, int inKind)
        : LobbyEvent(LobbyEvent_LoginInitStatus), mStatus(inStatus), mDescription(inDescription),
          mAllowIgnore(inAllowIgnore), mKind(inKind) {}
    GUIString mStatus; // 0x20
    GUIString mDescription; // 0x24
    bool mAllowIgnore; // 0x28
    int mKind; // 0x2c
};
class SetLoginFieldsEvent : public LobbyEvent {
public:
    SetLoginFieldsEvent(const GUIString& inUser, const GUIString& inPassword, bool inRemember)
        : LobbyEvent(LobbyEvent_SetLoginFields), mUserName(inUser), mPassword(inPassword), mRemember(inRemember) {}
    GUIString mUserName; // 0x20
    GUIString mPassword; // 0x24
    bool mRemember; // 0x28
};
class QueryAccountResultEvent : public LobbyEvent {
public:
    QueryAccountResultEvent(const GUIString& inEmail, bool inNewsletter)
        : LobbyEvent(LobbyEvent_QueryAccountResult), mEmail(inEmail), mNewsletter(inNewsletter) {}
    GUIString mEmail; // 0x20
    bool mNewsletter; // 0x24
};
class LoginEvent : public LobbyEvent {
public:
    LoginEvent(const GUIString& inPassword, bool inRemember, const GUIString& inUser)
        : LobbyEvent(LobbyEvent_Login), mUserName((std::wstring)inUser),
          mPassword((std::wstring)inPassword), mRemember(inRemember) {}
    std::wstring mUserName; std::wstring mPassword; bool mRemember; // 0x20, 0x30, 0x40; 0x44 bytes
};
class GetLostPasswordEvent : public LobbyEvent {
public:
    GetLostPasswordEvent(const GUIString& inUserName)
        : LobbyEvent(LobbyEvent_GetLostPassword), mUserName((std::wstring)inUserName) {}
    std::wstring mUserName; // 0x20; 0x30 bytes; vtable 0x853a38
    // Implicit destructor 0x7c06c0, emitted by the unit that creates the event.
};
class GetLostUserNameEvent : public LobbyEvent {
public:
    GetLostUserNameEvent(const GUIString& inEmail)
        : LobbyEvent(LobbyEvent_GetLostUserName), mEmail((std::string)inEmail) {}
    std::string mEmail; // 0x20; 0x30 bytes; vtable 0x853b14
    // Implicit destructor 0x7c0ab0, emitted by the unit that creates the event.
};
class LeftRoomEvent : public LobbyEvent {
public:
    int GetRoomSpec() const { return this->mRoomSpec; }
    LeftRoomEvent(int inRoomSpec, bool inForced, bool inFlag)
        : LobbyEvent(LobbyEvent_LeftRoom), mRoomSpec(inRoomSpec), mForced(inForced), mFlag(inFlag) {}
    int mRoomSpec; bool mForced; bool mFlag; // 0x20, 0x24, 0x25; 0x28 bytes
    // Implicit destructor 0x7e7400, emitted by RoutingLogic.
};
class SetChatInputEvent : public LobbyEvent {
public:
    SetChatInputEvent(int inCommand, const GUIString& inText, int inRoomSpec, bool inHandled)
        : LobbyEvent(LobbyEvent_SetChatInput), mCommand(inCommand), mText(inText), mRoomSpec(inRoomSpec), mHandled(inHandled) {}
    int mCommand; GUIString mText; int mRoomSpec; bool mHandled; // 0x20, 0x24, 0x28, 0x2c; 0x30 bytes
    // Implicit destructor: deleting 0x7bd9c0, normal 0x7bd9e0 (135 bytes).
};
class ChatCommandErrorEvent : public LobbyEvent {
public:
    ChatCommandErrorEvent(const GUIString& inText) : LobbyEvent(LobbyEvent_ChatCommandError), mText(inText) {}
    GUIString mText; // 0x20; 0x24 bytes; vtable 0x8559e0; created inline at 0x7eb622
    // Implicit destructor 0x7e3200, emitted by the unit that creates the event.
};
class CreateRoomEvent : public LobbyEvent {
public:
    CreateRoomEvent(const IPAddr& inAddress) : LobbyEvent(LobbyEvent_CreateRoom), mAddress(inAddress) {}
    IPAddr mAddress; // 0x20; 0x48 bytes
    // Implicit destructor 0x7c2a80, emitted by the unit that creates the event.
};
class JoinRoomEvent : public LobbyEvent {
public:
    JoinRoomEvent(const IPAddr& inAddress, unsigned long inGroupId)
        : LobbyEvent(LobbyEvent_JoinRoom), mAddress(inAddress), mGroupId(inGroupId) {}
    IPAddr mAddress; unsigned long mGroupId; // 0x20, 0x48; 0x4c bytes
    // Implicit destructor 0x7c2860, emitted by the unit that creates the event.
};
// Vtable 0x852f0c; created at 0x7ba990, which builds the default strings before reading the widgets.
class CreateAccountEvent : public LobbyEvent {
public:
    CreateAccountEvent() : LobbyEvent(LobbyEvent_CreateAccount) {}
    std::wstring mUserName; // 0x20
    std::wstring mPassword; // 0x30
    std::string mEmail; // 0x40
    unsigned char mMonth; // 0x50, not initialized by the constructor
    unsigned char mDay; // 0x51
    unsigned short mYear; // 0x52
    bool mNewsletter; // 0x54; 0x58 bytes in all
    // Implicit destructor 0x7bac50, emitted by the unit that creates the event.
};
class SyncGameListEvent : public LobbyEvent {
public:
    SyncGameListEvent(LobbyGameList* inGames);
    virtual ~SyncGameListEvent(); // 0x75b160; explicit: writes its own vtable pointer
    SmartPtr<LobbyGameList> mGames; // 0x20; 0x24 bytes
};
class JumpToRoomEvent : public LobbyEvent {
public:
    JumpToRoomEvent(const GUIString& inRoomName, unsigned short inGroupId, const IPAddr& inAddress)
        : LobbyEvent(LobbyEvent_JumpToRoom), mRoomName(inRoomName), mGroupId(inGroupId), mAddress(inAddress) {}
    GUIString mRoomName; unsigned short mGroupId; IPAddr mAddress; // 0x20, 0x24, 0x28; 0x50 bytes
    // Implicit destructor 0x7b9bc0, emitted by the unit that creates the event.
};
class QuickPlayEvent : public LobbyEvent {
public:
    QuickPlayEvent(int inGameType) : LobbyEvent(LobbyEvent_QuickPlay), mGameType(inGameType) {}
    int mGameType; // 0x20; 0x24 bytes; vtable 0x853cc0; created at 0x7c1750
};
// Vtable 0x855904, event type 100049, 0x34 bytes: an unowned image pointer at 0x20 and the link at 0x24.
class SetCrossPromotionEvent : public LobbyEvent {
public:
    SetCrossPromotionEvent(Image* inImage, const std::string& inLink)
        : LobbyEvent(LobbyEvent_SetCrossPromotion), mImage(inImage), mLink(inLink) {}
    Image* mImage; // 0x20, not owned (never retained or released)
    std::string mLink; // 0x24
    // Implicit destructor 0x7ddb70.
};
// Vtable 0x854444, event type 100060, 0x2c bytes; created at 0x7ca410.
class UpdateAccountEvent : public LobbyEvent {
public:
    UpdateAccountEvent(const GUIString& inEmail, const GUIString& inNewPassword, bool inNewsletter)
        : LobbyEvent(LobbyEvent_UpdateAccount), mEmail(inEmail), mNewPassword(inNewPassword), mNewsletter(inNewsletter) {}
    GUIString mEmail; // 0x20
    GUIString mNewPassword; // 0x24
    bool mNewsletter; // 0x28
    // Implicit destructor 0x7ca5f0, emitted by the unit that creates the event.
};
// Vtable 0x853df0, event type 100042, 0x48 bytes with the IPAddr at 0x20; created at 0x7c2f00.
class QueryServerGroupsEvent : public LobbyEvent {
public:
    QueryServerGroupsEvent(const IPAddr& inAddress) : LobbyEvent(LobbyEvent_QueryServerGroups), mAddress(inAddress) {}
    IPAddr mAddress; // 0x20..0x48
    // Implicit destructor 0x7c3120, emitted by the unit that creates the event.
};
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
// Creates the notice that the whole game list was rebuilt, holding the list
SyncGameListEvent::SyncGameListEvent(LobbyGameList* games)
    : LobbyEvent(LobbyEvent_SyncGameList),mGames(games) {}
// Destroys the notice, releasing the game list
SyncGameListEvent::~SyncGameListEvent() {}
}

// the receivers' reads at 8d68d2786: LobbyNetCtrlHost.cpp's EvField (but the WONGUI event's link) and the cases of
// LobbyCtrlHost.cpp, LobbyNetCtrlHost.cpp, LanLogicHost.cpp, LobbyLogicHost.cpp and RoutingLogic.cpp's EventNum
namespace Orig {
static intptr_t Dat(const GUIString& inText) { return (intptr_t)inText.mData.get(); }
static intptr_t EvField(ComponentEvent* e, EeWonLobbyCtlEvField f)
{
    typedef EeWonLobbyCtlEvField F;
    AnnounceClientLocationEvent* acl = static_cast<AnnounceClientLocationEvent*>(e);
    AnnounceFriendEvent* af = static_cast<AnnounceFriendEvent*>(e);
    RecvChatEvent* rc = static_cast<RecvChatEvent*>(e);
    JoinedRoomEvent* jr = static_cast<JoinedRoomEvent*>(e);
    LeftRoomEvent* lr = static_cast<LeftRoomEvent*>(e);
    AlertEvent* al = static_cast<AlertEvent*>(e);
    SetChatInputEvent* sci = static_cast<SetChatInputEvent*>(e);
    GroupInvitationEvent* gi = static_cast<GroupInvitationEvent*>(e);
    GroupJoinAttemptEvent* gja = static_cast<GroupJoinAttemptEvent*>(e);
    ClientModeratedEvent* cm = static_cast<ClientModeratedEvent*>(e);
    switch (f) {
    case F::AclHandled: return acl->mHandled ? 1 : 0;
    case F::AclRoomId: return acl->mRoomId;
    case F::AclClient: return (intptr_t)acl->mClient.get();
    case F::AclRoom: return Dat(acl->mRoom);
    case F::AclServer: return Dat(acl->mServer);
    case F::AfLocation: return af->mLocation;
    case F::AfFriend: return (intptr_t)af->mFriend.get();
    case F::AfHandled: return af->mHandled ? 1 : 0;
    case F::PtcClient: return (intptr_t) static_cast<PlayerTeamChangedEvent*>(e)->mClient.get();
    case F::RcSender: return (intptr_t)rc->mSender.get();
    case F::RcTarget: return (intptr_t)rc->mTarget.get();
    case F::RcRoomSpec: return rc->mRoomSpec;
    case F::RcText: return Dat(rc->mText);
    case F::RcIsTeam: return rc->IsTeam() ? 1 : 0;
    case F::RcIsWhisper: return rc->IsWhisper() ? 1 : 0;
    case F::RcIsWarning: return rc->IsWarning() ? 1 : 0;
    case F::RcIsEmote: return rc->IsEmote() ? 1 : 0;
    case F::JrRoomSpec: return jr->mRoomSpec;
    case F::JrClientId: return jr->mClientId;
    case F::JrGroupName: return Dat(jr->mGroupName);
    case F::JrServerName: return Dat(jr->mServerName);
    case F::JrClients: return (intptr_t)jr->mClients.get();
    case F::LrRoomSpec: return lr->mRoomSpec;
    case F::LrFlag: return lr->mFlag ? 1 : 0;
    case F::LrForced: return lr->mForced ? 1 : 0;
    case F::CceText: return Dat(static_cast<ChatCommandErrorEvent*>(e)->mText);
    case F::AlKind: return al->GetKind();
    case F::AlValue: return al->GetValue();
    case F::AlSeverity: return al->mSeverity;
    case F::AlText: return Dat(al->GetText());
    case F::SciHandled: return sci->mHandled ? 1 : 0;
    case F::SciRoomSpec: return sci->mRoomSpec;
    case F::SciCommand: return sci->mCommand;
    case F::SciText: return Dat(sci->mText);
    case F::GiKind: return gi->mKind;
    case F::GiClient: return (intptr_t)gi->mClient.get();
    case F::GiGameName: return Dat(gi->mGameName);
    case F::GiGameId: return gi->mGameId;
    case F::GiComment: return Dat(gi->mComment);
    case F::GiFlag: return gi->mFlag ? 1 : 0;
    case F::GiInvite: return gi->mInvite ? 1 : 0;
    case F::GjaKind: return gja->mKind;
    case F::GjaClient: return (intptr_t)gja->mClient.get();
    case F::GjaComment: return Dat(gja->mComment);
    case F::CmRoomSpec: return cm->mRoomSpec;
    case F::CmClient: return (intptr_t)cm->mClient.get();
    case F::CmFlag: return cm->mFlag ? 1 : 0;
    case F::CmKind: return cm->mModerationKind;
    case F::CmDuration: return (intptr_t)cm->mDuration;
    case F::CmRoomName: return Dat(cm->mRoomName);
    case F::CmComment: return Dat(cm->mComment);
    case F::GcGroup: return (intptr_t) static_cast<GroupChangedEvent*>(e)->mGroup.get();
    case F::GcServer: return (intptr_t) static_cast<GroupChangedEvent*>(e)->mServer.get();
    case F::ScServer: return (intptr_t) static_cast<ServerChangedEvent*>(e)->mServer.get();
    case F::FcFriend: return (intptr_t) static_cast<FriendChangedEvent*>(e)->mFriend.get();
    case F::LisStatus: return Dat(static_cast<LoginInitStatusEvent*>(e)->mStatus);
    case F::LisDescription: return Dat(static_cast<LoginInitStatusEvent*>(e)->mDescription);
    case F::LisAllowIgnore: return static_cast<LoginInitStatusEvent*>(e)->mAllowIgnore ? 1 : 0;
    case F::LisKind: return static_cast<LoginInitStatusEvent*>(e)->mKind;
    case F::SlfUserName: return Dat(static_cast<SetLoginFieldsEvent*>(e)->mUserName);
    case F::SlfPassword: return Dat(static_cast<SetLoginFieldsEvent*>(e)->mPassword);
    case F::QarEmail: return Dat(static_cast<QueryAccountResultEvent*>(e)->mEmail);
    case F::QarNewsletter: return static_cast<QueryAccountResultEvent*>(e)->mNewsletter ? 1 : 0;
    case F::GhdOp: return (intptr_t) static_cast<GotHTTPDocumentEvent*>(e)->mOp.get();
    default: break;
    }
    return 0;
}

static intptr_t CtlEvent(EeWonLobbyCtlOp inOp, ComponentEvent* anEvent)
{
    typedef EeWonLobbyCtlOp Op;
    switch (inOp) {
    case Op::EventFlags: return static_cast<SetRoomSpecFlagsEvent*>(anEvent)->mFlags;
    case Op::EventRoomSpec: return static_cast<ClientChangedEvent*>(anEvent)->mRoomSpec;
    case Op::EventIsModify: return static_cast<LobbyChangeEvent*>(anEvent)->IsModify() ? 1 : 0;
    case Op::EventIsAdd: return static_cast<LobbyChangeEvent*>(anEvent)->IsAdd() ? 1 : 0;
    case Op::EventIsDelete: return static_cast<LobbyChangeEvent*>(anEvent)->IsDelete() ? 1 : 0;
    case Op::ClientChangedClient: return (intptr_t) static_cast<ClientChangedEvent*>(anEvent)->mClient.get();
    case Op::PlayerChangedClient: return (intptr_t) static_cast<PlayerChangedEvent*>(anEvent)->mClient.get();
    case Op::EventGame: return (intptr_t) static_cast<GameChangedEvent*>(anEvent)->mGame.get();
    case Op::EventGames: return (intptr_t) static_cast<SyncGameListEvent*>(anEvent)->mGames.get();
    default: break;
    }
    return INTPTR_MIN;
}
static intptr_t LogicEvent(EeWonLobbyOp inOp, void* inA)
{
    switch (inOp) {
    case EeWonLobbyOp::EventDirectIsDelete: return static_cast<ChangeDirectConnectEvent*>(inA)->IsDelete();
    case EeWonLobbyOp::EventDirectIsAdd: return static_cast<ChangeDirectConnectEvent*>(inA)->IsAdd();
    case EeWonLobbyOp::EventDirectGame: return (intptr_t)static_cast<ChangeDirectConnectEvent*>(inA)->mGame.get();
    case EeWonLobbyOp::EventClientActionClient: return (intptr_t)static_cast<ClientActionEvent*>(inA)->mClient.get();
    case EeWonLobbyOp::EventClientActionRoomSpec: return static_cast<ClientActionEvent*>(inA)->mRoomSpec;
    case EeWonLobbyOp::EventClientActionAction: return static_cast<ClientActionEvent*>(inA)->mAction;
    case EeWonLobbyOp::EventJoinGame: return (intptr_t)static_cast<JoinGameEvent*>(inA)->mGame.get();
    case EeWonLobbyOp::EventActionGame: return (intptr_t)static_cast<GameActionEvent*>(inA)->mGame.get();
    case EeWonLobbyOp::EventAction: return static_cast<GameActionEvent*>(inA)->mAction;
    default: break;
    }
    return INTPTR_MIN;
}
static intptr_t RouteNum(ComponentEvent* anEvent, intptr_t b)
{
    switch (b) {
        case 1: return (intptr_t) static_cast<AcceptClientEvent*>(anEvent)->mClient.get();
        case 2: return static_cast<AcceptClientEvent*>(anEvent)->mAccepted;
        case 3: return (intptr_t) static_cast<FriendActionEvent*>(anEvent)->mFriend.get();
        case 4: return static_cast<FriendActionEvent*>(anEvent)->mAction;
        case 5: return static_cast<FriendActionEvent*>(anEvent)->mValue;
        case 6: return static_cast<JumpToRoomEvent*>(anEvent)->mGroupId;
        case 7: return static_cast<QuickPlayEvent*>(anEvent)->mGameType;
        case 8: return static_cast<ClientActionEvent*>(anEvent)->GetRoomSpec();
        case 9: return (intptr_t) static_cast<ClientActionEvent*>(anEvent)->GetClient();
        case 10: return static_cast<ClientActionEvent*>(anEvent)->GetAction();
        case 11: return (intptr_t) static_cast<JoinRoomEvent*>(anEvent)->mGroupId;
        case 12: return (intptr_t) static_cast<JoinFriendsRoomEvent*>(anEvent)->mFriend.get();
    }
    return INTPTR_MIN;
}
}
