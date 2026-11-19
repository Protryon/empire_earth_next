// C:\EEX\Empire Earth\Communications\EEServer.cpp (__FILE__ 0x4bb934): the game's lock-step server, 0xca90f..0xcd4f9
// (the WON error strings 0xcd4f9 open EEWonNet.cpp). Unit flags: the exe's module set.
// Game messages round 6: the server's and client's decisions (the connection handling, the boots and removals, the
// lobby's slots and players, the communications ticks' inputs, the votes, the laggers, the slot info, the migration's
// restart, the client's answers) are ee_netmsg session.rs over the host at the end of this file (EESessionHost: the
// world, lobby, UI and engine calls those members made); the members below forward to it. The golden:
// tests/net/ee_session_test.cpp (the original units, tests/net/orig_ee, over a stand-in world).
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/U2DSparseArray.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/UIControl.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMiniMap.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEMessage.h"
#include "ee/EEAction.h"
#include "ee/EEScenarioManager.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEDatabaseTables.h"
#include <mbstring.h>
#include "ee/EECommunications.h"
#include "ee/EEUIManager.h"
#include "ee/EEGameOptions.h"
#include "ee/EECPEngine.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEScreenEvent.h"
#include "rust/game/screens.h"   // EE_SCREENS_MG_*: the main game form's events (ee_screens main_game.rs)
#include "rust/sim/netmsg.h"
#include "ee/EEWonNet.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EEARust.h"
#include "lle/UResourceFile.h"
#include "ee/EETeam.h"
#include "ee/EEClient.h"
#include "ee/EEGameSound.h"
#include "ee/EEPhysics.h"
#include "ee/EEServer.h"
#include "ee/EEStringIDs.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEHumanPlayer.h"
long gDefaultPlayerColors[16] = {0, 1, 4, 9, 2, 15, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15};
void EELobbyPostConnectStatus(bool inConnected);   // 0x295ce3 (EELobbyInit.cpp; provisional name)
static unsigned long sSample = 0xffffffff;   // 0x4bb8b8 (ExecuteRemovePlayer: the last booted/out-of-synch sound)

// Creates the game server with the lock-step rates from the game preferences, no vote or load running, and its locks
EEServer::EEServer(UWideString& inName, unsigned long inNetworkAdapter, unsigned long inCurrentCommunicationsTick, unsigned long inCurrentPhysicsTick, bool inAllowNewConnections,
                   bool inAnnounce)
    : NESynchronousServer(16, 0x15e, 0x3c, gGamePreferences.mSettings.mMaxPTsPerSecond,
                          gGamePreferences.mSettings.mMinPTsPerSecond,
                          100.0f / gGamePreferences.mSettings.mPTRateTuningThreshold, gGamePreferences.mSettings.mMaxCTSendInterval,
                          gGamePreferences.mSettings.mCTSendIntervalStepUp, gGamePreferences.mSettings.mCTSendIntervalStepDown, inCurrentCommunicationsTick, inCurrentPhysicsTick, inName, inAllowNewConnections,
                          inAnnounce, inNetworkAdapter)
{
    this->mVoteEdict = 0;
    for (long i = 0; i < 256; i++)
        this->mVoted[i] = false;
    this->mYesVotes = 0;
    this->mNoVotes = 0;
    this->mNumVotes = 0;
    this->mNumLoadResponses = 0;
    this->mLoadState = 0;
    this->mNumDroppedLoaders = false;
    this->mHasLeftGame = false;
    this->mLagMask = 0;
    this->mIsMigrating = false;
    this->ResetFastSelection();
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mSearchForSlotLock) == keseOK, 103);
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mFastSelectionLock) == keseOK, 106);
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mAttemptToRemovePlayerLock) == keseOK, 109);
}

// Destroys the server, stopping its threads and releasing its three locks
EEServer::~EEServer()
{
    this->StopThreads();
    TSScheduler::DestroyCriticalSection(this->mSearchForSlotLock);
    TSScheduler::DestroyCriticalSection(this->mFastSelectionLock);
    TSScheduler::DestroyCriticalSection(this->mAttemptToRemovePlayerLock);
}

// ---- the members whose decisions are ee_netmsg session.rs's (EESession: the host's frame, then the Rust) ----

// A join request: refused for a wrong version or checksum, a closed game, a name in use or a full game; else a slot,
// the options and players sent and the saved game's or scenario's transfer started
void EEServer::ProcessConnectionRequest(unsigned char inSlot, const EEGameOptionsPlayer& inPlayer,
                                        const UWideString& inVersion, unsigned long inChecksum)
{
    EESession(this, true, EE_NETMSG_SESS_DO_CONNECTION_REQUEST, inSlot, (long long)inChecksum, 0, 0, &inPlayer, &inVersion);
}

// Removes a client: in a running game its player; the host leaving tells every client; in the lobby its slot reopened
void EEServer::BootPlayer(unsigned char inSlot, bool inWasBooted, bool inGameIsExiting)
{
    EESession(this, true, EE_NETMSG_SESS_DO_BOOT, inSlot, inWasBooted, inGameIsExiting);
}

// A player's removal on the host: ours ends the game, another's is announced and removed
void EEServer::ExecuteRemovePlayer(unsigned char inPlayer, bool inWasBooted, bool inPlayerIsOOS, bool inGameIsExiting)
{
    EESession(this, true, EE_NETMSG_SESS_DO_REMOVE, inPlayer, inWasBooted, inPlayerIsOOS, inGameIsExiting);
}

// A system message on the host: ping answers, communications ticks, join requests, disconnections, reconnections
void EEServer::ProcessSystemMessage(NEMessage* inMessage)
{
    EESession(this, true, EE_NETMSG_SESS_DO_SYSTEM_MESSAGE, (long long)(uintptr_t)inMessage);
}

// Filters a client message (whether to pass it on): votes, pause and join requests; the host removed turns client
bool EEServer::FilterPlayerInput(NEMessage* inMessage)
{
    return EESession(this, true, EE_NETMSG_SESS_DO_FILTER, (long long)(uintptr_t)inMessage) != 0;
}

// Stamps the host's local input with the fast-selection number; selection messages take a new one and run at once
void EEServer::PrepareLocalInput(NEMessage* inMessage)
{
    EESession(this, true, EE_NETMSG_SESS_DO_LOCAL_INPUT, (long long)(uintptr_t)inMessage);
}

// A lobby message on the host: player info, transfer status, the game start, a joined player, slot info
void EEServer::HandleLobbyInput(NEMessage* inMessage)
{
    EESession(this, true, EE_NETMSG_SESS_DO_LOBBY_INPUT, (long long)(uintptr_t)inMessage);
}

// Starts an edict vote (a running one closed with yes votes); true
bool EEServer::StartVote(unsigned short inEdict)
{
    return EESession(this, true, EE_NETMSG_SESS_DO_START_VOTE, inEdict) != 0;
}

// A slot's vote on the edict; once all have voted the result is broadcast
void EEServer::CastVote(unsigned char inIndex, bool inYes, unsigned char inCount)
{
    EESession(this, true, EE_NETMSG_SESS_DO_CAST_VOTE, inIndex, inYes, inCount);
}

// Restarts the game as the new host: the humans' migration slots, the new host announced, the restart
void EEServer::HandleReconnectWaitTimedOut()
{
    EESession(this, true, EE_NETMSG_SESS_DO_RECONNECT_TIMED_OUT);
}

// Broadcasts the lagging players: in multiplayer as a player bit mask (when it changed or forced), else the count
void EEServer::ReportLaggers(std::vector<unsigned char>& inSlots, bool inFlag)
{
    EESession(this, true, EE_NETMSG_SESS_DO_LAGGERS, inSlots.empty() ? 0 : (long long)(uintptr_t)&inSlots[0],
              (long long)inSlots.size(), inFlag);
}

// Sends every human player's network slot, address and firewall flag, when more than one human is left
void EEServer::SendPlayerSlotInfo()
{
    EESession(this, true, EE_NETMSG_SESS_DO_SLOT_INFO);
}

// ---- the members that stay C++ ----

// New (LLE Network Engine round 6): the server's connection response: its error, the version, the send-to slot it
// names, the current tick, the lobby slot
NEMessage* EEServer::NewConnectionResponse(long inError, unsigned char inSendToSlot, unsigned char inUISlot)
{
    return new NEMRecord(kNetMessageClientConnectionRequestResponse,
                         {inError, inUISlot, inSendToSlot, this->GetCurrentCommunicationsTick()}, {gGameVersion});
}

// New (round 6): the connection response sent to a slot from the server's (each sender's five statements), the slot
// forgotten after it when inUnregister
void EEServer::SendConnectionResponse(unsigned char inSlot, long inError, unsigned char inSendToSlot,
                                      unsigned char inUISlot, bool inUnregister)
{
    NEMessage* theResponse = this->NewConnectionResponse(inError, inSendToSlot, inUISlot);
    theResponse->SetOriginatingSlot(kSlotServer);
    this->SendMessageTo(inSlot, theResponse, true);
    theResponse->Dereference();
    if (inUnregister)
        this->UnregisterSlot(inSlot);
}

// New (round 6): a client's connection request (firewall flag, version, name, binary checksum): its player, its
// firewall flag, then the request checked and answered (ProcessConnectionRequest); the request released
void EEServer::TakeConnectionRequest(NEMRecord* inRequest)
{
    EEGameOptionsPlayer thePlayer;
    thePlayer.mName = inRequest->GetString(2);
    thePlayer.mTeam = 0;
    thePlayer.mType = 1;
    thePlayer.mStartingCitizens = gWorldTable.GetLongValue(kWorldStartingCitizensDefault);
    thePlayer.mPing = 0.0f;
    thePlayer.mIsReady = false;
    this->mBehindFirewall.insert(std::map<unsigned char, bool>::value_type(inRequest->GetOriginatingSlot(),
                                                                           inRequest->Get<bool>(0)));
    this->ProcessConnectionRequest(inRequest->GetOriginatingSlot(), thePlayer, inRequest->GetString(1),
                                   inRequest->Get<uint32_t>(3));
    inRequest->Dereference();
}

// Removes a client from its lobby slot (removed-by-host response), forgets its slot and resends the slot's info
void EEServer::RefuseClient(EEGameOptionsPlayer& inPlayer)
{
    this->SendConnectionResponse(this->GetSendToSlot(inPlayer.mSlot), kConnectionRemovedByHost, kSlotNone, 0xff, true);
    this->mSendToSlots.erase((unsigned char)inPlayer.mSlot);
    this->SendFullPlayerInformation(inPlayer.mSlot, inPlayer);
}

// Returns the first world player index below the maximum that holds no player, else 0xff
unsigned char EEServer::GetFreePlayerSlot()
{
    unsigned char theMax = this->GetMaxPlayers();
    unsigned char i;
    for (i = 0; i < theMax; i++) {
        if (gWorld.mPlayers[i] == NULL)
            break;
    }
    return i < theMax ? i : 0xff;
}

// Returns true when the world has no teams
bool EEServer::ServerSlot28()
{
    return (gWorld.mTeams.size() > 0) ? false : true;
}

// Returns whether a server migration is in progress
bool EEServer::IsMigrating()
{
    return this->mIsMigrating;
}

// Returns a player's name in a static string (left unchanged when the player is not present)
const UWideString& EEServer::ServerSlot54(unsigned char inPlayer)
{
    static UWideString sName("");
    if (this->IsPlayerPresent(inPlayer))
        sName = gWorld.mPlayers[inPlayer]->mName;
    return sName;
}

// Returns a world player's network slot, 0xff when the player is not present
unsigned char EEServer::GetPlayerSendToSlot(unsigned char inPlayer)
{
    return this->IsPlayerPresent(inPlayer) ? gWorld.mPlayers[inPlayer]->mSlot : 0xff;
}

// Returns the network slot of a lobby slot, 0xff when none
unsigned char EEServer::GetSendToSlot(unsigned char inPlayer)
{
    std::map<unsigned char, unsigned char>::iterator theIt = this->mSendToSlots.find(inPlayer);
    return theIt != this->mSendToSlots.end() ? theIt->second : 0xff;
}

// Returns the lobby slot that uses a network slot, 0 when none
unsigned char EEServer::GetLobbySlot(unsigned char inValue)
{
    for (std::map<unsigned char, unsigned char>::iterator theIt = this->mSendToSlots.begin(); theIt != this->mSendToSlots.end(); ++theIt)
        if (theIt->second == inValue)
            return theIt->first;
    return 0;
}

// Returns whether a world player index holds a player
bool EEServer::IsPlayerPresent(unsigned char inPlayer)
{
    return inPlayer < gWorld.mNumPlayers && gWorld.mPlayers[inPlayer] != NULL;
}

// Plays a warning sound: 1 out of sync, 2 resending
void EEServer::PlayWarningSound(long inSound)
{
    unsigned long theSample;
    unsigned long theInstance;
    if (inSound == 1 || inSound == 2)
        EELoadSound(UString(inSound == 1 ? "sounds\\OutOfSynch.wav" : "sounds\\Resending.wav"), 0x31, theSample, theInstance, 1);
}

// Clears the fast-selection state (no selection executing, id 0, empty table)
void EEServer::ResetFastSelection()
{
    this->mIsExecutingSelection = false;
    this->mU348 = 0;
    this->mFastSelectionID = 0;
    for (long i = 0; i < 16; i++)
        this->mFastSelection[i] = 0;
}

// Sends a file transfer's progress for the lobby slot of the given network slot
void EEServer::HandleXferStatus(unsigned char& inSlot, unsigned char& inPercentDone, uint32_t& inXferID,
                            unsigned char& inStatusID)
{
    unsigned char thePlayer = this->GetLobbySlot(inSlot);
    this->ProcessLocalUserInput(new NEMRecord(kNetMessageXferStatus, {thePlayer, inPercentDone, inXferID, inStatusID}),
                                kSlotNone);
}

// Sends a lobby slot's player options to everyone and refreshes the slot's line
void EEServer::SendFullPlayerInformation(unsigned char inPlayer, EEGameOptionsPlayer& inOptions)
{
    EEGameOptionsPlayer thePlayer;
    thePlayer.mType = inOptions.mType;
    thePlayer.mColor = inOptions.mColor;
    thePlayer.mName = inOptions.mName;
    thePlayer.mTeam = inOptions.mTeam;
    thePlayer.mStartingCitizens = inOptions.mStartingCitizens;
    thePlayer.mPing = inOptions.mPing;
    thePlayer.mIsReady = inOptions.mIsReady;
    thePlayer.mSlot = inOptions.mSlot;
    thePlayer.mB36 = inOptions.mB36;
    thePlayer.mEngine = inOptions.mEngine;   // port
    this->ProcessLocalUserInput(EENewFullPlayerInformation(thePlayer), kSlotNone);
    gUserInterface->PostLobbyUpdatePlayer(thePlayer.mSlot, 8);
}

// Sends a human slot's connection quality to everyone
void EEServer::SendPingStat(unsigned char inPlayer, float inPing)
{
    EEGameOptionsPlayer thePlayer;
    gGameOptions.mLock->ReadLock();
    thePlayer = gGameOptions.mPlayers[inPlayer];
    if (thePlayer.mType == 1 || thePlayer.mType == 2)
        this->ProcessLocalUserInput(new EEMRecord(kMessageUpdatePingStat, {inPlayer, inPing}), kSlotNone);
    gGameOptions.mLock->ReadUnlock();
}

// Posts message 0x12 to the UI thread
void EEServer::ServerSlot48()
{
    long theParameter1 = 0;
    long theParameter2 = 0;
    gUserInterface->mThread->Post(0x12, &theParameter1, &theParameter2, true);
}

// Returns the debug out-of-sync handling preference
bool EEServer::GetDebugOOSHandling()
{
    return gGamePreferences.mSettings.mDebugOOSHandling;
}

void EEServer::BeforeRemovingPlayers()
{
}

bool EEServer::IsClient()
{
    return false;
}

bool EEServer::IsServer()
{
    return true;
}

// ---- the host of ee_netmsg session.rs: the server's and the client's world calls, one operation each ----

// the forwarder's frame: the communications object, the bodies' two lobby slot registers and a join request's
// player and version, protocol 0 for the slot info
struct EESessionFrame {
    NESynchronousCommunications* mComms;
    bool mServer;
    EEGameOptionsPlayer mRegisters[2];
    const EEGameOptionsPlayer* mIn;
    const UWideString* mVersion;
    NEProtocol* mProtocol;
};

// the engine's protected members the host calls (a friend of EEServer reaches them through the server)
class EESessionAccess {
public:
    static void Unregister(EEServer* s, unsigned char inSlot) { s->UnregisterSlot(inSlot); }
    static void Broadcast(EEServer* s, NEMessage* m) { s->BroadcastMessage(m); }
    static void SendToAll(EEServer* s, NEMessage* m) { s->SendMessageToAll(m, false); }
};

static EEGameOptionsPlayer& EESessionRegister(EESessionFrame* f, long long inRegister)
{
    return inRegister == EE_NETMSG_SESS_R_IN ? const_cast<EEGameOptionsPlayer&>(*f->mIn) : f->mRegisters[inRegister];
}

static long long EESessionPlayerField(EEGameOptionsPlayer& p, long long inField, bool inSet, long long inValue)
{
    float thePing;
    switch (inField) {
    case EE_NETMSG_SESS_FIELD_TYPE: if (inSet) p.mType = (unsigned long)inValue; return (long long)p.mType;
    case EE_NETMSG_SESS_FIELD_LOADED_TYPE: if (inSet) p.mLoadedType = (long)inValue; return p.mLoadedType;
    case EE_NETMSG_SESS_FIELD_PING:   // the float's bits
        if (inSet) { uint32_t theBits = (uint32_t)inValue; memcpy(&p.mPing, &theBits, 4); }
        thePing = p.mPing;
        { uint32_t theBits; memcpy(&theBits, &thePing, 4); return theBits; }
    case EE_NETMSG_SESS_FIELD_SLOT: if (inSet) p.mSlot = (unsigned long)inValue; return (long long)p.mSlot;
    case EE_NETMSG_SESS_FIELD_COLOR: if (inSet) p.mColor = (unsigned long)inValue; return (long long)p.mColor;
    case EE_NETMSG_SESS_FIELD_TEAM: if (inSet) p.mTeam = (unsigned long)inValue; return (long long)p.mTeam;
    case EE_NETMSG_SESS_FIELD_CITIZENS: if (inSet) p.mStartingCitizens = (unsigned long)inValue; return (long long)p.mStartingCitizens;
    case EE_NETMSG_SESS_FIELD_TRANSFER_ID: if (inSet) p.mTransferID = (unsigned long)inValue; return (long long)p.mTransferID;
    case EE_NETMSG_SESS_FIELD_LOCAL: if (inSet) p.mIsLocal = inValue != 0; return p.mIsLocal;
    case EE_NETMSG_SESS_FIELD_READY: if (inSet) p.mIsReady = inValue != 0; return p.mIsReady;
    case EE_NETMSG_SESS_FIELD_B36: if (inSet) p.mB36 = inValue != 0; return p.mB36;
    default: if (inSet) p.mEngine = (unsigned long)inValue; return (long long)p.mEngine;   // port: the engine
    }
}

// the server's and client's members (bool and byte members as their values)
static long long EESessionMember(EESessionFrame* f, long long inMember, long long inIndex, bool inSet, long long v)
{
    EEServer* s = static_cast<EEServer*>(f->mComms);
    EEClient* c = static_cast<EEClient*>(f->mComms);
#define EE_SESSION_MEMBER(x, T) do { if (inSet) x = (T)v; return (long long)x; } while (0)
    switch (inMember) {
    case EE_NETMSG_SESS_SELF_HAS_LEFT:
        if (f->mServer) EE_SESSION_MEMBER(s->mHasLeftGame, bool); else EE_SESSION_MEMBER(c->mHasLeftGame, bool);
    case EE_NETMSG_SESS_SELF_MIGRATING:
        if (f->mServer) EE_SESSION_MEMBER(s->mIsMigrating, bool); else EE_SESSION_MEMBER(c->mIsMigrating, bool);
    case EE_NETMSG_SESS_SELF_FAST_ID:
        if (f->mServer) EE_SESSION_MEMBER(s->mFastSelectionID, unsigned char); else EE_SESSION_MEMBER(c->mFastSelectionID, unsigned char);
    case EE_NETMSG_SESS_SELF_EXECUTING:
        if (f->mServer) EE_SESSION_MEMBER(s->mIsExecutingSelection, bool); else EE_SESSION_MEMBER(c->mIsExecutingSelection, bool);
    case EE_NETMSG_SESS_SELF_LOAD_STATE: EE_SESSION_MEMBER(s->mLoadState, unsigned char);
    case EE_NETMSG_SESS_SELF_DROPPED_LOADERS: EE_SESSION_MEMBER(s->mNumDroppedLoaders, unsigned char);
    case EE_NETMSG_SESS_SELF_VOTE_EDICT: EE_SESSION_MEMBER(s->mVoteEdict, unsigned short);
    case EE_NETMSG_SESS_SELF_YES_VOTES: EE_SESSION_MEMBER(s->mYesVotes, unsigned short);
    case EE_NETMSG_SESS_SELF_NO_VOTES: EE_SESSION_MEMBER(s->mNoVotes, unsigned short);
    case EE_NETMSG_SESS_SELF_NUM_VOTES: EE_SESSION_MEMBER(s->mNumVotes, unsigned char);
    case EE_NETMSG_SESS_SELF_VOTED:
        if (inSet && inIndex < 0) {
            for (long k = 0; k < 256; k++)
                s->mVoted[k] = false;
            return 0;
        }
        EE_SESSION_MEMBER(s->mVoted[inIndex & 0xff], bool);
    case EE_NETMSG_SESS_SELF_LAG_MASK: EE_SESSION_MEMBER(s->mLagMask, unsigned short);
    case EE_NETMSG_SESS_SELF_UI_SLOT: EE_SESSION_MEMBER(c->mUISlotNumber, unsigned char);
    case EE_NETMSG_SESS_SELF_ALLOW_MIGRATION: EE_SESSION_MEMBER(c->mAllowServerMigration, bool);
    case EE_NETMSG_SESS_SELF_IGNORE_TIMEOUT: EE_SESSION_MEMBER(c->mIgnoreConnectionTimeout, bool);
    case EE_NETMSG_SESS_SELF_SERVER_LOST: EE_SESSION_MEMBER(c->mIsServerLost, bool);
    case EE_NETMSG_SESS_SELF_HAS_LOADED: EE_SESSION_MEMBER(c->mHasLoaded, bool);
    case EE_NETMSG_SESS_SELF_LOST_AFTER_LOADING: EE_SESSION_MEMBER(c->mServerLostAfterLoading, bool);
    default: EE_SESSION_MEMBER(c->mWasBooted, bool);
    }
#undef EE_SESSION_MEMBER
}

static void EESessionDisplay(long inText, EEUIFErrorCallback inCallback, bool inFlag)
{
    UWideString theText;
    gResource->GetString(inText, theText);
    gUIManager->DisplayMessage(theText, true, NULL, inCallback, inFlag);
}

static void EESessionPost(EEAction* inAction)
{
    gUserInterface->PostAction(inAction);
    inAction->Dereference();
}

static void EESessionWon(long long inKind, long long a, long long b)
{
    EEWonNetGameInfo theInfo;
    switch (inKind) {
    case EE_NETMSG_SESS_WON_COUNT:
        gWonNet->GetGameInfo(theInfo);
        theInfo.mPlayers = (unsigned long)a;
        theInfo.mHumans = (unsigned long)b;
        gWonNet->SetGameInfo(theInfo);
        break;
    case EE_NETMSG_SESS_WON_HUMANS_DEC:
        gWonNet->GetGameInfo(theInfo);
        theInfo.mHumans--;
        gWonNet->SetGameInfo(theInfo);
        break;
    case EE_NETMSG_SESS_WON_JOINED: gWonNet->PlayerJoined(); break;
    default:   // back to the game list
        gWonNet->ClearGameList();
        gWonNet->RejoinRoom(false);
        break;
    }
}

static long long EESessionUI(EESessionFrame* f, const long long* v)
{
    EEClient* c = static_cast<EEClient*>(f->mComms);
    UWideString theText;
    UWideString theFormat;
    UWideString theName;
    switch (v[0]) {
    case EE_NETMSG_SESS_UI_LOBBY_PLAYER: gUserInterface->PostLobbyUpdatePlayer((long)v[1], 8); break;
    case EE_NETMSG_SESS_UI_ACTION_REMOVE: EESessionPost(new EEARemovePlayer((unsigned char)v[1], v[2] != 0)); break;
    case EE_NETMSG_SESS_UI_ACTION_BOOT: EESessionPost(new EEABootClientFromCanceledGame((long)v[1])); break;
    case EE_NETMSG_SESS_UI_ACTION_MIGRATION: EESessionPost(new EEAServerMigration); break;
    case EE_NETMSG_SESS_UI_PAUSE_EVENT: {
        UIEvent* theEvent = EECreateScreenEvent(EE_SCREENS_MG_PAUSE);
        gUserInterface->PostEvent(theEvent, NULL);
        theEvent->Dereference();
        break;
    }
    case EE_NETMSG_SESS_UI_DISPLAY: EESessionDisplay((long)v[1], v[2] != 0 ? (EEUIFErrorCallback)CancelGameCallback : NULL, v[3] != 0); break;
    case EE_NETMSG_SESS_UI_CHAT_PLAYER:   // the game's line about player v2 (string v1 formats its name)
        gResource->GetString((long)v[1], theFormat);
        sprintf(theText, theFormat, (const char*)((EEPlayer*)(uintptr_t)v[2])->mName);
        gResource->GetString(IDS_MSG_GAME, theName);
        gUserInterface->AddChatLogEntry(GEColor(1.0f, 1.0f, 1.0f), theName, theText, gWorldTable.GetLongValue(kWorldChatLogDisplayTime) * 2, 0);
        break;
    case EE_NETMSG_SESS_UI_CHAT_TEXT:
        gResource->GetString((long)v[1], theText);
        gUserInterface->AddChatLogEntry(theText, (long)v[2]);
        break;
    case EE_NETMSG_SESS_UI_CLOSE_FORM: gUserInterface->CloseForm(NULL); break;
    case EE_NETMSG_SESS_UI_OPEN_FORM: gUserInterface->OpenForm(gUIManager->GetFormByID(v[1])); break;
    case EE_NETMSG_SESS_UI_PUSH_FORM: gUIManager->Push(gUIManager->GetFormByID(v[1])); break;
    case EE_NETMSG_SESS_UI_CANCEL_GAME: gUserInterface->PostCancelGame(); break;
    case EE_NETMSG_SESS_UI_CANCEL_CALLBACK: {
        unsigned long theUnused = 0;
        CancelGameCallback(&theUnused);
        break;
    }
    case EE_NETMSG_SESS_UI_LOBBY_FORM: gUserInterface->PostLobbyUpdateForm(); break;
    case EE_NETMSG_SESS_UI_HOST_MIGRATED: gUserInterface->PostHostMigrated(); break;
    case EE_NETMSG_SESS_UI_TRANSFER: gUserInterface->PostLobbyShowTransferProgress((long)v[1], (long)v[2]); break;
    case EE_NETMSG_SESS_UI_CONNECT_STATUS: EELobbyPostConnectStatus(v[1] != 0); break;
    case EE_NETMSG_SESS_UI_RETRY:   // the connecting box with the retries
        gResource->GetString((long)v[1], theFormat);
        sprintf(theText, "%s Retry: %ld times", (const char*)theFormat, c->GetNumConnectionTrys());
        gUIManager->DisplayMessage(theText, true, NULL, NULL, false);
        break;
    case EE_NETMSG_SESS_UI_LEAVE:   // back to the game list, the reason shown (with the host's version and ours) or
                                    // the boot posted, then disconnected
        if (gWonNet != NULL)
            EESessionWon(EE_NETMSG_SESS_WON_LEAVE, 0, 0);
        if (v[2] == EE_NETMSG_SESS_LEAVE_BOOT)
            EESessionPost(new EEABootClientFromCanceledGame((long)v[1]));
        else if (v[2] != EE_NETMSG_SESS_LEAVE_QUIET) {
            gUserInterface->CloseForm(NULL);
            gResource->GetString((long)v[1], theFormat);
            if (v[2] == EE_NETMSG_SESS_LEAVE_SHOW_VERSIONS)
                sprintf(theText, theFormat, (const char*)((NEMRecord*)(uintptr_t)v[3])->GetString(4), (const char*)gGameVersion);
            gUIManager->DisplayMessage(v[2] == EE_NETMSG_SESS_LEAVE_SHOW_VERSIONS ? theText : theFormat, true, NULL, NULL, true);
        }
        c->Disconnect();
        break;
    default: c->HandleSlotUpdate((unsigned char)v[1], EESessionRegister(f, v[2])); break;
    }
    return 0;
}

static long long EESessionRecord(const long long* v)
{
    unsigned char theType = (unsigned char)v[1];
    switch (v[2]) {   // the values' count
    case 0: return (long long)(uintptr_t)new EEMRecord(theType);
    case 2: return (long long)(uintptr_t)new EEMRecord(theType, {v[3], v[4]});
    case 3: return (long long)(uintptr_t)new EEMRecord(theType, {v[3], v[4], v[5]});
    default: return (long long)(uintptr_t)new EEMRecord(theType, {v[3], v[4], v[5], v[6]});
    }
}

static long long EESessionSend(EESessionFrame* f, const long long* v)
{
    EEServer* s = static_cast<EEServer*>(f->mComms);
    switch (v[0]) {
    case EE_NETMSG_SESS_SEND_RESPONSE:
        s->SendConnectionResponse((unsigned char)v[1], (long)v[2], (unsigned char)v[3], (unsigned char)v[4], v[5] != 0);
        return 0;
    case EE_NETMSG_SESS_SEND_LOCAL_RESPONSE:
        s->ProcessLocalUserInput(s->NewConnectionResponse((long)v[1], (unsigned char)v[2], (unsigned char)v[3]), (unsigned char)v[4]);
        return 0;
    case EE_NETMSG_SESS_SEND_FULL_OPTIONS:
        gCommunications->ProcessLocalUserInput(EENewFullGameOptions(gGameOptions), (unsigned char)v[1]);
        return 0;
    case EE_NETMSG_SESS_SEND_PLAYER_INFO: {
        EEMRecord* theInformation = EENewFullPlayerInformation(EESessionRegister(f, v[1]));
        if (v[3] != 0)
            theInformation->SetOriginatingSlot(kSlotServer);
        f->mComms->ProcessLocalUserInput(theInformation, (unsigned char)v[2]);
        return 0;
    }
    case EE_NETMSG_SESS_SEND_NEW_RECORD: return EESessionRecord(v);
    case EE_NETMSG_SESS_SEND_PING_STAT: {
        uint32_t theBits = (uint32_t)v[2];
        float theQuality;
        memcpy(&theQuality, &theBits, 4);
        s->SendPingStat((unsigned char)v[1], theQuality);
        return 0;
    }
    case EE_NETMSG_SESS_SEND_SLOT_INFO: {   // player v1 at send-to slot v2: the host's address, none, a client's
        unsigned char theIndex = (unsigned char)v[1];
        unsigned char theSlot = (unsigned char)v[2];
        UWideString theAddress;
        EEMRecord* theMessage;
        if (theSlot == kSlotServer)
            theMessage = new EEMRecord(kMessagePlayerSlotInfo, {theIndex, kSlotServer, s->mBehindFirewall[theSlot], EEGetLocalAddress(f->mProtocol, gGamePreferences.mSettings.GetNetworkAdapter())});
        else if (theSlot == kSlotNone) {
            theAddress = "";
            theMessage = new EEMRecord(kMessagePlayerSlotInfo, {theIndex, kSlotNone, s->mBehindFirewall[theSlot], theAddress});
        } else {
            s->GetClientAddress(theSlot, theAddress);
            theMessage = new EEMRecord(kMessagePlayerSlotInfo, {theIndex, theSlot, s->mBehindFirewall[theSlot], theAddress});
        }
        gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
        return 0;
    }
    case EE_NETMSG_SESS_SEND_PHYSICS_TICK:   // the inputs, the ticks, checksum and dump, PTs per second
        return (long long)(uintptr_t)new NEMRecord(kNetMessagePhysicsTick, {v[1], v[2], v[3], v[4], v[5]});
    default:   // the connection check's answer
        f->mComms->ProcessLocalUserInput(new NEMRecord(kNetMessagePlayerConnectionCheckResponse, {(uint32_t)v[1]}), kSlotNone);
        return 0;
    }
}

static long long EESessionMessage(EESessionFrame* f, const long long* v)
{
    EEServer* s = static_cast<EEServer*>(f->mComms);
    NEMessage* m = (NEMessage*)(uintptr_t)v[1];
    NEMessage* theInput;
    switch (v[0]) {
    case EE_NETMSG_SESS_MSG_TYPE: return m->GetType();
    case EE_NETMSG_SESS_MSG_ORIGIN: return m->GetOriginatingSlot();
    case EE_NETMSG_SESS_MSG_FOR_SLOT: return m->GetForSpecificSlot();
    case EE_NETMSG_SESS_MSG_NE_VALUE: return (long long)static_cast<NEMRecord*>(m)->Value((uint32_t)v[2]);
    case EE_NETMSG_SESS_MSG_RECORD: memcpy((void*)(uintptr_t)v[2], static_cast<EEMRecord*>(m)->mRecord, sizeof(static_cast<EEMRecord*>(m)->mRecord)); return 0;
    case EE_NETMSG_SESS_MSG_INPUT_COUNT: return static_cast<NEMRecord*>(m)->GetNumberOfPlayerInputs();
    case EE_NETMSG_SESS_MSG_INPUT: static_cast<NEMRecord*>(m)->GetPlayerInput((unsigned long)v[2], theInput); return (long long)(uintptr_t)theInput;
    case EE_NETMSG_SESS_MSG_REFERENCE: m->Reference(); return 0;
    case EE_NETMSG_SESS_MSG_DEREFERENCE: m->Dereference(); return 0;
    case EE_NETMSG_SESS_MSG_EXECUTE: m->Execute(); return 0;
    case EE_NETMSG_SESS_MSG_SET_FAST_ID: static_cast<EEMessage*>(m)->mFastSelectionID = (unsigned char)v[2]; return 0;
    case EE_NETMSG_SESS_MSG_IS_GAME: return EEGameMessageCast(m) != NULL;
    case EE_NETMSG_SESS_MSG_QUEUE: gPhysics->QueueServerMessage(m); return 0;
    case EE_NETMSG_SESS_MSG_LOG_RESPONSE: s->LogResponse(m); return 0;
    case EE_NETMSG_SESS_MSG_LOCAL: f->mComms->ProcessLocalUserInput(m, (unsigned char)v[2]); return 0;
    case EE_NETMSG_SESS_MSG_BROADCAST: m->SetOriginatingSlot(kSlotServer); EESessionAccess::Broadcast(s, m); return 0;
    case EE_NETMSG_SESS_MSG_SEND_ALL:
        m->SetOriginatingSlot(kSlotServer);
        EESessionAccess::SendToAll(s, m);
        m->Dereference();
        return 0;
    default: s->TakeConnectionRequest(static_cast<NEMRecord*>(m)); return 0;
    }
}

static long long EESessionComms(EESessionFrame* f, const long long* v)
{
    EEServer* s = static_cast<EEServer*>(f->mComms);
    EEClient* c = static_cast<EEClient*>(f->mComms);
    switch (v[0]) {
    case EE_NETMSG_SESS_COMMS_SET_CT: gCommunications->SetCurrentCommunicationsTick((unsigned long)v[1]); break;
    case EE_NETMSG_SESS_COMMS_SET_CTS_IN_PHYSICS: gCommunications->SetNumberOfCTsProcessedInPhysics((unsigned long)v[1]); break;
    case EE_NETMSG_SESS_COMMS_INCREMENT_CTS: gCommunications->IncrementNumberOfCTsProcessedInPhysics(); break;
    case EE_NETMSG_SESS_COMMS_SET_MP_SESSION: gCommunications->SetIsMultiplayerSession(v[1] != 0); break;
    case EE_NETMSG_SESS_COMMS_SET_HUMANS: f->mComms->SetNumHumanPlayers((unsigned char)v[1]); break;
    case EE_NETMSG_SESS_COMMS_SET_PT: {
        unsigned long theTick = (unsigned long)v[1];
        c->SetCurrentPhysicsTick(theTick);
        break;
    }
    case EE_NETMSG_SESS_COMMS_REGISTER: s->RegisterSlot((unsigned char)v[1]); break;
    case EE_NETMSG_SESS_COMMS_UNREGISTER: EESessionAccess::Unregister(s, (unsigned char)v[1]); break;
    case EE_NETMSG_SESS_COMMS_SEND_TO_CLEAR: s->mSendToSlots.clear(); break;
    case EE_NETMSG_SESS_COMMS_SEND_TO_INSERT:
        s->mSendToSlots.insert(std::map<unsigned char, unsigned char>::value_type((unsigned char)v[1], (unsigned char)v[2]));
        break;
    case EE_NETMSG_SESS_COMMS_SEND_TO_ERASE: s->mSendToSlots.erase((unsigned char)v[1]); break;
    case EE_NETMSG_SESS_COMMS_ABORT_XFER: s->AbortXfer((unsigned long)v[1]); break;
    case EE_NETMSG_SESS_COMMS_REMOVE_IN_PROGRESS: s->RemovePlayerFromGameInProgress((unsigned char)v[1], v[2] != 0, false, v[3] != 0); break;
    case EE_NETMSG_SESS_COMMS_XFER_GAME_FILE: {   // load type v1's file (the map type) to lobby slot v2's client
        FSFileSpec* theFile;
        EEPSaveGameHeader theHeader;
        unsigned long theXferID;
        if (EEOpenSaveGameFile((const char*)gGameOptions.mMapType, v[1] == 2 ? "Saved Games\\" : "Scenarios\\", theFile) == kErrorOK) {
            theHeader.mMajorVersion = kSaveGameMajorVersion;
            theHeader.mMinorVersion = kSaveGameMinorVersion;
            if (theHeader.LoadFromFile(*theFile) == kErrorOK && !EEIsMultiplayerGame(true))
                s->XferFileToClient(*theFile, theHeader.mChecksum, s->GetSendToSlot((unsigned char)v[2]), theXferID, false);
        }
        break;
    }
    case EE_NETMSG_SESS_COMMS_START_GAME: s->StartGame(); break;
    case EE_NETMSG_SESS_COMMS_PAUSE_GAME: s->PauseGame(); break;
    case EE_NETMSG_SESS_COMMS_BECOME_CLIENT:   // the host removed: the server deleted, a client in its place
        static_cast<NESynchronousServer*>(gCommunications)->StopPing();
        static_cast<NESynchronousServer*>(gCommunications)->ResetXfer();
        EESessionAccess::Unregister(s, kSlotServer);
        s->RegisterSlot(kSlotServer);
        if (gCommunications != NULL) {
            delete gCommunications;
            gCommunications = new EEClient;
            static_cast<NESynchronousClient*>(gCommunications)->StartThreads();
        }
        break;
    case EE_NETMSG_SESS_COMMS_LOWEST_CTS: static_cast<NESynchronousServer*>(gCommunications)->SetMessageManagerLowestCTs((unsigned long)v[1]); break;
    case EE_NETMSG_SESS_COMMS_DISCONNECT: c->Disconnect(); break;
    case EE_NETMSG_SESS_COMMS_LOCAL_OUT: gLocalPlayerOut = v[1] != 0; break;
    case EE_NETMSG_SESS_COMMS_SLEEP: PlatSleep((unsigned long)v[1]); break;
    default: NEProtocol::GetProtocol(0, f->mProtocol); break;
    }
    return 0;
}

static long long EESessionFact(EESessionFrame* f, const long long* v)
{
    EEServer* s = static_cast<EEServer*>(f->mComms);
    EEClient* c = static_cast<EEClient*>(f->mComms);
    UIForm* theForm;
    switch (v[0]) {
    case EE_NETMSG_SESS_FACT_MULTIPLAYER_GAME: return EEIsMultiplayerGame(true);
    case EE_NETMSG_SESS_FACT_OPTIONS_MULTIPLAYER: return gGameOptions.mIsMultiplayer;
    case EE_NETMSG_SESS_FACT_LOAD_TYPE: return gGameOptions.mLoadType;
    case EE_NETMSG_SESS_FACT_GAME_STATE: return s->GetGameState();
    case EE_NETMSG_SESS_FACT_DEFAULT_COLOR: return gDefaultPlayerColors[v[1]];
    case EE_NETMSG_SESS_FACT_CITIZENS_DEFAULT: return (long long)gWorldTable.GetLongValue(kWorldStartingCitizensDefault);
    case EE_NETMSG_SESS_FACT_VERSION_MATCH: return !(*f->mVersion != gGameVersion);
    case EE_NETMSG_SESS_FACT_BINARY_CHECKSUM: return gBinaryChecksum;
    case EE_NETMSG_SESS_FACT_CURRENT_CT: return (long long)f->mComms->GetCurrentCommunicationsTick();
    case EE_NETMSG_SESS_FACT_CTS_IN_PHYSICS: return (long long)gCommunications->GetNumberOfCTsProcessedInPhysics();
    case EE_NETMSG_SESS_FACT_HUMANS: return gCommunications->GetNumHumanPlayers();
    case EE_NETMSG_SESS_FACT_MP_SESSION: return f->mComms->GetIsMultiplayerSession();
    case EE_NETMSG_SESS_FACT_PLAYER_NUMBER: return (long long)f->mComms->GetPlayerNumber();
    case EE_NETMSG_SESS_FACT_CURRENT_PT: return (long long)c->GetCurrentPhysicsTick();
    case EE_NETMSG_SESS_FACT_MY_SEND_TO_SLOT: return c->GetMySendToSlot();
    case EE_NETMSG_SESS_FACT_NUM_PLAYERS: return s->GetNumberOfPlayers();
    case EE_NETMSG_SESS_FACT_WORLD_PLAYERS: return (long long)gWorld.mNumPlayers;
    case EE_NETMSG_SESS_FACT_TIME: return (long long)TMGetTime();
    case EE_NETMSG_SESS_FACT_WON: return gWonNet != NULL;
    case EE_NETMSG_SESS_FACT_SCREEN_ACTIVE: return gGameScreenActive;
    case EE_NETMSG_SESS_FACT_ENGINE_CHOICES: return (long long)EECPEngineChoiceCount();
    case EE_NETMSG_SESS_FACT_FORM_IS_CURRENT:   // the form looked up first (v2 0) or the current form first
        if (v[2] == 0) {
            theForm = gUIManager->GetFormByID(v[1]);
            return gUIManager->GetCurrentForm() == theForm;
        }
        theForm = gUIManager->GetCurrentForm();
        return theForm == gUIManager->GetFormByID(v[1]);
    case EE_NETMSG_SESS_FACT_FORM_VISIBLE: return gUIManager->IsFormVisible(gUIManager->GetFormByID(v[1]));
    case EE_NETMSG_SESS_FACT_SEND_TO_SLOT: return s->GetSendToSlot((unsigned char)v[1]);
    default: return s->GetPlayerSendToSlot((unsigned char)v[1]);
    }
}

static long long EESessionWorld(EESessionFrame* f, const long long* v)
{
    EEPlayer* p = (EEPlayer*)(uintptr_t)v[1];
    switch (v[0]) {
    case EE_NETMSG_SESS_WORLD_PLAYER: return (long long)(uintptr_t)gWorld.mPlayers[v[1]];
    case EE_NETMSG_SESS_WORLD_PLAYER_BY_SLOT: return (long long)(uintptr_t)gWorld.GetPlayerBySlot((unsigned char)v[1]);
    case EE_NETMSG_SESS_WORLD_IS_HUMAN: return p->IsHuman();
    case EE_NETMSG_SESS_WORLD_SLOT: return p->mSlot;
    case EE_NETMSG_SESS_WORLD_HAS_HUMAN: return p->mHumanPlayer != NULL;
    case EE_NETMSG_SESS_WORLD_ID: return (long long)p->mPlayerID;
    case EE_NETMSG_SESS_WORLD_SET_SLOT: p->mSlot = (unsigned char)v[2]; return 0;
    case EE_NETMSG_SESS_WORLD_SET_FIREWALL: p->mBehindFirewall = v[2] != 0; return 0;
    case EE_NETMSG_SESS_WORLD_NAME_FROM_MSG: static_cast<EEHumanPlayer*>(p)->SetName(((EEMRecord*)(uintptr_t)v[2])->mString); return 0;
    case EE_NETMSG_SESS_WORLD_RECONNECT: gWorld.GetPlayer((unsigned long)v[1])->mSlot = (unsigned char)v[2]; return 0;
    case EE_NETMSG_SESS_WORLD_REMOVE: gWorld.RemoveDisconnectedPlayer((unsigned long)v[1]); return 0;
    default: gWorld.GetCurrentPlayer()->ExecuteSelectionMessage((NEMessage*)(uintptr_t)v[1]); return 0;
    }
}

static long long EESessionRegisterOp(EESessionFrame* f, const long long* v)
{
    EEGameOptionsPlayer& r = EESessionRegister(f, v[0]);
    switch (v[1]) {
    case EE_NETMSG_SESS_REG_LOAD_OPTIONS: r = gGameOptions.mPlayers[v[2]]; return 0;
    case EE_NETMSG_SESS_REG_LOAD_LOBBY: r = gWorld.GetLobbyOptions().mPlayers[v[2]]; return 0;
    case EE_NETMSG_SESS_REG_LOAD_REG: r = EESessionRegister(f, v[2]); return 0;
    case EE_NETMSG_SESS_REG_STORE_OPTIONS: gGameOptions.mPlayers[v[2]] = r; return 0;
    case EE_NETMSG_SESS_REG_STORE_LOBBY: gWorld.GetLobbyOptions().mPlayers[v[2]] = r; return 0;
    case EE_NETMSG_SESS_REG_GET: return EESessionPlayerField(r, v[2], false, 0);
    case EE_NETMSG_SESS_REG_SET: return EESessionPlayerField(r, v[2], true, v[3]);
    case EE_NETMSG_SESS_REG_NAME_CLEAR: r.mName = ""; return 0;
    case EE_NETMSG_SESS_REG_NAME_COPY: r.mName = EESessionRegister(f, v[2]).mName; return 0;
    case EE_NETMSG_SESS_REG_NAME_ICMP:
        return PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)r.mName),
                           reinterpret_cast<const unsigned char*>((const char*)EESessionRegister(f, v[2]).mName)) == 0;
    case EE_NETMSG_SESS_REG_NAME_EQ: return r.mName == EESessionRegister(f, v[2]).mName;
    case EE_NETMSG_SESS_REG_NAME_FROM_MSG: r.mName = ((EEMRecord*)(uintptr_t)v[2])->mString; return 0;
    default:   // the name against world player v2's
        return PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)r.mName),
                           reinterpret_cast<const unsigned char*>((const char*)((EEPlayer*)(uintptr_t)v[2])->mName)) == 0;
    }
}

static int64_t EESessionHost(uint32_t inOp, EeNetSessCall* inCall)
{
    EESessionFrame* f = (EESessionFrame*)inCall->frame;
    const long long* v = (const long long*)inCall->v;
    EEServer* s = static_cast<EEServer*>(f->mComms);
    TSCriticalSection* theSection;
    switch (inOp) {
    case EE_NETMSG_SESS_OP_LOCK:
        switch (v[0]) {
        case EE_NETMSG_SESS_LOCK_CT: if (v[1]) s->LockSendCommunicationsTick(); else s->UnlockSendCommunicationsTick(); return 0;
        case EE_NETMSG_SESS_LOCK_OPTIONS_WRITE: if (v[1]) gGameOptions.mLock->WriteLock(); else gGameOptions.mLock->WriteUnlock(); return 0;
        case EE_NETMSG_SESS_LOCK_WORLD_WRITE: if (v[1]) gWorld.mLock->WriteLock(); else gWorld.mLock->WriteUnlock(); return 0;
        case EE_NETMSG_SESS_LOCK_PING: if (v[1]) s->StopPing(); else s->StartPing(); return 0;
        case EE_NETMSG_SESS_LOCK_SEARCH: theSection = s->mSearchForSlotLock; break;
        case EE_NETMSG_SESS_LOCK_ATTEMPT: theSection = s->mAttemptToRemovePlayerLock; break;
        case EE_NETMSG_SESS_LOCK_FAST: theSection = f->mServer ? s->mFastSelectionLock : static_cast<EEClient*>(f->mComms)->mFastSelectionLock; break;
        default: if (v[1]) gPhysics->mServerMessageQueueLock->Enter(); else gPhysics->mServerMessageQueueLock->Leave(); return 0;
        }
        if (v[1])
            theSection->Enter();
        else
            theSection->Leave();
        return 0;
    case EE_NETMSG_SESS_OP_FACT: return EESessionFact(f, v);
    case EE_NETMSG_SESS_OP_SELF_GET: return EESessionMember(f, v[0], v[1], false, 0);
    case EE_NETMSG_SESS_OP_SELF_SET: return EESessionMember(f, v[0], v[1], true, v[2]);
    case EE_NETMSG_SESS_OP_REG: return EESessionRegisterOp(f, v);
    case EE_NETMSG_SESS_OP_SEND: return EESessionSend(f, v);
    case EE_NETMSG_SESS_OP_MSG: return EESessionMessage(f, v);
    case EE_NETMSG_SESS_OP_UI: return EESessionUI(f, v);
    case EE_NETMSG_SESS_OP_WON: EESessionWon(v[0], v[1], v[2]); return 0;
    case EE_NETMSG_SESS_OP_SOUND:
        if (v[0] == 0) {   // the server's: loaded and played
            sSample = gSoundManager->Load(UString(v[1] ? "sounds\\OutOfSynch.wav" : "sounds\\Booted.wav"), 0x31, 1);
            if (!UIsNone32(sSample))
                gSoundManager->Play(sSample, 1.0f, 0.0f, 1.0f, false, NULL);
        } else {
            unsigned long theSample;
            unsigned long theInstance;
            EELoadSound(UString(v[0] == 1 ? "sounds\\OutOfSynch.wav" : "sounds\\Booted.wav"), 0x31, theSample, theInstance, 1);
        }
        return 0;
    case EE_NETMSG_SESS_OP_WORLD: return EESessionWorld(f, v);
    case EE_NETMSG_SESS_OP_COMMS: return EESessionComms(f, v);
    default:   // the game start
        if (v[0] == EE_NETMSG_SESS_START_PREP) {   // the offset start time, the physics' draw, the protocol's status
            NEProtocol* theProtocol = NULL;
            gStartTime = ((NEMRecord*)(uintptr_t)v[1])->Get<uint32_t>(0);
            gPhysics->GetRandom(0L, 0x7fffffffL);
            gWorld.GetNumTeams();
            NEProtocol::GetProtocol(0, theProtocol);
            theProtocol->GetStatus(UWideString(""), 0xffff0000);
        } else if (v[0] == EE_NETMSG_SESS_START_RESUME) {
            gGraphics->PostResume();
            while (gGraphics->mIsPaused)
                PlatSleep(1);
        } else {
            gPhysics->mThread->Post(0x404, NULL, NULL, true);
            while (!gPhysics->mTimerRunning)
                PlatSleep(1);
            gUIManager->mMainGameForm->SetUIState(0);
        }
        return 0;
    }
}

// the server's or client's member inWhat (EE_NETMSG_SESS_DO_*) over the host
long long EESession(NESynchronousCommunications* inComms, bool inServer, uint32_t inWhat, long long a, long long b,
                    long long c, long long d, const EEGameOptionsPlayer* inPlayer, const UWideString* inVersion)
{
    EESessionFrame theFrame;
    theFrame.mComms = inComms;
    theFrame.mServer = inServer;
    theFrame.mIn = inPlayer;
    theFrame.mVersion = inVersion;
    theFrame.mProtocol = NULL;
    return ee_netmsg_session(EESessionHost, &theFrame, inServer, inWhat, a, b, c, d);
}
