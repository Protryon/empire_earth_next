// (RECONSTRUCTION; file name inferred; class and member names are provisional). The
// game settings at 0x530308, kept in config.yaml (the original: the registry; vptr URegistry's after its inline ctor 0x13281e; an EEGameOptions at +4;
// init chain 0x13eaeb). The lobby's Start stores the started game's options (0x136b49: copies them when it is a new
// game, then saves).
#ifndef EE_EEGAMEPREFERENCES_H
#define EE_EEGAMEPREFERENCES_H

// Requires ee/EEGameOptions.h (and lle/UString.h, lle/UWideString.h).
#include "lle/UConfig.h"

// The preference values (sizeof 0x5e0): the ctor 0x1324f8 (the defaults) and the dtor
// 0x132466 are out of line in Empire Earth's preferences unit; the copy ctor is implicit (the game settings unit's
// COMDAT 0x1d720d copies every member: EEGameOptions' copy ctor, 7 UWideString and 8 UString copy ctors, 42 dword and
// 32 byte moves). The settings events pass it by value. Member types from the ctor's stores (fld vs mov), names from
// the registry keys of the save 0x135943 where it writes the member, else by offset (provisional name). Several members
// are
// also declared as separate globals by other units (the addresses in the comments): they are these members.
class EEGameSettings {
public:
    EEGameSettings();                       // 0x1324f8
    ~EEGameSettings();                      // 0x132466
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
    float mMouseSensitivity;                // +0x518 "Mouse Sensitivity"
    float mSideScrollSpeed;                 // +0x51c "Side Scroll Speed"
    float mGrabScrollSpeed;                 // +0x520 "Grab Scroll Speed"
    float mSoundVolume;                     // +0x524 "Sound Volume"
    float mMusicVolume;                     // +0x528 "Music Volume"
    float mMaxNumberOfSounds;               // +0x52c
    float mUnitScaleFactor;                 // +0x530
    float mZoomLock;                        // +0x534
    long mZoomStyle;                        // +0x538
    long mL53C;                             // +0x53c
    long mL540;                             // +0x540
    long mScreenMode;                       // +0x544
    long mButtonMIPLevelsToDrop;            // +0x548
    long mL54C;                             // +0x54c
    long mL550;                             // +0x550
    long mL554;                             // +0x554
    long mL558;                             // +0x558
    long mL55C;                             // +0x55c
    long mL560;                             // +0x560
    long mComputerPlayerDifficultyLevel;    // +0x564 0x530870: gDifficultyLevel
    long mGameBitDepth;                     // +0x568 "Game Bit Depth"
    unsigned long mGameWindowHeight;        // +0x56c "Game Window Height" (unsigned: Empire Earth.cpp 0x140251's fild qword)
    unsigned long mGameWindowWidth;         // +0x570 "Game Window Width"
    long mCTSendIntervalStepUp;             // +0x574
    long mCTSendIntervalStepDown;           // +0x578
    long mMaxPTsPerSecond;                  // +0x57c "Max PTs per Second"
    long mMinPTsPerSecond;                  // +0x580 "Min PTs per Second"
    long mMaxCTSendInterval;                // +0x584
    long mMIPLevelsToDrop;                  // +0x588
    long mModelDetail;                      // +0x58c
    long mEffectDetail;                     // +0x590
    long mNetworkAdapter;                   // +0x594 "Network Adapter"; 0x5308a0: gU5308A0
    unsigned long mPTRateTuningThreshold;   // +0x598 "PT Rate Tuning Threshold (percent)" (EEServer: fild qword)
    long mScreenID;                         // +0x59c "Screen ID"; 0x5308a8: gScreenIndex
    long mStartingKeyboardLayout;           // +0x5a0
    long mTerrainMIPLevelsToDrop;           // +0x5a4
    long mTextureBitDepth;                  // +0x5a8 "Texture Bit Depth"
    unsigned long mNumberOfAutoSaves;       // +0x5ac "Number Of AutoSaves" (EEPhysics 0x151d1f: jbe)
    unsigned long mAutoSaveInMilliseconds;  // +0x5b0 "AutoSave In Milliseconds"
    long mLastOpenPostGameStatsPage;        // +0x5b4
    long mL5B8;                             // +0x5b8
    long mL5BC;                             // +0x5bc
    bool mB5C0;                             // +0x5c0
    bool mZoomEnabled;                      // +0x5c1 what include/ee/EmpireEarth.h calls gZoomEnabled (0x5308cd)
    bool mB5C2;                             // +0x5c2
    bool mAnimationSmoothing;               // +0x5c3
    bool mB5C4;                             // +0x5c4
    bool mB5C5;                             // +0x5c5
    bool mB5C6;                             // +0x5c6
    bool mDisplayClouds;                    // +0x5c7
    bool mDisplayShadows;                   // +0x5c8
    bool mDisplayBattlefieldAmbients;       // +0x5c9
    bool mB5CA;                             // +0x5ca
    bool mB5CB;                             // +0x5cb
    bool mB5CC;                             // +0x5cc
    bool mEnableAnimalHerds;                // +0x5cd
    bool mTrilinearFilter;                  // +0x5ce
    bool mFullScreen;                       // +0x5cf (show the start screen: EEStartScreen::Start/Stop)
    bool mReverbEnabled;                    // +0x5d0 "Reverb Enabled"
    bool mWaitForVSync;                     // +0x5d1 "Wait for VSync"
    bool mAmbientSounds;                    // +0x5d2 "Ambient Sounds"
    bool mShowBlood;                        // +0x5d3
    bool mMusicEnabled;                     // +0x5d4
    bool mRightHandMouseOrientation;        // +0x5d5
    bool mB5D6;                             // +0x5d6
    bool mUnitLighting;                     // +0x5d7
    bool mUseAreaEffectOverlays;            // +0x5d8
    bool mUseCandidateWindow;               // +0x5d9
    bool mDisplayWeatherEffects;            // +0x5da
    bool mDebugOOSHandling;                 // +0x5db
    bool mTakeJPGScreenshots;               // +0x5dc
    bool mAntiAliasing;                     // +0x5dd
    bool mTakeEndGameScreenshots;           // +0x5de
    bool mShowAutoWaypoints;                // +0x5df
    // port (not in the original's 0x5e0 bytes): "Texture Resolution" 0 (the original textures), 2 or 4 (the
    // Data/Textures-4x pack, drawn at half size for 2); EESetTextureResolution applies it
    long mTextureResolution;
    // port: the modern display and audio settings (config.yaml; EEApplyPortSettings applies them)
    long mUIScale;                          // "UI Scale" percent; 0 Auto (GXAutoUIScale): the game size is the display's
    long mRenderResolution;                 // "Render Resolution" percent of the display's pixels (50..100)
    long mWindowMode;                       // "Window Mode" 0 full screen (borderless), 1 windowed
    long mFrameRateLimit;                   // "Frame Rate Limit" 0 v-sync, -1 unlimited, else frames a second
    long mTextureFiltering;                 // "Texture Filtering" 0 bilinear, 1 trilinear, 2 anisotropic
    long mBrightness;                       // "Brightness" percent (100 unchanged)
    long mMasterVolume;                     // "Master Volume" percent
    bool mMuteInBackground;                 // "Mute in Background"
    long mMaximumZoomOut;                   // "Maximum Zoom Out" percent of the original's farthest (100..300)
    long mGameLog;                          // "Game Log": 1 writes Logs/game-*.log for every game (ee/EEGameLog.h)
    char mAudioDevice[128];                 // "Audio Device" (empty: the system's default)
};

// port: makes the texture manager load the world's textures at inResolution (0, 2 or 4: EEGameSettings::
// mTextureResolution); textures already loaded keep theirs until released (EEPreferences.cpp)
void EESetTextureResolution(long inResolution);

// port: puts a settings' modern display and audio settings into effect (camera, GX, mixer; EEPreferences.cpp)
void EEApplyPortSettings(const EEGameSettings& inSettings);

class EEGamePreferences {
public:
    // out of line (Empire Earth.cpp's chain 0x13eaeb jumps to them; the vptr is URegistry's, not modelled)
    EEGamePreferences();                    // 0x13281e
    ~EEGamePreferences();                   // 0x132834
    const EEGameOptions& GetLastGameOptions() const { return this->mSettings.mGameOptions; }
    // never inlined: every caller (game settings 0x1d71c9.., 0x9b0db, 0x113685) calls the COMDAT 0x1d99fb
    // `lea eax, [ecx+4]` (provisional name)
    EEGameSettings& GetSettings() { return this->mSettings; }
    void SetLastGameOptions(const EEGameOptions& inOptions);   // 0x136b49
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

extern EEGamePreferences gGamePreferences;  // 0x530308
// 0x13286a: the resource defaults of gDefaultSettings' names (Empire Earth.cpp calls it before Load; provisional name)
void EEInitializeDefaultSettings();

#endif
