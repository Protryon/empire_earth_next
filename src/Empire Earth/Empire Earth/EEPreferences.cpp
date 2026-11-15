// C:\EEX\Empire Earth\Empire Earth\EEPreferences.cpp (file name inferred: no __FILE__ or RTTI; it sorts between
// EEMusicManager.cpp and EETeam.cpp). 0x13242b..0x136b6d: the game preferences (config.yaml; the original: the registry). EEGamePreferences::sDefaultSettings'
// init chain first, then EEGameSettings' dtor and ctor (the defaults), EEGamePreferences' ctor/dtor, IsNotKorean,
// the resource defaults, Load (+ the game options), Save (+ the game options) and SetLastGameOptions.
// The settings file calls go through the UConfig member (the original's URegistry at +0).
#include <windows.h>
#include "gx/GX.h"

#include <vector>

#include "lle/LowLevelEngine.h"
#include "platform/PlatFile.h"
#include "lle/UResourceFile.h"
#include "lle/GETextureManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEStringIDs.h"
#include "ee/EETechTreeTechID.h"
// the settings are kept in config.yaml (lle/UConfig.h; the original kept them in the registry key
// HKCU\Software\Mad Doc Software\EE-AOC and its "Game Options" subkey)

// 0x529b90 (init chain 0x13242b: ctor 0x132435, atexit 0x13243f, guarded dtor 0x13244b)
EEGameSettings EEGamePreferences::sDefaultSettings;
// SYNTHETIC: EXE 0x53242b   (wrapper)
// SYNTHETIC: EXE 0x532435   (ctor)
// SYNTHETIC: EXE 0x53243f   (registrar)
// SYNTHETIC: EXE 0x53244b   (guarded dtor)

bool gGameWindowSizeDefaulted;   // port: no saved game window size (EEPreferences Load)

// port: the texture packs (docs/DATA_FORMATS.md "Texture packs"). One pack ships, Data/Textures-4x (4x the original
// textures' size, a subset of them); resolution 2 draws its files at half size (one more MIP level dropped: a 2x2 box
// filter), 4 at full size; a texture the pack does not have is the original at either.
void EESetTextureResolution(long inResolution)
{
    if (inResolution == 2)
        GETextureManager::SetTexturePack("textures-4x", 2, 1);
    else if (inResolution == 4)
        GETextureManager::SetTexturePack("textures-4x", 2, 0);
    else
        GETextureManager::SetTexturePack(NULL, 0, 0);
}
extern float gCameraMaxZoom;     // port: EECameraControl.cpp

extern "C" void SPIMSetMasterVolume(float inVolume);   // the mixer plug-in (lle/SEMixer.h)
extern "C" void SPIMSetOutputDevice(const char* inName);

// port: see EEGamePreferences.h
void EEApplyPortSettings(const EEGameSettings& inSettings)
{
    gCameraMaxZoom = inSettings.mMaximumZoomOut / 100.0f;
    GXSetRenderPercent((unsigned long)inSettings.mRenderResolution);
    GXSetFrameLimit(inSettings.mFrameRateLimit);
    GXSetTextureFiltering((unsigned long)inSettings.mTextureFiltering);
    GXSetBrightness(inSettings.mBrightness / 100.0f);
    SPIMSetMasterVolume(inSettings.mMasterVolume / 100.0f);
    SPIMSetOutputDevice(inSettings.mAudioDevice);
}

// FUNCTION: EXE 0x532466
// Destroys the game settings (members only)
EEGameSettings::~EEGameSettings()
{
}

// FUNCTION: EXE 0x5324f8
// Creates the default game settings (Direct3D, volumes, scroll speeds, display, network and game options)
EEGameSettings::EEGameSettings()
{
    this->mHostGameName = "";
    this->mPlayerName = "";
    this->mSierraUserID = "";
    this->mSierraGameName = "";
    this->mRasterizerName = "Direct3D";
    this->mInstalledFromDirectory = "";
    this->mInstalledFromVolume = "";
    this->mLastGameFile = "";
    this->mLastQuickSave = "";
    this->mLastAutoSave = "";
    this->mLastScenarioFile = "";
    this->mWonNetPassword = "";
    this->mMouseSensitivity = 0.3125f;
    this->mSideScrollSpeed = 2.0f;
    this->mGrabScrollSpeed = 0.2f;
    this->mSoundVolume = 1.0f;
    this->mMusicVolume = 0.65f;
    this->mMaxNumberOfSounds = 1.0f;
    this->mUnitScaleFactor = 1.0f;
    this->mZoomLock = 1.0f;
    this->mZoomStyle = 0;
    this->mScreenMode = 1;
    this->mGameBitDepth = 16;
    this->mGameWindowHeight = 600;
    this->mGameWindowWidth = 800;
    this->mMaxPTsPerSecond = 125;
    this->mMinPTsPerSecond = 15;
    this->mPTRateTuningThreshold = 30;
    this->mScreenID = 0;
    this->mTextureBitDepth = 16;
    this->mL53C = 15020;
    this->mL540 = 0;
    this->mMaxCTSendInterval = 2250;
    this->mCTSendIntervalStepDown = 10;
    this->mCTSendIntervalStepUp = 125;
    this->mStartingKeyboardLayout = 0;
    this->mMIPLevelsToDrop = 0;
    this->mButtonMIPLevelsToDrop = 0;
    this->mTerrainMIPLevelsToDrop = 0;
    this->mModelDetail = 2;
    this->mNetworkAdapter = 0;
    this->mEffectDetail = 2;
    this->mNumberOfAutoSaves = 10;
    this->mAutoSaveInMilliseconds = 600000;
    this->mLastOpenPostGameStatsPage = 1;
    this->mL5BC = 1;
    this->mAnimationSmoothing = true;
    this->mDisplayClouds = true;
    this->mDisplayShadows = true;
    this->mDisplayBattlefieldAmbients = true;
    this->mB5C6 = false;
    this->mUnitLighting = true;
    this->mFullScreen = true;
    this->mReverbEnabled = true;
    this->mB5C0 = true;
    this->mWaitForVSync = true;
    this->mB5C2 = true;
    this->mAmbientSounds = true;
    this->mB5CA = true;
    this->mB5CB = true;
    this->mB5CC = true;
    this->mEnableAnimalHerds = true;
    this->mB5C5 = true;
    this->mShowBlood = true;
    this->mZoomEnabled = true;
    this->mMusicEnabled = true;
    this->mRightHandMouseOrientation = true;
    this->mB5C4 = false;
    this->mTrilinearFilter = true;
    this->mB5D6 = false;
    this->mUseAreaEffectOverlays = true;
    this->mDisplayWeatherEffects = false;
    this->mUseCandidateWindow = false;
    this->mDebugOOSHandling = true;
    this->mTakeJPGScreenshots = false;
    this->mAntiAliasing = false;
    this->mTakeEndGameScreenshots = false;
    this->mShowAutoWaypoints = false;
    this->mL54C = 0;
    this->mL550 = 0;
    this->mL554 = 0;
    this->mL558 = 0;
    this->mL55C = 0;
    this->mL560 = 0;
    this->mL5B8 = 0;
    this->mComputerPlayerDifficultyLevel = 1;
    this->mTextureResolution = 0;
    this->mUIScale = 0;
    this->mRenderResolution = 100;
    this->mWindowMode = 0;
    this->mFrameRateLimit = 0;
    this->mTextureFiltering = 2;
    this->mBrightness = 100;
    this->mMasterVolume = 100;
    this->mMuteInBackground = true;
    this->mMaximumZoomOut = 200;
    this->mGameLog = 1;
    this->mAudioDevice[0] = 0;
}

// FUNCTION: EXE 0x53281e
// Creates the game preferences with default settings
EEGamePreferences::EEGamePreferences()
{
}

// FUNCTION: EXE 0x532834
// Destroys the game preferences (members only)
EEGamePreferences::~EEGamePreferences()
{
}

// FUNCTION: EXE 0x532848
// Returns whether neither the game resources nor the system language is Korean
bool EEGamePreferences::IsNotKorean()
{
    bool theResult = true;
    if (gResource->GetLanguage() == 0x412 || UGetOSLanguage() == 0x412)
        theResult = false;
    return theResult;
}

// FUNCTION: EXE 0x53286a
// Sets the default player, host and Sierra names to the "unknown" string and the WON password to ""
void EEInitializeDefaultSettings()
{
    UWideString theName;
    gResource->GetString(IDS_GAME_UNKNOWN, theName);
    EEGamePreferences::sDefaultSettings.mHostGameName = theName;
    EEGamePreferences::sDefaultSettings.mPlayerName = theName;
    EEGamePreferences::sDefaultSettings.mSierraUserID = theName;
    EEGamePreferences::sDefaultSettings.mSierraGameName = theName;
    EEGamePreferences::sDefaultSettings.mWonNetPassword = "";
}

// port: the settings of config.yaml, one row each (Load and Save go through the table; docs/DATA_FORMATS.md
// "config.yaml"). Settings the port does not use any more are not stored: the rasterizer name (one renderer), the
// bit depths (32), "Wait for VSync" (the frame rate limit decides it), the WON and Sierra.com accounts, the IME
// candidate window and the values nothing reads (unit scale factor, animal herds, area effect overlays, button MIP
// levels, auto waypoints, keyboard layout, load game name).
namespace {

enum EESettingKind {
    kSettingLong,       // a long or unsigned long
    kSettingPercent,    // a float, stored as a whole percent
    kSettingBool,       // a bool, stored true/false
    kSettingLongFlag,   // a long 0 or 1, stored true/false
    kSettingWide,       // a UWideString
    kSettingNarrow,     // a UString
    kSettingChars,      // a char array (EEGameSettings::mAudioDevice)
};

struct EESetting {
    const char* mBlock;     // the block's path in config.yaml
    const char* mKey;
    EESettingKind mKind;
    void* mField;           // the member
    long mMinimum;          // the integers it may have (the stored percent for kSettingPercent)
    long mMaximum;
    bool mClamp;            // a value out of range is the nearest limit (else the default)
};

const long kAnyMinimum = -0x7fffffffL - 1;
const long kAnyMaximum = 0x7fffffffL;

// The rows over inSettings: the game settings, or with inGameOptions the last game's options
void EESettingRows(EEGameSettings& s, bool inGameOptions, std::vector<EESetting>& outRows)
{
#define EE_ROW(block, key, kind, field, minimum, maximum, clamp) \
    outRows.push_back(EESetting{block, key, kind, (void*)&s.field, minimum, maximum, clamp})
#define EE_INT(block, key, field) EE_ROW(block, key, kSettingLong, field, kAnyMinimum, kAnyMaximum, false)
#define EE_BOOL(block, key, field) EE_ROW(block, key, kSettingBool, field, 0, 0, false)
    outRows.clear();
    if (inGameOptions) {
        EE_INT("game_options.map", "size", mGameOptions.mMapSize);
        EE_ROW("game_options.map", "type", kSettingWide, mGameOptions.mMapType, 0, 0, false);
        EE_INT("game_options", "starting_resources", mGameOptions.mResources);
        EE_INT("game_options.epochs", "start", mGameOptions.mEpochStart);
        EE_INT("game_options.epochs", "end", mGameOptions.mEpochEnd);
        EE_INT("game_options", "unit_limit", mGameOptions.mUnitLimit);
        EE_INT("game_options", "wonders_for_victory", mGameOptions.mWondersForVictory);
        EE_INT("game_options", "variant", mGameOptions.mVariant);
        EE_INT("game_options", "difficulty", mGameOptions.mDifficulty);
        EE_INT("game_options", "speed", mGameOptions.mGameSpeed);
        EE_BOOL("game_options", "record_game", mGameOptions.mRecordGames);
        EE_BOOL("game_options", "reveal_map", mGameOptions.mRevealMap);
        EE_BOOL("game_options", "allow_custom_civs", mGameOptions.mAllowCustomCivs);
        EE_BOOL("game_options", "lock_teams", mGameOptions.mLockTeams);
        EE_BOOL("game_options", "lock_speed", mGameOptions.mLockSpeed);
        EE_BOOL("game_options", "cheat_codes", mGameOptions.mCheatCodes);
        EE_BOOL("game_options", "victory_allowed", mGameOptions.mVictory);
        return;
    }
    EE_ROW("player", "name", kSettingWide, mPlayerName, 0, 0, false);
    EE_INT("player", "computer_difficulty", mComputerPlayerDifficultyLevel);

    EE_ROW("display", "screen", kSettingLong, mScreenID, 0, kAnyMaximum, false);
    EE_ROW("display", "window_mode", kSettingLong, mWindowMode, 0, 1, false);   // 0 full screen, 1 windowed
    EE_ROW("display.resolution", "width", kSettingLong, mGameWindowWidth, 1, kAnyMaximum, false);
    EE_ROW("display.resolution", "height", kSettingLong, mGameWindowHeight, 1, kAnyMaximum, false);
    EE_ROW("display", "ui_scale", kSettingLong, mUIScale, 0, 300, false);   // percent; 0 Auto
    EE_ROW("display", "render_resolution", kSettingLong, mRenderResolution, 50, 100, false);   // percent
    EE_ROW("display", "frame_rate_limit", kSettingLong, mFrameRateLimit, -1, 1000, false);   // 0 v-sync, -1 none
    EE_ROW("display", "brightness", kSettingLong, mBrightness, 50, 200, false);   // percent

    EE_ROW("graphics", "texture_resolution", kSettingLong, mTextureResolution, 0, 4, false);   // 0, 2 or 4
    EE_ROW("graphics", "texture_filtering", kSettingLong, mTextureFiltering, 0, 2, false);   // bilinear, trilinear, anisotropic
    EE_BOOL("graphics", "trilinear_filter", mTrilinearFilter);
    EE_ROW("graphics.mip_levels_to_drop", "textures", kSettingLong, mMIPLevelsToDrop, 0, 8, true);
    EE_ROW("graphics.mip_levels_to_drop", "terrain", kSettingLong, mTerrainMIPLevelsToDrop, 0, 8, true);
    EE_ROW("graphics", "model_detail", kSettingLong, mModelDetail, kAnyMinimum, 2, true);
    EE_ROW("graphics", "effect_detail", kSettingLong, mEffectDetail, kAnyMinimum, 2, true);
    EE_BOOL("graphics", "unit_lighting", mUnitLighting);
    EE_BOOL("graphics", "animation_smoothing", mAnimationSmoothing);
    EE_BOOL("graphics", "shadows", mDisplayShadows);
    EE_BOOL("graphics", "clouds", mDisplayClouds);
    EE_BOOL("graphics", "battlefield_ambients", mDisplayBattlefieldAmbients);
    EE_BOOL("graphics", "show_blood", mShowBlood);

    EE_ROW("audio", "device", kSettingChars, mAudioDevice, 0, 0, false);   // empty: the system's default
    EE_ROW("audio", "master_volume", kSettingLong, mMasterVolume, 0, 100, false);
    EE_ROW("audio", "sound_volume", kSettingPercent, mSoundVolume, 0, 100, true);
    EE_BOOL("audio.music", "enabled", mMusicEnabled);
    EE_ROW("audio.music", "volume", kSettingPercent, mMusicVolume, 0, 100, true);
    EE_BOOL("audio", "reverb", mReverbEnabled);
    EE_BOOL("audio", "ambient_sounds", mAmbientSounds);
    EE_ROW("audio", "max_sounds", kSettingPercent, mMaxNumberOfSounds, 0, kAnyMaximum, true);   // percent
    EE_BOOL("audio", "mute_in_background", mMuteInBackground);

    EE_ROW("input", "mouse_sensitivity", kSettingPercent, mMouseSensitivity, 1, kAnyMaximum, true);
    EE_BOOL("input", "right_hand_mouse", mRightHandMouseOrientation);
    EE_ROW("input.scroll", "side_speed", kSettingPercent, mSideScrollSpeed, 1, kAnyMaximum, true);
    EE_ROW("input.scroll", "grab_speed", kSettingPercent, mGrabScrollSpeed, 1, kAnyMaximum, true);

    EE_ROW("camera", "maximum_zoom_out", kSettingLong, mMaximumZoomOut, 100, 300, false);   // percent
    EE_INT("camera", "zoom_style", mZoomStyle);
    EE_ROW("camera", "zoom_lock", kSettingPercent, mZoomLock, kAnyMinimum, kAnyMaximum, false);

    EE_ROW("autosave", "interval_ms", kSettingLong, mAutoSaveInMilliseconds, 0, kAnyMaximum, false);
    EE_ROW("autosave", "count", kSettingLong, mNumberOfAutoSaves, 0, kAnyMaximum, false);

    EE_ROW("network", "host_game_name", kSettingWide, mHostGameName, 0, 0, false);
    EE_ROW("network", "adapter", kSettingLong, mNetworkAdapter, 0, kAnyMaximum, false);
    EE_INT("network.pt_rate", "min", mMinPTsPerSecond);
    EE_INT("network.pt_rate", "max", mMaxPTsPerSecond);
    EE_INT("network.pt_rate", "tuning_threshold", mPTRateTuningThreshold);   // percent
    EE_INT("network.ct_send_interval", "max", mMaxCTSendInterval);
    EE_INT("network.ct_send_interval", "step_down", mCTSendIntervalStepDown);
    EE_INT("network.ct_send_interval", "step_up", mCTSendIntervalStepUp);

    EE_BOOL("screenshots", "jpg", mTakeJPGScreenshots);
    EE_BOOL("screenshots", "end_game", mTakeEndGameScreenshots);

    EE_ROW("debug", "game_log", kSettingLongFlag, mGameLog, 0, 0, false);   // Logs/game-*.log (ee/EEGameLog.h)
    EE_BOOL("debug", "oos_handling", mDebugOOSHandling);

    EE_ROW("recent", "game_file", kSettingNarrow, mLastGameFile, 0, 0, false);
    EE_ROW("recent", "scenario_file", kSettingNarrow, mLastScenarioFile, 0, 0, false);
    EE_ROW("recent", "quick_save", kSettingWide, mLastQuickSave, 0, 0, false);
    EE_ROW("recent", "auto_save", kSettingWide, mLastAutoSave, 0, 0, false);
    EE_INT("recent", "post_game_stats_page", mLastOpenPostGameStatsPage);
#undef EE_BOOL
#undef EE_INT
#undef EE_ROW
}

// Writes one setting's member to the file
long EEWriteSetting(UConfig& inConfig, const EESetting& inRow)
{
    long theValue;
    switch (inRow.mKind) {
    case kSettingLong:
        return inConfig.SetValue(inRow.mBlock, inRow.mKey, *(long*)inRow.mField);
    case kSettingPercent:
        theValue = *(float*)inRow.mField * 100.0f;
        if (inRow.mClamp)
            theValue = theValue < inRow.mMinimum ? inRow.mMinimum : theValue > inRow.mMaximum ? inRow.mMaximum : theValue;
        return inConfig.SetValue(inRow.mBlock, inRow.mKey, theValue);
    case kSettingBool:
        return inConfig.SetFlag(inRow.mBlock, inRow.mKey, *(bool*)inRow.mField);
    case kSettingLongFlag:
        return inConfig.SetFlag(inRow.mBlock, inRow.mKey, *(long*)inRow.mField != 0);
    case kSettingWide:
        return inConfig.SetValue(inRow.mBlock, inRow.mKey, *(UWideString*)inRow.mField);
    case kSettingNarrow:
        return inConfig.SetValue(inRow.mBlock, inRow.mKey, UWideString(*(UString*)inRow.mField));
    case kSettingChars:
        return inConfig.SetValue(inRow.mBlock, inRow.mKey, UWideString((const char*)inRow.mField));
    }
    return kErrorNotFound;
}

// Reads one setting into its member; a missing value (or one out of range) is the default's, written to the file
void EEReadSetting(UConfig& inConfig, const EESetting& inRow, const EESetting& inDefault)
{
    long theValue;
    bool theFlag;
    UWideString theString;
    bool theRead = false;
    switch (inRow.mKind) {
    case kSettingLong:
    case kSettingPercent:
        if (inConfig.GetValue(inRow.mBlock, inRow.mKey, theValue) == 0) {
            if (inRow.mClamp)
                theValue = theValue < inRow.mMinimum ? inRow.mMinimum : theValue > inRow.mMaximum ? inRow.mMaximum : theValue;
            if (theValue >= inRow.mMinimum && theValue <= inRow.mMaximum) {
                theRead = true;
                if (inRow.mKind == kSettingLong)
                    *(long*)inRow.mField = theValue;
                else
                    *(float*)inRow.mField = theValue * 0.01f;
            }
        }
        if (!theRead) {
            if (inRow.mKind == kSettingLong)
                *(long*)inRow.mField = *(const long*)inDefault.mField;
            else
                *(float*)inRow.mField = *(const float*)inDefault.mField;
        }
        break;
    case kSettingBool:
    case kSettingLongFlag:
        theRead = inConfig.GetFlag(inRow.mBlock, inRow.mKey, theFlag) == 0;
        if (inRow.mKind == kSettingBool)
            *(bool*)inRow.mField = theRead ? theFlag : *(const bool*)inDefault.mField;
        else
            *(long*)inRow.mField = theRead ? (theFlag ? 1 : 0) : *(const long*)inDefault.mField != 0;
        break;
    case kSettingWide:
    case kSettingNarrow:
    case kSettingChars:
        theRead = inConfig.GetValue(inRow.mBlock, inRow.mKey, theString) == 0;
        if (inRow.mKind == kSettingWide)
            *(UWideString*)inRow.mField = theRead ? theString : *(const UWideString*)inDefault.mField;
        else if (inRow.mKind == kSettingNarrow)
            *(UString*)inRow.mField = theRead ? UString(theString) : *(const UString*)inDefault.mField;
        else
            snprintf((char*)inRow.mField, sizeof(EEGameSettings::mAudioDevice), "%s",
                     theRead ? (const char*)theString : (const char*)inDefault.mField);
        break;
    }
    if (!theRead)
        EEWriteSetting(inConfig, inRow);
}

// The defaults of the settings (a default-constructed EEGameSettings: not a copy of sDefaultSettings, whose game
// options' assignment reads the world rules, not loaded yet) and of the last game options (the original's literals)
void EEDefaultSettings(EEGameSettings& outDefaults)
{
    const EEGameSettings& theDefaults = EEGamePreferences::sDefaultSettings;
    outDefaults.mPlayerName = theDefaults.mPlayerName;       // the names EEInitializeDefaultSettings set
    outDefaults.mHostGameName = theDefaults.mHostGameName;
    EEGameOptions& o = outDefaults.mGameOptions;
    o.mMapSize = 1;
    o.SetMapType(UWideString("Continental"));
    o.mResources = 0;
    o.mEpochStart = kTechEpochPrehistoric;
    o.mEpochEnd = EEEpochCount() - 1;
    o.mUnitLimit = 500;
    o.mWondersForVictory = 3;
    o.mVariant = 2;
    o.mDifficulty = 1;
    o.mGameSpeed = 1;
    o.mRecordGames = false;
    o.mRevealMap = false;
    o.mAllowCustomCivs = true;
    o.mLockTeams = false;
    o.mLockSpeed = false;
    o.mCheatCodes = true;
    o.mVictory = true;
}

// Reads the rows of one part of the settings
void EEReadSettings(UConfig& inConfig, EEGameSettings& ioSettings, bool inGameOptions)
{
    EEGameSettings theDefaults;
    EEDefaultSettings(theDefaults);
    std::vector<EESetting> theRows, theDefaultRows;
    EESettingRows(ioSettings, inGameOptions, theRows);
    EESettingRows(theDefaults, inGameOptions, theDefaultRows);
    for (size_t i = 0; i < theRows.size(); i++)
        EEReadSetting(inConfig, theRows[i], theDefaultRows[i]);
}

// Writes the rows of one part of the settings -> the first failure, else 0
long EEWriteSettings(UConfig& inConfig, EEGameSettings& inSettings, bool inGameOptions)
{
    std::vector<EESetting> theRows;
    EESettingRows(inSettings, inGameOptions, theRows);
    long theResult = 0;
    for (const EESetting& theRow : theRows) {
        long theWritten = EEWriteSetting(inConfig, theRow);
        if (theResult == 0)
            theResult = theWritten;
    }
    return theResult;
}

}   // namespace

// FUNCTION: EXE 0x5328dd
// Reads every game setting from config.yaml, writing the default for each missing value (no blood in Korean),
// then loads the game options; returns their result
long EEGamePreferences::Load()
{
    const EEGameSettings& theDefaults = EEGamePreferences::sDefaultSettings;
    long theWidth;
    // port: no saved game size: a full screen game takes the display's size (EEInitializeVideo)
    gGameWindowSizeDefaulted = this->mConfig.GetValue("display.resolution", "width", theWidth) != 0;
    EEReadSettings(this->mConfig, this->mSettings, false);
    if (this->mSettings.mTextureResolution != 0 && this->mSettings.mTextureResolution != 2
        && this->mSettings.mTextureResolution != 4) {
        this->mSettings.mTextureResolution = theDefaults.mTextureResolution;
        this->mConfig.SetValue("graphics", "texture_resolution", this->mSettings.mTextureResolution);
    }
    if (!this->IsNotKorean())
        this->mSettings.mShowBlood = false;

    // the settings that are not stored
    this->mSettings.mRasterizerName = theDefaults.mRasterizerName;
    this->mSettings.mGameBitDepth = 32;
    this->mSettings.mTextureBitDepth = 32;
    this->mSettings.mSierraUserID = theDefaults.mSierraUserID;
    this->mSettings.mSierraGameName = theDefaults.mSierraGameName;
    this->mSettings.mWonNetPassword = theDefaults.mWonNetPassword;
    this->mSettings.mUseCandidateWindow = theDefaults.mUseCandidateWindow;
    this->mSettings.mStartingKeyboardLayout = theDefaults.mStartingKeyboardLayout;
    this->mSettings.mButtonMIPLevelsToDrop = theDefaults.mButtonMIPLevelsToDrop;
    this->mSettings.mUnitScaleFactor = theDefaults.mUnitScaleFactor;
    this->mSettings.mEnableAnimalHerds = theDefaults.mEnableAnimalHerds;
    this->mSettings.mUseAreaEffectOverlays = theDefaults.mUseAreaEffectOverlays;
    this->mSettings.mScreenMode = theDefaults.mScreenMode;
    this->mSettings.mL5BC = theDefaults.mL5BC;
    this->mSettings.mL5B8 = 0;
    this->mSettings.mShowAutoWaypoints = false;
    this->mSettings.mB5C0 = true;
    this->mSettings.mL53C = 15020;
    this->mSettings.mL540 = 0;
    this->mSettings.mB5C5 = true;
    this->mSettings.mB5C2 = true;
    this->mSettings.mB5CA = true;
    this->mSettings.mB5CB = true;
    this->mSettings.mB5CC = true;
    this->mSettings.mDisplayWeatherEffects = false;
    this->mSettings.mZoomEnabled = true;
    this->mSettings.mB5C4 = this->mSettings.mL560 = this->mSettings.mL55C = this->mSettings.mL558 = this->mSettings.mL554 = this->mSettings.mL550 = this->mSettings.mL54C = 0;
    this->mSettings.mB5C6 = false;
    this->mSettings.mAntiAliasing = false;
    // the game's own location is where it runs from (the data root), never a stored path: a play folder can move
    this->mSettings.mInstalledFromVolume = UWideString(gStartupVolume);
    this->mSettings.mInstalledFromVolume.ToUpper();
    this->mSettings.mInstalledFromDirectory = UWideString(gStartupDirectory);
    this->mSettings.mInstalledFromDirectory.ToUpper();

    // port: full screen or windowed is the "window_mode" setting; the frame rate limit decides v-sync
    this->mSettings.mFullScreen = this->mSettings.mWindowMode == 0;
    this->mSettings.mWaitForVSync = this->mSettings.mFrameRateLimit == 0;
    EEApplyPortSettings(this->mSettings);
    EESetTextureResolution(this->mSettings.mTextureResolution);

    return this->LoadGameOptions();
}

// FUNCTION: EXE 0x53505e
// Reads the last game options from config.yaml's game_options block, writing defaults for missing values; returns 0
long EEGamePreferences::LoadGameOptions()
{
    EEReadSettings(this->mConfig, this->mSettings, true);
    return 0;
}

// FUNCTION: EXE 0x535943
// Writes every game setting to config.yaml, then the game options; returns the first failure (0 when every value
// was written)
long EEGamePreferences::Save()
{
    long theResult = EEWriteSettings(this->mConfig, this->mSettings, false);
    long theOptions = this->SaveGameOptions();
    return theResult != 0 ? theResult : theOptions;
}

// FUNCTION: EXE 0x53670d
// Writes the last game options to config.yaml's game_options block; returns the first failure, else 0
long EEGamePreferences::SaveGameOptions()
{
    return EEWriteSettings(this->mConfig, this->mSettings, true);
}

// FUNCTION: EXE 0x536b49
// Remembers the options of a new random-map game as the last game options, then saves the preferences
void EEGamePreferences::SetLastGameOptions(const EEGameOptions& inOptions)
{
    if (inOptions.mLoadType == 0)
        this->mSettings.mGameOptions = inOptions;
    this->Save();
}
