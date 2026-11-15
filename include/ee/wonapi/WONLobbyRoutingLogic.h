// RoutingLogic: internet play over WON routing servers (rooms, games, friends, chat, moderation)
#ifndef EE_WONAPI_WONLOBBYROUTINGLOGIC_H
#define EE_WONAPI_WONLOBBYROUTINGLOGIC_H
#include <time.h>
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyEvents.h"
class RoutingOpCompletion;
struct EeWonRouteLogic;
namespace WONAPI {
class LobbyGameList;
class AddFriendDialog;
class AdminActionDialog;
class BadUserDialog;
class CreateRoomDialog;
class RoomDialog;
class PasswordDialog;
// RTTI: RoutingLogic > NetLogic > LobbyLogic > RefCount; vtable 0x855a60.
// A Rust object since R6 WONLobby round 9 (rust/crates/ee_won/src/routelogic.rs): the servers it is on and
// connecting to, the joined rooms, the idle and moderator state, the last server refresh (the original's 0x40,
// 0x5c..0x5f, 0x60..0x70) and every method; this shell keeps the lists it owns, the dialogs and the data type.
class RoutingLogic : public NetLogic {
public:
    virtual void InitComponent(ComponentConfig* inConfig);
    RoutingLogic();
    virtual ~RoutingLogic();
    virtual bool HostGame(LobbyGame* inGame);
    virtual bool JoinGame(LobbyGame* inGame);
    LobbyClientList* GetClientList(); // the current server's players, or NULL
    LobbyServer* GetCurrentServer(); // the server joined, or NULL
    LobbyGroup* GetChatGroup(); // the chat room joined, or NULL
    virtual LobbyClient* GetMyClient();
    virtual void SendToCaptain(const ByteBuffer* inData);
    virtual void BroadcastToGame(const ByteBuffer* inData);
    virtual void SendToClient(LobbyClient* inClient, const ByteBuffer* inData);
    virtual void AdvertiseGame(LobbyGame* inGame);
    virtual void RemoveGameAdvertisement(LobbyGame*);
    virtual void KickClient(LobbyClient* inClient, bool inBan);
    virtual void SetGameInProgress(bool inClose);
    virtual void Reset();
    virtual void LeaveGame(LobbyGame*);
    virtual void Suspend();
    virtual void Resume();
    void EnterInternetScreen(bool); // 0x7ed430; enters the internet screen
    // the completions' targets (LogicCompletion<RoutingLogic>)
    void QueryServerGroupsDone(AsyncOp*); // 0x7e9e60; group query completion
    void GetUserStatusComplete(AsyncOp*); // 0x7e7460; user status query completion
    void ConnectionClosed(AsyncOp* inBaseOp); // 0x7e7320
    virtual void Logout();    // 0x7eeed0
    virtual void HandleControlEvent(int); // 0x7ea310
    virtual void HandleComponentEvent(ComponentEvent*); // 0x7ea3a0
    EeWonRouteLogic* mRust; // owned
    std::string mGameDataType; // 0x44, "G" from the constructor
    SmartPtr<LobbyServerList> mServerList; // 0x54
    SmartPtr<LobbyGameList> mGameList; // 0x58
    SmartPtr<AddFriendDialog> mAddFriendDialog; // 0x74; type descriptor 0x8e1608
    SmartPtr<AdminActionDialog> mAdminActionDialog; // 0x78; type descriptor 0x8e1630
    SmartPtr<BadUserDialog> mBadUserDialog; // 0x7c; type descriptor 0x8e1658
    SmartPtr<CreateRoomDialog> mCreateRoomDialog; // 0x80; type descriptor 0x8e1680
    SmartPtr<RoomDialog> mRoomDialog; // 0x84; type descriptor 0x8e16a8
    SmartPtr<PasswordDialog> mPasswordDialog; // 0x88; type descriptor 0x8e15e0
};
}
#endif
