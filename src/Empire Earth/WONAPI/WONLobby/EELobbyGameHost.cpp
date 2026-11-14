// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// The Empire Earth lobby game's shell and host (R6 WONLobby round 5): EELobbyGame and its player slots are one Rust
// object (rust/crates/ee_won/src/eegame.rs: the options, the slots, the counts, the file transfer, the details'
// player names, every method and hook). This unit is the shell (the class's methods and virtual hooks forward; a hook
// whose read threw throws ReadBufferException again for the lobby's handler), the slot view's methods, and the host:
// each op the original's statements on a buffer, a client, a player, the file transfer, the lobby or a global, with
// no decision. A read op catches the ReadBufferException it threw and answers INTPTR_MIN.
#include <windows.h>
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

extern GUIString gEELobbyGame_StartGameError;          // 0x9658b0 (EELobbyResource.cpp)
extern GUIString gEELobbyGame_EveryoneOnSameTeam;      // 0x9659ac (EELobbyResource.cpp)
extern GUIString gEELobbyGameStatus_NotInSavedGame;    // 0x9658cc (EELobbyResource.cpp)
extern GUIString gEELobbyGameStatus_InvalidVersion;    // 0x96591c (EELobbyResource.cpp)
extern GUIString gEELobbyGameStatus_CorruptBinaries;   // 0x9659b4 (EELobbyResource.cpp)

typedef EeWonEeGameCall GC;

static intptr_t EELobbyGameHost(void* inGame, EeWonEeGameOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d);

static intptr_t Threw(intptr_t inResult)
{
    if (inResult == INTPTR_MIN)
        throw ReadBufferException("Attempt to read past end of buffer.");
    return inResult;
}

// ---- the shell

// FUNCTION: EXE 0x69222b
// Creates a lobby game with every game option 0, its eight slots numbered and no saved game file here
EELobbyGame::EELobbyGame() : mRust(NULL)
{
    for (int i = 0; i < 8; i++) {
        this->mSlots[i].mGame = this;
        this->mSlots[i].mIndex = (unsigned char)i;
    }
    this->mRust = ee_won_eegame_new(this, EELobbyGameHost);
}

// FUNCTION: EXE 0x6922b2
// Destroys the lobby game, releasing its slots, player names and file transfer
EELobbyGame::~EELobbyGame()
{
    ee_won_eegame_free(this->mRust);
}

intptr_t EELobbyGame::RCall(EeWonEeGameCall inCall, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    return ee_won_eegame_call(this->mRust, inCall, a, b, c, d);
}

int EELobbyGame::GetNumFiles() { return (int)this->RCall(GC::GetNumFiles); }
GUIString EELobbyGame::GetFile(int inIndex) { return EELobbyStr(this->RCall(GC::GetFile, inIndex)); }
void EELobbyGame::SetGameFile(const GUIString& inName, unsigned long inSize, unsigned long inChecksum, bool inFlag)
{
    this->RCall(GC::SetGameFile, (intptr_t)inName.mData.get(), (intptr_t)(uint32_t)inSize, (intptr_t)(uint32_t)inChecksum,
                inFlag);
}
const GUIString& EELobbyGame::GetGameFileName()
{
    EELobbyFileTransfer* aTransfer = reinterpret_cast<EELobbyFileTransfer*>(this->RCall(GC::Field, 0));
    if (aTransfer != NULL)
        return aTransfer->mName;
    return gEmptyGUIString;
}
void EELobbyGame::StopTransfer() { this->RCall(GC::StopTransfer); }
void EELobbyGame::UpdateTransferProgress(int inDone, int inTotal) { this->RCall(GC::UpdateTransferProgress, inDone, inTotal); }
void EELobbyGame::SetTransferPercent(unsigned char inPercent) { this->RCall(GC::SetTransferPercent, inPercent); }
void EELobbyGame::StartTransfer() { this->RCall(GC::StartTransfer); }
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
// FUNCTION: EXE 0x692df3
// Returns the text for a refused join: not in the saved game, wrong version or modified game files
GUIString EELobbyGame::GetStatusStringHook(short inStatus)
{
    switch (this->RCall(GC::GetStatusStringHook, inStatus)) {
    case 1: return gEELobbyGameStatus_NotInSavedGame;
    case 2: return gEELobbyGameStatus_InvalidVersion;
    case 3: return gEELobbyGameStatus_CorruptBinaries;
    }
    return gEmptyGUIString;
}
void EELobbyGame::ResetSlots() { this->RCall(GC::ResetSlots); }
void EELobbyGame::CountPlayers(bool inNotify) { this->RCall(GC::CountPlayers, inNotify); }
EELobbyPlayerSlot* EELobbyGame::GetPlayerSlot(int inSlot)
{
    if (inSlot >= 0 && inSlot < 8)
        return &this->mSlots[inSlot];
    return NULL;
}
bool EELobbyGame::HasGameFile() { return this->RCall(GC::HasGameFile) != 0; }
long EELobbyGame::GetOption(int inOption) { return (long)this->RCall(GC::GetOption, inOption); }
void EELobbyGame::SetOption(int inOption, unsigned long inValue) { this->RCall(GC::SetOption, inOption, (intptr_t)(uint32_t)inValue); }
void EELobbyGame::ChangeOption(int inOption, unsigned long inValue, bool inFlag)
{
    this->RCall(GC::ChangeOption, inOption, (intptr_t)(uint32_t)inValue, inFlag);
}
void EELobbyGame::SendGameState() { this->RCall(GC::SendGameState); }
void EELobbyGame::ChangePlayerOption(int inSlot, int inWhat, unsigned char inValue) { this->RCall(GC::ChangePlayerOption, inSlot, inWhat, inValue); }
void EELobbyGame::SendConnectStatus(bool inConnected) { this->RCall(GC::SendConnectStatus, inConnected); }
void EELobbyGame::SendCancelConnect() { this->RCall(GC::SendCancelConnect); }
void EELobbyGame::SendOption(int inOption) { this->RCall(GC::SendOption, inOption); }
unsigned char EELobbyGame::GetFreeColor(int inExcept, unsigned char inColor) { return (unsigned char)this->RCall(GC::GetFreeColor, inExcept, inColor); }
void EELobbyGame::MakeColorsDistinct() { this->RCall(GC::MakeColorsDistinct); }
int EELobbyGame::GetMySlot() { return (int)this->RCall(GC::GetMySlot); }
int EELobbyGame::GetMyTeam() { return (int)this->RCall(GC::GetMyTeam); }
void EELobbyGame::GetTakenColors(bool* outTaken, int inExcept) { this->RCall(GC::GetTakenColors, (intptr_t)outTaken, inExcept); }
bool EELobbyGame::IsColorFree(unsigned char inColor) { return this->RCall(GC::IsColorFree, inColor) != 0; }

// ---- the slot view (EELobbyPlayerSlot's methods on the Rust game's slot)

intptr_t EELobbyPlayerSlot::RCall(EeWonEeGameCall inCall, intptr_t b, intptr_t c)
{
    return this->mGame->RCall(inCall, this->mIndex, b, c);
}
void EELobbyPlayerSlot::Reset() { this->RCall(GC::SlotReset); }
GUIString EELobbyPlayerSlot::GetName() { return EELobbyStr(this->RCall(GC::SlotGetName)); }
void EELobbyPlayerSlot::SetState(int inState) { this->RCall(GC::SlotSetState, (unsigned char)inState); }
unsigned char EELobbyPlayerSlot::GetOption(int inOption) { return (unsigned char)this->RCall(GC::SlotGetOption, inOption); }
void EELobbyPlayerSlot::SetOption(int inOption, unsigned char inValue) { this->RCall(GC::SlotSetOption, inOption, inValue); }
void EELobbyPlayerSlot::ClearPlayer() { this->RCall(GC::SlotClearPlayer); }
void EELobbyPlayerSlot::SetOpen() { this->RCall(GC::SlotSetOpen); }
void EELobbyPlayerSlot::SetClosed() { this->RCall(GC::SlotSetClosed); }
void EELobbyPlayerSlot::SetComputer() { this->RCall(GC::SlotSetComputer); }
void EELobbyPlayerSlot::SetName(const GUIString& inName) { this->RCall(GC::SlotSetName, (intptr_t)inName.mData.get()); }
void EELobbyPlayerSlot::SetPlayer(LobbyClient* inClient, EELobbyPlayer* inPlayer)
{
    this->RCall(GC::SlotSetPlayer, (intptr_t)inClient, (intptr_t)inPlayer);
}
unsigned char EELobbyPlayerSlot::Option(int inOption) { return (unsigned char)this->RCall(GC::SlotRawOption, inOption); }
void EELobbyPlayerSlot::SetRawOption(int inOption, unsigned char inValue) { this->RCall(GC::SlotSetRawOption, inOption, inValue); }
LobbyClient* EELobbyPlayerSlot::GetClient() { return reinterpret_cast<LobbyClient*>(this->RCall(GC::SlotClient)); }

// ---- the lobby player (EELobbyPlayer.cpp's)

// FUNCTION: EXE 0x699a3f
// Creates a lobby game player in slot 0 with no slot attached
EELobbyPlayer::EELobbyPlayer()
{
    this->mSlot = 0;
    this->mPlayerSlot = NULL;
}
// FUNCTION: EXE 0x699a75
// Destroys the lobby game player (members only)
EELobbyPlayer::~EELobbyPlayer() {}
// FUNCTION: EXE 0x699a80
// Reads the player's slot number from a game message; always succeeds
bool EELobbyPlayer::ReadData(ReadBuffer& inBuffer)
{
    this->mSlot = inBuffer.ReadByte();
    return true;
}
// FUNCTION: EXE 0x699a95
// Writes the player's slot number to a game message
void EELobbyPlayer::WriteData(WriteBuffer& outBuffer) { outBuffer.AppendByte(this->mSlot); }
// FUNCTION: EXE 0x699aa7
// Returns whether another player is on the same team (the rule is ee_won's)
bool EELobbyPlayer::IsTeammate(LobbyPlayer* inOther)
{
    unsigned char theTeam = static_cast<EELobbyPlayer*>(inOther)->GetTeam();
    return ee_won_eegame_is_teammate(this->GetTeam(), theTeam);
}
// FUNCTION: EXE 0x699ad1
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
    case Op::IsHost: return aGame->GetIsHost() ? 1 : 0;
    case Op::MaxPlayers: return aGame->GetMaxPlayers();
    case Op::SetMaxPlayers: aGame->SetMaxPlayers((long)a); return 0;
    case Op::GameType: return aGame->GetGameType();
    case Op::Clients: return (intptr_t)aGame->GetClients().mPtr;
    case Op::FindClient: return (intptr_t) reinterpret_cast<LobbyClientList*>(a)->FindClient((unsigned short)b);
    case Op::CurrentServer: return (intptr_t)LobbyGetCurrentServer();
    case Op::ServerClient: return (intptr_t) reinterpret_cast<LobbyServer*>(a)->GetClient((int)b);
    case Op::IterNew: return (intptr_t) new LobbyClientSetIterator(reinterpret_cast<LobbyClientList*>(a));
    case Op::IterHasMore: return reinterpret_cast<LobbyClientSetIterator*>(a)->HasMore() ? 1 : 0;
    case Op::IterNext: {
        LobbyClient* aNext = NULL;
        reinterpret_cast<LobbyClientSetIterator*>(a)->Next(&aNext);
        return (intptr_t)aNext;
    }
    case Op::IterFree: delete reinterpret_cast<LobbyClientSetIterator*>(a); return 0;
    case Op::ClientName: return (intptr_t)aClient->GetName().mData.get();
    case Op::ClientId: return aClient->GetClientId();
    case Op::ClientPlayer: return (intptr_t)(EELobbyPlayer*)aClient->GetPlayer().mPtr;
    case Op::ClientIsCaptain: return aClient->IsCaptain(1) ? 1 : 0;
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
    case Op::SameTeamBox: LobbyMessageBox(gEELobbyGame_StartGameError, gEELobbyGame_EveryoneOnSameTeam); return 0;
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
    case Op::TransferSetFileInfo: aTransfer->SetFileInfo(EELobbyStr(b), (unsigned long)c, (unsigned long)d); return 0;
    case Op::TransferSetSavesPath: {
        GUIString thePath = EELobbyFn2988E9();
        thePath.append(aTransfer->mName);
        aTransfer->SetPath(thePath);
        return 0;
    }
    case Op::TransferServe: aTransfer->Serve((unsigned short)b); return 0;
    case Op::TransferName: return (intptr_t)aTransfer->mName.mData.get();
    case Op::TransferSize: return (intptr_t)(uint32_t)aTransfer->mSize;
    case Op::TransferChecksum: return (intptr_t)(uint32_t)aTransfer->mChecksum;
    case Op::TransferSetHost: {
        IPAddr theAddress(aGame->GetAddress());
        theAddress.SetPort((unsigned short)b);
        aTransfer->SetHost(theAddress);
        return 0;
    }
    case Op::TransferStop: aTransfer->Stop(); return 0;
    case Op::TransferCloseClients: aTransfer->CloseClients(); return 0;
    case Op::TransferIsFileHere: return aTransfer->IsFileHere() ? 1 : 0;
    case Op::TransferDownload: aTransfer->Download(); return 0;
    }
    return 0;
}
