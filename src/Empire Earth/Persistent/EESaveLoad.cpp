// C:\EEX\Empire Earth\Persistent\EESaveLoad.cpp (__FILE__ 0x4c1b64): saving and loading a game, 0x14c6b1..0x14e839.
// Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"

// the last save/load error message (0x531c70; EESaveLoad's functions pass it as their error out-parameter, and the
// game and the scenario editor read it; provisional name)
UWideString gLoadSaveErrorMessage;

#include <time.h>
#include <vector>
#include "ee/EESaveGameFiles.h"

#include <stdio.h>
#include <string.h>
#include <deque>
#include <map>
#include "lle/GETerrainMesh.h"
#include "lle/UIForm.h"
#include <algorithm>
#include <functional>
void EEReportLoadFailure(const UString& inMessage, bool inArg, ESChecksum* inChecksum);  // 0x148362 (EEWorld.cpp's prototype)


#include "ee/EEStringIDs.h"
#include "ee/EEPersistentTag.h"
#include "rust/game/gamecpp.h"
class EEGameState;
extern EEGameState gGameState;
extern char gEmptyString[];             // 0x4e5b98 (the empty string; EECampaignProgress.cpp's declaration)

// Saving and loading a game is ee_game gamecpp persist_save.rs (game core lane, round 9): the save's chunk list and
// header, the chunks' write, the auto-saves' trim, the sound muted around a save or a load (MuteForSaveLoad
// 0x14d195, UnmuteAfterSaveLoad 0x14d253, their volumes 0x531cac/0x531ca8), the save file's open, the file's
// validation and header check and the lobby's check of a save's players; the functions below forward, and their C++
// calls and members are EECore9OpC's cases.

// Returns a save file's name with its last three characters replaced by "txt", or unchanged when asked to keep it
UString GetSaveTextName(const UString& inName, bool inKeep)
{
    return UString(ee_gamecpp_save_text_name((const char*)inName, inName.GetLength(), inKeep));   // gamecpp persist_text.rs
}

// Returns the current date and time as text (asctime), or "Date/time failed."
UString GetDateText()
{
    return UString(ee_gamecpp_date_text());   // gamecpp persist_text.rs
}

// (a save's old text twin is deleted by gamecpp persist_save.rs: delete_text_twin)

// A Rust callback run at the loaded game's first tick (gamecpp persistent.rs after_load)
void EEAfterLoadRust(void (*inRun)(uintptr_t), uintptr_t inData)
{
    EEAfterLoad([inRun, inData]() { inRun(inData); });
}

// Deletes the files matching a pattern in a directory, lowest (FSFileSpec order) first, until at most a given number
// remain
long TrimAutoSaves(const UString& inPattern, const UString& inDirectory, unsigned long inMax)
{
    return ee_gamecpp_ps_trim_auto_saves(&inPattern, &inDirectory, inMax);
}

// Writes a whole saved game (header, then every chunk) to a file; returns the first error
long WriteSaveGame(const UString& inName, unsigned long inTime, FSFile& inFile, FSFile& inOther)
{
    return ee_gamecpp_ps_write_save(&inName, inTime, &inFile, &inOther);
}

// Validates and loads a saved game file with the sound muted (multiplayer reload or into the game state),
// optionally resuming the graphics after; returns 9 when the file is not a valid save
long LoadSavedGameFile(FSFileSpec& inFile, bool inArg2, bool inQuiet, bool inResumeGraphics)
{
    return ee_gamecpp_ps_load_saved_game_file(&inFile, inArg2, inQuiet, inResumeGraphics);
}

// Opens and validates a saved game from a directory and loads it into the game state with the sound muted,
// optionally resuming the graphics after; returns the first error
long EELoadSavedGameFile(const UString& inFileName, bool inFlag1, bool inFlag2, UString inDirectory, bool inArg)
{
    return ee_gamecpp_ps_load_from_directory(&inFileName, inFlag1, inFlag2, &inDirectory, inArg);
}

// Saves the game to a file in a directory with the sound muted; returns the first error
long SaveGame(const UString& inFileName, unsigned long inTime, bool inFlag, UString inDescription)
{
    return ee_gamecpp_ps_save_game(&inFileName, inTime, inFlag, &inDescription);
}

// Writes the game into memory and then to the file (deleting the old text twin first) and reports "Game saved." or
// an error message; returns the first error (0x14d34b)
long SaveGameToFile(FSFileSpec& inFile, unsigned long inTime)
{
    return ee_gamecpp_ps_save_to_file(&inFile, inTime);
}

// Checks a save's players against the lobby (scenario slots for a multiplayer scenario, else every lobby player must
// be a saved one and all saved humans present); lists the problems, returns whether there are none
bool EECheckSavedGameOptions(EEGameOptions& inSaved, EEGameOptions& inCurrent, bool inFlag,
                             std::vector<UWideString>& outMissing, long inCount)
{
    return ee_gamecpp_ps_check_saved_options(&inSaved, &inCurrent, inFlag, &outMissing, inCount);
}

// Returns whether a file holds a save game this build can load; on failure sets the error message (the header
// check's specific text, else the generic "save game invalid")
bool EEValidateSaveGameFile(FSFile& inFile, UWideString& outErrorMessage)
{
    return ee_gamecpp_ps_validate(&inFile, &outErrorMessage);
}

// ------------------------------------------------------------------------- round 9, lane C: this file's bodies' C++


static_assert(IDS_FILE_COULDNT_BE_OPENED == 7152 && IDS_SAVE_GAME_INCOMPATIBLE_WITH_THIS_VERSION == 7154 && IDS_SAVE_GAME_CONTAINS_INVALID_DATA == 7156 && IDS_SAVE_GAME_IS_MISSING_DATA_AND_CANNOT == 7157 && IDS_THERE_ARE_TOO_MANY_PLAYERS_IN_LOBBY == 10034 && IDS_PLAYER_DOES_NOT_EXIST_IN_SAVED == 10037 && IDS_THIS_SAVED_GAME_REQUIRES_PLAYERS == 10038 && IDS_THERE_ARE_TOO_MANY_PLAYERS_TO_PLAY == 10039 && IDS_THIS_IS_NOT_VALID_MULTIPLAYER_SCENARIO == 10040 && IDS_ERROR_CREATING_FILE == 10049 && IDS_ERROR_SAVING_FILE == 10050 && IDS_INVALID_FILE_NAME == 10068 && kPersistentChecksum == 1 && kPersistentSeed == 2 && kPersistentCamera == 3 && kPersistentMap == 4 && kPersistentPlayers == 5 && kPersistentScenario == 6 && kPersistentWorld == 7 && kPersistentEffects == 10 && kPersistentMultiplayer == 8 && kPersistentEnvironment == 12 && kPersistentCalamities == 11, "gamecpp persist_save.rs");

#define P64(x) ((int64_t)(intptr_t)(x))
#define FILE_ (*(FSFile*)p)
#define SPEC ((FSFileSpec*)p)
#define LIST (*(std::vector<FSPersistent*>*)p)
#define HDR ((EEPSaveGameHeader*)p)
#define CHUNKS (*(std::vector<FSChunk>*)p)
#define CHUNK ((FSChunk*)p)
#define SPECS (*(std::vector<FSFileSpec>*)p)
#define WSTRS (*(std::vector<UWideString>*)p)
#define WSTR (*(UWideString*)p)
#define ST ((EEGameState*)p)
#define MEM ((FSMemoryFile*)p)
#define PERSISTENT ((FSPersistent*)p)

