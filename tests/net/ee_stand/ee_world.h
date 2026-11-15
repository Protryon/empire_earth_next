// The stand-in world of the server's and client's golden (tests/net/ee_session_test.cpp; game messages round 6):
// every game header EEServer.cpp and EEClient.cpp include is a stand-in under tests/net/ee_stand that includes this
// file. The classes have the members those units use, each call a trace line (T) with its arguments, its answers
// from the driver's state (gFake). The engine's own classes are the real ones: the strings, the locks, NEMRecord
// (NEMessage.cpp) and EEMRecord (EEMRecord.cpp), so every message the units send is traced by its exported bytes.
#ifndef EE_STAND_WORLD_H
#define EE_STAND_WORLD_H

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "lle/GEColor.h"
#include "lle/UResourceFile.h"
#include "ee/EEWorldConstants.h"
#include "ee/EEStringIDs.h"
#include "ee/EECPEngine.h"

void T(const char* inFormat, ...);              // one trace line
std::string TMsg(NEMessage* inMessage);         // a message: type, slots, exported bytes
std::string TName(const UWideString& inName);   // a string, quoted

// ---- the driver's state ----
struct EEFakeState {
    bool mMultiplayer;          // EEIsMultiplayerGame(true)
    unsigned long mTime;        // TMGetTime
    unsigned char mGameState;   // the server's GetGameState
    unsigned char mNumPlayers;  // the server's GetNumberOfPlayers
    unsigned long mMaxPlayers;
    unsigned long mCurrentCT;   // GetCurrentCommunicationsTick
    unsigned long mCurrentPT;   // GetCurrentPhysicsTick
    unsigned long mCTsInPhysics;
    unsigned char mNumHumans;
    bool mMultiplayerSession;
    bool mGamePaused;
    unsigned long mPlayerNumber;
    unsigned char mMySendToSlot;
    unsigned long mConnectionTrys;
    long mOpenSaveResult;       // EEOpenSaveGameFile
    long mHeaderResult;         // EEPSaveGameHeader::LoadFromFile
    unsigned long mHeaderChecksum;
    bool mFormVisible;          // IsFormVisible
    unsigned long mCurrentForm; // GetCurrentForm's id
    bool mWon;                  // gWonNet present
    unsigned long mWonPlayers, mWonHumans;
    long mRandom;               // gPhysics->GetRandom
    bool mGameScreenActive;
    unsigned long mSample;      // gSoundManager->Load
    unsigned long mEngineChoices;
};
extern EEFakeState gFake;

// ---- locks ----
class EEFakeLock {
public:
    explicit EEFakeLock(const char* inName) : mName(inName) {}
    void WriteLock() { T("%s wlock", mName); }
    void WriteUnlock() { T("%s wunlock", mName); }
    void ReadLock() { T("%s rlock", mName); }
    void ReadUnlock() { T("%s runlock", mName); }
    void Enter() { T("%s enter", mName); }
    void Leave() { T("%s leave", mName); }
    const char* mName;
};

// ---- the lobby's options ----
class EEGameOptionsPlayer {
public:
    EEGameOptionsPlayer()
        : mType(0), mLoadedType(0), mPing(0.0f), mSlot(0), mColor(0), mTeam(0), mStartingCitizens(0), mTransferID(0),
          mIsLocal(false), mIsReady(false), mB36(true), mEngine(0) {}
    unsigned long mType;
    long mLoadedType;
    UWideString mName;
    float mPing;
    unsigned long mSlot;
    unsigned long mColor;
    unsigned long mTeam;
    unsigned long mStartingCitizens;
    unsigned long mTransferID;
    bool mIsLocal;
    bool mIsReady;
    bool mB36;
    unsigned long mEngine;
};
std::string TPlayer(const EEGameOptionsPlayer& inPlayer);

class EEGameOptions {
public:
    EEGameOptions(const char* inName) : mLock(new EEFakeLock(inName)), mIsMultiplayer(false), mLoadType(0) {}
    const UWideString& GetMapType() const { return this->mMapType; }
    long GetGameSpeed() const { return 3; }
    long GetLoadType() const { return this->mLoadType; }
    long GetMapSize() const { return 1; }
    long GetEpochStart() const { return 2; }
    long GetEpochEnd() const { return 14; }
    long GetVariant() const { return 0; }
    long GetResources() const { return 1; }
    long GetUnitLimit() const { return 200; }
    long GetWondersForVictory() const { return 3; }
    long GetDifficulty() const { return 2; }
    bool GetVictory() const { return true; }
    bool GetLockTeams() const { return false; }
    bool GetLockSpeed() const { return true; }
    bool GetRevealMap() const { return false; }
    bool GetCheatCodes() const { return false; }
    bool GetRecordGames() const { return true; }
    bool GetAllowCustomCivs() const { return false; }
    bool GetB407() const { return false; }
    EEGameOptionsPlayer mPlayers[16];
    EEFakeLock* mLock;
    bool mIsMultiplayer;
    long mLoadType;
    UWideString mMapType;
};
extern EEGameOptions& gGameOptions;
bool EEIsMultiplayerGame(bool inFlag);
extern bool gLocalPlayerOut;

struct EEGameSettings {
    long mMaxPTsPerSecond, mMinPTsPerSecond, mMaxCTSendInterval, mCTSendIntervalStepUp, mCTSendIntervalStepDown;
    unsigned long mPTRateTuningThreshold;
    bool mDebugOOSHandling;
    long GetNetworkAdapter() const { return 2; }
};
struct EEGamePreferences {
    EEGameSettings mSettings;
};
extern EEGamePreferences gGamePreferences;

class EEWorldTable {
public:
    unsigned long GetLongValue(EEWorldConstant inID) const { return 1000 + (unsigned long)inID; }
};
extern EEWorldTable gWorldTable;

// ---- the world ----
class EEPlayer {
public:
    EEPlayer() : mSlot(0xff), mHumanPlayer(NULL), mPlayerID(0), mBehindFirewall(false), mHuman(false) {}
    virtual ~EEPlayer() {}
    bool IsHuman() { return this->mHuman; }
    void ExecuteSelectionMessage(NEMessage* inMessage) { T("player %lu execute selection %s", mPlayerID, TMsg(inMessage).c_str()); }
    UWideString mName;
    unsigned char mSlot;
    void* mHumanPlayer;
    unsigned long mPlayerID;
    bool mBehindFirewall;
    bool mHuman;
};
class EEHumanPlayer : public EEPlayer {
public:
    void SetName(const UWideString& inName) { T("player %lu set name %s", mPlayerID, TName(inName).c_str()); this->mName = inName; }
};
class EEWorld {
public:
    EEWorld() : mNumPlayers(0), mLock(new EEFakeLock("world")), mCurrent(0) { memset(mPlayers, 0, sizeof(mPlayers)); }
    EEGameOptions& GetLobbyOptions();
    void RemoveDisconnectedPlayer(unsigned long inPlayer) { T("world remove disconnected %lu", inPlayer); }
    EEPlayer* GetPlayer(unsigned long inPlayer) { T("world get player %lu", inPlayer); return this->mPlayers[inPlayer]; }
    EEPlayer* GetPlayerBySlot(unsigned char inSlot)
    {
        for (unsigned long i = 0; i < this->mNumPlayers; i++)
            if (this->mPlayers[i] != NULL && this->mPlayers[i]->mSlot == inSlot)
                return this->mPlayers[i];
        return NULL;
    }
    EEPlayer* GetCurrentPlayer() { return this->mPlayers[this->mCurrent]; }
    long GetNumTeams() { T("world num teams"); return 0; }
    EEPlayer* mPlayers[16];
    unsigned long mNumPlayers;
    std::vector<int> mTeams;
    EEFakeLock* mLock;
    unsigned long mCurrent;
};
extern EEWorld gWorld;

// ---- the UI ----
class UIForm {};
class EEUIFMainGame : public UIForm {
public:
    void SetUIState(long inState) { T("main game ui state %ld", inState); }
};
typedef void (*EEUIFErrorCallback)(UIForm* inForm);
class UIEvent : public UReferenceCounter {
public:
    UIEvent() { this->mReferenceCount = 1; }
    virtual ~UIEvent() {}
    unsigned long mEvent;
};
UIEvent* EECreateScreenEvent(unsigned long inEvent);
class EEAction : public UReferenceCounter {
public:
    EEAction() { this->mReferenceCount = 1; }
    virtual ~EEAction() {}
    virtual std::string Describe() = 0;
};
class EEARemovePlayer : public EEAction {
public:
    EEARemovePlayer(unsigned char inSlot, bool inExiting) : mSlot(inSlot), mExiting(inExiting) {}
    std::string Describe();
    unsigned char mSlot;
    bool mExiting;
};
class EEABootClientFromCanceledGame : public EEAction {
public:
    EEABootClientFromCanceledGame(long inText = 0) : mText(inText) {}
    std::string Describe();
    long mText;
};
class EEAServerMigration : public EEAction {
public:
    std::string Describe() { return "server migration"; }
};
class EEThreadFake {
public:
    explicit EEThreadFake(const char* inName) : mName(inName) {}
    void Post(unsigned long inMessage, void* inA, void* inB, bool inWait);
    const char* mName;
};
class EEUserInterface {
public:
    EEUserInterface() : mThread(new EEThreadFake("ui")) {}
    void PostLobbyUpdatePlayer(unsigned long inSlot, unsigned long inFlags) { T("ui lobby update player %lu %lu", inSlot, inFlags); }
    void PostAction(EEAction* inAction) { T("ui action %s", inAction->Describe().c_str()); }
    void AddChatLogEntry(const GEColor& inColor, const UWideString& inName, const UWideString& inText, long inTime, long inKind)
    {
        T("ui chat (%g %g %g) %s %s %ld %ld", inColor.mRed, inColor.mGreen, inColor.mBlue, TName(inName).c_str(),
          TName(inText).c_str(), inTime, inKind);
    }
    void AddChatLogEntry(const UWideString& inText, long inKind) { T("ui chat %s %ld", TName(inText).c_str(), inKind); }
    void PostEvent(UIEvent* inEvent, void* inData) { T("ui event %lu", inEvent->mEvent); }
    void PostLobbyShowTransferProgress(unsigned long inSlot, unsigned long inPercent) { T("ui transfer progress %lu %lu", inSlot, inPercent); }
    void PostLobbyUpdateForm() { T("ui lobby update form"); }
    void PostHostMigrated() { T("ui host migrated"); }
    void CloseForm(UIForm* inForm) { T("ui close form"); }
    void OpenForm(UIForm* inForm);
    void PostCancelGame() { T("ui cancel game"); }
    EEThreadFake* mThread;
};
extern EEUserInterface* gUserInterface;
class EEUIManager {
public:
    void DisplayMessage(const UWideString& inText, bool inA, UIForm* inForm, EEUIFErrorCallback inCallback, bool inB);
    void Push(UIForm* inForm);
    UIForm* GetFormByID(unsigned long inID);
    UIForm* GetCurrentForm();
    bool IsFormVisible(UIForm* inForm);
    EEUIFMainGame* mMainGameForm;
};
extern EEUIManager* gUIManager;
const unsigned long kFormMainGame = 11;
const unsigned long kFormLobby = 26;
const unsigned long kFormAvailableGames = 32;
extern UResourceFile* gResource;

// ---- sounds, graphics, physics ----
class SESoundManager {
public:
    unsigned long Load(const UString& inName, long inA, long inB) { T("sound load %s %ld %ld", (const char*)inName, inA, inB); return gFake.mSample; }
    void Play(unsigned long inSample, float inA, float inB, float inC, bool inLoop, void* inData)
    {
        T("sound play %lu %g %g %g %d", inSample, inA, inB, inC, inLoop);
    }
};
extern SESoundManager* gSoundManager;
long EELoadSound(const UString& inName, long inFlags, unsigned long& outSample, unsigned long& outInstance, long inCount);
class EEGraphics {
public:
    EEGraphics() : mIsPaused(false) {}
    void PostResume() { T("graphics resume"); }
    bool mIsPaused;
};
extern EEGraphics* gGraphics;
class EEPhysics {
public:
    EEPhysics() : mServerMessageQueueLock(new EEFakeLock("physics queue")), mThread(new EEThreadFake("physics")), mTimerRunning(true) {}
    long GetRandom(long inLow, long inHigh) { T("physics random %ld %ld", inLow, inHigh); return gFake.mRandom; }
    void QueueServerMessage(NEMessage* inMessage);
    EEFakeLock* mServerMessageQueueLock;
    EEThreadFake* mThread;
    bool mTimerRunning;
    std::vector<NEMessage*> mQueue;
};
extern EEPhysics* gPhysics;

// ---- files ----
class EEPSaveGameHeader {
public:
    EEPSaveGameHeader() : mMajorVersion(0), mMinorVersion(0), mChecksum(0) {}
    void SetMajorVersion(unsigned long inVersion) { this->mMajorVersion = inVersion; }
    void SetMinorVersion(unsigned long inVersion) { this->mMinorVersion = inVersion; }
    long LoadFromFile(FSFile& inFile);
    unsigned long mMajorVersion, mMinorVersion, mChecksum;
};
const unsigned long kSaveGameMajorVersion = 0;
const unsigned long kSaveGameMinorVersion = 268;
long EEOpenSaveGameFile(const char* inName, const char* inDirectory, FSFileSpec*& outFile);
extern UString gSavedGameExtension;
extern UString gScenarioExtension;

// ---- WON ----
struct EEWonNetGameInfo {
    unsigned long mPlayers;
    unsigned long mHumans;
};
class EEWonNet {
public:
    void GetGameInfo(EEWonNetGameInfo& outInfo) { T("won get info"); outInfo.mPlayers = gFake.mWonPlayers; outInfo.mHumans = gFake.mWonHumans; }
    void SetGameInfo(const EEWonNetGameInfo& inInfo) { T("won set info %lu %lu", inInfo.mPlayers, inInfo.mHumans); }
    void PlayerJoined() { T("won player joined"); }
    void ClearGameList() { T("won clear game list"); }
    void RejoinRoom(bool inFlag) { T("won rejoin room %d", inFlag); }
};
extern EEWonNet* gWonNet;

// ---- the game's globals ----
extern UWideString gGameVersion;
extern uint32_t gBinaryChecksum;
extern unsigned long& gNetworkAdapter;
extern unsigned long& gStartTime;
extern bool& gGameScreenActive;

// ---- the protocol ----
class NEProtocol {
public:
    static long GetProtocol(unsigned long inIndex, NEProtocol*& outProtocol);
    long GetStatus(const UWideString& inName, unsigned long inFlags) { T("protocol status %s %lx", TName(inName).c_str(), inFlags); return 0; }
};

// ---- the engine's communications (the base classes' members the units call) ----
class NESynchronousCommunications {
public:
    NESynchronousCommunications() {}
    virtual ~NESynchronousCommunications() {}
    virtual void ExecuteRemovePlayer(unsigned char inPlayer, bool inWasBooted, bool inPlayerIsOOS, bool inGameIsExiting) = 0;
    virtual unsigned long GetCurrentCommunicationsTick() const { T("comms current ct"); return gFake.mCurrentCT; }
    virtual bool IsClient() = 0;
    virtual bool IsServer() = 0;
    virtual long ProcessLocalUserInput(NEMessage* inMessage, unsigned char inPlayer);
    virtual void SetCurrentCommunicationsTick(unsigned long inTick) { T("comms set ct %lu", inTick); gFake.mCurrentCT = inTick; }
    bool GetIsGamePaused() { return gFake.mGamePaused; }
    bool GetIsMultiplayerSession() { T("comms get multiplayer session"); return gFake.mMultiplayerSession; }
    unsigned char GetNumHumanPlayers() const { T("comms humans"); return gFake.mNumHumans; }
    unsigned long GetNumberOfCTsProcessedInPhysics() { T("comms cts in physics"); return gFake.mCTsInPhysics; }
    unsigned long GetPlayerNumber() { T("comms player number"); return gFake.mPlayerNumber; }
    void IncrementNumberOfCTsProcessedInPhysics() { T("comms increment cts"); gFake.mCTsInPhysics++; }
    void SetIsMultiplayerSession(bool inSession) { T("comms set multiplayer session %d", inSession); gFake.mMultiplayerSession = inSession; }
    void SetNumHumanPlayers(unsigned char inCount) { T("comms set humans %u", inCount); gFake.mNumHumans = inCount; }
    void SetNumberOfCTsProcessedInPhysics(unsigned long inCount) { T("comms set cts in physics %lu", inCount); gFake.mCTsInPhysics = inCount; }
};
extern NESynchronousCommunications* gCommunications;

enum { kServerStateOpen = 0, kServerStateClosed = 1, kServerStateRunning = 3, kServerStatePaused = 4 };

class NESynchronousServer : public NESynchronousCommunications {
public:
    NESynchronousServer(unsigned char inMaxPlayers, unsigned long inDefaultSendInterval, unsigned char inPTsPerCT,
                        unsigned long inMaxPTs, unsigned long inMinPTs, float inTuning, unsigned long inMaxCTSend,
                        unsigned long inStepUp, unsigned long inStepDown, unsigned long inCT, unsigned long inPT,
                        UWideString& inName, bool inAllowNew, bool inAnnounce, unsigned long inAdapter);
    virtual ~NESynchronousServer() {}
    void ClearSendToSlotArray() { T("server clear send-to slots"); }
    long GetClientAddress(unsigned long inIndex, UWideString& outAddress);
    unsigned char GetGameState() const { T("server game state"); return gFake.mGameState; }
    unsigned long GetMaxPlayers() const { T("server max players"); return gFake.mMaxPlayers; }
    unsigned char GetNumberOfPlayers() const { T("server number of players"); return gFake.mNumPlayers; }
    void LockSendCommunicationsTick() { T("server lock ct"); }
    void UnlockSendCommunicationsTick() { T("server unlock ct"); }
    void ResetXfer() { T("server reset xfer"); }
    void StartGame() { T("server start game"); }
    void StartPing() { T("server start ping"); }
    void StopPing() { T("server stop ping"); }
    void AbortXfer(unsigned long inXferID) { T("server abort xfer %lu", inXferID); }
    long LogResponse(NEMessage* inMessage) { T("server log response %s", TMsg(inMessage).c_str()); return 0; }
    void PauseGame() { T("server pause game"); }
    void RegisterSlot(unsigned char inSlot) { T("server register slot %u", inSlot); }
    void UnregisterSlot(unsigned char inSlot) { T("server unregister slot %u", inSlot); }
    void RemovePlayerFromGameInProgress(unsigned char inPlayer, bool inWasBooted, bool inPlayerIsOOS, bool inGameIsExiting)
    {
        T("server remove player in progress %u %d %d %d", inPlayer, inWasBooted, inPlayerIsOOS, inGameIsExiting);
    }
    void SetMessageManagerLowestCTs(unsigned long inCT) { T("server lowest cts %lu", inCT); }
    long XferFileToClient(FSFileSpec& inFile, unsigned long inUserData, unsigned char inSlot, unsigned long& outXferID, bool inFlag)
    {
        T("server xfer file %lu to %u %d", inUserData, inSlot, inFlag);
        outXferID = 77;
        return 0;
    }
    long BroadcastMessage(NEMessage* inMessage) { T("server broadcast %s", TMsg(inMessage).c_str()); return 0; }
    long SendMessageTo(unsigned char inSlot, NEMessage* inMessage, bool inGuaranteed)
    {
        T("server send to %u %d %s", inSlot, inGuaranteed, TMsg(inMessage).c_str());
        return 0;
    }
    long SendMessageToAll(NEMessage* inMessage, bool inGuaranteed) { T("server send to all %d %s", inGuaranteed, TMsg(inMessage).c_str()); return 0; }
    virtual void StartThreads() { T("server start threads"); }
    virtual void StopThreads() { T("server stop threads"); }
    virtual void BootPlayer(unsigned char inSlot, bool inWasBooted, bool inGameIsExiting) = 0;
    virtual bool ServerSlot28() = 0;
    virtual bool IsMigrating() = 0;
    virtual void BeforeRemovingPlayers() = 0;
    virtual void HandleReconnectWaitTimedOut() = 0;
    virtual void HandleXferStatus(unsigned char& inSlot, unsigned char& inPercentDone, uint32_t& inXferID, unsigned char& inStatusID) = 0;
    virtual void PlayWarningSound(long inSound) = 0;
    virtual void ServerSlot48() = 0;
    virtual unsigned char GetFreePlayerSlot() = 0;
    virtual bool GetDebugOOSHandling() = 0;
    virtual const UWideString& ServerSlot54(unsigned char inPlayer) = 0;
    virtual unsigned char GetPlayerSendToSlot(unsigned char inPlayer) = 0;
    virtual bool FilterPlayerInput(NEMessage* inMessage) = 0;
    virtual void PrepareLocalInput(NEMessage* inMessage) = 0;
    virtual bool IsPlayerPresent(unsigned char inPlayer) = 0;
    virtual void ReportLaggers(std::vector<unsigned char>& inSlots, bool inFlag) = 0;
    virtual void ProcessSystemMessage(NEMessage* inMessage) = 0;
};

class NESynchronousClient : public NESynchronousCommunications {
public:
    NESynchronousClient() { T("client base ctor"); }
    virtual ~NESynchronousClient() {}
    unsigned long GetCurrentPhysicsTick() const { T("client current pt"); return gFake.mCurrentPT; }
    unsigned char GetMySendToSlot() const { T("client my send-to slot"); return gFake.mMySendToSlot; }
    unsigned long GetNumConnectionTrys() const { T("client connection trys"); return gFake.mConnectionTrys; }
    void SetCurrentPhysicsTick(unsigned long& inTick) { T("client set pt %lu", inTick); gFake.mCurrentPT = inTick; }
    long Disconnect() { T("client disconnect"); return 0; }
    virtual FSFile* CreateXferFile(const UWideString& inName, unsigned long inSize) = 0;
    virtual bool IsPlaybackRecordingSession() = 0;
    virtual unsigned long GetConnectTimeout() = 0;
    virtual unsigned char GetClientSlot() = 0;
    virtual void StartThreads() { T("client start threads"); }
    virtual void StopThreads() { T("client stop threads"); }
    virtual bool IgnoreConnectionTimeout() = 0;
    virtual void SetConnectionState(long inState) = 0;
    virtual void HandleMessage(NEMessage* inMessage) = 0;
    virtual void HandleUserInput(NEMessage* inMessage) = 0;
    virtual void HandleConnectionLost() = 0;
    virtual unsigned long HandleStartGame(unsigned long inParameter) = 0;
    virtual void HandleStopGame() = 0;
    virtual void HandleConnectionTimeoutWarning(unsigned long inRemaining) = 0;
};

#endif
