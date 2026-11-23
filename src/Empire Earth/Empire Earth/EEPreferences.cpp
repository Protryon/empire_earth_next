// C:\EEX\Empire Earth\Empire Earth\EEPreferences.cpp (file name inferred: no __FILE__ or RTTI; it sorts between
// EEMusicManager.cpp and EETeam.cpp). 0x13242b..0x136b6d: the game preferences (config.yaml; the original: the registry). EEGamePreferences::sDefaultSettings'
// init chain first, then EEGameSettings' dtor and ctor (the defaults), EEGamePreferences' ctor/dtor, IsNotKorean,
// the resource defaults, Load (+ the game options), Save (+ the game options) and SetLastGameOptions.
// The settings file calls go through the UConfig member (the original's URegistry at +0).
#include <windows.h>
#include "gx/GX.h"

#include <vector>

// (gx/GX.h: out of line, for EEScreens.cpp and Empire Earth.cpp, which declare it without the header)
void GXGameSize(unsigned long inPercent, bool inWindowed, unsigned long* outWidth, unsigned long* outHeight)
{
    gx_game_size(inPercent, inWindowed, outWidth, outHeight);
}

#include "lle/LowLevelEngine.h"
#include "platform/PlatFile.h"
#include "lle/UResourceFile.h"
#include "lle/GETextureManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEStringIDs.h"
#include "ee/EETechTreeTechID.h"
#include "rust/game/gamecpp.h"   // round 11, lane B (ee_game gamecpp preferences.rs)
// the settings are kept in config.yaml (lle/UConfig.h; the original kept them in the registry key
// HKCU\Software\Mad Doc Software\EE-AOC and its "Game Options" subkey)

// 0x529b90 (init chain 0x13242b: ctor 0x132435, atexit 0x13243f, guarded dtor 0x13244b)
EEGameSettings EEGamePreferences::sDefaultSettings;

bool gGameWindowSizeDefaulted;   // port: no saved game window size (EEPreferences Load)

// port: the texture packs (docs/DATA_FORMATS.md "Texture packs"; ee_game gamecpp preferences.rs)
void EESetTextureResolution(long inResolution) { ee_gamecpp_prefs_texture_resolution(inResolution); }

// port: see EEGamePreferences.h (the camera's farthest zoom, the renderer's and the mixer's settings)
void EEApplyPortSettings(const EEGameSettings& inSettings) { ee_gamecpp_prefs_apply_port(const_cast<EEGameSettings*>(&inSettings)); }

// Destroys the game settings (members only)
EEGameSettings::~EEGameSettings()
{
}

// Creates the default game settings (Direct3D, volumes, scroll speeds, display, network and game options): the plain
// members' are ee_app settings.rs's, the strings empty
EEGameSettings::EEGameSettings()
{
    ee_app_settings_default(this);
    this->mFullScreen = true;   // (also Rust's default) the test profile's windowed overlay patches this line (tools/build.py)
    this->mRasterizerName = "Direct3D";
}

// Creates the game preferences with default settings
EEGamePreferences::EEGamePreferences()
{
}

// Destroys the game preferences (members only)
EEGamePreferences::~EEGamePreferences()
{
}

// Returns whether neither the game resources nor the system language is Korean
bool EEGamePreferences::IsNotKorean() { return ee_gamecpp_prefs_is_not_korean(); }

// Sets the default player, host and Sierra names to the "unknown" string and the WON password to ""
void EEInitializeDefaultSettings() { ee_gamecpp_prefs_init_defaults(); }

// Reads every game setting from config.yaml, writing the default for each missing value (no blood in Korean),
// then loads the game options; returns their result (the rows and the rest: ee_game gamecpp preferences.rs, ee_app
// settings.rs)
long EEGamePreferences::Load()
{
    long theResult = ee_gamecpp_prefs_load(this);
    // (also after_load's) full screen is window mode 0; the test profile's windowed overlay patches this line
    this->mSettings.mFullScreen = this->mSettings.mWindowMode == 0;
    return theResult;
}

// Reads the last game options from config.yaml's game_options block, writing defaults for missing values; returns 0
long EEGamePreferences::LoadGameOptions() { return ee_gamecpp_prefs_load_options(this); }

// Writes every game setting to config.yaml, then the game options; returns the first failure (0 when every value
// was written)
long EEGamePreferences::Save() { return ee_gamecpp_prefs_save(this); }

// Writes the last game options to config.yaml's game_options block; returns the first failure, else 0
long EEGamePreferences::SaveGameOptions() { return ee_gamecpp_prefs_save_options(this); }

// Remembers the options of a new random-map game as the last game options, then saves the preferences
void EEGamePreferences::SetLastGameOptions(const EEGameOptions& inOptions) { ee_gamecpp_prefs_set_last_options(this, &inOptions); }
