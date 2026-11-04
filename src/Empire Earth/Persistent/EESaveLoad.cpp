// C:\EEX\Empire Earth\Persistent\EESaveLoad.cpp (__FILE__ 0x4c1b64): saving and loading a game, 0x14c6b1..0x14e839.
// Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"

// two keys of EEASaveGame's save format that nothing here uses (only their chains reference them; provisional names)
const UString kEEASaveGameChecksumKey("EEASaveGame-Checksum");
const UString kEEASaveGameMapSeedKey("EEASaveGame-MapSeed");

// the last save/load error message (0x531c70; EESaveLoad's functions pass it as their error out-parameter, and the
// game and the scenario editor read it; provisional name)
UWideString gLoadSaveErrorMessage;

// SYNTHETIC: EXE 0x54c6b1
// SYNTHETIC: EXE 0x54c6bb
// SYNTHETIC: EXE 0x54c6cc
// SYNTHETIC: EXE 0x54c6d8
// SYNTHETIC: EXE 0x54c6e3
// SYNTHETIC: EXE 0x54c6ed
// SYNTHETIC: EXE 0x54c6fe
// SYNTHETIC: EXE 0x54c70a
// SYNTHETIC: EXE 0x54c715
// SYNTHETIC: EXE 0x54c71f
// SYNTHETIC: EXE 0x54c72a
// SYNTHETIC: EXE 0x54c736

#include <time.h>
#include <vector>
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "ee/EmpireEarth.h"
#include "ee/EESaveLoad.h"
#include "ee/EESaveGameFiles.h"

// FUNCTION: EXE 0x54c741
// Returns a save file's name with its last three characters replaced by "txt", or unchanged when asked to keep it
UString Fn14C741(const UString& inName, bool inKeep)
{
    UString theName;
    if (!inKeep) {
        theName.Copy((const char*)inName, 0, inName.GetLength() - 3, 0, false);
        theName += "txt";
    } else {
        theName += inName;
    }
    return theName;
}

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
#include "lle/TSEvent.h"
#include "lle/SEConductor.h"
#include "lle/SEMixer.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/TSCriticalSection.h"
#include "LostDebug.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EECommunications.h"
#include "ee/EEGameSound.h"
#include "ee/EEUIFMainGame.h"
#include "lle/FSMemoryFile.h"
#include "lle/FSDebugFile.h"
#include "ee/EEDataPreLoading.h"
#include "ee/EEUIFScenarioEditor.h"
#include "lle/FSChunk.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EEGameStateLoader.h"
void EEReportLoadFailure(const UString& inMessage, bool inArg, ESChecksum* inChecksum);  // 0x148362 (EEWorld.cpp's prototype)
#include "ee/EEPersistentTemplates.h"
#include "ee/EEPChecksum.h"
#include "ee/EEPPlayerHelper.h"
#include "ee/EEPWorldHelper.h"
#include "ee/EEPMultiplayerHelper.h"
#include "ee/EECameraControl.h"

extern unsigned long gMaxPlayers;       // 0x5318cc (EESERBUnitTypes.h's declaration; the save header's mL4E8)

// FUNCTION: EXE 0x54c7a7
// Builds the list of chunks a save writes, in chunk order (each chunk new; the game objects are only borrowed)
void Fn14C7A7(std::vector<FSPersistent*>& outList)
{
    GE3DPoint theLocation;
    std::vector<EETeam> theTeams;
    EEPChecksum* theChecksum = new EEPChecksum(false);
    gWorld.GetChecksum(*theChecksum);
    outList.push_back(theChecksum);
    outList.push_back(new EEPTGeneric<unsigned long>(gUserInterface->mRandomSeed, 2));
    EEPTPRestorable<EEMap>* theMap = new EEPTPRestorable<EEMap>(gWorld.GetMap(), 4);
    theMap->mOwned = false;
    outList.push_back(theMap);
    gWorld.mCameraControl.GetLocation(gWorld.mCameraControl.mFraction, theLocation);
    outList.push_back(new EEPTRestorable<GE3DPoint>(theLocation, 3));
    EEPTPRestorable<EEScenarioManager>* theScenario =
        new EEPTPRestorable<EEScenarioManager>(gWorld.mScenarioManager, 6);
    theScenario->mOwned = false;
    outList.push_back(theScenario);
    EEPTPRestorable<EEEffectManager>* theEffects = new EEPTPRestorable<EEEffectManager>(gWorld.mEffectManager, 10);
    theEffects->mOwned = false;
    outList.push_back(theEffects);
    EEPTPRestorable<EEEnvironmentManager>* theEnvironment =
        new EEPTPRestorable<EEEnvironmentManager>(gWorld.mEnvironmentManager, 12);
    theEnvironment->mOwned = false;
    outList.push_back(theEnvironment);
    outList.push_back(new EEPPlayerHelper(gWorld));
    outList.push_back(new EEPWorldHelper(gWorld));
    outList.push_back(new EEPMultiplayerHelper());
    outList.push_back(new EEPCalamityHelper());
}

// FUNCTION: EXE 0x54ca3d
// Fills a save header: name, versions, player names, date, chunk tags, scenario media strings and game options
void Fn14CA3D(const UString& inName, EEPSaveGameHeader& outHeader, std::vector<FSPersistent*>& inList)
{
    std::vector<long> theTags;
    long theTag = 0;
    UString theName;
    UString thePlayers;
    UString theDate;
    outHeader.mName = inName;
    { UString theDescription("No game description provided."); outHeader.mDescription = theDescription; }
    outHeader.mMajorVersion = kSaveGameMajorVersion;
    outHeader.mMinorVersion = kSaveGameMinorVersion;
    for (unsigned long i = 1; i < gWorld.mNumPlayers; i++) {
        theName = (const char*)gWorld.mPlayers[i]->mName;
        thePlayers += theName;
        if (i + 1 < gWorld.mNumPlayers)
            thePlayers += " ";
    }
    outHeader.mPlayerNames = thePlayers;
    theDate = Fn14CED5();
    outHeader.mDate = theDate;
    for (std::vector<FSPersistent*>::iterator theIterator = inList.begin(); theIterator != inList.end();
         theIterator++) {
        theTag = (*theIterator)->GetTagID();
        theTags.push_back(theTag);
    }
    outHeader.mChunkTypes = theTags;
    if (gWorld.mScenarioManager != NULL)
        outHeader.mMediaStrings = gWorld.mScenarioManager->mMediaStrings;
    outHeader.mGameOptions = gWorld.mGameOptions;
    outHeader.mNumPlayers = gMaxPlayers;
}

// FUNCTION: EXE 0x54cbc7
// Deletes the files matching a pattern in a directory, lowest (FSFileSpec order) first, until at most a given number
// remain
long Fn14CBC7(const UString& inPattern, const UString& inDirectory, unsigned long inMax)
{
    std::vector<FSFileSpec> theFiles;
    FSFileSpec theFile;
    FSFileSpec theDirectory;
    theDirectory.SetDirectory(inDirectory);
    for (long theResult = theDirectory.GetFirstFile(inPattern, theFile); theResult == kErrorOK;
         theResult = theDirectory.GetNextFile(theFile)) {
        unsigned long i;
        for (i = 0; i < theFiles.size(); i++)
            if (theFile < theFiles[i])
                break;
        theFiles.insert(theFiles.begin() + i, theFile);
        if (theFiles.size() > inMax) {
            theFiles[0].Delete();
            theFiles.erase(theFiles.begin());
        }
    }
    return 0;
}

// FUNCTION: EXE 0x54ccfb
// Writes each listed chunk to a file; on success deletes the chunks and empties the list; returns the first error
long Fn14CCFB(std::vector<FSPersistent*>& inList, FSFile& inFile)
{
    std::vector<FSChunk> theChunks;
    long theResult = kErrorOK;
    unsigned long i;
    theChunks.resize(inList.size());
    for (i = 0; i < inList.size() && theResult == kErrorOK; i++) {
        theResult = theChunks[i].SavePersistentToChunk(kSaveGameMajorVersion, kSaveGameMinorVersion, inList[i]);
        if (theResult != kErrorOK) break;
        theResult = theChunks[i].SaveChunkToFile(inFile);
    }
    if (theResult == kErrorOK) {
        for (i = 0; i < inList.size(); i++)
            delete inList[i];
        inList.clear();
    }
    theChunks.clear();
    return theResult;
}

// FUNCTION: EXE 0x54ce06
// Writes a whole saved game (header, then every chunk) to a file; returns the first error
long Fn14CE06(const UString& inName, unsigned long inTime, FSFile& inFile, FSFile& inOther)
{
    EEPSaveGameHeader theHeader;
    std::vector<FSPersistent*> theList;
    Fn14C7A7(theList);
    Fn14CA3D(inName, theHeader, theList);
    theHeader.mChecksum = inTime;
    long theResult = theHeader.Save(kSaveGameMajorVersion, kSaveGameMinorVersion, inFile);
    if (theResult == kErrorOK)
        theResult = Fn14CCFB(theList, inOther);
    return theResult;
}

// FUNCTION: EXE 0x54ce98
// Finds a save file by name in a directory on the data path; returns the find error
long EEOpenSaveGameFile(const UString& inName, const UString& inDirectory, FSFileSpec*& outFile)
{
    UString thePath = inDirectory + inName;
    return gDataPath->FindFile(thePath, outFile);
}

// FUNCTION: EXE 0x54ced5
// Returns the current date and time as text (asctime), or "Date/time failed."
UString Fn14CED5()
{
    UString theDate;
    time_t theTime;
    if (time(&theTime) != -1)
        theDate = asctime(localtime(&theTime));
    else
        theDate = "Date/time failed.";
    return theDate;
}
#include "ee/EEStringIDs.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
class EEGameState;
extern EEGameState gGameState;
long Fn14D195(bool inLoading, bool inQuiet);                         // 0x14d195 (mutes the sound before a save/load)
long Fn14D253(bool inLoading, bool inQuiet);                         // 0x14d253 (restores it afterwards)
long Fn14D2E3(FSFileSpec& inFile, bool inArg, EEGameState& ioState);   // 0x14d2e3 (loads the game under gLoadingSavedGame)

// FUNCTION: EXE 0x54cf33
// Validates and loads a saved game file with the sound muted (multiplayer reload or into the game state),
// optionally resuming the graphics after; returns 9 when the file is not a valid save
long Fn14CF33(FSFileSpec& inFile, bool inArg2, bool inQuiet, bool inResumeGraphics)
{
    UString theUnused;
    long theResult;
    if (Fn14D195(true, inQuiet) == 0 && EEValidateSaveGameFile(inFile, gLoadSaveErrorMessage) == true) {
        theResult = Fn14D2E3(inFile, inArg2, gGameState);
        if (theResult == 0)
            theResult = Fn14D253(true, inQuiet);
    } else {
        theResult = 9;
    }
    if (inResumeGraphics)
        gGraphics->PostResume();
    return theResult;
}

long Fn14D0E6(FSFileSpec& outFile, bool inLoading, const UString& inName, const UString& inDirectory);   // 0x14d0e6
long Fn14D34B(FSFileSpec& inFile, unsigned long inTime);              // 0x14d34b (writes the game)
long Fn14D4FD(EEGameState& ioState, FSFile& inFile);                 // 0x14d4fd
long Fn14D521(FSFile& inFile);                                       // 0x14d521

// the volumes Fn14D195 mutes and Fn14D253 restores (0x531cac, 0x531ca8; provisional names)
static float sMasterVolume;
static float sMusicVolume;

// FUNCTION: EXE 0x54cfb1
// Opens and validates a saved game from a directory and loads it into the game state with the sound muted,
// optionally resuming the graphics after; returns the first error
long EELoadSavedGameFile(const UString& inFileName, bool inFlag1, bool inFlag2, UString inDirectory, bool inArg)
{
    FSFileSpec theFile;
    UString theUnused;
    long theResult = Fn14D0E6(theFile, true, inFileName, inDirectory);
    if (theResult != kErrorOK)
        return theResult;
    if (Fn14D195(false, inFlag2) == 0 && EEValidateSaveGameFile(theFile, gLoadSaveErrorMessage) == true) {
        theResult = Fn14D2E3(theFile, inFlag1, gGameState);
        if (theResult == kErrorOK)
            theResult = Fn14D253(true, inFlag2);
    } else {
        theResult = kErrorFailed;
    }
    if (inArg)
        gUserInterface->ResumeGraphics();
    return theResult;
}

// FUNCTION: EXE 0x54d066
// Saves the game to a file in a directory with the sound muted; returns the first error
// the original reads the physics CT count into a local it never uses (0x14d080; a discarded getter call is elided)
long SaveGame(const UString& inFileName, unsigned long inTime, bool inFlag, UString inDescription)
{
    FSFileSpec theFile;
    UString theUnused;
    unsigned long theCT = gCommunications->GetNumberOfCTsProcessedInPhysics();
    long theResult = Fn14D0E6(theFile, false, inFileName, inDescription);
    if (theResult != kErrorOK)
        return theResult;
    Fn14D195(false, inFlag);
    theResult = Fn14D34B(theFile, inTime);
    Fn14D253(false, inFlag);
    return theResult;
}

// FUNCTION: EXE 0x54d0e6
// Points a file spec at the startup directory plus a subdirectory and name and opens it (recreated first for a save);
// sets the load/save error message on failure
long Fn14D0E6(FSFileSpec& outFile, bool inLoading, const UString& inName, const UString& inDirectory)
{
    long theResult = kErrorOK;
    outFile.SetVolume(gStartupVolume);
    outFile.SetDirectory(gStartupDirectory + inDirectory);
    outFile.SetFile(inName);
    unsigned long theAccess = inLoading ? 1 : 2;
    if (theAccess == 2) {
        outFile.Delete();
        theResult = outFile.Create();
        if (theResult != kErrorOK) {
            if (theResult == kErrorFailed)
                gResource->GetString(IDS_INVALID_FILE_NAME, gLoadSaveErrorMessage);
            else
                gResource->GetString(IDS_ERROR_CREATING_FILE, gLoadSaveErrorMessage);
        }
    }
    if (theResult == kErrorOK) {
        theResult = outFile.Open(theAccess);
        if (theResult != kErrorOK)
            gResource->GetString(IDS_FILE_COULDNT_BE_OPENED, gLoadSaveErrorMessage);
    }
    return theResult;
}

// FUNCTION: EXE 0x54d195
// Mutes the sound and music (keeping their volumes) and waits 642 ms before a save or load; before a save it applies
// the map's vertex colours, before a host's non-quiet load it shows the splash form
long Fn14D195(bool inLoading, bool inQuiet)
{
    if (!inLoading)
        gWorld.GetMap()->ApplyVertexColors();
    sMasterVolume = gSoundManager->GetMasterVolume();
    sMusicVolume = gConductor->GetConductor().GetVolume();
    gSoundManager->SetMasterVolume(0.0f);
    gConductor->GetConductor().SetVolume(0.0f);
    Sleep(0x282);
    if (inQuiet || gCommunications == NULL || !gCommunications->IsServer() || inLoading != true)
        return 0;
    gUIManager->LockUIForCopyData();
    gUIManager->SwapForm(gUIManager->GetFormByID(kFormSplash));
    gUIManager->UnlockUIForCopyData();
    return 0;
}

// FUNCTION: EXE 0x54d253
// Restores the sound and music volumes after a save or load; after a save it shows the selection areas again, after
// a host's non-quiet load it opens the main game form and waits for the next render
long Fn14D253(bool inLoading, bool inQuiet)
{
    if (!inLoading)
        gWorld.GetCurrentPlayer()->mActiveGroup->ShowSelectionAreas();
    gSoundManager->SetMasterVolume(sMasterVolume);
    gConductor->GetConductor().SetVolume(sMusicVolume);
    if (!inQuiet && gCommunications->IsServer() && inLoading == true) {
        gUserInterface->OpenForm(gUIManager->mMainGameForm);
        gGraphics->mRenderNow->Wait(INFINITE);
    }
    return 0;
}

// FUNCTION: EXE 0x54d2e3
// Loads a saved game file into a game state, or for a reload into the world, while the saved-game flag is set;
// sets the "save game invalid" message on failure
long Fn14D2E3(FSFileSpec& inFile, bool inArg, EEGameState& ioState)
{
    long theResult;
    gLoadingSavedGame = true;
    if (!inArg)
        theResult = Fn14D4FD(ioState, inFile);
    else
        theResult = Fn14D521(inFile);
    if (theResult != kErrorOK)
        ESVERIFY_AT(::gResource->GetString(keeslSaveGameInvalid, ::gLoadSaveErrorMessage) == keseOK, 669);
    gLoadingSavedGame = false;
    return theResult;
}

// FUNCTION: EXE 0x54d34b
// Writes the game into memory and then to the file (deleting the old text twin first) and reports "Game saved." or
// an error message; returns the first error
long Fn14D34B(FSFileSpec& inFile, unsigned long inTime)
{
    FSMemoryFile theMemory(0x400, true, 1000);
    FSDebugFile theDebugFile(&theMemory);
    UString theTextName;
    UString theName;
    UString theFile = inFile.GetFile();
    theTextName = Fn14C741(theFile, true);
    theName = Fn14C741(theFile, false);
    theDebugFile.SetVolume(inFile.GetVolume());
    theDebugFile.SetDirectory(inFile.GetDirectory());
    theDebugFile.SetFile(theName);
    theDebugFile.Delete();
    long theResult = Fn14CE06(theTextName, inTime, theMemory, theMemory);
    if (theResult == kErrorOK) {
        theResult = theMemory.Save(inFile);
        if (theResult == kErrorOK)
            theResult = inFile.Close();
    } else {
        gResource->GetString(IDS_ERROR_SAVING_FILE, gLoadSaveErrorMessage);
    }
    if (theResult == kErrorOK) {
        UString theMessage("Game saved.  ");
        EEReportLoadFailure(theMessage, false, NULL);
    } else {
        UString theMessage("Error saving game.  Please report this bug.");
        EEReportLoadFailure(theMessage, false, NULL);
    }
    return theResult;
}

// FUNCTION: EXE 0x54d4fd
// Loads a saved game file into a game state; returns 9 when it failed
long Fn14D4FD(EEGameState& ioState, FSFile& inFile)
{
    EEGameStateLoader theLoader(&ioState);
    return theLoader.Load(inFile) == true ? kErrorOK : 9;
}

// FUNCTION: EXE 0x54d521
// Loads a saved game file into the global game state and, when it worked, restarts the world from it (scenario
// editor and multiplayer reload)
long Fn14D521(FSFile& inFile)
{
    UWideString theUnused;
    gGameState.Clear();
    long theResult = Fn14D4FD(gGameState, inFile);
    if (gGameState.mIsLoadedGame && theResult == kErrorOK) {
        if (gUIManager->mScenarioEditorForm != NULL)
            gUIManager->mScenarioEditorForm->ClearSelection();
        gDataPreLoading->WaitForMessageQueue();
        gWorld.Clear();
        gWorld.StartGame(&gGameState, true);
        gGameState.Reset();
    }
    return theResult;
}

// FUNCTION: EXE 0x54d5ac
// Checks a save's players against the lobby (scenario slots for a multiplayer scenario, else every lobby player must
// be a saved one and all saved humans present); lists the problems, returns whether there are none
bool EECheckSavedGameOptions(EEGameOptions& inSaved, EEGameOptions& inCurrent, bool inFlag,
                             std::vector<UWideString>& outMissing, long inCount)
{
    std::vector<UWideString> theHumans;
    std::vector<UWideString> theComputers;
    std::vector<UWideString> theCurrent;
    UWideString theFormat;
    UWideString theMessage;
    unsigned long theOpen = 0;
    unsigned long theNeeded = 1;
    outMissing.clear();
    for (unsigned long i = 0; i < 16; i++) {
        if (inSaved.mPlayers[i].mType == 1) {
            theHumans.push_back(inSaved.mPlayers[i].mName);
        } else if (inSaved.mPlayers[i].mType == 2) {
            theComputers.push_back(inSaved.mPlayers[i].mName);
        } else if (inSaved.mPlayers[i].mType == 0) {
            theOpen++;
            if (inSaved.mPlayers[i].mB36 == false && i < inCount)
                theNeeded++;
        }
        if (inCurrent.mPlayers[i].mType == 1 || inCurrent.mPlayers[i].mType == 2)
            theCurrent.push_back(inCurrent.mPlayers[i].mName);
    }
    if (inCurrent.mLoadType == 1) {
        if (theHumans.size() != 1 || theOpen != 15) {
            gResource->GetString(IDS_THIS_IS_NOT_VALID_MULTIPLAYER_SCENARIO, theMessage);
            outMissing.push_back(theMessage);
        }
        if (theCurrent.size() > theNeeded) {
            gResource->GetString(IDS_THERE_ARE_TOO_MANY_PLAYERS_TO_PLAY, theFormat);
            sprintf(theMessage, theFormat, theNeeded);
            outMissing.push_back(theMessage);
        }
    } else if (theCurrent.size() <= theHumans.size() + theComputers.size()) {
        unsigned long theFound = 0;
        unsigned long j = 0;
        while (true) {
            if (j >= theCurrent.size())
                break;
            if (std::find(theHumans.begin(), theHumans.end(), theCurrent[j]) != theHumans.end()) {
                theFound++;
            } else if (std::find(theComputers.begin(), theComputers.end(), theCurrent[j])
                       == theComputers.end()) {
                gResource->GetString(IDS_PLAYER_DOES_NOT_EXIST_IN_SAVED, theFormat);
                sprintf(theMessage, theFormat, (const char*)theCurrent[j]);
                outMissing.push_back(theMessage);
                inFlag = false;
            }
            j++;
        }
        if (inFlag == true && theFound != theHumans.size()) {
            gResource->GetString(IDS_THIS_SAVED_GAME_REQUIRES_PLAYERS, theFormat);
            sprintf(theMessage, theFormat, theHumans.size(), theFound);
            outMissing.push_back(theMessage);
        }
    } else {
        gResource->GetString(IDS_THERE_ARE_TOO_MANY_PLAYERS_IN_LOBBY, theMessage);
        outMissing.push_back(theMessage);
    }
    return outMissing.size() == 0;
}

extern char gU4E5B98[];                 // 0x4e5b98 (the empty string; EECampaignProgress.cpp's declaration)
static bool Fn14DA7C(EEPSaveGameHeader& inHeader, UWideString& outErrorMessage, FSFile& inFile);

// FUNCTION: EXE 0x54d946
// Returns whether a file holds a save game this build can load; on failure sets the error message (always the generic
// "save game invalid" once the file opened)
bool EEValidateSaveGameFile(FSFile& inFile, UWideString& outErrorMessage)
{
    EEPSaveGameHeader theHeader;
    std::vector<long> theTags;
    FSMemoryFile theMemory(1000, true, 1);
    bool theResult = false;
    outErrorMessage = gU4E5B98;
    inFile.Close();
    if (inFile.Open(kFileAccessRead) == kErrorOK) {
        if (theMemory.Load(inFile) == kErrorOK
            && theHeader.Load(kSaveGameMajorVersion, kSaveGameMinorVersion, theMemory) == kErrorOK
            && Fn14DA7C(theHeader, outErrorMessage, theMemory)) {
            theResult = true;
            inFile.Close();
            inFile.Open(kFileAccessRead);
        } else {
            ESVERIFY_AT(::gResource->GetString(keeslSaveGameInvalid, outErrorMessage) == keseOK, 967);
        }
    } else {
        gResource->GetString(IDS_FILE_COULDNT_BE_OPENED, outErrorMessage);
    }
    return theResult;
}

static long Fn14DC54(std::vector<long>& inTags, long inTag);

// FUNCTION: EXE 0x54da7c
// Checks a save header's version and chunk tags (each of 1..7, 10 and 11 once, no 0) and that every chunk's size is
// readable and sane; sets outErrorMessage on failure (it wrote the global message,
// which only worked because every caller passed it)
static bool Fn14DA7C(EEPSaveGameHeader& inHeader, UWideString& outErrorMessage, FSFile& inFile)
{
    std::vector<long> theTags;
    bool theResult = false;
    long theError = kErrorOK;
    unsigned long theSize;
    if (inHeader.mMajorVersion <= kSaveGameMajorVersion && inHeader.mMinorVersion <= kSaveGameMinorVersion) {
        theTags = inHeader.mChunkTypes;
        if (Fn14DC54(theTags, 0) == 0) {
            if (Fn14DC54(theTags, 1) == 1 && Fn14DC54(theTags, 2) == 1 && Fn14DC54(theTags, 3) == 1
                && Fn14DC54(theTags, 4) == 1 && Fn14DC54(theTags, 5) == 1 && Fn14DC54(theTags, 6) == 1
                && Fn14DC54(theTags, 7) == 1 && Fn14DC54(theTags, 10) == 1 && Fn14DC54(theTags, 11) == 1) {
                for (unsigned long i = 0; i < theTags.size() && theError == kErrorOK; i++) {
                    FSChunk theChunk;
                    theError = theChunk.LoadChunkHeader(inFile);
                    if (theError == kErrorOK) {
                        if (theChunk.GetChunkSize(theSize)) {
                            if (theSize <= 0x80000000)
                                theError = theChunk.SkipChunk(inFile);
                            else
                                theError = 0xe;
                        } else
                            theError = 3;
                    }
                }
                if (theError == kErrorOK)
                    theResult = true;
            } else {
                gResource->GetString(IDS_SAVE_GAME_IS_MISSING_DATA_AND_CANNOT, outErrorMessage);
            }
        } else {
            gResource->GetString(IDS_SAVE_GAME_CONTAINS_INVALID_DATA, outErrorMessage);
        }
    } else {
        gResource->GetString(IDS_SAVE_GAME_INCOMPATIBLE_WITH_THIS_VERSION, outErrorMessage);
    }
    return theResult;
}

// FUNCTION: EXE 0x54dc54
// Counts how many tags in a list equal a given tag
static long Fn14DC54(std::vector<long>& inTags, long inTag)
{
    long theCount = 0;
    for (std::vector<long>::iterator theIterator = inTags.begin(); theIterator != inTags.end(); theIterator++)
        if (*theIterator == inTag)
            theCount++;
    return theCount;
}

// TEMPLATE: EXE 0x54dc77
// ?erase@?$vector@VFSFileSpec@@V?$allocator@VFSFileSpec@@@std@@@std@@QAE?AV?$_Ptrit@VFSFileSpec@@HPAV1@AAV1@PAV1@AAV1@@2@V32@@Z
// TEMPLATE: EXE 0x54dceb
// ?push_back@?$vector@PAVFSPersistent@@V?$allocator@PAVFSPersistent@@@std@@@std@@QAEXABQAVFSPersistent@@@Z
// TEMPLATE: EXE 0x54dd17
// ?resize@?$vector@VFSChunk@@V?$allocator@VFSChunk@@@std@@@std@@QAEXI@Z
// TEMPLATE: EXE 0x54e1f6
// ?find@std@@YA?AV?$_Ptrit@VUWideString@@HPAV1@AAV1@PAV1@AAV1@@1@V21@0ABVUWideString@@@Z
// TEMPLATE: EXE 0x54e21f
// ?copy@std@@YA?AV?$_Ptrit@VFSFileSpec@@HPAV1@AAV1@PAV1@AAV1@@1@V21@00@Z
// TEMPLATE: EXE 0x54e30c
// ??4EESMMediaStrings@@QAEAAV0@ABV0@@Z
// TEMPLATE: EXE 0x54e35c
// ?push_back@?$vector@VUWideString@@V?$allocator@VUWideString@@@std@@@std@@QAEXABVUWideString@@@Z
// TEMPLATE: EXE 0x54e388
// ?clear@?$vector@VUWideString@@V?$allocator@VUWideString@@@std@@@std@@QAEXXZ
// TEMPLATE: EXE 0x54e3c3
// ?insert@?$vector@VUWideString@@V?$allocator@VUWideString@@@std@@@std@@QAE?AV?$_Ptrit@VUWideString@@HPAV1@AAV1@PAV1@AAV1@@2@V32@ABVUWideString@@@Z
// TEMPLATE: EXE 0x54e41d
// ?erase@?$vector@VUWideString@@V?$allocator@VUWideString@@@std@@@std@@QAE?AV?$_Ptrit@VUWideString@@HPAV1@AAV1@PAV1@AAV1@@2@V32@0@Z
// TEMPLATE: EXE 0x54e488
// ?insert@?$vector@VUWideString@@V?$allocator@VUWideString@@@std@@@std@@QAEXV?$_Ptrit@VUWideString@@HPAV1@AAV1@PAV1@AAV1@@2@IABVUWideString@@@Z
// TEMPLATE: EXE 0x54e764
// ?_Ufill@?$vector@VUWideString@@V?$allocator@VUWideString@@@std@@@std@@IAEPAVUWideString@@PAV3@IABV3@@Z
// TEMPLATE: EXE 0x54e7ac
// ?copy@std@@YAPAVUWideString@@V?$_Ptrit@VUWideString@@HPAV1@AAV1@PAV1@AAV1@@1@0PAV2@@Z
// TEMPLATE: EXE 0x54e7d1
// ?fill@std@@YAXV?$_Ptrit@VUWideString@@HPAV1@AAV1@PAV1@AAV1@@1@0ABVUWideString@@@Z
// TEMPLATE: EXE 0x54e7ef
// ?copy_backward@std@@YA?AV?$_Ptrit@VUWideString@@HPAV1@AAV1@PAV1@AAV1@@1@V21@00@Z
// TEMPLATE: EXE 0x54e81c
// ?_Construct@std@@YAXPAVUWideString@@ABV2@@Z

// TEMPLATE: EXE 0x54dd6f
// ?resize@?$vector@VFSChunk@@V?$allocator@VFSChunk@@@std@@@std@@QAEXIVFSChunk@@@Z
// TEMPLATE: EXE 0x54de2c
// ?erase@?$vector@VFSChunk@@V?$allocator@VFSChunk@@@std@@@std@@QAE?AV?$_Ptrit@VFSChunk@@HPAV1@AAV1@PAV1@AAV1@@2@V32@0@Z
// TEMPLATE: EXE 0x54ded0
// ?insert@?$vector@VFSChunk@@V?$allocator@VFSChunk@@@std@@@std@@QAEXV?$_Ptrit@VFSChunk@@HPAV1@AAV1@PAV1@AAV1@@2@IABVFSChunk@@@Z
// TEMPLATE: EXE 0x54e1a8
// ?_Ufill@?$vector@VFSChunk@@V?$allocator@VFSChunk@@@std@@@std@@IAEPAVFSChunk@@PAV3@IABV3@@Z
// TEMPLATE: EXE 0x54e249
// ?copy@std@@YAPAVFSChunk@@V?$_Ptrit@VFSChunk@@HPAV1@AAV1@PAV1@AAV1@@1@0PAV2@@Z
// TEMPLATE: EXE 0x54e26e
// ?_Ucopy@?$vector@VFSChunk@@V?$allocator@VFSChunk@@@std@@@std@@IAEPAVFSChunk@@V?$_Ptrit@VFSChunk@@HPAV1@AAV1@PAV1@AAV1@@2@0PAV3@@Z
// TEMPLATE: EXE 0x54e2a4
// ?fill@std@@YAXV?$_Ptrit@VFSChunk@@HPAV1@AAV1@PAV1@AAV1@@1@0ABVFSChunk@@@Z
// TEMPLATE: EXE 0x54e2c2
// ?copy_backward@std@@YA?AV?$_Ptrit@VFSChunk@@HPAV1@AAV1@PAV1@AAV1@@1@V21@00@Z
// TEMPLATE: EXE 0x54e2ef
// ?_Construct@std@@YAXPAVFSChunk@@ABV2@@Z
// TEMPLATE: EXE 0x54e795
// ??G?$_Ptrit@VUWideString@@HPAV1@AAV1@PAV1@AAV1@@std@@QBE?AV01@H@Z
// TEMPLATE: EXE 0x54dd34
// ?clear@?$vector@VFSChunk@@V?$allocator@VFSChunk@@@std@@@std@@QAEXXZ
// TEMPLATE: EXE 0x54de97
// ?_Tidy@?$vector@VFSChunk@@V?$allocator@VFSChunk@@@std@@@std@@IAEXXZ
// TEMPLATE: EXE 0x54e1d9
// ??_GFSChunk@@UAEPAXI@Z
