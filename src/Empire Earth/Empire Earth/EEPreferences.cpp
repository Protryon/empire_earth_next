// C:\EEX\Empire Earth\Empire Earth\EEPreferences.cpp (file name inferred: no __FILE__ or RTTI; it sorts between
// EEMusicManager.cpp and EETeam.cpp). 0x13242b..0x136b6d: the registry-backed game preferences. EEGamePreferences::sDefaultSettings'
// init chain first, then EEGameSettings' dtor and ctor (the defaults), EEGamePreferences' ctor/dtor, IsNotKorean,
// the resource defaults, Load (+ the "Game Options" key), Save (+ the "Game Options" key) and SetLastGameOptions.
// The registry calls go through the URegistry member at +0.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/UEncryption.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEStringIDs.h"
#include "ee/EETechTreeTechID.h"
#define kEERootKey ((long)HKEY_CURRENT_USER)
#define kEERegistryKey "Software\\Mad Doc Software\\EE-AOC"
#define kEEGameOptionsKey "Software\\Mad Doc Software\\EE-AOC\\Game Options"
#define kEEOriginalRegistryKey "Software\\SSSI\\Empire Earth"

// 0x529b90 (init chain 0x13242b: ctor 0x132435, atexit 0x13243f, guarded dtor 0x13244b)
EEGameSettings EEGamePreferences::sDefaultSettings;
// SYNTHETIC: EXE 0x53242b   (wrapper)
// SYNTHETIC: EXE 0x532435   (ctor)
// SYNTHETIC: EXE 0x53243f   (registrar)
// SYNTHETIC: EXE 0x53244b   (guarded dtor)

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
    this->mOriginalInstalledFromDirectory = "";
    this->mOriginalInstalledFromVolume = "";
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

// FUNCTION: EXE 0x5328dd
// Reads every game setting from the registry, writing the default for each missing value (no blood in Korean),
// then loads the game options; returns their result
long EEGamePreferences::Load()
{
    UWideString theVolume(gStartupVolume);
    UWideString theDirectory(gStartupDirectory);
    UWideString theString;
    UString thePassword;
    UString theEncryptedPassword;
    long theValue;
    long theResult;
    char theDirectoryBuffer[500];

    this->mRegistry.CreateRegistryKey(kEERootKey, UWideString(kEERegistryKey));
    this->mRegistry.CreateRegistryKey(kEERootKey, UWideString(kEEGameOptionsKey));
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Rasterizer Name"), theString);
    if (theResult == 0) {
        this->mSettings.mRasterizerName = theString;
    } else {
        this->mSettings.mRasterizerName = EEGamePreferences::sDefaultSettings.mRasterizerName;
        theString = this->mSettings.mRasterizerName;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Rasterizer Name"), theString);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Mouse Sensitivity"), theValue);
    if (theResult == 0) {
        if (theValue <= 1)
            theValue = 1;
        this->mSettings.mMouseSensitivity = theValue * 0.01f;
    } else {
        this->mSettings.mMouseSensitivity = EEGamePreferences::sDefaultSettings.mMouseSensitivity;
        theValue = this->mSettings.mMouseSensitivity * 100.0f;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Mouse Sensitivity"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Side Scroll Speed"), theValue);
    if (theResult == 0) {
        this->mSettings.mSideScrollSpeed = theValue * 0.01f;
    } else {
        this->mSettings.mSideScrollSpeed = EEGamePreferences::sDefaultSettings.mSideScrollSpeed;
        theValue = this->mSettings.mSideScrollSpeed * 100.0f;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Side Scroll Speed"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Grab Scroll Speed"), theValue);
    if (theResult == 0) {
        this->mSettings.mGrabScrollSpeed = theValue * 0.01f;
    } else {
        this->mSettings.mGrabScrollSpeed = EEGamePreferences::sDefaultSettings.mGrabScrollSpeed;
        theValue = this->mSettings.mGrabScrollSpeed * 100.0f;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Grab Scroll Speed"), theValue);
    }
    this->mSettings.mScreenMode = EEGamePreferences::sDefaultSettings.mScreenMode;
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Game Bit Depth"), theValue);
    if (theResult == 0) {
        this->mSettings.mGameBitDepth = theValue;
    } else {
        this->mSettings.mGameBitDepth = EEGamePreferences::sDefaultSettings.mGameBitDepth;
        theValue = this->mSettings.mGameBitDepth;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Game Bit Depth"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Network Adapter"), theValue);
    if (theResult == 0) {
        this->mSettings.mNetworkAdapter = theValue;
    } else {
        this->mSettings.mNetworkAdapter = EEGamePreferences::sDefaultSettings.mNetworkAdapter;
        theValue = this->mSettings.mNetworkAdapter;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Network Adapter"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Game Window Height"), theValue);
    if (theResult == 0) {
        this->mSettings.mGameWindowHeight = theValue;
    } else {
        this->mSettings.mGameWindowHeight = EEGamePreferences::sDefaultSettings.mGameWindowHeight;
        theValue = this->mSettings.mGameWindowHeight;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Game Window Height"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Game Window Width"), theValue);
    if (theResult == 0) {
        this->mSettings.mGameWindowWidth = theValue;
    } else {
        this->mSettings.mGameWindowWidth = EEGamePreferences::sDefaultSettings.mGameWindowWidth;
        theValue = this->mSettings.mGameWindowWidth;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Game Window Width"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Screen ID"), theValue);
    if (theResult == 0) {
        this->mSettings.mScreenID = theValue;
    } else {
        this->mSettings.mScreenID = EEGamePreferences::sDefaultSettings.mScreenID;
        theValue = this->mSettings.mScreenID;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Screen ID"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Texture Bit Depth"), theValue);
    if (theResult == 0) {
        this->mSettings.mTextureBitDepth = theValue;
    } else {
        this->mSettings.mTextureBitDepth = EEGamePreferences::sDefaultSettings.mTextureBitDepth;
        theValue = this->mSettings.mTextureBitDepth;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Texture Bit Depth"), theValue);
    }
    this->mSettings.mL5BC = EEGamePreferences::sDefaultSettings.mL5BC;
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Wait for VSync"), theValue);
    if (theResult == 0) {
        this->mSettings.mWaitForVSync = theValue != 0;
    } else {
        this->mSettings.mWaitForVSync = EEGamePreferences::sDefaultSettings.mWaitForVSync;
        theValue = this->mSettings.mWaitForVSync != false;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Wait for VSync"), theValue);
    }
    this->mSettings.mFullScreen = true;
    this->mSettings.mL5B8 = 0;
    this->mSettings.mShowAutoWaypoints = false;
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Installed From Volume"), theString);
    if (theResult == 0) {
        this->mSettings.mInstalledFromVolume = theString;
    } else {
        this->mSettings.mInstalledFromVolume = theVolume;
        if (this->mSettings.mInstalledFromVolume.GetLength() != 0) {
            theString = this->mSettings.mInstalledFromVolume;
            this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Installed From Volume"), theString);
        } else {
            GetCurrentDirectoryA(499, theDirectoryBuffer);
            theString = theDirectoryBuffer;
            if (theString.GetLength() > 1) {
                int i = theString[0] == '\\' ? 2 : 0;
                while (i < theString.GetLength() && theString[i] != '\\')
                    i++;
                if (i < theString.GetLength()) {
                    theString = theString.GetIndexedSubstring(0, i - 1);
                    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Installed From Volume"), theString);
                    this->mSettings.mInstalledFromVolume = theString;
                }
            }
        }
    }
    this->mSettings.mInstalledFromVolume.ToUpper();
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Installed From Directory"), theString);
    if (theResult == 0) {
        this->mSettings.mInstalledFromDirectory = theString;
    } else {
        this->mSettings.mInstalledFromDirectory = theDirectory;
        if (this->mSettings.mInstalledFromDirectory.GetLength() != 0) {
            theString = this->mSettings.mInstalledFromDirectory;
            this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Installed From Directory"), theString);
        } else {
            GetCurrentDirectoryA(499, theDirectoryBuffer);
            theString = theDirectoryBuffer;
            if (theString.GetLength() > 1) {
                int i = theString[0] == '\\' ? 2 : 0;
                while (i < theString.GetLength() && theString[i] != '\\')
                    i++;
                if (i < theString.GetLength()) {
                    theString = theString.GetIndexedSubstring(i, theString.GetLength() - 1);
                    theString += "\\";
                    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Installed From Directory"), theString);
                    this->mSettings.mInstalledFromDirectory = theString;
                }
            }
        }
    }
    this->mSettings.mInstalledFromDirectory.ToUpper();
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEEOriginalRegistryKey), UWideString("Installed From Volume"), theString);
    if (theResult == 0) {
        this->mSettings.mOriginalInstalledFromVolume = theString;
        this->mSettings.mOriginalInstalledFromVolume.ToUpper();
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEEOriginalRegistryKey), UWideString("Installed From Directory"), theString);
    if (theResult == 0) {
        this->mSettings.mOriginalInstalledFromDirectory = theString;
        this->mSettings.mOriginalInstalledFromDirectory.ToUpper();
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Player Name"), theString);
    if (theResult == 0) {
        this->mSettings.mPlayerName = theString;
    } else {
        this->mSettings.mPlayerName = EEGamePreferences::sDefaultSettings.mPlayerName;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Player Name"), this->mSettings.mPlayerName);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("WonNet Password"), theString);
    if (theResult == 0 && theString.GetLength() > 0) {
        theEncryptedPassword = theString;
        if (UEDecryptUString(theEncryptedPassword, thePassword) == true) {
            this->mSettings.mWonNetPassword = thePassword;
            gMultiplayerString = thePassword;
        }
    } else {
        this->mSettings.mWonNetPassword = EEGamePreferences::sDefaultSettings.mWonNetPassword;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("WonNet Password"), UWideString(this->mSettings.mWonNetPassword));
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Host Game Name"), theString);
    if (theResult == 0) {
        this->mSettings.mHostGameName = theString;
    } else {
        this->mSettings.mHostGameName = EEGamePreferences::sDefaultSettings.mHostGameName;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Host Game Name"), this->mSettings.mHostGameName);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Sierra.com User ID"), theString);
    if (theResult == 0) {
        this->mSettings.mSierraUserID = theString;
    } else {
        this->mSettings.mSierraUserID = EEGamePreferences::sDefaultSettings.mSierraUserID;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Sierra.com User ID"), this->mSettings.mSierraUserID);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Sierra.com Game Name"), theString);
    if (theResult == 0) {
        this->mSettings.mSierraGameName = theString;
    } else {
        this->mSettings.mSierraGameName = EEGamePreferences::sDefaultSettings.mSierraGameName;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Sierra.com Game Name"), this->mSettings.mSierraGameName);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Last Quick Save"), theString);
    if (theResult == 0) {
        this->mSettings.mLastQuickSave = theString;
    } else {
        this->mSettings.mLastQuickSave = EEGamePreferences::sDefaultSettings.mLastQuickSave;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Last Quick Save"), this->mSettings.mLastQuickSave);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Last Auto Save"), theString);
    if (theResult == 0) {
        this->mSettings.mLastAutoSave = theString;
    } else {
        this->mSettings.mLastAutoSave = EEGamePreferences::sDefaultSettings.mLastAutoSave;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Last Auto Save"), this->mSettings.mLastAutoSave);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Load Game Name"), theString);
    if (theResult == 0) {
        this->mSettings.mLoadGameName = theString;
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Music Volume"), theValue);
    if (theResult == 0) {
        this->mSettings.mMusicVolume = theValue * 0.01f;
    } else {
        this->mSettings.mMusicVolume = EEGamePreferences::sDefaultSettings.mMusicVolume;
        theValue = this->mSettings.mMusicVolume * 100.0f;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Music Volume"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Sound Volume"), theValue);
    if (theResult == 0) {
        this->mSettings.mSoundVolume = theValue * 0.01f;
    } else {
        this->mSettings.mSoundVolume = EEGamePreferences::sDefaultSettings.mSoundVolume;
        theValue = this->mSettings.mSoundVolume * 100.0f;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Sound Volume"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Reverb Enabled"), theValue);
    if (theResult == 0) {
        this->mSettings.mReverbEnabled = theValue == 1;
    } else {
        this->mSettings.mReverbEnabled = EEGamePreferences::sDefaultSettings.mReverbEnabled;
        theValue = this->mSettings.mReverbEnabled != false;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Reverb Enabled"), theValue);
    }
    this->mSettings.mB5C0 = true;
    this->mSettings.mL53C = 15020;
    this->mSettings.mL540 = 0;
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("AutoSave In Milliseconds"), theValue);
    if (theResult == 0) {
        this->mSettings.mAutoSaveInMilliseconds = theValue;
    } else {
        this->mSettings.mAutoSaveInMilliseconds = EEGamePreferences::sDefaultSettings.mAutoSaveInMilliseconds;
        theValue = this->mSettings.mAutoSaveInMilliseconds;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("AutoSave In Milliseconds"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Number Of AutoSaves"), theValue);
    if (theResult == 0) {
        this->mSettings.mNumberOfAutoSaves = theValue;
    } else {
        this->mSettings.mNumberOfAutoSaves = EEGamePreferences::sDefaultSettings.mNumberOfAutoSaves;
        theValue = this->mSettings.mNumberOfAutoSaves;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Number Of AutoSaves"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Max PTs per Second"), theValue);
    if (theResult == 0) {
        this->mSettings.mMaxPTsPerSecond = theValue;
    } else {
        this->mSettings.mMaxPTsPerSecond = EEGamePreferences::sDefaultSettings.mMaxPTsPerSecond;
        theValue = this->mSettings.mMaxPTsPerSecond;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Max PTs per Second"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Min PTs per Second"), theValue);
    if (theResult == 0) {
        this->mSettings.mMinPTsPerSecond = theValue;
    } else {
        this->mSettings.mMinPTsPerSecond = EEGamePreferences::sDefaultSettings.mMinPTsPerSecond;
        theValue = this->mSettings.mMinPTsPerSecond;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Min PTs per Second"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("PT Rate Tuning Threshold (percent)"), theValue);
    if (theResult == 0) {
        this->mSettings.mPTRateTuningThreshold = theValue;
    } else {
        this->mSettings.mPTRateTuningThreshold = EEGamePreferences::sDefaultSettings.mPTRateTuningThreshold;
        theValue = this->mSettings.mPTRateTuningThreshold;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("PT Rate Tuning Threshold (percent)"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Max CT Send Interval"), theValue);
    if (theResult == 0) {
        this->mSettings.mMaxCTSendInterval = theValue;
    } else {
        this->mSettings.mMaxCTSendInterval = EEGamePreferences::sDefaultSettings.mMaxCTSendInterval;
        theValue = this->mSettings.mMaxCTSendInterval;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Max CT Send Interval"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("CT Send Interval Step Down"), theValue);
    if (theResult == 0) {
        this->mSettings.mCTSendIntervalStepDown = theValue;
    } else {
        this->mSettings.mCTSendIntervalStepDown = EEGamePreferences::sDefaultSettings.mCTSendIntervalStepDown;
        theValue = this->mSettings.mCTSendIntervalStepDown;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("CT Send Interval Step Down"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("CT Send Interval Step Up"), theValue);
    if (theResult == 0) {
        this->mSettings.mCTSendIntervalStepUp = theValue;
    } else {
        this->mSettings.mCTSendIntervalStepUp = EEGamePreferences::sDefaultSettings.mCTSendIntervalStepUp;
        theValue = this->mSettings.mCTSendIntervalStepUp;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("CT Send Interval Step Up"), theValue);
    }
    this->mSettings.mB5C5 = true;
    this->mSettings.mB5C2 = true;
    this->mSettings.mB5CA = true;
    this->mSettings.mB5CB = true;
    this->mSettings.mB5CC = true;
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Enable Animal Herds"), theValue);
    if (theResult == 0) {
        this->mSettings.mEnableAnimalHerds = theValue == 1;
    } else {
        this->mSettings.mEnableAnimalHerds = EEGamePreferences::sDefaultSettings.mEnableAnimalHerds;
        theValue = this->mSettings.mEnableAnimalHerds != false;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Enable Animal Herds"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Ambient Sounds"), theValue);
    if (theResult == 0) {
        this->mSettings.mAmbientSounds = theValue == 1;
    } else {
        this->mSettings.mAmbientSounds = EEGamePreferences::sDefaultSettings.mAmbientSounds;
        theValue = this->mSettings.mAmbientSounds != false;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Ambient Sounds"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("UseAreaEffectOverlays"), theValue);
    if (theResult == 0) {
        this->mSettings.mUseAreaEffectOverlays = theValue == 1;
    } else {
        this->mSettings.mUseAreaEffectOverlays = EEGamePreferences::sDefaultSettings.mUseAreaEffectOverlays;
        theValue = this->mSettings.mUseAreaEffectOverlays != false;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("UseAreaEffectOverlays"), theValue);
    }
    this->mSettings.mDisplayWeatherEffects = false;
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("UseCandidateWindow"), theValue);
    if (theResult == 0) {
        this->mSettings.mUseCandidateWindow = theValue == 1;
    } else {
        this->mSettings.mUseCandidateWindow = EEGamePreferences::sDefaultSettings.mUseCandidateWindow;
        theValue = this->mSettings.mUseCandidateWindow != false;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("UseCandidateWindow"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Last Game File"), theString);
    if (theResult == 0) {
        this->mSettings.mLastGameFile = theString;
    } else {
        this->mSettings.mLastGameFile = EEGamePreferences::sDefaultSettings.mLastGameFile;
        theString = this->mSettings.mLastGameFile;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Last Game File"), theString);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Last Scenario File"), theString);
    if (theResult == 0) {
        this->mSettings.mLastScenarioFile = theString;
    } else {
        this->mSettings.mLastScenarioFile = EEGamePreferences::sDefaultSettings.mLastScenarioFile;
        theString = this->mSettings.mLastScenarioFile;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Last Scenario File"), theString);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Show Blood"), theValue);
    if (theResult == 0) {
        this->mSettings.mShowBlood = theValue == 1;
    } else {
        this->mSettings.mShowBlood = EEGamePreferences::sDefaultSettings.mShowBlood;
        theValue = this->mSettings.mShowBlood != false;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Show Blood"), theValue);
    }
    if (!this->IsNotKorean())
        this->mSettings.mShowBlood = false;
    this->mSettings.mZoomEnabled = true;
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Right Hand Mouse Orientation"), theValue);
    if (theResult == 0) {
        this->mSettings.mRightHandMouseOrientation = theValue == 1;
    } else {
        this->mSettings.mRightHandMouseOrientation = EEGamePreferences::sDefaultSettings.mRightHandMouseOrientation;
        theValue = this->mSettings.mRightHandMouseOrientation != false;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Right Hand Mouse Orientation"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Music Enabled"), theValue);
    if (theResult == 0) {
        this->mSettings.mMusicEnabled = theValue == 1;
    } else {
        this->mSettings.mMusicEnabled = EEGamePreferences::sDefaultSettings.mMusicEnabled;
        theValue = this->mSettings.mMusicEnabled != false;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Music Enabled"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Max Number Of Sounds (percent)"), theValue);
    if (theResult == 0) {
        this->mSettings.mMaxNumberOfSounds = theValue * 0.01f;
    } else {
        this->mSettings.mMaxNumberOfSounds = EEGamePreferences::sDefaultSettings.mMaxNumberOfSounds;
        theValue = this->mSettings.mMaxNumberOfSounds * 100.0f;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Max Number Of Sounds (percent)"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("ZoomStyle"), theValue);
    if (theResult == 0) {
        this->mSettings.mZoomStyle = theValue;
    } else {
        this->mSettings.mZoomStyle = EEGamePreferences::sDefaultSettings.mZoomStyle;
        theValue = this->mSettings.mZoomStyle;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("ZoomStyle"), theValue);
    }
    this->mSettings.mB5C4 = this->mSettings.mL560 = this->mSettings.mL55C = this->mSettings.mL558 = this->mSettings.mL554 = this->mSettings.mL550 = this->mSettings.mL54C = 0;
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Starting Keyboard Layout"), theValue);
    this->mSettings.mStartingKeyboardLayout = theResult == 0 ? theValue : EEGamePreferences::sDefaultSettings.mStartingKeyboardLayout;
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Unit Scale Factor"), theValue);
    if (theResult == 0) {
        this->mSettings.mUnitScaleFactor = theValue * 0.01f;
    } else {
        this->mSettings.mUnitScaleFactor = EEGamePreferences::sDefaultSettings.mUnitScaleFactor;
        theValue = this->mSettings.mUnitScaleFactor * 100.0f;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Unit Scale Factor"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Unit Lighting"), theValue);
    if (theResult == 0) {
        this->mSettings.mUnitLighting = theValue == 1;
    } else {
        this->mSettings.mUnitLighting = EEGamePreferences::sDefaultSettings.mUnitLighting;
        theValue = this->mSettings.mUnitLighting != false;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Unit Lighting"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Model Detail"), theValue);
    if (theResult == 0) {
        if (theValue > 2)
            theValue = 2;
        this->mSettings.mModelDetail = theValue;
    } else {
        this->mSettings.mModelDetail = EEGamePreferences::sDefaultSettings.mModelDetail;
        theValue = this->mSettings.mModelDetail;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Model Detail"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("MIP Levels to Drop"), theValue);
    if (theResult == 0) {
        this->mSettings.mMIPLevelsToDrop = theValue;
    } else {
        this->mSettings.mMIPLevelsToDrop = EEGamePreferences::sDefaultSettings.mMIPLevelsToDrop;
        theValue = this->mSettings.mMIPLevelsToDrop;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("MIP Levels to Drop"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Zoom Lock"), theValue);
    if (theResult == 0) {
        this->mSettings.mZoomLock = theValue * 0.01f;
    } else {
        this->mSettings.mZoomLock = EEGamePreferences::sDefaultSettings.mZoomLock;
        theValue = this->mSettings.mZoomLock * 100.0f;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Zoom Lock"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Effect Detail"), theValue);
    if (theResult == 0) {
        if (theValue > 2)
            theValue = 2;
        this->mSettings.mEffectDetail = theValue;
    } else {
        this->mSettings.mEffectDetail = EEGamePreferences::sDefaultSettings.mEffectDetail;
        theValue = this->mSettings.mEffectDetail;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Effect Detail"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Trilinear Filter"), theValue);
    if (theResult == 0) {
        this->mSettings.mTrilinearFilter = theValue == 1;
    } else {
        this->mSettings.mTrilinearFilter = EEGamePreferences::sDefaultSettings.mTrilinearFilter;
        theValue = this->mSettings.mTrilinearFilter != false;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Trilinear Filter"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Button MIP Levels to Drop"), theValue);
    if (theResult == 0) {
        this->mSettings.mButtonMIPLevelsToDrop = theValue;
    } else {
        this->mSettings.mButtonMIPLevelsToDrop = EEGamePreferences::sDefaultSettings.mButtonMIPLevelsToDrop;
        theValue = this->mSettings.mButtonMIPLevelsToDrop != 0;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Button MIP Levels to Drop"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Terrain MIP Levels to Drop"), theValue);
    if (theResult == 0) {
        this->mSettings.mTerrainMIPLevelsToDrop = theValue;
    } else {
        this->mSettings.mTerrainMIPLevelsToDrop = EEGamePreferences::sDefaultSettings.mTerrainMIPLevelsToDrop;
        theValue = this->mSettings.mTerrainMIPLevelsToDrop != 0;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Terrain MIP Levels to Drop"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Animation Smoothing"), theValue);
    if (theResult == 0) {
        this->mSettings.mAnimationSmoothing = theValue == 1;
    } else {
        this->mSettings.mAnimationSmoothing = EEGamePreferences::sDefaultSettings.mAnimationSmoothing;
        theValue = this->mSettings.mAnimationSmoothing != false;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Animation Smoothing"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Display Shadows"), theValue);
    if (theResult == 0) {
        this->mSettings.mDisplayShadows = theValue == 1;
    } else {
        this->mSettings.mDisplayShadows = EEGamePreferences::sDefaultSettings.mDisplayShadows;
        theValue = this->mSettings.mDisplayShadows != false;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Display Shadows"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Display Clouds"), theValue);
    if (theResult == 0) {
        this->mSettings.mDisplayClouds = theValue == 1;
    } else {
        this->mSettings.mDisplayClouds = EEGamePreferences::sDefaultSettings.mDisplayClouds;
        theValue = this->mSettings.mDisplayClouds != false;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Display Clouds"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Display Battlefield Ambients"), theValue);
    if (theResult == 0) {
        this->mSettings.mDisplayBattlefieldAmbients = theValue == 1;
    } else {
        this->mSettings.mDisplayBattlefieldAmbients = EEGamePreferences::sDefaultSettings.mDisplayBattlefieldAmbients;
        theValue = this->mSettings.mDisplayBattlefieldAmbients != false;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Display Battlefield Ambients"), theValue);
    }
    this->mSettings.mB5C6 = false;
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Computer Player Difficulty Level"), theValue);
    if (theResult == 0) {
        this->mSettings.mComputerPlayerDifficultyLevel = theValue;
    } else {
        this->mSettings.mComputerPlayerDifficultyLevel = EEGamePreferences::sDefaultSettings.mComputerPlayerDifficultyLevel;
        theValue = this->mSettings.mComputerPlayerDifficultyLevel;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Computer Player Difficulty Level"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Debug OOS Handling"), theValue);
    if (theResult == 0) {
        this->mSettings.mDebugOOSHandling = theValue == 1;
    } else {
        this->mSettings.mDebugOOSHandling = EEGamePreferences::sDefaultSettings.mDebugOOSHandling;
        theValue = this->mSettings.mDebugOOSHandling != false;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Debug OOS Handling"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Take JPG Screenshots"), theValue);
    if (theResult == 0) {
        this->mSettings.mTakeJPGScreenshots = theValue == 1;
    } else {
        this->mSettings.mTakeJPGScreenshots = EEGamePreferences::sDefaultSettings.mTakeJPGScreenshots;
        theValue = this->mSettings.mTakeJPGScreenshots != false;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Take JPG Screenshots"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Take End Game Screenshots"), theValue);
    if (theResult == 0) {
        this->mSettings.mTakeEndGameScreenshots = theValue == 1;
    } else {
        this->mSettings.mTakeEndGameScreenshots = EEGamePreferences::sDefaultSettings.mTakeEndGameScreenshots;
        theValue = this->mSettings.mTakeEndGameScreenshots != false;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Take End Game Screenshots"), theValue);
    }
    this->mSettings.mAntiAliasing = false;
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Show Auto Waypoints"), theValue);
    if (theResult == 0) {
        this->mSettings.mShowAutoWaypoints = theValue == 1;
    } else {
        this->mSettings.mShowAutoWaypoints = EEGamePreferences::sDefaultSettings.mShowAutoWaypoints;
        theValue = this->mSettings.mShowAutoWaypoints != false;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Show Auto Waypoints"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Last Open Post Game Stats Page"), theValue);
    if (theResult == 0) {
        this->mSettings.mLastOpenPostGameStatsPage = theValue;
    } else {
        this->mSettings.mLastOpenPostGameStatsPage = EEGamePreferences::sDefaultSettings.mLastOpenPostGameStatsPage;
        theValue = this->mSettings.mLastOpenPostGameStatsPage;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Last Open Post Game Stats Page"), theValue);
    }

    return this->LoadGameOptions();
}

// FUNCTION: EXE 0x53505e
// Reads the last game options from the registry's "Game Options" key, writing defaults for missing values;
// returns 0
long EEGamePreferences::LoadGameOptions()
{
    UWideString theString;
    long theValue;
    long theResult;

    this->mRegistry.CreateRegistryKey(kEERootKey, UWideString(kEEGameOptionsKey));
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Map Size"), theValue);
    if (theResult == 0) {
        this->mSettings.mGameOptions.mMapSize = theValue;
    } else {
        theValue = this->mSettings.mGameOptions.mMapSize = 1;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Map Size"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Map Type"), theString);
    if (theResult == 0) {
        this->mSettings.mGameOptions.mMapType = theString;
    } else {
        // the member and the registry both get the default map type
        theString = "Continental";
        this->mSettings.mGameOptions.SetMapType(theString);
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Map Type"), theString);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Starting Resources"), theValue);
    if (theResult == 0) {
        this->mSettings.mGameOptions.mResources = theValue;
    } else {
        theValue = this->mSettings.mGameOptions.mResources = 0;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Starting Resources"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Starting Epoch"), theValue);
    if (theResult == 0) {
        this->mSettings.mGameOptions.mEpochStart = theValue;
    } else {
        theValue = this->mSettings.mGameOptions.mEpochStart = kTechEpochPrehistoric;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Starting Epoch"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Ending Epoch"), theValue);
    if (theResult == 0) {
        this->mSettings.mGameOptions.mEpochEnd = theValue;
    } else {
        theValue = this->mSettings.mGameOptions.mEpochEnd = kTechEpochSpace;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Ending Epoch"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Game Unit Limit"), theValue);
    if (theResult == 0) {
        this->mSettings.mGameOptions.mUnitLimit = theValue;
    } else {
        theValue = this->mSettings.mGameOptions.mUnitLimit = 500;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Game Unit Limit"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Wonders For Victory"), theValue);
    if (theResult == 0) {
        this->mSettings.mGameOptions.mWondersForVictory = theValue;
    } else {
        theValue = this->mSettings.mGameOptions.mWondersForVictory = 3;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Wonders For Victory"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Game Variant"), theValue);
    if (theResult == 0) {
        this->mSettings.mGameOptions.mVariant = theValue;
    } else {
        theValue = this->mSettings.mGameOptions.mVariant = 2;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Game Variant"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Difficulty Level"), theValue);
    if (theResult == 0) {
        this->mSettings.mGameOptions.mDifficulty = theValue;
    } else {
        theValue = this->mSettings.mGameOptions.mDifficulty = 1;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Difficulty Level"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Game Speed"), theValue);
    if (theResult == 0) {
        this->mSettings.mGameOptions.mGameSpeed = theValue;
    } else {
        theValue = this->mSettings.mGameOptions.mGameSpeed = 1;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Game Speed"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Record Game"), theValue);
    if (theResult == 0) {
        this->mSettings.mGameOptions.mRecordGames = theValue == 1;
    } else {
        this->mSettings.mGameOptions.mRecordGames = false;
        theValue = 0;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Record Game"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Reveal Map"), theValue);
    if (theResult == 0) {
        this->mSettings.mGameOptions.mRevealMap = theValue == 1;
    } else {
        this->mSettings.mGameOptions.mRevealMap = false;
        theValue = 0;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Reveal Map"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Allow Custom Civs"), theValue);
    if (theResult == 0) {
        this->mSettings.mGameOptions.mAllowCustomCivs = theValue == 1;
    } else {
        this->mSettings.mGameOptions.mAllowCustomCivs = true;
        theValue = 1;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Allow Custom Civs"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Lock Teams"), theValue);
    if (theResult == 0) {
        this->mSettings.mGameOptions.mLockTeams = theValue == 1;
    } else {
        this->mSettings.mGameOptions.mLockTeams = false;
        theValue = 0;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Lock Teams"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Lock Speed"), theValue);
    if (theResult == 0) {
        this->mSettings.mGameOptions.mLockSpeed = theValue == 1;
    } else {
        this->mSettings.mGameOptions.mLockSpeed = false;
        theValue = 0;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Lock Speed"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Cheat Codes"), theValue);
    if (theResult == 0) {
        this->mSettings.mGameOptions.mCheatCodes = theValue == 1;
    } else {
        this->mSettings.mGameOptions.mCheatCodes = true;
        theValue = 1;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Cheat Codes"), theValue);
    }
    theResult = this->mRegistry.GetRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Victory Allowed"), theValue);
    if (theResult == 0) {
        this->mSettings.mGameOptions.mVictory = theValue == 1;
    } else {
        this->mSettings.mGameOptions.mVictory = true;
        theValue = 1;
        this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Victory Allowed"), theValue);
    }
    return 0;
}

// FUNCTION: EXE 0x535943
// Writes every game setting to the registry (the WON password encrypted), then the game options; returns their
// result
long EEGamePreferences::Save()
{
    UWideString theRasterizerName(this->mSettings.mRasterizerName);
    UWideString theVolume(this->mSettings.mInstalledFromVolume);
    UWideString theDirectory(this->mSettings.mInstalledFromDirectory);
    UWideString theLastGameFile(this->mSettings.mLastGameFile);
    UWideString theLastScenarioFile(this->mSettings.mLastScenarioFile);
    UWideString theLastQuickSave((const char*)this->mSettings.mLastQuickSave);
    UWideString theLastAutoSave((const char*)this->mSettings.mLastAutoSave);
    UString theEncryptedPassword;
    long theValue = 0;

    this->mRegistry.CreateRegistryKey(kEERootKey, UWideString(kEERegistryKey));
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Rasterizer Name"), theRasterizerName);
    theValue = this->mSettings.mMouseSensitivity * 100.0f;
    if (theValue <= 1)
        theValue = 1;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Mouse Sensitivity"), theValue);
    theValue = this->mSettings.mSideScrollSpeed * 100.0f;
    if (theValue <= 1)
        theValue = 1;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Side Scroll Speed"), theValue);
    theValue = this->mSettings.mGrabScrollSpeed * 100.0f;
    if (theValue <= 1)
        theValue = 1;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Grab Scroll Speed"), theValue);
    theValue = this->mSettings.mGameBitDepth;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Game Bit Depth"), theValue);
    theValue = this->mSettings.mNetworkAdapter;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Network Adapter"), theValue);
    theValue = this->mSettings.mGameWindowHeight;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Game Window Height"), theValue);
    theValue = this->mSettings.mGameWindowWidth;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Game Window Width"), theValue);
    theValue = this->mSettings.mScreenID;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Screen ID"), theValue);
    theValue = this->mSettings.mTextureBitDepth;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Texture Bit Depth"), theValue);
    theValue = this->mSettings.mWaitForVSync;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Wait for VSync"), theValue);
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Installed From Volume"), theVolume);
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Installed From Directory"), theDirectory);
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Player Name"), this->mSettings.mPlayerName);
    if (this->mSettings.mWonNetPassword.GetLength() > 0)
        UEEncryptUString(this->mSettings.mWonNetPassword, theEncryptedPassword);
    else
        theEncryptedPassword = "";
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("WonNet Password"), UWideString(theEncryptedPassword));
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Host Game Name"), this->mSettings.mHostGameName);
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Sierra.com User ID"), this->mSettings.mSierraUserID);
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Sierra.com Game Name"), this->mSettings.mSierraGameName);
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Last Game File"), theLastGameFile);
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Last Scenario File"), theLastScenarioFile);
    theValue = this->mSettings.mMusicVolume * 100.0f;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Music Volume"), theValue);
    theValue = this->mSettings.mSoundVolume * 100.0f;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Sound Volume"), theValue);
    theValue = this->mSettings.mReverbEnabled != false;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Reverb Enabled"), theValue);
    theValue = this->mSettings.mAmbientSounds != false;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Ambient Sounds"), theValue);
    theValue = this->mSettings.mUseAreaEffectOverlays != false;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("UseAreaEffectOverlays"), theValue);
    this->mSettings.mDisplayWeatherEffects = false;
    theValue = this->mSettings.mAutoSaveInMilliseconds;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("AutoSave In Milliseconds"), theValue);
    theValue = this->mSettings.mNumberOfAutoSaves;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Number Of AutoSaves"), theValue);
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Last Quick Save"), theLastQuickSave);
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Last Auto Save"), theLastAutoSave);
    theValue = this->mSettings.mMaxPTsPerSecond;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Max PTs per Second"), theValue);
    theValue = this->mSettings.mMinPTsPerSecond;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Min PTs per Second"), theValue);
    theValue = this->mSettings.mPTRateTuningThreshold;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("PT Rate Tuning Threshold (percent)"), theValue);
    theValue = this->mSettings.mShowBlood != false;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Show Blood"), theValue);
    theValue = this->mSettings.mRightHandMouseOrientation != false;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Right Hand Mouse Orientation"), theValue);
    theValue = this->mSettings.mMusicEnabled != false;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Music Enabled"), theValue);
    theValue = this->mSettings.mMaxNumberOfSounds * 100.0f;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Max Number Of Sounds (percent)"), theValue);
    theValue = this->mSettings.mZoomStyle;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("ZoomStyle"), theValue);
    theValue = this->mSettings.mUnitLighting != false;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Unit Lighting"), theValue);
    theValue = this->mSettings.mTrilinearFilter != false;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Trilinear Filter"), theValue);
    theValue = this->mSettings.mModelDetail;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Model Detail"), theValue);
    theValue = this->mSettings.mMIPLevelsToDrop;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("MIP Levels to Drop"), theValue);
    theValue = this->mSettings.mButtonMIPLevelsToDrop;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Button MIP Levels to Drop"), theValue);
    theValue = this->mSettings.mTerrainMIPLevelsToDrop;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Terrain MIP Levels to Drop"), theValue);
    theValue = this->mSettings.mZoomLock * 100.0f;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Zoom Lock"), theValue);
    theValue = this->mSettings.mEffectDetail;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Effect Detail"), theValue);
    theValue = this->mSettings.mAnimationSmoothing != false;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Animation Smoothing"), theValue);
    theValue = this->mSettings.mDisplayShadows != false;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Display Shadows"), theValue);
    theValue = this->mSettings.mDisplayClouds != false;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Display Clouds"), theValue);
    theValue = this->mSettings.mDisplayBattlefieldAmbients != false;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Display Battlefield Ambients"), theValue);
    theValue = this->mSettings.mComputerPlayerDifficultyLevel;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Computer Player Difficulty Level"), theValue);
    theValue = this->mSettings.mDebugOOSHandling != false;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Debug OOS Handling"), theValue);
    theValue = this->mSettings.mTakeJPGScreenshots != false;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Take JPG Screenshots"), theValue);
    theValue = this->mSettings.mTakeEndGameScreenshots != false;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Take End Game Screenshots"), theValue);
    theValue = this->mSettings.mLastOpenPostGameStatsPage;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEERegistryKey), UWideString("Last Open Post Game Stats Page"), theValue);
    return this->SaveGameOptions();
}

// FUNCTION: EXE 0x53670d
// Writes the last game options to the registry's "Game Options" key; returns the last write's result
long EEGamePreferences::SaveGameOptions()
{
    UWideString theString;
    long theValue;

    this->mRegistry.CreateRegistryKey(kEERootKey, UWideString(kEEGameOptionsKey));
    theValue = this->mSettings.mGameOptions.mMapSize;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Map Size"), theValue);
    theString = this->mSettings.mGameOptions.mMapType;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Map Type"), theString);
    theValue = this->mSettings.mGameOptions.mResources;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Starting Resources"), theValue);
    theValue = this->mSettings.mGameOptions.mEpochStart;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Starting Epoch"), theValue);
    theValue = this->mSettings.mGameOptions.mEpochEnd;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Ending Epoch"), theValue);
    theValue = this->mSettings.mGameOptions.mUnitLimit;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Game Unit Limit"), theValue);
    theValue = this->mSettings.mGameOptions.mWondersForVictory;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Wonders For Victory"), theValue);
    theValue = this->mSettings.mGameOptions.mVariant;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Game Variant"), theValue);
    theValue = this->mSettings.mGameOptions.mDifficulty;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Difficulty Level"), theValue);
    theValue = this->mSettings.mGameOptions.mGameSpeed;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Game Speed"), theValue);
    theValue = this->mSettings.mGameOptions.mRecordGames == true;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Record Game"), theValue);
    theValue = this->mSettings.mGameOptions.mRevealMap == true;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Reveal Map"), theValue);
    theValue = this->mSettings.mGameOptions.mAllowCustomCivs == true;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Allow Custom Civs"), theValue);
    theValue = this->mSettings.mGameOptions.mLockTeams == true;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Lock Teams"), theValue);
    theValue = this->mSettings.mGameOptions.mLockSpeed == true;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Lock Speed"), theValue);
    theValue = this->mSettings.mGameOptions.mCheatCodes == true;
    this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Cheat Codes"), theValue);
    theValue = this->mSettings.mGameOptions.mVictory == true;
    return this->mRegistry.CreateRegistryValue(kEERootKey, UWideString(kEEGameOptionsKey), UWideString("Victory Allowed"), theValue);
}

// FUNCTION: EXE 0x536b49
// Remembers the options of a new random-map game as the last game options, then saves the preferences
void EEGamePreferences::SetLastGameOptions(const EEGameOptions& inOptions)
{
    if (inOptions.mLoadType == 0)
        this->mSettings.mGameOptions = inOptions;
    this->Save();
}
