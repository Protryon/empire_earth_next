// The lobby logics' one shell (R6 WON lobby and network round 36: ee_won_lobby logicshell.rs, RustLogic in
// ee/wonapi/WONLobbyLogic.h) against the six classes it replaces, as they were at 71c097464: AccountLogic and
// InitLogic (WONLobbyAccountLogic.h, AccountLogic.cpp, InitLogic.cpp), DialogLogic (WONLobbyDialogLogic.h,
// DialogLogic.cpp), RoutingLogic with RoutingOpCompletion (WONLobbyRoutingLogic.h, RoutingLogic.cpp), StagingLogic
// (WONLobbyStaging.h, LobbyLogicHost.cpp) and LanLogic (WONLobbyLan.h, LanLogicHost.cpp), and LogicCompletion<T>
// (WONRouting.h). "WON goldens by reach": a LAN game runs every kind's InitComponent, events, timer reset and kill
// hook and the LAN and dialog completions, but none of the routing kind's NetLogic calls or the Internet logics'
// completions.
//
// Each case runs on both sides: the original class's method (Orig, its calls on the Rust object logged in place of
// the object: ee_won_*_call are the log here), then the shell's method as it is (New: RustLogic's forwards copied,
// logicshell.rs's route, dialog rows and completion route through their numbers, each logged as the call the shell
// makes). The traces and results must be equal. Then every route is spoiled in turn on the new side (its call, an
// argument or its answer changed): each spoiled route must make some case differ. The exit code is the real pass's
// mismatches plus the spoiled routes not caught.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <stdexcept>
#include "rust/won_lobby/logicshell.h"
#include "rust/won_lobby/logics.h"
#include "rust/won_lobby/routelogic.h"
#include "rust/won_lobby/stagingobj.h"
#include "rust/won_lobby/lanobj.h"
#include "rust/won_lobby/dlglogic.h"
#include "rust/won_lobby/lobbydlg.h"
#include "rust/won_lobby/lobbyids.h"
#include "rust/won_net/hostop.h"
#include "ee/wonapi/WONLobbyGuard.h"

// ---- the world both sides run over

static std::vector<std::string> sTrace;
static void Log(const std::string& s) { sTrace.push_back(s); }
static std::string N(long long v) { return std::to_string(v); }
// a call on the kind's Rust object: the target (1 logic, 2 dialog logic, 3 routing, 4 staging, 5 LAN), the call's
// number, four arguments
static intptr_t LogCall(int inTarget, uint32_t inCall, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    Log("call " + N(inTarget) + ":" + N(inCall) + " " + N(a) + " " + N(b) + " " + N(c) + " " + N(d));
    return sTrace.size() * 7 + 1; // an answer both sides get (never 0: a bool result shows)
}

struct ByteBuffer {
    const char* mData;
    unsigned long mLength;
    const char* data() const { return this->mData; }
    unsigned long length() const { return this->mLength; }
};
struct ComponentConfig { int mFailAt; }; // the lookup that finds nothing (-1: none)
struct ComponentEvent { int mType; };
struct LobbyGame {};
struct LobbyClient {};
struct AsyncOp {};
struct LobbyRustDialog { EeWonLobbyDlgKind mKind; std::string mName; };
static std::vector<LobbyRustDialog*> sDialogs;
static int sLookups;
// SafeGetDialog over the world: the named dialog of the kind, or the missing component's throw
static void SafeGetDialog(ComponentConfig* inConfig, LobbyRustDialog*& outPtr, EeWonLobbyDlgKind k, const char* inName,
                          const char* inFile, int inLine)
{
    Log(std::string("find ") + inName + " kind " + N((int)k) + " at " + inFile + ":" + N(inLine));
    if (inConfig->mFailAt == sLookups++) throw std::runtime_error(std::string("missing ") + inName);
    LobbyRustDialog* d = new LobbyRustDialog;
    d->mKind = k;
    d->mName = inName;
    sDialogs.push_back(d);
    outPtr = d;
}
static std::string DialogName(const LobbyRustDialog* d) { return d == NULL ? "null" : d->mName + "/" + N((int)d->mKind); }
static bool LobbyLeaveGame(bool inConfirm) { Log(std::string("LobbyLeaveGame ") + N(inConfirm)); return true; }
// the tracker's answer for RemoveTrackedOp
static bool sTracked;

// ---- the original side: the classes as they were, their Rust objects the log

namespace Orig {
#define ee_won_logic_call(p, call, a, b, c) LogCall(1, (uint32_t)(call), a, b, c, 0)
#define ee_won_dlglogic_call(p, call, a, b, c, d, e) LogCall(2, (uint32_t)(call), a, (b) + (c) + (d) + (e), 0, 0)
#define ee_won_routelogic_call(p, call, a, b) LogCall(3, (uint32_t)(call), a, b, 0, 0)
#define ee_won_staging_call(p, call, a, b, c, d) LogCall(4, (uint32_t)(call), (intptr_t)(a), (intptr_t)(b), c, d)
#define ee_won_lan_call(p, call, a, b, c, d) LogCall(5, (uint32_t)(call), (intptr_t)(a), (intptr_t)(b), c, d)
static const char* const kAccountFile = "C:\\EEX\\Empire Earth\\WONAPI\\WONLobby\\AccountLogic.cpp"; // its __FILE__
static const char* const kInitFile = "C:\\EEX\\Empire Earth\\WONAPI\\WONLobby\\InitLogic.cpp";
static const char* const kRoutingFile = "C:\\EEX\\Empire Earth\\WONAPI\\WONLobby\\RoutingLogic.cpp";

struct LobbyLogic {
    bool mKilled = false;
    void* mRust = NULL;
    virtual ~LobbyLogic() {}
    virtual void KillHook() {}
    virtual void InitComponent(ComponentConfig*) {}
    virtual void Reset() {}
    virtual void HandleControlEvent(int) {}
    virtual void HandleComponentEvent(ComponentEvent*) {}
    bool RemoveTrackedOp(AsyncOp* inOp) { Log("untrack " + N((intptr_t)inOp)); return sTracked; }
};
struct NetLogic : LobbyLogic {
    virtual bool HostGame(LobbyGame* inGame) = 0;
    virtual bool JoinGame(LobbyGame* inGame) = 0;
    virtual LobbyClient* GetMyClient() = 0;
    virtual void SendToCaptain(const ByteBuffer* inBuffer) = 0;
    virtual void BroadcastToGame(const ByteBuffer* inBuffer) = 0;
    virtual void SendToClient(LobbyClient* inClient, const ByteBuffer* inBuffer) = 0;
    virtual void AdvertiseGame(LobbyGame* inGame) = 0;
    virtual void RemoveGameAdvertisement(LobbyGame* inGame) = 0;
    virtual void KickClient(LobbyClient* inClient, bool inFlag) = 0;
    virtual void SetGameInProgress(bool inFlag) = 0;
    virtual void LeaveGame(LobbyGame* inGame) = 0;
    virtual void Suspend() = 0;
    virtual void Resume() = 0;
    virtual void Logout() = 0;
};
// LogicCompletion<T>::Run (WONRouting.h)
template <class T> struct LogicCompletion {
    typedef void (T::*Callback)(AsyncOp* inOp);
    LogicCompletion(Callback inCallback, T* inObject) : mCallback(inCallback), mObject(inObject) {}
    bool mCheckTracker = false;
    void Run(AsyncOp* inOp)
    {
        if (ee_won_logic_completion_runs(this->mObject->mKilled, this->mCheckTracker, &Untrack, this->mObject, inOp))
            (this->mObject->*mCallback)(inOp);
    }
    static bool Untrack(void* inLogic, void* inOp) { return static_cast<T*>(inLogic)->RemoveTrackedOp(static_cast<AsyncOp*>(inOp)); }
    Callback mCallback;
    T* mObject;
};

struct AccountLogic : LobbyLogic {
    LobbyRustDialog* mCreditsDialog = NULL;
    LobbyRustDialog* mTOUDialog = NULL;
    virtual void InitComponent(ComponentConfig* inConfig)
    {
        SafeGetDialog(inConfig, this->mCreditsDialog, EeWonLobbyDlgKind::Credits, "CreditsDialog", kAccountFile, 43);
        SafeGetDialog(inConfig, this->mTOUDialog, EeWonLobbyDlgKind::Tou, "TOUDialog", kAccountFile, 44);
    }
    virtual void HandleControlEvent(int inControlId) { ee_won_logic_call(this->mRust, EeWonLogicCall::AcctControlId, inControlId, 0, 0); }
    virtual void HandleComponentEvent(ComponentEvent* inEvent)
    {
        ee_won_logic_call(this->mRust, EeWonLogicCall::AcctEvent, (intptr_t)inEvent, inEvent->mType, 0);
    }
};
struct InitLogic : LobbyLogic {
    LobbyRustDialog* mNetworkAdapterDialog = NULL;
    virtual void KillHook() { this->Call(EeWonLogicCall::InitKillHook); }
    virtual void InitComponent(ComponentConfig* inConfig)
    {
        SafeGetDialog(inConfig, this->mNetworkAdapterDialog, EeWonLobbyDlgKind::ChooseAdapter, "ChooseNetworkAdapterDialog", kInitFile, 49);
    }
    virtual void HandleControlEvent(int inControlId) { this->Call(EeWonLogicCall::InitControlId, inControlId); }
    virtual void HandleComponentEvent(ComponentEvent* inEvent) { this->Call(EeWonLogicCall::InitEvent, inEvent->mType); }
    void DNSCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitDns, (intptr_t)inOp); }
    void HTTPDocumentCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitDocument, (intptr_t)inOp); }
    void CrossPromotionImageCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitPromotionImage, (intptr_t)inOp); }
    void DirectoryCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitDirectory, (intptr_t)inOp); }
    void VersionCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitVersion, (intptr_t)inOp); }
    void AcceptCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitAccept, (intptr_t)inOp); }
    void FirewallCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitFirewall, (intptr_t)inOp); }
    intptr_t Call(EeWonLogicCall inCall, intptr_t inA = 0) { return ee_won_logic_call(this->mRust, inCall, inA, 0, 0); }
};
struct DialogLogic : LobbyLogic {
    virtual void InitComponent(ComponentConfig* inConfig) { this->Call(EeWonDlgLogicCall::Init, (intptr_t)inConfig); }
    virtual void KillHook() { this->Call(EeWonDlgLogicCall::KillHook); }
    virtual void Reset() { this->Call(EeWonDlgLogicCall::Reset); }
    void StatusOpComplete(AsyncOp* inOp) { this->Call(EeWonDlgLogicCall::StatusOpComplete, (intptr_t)inOp); }
    intptr_t Call(EeWonDlgLogicCall inCall, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0,
                  intptr_t e = 0) { return ee_won_dlglogic_call(this->mRust, inCall, a, b, c, d, e); }
};
struct RoutingLogic;
static intptr_t Call(RoutingLogic* inLogic, EeWonRouteLogicCall inCall, intptr_t a = 0, intptr_t b = 0);
struct RoutingLogic : NetLogic {
    LobbyRustDialog* mAddFriendDialog = NULL; // 0x74
    LobbyRustDialog* mAdminActionDialog = NULL; // 0x78
    LobbyRustDialog* mBadUserDialog = NULL; // 0x7c
    LobbyRustDialog* mCreateRoomDialog = NULL; // 0x80
    LobbyRustDialog* mRoomDialog = NULL; // 0x84
    LobbyRustDialog* mPasswordDialog = NULL; // 0x88
    virtual void InitComponent(ComponentConfig* inConfig)
    {
        SafeGetDialog(inConfig, this->mAddFriendDialog, EeWonLobbyDlgKind::AddFriend, "AddFriendDialog", kRoutingFile, 125);
        SafeGetDialog(inConfig, this->mAdminActionDialog, EeWonLobbyDlgKind::AdminAction, "AdminActionDialog", kRoutingFile, 126);
        SafeGetDialog(inConfig, this->mBadUserDialog, EeWonLobbyDlgKind::BadUser, "BadUserDialog", kRoutingFile, 127);
        SafeGetDialog(inConfig, this->mCreateRoomDialog, EeWonLobbyDlgKind::CreateRoom, "CreateRoomDialog", kRoutingFile, 128);
        SafeGetDialog(inConfig, this->mPasswordDialog, EeWonLobbyDlgKind::Password, "PasswordDialog", kRoutingFile, 129);
        SafeGetDialog(inConfig, this->mRoomDialog, EeWonLobbyDlgKind::Room, "RoomDialog", kRoutingFile, 130);
    }
    virtual bool HostGame(LobbyGame* inGame) { return Call(this, EeWonRouteLogicCall::HostGame, (intptr_t)inGame) != 0; }
    virtual bool JoinGame(LobbyGame* inGame) { return Call(this, EeWonRouteLogicCall::JoinGame, (intptr_t)inGame) != 0; }
    virtual LobbyClient* GetMyClient() { return reinterpret_cast<LobbyClient*>(Call(this, EeWonRouteLogicCall::MyClient)); }
    virtual void SendToCaptain(const ByteBuffer* inData) { Call(this, EeWonRouteLogicCall::SendToCaptain, (intptr_t)inData); }
    virtual void BroadcastToGame(const ByteBuffer* inData) { Call(this, EeWonRouteLogicCall::BroadcastToGame, (intptr_t)inData); }
    virtual void SendToClient(LobbyClient* inClient, const ByteBuffer* inData)
    {
        Call(this, EeWonRouteLogicCall::SendToClient, (intptr_t)inClient, (intptr_t)inData);
    }
    virtual void AdvertiseGame(LobbyGame* inGame) { Call(this, EeWonRouteLogicCall::Advertise, (intptr_t)inGame); }
    virtual void RemoveGameAdvertisement(LobbyGame*) { Call(this, EeWonRouteLogicCall::RemoveAdvertisement); }
    virtual void KickClient(LobbyClient* inClient, bool inBan) { Call(this, EeWonRouteLogicCall::KickClient, (intptr_t)inClient, inBan); }
    virtual void SetGameInProgress(bool inClose) { Call(this, EeWonRouteLogicCall::SetGameInProgress, inClose); }
    virtual void Reset() { Call(this, EeWonRouteLogicCall::Reset); }
    virtual void LeaveGame(LobbyGame*) { Call(this, EeWonRouteLogicCall::LeaveGame); }
    virtual void Suspend() { Call(this, EeWonRouteLogicCall::Suspend); }
    virtual void Resume() { Call(this, EeWonRouteLogicCall::Resume); }
    void GetUserStatusComplete(AsyncOp* inOp) { Call(this, EeWonRouteLogicCall::UserStatusDone, (intptr_t)inOp); }
    void QueryServerGroupsDone(AsyncOp* inOp) { Call(this, EeWonRouteLogicCall::GroupsDone, (intptr_t)inOp); }
    void ConnectionClosed(AsyncOp* inOp) { Call(this, EeWonRouteLogicCall::ConnectionClosed, (intptr_t)inOp); }
    virtual void HandleControlEvent(int controlId) { Call(this, EeWonRouteLogicCall::ControlId, controlId); }
    virtual void HandleComponentEvent(ComponentEvent* event)
    {
        Call(this, EeWonRouteLogicCall::Event, (intptr_t)event, event->mType);
    }
    virtual void Logout() { Call(this, EeWonRouteLogicCall::Logout); }
};
static intptr_t Call(RoutingLogic* inLogic, EeWonRouteLogicCall inCall, intptr_t a, intptr_t b)
{
    return ee_won_routelogic_call(inLogic->mRust, inCall, a, b);
}
// RoutingLogic.cpp's notice completion
struct RoutingOpCompletion {
    RoutingOpCompletion(int inCode, RoutingLogic* inLogic) : mCode(inCode), mLogic(inLogic) {}
    void Run(AsyncOp* inOp)
    {
        ee_won_routelogic_call(this->mLogic->mRust, EeWonRouteLogicCall::Notify, this->mCode, (intptr_t)inOp);
    }
    int mCode;
    RoutingLogic* mLogic;
};
struct StagingLogic : LobbyLogic {
    LobbyRustDialog* mCreateGameDialog = NULL;
    virtual void InitComponent(ComponentConfig* inConfig)
    {
        SafeGetDialog(inConfig, this->mCreateGameDialog, EeWonLobbyDlgKind::CreateGame, "CreateGameDialog", "StagingLogic.cpp", 41);
    }
    virtual void HandleControlEvent(int inControlId)
    {
        int aWhich = inControlId == ID_HostGame ? 1 : inControlId == ID_LeaveGame ? 2 : inControlId == ID_Logout ? 3
                   : inControlId == ID_StartGame ? 4 : 0;
        if (aWhich != 0) this->Call(EeWonStagingCall::Control, NULL, NULL, aWhich);
    }
    virtual void HandleComponentEvent(ComponentEvent* inEvent)
    {
        this->Call(EeWonStagingCall::ComponentEvent, inEvent, NULL, inEvent->mType);
    }
    intptr_t Call(EeWonStagingCall inCall, void* inA = NULL, void* inB = NULL, intptr_t inC = 0)
    { return ee_won_staging_call(this->mRust, inCall, inA, inB, inC, 0); }
};
struct LanLogic : NetLogic {
    LobbyRustDialog* mDirectConnectDialog = NULL; // 0x40
    LobbyRustDialog* mLanNameDialog = NULL; // 0x44
    LobbyRustDialog* mLanPortDialog = NULL; // 0x48
    LobbyRustDialog* mPasswordDialog = NULL; // 0x4c
    virtual void InitComponent(ComponentConfig* inConfig)
    {
        SafeGetDialog(inConfig, this->mDirectConnectDialog, EeWonLobbyDlgKind::DirectConnect, "DirectConnectDialog", "LanLogic.cpp", 105);
        SafeGetDialog(inConfig, this->mLanNameDialog, EeWonLobbyDlgKind::LanName, "LanNameDialog", "LanLogic.cpp", 106);
        SafeGetDialog(inConfig, this->mLanPortDialog, EeWonLobbyDlgKind::LanPort, "LanPortDialog", "LanLogic.cpp", 107);
        SafeGetDialog(inConfig, this->mPasswordDialog, EeWonLobbyDlgKind::Password, "PasswordDialog", "LanLogic.cpp", 108);
    }
    virtual void HandleControlEvent(int inControlId)
    {
        int aWhich = inControlId == ID_AddLANName ? 1 : inControlId == ID_ChangeLANBroadcast ? 2
                   : inControlId == ID_RefreshGameList ? 3 : inControlId == ID_ShowLanScreen ? 4 : 0;
        if (aWhich != 0) this->Call(EeWonLanCall::Control, NULL, NULL, aWhich);
    }
    virtual void HandleComponentEvent(ComponentEvent* inEvent)
    {
        this->Call(EeWonLanCall::ComponentEvent, inEvent, NULL, inEvent->mType);
    }
    virtual bool HostGame(LobbyGame* inGame) { return this->Call(EeWonLanCall::HostGame, inGame) != 0; }
    virtual bool JoinGame(LobbyGame* inGame) { return this->Call(EeWonLanCall::JoinGame, inGame) != 0; }
    virtual LobbyClient* GetMyClient() { return reinterpret_cast<LobbyClient*>(this->Call(EeWonLanCall::MyClient)); }
    virtual void SendToCaptain(const ByteBuffer* inBuffer)
    { this->CallMessage(EeWonLanCall::SendToCaptain, inBuffer, NULL); }
    virtual void BroadcastToGame(const ByteBuffer* inBuffer)
    { this->CallMessage(EeWonLanCall::BroadcastToGame, inBuffer, NULL); }
    virtual void SendToClient(LobbyClient* inClient, const ByteBuffer* inBuffer)
    { this->CallMessage(EeWonLanCall::SendToClient, inBuffer, inClient); }
    virtual void AdvertiseGame(LobbyGame* inGame) { this->Call(EeWonLanCall::AdvertiseGame, inGame); }
    virtual void RemoveGameAdvertisement(LobbyGame* inGame)
    { this->Call(EeWonLanCall::RemoveGameAdvertisement, inGame); }
    virtual void KickClient(LobbyClient* inClient, bool) { this->Call(EeWonLanCall::KickClient, NULL, inClient); }
    virtual void SetGameInProgress(bool) {}
    virtual void LeaveGame(LobbyGame*) { this->Call(EeWonLanCall::LeaveGame); }
    virtual void Suspend() {}
    virtual void Resume() { LobbyLeaveGame(0); }
    virtual void Logout() { this->Call(EeWonLanCall::Logout); }
    intptr_t Call(EeWonLanCall inCall, void* inA = NULL, void* inB = NULL, intptr_t inC = 0)
    { return ee_won_lan_call(this->mRust, inCall, inA, inB, inC, 0); }
    void CallMessage(EeWonLanCall inCall, const ByteBuffer* inMessage, LobbyClient* inClient)
    {
        ee_won_lan_call(this->mRust, inCall, const_cast<ByteBuffer*>(inMessage), inClient,
                        inMessage != NULL ? (intptr_t)inMessage->data() : 0,
                        inMessage != NULL ? (intptr_t)inMessage->length() : 0);
    }
    void HandleSocketClosed(AsyncOp* inOp) { this->Call(EeWonLanCall::SocketClosed, inOp); }
    void HandleNetworkTimer(AsyncOp* inOp) { this->Call(EeWonLanCall::NetworkTimer, inOp); }
    void HandleRecvMsg(AsyncOp* inOp) { this->Call(EeWonLanCall::RecvMsg, inOp); }
    void HandleDiscoveryTimer(AsyncOp* inOp) { this->Call(EeWonLanCall::DiscoveryTimer, inOp); }
    void HandleDiscoveryRecv(AsyncOp* inOp) { this->Call(EeWonLanCall::DiscoveryRecv, inOp); }
    void HandleAccept(AsyncOp* inOp) { this->Call(EeWonLanCall::Accept, inOp); }
    void HandlePeerAuthDone(AsyncOp* inOp) { this->Call(EeWonLanCall::PeerAuthDone, inOp); }
};
#undef ee_won_logic_call
#undef ee_won_dlglogic_call
#undef ee_won_routelogic_call
#undef ee_won_staging_call
#undef ee_won_lan_call
}

// ---- the new side: RustLogic's forwards as they are (WONLobbyLogic.h, LobbyLogic.cpp), logicshell.rs's routes
// through their numbers, each logged as the call logicshell.rs's `run` makes on the kind's object

namespace New {
using namespace WONAPI; // WonGuard, WonRethrow
// the spoiled route (-1: none): its index among the routes asked for in a run of all cases is not stable, so a route
// is named by its kind and virtual or completion
static int sSpoilKind = -1, sSpoilRow = -1; // the row: a virtual's number, or 100 + a completion's
static bool sSpoilHit;
static EeWonLogicShellRoute Spoil(EeWonLogicShellRoute r, int inKind, int inRow)
{
    if (inKind != sSpoilKind || inRow != sSpoilRow) return r;
    sSpoilHit = true;
    if (r.target <= 0) { r.target = 1; r.call = 1; } // an answer (or no completion): a call instead
    else if (r.args[0] != 0 || r.args[1] != 0 || r.args[2] != 0 || r.args[3] != 0) { r.args[0] += 1; r.args[1] += 1; r.args[2] += 1; r.args[3] += 1; }
    else r.call += 1;
    return r;
}
// logicshell.rs `run`: the route as the call on the kind's object (the dialog logic's four unused arguments are 0)
static intptr_t Run(const EeWonLogicShellRoute& r)
{
    if (r.target == 0) return r.answer;
    return LogCall(r.target, r.call, r.args[0], r.args[1], r.args[2], r.args[3]);
}
typedef intptr_t (*FindFn)(void*, void*, int32_t, EeWonLobbyDlgKind, const char*, const char*, int32_t);
// ee_won_logicshell_virtual, its statements over the routes' numbers
static intptr_t ShellVirtual(EeWonLogicShellKind inKind, EeWonLogicShellV v, intptr_t a, intptr_t b, intptr_t c, intptr_t d, void* inLogic, FindFn inFind)
{
    if (v == EeWonLogicShellV::InitComponent) {
        int32_t aSlot, aLine;
        EeWonLobbyDlgKind aDlgKind;
        const char *aName, *aFile;
        for (size_t i = 0; ee_won_logicshell_dialog(inKind, i, &aSlot, &aDlgKind, &aName, &aFile, &aLine); i++)
            if (inFind(inLogic, reinterpret_cast<void*>(a), aSlot, aDlgKind, aName, aFile, aLine) == kWonThrown) return kWonThrown;
    }
    EeWonLogicShellRoute r = Spoil(ee_won_logicshell_route(inKind, v, a, b, c, d), (int)inKind, (int)v);
    intptr_t anAnswer = Run(r);
    if (v == EeWonLogicShellV::Resume) return r.target == 0 && r.answer == 1;
    return anAnswer;
}
struct LobbyLogic {
    bool mKilled = false;
    virtual ~LobbyLogic() {}
    bool RemoveTrackedOp(AsyncOp* inOp) { Log("untrack " + N((intptr_t)inOp)); return sTracked; }
};
struct RustLogic;
static intptr_t ShellFindDialog(void* inLogic, void* inConfig, int32_t inSlot, EeWonLobbyDlgKind inKind, const char* inName, const char* inFile, int32_t inLine);
struct RustLogic : LobbyLogic {
    explicit RustLogic(EeWonLogicShellKind inKind) : mKind(inKind) {}
    virtual void KillHook() { this->V(EeWonLogicShellV::KillHook); }
    virtual void InitComponent(ComponentConfig* inConfig) { WonRethrow(this->V(EeWonLogicShellV::InitComponent, (intptr_t)inConfig)); }
    virtual void Reset() { this->V(EeWonLogicShellV::Reset); }
    virtual void HandleControlEvent(int inControlId) { this->V(EeWonLogicShellV::HandleControlEvent, inControlId); }
    virtual void HandleComponentEvent(ComponentEvent* inEvent) { this->V(EeWonLogicShellV::HandleComponentEvent, (intptr_t)inEvent, inEvent->mType); }
    bool HostGame(LobbyGame* inGame) { return this->V(EeWonLogicShellV::HostGame, (intptr_t)inGame) != 0; }
    bool JoinGame(LobbyGame* inGame) { return this->V(EeWonLogicShellV::JoinGame, (intptr_t)inGame) != 0; }
    LobbyClient* GetMyClient() { return reinterpret_cast<LobbyClient*>(this->V(EeWonLogicShellV::GetMyClient)); }
    void SendToCaptain(const ByteBuffer* inBuffer) { this->VMessage(EeWonLogicShellV::SendToCaptain, inBuffer, NULL); }
    void BroadcastToGame(const ByteBuffer* inBuffer) { this->VMessage(EeWonLogicShellV::BroadcastToGame, inBuffer, NULL); }
    void SendToClient(LobbyClient* inClient, const ByteBuffer* inBuffer) { this->VMessage(EeWonLogicShellV::SendToClient, inBuffer, inClient); }
    void AdvertiseGame(LobbyGame* inGame) { this->V(EeWonLogicShellV::AdvertiseGame, (intptr_t)inGame); }
    void RemoveGameAdvertisement(LobbyGame* inGame) { this->V(EeWonLogicShellV::RemoveGameAdvertisement, (intptr_t)inGame); }
    void KickClient(LobbyClient* inClient, bool inBan) { this->V(EeWonLogicShellV::KickClient, (intptr_t)inClient, inBan); }
    void SetGameInProgress(bool inFlag) { this->V(EeWonLogicShellV::SetGameInProgress, inFlag); }
    void LeaveGame(LobbyGame* inGame) { this->V(EeWonLogicShellV::LeaveGame, (intptr_t)inGame); }
    void Suspend() { this->V(EeWonLogicShellV::Suspend); }
    void Resume() { if (this->V(EeWonLogicShellV::Resume) != 0) LobbyLeaveGame(0); }
    void Logout() { this->V(EeWonLogicShellV::Logout); }
    intptr_t V(EeWonLogicShellV inVirtual, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0)
    {
        return ShellVirtual(this->mKind, inVirtual, a, b, c, d, this, ShellFindDialog);
    }
    void VMessage(EeWonLogicShellV inVirtual, const ByteBuffer* inMessage, LobbyClient* inClient)
    { this->V(inVirtual, (intptr_t)inMessage, (intptr_t)inClient, inMessage != NULL ? (intptr_t)inMessage->data() : 0, inMessage != NULL ? (intptr_t)inMessage->length() : 0); }
    EeWonLogicShellKind mKind;
    LobbyRustDialog* mDialog[6] = {};
};
static intptr_t ShellFindDialog(void* inLogic, void* inConfig, int32_t inSlot, EeWonLobbyDlgKind inKind, const char* inName, const char* inFile, int32_t inLine)
{
    return WON_GUARDED_V(SafeGetDialog(static_cast<ComponentConfig*>(inConfig), static_cast<RustLogic*>(inLogic)->mDialog[inSlot], inKind, inName, inFile, inLine));
}
// ShellCompletion::Run over ee_won_logicshell_done's statements
struct ShellCompletion {
    ShellCompletion(RustLogic* inLogic, EeWonLogicShellDone inDone, int inCode = 0) : mDone(inDone), mCode(inCode), mLogic(inLogic) {}
    bool mCheckTracker = false;
    void Run(AsyncOp* inOp)
    {
        EeWonLogicShellRoute r = Spoil(ee_won_logicshell_done_route(this->mLogic->mKind, this->mDone, this->mCode, (intptr_t)inOp), (int)this->mLogic->mKind, 100 + (int)this->mDone);
        if (r.target < 0) return;
        if (r.checked && !ee_won_logic_completion_runs(this->mLogic->mKilled, this->mCheckTracker, &Untrack, this->mLogic, inOp)) return;
        Run2(r);
    }
    static void Run2(const EeWonLogicShellRoute& r) { New::Run(r); }
    static bool Untrack(void* inLogic, void* inOp) { return static_cast<RustLogic*>(inLogic)->RemoveTrackedOp(static_cast<AsyncOp*>(inOp)); }
    EeWonLogicShellDone mDone;
    int mCode;
    RustLogic* mLogic;
};
}

// ---- the cases

static const int kIds[] = {0, 1, 200009, 200010, 200011, 200014, 200015, 200016, 200019, 200020, 200012, 199999, 100027, -1};
static ByteBuffer sBuffer = {"abcdef", 6};
static ComponentEvent sEvent = {29};
static ComponentConfig sConfig = {-1};
static int sNotifyCode;
static LobbyGame sGame;
static LobbyClient sClient;
static AsyncOp sOp;

static void Reset()
{
    sTrace.clear();
    sLookups = 0;
    sDialogs.clear();
}

// one kind's virtual on either side (inOrig NULL: the new side), the result logged
template <class O> static void RunVirtual(O* o, New::RustLogic* n, int v, int inArg)
{
    sConfig.mFailAt = inArg % 8 - 1;
    sEvent.mType = kIds[inArg % 14];
    try {
        switch ((EeWonLogicShellV)v) {
        case EeWonLogicShellV::KillHook: o ? o->KillHook() : n->KillHook(); break;
        case EeWonLogicShellV::InitComponent: o ? o->InitComponent(&sConfig) : n->InitComponent(&sConfig); break;
        case EeWonLogicShellV::Reset: o ? o->Reset() : n->Reset(); break;
        case EeWonLogicShellV::HandleControlEvent: o ? o->HandleControlEvent(kIds[inArg % 14]) : n->HandleControlEvent(kIds[inArg % 14]); break;
        case EeWonLogicShellV::HandleComponentEvent: o ? o->HandleComponentEvent(&sEvent) : n->HandleComponentEvent(&sEvent); break;
        default: break;
        }
    } catch (std::exception& e) {
        Log(std::string("threw ") + e.what());
    }
}
template <class O> static void RunNet(O* o, New::RustLogic* n, int v, int inArg)
{
    const ByteBuffer* aBuffer = (inArg & 1) ? &sBuffer : NULL;
    bool aFlag = (inArg & 2) != 0;
    switch ((EeWonLogicShellV)v) {
    case EeWonLogicShellV::HostGame: Log("= " + N(o ? o->HostGame(&sGame) : n->HostGame(&sGame))); break;
    case EeWonLogicShellV::JoinGame: Log("= " + N(o ? o->JoinGame(&sGame) : n->JoinGame(&sGame))); break;
    case EeWonLogicShellV::GetMyClient: Log("= " + N((intptr_t)(o ? o->GetMyClient() : n->GetMyClient()))); break;
    case EeWonLogicShellV::SendToCaptain: o ? o->SendToCaptain(aBuffer) : n->SendToCaptain(aBuffer); break;
    case EeWonLogicShellV::BroadcastToGame: o ? o->BroadcastToGame(aBuffer) : n->BroadcastToGame(aBuffer); break;
    case EeWonLogicShellV::SendToClient: o ? o->SendToClient(&sClient, aBuffer) : n->SendToClient(&sClient, aBuffer); break;
    case EeWonLogicShellV::AdvertiseGame: o ? o->AdvertiseGame(&sGame) : n->AdvertiseGame(&sGame); break;
    case EeWonLogicShellV::RemoveGameAdvertisement: o ? o->RemoveGameAdvertisement(&sGame) : n->RemoveGameAdvertisement(&sGame); break;
    case EeWonLogicShellV::KickClient: o ? o->KickClient(&sClient, aFlag) : n->KickClient(&sClient, aFlag); break;
    case EeWonLogicShellV::SetGameInProgress: o ? o->SetGameInProgress(aFlag) : n->SetGameInProgress(aFlag); break;
    case EeWonLogicShellV::LeaveGame: o ? o->LeaveGame(&sGame) : n->LeaveGame(&sGame); break;
    case EeWonLogicShellV::Suspend: o ? o->Suspend() : n->Suspend(); break;
    case EeWonLogicShellV::Resume: o ? o->Resume() : n->Resume(); break;
    case EeWonLogicShellV::Logout: o ? o->Logout() : n->Logout(); break;
    default: break;
    }
}

static int sCases, sMismatches;
static void Compare(const std::vector<std::string>& inOrig, const std::string& inWhat, bool inReport)
{
    sCases++;
    if (inOrig == sTrace) return;
    sMismatches++;
    if (!inReport || sMismatches > 12) return;
    printf("MISMATCH %s\n", inWhat.c_str());
    for (size_t i = 0; i < inOrig.size() || i < sTrace.size(); i++)
        printf("  orig %-60s new %s\n", i < inOrig.size() ? inOrig[i].c_str() : "-", i < sTrace.size() ? sTrace[i].c_str() : "-");
}
// the dialogs a side holds after InitComponent, in the members' order
static void LogDialogs(LobbyRustDialog* const* inDialogs, int inCount)
{
    for (int i = 0; i < inCount; i++) Log("dialog " + N(i) + " " + DialogName(inDialogs[i]));
}

// a kind's LobbyLogic virtuals (each with every argument set) and, for the network kinds, NetLogic's
template <class O> static void KindCases(EeWonLogicShellKind inKind, bool inNet, int inDialogCount, LobbyRustDialog* O::*const* inMembers, bool inReport)
{
    for (int v = (int)EeWonLogicShellV::KillHook; v <= (int)EeWonLogicShellV::Logout; v++) {
        bool aNetVirtual = v >= (int)EeWonLogicShellV::HostGame;
        if (aNetVirtual && !inNet) continue;
        for (int anArg = 0; anArg < 14; anArg++) {
            O anOrig;
            New::RustLogic aNew(inKind);
            Reset();
            if (aNetVirtual) RunNet<O>(&anOrig, NULL, v, anArg); else RunVirtual<O>(&anOrig, NULL, v, anArg);
            if (v == (int)EeWonLogicShellV::InitComponent) {
                std::vector<LobbyRustDialog*> aHeld;
                for (int i = 0; i < inDialogCount; i++) aHeld.push_back(anOrig.*inMembers[i]);
                LogDialogs(aHeld.data(), inDialogCount);
            }
            std::vector<std::string> aTrace = sTrace;
            Reset();
            if (aNetVirtual) RunNet<O>(NULL, &aNew, v, anArg); else RunVirtual<O>(NULL, &aNew, v, anArg);
            if (v == (int)EeWonLogicShellV::InitComponent) LogDialogs(aNew.mDialog, inDialogCount);
            Compare(aTrace, "kind " + N((int)inKind) + " virtual " + N(v) + " arg " + N(anArg), inReport);
        }
    }
}
// RunNet needs the NetLogic methods on every O it is instantiated with: the plain logics get none (never called)
namespace Orig {
struct NoNet {
    bool HostGame(LobbyGame*) { return false; }
    bool JoinGame(LobbyGame*) { return false; }
    LobbyClient* GetMyClient() { return NULL; }
    void SendToCaptain(const ByteBuffer*) {}
    void BroadcastToGame(const ByteBuffer*) {}
    void SendToClient(LobbyClient*, const ByteBuffer*) {}
    void AdvertiseGame(LobbyGame*) {}
    void RemoveGameAdvertisement(LobbyGame*) {}
    void KickClient(LobbyClient*, bool) {}
    void SetGameInProgress(bool) {}
    void LeaveGame(LobbyGame*) {}
    void Suspend() {}
    void Resume() {}
    void Logout() {}
};
struct AccountLogicT : AccountLogic, NoNet {};
struct InitLogicT : InitLogic, NoNet {};
struct DialogLogicT : DialogLogic, NoNet {};
struct StagingLogicT : StagingLogic, NoNet {};
}

// the completion's mCheckTracker (LobbyLogic::RunTrackedOp sets it)
template <class T> static void SetCheck(Orig::LogicCompletion<T>& c, bool inCheck) { c.mCheckTracker = inCheck; }
static void SetCheck(Orig::RoutingOpCompletion&, bool) {} // a LogicCompletionBase whose flag its Run never read

// a completion on both sides: the logic killed or not, the tracker checked or not, the op tracked or not
template <class O, class C> static void DoneCase(EeWonLogicShellKind inKind, EeWonLogicShellDone inDone, int inCode, C (*inMake)(O*), bool inReport)
{
    for (int aBits = 0; aBits < 8; aBits++) {
        O anOrig;
        New::RustLogic aNew(inKind);
        anOrig.mKilled = aNew.mKilled = (aBits & 1) != 0;
        sTracked = (aBits & 4) != 0;
        Reset();
        C aCompletion = inMake(&anOrig);
        SetCheck(aCompletion, (aBits & 2) != 0);
        aCompletion.Run(&sOp);
        std::vector<std::string> aTrace = sTrace;
        Reset();
        New::ShellCompletion aShell(&aNew, inDone, inCode);
        aShell.mCheckTracker = (aBits & 2) != 0;
        aShell.Run(&sOp);
        Compare(aTrace, "kind " + N((int)inKind) + " completion " + N((int)inDone) + " bits " + N(aBits), inReport);
    }
}
#define DONE(K, T, Member, D) \
    DoneCase<Orig::T, Orig::LogicCompletion<Orig::T> >(EeWonLogicShellKind::K, EeWonLogicShellDone::D, 0, \
        +[](Orig::T* o) { return Orig::LogicCompletion<Orig::T>(&Orig::T::Member, o); }, inReport)

static void AllCases(bool inReport)
{
    static LobbyRustDialog* Orig::AccountLogicT::*const kAccount[] = {&Orig::AccountLogicT::mCreditsDialog, &Orig::AccountLogicT::mTOUDialog};
    static LobbyRustDialog* Orig::InitLogicT::*const kInit[] = {&Orig::InitLogicT::mNetworkAdapterDialog};
    static LobbyRustDialog* Orig::RoutingLogic::*const kRouting[] = {&Orig::RoutingLogic::mAddFriendDialog, &Orig::RoutingLogic::mAdminActionDialog, &Orig::RoutingLogic::mBadUserDialog, &Orig::RoutingLogic::mCreateRoomDialog, &Orig::RoutingLogic::mRoomDialog, &Orig::RoutingLogic::mPasswordDialog};
    static LobbyRustDialog* Orig::StagingLogicT::*const kStaging[] = {&Orig::StagingLogicT::mCreateGameDialog};
    static LobbyRustDialog* Orig::LanLogic::*const kLan[] = {&Orig::LanLogic::mDirectConnectDialog, &Orig::LanLogic::mLanNameDialog, &Orig::LanLogic::mLanPortDialog, &Orig::LanLogic::mPasswordDialog};
    KindCases<Orig::AccountLogicT>(EeWonLogicShellKind::Account, false, 2, kAccount, inReport);
    KindCases<Orig::InitLogicT>(EeWonLogicShellKind::Init, false, 1, kInit, inReport);
    KindCases<Orig::DialogLogicT>(EeWonLogicShellKind::Dialog, false, 0, NULL, inReport);
    KindCases<Orig::RoutingLogic>(EeWonLogicShellKind::Routing, true, 6, kRouting, inReport);
    KindCases<Orig::StagingLogicT>(EeWonLogicShellKind::Staging, false, 1, kStaging, inReport);
    KindCases<Orig::LanLogic>(EeWonLogicShellKind::Lan, true, 4, kLan, inReport);
    DONE(Init, InitLogic, DNSCompletion, InitDns);
    DONE(Init, InitLogic, HTTPDocumentCompletion, InitDocument);
    DONE(Init, InitLogic, CrossPromotionImageCompletion, InitPromotionImage);
    DONE(Init, InitLogic, DirectoryCompletion, InitDirectory);
    DONE(Init, InitLogic, VersionCompletion, InitVersion);
    DONE(Init, InitLogic, AcceptCompletion, InitAccept);
    DONE(Init, InitLogic, FirewallCompletion, InitFirewall);
    DONE(Routing, RoutingLogic, GetUserStatusComplete, RouteUserStatus);
    DONE(Routing, RoutingLogic, QueryServerGroupsDone, RouteGroups);
    DONE(Routing, RoutingLogic, ConnectionClosed, RouteClosed);
    for (int aCode = 0; aCode < 40; aCode += 7)
        DoneCase<Orig::RoutingLogic, Orig::RoutingOpCompletion>(EeWonLogicShellKind::Routing, EeWonLogicShellDone::RouteNotify, sNotifyCode = aCode,
            +[](Orig::RoutingLogic* o) { return Orig::RoutingOpCompletion(sNotifyCode, o); }, inReport);
    DONE(Lan, LanLogic, HandleSocketClosed, LanSocketClosed);
    DONE(Lan, LanLogic, HandleNetworkTimer, LanNetworkTimer);
    DONE(Lan, LanLogic, HandleRecvMsg, LanRecvMsg);
    DONE(Lan, LanLogic, HandleDiscoveryTimer, LanDiscoveryTimer);
    DONE(Lan, LanLogic, HandleDiscoveryRecv, LanDiscoveryRecv);
    DONE(Lan, LanLogic, HandleAccept, LanAccept);
    DONE(Lan, LanLogic, HandlePeerAuthDone, LanPeerAuthDone);
    DONE(Dialog, DialogLogic, StatusOpComplete, DlgStatusOp);
}

int main()
{
    sCases = sMismatches = 0;
    AllCases(true);
    int aCases = sCases, aFailures = sMismatches;
    printf("won_shell36_test: %d cases, %d mismatches\n", aCases, aFailures);
    // every route spoiled in turn: the rows a case asks for (a kind's virtual, a kind's own completion)
    int aSpoiled = 0, aCaught = 0;
    for (int aKind = (int)EeWonLogicShellKind::Account; aKind <= (int)EeWonLogicShellKind::Lan; aKind++) {
        for (int aRow = 1; aRow < 120; aRow++) {
            New::sSpoilKind = aKind;
            New::sSpoilRow = aRow;
            New::sSpoilHit = false;
            sCases = sMismatches = 0;
            AllCases(false);
            if (!New::sSpoilHit) continue; // no case asks for the row (a NetLogic virtual of a plain logic)
            aSpoiled++;
            if (sMismatches != 0) aCaught++;
            else printf("NOT CAUGHT: kind %d row %d\n", aKind, aRow);
        }
    }
    New::sSpoilKind = New::sSpoilRow = -1;
    printf("won_shell36_test: %d of %d spoiled routes caught\n", aCaught, aSpoiled);
    return aFailures + (aSpoiled - aCaught);
}
