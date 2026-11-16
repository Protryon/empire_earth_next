// The lobby model's player, client, client list and game (R6 WONLobby round 10): included by WONLobbyGUI.h inside
// namespace WONAPI where the classes were. Each class is a shell over its Rust object (rust/crates/ee_won/src/
// modelobj.rs: the members and the methods); the shells keep the vtables, the reference counts, the game code's
// hooks and factories, and the methods over buffers and the routing server's records (WONLobby/LobbyModelHost.cpp).
// The rest of the model (friends, rooms, servers, the lists) is WONLobbyModel.h's.
#ifndef EE_WONAPI_WONLOBBYMODELOBJ_H
#define EE_WONAPI_WONLOBBYMODELOBJ_H
#include "rust/ee_won.h"

typedef EeWonModelField LobbyMF;
typedef EeWonModelCall LobbyMC;
struct RoutingClientInfo;

// the shells' base: the Rust object at the first member (asserted by LobbyModelHost.cpp: modelobj.rs reads it)
class LobbyModelObject : public RefCount {
public:
    explicit LobbyModelObject(EeWonModelKind inKind);
    virtual ~LobbyModelObject();
    intptr_t MGet(LobbyMF inField) const { return ee_won_model_get(this->mModel, inField); }
    void MSet(LobbyMF inField, intptr_t inValue) { ee_won_model_set(this->mModel, inField, inValue); }
    intptr_t M(LobbyMC inCall, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0) const
    {
        return ee_won_model_call(this->mModel, inCall, a, b, c, (intptr_t)GetEmptyGUIStringData());
    }
    GUIString MText(LobbyMF inField) const { return GUIString((GUIStringData*)this->MGet(inField)); }
    void MSetText(LobbyMF inField, const GUIString& inText) { this->MSet(inField, (intptr_t)inText.mData.get()); }
    IPAddr MAddr(LobbyMF inField) const
    {
        return IPAddr::FromValue(ee_won_ipaddr_clone((const EeWonIpAddr*)this->MGet(inField)));
    }
    void MSetAddr(LobbyMF inField, const IPAddr& inAddr) { this->MSet(inField, (intptr_t)inAddr.GetValue()); }
    EeWonModel* mModel; // owned
};
// a held object the Rust gave (a SmartPtr return): the SmartPtr's reference, the Rust's given back
template <class T> SmartPtr<T> LobbyModelHeld(intptr_t inObj)
{
    SmartPtr<T> aPtr((T*)inObj);
    if (inObj != 0) ((T*)inObj)->Release();
    return aPtr;
}

// RTTI LobbyPlayer; vtable 0x852c54, four slots; size 0x0c.
class LobbyPlayer : public LobbyModelObject {
public:
    LobbyPlayer() : LobbyModelObject(EeWonModelKind::Player) {}
    virtual bool ReadData(ReadBuffer& inBuffer) { return true; } // returns true (shared body at 0x387ff0)
    virtual void WriteData(WriteBuffer& outBuffer) {} // empty (shared body at 0x30a800)
    virtual bool IsTeammate(LobbyPlayer* inOther) { return false; } // 0x3a90d0: the base player has no teams
    bool ReadFromBuffer(ReadBuffer& inBuffer);
    void WriteToBuffer(WriteBuffer& outBuffer);
    bool IsSameTeam(LobbyPlayer* inOther) { return this->M(LobbyMC::PlayerIsSameTeam, (intptr_t)inOther) != 0; }
protected:
    typedef LobbyPlayer* (*LobbyPlayerFactory)();
    static LobbyPlayerFactory mPlayerFactory;
    static LobbyPlayer* DefaultPlayerFactory() { return new LobbyPlayer; }
public:
    static LobbyPlayer* CreatePlayer() { return LobbyPlayer::mPlayerFactory(); }
    static void SetPlayerFactory(LobbyPlayerFactory factory) { LobbyPlayer::mPlayerFactory=factory; }
    bool GetReady() const { return this->MGet(LobbyMF::PlayerReady) != 0; }
    void SetReady(bool inReady) { this->MSet(LobbyMF::PlayerReady, inReady); }
    unsigned short GetPing() const { return (unsigned short)this->MGet(LobbyMF::PlayerPing); }
    void SetPing(unsigned short inPing) { this->MSet(LobbyMF::PlayerPing, inPing); }
};

// a client of the lobby's room (0x2c bytes in the original: 0x3a51a0)
class LobbyClient : public LobbyModelObject {
public:
    LobbyClient() : LobbyModelObject(EeWonModelKind::Client) {}
    void ClearGroup(unsigned short inId, bool inGame) { this->M(LobbyMC::ClientClearGroup, inId, inGame); }
    bool IsInChatGroup() { return this->M(LobbyMC::ClientInChat) != 0; }
    bool IsFriend() { return this->MGet(LobbyMF::ClientIsFriend) != 0; }
    bool IsInGameGroup() { return this->M(LobbyMC::ClientInGame) != 0; }
    void ReadRoutingInfo(RoutingClientInfo* inInfo);
    void UpdateIgnoreStatus() { this->M(LobbyMC::ClientUpdateIgnore); }
    bool IsAnonymous() { return this->M(LobbyMC::ClientAnonymous) != 0; }
    bool IsMuted(bool inGame) { return this->M(LobbyMC::ClientMuted, inGame) != 0; }
    bool IsAway() { return this->M(LobbyMC::ClientAway) != 0; }
    bool IsAdmin() { return this->M(LobbyMC::ClientAdmin) != 0; }
    bool IsModerator() { return this->M(LobbyMC::ClientModerator) != 0; }
    bool IsCaptain(bool inGame) { return this->M(LobbyMC::ClientIsCaptain, inGame) != 0; }
    void SetCaptain(bool inCaptain, bool inGame) { this->M(LobbyMC::ClientSetCaptain, inCaptain, inGame); }
    void SetPlayer(LobbyPlayer* inPlayer) { this->MSet(LobbyMF::ClientPlayer, (intptr_t)inPlayer); }
    bool HasPlayer() { return this->M(LobbyMC::ClientHasPlayer) != 0; }
    bool IsReady() { return this->M(LobbyMC::ClientIsReady) != 0; }
    unsigned short GetPing() { return (unsigned short)this->M(LobbyMC::ClientPing); }
    bool IsTeammate(LobbyClient* inOther) { return this->M(LobbyMC::ClientIsTeammate, (intptr_t)inOther) != 0; }

    unsigned short GetClientId() { return (unsigned short)this->MGet(LobbyMF::ClientId); }
    void SetIsFriend(bool inFriend) { this->MSet(LobbyMF::ClientIsFriend, inFriend); }
    void SetInvited(bool inValue) { this->MSet(LobbyMF::ClientInvited, inValue); }
    unsigned long GetWONUserId() const { return (unsigned long)this->MGet(LobbyMF::ClientWonUserId); }
    unsigned long GetCDKeyId() const { return (unsigned long)this->MGet(LobbyMF::ClientCdKeyId); }
    unsigned short GetChatGroupId() const { return (unsigned short)this->MGet(LobbyMF::ClientChatGroupId); }
    unsigned short GetGameGroupId() const { return (unsigned short)this->MGet(LobbyMF::ClientGameGroupId); }
    void SetGroupId(unsigned short inId, bool inGame) { this->M(LobbyMC::ClientSetGroupId, inId, inGame); }
    void SetMemberFlags(unsigned char inFlags, bool inGame) { this->M(LobbyMC::ClientSetMemberFlags, inFlags, inGame); }
    bool IsIgnored() const { return this->MGet(LobbyMF::ClientIgnored) != 0; }
    bool IsBlocked() const { return this->MGet(LobbyMF::ClientBlocked) != 0; }
    GUIString GetName() const { return this->MText(LobbyMF::ClientName); }
    void SetName(const GUIString& inName) { this->MSetText(LobbyMF::ClientName, inName); }
    void SetClientId(unsigned short inId) { this->MSet(LobbyMF::ClientId, inId); }
    void SetClientFlags(unsigned long inFlags) { this->MSet(LobbyMF::ClientFlags, (intptr_t)inFlags); }
    void SetBlocked(bool inBlocked) { this->MSet(LobbyMF::ClientBlocked, inBlocked); }
    void SetIgnored(bool inIgnored) { this->MSet(LobbyMF::ClientIgnored, inIgnored); }
    bool IsInvited() const { return this->MGet(LobbyMF::ClientInvited) != 0; }
    SmartPtr<LobbyPlayer> GetPlayer() const { return SmartPtr<LobbyPlayer>((LobbyPlayer*)this->MGet(LobbyMF::ClientPlayer)); }
};

// RTTI LobbyClientList : RefCount; constructor 0x3a55f0, 0x14 bytes. GetCount / ClientAt walk the clients in id order.
class LobbyClientList : public LobbyModelObject {
public:
    LobbyClientList() : LobbyModelObject(EeWonModelKind::ClientList) {}
    void Clear() { this->M(LobbyMC::ListClear); }
    void SyncClients(const std::map<unsigned short, SmartPtr<RoutingClientInfo> >& inMap);
    void UpdateIgnoreStatus() { this->M(LobbyMC::ListUpdateIgnore); }
    void AddClient(LobbyClient* inClient) { this->M(LobbyMC::ListAdd, (intptr_t)inClient); }
    LobbyClient* FindClient(unsigned short inId) { return (LobbyClient*)this->M(LobbyMC::ListFind, inId); }
    SmartPtr<LobbyClient> RemoveClient(unsigned short inId) { return LobbyModelHeld<LobbyClient>(this->M(LobbyMC::ListRemove, inId)); }
    LobbyClient* FindClientByWONId(unsigned long inId) { return (LobbyClient*)this->M(LobbyMC::ListFindByWonId, (intptr_t)inId); }
    void ClearInvited() { this->M(LobbyMC::ListClearInvited); }
    size_t GetCount() const { return (size_t)this->M(LobbyMC::ListCount); }
    LobbyClient* ClientAt(size_t i) const { return (LobbyClient*)this->M(LobbyMC::ListAt, (intptr_t)i); } // in id order
};

// Generic non-owning client iterator: 0x3a5f70..0x3a6140. It walks as the map's iterator walked: the next client
// after the last one returned.
class LobbyClientIterator {
public:
    LobbyClientIterator(LobbyClientList* inList) : mList(inList) { this->Rewind(); }
    bool HasMore() { return this->mList != NULL && this->mList->M(LobbyMC::ListIterHasMore, (intptr_t)&this->mItr) != 0; }
    LobbyClient* Next() { return this->mList != NULL ? (LobbyClient*)this->mList->M(LobbyMC::ListIterNext, (intptr_t)&this->mItr) : NULL; }
    void Rewind() { ee_won_model_cursor_reset(&this->mItr); }
private:
    LobbyClientList* mList;                 // NULL: no clients
    EeWonModelCursor mItr;
};

// Iterator over the clients that have players (provisional name)
class LobbyClientSetIterator {
public:
    LobbyClientSetIterator(LobbyClientList* inList) : mList(inList) { this->Rewind(); }
    bool HasMore() { return this->mList != NULL && this->mList->M(LobbyMC::ListSetHasMore, (intptr_t)&this->mItr) != 0; }
    LobbyPlayer* Next(LobbyClient** outClient)
    {
        return this->mList != NULL ? (LobbyPlayer*)this->mList->M(LobbyMC::ListSetNext, (intptr_t)&this->mItr, (intptr_t)outClient) : NULL;
    }
    void Rewind() { ee_won_model_cursor_reset(&this->mItr); }
private:
    LobbyClientList* mList;
    EeWonModelCursor mItr;
};

// a game of the lobby (0x68 bytes: original factory 0x3a6730; vtable 26 slots, the game's overrides named by offset)
class LobbyGame : public LobbyModelObject {
public:
    void ClearPlayers() { this->M(LobbyMC::GameClearPlayers); }
    bool IsSameGame(LobbyGame* inOther) { return this->M(LobbyMC::GameIsSameGame, (intptr_t)inOther) != 0; }
    void ReadGroup(LobbyGroup* inGroup) { this->M(LobbyMC::GameReadGroup, (intptr_t)inGroup); }
    bool ReadSummaryBuffer(const ByteBuffer* inBuffer);
    ConstSmartPtr<ByteBuffer> GetJoinRequest();
    void HandleHostGame(LobbyClient* inClient) { this->M(LobbyMC::GameHandleHost, (intptr_t)inClient); }
    bool AreAllReady() { return this->M(LobbyMC::GameAllReady) != 0; }
    bool CanStartGame() { return this->M(LobbyMC::GameCanStart) != 0; }
    short HandleJoinRequest(ReadBuffer& inBuffer, LobbyClient* inClient, WriteBuffer& outBuffer);
    ConstSmartPtr<ByteBuffer> GetJoinReply(ReadBuffer& inBuffer, LobbyClient* inClient);
    short HandleJoinReply(ReadBuffer& inBuffer);
    GUIString GetStatusString(short inStatus);
    LobbyGame() : LobbyModelObject(EeWonModelKind::Game) {}
    virtual bool PreNetJoinGameHook() { return true; }
    virtual bool PostNetJoinGameHook() { return true; }
    virtual bool PreNetCreateGameHook() { return true; }
    virtual bool PostNetCreateGameHook() { return true; }
    virtual void PreNetLeaveGameHook() {}
    virtual void PostNetLeaveGameHook() {}
    virtual void NotifyStartGameHook() {}
    virtual bool TryStartGameHook() { return true; } // (EELobbyGame 0x292719: can the game start)
    virtual bool CanStartGameHook() { return true; }
    virtual bool CanReadyHook() { return true; }
    virtual short GetJoinStatusHook() { return 0; } // 0x7a7a80: joinable
    // (EELobbyGame's overrides): +0x30 a new game hosted by inClient, +0x34 the game's version
    virtual void HandleHostGameHook(LobbyClient* inClient) {}
    virtual void GetJoinGameRequestHook(WriteBuffer& outBuffer) {}
    // +0x38 a join request's check
    virtual short HandleJoinGameRequestHook(ReadBuffer& inBuffer, LobbyClient* inClient) { return 0; } // returns 0
    // +0x3c the game's state for a joining client
    virtual void GetJoinGameReplyHook(LobbyClient* inClient, WriteBuffer& outBuffer) {}
    virtual void CopyFromHook(LobbyGame* inGame) {}
    virtual void HandleJoinGameReplyHook(ReadBuffer& inBuffer) {} // inline; +0x44 a joined game's state read
    //: +0x48 a client left (inPlayer: its player), +0x4c a client joined
    virtual void HandlePlayerLeftHook(LobbyClient* inClient, LobbyPlayer* inPlayer) {}
    virtual void HandlePlayerJoinedHook(LobbyClient* inClient) {}
    //: +0x50 the summary (three options), +0x54 the details (options and the slots' clients), +0x58 the summary
    // read back, +0x64 a join status' text
    virtual void WriteSummaryHook(WriteBuffer& outBuffer) {}
    virtual void WriteDetailsHook(WriteBuffer& outBuffer) {}
    virtual bool ReadSummaryHook(ReadBuffer& inBuffer) { return true; }
    virtual bool ReadDetailsHook(ReadBuffer& inBuffer) { return true; } // inline; +0x5c the details read back
    // +0x60 a game message (inType 0x64..0x6a) from inFrom
    virtual void HandleGameMessageHook(unsigned char inType, ReadBuffer& inBuffer, LobbyClient* inFrom) {}
    virtual GUIString GetStatusStringHook(short inStatus) { return gEmptyGUIString; } // returns an empty string
    void CopyFrom(LobbyGame* inGame) { this->M(LobbyMC::GameCopyFrom, (intptr_t)inGame); }
    void SetClients(LobbyClientList* inClients) { this->MSet(LobbyMF::GameClients, (intptr_t)inClients); }
    void SetName(const GUIString& inName) { this->MSetText(LobbyMF::GameName, inName); }
    void SetPassword(const GUIString& inPassword) { this->MSetText(LobbyMF::GamePassword, inPassword); }
    void SetAskToJoin(bool inAsk) { this->MSet(LobbyMF::GameAskToJoin, inAsk); }
    void SetInviteOnly(bool inInvite) { this->MSet(LobbyMF::GameInviteOnly, inInvite); }
    void SetSkill(long inSkill) { this->MSet(LobbyMF::GameSkill, inSkill); }
    void SetAddress(const IPAddr& inAddress) { this->MSetAddr(LobbyMF::GameAddress, inAddress); }
    void SetIsHost(bool inHost) { this->MSet(LobbyMF::GameIsHost, inHost); }
    void SetCaptainId(unsigned short inId) { this->MSet(LobbyMF::GameCaptainId, inId); }
    long GetSkill() const { return (long)this->MGet(LobbyMF::GameSkill); }
    bool GetAskToJoin() const { return this->MGet(LobbyMF::GameAskToJoin) != 0; }
    bool GetHasPassword() const { return this->MGet(LobbyMF::GameHasPassword) != 0; }
    LobbyClient* FindClient(unsigned short inId) { return (LobbyClient*)this->M(LobbyMC::GameFindClient, inId); }
    bool MatchesRoomSpec(long inFlags) { return this->M(LobbyMC::GameMatchesRoomSpec, inFlags) != 0; }
    bool IsPinging() { return this->M(LobbyMC::GamePinging) != 0; }
    bool IsQueryingDetails() { return this->M(LobbyMC::GameQuerying) != 0; }
    bool PreNetJoinGame() { return this->PreNetJoinGameHook(); }
    bool PostNetJoinGame() { return this->PostNetJoinGameHook(); }
    bool PreNetCreateGame() { return this->PreNetCreateGameHook(); }
    bool PostNetCreateGame() { return this->PostNetCreateGameHook(); }
    void PreNetLeaveGame() { this->PreNetLeaveGameHook(); }
    void PostNetLeaveGame() { this->PostNetLeaveGameHook(); }
    void WriteDetails(WriteBuffer& outBuffer) { this->WriteDetailsHook(outBuffer); }
    bool CanReady() { return this->CanReadyHook(); }
    bool TryStartGame() { return this->TryStartGameHook(); }
    void HandlePlayerJoined(LobbyClient* inClient) { this->HandlePlayerJoinedHook(inClient); }
    void HandleGameMessage(unsigned char inType, ReadBuffer& inBuffer, LobbyClient* inFrom) { this->HandleGameMessageHook(inType, inBuffer, inFrom); }
    bool IsFull() { return this->M(LobbyMC::GameIsFull) != 0; }
    bool CanJoin() { return this->GetJoinStatus() == 0; }
    short GetJoinStatus() { return (short)this->M(LobbyMC::GameJoinStatus); }
    void WriteSummary(WriteBuffer& outBuffer);
    bool ReadSummary(ReadBuffer& inBuffer);
    void RemoveClient(LobbyClient* inClient) { this->M(LobbyMC::GameRemoveClient, (intptr_t)inClient); }
    bool ReadDetails(ReadBuffer& inBuffer);

    bool IsInProgress() const { return this->MGet(LobbyMF::GameInProgress) != 0; }
    bool GetShowDetails() const { return this->MGet(LobbyMF::GameShowDetails) != 0; }
    long GetGameType() { return (long)this->MGet(LobbyMF::GameType); }
    long GetNumPlayers() { return (long)this->MGet(LobbyMF::GameNumPlayers); }
    long GetMaxPlayers() { return (long)this->MGet(LobbyMF::GameOpenSlots); }
    void SetMaxPlayers(long inSlots) { this->MSet(LobbyMF::GameOpenSlots, inSlots); }
    unsigned short GetPing() { return (unsigned short)this->MGet(LobbyMF::GamePing); }
    void SetPing(unsigned short inPing) { this->MSet(LobbyMF::GamePing, inPing); }
protected:
    typedef LobbyGame* (*LobbyGameFactory)();
    static LobbyGameFactory mGameFactory;
    static LobbyGame* DefaultGameFactory() { return new LobbyGame; }
public:
    static LobbyGame* CreateGame(LobbyGameType inType);
    static void SetGameFactory(LobbyGameFactory factory) { LobbyGame::mGameFactory=factory; }

    unsigned short GetGroupId() const { return (unsigned short)this->MGet(LobbyMF::GameGroupId); }
    bool GetIsFiltered() const { return this->MGet(LobbyMF::GameFiltered) != 0; }
    GUIString GetName() const { return this->MText(LobbyMF::GameName); }
    GUIString GetPassword() const { return this->MText(LobbyMF::GamePassword); }
    IPAddr GetAddress() const { return this->MAddr(LobbyMF::GameAddress); }
    SmartPtr<LobbyClientList> GetClients() const { return SmartPtr<LobbyClientList>((LobbyClientList*)this->MGet(LobbyMF::GameClients)); }
    unsigned short GetCaptainId() const { return (unsigned short)this->MGet(LobbyMF::GameCaptainId); }
    void SetGameType(long inType) { this->MSet(LobbyMF::GameType, inType); }
    void SetGroupId(unsigned short inId) { this->MSet(LobbyMF::GameGroupId, inId); }
    bool GetHaveDetails() const { return this->MGet(LobbyMF::GameHaveDetails) != 0; }
    void SetInProgress(bool inValue) { this->MSet(LobbyMF::GameInProgress, inValue); }
    bool GetInviteOnly() const { return this->MGet(LobbyMF::GameInviteOnly) != 0; }
    bool IsInvited() const { return this->MGet(LobbyMF::GameInvited) != 0; }
    void SetInvited(bool inValue) { this->MSet(LobbyMF::GameInvited, inValue); }
    void SetIsFiltered(bool inValue) { this->MSet(LobbyMF::GameFiltered, inValue); }
    bool GetIsHost() const { return this->MGet(LobbyMF::GameIsHost) != 0; }
    long GetLastSeenTime() const { return (long)this->MGet(LobbyMF::GameLastSeen); }
    void SetLastSeenTime(long inTime) { this->MSet(LobbyMF::GameLastSeen, inTime); }
    bool GetPingReplied() const { return this->MGet(LobbyMF::GamePingReplied) != 0; }
    void SetPingReplied(bool inValue) { this->MSet(LobbyMF::GamePingReplied, inValue); }
    void SetShowDetails(bool inValue) { this->MSet(LobbyMF::GameShowDetails, inValue); }
};

#endif
