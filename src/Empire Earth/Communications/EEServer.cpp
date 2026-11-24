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
#include "lle/SESoundManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEMessage.h"
#include "ee/EEScenarioManager.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEDatabaseTables.h"
#include <mbstring.h>
#include "ee/EECommunications.h"
#include "ee/EEUIManager.h"
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
#include "ee/EEUIFMainGame.h"
#include "ee/EEHumanPlayer.h"
#include "rust/game/gamecpp.h"
#include "ee/EEUnit.h"
#include "ee/EESMMediaStrings.h"
#include "rust/sim/netmsg.h"   // ee_netmsg session.rs (EESession: EEServer.cpp's host)
#include <list>
#include <string>
long gDefaultPlayerColors[16] = {0, 1, 4, 9, 2, 15, 5, 6, 8, 9, 10, 11, 12, 13, 14, 15};
void EELobbyPostConnectStatus(bool inConnected);   // 0x295ce3 (EELobbyInit.cpp; provisional name)
static unsigned long sSample = 0xffffffff;   // 0x4bb8b8 (ExecuteRemovePlayer: the last booted/out-of-synch sound)

// Creates the game server with the lock-step rates from the game preferences, no vote or load running, and its locks
EEServer::EEServer(UWideString& inName, unsigned long inNetworkAdapter, unsigned long inCurrentCommunicationsTick, unsigned long inCurrentPhysicsTick, bool inAllowNewConnections,
                   bool inAnnounce)
    : NESynchronousServer(GameHost<EEServer>, GameMessageManagerHost<EEServer>, 16, 0x15e, 0x3c, gGamePreferences.mSettings.mMaxPTsPerSecond,
                          gGamePreferences.mSettings.mMinPTsPerSecond,
                          100.0f / gGamePreferences.mSettings.mPTRateTuningThreshold, gGamePreferences.mSettings.mMaxCTSendInterval,
                          gGamePreferences.mSettings.mCTSendIntervalStepUp, gGamePreferences.mSettings.mCTSendIntervalStepDown, inCurrentCommunicationsTick, inCurrentPhysicsTick, inName, inAllowNewConnections,
                          inAnnounce, inNetworkAdapter)
{
    ee_gamecpp_server_init(this);   // gamecpp session_objs.rs: the members, the fast selection, the three locks
}

// Destroys the server, stopping its threads and releasing its three locks
EEServer::~EEServer()
{
    ee_gamecpp_server_drop(this);
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

// Removes a client from its lobby slot (removed-by-host response), forgets its slot and resends the slot's info
void EEServer::RefuseClient(EEGameOptionsPlayer& inPlayer)
{
    ee_gamecpp_server_refuse(this, &inPlayer);
}

// Returns the first world player index below the maximum that holds no player, else 0xff
unsigned char EEServer::GetFreePlayerSlot()
{
    return ee_gamecpp_server_free_player_slot(this);
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
    ee_gamecpp_server_warning_sound(inSound);
}

// Sends a file transfer's progress for the lobby slot of the given network slot
void EEServer::HandleXferStatus(unsigned char& inSlot, unsigned char& inPercentDone, uint32_t& inXferID,
                            unsigned char& inStatusID)
{
    ee_gamecpp_server_xfer_status(this, inSlot, inPercentDone, inXferID, inStatusID);
}

// Posts message 0x12 to the UI thread
void EEServer::ServerSlot48()
{
    ee_gamecpp_server_post_12();
}

// Returns the debug out-of-sync handling preference
bool EEServer::GetDebugOOSHandling()
{
    return gGamePreferences.mSettings.mDebugOOSHandling;
}

void EEServer::BeforeRemovingPlayers()
{
}

// ---- the host of ee_netmsg session.rs: ee_game gamecpp server.rs (ee_gamecpp_session_host) over the cases below ----

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
    static void SendTo(EEServer* s, unsigned char inSlot, NEMessage* m) { s->SendMessageTo(inSlot, m, true); }
};

static EEGameOptionsPlayer& EESessionRegister(EESessionFrame* f, long long inRegister)
{
    return inRegister == EE_NETMSG_SESS_R_IN ? const_cast<EEGameOptionsPlayer&>(*f->mIn) : f->mRegisters[inRegister];
}

// the server's or client's member inWhat (EE_NETMSG_SESS_DO_*) over the host
long long EESession(NESynchronousCommunications* inComms, bool inServer, uint32_t inWhat, long long a, long long b,
                    long long c, long long d, const EEGameOptionsPlayer* inPlayer, const UWideString* inVersion)
{
    return ee_gamecpp_session(inComms, inServer, inWhat, a, b, c, d, inPlayer, inVersion);
}

// ------------------------------------------------------------------------- round 9, lane D: this file's bodies' C++


static_assert(kSlotServer == 0xfe && kSlotNone == 0xff, "gamecpp server.rs SLOT_SERVER, SLOT_NONE");

#define P64(x) ((int64_t)(intptr_t)(x))
#define SF ((EESessionFrame*)p)
#define SS static_cast<EEServer*>(SF->mComms)
#define SC static_cast<EEClient*>(SF->mComms)
#define REG(k) EESessionRegister(SF, a[k])
#define MSG ((NEMessage*)a[0])
#define WSTR(k) (*(UWideString*)a[k])

double EECore9OpD7F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EETechTreeNode.cpp
double EECore9OpDF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    return EECore9OpD7F(inOp, p, a, f);
}

// ------------------------------------------------------------------------- round 11, lane C: the server's bodies' C++

int64_t EECore11OpC7(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

static_assert(kConnectionRemovedByHost == 5 && kMessageUpdatePingStat == 0x7a, "gamecpp session_objs.rs");
// the server's plain tail (gamecpp session_objs.rs EeGamecppServerTail): mVoted .. mHasLeftGame
static_assert(offsetof(EEServer, mVoteEdict) - offsetof(EEServer, mVoted) == 256
                  && offsetof(EEServer, mLagMask) - offsetof(EEServer, mVoted) == 262
                  && offsetof(EEServer, mNumVotes) - offsetof(EEServer, mVoted) == 264
                  && offsetof(EEServer, mNumDroppedLoaders) - offsetof(EEServer, mVoted) == 267
                  && offsetof(EEServer, mFastSelection) - offsetof(EEServer, mVoted) == 268
                  && offsetof(EEServer, mU348) - offsetof(EEServer, mVoted) == 284
                  && offsetof(EEServer, mHasLeftGame) - offsetof(EEServer, mVoted) == 288,
              "gamecpp session_objs.rs EeGamecppServerTail");

#define SV ((EEServer*)p)
#define GOP ((EEGameOptionsPlayer*)p)
#define REQ ((NEMRecord*)a[0])

#define FILESPEC ((FSFileSpec*)a[0])
#define CL ((EEClient*)p)
extern UString gCampaignExtension;          // 0x530b78 (ee/EECampaignFormManager.h, which needs the UI form headers)
// the C++ calls and members of the server's bodies in Rust (gamecpp session_objs.rs): p the server unless noted, one
// statement each
int64_t EECore11OpC2(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    // the session's frame (a0 the communications, a1 the server flag, a2 a join request's player, a3 its version)
    case O::C11FrameNew: return P64((new EESessionFrame{(NESynchronousCommunications*)a[0], a[1] != 0, {}, (const EEGameOptionsPlayer*)a[2], (const UWideString*)a[3], NULL}));
    case O::C11FrameDelete: delete SF;
        break;
    case O::C11FrameComms: return P64(SF->mComms);
    // the server's members and locks
    case O::C11SvTail: return P64(&SV->mVoted[0]);
    case O::C11SvCreateSearchLock: ESVERIFY_AT(TSScheduler::CreateCriticalSection(SV->mSearchForSlotLock) == keseOK, 103);
        break;
    case O::C11SvCreateFastLock: ESVERIFY_AT(TSScheduler::CreateCriticalSection(SV->mFastSelectionLock) == keseOK, 106);
        break;
    case O::C11SvCreateAttemptLock: ESVERIFY_AT(TSScheduler::CreateCriticalSection(SV->mAttemptToRemovePlayerLock) == keseOK, 109);
        break;
    case O::C11SvStopThreads: SV->StopThreads();
        break;
    case O::C11SvDestroySearchLock: TSScheduler::DestroyCriticalSection(SV->mSearchForSlotLock);
        break;
    case O::C11SvDestroyFastLock: TSScheduler::DestroyCriticalSection(SV->mFastSelectionLock);
        break;
    case O::C11SvDestroyAttemptLock: TSScheduler::DestroyCriticalSection(SV->mAttemptToRemovePlayerLock);
        break;
    case O::C11SvMaxPlayers: return SV->GetMaxPlayers();
    case O::C11SvSendToSlot: return SV->GetSendToSlot((unsigned char)a[0]);
    case O::C11SvLobbySlot: return SV->GetLobbySlot((unsigned char)a[0]);
    case O::C11SvSendToErase: SV->mSendToSlots.erase((unsigned char)a[0]);
        break;
    case O::C11SvUnregister: EESessionAccess::Unregister(SV, (unsigned char)a[0]);
        break;
    // the sends
    case O::C11SvNewResponse: return P64(SV->NewConnectionResponse((long)a[0], (unsigned char)a[1], (unsigned char)a[2]));
    case O::C11SvSendTo: EESessionAccess::SendTo(SV, (unsigned char)a[0], (NEMessage*)a[1]);
        break;
    case O::C11SvSendPlayerInfo: SV->ProcessLocalUserInput(EENewFullPlayerInformation(*(EEGameOptionsPlayer*)a[0]), kSlotNone);
        break;
    case O::C11SvPingStat: SV->ProcessLocalUserInput(new EEMRecord(kMessageUpdatePingStat, {(unsigned char)a[0], f[0]}), kSlotNone);
        break;
    case O::C11SvXferStatus: SV->ProcessLocalUserInput(new NEMRecord(kNetMessageXferStatus, {(unsigned char)a[0], (unsigned char)a[1], (uint32_t)a[2], (unsigned char)a[3]}), kSlotNone);
        break;
    // a connection request (a0)
    case O::C11RequestFirewall: SV->mBehindFirewall.insert(std::map<unsigned char, bool>::value_type(REQ->GetOriginatingSlot(), REQ->Get<bool>(0)));
        break;
    case O::C11RequestString: *(UWideString*)p = REQ->GetString((uint32_t)a[1]);
        break;
    case O::C11RequestChecksum: return (long long)REQ->Get<uint32_t>(3);
    // a lobby slot's player (p)
    case O::C11PlayerNew: return P64(new EEGameOptionsPlayer);
    case O::C11PlayerDelete: delete GOP;
        break;
    case O::C11PlayerFields: return P64(static_cast<EeAppOptionsPlayerFields*>(GOP));
    case O::C11PlayerNameCopy: GOP->mName = ((EEGameOptionsPlayer*)a[0])->mName;
        break;
    case O::C11PlayerNameFromRequest: GOP->mName = REQ->GetString((uint32_t)a[1]);
        break;
    case O::C11OptionsReadLock: gGameOptions.mLock->ReadLock();
        break;
    case O::C11OptionsReadUnlock: gGameOptions.mLock->ReadUnlock();
        break;
    case O::C11OptionsPlayerFields: return P64(static_cast<EeAppOptionsPlayerFields*>(&gGameOptions.mPlayers[a[0]]));
    // the sounds and the UI
    case O::C11WarningSound: EELoadSound(UString(a[0] == 1 ? "sounds\\OutOfSynch.wav" : "sounds\\Resending.wav"), 0x31, *(unsigned long*)a[1], *(unsigned long*)a[2], 1);
        break;
    case O::C11UiPost12: gUserInterface->GetThread()->Post(0x12, (void*)a[0], (void*)a[1], true);
        break;
    // (EECore9OpD) the C++ calls and members of the session host's body in Rust (gamecpp server.rs; EeGameCpp's lane D section):
    // p the session's frame (EESessionFrame) unless noted, one statement each
    // the locks
    case O::SsLockCt: SS->LockSendCommunicationsTick();
        break;
    case O::SsUnlockCt: SS->UnlockSendCommunicationsTick();
        break;
    case O::SsOptionsWriteLock: gGameOptions.mLock->WriteLock();
        break;
    case O::SsOptionsWriteUnlock: gGameOptions.mLock->WriteUnlock();
        break;
    case O::SsWorldWriteLock: gWorld.mLock->WriteLock();
        break;
    case O::SsWorldWriteUnlock: gWorld.mLock->WriteUnlock();
        break;
    case O::SsStopPing: SS->StopPing();
        break;
    case O::SsStartPing: SS->StartPing();
        break;
    case O::SsSearchLock: return P64(SS->mSearchForSlotLock);
    case O::SsAttemptLock: return P64(SS->mAttemptToRemovePlayerLock);
    case O::SsServerFastLock: return P64(SS->mFastSelectionLock);
    case O::SsClientFastLock: return P64(SC->mFastSelectionLock);
    case O::SsSectionEnter: ((TSCriticalSection*)a[0])->Enter();
        break;
    case O::SsSectionLeave: ((TSCriticalSection*)a[0])->Leave();
        break;
    case O::SsPhysicsQueueEnter: gPhysics->mServerMessageQueueLock->Enter();
        break;
    case O::SsPhysicsQueueLeave: gPhysics->mServerMessageQueueLock->Leave();
        break;
    case O::SsFrameServer: return SF->mServer;
    // the facts
    case O::SsMultiplayerGame: return EEIsMultiplayerGame(true);
    case O::SsOptionsMultiplayer: return gGameOptions.mIsMultiplayer;
    case O::SsLoadType: return gGameOptions.mLoadType;
    case O::SsGameState: return SS->GetGameState();
    case O::SsDefaultColor: return gDefaultPlayerColors[a[0]];
    case O::SsCitizensDefault: return (long long)gWorldTable.GetLongValue(kWorldStartingCitizensDefault);
    case O::SsVersionMatch: return !(*SF->mVersion != gGameVersion);
    case O::SsBinaryChecksum: return gBinaryChecksum;
    case O::SsCurrentCt: return (long long)SF->mComms->GetCurrentCommunicationsTick();
    case O::SsCtsInPhysics: return (long long)gCommunications->GetNumberOfCTsProcessedInPhysics();
    case O::SsHumans: return gCommunications->GetNumHumanPlayers();
    case O::SsMpSession: return SF->mComms->GetIsMultiplayerSession();
    case O::SsPlayerNumber: return (long long)SF->mComms->GetPlayerNumber();
    case O::SsCurrentPt: return (long long)SC->GetCurrentPhysicsTick();
    case O::SsMySendToSlot: return SC->GetMySendToSlot();
    case O::SsNumPlayers: return SS->GetNumberOfPlayers();
    case O::SsWorldPlayers: return (long long)gWorld.mNumPlayers;
    case O::SsTime: return (long long)TMGetTime();
    case O::SsWon: return gWonNet != NULL;
    case O::SsScreenActive: return gGameScreenActive;
    case O::SsEngineChoices: return (long long)EECPEngineChoiceCount();
    case O::SsFormById: return P64(gUIManager->GetFormByID(a[0]));
    case O::SsCurrentForm: return P64(gUIManager->GetCurrentForm());
    case O::SsFormVisible: return gUIManager->IsFormVisible(gUIManager->GetFormByID(a[0]));
    case O::SsSendToSlot: return SS->GetSendToSlot((unsigned char)a[0]);
    case O::SsPlayerSendToSlot: return SS->GetPlayerSendToSlot((unsigned char)a[0]);
    // the server's and client's members (their addresses)
    case O::SsSHasLeft: return P64(&SS->mHasLeftGame);
    case O::SsCHasLeft: return P64(&SC->mHasLeftGame);
    case O::SsSMigrating: return P64(&SS->mIsMigrating);
    case O::SsCMigrating: return P64(&SC->mIsMigrating);
    case O::SsSFastId: return P64(&SS->mFastSelectionID);
    case O::SsCFastId: return P64(&SC->mFastSelectionID);
    case O::SsSExecuting: return P64(&SS->mIsExecutingSelection);
    case O::SsCExecuting: return P64(&SC->mIsExecutingSelection);
    case O::SsLoadState: return P64(&SS->mLoadState);
    case O::SsDroppedLoaders: return P64(&SS->mNumDroppedLoaders);
    case O::SsVoteEdict: return P64(&SS->mVoteEdict);
    case O::SsYesVotes: return P64(&SS->mYesVotes);
    case O::SsNoVotes: return P64(&SS->mNoVotes);
    case O::SsNumVotes: return P64(&SS->mNumVotes);
    case O::SsVoted: return P64(&SS->mVoted[0]);
    case O::SsLagMask: return P64(&SS->mLagMask);
    case O::SsUiSlot: return P64(&SC->mUISlotNumber);
    case O::SsAllowMigration: return P64(&SC->mAllowServerMigration);
    case O::SsIgnoreTimeout: return P64(&SC->mIgnoreConnectionTimeout);
    case O::SsServerLost: return P64(&SC->mIsServerLost);
    case O::SsHasLoaded: return P64(&SC->mHasLoaded);
    case O::SsLostAfterLoading: return P64(&SC->mServerLostAfterLoading);
    case O::SsWasBooted: return P64(&SC->mWasBooted);
    // the lobby slot registers (a0 the register)
    case O::SsReg: return P64(static_cast<EeAppOptionsPlayerFields*>(&REG(0)));
    case O::SsRegLoadOptions: REG(0) = gGameOptions.mPlayers[a[1]];
        break;
    case O::SsRegLoadLobby: REG(0) = gWorld.GetLobbyOptions().mPlayers[a[1]];
        break;
    case O::SsRegLoadReg: REG(0) = REG(1);
        break;
    case O::SsRegStoreOptions: gGameOptions.mPlayers[a[1]] = REG(0);
        break;
    case O::SsRegStoreLobby: gWorld.GetLobbyOptions().mPlayers[a[1]] = REG(0);
        break;
    case O::SsRegNameClear: REG(0).mName = "";
        break;
    case O::SsRegNameCopy: REG(0).mName = REG(1).mName;
        break;
    case O::SsRegNameIcmp: return PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)REG(0).mName),
                                              reinterpret_cast<const unsigned char*>((const char*)REG(1).mName)) == 0;
    case O::SsRegNameEq: return REG(0).mName == REG(1).mName;
    case O::SsRegNameFromMsg: REG(0).mName = ((EEMRecord*)a[1])->mString;
        break;
    case O::SsRegNameIcmpPlayer: return PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)REG(0).mName),
                                                    reinterpret_cast<const unsigned char*>((const char*)((EEPlayer*)a[1])->mName)) == 0;
    // the sends
    case O::SsSendLocalResponse: SS->ProcessLocalUserInput(SS->NewConnectionResponse((long)a[0], (unsigned char)a[1], (unsigned char)a[2]), (unsigned char)a[3]);
        break;
    case O::SsSendFullOptions: gCommunications->ProcessLocalUserInput(EENewFullGameOptions(gGameOptions), (unsigned char)a[0]);
        break;
    case O::SsNewPlayerInfo: return P64(static_cast<NEMessage*>(EENewFullPlayerInformation(REG(0))));
    case O::SsMsgSetOriginServer: MSG->SetOriginatingSlot(kSlotServer);
        break;
    case O::SsLocalInput: SF->mComms->ProcessLocalUserInput(MSG, (unsigned char)a[1]);
        break;
    case O::SsCommsLocalInput: gCommunications->ProcessLocalUserInput(MSG, (unsigned char)a[1]);
        break;
    case O::SsNewRecord0: return P64(static_cast<NEMessage*>(new EEMRecord((unsigned char)a[0])));
    case O::SsNewRecord2: return P64(static_cast<NEMessage*>(new EEMRecord((unsigned char)a[0], {(long long)a[1], (long long)a[2]})));
    case O::SsNewRecord3: return P64(static_cast<NEMessage*>(new EEMRecord((unsigned char)a[0], {(long long)a[1], (long long)a[2], (long long)a[3]})));
    case O::SsNewRecord4: return P64(static_cast<NEMessage*>(new EEMRecord((unsigned char)a[0], {(long long)a[1], (long long)a[2], (long long)a[3], (long long)a[4]})));
    case O::SsNewString: return P64(new UWideString);
    case O::SsDeleteString: delete (UWideString*)p;
        break;
    case O::SsClientAddress: SS->GetClientAddress((unsigned char)a[0], WSTR(1));
        break;
    case O::SsSlotInfoServer: return P64(static_cast<NEMessage*>(new EEMRecord(kMessagePlayerSlotInfo, {(unsigned char)a[0], kSlotServer, SS->mBehindFirewall[kSlotServer], EEGetLocalAddress(SF->mProtocol, gGamePreferences.mSettings.GetNetworkAdapter())})));
    case O::SsSlotInfoRecord: return P64(static_cast<NEMessage*>(new EEMRecord(kMessagePlayerSlotInfo, {(unsigned char)a[0], (unsigned char)a[1], SS->mBehindFirewall[(unsigned char)a[1]], WSTR(2)})));
    case O::SsNewPhysicsTick: return P64(static_cast<NEMessage*>(new NEMRecord(kNetMessagePhysicsTick, {(long long)a[0], (long long)a[1], (long long)a[2], (long long)a[3], (long long)a[4]})));
    case O::SsConnectionCheckResponse: SF->mComms->ProcessLocalUserInput(new NEMRecord(kNetMessagePlayerConnectionCheckResponse, {(uint32_t)a[0]}), kSlotNone);
        break;
    // the messages (a0)
    case O::SsMsgType: return MSG->GetType();
    case O::SsMsgOrigin: return MSG->GetOriginatingSlot();
    case O::SsMsgForSlot: return MSG->GetForSpecificSlot();
    case O::SsMsgNeValue: return (long long)static_cast<NEMRecord*>(MSG)->Value((uint32_t)a[1]);
    case O::SsMsgRecord: memcpy((void*)a[1], static_cast<EEMRecord*>(MSG)->mRecord, sizeof(static_cast<EEMRecord*>(MSG)->mRecord));
        break;
    case O::SsMsgInputCount: return static_cast<NEMRecord*>(MSG)->GetNumberOfPlayerInputs();
    case O::SsMsgInput: static_cast<NEMRecord*>(MSG)->GetPlayerInput((unsigned long)a[1], *(NEMessage**)a[2]);
        break;
    case O::SsMsgReference: MSG->Reference();
        break;
    case O::SsMsgDereference: MSG->Dereference();
        break;
    case O::SsMsgExecute: MSG->Execute();
        break;
    case O::SsMsgSetFastId: static_cast<EEMessage*>(MSG)->mFastSelectionID = (unsigned char)a[1];
        break;
    case O::SsMsgIsGame: return EEGameMessageCast(MSG) != NULL;
    case O::SsMsgQueue: gPhysics->QueueServerMessage(MSG);
        break;
    case O::SsMsgLogResponse: SS->LogResponse(MSG);
        break;
    case O::SsBroadcast: EESessionAccess::Broadcast(SS, MSG);
        break;
    case O::SsSendToAll: EESessionAccess::SendToAll(SS, MSG);
        break;
    // the user interface
    case O::SsLobbyUpdatePlayer: gUserInterface->PostLobbyUpdatePlayer((long)a[0], 8);
        break;
    case O::SsNewRemovePlayer: return P64(static_cast<EEAction*>(new EEARemovePlayer((unsigned char)a[0], a[1] != 0)));
    case O::SsNewBootClient: return P64(static_cast<EEAction*>(new EEABootClientFromCanceledGame((long)a[0])));
    case O::SsNewServerMigration: return P64(static_cast<EEAction*>(new EEAServerMigration));
    case O::SsPostAction: gUserInterface->PostAction((EEAction*)a[0]);
        break;
    case O::SsActionDereference: ((EEAction*)a[0])->Dereference();
        break;
    case O::SsNewPauseEvent: return P64(EECreateScreenEvent(EE_SCREENS_MG_PAUSE));
    case O::SsPostEvent: gUserInterface->PostEvent((UIEvent*)a[0], NULL);
        break;
    case O::SsEventDereference: ((UIEvent*)a[0])->Dereference();
        break;
    case O::SsGetString: gResource->GetString((long)a[0], WSTR(1));
        break;
    case O::SsDisplayMessage: gUIManager->DisplayMessage(WSTR(0), true, NULL, a[1] != 0 ? (EEUIFErrorCallback)CancelGameCallback : NULL, a[2] != 0);
        break;
    case O::SsSprintfName: sprintf(WSTR(0), WSTR(1), (const char*)((EEPlayer*)a[2])->mName);
        break;
    case O::SsIdsMsgGame: return IDS_MSG_GAME;
    case O::SsChatGameLine: gUserInterface->AddChatLogEntry(GEColor(1.0f, 1.0f, 1.0f), WSTR(0), WSTR(1), gWorldTable.GetLongValue(kWorldChatLogDisplayTime) * 2, 0);
        break;
    case O::SsChatText: gUserInterface->AddChatLogEntry(WSTR(0), (long)a[1]);
        break;
    case O::SsCloseForm: gUserInterface->CloseForm(NULL);
        break;
    case O::SsOpenForm: gUserInterface->OpenForm(gUIManager->GetFormByID(a[0]));
        break;
    case O::SsPushForm: gUIManager->Push(gUIManager->GetFormByID(a[0]));
        break;
    case O::SsCancelGame: gUserInterface->PostCancelGame();
        break;
    case O::SsCancelCallback: CancelGameCallback((void*)a[0]);
        break;
    case O::SsLobbyForm: gUserInterface->PostLobbyUpdateForm();
        break;
    case O::SsHostMigrated: gUserInterface->PostHostMigrated();
        break;
    case O::SsTransfer: gUserInterface->PostLobbyShowTransferProgress((long)a[0], (long)a[1]);
        break;
    case O::SsConnectStatus: EELobbyPostConnectStatus(a[0] != 0);
        break;
    case O::SsSprintfRetry: sprintf(WSTR(0), "%s Retry: %ld times", (const char*)WSTR(1), SC->GetNumConnectionTrys());
        break;
    case O::SsSprintfVersions: sprintf(WSTR(0), WSTR(1), (const char*)((NEMRecord*)a[2])->GetString(4), (const char*)gGameVersion);
        break;
    // (EECore9OpD2) the session host's second half (the lobby service, the sounds, the world, the communications, the game start)
    // the lobby service
    case O::SsNewWonInfo: return P64(new EEWonNetGameInfo);
    case O::SsDeleteWonInfo: delete (EEWonNetGameInfo*)p;
        break;
    case O::SsWonGetInfo: gWonNet->GetGameInfo(*(EEWonNetGameInfo*)a[0]);
        break;
    case O::SsWonSetInfo: gWonNet->SetGameInfo(*(EEWonNetGameInfo*)a[0]);
        break;
    case O::SsWonInfoHumans: return P64(&((EEWonNetGameInfo*)a[0])->mHumans);
    case O::SsWonInfoPlayers: return P64(&((EEWonNetGameInfo*)a[0])->mPlayers);
    case O::SsWonJoined: gWonNet->PlayerJoined();
        break;
    case O::SsWonClearGameList: gWonNet->ClearGameList();
        break;
    case O::SsWonRejoinRoom: gWonNet->RejoinRoom(false);
        break;
    // the sounds (sSample: the server's last)
    case O::SsSoundLoadServer: return sSample = gSoundManager->Load(UString(a[0] != 0 ? "sounds\\OutOfSynch.wav" : "sounds\\Booted.wav"), 0x31, 1);
    case O::SsSoundPlayServer: gSoundManager->Play(sSample, 1.0f, 0.0f, 1.0f, false, NULL);
        break;
    case O::SsLoadSound: EELoadSound(UString(a[0] != 0 ? "sounds\\OutOfSynch.wav" : "sounds\\Booted.wav"), 0x31, *(unsigned long*)a[1], *(unsigned long*)a[2], 1);
        break;
    // the world (p: an EEPlayer* for the Pl ops)
    case O::SsWorldPlayer: return P64(gWorld.mPlayers[a[0]]);
    case O::SsWorldPlayerBySlot: return P64(gWorld.GetPlayerBySlot((unsigned char)a[0]));
    case O::SsPlIsHuman: return ((EEPlayer*)p)->IsHuman();
    case O::SsPlSlot: return ((EEPlayer*)p)->mSlot;
    case O::SsPlHasHuman: return ((EEPlayer*)p)->mHumanPlayer != NULL;
    case O::SsPlId: return (long long)((EEPlayer*)p)->mPlayerID;
    case O::SsPlSetSlot: ((EEPlayer*)p)->mSlot = (unsigned char)a[0];
        break;
    case O::SsPlSetFirewall: ((EEPlayer*)p)->mBehindFirewall = a[0] != 0;
        break;
    case O::SsPlNameFromMsg: static_cast<EEHumanPlayer*>((EEPlayer*)p)->SetName(((EEMRecord*)a[0])->mString);
        break;
    case O::SsWorldReconnect: gWorld.GetPlayer((unsigned long)a[0])->mSlot = (unsigned char)a[1];
        break;
    case O::SsWorldRemove: gWorld.RemoveDisconnectedPlayer((unsigned long)a[0]);
        break;
    case O::SsExecuteSelection: gWorld.GetCurrentPlayer()->ExecuteSelectionMessage((NEMessage*)a[0]);
        break;
    // the communications
    case O::SsSetCt: gCommunications->SetCurrentCommunicationsTick((unsigned long)a[0]);
        break;
    case O::SsSetCtsInPhysics: gCommunications->SetNumberOfCTsProcessedInPhysics((unsigned long)a[0]);
        break;
    case O::SsIncrementCts: gCommunications->IncrementNumberOfCTsProcessedInPhysics();
        break;
    case O::SsSetMpSession: gCommunications->SetIsMultiplayerSession(a[0] != 0);
        break;
    case O::SsSetHumans: SF->mComms->SetNumHumanPlayers((unsigned char)a[0]);
        break;
    case O::SsSetPt: SC->SetCurrentPhysicsTick(*(unsigned long*)a[0]);
        break;
    case O::SsRegisterSlot: SS->RegisterSlot((unsigned char)a[0]);
        break;
    case O::SsUnregisterSlot: EESessionAccess::Unregister(SS, (unsigned char)a[0]);
        break;
    case O::SsSendToClear: SS->mSendToSlots.clear();
        break;
    case O::SsSendToInsert: SS->mSendToSlots.insert(std::map<unsigned char, unsigned char>::value_type((unsigned char)a[0], (unsigned char)a[1]));
        break;
    case O::SsSendToErase: SS->mSendToSlots.erase((unsigned char)a[0]);
        break;
    case O::SsAbortXfer: SS->AbortXfer((unsigned long)a[0]);
        break;
    case O::SsRemoveInProgress: SS->RemovePlayerFromGameInProgress((unsigned char)a[0], a[1] != 0, false, a[2] != 0);
        break;
    case O::SsOpenSaveGame: return EEOpenSaveGameFile((const char*)gGameOptions.mMapType, a[0] != 0 ? "Saved Games\\" : "Scenarios\\", *(FSFileSpec**)a[1]) == kErrorOK;
    case O::SsNewHeader: return P64(new EEPSaveGameHeader);
    case O::SsDeleteHeader: delete (EEPSaveGameHeader*)p;
        break;
    case O::SsHeaderSetVersions: ((EEPSaveGameHeader*)a[0])->mMajorVersion = kSaveGameMajorVersion, ((EEPSaveGameHeader*)a[0])->mMinorVersion = kSaveGameMinorVersion;
        break;
    case O::SsHeaderLoad: return ((EEPSaveGameHeader*)a[0])->LoadFromFile(*(FSFileSpec*)a[1]) == kErrorOK;
    case O::SsXferToClient: SS->XferFileToClient(*(FSFileSpec*)a[0], ((EEPSaveGameHeader*)a[1])->mChecksum, SS->GetSendToSlot((unsigned char)a[2]), *(unsigned long*)a[3], false);
        break;
    case O::SsStartGame: SS->StartGame();
        break;
    case O::SsPauseGame: SS->PauseGame();
        break;
    case O::SsCommsStopPing: static_cast<NESynchronousServer*>(gCommunications)->StopPing();
        break;
    case O::SsCommsResetXfer: static_cast<NESynchronousServer*>(gCommunications)->ResetXfer();
        break;
    case O::SsCommsPresent: return gCommunications != NULL;
    case O::SsCommsDelete: delete gCommunications;
        break;
    case O::SsCommsNewClient: gCommunications = new EEClient;
        break;
    case O::SsClientStartThreads: static_cast<NESynchronousClient*>(gCommunications)->StartThreads();
        break;
    case O::SsLowestCts: static_cast<NESynchronousServer*>(gCommunications)->SetMessageManagerLowestCTs((unsigned long)a[0]);
        break;
    case O::SsDisconnect: SC->Disconnect();
        break;
    case O::SsLocalOut: gLocalPlayerOut = a[0] != 0;
        break;
    case O::SsSleep: PlatSleep((unsigned long)a[0]);
        break;
    case O::SsFrameProtocol: NEProtocol::GetProtocol(0, SF->mProtocol);
        break;
    // the game start
    case O::SsStartTimeFromMsg: gStartTime = ((NEMRecord*)a[0])->Get<uint32_t>(0);
        break;
    case O::SsPhysicsRandom: gPhysics->GetRandom(0L, 0x7fffffffL);
        break;
    case O::SsNumTeams: gWorld.GetNumTeams();
        break;
    case O::SsGetProtocol: NEProtocol::GetProtocol(0, *(NEProtocol**)a[0]);
        break;
    case O::SsProtocolStatus: ((NEProtocol*)a[0])->GetStatus(UWideString(""), 0xffff0000);
        break;
    case O::SsPostResume: gGraphics->PostResume();
        break;
    case O::SsGfxPaused: return gGraphics->mIsPaused;
    case O::SsPhysicsPostStart: gPhysics->mThread->Post(0x404, NULL, NULL, true);
        break;
    case O::SsTimerRunning: return gPhysics->mTimerRunning;
    case O::SsMainGameUiState0: gUIManager->GetMainGameForm()->SetUIState(0);
        break;
    // (EECore11OpC3) the C++ calls and members of the client's bodies in Rust (gamecpp session_objs.rs; EEServer.cpp's EECore11OpC2
    // default): p the client, one statement each
    case O::C11ClTail: return P64(&CL->mHasLeftGame);
    case O::C11ClCreateLock: ESVERIFY_AT(TSScheduler::CreateCriticalSection(CL->mFastSelectionLock) == keseOK, 67);
        break;
    case O::C11ClStopThreads: CL->StopThreads();
        break;
    case O::C11ClDestroyLock: TSScheduler::DestroyCriticalSection(CL->mFastSelectionLock);
        break;
    // (EECore9OpD3) the C++ calls of the client's bodies in Rust (gamecpp server.rs; EEServer.cpp's EECore9OpD2 default): one statement
    // each
    case O::ClNewBootDefault: return P64(static_cast<EEAction*>(new EEABootClientFromCanceledGame));
    case O::ClNameLength: return WSTR(0).GetLength();
    case O::ClExtension: WSTR(1) = WSTR(0).GetIndexedSubstring(WSTR(0).GetLength() - 4, WSTR(0).GetLength());
        break;
    case O::ClExtIsSaved: return PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)WSTR(0)), reinterpret_cast<const unsigned char*>((const char*)gSavedGameExtension)) == 0;
    case O::ClExtIsScenario: return PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)WSTR(0)), reinterpret_cast<const unsigned char*>((const char*)gScenarioExtension)) == 0;
    case O::ClExtIsCampaign: return PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)WSTR(0)), reinterpret_cast<const unsigned char*>((const char*)gCampaignExtension)) == 0;
    case O::ClNewFileSpec: return P64(new FSFileSpec);
    case O::ClFileSetVolume: FILESPEC->SetVolume(gStartupVolume);
        break;
    case O::ClFileSetDirectory: FILESPEC->SetDirectory(gStartupDirectory + "data\\" + (a[1] == 0 ? "Saved Games\\" : a[1] == 1 ? "Scenarios\\" : "Campaigns\\"));
        break;
    case O::ClFileSetFile: FILESPEC->SetFile((const char*)WSTR(1));
        break;
    case O::ClFileFree: delete FILESPEC;
        break;
    case O::ClFileOpenRead: return FILESPEC->Open(kFileAccessRead) == kErrorOK;
    case O::ClFileClose: FILESPEC->Close();
        break;
    case O::ClFileDeleteOnDisk: FILESPEC->Delete();
        break;
    case O::ClFileCreateWrite: return FILESPEC->Create() == kErrorOK && FILESPEC->Open(kFileAccessWrite) == kErrorOK;
    case O::ClHeaderLoad: ((EEPSaveGameHeader*)a[1])->LoadFromFile(*FILESPEC);
        break;
    case O::ClHeaderChecksum: return (long long)((EEPSaveGameHeader*)a[0])->mChecksum;
    case O::ClIdsNotResponding: return IDS_HOST_IS_NOT_RESPONDING_PLEASE_WAIT;
    case O::ClIdsSeconds: return IDS_SECONDS;
    case O::ClTimeoutFormat: WSTR(0) = WSTR(1) + UWideString("  (%d ") + WSTR(2) + UWideString(")");
        break;
    case O::ClSprintfSeconds: sprintf(WSTR(0), WSTR(1), (unsigned long)a[2]);
        break;
    default: return EECore11OpC7(inOp, p, a, f);
    }
    return 0;
}
