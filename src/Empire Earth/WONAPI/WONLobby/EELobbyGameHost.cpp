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
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/EELobbyGameCode.h"
#include "rust/ee_won.h"

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
    case Op::SavedGameInfo: {
        unsigned long aSize = *reinterpret_cast<uint32_t*>(b), aChecksum = *reinterpret_cast<uint32_t*>(c);
        EELobbyGetSavedGameInfo(reinterpret_cast<const char*>(a), aSize, aChecksum);
        *reinterpret_cast<uint32_t*>(b) = (uint32_t)aSize;
        *reinterpret_cast<uint32_t*>(c) = (uint32_t)aChecksum;
        return 0;
    }
    case Op::Time: return (intptr_t)(uint32_t)(unsigned long)PlatTimeMilliseconds64();
    case Op::MyGame: return (intptr_t)GetMyLobbyGame();
    case Op::GameProgress:
        static_cast<EELobbyGame*>(reinterpret_cast<LobbyGame*>(a))->UpdateTransferProgress((int)b, (int)c);
        return 0;
    case Op::Succeeded: return anOp->Succeeded() ? 1 : 0;
    case Op::OpSocket: return (intptr_t) static_cast<SocketOp*>(anOp)->GetSocket();
    case Op::AcceptedSocket: return (intptr_t) static_cast<AcceptOp*>(anOp)->GetAcceptedSocket();
    case Op::BytesRead: return (intptr_t) static_cast<RecvChunkOp*>(anOp)->GetRecvOp()->GetBytesRead();
    case Op::AddRef: reinterpret_cast<RefCount*>(a)->AddRef(); return 0;
    case Op::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case Op::Kill: reinterpret_cast<AsyncSocket*>(a)->Kill(); return 0;
    case Op::NewSocket: {
        QueueSocket* aNew = new QueueSocket(0);
        aNew->AddRef();
        return (intptr_t) static_cast<AsyncSocket*>(aNew);
    }
    case Op::Bind: return aSocket->Fn2B0D90((unsigned short)b, 0);
    case Op::Listen: return aSocket->Listen(1000);
    case Op::RepeatAccept:
        aSocket->SetRepeatCompletion(new EELobbyTransferCompletion(aTransfer, EeWonEeXferCall::OnClientAccepted));
        aSocket->SetRepeatOp(new AcceptOp(NULL));
        return 0;
    case Op::QueueSend:
        aSocket->SetCloseCompletion(new EELobbyTransferCompletion(aTransfer, EeWonEeXferCall::OnClientClosed));
        aSocket->QueueOp(new SendFileOp(std::string(reinterpret_cast<const char*>(b), (size_t)c)), -1);
        aSocket->QueueOp(new CloseOp(NULL), -1);
        return 0;
    case Op::HostValid: return aTransfer->mHost.IsValid() ? 1 : 0;
    case Op::QueueDownload: {
        aSocket->SetCloseCompletion(new EELobbyTransferCompletion(aTransfer, EeWonEeXferCall::OnClientClosed));
        SmartPtr<RecvBytesOp> theOp = new RecvBytesOp(0, NULL);
        theOp->SetCompletion(new EELobbyTransferCompletion(aTransfer, EeWonEeXferCall::OnFileReceived));
        theOp->SetFilePath(std::string(reinterpret_cast<const char*>(b), (size_t)c));
        theOp->SetProgressCompletion(new EELobbyTransferCompletion(aTransfer, EeWonEeXferCall::OnProgress), 10000);
        aSocket->QueueOp(new ConnectOp(aTransfer->mHost), -1);
        aSocket->QueueOp(theOp.mPtr, -1);
        return 0;
    }
    case Op::Open: return (intptr_t)PlatFOpen(reinterpret_cast<const char*>(a), "rb");
    case Op::Read: return (intptr_t)fread(reinterpret_cast<void*>(b), 1, (size_t)c, reinterpret_cast<FILE*>(a));
    case Op::Close: fclose(reinterpret_cast<FILE*>(a)); return 0;
    case Op::WaitForWrite: aSend->GetSocket()->WaitForWrite(aSend->TimeLeft()); return 0;
    case Op::Send:
        return aSend->GetSocket()->Send(reinterpret_cast<const void*>(a), (unsigned long)b, reinterpret_cast<int*>(c));
    case Op::TimeLeft: return (intptr_t)aSend->TimeLeft();
    }
    return 0;
}

typedef EeWonEeGameCall GC;

static intptr_t EELobbyGameHost(void* inGame, EeWonEeGameOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d);

static intptr_t Threw(intptr_t inResult)
{
    if (inResult == INTPTR_MIN)
        throw ReadBufferException("Attempt to read past end of buffer.");
    return inResult;
}

// ---- the shell

// Creates a lobby game with every game option 0, its eight slots numbered and no saved game file here
EELobbyGame::EELobbyGame() : mRust(NULL)
{
    for (int i = 0; i < 8; i++) {
        this->mSlots[i].mGame = this;
        this->mSlots[i].mIndex = (unsigned char)i;
    }
    this->mRust = ee_won_eegame_new_model(this, EELobbyGameHost);
}

// Destroys the lobby game, releasing its slots, player names and file transfer
EELobbyGame::~EELobbyGame()
{
    ee_won_eegame_free(this->mRust);
}

intptr_t EELobbyGame::RCall(EeWonEeGameCall inCall, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    return ee_won_eegame_call(this->mRust, inCall, a, b, c, d);
}

void EELobbyGame::UpdateTransferProgress(int inDone, int inTotal) { this->RCall(GC::UpdateTransferProgress, inDone, inTotal); }
bool EELobbyGame::CanReadyHook() { return this->RCall(GC::CanReadyHook) != 0; }
bool EELobbyGame::CanStartGameHook() { return this->RCall(GC::CanStartGameHook) != 0; }
bool EELobbyGame::TryStartGameHook() { return this->RCall(GC::TryStartGameHook) != 0; }
void EELobbyGame::PostNetLeaveGameHook() { this->RCall(GC::PostNetLeaveGameHook); }
void EELobbyGame::NotifyStartGameHook() { this->RCall(GC::NotifyStartGameHook); }
void EELobbyGame::HandleHostGameHook(LobbyClient* inClient) { this->RCall(GC::HandleHostGameHook, (intptr_t)inClient); }
void EELobbyGame::GetJoinGameRequestHook(WriteBuffer& outBuffer) { this->RCall(GC::GetJoinGameRequestHook, (intptr_t)&outBuffer); }
short EELobbyGame::HandleJoinGameRequestHook(ReadBuffer& inBuffer, LobbyClient* inClient)
{
    return (short)Threw(this->RCall(GC::HandleJoinGameRequestHook, (intptr_t)&inBuffer, (intptr_t)inClient));
}
void EELobbyGame::GetJoinGameReplyHook(LobbyClient* inClient, WriteBuffer& outBuffer)
{
    this->RCall(GC::GetJoinGameReplyHook, (intptr_t)inClient, (intptr_t)&outBuffer);
}
void EELobbyGame::HandleJoinGameReplyHook(ReadBuffer& inBuffer) { Threw(this->RCall(GC::HandleJoinGameReplyHook, (intptr_t)&inBuffer)); }
void EELobbyGame::HandlePlayerLeftHook(LobbyClient* inClient, LobbyPlayer* inPlayer)
{
    this->RCall(GC::HandlePlayerLeftHook, (intptr_t)inClient, (intptr_t) static_cast<EELobbyPlayer*>(inPlayer));
}
void EELobbyGame::HandlePlayerJoinedHook(LobbyClient* inClient) { this->RCall(GC::HandlePlayerJoinedHook, (intptr_t)inClient); }
void EELobbyGame::WriteSummaryHook(WriteBuffer& outBuffer) { this->RCall(GC::WriteSummaryHook, (intptr_t)&outBuffer); }
void EELobbyGame::WriteDetailsHook(WriteBuffer& outBuffer) { this->RCall(GC::WriteDetailsHook, (intptr_t)&outBuffer); }
bool EELobbyGame::ReadSummaryHook(ReadBuffer& inBuffer) { return Threw(this->RCall(GC::ReadSummaryHook, (intptr_t)&inBuffer)) != 0; }
bool EELobbyGame::ReadDetailsHook(ReadBuffer& inBuffer) { return Threw(this->RCall(GC::ReadDetailsHook, (intptr_t)&inBuffer)) != 0; }
void EELobbyGame::HandleGameMessageHook(unsigned char inType, ReadBuffer& inBuffer, LobbyClient* inFrom)
{
    Threw(this->RCall(GC::HandleGameMessageHook, inType, (intptr_t)&inBuffer, (intptr_t)inFrom));
}
// Returns the text for a refused join: not in the saved game, wrong version or modified game files
GUIString EELobbyGame::GetStatusStringHook(short inStatus)
{
    switch (this->RCall(GC::GetStatusStringHook, inStatus)) {
    case 1: return EE_LOBBY_STRING(gEELobbyGameStatus_NotInSavedGame);
    case 2: return EE_LOBBY_STRING(gEELobbyGameStatus_InvalidVersion);
    case 3: return EE_LOBBY_STRING(gEELobbyGameStatus_CorruptBinaries);
    }
    return gEmptyGUIString;
}

// ---- the slot view (EELobbyPlayerSlot's methods on the Rust game's slot)

intptr_t EELobbyPlayerSlot::RCall(EeWonEeGameCall inCall, intptr_t b, intptr_t c)
{
    return this->mGame->RCall(inCall, this->mIndex, b, c);
}
unsigned char EELobbyPlayerSlot::Option(int inOption) { return (unsigned char)this->RCall(GC::SlotRawOption, inOption); }

// ---- the lobby player (EELobbyPlayer.cpp's)

// Creates a lobby game player in slot 0 with no slot attached
EELobbyPlayer::EELobbyPlayer()
{
    this->mSlot = 0;
    this->mPlayerSlot = NULL;
}
// Destroys the lobby game player (members only)
EELobbyPlayer::~EELobbyPlayer() {}
// Reads the player's slot number from a game message; always succeeds
bool EELobbyPlayer::ReadData(ReadBuffer& inBuffer)
{
    this->mSlot = inBuffer.ReadByte();
    return true;
}
// Writes the player's slot number to a game message
void EELobbyPlayer::WriteData(WriteBuffer& outBuffer) { outBuffer.AppendByte(this->mSlot); }
// Returns whether another player is on the same team (the rule is ee_won's)
bool EELobbyPlayer::IsTeammate(LobbyPlayer* inOther)
{
    unsigned char theTeam = static_cast<EELobbyPlayer*>(inOther)->GetTeam();
    return ee_won_eegame_is_teammate(this->GetTeam(), theTeam);
}
// Returns the team of the player's slot, or 0 when the player has no slot
unsigned char EELobbyPlayer::GetTeam()
{
    if (this->mPlayerSlot != NULL)
        return this->mPlayerSlot->Option(1);
    return 0;
}

// ---- the host

static intptr_t Read(EeWonEeGameOp inOp, ReadBuffer* inBuffer)
{
    typedef EeWonEeGameOp Op;
    try {
        switch (inOp) {
        case Op::ReadByte: return inBuffer->ReadByte();
        case Op::ReadShort: return inBuffer->ReadShort();
        case Op::ReadBool: return inBuffer->ReadBool() ? 1 : 0;
        case Op::ReadLong: return inBuffer->ReadLong();
        case Op::ReadWString: {
            std::wstring aText;
            inBuffer->ReadWString(aText, 2);
            GUIStringData* aData = new GUIStringData(aText);
            aData->AddRef();
            return (intptr_t)aData;
        }
        default: return 0;
        }
    } catch (ReadBufferException&) {
        return INTPTR_MIN;
    }
}

static intptr_t EELobbyGameHost(void* inGame, EeWonEeGameOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    typedef EeWonEeGameOp Op;
    EELobbyGame* aGame = static_cast<EELobbyGame*>(inGame);
    WriteBuffer* aWrite = reinterpret_cast<WriteBuffer*>(a);
    LobbyClient* aClient = reinterpret_cast<LobbyClient*>(a);
    EELobbyPlayer* aPlayer = reinterpret_cast<EELobbyPlayer*>(a);
    EELobbyFileTransfer* aTransfer = reinterpret_cast<EELobbyFileTransfer*>(a);
    switch (inOp) {
    case Op::AddRef: reinterpret_cast<RefCount*>(a)->AddRef(); return 0;
    case Op::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case Op::ReadByte: case Op::ReadShort: case Op::ReadBool: case Op::ReadLong: case Op::ReadWString:
        return Read(inOp, reinterpret_cast<ReadBuffer*>(a));
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
    case Op::SendToClient:
        LobbySendToClient(reinterpret_cast<LobbyClient*>(b), aWrite->ToByteBuffer(true).get());
        return 0;
    case Op::FreeBuffer: delete aWrite; return 0;
    case Op::CurrentServer: return (intptr_t)LobbyGetCurrentServer();
    case Op::PlayerSlot: return aPlayer->GetSlot();
    case Op::PlayerTeam: return aPlayer->GetTeam();
    case Op::PlayerAttach:
        aPlayer->mPlayerSlot = &aGame->mSlots[b];
        aPlayer->mSlot = (unsigned char)b;
        return 0;
    case Op::PlayerDetach: aPlayer->mPlayerSlot = NULL; return 0;
    case Op::MyPlayer: return (intptr_t) static_cast<EELobbyPlayer*>(GetMyLobbyPlayer());
    case Op::IAmCaptain: return LobbyIAmCaptain() ? 1 : 0;
    case Op::NotifyStagingChange: LobbyNotifyStagingChange(); return 0;
    case Op::AdvertiseGame: LobbyNetAdvertiseGame(); return 0;
    case Op::ResetReadyFlags: LobbyResetReadyFlags(); return 0;
    case Op::SyncPlayer: LobbyBroadcastEvent(new EELobbySyncPlayerEvent((int)a), true); return 0;
    case Op::SyncOption: LobbyBroadcastEvent(new EELobbySyncOptionEvent((int)a), true); return 0;
    case Op::TeamChanged: LobbyBroadcastEvent(new PlayerTeamChangedEvent(aClient, b != 0), true); return 0;
    case Op::LobbyEvent: LobbyBroadcastLobbyEvent((int)a, true); return 0;
    case Op::KickClient: LobbyKickClient(aClient, false); return 0;
    case Op::SameTeamBox:
        LobbyMessageBox(EE_LOBBY_STRING(gEELobbyGame_StartGameError), EE_LOBBY_STRING(gEELobbyGame_EveryoneOnSameTeam));
        return 0;
    case Op::HostGame: return EELobbyHostGame() ? 1 : 0;
    case Op::CancelConnectDialog: EELobbyCancelConnectDialog(); return 0;
    case Op::SetSlotConnected: EELobbySetSlotConnected((unsigned char)a, b != 0); return 0;
    case Op::ConnectDialogRunning: return EELobbyIsConnectDialogRunning() ? 1 : 0;
    case Op::FileTransferPort: return EELobbyGetFileTransferPort();
    case Op::NumMapTypes: return (intptr_t)EELobbyGetNumMapTypes();
    case Op::SetDefaultOptions: EELobbySetDefaultGameOptions(aGame); return 0;
    case Op::Version: {
        GUIString aVersion(GetEmptyGUIStringData());
        unsigned long aChecksum;
        EELobbyGetVersion(aVersion, aChecksum);
        *reinterpret_cast<uint32_t*>(a) = (uint32_t)aChecksum;
        GUIStringData* aData = aVersion.mData.get();
        aData->AddRef();
        return (intptr_t)aData;
    }
    case Op::StringsEqual: return EELobbyStr(a) == EELobbyStr(b) ? 1 : 0;
    case Op::TransferNew: return (intptr_t) new EELobbyFileTransfer;
    case Op::TransferSetFileInfo:
        aTransfer->mName = EELobbyStr(b);
        return ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::SetFileInfo, (uint32_t)c, (uint32_t)d);
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
    case Op::TransferSetHost: {
        IPAddr theAddress(aGame->GetAddress());
        theAddress.SetPort((unsigned short)b);
        aTransfer->mHost = theAddress;
        return 0;
    }
    case Op::TransferStop: return ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::Stop, 0, 0);
    case Op::TransferCloseClients: return ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::CloseClients, 0, 0);
    case Op::TransferIsFileHere: return ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::IsFileHere, 0, 0);
    case Op::TransferDownload: return ee_won_eexfer_call(aTransfer->mRust, EeWonEeXferCall::Download, 0, 0);
    }
    return 0;
}
