// New (port): the game log is Rust (ee/EEGameLog.h; rust/crates/ee_game/src/game_log.rs). This unit holds round 11
// lane B's chain link: the C++ calls and members of the game log, the preferences, the game options, the ambient
// sounds, the music manager and the statistics sweep (ee_game gamecpp game_log.rs, preferences.rs, options.rs,
// ambient_io.rs, music_io.rs, stats_capture.rs), one statement each.
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <list>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "lle/LowLevelEngine.h"
#include "lle/SESoundManager.h"
#include "lle/SEScore.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/U2DSparseArray.h"
#include "lle/UResourceFile.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIForest.h"
#include "ee/EETown.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAmbientSound.h"
#include "ee/EEAmbientSoundManager.h"
#include "ee/EEMusicObject.h"
#include "ee/EEMusicManager.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEStringIDs.h"
#include "ee/EETechTreeTechID.h"
#include "ee/EEUIManager.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEGameVariant.h"
#include "ee/EERandomMapGenerators.h"
#include "ee/EERulesWorld.h"
#include "ee/EEStartingResources.h"
#include "ee/EECPTrace.h"
#include "ee/EECPEngine.h"
#include "ee/EEColorTable.h"
#include "ee/EEStatisticsRecord.h"
#include "ee/EEStatisticsHistory.h"
#include "ee/EEEMStorm.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EETeam.h"
#include "ee/EEVCWonder.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/EEUserInterface.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "ee/EECheatCodes.h"
#include "ee/EECivilizationManager.h"
#include "rust/sim/app.h"
#include "rust/sim/tech.h"
#include <utility>
#include "rust/game/gamecpp.h"
#include "rust/game/game_log.h"
#include "ee/EEGameLog.h"

extern EEAIContinentManager gContinentManager;
extern EEGameOptions& gLobbyGameOptions;     // 0x53030c (SetDefaults starts from it)
extern bool gGameWindowSizeDefaulted;        // EEPreferences.cpp
extern float gCameraMaxZoom;                 // EECameraControl.cpp
extern bool& gInCinematic;                   // 0x5318e0
extern char gEmptyString[];                  // 0x4e5b98 (pooled "")
extern EECivilizationManager gCivilizationManager;

// the members the Rust reads in place (gamecpp music_io.rs EeGamecppMusicMembers, stats_capture.rs' tails)
static_assert(offsetof(EEMusicManager, mRequestedIntensity) - offsetof(EEMusicManager, mRandom) == 16 + 10 * sizeof(void*) + 5 * sizeof(long) &&
                  offsetof(EEStatisticsRecord, mIsHuman) - offsetof(EEStatisticsRecord, mResult) == 15 * sizeof(long) + 2 &&
                  offsetof(EEStatisticsHistory, mIsCampaign) - offsetof(EEStatisticsHistory, mStartTime) == 4 * sizeof(long) + 3,
              "gamecpp music_io.rs, stats_capture.rs");
static_assert(kUnitCategoryResource == 1 && kTechEpochPrehistoric == 0, "gamecpp cheats.rs");

// (ee/EEGameLog.h; ee/EEPlayer.h's AddResource declares it without the header)
void EEGameLogIncome(EEPlayer* inPlayer, long inResource, long inAmount)
{
    ee_game_log_income((uint64_t)(uintptr_t)inPlayer, inResource, inAmount);
}

// ------------------------------------------------------------------------- round 11, lane B: the Empire Earth directory's managers (preferences, game options, sounds, music, the game log, the camera, the app captures)


#define P64(x) ((int64_t)(intptr_t)(x))
#define B_SET ((EEGameSettings*)p)
#define B_W ((UWideString*)p)
#define B_N ((UString*)p)
#define B_OPT ((EEGameOptions*)p)
#define B_AM ((EEAmbientSoundManager*)p)
#define B_SND ((EEAmbientSound*)p)
#define B_FILE ((FSFile*)p)
#define B_MUS ((EEMusicManager*)p)
#define B_PL ((EEPlayer*)p)
#define B_UN ((EEUnit*)p)
#define B_CX ((EEComplexUnit*)p)
#define B_HIS ((EEStatisticsHistory*)p)
#define B_REC ((EEStatisticsRecord*)p)
#define B_AI ((EEComputerPlayer*)p)
#define B_EM ((EEEnvironmentManager*)p)
#define B_CV ((EECivilization*)p)
#define B_LIST ((std::vector<EECivUpgrade>*)p)
#define B_VC ((EEVictoryCondition*)p)
#define B_TEXT(x) snprintf((char*)a[0], (size_t)a[1], "%s", (x))

// the C++ calls and members of lane B's bodies in Rust (gamecpp; EeGameCpp's round 11 lane B section): one
// statement each (the camera's are EECameraControl.cpp's EECore10OpD7)
int64_t EECore11OpC2(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEServer.cpp
int64_t EECore11OpB(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    // the preferences (preferences.rs)
    case O::EpDefaultSettings: return P64(&EEGamePreferences::sDefaultSettings);
    case O::EpNewSettings: return P64(new EEGameSettings());
    case O::EpDeleteSettings: delete B_SET;
        break;
    case O::EpWindowSizeDefaulted: gGameWindowSizeDefaulted = a[0] != 0;
        break;
    case O::EpResourceLanguage: return gResource->GetLanguage();
    case O::EpCameraMaxZoom: gCameraMaxZoom = f[0];
        break;
    case O::EpWideLength: return (int64_t)strlen((const char*)*B_W);
    case O::EpWideGet: B_TEXT((const char*)*B_W);
        break;
    case O::EpWideSet: *B_W = (const char*)a[0];
        break;
    case O::EpWideCopy: *B_W = *(const UWideString*)a[0];
        break;
    case O::EpNarrowLength: return (int64_t)strlen((const char*)UWideString(*B_N));
    case O::EpNarrowGet: B_TEXT((const char*)UWideString(*B_N));
        break;
    case O::EpNarrowSet: *B_N = UString(UWideString((const char*)a[0]));
        break;
    case O::EpNarrowCopy: *B_N = *(const UString*)a[0];
        break;
    case O::EpNarrowUpper: B_N->ToUpper();
        break;
    case O::EpStartupVolume: *B_N = UWideString(gStartupVolume);
        break;
    case O::EpStartupDirectory: *B_N = UWideString(gStartupDirectory);
        break;
    case O::EpAssignLastOptions: B_SET->mGameOptions = *(const EEGameOptions*)a[0];
        break;
    case O::EpUnknownName: gResource->GetString(IDS_GAME_UNKNOWN, *B_W);
        break;
    // the game options (options.rs)
    case O::EoSlotEmpty: B_OPT->mPlayers[a[0]] = EEGameOptionsPlayer();
        break;
    case O::EoSlotCopy: B_OPT->mPlayers[a[0]] = ((const EEGameOptions*)a[1])->mPlayers[a[0]];
        break;
    case O::EoWideLoad: return B_W->Load(*(FSFile*)a[0]);
    case O::EoWideSave: return B_W->Save(*(FSFile*)a[0]);
    case O::EoVariantKnown: return gGameVariants[a[0]] != NULL;
    case O::EoPrimitive: return EESavePrimitive(*B_FILE, (int32_t)a[0], (EeUnitsPlayerSaveIo*)a[1]);
    case O::EoMapSizes: return (int64_t)EEGetRules().mGameOptions.mMapSizes.size();
    case O::EoMapSizeDefault: return EEGetRules().mGameOptions.mMapSizeDefault;
    case O::EoMapSizeTiles: return EEGetRules().mGameOptions.mMapSizes[(size_t)a[0]].mTiles;
    case O::EoDifficulties: return (int64_t)EEGetRules().mGameOptions.mDifficulties.size();
    case O::EoSpeeds: return (int64_t)EEGetRules().mGameOptions.mSpeeds.size();
    case O::EoSpeedDefault: return EEGetRules().mGameOptions.mSpeedDefault;
    case O::EoWondersDefault: return EEGetRules().mGameOptions.mWondersDefault;
    case O::EoWondersMaximum: return EERuleLong(EEGetRules().mGameOptions.mWondersMaximum);
    case O::EoUnitLimitMinimum: return EERuleLong(EEGetRules().mGameOptions.mUnitLimitMinimum);
    case O::EoUnitLimitMaximum: return EERuleLong(EEGetRules().mGameOptions.mUnitLimitMaximum);
    case O::EoUnitLimitDefault: return EERuleLong(EEGetRules().mGameOptions.mUnitLimitDefault);
    case O::EoResourceLevels: return EEGetNumberOfResourceLevels();
    case O::EoVariants: return EEGetNumberOfGameVariants();
    case O::EoFindGenerator: return EERMFindGenerator(*B_W, *(unsigned long*)a[0]);
    case O::EoFirstGeneratorName: *B_W = EERMGetGeneratorName(0);
        break;
    case O::EoInScenarioEditor: return gUIManager->mInScenarioEditor;
    case O::EoLobbyOptions: return P64(&gLobbyGameOptions);
    case O::EoDifficultyLevel: return (unsigned long)gDifficultyLevel;
    case O::EoAssign: *B_OPT = *(const EEGameOptions*)a[0];
        break;
    // the ambient sounds (ambient_io.rs)
    case O::EaSoundCount: return (int64_t)B_AM->mSounds.size();
    case O::EaSoundAt: return P64(&B_AM->mSounds[a[0]]);
    case O::EaSoundFields: return P64(static_cast<EeAppAmbientSoundFields*>(B_SND));
    case O::EaPlay: gSoundManager->Play(B_SND->mSound->GetSampleID(), f[0], f[1], f[2], false, NULL);
        break;
    case O::EaHasInstance: return B_SND->mInstance != NULL;
    case O::EaStart: B_SND->mInstance = gSoundManager->PlayAndReturnReferencedNode(B_SND->mSound->GetSampleID(), f[0], f[1], f[2], a[0] != 0, NULL);
        break;
    case O::EaSetParameters: gSoundManager->SetSoundParameters(B_SND->mInstance, f[0], f[1], f[2]);
        break;
    case O::EaStop: gSoundManager->Stop(B_SND->mInstance, true);
        break;
    case O::EaReleaseInstance: B_SND->ReleaseInstance();
        break;
    case O::EaHasSound: return B_SND->mSound != NULL;
    case O::EaGetSound: return P64(B_SND->mSound = EESoundObject::Get(B_SND->mSoundIndex));
    case O::EaLoadMedia: B_SND->mSound->LoadMedia();
        break;
    case O::EaLooping: return B_SND->mSound->GetLooping();
    case O::EaUnloadMedia: B_SND->mSound->UnloadMedia();
        break;
    case O::EaForgetSound: B_SND->mSound = NULL;
        break;
    case O::EaVisibleTiles: return gWorld.mMap->mTerrainMesh.GetNumVisibleTiles();
    case O::EaFileOpen: B_FILE->Open(kFileAccessRead);
        break;
    case O::EaSeed: B_AM->mRandom.SetSeed(TMGetTime());
        break;
    case O::EaFileReadCount: return B_FILE->Read(*(unsigned long*)a[0]);
    case O::EaResize: B_AM->mSounds.resize((size_t)a[0]);
        break;
    case O::EaFileReadBytes: return B_FILE->Read((unsigned char*)a[0], (unsigned long)a[1], *(unsigned long*)a[2]);
    case O::EaFileClose: B_FILE->Close();
        break;
    case O::EaGoalState: return P64(B_CX->mGoalState);
    case O::EaGoalKind: return ((EEUnitGoal*)p)->mKind;
    case O::EaPlayerEpoch: return B_PL->mTechnologies->mCurrentEpoch;
    case O::EaTownCount: return (int64_t)B_PL->mTowns.size();
    case O::EaTownHullContains: return B_PL->mTowns[a[0]]->HullContains(U2DPoint<long>((long)a[1], (long)a[2]));
    case O::EaForestAt: return P64(gContinentManager.GetForestAt((EEUnit*)a[0]));
    case O::EaForestKind: return ((EEAIForest*)p)->GetResourceKind();
    // the music manager (music_io.rs)
    case O::EmMembers: return P64(&B_MUS->mRandom);
    case O::EmConductorIntensity: B_MUS->mConductor->SetIntensity((unsigned long)a[0], true);
        break;
    case O::EmTechnologies: return P64(B_PL->mTechnologies);
    case O::EmCurrentEpoch: return ((EETechTree*)p)->mCurrentEpoch;
    // the game log (game_log.rs)
    case O::EgIsHuman: return B_PL->IsHuman();
    case O::EgComputer: return P64(B_PL->mComputerPlayer);
    case O::EgColorIndex: return EEColorTable::FindIndex(B_PL->mColor);
    case O::EgName: return P64(&B_PL->mName);
    case O::EgCivilizationName: B_TEXT((const char*)B_PL->mCivilization.GetName());
        break;
    case O::EgPersonality: B_TEXT(std::string(EECPPersonality(B_AI).mID).c_str());
        break;
    case O::EgCheatNames: B_TEXT(std::string(EECPCheatNames(B_AI)).c_str());
        break;
    case O::EgEngineName: B_TEXT(std::string(EECPEngineName(B_AI)).c_str());
        break;
    case O::EgGatherResource: return P64(static_cast<EEUGRust*>((EEUnitGoal*)p)->GetGatherResource());
    case O::EgLastAttacker: return P64(B_UN->mComplexUnit->mLastAttacker);
    case O::EgGameTime: return (unsigned long)((EEPhysics*)p)->mGameTime;
    case O::EgGameOver: return ((EEPhysics*)p)->mGameOver;
    case O::EgTraceOn: gCPTraceOn = true;
        break;
    case O::EgLobbyOptions: return P64(&gWorld.mLobbyOptions);
    // the statistics sweep (stats_capture.rs)
    case O::EwRecordStatistics: gWorld.RecordStatistics();
        break;
    case O::EwHistory: return P64(&gWorld.mStatistics);
    case O::EwHistoryTail: return P64(&B_HIS->mStartTime);
    case O::EwCampaignName: return P64(&B_HIS->mCampaignName);
    case O::EwRecordCount: return (int64_t)B_HIS->mRecords.size();
    case O::EwRecordAt: return P64(&B_HIS->mRecords[a[0]]);
    case O::EwRecordTail: return P64(&B_REC->mResult);
    case O::EwRecordColor: return P64(&B_REC->mColor);
    case O::EwColorSize: return sizeof(GEColor);
    case O::EwRecordName: return P64(&B_REC->mName);
    case O::EwRecordSuffix: return P64(&B_REC->mNameSuffix);
    case O::EwRecordStatisticsOf: return P64(&B_REC->mStatistics);
    case O::EwStatsSeries: return P64(((EEPlayerStatistics*)p)->mStats);
    case O::EwStatsEndTime: return ((EEPlayerStatistics*)p)->mEndTime;
    case O::EwNumPlayers: return gWorld.mNumPlayers;
    case O::EwPlayerAt: return P64(gWorld.mPlayers[a[0]]);
    case O::EwPlayerStatistics: return P64(&B_PL->mSavedGroups);
    case O::EwPhysicsSeed: return gPhysics->mRandom.GetSeed();
    // round 10, lane D: the environment manager (gamecpp weather.rs) and the wonder victory (civilization.rs)
    case O::DeManager: return P64(gWorld.mEnvironmentManager);
    case O::DeNewStorm: return P64(new EEEMStorm((long)a[0], B_EM));
    case O::DeStormPush: B_EM->mStorms.push_back((EEEMStorm*)a[0]);
        break;
    case O::DeStormCount: return (int64_t)B_EM->mStorms.size();
    case O::DeStormAt: return P64(B_EM->mStorms[a[0]]);
    case O::DeStormSet: B_EM->mStorms[a[0]] = (EEEMStorm*)a[1];
        break;
    case O::DeStormErase: B_EM->mStorms.erase(B_EM->mStorms.begin() + a[0]);
        break;
    case O::DeStormResize: B_EM->mStorms.resize((size_t)a[0], 0);
        break;
    case O::DeStormClear: B_EM->mStorms.clear();
        break;
    case O::DeStormDelete: delete (EEEMStorm*)p;
        break;
    case O::DeState: return P64(&B_EM->mState);
    case O::DeWind: return P64(&B_EM->mWind);
    case O::DeWindTarget: return P64(&B_EM->mWindTarget);
    case O::DeWindRate: return P64(&B_EM->mWindRate);
    case O::DeNextStep: return P64(&B_EM->mNextStep);
    case O::DeInterval: return P64(&B_EM->mInterval);
    case O::DeMaxStorms: return P64(&B_EM->mMaxStorms);
    case O::DeL50: return P64(&B_EM->mL50);
    case O::DeEnabled: return P64(&B_EM->mIsEnabled);
    case O::DeC18: return P64(&B_EM->mC18);
    case O::DeC24: return P64(&B_EM->mC24);
    case O::DeColorLoad: return ((GEColor*)p)->Load(*(FSFile*)a[0]);
    case O::DeColorSave: return ((GEColor*)p)->Save(*(FSFile*)a[0]);
    case O::EbEnvF40: return P64(&B_EM->mF40);
    case O::EbEnvF44: return P64(&B_EM->mF44);
    case O::DwTeamCount: return B_VC->mTeam->GetNumPlayers();
    case O::DwTeamPlayer: return B_VC->mTeam->mPlayers[a[0]];
    case O::DwNeeded: return gWorld.mNumberOfWondersToWin;
    case O::DwWonderCount: return ((EEPlayer*)p)->GetNumWonderUnits();
    case O::DwWonderTime: return ((EEPlayer*)p)->GetWonderTime((long)a[0]);
    case O::DwVictoryTime: return gWorldTable.GetLongValue(kWorldWonderVictoryTime);
    case O::DwGameTime: return gPhysics->mGameTime;
    case O::DwInCinematic: return gInCinematic;
    // round 10, lane D: the cheats (cheats.rs) and the civilization (civilization.rs)
    case O::DcStrBytes: return P64((const char*)*(UWideString*)p);
    case O::DcMultiplayer: return gCommunications->GetIsMultiplayerSession();
    case O::DcCheatsEnabled: return gWorld.mCheatsEnabled;
    case O::DcNumPlayers: return (int64_t)gWorld.mNumPlayers;
    case O::DcChatLog: gUserInterface->AddChatLogEntry(UWideString((const char*)a[0]), 0);
        break;
    case O::DcClosestTile: return gWorld.mMap != NULL && gWorld.mMap->GetClosestTile((GE3DPoint*)a[0], (long*)a[1], (long*)a[2]) != NULL;
    case O::DcWorldUnit: return P64(static_cast<EeUnitsRtsUnitFields*>(gWorld.GetUnit((unsigned long)a[0])));
    case O::DcStats: return P64(static_cast<EeUnitsUnitClassUpgradeFields*>(B_UN->mStats));
    case O::DcIsViewed: return B_PL->mViewedUnit == (EEUnit*)a[0];
    case O::DcActiveContains: return B_PL->mActiveGroup->Contains((EEComplexUnit*)a[0]);
    case O::DcCxKill: ((EEComplexUnit*)p)->Kill(0, 0, 0);
        break;
    case O::DcPlayerKillUnit: B_PL->KillUnit((EEUnit*)a[0], false);
        break;
    case O::DcExplore: gWorld.mMap->ExploreUnseen(B_PL);
        break;
    case O::DcNewMakeVisible: return P64(new EEMRecord(kMessageMakeWorldVisible));
    case O::DcSetOriginatingSlot: ((EEMRecord*)p)->SetOriginatingSlot(((EEPlayer*)a[0])->mSlot);
        break;
    case O::DcProcessInput: gCommunications->ProcessLocalUserInput((EEMRecord*)p, kSlotNone);
        break;
    case O::DcClassCount: return EEUnitClassCount();
    case O::DcPlayerUpgradeCount: return (int64_t)B_PL->mUnitClassUpgrades.size();
    case O::DcPlayerUpgrade: return P64(B_PL->mUnitClassUpgrades[a[0]]);
    case O::DcCanImprove: return B_PL->CanImprove((long)a[0], (unsigned long)a[1], NULL);
    case O::DcClassMaxLevel: return ((EEUnitClass*)p)->mImprovements->mMaxLevel;
    case O::DcUpgradeLevel: return ((EEUnitClassUpgrade*)p)->mImprovement.GetLevel((unsigned long)a[0]);
    case O::DcApplyImprovement: B_PL->ApplyImprovement((long)a[0], (unsigned long)a[1], false);
        break;
    case O::DcEraseQueued: B_PL->EraseQueuedImprovements((long)a[0]);
        break;
    case O::DcCurrentEpoch: return B_PL->mTechnologies->mCurrentEpoch;
    case O::DcSetEpoch: B_PL->SetEpoch((long)a[0]);
        break;
    case O::DcSetHitPoints: B_UN->SetHitPoints((long)a[0]);
        break;
    case O::DcEndGame: gPhysics->EndGame(a[0] != 0);
        break;
    case O::DcAddResource: B_PL->AddResource((long)a[0], (long)a[1]);
        break;
    // the app profile's cheat sweep (TEST ONLY)
    case O::DcParse: gCheatCodes->ParseCheatCode(UWideString((const char*)a[0]), *(long*)a[1], *(long*)a[2]);
        break;
    case O::DcApply: gCheatCodes->ApplyCheatCode((long)a[0], (long)a[1], (long)a[2]);
        break;
    case O::DcSwapCheatsEnabled: return std::exchange(gWorld.mCheatsEnabled, a[0] != 0);
    case O::DvAvailable: return P64(&B_CV->mAvailablePoints);
    case O::DvMaximum: return P64(&B_CV->mMaxPoints);
    case O::DvNoPointsAdded: return P64(&B_CV->mNoPointsAdded);
    case O::DvUnchanged: return P64(&B_CV->mUnchanged);
    case O::DvPurchased: return P64(&B_CV->mPurchasedUpgrades);
    case O::DvDeferred: return P64(&B_CV->mDeferredUpgrades);
    case O::DvListCount: return (int64_t)B_LIST->size();
    case O::DvListAt: return (*B_LIST)[a[0]];
    case O::DvPurchasedPush: B_CV->mPurchasedUpgrades.push_back((EECivUpgrade)a[0]);
        break;
    case O::DvDeferredPush: B_CV->mDeferredUpgrades.push_back((EECivUpgrade)a[0]);
        break;
    case O::DvDeferredClear: B_CV->mDeferredUpgrades.clear();
        break;
    case O::DvNameEmpty: B_CV->mName = gEmptyString;
        break;
    case O::DvNameLoad: return B_CV->mName.Load(*(FSFile*)a[0]);
    case O::DvNameSave: return B_CV->mName.Save(*(FSFile*)a[0]);
    case O::DvUStrLoad: return ((UString*)p)->Load(*(FSFile*)a[0]);
    case O::DvNameNarrow: B_CV->mName = (const char*)*(UString*)a[0];
        break;
    case O::DvCanAfford: return B_CV->CanAfford((EECivUpgrade)a[0], (unsigned long*)a[1], NULL);
    case O::DvApplyUpgrade: return gCivilizationManager.ApplyUpgrade((EECivUpgrade)a[0], (long)a[1]);
    default: return EECore11OpC2(inOp, p, a, f);
    }
    return 0;
}

double EECore11OpC5F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEContagionManager.cpp
double EECore11OpBF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::EaRandom: return B_AM->Random();
    case O::EaTimeOfDay: return gWorld.GetTimeOfDay();
    case O::EaVisibleWater: return gWorld.mMap->mTerrainMesh.GetVisibleWaterTilePercentage();
    case O::EaVisibleSpace: return gWorld.mMap->mTerrainMesh.GetVisibleSpaceTilePercentage();
    case O::EmReverbGain: return ((EEMusicObject*)p)->GetReverbGain();
    case O::EmReverbHighFreqRatio: return ((EEMusicObject*)p)->GetReverbHighFreqRTRatio();
    case O::EmReverbMix: return ((EEMusicObject*)p)->GetReverbMix();
    case O::EmReverbTime: return ((EEMusicObject*)p)->GetReverbTime();
    case O::EgColorRed: return B_PL->mColor.mRed;
    case O::EgColorGreen: return B_PL->mColor.mGreen;
    case O::EgColorBlue: return B_PL->mColor.mBlue;
    default: return EECore11OpC5F(inOp, p, a, f);
    }
}
