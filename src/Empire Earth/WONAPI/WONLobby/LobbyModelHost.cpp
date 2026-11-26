// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// The lobby model's shells and host (NEW, no original address: R6 WONLobby round 10). LobbyModel.cpp's classes are
// Rust objects (rust/crates/ee_won_lobby/src/modelobj.rs: every member and the methods over them); this unit is the
// shells' base, the host the Rust calls (the factories, the game code's hooks, a record's name as a GUIString, the
// persistent ignore and friend lists, the connection's close) and the methods that read and write the C++ buffers
// and records: the directory's entries handed to the Rust as values, the summaries, the join request and reply, a
// player's bytes, the status texts. The routing server's records are read in Rust (R6 WONLobby round 12).
#include <windows.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "rust/won_lobby/model.h"
#include "rust/won_lobby/modelobj.h"
#include "rust/won_lobby/staging.h"
#include "rust/won_lobby/modelbuf.h"
#include "rust/won_lobby/lobbyhost.h"
#include "rust/won_lobby/dirread.h"
namespace WONAPI {

static_assert(offsetof(LobbyModelObject, mModel) == 2 * sizeof(void*), "modelobj.rs reads the model at this offset");

static intptr_t Held(RefCount* inObj)
{
    if (inObj != NULL) inObj->AddRef();
    return (intptr_t)inObj;
}

static std::wstring UnitsText(const uint16_t* inUnits, size_t inCount)
{
    std::wstring aText;
    WonWideSink(&aText, inUnits, inCount);
    return aText;
}

// the C++ the model calls (modelobj.rs EeWonModelOp documents each)
static intptr_t ModelHost(EeWonModelOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonModelOp O;
    switch (inOp) {
    case O::AddRef: reinterpret_cast<RefCount*>(a)->AddRef(); return 0;
    case O::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case O::NewClient: return Held(new LobbyClient);
    case O::NewClientList: return Held(new LobbyClientList);
    case O::NewGroup: return Held(new LobbyGroup);
    case O::NewServer: return Held(new LobbyServer);
    case O::NewGame: return Held(LobbyGame::CreateGame(static_cast<LobbyGameType>(a)));
    case O::NewPlayer: return Held(LobbyPlayer::CreatePlayer());
    case O::NameIgnored: return LobbyIsNameIgnored(GUIString(reinterpret_cast<GUIStringData*>(a)));
    case O::CdKeyBlocked: return LobbyIsCDKeyBlocked((unsigned long)a);
    case O::FindFriend: return (intptr_t)LobbyFindFriend((unsigned long)a);
    case O::PlayerIsTeammate: return reinterpret_cast<LobbyPlayer*>(a)->IsTeammate(reinterpret_cast<LobbyPlayer*>(b));
    case O::GameCopyFromHook: reinterpret_cast<LobbyGame*>(a)->CopyFromHook(reinterpret_cast<LobbyGame*>(b)); return 0;
    case O::GameHostHook: reinterpret_cast<LobbyGame*>(a)->HandleHostGameHook(reinterpret_cast<LobbyClient*>(b)); return 0;
    case O::GamePlayerLeftHook:
        reinterpret_cast<LobbyGame*>(a)->HandlePlayerLeftHook(reinterpret_cast<LobbyClient*>(b), reinterpret_cast<LobbyPlayer*>(c));
        return 0;
    case O::GameJoinStatusHook: return reinterpret_cast<LobbyGame*>(a)->GetJoinStatusHook();
    case O::GameCanStartHook: return reinterpret_cast<LobbyGame*>(a)->CanStartGameHook();
    case O::ConnectionClose: reinterpret_cast<RoutingConnection*>(a)->QueueClose(-1); return 0;
    case O::GameReadSummaryBytes: {
        ReadBuffer aBuffer(reinterpret_cast<const char*>(b), (unsigned long)c);
        return reinterpret_cast<LobbyGame*>(a)->ReadSummary(aBuffer);
    }
    }
    return 0;
}

LobbyModelObject::LobbyModelObject(EeWonModelKind inKind)
    : mModel(ee_won_model_new(this, inKind, ModelHost, GetEmptyGUIStringData())) {}
LobbyModelObject::~LobbyModelObject() { ee_won_model_free(this->mModel); }

// ---- the directory's records (the routing server's are read in Rust: modelobj.rs route_record_op; the directory's
// listing by ee_won_lobby dirread.rs since R6 WON round 34, ReadDirEntity among it, over these statements)

static intptr_t DirReadHost(EeWonDirReadOp inOp, void* inObj, intptr_t a, intptr_t b)
{
    typedef EeWonDirReadOp O;
    LobbyServer* aServer = static_cast<LobbyServer*>(inObj);
    switch (inOp) {
    case O::ServerListInsert: return static_cast<LobbyServerList*>(inObj)->M(LobbyMC::ServerListInsert, a); // the address's value
    case O::ServerConnected: return aServer->IsConnected();
    case O::ServerSetName: aServer->MSetText(LobbyMF::ServerName, GUIString(UnitsText(reinterpret_cast<const uint16_t*>(a), (size_t)b))); return 0;
    case O::ServerSetAddr: aServer->MSet(LobbyMF::ServerAddress, a); return 0; // MSetAddr: the value copied
    case O::ServerSetListed: aServer->M(LobbyMC::ServerSetListed, a); return 0;
    case O::ServerAddressesClear: aServer->M(LobbyMC::ServerAddressesClear); return 0;
    case O::ServerAddressAdd: aServer->M(LobbyMC::ServerAddressAdd, a, b); return 0;
    }
    return 0;
}
// Adds the directory's servers by address and refreshes the listing of each one not connected
void LobbyServerList::UpdateFromDirectory(const DirList& inEntities) { ee_won_lobby_dir_update(this, inEntities.get(), DirReadHost); }

// ---- the game's buffers: their bodies are modelbuf.rs's (R6 WON round 33) over these statements

static intptr_t HeldConst(const RefCount* inObj) { return Held(const_cast<RefCount*>(inObj)); }
static intptr_t HeldText(const GUIString& inText) { return Held(inText.mData.get()); }
static std::string Utf16LE(const std::wstring& inText)
{
    std::string aBytes;
    WideAppendUtf16LE(aBytes, inText);
    return aBytes;
}
static void SinkBytes(intptr_t inSink, const std::string& inBytes) { ee_won_lobby_sink(reinterpret_cast<void*>(inSink), reinterpret_cast<const uint8_t*>(inBytes.data()), inBytes.length()); }
// a read's answer, or THREW (modelbuf.rs) for a read past the buffer's end
template <class F> static intptr_t ReadOr(F f)
{
    try {
        return f();
    } catch (ReadBufferException&) {
        return INTPTR_MIN;
    }
}
#define READ_OR(...) ReadOr([&] { return (intptr_t)(__VA_ARGS__); })
#define READ_OR_V(...) ReadOr([&] { __VA_ARGS__; return (intptr_t)0; })
static intptr_t BufHost(void* inObj, EeWonModelBufOp inOp, intptr_t a, intptr_t b, intptr_t c)
{
    typedef EeWonModelBufOp Op;
    LobbyGame* g = static_cast<LobbyGame*>(inObj);
    LobbyPlayer* pl = static_cast<LobbyPlayer*>(inObj);
    ReadBuffer* rb = reinterpret_cast<ReadBuffer*>(a);
    WriteBuffer* wb = reinterpret_cast<WriteBuffer*>(a);
    switch (inOp) {
    case Op::BufPos: return (intptr_t)(rb->pos());
    case Op::BufData: return (intptr_t)(rb->data());
    case Op::BufLength: return (intptr_t)(rb->length());
    case Op::BufSetPos: rb->SetPos((int)b); return 0;
    case Op::ReadShort: return READ_OR(rb->ReadShort());
    case Op::ReadBool: return READ_OR(rb->ReadBool());
    case Op::WriteNew: return (intptr_t)(new WriteBuffer(0, true));
    case Op::WriteFree: delete wb; return 0;
    case Op::AppendBytes: wb->AppendBytes(reinterpret_cast<const void*>(b), (unsigned long)c); return 0;
    case Op::AppendByte: wb->AppendByte((unsigned char)b); return 0;
    case Op::AppendShort: wb->AppendShort((short)b); return 0;
    case Op::SetShort: wb->SetShort((unsigned long)b, (short)c); return 0;
    case Op::ToByteBuffer: return (intptr_t)(HeldConst(wb->ToByteBuffer(true).get()));
    case Op::Release: reinterpret_cast<RefCount*>(a)->Release(); return 0;
    case Op::GameType: return (intptr_t)(g->GetGameType());
    case Op::GameAddressSixByte: memcpy(reinterpret_cast<void*>(a), g->GetAddress().GetSixByte(), 6); return 0;
    case Op::ProductId: return (intptr_t)(GetLobbyProductId());
    case Op::InProgress: return (g->IsInProgress()) ? 1 : 0;
    case Op::HasPassword: return (g->GetPassword().length() != 0) ? 1 : 0;
    case Op::InviteOnly: return (g->GetInviteOnly()) ? 1 : 0;
    case Op::AskToJoin: return (g->GetAskToJoin()) ? 1 : 0;
    case Op::Skill: return (intptr_t)(g->GetSkill());
    case Op::NumPlayers: return (intptr_t)(g->GetNumPlayers());
    case Op::MaxPlayers: return (intptr_t)(g->GetMaxPlayers());
    case Op::NameUtf16: SinkBytes(a, Utf16LE((std::wstring)g->GetName())); return 0;
    case Op::WriteSummaryHook: g->WriteSummaryHook(*wb); return 0;
    case Op::StoreSummary: g->M(LobbyMC::GameStoreSummary, a, b); return 0;
    case Op::ReadSummaryHook: return READ_OR(g->ReadSummaryHook(*rb));
    case Op::ReadDetailsHook: return READ_OR(g->ReadDetailsHook(*rb));
    case Op::SetHaveDetails: g->MSet(LobbyMF::GameHaveDetails, true); return 0;
    case Op::Ping: return (intptr_t)(g->GetPing());
    case Op::JoinRequestHook: g->GetJoinGameRequestHook(*wb); return 0;
    case Op::IsFull: return (g->IsFull()) ? 1 : 0;
    case Op::ConfigGet: return (intptr_t)(GetLobbyConfig());
    case Op::ConfigAllowDuplicates: return (reinterpret_cast<LobbyConfig*>(a)->mAllowDuplicateNames) ? 1 : 0;
    case Op::ClientsGet: return (intptr_t)(Held(g->GetClients().get()));
    case Op::ListCount: return (intptr_t)(reinterpret_cast<LobbyClientList*>(a)->GetCount());
    case Op::ListAt: return (intptr_t)(reinterpret_cast<LobbyClientList*>(a)->ClientAt((size_t)b));
    case Op::ClientSameName: return (reinterpret_cast<LobbyClient*>(a)->GetName() == reinterpret_cast<LobbyClient*>(b)->GetName()) ? 1 : 0;
    case Op::PlayerCreate: return (intptr_t)(Held(LobbyPlayer::CreatePlayer()));
    case Op::ClientSetPlayer: reinterpret_cast<LobbyClient*>(a)->SetPlayer(reinterpret_cast<LobbyPlayer*>(b)); return 0;
    case Op::PlayerSetPing: reinterpret_cast<LobbyPlayer*>(a)->SetPing((unsigned short)b); return 0;
    case Op::JoinRequestHookRead: return READ_OR(g->HandleJoinGameRequestHook(*rb, reinterpret_cast<LobbyClient*>(b)));
    case Op::SetNumPlayers: g->MSet(LobbyMF::GameNumPlayers, (long)a); return 0;
    case Op::IterNew: return (intptr_t)(new LobbyClientSetIterator(reinterpret_cast<LobbyClientList*>(a)));
    case Op::IterHasMore: return (reinterpret_cast<LobbyClientSetIterator*>(a)->HasMore()) ? 1 : 0;
    case Op::IterNext: return (intptr_t)(reinterpret_cast<LobbyClientSetIterator*>(a)->Next(reinterpret_cast<LobbyClient**>(b)));
    case Op::IterFree: delete reinterpret_cast<LobbyClientSetIterator*>(a); return 0;
    case Op::ClientId: return (intptr_t)(reinterpret_cast<LobbyClient*>(a)->GetClientId());
    case Op::PlayerWrite: reinterpret_cast<LobbyPlayer*>(a)->WriteToBuffer(*reinterpret_cast<WriteBuffer*>(b)); return 0;
    case Op::JoinReplyHook: g->GetJoinGameReplyHook(reinterpret_cast<LobbyClient*>(a), *reinterpret_cast<WriteBuffer*>(b)); return 0;
    case Op::HandleJoinRequest: return (intptr_t)(g->HandleJoinRequest(*rb, reinterpret_cast<LobbyClient*>(b), *reinterpret_cast<WriteBuffer*>(c)));
    case Op::FindClient: return (intptr_t)(g->FindClient((unsigned short)a));
    case Op::PlayerReadFrom: return (reinterpret_cast<LobbyPlayer*>(a)->ReadFromBuffer(*reinterpret_cast<ReadBuffer*>(b))) ? 1 : 0;
    case Op::JoinReplyHookRead: return READ_OR_V(g->HandleJoinGameReplyHook(*rb));
    case Op::StatusHook: return (intptr_t)(HeldText(g->GetStatusStringHook((short)a)));
    case Op::PlayerReadData: return READ_OR(pl->ReadData(*rb));
    case Op::SelfSetPing: pl->SetPing((unsigned short)a); return 0;
    case Op::SelfSetReady: pl->SetReady(a != 0); return 0;
    case Op::PlayerPing: return (intptr_t)(pl->GetPing());
    case Op::PlayerReady: return (pl->GetReady()) ? 1 : 0;
    case Op::PlayerWriteData: pl->WriteData(*wb); return 0;
    }
    return 0;
}
static intptr_t Buf(void* inObj, EeWonModelBufCall inCall, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0) { return ee_won_modelbuf_call(inObj, BufHost, inCall, a, b, c); }
static ConstSmartPtr<ByteBuffer> AdoptBuffer(intptr_t inHeld)
{
    ConstSmartPtr<ByteBuffer> aBuffer(reinterpret_cast<ByteBuffer*>(inHeld));
    reinterpret_cast<ByteBuffer*>(inHeld)->Release();
    return aBuffer;
}
static GUIString AdoptText(intptr_t inHeld)
{
    GUIString aText(reinterpret_cast<GUIStringData*>(inHeld));
    reinterpret_cast<GUIStringData*>(inHeld)->Release();
    return aText;
}
typedef EeWonModelBufCall BC;

// Writes the game's listing summary (address or product id, name, skill, join flags, players, slots)
void LobbyGame::WriteSummary(WriteBuffer& outBuffer) { Buf(this, BC::WriteSummary, (intptr_t)&outBuffer); }
// Reads a game's listing summary; false for another product or a short buffer
// (a short summary keeps the fields read before the cut, as the original assigned them while reading)
bool LobbyGame::ReadSummary(ReadBuffer& inBuffer) { return Buf(this, BC::ReadSummary, (intptr_t)&inBuffer) != 0; }
// Reads the game code's detailed game description and marks the details as received
bool LobbyGame::ReadDetails(ReadBuffer& inBuffer) { return Buf(this, BC::ReadDetails, (intptr_t)&inBuffer) != 0; }
// Builds the join request for the game: message 1, the local ping and the game code's data
ConstSmartPtr<ByteBuffer> LobbyGame::GetJoinRequest() { return AdoptBuffer(Buf(this, BC::JoinRequest)); }
// Admits or refuses a join request; on success writes the player count and every player into the reply
short LobbyGame::HandleJoinRequest(ReadBuffer& inBuffer, LobbyClient* inClient, WriteBuffer& outBuffer) { return (short)Buf(this, BC::HandleJoinRequest, (intptr_t)&inBuffer, (intptr_t)inClient, (intptr_t)&outBuffer); }
// Handles a join request and builds the reply: message 2, the join status and the players
ConstSmartPtr<ByteBuffer> LobbyGame::GetJoinReply(ReadBuffer& inBuffer, LobbyClient* inClient) { return AdoptBuffer(Buf(this, BC::JoinReply, (intptr_t)&inBuffer, (intptr_t)inClient)); }
// Reads the host's join reply: on success creates every listed player; returns the join status
short LobbyGame::HandleJoinReply(ReadBuffer& inBuffer) { return (short)Buf(this, BC::HandleJoinReply, (intptr_t)&inBuffer); }
// Returns the text for a join status: the lobby's strings, the game code's text, or the number
GUIString LobbyGame::GetStatusString(short status) { return AdoptText(Buf(this, BC::StatusString, status)); }
// Creates a game of the given type through the installed game factory
LobbyGame* LobbyGame::CreateGame(LobbyGameType inType)
{
    LobbyGame* game=LobbyGame::mGameFactory();
    game->SetGameType(inType);
    return game;
}
// Reads the player's ping, ready state and game data; false on a short buffer
bool LobbyPlayer::ReadFromBuffer(ReadBuffer& inBuffer) { return Buf(this, BC::PlayerReadFrom, (intptr_t)&inBuffer) != 0; }
// Writes the player's ping, ready state and game data
void LobbyPlayer::WriteToBuffer(WriteBuffer& outBuffer) { Buf(this, BC::PlayerWriteTo, (intptr_t)&outBuffer); }

// Factories the game code replaces to create its own game and player types: LobbyGame::mGameFactory (0x8dec84)
// and LobbyPlayer::mPlayerFactory (0x8dec88) start as the defaults at 0x7a6730 and 0x7a5140 (no CRT initializer)
LobbyGame::LobbyGameFactory LobbyGame::mGameFactory = LobbyGame::DefaultGameFactory;
LobbyPlayer::LobbyPlayerFactory LobbyPlayer::mPlayerFactory = LobbyPlayer::DefaultPlayerFactory;
}
