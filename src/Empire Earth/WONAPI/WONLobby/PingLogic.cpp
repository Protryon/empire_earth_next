// Game pings over the lobby UDP manager: timing a game host's reply (optionally with its summary and
// details), queueing pings beyond the connection-speed limit, and answering pings for the hosted game.
// PingLogic and PingGameOp are Rust objects (rust/crates/ee_won_lobby/src/logics.rs, R6 WONLobby round 8): the queue,
// the request ids, the retries, the request's and reply's bytes and every decision. PingLogic is a RustLogic of the
// Ping kind since R6 WON round 39 (logicshell.rs routes its virtuals and completions); this unit keeps its maker and
// the host's ops: each one the original's calls on a C++ object.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyLogic.h"
#include "rust/won_net/status_codes.h"
#include "rust/won_lobby/logics.h"
#include "rust/won_net/buffer.h"
using namespace WONAPI;

namespace WONAPI {
void LobbyNotifyPingChange(LobbyGame*); // update game UI/status
}

// One ping of a game host, retried by PingLogic: the original's PingGameOp (0x98 bytes) is a UDPManagerSendRecvOp
// with a Rust body since R6 WON lobby and network round 36 (logics.rs EeWonPingOp: the game, the request id, the
// ping, the flags, the retries, the reply; its RunHook and CheckResponse)
namespace WONAPI {
// a reader over a Rust reader's state, the position written back
template <class F> static intptr_t WithReader(intptr_t inState, F inRead)
{
    ReadBuffer aReader(NULL, 0);
    aReader.mState = *reinterpret_cast<EeWonReadBuffer*>(inState);
    intptr_t aResult = inRead(aReader);
    *reinterpret_cast<EeWonReadBuffer*>(inState) = aReader.mState;
    return aResult;
}

static LobbyGame* G(intptr_t a) { return reinterpret_cast<LobbyGame*>(a); }
// a ping op's members (NULL for no op or another op)
static EeWonPingOp* Rec(AsyncOp* inOp) { return static_cast<EeWonPingOp*>(ee_won_udp_body_of(static_cast<RefCount*>(inOp), EE_WON_UDP_BODY_PING)); }
// a new op with one reference the Rust gives back (the original's SmartPtr local)
static intptr_t Held(AsyncOp* inOp)
{
    inOp->AddRef();
    return (intptr_t)inOp;
}

static intptr_t PingLogicHost(void* inObj, EeWonLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonLogicOp O;
    RustLogic* aLogic = static_cast<RustLogic*>(inObj);
    LobbyConfig* aConfig;
    switch (inOp) {
    case O::UdpManager: return (intptr_t)LobbyGetUDPManager();
    case O::SetDefaultRecv:
        reinterpret_cast<UDPManager*>(a)->SetDefaultCompletion(new ShellCompletion(aLogic, EeWonLogicShellDone::PingRecv));
        return 0;
    case O::EventGame: return LobbyEvtGet(reinterpret_cast<ComponentEvent*>(a), LobbyEvtField::Game);
    case O::EventFlag: return LobbyEvtGet(reinterpret_cast<ComponentEvent*>(a), LobbyEvtField::Flag);
    case O::RecvMsg: {
        const ByteBuffer* aBuffer = reinterpret_cast<UDPManagerRecvOp*>(a)->GetMsg();
        *reinterpret_cast<size_t*>(b) = aBuffer != NULL ? aBuffer->length() : 0;
        return (intptr_t)(aBuffer != NULL ? aBuffer->data() : NULL);
    }
    case O::MyGame: return (intptr_t)GetMyLobbyGame();
    case O::GameIsHost: return G(a)->GetIsHost();
    case O::GameType: return (intptr_t)G(a)->GetGameType();
    case O::GameInProgress: return G(a)->IsInProgress();
    case O::GameIsPinging: return G(a)->IsPinging();
    case O::GameQueryingDetails: return G(a)->IsQueryingDetails();
    case O::GameSetPing: G(a)->SetPing((unsigned short)b); return 0;
    case O::GameSetPingReplied: G(a)->SetPingReplied(true); return 0;
    case O::NoPingInProgress: aConfig = GetLobbyConfig(); return aConfig != NULL && aConfig->mNoPingInProgress;
    case O::PingCeiling: aConfig = GetLobbyConfig(); return aConfig != NULL ? (intptr_t)aConfig->mPingCeiling : 5000;
    case O::ConnectionSpeed: return LobbyGetConnectionSpeed();
    case O::ReplyNew: return (intptr_t)new WriteBuffer(3, true);
    case O::BufAppend: reinterpret_cast<WriteBuffer*>(a)->AppendBytes(reinterpret_cast<const void*>(b), (unsigned long)c); return 0;
    case O::WriteSummary: G(a)->WriteSummary(*reinterpret_cast<WriteBuffer*>(b)); return 0;
    case O::WriteDetails: G(a)->WriteDetails(*reinterpret_cast<WriteBuffer*>(b)); return 0;
    case O::ReplySend:
        reinterpret_cast<UDPManager*>(a)->Send(reinterpret_cast<WriteBuffer*>(b)->ToByteBuffer(true).get(),
                                               reinterpret_cast<UDPManagerRecvOp*>(c)->GetFromAddr());
        return 0;
    case O::BufFree: delete reinterpret_cast<WriteBuffer*>(a); return 0;
    // (the ping op's request bytes and time are its Rust body's: logics.rs EeWonPingOp, R6 WON round 40)
    case O::RunTracked:
        aLogic->RunTrackedOp(reinterpret_cast<AsyncOp*>(a), new ShellCompletion(aLogic, EeWonLogicShellDone::PingDone), (unsigned long)b);
        return 0;
    case O::ReadSummary: return WithReader(b, [a](ReadBuffer& r) -> intptr_t { return G(a)->ReadSummary(r) ? 1 : 0; });
    case O::ReadDetails: return WithReader(b, [a](ReadBuffer& r) -> intptr_t { G(a)->ReadDetails(r); return 0; });
    case O::BroadcastGameChanged: LobbyBroadcastEvent(GameChangedEvent(G(a), 2), true); return 0;
    case O::NotifyPing: LobbyNotifyPingChange(G(a)); return 0;
    case O::NumTracked: return aLogic->mTracker.GetNumTrack();
    case O::TrackedGame: return (intptr_t)ee_won_pingop_call(Rec(aLogic->mTracker.GetTrackedOp((int)a)), EeWonPingOpCall::Field, 5);
    case O::KillTracked: aLogic->mTracker.KillAll(); return 0;
    case O::NewPingOp: return Held(new UDPManagerSendRecvOp(ee_won_pingop_body(a, PingLogicHost, (unsigned short)b, (unsigned char)c), G(a)->GetAddress(), reinterpret_cast<UDPManager*>(d)));
    default: return 0;
    }
}

// The ping logic's Rust object over the shell
void* PingLogicMake(RustLogic* inSelf) { return ee_won_logic_new(EeWonLogicKind::Ping, inSelf, PingLogicHost); }
}
