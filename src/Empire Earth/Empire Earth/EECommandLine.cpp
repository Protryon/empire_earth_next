// C:\EEX\Empire Earth\Empire Earth\EECommandLine.cpp (file name inferred: no __FILE__, no class of its own; the
// command-line parser Empire Earth.cpp calls after startup (0x12d703; the first pass, 0x12d9e5, only chose whether
// to play the intro movies and is gone with them) with their
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
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousClient.h"
#include "lle/NESynchronousServer.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EmpireEarth.h"
#include "ee/EECommunications.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEMessage.h"
#include "ee/EEServer.h"
#include "ee/EEClient.h"
#include "ee/EEARust.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEScreenEvent.h"
#include "rust/game/screens.h"   // EE_SCREENS_MAIN_MULTI_PLAYER (ee_screens menus.rs)
#include "ee/EESaveLoad.h"
#include "ee/EESaveGameFiles.h"
#include "rust/game/gamecpp.h"

extern bool& gGameStateIsMultiplayer;        // 0x52fa88 (cleared when a local server starts; provisional name)
long EELoadSavedGameFile(const UString& inFileName, bool inFlag1, bool inFlag2, UString inDirectory, bool inArg);
void CreatePlaneModels();                   // 0x1030ab (EEEffect.cpp)

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

// "update_saved_games": converts every saved game and scenario to "UPDATED <name>", logging to "saved game
// conversion.log", and deletes each original once its conversion succeeded
void EEUpdateSavedGames()
{
    FSFileSpec theLog;
    FSFileSpec theSearch;
    FSFileSpec theFile;
    std::vector<UString> theNames;
    gWorld.mLock->WriteLock();
    CreatePlaneModels();
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
        fprintf(theStream, "Saved game conversion started on %02ld/%02ld/%02ld, %02ld:%02ld.%02ld\n", theTime.mMonth + 1,
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
            if (theStatus != 0)
                continue;   // keep the original when its conversion failed (it was deleted either way)
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
            if (theStatus != 0)
                continue;   // keep the original when its conversion failed (it was deleted either way)
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

// Runs the command-line commands after startup; without one, starts a client and opens the main menu. Returns
// false when the game should quit (update_saved_games) (ee_game gamecpp entry.rs; the original's four file statics
// "-", "/t", "\"", "Text-" were its tokenizer's)
bool EEParseCommandLine(const char* inCommandLine)
{
    return ee_gamecpp_entry_parse_command_line(inCommandLine);
}

// ------------------------------------------------------------------------- the command line's C++ (gamecpp entry.rs)

#define P64(x) ((int64_t)(intptr_t)(x))
#define SPEC ((FSFileSpec*)p)
#define SERVER static_cast<EEServer*>(gCommunications)

// The command line's C++ calls and members (EeGameCpp's lane E section; Empire Earth.cpp's EECore9OpE default): one
// statement each; the end of the chain
int64_t EECore10OpA(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // round 10 (EEGraphics.cpp)

int64_t EECommandLineOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::EntryNewClient: gCommunications = new EEClient;
        break;
    case O::EntryClientSetTries: static_cast<EEClient*>(gCommunications)->SetMaxNumConnectionTrys((long)a[0]);
        break;
    case O::EntryClientStartThreads: static_cast<EEClient*>(gCommunications)->StartThreads();
        break;
    case O::EntryDeleteCommunicationsKeep: delete gCommunications;
        break;
    case O::EntryNullCommunications: gCommunications = NULL;
        break;
    case O::EntrySetPlayerName: gPlayerName = UWideString((const char*)a[0]);
        break;
    case O::EntrySetHostGameName: gGamePreferences.mSettings.mHostGameName = UWideString((const char*)a[0]);
        break;
    case O::EntryPushMainMenu: gUIManager->Push(gUIManager->GetFormByID(kFormMainMenu));
        break;
    case O::EntryNewMultiPlayerEvent: return P64(EECreateScreenEvent(EE_SCREENS_MAIN_MULTI_PLAYER));
    case O::EntryProcessOnMainMenu: ((UIEvent*)p)->ProcessEvent(gUIManager->GetFormByID(kFormMainMenu), NULL);
        break;
    case O::EntryDereferenceEvent: ((UIEvent*)p)->Dereference();
        break;
    case O::EntryNewJoinOrHost: return P64(static_cast<EEAction*>(new EEAJoinOrHostAGame((long)a[0], UWideString((const char*)a[1]), UWideString((const char*)a[2]))));
    case O::EntryPostAction: gUserInterface->PostAction((EEAction*)p);
        break;
    case O::EntryDereferenceAction: ((EEAction*)p)->Dereference();
        break;
    case O::EntryOpenSavedGame: return EEOpenSaveGameFile(UString((const char*)a[0]), "Saved Games\\", *(FSFileSpec**)a[1]);
    case O::EntryOptionsWriteLock: gGameOptions.mLock->WriteLock();
        break;
    case O::EntryOptionsWriteUnlock: gGameOptions.mLock->WriteUnlock();
        break;
    case O::EntryOptionsLoadType: gGameOptions.mLoadType = (long)a[0];
        break;
    case O::EntryOptionsMapTypeOf: gGameOptions.mMapType = UWideString((const char*)SPEC->GetFile());
        break;
    case O::EntryNotMultiplayer: gGameStateIsMultiplayer = false;
        break;
    case O::EntryNewServer: gCommunications = new EEServer(UWideString("EEName"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, true, true);
        break;
    case O::EntryServerStartThreads: SERVER->StartThreads();
        break;
    case O::EntryServerNoLoadResponses: SERVER->mNumLoadResponses = 0;
        break;
    case O::EntrySetGameStateLoadType: gGameStateLoadType = (long)a[0];
        break;
    case O::EntryServerStopPing: SERVER->StopPing();
        break;
    case O::EntryServerLoadState: SERVER->mLoadState = (long)a[0];
        break;
    case O::EntryNotInEditor: gUIManager->mInScenarioEditor = false;
        break;
    case O::EntryStartLoadingInput: gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageStartLoading, {1, TMGetTime()}), kSlotNone);
        break;
    case O::EntryUpdateSavedGames: EEUpdateSavedGames();
        break;
    default: return EECore10OpA(inOp, p, a, f);
    }
    return 0;
}
