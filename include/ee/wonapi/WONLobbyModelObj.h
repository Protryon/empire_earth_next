// The lobby model's player, client, client list and game (R6 WONLobby round 10): included by WONLobbyGUI.h inside
// namespace WONAPI where the classes were. Each class is a shell over its Rust object (rust/crates/ee_won_net/src//
// modelobj.rs: the members and the methods); the shells keep the vtables, the reference counts, the game code's
// hooks and factories, and the methods over buffers (WONLobby/LobbyModelHost.cpp; the routing server's records are
// read in Rust since R6 WONLobby round 12).
// The rest of the model (friends, rooms, servers, the lists) is WONLobbyModel.h's.
#ifndef EE_WONAPI_WONLOBBYMODELOBJ_H
#define EE_WONAPI_WONLOBBYMODELOBJ_H
#include "rust/won_lobby/records.h"
#include "ee/wonapi/WONDriver.h" // EE_WON_DRV
#include "rust/won_lobby/modelobj.h"
#include "rust/won_net/ipobj.h"
// (rust/won_lobby/eegame.h and ee/wonapi/WONLobbyGuard.h: WONLobbyResource.h, at global scope)

typedef EeWonModelField LobbyMF;
typedef EeWonModelCall LobbyMC;

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
    // the game's player (EELobbyPlayer, folded in R6 WON lobby and network round 38) is the kind EePlayer: its slot,
    // read and written after the ready flag and deciding IsSameTeam, is the Rust object's (modelobj.rs)
    explicit LobbyPlayer(EeWonModelKind inKind = EeWonModelKind::Player) : LobbyModelObject(inKind) {}
    virtual bool ReadData(ReadBuffer& inBuffer) { return true; } // returns true (shared body at 0x387ff0)
    virtual void WriteData(WriteBuffer& outBuffer) {} // empty (shared body at 0x30a800)
    virtual bool IsTeammate(LobbyPlayer* inOther) { return false; } // 0x3a90d0: the base player has no teams
    bool ReadFromBuffer(ReadBuffer& inBuffer);
    void WriteToBuffer(WriteBuffer& outBuffer);
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
#include EE_WON_DRV(Model_LobbyPlayer) // the drivers' members
};

// a client of the lobby's room (0x2c bytes in the original: 0x3a51a0)
class LobbyClient : public LobbyModelObject {
public:
    LobbyClient() : LobbyModelObject(EeWonModelKind::Client) {}
    void SetCaptain(bool inCaptain, bool inGame) { this->M(LobbyMC::ClientSetCaptain, inCaptain, inGame); }
    void SetPlayer(LobbyPlayer* inPlayer) { this->MSet(LobbyMF::ClientPlayer, (intptr_t)inPlayer); }
    bool IsReady() { return this->M(LobbyMC::ClientIsReady) != 0; }
    unsigned short GetPing() { return (unsigned short)this->M(LobbyMC::ClientPing); }
    bool IsTeammate(LobbyClient* inOther) { return this->M(LobbyMC::ClientIsTeammate, (intptr_t)inOther) != 0; }

    unsigned short GetClientId() { return (unsigned short)this->MGet(LobbyMF::ClientId); }
    unsigned long GetWONUserId() const { return (unsigned long)this->MGet(LobbyMF::ClientWonUserId); }
    void SetGroupId(unsigned short inId, bool inGame) { this->M(LobbyMC::ClientSetGroupId, inId, inGame); }
    bool IsIgnored() const { return this->MGet(LobbyMF::ClientIgnored) != 0; }
    bool IsBlocked() const { return this->MGet(LobbyMF::ClientBlocked) != 0; }
    GUIString GetName() const { return this->MText(LobbyMF::ClientName); }
    void SetName(const GUIString& inName) { this->MSetText(LobbyMF::ClientName, inName); }
    void SetBlocked(bool inBlocked) { this->MSet(LobbyMF::ClientBlocked, inBlocked); }
    void SetIgnored(bool inIgnored) { this->MSet(LobbyMF::ClientIgnored, inIgnored); }
    SmartPtr<LobbyPlayer> GetPlayer() const { return SmartPtr<LobbyPlayer>((LobbyPlayer*)this->MGet(LobbyMF::ClientPlayer)); }
#include EE_WON_DRV(Model_LobbyClient) // the drivers' members
};

// RTTI LobbyClientList : RefCount; constructor 0x3a55f0, 0x14 bytes. GetCount / ClientAt walk the clients in id order.
class LobbyClientList : public LobbyModelObject {
public:
    LobbyClientList() : LobbyModelObject(EeWonModelKind::ClientList) {}
    void Clear() { this->M(LobbyMC::ListClear); }
    void AddClient(LobbyClient* inClient) { this->M(LobbyMC::ListAdd, (intptr_t)inClient); }
    LobbyClient* FindClient(unsigned short inId) { return (LobbyClient*)this->M(LobbyMC::ListFind, inId); }
    SmartPtr<LobbyClient> RemoveClient(unsigned short inId) { return LobbyModelHeld<LobbyClient>(this->M(LobbyMC::ListRemove, inId)); }
    LobbyClient* FindClientByWONId(unsigned long inId) { return (LobbyClient*)this->M(LobbyMC::ListFindByWonId, (intptr_t)inId); }
    size_t GetCount() const { return (size_t)this->M(LobbyMC::ListCount); }
    LobbyClient* ClientAt(size_t i) const { return (LobbyClient*)this->M(LobbyMC::ListAt, (intptr_t)i); } // in id order
#include EE_WON_DRV(Model_LobbyClientList) // the drivers' members
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

// the EE game's text of a refused join by the Rust game's answer (1..3); gEmptyGUIString for 0 (EELobbyGameHost.cpp)
GUIString EELobbyStatusString(intptr_t inStatus);

// a game of the lobby (0x68 bytes: original factory 0x3a6730; vtable 26 slots, the game's overrides named by offset)
class LobbyGame : public LobbyModelObject {
public:
    void ClearPlayers() { this->M(LobbyMC::GameClearPlayers); }
    ConstSmartPtr<ByteBuffer> GetJoinRequest();
    bool AreAllReady() { return this->M(LobbyMC::GameAllReady) != 0; }
    bool CanStartGame() { return this->M(LobbyMC::GameCanStart) != 0; }
    short HandleJoinRequest(ReadBuffer& inBuffer, LobbyClient* inClient, WriteBuffer& outBuffer);
    ConstSmartPtr<ByteBuffer> GetJoinReply(ReadBuffer& inBuffer, LobbyClient* inClient);
    short HandleJoinReply(ReadBuffer& inBuffer);
    GUIString GetStatusString(short inStatus);
    // (the EE factory's game: with the Rust game over its host)
    LobbyGame(EeWonEeGameHostFn inEEHost = NULL)
        : LobbyModelObject(EeWonModelKind::Game), mEE(inEEHost ? ee_won_eegame_new_model(this, inEEHost) : NULL) {}
    // R6 WON lobby and network round 37: the class EELobbyGame (RTTI EELobbyGame > LobbyGame, 0x180 bytes; its
    // overrides forwards to the Rust game) folded in: a game the EE factory made (EELobbyGameHost.cpp's
    // EELobbyNewGame) holds the Rust game (ee_won_lobby eegame.rs: the options, the slots, the file transfer, every
    // hook), and its hooks below are the Rust game's; a plain game's (mEE NULL) are the base's
    virtual ~LobbyGame() { ee_won_eegame_free(this->mEE); }
    EeWonEeGame* mEE;                       // owned
    intptr_t EE(EeWonEeGameCall inCall, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0)
    { return ee_won_eegame_call(this->mEE, inCall, a, b, c, d); }
    virtual void PostNetLeaveGameHook() { if (this->mEE) this->EE(EeWonEeGameCall::PostNetLeaveGameHook); } // EE +0x18 0x292797
    virtual void NotifyStartGameHook() { if (this->mEE) this->EE(EeWonEeGameCall::NotifyStartGameHook); }
    virtual bool TryStartGameHook() { return !this->mEE || this->EE(EeWonEeGameCall::TryStartGameHook) != 0; } // EE 0x292719 (false: everyone is on one team)
    virtual bool CanStartGameHook() { return !this->mEE || this->EE(EeWonEeGameCall::CanStartGameHook) != 0; }
    virtual bool CanReadyHook() { return !this->mEE || this->EE(EeWonEeGameCall::CanReadyHook) != 0; } // EE 0x2926df
    virtual short GetJoinStatusHook() { return 0; } // 0x7a7a80: joinable
    // (EELobbyGame's overrides): +0x30 a new game hosted by inClient, +0x34 the game's version
    virtual void HandleHostGameHook(LobbyClient* inClient) { if (this->mEE) this->EE(EeWonEeGameCall::HandleHostGameHook, (intptr_t)inClient); }
    virtual void GetJoinGameRequestHook(WriteBuffer& outBuffer) { if (this->mEE) this->EE(EeWonEeGameCall::GetJoinGameRequestHook, (intptr_t)&outBuffer); }
    // +0x38 a join request's check
    virtual short HandleJoinGameRequestHook(ReadBuffer& inBuffer, LobbyClient* inClient) // the base's 0
    { return this->mEE ? (short)WonRethrow(this->EE(EeWonEeGameCall::HandleJoinGameRequestHook, (intptr_t)&inBuffer, (intptr_t)inClient)) : 0; }
    // +0x3c the game's state for a joining client
    virtual void GetJoinGameReplyHook(LobbyClient* inClient, WriteBuffer& outBuffer)
    { if (this->mEE) this->EE(EeWonEeGameCall::GetJoinGameReplyHook, (intptr_t)inClient, (intptr_t)&outBuffer); }
    virtual void CopyFromHook(LobbyGame* inGame) {}
    virtual void HandleJoinGameReplyHook(ReadBuffer& inBuffer) // +0x44 a joined game's state read
    { if (this->mEE) WonRethrow(this->EE(EeWonEeGameCall::HandleJoinGameReplyHook, (intptr_t)&inBuffer)); }
    //: +0x48 a client left (inPlayer: its player), +0x4c a client joined
    virtual void HandlePlayerLeftHook(LobbyClient* inClient, LobbyPlayer* inPlayer)
    { if (this->mEE) this->EE(EeWonEeGameCall::HandlePlayerLeftHook, (intptr_t)inClient, (intptr_t)inPlayer); }
    virtual void HandlePlayerJoinedHook(LobbyClient* inClient) { if (this->mEE) this->EE(EeWonEeGameCall::HandlePlayerJoinedHook, (intptr_t)inClient); }
    //: +0x50 the summary (three options), +0x54 the details (options and the slots' clients), +0x58 the summary
    // read back, +0x64 a join status' text
    virtual void WriteSummaryHook(WriteBuffer& outBuffer) { if (this->mEE) this->EE(EeWonEeGameCall::WriteSummaryHook, (intptr_t)&outBuffer); }
    virtual void WriteDetailsHook(WriteBuffer& outBuffer) { if (this->mEE) this->EE(EeWonEeGameCall::WriteDetailsHook, (intptr_t)&outBuffer); }
    virtual bool ReadSummaryHook(ReadBuffer& inBuffer) { return !this->mEE || WonRethrow(this->EE(EeWonEeGameCall::ReadSummaryHook, (intptr_t)&inBuffer)) != 0; }
    virtual bool ReadDetailsHook(ReadBuffer& inBuffer) { return !this->mEE || WonRethrow(this->EE(EeWonEeGameCall::ReadDetailsHook, (intptr_t)&inBuffer)) != 0; } // +0x5c
    // +0x60 a game message (inType 0x64..0x6a) from inFrom
    virtual void HandleGameMessageHook(unsigned char inType, ReadBuffer& inBuffer, LobbyClient* inFrom)
    { if (this->mEE) WonRethrow(this->EE(EeWonEeGameCall::HandleGameMessageHook, inType, (intptr_t)&inBuffer, (intptr_t)inFrom)); }
    // the base's an empty string; the EE game's the text of a refused join (EELobbyGameHost.cpp)
    virtual GUIString GetStatusStringHook(short inStatus) { return EELobbyStatusString(this->mEE ? this->EE(EeWonEeGameCall::GetStatusStringHook, inStatus) : 0); }
    void CopyFrom(LobbyGame* inGame) { this->M(LobbyMC::GameCopyFrom, (intptr_t)inGame); }
    void SetName(const GUIString& inName) { this->MSetText(LobbyMF::GameName, inName); }
    void SetPassword(const GUIString& inPassword) { this->MSetText(LobbyMF::GamePassword, inPassword); }
    void SetAskToJoin(bool inAsk) { this->MSet(LobbyMF::GameAskToJoin, inAsk); }
    void SetInviteOnly(bool inInvite) { this->MSet(LobbyMF::GameInviteOnly, inInvite); }
    void SetSkill(long inSkill) { this->MSet(LobbyMF::GameSkill, inSkill); }
    void SetAddress(const IPAddr& inAddress) { this->MSetAddr(LobbyMF::GameAddress, inAddress); }
    long GetSkill() const { return (long)this->MGet(LobbyMF::GameSkill); }
    bool GetAskToJoin() const { return this->MGet(LobbyMF::GameAskToJoin) != 0; }
    bool GetHasPassword() const { return this->MGet(LobbyMF::GameHasPassword) != 0; }
    LobbyClient* FindClient(unsigned short inId) { return (LobbyClient*)this->M(LobbyMC::GameFindClient, inId); }
    bool MatchesRoomSpec(long inFlags) { return this->M(LobbyMC::GameMatchesRoomSpec, inFlags) != 0; }
    bool IsPinging() { return this->M(LobbyMC::GamePinging) != 0; }
    bool IsQueryingDetails() { return this->M(LobbyMC::GameQuerying) != 0; }
    // the SDK's pre/post join, create and leave hooks: neither the base's nor the EE game's do anything (no game
    // class overrides them since EELobbyGame's fold, R6 WON lobby and network round 37)
    bool PreNetJoinGame() { return true; }
    bool PostNetJoinGame() { return true; }
    bool PreNetCreateGame() { return true; }
    bool PostNetCreateGame() { return true; }
    void PreNetLeaveGame() {}
    void PostNetLeaveGame() { this->PostNetLeaveGameHook(); }
    void WriteDetails(WriteBuffer& outBuffer) { this->WriteDetailsHook(outBuffer); }
    bool CanReady() { return this->CanReadyHook(); }
    bool TryStartGame() { return this->TryStartGameHook(); }
    void HandlePlayerJoined(LobbyClient* inClient) { this->HandlePlayerJoinedHook(inClient); }
    void HandleGameMessage(unsigned char inType, ReadBuffer& inBuffer, LobbyClient* inFrom) { this->HandleGameMessageHook(inType, inBuffer, inFrom); }
    bool IsFull() { return this->M(LobbyMC::GameIsFull) != 0; }
    void WriteSummary(WriteBuffer& outBuffer);
    bool ReadSummary(ReadBuffer& inBuffer);
    void RemoveClient(LobbyClient* inClient) { this->M(LobbyMC::GameRemoveClient, (intptr_t)inClient); }
    bool ReadDetails(ReadBuffer& inBuffer);

    bool IsInProgress() const { return this->MGet(LobbyMF::GameInProgress) != 0; }
    long GetGameType() { return (long)this->MGet(LobbyMF::GameType); }
    long GetNumPlayers() { return (long)this->MGet(LobbyMF::GameNumPlayers); }
    long GetMaxPlayers() { return (long)this->MGet(LobbyMF::GameOpenSlots); }
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
    GUIString GetName() const { return this->MText(LobbyMF::GameName); }
    GUIString GetPassword() const { return this->MText(LobbyMF::GamePassword); }
    IPAddr GetAddress() const { return this->MAddr(LobbyMF::GameAddress); }
    SmartPtr<LobbyClientList> GetClients() const { return SmartPtr<LobbyClientList>((LobbyClientList*)this->MGet(LobbyMF::GameClients)); }
    unsigned short GetCaptainId() const { return (unsigned short)this->MGet(LobbyMF::GameCaptainId); }
    void SetGameType(long inType) { this->MSet(LobbyMF::GameType, inType); }
    void SetGroupId(unsigned short inId) { this->MSet(LobbyMF::GameGroupId, inId); }
    void SetInProgress(bool inValue) { this->MSet(LobbyMF::GameInProgress, inValue); }
    bool GetInviteOnly() const { return this->MGet(LobbyMF::GameInviteOnly) != 0; }
    bool GetIsHost() const { return this->MGet(LobbyMF::GameIsHost) != 0; }
    void SetPingReplied(bool inValue) { this->MSet(LobbyMF::GamePingReplied, inValue); }
    void SetShowDetails(bool inValue) { this->MSet(LobbyMF::GameShowDetails, inValue); }
#include EE_WON_DRV(Model_LobbyGame) // the drivers' members
};

#endif
