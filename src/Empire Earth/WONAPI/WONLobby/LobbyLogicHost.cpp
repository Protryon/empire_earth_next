// The lobby logic objects' C++ host (NEW, no original address: units.EXTRA_UNITS): the makers of the logic modules
// that are Rust objects (StagingLogic: rust/crates/ee_won_lobby/src/stagingobj.rs) and ee_won_lobbycpp_call, the one
// table of their calls out (lobbyhost.rs EeWonLobbyOp): each op is a run of the original's C++ statements with no
// decision between them (LanLogic's in LanLogicHost.cpp). It replaces StagingLogic.cpp.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <time.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyStaging.h"
#include "ee/wonapi/WONLobbyLan.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONStatus.h"
#include "rust/won_lobby/lobbyhost.h"
#include "rust/won_lobby/staging.h"
#include "rust/won_lobby/stagingobj.h"
namespace WONAPI {
void LobbyNetSendToCaptain(const ByteBuffer*);
void LobbyNetBroadcastToGame(const ByteBuffer*);
void LobbyNetSendToClient(LobbyClient*, const ByteBuffer*);
void LobbyNetKickClient(LobbyClient*, bool);
void LobbyNetSetGameInProgress(bool);
void LobbyMessageBox(const GUIString&, const GUIString&);
void LobbyNetRemoveGameAdvertisement(LobbyGame*);
void LobbyNetLeaveGame(LobbyGame*);
}
using namespace WONAPI;

intptr_t LanLogicHostCall(RustLogic* inLan, EeWonLobbyOp inOp, void* inA, void* inB, intptr_t inC, intptr_t inD);
extern "C" intptr_t ee_won_lobbycpp_call(void* inLogic, EeWonLobbyOp inOp, void* inA, void* inB, intptr_t inC,
                                         intptr_t inD);

// ---- StagingLogic: the RustLogic of kind Staging (its virtuals and its dialog's lookup: logicshell.rs)

namespace WONAPI {
void* StagingLogicMake(RustLogic* inSelf) { return ee_won_staging_new_model(inSelf, ee_won_lobbycpp_call); }

// The room message from the buffer's position (ee_won reads its type and fixed fields)
void StagingHandleMessage(RustLogic* inStaging, ReadBuffer& inBuffer, LobbyClient* inClient)
{
    inStaging->Call(EeWonStagingCall::HandleMessage, &inBuffer, inClient, (intptr_t)(reinterpret_cast<const uint8_t*>(inBuffer.data()) + inBuffer.pos()), (intptr_t)(inBuffer.length() - inBuffer.pos()));
}
}

// ---- the calls out



// a message op's message: the C++ ByteBuffer, or one over the bytes Rust built, or none
const ByteBuffer* WONAPI::LobbyHostMessage(void* inBuffer, intptr_t inBytes, intptr_t inLength, ByteBufferPtr& outHold)
{
    if (inBuffer != NULL) return static_cast<const ByteBuffer*>(inBuffer);
    if (inBytes == 0) return NULL;
    outHold = new ByteBuffer(reinterpret_cast<const void*>(inBytes), (unsigned long)inLength, false);
    return outHold.get();
}

template <class T> static intptr_t Held(T* inObj)
{
    if (inObj != NULL) inObj->AddRef();
    return (intptr_t)inObj;
}

extern "C" intptr_t ee_won_lobbycpp_call(void* inLogic, EeWonLobbyOp inOp, void* inA, void* inB, intptr_t inC,
                                         intptr_t inD)
{
    // the C++ object the Rust one belongs to (a StagingLogic for the St ops; a LanLogic's ops go to LanLogicHost.cpp)
    RustLogic* st = static_cast<RustLogic*>(inLogic);
    LobbyGame* g = static_cast<LobbyGame*>(inA);
    LobbyClient* cl = static_cast<LobbyClient*>(inA);
    LobbyClient* clB = static_cast<LobbyClient*>(inB);
    ReadBuffer* buf = static_cast<ReadBuffer*>(inB);
    // a box's or status wait's texts: the title's and the details' resource rows (lobbyhost.rs `lobby_text`)
    const GUIString& aTitle = gLobbyResources[inC & 0xffff].mString;
    const GUIString& aDetails = gLobbyResources[(inC >> 16) & 0xffff].mString;
    ByteBufferPtr aHold;
    switch (inOp) {
    // StagingLogic's members
    case EeWonLobbyOp::StGame: return (intptr_t)st->Slot<LobbyGame>(kStagingGame);
    case EeWonLobbyOp::StMyClient: return (intptr_t)st->Slot<LobbyClient>(kStagingMyClient);
    case EeWonLobbyOp::StSetMyClientFromNet: st->mSlot[kStagingMyClient] = LobbyGetNetMyClient(); return (intptr_t)st->Slot<LobbyClient>(kStagingMyClient);
    case EeWonLobbyOp::StSetGame: st->mSlot[kStagingGame] = g; return 0;
    case EeWonLobbyOp::StJoinOpNew: st->mSlot[kStagingJoinOp] = new AsyncOp; return 0;
    case EeWonLobbyOp::StSendJoinRequest: st->CallMessage(EeWonStagingCall::SendToCaptain, st->Slot<LobbyGame>(kStagingGame)->GetJoinRequest().get(), NULL); return 0;
    // StCreateDialogRun, StTakeDialogGame and GameNewCopy are lobbyops.rs's over these (R6 WON round 32)
    case EeWonLobbyOp::StCreateSetType: st->mDialog[kStagingCreateGame]->LCall(EeWonLobbyDlgCall::SetGameType, (int)inC); return 0;
    case EeWonLobbyOp::StCreateReset: st->mDialog[kStagingCreateGame]->LCall(EeWonLobbyDlgCall::Reset, false); return 0;
    case EeWonLobbyOp::StCreateDo: return LobbyDoDialog(st->mDialog[kStagingCreateGame].get());
    case EeWonLobbyOp::StDialogGameHeld: return Held(st->mDialog[kStagingCreateGame]->GetGame().get());
    case EeWonLobbyOp::GameSetMyAddress: g->SetAddress(LobbyGetMyAddress((int)inC)); return 0;
    case EeWonLobbyOp::GameCreateHeld: return Held(LobbyGame::CreateGame(static_cast<LobbyGameType>(g->GetGameType())));
    case EeWonLobbyOp::GameCopyFrom: g->CopyFrom(static_cast<LobbyGame*>(inB)); return 0;
    case EeWonLobbyOp::StClientModerated: LobbyBroadcastEvent(ClientModeratedEvent(cl, (int)inC, st->Slot<LobbyGame>(kStagingGame)->GetName(), (int)(inD >> 1), 0, (inD & 1) != 0, gEmptyGUIString), true); return 0;
    // a game
    case EeWonLobbyOp::GameHold: return Held(g);
    case EeWonLobbyOp::GameRelease: g->Release(); return 0;
    case EeWonLobbyOp::GameCanReady: return g->CanReady();
    case EeWonLobbyOp::GameCanStartAndTry: return g->CanStartGame() && g->TryStartGame();
    case EeWonLobbyOp::GamePreNetCreate: return g->PreNetCreateGame();
    case EeWonLobbyOp::GamePostNetCreate: return g->PostNetCreateGame();
    case EeWonLobbyOp::GamePreNetJoin: return g->PreNetJoinGame();
    case EeWonLobbyOp::GamePostNetJoin: return g->PostNetJoinGame();
    case EeWonLobbyOp::GameHandlePlayerJoined: g->HandlePlayerJoined(clB); return 0;
    // GameResetReadyFlags and GameJoinReplySend are lobbyops.rs's over these (R6 WON round 36)
    case EeWonLobbyOp::GameClientsIterNew: return (intptr_t) new LobbyClientSetIterator(g->GetClients().get());
    case EeWonLobbyOp::ClientsIterHasMore: return static_cast<LobbyClientSetIterator*>(inA)->HasMore();
    case EeWonLobbyOp::ClientsIterNext: return (intptr_t) static_cast<LobbyClientSetIterator*>(inA)->Next(static_cast<LobbyClient**>(inB));
    case EeWonLobbyOp::ClientsIterFree: delete static_cast<LobbyClientSetIterator*>(inA); return 0;
    case EeWonLobbyOp::PlayerSetReady: static_cast<LobbyPlayer*>(inA)->SetReady(inC != 0); return 0;
    case EeWonLobbyOp::GameJoinReplyHeld: return LobbyReadGuard([&] { return Held(const_cast<ByteBuffer*>(g->GetJoinReply(*buf, reinterpret_cast<LobbyClient*>(inD)).get())); });
    case EeWonLobbyOp::StSendToClientBuf: st->CallMessage(EeWonStagingCall::SendToClient, static_cast<const ByteBuffer*>(inA), clB); return 0;
    case EeWonLobbyOp::GameHandleJoinReply: return LobbyReadGuard([&] { return g->HandleJoinReply(*buf); });
    case EeWonLobbyOp::GameHandleGameMessage: return LobbyReadGuard([&] { g->HandleGameMessage((unsigned char)inC, *buf, reinterpret_cast<LobbyClient*>(inD)); return 0; });
    case EeWonLobbyOp::GameShowStatus:
        LobbyMessageBox(LOBBY_STRING(GameStagingLogic_JoinGameErrorTitle_String), g->GetStatusString((short)inC));
        return 0;
    case EeWonLobbyOp::GameDirectConnect: LobbyDispatchEvent(ChangeDirectConnectEvent(1, g)); return 0;
    case EeWonLobbyOp::GameShowOptions: {
        LobbyOpenRustPopup(EeWonLobbyPopupKind::Game, (intptr_t)g);
        return 0;
    }
    // a client
    // ClientReadPlayer and ClientBroadcastJoined are lobbyops.rs's over these (R6 WON round 36)
    case EeWonLobbyOp::PlayerCreateHeld: return Held(LobbyPlayer::CreatePlayer());
    case EeWonLobbyOp::PlayerReadFrom: return LobbyReadGuard([&] { return static_cast<LobbyPlayer*>(inA)->ReadFromBuffer(*buf); });
    case EeWonLobbyOp::ClientSetPlayer: cl->SetPlayer(static_cast<LobbyPlayer*>(inB)); return 0;
    case EeWonLobbyOp::WbufNew: return (intptr_t) new WriteBuffer(0, true);
    case EeWonLobbyOp::WbufAppend: static_cast<WriteBuffer*>(inA)->AppendBytes(reinterpret_cast<const void*>(inC), (unsigned long)inD); return 0;
    case EeWonLobbyOp::ClientWritePlayer: cl->GetPlayer()->WriteToBuffer(*static_cast<WriteBuffer*>(inB)); return 0;
    case EeWonLobbyOp::StBroadcastWbuf: st->CallMessage(EeWonStagingCall::BroadcastToGame, static_cast<WriteBuffer*>(inA)->ToByteBuffer(true).get(), NULL); return 0;
    case EeWonLobbyOp::WbufFree: delete static_cast<WriteBuffer*>(inA); return 0;
    case EeWonLobbyOp::ClientChanged: LobbyBroadcastEvent(PlayerChangedEvent(cl, (int)inC), true); return 0;
    case EeWonLobbyOp::ClientTeamChanged: LobbyBroadcastEvent(PlayerTeamChangedEvent(cl, (int)inC), true); return 0;
    // a message, a read buffer, events
    case EeWonLobbyOp::BufSkip: buf->SetPos((int)(buf->pos() + inC)); return 0;
    case EeWonLobbyOp::HandleBytes: StagingHandleBytes(st, ReadBuffer(reinterpret_cast<const void*>(inC), (unsigned long)inD), clB); return 0;
    case EeWonLobbyOp::NetSendToCaptain: LobbyNetSendToCaptain(LobbyHostMessage(inA, inC, inD, aHold)); return 0;
    case EeWonLobbyOp::NetBroadcastToGame: LobbyNetBroadcastToGame(LobbyHostMessage(inA, inC, inD, aHold)); return 0;
    case EeWonLobbyOp::NetSendToClient: LobbyNetSendToClient(clB, LobbyHostMessage(inA, inC, inD, aHold)); return 0;
    case EeWonLobbyOp::NetKickClient: LobbyNetKickClient(clB, inC != 0); return 0;
    case EeWonLobbyOp::NetRemoveGameAdvertisement: LobbyNetRemoveGameAdvertisement(g); return 0;
    case EeWonLobbyOp::NetHostGame: return LobbyNetHostGame(g);
    case EeWonLobbyOp::NetJoinGame: return LobbyNetJoinGame(g);
    case EeWonLobbyOp::NetAdvertiseGame: LobbyNetAdvertiseGame(); return 0;
    // the lobby
    case EeWonLobbyOp::BroadcastLobbyEvent: LobbyBroadcastLobbyEvent((int)inC, true); return 0;
    case EeWonLobbyOp::MessageBox: LobbyMessageBox(aTitle, aDetails); return 0;
    case EeWonLobbyOp::YesNo: return LobbyDoYesNoDialog(aTitle, aDetails, true);
    // RunWaitOp is lobbyops.rs's over these and LanTimerAlloc (R6 WON round 36)
    case EeWonLobbyOp::RunOpWithStatusMode: LobbyRunOpWithStatus(static_cast<AsyncOp*>(static_cast<RefCount*>(inA)), aTitle, aDetails, (int)inD, 500); return 0;
    case EeWonLobbyOp::OpIsKilled: return static_cast<AsyncOp*>(static_cast<RefCount*>(inA))->IsKilled();
    case EeWonLobbyOp::PlaySound: LobbyPlaySound(gLobbyResources[inC].mSound.get()); return 0; // a resource row
    case EeWonLobbyOp::FirewallTestDone: return LobbyIsFirewallTestDone();
    case EeWonLobbyOp::BehindFirewall: return LobbyIsBehindFirewall();
    case EeWonLobbyOp::GameScreenType: return LobbyGetGameScreenType();
    // the statements of the cases lobbyops.rs answers (R6 WONLobby round 29: the join op, the logout, leaving a game,
    // the game's progress and details, the join without a ping)
    case EeWonLobbyOp::StJoinOpIs: return st->mSlot[kStagingJoinOp].get() != NULL;
    case EeWonLobbyOp::StJoinOpGet: return (intptr_t)st->Slot<AsyncOp>(kStagingJoinOp);
    case EeWonLobbyOp::StJoinOpFinishSuccess: st->Slot<AsyncOp>(kStagingJoinOp)->Finish(WS_Success);
        return 0;
    case EeWonLobbyOp::StJoinOpClear: st->mSlot[kStagingJoinOp] = NULL;
        return 0;
    case EeWonLobbyOp::StJoinOpKillNow: st->Slot<AsyncOp>(kStagingJoinOp)->Kill(true);
        return 0;
    case EeWonLobbyOp::RunOpWithStatusRows: LobbyRunOpWithStatus(static_cast<AsyncOp*>(static_cast<RefCount*>(inA)), aTitle, aDetails, 1, 0);
        return 0;
    case EeWonLobbyOp::MessageBoxRows: LobbyMessageBox(aTitle, aDetails);
        return 0;
    case EeWonLobbyOp::StCreateDialogResetAll: st->mDialog[kStagingCreateGame]->LCall(EeWonLobbyDlgCall::Reset, true);
        return 0;
    case EeWonLobbyOp::NetLogout: LobbyNetLogout();
        return 0;
    case EeWonLobbyOp::KillPings: LobbyKillPings(0);
        return 0;
    case EeWonLobbyOp::ShowLoginScreen: LobbyShowLoginScreen();
        return 0;
    case EeWonLobbyOp::SaveGlobalPrefs: LobbySaveGlobalPrefs(false);
        return 0;
    case EeWonLobbyOp::SaveUserPrefs: LobbySaveUserPrefs(false);
        return 0;
    case EeWonLobbyOp::LogonSoundStop: LOBBY_SOUND(LobbyGlobal_SierraLogon_Sound)->Stop();
        return 0;
    case EeWonLobbyOp::GameClearPlayers: g->ClearPlayers();
        return 0;
    case EeWonLobbyOp::StClearMyClient: st->mSlot[kStagingMyClient] = NULL;
        return 0;
    case EeWonLobbyOp::GamePreNetLeave: g->PreNetLeaveGame();
        return 0;
    case EeWonLobbyOp::NetLeaveGame: LobbyNetLeaveGame(g);
        return 0;
    case EeWonLobbyOp::GamePostNetLeave: g->PostNetLeaveGame();
        return 0;
    case EeWonLobbyOp::GameSetInProgress: g->SetInProgress(inC != 0);
        return 0;
    case EeWonLobbyOp::NetSetGameInProgress: LobbyNetSetGameInProgress(inC != 0);
        return 0;
    case EeWonLobbyOp::LanPingGameNotify: LobbyPingGame(g, true);
        return 0;
    case EeWonLobbyOp::QueryGameDetails: LobbyQueryGameDetails(g);
        return 0;
    case EeWonLobbyOp::GameSetShowDetails: g->SetShowDetails(inC != 0);
        return 0;
    case EeWonLobbyOp::LobbyConfigGet: return (intptr_t)GetLobbyConfig();
    case EeWonLobbyOp::ConfigAllowJoinWithoutPing: return static_cast<LobbyConfig*>(inA)->mAllowJoinGameWithoutPing;
    case EeWonLobbyOp::Exit: LobbyExit((int)inC); return 0;
    case EeWonLobbyOp::Now: return (intptr_t)time(NULL);
    default: return LanLogicHostCall(static_cast<RustLogic*>(inLogic), inOp, inA, inB, inC, inD);
    }
}
