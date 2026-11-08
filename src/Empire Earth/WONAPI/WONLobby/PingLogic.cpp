// Game pings over the lobby UDP manager: timing a game host's reply (optionally with its summary and
// details), queueing pings beyond the connection-speed limit, and answering pings for the hosted game
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
using namespace WONAPI;

// One ping of a game host, retried by PingLogic (global namespace in the original's RTTI)
class PingGameOp : public WONAPI::UDPManagerSendRecvOp {
public:
    PingGameOp(WONAPI::LobbyGame*, unsigned short, unsigned char);

    virtual void RunHook();
    virtual WONAPI::WONStatus CheckResponse(WONAPI::ReadBuffer&);
    WONAPI::SmartPtr<WONAPI::LobbyGame> mGame; // 0x88
    unsigned short mRequestId; // 0x8c
    unsigned short mPing; // 0x8e, set by RunHook, not by the constructor
    unsigned char mReplyFlags; // 0x90
    unsigned short mRetries; // 0x92
    WONAPI::ByteBufferPtr mReply; // 0x94
};

// FUNCTION: WON 0x007e5ed0
// Creates a ping of a game's address with a request id and the summary/details flags to ask for
PingGameOp::PingGameOp(LobbyGame* inGame, unsigned short inRequestId,
                       unsigned char inFlags)
{
    this->mGame = inGame;
    this->mRetries = 0;
    this->mRequestId = inRequestId;
    this->mReplyFlags = inFlags;
    this->mAddress = this->mGame->mAddress;
}

// FUNCTION: WON 0x007e6240
// Builds the ping request (id, game type, flags), marks no reply yet and sends it
void PingGameOp::RunHook()
{
    WriteBuffer aBuffer(0, true);
    aBuffer.AppendByte(7);
    aBuffer.AppendShort(this->mRequestId);
    aBuffer.AppendByte(this->mGame->GetGameType());
    aBuffer.AppendByte(this->mReplyFlags);
    this->mOutgoing = aBuffer.ToByteBuffer(true);
    this->mPing = 0xffff;
    UDPManagerSendRecvOp::RunHook();
}

// FUNCTION: WON 0x007e6360
// Accepts the reply to this request: records the ping time and keeps any summary/details data
WONStatus PingGameOp::CheckResponse(ReadBuffer& inBuffer)
{
    if (inBuffer.ReadByte() != 8)
        return WS_None; // not a reply to this request
    if (inBuffer.ReadShort() != this->mRequestId)
        return WS_None; // the signed ReadShort is compared with the unsigned member
    unsigned long aStart = this->mStartTime;
    this->mPing = GetTickCount() - aStart;
    this->mReplyFlags = 0;
    if (!(inBuffer.Available() > 0))
        return WS_Success;
    this->mReplyFlags = inBuffer.ReadByte();
    unsigned long aLength = inBuffer.Available();
    this->mReply = new ByteBuffer(inBuffer.ReadBytes(aLength), aLength, false);
    return WS_Success;
}

namespace WONAPI {
UDPManager* LobbyGetUDPManager();
unsigned short LobbyGetConnectionSpeed(); // most pings in flight at once (0: no limit); callers use the low 16 bits
void LobbyNotifyPingChange(LobbyGame*); // update game UI/status

// FUNCTION: WON 0x007e6490
// Creates the ping logic with request ids starting at 0
PingLogic::PingLogic()
{
    this->mNextRequestId = 0;
}

// FUNCTION: WON 0x007e6520
// Destroys the ping logic (members only)
PingLogic::~PingLogic() {}

// FUNCTION: WON 0x007e65d0
// Routes unsolicited UDP packets to the ping request handler
void PingLogic::InitComponent(ComponentConfig*)
{
    UDPManager* aManager = LobbyGetUDPManager();
    if (aManager != NULL)
        aManager->mDefaultCompletion = new LogicCompletion<PingLogic>(&PingLogic::HandleUDPRecv, this);
}

// FUNCTION: WON 0x007e6680
// Pings a game when asked by a ping-game event
void PingLogic::HandleComponentEvent(ComponentEvent* inEvent)
{
    if (inEvent->mType == LobbyEvent_PingGame) {
        PingGameEvent* anEvent = static_cast<PingGameEvent*>(inEvent);
        this->PingGame(anEvent->mGame.get(), anEvent->mFlag, false);
    }
}

// FUNCTION: WON 0x007e66a0
// Host: answers a ping for the hosted game type with the requested summary/details (not in progress if barred)
void PingLogic::HandlePingRequest(ReadBuffer& inBuffer, const IPAddr& inAddress)
{
    LobbyGame* aGame = GetMyLobbyGame();
    if (aGame == NULL || !aGame->mIsHost)
        return;
    unsigned short anId = inBuffer.ReadShort();
    unsigned char aKind = inBuffer.ReadByte();
    if (aGame->GetGameType() != aKind)
        return;
    if (aGame->mInProgress) {
        LobbyConfig* aConfig = GetLobbyConfig();
        if (aConfig != NULL && aConfig->mNoPingInProgress)
            return;
    }
    WriteBuffer aBuffer(3, true);
    aBuffer.AppendByte(8);
    aBuffer.AppendShort(anId);
    unsigned char aFlags = inBuffer.ReadByte() & 3;
    aBuffer.AppendByte(aFlags);
    if (aFlags & 1)
        aGame->WriteSummary(aBuffer);
    if (aFlags & 2)
        aGame->WriteDetails(aBuffer);
    UDPManager* aManager = LobbyGetUDPManager();
    if (aManager != NULL)
        aManager->Send(aBuffer.ToByteBuffer(true).get(), inAddress);
}

// FUNCTION: WON 0x007e6810
// Dispatches an unsolicited UDP packet; only ping requests are handled
void PingLogic::HandleUDPRecv(AsyncOp* inOp)
{
    UDPManagerRecvOp* anOp = static_cast<UDPManagerRecvOp*>(inOp);
    if (anOp->GetStatus() != WS_Success || anOp->mKilled)
        return;
    const ByteBuffer* aBuffer = anOp->mReceivedData.get();
    ReadBuffer aReadBuffer(aBuffer != NULL ? aBuffer->data() : NULL,
                          aBuffer != NULL ? aBuffer->length() : 0);
    const IPAddr& anAddr = anOp->mAddress;
    try {
        int aType = (char)aReadBuffer.ReadByte();
        switch (aType) {
        case 7: this->HandlePingRequest(aReadBuffer, anAddr); break;
        }
    }
    catch (ReadBufferException&) {}
}

// FUNCTION: WON 0x007e68b0
// Retries an unanswered ping up to 3 times, else records the result and starts queued pings up to the limit
void PingLogic::PingGameDone(AsyncOp* inOp)
{
    PingGameOp* anOp = static_cast<PingGameOp*>(inOp);
    if (anOp->mKilled)
        return;
    unsigned short aPing = anOp->mPing;
    LobbyGame* aGame = anOp->mGame.get();
    if (aPing != 0xffff)
        aGame->mPingReplied = true;
    else if (anOp->mRetries < 3) {
        ++anOp->mRetries;
        LobbyConfig* aConfig = GetLobbyConfig();
        unsigned long aTimeout = aConfig != NULL ? aConfig->mPingCeiling : 5000;
        this->RunTrackedOp(anOp, new LogicCompletion<PingLogic>(&PingLogic::PingGameDone, this), aTimeout);
        return;
    }
    aGame->SetPing(aPing);
    if (anOp->mReply.get() != NULL) {
        ReadBuffer aBuffer(anOp->mReply->data(), anOp->mReply->length());
        unsigned char aFlags = anOp->mReplyFlags;
        bool aReadOk = true;
        if (aFlags & 1)
            aReadOk = aGame->ReadSummary(aBuffer) != 0;
        if ((aFlags & 2) && aReadOk)
            aGame->ReadDetails(aBuffer);
    }
    LobbyBroadcastEvent(new GameChangedEvent(aGame, 2), true);
    LobbyNotifyPingChange(aGame);
    int aMaximum = LobbyGetConnectionSpeed();
    while (!this->mPending.empty()) {
        if (static_cast<long>(this->mTracker.mMaps.mEntries.size()) >= aMaximum && aMaximum != 0)
            break;
        SmartPtr<LobbyGame> aPendingGame = this->mPending.front();
        this->mPending.pop_front();
        if (!aPendingGame->IsPinging())
            continue;
        if (aPendingGame->IsQueryingDetails())
            continue;
        aPendingGame->SetPing(0xfffe);
        this->PingGame(aPendingGame.get(), false, true);
    }
}

// FUNCTION: WON 0x007e6b40
// Returns the number of queued plus running pings
int PingLogic::GetNumPingsToDo()
{
    return this->mPending.size() + this->mTracker.mMaps.mEntries.size();
}

// FUNCTION: WON 0x007e6b50
// Pings a game (with details on request), or queues it when the connection-speed limit is reached
void PingLogic::PingGame(LobbyGame* inGame, bool inDetails, bool inForce)
{
    if (inGame == NULL)
        return;
    if (inGame->mInProgress) {
        LobbyConfig* aConfig = GetLobbyConfig();
        if (aConfig != NULL && aConfig->mNoPingInProgress)
            return;
    }
    if (inDetails) {
        if (inGame->IsQueryingDetails())
            return;
    }
    else if (inGame->IsPinging())
        return;
    if (!inForce && !inDetails) {
        unsigned short aMaximum = LobbyGetConnectionSpeed();
        if (static_cast<long>(this->mTracker.mMaps.mEntries.size()) >= aMaximum && aMaximum > 0) {
            inGame->mPing = 0xfffd;
            LobbyNotifyPingChange(inGame);
            this->mPending.push_back(inGame);
            return;
        }
    }
    UDPManager* aManager = LobbyGetUDPManager();
    if (aManager == NULL)
        return;
    inGame->SetPing(inDetails ? 0xfffc : 0xfffd);
    LobbyNotifyPingChange(inGame);
    unsigned char aFlags = 0;
    if (inGame->GetGameType() == 3)
        aFlags = 1;
    if (inDetails)
        aFlags |= 2;
    SmartPtr<PingGameOp> anOp = new PingGameOp(inGame, this->mNextRequestId++, aFlags);
    anOp->SetUDPManager(aManager);
    LobbyConfig* aConfig = GetLobbyConfig();
    unsigned long aTimeout = aConfig != NULL ? aConfig->mPingCeiling : 5000;
    this->RunTrackedOp(anOp.get(), new LogicCompletion<PingLogic>(&PingLogic::PingGameDone, this), aTimeout);
}

// FUNCTION: WON 0x007e6db0
// Cancels every queued and running ping, marking those games as not pinged
void PingLogic::KillPings(int)
{
    std::list<SmartPtr<LobbyGame> >::iterator aGameItr = this->mPending.begin();
    while (true) {
        if (aGameItr == this->mPending.end()) break;
        (*aGameItr)->SetPing(0xfffe);
        ++aGameItr;
    }
    AsyncOpTrackerMaps::EntryMap::iterator anItr;
    for (anItr = this->mTracker.mMaps.mEntries.begin(); anItr != this->mTracker.mMaps.mEntries.end(); ++anItr) {
        PingGameOp* anOp = dynamic_cast<PingGameOp*>(anItr->second->mOp.get());
        if (anOp != NULL)
            anOp->mGame->SetPing(0xfffe);
    }
    this->mPending.clear();
    this->mTracker.KillAll();
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007e6110
// (public: virtual __thiscall PingGameOp::~PingGameOp(void)    [??1PingGameOp@@UAE@XZ])
// TEMPLATE: WON 0x007e6500
// (public: virtual void * __thiscall WONAPI::PingLogic::`scalar deleting destructor'(unsigned int)    [??_GPingLogic@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007e60f0
// (public: virtual void * __thiscall PingGameOp::`scalar deleting destructor'(unsigned int)    [??_GPingGameOp@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f1470
// (public: virtual void * __thiscall WONAPI::LogicCompletion<class WONAPI::PingLogic>::`scalar deleting destructor'(unsigned int)    [??_G?$LogicCompleti)
// TEMPLATE: WON 0x007f15d0
// (public: virtual __thiscall WONAPI::LogicCompletion<class WONAPI::PingLogic>::~LogicCompletion<class WONAPI::PingLogic>(void)    [??1?$LogicCompletion@)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f1b00
// (public: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::LobbyGame>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::LobbyGame>)
// TEMPLATE: WON 0x007f26a0
// (protected: void __thiscall std::list<class WONAPI::SmartPtr<class WONAPI::LobbyGame>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::LobbyGa)
