// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// The lobby model's shells and host (NEW, no original address: R6 WONLobby round 10). LobbyModel.cpp's classes are
// Rust objects (rust/crates/ee_won_lobby/src/modelobj.rs: every member and the methods over them); this unit is the
// shells' base, the host the Rust calls (the factories, the game code's hooks, the routing server's records, the
// persistent ignore and friend lists, the connection's close) and the methods that read and write the C++ buffers
// and records: the routing infos and the directory's entries handed to the Rust as values, the summaries, the join
// request and reply, a player's bytes, the status texts.
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
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyParser.h"
#include "rust/won_lobby/model.h"
#include "rust/won_lobby/modelobj.h"
#include "rust/won_lobby/staging.h"
namespace WONAPI {

static_assert(offsetof(LobbyModelObject, mModel) == 2 * sizeof(void*), "modelobj.rs reads the model at this offset");

static intptr_t Held(RefCount* inObj)
{
    if (inObj != NULL) inObj->AddRef();
    return (intptr_t)inObj;
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
    case O::ClientReadRouting: reinterpret_cast<LobbyClient*>(a)->ReadRoutingInfo(reinterpret_cast<RoutingClientInfo*>(b)); return 0;
    case O::GroupReadRouting:
        reinterpret_cast<LobbyGroup*>(a)->ReadRoutingInfo(reinterpret_cast<RoutingGroupInfo*>(b), reinterpret_cast<LobbyClientList*>(c));
        return 0;
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
    }
    return 0;
}

LobbyModelObject::LobbyModelObject(EeWonModelKind inKind)
    : mModel(ee_won_model_new(this, inKind, ModelHost, GetEmptyGUIStringData())) {}
LobbyModelObject::~LobbyModelObject() { ee_won_model_free(this->mModel); }

static void ModelAppendBytes(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<std::string*>(inContext)->append(reinterpret_cast<const char*>(inBytes), inLength);
}
static void ModelAppendUnits(void* inContext, const uint16_t* inUnits, size_t inCount)
{
    std::wstring* anOut = static_cast<std::wstring*>(inContext);
    for (size_t i = 0; i < inCount; i++)
        WideAppendUtf16Unit(*anOut, inUnits[i]);
}

// ---- the routing server's and the directory's records

// Copies the client's name, flags, ids and CD key id from the routing server's client info
void LobbyClient::ReadRoutingInfo(RoutingClientInfo* inInfo)
{
    intptr_t aValues[4] = { (intptr_t)inInfo->mFlags, inInfo->mClientId, (intptr_t)inInfo->mUserId, (intptr_t)inInfo->mAddress };
    GUIString aName(new GUIStringData(inInfo->mName));
    this->M(LobbyMC::ClientSetRouting, (intptr_t)aName.mData.get(), (intptr_t)aValues);
}

// the merge of a routing map into a model list (the Rust applies ee_won_model_sync_plan): its ids and records
template <class Map> static void SyncArgs(const Map& inMap, std::vector<uint16_t>& outIds, std::vector<intptr_t>& outInfos)
{
    for (typename Map::const_iterator it = inMap.begin(); it != inMap.end(); ++it) {
        outIds.push_back(it->first);
        outInfos.push_back((intptr_t)it->second.get());
    }
}

// Merges the routing server's client map into the list: drops gone clients, adds new ones, refreshes the rest
void LobbyClientList::SyncClients(const RoutingClientMap& inMap)
{
    std::vector<uint16_t> anIds;
    std::vector<intptr_t> anInfos;
    SyncArgs(inMap, anIds, anInfos);
    this->M(LobbyMC::ListSync, anIds.empty() ? 0 : (intptr_t)&anIds[0], anInfos.empty() ? 0 : (intptr_t)&anInfos[0], anIds.size());
}

// Merges the routing server's client map into the server's clients and refreshes who is ignored
void LobbyServer::SyncClients(const RoutingClientMap& inMap)
{
    std::vector<uint16_t> anIds;
    std::vector<intptr_t> anInfos;
    SyncArgs(inMap, anIds, anInfos);
    this->M(LobbyMC::ServerSyncClients, anIds.empty() ? 0 : (intptr_t)&anIds[0], anInfos.empty() ? 0 : (intptr_t)&anInfos[0], anIds.size());
}

// Merges the routing server's room map into the server's rooms and recounts the people in them
void LobbyServer::SyncGroups(const RoutingGroupMap& inGroups, bool inUpdateClients)
{
    std::vector<uint16_t> anIds;
    std::vector<intptr_t> anInfos;
    SyncArgs(inGroups, anIds, anInfos);
    this->M(LobbyMC::ServerSyncGroups, anIds.empty() ? 0 : (intptr_t)&anIds[0], anInfos.empty() ? 0 : (intptr_t)&anInfos[0],
            (intptr_t)((uint64_t)anIds.size() | (uint64_t)inUpdateClients << 32));
}

// Copies a room's id, name, member count, flags, captain and capacity from the routing server; fills members
void LobbyGroup::ReadRoutingInfo(RoutingGroupInfo* inInfo, LobbyClientList* inClients)
{
    // the routing field at +0x2a is the current member count despite its name
    intptr_t aValues[5] = { inInfo->mId, inInfo->mMaxPlayers, (intptr_t)inInfo->mFlags, inInfo->mCaptainId, inInfo->mMaxClients };
    GUIString aName(new GUIStringData(inInfo->mName));
    this->M(LobbyMC::GroupSetRouting, (intptr_t)aName.mData.get(), (intptr_t)aValues);
    if (inClients != NULL) this->SetMembers(inInfo->mMembers, inClients);
}

// Rebuilds the member list from the routing server, setting each member's flags, captaincy and room
void LobbyGroup::SetMembers(const RoutingMemberMap& inMembers, LobbyClientList* inClients)
{
    std::vector<uint16_t> anIds;
    std::vector<uint8_t> aFlags;
    for (RoutingMemberMap::const_iterator it = inMembers.begin(); it != inMembers.end(); ++it) {
        anIds.push_back(it->first);
        aFlags.push_back(it->second->mFlags);
    }
    this->M(LobbyMC::GroupSetMembersList, (intptr_t)inClients);
    this->M(LobbyMC::GroupSetMembers, anIds.empty() ? 0 : (intptr_t)&anIds[0], aFlags.empty() ? 0 : (intptr_t)&aFlags[0], anIds.size());
}

// Reads the server's name, address, population (_C) and address names (_L) from its directory entry
void LobbyServer::ReadDirEntity(DirEntity* inEntity)
{
    this->MSetText(LobbyMF::ServerName, GUIString(inEntity->mDisplayName));
    this->MSetAddr(LobbyMF::ServerAddress, inEntity->GetNetAddrAsIP());
    const DirDataObjectList& aList = inEntity->mDataObjects;
    for (DirDataObjectList::const_iterator anItr = aList.begin(); anItr != aList.end(); ++anItr) {
        const DirDataObject& anObject = *anItr;
        uint16_t aCount = 0;
        const ByteBuffer* anData = anObject.mData.get();
        int aKind = ee_won_model_dir_object(reinterpret_cast<const uint8_t*>(anObject.mDataType.data()),
                                            anObject.mDataType.length(),
                                            anData != NULL ? reinterpret_cast<const uint8_t*>(anData->data()) : NULL,
                                            anData != NULL ? (size_t)anData->length() : 0, &aCount);
        if (aKind == 1) {
            this->M(LobbyMC::ServerSetListed, aCount);
        } else if (aKind == 2) {
            this->M(LobbyMC::ServerAddressesClear);
            std::string aText(anObject.mData->data(), anObject.mData->length());
            StringParser aParser(aText.c_str());
            std::list<std::string> aNames;
            aParser.ReadValue(aNames);
            for (std::list<std::string>::iterator aName = aNames.begin(); aName != aNames.end(); ++aName)
                this->M(LobbyMC::ServerAddressAdd, (intptr_t)aName->data(), (intptr_t)aName->length());
        }
    }
}

// Adds the directory's servers by address and refreshes the listing of each one not connected
void LobbyServerList::UpdateFromDirectory(const DirEntityList& inEntities)
{
    for (DirEntityList::const_iterator it = inEntities.begin(); it != inEntities.end(); ++it) {
        DirEntity* entity = it->get();
        IPAddr anAddress = entity->GetNetAddrAsIP();
        LobbyServer* aServer = (LobbyServer*)this->M(LobbyMC::ServerListInsert, (intptr_t)anAddress.GetValue());
        if (!aServer->IsConnected()) aServer->ReadDirEntity(entity);
    }
}

// ---- the game's buffers

// Writes the game's listing summary (address or product id, name, skill, join flags, players, slots)
void LobbyGame::WriteSummary(WriteBuffer& outBuffer)
{
    EeWonGameSummary aSummary;
    memset(&aSummary, 0, sizeof(aSummary));
    aSummary.game_type = (int32_t)this->GetGameType();
    IPAddr anAddress = this->GetAddress();
    if (aSummary.game_type == 2)
        memcpy(aSummary.address, anAddress.GetSixByte(), 6);
    else
        aSummary.product_id = GetLobbyProductId();
    aSummary.in_progress = this->IsInProgress();
    aSummary.has_password = this->GetPassword().length() != 0;
    aSummary.invite_only = this->GetInviteOnly();
    aSummary.ask_to_join = this->GetAskToJoin();
    aSummary.skill = (int32_t)this->GetSkill();
    aSummary.players = (int32_t)this->GetNumPlayers();
    aSummary.open_slots = (int32_t)this->GetMaxPlayers();
    std::string aName, aBytes;
    if (aSummary.game_type != 2)
        WideAppendUtf16LE(aName, (std::wstring)this->GetName());
    ee_won_game_write_summary(&aSummary, reinterpret_cast<const uint8_t*>(aName.data()), aName.length(),
                              ModelAppendBytes, &aBytes);
    outBuffer.AppendBytes(aBytes.data(), aBytes.length());
    this->WriteSummaryHook(outBuffer);
}

// Reads a game's listing summary; false for another product or a short buffer
// (a short summary keeps the fields read before the cut, as the original assigned them while reading)
bool LobbyGame::ReadSummary(ReadBuffer& inBuffer)
{
    long aStart = inBuffer.pos();
    EeWonGameSummary aSummary;
    std::wstring aName;
    ee_won_game_read_summary(reinterpret_cast<const uint8_t*>(inBuffer.data()) + aStart,
                             (size_t)(inBuffer.length() - aStart), (int32_t)this->GetGameType(), GetLobbyProductId(),
                             &aSummary, ModelAppendUnits, &aName);
    GUIString aText;
    if (aSummary.read & 4) aText = GUIString(new GUIStringData(aName));
    this->M(LobbyMC::GameStoreSummary, (intptr_t)&aSummary, (intptr_t)aText.mData.get());
    if (!aSummary.ok) return false;
    inBuffer.SetPos((int)(aStart + aSummary.rest));
    try {
        return this->ReadSummaryHook(inBuffer);
    }
    catch (ReadBufferException&) { return false; }
}

// Reads the game code's detailed game description and marks the details as received
bool LobbyGame::ReadDetails(ReadBuffer& inBuffer)
{
    try {
        if (this->ReadDetailsHook(inBuffer)) {
            this->MSet(LobbyMF::GameHaveDetails, true);
            return true;
        }
        return false;
    }
    catch (ReadBufferException&) { return false; }
}

// Reads the game's listing summary from a byte buffer; false without one
bool LobbyGame::ReadSummaryBuffer(const ByteBuffer* inData)
{
    if (inData == NULL) return false;
    ReadBuffer buffer(inData->data(),inData->length());
    return this->ReadSummary(buffer);
}

// Builds the join request for the game: message 1, the local ping and the game code's data
ConstSmartPtr<ByteBuffer> LobbyGame::GetJoinRequest()
{
    WriteBuffer buffer(0,true);
    EeWonRoomMsg head;
    memset(&head,0,sizeof(head));
    head.kind=1;
    head.ping=this->GetPing();
    std::string bytes;
    ee_won_room_message(&head,ModelAppendBytes,&bytes);
    buffer.AppendBytes(bytes.data(),bytes.length());
    this->GetJoinGameRequestHook(buffer);
    return buffer.ToByteBuffer(true);
}

// Admits or refuses a join request; on success writes the player count and every player into the reply
short LobbyGame::HandleJoinRequest(ReadBuffer& inBuffer,LobbyClient* inClient,WriteBuffer& outBuffer)
{
    SmartPtr<LobbyPlayer> player;
    short check=ee_won_game_host_join_check(this->IsFull(),this->IsInProgress());
    if (check!=0) return check;
    LobbyConfig* config=GetLobbyConfig();
    SmartPtr<LobbyClientList> clients=this->GetClients();
    if (config!=NULL && !config->mAllowDuplicateNames) {
        for (size_t i=0; i<clients->GetCount(); ++i) {
            LobbyClient* aMember=clients->ClientAt(i);
            if (aMember->GetName()==inClient->GetName() && aMember!=inClient) return 8;
        }
    }
    player=LobbyPlayer::CreatePlayer();
    inClient->SetPlayer(player.get());
    try {
        short anId=inBuffer.ReadShort();
        player->SetPing(anId);
        short result=this->HandleJoinGameRequestHook(inBuffer,inClient);
        if (result!=0) { inClient->SetPlayer(NULL); return result; }
    } catch (ReadBufferException&) {
        inClient->SetPlayer(NULL);
        return 4;
    }
    long aPlayers=this->GetNumPlayers()+1;
    this->MSet(LobbyMF::GameNumPlayers, aPlayers);
    outBuffer.AppendShort(aPlayers);
    SmartPtr<LobbyClientList> aMembers=this->GetClients();
    LobbyClientSetIterator it(aMembers.get());
    while (it.HasMore()) {
        LobbyClient* client;
        LobbyPlayer* otherPlayer=it.Next(&client);
        if (otherPlayer!=NULL) {
            outBuffer.AppendShort(client->GetClientId());
            otherPlayer->WriteToBuffer(outBuffer);
        }
    }
    this->GetJoinGameReplyHook(inClient,outBuffer);
    return 0;
}

// Handles a join request and builds the reply: message 2, the join status and the players
ConstSmartPtr<ByteBuffer> LobbyGame::GetJoinReply(ReadBuffer& inBuffer,LobbyClient* inClient)
{
    WriteBuffer buffer(0,true);
    buffer.AppendByte(2);
    buffer.AppendShort(0);
    buffer.SetShort(1,this->HandleJoinRequest(inBuffer,inClient,buffer));
    return buffer.ToByteBuffer(true);
}

// Reads the host's join reply: on success creates every listed player; returns the join status
short LobbyGame::HandleJoinReply(ReadBuffer& inBuffer)
{
    short result;
    try {
        result=inBuffer.ReadShort();
        if (result!=0) return result;
        unsigned short count=inBuffer.ReadShort();
        for (int i=0; i<count; ++i) {
            LobbyClient* client=this->FindClient(inBuffer.ReadShort());
            if (client!=NULL) {
                SmartPtr<LobbyPlayer> player=LobbyPlayer::CreatePlayer();
                if (player->ReadFromBuffer(inBuffer)) client->SetPlayer(player.get());
            }
        }
        this->HandleJoinGameReplyHook(inBuffer);
    } catch (ReadBufferException&) { result=4; }
    return result;
}

// Returns the text for a join status: the lobby's strings, the game code's text, or the number
GUIString LobbyGame::GetStatusString(short status)
{
    switch(ee_won_model_status_text(status)) {
    case 0: return LOBBY_STRING(LobbyGameStatus_Success_String);
    case 1: return LOBBY_STRING(LobbyGameStatus_GameFull_String);
    case 2: return LOBBY_STRING(LobbyGameStatus_GameInProgress_String);
    case 3: return LOBBY_STRING(LobbyGameStatus_UnpackFailure_String);
    case 4: return LOBBY_STRING(LobbyGameStatus_NotInvited_String);
    case 5: return LOBBY_STRING(LobbyGameStatus_CaptainRejectedYou_String);
    case 6: return LOBBY_STRING(LobbyGameStatus_InvalidPassword_String);
    case 7: return LOBBY_STRING(LobbyGameStatus_DuplicateName_String);
    }
    GUIString custom=this->GetStatusStringHook(status);
    if(custom.length()!=0) return custom;
    char buffer[50];
    sprintf(buffer,"%d",static_cast<int>(status));
    return GUIString(new GUIStringData(buffer));
}

// Creates a game of the given type through the installed game factory
LobbyGame* LobbyGame::CreateGame(LobbyGameType inType)
{
    LobbyGame* game=LobbyGame::mGameFactory();
    game->SetGameType(inType);
    return game;
}

// Reads the player's ping, ready state and game data; false on a short buffer
bool LobbyPlayer::ReadFromBuffer(ReadBuffer& inBuffer)
{
    try {
        this->SetPing(inBuffer.ReadShort());
        this->SetReady(inBuffer.ReadBool());
        return this->ReadData(inBuffer);
    }
    catch (ReadBufferException&) {
        return false;
    }
}

// Writes the player's ping, ready state and game data
void LobbyPlayer::WriteToBuffer(WriteBuffer& outBuffer)
{
    std::string aHead;
    ee_won_room_player_head(this->GetPing(), this->GetReady(), ModelAppendBytes, &aHead);
    outBuffer.AppendBytes(aHead.data(), aHead.length());
    this->WriteData(outBuffer);
}

// Factories the game code replaces to create its own game and player types: LobbyGame::mGameFactory (0x8dec84)
// and LobbyPlayer::mPlayerFactory (0x8dec88) start as the defaults at 0x7a6730 and 0x7a5140 (no CRT initializer)
LobbyGame::LobbyGameFactory LobbyGame::mGameFactory = LobbyGame::DefaultGameFactory;
LobbyPlayer::LobbyPlayerFactory LobbyPlayer::mPlayerFactory = LobbyPlayer::DefaultPlayerFactory;
}
