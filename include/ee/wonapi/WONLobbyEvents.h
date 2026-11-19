#ifndef EE_WONAPI_WONLOBBYEVENTS_H
#define EE_WONAPI_WONLOBBYEVENTS_H
// The lobby's events (R6 WONLobby round 18): one C++ shell over a Rust event (rust/crates/ee_won_lobby/src/
// lobbyevt.rs: the members in each original class's declaration order, the references taken and given back as the
// original SmartPtr and GUIString members did). The makers below keep the original constructors' signatures (call
// them without `new`); a receiver reads a member by its original name (LobbyEvtGet, LobbyEvtText, ...).
// tests/won/won_lobbyevt_test.cpp includes this header over its own world (EE_LOBBYEVT_STANDIN).
#ifndef EE_LOBBYEVT_STANDIN
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyEventTypes.h"
#endif
#include <string>
#include "rust/won_lobby/lobbyevt.h"
namespace WONAPI {
class LobbyFriend;
class LobbyGroup;
class LobbyGameList;
typedef EeWonLobbyEvtKind LobbyEvtKind;
typedef EeWonLobbyEvtField LobbyEvtField;

class LobbyRustEvent : public LobbyEvent {
public:
    LobbyRustEvent(int inType, EeWonLobbyEvt* inEvt) : LobbyEvent(inType), mEvt(inEvt) { ee_won_lobbyevt_bind(inEvt, this); }
    virtual ~LobbyRustEvent() { ee_won_lobbyevt_free(this->mEvt); }
    EeWonLobbyEvt* mEvt;
};
// a member's value: a number, an object (no reference added), an address's value
inline intptr_t LobbyEvtGet(const ComponentEvent* e, LobbyEvtField f)
{ return ee_won_lobbyevt_get(static_cast<const LobbyRustEvent*>(e)->mEvt, f); }
inline void LobbyEvtSet(ComponentEvent* e, LobbyEvtField f, intptr_t v) { ee_won_lobbyevt_set(static_cast<LobbyRustEvent*>(e)->mEvt, f, v); }
inline GUIString LobbyEvtText(const ComponentEvent* e, LobbyEvtField f) { return GUIString((GUIStringData*)LobbyEvtGet(e, f)); }
inline IPAddr LobbyEvtAddr(const ComponentEvent* e, LobbyEvtField f)
{ return IPAddr::FromValue(ee_won_ipaddr_clone((const EeWonIpAddr*)LobbyEvtGet(e, f))); }
// a copied std::wstring / std::string member
template <class S> S LobbyEvtStr(const ComponentEvent* e, LobbyEvtField f)
{
    size_t n = ee_won_lobbyevt_len(static_cast<const LobbyRustEvent*>(e)->mEvt, f);
    return S((const typename S::value_type*)LobbyEvtGet(e, f), n / sizeof(typename S::value_type));
}
template <class S> void LobbyEvtSetStr(ComponentEvent* e, LobbyEvtField f, const S& s)
{ ee_won_lobbyevt_set_bytes(static_cast<LobbyRustEvent*>(e)->mEvt, f, (const uint8_t*)s.data(), s.size() * sizeof(typename S::value_type)); }

// the makers' parts: the values in member order (an object or text referenced by the event, an address copied, a
// string's bytes copied)
inline void LobbyEvtRefs(EeWonLobbyEvtRef inOp, intptr_t inObj)
{ if (inOp == EeWonLobbyEvtRef::AddRef) ((RefCount*)inObj)->AddRef(); else ((RefCount*)inObj)->Release(); }
template <size_t N> LobbyEvent* LobbyEvtNew(int inType, LobbyEvtKind inKind, const intptr_t (&inArgs)[N])
{ return new LobbyRustEvent(inType, ee_won_lobbyevt_new(inKind, inArgs, N, LobbyEvtRefs)); }
inline intptr_t LobbyEvtObj(const void* p) { return (intptr_t)p; }
inline intptr_t LobbyEvtDat(const GUIString& s) { return (intptr_t)s.mData.get(); }
inline intptr_t LobbyEvtIp(const IPAddr& a) { return (intptr_t)a.GetValue(); }
#define EE_LOBBYEVT_BYTES(s) (intptr_t)(s).data(), (intptr_t)((s).size() * sizeof((s)[0]))
#define EE_LOBBYEVT(name, kind, params, ...) \
    inline LobbyEvent* name params { const intptr_t aArgs_[] = {__VA_ARGS__}; return LobbyEvtNew(LobbyEvent_##kind, LobbyEvtKind::kind, aArgs_); }

EE_LOBBYEVT(AcceptClientEvent, AcceptClient, (LobbyClient* c, bool inAccepted), LobbyEvtObj(c), inAccepted)
EE_LOBBYEVT(AlertEvent, Alert, (int k, int v, int s, const GUIString& t), k, v, s, LobbyEvtDat(t))
EE_LOBBYEVT(AnnounceClientLocationEvent, AnnounceClientLocation,
    (LobbyClient* c, const GUIString& inServer, const GUIString& inRoom, int inRoomId),
    LobbyEvtObj(c), LobbyEvtDat(inServer), LobbyEvtDat(inRoom), inRoomId, false)
EE_LOBBYEVT(AnnounceFriendEvent, AnnounceFriend, (LobbyFriend* f, int inLocation, int inChangeKind),
    inChangeKind, LobbyEvtObj(f), inLocation, false)
EE_LOBBYEVT(FriendActionEvent, FriendAction, (int a, LobbyFriend* f, int v), a, LobbyEvtObj(f), v)
EE_LOBBYEVT(FriendChangedEvent, FriendChanged, (LobbyFriend* f, int k), k, LobbyEvtObj(f))
EE_LOBBYEVT(GameActionEvent, GameAction, (int a, LobbyGame* g), a, LobbyEvtObj(g))
EE_LOBBYEVT(GameChangedEvent, GameChanged, (LobbyGame* g, int k), k, LobbyEvtObj(g))
EE_LOBBYEVT(JoinFriendsRoomEvent, JoinFriendsRoom, (LobbyFriend* f), LobbyEvtObj(f))
EE_LOBBYEVT(JoinGameEvent, JoinGame, (LobbyGame* g), LobbyEvtObj(g))
EE_LOBBYEVT(PingGameEvent, PingGame, (LobbyGame* g, bool inFlag), LobbyEvtObj(g), inFlag)
EE_LOBBYEVT(PlayerChangedEvent, PlayerChanged, (LobbyClient* c, int k), k, LobbyEvtObj(c))
EE_LOBBYEVT(PlayerTeamChangedEvent, PlayerTeamChanged, (LobbyClient* c, int k), k, LobbyEvtObj(c))
EE_LOBBYEVT(ServerChangedEvent, ServerChanged, (LobbyServer* s, int k), k, LobbyEvtObj(s))
EE_LOBBYEVT(ClientActionEvent, ClientAction, (int a, const int& inRoomSpec, LobbyClient* c), a, inRoomSpec, LobbyEvtObj(c))
inline LobbyEvent* GotHTTPDocumentEvent(RustServerOp* inOp)
{ const intptr_t v[] = {LobbyEvtObj(inOp)}; return LobbyEvtNew(LobbyEvent_GotHTTPDocument, LobbyEvtKind::GotHttpDocument, v); }
EE_LOBBYEVT(ChangeDirectConnectEvent, ChangeDirectConnect, (int k, LobbyGame* g), k, LobbyEvtObj(g))
EE_LOBBYEVT(ClientChangedEvent, ClientChanged, (LobbyClient* c, const int& inRoomSpec, int k), k, LobbyEvtObj(c), inRoomSpec)
EE_LOBBYEVT(ClientModeratedEvent, ClientModerated, (LobbyClient* c, int inRoomSpec, const GUIString& inRoomName,
    int inModerationKind, unsigned long inDuration, bool inFlag, const GUIString& inComment),
    LobbyEvtObj(c), inRoomSpec, LobbyEvtDat(inRoomName), inModerationKind, (intptr_t)inDuration, inFlag, LobbyEvtDat(inComment))
EE_LOBBYEVT(GroupChangedEvent, GroupChanged, (LobbyGroup* g, LobbyServer* s, int k), k, LobbyEvtObj(s), LobbyEvtObj(g))
EE_LOBBYEVT(GroupInvitationEvent, GroupInvitation, (int k, const GUIString& inGameName, unsigned short inGameId,
    LobbyClient* c, bool inInvite, const GUIString& inComment, bool inFlag),
    k, LobbyEvtDat(inGameName), inGameId, LobbyEvtObj(c), inInvite, inFlag, LobbyEvtDat(inComment))
EE_LOBBYEVT(GroupJoinAttemptEvent, GroupJoinAttempt, (int k, LobbyClient* c, const GUIString& inComment),
    k, LobbyEvtObj(c), LobbyEvtDat(inComment))
EE_LOBBYEVT(JoinedRoomEvent, JoinedRoom, (const GUIString& inServerName, const GUIString& inGroupName,
    const int& inRoomSpec, unsigned short inClientId, LobbyClientList* inClients),
    LobbyEvtObj(inClients), LobbyEvtDat(inServerName), LobbyEvtDat(inGroupName), inClientId, inRoomSpec)
EE_LOBBYEVT(RecvChatEvent, RecvChat, (const int& inRoomSpec, unsigned long inFlags, LobbyClient* inSender,
    const GUIString& inText, LobbyClient* inTarget),
    LobbyEvtObj(inSender), LobbyEvtObj(inTarget), inRoomSpec, (intptr_t)inFlags, LobbyEvtDat(inText))
EE_LOBBYEVT(SetRoomSpecFlagsEvent, SetRoomSpecFlags, (int inFlags), inFlags)
EE_LOBBYEVT(LoginInitStatusEvent, LoginInitStatus, (const GUIString& inStatus, const GUIString& inDescription,
    bool inAllowIgnore, int inKind), LobbyEvtDat(inStatus), LobbyEvtDat(inDescription), inAllowIgnore, inKind)
EE_LOBBYEVT(SetLoginFieldsEvent, SetLoginFields, (const GUIString& inUser, const GUIString& inPassword, bool inRemember),
    LobbyEvtDat(inUser), LobbyEvtDat(inPassword), inRemember)
EE_LOBBYEVT(QueryAccountResultEvent, QueryAccountResult, (const GUIString& inEmail, bool inNewsletter),
    LobbyEvtDat(inEmail), inNewsletter)
inline LobbyEvent* LoginEvent(const GUIString& inPassword, bool inRemember, const GUIString& inUser)
{
    std::wstring u = (std::wstring)inUser, p = (std::wstring)inPassword;
    const intptr_t v[] = {EE_LOBBYEVT_BYTES(u), EE_LOBBYEVT_BYTES(p), inRemember};
    return LobbyEvtNew(LobbyEvent_Login, LobbyEvtKind::Login, v);
}
inline LobbyEvent* GetLostPasswordEvent(const GUIString& inUserName)
{ std::wstring u = (std::wstring)inUserName; const intptr_t v[] = {EE_LOBBYEVT_BYTES(u)}; return LobbyEvtNew(LobbyEvent_GetLostPassword, LobbyEvtKind::GetLostPassword, v); }
inline LobbyEvent* GetLostUserNameEvent(const GUIString& inEmail)
{ std::string m = (std::string)inEmail; const intptr_t v[] = {EE_LOBBYEVT_BYTES(m)}; return LobbyEvtNew(LobbyEvent_GetLostUserName, LobbyEvtKind::GetLostUserName, v); }
EE_LOBBYEVT(LeftRoomEvent, LeftRoom, (int inRoomSpec, bool inForced, bool inFlag), inRoomSpec, inForced, inFlag)
EE_LOBBYEVT(SetChatInputEvent, SetChatInput, (int inCommand, const GUIString& inText, int inRoomSpec, bool inHandled),
    inCommand, LobbyEvtDat(inText), inRoomSpec, inHandled)
EE_LOBBYEVT(ChatCommandErrorEvent, ChatCommandError, (const GUIString& inText), LobbyEvtDat(inText))
EE_LOBBYEVT(CreateRoomEvent, CreateRoom, (const IPAddr& inAddress), LobbyEvtIp(inAddress))
EE_LOBBYEVT(JoinRoomEvent, JoinRoom, (const IPAddr& inAddress, unsigned long inGroupId), LobbyEvtIp(inAddress), (intptr_t)inGroupId)
// the original built its default strings, then the caller wrote every member (the month, day and year read 0 until then)
inline LobbyEvent* CreateAccountEvent()
{ const intptr_t v[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; return LobbyEvtNew(LobbyEvent_CreateAccount, LobbyEvtKind::CreateAccount, v); }
EE_LOBBYEVT(SyncGameListEvent, SyncGameList, (LobbyGameList* inGames), LobbyEvtObj(inGames))
EE_LOBBYEVT(JumpToRoomEvent, JumpToRoom, (const GUIString& inRoomName, unsigned short inGroupId, const IPAddr& inAddress),
    LobbyEvtDat(inRoomName), inGroupId, LobbyEvtIp(inAddress))
EE_LOBBYEVT(QuickPlayEvent, QuickPlay, (int inGameType), inGameType)
// the image is not owned (never retained or released)
inline LobbyEvent* SetCrossPromotionEvent(Image* inImage, const std::string& inLink)
{ const intptr_t v[] = {LobbyEvtObj(inImage), EE_LOBBYEVT_BYTES(inLink)}; return LobbyEvtNew(LobbyEvent_SetCrossPromotion, LobbyEvtKind::SetCrossPromotion, v); }
EE_LOBBYEVT(UpdateAccountEvent, UpdateAccount, (const GUIString& inEmail, const GUIString& inNewPassword, bool inNewsletter),
    LobbyEvtDat(inEmail), LobbyEvtDat(inNewPassword), inNewsletter)
EE_LOBBYEVT(QueryServerGroupsEvent, QueryServerGroups, (const IPAddr& inAddress), LobbyEvtIp(inAddress))
#undef EE_LOBBYEVT
}
#endif
