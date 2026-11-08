#ifndef EE_WONAPI_WONLOBBYEVENTS_H
#define EE_WONAPI_WONLOBBYEVENTS_H
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyEventTypes.h"
namespace WONAPI {
class LobbyFriend;
class LobbyGroup;
class LobbyGameList;
// RTTI 0x851af0; LobbyEvent is 0x20 bytes, the change kind is at 0x20, 0x24 bytes in all.
class LobbyChangeEvent : public LobbyEvent {
public:
    LobbyChangeEvent(int inType, int inChangeKind); // 0x7593a0
    bool IsAdd();                                // 0x759450
    bool IsDelete();                             // 0x759460
    bool IsModify();                             // 0x759470; kind 2
    int mChangeKind;
};
class AcceptClientEvent : public LobbyEvent {
public:
    virtual ~AcceptClientEvent(); // explicit: writes its own vtable pointer
    AcceptClientEvent(LobbyClient* inClient, bool inAccepted); // 0x759480
    SmartPtr<LobbyClient> mClient; // 0x20
    bool mAccepted; // 0x24
};
class AlertEvent : public LobbyEvent {
public:
    virtual ~AlertEvent(); // explicit: writes its own vtable pointer
    AlertEvent(int inKind, int inValue, int inSeverity, const GUIString& inText); // 0x759580
    int GetKind() const { return this->mKind; }
    int GetValue() const { return this->mValue; }
    int GetSeverity() const { return this->mSeverity; }
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
        const GUIString& inRoom, int inRoomId); // 0x759710
    SmartPtr<LobbyClient> mClient; // 0x20
    GUIString mServer; // 0x24
    GUIString mRoom; // 0x28
    int mRoomId; // 0x2c
    bool mHandled; // 0x30
};
class AnnounceFriendEvent : public LobbyChangeEvent {
public:
    virtual ~AnnounceFriendEvent(); // explicit: writes its own vtable pointer
    AnnounceFriendEvent(LobbyFriend* inFriend, int inLocation, int inChangeKind); // 0x759890
    SmartPtr<LobbyFriend> mFriend; // 0x24
    int mLocation; // 0x28
    bool mHandled; // 0x2c
};
class FriendActionEvent : public LobbyEvent {
public:
    virtual ~FriendActionEvent(); // explicit: writes its own vtable pointer
    FriendActionEvent(int inAction, LobbyFriend* inFriend, int inValue); // 0x759ea0
    int mAction; // 0x20
    SmartPtr<LobbyFriend> mFriend; // 0x24
    int mValue; // 0x28
};
class FriendChangedEvent : public LobbyChangeEvent {
public:
    virtual ~FriendChangedEvent(); // explicit: writes its own vtable pointer
    FriendChangedEvent(LobbyFriend* inFriend, int inChangeKind); // 0x759fa0
    SmartPtr<LobbyFriend> mFriend; // 0x24
};
class GameActionEvent : public LobbyEvent {
public:
    virtual ~GameActionEvent(); // explicit: writes its own vtable pointer
    GameActionEvent(int inAction, LobbyGame* inGame); // 0x75a0a0
    int mAction; // 0x20
    SmartPtr<LobbyGame> mGame; // 0x24
};
class GameChangedEvent : public LobbyChangeEvent {
public:
    virtual ~GameChangedEvent(); // explicit: writes its own vtable pointer
    GameChangedEvent(LobbyGame* inGame, int inChangeKind); // 0x75a1a0
    SmartPtr<LobbyGame> mGame; // 0x24
};
class JoinFriendsRoomEvent : public LobbyEvent {
public:
    virtual ~JoinFriendsRoomEvent(); // explicit: writes its own vtable pointer
    JoinFriendsRoomEvent(LobbyFriend* inFriend); // 0x75a7b0
    SmartPtr<LobbyFriend> mFriend; // 0x20
};
class JoinGameEvent : public LobbyEvent {
public:
    virtual ~JoinGameEvent(); // explicit: writes its own vtable pointer
    JoinGameEvent(LobbyGame* inGame); // 0x75a8b0
    SmartPtr<LobbyGame> mGame; // 0x20
};
class PingGameEvent : public LobbyEvent {
public:
    virtual ~PingGameEvent(); // explicit: writes its own vtable pointer
    PingGameEvent(LobbyGame* inGame, bool inFlag); // 0x75ab30
    SmartPtr<LobbyGame> mGame; // 0x20
    bool mFlag; // 0x24
};
class PlayerChangedEvent : public LobbyChangeEvent {
public:
    virtual ~PlayerChangedEvent(); // explicit: writes its own vtable pointer
    PlayerChangedEvent(LobbyClient* inClient, int inChangeKind); // 0x75ac30
    SmartPtr<LobbyClient> mClient; // 0x24
};
class PlayerTeamChangedEvent : public LobbyChangeEvent {
public:
    virtual ~PlayerTeamChangedEvent(); // explicit: writes its own vtable pointer
    PlayerTeamChangedEvent(LobbyClient* inClient, int inChangeKind); // 0x75ad30
    SmartPtr<LobbyClient> mClient; // 0x24
};
class ServerChangedEvent : public LobbyChangeEvent {
public:
    virtual ~ServerChangedEvent(); // explicit: writes its own vtable pointer
    ServerChangedEvent(LobbyServer* inServer, int inChangeKind); // 0x75aff0
    SmartPtr<LobbyServer> mServer; // 0x24
};
class ClientActionEvent : public LobbyEvent {
public:
    virtual ~ClientActionEvent(); // explicit: writes its own vtable pointer
    ClientActionEvent(int inAction, const int& inRoomSpec, LobbyClient* inClient); // 0x759a90
    int GetAction() const { return this->mAction; }
    int GetRoomSpec() const { return this->mRoomSpec; }
    LobbyClient* GetClient() const { return this->mClient.get(); }
    int mAction; int mRoomSpec; SmartPtr<LobbyClient> mClient; // 0x20, 0x24, 0x28
};
class GotHTTPDocumentEvent : public LobbyEvent {
public:
    virtual ~GotHTTPDocumentEvent(); // explicit: writes its own vtable pointer
    GotHTTPDocumentEvent(GetHTTPDocumentOp* inOp); // 0x75a2a0; used by InitLogic::HTTPDocumentCompletion (0x7de280)
    SmartPtr<GetHTTPDocumentOp> mOp; // 0x20; 0x24 bytes
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
    ClientChangedEvent(LobbyClient* inClient, const int& inRoomSpec, int inChangeKind); // 0x759b90
    SmartPtr<LobbyClient> mClient; int mRoomSpec; // 0x24, 0x28
};
class ClientModeratedEvent : public LobbyEvent {
public:
    virtual ~ClientModeratedEvent(); // explicit: writes its own vtable pointer
    ClientModeratedEvent(LobbyClient* inClient, int inRoomSpec, const GUIString& inRoomName,
        int inModerationKind, unsigned long inDuration, bool inFlag, const GUIString& inComment); // 0x759c90
    SmartPtr<LobbyClient> mClient; int mRoomSpec; GUIString mRoomName;
    int mModerationKind; unsigned long mDuration; bool mFlag; GUIString mComment; // 0x20, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x38
};
class GroupChangedEvent : public LobbyChangeEvent {
public:
    virtual ~GroupChangedEvent(); // explicit: writes its own vtable pointer
    GroupChangedEvent(LobbyGroup* inGroup, LobbyServer* inServer, int inChangeKind); // 0x75a3a0
    SmartPtr<LobbyServer> mServer; SmartPtr<LobbyGroup> mGroup; // 0x24, 0x28
};
class GroupInvitationEvent : public LobbyEvent {
public:
    virtual ~GroupInvitationEvent(); // explicit: writes its own vtable pointer
    GroupInvitationEvent(int inKind, const GUIString& inGameName, unsigned short inGameId,
        LobbyClient* inClient, bool inInvite, const GUIString& inComment, bool inFlag); // 0x75a4e0
    int mKind; GUIString mGameName; unsigned short mGameId; SmartPtr<LobbyClient> mClient;
    bool mInvite; bool mFlag; GUIString mComment; // 0x20, 0x24, 0x28, 0x2c, 0x30, 0x31, 0x34
};
class GroupJoinAttemptEvent : public LobbyEvent {
public:
    virtual ~GroupJoinAttemptEvent(); // explicit: writes its own vtable pointer
    GroupJoinAttemptEvent(int inKind, LobbyClient* inClient, const GUIString& inComment); // 0x75a670
    int mKind; SmartPtr<LobbyClient> mClient; GUIString mComment; // 0x20, 0x24, 0x28
};
class JoinedRoomEvent : public LobbyEvent {
public:
    int GetRoomSpec() const { return this->mRoomSpec; }
    virtual ~JoinedRoomEvent(); // explicit: writes its own vtable pointer
    JoinedRoomEvent(const GUIString& inServerName, const GUIString& inGroupName,
        const int& inRoomSpec, unsigned short inClientId, LobbyClientList* inClients); // 0x75a9b0
    SmartPtr<LobbyClientList> mClients; GUIString mServerName; GUIString mGroupName;
    unsigned short mClientId; int mRoomSpec; // 0x20, 0x24, 0x28, 0x2c, 0x30
};
class RecvChatEvent : public LobbyEvent {
public:
    virtual ~RecvChatEvent(); // explicit: writes its own vtable pointer
    RecvChatEvent(const int& inRoomSpec, unsigned long inFlags, LobbyClient* inSender,
        const GUIString& inText, LobbyClient* inTarget); // 0x75ae30
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
    SyncGameListEvent(LobbyGameList* inGames); // 0x75b0f0
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
}
#endif
