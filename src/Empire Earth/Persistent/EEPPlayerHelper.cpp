// C:\EEX\Empire Earth\Persistent\EEPPlayerHelper.cpp (__FILE__ 0x4c17c0, in CreatePlayer's and the player-type
// ESVERIFYs). The saved players: 0x14b22d..0x14b7a5. Unit flags: the exe's module set.
// Data: the vtable 0x442288; the four class static keys share the guard byte 0x531b00.
// CreatePlayer and GetPlayerType are file statics (cdecl), each emitted right before the function that follows its
// first user.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEHumanPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEPPlayerHelper.h"
#include "ee/EEPersistentTag.h"
#include "ee/EECPEngine.h"
#include "ee/EECPNext.h"
#include "ee/EECPNextUnits.h"

const UString EEPPlayerHelper::sPlayerCount("EEPPlayerHelper-PlayerCount");
const UString EEPPlayerHelper::sPlayerNum("EEPPlayerHelper-PlayerNum");
const UString EEPPlayerHelper::sPlayerCode("EEPPlayerHelper-PlayerCode");
const UString EEPPlayerHelper::sScenarioPlayerIndex("EEPPlayerHelper-ScenarioPlayerIndex");

// FUNCTION: EXE 0x54b339
// Creates an empty saved-players chunk for loading (three player slots, local player 0)
EEPPlayerHelper::EEPPlayerHelper()
    : FSPersistent(kPersistentPlayers)
{
    this->mLocalPlayerIndex = 0;
    this->mLoaded = false;
    this->mIsForLoading = true;
    this->mMaxPlayers = 3;
}

// SYNTHETIC: EXE 0x54b380
// (the scalar deleting destructor, emitted with the vtable; the implicit dtor inline)

// FUNCTION: EXE 0x54b3a6
// Creates the saved-players chunk for saving from the world's existing players, slot count and current player
EEPPlayerHelper::EEPPlayerHelper(EEWorld& inWorld)
    : FSPersistent(kPersistentPlayers)
{
    for (long i = 0; i < 17; i++) {
        EEPlayer* thePlayer = inWorld.mPlayers[i];
        if (thePlayer != NULL)
            this->mPlayers.push_back(thePlayer);
    }
    this->mIsForLoading = false;
    this->mLocalPlayerIndex = inWorld.mCurrentPlayer;
    this->mLoaded = false;
    this->mMaxPlayers = inWorld.mMaxPlayers;
}

// FUNCTION: EXE 0x54b42f
// Creates an empty saved-players chunk for loading (persistent factory)
FSPersistent* EEPPlayerHelper::CreateDefault() const
{
    return new EEPPlayerHelper;
}

// FUNCTION: EXE 0x54b446
// Adds every player's checksum and the local player index to a running checksum
void EEPPlayerHelper::GetChecksum(ESChecksum& ioChecksum) const
{
    for (unsigned long i = 0; i < this->mPlayers.size(); i++)
        this->mPlayers[i]->GetChecksum(ioChecksum);
    ioChecksum.AddChecksum("EEPPlayerHelper::mLocalPlayerIndex", this->mLocalPlayerIndex);
}

// FUNCTION: EXE 0x54b4a3
// Returns the loaded local player index; error 13 unless the chunk was loaded
long EEPPlayerHelper::GetLocalPlayerIndex(long& outIndex)
{
    long theResult = 13;
    if (this->mIsForLoading && this->mLoaded) {
        outIndex = this->mLocalPlayerIndex;
        theResult = kErrorOK;
    }
    return theResult;
}

// FUNCTION: EXE 0x54b4c0
// Returns the loaded players; error 13 unless the chunk was loaded
long EEPPlayerHelper::GetPlayers(std::vector<EEPlayer*>& outPlayers)
{
    long theResult = 13;
    if (this->mIsForLoading && this->mLoaded) {
        outPlayers = this->mPlayers;
        theResult = kErrorOK;
    }
    return theResult;
}

static EEPlayer* CreatePlayer(long inType);
static long GetPlayerType(EEPlayer* inPlayer);

// FUNCTION: EXE 0x54b4e1
// Reads the slot count, the local player index and each player (human or computer) from a saved game, then fills
// each player's ally list with the allies that share line of sight; returns the first read error
long EEPPlayerHelper::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = kErrorOK;
    EEPlayer* thePlayer = NULL;
    unsigned long theCount = 0;
    long theType = 0;
    unsigned long i;
    unsigned long j;
    if (this->mIsForLoading) {
        theResult = inFile.Read(theCount);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(this->mMaxPlayers);
            if (theResult == kErrorOK) {
                theResult = inFile.Read(this->mLocalPlayerIndex);
                if (theResult == kErrorOK) {
                    for (i = 0; i < theCount && theResult == kErrorOK; i++) {
                        theResult = inFile.Read(theType);
                        if (theResult == kErrorOK) {
                            thePlayer = CreatePlayer(theType);
                            // An unknown player type (corrupt save) creates no player: fail the load instead of
                            // calling through NULL.
                            if (thePlayer == NULL)
                                theResult = kErrorFailed;
                            else
                                theResult = thePlayer->Load(inMajorVersion, inMinorVersion, inFile);
                            // port: a computer player's engine from version 0.251 (ee/EECPEngine.h; older: classic),
                            // and from 0.253 the next engine's AI id after it (a uint32 length, then the bytes;
                            // before, the next engine played boom). An AI the manifest no longer lists fails the load.
                            if (theResult == kErrorOK && theType == 1 && thePlayer->mComputerPlayer != NULL
                                && (inMajorVersion > 0 || inMinorVersion >= 251)) {
                                unsigned char theEngine = kEECPEngineClassic;
                                theResult = inFile.Read(theEngine);
                                thePlayer->mComputerPlayer->mEngine = kEECPEngineClassic;
                                if (theResult == kErrorOK && theEngine == kEECPEngineNext) {
                                    std::string theAI = "boom";
                                    if (inMajorVersion > 0 || inMinorVersion >= 253) {
                                        unsigned long theLength = 0;
                                        theResult = inFile.Read(theLength);
                                        if (theResult == kErrorOK && (theLength == 0 || theLength > 64))
                                            theResult = kErrorFailed;
                                        if (theResult == kErrorOK) {
                                            theAI.assign(theLength, '\0');
                                            unsigned long theRead = 0;
                                            theResult = inFile.Read(&theAI[0], theLength, theRead);
                                            if (theResult == kErrorOK && theRead != theLength)
                                                theResult = kErrorFailed;
                                        }
                                    }
                                    if (theResult == kErrorOK && EECPAIByID(theAI) == NULL) {
                                        EECPEngineLog("the saved game's player %lu plays the AI '%s', which "
                                                      "Data/ai/ais.json does not list: the game cannot be loaded",
                                                      thePlayer->mPlayerID, theAI.c_str());
                                        theResult = kErrorFailed;
                                    }
                                    if (theResult == kErrorOK) {
                                        thePlayer->mComputerPlayer->mEngine = kEECPEngineNext;
                                        EECPNextGetState(thePlayer->mComputerPlayer)->mAI = theAI;
                                    }
                                }
                            }
                            // port: from version 0.252 the next engine's state and its script's memory (ee/EECPNext.h
                            // EECPNextSaveState; a uint32 length, 0 for none, then the bytes)
                            if (theResult == kErrorOK && theType == 1 && thePlayer->mComputerPlayer != NULL
                                && (inMajorVersion > 0 || inMinorVersion >= 252)) {
                                unsigned long theLength = 0;
                                theResult = inFile.Read(theLength);
                                if (theResult == kErrorOK && theLength > 0) {
                                    std::string theBytes(theLength, '\0');
                                    unsigned long theRead = 0;
                                    theResult = theLength > (64u << 20) ? kErrorFailed
                                        : inFile.Read(&theBytes[0], theLength, theRead);
                                    if (theResult == kErrorOK && (theRead != theLength
                                        || !EECPNextLoadState(thePlayer->mComputerPlayer, theBytes)))
                                        theResult = kErrorFailed;
                                }
                            }
                            // port: from version 0.254 the per-unit Lua interface's state (ee/EECPNextUnits.h; a uint32
                            // length, 0 for none, then the bytes)
                            if (theResult == kErrorOK && theType == 1 && thePlayer->mComputerPlayer != NULL
                                && (inMajorVersion > 0 || inMinorVersion >= 254)) {
                                unsigned long theLength = 0;
                                theResult = inFile.Read(theLength);
                                if (theResult == kErrorOK && theLength > 0) {
                                    std::string theBytes(theLength, ' ');
                                    unsigned long theRead = 0;
                                    theResult = theLength > (64u << 20) ? kErrorFailed
                                        : inFile.Read(&theBytes[0], theLength, theRead);
                                    if (theResult == kErrorOK && (theRead != theLength
                                        || !EECPUnitsLoad(thePlayer->mComputerPlayer, theBytes)))
                                        theResult = kErrorFailed;
                                }
                            }
                            if (theResult == kErrorOK)
                                this->mPlayers.push_back(thePlayer);
                        }
                        this->mLoaded = true;
                    }
                }
            }
        }
    }
    for (i = 0; i < this->mPlayers.size(); i++) {
        for (j = 0; j < this->mPlayers.size(); j++) {
            if (j != i && this->mPlayers[j]->IsAlliedWith(this->mPlayers[i]) && this->mPlayers[j]->mSharedLOS == true) {
                EEPlayer* theAlly = this->mPlayers[i];
                this->mPlayers[j]->mAllies.push_back(theAlly);
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x54b629
// Creates an empty human (type 0) or computer (type 1) player; NULL (and an assert for type 3) otherwise
static EEPlayer* CreatePlayer(long inType)
{
    EEPlayer* thePlayer = NULL;
    switch (inType) {
    case 0:
        thePlayer = new EEHumanPlayer;
        break;
    case 1:
        thePlayer = new EEComputerPlayer;
        break;
    case 3:
        ESVERIFY_AT(false, 251);
        break;
    }
    return thePlayer;
}

// FUNCTION: EXE 0x54b689
// Returns a player's save type: 0 human, 1 computer, 3 for both (asserts when it is neither)
static long GetPlayerType(EEPlayer* inPlayer)
{
    bool theIsComputerPlayer = inPlayer->mComputerPlayer != NULL;
    bool theIsHumanPlayer = inPlayer->mHumanPlayer != NULL;
    ESVERIFY_AT(theIsComputerPlayer || theIsHumanPlayer, 274);
    if (theIsComputerPlayer && theIsHumanPlayer)
        return 3;
    if (theIsHumanPlayer)
        return 0;
    return 1;
}

// FUNCTION: EXE 0x54b6d0
// Writes the player count, slot count, local player index and each player with its type to a save; fails for a
// loading chunk or no players
long EEPPlayerHelper::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    if (this->mIsForLoading || this->mPlayers.size() <= 0)
        return kErrorFailed;
    unsigned long theCount = this->mPlayers.size();
    long theResult = outFile.Write(theCount);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mMaxPlayers);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mLocalPlayerIndex);
    if (theResult != kErrorOK)
        return theResult;
    for (unsigned long i = 0; i < theCount && theResult == kErrorOK; i++) {
        EEPlayer* thePlayer = this->GetPlayer(i);

        long theType = GetPlayerType(thePlayer);
        theResult = outFile.Write(theType);
        if (theResult == kErrorOK)
            theResult = thePlayer->Save(inMajorVersion, inMinorVersion, outFile);
        if (theResult == kErrorOK && theType == 1)   // port: the computer player's engine (Load: from 0.251)
            theResult = outFile.Write(thePlayer->mComputerPlayer->mEngine);
        if (theResult == kErrorOK && theType == 1 && thePlayer->mComputerPlayer->mEngine == kEECPEngineNext) {
            // port: the next engine's AI id (Load: from 0.253)
            std::string theAI = EECPEngineAI(thePlayer->mComputerPlayer);
            if (theAI.empty())
                theAI = "boom";
            unsigned long theLength = (unsigned long)theAI.size();
            unsigned long theWritten = 0;
            theResult = outFile.Write(theLength);
            if (theResult == kErrorOK)
                theResult = outFile.Write(theAI.data(), theLength, theWritten);
        }
        if (theResult == kErrorOK && theType == 1) {   // port: the next engine's state (Load: from 0.252)
            std::string theBytes;
            EECPNextSaveState(thePlayer->mComputerPlayer, theBytes);   // a script's failure is logged; its state goes
            unsigned long theLength = (unsigned long)theBytes.size();
            theResult = outFile.Write(theLength);
            unsigned long theWritten = 0;
            if (theResult == kErrorOK && theLength > 0)
                theResult = outFile.Write(theBytes.data(), theLength, theWritten);
        }
        if (theResult == kErrorOK && theType == 1) {   // port: the per-unit Lua interface's state (Load: from 0.254)
            std::string theBytes;
            EECPUnitsSave(thePlayer->mComputerPlayer, theBytes);
            unsigned long theLength = (unsigned long)theBytes.size();
            theResult = outFile.Write(theLength);
            unsigned long theWritten = 0;
            if (theResult == kErrorOK && theLength > 0)
                theResult = outFile.Write(theBytes.data(), theLength, theWritten);
        }
    }
    return theResult;
    return kErrorFailed;
}

// SYNTHETIC: EXE 0x54b22d
// SYNTHETIC: EXE 0x54b237
// SYNTHETIC: EXE 0x54b248
// SYNTHETIC: EXE 0x54b254
// SYNTHETIC: EXE 0x54b270
// SYNTHETIC: EXE 0x54b27a
// SYNTHETIC: EXE 0x54b28b
// SYNTHETIC: EXE 0x54b297
// SYNTHETIC: EXE 0x54b2b3
// SYNTHETIC: EXE 0x54b2bd
// SYNTHETIC: EXE 0x54b2ce
// SYNTHETIC: EXE 0x54b2da
// SYNTHETIC: EXE 0x54b2f6
// SYNTHETIC: EXE 0x54b300
// SYNTHETIC: EXE 0x54b311
// SYNTHETIC: EXE 0x54b31d

// Does nothing (no address: empty, ICF-folded into 0x157269; EEGameStateLoader::RestorePlayers 0x14a98d calls it
// with its EEGameState: `push [esi]; mov ecx, [ebp+8] (the EEPPlayerHelper*); call 0x157269`, a direct thiscall on
// the helper; provisional name)
void EEPPlayerHelper::Fn157269(EEGameState* inState)
{
}
