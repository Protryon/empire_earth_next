// C:\EEX\Empire Earth\Empire Earth\EEGameState.cpp (__FILE__ 0x4bfa30 family, Create's asserts). 0x1304e4..0x131b7c
// Clear, RepairRestorables, Init, Create, Reset, the manager setters, EEGetPlayerColor and the
// vector<EETeam>/ESChecksum COMDATs. EEMusicManager.cpp's chains start at 0x131b7c. Unit flags: the exe module set.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <deque>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "lle/UResourceFile.h"
#include "lle/GEColor.h"
#include "lle/GETerrainMesh.h"
#include "lle/UIFormManager.h"
#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEHumanPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEGameState.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECPEngine.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERulesWorld.h"
#include "ee/EEColorTable.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EECampaignFormManager.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEPCalamityHelper.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "ee/EEUserInterface.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEGameStateLoader.h"
#include "ee/EERandomMapClass.h"
#include "ee/EEStartingResources.h"
#include "ee/EEGamePreferences.h"
#include "rust/game/gamecpp.h"

extern char gEmptyString[];                 // 0x4e5b98 (pooled "")
extern long gDefaultTerrain;                // EETerrainTable.cpp

bool EEGameState::Create(EEGameOptions& inOptions, UWideString& outErrorMessage)
{
    return ee_gamecpp_state_create(this, &inOptions, &outErrorMessage);
}

// Returns the colour of a player colour index (the default colour past the table's end)
GEColor EEGetPlayerColor(unsigned long inIndex)
{
    return EEColorTable::GetColor(inIndex);
}

// Returns a starting resource level by index
EEStartingResources* EEGetResourceLevel(long inIndex)
{
    return EEStartingResources::sTable[inIndex];
}


// ------------------------------------------------------------------------- the state bodies' C++ (gamecpp state.rs)

int64_t EEMessageOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

#define P64(x) ((int64_t)(intptr_t)(x))
#define ST ((EEGameState*)p)
#define OPT ((EEGameOptions*)p)
#define SLOT ((EEGameOptionsPlayer*)p)
#define PL ((EEPlayer*)p)
#define WSTR(i) (*(UWideString*)a[i])

// The game state's C++ calls and members (EeGameCpp's state section; EEWorld.cpp's EEWorldOp default): one statement
// each
int64_t EEGameStateOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::StTail: return P64(&ST->mMap);
    case O::StChecksumReset: ST->mChecksum = ESChecksum(false);
        break;
    case O::StDeleteMap: delete ST->mMap;
        break;
    case O::StDeleteScenario: delete ST->mScenarioManager;
        break;
    case O::StDeleteEffects: delete ST->mEffectManager;
        break;
    case O::StDeleteCalamity: delete ST->mCalamityHelper;
        break;
    case O::StDeleteEnvironment: delete ST->mEnvironmentManager;
        break;
    case O::StRestorableRepair: ((FSRestorable*)p)->RepairPointers((long)a[0], (unsigned long)a[1]);
        break;
    case O::StOptionsPtr: return P64(&ST->mGameOptions);
    case O::StNewMap: ST->mMap = new EEMap((long)a[0], (long)a[1], (unsigned long)a[2], gDefaultTerrain);
        break;
    case O::StNewScenarioManager: ST->mScenarioManager = new EEScenarioManager;
        break;
    case O::StNewEffectManager: ST->mEffectManager = new EEEffectManager;
        break;
    case O::StNewCalamityHelper: ST->mCalamityHelper = new EEPCalamityHelper;
        break;
    case O::StNewEnvironmentManager: ST->mEnvironmentManager = new EEEnvironmentManager;
        break;
    case O::CalamityHelperReset: ((EEPCalamityHelper*)p)->Reset();
        break;
    // the options and their slots
    case O::OptFields: return P64(static_cast<EeAppOptionsFields*>(OPT));
    case O::OptPlayer: return P64(&OPT->mPlayers[a[0]]);
    case O::OptPlayerFields: return P64(static_cast<EeAppOptionsPlayerFields*>(SLOT));
    case O::OptPlayerAssign: *SLOT = *(EEGameOptionsPlayer*)a[0];
        break;
    case O::OptPlayerCopyName: SLOT->mName = ((EEGameOptionsPlayer*)a[0])->mName;
        break;
    case O::OptMapType: return P64(&OPT->mMapType);
    case O::OptCampaignNameLength: return OPT->mCampaignName.GetLength();
    case O::OptGetCampaignMapName: return OPT->GetCampaignMapName(WSTR(0));
    case O::OptCopyCampaignName: OPT->mCampaignName = ((EEGameOptions*)a[0])->mCampaignName;
        break;
    case O::OptCopyScenarioName: OPT->mScenarioName = ((EEGameOptions*)a[0])->mScenarioName;
        break;
    case O::OptSetMapName: OPT->mMapName = WSTR(0);
        break;
    case O::OptGetMapDimensions: OPT->GetMapDimensions(*(long*)a[0], *(long*)a[1]);
        break;
    case O::OptAssign: *OPT = *(EEGameOptions*)a[0];
        break;
    // the strings, the files and the data path
    case O::ResString: return gResource->GetString((long)a[0], *(UWideString*)p);
    case O::ResStringVerify: ESVERIFY_AT(::gResource->GetString((long)a[0], *(UWideString*)p) == keseOK, (long)a[1]);
        break;
    case O::StrFormatFileError: sprintf(*(UWideString*)p, "%s\n\"%s\"", (const char*)*(UWideString*)p, (const char*)WSTR(0));
        break;
    case O::StrAssignEmpty: *(UWideString*)p = gEmptyString;
        break;
    case O::ArchiveNew: return P64(new FSArchive);
    case O::ArchiveDelete: delete (FSArchive*)p;
        break;
    case O::ArchiveSet: ((FSArchive*)p)->SetArchive(*(FSFileSpec*)a[0]);
        break;
    case O::DataPathAddArchive: gDataPath->AddArchive(*(FSArchive*)p, true);
        break;
    case O::DataPathFindFile: return gDataPath->FindFile(UString(WSTR(0)), *(FSFileSpec**)a[1]);
    case O::UiSetLoadingText: gUserInterface->SetLoadingText((long)a[0]);
        break;
    case O::UiRandomSeed: return gUserInterface->mRandomSeed;
    case O::FileSpecNew: return P64(new FSFileSpec);
    case O::FileSpecDelete: delete (FSFileSpec*)p;
        break;
    case O::FileSpecSetVolumeStartup: ((FSFileSpec*)p)->SetVolume(gStartupVolume);
        break;
    case O::FileSpecSetStartupData: ((FSFileSpec*)p)->SetDirectory(gStartupDirectory + "data\\" + (a[0] ? "Saved Games\\" : "Scenarios\\"));
        break;
    case O::FileSpecSetFileW: ((FSFileSpec*)p)->SetFile((const char*)WSTR(0));
        break;
    case O::FileOpenReadSpec: return ((FSFileSpec*)p)->Open(kFileAccessRead);
    case O::CampaignNew: return P64(new EECampaign);
    case O::CampaignDelete: delete (EECampaign*)p;
        break;
    case O::CampaignFileName: *(UWideString*)p = UWideString("Campaigns\\") + ((EEGameOptions*)a[0])->mCampaignName + gCampaignExtension;
        break;
    case O::CampaignLoad: return ((EECampaign*)p)->Load(WSTR(0), WSTR(1), false);
    case O::ValidateSaveGameFile: return EEValidateSaveGameFile(*(FSFileSpec*)p, WSTR(0));
    case O::LoaderNew: return P64(new EEGameStateLoader(&gGameState));
    case O::LoaderDelete: delete (EEGameStateLoader*)p;
        break;
    case O::LoaderLoad: return ((EEGameStateLoader*)p)->Load(*(FSFileSpec*)a[0]);
    // the players
    case O::PlSetNameFromSlot: PL->mName = ((EEGameOptionsPlayer*)a[0])->mName;
        break;
    case O::CpSetReplaceOnLoad: ((EEComputerPlayer*)p)->mReplaceOnLoad = a[0] != 0;
        break;
    case O::NewWorldPlayer: return P64(static_cast<EEPlayer*>(new EEHumanPlayer(UWideString("World"), 0)));
    case O::NewHumanPlayerNamed: return P64(static_cast<EEPlayer*>(new EEHumanPlayer(SLOT->mName, (unsigned long)a[0])));
    case O::CpEngineResolve: return EECPEngineResolve((unsigned long)a[0], (unsigned long)a[1], (unsigned long)a[2], (unsigned long)a[3]);
    case O::CpEngineSet: EECPEngineSet((EEComputerPlayer*)p, (unsigned long)a[0]);
        break;
    case O::WorldTableLong: return gWorldTable.GetLongValue((EEWorldConstant)a[0]);
    case O::PlSetCivilizationPoints: PL->SetCivilizationPoints((long)a[0]);
        break;
    case O::ResourceLevelAmount: return EEGetResourceLevel((long)a[0])->mAmounts[a[1]];
    case O::RulesSpeedCount: return (int64_t)EEGetRules().mGameOptions.mSpeeds.size();
    case O::RulesSpeedDefault: return EEGetRules().mGameOptions.mSpeedDefault;
    case O::VecPlayersNew: return P64(new std::vector<EEPlayer*>);
    case O::VecPlayersDelete: delete (std::vector<EEPlayer*>*)p;
        break;
    case O::VecPlayersPush: ((std::vector<EEPlayer*>*)p)->push_back((EEPlayer*)a[0]);
        break;
    case O::VecPlayersSize: return (int64_t)((std::vector<EEPlayer*>*)p)->size();
    case O::CreateRandomMap: return EERandomMap::CreateRandomMap(OPT, *(std::vector<EEPlayer*>*)a[0], (unsigned long)a[1], OPT->mMapType, (unsigned long)a[2], (EEMap*)a[3], *(GE3DPoint*)a[4]);
    case O::RandomNew: return P64(new URandomIntegers);
    case O::RandomDelete: delete (URandomIntegers*)p;
        break;
    case O::RandomSetSeed: ((URandomIntegers*)p)->SetSeed((long)a[0]);
        break;
    case O::RandomGenerate: return ((URandomIntegers*)p)->Generate((long)a[0], (long)a[1]);
    case O::LobbyWriteLock: gWorld.mLobbyOptions.mLock->WriteLock();
        break;
    case O::LobbyWriteUnlock: gWorld.mLobbyOptions.mLock->WriteUnlock();
        break;
    case O::LobbyMapTypeScenDefault: gWorld.mLobbyOptions.mMapType = UWideString(UString("ScenDefault") + gScenarioExtension);
        break;
    case O::CxSetPoolSize: EEComplexUnit::SetPoolSize((unsigned long)a[0], (unsigned long)a[1]);
        break;
    default: return EEMessageOp(inOp, p, a, f);   // the message bodies' (EEMessage.cpp)
    }
    return 0;
}

double EECore9OpAF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EERenderObject.cpp
double EEGameStateOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::RulesSpeedFactor: return EERuleFloat(EEGetRules().mGameOptions.mSpeeds[a[0]].mFactor);
    case O::PrefMaxSounds: return gGamePreferences.mSettings.GetMaxNumberOfSounds();
    default: return EECore9OpAF(inOp, p, a, f);
    }
}
