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

#include <time.h>
#include <vector>
#include "ee/EmpireEarth.h"
#include "ee/EESaveLoad.h"
#include "ee/EESaveGameFiles.h"

#include <stdio.h>
#include <string.h>
#include <deque>
#include <map>
#include "lle/UResourceFile.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/SESoundManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NESynchronousClient.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EETeam.h"
#include "ee/EEGameState.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEPCalamityHelper.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EECommunications.h"
#include "ee/EEGameSound.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEDataPreLoading.h"
#include "ee/EEUIFScenarioEditor.h"
#include "lle/FSChunk.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include <algorithm>
#include <functional>
#include "ee/EEPSaveGameHeader.h"
#include "ee/EEGameStateLoader.h"
void EEReportLoadFailure(const UString& inMessage, bool inArg, ESChecksum* inChecksum);  // 0x148362 (EEWorld.cpp's prototype)
#include "ee/EEPersistentTemplates.h"
#include "ee/EEPChecksum.h"
#include "ee/EEPPlayerHelper.h"
#include "ee/EEPWorldHelper.h"
#include "ee/EEPMultiplayerHelper.h"

extern unsigned long& gMaxPlayers;       // 0x5318cc (EESERBUnitTypes.h's declaration; the save header's mL4E8)

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
    UString theDate;
    time_t theTime;
    if (time(&theTime) != -1)
        theDate = asctime(localtime(&theTime));
    else
        theDate = "Date/time failed.";
    return theDate;
}

// The old text twin (".txt") of a save deleted, as the original's unused debug file's Delete did (FSBasicFile::Delete:
// a path longer than PLAT_MAX_PATH is left alone, a failure records the native error)
static void DeleteSaveTextTwin(FSFileSpec& inFile)
{
    UString theName = GetSaveTextName(inFile.GetFile(), false);
    if (inFile.GetVolume().GetLength() + inFile.GetDirectory().GetLength() + theName.GetLength() <= PLAT_MAX_PATH) {
        UString theTextPath(PLAT_MAX_PATH);
        theTextPath = inFile.GetVolume() + inFile.GetDirectory() + theName;
        if (!PlatDeleteFile(theTextPath))
            ESRecordNativeError(PlatGetLastError());
    }
}

// A Rust callback run at the loaded game's first tick (gamecpp persistent.rs after_load)
static void EEAfterLoadRust(void (*inRun)(uintptr_t), uintptr_t inData)
{
    EEAfterLoad([inRun, inData]() { inRun(inData); });
}

// Builds the list of chunks a save writes, in chunk order (each chunk new; the game objects are only borrowed)
void BuildSaveGameChunks(std::vector<FSPersistent*>& outList)
{
    ee_gamecpp_ps_build_chunks(&outList);
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

int64_t EEPWorldOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EEPWorldOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

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

// The save and load bodies' C++ calls and members (EEGraphics.cpp's chain: EECore9OpB's default): one statement each
int64_t EECore9OpC(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::PfReadULong: return FILE_.Read(*(unsigned long*)a[0]);
    case O::PfReadLong: return FILE_.Read(*(long*)a[0]);
    case O::PfReadU16: return FILE_.Read(*(unsigned short*)a[0]);
    case O::PfReadI8: return FILE_.Read(*(signed char*)a[0]);
    case O::PfReadU8: return FILE_.Read(*(unsigned char*)a[0]);
    case O::PfReadF32: return FILE_.Read(*(float*)a[0]);
    case O::PfReadBool: return FILE_.Read(*(bool*)a[0]);
    case O::PfReadChars: return FILE_.Read((char*)a[0], (unsigned long)a[1], *(unsigned long*)a[2]);
    case O::PfWriteULong: return FILE_.Write(*(const unsigned long*)a[0]);
    case O::PfWriteLong: return FILE_.Write(*(const long*)a[0]);
    case O::PfWriteU16: return FILE_.Write(*(const unsigned short*)a[0]);
    case O::PfWriteI8: return FILE_.Write(*(const signed char*)a[0]);
    case O::PfWriteU8: return FILE_.Write(*(const unsigned char*)a[0]);
    case O::PfWriteF32: return FILE_.Write(*(const float*)a[0]);
    case O::PfWriteBool: return FILE_.Write(*(const bool*)a[0]);
    case O::PfWriteChars: return FILE_.Write((const char*)a[0], (unsigned long)a[1], *(unsigned long*)a[2]);
    case O::PvLongsNew: return P64(new std::vector<long>);
    case O::PvLongsDelete: delete (std::vector<long>*)p;
        break;
    case O::PvLongsCount: return (int64_t)((std::vector<long>*)p)->size();
    case O::PvLongsAt: return (*(std::vector<long>*)p)[a[0]];
    case O::PvAfterLoad: EEAfterLoadRust((void (*)(uintptr_t))a[0], (uintptr_t)a[1]);
        break;
    case O::PsMajorVersion: return kSaveGameMajorVersion;
    case O::PsListNew: return P64(new std::vector<FSPersistent*>);
    case O::PsListDelete: delete &LIST;
        break;
    case O::PsListPush: LIST.push_back((FSPersistent*)a[0]);
        break;
    case O::PsListCount: return (int64_t)LIST.size();
    case O::PsListAt: return P64(LIST[a[0]]);
    case O::PsListClear: LIST.clear();
        break;
    case O::PsPersistentTag: return PERSISTENT->GetTagID();
    case O::PsPersistentDelete: delete PERSISTENT;
        break;
    case O::PsNewChecksumChunk: return P64(static_cast<FSPersistent*>(new EEPChecksum(false)));
    case O::PsWorldChecksum: gWorld.GetChecksum(*static_cast<EEPChecksum*>(PERSISTENT));
        break;
    case O::PsNewSeedChunk: return P64(static_cast<FSPersistent*>(new EEPTGeneric<unsigned long>(gUserInterface->mRandomSeed, 2)));
    case O::PsNewMapChunk: return P64(static_cast<FSPersistent*>(new EEPTPRestorable<EEMap>(gWorld.GetMap(), 4)));
    case O::PsMapChunkUnown: static_cast<EEPTPRestorable<EEMap>*>(PERSISTENT)->mOwned = false;
        break;
    case O::PsMapChunkObject: return P64(static_cast<EEPTPRestorable<EEMap>*>(PERSISTENT)->mObject);
    case O::PsNewCameraChunk: return P64(static_cast<FSPersistent*>(new EEPTRestorable<GE3DPoint>(*(GE3DPoint*)a[0], 3)));
    case O::PsCameraChunkValue: return P64(&static_cast<EEPTRestorable<GE3DPoint>*>(PERSISTENT)->mValue);
    case O::PsNewScenarioChunk: return P64(static_cast<FSPersistent*>(new EEPTPRestorable<EEScenarioManager>(gWorld.mScenarioManager, 6)));
    case O::PsScenarioChunkUnown: static_cast<EEPTPRestorable<EEScenarioManager>*>(PERSISTENT)->mOwned = false;
        break;
    case O::PsScenarioChunkObject: return P64(static_cast<EEPTPRestorable<EEScenarioManager>*>(PERSISTENT)->mObject);
    case O::PsNewEffectsChunk: return P64(static_cast<FSPersistent*>(new EEPTPRestorable<EEEffectManager>(gWorld.mEffectManager, 10)));
    case O::PsEffectsChunkUnown: static_cast<EEPTPRestorable<EEEffectManager>*>(PERSISTENT)->mOwned = false;
        break;
    case O::PsEffectsChunkObject: return P64(static_cast<EEPTPRestorable<EEEffectManager>*>(PERSISTENT)->mObject);
    case O::PsNewEnvironmentChunk: return P64(static_cast<FSPersistent*>(new EEPTPRestorable<EEEnvironmentManager>(gWorld.mEnvironmentManager, 12)));
    case O::PsEnvironmentChunkUnown: static_cast<EEPTPRestorable<EEEnvironmentManager>*>(PERSISTENT)->mOwned = false;
        break;
    case O::PsEnvironmentChunkObject: return P64(static_cast<EEPTPRestorable<EEEnvironmentManager>*>(PERSISTENT)->mObject);
    case O::PsNewPlayerHelper: return P64(static_cast<FSPersistent*>(new EEPPlayerHelper(gWorld)));
    case O::PsNewWorldHelper: return P64(static_cast<FSPersistent*>(new EEPWorldHelper(gWorld)));
    case O::PsNewMultiplayerHelper: return P64(static_cast<FSPersistent*>(new EEPMultiplayerHelper()));
    case O::PsNewCalamityHelper: return P64(static_cast<FSPersistent*>(new EEPCalamityHelper()));
    case O::PsCastChecksum: return P64(dynamic_cast<EEPChecksum*>(PERSISTENT));
    case O::PsCastSeed: return P64(dynamic_cast<EEPTGeneric<unsigned long>*>(PERSISTENT));
    case O::PsSeedValue: return (int64_t)((EEPTGeneric<unsigned long>*)p)->mValue;
    case O::PsCastEffects: return P64(static_cast<FSPersistent*>(dynamic_cast<EEPTPRestorable<EEEffectManager>*>(PERSISTENT)));
    case O::PsCastCamera: return P64(static_cast<FSPersistent*>(dynamic_cast<EEPTRestorable<GE3DPoint>*>(PERSISTENT)));
    case O::PsCastMap: return P64(static_cast<FSPersistent*>(dynamic_cast<EEPTPRestorable<EEMap>*>(PERSISTENT)));
    case O::PsCastPlayers: return P64(dynamic_cast<EEPPlayerHelper*>(PERSISTENT));
    case O::PsCastWorld: return P64(dynamic_cast<EEPWorldHelper*>(PERSISTENT));
    case O::PsCastScenario: return P64(static_cast<FSPersistent*>(dynamic_cast<EEPTPRestorable<EEScenarioManager>*>(PERSISTENT)));
    case O::PsCastCalamities: return P64(dynamic_cast<EEPCalamityHelper*>(PERSISTENT));
    case O::PsCastEnvironment: return P64(static_cast<FSPersistent*>(dynamic_cast<EEPTPRestorable<EEEnvironmentManager>*>(PERSISTENT)));
    case O::PsHeaderNew: return P64(new EEPSaveGameHeader);
    case O::PsHeaderDelete: delete HDR;
        break;
    case O::PsHeaderSave: return HDR->Save((unsigned long)a[0], (unsigned long)a[1], *(FSFile*)a[2]);
    case O::PsHeaderLoad: return HDR->Load((unsigned long)a[0], (unsigned long)a[1], *(FSFile*)a[2]);
    case O::PsHdrSetName: HDR->mName = *(const UString*)a[0];
        break;
    case O::PsHdrDefaultDescription: HDR->mDescription = UString("No game description provided.");
        break;
    case O::PsHdrVersions: return P64(&HDR->mMajorVersion);
    case O::PsHdrNumbers: return P64(&HDR->mL4E4);
    case O::PsHdrNamesClear: HDR->mPlayerNames = UString();
        break;
    case O::PsHdrNamesAppendPlayer: HDR->mPlayerNames += UString((const char*)gWorld.mPlayers[a[0]]->mName);
        break;
    case O::PsHdrNamesAppendSpace: HDR->mPlayerNames += " ";
        break;
    case O::PsHdrSetDate: HDR->mDate = GetDateText();
        break;
    case O::PsHdrTagsClear: HDR->mChunkTypes.clear();
        break;
    case O::PsHdrTagsPush: HDR->mChunkTypes.push_back((long)a[0]);
        break;
    case O::PsHdrTagsCount: return (int64_t)HDR->mChunkTypes.size();
    case O::PsHdrTagAt: return HDR->mChunkTypes[a[0]];
    case O::PsWorldScenario: return P64(gWorld.mScenarioManager);
    case O::PsHdrMediaFromScenario: HDR->mMediaStrings = gWorld.mScenarioManager->mMediaStrings;
        break;
    case O::PsHdrOptionsFromWorld: HDR->mGameOptions = gWorld.mGameOptions;
        break;
    case O::PsMaxPlayers: return gMaxPlayers;
    case O::PsSpecNew: return P64(new FSFileSpec);
    case O::PsSpecDelete: delete SPEC;
        break;
    case O::PsSpecSetDirectory: SPEC->SetDirectory(*(const UString*)a[0]);
        break;
    case O::PsSpecFirstFile: return SPEC->GetFirstFile(*(const UString*)a[0], *(FSFileSpec*)a[1]);
    case O::PsSpecNextFile: return SPEC->GetNextFile(*(FSFileSpec*)a[0]);
    case O::PsSpecsNew: return P64(new std::vector<FSFileSpec>);
    case O::PsSpecsDelete: delete &SPECS;
        break;
    case O::PsSpecsCount: return (int64_t)SPECS.size();
    case O::PsSpecLess: return *SPEC < (*(std::vector<FSFileSpec>*)a[0])[a[1]];
    case O::PsSpecsInsert: SPECS.insert(SPECS.begin() + a[0], *(FSFileSpec*)a[1]);
        break;
    case O::PsSpecsDeleteFirst: SPECS[0].Delete();
        break;
    case O::PsSpecsEraseFirst: SPECS.erase(SPECS.begin());
        break;
    case O::PsSpecSetStartupVolume: SPEC->SetVolume(gStartupVolume);
        break;
    case O::PsSpecSetStartupDirectory: SPEC->SetDirectory(gStartupDirectory + *(const UString*)a[0]);
        break;
    case O::PsSpecSetFile: SPEC->SetFile(*(const UString*)a[0]);
        break;
    case O::PsSpecDeleteFile: SPEC->Delete();
        break;
    case O::PsSpecCreate: return SPEC->Create();
    case O::PsSpecOpen: return SPEC->Open((unsigned long)a[0]);
    case O::PsSpecClose: return FILE_.Close();
    case O::PsSpecOpenAny: return FILE_.Open((unsigned long)a[0]);
    case O::PsFileIsOpen: return FILE_.GetIsOpen();
    case O::PsFileCloseAny: return FILE_.Close();
    case O::PsChunksNew: return P64(new std::vector<FSChunk>);
    case O::PsChunksDelete: delete &CHUNKS;
        break;
    case O::PsChunksResize: CHUNKS.resize((size_t)a[0]);
        break;
    case O::PsChunksClear: CHUNKS.clear();
        break;
    case O::PsChunkSavePersistent: return CHUNKS[a[0]].SavePersistentToChunk((unsigned long)a[1], (unsigned long)a[2], (FSPersistent*)a[3]);
    case O::PsChunkSaveToFile: return CHUNKS[a[0]].SaveChunkToFile(*(FSFile*)a[1]);
    case O::PsChunkNew: return P64(new FSChunk);
    case O::PsChunkDelete: delete CHUNK;
        break;
    case O::PsChunkLoadHeader: return CHUNK->LoadChunkHeader(*(FSFile*)a[0]);
    case O::PsChunkGetSize: return CHUNK->GetChunkSize(*(unsigned long*)a[0]);
    case O::PsChunkSkip: return CHUNK->SkipChunk(*(FSFile*)a[0]);
    case O::PsChunkLoadFromFile: return CHUNK->LoadChunkFromFile(*(FSFile*)a[0]);
    case O::PsChunkLoadPersistent: return CHUNK->LoadPersistentFromChunk(*(FSPersistent**)a[0]);
    case O::PsMemFileNew: return P64(new FSMemoryFile(0x400, true, 1000));
    case O::PsMemFileNew1000: return P64(new FSMemoryFile(1000, true, 1));
    case O::PsMemFileDelete: delete MEM;
        break;
    case O::PsMemFileSave: return MEM->Save(*(FSFile*)a[0]);
    case O::PsMemFileLoad: return MEM->Load(*(FSFile*)a[0]);
    case O::PsUStrNew: return P64(new UString);
    case O::PsUStrDelete: delete (UString*)p;
        break;
    case O::PsSaveTextName: *(UString*)a[0] = GetSaveTextName(SPEC->GetFile(), a[1] != 0);
        break;
    case O::PsDeleteTextTwin: DeleteSaveTextTwin(*SPEC);
        break;
    case O::PsReportSaved: EEReportLoadFailure(UString("Game saved.  "), false, NULL);
        break;
    case O::PsReportSaveError: EEReportLoadFailure(UString("Error saving game.  Please report this bug."), false, NULL);
        break;
    case O::PsMessageString: gResource->GetString((long)a[0], gLoadSaveErrorMessage);
        break;
    case O::PsMessageStringVerify: ESVERIFY_AT(::gResource->GetString((long)a[0], ::gLoadSaveErrorMessage) == keseOK, 669);
        break;
    case O::PsResourceString: gResource->GetString((long)a[0], WSTR);
        break;
    case O::PsResourceStringVerify: ESVERIFY_AT(::gResource->GetString((long)a[0], WSTR) == keseOK, 967);
        break;
    case O::PsGameState: return P64(&gGameState);
    case O::PsValidateIntoMessage: return EEValidateSaveGameFile(FILE_, gLoadSaveErrorMessage);
    case O::PsGraphicsPostResume: gGraphics->PostResume();
        break;
    case O::PsUiResumeGraphics: gUserInterface->ResumeGraphics();
        break;
    case O::PsMapApplyVertexColors: gWorld.GetMap()->ApplyVertexColors();
        break;
    case O::PsSetMasterVolume: gSoundManager->SetMasterVolume(f[0]);
        break;
    case O::PsSetMusicVolume: gConductor->GetConductor().SetVolume(f[0]);
        break;
    case O::PsSleep: PlatSleep((unsigned long)a[0]);
        break;
    case O::PsCommPresent: return gCommunications != NULL;
    case O::PsShowSplash: gUIManager->SwapForm(gUIManager->GetFormByID(kFormSplash));
        break;
    case O::PsShowSelectionAreas: gWorld.GetCurrentPlayer()->mActiveGroup->ShowSelectionAreas();
        break;
    case O::PsOpenMainGameForm: gUserInterface->OpenForm(gUIManager->GetMainGameForm());
        break;
    case O::PsWaitRenderNow: gGraphics->mRenderNow->Wait(kTSWaitInfinite);
        break;
    case O::PsSetLoadingSavedGame: gLoadingSavedGame = a[0] != 0;
        break;
    case O::PsStClear: ST->Clear();
        break;
    case O::PsStReset: ST->Reset();
        break;
    case O::PsStInit: ST->Init();
        break;
    case O::PsStWriteLock: ST->WriteLock();
        break;
    case O::PsStWriteUnlock: ST->WriteUnlock();
        break;
    case O::PsStFinishLoad: ST->FinishLoad((unsigned long)a[0], (unsigned long)a[1]);
        break;
    case O::PsStOptionsFromHeader: ST->mGameOptions = ((EEPSaveGameHeader*)a[0])->mGameOptions;
        break;
    case O::PsStSetChecksum: ST->mChecksum = *(EEPChecksum*)a[0];
        break;
    case O::PsStSetEffectManager: ST->SetEffectManager((EEEffectManager*)a[0]);
        break;
    case O::PsStSetMap: ST->SetMap((EEMap*)a[0]);
        break;
    case O::PsStSetCalamityHelper: ST->SetCalamityHelper((EEPCalamityHelper*)a[0]);
        break;
    case O::PsStSetEnvironmentManager: ST->SetEnvironmentManager((EEEnvironmentManager*)a[0]);
        break;
    case O::PsStRestorablePushEffects: ST->mRestorables.push_back((EEEffectManager*)a[0]);
        break;
    case O::PsStRestorablePushCalamities: ST->mRestorables.push_back((EEPCalamityHelper*)a[0]);
        break;
    case O::PsStRestorablePushEnvironment: ST->mRestorables.push_back((EEEnvironmentManager*)a[0]);
        break;
    case O::PsStLoadedPlayerPush: ST->mLoadedPlayers.push_back((EEPlayer*)a[0]);
        break;
    case O::PsPlayerRemoveAlly: ((EEPlayer*)p)->RemoveAlly((EEPlayer*)a[0]);
        break;
    case O::PsEditorFormPresent: return gUIManager->GetScenarioEditorForm() != NULL;
    case O::PsEditorClearSelection: gUIManager->GetScenarioEditorForm()->ClearSelection();
        break;
    case O::PsPreloadWaitQueue: gDataPreLoading->WaitForMessageQueue();
        break;
    case O::PsWorldClear: gWorld.Clear();
        break;
    case O::PsWorldStartGame: gWorld.StartGame((EEGameState*)a[0], true);
        break;
    case O::PsInScenarioEditor: return gUIManager->mInScenarioEditor;
    case O::PsTextIsUtf8: return EESaveTextIsUtf8((unsigned long)a[0], (unsigned long)a[1]);
    case O::PsLegacyTextLoad: USetLegacyTextLoad(a[0] != 0);
        break;
    case O::PsReportCorrupt: EEReportLoadFailure("Load game file is corrupt.", false, NULL);
        break;
    case O::PsDebugCorrupt: ESDebugOut("EE load failure: Load game file is corrupt.\n");
        break;
    case O::PsDataPathFind: return gDataPath->FindFile(*(const UString*)a[0], *(FSFileSpec**)a[1]);
    case O::PsMapL195610: return ((EEMap*)p)->mL195610;
    case O::PsMapL195614: return ((EEMap*)p)->mL195614;
    case O::PsPphMaxPlayers: return (int64_t)((EEPPlayerHelper*)p)->mMaxPlayers;
    case O::PsPphLocalIndex: return ((EEPPlayerHelper*)p)->GetLocalPlayerIndex(*(long*)a[0]);
    case O::PsPphPlayerCount: return (int64_t)((EEPPlayerHelper*)p)->mPlayers.size();
    case O::PsPphPlayerAt: return P64(((EEPPlayerHelper*)p)->mPlayers[a[0]]);
    case O::PsWStrsNew: return P64(new std::vector<UWideString>);
    case O::PsWStrsDelete: delete &WSTRS;
        break;
    case O::PsWStrsClear: WSTRS.clear();
        break;
    case O::PsWStrsPush: WSTRS.push_back(*(UWideString*)a[0]);
        break;
    case O::PsWStrsPushSlotName: WSTRS.push_back(((EEGameOptions*)a[0])->mPlayers[a[1]].mName);
        break;
    case O::PsWStrsCount: return (int64_t)WSTRS.size();
    case O::PsWStrsAt: return P64(&WSTRS[a[0]]);
    case O::PsWStrsContains: return std::find(WSTRS.begin(), WSTRS.end(), *(UWideString*)a[0]) != WSTRS.end();
    case O::PsWStrNew: return P64(new UWideString);
    case O::PsWStrDelete: delete &WSTR;
        break;
    case O::PsWStrClear: WSTR = gEmptyString;
        break;
    case O::PsWStrLength: return WSTR.GetLength();
    case O::PsSprintfNumber: sprintf(WSTR, *(UWideString*)a[0], (unsigned long)a[1]);
        break;
    case O::PsSprintfText: sprintf(WSTR, *(UWideString*)a[0], (const char*)*(UWideString*)a[1]);
        break;
    case O::PsSprintfTwoNumbers: sprintf(WSTR, *(UWideString*)a[0], (size_t)a[1], (unsigned long)a[2]);
        break;
    case O::PsLoadProgress: gUserInterface->SetLoadProgress(f[0]);
        break;
    default: return EEPWorldOp(inOp, p, a, f);
    }
    return 0;
}

double EECore9OpCF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::PsMasterVolume: return gSoundManager->GetMasterVolume();
    case O::PsMusicVolume: return gConductor->GetConductor().GetVolume();
    default: return EEPWorldOpF(inOp, p, a, f);
    }
}
