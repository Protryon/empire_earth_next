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
// Creates the default game settings (Direct3D, volumes, scroll speeds, display, network and game options): the plain
// members' are ee_app settings.rs's
EEGameSettings::EEGameSettings()
{
    ee_app_settings_default(this);
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

// port: the settings of config.yaml, one row each: the rows, their reads (the default written back for a missing or
// out-of-range value) and writes are ee_app settings.rs's (docs/DATA_FORMATS.md "config.yaml"); the C++ string
// members are reached through the callback below.
namespace {

struct EESettingsStrings {
    EEGameSettings* mSettings;
    const EEGameSettings* mDefaults;
};

// a string row's member (ee_app settings.rs EE_APP_SETTINGS_*): the wide ones, else NULL
UWideString* EESettingWide(EEGameSettings& s, uint32_t inID)
{
    switch (inID) {
    case EE_APP_SETTINGS_PLAYER_NAME: return &s.mPlayerName;
    case EE_APP_SETTINGS_HOST_GAME_NAME: return &s.mHostGameName;
    case EE_APP_SETTINGS_LAST_QUICK_SAVE: return &s.mLastQuickSave;
    case EE_APP_SETTINGS_LAST_AUTO_SAVE: return &s.mLastAutoSave;
    case EE_APP_SETTINGS_MAP_TYPE: return &s.mGameOptions.mMapType;
    }
    return NULL;
}
UString& EESettingNarrow(EEGameSettings& s, uint32_t inID)
{
    return inID == EE_APP_SETTINGS_LAST_GAME_FILE ? s.mLastGameFile : s.mLastScenarioFile;
}

uint32_t EESettingsString(void* inContext, int32_t inOp, uint32_t inID, char* ioBuffer, uint32_t inSize)
{
    EESettingsStrings* theContext = static_cast<EESettingsStrings*>(inContext);
    EEGameSettings& s = *theContext->mSettings;
    UWideString* theWide = EESettingWide(s, inID);
    switch (inOp) {
    case EE_APP_SETTINGS_STRING_LENGTH:
    case EE_APP_SETTINGS_STRING_GET: {
        UWideString theText = theWide != NULL ? *theWide : UWideString(EESettingNarrow(s, inID));
        if (inOp == EE_APP_SETTINGS_STRING_LENGTH)
            return (uint32_t)strlen((const char*)theText);
        snprintf(ioBuffer, inSize, "%s", (const char*)theText);
        return 0;
    }
    case EE_APP_SETTINGS_STRING_SET:
        if (theWide != NULL)
            *theWide = ioBuffer;
        else
            EESettingNarrow(s, inID) = UString(UWideString(ioBuffer));
        return 0;
    case EE_APP_SETTINGS_STRING_DEFAULT: {
        EEGameSettings& d = const_cast<EEGameSettings&>(*theContext->mDefaults);
        if (theWide != NULL)
            *theWide = *EESettingWide(d, inID);
        else
            EESettingNarrow(s, inID) = EESettingNarrow(d, inID);
        return 0;
    }
    }
    return 0;
}

// The defaults of the settings (a default-constructed EEGameSettings: not a copy of sDefaultSettings, whose game
// options' assignment reads the world rules, not loaded yet) and of the last game options (the original's literals)
void EEDefaultSettings(EEGameSettings& outDefaults)
{
    const EEGameSettings& theDefaults = EEGamePreferences::sDefaultSettings;
    outDefaults.mPlayerName = theDefaults.mPlayerName;       // the names EEInitializeDefaultSettings set
    outDefaults.mHostGameName = theDefaults.mHostGameName;
    ee_app_settings_last_options_default(&outDefaults.mGameOptions, EEEpochCount());
    outDefaults.mGameOptions.SetMapType(UWideString("Continental"));
}

// Reads the rows of one part of the settings
void EEReadSettings(EEGameSettings& ioSettings, bool inGameOptions)
{
    EEGameSettings theDefaults;
    EEDefaultSettings(theDefaults);
    EESettingsStrings theContext = {&ioSettings, &theDefaults};
    ee_app_settings_read(&ioSettings, &ioSettings.mGameOptions, &theDefaults, &theDefaults.mGameOptions, inGameOptions,
                         &theContext, EESettingsString);
}

// Writes the rows of one part of the settings -> the first failure, else 0
long EEWriteSettings(EEGameSettings& inSettings, bool inGameOptions)
{
    EESettingsStrings theContext = {&inSettings, &inSettings};
    return ee_app_settings_write(&inSettings, &inSettings.mGameOptions, inGameOptions, &theContext, EESettingsString);
}

}   // namespace

// FUNCTION: EXE 0x5328dd
// Reads every game setting from config.yaml, writing the default for each missing value (no blood in Korean),
// then loads the game options; returns their result (the rows and the rest: ee_app settings.rs)
long EEGamePreferences::Load()
{
    const EEGameSettings& theDefaults = EEGamePreferences::sDefaultSettings;
    long theWidth;
    // port: no saved game size: a full screen game takes the display's size (EEInitializeVideo)
    gGameWindowSizeDefaulted = this->mConfig.GetValue("display.resolution", "width", theWidth) != 0;
    EEReadSettings(this->mSettings, false);
    // the texture resolution (0, 2 or 4), the blood, the settings that are not stored and the port's switches
    if (ee_app_settings_after_load(&this->mSettings, &theDefaults, !this->IsNotKorean()))
        this->mConfig.SetValue("graphics", "texture_resolution", this->mSettings.mTextureResolution);
    this->mSettings.mRasterizerName = theDefaults.mRasterizerName;
    this->mSettings.mSierraUserID = theDefaults.mSierraUserID;
    this->mSettings.mSierraGameName = theDefaults.mSierraGameName;
    this->mSettings.mWonNetPassword = theDefaults.mWonNetPassword;
    // the game's own location is where it runs from (the data root), never a stored path: a play folder can move
    this->mSettings.mInstalledFromVolume = UWideString(gStartupVolume);
    this->mSettings.mInstalledFromVolume.ToUpper();
    this->mSettings.mInstalledFromDirectory = UWideString(gStartupDirectory);
    this->mSettings.mInstalledFromDirectory.ToUpper();
    EEApplyPortSettings(this->mSettings);
    EESetTextureResolution(this->mSettings.mTextureResolution);

    return this->LoadGameOptions();
}

// FUNCTION: EXE 0x53505e
// Reads the last game options from config.yaml's game_options block, writing defaults for missing values; returns 0
long EEGamePreferences::LoadGameOptions()
{
    EEReadSettings(this->mSettings, true);
    return 0;
}

// FUNCTION: EXE 0x535943
// Writes every game setting to config.yaml, then the game options; returns the first failure (0 when every value
// was written)
long EEGamePreferences::Save()
{
    long theResult = EEWriteSettings(this->mSettings, false);
    long theOptions = this->SaveGameOptions();
    return theResult != 0 ? theResult : theOptions;
}

// FUNCTION: EXE 0x53670d
// Writes the last game options to config.yaml's game_options block; returns the first failure, else 0
long EEGamePreferences::SaveGameOptions()
{
    return EEWriteSettings(this->mSettings, true);
}

// FUNCTION: EXE 0x536b49
// Remembers the options of a new random-map game as the last game options, then saves the preferences
void EEGamePreferences::SetLastGameOptions(const EEGameOptions& inOptions)
{
    if (inOptions.mLoadType == 0)
        this->mSettings.mGameOptions = inOptions;
    this->Save();
}
