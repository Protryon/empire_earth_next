// C:\EEX\Empire Earth\Persistent\EEGameStateLoader.cpp (file name inferred: no __FILE__, no RTTI).
// 0x14a009..0x14ac35: the saved-game loader (EEGameStateLoader, ee/EEGameStateLoader.h; EEGameState::Create and
// EESaveLoad 0x14d4fd use it), EELoadPersistent, the scenario flag GetIsLoadingScenario, EEGameState's map/effect/scenario
// setters and the COMDATs first used here. EEPCalamityHelper.cpp / EEPChecksum.cpp come before it,
// EEPMultiplayerHelper.cpp after. Unit flags: the exe module set.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <deque>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GERectangle.h"
#include "lle/NEMessage.h"
#include "lle/U2DPoint.h"
#include "lle/UIEvent.h"
#include "lle/UResourceFile.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/GEModelManager.h"
#include "lle/FSFileSpec.h"
#include "lle/FSFile.h"
#include "lle/FSMemoryFile.h"
#include "lle/ESChecksum.h"
#include "lle/TSScheduler.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DRectangle.h"
#include "lle/USingleton.h"
#include "lle/SESoundManager.h"
#include "lle/UIControl.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIFormManager.h"
#include "lle/GELine.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "lle/NESynchronousClient.h"
#include "lle/NESynchronousServer.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEHumanPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EETeam.h"
#include "ee/EEGameState.h"
#include "ee/EEGameOptions.h"
#include "ee/EEStatisticsRecord.h"
#include "ee/EEDatabase.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEColorTable.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EECampaignFormManager.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEPCalamityHelper.h"
#include "ee/EEEffectManager.h"
#include "lle/FSChunk.h"
#include "lle/FSDebugFile.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EECommunications.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EEPersistentTemplates.h"
#include "ee/EEPChecksum.h"
#include "ee/EEPPlayerHelper.h"
#include "ee/EEPWorldHelper.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EESaveLoad.h"
#include "ee/EEGameStateLoader.h"
#include "ee/EEPersistentTag.h"

void EEReportLoadFailure(const UString& inMessage, bool inArg, ESChecksum* inChecksum);  // 0x148362 (EEWorld.cpp's prototype)

// 0x531a40: set while Load reads a scenario (GetIsLoadingScenario's byte; provisional name)
static bool sLoadingScenario;

// FUNCTION: EXE 0x54a009
// Creates a loader that restores a saved game into the given game state
EEGameStateLoader::EEGameStateLoader(EEGameState* inState)
{
    this->mState = inState;
}

// FUNCTION: EXE 0x54a014
// Returns whether a file name ends in the scenario extension (case-insensitive)
bool EEIsScenarioFile(const UString& inFile)
{
    UString theFile;
    UString theExtension;
    bool theResult = false;
    theFile = inFile;
    theFile.ToLower();
    theExtension = theFile.GetSubstring(theFile.GetLength() - gScenarioExtension.GetLength(),
                                        gScenarioExtension.GetLength());
    theExtension.ToLower();
    if (theExtension.Equals(gScenarioExtension))
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x54a09e
// Returns whether a scenario file is being loaded right now
bool GetIsLoadingScenario()
{
    return sLoadingScenario;
}

// FUNCTION: EXE 0x54a0a4
// Reads every saved chunk in tag order into the game state (checksum, seed, effects, camera, map, players, world, ...)
// while advancing the load progress bar; returns 3 for an unknown tag or a chunk of the wrong type
long EEGameStateLoader::RestorePersistents(std::vector<long>& inTags, FSFile& inFile)
{
    FSPersistent* thePersistent = NULL;
    long theResult = kErrorOK;
    for (unsigned long i = 0; i < inTags.size() && theResult == kErrorOK; i++) {
        gUserInterface->SetLoadProgress((float)i / inTags.size() * 0.4f + 0.1f);
        theResult = this->ReadPersistent(inFile, thePersistent);
        if (theResult != kErrorOK)
            continue;
        switch (thePersistent->GetTagID()) {
        case kPersistentChecksum: {
            EEPChecksum* theChecksum = dynamic_cast<EEPChecksum*>(thePersistent);
            if (theChecksum == NULL) {
                theResult = 3;
                break;
            }
            this->mState->mChecksum = *theChecksum;
            break;
        }
        case kPersistentSeed: {
            EEPTGeneric<unsigned long>* theValue = dynamic_cast<EEPTGeneric<unsigned long>*>(thePersistent);
            if (theValue == NULL) {
                theResult = 3;
                break;
            }
            this->mState->mRandomSeed = theValue->mValue;
            break;
        }
        case kPersistentEffects: {
            EEPTPRestorable<EEEffectManager>* theEffects =
                dynamic_cast<EEPTPRestorable<EEEffectManager>*>(thePersistent);
            if (theEffects == NULL) {
                theResult = 3;
                break;
            }
            EEEffectManager* theManager = theEffects->mObject;
            this->mState->SetEffectManager(theManager);
            this->mState->mRestorables.push_back(theManager);
            theEffects->mOwned = false;
            break;
        }
        case kPersistentCamera: {
            EEPTRestorable<GE3DPoint>* thePoint = dynamic_cast<EEPTRestorable<GE3DPoint>*>(thePersistent);
            if (thePoint == NULL) {
                theResult = 3;
                break;
            }
            GE3DPoint theCenter = thePoint->mValue;
            if (this->mState->mMinorVersion > 0x42)
                this->mState->mMapCenter = theCenter;
            else
                this->mState->SetDefaultMapCenter();
            break;
        }
        case kPersistentMap: {
            EEPTPRestorable<EEMap>* theMap = dynamic_cast<EEPTPRestorable<EEMap>*>(thePersistent);
            if (theMap == NULL) {
                theResult = 3;
                break;
            }
            this->mState->SetMap(theMap->mObject);
            theMap->mOwned = false;
            break;
        }
        case kPersistentPlayers: {
            EEPPlayerHelper* thePlayers = dynamic_cast<EEPPlayerHelper*>(thePersistent);
            if (thePlayers == NULL) {
                theResult = 3;
                break;
            }
            this->RestorePlayers(thePlayers);
            break;
        }
        case kPersistentWorld: {
            EEPWorldHelper* theWorld = dynamic_cast<EEPWorldHelper*>(thePersistent);
            if (theWorld == NULL) {
                theResult = 3;
                break;
            }
            theWorld->Restore(*this->mState);
            break;
        }
        case kPersistentScenario: {
            EEPTPRestorable<EEScenarioManager>* theScenario =
                dynamic_cast<EEPTPRestorable<EEScenarioManager>*>(thePersistent);
            if (theScenario == NULL) {
                theResult = 3;
                break;
            }
            this->mState->SetScenarioManager(theScenario->mObject);
            theScenario->mOwned = false;
            break;
        }
        case kPersistentCalamities: {
            EEPCalamityHelper* theCalamities = dynamic_cast<EEPCalamityHelper*>(thePersistent);
            if (theCalamities == NULL) {
                theResult = 3;
                break;
            }
            this->mState->SetCalamityHelper(theCalamities);
            this->mState->mRestorables.push_back(theCalamities);
            thePersistent = NULL;
            break;
        }
        case kPersistentEnvironment: {
            EEPTPRestorable<EEEnvironmentManager>* theEnvironment =
                dynamic_cast<EEPTPRestorable<EEEnvironmentManager>*>(thePersistent);
            if (theEnvironment == NULL) {
                theResult = 3;
                break;
            }
            EEEnvironmentManager* theManager = theEnvironment->mObject;
            this->mState->SetEnvironmentManager(theManager);
            this->mState->mRestorables.push_back(theManager);
            theEnvironment->mOwned = false;
            break;
        }
        case kPersistentMultiplayer:
            break;
        default:
            theResult = 3;
            break;
        }
        delete thePersistent;
        thePersistent = NULL;
    }
    return theResult;
}

// FUNCTION: EXE 0x54a3eb
// Loads a saved game or scenario file into memory and restores the game state from it; returns whether it worked
bool EEGameStateLoader::Load(FSFile& inFile)
{
    FSMemoryFile theMemory(1000, true, 1);
    FSDebugFile theDebugFile(&theMemory);
    UString theName;
    bool theResult = false;
    theName = GetSaveTextName(inFile.GetFile(), false);
    sLoadingScenario = EEIsScenarioFile(inFile.GetFile());
    if ((inFile.GetIsOpen() || inFile.Open(kFileAccessRead) == kErrorOK) && theMemory.Load(inFile) == kErrorOK) {
        inFile.Close();
        theResult = this->LoadGame(theMemory, theMemory);
        theMemory.Close();
    }
    sLoadingScenario = false;
    return theResult;
}

// FUNCTION: EXE 0x54a4ef
// Restores the game state from a saved header and its chunks; on failure reports "Load game file is corrupt."
// and clears the state; returns whether it worked
bool EEGameStateLoader::LoadGame(FSFile& inHeaderFile, FSFile& inFile)
{
    std::vector<long> theTags;
    EEPSaveGameHeader theHeader;
    bool theResult = false;
    this->mState->WriteLock();
    this->mState->Init();
    if (gUIManager->mInScenarioEditor)
        this->mState->mInScenarioEditor = true;
    if (theHeader.Load(kSaveGameMajorVersion, kSaveGameMinorVersion, inHeaderFile) == kErrorOK) {
        this->mState->mMajorVersion = theHeader.mMajorVersion;
        this->mState->mMinorVersion = theHeader.mMinorVersion;
        this->mState->mGameOptions = theHeader.mGameOptions;
        theTags = theHeader.mChunkTypes;
        if (this->RestorePersistents(theTags, inFile) == kErrorOK)
            theResult = true;
        this->mState->FinishLoad(theHeader.mMajorVersion, theHeader.mMinorVersion);
    }
    if (!theResult) {
        EEReportLoadFailure("Load game file is corrupt.", false, NULL);
        this->mState->Clear();
    }
    this->mState->mIsLoadedGame = theResult;
    this->mState->WriteUnlock();
    return theResult;
}

// FUNCTION: EXE 0x54a61f
// Reads one chunk (by tag) out of a saved game file without loading the rest; returns the read error
long EELoadPersistent(const UString& inFile, long inTagID, FSPersistent*& outPersistent)
{
    EEPSaveGameHeader theHeader;
    FSMemoryFile theMemory(1000, true, 1);
    std::vector<long> theTags;
    FSFileSpec* theFile = NULL;
    long theResult = gDataPath->FindFile(inFile, theFile);
    if (theResult == kErrorOK) {
        theResult = theFile->Open(kFileAccessRead);
        if (theResult == kErrorOK) {
            theMemory.Load(*theFile);
            theResult = theHeader.Load(kSaveGameMajorVersion, kSaveGameMinorVersion, theMemory);
            theTags = theHeader.mChunkTypes;
            for (unsigned long i = 0; i < theTags.size(); i++) {
                FSChunk theChunk;
                if (theTags[i] == inTagID) {
                    theResult = theChunk.LoadChunkFromFile(theMemory);
                    if (theResult == kErrorOK)
                        theResult = theChunk.LoadPersistentFromChunk(outPersistent);
                    break;
                }
                theChunk.SkipChunk(theMemory);
            }
        }
        delete theFile;
    }
    return theResult;
}

// FUNCTION: EXE 0x54a765
// Reads the next chunk of a saved game and creates its persistent object; returns the read error
long EEGameStateLoader::ReadPersistent(FSFile& inFile, FSPersistent*& outPersistent)
{
    FSChunk theChunk;
    outPersistent = NULL;
    long theResult = theChunk.LoadChunkFromFile(inFile);
    if (theResult == kErrorOK)
        theResult = theChunk.LoadPersistentFromChunk(outPersistent);
    return theResult;
}

// FUNCTION: EXE 0x54a7ab
// Moves the loaded players into the game state (extra players beyond the slot count become closed loaded players)
// and, when hosting, gives the server slot to the local player; returns the player helper's error
long EEGameStateLoader::RestorePlayers(EEPPlayerHelper* inHelper)
{
    std::vector<EEPlayer*> thePlayers;
    EEGameOptionsPlayer theOptions;
    long theLocal;
    long theResult = inHelper->GetLocalPlayerIndex(theLocal);
    if (theResult == kErrorOK) {
        this->mState->mLocalPlayer = theLocal;
        theResult = inHelper->GetPlayers(thePlayers);
        if (this->mState->mInScenarioEditor == true) {
            unsigned long theCount = thePlayers.size();
            this->mState->mPlayers.resize(theCount);
            this->mState->mNumPlayersMade = inHelper->mMaxPlayers;
        } else {
            unsigned long theCount = inHelper->mMaxPlayers;
            this->mState->mPlayers.resize(theCount);
            this->mState->mNumPlayersMade = theCount;
        }
        if (theResult == kErrorOK) {
            for (unsigned long i = 0; i < thePlayers.size(); i++) {
                EEPlayer* thePlayer = thePlayers[i];
                if (this->mState->mInScenarioEditor == false && i >= inHelper->mMaxPlayers) {
                    this->mState->mLoadedPlayers.push_back(thePlayer);
                    for (unsigned long j = 0; j < this->mState->GetNumPlayers(); j++)
                        this->mState->GetPlayer(j)->RemoveAlly(thePlayer);
                    theOptions = this->mState->mGameOptions.mPlayers[i - 1];
                    theOptions.mType = 4;
                    this->mState->mGameOptions.mPlayers[i - 1] = theOptions;
                } else {
                    *(this->mState->mPlayers.begin() + i) = thePlayer;
                }
            }
        }
    }
    // Only with the players read: a failed read left the vector empty and theLocal unset.
    if (theResult == kErrorOK && gCommunications != NULL && gCommunications->IsServer()) {
        if (thePlayers.size() > 1)
            thePlayers[1]->mSlot = kSlotNone;
        if ((unsigned long)theLocal < thePlayers.size())
            thePlayers[theLocal]->mSlot = kSlotServer;
    }
    inHelper->Fn157269(this->mState);
    return theResult;
}

// FUNCTION: EXE 0x54a9ac
// Centres the camera on the middle of the map, if there is one
void EEGameState::SetDefaultMapCenter()
{
    if (this->mMap == NULL)
        return;
    float theCenter = (float)(this->mMap->mL195610 + this->mMap->mL195614) * 0.5f;
    theCenter += 0.5f;
    this->mMapCenter = GE3DPoint(theCenter, theCenter, 0.0f);
}

// FUNCTION: EXE 0x54a9e8
// Replaces the game state's effect manager, deleting the old one
void EEGameState::SetEffectManager(EEEffectManager* inManager)
{
    delete this->mEffectManager;
    this->mEffectManager = inManager;
}

// FUNCTION: EXE 0x54aa06
// Replaces the game state's map, deleting the old one
void EEGameState::SetMap(EEMap* inMap)
{
    delete this->mMap;
    this->mMap = inMap;
}

// FUNCTION: EXE 0x54aa24
// Replaces the game state's scenario manager, deleting the old one
void EEGameState::SetScenarioManager(EEScenarioManager* inManager)
{
    delete this->mScenarioManager;
    this->mScenarioManager = inManager;
}

// TODO 0x54aa42 (360 bytes, vector<EETeam>::operator=: no user found in this unit yet)


// TODO 0x54abaa (25 bytes: an implicit operator= of a class with a vector<pointer> at +4; owner unknown)
// TODO 0x54abc3 (22 bytes: the size of a vector of 0x18-byte elements at +4; owner unknown)
// TEMPLATE: EXE 0x54abd9
// ?push_back@?$vector@PAVFSRestorable@@V?$allocator@PAVFSRestorable@@@std@@@std@@QAEXABQAVFSRestorable@@@Z
// TEMPLATE: EXE 0x54ac05
// ??G?$_Ptrit@PAVEEPlayer@@HPAPAV1@AAPAV1@PAPAV1@AAPAV1@@std@@QBE?AV01@H@Z
// TODO 0x54ac19 (28 bytes: EETeam's scalar deleting dtor COMDAT)
