#include <stdio.h>
// C:\EEX\Empire Earth\WONAPI\WONLobby\LobbyModel.cpp (file name inferred: no __FILE__)
// The lobby's data model: clients, friends, games, rooms, players, servers and the server list
// (0x7a51a0..0x7aa9e0, followed by the template functions it instantiates through 0x7ae8d0).
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233

#include <windows.h>
#include <string.h>
#include <time.h>
#include <set>
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
#include "rust/ee_rust.h"
// LobbyGame's join rules, its listing summary's fields and the join request's and a player's heads are ee_won's
// staging module (rust/crates/ee_won/src/staging.rs); the lists' decisions (the routing maps' merges, the
// populations, what the flags say, the ping states, the stale rule, the status texts, the ready check, the
// directory's data objects) are its model module (model.rs). The classes keep their members, maps and the game
// code's hooks.
namespace WONAPI {
LobbyClientMap& GetEmptyClientMap();

// the merge plan of a lobby map and a routing map (both keyed by id): ee_won_model_sync_plan
template <class LobbyMap, class InfoMap>
static std::vector<EeWonSyncStep> ModelSyncPlan(const LobbyMap& inLobby, const InfoMap& inInfos,
                                                std::vector<typename InfoMap::const_iterator>& outInfos)
{
    std::vector<uint16_t> anExisting, anIncoming;
    for (typename LobbyMap::const_iterator it = inLobby.begin(); it != inLobby.end(); ++it)
        anExisting.push_back(it->first);
    for (typename InfoMap::const_iterator it = inInfos.begin(); it != inInfos.end(); ++it) {
        anIncoming.push_back(it->first);
        outInfos.push_back(it);
    }
    std::vector<EeWonSyncStep> aPlan(anExisting.size() + anIncoming.size() + 1);
    size_t aCount = ee_won_model_sync_plan(anExisting.empty() ? NULL : &anExisting[0], anExisting.size(),
                                           anIncoming.empty() ? NULL : &anIncoming[0], anIncoming.size(), &aPlan[0]);
    aPlan.resize(aCount);
    return aPlan;
}

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

// FUNCTION: WON 0x7a51a0
// Creates a lobby client with no id, no room or game membership and no flags
LobbyClient::LobbyClient() : mName(GetEmptyGUIStringData())
{
    this->mClientId = 0xffff;
    this->mChatGroupId = 0xffff;
    this->mGameGroupId = 0xffff;
    this->mClientFlags = 0;
    this->mWONUserId = 0;
    this->mCDKeyId = 0;
    this->mChatMemberFlags = 0;
    this->mGameMemberFlags = 0;
    this->mIgnored = false;
    this->mBlocked = false;
    this->mIsFriend = false;
    this->mIsGameCaptain = false;
    this->mIsChatCaptain = false;
    this->mInvited = false;
}

// FUNCTION: WON 0x7a5260
// Destroys the client, releasing its name and player data
LobbyClient::~LobbyClient() {}

// FUNCTION: WON 0x7a52f0
// Takes the client out of the given chat room or game if it is in it, clearing its flags and captaincy there
void LobbyClient::ClearGroup(unsigned short inId, bool inGame)
{
    if (inGame) {
        if (inId == this->mGameGroupId) {
            this->mGameGroupId = 0xffff; this->mGameMemberFlags = 0; this->mIsGameCaptain = false;
        }
    } else if (inId == this->mChatGroupId) {
        this->mChatGroupId = 0xffff; this->mChatMemberFlags = 0; this->mIsChatCaptain = false;
    }
}

// FUNCTION: WON 0x7a5330
// Returns whether the client is in a chat room
bool LobbyClient::IsInChatGroup() { return this->mChatGroupId != 0xffff; }

// FUNCTION: WON 0x7a5340
// Returns whether the client is in a game
bool LobbyClient::IsInGameGroup() { return this->mGameGroupId != 0xffff; }

// FUNCTION: WON 0x7a5350
// Copies the client's name, flags, ids and CD key id from the routing server's client info
void LobbyClient::ReadRoutingInfo(RoutingClientInfo* inInfo)
{
    this->mName = GUIString(new GUIStringData(inInfo->mName));
    this->mClientFlags = inInfo->mFlags;
    this->mClientId = inInfo->mClientId;
    this->mWONUserId = inInfo->mUserId;
    this->mCDKeyId = inInfo->mAddress;
}

// FUNCTION: WON 0x7a5450
// Refreshes whether the client is ignored by name and blocked by CD key
void LobbyClient::UpdateIgnoreStatus()
{
    this->mIgnored = LobbyIsNameIgnored(this->mName);
    this->mBlocked = LobbyIsCDKeyBlocked(this->mCDKeyId);
}

// FUNCTION: WON 0x7a5470
// Returns whether the client is flagged anonymous on the server
bool LobbyClient::IsAnonymous()
{
    return ee_won_model_client_status((uint32_t)this->mClientFlags, this->mChatMemberFlags, this->mGameMemberFlags)
        .anonymous;
}

// FUNCTION: WON 0x7a5480
// Returns whether the client is muted server-wide or in the given chat room or game
bool LobbyClient::IsMuted(bool inGame)
{
    EeWonClientStatus aStatus =
        ee_won_model_client_status((uint32_t)this->mClientFlags, this->mChatMemberFlags, this->mGameMemberFlags);
    return inGame ? aStatus.muted_game : aStatus.muted_chat;
}

// FUNCTION: WON 0x7a54b0
// Returns whether the client is flagged away
bool LobbyClient::IsAway() { return ee_won_model_client_status((uint32_t)this->mClientFlags, 0, 0).away; }

// FUNCTION: WON 0x7a54c0
// Returns whether the client is a server administrator
bool LobbyClient::IsAdmin() { return ee_won_model_client_status((uint32_t)this->mClientFlags, 0, 0).admin; }

// FUNCTION: WON 0x7a54d0
// Returns whether the client is a moderator
bool LobbyClient::IsModerator() { return ee_won_model_client_status((uint32_t)this->mClientFlags, 0, 0).moderator; }

// FUNCTION: WON 0x7a54e0
// Returns whether the client captains its game or its chat room
bool LobbyClient::IsCaptain(bool inGame) { return inGame ? this->mIsGameCaptain : this->mIsChatCaptain; }

// FUNCTION: WON 0x7a5500
// Sets whether the client captains its game or its chat room
void LobbyClient::SetCaptain(bool inCaptain, bool inGame)
{
    if (inGame) this->mIsGameCaptain = inCaptain;
    else this->mIsChatCaptain = inCaptain;
}

// FUNCTION: WON 0x7a5520
// Attaches the client's in-game player data (NULL detaches it)
void LobbyClient::SetPlayer(LobbyPlayer* inPlayer) { this->mPlayer = inPlayer; }

// FUNCTION: WON 0x7a5580
// Returns whether the client has joined the game as a player
bool LobbyClient::HasPlayer() { return this->mPlayer.get() != NULL; }

// FUNCTION: WON 0x7a5590
// Returns whether the client's player has clicked ready; false without a player
bool LobbyClient::IsReady() { return this->mPlayer.get() != NULL ? this->mPlayer->mIsReady : false; }

// FUNCTION: WON 0x7a55a0
// Returns the client's player ping, or 0xfffe (unknown) without a player
unsigned short LobbyClient::GetPing()
{
    if (this->mPlayer.get() != NULL) return this->mPlayer->mPing;
    return 0xfffe;
}

// FUNCTION: WON 0x7a55c0
// Returns whether both clients have players on the same team
bool LobbyClient::IsTeammate(LobbyClient* inOther)
{
    if (inOther == NULL || this->mPlayer.get() == NULL || inOther->mPlayer.get() == NULL) return false;
    return this->mPlayer->IsSameTeam(inOther->mPlayer.get());
}

// FUNCTION: WON 0x7a55f0
// Creates an empty client list
LobbyClientList::LobbyClientList() {}

// FUNCTION: WON 0x7a56a0
// Destroys the client list, releasing every client
LobbyClientList::~LobbyClientList() {}

// FUNCTION: WON 0x7a5770
// Removes every client from the list
void LobbyClientList::Clear() { this->mClients.clear(); }

// FUNCTION: WON 0x7a5820
// Merges the routing server's client map into the list: drops gone clients, adds new ones, refreshes the rest
// (the plan is ee_won_model_sync_plan; a new client goes in under the routing map's key, its own id)
void LobbyClientList::SyncClients(const RoutingClientMap& inMap)
{
    std::vector<RoutingClientMap::const_iterator> anInfos;
    std::vector<EeWonSyncStep> aPlan = ModelSyncPlan(this->mClients, inMap, anInfos);
    LobbyClientMap::iterator aClient = this->mClients.begin();
    LobbyClientMap::iterator anInsert = this->mClients.end(); // the original hinted begin(), dangling after an erase
    for (size_t k = 0; k < aPlan.size(); ++k) {
        switch (aPlan[k].kind) {
        case 0: this->mClients.erase(aClient++); break;
        case 1: {
            RoutingClientMap::const_iterator anInfo = anInfos[aPlan[k].incoming];
            SmartPtr<LobbyClient> aNewClient = new LobbyClient;
            aNewClient->ReadRoutingInfo(anInfo->second.get());
            anInsert = this->mClients.insert(anInsert, LobbyClientMap::value_type(anInfo->first, aNewClient));
            break;
        }
        case 2: {
            SmartPtr<LobbyClient> aCopy = aClient->second;
            aCopy->ReadRoutingInfo(anInfos[aPlan[k].incoming]->second.get());
            ++aClient;
            break;
        }
        case 3: this->mClients.erase(aClient, this->mClients.end()); aClient = this->mClients.end(); break;
        }
    }
}

// FUNCTION: WON 0x7a5c50
// Refreshes the ignored/blocked state of every client in the list
void LobbyClientList::UpdateIgnoreStatus()
{
    for (LobbyClientMap::iterator it = this->mClients.begin(); it != this->mClients.end(); ++it)
        it->second->UpdateIgnoreStatus();
}

// FUNCTION: WON 0x7a5cc0
// Adds the client under its id, replacing any client with the same id
void LobbyClientList::AddClient(LobbyClient* inClient) {
    unsigned short clientId = inClient->mClientId;
    this->mClients[clientId] = inClient;
}

// FUNCTION: WON 0x7a5d30
// Returns the client with the given id, or NULL
LobbyClient* LobbyClientList::FindClient(unsigned short inId)
{
    LobbyClientMap::iterator anItr = this->mClients.find(inId);
    return anItr != this->mClients.end() ? anItr->second.get() : NULL;
}

// FUNCTION: WON 0x7a5d90
// Removes the client with the given id and returns it (NULL if absent)
SmartPtr<LobbyClient> LobbyClientList::RemoveClient(unsigned short inId)
{
    LobbyClientMap::iterator anItr = this->mClients.find(inId);
    if (anItr != this->mClients.end()) {
        SmartPtr<LobbyClient> aClient = anItr->second;
        this->mClients.erase(anItr);
        return aClient;
    }
    return NULL;
}

// FUNCTION: WON 0x7a5ea0
// Returns the first client with the given WON user id, or NULL
LobbyClient* LobbyClientList::FindClientByWONId(unsigned long inId)
{
    for (LobbyClientMap::iterator anItr = this->mClients.begin(); anItr != this->mClients.end(); ++anItr)
    {
        LobbyClient* aClient = anItr->second;
        if (aClient->GetWONUserId() == inId) return aClient;
    }
    return NULL;
}

// FUNCTION: WON 0x7a5f10
// Clears the invited mark on every client in the list
void LobbyClientList::ClearInvited()
{
    for (LobbyClientMap::iterator anItr = this->mClients.begin(); anItr != this->mClients.end(); ++anItr)
        anItr->second->SetInvited(false);
}

// FUNCTION: WON 0x7a5f70
// Starts iterating the list's clients (an empty list when NULL)
LobbyClientIterator::LobbyClientIterator(LobbyClientList* inList)
    : mClients(inList != NULL ? &inList->mClients : &GetEmptyClientMap())
{
    this->Rewind();
}

// FUNCTION: WON 0x7a5fa0
// Returns the shared empty client map used by iterators over no list
LobbyClientMap& GetEmptyClientMap() { static LobbyClientMap aMap; return aMap; }

// FUNCTION: WON 0x7a60b0
// Returns whether clients remain
bool LobbyClientIterator::HasMore() { return this->mItr != this->mClients->end(); }

// FUNCTION: WON 0x7a60d0
// Returns the next client and advances, or NULL at the end
LobbyClient* LobbyClientIterator::Next()
{
    if (this->mItr != this->mClients->end()) {
        LobbyClient* aClient = this->mItr->second.get();
        ++this->mItr;
        return aClient;
    }
    return NULL;
}

// FUNCTION: WON 0x7a6140
// Restarts the iteration at the first client
void LobbyClientIterator::Rewind() { this->mItr = this->mClients->begin(); }

// FUNCTION: WON 0x7a6150
// Starts iterating the list's clients that joined the game as players (an empty list when NULL)
LobbyClientSetIterator::LobbyClientSetIterator(LobbyClientList* inList)
    : mClients(inList != NULL ? &inList->mClients : &GetEmptyClientMap())
{
    this->Rewind();
}

// FUNCTION: WON 0x7a6180
// Skips to the next client that has a player; returns whether one remains
bool LobbyClientSetIterator::HasMore()
{
    if (this->mReady) return true;
    while (this->mItr != this->mClients->end()) {
        if (this->mItr->second->mPlayer.get() != NULL) {
            this->mReady = true;
            return true;
        }
        ++this->mItr;
    }
    return false;
}

// FUNCTION: WON 0x7a6210
// Returns the next player and advances, also passing out its client; NULL at the end
LobbyPlayer* LobbyClientSetIterator::Next(LobbyClient** outClient)
{
    if (this->HasMore()) {
        LobbyClient* aClient = this->mItr->second.get();
        if (outClient != NULL) *outClient = aClient;
        LobbyPlayer* aPlayer = aClient->mPlayer.get();
        this->mReady = false;
        ++this->mItr;
        return aPlayer;
    }
    return NULL;
}

// FUNCTION: WON 0x7a62a0
// Restarts the iteration at the first client
void LobbyClientSetIterator::Rewind() { this->mItr = this->mClients->begin(); this->mReady = false; }

// FUNCTION: WON 0x7a62b0
// Creates a friends-list entry for the given name and WON user id, with no known location
LobbyFriend::LobbyFriend(const GUIString& inName, unsigned long inUserId)
    : mName(inName), mLocation(GetEmptyGUIStringData())
{
    this->mUserId = inUserId;
    this->mFlags = 0;
    this->mLastSearch = 0;
    this->mLastLocate = 0;
    this->mSearchStart = 0;
    this->mAnonymous = false;
}


// FUNCTION: WON 0x7a6440
// Records the server address, server name and status flags where the friend was found
void LobbyFriend::SetLocation(const IPAddr& inAddress, const GUIString& inLocation,
                          int inFlags)
{
    this->mAddress = inAddress;
    this->mLocation = inLocation;
    this->mFlags = inFlags;
}

// FUNCTION: WON 0x7a65b0
// Forgets where the friend is: address, location name, status, last locate time and anonymity
void LobbyFriend::ClearLocation()
{
    this->mAddress = IPAddr();
    this->mLocation = GetEmptyGUIStringData();
    this->mFlags = 0;
    this->mLastLocate = 0;
    this->mAnonymous = false;
}

// FUNCTION: WON 0x7a66b0
// Sets the friend's status (in a chat room, in a game) and anonymity from their lobby client
void LobbyFriend::SetStatusFromClient(LobbyClient* inClient)
{
    this->mFlags = ee_won_model_friend_status(inClient->IsInChatGroup(), inClient->IsInGameGroup());
    this->mAnonymous = inClient->IsAnonymous();
}

// FUNCTION: WON 0x7a66f0
// Sets whether the friend is anonymous
void LobbyFriend::SetAnonymous(bool inAnonymous) { this->mAnonymous = inAnonymous; }

// FUNCTION: WON 0x7a6700
// Marks the friend as being located (replacing the other status flags) and records when the search started
void LobbyFriend::StartLocate(unsigned long inTime)
{
    this->mFlags = ee_won_model_friend_start_locate();
    this->mSearchStart = inTime;
}

// FUNCTION: WON 0x7a6710
// Clears the locating mark and records the time of the last locate
void LobbyFriend::EndLocate(unsigned long inTime)
{
    this->mFlags = ee_won_model_friend_end_locate(this->mFlags);
    this->mLastLocate = inTime;
}

// FUNCTION: WON 0x7a6790
// Creates an empty game: no captain or room, no players or slots, ping unknown
LobbyGame::LobbyGame() : mName(GetEmptyGUIStringData()), mPassword(GetEmptyGUIStringData())
{
    this->mCaptainId = this->mGroupId = 0xffff;
    this->mGameType = 0;
    this->mInviteOnly = false;
    this->mAskToJoin = false;
    this->mInProgress = false;
    this->mHasPassword = false;
    this->mInvited = false;
    this->mIsHost = false;
    this->mShowDetails = false;
    this->mHaveDetails = false;
    this->mPingReplied = false;
    this->mSkill = 0;
    this->mNumPlayers = 0;
    this->mNumOpenSlots = 0;
    this->mPing = 0xfffe;
    this->mLastSeenTime = 0;
}

// FUNCTION: WON 0x7a69d0
// Copies every setting of another game (name, password, flags, counts, address, ping) and its game-code data
void LobbyGame::CopyFrom(LobbyGame* inGame)
{
    this->mName = inGame->mName;
    this->mPassword = inGame->mPassword;
    this->mHasPassword = inGame->mHasPassword;
    this->mInviteOnly = inGame->mInviteOnly;
    this->mAskToJoin = inGame->mAskToJoin;
    this->mInProgress = inGame->mInProgress;
    this->mInvited = inGame->mInvited;
    this->mIsHost = inGame->mIsHost;
    this->mShowDetails = inGame->mShowDetails;
    this->mHaveDetails = inGame->mHaveDetails;
    this->mPingReplied = inGame->mPingReplied;
    this->mLastSeenTime = inGame->mLastSeenTime;
    this->mNumPlayers = inGame->mNumPlayers;
    this->mNumOpenSlots = inGame->mNumOpenSlots;
    this->mGameType = inGame->mGameType;
    this->mSkill = inGame->mSkill;
    this->mClients = inGame->mClients.get();
    this->mAddress = inGame->mAddress;
    this->mCaptainId = inGame->mCaptainId;
    this->mGroupId = inGame->mGroupId;
    this->mPing = inGame->mPing;
    this->CopyFromHook(inGame);
}

// FUNCTION: WON 0x7a6c10
// Sets the client list of the game's members
void LobbyGame::SetClients(LobbyClientList* inClients) { this->mClients = inClients; }

// FUNCTION: WON 0x7a6c70
// Returns the game member with the given client id, or NULL
LobbyClient* LobbyGame::FindClient(unsigned short inId)
{
    if (this->mClients.get() != NULL) return this->mClients->FindClient(inId);
    return NULL;
}

// FUNCTION: WON 0x7a6d40
// Returns whether the game suits the room: Internet games for flag 4, LAN or direct-connect games for flag 8
bool LobbyGame::MatchesRoomSpec(long inFlags)
{
    return ee_won_game_matches_room_spec((int32_t)inFlags, (int32_t)this->mGameType);
}

// FUNCTION: WON 0x7a6d70
// Returns whether a ping or a details query is outstanding for the game
bool LobbyGame::IsPinging() { return ee_won_model_game_pinging(this->mPing); }

// FUNCTION: WON 0x7a6d90
// Returns whether a details query is outstanding for the game
bool LobbyGame::IsQueryingDetails() { return ee_won_model_game_querying(this->mPing); }

// FUNCTION: WON 0x7a6da0
// Lets the game code prepare before the lobby joins the game on the network
bool LobbyGame::PreNetJoinGame() { return this->PreNetJoinGameHook(); }

// FUNCTION: WON 0x7a6db0
// Lets the game code react after the lobby joined the game on the network
bool LobbyGame::PostNetJoinGame() { return this->PostNetJoinGameHook(); }

// FUNCTION: WON 0x7a6dc0
// Lets the game code prepare before the lobby creates the game on the network
bool LobbyGame::PreNetCreateGame() { return this->PreNetCreateGameHook(); }

// FUNCTION: WON 0x7a6dd0
// Lets the game code react after the lobby created the game on the network
bool LobbyGame::PostNetCreateGame() { return this->PostNetCreateGameHook(); }

// FUNCTION: WON 0x7a6de0
// Lets the game code react before the lobby leaves the game on the network
void LobbyGame::PreNetLeaveGame() { this->PreNetLeaveGameHook(); }

// FUNCTION: WON 0x7a6df0
// Lets the game code react after the lobby left the game on the network
void LobbyGame::PostNetLeaveGame() { this->PostNetLeaveGameHook(); }

// FUNCTION: WON 0x7a6ed0
// Writes the game's listing summary (address or product id, name, skill, join flags, players, slots)
void LobbyGame::WriteSummary(WriteBuffer& outBuffer)
{
    EeWonGameSummary aSummary;
    memset(&aSummary, 0, sizeof(aSummary));
    aSummary.game_type = (int32_t)this->mGameType;
    if (this->mGameType == 2)
        memcpy(aSummary.address, this->mAddress.GetSixByte(), 6);
    else
        aSummary.product_id = GetLobbyProductId();
    aSummary.in_progress = this->mInProgress;
    aSummary.has_password = this->mPassword.length() != 0;
    aSummary.invite_only = this->mInviteOnly;
    aSummary.ask_to_join = this->mAskToJoin;
    aSummary.skill = (int32_t)this->mSkill;
    aSummary.players = (int32_t)this->mNumPlayers;
    aSummary.open_slots = (int32_t)this->mNumOpenSlots;
    std::string aName, aBytes;
    if (this->mGameType != 2)
        WideAppendUtf16LE(aName, (std::wstring)this->mName);
    ee_won_game_write_summary(&aSummary, reinterpret_cast<const uint8_t*>(aName.data()), aName.length(),
                              ModelAppendBytes, &aBytes);
    outBuffer.AppendBytes(aBytes.data(), aBytes.length());
    this->WriteSummaryHook(outBuffer);
}

// FUNCTION: WON 0x7a7000
// Reads a game's listing summary; false for another product or a short buffer
// (a short summary keeps the fields read before the cut, as the original assigned them while reading)
bool LobbyGame::ReadSummary(ReadBuffer& inBuffer)
{
    long aStart = inBuffer.pos();
    EeWonGameSummary aSummary;
    std::wstring aName;
    ee_won_game_read_summary(reinterpret_cast<const uint8_t*>(inBuffer.data()) + aStart,
                             (size_t)(inBuffer.length() - aStart), (int32_t)this->mGameType, GetLobbyProductId(),
                             &aSummary, ModelAppendUnits, &aName);
    if (aSummary.read & 1) this->mAddress.SetSixByte(aSummary.address);
    if (aSummary.read & 2) this->mInProgress = aSummary.in_progress;
    if (aSummary.read & 4) this->mName = GUIString(new GUIStringData(aName));
    if (aSummary.read & 8) this->mSkill = aSummary.skill;
    if (aSummary.read & 0x10) {
        this->mHasPassword = aSummary.has_password;
        this->mInviteOnly = aSummary.invite_only;
        this->mAskToJoin = aSummary.ask_to_join;
    }
    if (aSummary.read & 0x20) this->mNumPlayers = aSummary.players;
    if (aSummary.read & 0x40) this->mNumOpenSlots = aSummary.open_slots;
    if (!aSummary.ok) return false;
    inBuffer.SetPos((int)(aStart + aSummary.rest));
    try {
        return this->ReadSummaryHook(inBuffer);
    }
    catch (ReadBufferException&) { return false; }
}

// FUNCTION: WON 0x7a7240
// Writes the game code's detailed game description
void LobbyGame::WriteDetails(WriteBuffer& outBuffer) { this->WriteDetailsHook(outBuffer); }

// FUNCTION: WON 0x7a7250
// Reads the game code's detailed game description and marks the details as received
bool LobbyGame::ReadDetails(ReadBuffer& inBuffer)
{
    try {
        if (this->ReadDetailsHook(inBuffer)) {
            this->mHaveDetails = true;
            return true;
        }
        return false;
    }
    catch (ReadBufferException&) { return false; }
}

// FUNCTION: WON 0x7a74b0
// Returns whether the game code lets the local player click ready
bool LobbyGame::CanReady() { return this->CanReadyHook(); }

// FUNCTION: WON 0x7a74c0
// Asks the game code to start the game; returns whether it did
bool LobbyGame::TryStartGame() { return this->TryStartGameHook(); }

// FUNCTION: WON 0x7a74d0
// Passes a newly joined player to the game code
void LobbyGame::HandlePlayerJoined(LobbyClient* inClient) { this->HandlePlayerJoinedHook(inClient); }

// FUNCTION: WON 0x7a74e0
// Removes the client's player from the game, lowering the player count and telling the game code
void LobbyGame::RemoveClient(LobbyClient* inClient)
{
    SmartPtr<LobbyPlayer> aPlayer(inClient->mPlayer);
    if (aPlayer.get() == NULL) return;
    --this->mNumPlayers;
    inClient->SetPlayer(NULL);
    this->HandlePlayerLeftHook(inClient, aPlayer.get());
}

// FUNCTION: WON 0x7a79e0
// Passes a game message from another client to the game code
void LobbyGame::HandleGameMessage(unsigned char inType, ReadBuffer& inBuffer, LobbyClient* inFrom)
{ this->HandleGameMessageHook(inType, inBuffer, inFrom); }

// FUNCTION: WON 0x7a7a00
// Returns whether every open slot is taken (never full without slots)
bool LobbyGame::IsFull() { return ee_won_game_is_full((int32_t)this->mNumPlayers, (int32_t)this->mNumOpenSlots); }

// FUNCTION: WON 0x7a7a20
// Returns whether the local player may join the game
bool LobbyGame::CanJoin() { return this->GetJoinStatus() == 0; }

// FUNCTION: WON 0x7a7a30
// Returns why the game cannot be joined (full, not invited, in progress, game code's reason) or 0
short LobbyGame::GetJoinStatus()
{
    short aStatus = ee_won_game_join_status(this->IsFull(), this->mInviteOnly, this->mInvited, this->mIsHost,
                                            this->mInProgress);
    return aStatus >= 0 ? aStatus : this->GetJoinStatusHook();
}

// FUNCTION: WON 0x7a8a90
// Creates an empty chat room or game group with its own client list
LobbyGroup::LobbyGroup() : mName(GetEmptyGUIStringData()), mClients(NULL)
{
    this->mNumClients = 0; this->mMaxClients = 0; this->mFlags = 0;
    this->mId = 0xffff; this->mCaptainId = 0xffff;
    this->mClients = new LobbyClientList;
}

// FUNCTION: WON 0x7a8ba0
// Destroys the group, releasing its client list
LobbyGroup::~LobbyGroup() {}

// FUNCTION: WON 0x7a8c30
// Copies a room's id, name, member count, flags, captain and capacity from the routing server; fills members
void LobbyGroup::ReadRoutingInfo(RoutingGroupInfo* inInfo, LobbyClientList* inClients)
{
    this->mId = inInfo->mId;
    this->mName = GUIString(new GUIStringData(inInfo->mName));
    this->mNumClients = inInfo->mMaxPlayers; // the routing field at +0x2a is the current member count despite its name
    this->mFlags = inInfo->mFlags;
    this->mCaptainId = inInfo->mCaptainId;
    this->mMaxClients = inInfo->mMaxClients;
    if (inClients != NULL) this->SetMembers(inInfo->mMembers, inClients);
}

// FUNCTION: WON 0x7a8d50
// Rebuilds the member list from the routing server, setting each member's flags, captaincy and room
void LobbyGroup::SetMembers(const RoutingMemberMap& inMembers, LobbyClientList* inClients)
{
    this->mClients->Clear();
    RoutingMemberMap::const_iterator it = inMembers.begin();
    while (it != inMembers.end()) {
        LobbyClient* client = inClients->FindClient(it->first);
        if (client != NULL) {
            RoutingMemberInfo* aMember = it->second;
            client->SetMemberFlags(aMember->mFlags, this->IsGameGroup());
            client->SetCaptain(client->GetClientId() == this->mCaptainId, this->IsGameGroup());
            client->SetGroupId(this->mId, this->IsGameGroup());
            this->mClients->AddClient(client);
        }
        ++it;
    }
    this->mNumClients = this->mClients->mClients.size();
}

// FUNCTION: WON 0x7a8e40
// Adds the client as a member with the given flags, marking its room and captaincy
void LobbyGroup::AddClient(LobbyClient* inClient, unsigned char inFlags)
{
    this->mClients->AddClient(inClient);
    inClient->SetGroupId(this->mId, this->IsGameGroup());
    inClient->SetMemberFlags(inFlags, this->IsGameGroup());
    inClient->SetCaptain(inClient->GetClientId() == this->mCaptainId, this->IsGameGroup());
    this->mNumClients = this->mClients->mClients.size();
}

// FUNCTION: WON 0x7a8eb0
// Removes the member with the given id, clearing its room membership; returns it
SmartPtr<LobbyClient> LobbyGroup::RemoveClient(unsigned short inId)
{
    SmartPtr<LobbyClient> client = this->mClients->RemoveClient(inId);
    this->mNumClients = this->mClients->mClients.size();
    if (client.get() != NULL) client->ClearGroup(this->mId, this->IsGameGroup());
    return client;
}

// FUNCTION: WON 0x7a8f80
// Moves the captaincy to the given member (0xffff: none); returns the new captain, passes out the old
LobbyClient* LobbyGroup::SetCaptain(unsigned short inId, LobbyClient** outOld)
{
    LobbyClient* oldClient = this->GetCaptain();
    if (oldClient != NULL) oldClient->SetCaptain(false, this->IsGameGroup());
    LobbyClient* newClient = NULL;
    if (inId != 0xffff) {
        newClient = this->FindClient(inId);
        if (newClient != NULL) newClient->SetCaptain(true, this->IsGameGroup());
    }
    if (outOld != NULL) *outOld = oldClient;
    this->mCaptainId = inId;
    return newClient;
}

// FUNCTION: WON 0x7a8ff0
// Returns the group's captain, or NULL
LobbyClient* LobbyGroup::GetCaptain()
{
    if (this->mCaptainId != 0xffff) return this->FindClient(this->mCaptainId);
    return NULL;
}

// FUNCTION: WON 0x7a9010
// Returns the member with the given client id, or NULL
LobbyClient* LobbyGroup::FindClient(unsigned short inId) { return this->mClients->FindClient(inId); }

// FUNCTION: WON 0x7a9020
// Returns whether joining the group needs a password
bool LobbyGroup::IsPasswordProtected() { return ee_won_model_group_kind((uint32_t)this->mFlags).password; }

// FUNCTION: WON 0x7a9030
// Returns whether joining the group needs the captain's approval
bool LobbyGroup::IsAskToJoin() { return ee_won_model_group_kind((uint32_t)this->mFlags).ask_to_join; }

// FUNCTION: WON 0x7a9040
// Returns whether the group can only be joined by invitation
bool LobbyGroup::IsInviteOnly() { return ee_won_model_group_kind((uint32_t)this->mFlags).invite_only; }

// FUNCTION: WON 0x7a9050
// Returns whether the group is a chat room
bool LobbyGroup::IsChatGroup() { return ee_won_model_group_kind((uint32_t)this->mFlags).chat; }

// FUNCTION: WON 0x7a9060
// Returns whether the group is a game
bool LobbyGroup::IsGameGroup() { return !ee_won_model_group_kind((uint32_t)this->mFlags).chat; }

// FUNCTION: WON 0x7a9070
// Returns whether the group is a chat room anyone may enter (no password, invitation or approval)
bool LobbyGroup::IsPublicChatGroup() { return ee_won_model_group_kind((uint32_t)this->mFlags).public_chat; }

// FUNCTION: WON 0x7a90b0
// Creates a player who is not ready and has an unknown ping
LobbyPlayer::LobbyPlayer() { this->mPing=0xfffe; this->mIsReady=false; }

// FUNCTION: WON 0x7a90d0
// Returns false: the base player has no teams
bool LobbyPlayer::IsTeammate(LobbyPlayer* inOther) { return false; }

// FUNCTION: WON 0x7a90f0
// Reads the player's ping, ready state and game data; false on a short buffer
bool LobbyPlayer::ReadFromBuffer(ReadBuffer& inBuffer)
{
    try {
        this->mPing = inBuffer.ReadShort();
        this->mIsReady = inBuffer.ReadBool();
        return this->ReadData(inBuffer);
    }
    catch (ReadBufferException&) {
        return false;
    }
}

// FUNCTION: WON 0x7a9170
// Writes the player's ping, ready state and game data
void LobbyPlayer::WriteToBuffer(WriteBuffer& outBuffer)
{
    std::string aHead;
    ee_won_room_player_head(this->mPing, this->mIsReady, ModelAppendBytes, &aHead);
    outBuffer.AppendBytes(aHead.data(), aHead.length());
    this->WriteData(outBuffer);
}

// FUNCTION: WON 0x7a91a0
// Returns whether the other player is this one or on its team
bool LobbyPlayer::IsSameTeam(LobbyPlayer* inOther)
{
    if (this == inOther) return true;
    if (inOther == NULL) return false;
    return this->IsTeammate(inOther);
}

// FUNCTION: WON 0x7a91c0
// Creates a disconnected lobby server entry with an empty client list
LobbyServer::LobbyServer() : mName(GetEmptyGUIStringData()), mClients(NULL), mConnection(NULL)
{
    this->mNumChatClients = 0; this->mNumClients = 0; this->mLastPing = 0; this->mQueryingGroups = false; this->mMyClientId = 0xffff;
    this->mClients = new LobbyClientList;
}

// FUNCTION: WON 0x7a9490
// Returns whether the server lists the given address name (case-insensitive)
bool LobbyServer::HasAddress(const std::string& inName)
{
    return this->mAddresses.find(inName) != this->mAddresses.end();
}


// FUNCTION: WON 0x7a9640
// Sets the server's routing connection
void LobbyServer::SetConnection(RoutingConnection* inConnection) { this->mConnection = inConnection; }

// FUNCTION: WON 0x7a9ad0
// Merges the routing server's client map into the server's clients and refreshes who is ignored
void LobbyServer::SyncClients(const RoutingClientMap& inMap)
{
    this->mClients->SyncClients(inMap);
    this->mClients->UpdateIgnoreStatus();
}

// FUNCTION: WON 0x7a9af0
// Marks which clients are friends and records this server as each friend's location
void LobbyServer::UpdateFriends()
{
    LobbyClientMap& aClients = this->mClients->mClients;
    for (LobbyClientMap::iterator it = aClients.begin(); it != aClients.end(); ++it) {
        LobbyClient* client = it->second;
        LobbyFriend* aFriend = LobbyFindFriend(client->GetWONUserId());
        if (aFriend != NULL) {
            client->SetIsFriend(true);
            aFriend->SetLocation(this->mAddress, this->mName, 0);
            aFriend->SetStatusFromClient(client);
        } else client->SetIsFriend(false);
    }
}

// FUNCTION: WON 0x7a9b60
// Forgets the location of anonymous friends on the server and stamps the others' last locate time
void LobbyServer::ClearFriendLocations()
{
    time_t now = time(NULL);
    LobbyClientMap& aClients = this->mClients->mClients;
    for (LobbyClientMap::iterator it = aClients.begin(); it != aClients.end(); ++it) {
        LobbyClient* client = it->second;
        if (client->IsFriend()) {
            LobbyFriend* aFriend = LobbyFindFriend(client->GetWONUserId());
            if (aFriend != NULL) {
                if (client->IsAnonymous()) aFriend->ClearLocation();
                else aFriend->SetLastLocate(now);
            }
        }
    }
}

// FUNCTION: WON 0x7a9be0
// Merges the routing server's room map into the server's rooms and recounts the people in them
// (the plan is ee_won_model_sync_plan, the counts EeWonPopulation; a new room goes in under the routing map's key,
// its own id)
void LobbyServer::SyncGroups(const RoutingGroupMap& inGroups, bool inUpdateClients)
{
    EeWonPopulation aPopulation;
    memset(&aPopulation, 0, sizeof(aPopulation));
    LobbyClientList* clients = inUpdateClients ? this->mClients : SmartPtr<LobbyClientList>(NULL);
    std::vector<RoutingGroupMap::const_iterator> anInfos;
    std::vector<EeWonSyncStep> aPlan = ModelSyncPlan(this->mGroups, inGroups, anInfos);
    LobbyGroupMap::iterator groupIt = this->mGroups.begin();
    LobbyGroupMap::iterator insertIt = this->mGroups.end(); // the original hinted begin(), dangling after an erase
    for (size_t k = 0; k < aPlan.size(); ++k) {
        switch (aPlan[k].kind) {
        case 0: this->mGroups.erase(groupIt++); break;
        case 1: {
            RoutingGroupMap::const_iterator infoIt = anInfos[aPlan[k].incoming];
            SmartPtr<LobbyGroup> group = new LobbyGroup;
            group->ReadRoutingInfo(infoIt->second.get(), clients);
            insertIt = this->mGroups.insert(insertIt, LobbyGroupMap::value_type(infoIt->first, group));
            ee_won_model_population(&aPopulation, 0, (uint32_t)group->mFlags, (uint32_t)group->mNumClients);
            break;
        }
        case 2: {
            SmartPtr<LobbyGroup> group = groupIt->second;
            group->ReadRoutingInfo(anInfos[aPlan[k].incoming]->second.get(), clients);
            ee_won_model_population(&aPopulation, 0, (uint32_t)group->mFlags, (uint32_t)group->mNumClients);
            ++groupIt;
            break;
        }
        case 3: this->mGroups.erase(groupIt, this->mGroups.end()); groupIt = this->mGroups.end(); break;
        }
    }
    this->mNumChatClients = aPopulation.chat;
    this->mNumClients = aPopulation.total;
}

// FUNCTION: WON 0x7a9f50
// Creates a client from the routing server's info, checks ignore/block and adds it
LobbyClient* LobbyServer::AddClient(RoutingClientInfo* inInfo)
{
    SmartPtr<LobbyClient> client = new LobbyClient;
    client->ReadRoutingInfo(inInfo);
    client->UpdateIgnoreStatus();
    this->mClients->AddClient(client.get());
    return client.get();
}

// FUNCTION: WON 0x7aa000
// Adds the client to the room or game, counting it in the chat population for chat rooms
void LobbyServer::AddClientToGroup(LobbyClient* inClient, unsigned char inFlags, LobbyGroup* inGroup)
{
    inGroup->AddClient(inClient, inFlags);
    EeWonPopulation aPopulation = { this->mNumChatClients, this->mNumClients };
    ee_won_model_population(&aPopulation, 1, (uint32_t)inGroup->mFlags, 0);
    this->mNumChatClients = aPopulation.chat;
}

// FUNCTION: WON 0x7aa030
// Creates a room from the routing server's info, fills its members and adds it
LobbyGroup* LobbyServer::AddGroup(RoutingGroupInfo* inInfo)
{
    SmartPtr<LobbyGroup> group = new LobbyGroup;
    group->ReadRoutingInfo(inInfo, this->mClients.get());
    this->mGroups[group->GetId()] = group;
    return group.get();
}

// FUNCTION: WON 0x7aa170
// Removes the room with the given id and returns it (NULL if absent)
SmartPtr<LobbyGroup> LobbyServer::RemoveGroup(unsigned short inId)
{
    LobbyGroupMap::iterator it = this->mGroups.find(inId);
    if (it == this->mGroups.end()) return NULL;
    SmartPtr<LobbyGroup> group = it->second;
    this->mGroups.erase(it);
    return group;
}

// FUNCTION: WON 0x7aa280
// Removes the client with the given id from the server and returns it
SmartPtr<LobbyClient> LobbyServer::RemoveClient(unsigned short inId)
{
    return this->mClients->RemoveClient(inId);
}

// FUNCTION: WON 0x7aa2b0
// Removes the client from the room or game, lowering the chat population for chat rooms
SmartPtr<LobbyClient> LobbyServer::RemoveClientFromGroup(unsigned short inId, LobbyGroup* inGroup)
{
    SmartPtr<LobbyClient> client = inGroup->RemoveClient(inId);
    if (client.get() != NULL) {
        EeWonPopulation aPopulation = { this->mNumChatClients, this->mNumClients };
        ee_won_model_population(&aPopulation, 2, (uint32_t)inGroup->mFlags, 0);
        this->mNumChatClients = aPopulation.chat;
    }
    return client;
}

// FUNCTION: WON 0x7aa370
// Returns the client with the given id, or NULL
LobbyClient* LobbyServer::GetClient(int inId) { return this->mClients->FindClient(inId); }

// FUNCTION: WON 0x7aa380
// Returns the client with the given WON user id, or NULL
LobbyClient* LobbyServer::FindClientByWONId(unsigned long inId) { return this->mClients->FindClientByWONId(inId); }

// FUNCTION: WON 0x7aa390
// Returns the local player's client on this server, or NULL
LobbyClient* LobbyServer::GetMyClient()
{
    if (this->mMyClientId == 0xffff) return NULL;
    return this->GetClient(this->mMyClientId);
}

// FUNCTION: WON 0x7aa3b0
// Returns the room or game with the given id, or NULL
LobbyGroup* LobbyServer::GetGroup(int inId)
{
    LobbyGroupMap::iterator it = this->mGroups.find(inId);
    return it != this->mGroups.end() ? it->second.get() : NULL;
}

// FUNCTION: WON 0x7aa410
// Returns the server's population: the live client count when connected, else the listed count
int LobbyServer::GetNumClients()
{
    return ee_won_model_server_num_clients(this->IsConnected(), (uint32_t)this->mClients->mClients.size(),
                                           this->mNumClients);
}

// FUNCTION: WON 0x7aa430
// Returns whether the lobby is connected to the server
bool LobbyServer::IsConnected() { return this->mConnection.get() != NULL; }

// FUNCTION: WON 0x7aa440
// Removes every room and game of the server
void LobbyServer::ClearGroups() { this->mGroups.clear(); }

// FUNCTION: WON 0x7aa4f0
// Creates an empty server list
LobbyServerList::LobbyServerList() {}

// FUNCTION: WON 0x7aa5a0
// Destroys the server list, releasing every server
LobbyServerList::~LobbyServerList() {}

// FUNCTION: WON 0x7aa670
// Adds the directory's servers by address and refreshes the listing of each one not connected
void LobbyServerList::UpdateFromDirectory(const DirEntityList& inEntities)
{
    for (DirEntityList::const_iterator it = inEntities.begin(); it != inEntities.end(); ++it) {
        DirEntity* entity = it->get();
        LobbyServerMap::iterator serverIt = this->mServers.insert(LobbyServerMap::value_type(entity->GetNetAddrAsIP(), NULL)).first;
        if (serverIt->second.get() == NULL) serverIt->second = new LobbyServer;
        LobbyServer* aServer = serverIt->second.get();
        if (!aServer->IsConnected()) aServer->ReadDirEntity(entity);
    }
}

// FUNCTION: WON 0x7aa980
// Returns the server at the given address, or NULL
LobbyServer* LobbyServerList::FindServer(const IPAddr& inAddress)
{
    LobbyServerMap::iterator it = this->mServers.find(inAddress);
    return it == this->mServers.end() ? NULL : it->second.get();
}


}








namespace WONAPI {
// FUNCTION: WON 0x7a6c90
// Detaches the player data of every member of the game
void LobbyGame::ClearPlayers()
{
    LobbyClientList* clients = this->mClients.get();
    if (clients == NULL) return;
    for (LobbyClientMap::iterator it=clients->mClients.begin(); it!=clients->mClients.end(); ++it)
        it->second->SetPlayer(NULL);
}
// FUNCTION: WON 0x7a6d00
// Returns whether two games are the same: same type and same room (Internet) or same address
bool LobbyGame::IsSameGame(LobbyGame* inOther)
{
    return ee_won_model_same_game((int32_t)this->mGameType, (int32_t)inOther->mGameType, this->mGroupId,
                                  inOther->mGroupId, inOther->mAddress == this->mAddress);
}
// FUNCTION: WON 0x7a6e00
// Copies the game room's name, join restrictions, id, captain and members into the game
void LobbyGame::ReadGroup(LobbyGroup* inGroup)
{
    this->mName = inGroup->mName;
    this->mHasPassword = inGroup->IsPasswordProtected();
    this->mInviteOnly = inGroup->IsInviteOnly();
    this->mAskToJoin = inGroup->IsAskToJoin();
    this->mGroupId = inGroup->mId;
    this->mCaptainId = inGroup->mCaptainId;
    this->mClients = inGroup->mClients;
}
// FUNCTION: WON 0x7a7200
// Reads the game's listing summary from a byte buffer; false without one
bool LobbyGame::ReadSummaryBuffer(const ByteBuffer* inData)
{
    if (inData == NULL) return false;
    ReadBuffer buffer(inData->data(),inData->length());
    return this->ReadSummary(buffer);
}
// FUNCTION: WON 0x7a72c0
// Builds the join request for the game: message 1, the local ping and the game code's data
ConstSmartPtr<ByteBuffer> LobbyGame::GetJoinRequest()
{
    WriteBuffer buffer(0,true);
    EeWonRoomMsg head;
    memset(&head,0,sizeof(head));
    head.kind=1;
    head.ping=this->mPing;
    std::string bytes;
    ee_won_room_message(&head,ModelAppendBytes,&bytes);
    buffer.AppendBytes(bytes.data(),bytes.length());
    this->GetJoinGameRequestHook(buffer);
    return buffer.ToByteBuffer(true);
}
// FUNCTION: WON 0x7a7360
// Gives the host a player, lets the game code set up and counts one player
void LobbyGame::HandleHostGame(LobbyClient* inClient)
{
    SmartPtr<LobbyPlayer> player = LobbyPlayer::CreatePlayer();
    inClient->SetPlayer(player.get());
    this->HandleHostGameHook(inClient);
    this->mNumPlayers = 1;
}
// FUNCTION: WON 0x7a7400
// Returns whether every player except the captain has clicked ready
bool LobbyGame::AreAllReady()
{
    LobbyClientMap& clients = this->mClients->mClients;
    std::vector<EeWonMemberReady> members;
    for (LobbyClientMap::iterator it=clients.begin(); it!=clients.end(); ++it) {
        LobbyClient* client=it->second.get();
        EeWonMemberReady member;
        member.has_player=client->HasPlayer();
        member.game_captain=client->IsCaptain(1);
        member.ready=client->IsReady();
        members.push_back(member);
    }
    return ee_won_model_all_ready(members.empty() ? NULL : &members[0], members.size());
}
// FUNCTION: WON 0x7a7490
// Returns whether all players are ready and the game code allows the start
bool LobbyGame::CanStartGame()
{
    if (!this->AreAllReady()) return false;
    return this->CanStartGameHook();
}
// FUNCTION: WON 0x7a7570
// Admits or refuses a join request; on success writes the player count and every player into the reply
short LobbyGame::HandleJoinRequest(ReadBuffer& inBuffer,LobbyClient* inClient,WriteBuffer& outBuffer)
{
    SmartPtr<LobbyPlayer> player;
    short check=ee_won_game_host_join_check(this->IsFull(),this->mInProgress);
    if (check!=0) return check;
    LobbyConfig* config=GetLobbyConfig();
    if (config!=NULL && !config->mAllowDuplicateNames) {
        for (LobbyClientMap::iterator it=this->mClients->mClients.begin(); it!=this->mClients->mClients.end(); ++it)
            if (it->second->GetName()==inClient->GetName() && it->second.get()!=inClient) return 8;
    }
    player=LobbyPlayer::CreatePlayer();
    inClient->SetPlayer(player.get());
    try {
        short anId=inBuffer.ReadShort();
        player->mPing=anId;
        short result=this->HandleJoinGameRequestHook(inBuffer,inClient);
        if (result!=0) { inClient->SetPlayer(NULL); return result; }
    } catch (ReadBufferException&) {
        inClient->SetPlayer(NULL);
        return 4;
    }
    outBuffer.AppendShort(++this->mNumPlayers);
    LobbyClientSetIterator it(this->mClients.get());
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
// FUNCTION: WON 0x7a7830
// Handles a join request and builds the reply: message 2, the join status and the players
ConstSmartPtr<ByteBuffer> LobbyGame::GetJoinReply(ReadBuffer& inBuffer,LobbyClient* inClient)
{
    WriteBuffer buffer(0,true);
    buffer.AppendByte(2);
    buffer.AppendShort(0);
    buffer.SetShort(1,this->HandleJoinRequest(inBuffer,inClient,buffer));
    return buffer.ToByteBuffer(true);
}
// FUNCTION: WON 0x7a78e0
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

// FUNCTION: WON 0x7a7c50
// Creates an empty game list for the given game type
LobbyGameList::LobbyGameList(int inType) : mIdItr(),mAddrItr() { this->mType=inType; }
// FUNCTION: WON 0x7a7e00
// Destroys the game list, releasing every game
LobbyGameList::~LobbyGameList() {}
// FUNCTION: WON 0x7a7f10
// Removes every game from the list
void LobbyGameList::Clear() { this->mIdGames.clear(); this->mAddrGames.clear(); }
// FUNCTION: WON 0x7a8030
// Adds the game under its room id (Internet) or its address, setting its type to the list's
void LobbyGameList::AddGame(LobbyGame* inGame)
{
    inGame->mGameType=this->mType;
    if (ee_won_model_game_list_by_id(this->mType)) {
        unsigned short id = inGame->mGroupId;
        this->mIdGames[id]=inGame;
    }
    else this->mAddrGames[inGame->mAddress]=inGame;
}
// FUNCTION: WON 0x7a80f0
// Returns the game in the given room, creating it if new; passes out whether it existed
LobbyGame* LobbyGameList::GetOrCreateGame(unsigned short inId,bool* outExisted)
{
    LobbyIdGameMap::iterator it=this->mIdGames.insert(LobbyIdGameMap::value_type(inId,NULL)).first;
    SmartPtr<LobbyGame>& game=it->second;
    if (game.get()==NULL) {
        game=LobbyGame::CreateGame(static_cast<LobbyGameType>(this->mType));
        game->mGroupId=inId;
        if (outExisted!=NULL) *outExisted=false;
    } else if (outExisted!=NULL) *outExisted=true;
    return game.get();
}
// FUNCTION: WON 0x7a8290
// Returns the game at the given address, creating it if new; passes out whether it existed
LobbyGame* LobbyGameList::GetOrCreateGame(const IPAddr& inAddress,bool* outExisted)
{
    LobbyAddrGameMap::iterator it=this->mAddrGames.insert(LobbyAddrGameMap::value_type(inAddress,NULL)).first;
    SmartPtr<LobbyGame>& game=it->second;
    if (game.get()==NULL) {
        game=LobbyGame::CreateGame(static_cast<LobbyGameType>(this->mType));
        game->mAddress=inAddress;
        if (outExisted!=NULL) *outExisted=false;
    } else if (outExisted!=NULL) *outExisted=true;
    return game.get();
}
// FUNCTION: WON 0x7a8560
// Returns the game in the given room, or NULL
LobbyGame* LobbyGameList::GetGame(unsigned short inId)
{
    LobbyIdGameMap::iterator it=this->mIdGames.find(inId);
    return it!=this->mIdGames.end()?it->second.get():NULL;
}
// FUNCTION: WON 0x7a85c0
// Returns the game at the given address, or NULL
LobbyGame* LobbyGameList::GetGame(const IPAddr& inAddress)
{
    LobbyAddrGameMap::iterator it=this->mAddrGames.find(inAddress);
    return it!=this->mAddrGames.end()?it->second.get():NULL;
}
// FUNCTION: WON 0x7a8620
// Removes the game in the given room, resetting its ping; returns it (NULL if absent)
SmartPtr<LobbyGame> LobbyGameList::RemoveGame(unsigned short inId)
{
    LobbyIdGameMap::iterator it=this->mIdGames.find(inId);
    if (it==this->mIdGames.end()) return NULL;
    SmartPtr<LobbyGame> game=it->second;
    game->mPing=0xfffe;
    this->mIdGames.erase(it);
    return game;
}
// FUNCTION: WON 0x7a8730
// Removes the game at the given address, resetting its ping; returns it (NULL if absent)
SmartPtr<LobbyGame> LobbyGameList::RemoveGame(const IPAddr& inAddress)
{
    LobbyAddrGameMap::iterator it=this->mAddrGames.find(inAddress);
    if (it==this->mAddrGames.end()) return NULL;
    SmartPtr<LobbyGame> game=it->second;
    game->mPing=0xfffe;
    this->mAddrGames.erase(it);
    return game;
}
// FUNCTION: WON 0x7a8850
// Restarts iteration over the games
void LobbyGameList::Rewind()
{
    if (ee_won_model_game_list_by_id(this->mType)) this->mIdItr=this->mIdGames.begin();
    else this->mAddrItr=this->mAddrGames.begin();
}
// FUNCTION: WON 0x7a8870
// Returns whether games remain in the iteration
bool LobbyGameList::HasMore()
{
    if (ee_won_model_game_list_by_id(this->mType)) return this->mIdItr!=this->mIdGames.end();
    return this->mAddrItr!=this->mAddrGames.end();
}
// FUNCTION: WON 0x7a88a0
// Returns the next game and advances (no end check: callers test HasMore first)
LobbyGame* LobbyGameList::Next()
{
    if (ee_won_model_game_list_by_id(this->mType)) return (this->mIdItr++)->second.get();
    return (this->mAddrItr++)->second.get();
}
// FUNCTION: WON 0x7a8960
// Returns whether the list has no games
bool LobbyGameList::IsEmpty()
{
    if (ee_won_model_game_list_by_id(this->mType)) return this->mIdGames.empty();
    return this->mAddrGames.empty();
}
// FUNCTION: WON 0x7a8980
// Drops LAN/direct games not seen for longer than the given seconds; returns whether any went
bool LobbyGameList::RemoveStaleGames(long inMaxAge)
{
    if (ee_won_model_game_list_by_id(this->mType)) return false;
    time_t now=time(NULL);
    unsigned long oldSize=this->mAddrGames.size();
    LobbyAddrGameMap::iterator it=this->mAddrGames.begin();
    while (it!=this->mAddrGames.end()) {
        if (ee_won_model_game_stale((int64_t)now,(int32_t)it->second->mLastSeenTime,(int32_t)inMaxAge)) {
            it->second->mPing=0xfffe;
            this->mAddrGames.erase(it++);
        } else ++it;
    }
    return this->mAddrGames.size()<oldSize;
}
}
namespace WONAPI {
// FUNCTION: WON 0x7a68f0
// Destroys the game, releasing its name, password, address and member list
LobbyGame::~LobbyGame() {}
// FUNCTION: WON 0x7a9500
// Destroys the server entry, releasing its rooms, clients, connection and address names
LobbyServer::~LobbyServer() {}

}

namespace WONAPI {
// FUNCTION: WON 0x007a9700
// Reads the server's name, address, population (_C) and address names (_L) from its directory entry
void LobbyServer::ReadDirEntity(DirEntity* inEntity)
{
    this->mName = GUIString(inEntity->mDisplayName);
    this->mAddress = inEntity->GetNetAddrAsIP();
    const DirDataObjectList& aList = inEntity->mDataObjects;
    if (!aList.empty())
    for (DirDataObjectList::const_iterator anItr = aList.begin(); anItr != aList.end(); ++anItr) {
        const DirDataObject& anObject = *anItr;
        uint16_t aCount = 0;
        const ByteBuffer* anData = anObject.mData.get();
        int aKind = ee_won_model_dir_object(reinterpret_cast<const uint8_t*>(anObject.mDataType.data()),
                                            anObject.mDataType.length(),
                                            anData != NULL ? reinterpret_cast<const uint8_t*>(anData->data()) : NULL,
                                            anData != NULL ? (size_t)anData->length() : 0, &aCount);
        if (aKind == 1) {
            this->mNumClients = aCount;
        } else if (aKind == 2) {
            this->mAddresses.clear();
            const ByteBuffer* aData = anObject.mData.get();
            std::string aText(anObject.mData->data(),anObject.mData->length());
            StringParser aParser(aText.c_str());
            std::list<std::string> aNames;
            aParser.ReadValue(aNames);
            for (std::list<std::string>::iterator aName = aNames.begin();
                 aName != aNames.end(); ++aName)
                this->mAddresses.insert(*aName);
        }
    }
}

// (0x7a9ac0, ShortFromLittleEndian: the directory's count is decoded little-endian by ee_won_model_dir_object)
}

namespace WONAPI {
// FUNCTION: WON 0x007a96a0
// Disconnects from the server, keeping its last population and forgetting the local client and clients
void LobbyServer::Disconnect()
{
    if (this->mConnection.get() != NULL) {
        this->ClearFriendLocations();
        this->mNumClients = this->mClients->mClients.size();
        this->mConnection->QueueClose(-1);
        this->mConnection = NULL;
    }
    this->mMyClientId = 0xffff;
    this->mClients->Clear();
}
}

namespace WONAPI {
// FUNCTION: WON 0x007a7a90
// Returns the text for a join status: the lobby's strings, the game code's text, or the number
GUIString LobbyGame::GetStatusString(short status)
{
    switch(ee_won_model_status_text(status)) {
    case 0: return ::LobbyGameStatus_Success_String;
    case 1: return ::LobbyGameStatus_GameFull_String;
    case 2: return ::LobbyGameStatus_GameInProgress_String;
    case 3: return ::LobbyGameStatus_UnpackFailure_String;
    case 4: return ::LobbyGameStatus_NotInvited_String;
    case 5: return ::LobbyGameStatus_CaptainRejectedYou_String;
    case 6: return ::LobbyGameStatus_InvalidPassword_String;
    case 7: return ::LobbyGameStatus_DuplicateName_String;
    }
    GUIString custom=this->GetStatusStringHook(status);
    if(custom.length()!=0) return custom;
    char buffer[50];
    sprintf(buffer,"%d",static_cast<int>(status));
    return GUIString(new GUIStringData(buffer));
}
}

namespace WONAPI {
// Returns whether the client is on the local player's friends list (no marker: the linker folds its body
// with an identical function)
bool LobbyClient::IsFriend()
{
    return this->mIsFriend;
}
}

namespace WONAPI {
// Destroys the player (no marker: the linker folds its body with an identical function)
LobbyPlayer::~LobbyPlayer() {}
}

namespace WONAPI {
// FUNCTION: WON 0x007a7a80
// Returns 0 (joinable): the default for a game code with no extra join rules
short LobbyGame::GetJoinStatusHook()
{
    return 0;
}
}

namespace WONAPI {
// FUNCTION: WON 0x007a6c00
// Creates a game of the given type through the installed game factory
LobbyGame* LobbyGame::CreateGame(LobbyGameType inType)
{
    LobbyGame* game=LobbyGame::mGameFactory();
    game->mGameType=inType;
    return game;
}
// FUNCTION: WON 0x007a90e0
// Creates a player through the installed player factory
LobbyPlayer* LobbyPlayer::CreatePlayer()
{
    return LobbyPlayer::mPlayerFactory();
}
}

namespace WONAPI {
// Factories the game code replaces to create its own game and player types: LobbyGame::mGameFactory (0x8dec84)
// and LobbyPlayer::mPlayerFactory (0x8dec88) start as the defaults at 0x7a6730 and 0x7a5140 (no CRT initializer)
LobbyGame::LobbyGameFactory LobbyGame::mGameFactory = LobbyGame::DefaultGameFactory;
LobbyPlayer::LobbyPlayerFactory LobbyPlayer::mPlayerFactory = LobbyPlayer::DefaultPlayerFactory;
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a5240
// (public: virtual void * __thiscall WONAPI::LobbyClient::`scalar deleting destructor'(unsigned int)    [??_GLobbyClient@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007a5680
// (public: virtual void * __thiscall WONAPI::LobbyClientList::`scalar deleting destructor'(unsigned int)    [??_GLobbyClientList@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007a6380
// (public: virtual __thiscall WONAPI::LobbyFriend::~LobbyFriend(void)    [??1LobbyFriend@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x007a6730
// (protected: static class WONAPI::LobbyGame * __cdecl WONAPI::LobbyGame::DefaultGameFactory(void)    [?DefaultGameFactory@LobbyGame@WONAPI@@KAPAV12@XZ])
// TEMPLATE: WON 0x007a68d0
// (public: virtual void * __thiscall WONAPI::LobbyGame::`scalar deleting destructor'(unsigned int)    [??_GLobbyGame@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007a7d20
// (public: virtual void * __thiscall WONAPI::LobbyGameList::`scalar deleting destructor'(unsigned int)    [??_GLobbyGameList@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007a8b80
// (public: virtual void * __thiscall WONAPI::LobbyGroup::`scalar deleting destructor'(unsigned int)    [??_GLobbyGroup@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007a92f0
// (public: virtual void * __thiscall WONAPI::LobbyServer::`scalar deleting destructor'(unsigned int)    [??_GLobbyServer@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007a93d0
// (public: __thiscall std::map<unsigned short,class WONAPI::SmartPtr<class WONAPI::LobbyGroup>,struct std::less<unsigned short>,class std::allocator<stru)
// TEMPLATE: WON 0x007aa580
// (public: virtual void * __thiscall WONAPI::LobbyServerList::`scalar deleting destructor'(unsigned int)    [??_GLobbyServerList@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a6870
// (public: virtual short __thiscall WONAPI::LobbyGame::Virtual38(class WONAPI::ReadBuffer &,class WONAPI::LobbyClient *)    [?Virtual38@LobbyGame@WONAPI@)
// TEMPLATE: WON 0x007a6890
// (public: virtual class WONAPI::GUIString __thiscall WONAPI::LobbyGame::Virtual64(short)    [?Virtual64@LobbyGame@WONAPI@@UAE?AVGUIString@2@F@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a6360
// (public: virtual void * __thiscall WONAPI::LobbyFriend::`scalar deleting destructor'(unsigned int)    [??_GLobbyFriend@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a5c20
// (public: __thiscall std::pair<unsigned short const ,class WONAPI::SmartPtr<class WONAPI::LobbyClient> >::~pair<unsigned short const ,class WONAPI::Smar)
// TEMPLATE: WON 0x007a9310
// (public: __thiscall std::set<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,struct WONAPI::StringLessNoCase,cl)
// TEMPLATE: WON 0x007aa8f0
// (public: __thiscall std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::LobbyServer> >::~pair<class WONAPI::IPAddr const ,class )

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007a7d40
// (public: __thiscall std::map<unsigned short,class WONAPI::SmartPtr<class WONAPI::LobbyGame>,struct std::less<unsigned short>,class std::allocator<struc)
// TEMPLATE: WON 0x007a84d0
// (public: __thiscall std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::LobbyGame> >::~pair<class WONAPI::IPAddr const ,class WO)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007aace0
// (public: class std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<class WONAPI::LobbyClient> >,struct std::_Mapk<u)
// TEMPLATE: WON 0x007ab310
// (public: class std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<class WONAPI::LobbyGame> >,struct std::_Mapk<uns)
// TEMPLATE: WON 0x007ab990
// (public: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::LobbyGame> >,struct st)
// TEMPLATE: WON 0x007ac070
// (public: class std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<class WONAPI::LobbyGroup> >,struct std::_Mapk<un)
// TEMPLATE: WON 0x007ac560
// (public: __thiscall std::pair<unsigned short const ,class WONAPI::SmartPtr<class WONAPI::LobbyGroup> >::pair<unsigned short const ,class WONAPI::SmartP)
// TEMPLATE: WON 0x007ac5a0
// (public: class std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<class WONAPI::LobbyClient> >,struct std::_Mapk<u)
// TEMPLATE: WON 0x007ac700
// (protected: void __thiscall std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<class WONAPI::LobbyClient> >,struct)
// TEMPLATE: WON 0x007aca40
// (protected: class std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<class WONAPI::LobbyGame> >,struct std::_Mapk<)
// TEMPLATE: WON 0x007acd40
// (public: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::LobbyGame> >,struct st)
// TEMPLATE: WON 0x007acdf0
// (protected: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::LobbyGame> >,struct)
// TEMPLATE: WON 0x007ad0f0
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::basic_string<char,struct s)
// TEMPLATE: WON 0x007ad1a0
// (protected: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::basic_string<char,struc)
// TEMPLATE: WON 0x007ad940
// (protected: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::LobbyServer> >,stru)
// TEMPLATE: WON 0x007adc40
// (protected: void __thiscall std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<class WONAPI::LobbyGame> >,struct s)
// TEMPLATE: WON 0x007adc80
// (protected: void __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::LobbyGame)
// TEMPLATE: WON 0x007adcc0
// (protected: struct std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::LobbyGame> >,struc)
// TEMPLATE: WON 0x007ae260
// (protected: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::basic_string<)
// TEMPLATE: WON 0x007ae2a0
// (protected: struct std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::basic_string<char,stru)
// TEMPLATE: WON 0x007ae330
// (protected: void __thiscall std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<class WONAPI::LobbyGroup> >,struct )
// TEMPLATE: WON 0x007ae370
// (public: class std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::LobbyServer> >,struct )
// TEMPLATE: WON 0x007ae890
// (protected: void __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::LobbyServ)
// TEMPLATE: WON 0x007ae8d0
// (public: void __thiscall std::_Tree<class WONAPI::IPAddr,struct std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::LobbyGame> >)
// TEMPLATE: WON 0x007aa9e0
// (public: class WONAPI::SmartPtr<class WONAPI::LobbyClient> & __thiscall std::map<unsigned short,class WONAPI::SmartPtr<class WONAPI::LobbyClient>,struc)
// TEMPLATE: WON 0x007ab1d0
// (public: class WONAPI::SmartPtr<class WONAPI::LobbyGame> & __thiscall std::map<unsigned short,class WONAPI::SmartPtr<class WONAPI::LobbyGame>,struct st)
// TEMPLATE: WON 0x007ab800
// (public: class WONAPI::SmartPtr<class WONAPI::LobbyGame> & __thiscall std::map<class WONAPI::IPAddr,class WONAPI::SmartPtr<class WONAPI::LobbyGame>,str)
// TEMPLATE: WON 0x007ac610
// (public: struct std::pair<class std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<class WONAPI::LobbyClient> >,st)
// TEMPLATE: WON 0x007ac740
// (protected: class std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<class WONAPI::LobbyClient> >,struct std::_Map)
// TEMPLATE: WON 0x007ad4a0
// (public: struct std::pair<class std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<class WONAPI::LobbyGroup> >,str)
// TEMPLATE: WON 0x007ad590
// (public: class std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<class WONAPI::LobbyGroup> >,struct std::_Mapk<un)
// TEMPLATE: WON 0x007ad640
// (protected: class std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<class WONAPI::LobbyGroup> >,struct std::_Mapk)
// TEMPLATE: WON 0x007add10
// (public: class std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::basic_string<char,struct s)
// TEMPLATE: WON 0x007ae1f0
// (protected: void __thiscall std::_Tree<class std::basic_string<char,struct std::char_traits<char>,class std::allocator<char> >,class std::basic_string<)
// TEMPLATE: WON 0x007ae2f0
// (protected: void __thiscall std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<class WONAPI::LobbyGroup> >,struct )
// TEMPLATE: WON 0x007ae930
// (void __cdecl std::_Construct(struct std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::LobbyGame> > *,struct std::pair<class W)
// TEMPLATE: WON 0x007ae9e0
// (void __cdecl std::_Construct(struct std::pair<class WONAPI::IPAddr const ,class WONAPI::SmartPtr<class WONAPI::LobbyServer> > *,struct std::pair<class)
// TEMPLATE: WON 0x007a6880
// (public: virtual void __thiscall WONAPI::LobbyGame::Virtual60(unsigned char,class WONAPI::ReadBuffer &,class WONAPI::LobbyClient *))
// TEMPLATE: WON 0x0076cff0
// (protected: void __thiscall std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<class WONAPI::LobbyGroup> >,struct )
// TEMPLATE: WON 0x007aab20
// (public: class std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<class WONAPI::LobbyClient> >,struct std::_Mapk<u)
// TEMPLATE: WON 0x007abeb0
// (public: class std::_Tree<unsigned short,struct std::pair<unsigned short const ,class WONAPI::SmartPtr<class WONAPI::LobbyGroup> >,struct std::_Mapk<un)
// TEMPLATE: WON 0x007a5140
// (protected: static class WONAPI::LobbyPlayer * __cdecl WONAPI::LobbyPlayer::DefaultPlayerFactory(void))
