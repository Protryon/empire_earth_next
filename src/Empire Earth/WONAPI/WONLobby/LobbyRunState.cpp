// Running the lobby: start, modal message loop, exit status, suspend and resume (their bodies are Rust:
// ee_won_lobby lobbyrun.rs, R6 WON round 32)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyAccountLogic.h"
#include "rust/won_lobby/lobbyrun.h"

namespace WONAPI {
extern SmartPtr<Dialog> gLobbyFailureDialog;
extern bool gLobbyInitFailed;
void LobbyNetSuspend(); // calls the network logic's virtual at +0x44
void LobbyNetResume(); // calls the network logic's virtual at +0x48

// the functions' statements (lobbyrun.rs composes them, R6 WON round 32)
static intptr_t RunHost(EeWonRunOp inOp, intptr_t a)
{
    typedef EeWonRunOp Op;
    switch (inOp) {
    case Op::InitFailedWithDialog: return gLobbyInitFailed && gLobbyFailureDialog.get() != NULL;
    case Op::FailureWindow: return (intptr_t)gLobbyFailureDialog->GetRootWindow();
    case Op::WindowVisible: return reinterpret_cast<Window*>(a)->PlatformIsVisible() ? 1 : 0;
    case Op::WindowRunFailureDialog: reinterpret_cast<Window*>(a)->RunDialog(gLobbyFailureDialog.get()); return 0;
    case Op::HasData: return gLobbyData.get() != NULL;
    case Op::SetStatus: gLobbyData->mLobbyStatus = static_cast<LobbyStatus>(a); return 0;
    case Op::Status: return gLobbyData->mLobbyStatus;
    case Op::SetRunMode: gLobbyData->mRunMode = static_cast<LobbyRunMode>(a); return 0;
    case Op::RunMode: return gLobbyData->mRunMode;
    case Op::SetSuspended: gLobbyData->mSuspended = a != 0; return 0;
    case Op::LobbyWindow: return (intptr_t)GetLobbyWindow();
    case Op::MessageLoop: reinterpret_cast<WindowManager*>(a)->MessageLoop(); return 0;
    case Op::ExitMessageLoop: reinterpret_cast<WindowManager*>(a)->ExitMessageLoop(); return 0;
    case Op::BroadcastLobbyEvent: LobbyBroadcastLobbyEvent((int)a, true); return 0;
    case Op::MusicStop: LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->Stop(); return 0;
    case Op::LogonSoundStop: LOBBY_SOUND(LobbyGlobal_SierraLogon_Sound)->Stop(); return 0;
    case Op::PingsToDo: return LobbyGetNumPingsToDo();
    case Op::KillPings: LobbyKillPings(1); return 0;
    case Op::NetSuspend: LobbyNetSuspend(); return 0;
    case Op::MusicOn: return LobbyGetLobbyMusic() ? 1 : 0;
    case Op::MusicResume: LOBBY_SOUND(LobbyGlobal_LobbyMusic_Sound)->Resume(); return 0;
    case Op::NetResume: LobbyNetResume(); return 0;
    case Op::InitLogicInit: gLobbyData->mInitLogic->Call(EeWonLogicCall::InitInit); return 0;
    }
    return 0;
}

int LobbyRun() { return (int)ee_won_lobbyrun_call(RunHost, EeWonRunCall::Run, 0); }
void LobbyExit(int status) { ee_won_lobbyrun_call(RunHost, EeWonRunCall::Exit, status); }
void LobbySuspend() { ee_won_lobbyrun_call(RunHost, EeWonRunCall::Suspend, 0); }
void LobbyResume() { ee_won_lobbyrun_call(RunHost, EeWonRunCall::Resume, 0); }
// WON SDK lobby entry point: starts the lobby's start-up sequence (connect and log in)
void LobbyStart() { ee_won_lobbyrun_call(RunHost, EeWonRunCall::Start, 0); }
}
