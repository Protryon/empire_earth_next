// The game's server and client (EEServer.cpp, EEClient.cpp) over a stand-in world (game messages round 6): random
// calls of every member the engine and the game make, each over a random world, lobby and connection state (no gate
// sends a refusal or migrates a host: this is where the refusal branches, the boots, the votes and the migration are
// checked). Every world call is a trace line (tests/net/ee_stand/ee_world.h), every message sent its exported bytes,
// and after each call the members and the lobby slots that changed. The trace is compared with
// tests/net/golden_session.txt, which the original units made (ee_session_orig: tests/net/orig_ee).
//
// `--print FILE` writes the trace; `--steps N` the number of calls (the golden's: the default).
#include <windows.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>

#include "ee_world.h"
#include "ee/EEMessage.h"
#include "ee/EEServer.h"
#include "ee/EEClient.h"
#include "rust/sim/netmsg.h"

// ---- the trace ----
static std::string sTrace;
static unsigned long sLines;
void T(const char* inFormat, ...)
{
    char theLine[4096];
    va_list theArgs;
    va_start(theArgs, inFormat);
    vsnprintf(theLine, sizeof(theLine), inFormat, theArgs);
    va_end(theArgs);
    sTrace += theLine;
    sTrace += '\n';
    sLines++;
}

std::string TName(const UWideString& inName)
{
    return std::string("\"") + (const char*)inName + "\"";
}

static std::string Hex(const unsigned char* inData, size_t inSize)
{
    static const char kDigits[] = "0123456789abcdef";
    std::string theText;
    for (size_t i = 0; i < inSize; i++) {
        theText += kDigits[inData[i] >> 4];
        theText += kDigits[inData[i] & 15];
    }
    return theText;
}

std::string TMsg(NEMessage* inMessage)
{
    if (inMessage == NULL)
        return "null";
    unsigned char* theData = NULL;
    unsigned long theSize = 0;
    inMessage->ExportMessage(&theData, theSize);
    char theHead[64];
    snprintf(theHead, sizeof(theHead), "[%02x o%02x f%02x ", inMessage->GetType(), inMessage->GetOriginatingSlot(),
             inMessage->GetForSpecificSlot());
    // a game message's selection id (byte 4) is not set by its constructor: it is shown only where a call set it
    std::string theBytes = Hex(theData, theSize);
    if (EEGameMessageCast(inMessage) != NULL && theBytes.size() >= 10)
        theBytes.replace(8, 2, "..");
    delete[] theData;
    return theHead + theBytes + "]";
}

std::string TPlayer(const EEGameOptionsPlayer& p)
{
    char theText[256];
    snprintf(theText, sizeof(theText), "type %lu loaded %ld %s ping %g slot %lu colour %lu team %lu cit %lu xfer %lu local %d ready %d b36 %d engine %lu",
             p.mType, p.mLoadedType, TName(p.mName).c_str(), p.mPing, p.mSlot, p.mColor, p.mTeam, p.mStartingCitizens,
             p.mTransferID, p.mIsLocal, p.mIsReady, p.mB36, p.mEngine);
    return theText;
}

// ---- the engine functions the units call ----
void ESVerify(const char* inFile, unsigned long inLine, const char* inExpression)
{
    T("ESVERIFY %lu %s", inLine, inExpression);
}
unsigned long ESRecordNativeError(unsigned long inError)
{
    return inError;
}
void ESChecksum::AddChecksum(const UString&, unsigned long)
{
}
float URandom::Generate()
{
    this->mSeed = (this->mMultiplier * this->mSeed + this->mIncrement) % this->mModulus;
    return (float)((double)this->mSeed / (double)this->mModulus);
}
unsigned long TMGetTime()
{
    T("time");
    return gFake.mTime;
}

// ---- the stand-in world's objects and answers ----
EEFakeState gFake;
static EEGameOptions sOptions("options");
static EEGameOptions sLobby("lobby");
EEGameOptions& gGameOptions = sOptions;
EEGameOptions& EEWorld::GetLobbyOptions()
{
    return sLobby;
}
bool EEIsMultiplayerGame(bool inFlag)
{
    T("is multiplayer game %d", inFlag);
    return inFlag ? gFake.mMultiplayer : true;
}
bool gLocalPlayerOut;
EEGamePreferences gGamePreferences = {{25, 5, 400, 20, 30, 80, false}};
EEWorldTable gWorldTable;
EEWorld gWorld;
static EEUserInterface sUserInterface;
EEUserInterface* gUserInterface = &sUserInterface;
static EEUIManager sUIManager;
EEUIManager* gUIManager = &sUIManager;
static UIForm sForms[64];
static EEUIFMainGame sMainGame;
static unsigned long FormID(UIForm* inForm)
{
    return inForm == NULL ? 999 : (unsigned long)(inForm - sForms);
}
void EEUIManager::DisplayMessage(const UWideString& inText, bool inA, UIForm* inForm, EEUIFErrorCallback inCallback, bool inB)
{
    T("ui display %s %d %lu %s %d", TName(inText).c_str(), inA, FormID(inForm),
      inCallback == NULL ? "-" : inCallback == (EEUIFErrorCallback)CancelGameCallback ? "cancel" : "?", inB);
}
void EEUIManager::Push(UIForm* inForm)
{
    T("ui push %lu", FormID(inForm));
}
UIForm* EEUIManager::GetFormByID(unsigned long inID)
{
    T("ui form %lu", inID);
    return &sForms[inID];
}
UIForm* EEUIManager::GetCurrentForm()
{
    T("ui current form");
    return &sForms[gFake.mCurrentForm];
}
bool EEUIManager::IsFormVisible(UIForm* inForm)
{
    T("ui visible %lu", FormID(inForm));
    return gFake.mFormVisible;
}
void EEUserInterface::OpenForm(UIForm* inForm)
{
    T("ui open %lu", FormID(inForm));
}
void EEThreadFake::Post(unsigned long inMessage, void* inA, void* inB, bool inWait)
{
    T("%s thread post %lx %ld %ld %d", mName, inMessage, inA ? *(long*)inA : -1L, inB ? *(long*)inB : -1L, inWait);
}
UIEvent* EECreateScreenEvent(unsigned long inEvent)
{
    UIEvent* theEvent = new UIEvent;
    theEvent->mEvent = inEvent;
    return theEvent;
}
std::string EEARemovePlayer::Describe()
{
    char theText[64];
    snprintf(theText, sizeof(theText), "remove player %u %d", mSlot, mExiting);
    return theText;
}
std::string EEABootClientFromCanceledGame::Describe()
{
    char theText[64];
    snprintf(theText, sizeof(theText), "boot client %ld", mText);
    return theText;
}
static long sResourceStorage[8];
UResourceFile* gResource = reinterpret_cast<UResourceFile*>(sResourceStorage);
long UResourceFile::GetString(long inID, UWideString& outString)
{
    char theText[64];
    switch (inID) {
    case IDS_PLAYER_IS_OUT_OF_SYNCH:
    case IDS_PLAYER_HAS_DROPPED_OUT_OF_GAME:
    case IDS_HOST_IS_OUT_OF_SYNCH:
        snprintf(theText, sizeof(theText), "<%ld %%s>", inID);
        break;
    case IDS_TO_JOIN_THIS_GAME_YOU:
    case IDS_GAME_IS_IN_PROGRESS:
        snprintf(theText, sizeof(theText), "<%ld %%s %%s>", inID);
        break;
    default:
        snprintf(theText, sizeof(theText), "<%ld>", inID);
        break;
    }
    T("resource %ld", inID);
    outString = theText;
    return 0;
}
static SESoundManager sSoundManager;
SESoundManager* gSoundManager = &sSoundManager;
long EELoadSound(const UString& inName, long inFlags, unsigned long& outSample, unsigned long& outInstance, long inCount)
{
    T("load sound %s %ld %ld", (const char*)inName, inFlags, inCount);
    outSample = 5;
    outInstance = 6;
    return 0;
}
static EEGraphics sGraphics;
EEGraphics* gGraphics = &sGraphics;
static EEPhysics sPhysics;
EEPhysics* gPhysics = &sPhysics;
void EEPhysics::QueueServerMessage(NEMessage* inMessage)
{
    T("physics queue %s ref %lu", TMsg(inMessage).c_str(), inMessage->GetReferenceCount());
    this->mQueue.push_back(inMessage);
}
long EEPSaveGameHeader::LoadFromFile(FSFile& inFile)
{
    T("header load %lu %lu", mMajorVersion, mMinorVersion);
    this->mChecksum = gFake.mHeaderChecksum;
    return gFake.mHeaderResult;
}
static long sFileStorage[16];
long EEOpenSaveGameFile(const char* inName, const char* inDirectory, FSFileSpec*& outFile)
{
    T("open save %s %s", inName, inDirectory);
    outFile = reinterpret_cast<FSFileSpec*>(sFileStorage);
    return gFake.mOpenSaveResult;
}
UString gSavedGameExtension(".sav");
UString gScenarioExtension(".scn");
UString gCampaignExtension(".cpn");
UString gStartupDirectory("C:\\Game\\");
UString gStartupVolume("C:");
static EEWonNet sWonNet;
EEWonNet* gWonNet;
UWideString gGameVersion("1.04");
uint32_t gBinaryChecksum = 0x1234;
static unsigned long sNetworkAdapter = 3, sStartTime;
unsigned long& gNetworkAdapter = sNetworkAdapter;
unsigned long& gStartTime = sStartTime;
static bool sGameScreenActive;
bool& gGameScreenActive = sGameScreenActive;
static NEProtocol sProtocol;
long NEProtocol::GetProtocol(unsigned long inIndex, NEProtocol*& outProtocol)
{
    T("protocol get %lu", inIndex);
    outProtocol = &sProtocol;
    return 0;
}
NESynchronousCommunications* gCommunications;
long NESynchronousCommunications::ProcessLocalUserInput(NEMessage* inMessage, unsigned char inPlayer)
{
    T("comms local input %u %s", inPlayer, TMsg(inMessage).c_str());
    inMessage->Dereference();
    return 0;
}
NESynchronousServer::NESynchronousServer(unsigned char inMaxPlayers, unsigned long inDefaultSendInterval, unsigned char inPTsPerCT,
                                         unsigned long inMaxPTs, unsigned long inMinPTs, float inTuning, unsigned long inMaxCTSend,
                                         unsigned long inStepUp, unsigned long inStepDown, unsigned long inCT, unsigned long inPT,
                                         UWideString& inName, bool inAllowNew, bool inAnnounce, unsigned long inAdapter)
{
    T("server base ctor %u %lu %u %lu %lu %g %lu %lu %lu %lu %lu %s %d %d %lu", inMaxPlayers, inDefaultSendInterval,
      inPTsPerCT, inMaxPTs, inMinPTs, inTuning, inMaxCTSend, inStepUp, inStepDown, inCT, inPT, TName(inName).c_str(),
      inAllowNew, inAnnounce, inAdapter);
}
long NESynchronousServer::GetClientAddress(unsigned long inIndex, UWideString& outAddress)
{
    T("server client address %lu", inIndex);
    char theText[32];
    snprintf(theText, sizeof(theText), "10.0.0.%lu", inIndex);
    outAddress = theText;
    return 0;
}
UWideString& EEGetLocalAddress(NEProtocol* inProtocol, unsigned long inAdapter)
{
    static UWideString sAddress("192.168.1.1");
    T("local address %lu", inAdapter);
    return sAddress;
}
unsigned long EECPEngineChoiceCount()
{
    return gFake.mEngineChoices;
}
void EELobbyPostConnectStatus(bool inConnected)
{
    T("lobby connect status %d", inConnected);
}
// the game messages' Execute (EEMessage.cpp's host, not linked): traced
void EEMRecord::Execute()
{
    T("execute %s", TMsg(this).c_str());
}

// ---- the random stream ----
static uint32_t sSeed = 12345;
static uint32_t R(uint32_t inRange)
{
    sSeed = sSeed * 1103515245u + 12345u;
    return (sSeed >> 8) % inRange;
}
static bool Chance(uint32_t inPercent)
{
    return R(100) < inPercent;
}
static const char* const kNames[] = {"Ann", "ann", "Bob", "", "Cy", "Dee"};
static const unsigned char kSlots[] = {1, 2, 3, 4, 0xfe, 0xff, 0};

static void RandomPlayer(EEGameOptionsPlayer& p, unsigned long inSlot)
{
    static const unsigned long kTypes[] = {0, 1, 2, 3, 3, 4, 1, 2};
    p.mType = kTypes[R(8)];
    p.mLoadedType = (long)R(4);
    p.mName = kNames[R(6)];
    p.mPing = (float)R(5) * 0.25f;
    p.mSlot = Chance(90) ? inSlot : R(16);
    p.mColor = R(16);
    p.mTeam = R(4);
    p.mStartingCitizens = R(10);
    p.mTransferID = Chance(30) ? R(5) : 0;
    p.mIsLocal = Chance(30);
    p.mIsReady = Chance(50);
    p.mB36 = Chance(70);
    p.mEngine = R(4);
}

static EEPlayer sPlayers[16];
static EEHumanPlayer sHumans[16];

static void RandomWorld()
{
    gFake.mMultiplayer = Chance(40);
    gFake.mTime = 1000 + R(2000);
    static const unsigned char kStates[] = {0, 1, 2, 3, 4, 0, 1};
    gFake.mGameState = kStates[R(7)];
    gFake.mNumPlayers = (unsigned char)R(5);
    gFake.mMaxPlayers = R(10);
    gFake.mCurrentCT = R(100);
    gFake.mCurrentPT = Chance(50) ? 0 : R(100);
    gFake.mCTsInPhysics = R(100);
    gFake.mNumHumans = (unsigned char)R(5);
    gFake.mMultiplayerSession = Chance(50);
    gFake.mGamePaused = Chance(10);
    gFake.mPlayerNumber = R(5);
    gFake.mMySendToSlot = kSlots[R(7)];
    gFake.mConnectionTrys = R(5);
    gFake.mOpenSaveResult = Chance(70) ? 0 : 3;
    gFake.mHeaderResult = Chance(70) ? 0 : 2;
    gFake.mHeaderChecksum = R(3);
    gFake.mFormVisible = Chance(50);
    static const unsigned long kForms[] = {11, 26, 32, 5};
    gFake.mCurrentForm = kForms[R(4)];
    gFake.mWon = Chance(50);
    gWonNet = gFake.mWon ? &sWonNet : NULL;
    gFake.mWonPlayers = R(8);
    gFake.mWonHumans = R(8);
    gFake.mRandom = (long)R(100000);
    gFake.mGameScreenActive = Chance(60);
    sGameScreenActive = gFake.mGameScreenActive;
    gFake.mSample = Chance(80) ? R(9) : 0xffffffffu;
    gFake.mEngineChoices = 2 + R(3);
    gLocalPlayerOut = Chance(50);
    sOptions.mIsMultiplayer = Chance(50);
    sOptions.mLoadType = (long)R(3);
    sOptions.mMapType = Chance(50) ? "Map One" : "Second";
    for (unsigned long i = 0; i < 16; i++) {
        RandomPlayer(sOptions.mPlayers[i], i);
        RandomPlayer(sLobby.mPlayers[i], i);
    }
    // the world: players 0 .. n-1 present (the host's first), a few beyond
    gWorld.mNumPlayers = 1 + R(6);
    for (unsigned long i = 0; i < 16; i++) {
        bool theHuman = i == 0 || Chance(60);
        EEPlayer* p = theHuman ? static_cast<EEPlayer*>(&sHumans[i]) : &sPlayers[i];
        p->mName = kNames[R(6)];
        p->mSlot = i == 0 && Chance(80) ? 0xfe : kSlots[R(7)];
        p->mHuman = theHuman;
        p->mHumanPlayer = theHuman ? (void*)p : NULL;
        p->mPlayerID = i;
        p->mBehindFirewall = Chance(30);
        gWorld.mPlayers[i] = i < gWorld.mNumPlayers || Chance(20) ? p : NULL;
    }
    gWorld.mCurrent = R(gWorld.mNumPlayers);
    gWorld.mTeams.assign(R(2), 0);
}

static void RandomServer(EEServer* s)
{
    s->mSendToSlots.clear();
    s->mBehindFirewall.clear();
    for (unsigned char i = 0; i < 16; i++) {
        if (Chance(40))
            s->mSendToSlots[i] = kSlots[R(7)];
        if (Chance(30))
            s->mBehindFirewall[kSlots[R(7)]] = Chance(50);
    }
    memset(s->mVoted, 0, sizeof(s->mVoted));
    for (int i = 0; i < 4; i++)
        if (Chance(30))
            s->mVoted[R(8)] = true;
    s->mVoteEdict = Chance(50) ? 0 : (unsigned short)(1 + R(20));
    s->mYesVotes = (unsigned short)R(4);
    s->mNoVotes = (unsigned short)R(4);
    s->mNumVotes = (unsigned char)R(5);
    s->mLagMask = (unsigned short)R(8);
    s->mNumLoadResponses = (unsigned char)R(3);
    s->mLoadState = (unsigned char)R(3);
    s->mNumDroppedLoaders = (unsigned char)R(3);
    s->mFastSelectionID = (unsigned char)R(256);
    s->mIsExecutingSelection = false;
    s->mIsMigrating = Chance(20);
    s->mHasLeftGame = Chance(20);
}

static void RandomClient(EEClient* c)
{
    static const unsigned char kUISlots[] = {0xff, 0xff, 0, 1, 2};
    c->mUISlotNumber = kUISlots[R(5)];
    c->mHasLeftGame = Chance(20);
    c->mFastSelectionID = (unsigned char)R(256);
    c->mIsExecutingSelection = false;
    c->mIsMigrating = Chance(20);
    c->mAllowServerMigration = Chance(70);
    c->mIgnoreConnectionTimeout = Chance(20);
    c->mIsServerLost = Chance(20);
    c->mHasLoaded = Chance(40);
    c->mServerLostAfterLoading = Chance(20);
    c->mWasBooted = Chance(20);
}

// ---- the state compared after each call ----
static void State(std::map<std::string, std::string>& outState, EEServer* s, EEClient* c)
{
    char theKey[64];
    char theValue[512];
    for (int i = 0; i < 16; i++) {
        snprintf(theKey, sizeof(theKey), "options %d", i);
        outState[theKey] = TPlayer(sOptions.mPlayers[i]);
        snprintf(theKey, sizeof(theKey), "lobby %d", i);
        outState[theKey] = TPlayer(sLobby.mPlayers[i]);
    }
    for (int i = 0; i < 16; i++) {
        EEPlayer* p = gWorld.mPlayers[i];
        if (p == NULL)
            continue;
        snprintf(theKey, sizeof(theKey), "player %d", i);
        snprintf(theValue, sizeof(theValue), "%s slot %u firewall %d", TName(p->mName).c_str(), p->mSlot, p->mBehindFirewall);
        outState[theKey] = theValue;
    }
    snprintf(theValue, sizeof(theValue), "out %d start %lu ct %lu pt %lu cts %lu humans %u mpsession %d", gLocalPlayerOut,
             sStartTime, gFake.mCurrentCT, gFake.mCurrentPT, gFake.mCTsInPhysics, gFake.mNumHumans, gFake.mMultiplayerSession);
    outState["globals"] = theValue;
    if (s != NULL) {
        std::string theMap;
        for (std::map<unsigned char, unsigned char>::iterator it = s->mSendToSlots.begin(); it != s->mSendToSlots.end(); ++it) {
            snprintf(theValue, sizeof(theValue), "%u>%u ", it->first, it->second);
            theMap += theValue;
        }
        outState["server send-to"] = theMap;
        theMap.clear();
        for (std::map<unsigned char, bool>::iterator it = s->mBehindFirewall.begin(); it != s->mBehindFirewall.end(); ++it) {
            snprintf(theValue, sizeof(theValue), "%u:%d ", it->first, it->second);
            theMap += theValue;
        }
        outState["server firewall"] = theMap;
        theMap.clear();
        for (int i = 0; i < 256; i++)
            if (s->mVoted[i]) {
                snprintf(theValue, sizeof(theValue), "%d ", i);
                theMap += theValue;
            }
        snprintf(theValue, sizeof(theValue), "edict %u yes %u no %u votes %u voted %s", s->mVoteEdict, s->mYesVotes,
                 s->mNoVotes, s->mNumVotes, theMap.c_str());
        outState["server vote"] = theValue;
        snprintf(theValue, sizeof(theValue), "lag %u loads %u state %u dropped %u fs %u exec %d migrating %d left %d",
                 s->mLagMask, s->mNumLoadResponses, s->mLoadState, s->mNumDroppedLoaders, s->mFastSelectionID,
                 s->mIsExecutingSelection, s->mIsMigrating, s->mHasLeftGame);
        outState["server"] = theValue;
    }
    if (c != NULL) {
        snprintf(theValue, sizeof(theValue), "ui %u left %d fs %u exec %d migrating %d allow %d ignore %d lost %d loaded %d lostafter %d booted %d",
                 c->mUISlotNumber, c->mHasLeftGame, c->mFastSelectionID, c->mIsExecutingSelection, c->mIsMigrating,
                 c->mAllowServerMigration, c->mIgnoreConnectionTimeout, c->mIsServerLost, c->mHasLoaded,
                 c->mServerLostAfterLoading, c->mWasBooted);
        outState["client"] = theValue;
    }
}

static void Changes(const std::map<std::string, std::string>& inBefore, const std::map<std::string, std::string>& inAfter)
{
    for (std::map<std::string, std::string>::const_iterator it = inAfter.begin(); it != inAfter.end(); ++it) {
        std::map<std::string, std::string>::const_iterator theOld = inBefore.find(it->first);
        if (theOld == inBefore.end() || theOld->second != it->second)
            T("~ %s: %s", it->first.c_str(), it->second.c_str());
    }
}

// ---- messages ----
static void RandomFields(NEMRecord* m)
{
    EeNetMsgNeType theType = ee_netmsg_ne_type(m->GetType());
    for (uint32_t i = 0; i < theType.fields; i++) {
        EeNetMsgNeField f = ee_netmsg_ne_field(m->GetType(), i);
        if (f.kind == EE_NETMSG_NE_FIELD_INTEGER) {
            uint32_t theValue = Chance(30) ? R(8) : Chance(50) ? 0xff : R(0x20000);
            memcpy(m->mRecord + f.offset, &theValue, f.size < 4 ? f.size : 4);
        } else if (f.kind == EE_NETMSG_NE_FIELD_BOOL)
            m->mRecord[f.offset] = Chance(50);
        else if (f.kind == EE_NETMSG_NE_FIELD_STRING)
            m->mStrings[f.offset] = Chance(70) ? "1.04" : kNames[R(6)];
    }
}

static NEMRecord* NewEngine(unsigned char inType)
{
    NEMRecord* m = new NEMRecord(inType);
    RandomFields(m);
    m->SetOriginatingSlot(kSlots[R(7)]);
    m->SetForSpecificSlot(Chance(50) ? 0xff : kSlots[R(7)]);
    return m;
}

static EEMRecord* NewGame(unsigned char inType)
{
    EEMRecord* m = new EEMRecord(inType);
    for (uint32_t i = 1;; i++) {
        EeNetMsgRecordField f = ee_netmsg_record_member(inType, i);
        if (f.kind == EE_NETMSG_RECORD_FIELD_NONE)
            break;
        unsigned char* theTo = m->mRecord + f.offset;
        if (f.kind == EE_NETMSG_RECORD_FIELD_INTEGER) {
            uint32_t theValue = Chance(60) ? R(6) : R(0x10000);
            memcpy(theTo, &theValue, f.size < 4 ? f.size : 4);
        } else if (f.kind == EE_NETMSG_RECORD_FIELD_BOOL)
            *theTo = Chance(50);
        else if (f.kind == EE_NETMSG_RECORD_FIELD_FLOAT) {
            float theValue = (float)R(8) * 0.5f;
            memcpy(theTo, &theValue, 4);
        } else if (f.kind == EE_NETMSG_RECORD_FIELD_STRING)
            m->mString = kNames[R(6)];
    }
    m->SetOriginatingSlot(kSlots[R(7)]);
    m->SetForSpecificSlot(Chance(50) ? 0xff : kSlots[R(7)]);
    m->mFastSelectionID = (unsigned char)R(256);
    return m;
}

// a lobby input: a game or engine message of the types the lobby handles (and some it does not)
static NEMessage* NewLobbyInput()
{
    static const unsigned char kGame[] = {kMessageFullPlayerInformation, kMessagePlayerInformation, kMessageGameOptions,
                                          kMessagePlayerSlotInfo, kMessageStartLoading, kMessageUpdatePingStat, kMessageChat,
                                          kMessageFullGameOptions, kMessageGameSpeed};
    static const unsigned char kEngine[] = {kNetMessageXferStatus, kNetMessageStartGame, kNetMessageClientConnectionRequestResponse,
                                            kNetMessagePlayerConnectionCheck, kNetMessagePauseGame};
    if (Chance(55)) {
        EEMRecord* m = NewGame(kGame[R(sizeof(kGame))]);
        if (m->GetType() == kMessagePlayerSlotInfo)
            m->mRecord[ee_netmsg_record_member(kMessagePlayerSlotInfo, 1).offset] = (unsigned char)R(16);
        if (m->GetType() == kMessageFullPlayerInformation)
            m->Set<int32_t>(1, (int32_t)R(16));
        return m;
    }
    NEMRecord* m = NewEngine(kEngine[R(sizeof(kEngine))]);
    if (m->GetType() == kNetMessageXferStatus)
        m->Set<unsigned char>(0, (unsigned char)R(16));
    if (m->GetType() == kNetMessageClientConnectionRequestResponse)
        m->Set<int32_t>(0, Chance(50) ? 0 : (int32_t)R(9));
    return m;
}

static NEMRecord* NewTick(bool inLobby)
{
    NEMRecord* m = NewEngine(kNetMessageCommunicationsTick);
    m->Set<unsigned char>(1, inLobby ? 0xff : (unsigned char)R(4));
    unsigned long theCount = R(5);
    for (unsigned long i = 0; i < theCount; i++) {
        NEMessage* theInput = inLobby ? NewLobbyInput() : (NEMessage*)NewGame(Chance(50) ? kMessageGameSpeed : kMessageChat);
        theInput->SetForSpecificSlot(Chance(50) ? 0xff : kSlots[R(7)]);
        m->AddPlayerInput(theInput);
    }
    return m;
}

static const unsigned char kSelections[] = {kMessageSelectUnit, kMessageAddUnitToSelectedGroup, kMessageRemoveUnitFromSelectedGroup,
                                            kMessageUnselectAllUnits, kMessageAddUnitsToSelectedGroup,
                                            kMessageAddAndRemoveUnitsFromSelectedGroup, kMessageAlwaysAddUnitToSelectedGroup,
                                            kMessageSavedGroupAddSelection, kMessageSavedGroupSelect, kMessageSelectTheseUnits};

static NEMessage* NewInput()
{
    if (Chance(40))
        return NewGame(kSelections[R(sizeof(kSelections))]);
    if (Chance(70))
        return NewGame(Chance(50) ? kMessageChat : kMessageGameSpeed);
    return NewEngine(kNetMessagePauseGame);
}

static void Release(NEMessage* inMessage)
{
    T("ref %lu", inMessage->GetReferenceCount());
    inMessage->Dereference();
}

// the connection response's errors
static int32_t RandomError()
{
    static const int32_t kErrors[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x2751};
    return kErrors[R(11)];
}

static EEServer* sServer;
static EEClient* sClient;

static EEServer* NewServer()
{
    return new EEServer(UWideString("Host"), 1, 0, 0, true, true);
}

// ---- one call ----
static void ServerCall(unsigned long inStep)
{
    EEServer* s = sServer;
    gCommunications = s;
    RandomServer(s);
    std::map<std::string, std::string> theBefore, theAfter;
    State(theBefore, s, NULL);
    bool theBecameClient = false;   // the server deleted (a new one may reuse its address: compared by this flag)
    unsigned long theCall = R(25);
    T("== %lu server %lu", inStep, theCall);
    switch (theCall) {
    case 0:
    case 1: {   // a join request (the host's own: inSlot 0xfe)
        EEGameOptionsPlayer p;
        RandomPlayer(p, R(16));
        p.mName = kNames[R(6)];
        unsigned char theSlot = Chance(30) ? 0xfe : (unsigned char)(1 + R(4));
        UWideString theVersion(Chance(85) ? "1.04" : "1.00");
        unsigned long theChecksum = Chance(85) ? 0x1234 : 0x99;
        T("call connection request %u %s %s %lx", theSlot, TPlayer(p).c_str(), TName(theVersion).c_str(), theChecksum);
        s->ProcessConnectionRequest(theSlot, p, theVersion, theChecksum);
        break;
    }
    case 2:
    case 3: {   // a request over the wire (system message or filtered)
        NEMRecord* m = NewEngine(kNetMessageClientConnectionRequest);
        m->SetOriginatingSlot((unsigned char)(1 + R(4)));
        m->Reference();
        T("call %s %s", theCall == 2 ? "system" : "filter", TMsg(m).c_str());
        if (theCall == 2)
            s->ProcessSystemMessage(m);
        else
            T("= %d", s->FilterPlayerInput(m));
        Release(m);
        break;
    }
    case 4: {
        unsigned char theSlot = Chance(30) ? 0xfe : kSlots[R(7)];
        bool theBooted = Chance(50), theExiting = Chance(30);
        T("call boot %u %d %d", theSlot, theBooted, theExiting);
        s->BootPlayer(theSlot, theBooted, theExiting);
        break;
    }
    case 5: {
        EEGameOptionsPlayer p;
        RandomPlayer(p, R(16));
        T("call refuse %s", TPlayer(p).c_str());
        s->RefuseClient(p);
        break;
    }
    case 6: {
        unsigned char thePlayer = Chance(30) ? 0xfe : (unsigned char)R(gWorld.mNumPlayers);
        bool a = Chance(50), b = Chance(50), c = Chance(30);
        T("call remove %u %d %d %d", thePlayer, a, b, c);
        s->ExecuteRemovePlayer(thePlayer, a, b, c);
        break;
    }
    case 7: {
        unsigned char p = (unsigned char)R(20);
        T("call getters %u", p);
        T("= free %u teams %d migrating %d name %s sendto %u slot %u lobby %u present %d oos %d", s->GetFreePlayerSlot(),
          s->ServerSlot28(), s->IsMigrating(), TName(s->ServerSlot54(p)).c_str(), s->GetPlayerSendToSlot(p),
          s->GetSendToSlot(p), s->GetLobbySlot(kSlots[R(7)]), s->IsPlayerPresent(p), s->GetDebugOOSHandling());
        s->ServerSlot48();
        s->PlayWarningSound((long)R(4));
        s->BeforeRemovingPlayers();
        break;
    }
    case 8:
    case 9:
    case 10: {   // system messages
        static const unsigned char kTypes[] = {kNetMessagePlayerConnectionCheckResponse, kNetMessageCommunicationsTick,
                                               kNetMessageClientDisconnectionNotification, kNetMessageClientReconnectionRequest,
                                               kMessageNewEdictEnacted, kMessageLoadOrSaveGame, kMessageLagStatus,
                                               kNetMessagePauseGame, kNetMessageCommunicationsTick};
        unsigned char theType = kTypes[R(sizeof(kTypes))];
        NEMessage* m;
        if (theType == kNetMessageCommunicationsTick)
            m = NewTick(Chance(50));
        else if (theType >= 0x41)
            m = NewGame(theType);
        else
            m = NewEngine(theType);
        if (theType == kNetMessagePlayerConnectionCheckResponse)
            static_cast<NEMRecord*>(m)->Set<uint32_t>(0, gFake.mTime - R(2000));
        if (theType == kNetMessageClientReconnectionRequest)
            static_cast<NEMRecord*>(m)->Set<uint32_t>(1, R(gWorld.mNumPlayers));
        m->Reference();
        T("call system %s", TMsg(m).c_str());
        s->ProcessSystemMessage(m);
        Release(m);
        break;
    }
    case 11:
    case 12: {   // filtered inputs
        static const unsigned char kTypes[] = {kMessageLoadOrSaveGameResponse, kMessageEdictVote, kMessageEdictProposal,
                                               kNetMessageClientRequestPause, kNetMessageClientConnectionRequestResponse,
                                               kNetMessageClientConnectionRequestResponse, kMessageChat};
        unsigned char theType = kTypes[R(sizeof(kTypes))];
        NEMessage* m = theType >= 0x41 ? (NEMessage*)NewGame(theType) : NewEngine(theType);
        if (theType == kNetMessageClientConnectionRequestResponse) {
            static_cast<NEMRecord*>(m)->Set<int32_t>(0, Chance(60) ? (Chance(50) ? 5 : 6) : RandomError());
            if (Chance(60))
                m->SetForSpecificSlot(0xfe);
        }
        if (theType == kMessageEdictVote)
            m->SetOriginatingSlot((unsigned char)R(8));
        m->Reference();
        T("call filter %s", TMsg(m).c_str());
        bool theResult = s->FilterPlayerInput(m);
        T("= %d", theResult);
        if (gCommunications != s) {   // the host became a client: the server is gone
            T("became client");
            theBecameClient = true;
            delete gCommunications;
            sServer = NewServer();
        }
        Release(m);
        break;
    }
    case 13: {
        NEMessage* m = NewInput();
        m->Reference();
        T("call prepare %s", TMsg(m).c_str());
        s->PrepareLocalInput(m);
        T("= fs %d", EEGameMessageCast(m) != NULL ? EEGameMessageCast(m)->mFastSelectionID : -1);
        Release(m);
        break;
    }
    case 14:
    case 15:
    case 16: {
        NEMessage* m = NewLobbyInput();
        m->Reference();
        T("call lobby %s", TMsg(m).c_str());
        s->HandleLobbyInput(m);
        Release(m);
        break;
    }
    case 17: {
        unsigned short theEdict = (unsigned short)R(30);
        T("call start vote %u", theEdict);
        T("= %d", s->StartVote(theEdict));
        break;
    }
    case 18: {
        unsigned char i = (unsigned char)R(8);
        bool y = Chance(50);
        unsigned char n = (unsigned char)R(3);
        T("call cast vote %u %d %u", i, y, n);
        s->CastVote(i, y, n);
        break;
    }
    case 19:
        T("call reconnect wait timed out");
        s->HandleReconnectWaitTimedOut();
        break;
    case 20: {
        std::vector<unsigned char> theSlots;
        for (unsigned long i = R(4); i > 0; i--)
            theSlots.push_back(kSlots[R(7)]);
        bool theFlag = Chance(50);
        T("call laggers %u %d", (unsigned)theSlots.size(), theFlag);
        s->ReportLaggers(theSlots, theFlag);
        break;
    }
    case 21:
        T("call slot info");
        s->SendPlayerSlotInfo();
        break;
    case 22: {
        unsigned char a = kSlots[R(7)], b = (unsigned char)R(101), d = (unsigned char)R(5);
        uint32_t c = R(3) == 0 ? 0xffff : R(9);
        T("call xfer status %u %u %u %u", a, b, c, d);
        s->HandleXferStatus(a, b, c, d);
        break;
    }
    case 23: {
        EEGameOptionsPlayer p;
        RandomPlayer(p, R(16));
        unsigned char i = (unsigned char)R(16);
        T("call full player %u %s", i, TPlayer(p).c_str());
        s->SendFullPlayerInformation(i, p);
        break;
    }
    case 24: {
        unsigned char i = (unsigned char)R(16);
        float f = (float)R(6) * 0.2f;
        T("call ping stat %u %g", i, f);
        s->SendPingStat(i, f);
        break;
    }
    }
    for (size_t i = 0; i < gPhysics->mQueue.size(); i++)
        gPhysics->mQueue[i]->Dereference();
    gPhysics->mQueue.clear();
    State(theAfter, theBecameClient ? NULL : s, NULL);
    Changes(theBefore, theAfter);
}

static void ClientCall(unsigned long inStep)
{
    EEClient* c = sClient;
    gCommunications = c;
    RandomClient(c);
    std::map<std::string, std::string> theBefore, theAfter;
    State(theBefore, NULL, c);
    unsigned long theCall = R(14);
    T("== %lu client %lu", inStep, theCall);
    switch (theCall) {
    case 0: {
        EEGameOptionsPlayer p;
        RandomPlayer(p, R(3));
        unsigned char i = (unsigned char)R(16);
        T("call slot update %u %s", i, TPlayer(p).c_str());
        c->HandleSlotUpdate(i, p);
        break;
    }
    case 1: {
        long theState = (long)R(6);
        T("call connection state %ld", theState);
        c->SetConnectionState(theState);
        break;
    }
    case 2: {
        unsigned char thePlayer = Chance(30) ? 0xfe : Chance(30) ? (unsigned char)gFake.mPlayerNumber : (unsigned char)R(gWorld.mNumPlayers);
        bool a = Chance(50), b = Chance(50), d = Chance(30);
        if (thePlayer == 0xfe && gWorld.GetPlayerBySlot(0xfe) == NULL)
            b = false;   // the host's out-of-sync line needs the host's player
        T("call remove %u %d %d %d", thePlayer, a, b, d);
        c->ExecuteRemovePlayer(thePlayer, a, b, d);
        break;
    }
    case 3:
        T("call getters");
        T("= ignore %d playback %d timeout %lu slot %u server %d client %d", c->IgnoreConnectionTimeout(),
          c->IsPlaybackRecordingSession(), c->GetConnectTimeout(), c->GetClientSlot(), c->IsServer(), c->IsClient());
        c->HandleStopGame();
        break;
    case 4:
    case 5:
    case 6:
    case 7: {
        static const unsigned char kTypes[] = {kNetMessageClientReconnectionRequestResponse, kNetMessageCommunicationsTick,
                                               kNetMessageClientConnectionRequestResponse, kNetMessageClientConnectionRequestResponse,
                                               kNetMessageClientConnectionRequestResponse, kMessageNewEdictEnacted,
                                               kMessageLoadOrSaveGame, kMessageLagStatus, kMessageToggleDayNight,
                                               kMessageSnapToDaylight, kMessageChat, kNetMessagePauseGame};
        unsigned char theType = kTypes[R(sizeof(kTypes))];
        NEMessage* m;
        if (theType == kNetMessageCommunicationsTick)
            m = NewTick(Chance(50));
        else if (theType >= 0x41)
            m = NewGame(theType);
        else
            m = NewEngine(theType);
        if (theType == kNetMessageClientConnectionRequestResponse)
            static_cast<NEMRecord*>(m)->Set<int32_t>(0, RandomError());
        m->Reference();
        // LoadOrSave and LagStatus are released by HandleMessage: one more reference for the trace's
        if (theType == kMessageLoadOrSaveGame || theType == kMessageLagStatus)
            m->Reference();
        T("call message %s", TMsg(m).c_str());
        c->HandleMessage(m);
        Release(m);
        if (theType == kMessageLoadOrSaveGame || theType == kMessageLagStatus)
            m->Dereference();
        break;
    }
    case 8: {
        NEMessage* m = NewInput();
        m->Reference();
        T("call user input %s", TMsg(m).c_str());
        c->HandleUserInput(m);
        T("= fs %d", EEGameMessageCast(m) != NULL ? EEGameMessageCast(m)->mFastSelectionID : -1);
        Release(m);
        break;
    }
    case 9:
        T("call connection lost");
        c->HandleConnectionLost();
        break;
    case 10:
    case 11: {
        NEMessage* m = NewLobbyInput();
        m->Reference();
        T("call lobby %s", TMsg(m).c_str());
        c->HandleLobbyInput(m);
        Release(m);
        break;
    }
    case 12:
        T("call start game");
        T("= %lu", c->HandleStartGame(R(3)));
        break;
    case 13: {
        unsigned long theRemaining = R(80000);
        T("call timeout warning %lu", theRemaining);
        c->HandleConnectionTimeoutWarning(theRemaining);
        break;
    }
    }
    for (size_t i = 0; i < gPhysics->mQueue.size(); i++)
        gPhysics->mQueue[i]->Dereference();
    gPhysics->mQueue.clear();
    State(theAfter, NULL, c);
    Changes(theBefore, theAfter);
}

int main(int argc, char** argv)
{
    const char* thePrint = NULL;
    unsigned long theSteps = 6000;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--print") == 0 && i + 1 < argc)
            thePrint = argv[++i];
        else if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc)
            theSteps = strtoul(argv[++i], NULL, 10);
    }
    sUIManager.mMainGameForm = &sMainGame;
    sServer = NewServer();
    sClient = new EEClient;
    for (unsigned long i = 0; i < theSteps; i++) {
        RandomWorld();
        if (R(5) < 3)
            ServerCall(i);
        else
            ClientCall(i);
    }
    delete sServer;
    delete sClient;
    if (thePrint != NULL) {
        FILE* f = fopen(thePrint, "wb");
        fwrite(sTrace.data(), 1, sTrace.size(), f);
        fclose(f);
        printf("ee_session_test: %lu trace lines written to %s\n", sLines, thePrint);
        return 0;
    }
    // the golden (tests/net/golden_session.txt, beside this source)
    std::string theGolden;
    const char* thePaths[] = {"../../../../../tests/net/golden_session.txt", "tests/net/golden_session.txt"};
    for (const char* thePath : thePaths) {
        FILE* f = fopen(thePath, "rb");
        if (f == NULL)
            continue;
        char theBuffer[65536];
        size_t n;
        while ((n = fread(theBuffer, 1, sizeof(theBuffer), f)) > 0)
            theGolden.append(theBuffer, n);
        fclose(f);
        break;
    }
    if (theGolden.empty()) {
        printf("ee_session_test: no golden (tests/net/golden_session.txt)\n");
        return 1;
    }
    if (theGolden != sTrace) {
        size_t i = 0;
        while (i < theGolden.size() && i < sTrace.size() && theGolden[i] == sTrace[i])
            i++;
        size_t theLine = 1;
        for (size_t j = 0; j < i; j++)
            theLine += sTrace[j] == '\n';
        printf("ee_session_test: %lu trace lines, DIFFERENT from the golden at line %u\n", sLines, (unsigned)theLine);
        return 1;
    }
    printf("ee_session_test: %lu trace lines, equal to the golden\n", sLines);
    return 0;
}
