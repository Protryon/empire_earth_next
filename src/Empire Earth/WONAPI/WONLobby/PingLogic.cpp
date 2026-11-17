// Game pings over the lobby UDP manager: timing a game host's reply (optionally with its summary and
// details), queueing pings beyond the connection-speed limit, and answering pings for the hosted game.
// PingLogic and PingGameOp are Rust objects (rust/crates/ee_won_lobby/src/logics.rs, R6 WONLobby round 8): the queue,
// the request ids, the retries, the request's and reply's bytes and every decision. This unit keeps the shells
// (the tracker, the completions, the op's game) and the host's ops: each one the original's calls on a C++ object.
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
#include "ee/wonapi/WONLobbyPing.h"
#include "ee/wonapi/WONStatus.h"
#include "rust/ee_won.h"
using namespace WONAPI;

namespace WONAPI {
void LobbyNotifyPingChange(LobbyGame*); // update game UI/status
intptr_t PingLogicHost(void* inObj, EeWonLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
}

// One ping of a game host, retried by PingLogic (global namespace in the original's RTTI)
class PingGameOp : public WONAPI::UDPManagerSendRecvOp {
public:
    PingGameOp(WONAPI::LobbyGame* inGame, unsigned short inRequestId, unsigned char inFlags)
        : mGame(inGame), mRust(ee_won_pingop_new(this, WONAPI::PingLogicHost, inRequestId, inFlags))
    {
        this->SetDestAddr(this->mGame->GetAddress());
    }
    virtual ~PingGameOp() { ee_won_pingop_free(this->mRust); }
    virtual void RunHook() { ee_won_pingop_call(this->mRust, EeWonPingOpCall::RunHook, (intptr_t)this->mGame.get()); }
    virtual WONAPI::WONStatus CheckResponse(WONAPI::ReadBuffer& inBuffer)
    {
        int64_t aStatus = ee_won_pingop_call(this->mRust, EeWonPingOpCall::CheckResponse, (intptr_t)&inBuffer.mState);
        if (aStatus == EE_WON_LOGIC_THREW)
            throw WONAPI::ReadBufferException("Attempt to read past end of buffer.");
        return (WONAPI::WONStatus)aStatus;
    }
    WONAPI::SmartPtr<WONAPI::LobbyGame> mGame; // 0x88
    EeWonPingOp* mRust; // the request id, the ping, the flags, the retries, the reply
};

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

intptr_t PingLogicHost(void* inObj, EeWonLogicOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonLogicOp O;
    PingLogic* aLogic = static_cast<PingLogic*>(inObj);
    LobbyConfig* aConfig;
    switch (inOp) {
    case O::AddRef: reinterpret_cast<RefCount*>(a)->AddRef(); return 0;
    case O::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case O::OpStatus: return (int)reinterpret_cast<AsyncOp*>(a)->GetStatus();
    case O::OpKilled: return reinterpret_cast<AsyncOp*>(a)->IsKilled();
    case O::UdpManager: return (intptr_t)LobbyGetUDPManager();
    case O::SetDefaultRecv:
        reinterpret_cast<UDPManager*>(a)->SetDefaultCompletion(
            new LogicCompletion<PingLogic>(&PingLogic::HandleUDPRecv, aLogic));
        return 0;
    case O::EventGame: return (intptr_t)reinterpret_cast<PingGameEvent*>(a)->mGame.get();
    case O::EventFlag: return reinterpret_cast<PingGameEvent*>(a)->mFlag;
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
    case O::RequestSend: {
        PingGameOp* anOp = static_cast<PingGameOp*>(inObj);
        WriteBuffer aBuffer(0, true);
        aBuffer.AppendBytes(reinterpret_cast<const void*>(a), (unsigned long)b);
        anOp->SetOutgoing(aBuffer.ToByteBuffer(true));
        anOp->UDPManagerSendRecvOp::RunHook();
        return 0;
    }
    case O::Elapsed: {
        unsigned long aStart = static_cast<PingGameOp*>(inObj)->GetStartTime();
        return (intptr_t)(unsigned long)(WONTickCount() - aStart);
    }
    case O::OpGame: return (intptr_t)static_cast<PingGameOp*>(reinterpret_cast<AsyncOp*>(a))->mGame.get();
    case O::OpRecord: return (intptr_t)static_cast<PingGameOp*>(reinterpret_cast<AsyncOp*>(a))->mRust;
    case O::RunTracked:
        aLogic->RunTrackedOp(reinterpret_cast<AsyncOp*>(a), new LogicCompletion<PingLogic>(&PingLogic::PingGameDone, aLogic),
                             (unsigned long)b);
        return 0;
    case O::ReadSummary: return WithReader(b, [a](ReadBuffer& r) -> intptr_t { return G(a)->ReadSummary(r) ? 1 : 0; });
    case O::ReadDetails: return WithReader(b, [a](ReadBuffer& r) -> intptr_t { G(a)->ReadDetails(r); return 0; });
    case O::BroadcastGameChanged: LobbyBroadcastEvent(new GameChangedEvent(G(a), 2), true); return 0;
    case O::NotifyPing: LobbyNotifyPingChange(G(a)); return 0;
    case O::NumTracked: return aLogic->mTracker.GetNumTrack();
    case O::TrackedGame: {
        PingGameOp* anOp = dynamic_cast<PingGameOp*>(aLogic->mTracker.GetTrackedOp((int)a));
        return (intptr_t)(anOp != NULL ? anOp->mGame.get() : NULL);
    }
    case O::KillTracked: aLogic->mTracker.KillAll(); return 0;
    case O::NewPingOp: {
        PingGameOp* anOp = new PingGameOp(G(a), (unsigned short)b, (unsigned char)c);
        anOp->AddRef();
        anOp->SetUDPManager(reinterpret_cast<UDPManager*>(d));
        return (intptr_t)static_cast<AsyncOp*>(anOp);
    }
    default: return 0;
    }
}

PingLogic::PingLogic() : mRust(ee_won_logic_new(EeWonLogicKind::Ping, this, PingLogicHost)) {}

PingLogic::~PingLogic() { ee_won_logic_free(this->mRust); }

void PingLogic::InitComponent(ComponentConfig*) { this->Call(EeWonLogicCall::PingInit); }

void PingLogic::HandleComponentEvent(ComponentEvent* inEvent)
{
    this->Call(EeWonLogicCall::PingEvent, (intptr_t)inEvent, inEvent->mType);
}

void PingLogic::HandleUDPRecv(AsyncOp* inOp) { this->Call(EeWonLogicCall::PingRecv, (intptr_t)inOp); }

void PingLogic::PingGameDone(AsyncOp* inOp) { this->Call(EeWonLogicCall::PingDone, (intptr_t)inOp); }

int PingLogic::GetNumPingsToDo() { return (int)this->Call(EeWonLogicCall::PingNumToDo); }

void PingLogic::PingGame(LobbyGame* inGame, bool inDetails, bool inForce)
{
    this->Call(EeWonLogicCall::PingGame, (intptr_t)inGame, inDetails, inForce);
}

void PingLogic::KillPings(int) { this->Call(EeWonLogicCall::PingKill); }
}
