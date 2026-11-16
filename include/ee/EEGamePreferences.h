// (RECONSTRUCTION; file name inferred; class and member names are provisional). The
// game settings at 0x530308, kept in config.yaml (the original: the registry; vptr URegistry's after its inline ctor 0x13281e; an EEGameOptions at +4;
// init chain 0x13eaeb). The lobby's Start stores the started game's options (0x136b49: copies them when it is a new
// game, then saves).
#ifndef EE_EEGAMEPREFERENCES_H
#define EE_EEGAMEPREFERENCES_H

// Requires ee/EEGameOptions.h (and lle/UString.h, lle/UWideString.h).
#include "lle/UConfig.h"
#include "rust/ee_app.h"

// The preference values (sizeof 0x5e0 in the original): the ctor 0x1324f8 (the defaults: ee_app settings.rs) and the
// dtor 0x132466 are out of line in Empire Earth's preferences unit; the copy ctor is implicit (the game settings unit's
// COMDAT 0x1d720d copies every member: EEGameOptions' copy ctor, 7 UWideString and 8 UString copy ctors, 42 dword and
// 32 byte moves). The settings events pass it by value. Member types from the ctor's stores (fld vs mov), names from
// the registry keys of the save 0x135943 where it writes the member, else by offset (provisional name). Several members
// are
// also declared as separate globals by other units (the addresses in the comments): they are these members.
class EEGameSettings : public EeAppSettingsFields {
public:
    EEGameSettings();
    ~EEGameSettings();
    // inline (the local servers' ctor argument is loaded into eax before the other pushes: SIE Start 0x1e8310, SPE
    // RandomMap 0x1e8d4c; provisional name)
    long GetNetworkAdapter() const { return this->mNetworkAdapter; }
    long GetScreenID() const { return this->mScreenID; }  // inline (Empire Earth.cpp loads it into a register first; 0x14382b)
    long GetScreenMode() const { return this->mScreenMode; }  // inline (same sign: 0x14265e ActivateScreen)
    float GetSideScrollSpeed() const { return this->mSideScrollSpeed; }  // inline (EEAScroll 0x478e loads it first)
    bool GetFullScreen() const { return this->mFullScreen; }  // inline (Empire Earth.cpp 0x140306/0x1444d0)

    EEGameOptions mGameOptions;             // +0x000 (the single-player random map starts from it)
    UWideString mHostGameName;              // +0x40c "Host Game Name"
    // inline: the grab scroll event copies the value to a temporary before its GetValue call (0x1d9a51)
    float GetGrabScrollSpeed() const { return this->mGrabScrollSpeed; }
    float GetMaxNumberOfSounds() const { return this->mMaxNumberOfSounds; }  // inline (EEInitializeSounds 0x543557 copies it to a frame temp before GetMaxChannels' call)
    float GetMusicVolume() const { return this->mMusicVolume; }  // inline (the music fades compare it as a value)
    UWideString mLoadGameName;              // +0x420
    UWideString mLastQuickSave;             // +0x434
    UWideString mLastAutoSave;              // +0x448
    UWideString mPlayerName;                // +0x45c "Player Name"; 0x530768: gPlayerName
    UWideString mSierraUserID;              // +0x470 "Sierra.com User ID"
    UWideString mSierraGameName;            // +0x484 "Sierra.com Game Name"; 0x530790: EmpireEarth.h's gHostedGameName
    UString mRasterizerName;                // +0x498 "Rasterizer Name"
    UString mInstalledFromVolume;           // +0x4a8
    UString mInstalledFromDirectory;        // +0x4b8
    // inline setters (SLE SaveFile's inline budget needs them with EEUIManager::GetInScenarioEditor)
    void SetLoadSavedGameName(const UString& inName) { this->mLastGameFile = inName; }
    void SetLoadScenarioName(const UString& inName) { this->mLastScenarioFile = inName; }
    UString mLastGameFile;                  // +0x4e8 0x5307f4: gLoadSavedGameName
    UString mLastScenarioFile;              // +0x4f8 0x530804: gLoadScenarioName
    UString mWonNetPassword;                // +0x508 "WonNet Password" (encrypted)
    // the plain members (mMouseSensitivity .. mAudioDevice, the original's +0x518 .. +0x5df and the port's) are
    // EeAppSettingsFields, the base (rust/ee_app.h; ee_app settings.rs: their names, types, order and defaults)
};

// port: makes the texture manager load the world's textures at inResolution (0, 2 or 4: EEGameSettings::
// mTextureResolution); textures already loaded keep theirs until released (EEPreferences.cpp)
void EESetTextureResolution(long inResolution);

// port: puts a settings' modern display and audio settings into effect (camera, GX, mixer; EEPreferences.cpp)
void EEApplyPortSettings(const EEGameSettings& inSettings);

class EEGamePreferences {
public:
    // out of line (Empire Earth.cpp's chain 0x13eaeb jumps to them; the vptr is URegistry's, not modelled)
    EEGamePreferences();
    ~EEGamePreferences();
    const EEGameOptions& GetLastGameOptions() const { return this->mSettings.mGameOptions; }
    // never inlined: every caller (game settings 0x1d71c9.., 0x9b0db, 0x113685) calls the COMDAT 0x1d99fb
    // `lea eax, [ecx+4]` (provisional name)
    EEGameSettings& GetSettings() { return this->mSettings; }
    void SetLastGameOptions(const EEGameOptions& inOptions);
    // 0x132848: false when the resource language or the OS language is Korean (0x412); the video tab shows check box
    // 13 only when true (provisional name)
    bool IsNotKorean();
    long Save();                            // 0x135943: writes the settings to config.yaml (the settings' Ok
                                            // returns its result)
    // (provisional names)
    long Load();                            // 0x1328dd: reads the settings from config.yaml (defaults where missing)
    long LoadGameOptions();                 // 0x13505e: the game_options block (Load's tail)
    long SaveGameOptions();                 // 0x13670d (Save's tail)

    // +0x000 in the original: a URegistry member, not a base (the ctor 0x13281e and dtor 0x132834 store only its vptr);
    // the settings file (lle/UConfig.h) in its place
    UConfig mConfig;
    EEGameSettings mSettings;               // +0x004 (was `EEGameOptions mLastGameOptions`: its first member)

    // 0x529b90: the default settings, a CLASS STATIC (its init chain's atexit dtor 0x13244b is guarded; owning class
    // and provisional name). Its mSoundVolume/mMusicVolume (0x52a0b4/0x52a0b8) are the load's
    // defaults and the audio tab's comparison values.
    static EEGameSettings sDefaultSettings;
};

extern EEGamePreferences gGamePreferences;
// 0x13286a: the resource defaults of gDefaultSettings' names (Empire Earth.cpp calls it before Load; provisional name)
void EEInitializeDefaultSettings();

#endif
