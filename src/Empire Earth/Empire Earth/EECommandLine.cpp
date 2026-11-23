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

// (EEConvertGameFile and EEUpdateSavedGames, "update_saved_games": ee_game gamecpp saved_games.rs over EECore10OpE2)

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
    default: return EECore10OpA(inOp, p, a, f);
    }
    return 0;
}

// ------------------------------------------------------------------------- round 10, lane E: saved_games.rs's C++

#define USTRA(k) (*(UString*)a[k])

int64_t EECore10OpE3(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEWorld.cpp

// the C++ calls and members of "update_saved_games" (gamecpp saved_games.rs): one statement each
int64_t EECore10OpE2(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::UsgWorldWriteLock: gWorld.mLock->WriteLock();
        break;
    case O::UsgWorldWriteUnlock: gWorld.mLock->WriteUnlock();
        break;
    case O::UsgSetDirectory: SPEC->SetDirectory(gStartupDirectory + (const char*)a[0]);
        break;
    case O::UsgSetFile: SPEC->SetFile((const char*)a[0]);
        break;
    case O::UsgDelete: SPEC->Delete();
        break;
    case O::UsgCreate: SPEC->Create();
        break;
    case O::UsgFOpenWrite: return SPEC->FOpen("w");
    case O::UsgPuts: fputs((const char*)a[0], (FILE*)*SPEC);
        break;
    case O::UsgFlush: fflush((FILE*)*SPEC);
        break;
    case O::UsgClose: SPEC->Close();
        break;
    case O::UsgFirstFile: return SPEC->GetFirstFile((const char*)a[0], *(FSFileSpec*)a[1]);
    case O::UsgFileName: return P64((const char*)SPEC->GetFile());
    case O::UsgLoad: return EELoadSavedGameFile((const char*)a[0], true, false, (const char*)a[1], 0);
    case O::UsgCameraChanged: gWorld.mCameraControl.SetChanged();
        break;
    case O::UsgCameraUpdate: gWorld.mCameraControl.Update(f[0]);
        break;
    case O::UsgSave: return SaveGame((const char*)a[0], (unsigned long)a[1], false, (const char*)a[2]);
    default: return EECore10OpE3(inOp, p, a, f);
    }
    return 0;
}
