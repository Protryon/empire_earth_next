// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// The Empire Earth lobby game's shell and host (R6 WONLobby round 5): EELobbyGame and its player slots are one Rust
// object (rust/crates/ee_won_lobby/src/eegame.rs: the options, the slots, the counts, the file transfer, the details'
// player names, every method and hook). This unit is the shell (the class's methods and virtual hooks forward; a hook
// whose read threw throws ReadBufferException again for the lobby's handler), the slot view's methods, and the host:
// each op the original's statements on a buffer, a client, a player, the file transfer, the lobby or a global, with
// no decision. A read op catches the ReadBufferException it threw and answers INTPTR_MIN. The saved game's transfer
// (EELobbyFileTransfer.cpp: the transfer and its SendFileOp) is ee_won's eexfer.rs; its shells and host are here too.
#include <windows.h>
#include "platform/PlatFile.h"
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/EmpireEarth.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/EELobbyGameCode.h"
#include "rust/won_lobby/eegame.h"
#include "rust/won_lobby/eexfer.h"
#include "ee/wonapi/WONLobbyGuard.h"

using namespace WONAPI;


// ---- the saved game's transfer (ee_won eexfer.rs): the shells, a completion running a callback on the window
// thread (LobbyCompletion's), the host

static intptr_t EELobbyTransferHost(void* inObj, EeWonEeXferOp inOp, intptr_t a, intptr_t b, intptr_t c);

// the host's side of a saved game's transfer: sends the file over one client's socket (RTTI SendFileOp > SocketOp,
// vtable 0x84c408)
class SendFileOp : public SocketOp {
public:
    SendFileOp(const std::string& inPath)
        : SocketOp(NULL), mRust(ee_won_eexfer_send_new(this, EELobbyTransferHost,
                                                       reinterpret_cast<const uint8_t*>(inPath.data()), inPath.size()))
    {
        this->SetWaitForEvent(1, true);
    }
    virtual ~SendFileOp() { ee_won_eexfer_send_free(this->mRust); }
    virtual void CleanupHook() { ee_won_eexfer_send_call(this->mRust, EeWonEeXferSendCall::Cleanup); }
    virtual WONStatus Start() { return ee_won_eexfer_send_call(this->mRust, EeWonEeXferSendCall::Start); }
    virtual WONStatus Continue() { return ee_won_eexfer_send_call(this->mRust, EeWonEeXferSendCall::Continue); }
    EeWonEeXferSend* mRust;
};

// LobbyCompletion<EELobbyFileTransfer>: the callback runs on the window thread, the transfer held
class EELobbyTransferCompletion : public LobbyCompletionBase {
public:
    EELobbyTransferCompletion(EELobbyFileTransfer* inTransfer, EeWonEeXferCall inCall)
        : mTransfer(inTransfer), mCall(inCall) {}
    virtual void Run(AsyncOp* inOp) { ee_won_eexfer_call(this->mTransfer->mRust, this->mCall, (intptr_t)inOp, 0); }
    SmartPtr<EELobbyFileTransfer> mTransfer;
    EeWonEeXferCall mCall;
};

EELobbyFileTransfer::EELobbyFileTransfer() : mRust(ee_won_eexfer_new(this, EELobbyTransferHost)) {}
EELobbyFileTransfer::~EELobbyFileTransfer() { ee_won_eexfer_free(this->mRust); }

static intptr_t EELobbyTransferHost(void* inObj, EeWonEeXferOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonEeXferOp Op;
    EELobbyFileTransfer* aTransfer = static_cast<EELobbyFileTransfer*>(inObj);
    SendFileOp* aSend = static_cast<SendFileOp*>(inObj);
    AsyncOp* anOp = reinterpret_cast<AsyncOp*>(a);
    QueueSocket* aSocket = static_cast<QueueSocket*>(reinterpret_cast<AsyncSocket*>(a));
    switch (inOp) {
    case Op::SavedGameInfo: EELobbyGetSavedGameInfo(reinterpret_cast<const char*>(a), *reinterpret_cast<uint32_t*>(b), *reinterpret_cast<uint32_t*>(c)); return 0;
    case Op::Time: return (intptr_t)(uint32_t)(unsigned long)PlatTimeMilliseconds64();
    case Op::MyGame: return (intptr_t)GetMyLobbyGame();
    case Op::GameProgress:
        reinterpret_cast<LobbyGame*>(a)->EE(EeWonEeGameCall::UpdateTransferProgress, (int)b, (int)c);
        return 0;
    case Op::BytesRead: return (intptr_t) RecvChunkOf(anOp)->GetBytesRead();
    case Op::Bind: return aSocket->Fn2B0D90((unsigned short)b, 0);
    case Op::HostValid: return aTransfer->mHost.IsValid() ? 1 : 0;
    case Op::Open: return (intptr_t)PlatFOpen(reinterpret_cast<const char*>(a), "rb");
    case Op::Read: return (intptr_t)fread(reinterpret_cast<void*>(b), 1, (size_t)c, reinterpret_cast<FILE*>(a));
    case Op::Close: fclose(reinterpret_cast<FILE*>(a)); return 0;
    case Op::WaitForWrite: aSend->GetSocket()->WaitForWrite(aSend->TimeLeft()); return 0;
    case Op::Send:
        return aSend->GetSocket()->Send(reinterpret_cast<const void*>(a), (unsigned long)b, reinterpret_cast<int*>(c));
    case Op::TimeLeft: return (intptr_t)aSend->TimeLeft();
    // R6 WON round 34: the statements of the compound cases eesplit.rs composes (the sockets, the completions, the
    // receive op and the queued ops)
    case Op::NewQueueSocket: return (intptr_t) static_cast<AsyncSocket*>(new QueueSocket(0));
    case Op::SetRepeatCompletion: aSocket->SetRepeatCompletion(new EELobbyTransferCompletion(aTransfer, (EeWonEeXferCall)b)); return 0;
    case Op::SetRepeatAccept: aSocket->SetRepeatOp(new SocketOp(EeWonCoreSocketOp::Accept)); return 0;
    case Op::SetCloseCompletion: aSocket->SetCloseCompletion(new EELobbyTransferCompletion(aTransfer, (EeWonEeXferCall)b)); return 0;
    case Op::QueueSendFile: aSocket->QueueOp(new SendFileOp(std::string(reinterpret_cast<const char*>(b), (size_t)c)), -1); return 0;
    case Op::QueueCloseOp: aSocket->QueueOp(new SocketOp(EeWonCoreSocketOp::Close), -1); return 0;
    case Op::NewRecvBytes: return (intptr_t) static_cast<AsyncOp*>(new SocketOp(EeWonCoreSocketOp::RecvBytes));
    case Op::OpSetCompletion: anOp->SetCompletion(new EELobbyTransferCompletion(aTransfer, (EeWonEeXferCall)b)); return 0;
    case Op::OpSetFilePath: static_cast<SocketOp*>(anOp)->SetFilePath(std::string(reinterpret_cast<const char*>(b), (size_t)c)); return 0;
    case Op::OpSetProgress: static_cast<SocketOp*>(anOp)->SetProgressCompletion(new EELobbyTransferCompletion(aTransfer, EeWonEeXferCall::OnProgress), 10000); return 0;
    case Op::QueueConnect: aSocket->QueueOp(new SocketOp(EeWonCoreSocketOp::Connect, NULL, NULL, aTransfer->mHost.GetValue()), -1); return 0;
    case Op::QueueOpOn: aSocket->QueueOp(static_cast<SocketOp*>(reinterpret_cast<AsyncOp*>(b)), -1); return 0;
    }
    return 0;
}

typedef EeWonEeGameCall GC;

// EELobbyGame's notifications to the lobby's controls: a player slot (type 100063) or a game option (100062)
// changed, its number in the event's Value (the original's EELobbySyncPlayerEvent / EELobbySyncOptionEvent, mParam
// +0x20; Rust events since R6 WON lobby and network round 37, lobbyevt.rs)
static LobbyEvent* EELobbySyncEvent(int inType, LobbyEvtKind inKind, int inValue)
{
    const intptr_t aArgs[] = {inValue};
    return LobbyEvtNew(inType, inKind, aArgs);
}

static intptr_t EELobbyGameHost(void* inGame, EeWonEeGameOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d);


// ---- the shell (R6 WON lobby and network round 37: EELobbyGame folded into LobbyGame, whose hooks are the Rust
// game's when it holds one)

// the EE factory's game: every game option 0, its eight slots (the Rust game's) and no saved game file here
LobbyGame* WONAPI::EELobbyNewGame() { return new LobbyGame(EELobbyGameHost); }

// Returns the text for a refused join: not in the saved game, wrong version or modified game files
GUIString WONAPI::EELobbyStatusString(intptr_t inStatus)
{
    switch (inStatus) {
    case 1: return EE_LOBBY_STRING(gEELobbyGameStatus_NotInSavedGame);
    case 2: return EE_LOBBY_STRING(gEELobbyGameStatus_InvalidVersion);
    case 3: return EE_LOBBY_STRING(gEELobbyGameStatus_CorruptBinaries);
    }
    return gEmptyGUIString;
}

// ---- the host

// a message's text as a new GUIStringData with one reference (ReadWString's; a short buffer throws)
static GUIStringData* ReadText(ReadBuffer* inBuffer)
{
    std::wstring aText;
    inBuffer->ReadWString(aText, 2);
    GUIStringData* aData = NewGUIStringData(aText);
    aData->AddRef();
    return aData;
}

static intptr_t EELobbyGameHost(void* inGame, EeWonEeGameOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonEeGameOp Op;
    LobbyGame* aGame = static_cast<LobbyGame*>(inGame);
    WriteBuffer* aWrite = reinterpret_cast<WriteBuffer*>(a);
    LobbyClient* aClient = reinterpret_cast<LobbyClient*>(a);
    LobbyPlayer* aPlayer = reinterpret_cast<LobbyPlayer*>(a);
    EELobbyFileTransfer* aTransfer = reinterpret_cast<EELobbyFileTransfer*>(a);
    switch (inOp) {
    case Op::AddRef: reinterpret_cast<RefCount*>(a)->AddRef(); return 0;
    case Op::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    // a read past the message's end: the exception kept (WonGuard), kWonThrown answered, the shell rethrows it
    case Op::ReadByte: return WON_GUARDED(reinterpret_cast<ReadBuffer*>(a)->ReadByte());
    case Op::ReadShort: return WON_GUARDED(reinterpret_cast<ReadBuffer*>(a)->ReadShort());
    case Op::ReadBool: return WON_GUARDED(reinterpret_cast<ReadBuffer*>(a)->ReadBool() ? 1 : 0);
    case Op::ReadLong: return WON_GUARDED(reinterpret_cast<ReadBuffer*>(a)->ReadLong());
    case Op::ReadWString: return WON_GUARDED(ReadText(reinterpret_cast<ReadBuffer*>(a)));
    case Op::Available: return reinterpret_cast<ReadBuffer*>(a)->Available();
    case Op::AppendByte: aWrite->AppendByte((unsigned char)b); return 0;
    case Op::AppendShort: aWrite->AppendShort((short)b); return 0;
    case Op::AppendBool: aWrite->AppendBool(b != 0); return 0;
    case Op::AppendLong: aWrite->AppendLong((int32_t)b); return 0;
    case Op::AppendWString: aWrite->AppendWString(EELobbyStr(b), 2); return 0;
    case Op::Length: return (intptr_t)aWrite->length();
    case Op::SetByte: aWrite->SetByte((unsigned long)b, (unsigned char)c); return 0;
    case Op::NewBuffer: return (intptr_t) new WriteBuffer(0, true);
    case Op::BroadcastToGame: LobbyBroadcastToGame(aWrite->ToByteBuffer(true).get()); return 0;
    case Op::SendToCaptain: LobbySendToCaptain(aWrite->ToByteBuffer(true).get()); return 0;
    case Op::SendToClient: LobbySendToClient(reinterpret_cast<LobbyClient*>(b), aWrite->ToByteBuffer(true).get()); return 0;
    case Op::FreeBuffer: delete aWrite; return 0;
    case Op::CurrentServer: return (intptr_t)LobbyGetCurrentServer();
    case Op::PlayerSlot: return aPlayer->MGet(LobbyMF::PlayerSlot);
    case Op::PlayerTeam: return aPlayer->M(LobbyMC::PlayerTeam);
    case Op::PlayerDetach: aPlayer->MSet(LobbyMF::PlayerSlotGame, 0); return 0;
    case Op::MyPlayer: return (intptr_t)GetMyLobbyPlayer();
    case Op::IAmCaptain: return LobbyIAmCaptain() ? 1 : 0;
    case Op::NotifyStagingChange: LobbyNotifyStagingChange(); return 0;
    case Op::AdvertiseGame: LobbyNetAdvertiseGame(); return 0;
    case Op::ResetReadyFlags: LobbyResetReadyFlags(); return 0;
    case Op::SyncPlayer: LobbyBroadcastEvent(EELobbySyncEvent(LobbyEvent_EELobbySyncPlayer, LobbyEvtKind::EELobbySyncPlayer, (int)a), true); return 0;
    case Op::SyncOption: LobbyBroadcastEvent(EELobbySyncEvent(LobbyEvent_EELobbySyncOption, LobbyEvtKind::EELobbySyncOption, (int)a), true); return 0;
    case Op::TeamChanged: LobbyBroadcastEvent(PlayerTeamChangedEvent(aClient, b != 0), true); return 0;
    case Op::LobbyEvent: LobbyBroadcastLobbyEvent((int)a, true); return 0;
    case Op::KickClient: LobbyKickClient(aClient, false); return 0;
    case Op::SameTeamBox: LobbyMessageBox(EE_LOBBY_STRING(gEELobbyGame_StartGameError), EE_LOBBY_STRING(gEELobbyGame_EveryoneOnSameTeam)); return 0;
    case Op::HostGame: return EELobbyHostGame() ? 1 : 0;
    case Op::CancelConnectDialog: EELobbyCancelConnectDialog(); return 0;
    case Op::SetSlotConnected: EELobbySetSlotConnected((unsigned char)a, b != 0); return 0;
    case Op::ConnectDialogRunning: return EELobbyIsConnectDialogRunning() ? 1 : 0;
    case Op::FileTransferPort: return EELobbyGetFileTransferPort();
    case Op::NumMapTypes: return (intptr_t)EELobbyGetNumMapTypes();
    case Op::SetDefaultOptions: EELobbySetDefaultGameOptions(aGame); return 0;
    // EELobbyGetVersion's statements (eesplit.rs composes Version, R6 WON round 35): the text EELobbyToGUIString makes
    case Op::VersionTextNew: return (intptr_t) NewGUIStringData(StringToWString(std::string(gGameVersion), -1));
    case Op::VersionChecksum: return (intptr_t)gBinaryChecksum;
    case Op::StringsEqual: return EELobbyStr(a) == EELobbyStr(b) ? 1 : 0;
    case Op::TransferNew: return (intptr_t) new EELobbyFileTransfer;
    case Op::TransferSetSavesPath: {
        GUIString thePath = EELobbyFn2988E9();
        thePath.append(aTransfer->mName);
        std::string aPath = thePath;
        return ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::SetPath, (intptr_t)aPath.data(), aPath.size());
    }
    case Op::TransferServe: ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::Serve, (unsigned short)b, 0); return 0;
    case Op::TransferName: return (intptr_t)aTransfer->mName.mData.get();
    case Op::TransferSize: return ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::Field, 2, 0);
    case Op::TransferChecksum: return ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::Field, 3, 0);
    case Op::TransferStop: return ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::Stop, 0, 0);
    case Op::TransferCloseClients: return ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::CloseClients, 0, 0);
    case Op::TransferIsFileHere: return ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::IsFileHere, 0, 0);
    case Op::TransferDownload: return ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::Download, 0, 0);
    // R6 WON round 34: the statements of the compound cases eesplit.rs composes
    case Op::PlayerSetSlotGame: aPlayer->MSet(LobbyMF::PlayerSlotGame, (intptr_t)aGame->mEE); return 0;
    case Op::PlayerSetSlotIndex: aPlayer->MSet(LobbyMF::PlayerSlotIndex, (unsigned char)b); return 0;
    case Op::PlayerSetSlot: aPlayer->MSet(LobbyMF::PlayerSlot, (unsigned char)b); return 0;
    case Op::TransferSetName: aTransfer->mName = EELobbyStr(b); return 0;
    case Op::TransferRust: return (intptr_t)aTransfer->mRust;
    case Op::TransferHostFromGame: aTransfer->mHost = aGame->GetAddress(); return 0;
    case Op::TransferHostSetPort: aTransfer->mHost.SetPort((unsigned short)b); return 0;
    }
    return 0;
}
