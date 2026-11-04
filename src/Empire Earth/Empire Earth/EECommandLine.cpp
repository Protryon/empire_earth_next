// C:\EEX\Empire Earth\Empire Earth\EECommandLine.cpp (file name inferred: no __FILE__, no class of its own; the
// command-line parsers Empire Earth.cpp calls (0x12d9e5 before the movies, 0x12d703 after startup) with their
// commands "hostgame", "joinbyip", "update_saved_games", "loadgame", "balancer"; placed between EECivilization.cpp
// and the storm unit EEEMStorm.cpp (0x12dba8..), so the name sorts between them). 0x12cac8..0x12dba8: four file
// static UString $E chains ("-", "/t", "\"", "Text-"), loadgame, the saved game / scenario conversion, the command
// matcher, hostgame / joinbyip, EEUIEMSEMultiPlayer's ??_G, the parsers and vector<UString>::clear's COMDATs.
// Unit flags: the exe module set.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mbstring.h>
#include <algorithm>
#include <string>
#include <list>

#include "lle/LowLevelEngine.h"
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
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/NESynchronousClient.h"
#include "lle/NESynchronousServer.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EmpireEarth.h"
#include "ee/EECommunications.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEMessage.h"
#include "ee/EEServer.h"
#include "ee/EEClient.h"
#include "ee/EEAction.h"
#include "ee/EEAJoinOrHostAGame.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIEMainScreenEvents.h"
#include "ee/EECameraControl.h"
#include "ee/EESaveLoad.h"
#include "ee/EESaveGameFiles.h"

extern bool gGameStateIsMultiplayer;        // 0x52fa88 (cleared when a local server starts; provisional name)
long EELoadSavedGameFile(const UString& inFileName, bool inFlag1, bool inFlag2, UString inDirectory, bool inArg);  // 0x14cfb1
void Fn1030AB();                            // 0x1030ab (EEEffect.cpp)

// The four file statics (0x529ad0, 0x529ac0, 0x529ab0, 0x529aa0)
// chain at 0x52cac8 (wrapper)
//   object 0x929ad0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
static UString sDash("-");
// SYNTHETIC: EXE 0x52cac8   (wrapper)
// SYNTHETIC: EXE 0x52cad2   (ctor)
// SYNTHETIC: EXE 0x52cae3   (registrar)
// SYNTHETIC: EXE 0x52caef   (dtor)

// chain at 0x52cafa (wrapper)
//   object 0x929ac0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
static UString sTab("/t");
// SYNTHETIC: EXE 0x52cafa   (wrapper)
// SYNTHETIC: EXE 0x52cb04   (ctor)
// SYNTHETIC: EXE 0x52cb15   (registrar)
// SYNTHETIC: EXE 0x52cb21   (dtor)

// chain at 0x52cb2c (wrapper)
//   object 0x929ab0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
static UString sQuote("\"");
// SYNTHETIC: EXE 0x52cb2c   (wrapper)
// SYNTHETIC: EXE 0x52cb36   (ctor)
// SYNTHETIC: EXE 0x52cb47   (registrar)
// SYNTHETIC: EXE 0x52cb53   (dtor)

// chain at 0x52cb5e (wrapper)
//   object 0x929aa0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
static UString sText("Text-");
// SYNTHETIC: EXE 0x52cb5e   (wrapper)
// SYNTHETIC: EXE 0x52cb68   (ctor)
// SYNTHETIC: EXE 0x52cb79   (registrar)
// SYNTHETIC: EXE 0x52cb85   (dtor)


// The balancer command's target: compiled away in the shipping build (an empty cdecl body ICF-folded into 0xe123d,
// no marker) (provisional name).
void EERunBalancer(const UString& inFile)
{
}

// FUNCTION: EXE 0x52cb90
// "loadgame": starts a saved game on a local server, or falls back to a client, then opens the main menu
long EELoadGameFromCommandLine(const UString& inName)
{
    FSFileSpec* theFile = NULL;
    if (gCommunications != NULL) {
        delete gCommunications;
        // Cleared so the second delete below does not free it again.
        gCommunications = NULL;
    }
    long theResult = EEOpenSaveGameFile(inName, "Saved Games\\", theFile);
    if (theResult == kErrorOK) {
        gGameOptions.mLock->WriteLock();
        gGameOptions.SetDefaults();
        gGameOptions.mLoadType = 2;
        { UWideString theTemp((const char*)theFile->GetFile()); gGameOptions.mMapType = theTemp; }
        gGameOptions.mLock->WriteUnlock();
        if (theFile != NULL)
            delete theFile;
        gGameStateIsMultiplayer = false;
        if (gCommunications != NULL)
            delete gCommunications;
        gCommunications = new EEServer(UWideString("EEName"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, true, true);
        static_cast<EEServer*>(gCommunications)->StartThreads();
        static_cast<EEServer*>(gCommunications)->mNumLoadResponses = 0;
        gGameStateLoadType = 2;
        static_cast<EEServer*>(gCommunications)->StopPing();
        static_cast<EEServer*>(gCommunications)->mLoadState = 1;
        gUIManager->mInScenarioEditor = false;
        gCommunications->ProcessLocalUserInput(new EEMStartLoading(1, TMGetTime()), kSlotNone);
    } else {
        gCommunications = new EEClient;
        static_cast<EEClient*>(gCommunications)->StartThreads();
    }
    gUIManager->Push(gUIManager->GetFormByID(kFormMainMenu));
    return 0;
}

// FUNCTION: EXE 0x52cda7
// Loads one saved game or scenario from a directory and saves it again under a new name; returns the first error
long EEConvertGameFile(const UString& inDirectory, const UString& inFile, const UString& inNewFile)
{
    long theResult = EELoadSavedGameFile(inFile, true, false, inDirectory, 0);
    if (theResult != 0)
        return theResult;
    gWorld.mCameraControl.SetChanged();
    gWorld.mCameraControl.Update(1.0f);
    theResult = SaveGame(inNewFile, TMGetTime(), false, inDirectory);
    return theResult;
}

// FUNCTION: EXE 0x52ce0c
// "update_saved_games": converts every saved game and scenario to "UPDATED <name>", logging to "saved game
// conversion.log", and deletes the originals (even when a conversion failed)
void EEUpdateSavedGames()
{
    FSFileSpec theLog;
    FSFileSpec theSearch;
    FSFileSpec theFile;
    std::vector<UString> theNames;
    gWorld.mLock->WriteLock();
    Fn1030AB();
    if (gCommunications != NULL)
        delete gCommunications;
    gCommunications = new EEServer(UWideString("EEName"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, true, true);
    static_cast<EEServer*>(gCommunications)->StartThreads();
    TMTime theTime;
    TMGetTimeOfDay(theTime);
    theLog.SetVolume(gStartupVolume);
    theLog.SetDirectory(gStartupDirectory);
    theLog.SetFile("saved game conversion.log");
    theLog.Delete();
    theLog.Create();
    if (theLog.FOpen("w") == 0) {
        FILE* theStream = theLog;
        fprintf(theStream, "Saved game conversion started on %02d/%02d/%02d, %02d:%02d.%02d\n", theTime.mMonth + 1,
                theTime.mDay, theTime.mYear, theTime.mHour, theTime.mMinute, theTime.mSecond);
        fflush(theStream);

        theNames.clear();
        theSearch.SetVolume(gStartupVolume);
        theSearch.SetDirectory(gStartupDirectory + "Data\\Saved Games\\");
        long theStatus = theSearch.GetFirstFile("*.ees", theFile);
        if (theStatus == 0) {
            do {
                if (theFile.GetFile().Find("UPDATED ", 0) != 0)
                    theNames.push_back(theFile.GetFile());
            } while (theSearch.GetNextFile(theFile) == 0);
        }
        for (unsigned long i = 0; i < theNames.size(); i++) {
            fprintf(theStream, "Beginning conversion of file %s... ", (const char*)theNames[i]);
            fflush(theStream);
            {
                UString thePrefix("UPDATED ");
                UString theDirectory("Data\\Saved Games\\");
                theStatus = EEConvertGameFile(theDirectory, theNames[i], thePrefix + theNames[i]);
            }
            if (theStatus == 0) fprintf(theStream, "success!\n"); else fprintf(theStream, "failed!\n");
            fflush(theStream);
            theFile.SetVolume(gStartupVolume);
            theFile.SetDirectory(gStartupDirectory + "Data\\Saved Games\\");
            theFile.SetFile(theNames[i]);
            theFile.Delete();
        }

        theNames.clear();
        theSearch.SetVolume(gStartupVolume);
        theSearch.SetDirectory(gStartupDirectory + "Data\\Scenarios\\");
        theStatus = theSearch.GetFirstFile("*.scn", theFile);
        if (theStatus == 0) {
            do {
                if (theFile.GetFile().Find("UPDATED ", 0) != 0)
                    theNames.push_back(theFile.GetFile());
            } while (theSearch.GetNextFile(theFile) == 0);
        }
        for (unsigned long j = 0; j < theNames.size(); j++) {
            fprintf(theStream, "Beginning conversion of file %s... ", (const char*)theNames[j]);
            fflush(theStream);
            {
                UString thePrefix("UPDATED ");
                UString theDirectory("Data\\Scenarios\\");
                theStatus = EEConvertGameFile(theDirectory, theNames[j], thePrefix + theNames[j]);
            }
            if (theStatus == 0) fprintf(theStream, "success!\n"); else fprintf(theStream, "failed!\n");
            fflush(theStream);
            theFile.SetVolume(gStartupVolume);
            theFile.SetDirectory(gStartupDirectory + "Data\\Scenarios\\");
            theFile.SetFile(theNames[j]);
            theFile.Delete();
        }

        fprintf(theStream, "Conversion completed!\n");
        fflush(theStream);
        theLog.Close();
    }
    gWorld.mLock->WriteUnlock();
}

// FUNCTION: EXE 0x52d3a4
// Returns whether an argument starts with a command (case-insensitively: the argument is lowered first)
bool EEIsCommand(UString inArgument, UString inCommand)
{
    bool theResult = false;
    unsigned long theLength = inCommand.GetLength();
    UString theStart;
    inArgument.ToLower();
    theStart = inArgument.GetSubstring(0, theLength);
    if (theStart.Equals(inCommand, theLength))
        theResult = true;
    return theResult;
}

// FUNCTION: EXE 0x52d414
// "hostgame": hosts a multiplayer game with the given game name as the given player
bool EEHostGameFromCommandLine(const UString& inPlayerName, const UString& inGameName)
{
    if (gCommunications != NULL) {
        delete gCommunications;
        gCommunications = NULL;
    }
    gCommunications = new EEClient;
    static_cast<EEClient*>(gCommunications)->StartThreads();
    gPlayerName = UWideString((const char*)inPlayerName);
    gGamePreferences.mSettings.mHostGameName = UWideString((const char*)inGameName);
    gUIManager->Push(gUIManager->GetFormByID(kFormMainMenu));
    EEUIEMSEMultiPlayer* theEvent = new EEUIEMSEMultiPlayer;
    theEvent->ProcessEvent(gUIManager->GetFormByID(kFormMainMenu), NULL);
    theEvent->Dereference();
    EEAction* theAction = new EEAJoinOrHostAGame(0, UWideString((const char*)inPlayerName),
                                                 UWideString((const char*)inGameName));
    gUserInterface->PostAction(theAction);
    theAction->Dereference();
    return true;
}

// FUNCTION: EXE 0x52d5a5
// "joinbyip": joins the game at an address as the given player, trying a given number of times
bool EEJoinByIPFromCommandLine(const UString& inPlayerName, const UString& inAddress, long inTrys)
{
    if (gCommunications != NULL) {
        delete gCommunications;
        gCommunications = NULL;
    }
    gCommunications = new EEClient;
    static_cast<EEClient*>(gCommunications)->SetMaxNumConnectionTrys(inTrys);
    static_cast<EEClient*>(gCommunications)->StartThreads();
    gPlayerName = UWideString((const char*)inPlayerName);
    gUIManager->Push(gUIManager->GetFormByID(kFormMainMenu));
    EEUIEMSEMultiPlayer* theEvent = new EEUIEMSEMultiPlayer;
    theEvent->ProcessEvent(gUIManager->GetFormByID(kFormMainMenu), NULL);
    theEvent->Dereference();
    EEAction* theAction = new EEAJoinOrHostAGame(2, UWideString((const char*)inPlayerName),
                                                 UWideString((const char*)inAddress));
    gUserInterface->PostAction(theAction);
    theAction->Dereference();
    return true;
}

// FUNCTION: EXE 0x52d703
// Runs the command-line commands after startup; without one, starts a client and opens the main menu. Returns
// false when the game should quit (update_saved_games)
bool EEParseCommandLine(const char* inCommandLine)
{
    std::vector<UString> theArguments;
    std::vector<UString> theTokens;
    UString theLine(inCommandLine);
    UString theArgument;
    bool theResult = true;
    bool theNoCommand = true;
    if (theLine.NestedTokenize(sDash, UString("\"\""), theArguments)) {
        for (std::vector<UString>::iterator theIt = theArguments.begin(); theIt != theArguments.end(); theIt++) {
            theArgument = *theIt;
            if (EEIsCommand(theArgument, UString("hostgame"))) {
                theArgument.Tokenize(sQuote, theTokens);
                if (theTokens.size() == 4) {
                    theNoCommand = false;
                    EEHostGameFromCommandLine(theTokens[1], theTokens[3]);
                }
            } else if (EEIsCommand(theArgument, UString("joinbyip"))) {
                theArgument.Tokenize(sQuote, theTokens);
                if (theTokens.size() == 6) {
                    theNoCommand = false;
                    EEJoinByIPFromCommandLine(theTokens[1], theTokens[3], atoi(theTokens[5]));
                }
            } else if (EEIsCommand(theArgument, UString("update_saved_games"))) {
                EEUpdateSavedGames();
                theResult = false;
                theNoCommand = false;
            } else if (EEIsCommand(theArgument, UString("loadgame"))) {
                theArgument.Tokenize(sQuote, theTokens);
                if (theTokens.size() == 2) {
                    EELoadGameFromCommandLine(theTokens[1]);
                    theNoCommand = false;
                }
            } else if (EEIsCommand(theArgument, UString("balancer"))) {
                theArgument.Tokenize(sQuote, theTokens);
                if (theTokens.size() == 2) {
                    EERunBalancer(theTokens[1]);
                    theNoCommand = false;
                }
            }
        }
    }
    if (theNoCommand == true) {
        if (gCommunications != NULL)
            delete gCommunications;
        gCommunications = new EEClient;
        static_cast<EEClient*>(gCommunications)->StartThreads();
        gUIManager->Push(gUIManager->GetFormByID(kFormMainMenu));
    }
    return theResult;
}

// FUNCTION: EXE 0x52d9e5
// Scans the command line at startup: no intro movies when it hosts or joins a game; always returns true
bool EEParseCommandLineFirstPass(const char* inCommandLine, bool& outShowMovies)
{
    std::vector<UString> theArguments;
    std::vector<UString> theTokens;
    UString theLine(inCommandLine);
    UString theArgument;
    outShowMovies = true;
    if (!theLine.Tokenize(sDash, theArguments))
        return true;
    for (std::vector<UString>::iterator theIt = theArguments.begin(); theIt != theArguments.end(); theIt++) {
        theArgument = *theIt;
        if (EEIsCommand(theArgument, UString("hostgame")))
            outShowMovies = false;
        else if (EEIsCommand(theArgument, UString("joinbyip")))
            outShowMovies = false;
    }
    return true;
}

// The deferred COMDATs: EEMStartLoading ctor, EEUIEMSEMultiPlayer ??_G, vector<UString>::clear/erase, copy
// TEMPLATE: EXE 0x52cd73
// TEMPLATE: EXE 0x52d57a
// TEMPLATE: EXE 0x52dadd
// TEMPLATE: EXE 0x52db18
// TEMPLATE: EXE 0x52db83
