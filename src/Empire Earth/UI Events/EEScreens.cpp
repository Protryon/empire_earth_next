// C:\EEX\Empire Earth\UI Events\EEScreens.cpp (NEW, no original address: units.EXTRA_UNITS). The host of the game
// screens that are Rust's whole (rust/crates/ee_screens menus.rs: the main menu, the editors' sub-menu, the single
// player and play campaign screens; settings_form.rs: the game settings; docs/RUST_PLAN.md "R6 game screens", step
// 2; lobby_form.rs: the lobby and its load screens; main_game_options.rs: the in-game options menu; main_game.rs:
// the main game form's events; chat.rs: the chat forms' events; hot_keys.rs: the hot keys; diplomacy.rs; camera.rs:
// the camera, scroll and zoom events; civ_builder.rs: the civilization builder's events): EEScreenEvent's
// ProcessEvent, the C++ events the screens still hang on other forms, and the toolkit's and the game's calls the Rust
// dispatchers make (ee_screenscpp_*). It replaces EEUIEMainScreenEvents.cpp, EEUIEMultiplayerScreenEvents.cpp,
// EEUIEEmpireEarthEditorsEvents.cpp, EEUIESinglePlayerEvents.cpp, EEUIEPlayCampaignEvents.cpp,
// EEUIEGameSettingsEvents.cpp, EEUIELobbyEvents.cpp, EEUIEMainGameOptionsEvents.cpp, EEUIEMainGameEvents.cpp,
// EEUIEChatEvents.cpp, EEUIEHotKeyEvents.cpp, EEUIEDiplomacyEvents.cpp, EEUIECameraEvents.cpp,
// EEUIEScrollZoomEvents.cpp, EEUIEWheelZoom.cpp, EEUIEFormationChange.cpp and EECBUIEvents.cpp.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <string>
#include <list>
#include <set>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UIEditBox.h"
#include "lle/UICheckBox.h"
#include "lle/UIRadioButton.h"
#include "lle/UISlider.h"
#include "lle/UIComboBox.h"
#include "lle/UIESliderChange.h"
#include "lle/SEMixer.h"
#include "lle/SEConductor.h"
#include "lle/NEProtocol.h"
#include "lle/NESynchronousServer.h"
#include "lle/UIScrollBar.h"
#include "lle/UIFileListBox.h"
#include "lle/FSRestorable.h"
#include "lle/UILayoutUtility.h"
#include "lle/UITextureBackground.h"
#include "lle/UIBFull.h"
#include "lle/UIBitmap.h"
#include "lle/UIBlinkingButton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EECivilization.h"
#include "ee/EECommunications.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGameState.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEServer.h"
#include "ee/EEWonNet.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EECampaignFormManager.h"
#include "ee/EEScreenEvent.h"
#include "ee/EEMouse.h"
#include "ee/EEGameSound.h"
#include "ee/EEMusicManager.h"
#include "ee/EEAmbientSoundManager.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/RTSGraphics.h"
#include "ee/EEDisplayDevices.h"
#include "ee/EEMessage.h"
#include "ee/EEAction.h"
#include "ee/EEASaveGame.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EEUISCIFormManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EECampaignProgress.h"
#include "ee/EEUIBMapBackground.h"
#include "ee/EEStringIDs.h"
#include "ee/EEUICPing.h"
#include "ee/EEARemovePlayer.h"
#include "ee/EECPEngine.h"
#include "ee/EERandomMapGenerators.h"
#include "ee/EEStartingResources.h"
#include "ee/EEGameVariant.h"
#include "ee/EERules.h"
#include "ee/EERulesWorld.h"
#include "ee/EETechTreeTechID.h"
#include "ee/EEAUserPauseGame.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "lle/NESynchronousClient.h"
#include "lle/TSThread.h"
#include "ee/EEUIFMGMainGroup.h"
#include "ee/EEUIEChatLogEvents.h"
#include "ee/EEUnitAction.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUIBSavedGroup.h"
#include "ee/EEMiniMap.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EESoundObject.h"
#include "ee/EEGraphicObject.h"
#include "ee/EECBUIListBox.h"
#include "ee/EECivilizationManager.h"
#include "ee/EEAPlayerEvent.h"
#include "ee/EECheatCodes.h"
#include "lle/GETextAlignment.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EERenderObject.h"
#include "ee/EECameraControl.h"
#include "platform/PlatWindow.h"
#include "ee/EEFormation.h"
#include "ee/EECivilizationBuilder.h"
#include "ee/EECBEditableCivilization.h"
#include "ee/EEScenarioEditorIDs.h"
#include "rust/ee_screens.h"

extern float gCameraMaxZoom;   // port: EECameraControl.cpp (the farther zoom-out; 1.0 in the original)
extern GEViewport gU4E8F08;   // 0x4e8f08 (EERenderObject.cpp; the render viewport: the aircraft hot keys' point)
void GXGameSize(unsigned long inPercent, bool inWindowed, unsigned long* outWidth, unsigned long* outHeight);   // gx/GX.h
extern "C" unsigned long SPIMGetOutputDevices(char (*outNames)[128], unsigned long inCapacity);   // lle/SEMixer.h
unsigned long EEMenuWidth();   // Empire Earth.cpp
unsigned long EEMenuHeight();

UString gMultiplayerString("");             // 0x562fd8 (EEUserInterface.cpp keeps the WON password in it)
EEUIFMainGame* gSavedMainGameForm;          // 0x563144 (.bss among the UI events' statics; ee/EmpireEarth.h)
static EECivilization sCivilization;        // 0x962f08: the civilization the editors' sub-menu opens the builder on

long EEScreenEvent::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    return ee_screens_event(this->mEvent, inForm, inEntity);
}

static UWideString sCampaignError;   // the last campaign load's error (EE_SCREENS_TEXT_CAMPAIGN_ERROR)

// the custom campaign form's no-campaigns message, closed
static void EEScreensNoCustomCampaigns(UIForm* inForm)
{
    ee_screens_event(EE_SCREENS_CC_NONE_FOUND, inForm, NULL);
}

// the settings form's Restore Default question, answered yes
static void EEScreensRestoreDefaultYes(UIForm* inForm)
{
    ee_screens_event(EE_SCREENS_GS_RESTORE_YES, inForm, NULL);
}

// ---- the load and save screens' (save_load.rs): the questions' callbacks get the list or the pending save object

// the pending save (an EEMLoadOrSaveGame or an EEASaveGame, as the "form" DisplayQuestion hands its callbacks) between
// its creation and the save event's end
static UIForm* sSaveObject;

// the delete question, answered yes: the list's selected file deleted
static void EEScreensDeleteSelectedFile(UIForm* inList)
{
    reinterpret_cast<UIFileListBox*>(inList)->DeleteSelectedFile();
}

// a saved game's save message to every player, waiting up to 15 s; the save handed on
static void EEScreensSendSave(UIForm* inMessage)
{
    static_cast<NESynchronousServer*>(gCommunications)->SendToAllAndWait(reinterpret_cast<NEMessage*>(inMessage), 15000, 12);
    ee_screens_event(EE_SCREENS_SL_SAVED, NULL, NULL);
}

// a scenario's save action posted, our reference dropped; the save handed on
static void EEScreensPostSave(UIForm* inAction)
{
    gUserInterface->PostAction(reinterpret_cast<EEAction*>(inAction));
    reinterpret_cast<EEAction*>(inAction)->Dereference();
    ee_screens_event(EE_SCREENS_SL_SAVED, NULL, NULL);
}

// the overwrite question, answered no: the pending save dropped
static void EEScreensDropSave(UIForm* inObject)
{
    reinterpret_cast<UReferenceCounter*>(inObject)->Dereference();
}

// ---- the campaign scenario select and scenario intro forms' (scenario_screens.rs)

static UString sCampaignDefaultUser("default");   // 0x5d2ae0 (gCampaignDefaultUser): the campaign progress's user

// the intro's scenario file (GetScenario's, for Start's load or game)
static FSFileSpec& EEScreensScenario()
{
    static FSFileSpec sScenario;
    return sScenario;
}

// a campaign scenario's sound and map below the campaign's scenario prefix
struct EEScreensMedia {
    static void Prefix(EESMMediaStrings& ioStrings)
    {
        ioStrings.mSoundFile = gU52A280 + ioStrings.mSoundFile;
        ioStrings.mMapFile = gU52A280 + ioStrings.mMapFile;
    }
};

// the campaign's open-error message, closed
static void EEScreensCloseCampaign(UIForm* inForm)
{
    ee_screens_event(EE_SCREENS_CS_CLOSE_CALLBACK, inForm, NULL);
}

// a put's texts joined by 0 bytes
static std::vector<UString> Parts(const uint8_t* inText, size_t inLength)
{
    std::vector<UString> theParts;
    std::string theText((const char*)inText, inLength);
    size_t theStart = 0;
    for (;;) {
        size_t theEnd = theText.find('\0', theStart);
        theParts.push_back(UString(theText.substr(theStart, theEnd == std::string::npos ? std::string::npos
                                                                                     : theEnd - theStart).c_str()));
        if (theEnd == std::string::npos)
            break;
        theStart = theEnd + 1;
    }
    while (theParts.size() < 3)
        theParts.push_back(UString(""));
    return theParts;
}

// ---- the lobby's and its load screens' (lobby_form.rs)

static EEGameOptionsPlayer* sLobbyBoot;   // the boot question's slot copy (its answer frees it)
static EEPSaveGameHeader* sLobbyHeader;   // the load screen's header (the next load replaces it)
static FSFileSpec* sLobbyFile;            // its file (the transfers read it; never freed, as the original's)
static EEGameOptions* sLobbyCopy;         // the game type's copy of the options (a random map's)
static UWideString sLobbyMissing;         // the last options check's missing lines, each ended by a line break
static unsigned long sLobbyStartTime;     // 0x562fd0: the lobby's Start time (provisional name)

// the boot question's answers: the copy the question was asked on, then the lobby's
static void EEScreensBootYes(UIForm* inCopy)
{
    sLobbyBoot = reinterpret_cast<EEGameOptionsPlayer*>(inCopy);
    ee_screens_event(EE_SCREENS_LE_BOOT_YES, NULL, NULL);
}
static void EEScreensBootNo(UIForm* inCopy)
{
    sLobbyBoot = reinterpret_cast<EEGameOptionsPlayer*>(inCopy);
    ee_screens_event(EE_SCREENS_LE_BOOT_NO, NULL, NULL);
}

// the options of a source (EE_SCREENS_LE_SOURCE_*: the lobby's, the loaded header's, the game type's copy) and a slot
static EEGameOptions* EEScreensLobbyOptions(uint32_t inSource)
{
    if (inSource == EE_SCREENS_LE_SOURCE_HEADER && sLobbyHeader)
        return &sLobbyHeader->mGameOptions;
    if (inSource == EE_SCREENS_LE_SOURCE_COPY && sLobbyCopy)
        return sLobbyCopy;
    return &gGameOptions;
}
static EEGameOptionsPlayer* EEScreensLobbySlot(uint32_t inSource, uint32_t inIndex)
{
    if (inSource == EE_SCREENS_LE_SOURCE_BOOT)
        return sLobbyBoot;
    if (inIndex >= 16)
        return NULL;
    return &EEScreensLobbyOptions(inSource)->mPlayers[inIndex];
}

// an option (EEMGameOptions' numbers; 0 the load type) of the lobby options
static void EEScreensLobbySetOption(uint32_t inOption, long inValue)
{
    EEGameOptions& theOptions = gGameOptions;
    switch (inOption) {
    case 0: theOptions.mLoadType = inValue; break;
    case EE_SCREENS_OPTION_MAP_SIZE: theOptions.mMapSize = inValue; break;
    case EE_SCREENS_OPTION_RESOURCES: theOptions.mResources = inValue; break;
    case EE_SCREENS_OPTION_EPOCH_START: theOptions.mEpochStart = inValue; break;
    case EE_SCREENS_OPTION_EPOCH_END: theOptions.mEpochEnd = inValue; break;
    case EE_SCREENS_OPTION_UNIT_LIMIT: theOptions.mUnitLimit = inValue; break;
    case EE_SCREENS_OPTION_WONDERS: theOptions.mWondersForVictory = inValue; break;
    case EE_SCREENS_OPTION_VARIANT: theOptions.mVariant = inValue; break;
    case EE_SCREENS_OPTION_DIFFICULTY: theOptions.mDifficulty = inValue; break;
    case EE_SCREENS_OPTION_SPEED: theOptions.mGameSpeed = inValue; break;
    case EE_SCREENS_OPTION_RECORD_GAMES: theOptions.mRecordGames = inValue != 0; break;
    case EE_SCREENS_OPTION_CUSTOM_CIVS: theOptions.mAllowCustomCivs = inValue != 0; break;
    case EE_SCREENS_OPTION_REVEAL_MAP: theOptions.mRevealMap = inValue != 0; break;
    case EE_SCREENS_OPTION_LOCK_TEAMS: theOptions.mLockTeams = inValue != 0; break;
    case EE_SCREENS_OPTION_LOCK_SPEED: theOptions.mLockSpeed = inValue != 0; break;
    case EE_SCREENS_OPTION_CHEAT_CODES: theOptions.mCheatCodes = inValue != 0; break;
    case 17: theOptions.mVictory = inValue != 0; break;
    }
}

// a value of the rules' tables the lobby lists (EE_SCREENS_LE_RULE_*)
static uint32_t EEScreensLobbyRule(uint32_t inWhich, uint32_t inArg)
{
    const EEGameOptionRules& theRules = EEGetRules().mGameOptions;   // gameoptions.yaml
    switch (inWhich) {
    case EE_SCREENS_LE_RULE_EPOCHS: return (uint32_t)EEEpochCount();
    case EE_SCREENS_LE_RULE_UNIT_MINIMUM: return (uint32_t)EERuleLong(theRules.mUnitLimitMinimum);
    case EE_SCREENS_LE_RULE_UNIT_MAXIMUM: return (uint32_t)EERuleLong(theRules.mUnitLimitMaximum);
    case EE_SCREENS_LE_RULE_UNIT_STEP: return (uint32_t)theRules.mUnitLimitStep;
    case EE_SCREENS_LE_RULE_WONDERS_MAXIMUM: return (uint32_t)EERuleLong(theRules.mWondersMaximum);
    case EE_SCREENS_LE_RULE_MAP_SIZES: return (uint32_t)theRules.mMapSizes.size();
    case EE_SCREENS_LE_RULE_MAP_SIZE_NAME:
        return inArg < theRules.mMapSizes.size() ? (uint32_t)theRules.mMapSizes[inArg].mNameID : 0;
    case EE_SCREENS_LE_RULE_DIFFICULTIES: return (uint32_t)theRules.mDifficulties.size();
    case EE_SCREENS_LE_RULE_DIFFICULTY_NAME:
        return inArg < theRules.mDifficulties.size() ? (uint32_t)theRules.mDifficulties[inArg].mNameID : 0;
    case EE_SCREENS_LE_RULE_SPEEDS: return (uint32_t)theRules.mSpeeds.size();
    case EE_SCREENS_LE_RULE_SPEED_NAME:
        return inArg < theRules.mSpeeds.size() ? (uint32_t)theRules.mSpeeds[inArg].mNameID : 0;
    case EE_SCREENS_LE_RULE_VARIANTS: return (uint32_t)gGameVariants.size();
    case EE_SCREENS_LE_RULE_VARIANT_SELECTABLE:
        return inArg < gGameVariants.size() && gGameVariants[inArg]->mSelectable == true;
    case EE_SCREENS_LE_RULE_VARIANT_NAME:
        return inArg < gGameVariants.size() ? (uint32_t)EEGetGameVariantAt(inArg)->mNameID : 0;
    case EE_SCREENS_LE_RULE_RESOURCE_LEVELS: return (uint32_t)EEGetNumberOfResourceLevels();
    case EE_SCREENS_LE_RULE_RESOURCE_LEVEL_NAME: return (uint32_t)EEGetResourceLevel((long)inArg)->GetNameID();
    case EE_SCREENS_LE_RULE_STARTING_RESOURCES_NAME:
        return inArg < EEStartingResources::sTable.size() ? (uint32_t)EEStartingResources::Get(inArg)->mNameID : 0;
    case EE_SCREENS_LE_RULE_EPOCH_TITLE: return (uint32_t)EEEpochTitleID((long)(int32_t)inArg);
    case EE_SCREENS_LE_RULE_GENERATORS: return (uint32_t)EERMGetNumberOfGenerators();
    case EE_SCREENS_LE_RULE_ENGINES: return (uint32_t)EECPEngineChoiceCount();
    case EE_SCREENS_LE_RULE_DEFAULT_COLOUR: return inArg < 16 ? (uint32_t)gDefaultPlayerColors[inArg] : 0;
    case EE_SCREENS_LE_RULE_STARTING_CITIZENS: return gWorldTable.GetLongValue(kWorldStartingCitizensDefault);
    }
    return 0;
}

// a save file's header and the options check: the start's (against the lobby options) or the load screen's (against
// a copy with the load type), EE_SCREENS_LE_LOAD_*; the missing lines kept
static uint32_t EEScreensLobbyLoad(const UString& inFile, const UString& inDirectory, EEPSaveGameHeader& ioHeader,
                                   FSFileSpec*& outFile, bool inStart, unsigned long inLoadType)
{
    if (EEOpenSaveGameFile(inFile, inDirectory, outFile) != kErrorOK)
        return EE_SCREENS_LE_LOAD_NOT_FOUND;
    ioHeader.mMajorVersion = kSaveGameMajorVersion;
    ioHeader.mMinorVersion = kSaveGameMinorVersion;
    if (ioHeader.LoadFromFile(*outFile) != kErrorOK)
        return EE_SCREENS_LE_LOAD_READ_ERROR;
    EEGameOptions theOptions;
    std::vector<UWideString> theMissing;
    bool theMatch;
    theOptions = ioHeader.mGameOptions;
    if (inStart)
        theMatch = EECheckSavedGameOptions(theOptions, gGameOptions, true, theMissing, ioHeader.mNumPlayers - 1);
    else {
        EEGameOptions theCurrent;
        theCurrent = gGameOptions;
        theCurrent.mLoadType = inLoadType;
        theMatch = EECheckSavedGameOptions(theOptions, theCurrent, false, theMissing, ioHeader.mNumPlayers - 1);
    }
    if (theMatch)
        return EE_SCREENS_LE_LOAD_OK;
    sLobbyMissing = "";
    for (unsigned long i = 0; i < theMissing.size(); i++)
        sLobbyMissing += theMissing[i] + "\n";
    return EE_SCREENS_LE_LOAD_MISMATCH;
}

// ---- the in-game options menu's (main_game_options.rs): its questions' answers

static void EEScreensResignYes(UIForm*) { ee_screens_event(EE_SCREENS_GO_RESIGN_YES, NULL, NULL); }
static void EEScreensRestartYes(UIForm*) { ee_screens_event(EE_SCREENS_GO_RESTART_YES, NULL, NULL); }
static void EEScreensToEditorYes(UIForm*) { ee_screens_event(EE_SCREENS_GO_RETURN_TO_EDITOR_YES, NULL, NULL); }
static void EEScreensExitYes(UIForm*) { ee_screens_event(EE_SCREENS_GO_EXIT_YES, NULL, NULL); }

UIEvent* EECreateScreenEvent(unsigned long inEvent)
{
    if (inEvent == 0)
        return NULL;
    return new EEScreenEvent(inEvent);
}

static float Float(uint32_t inBits)
{
    float theValue;
    memcpy(&theValue, &inBits, 4);
    return theValue;
}
static uint32_t Bits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

static UWideString Text(const uint8_t* inText, size_t inLength)
{
    return UWideString(std::string((const char*)inText, inLength).c_str());
}

// ---- the hot keys' (hot_keys.rs): inForm is the main game form

// the last snapshot's complex units (EE_SCREENS_GAME_HK_CENTER's record index), filled by ee_screenscpp_hot_key_units
static std::vector<EEComplexUnit*> sHotKeyUnits;

// the current player's units as the hot keys see them (hot_keys.rs EeScreensHotKeyUnit; the aircraft's with the
// squared distances from the point under the view's centre); up to inCapacity into outUnits; their count
extern "C" size_t ee_screenscpp_hot_key_units(bool inAircraft, EeScreensHotKeyUnit* outUnits, size_t inCapacity)
{
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    EESelectedGroup* theGroup = thePlayer->mSelectedGroup;
    GE3DPoint theTarget;
    GE3DPoint theLocation;
    if (inAircraft) {
        GE2DPoint theCursor(gU4E8F08.GetXCenter(), gU4E8F08.GetYCenter());
        if (gWorld.mMap->GetLocationAt(theCursor, theTarget, true) == NULL) {
            long theX;
            long theY;
            gWorld.mMap->GetTileAt(theCursor, theX, theY);
            theTarget.SetX(theX + 0.5f);
            theTarget.SetY(theY + 0.5f);
        }
        theTarget.SetZ(0.0f);
        theLocation.SetZ(0.0f);
    }
    sHotKeyUnits.clear();
    size_t theCount = 0;
    for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit; theUnit = static_cast<EEUnit*>(theUnit->mNext), theCount++) {
        EEComplexUnit* theComplex = theUnit->mComplexUnit;
        sHotKeyUnits.push_back(theComplex);
        if (theCount >= inCapacity)
            continue;
        EeScreensHotKeyUnit& r = outUnits[theCount];
        r = EeScreensHotKeyUnit();
        r.unit_id = theUnit->mUnitID;
        if (theComplex == NULL)
            continue;
        r.flags |= EE_SCREENS_HK_F_COMPLEX;
        if (theUnit->mHitPoints > 0)
            r.flags |= EE_SCREENS_HK_F_ALIVE;
        if (theUnit->mTile != NULL)
            r.flags |= EE_SCREENS_HK_F_ON_TILE;
        if ((r.flags & (EE_SCREENS_HK_F_ALIVE | EE_SCREENS_HK_F_ON_TILE)) == (EE_SCREENS_HK_F_ALIVE | EE_SCREENS_HK_F_ON_TILE)) {
            // the unit selection's fields
            r.complex_id = theComplex->mUnitID;
            r.unit_type = theUnit->mUnitClass->mUnitType;
            r.category = theUnit->mUnitClass->mCategory;
            r.command = theComplex->mCommand ? theComplex->mCommand->mType : 0;
            r.max_velocity = Bits(theUnit->mStats->mMaxVelocity);
        }
        if (theGroup->Contains(theUnit->GetComplexUnit()))
            r.flags |= EE_SCREENS_HK_F_SELECTED;
        // the aircraft's fields (of the complex unit)
        r.c_category = theComplex->mUnitClass->mCategory;
        r.c_hit_points = theComplex->mHitPoints;
        if (theComplex->mIsCommandable)
            r.flags |= EE_SCREENS_HK_F_C_COMMANDABLE;
        if (theComplex->mTileX != -10000)
            r.flags |= EE_SCREENS_HK_F_C_TILE;
        if (theComplex->mContainer)
            r.flags |= EE_SCREENS_HK_F_C_CONTAINER;
        if (theComplex->mIsDisabled)
            r.flags |= EE_SCREENS_HK_F_C_DISABLED;
        if (theComplex->mPreviousPart != NULL)
            r.flags |= EE_SCREENS_HK_F_C_PREVIOUS_PART;
        if (theComplex->mGoalState) {
            r.flags |= EE_SCREENS_HK_F_C_GOAL;
            r.c_goal_kind = theComplex->mGoalState->mKind;
        }
        r.c_family = theComplex->mUnitClass->mFamily;
        r.c_ammo = theComplex->mUnitClass->mAmmoType;
        r.c_type = theComplex->mUnitClass->mUnitType;
        r.c_max_hit_points = theComplex->mStats->mMaxHitPoints;
        if (inAircraft) {
            if (theComplex->mContainer == NULL) {
                theLocation.SetX(theComplex->mPhysicalModel.GetLocation().GetX());
                theLocation.SetY(theComplex->mPhysicalModel.GetLocation().GetY());
            } else {
                theLocation.SetX(theComplex->mContainer->mPhysicalModel.GetLocation().GetX());
                theLocation.SetY(theComplex->mContainer->mPhysicalModel.GetLocation().GetY());
            }
            r.distance = Bits(theLocation.DistanceToSquared(theTarget));
        }
    }
    return theCount;
}

static uint32_t EEScreensHotKeyCall(UIForm* inForm, uint32_t inOp, uint32_t inA, uint32_t inB, uint32_t inC)
{
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    switch (inOp) {
    case EE_SCREENS_GAME_HK_UPDATE_SELECTION:
        thePlayer->UpdateSelection(NULL, true);
        break;
    case EE_SCREENS_GAME_HK_RESET_SAVED:
        thePlayer->mSavedGroups.Reset(0x14, true);
        break;
    case EE_SCREENS_GAME_HK_GROUP:
        if (inA == 0)
            gCommunications->ProcessLocalUserInput(new EEMSavedGroupSelect((unsigned char)inB, inC != 0), kSlotNone);
        else if (inA == 1)
            gCommunications->ProcessLocalUserInput(new EEMSavedGroupAddSelection((unsigned char)inB), kSlotNone);
        else
            gCommunications->ProcessLocalUserInput(new EEMSavedGroupCreate((unsigned char)inB), kSlotNone);
        break;
    case EE_SCREENS_OP_HK_GROUP_BUTTON:
        static_cast<EEUIBSavedGroup*>(inForm->GetControl(inA))->SetUnit(thePlayer->mSelectedGroup->GetFirstUnit());
        break;
    case EE_SCREENS_GAME_HK_PLAYER_EVENT: {
        EEAction* theAction = new EEAPlayerEvent;
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
        break;
    }
    case EE_SCREENS_GAME_HK_SHIFT: {
        ISEntity* theKey;
        gISKeyboard->GetEntity(kKeyShift, theKey);
        return (uint32_t)theKey->GetValue();
    }
    case EE_SCREENS_GAME_HK_SELECT_UNIT:
        gCommunications->ProcessLocalUserInput(new EEMSelectUnit(inA), kSlotNone);
        break;
    case EE_SCREENS_GAME_HK_ADD_UNIT:
        gCommunications->ProcessLocalUserInput(new EEMAlwaysAddUnitToSelectedGroup(inA), kSlotNone);
        break;
    case EE_SCREENS_GAME_HK_CENTER: {
        // the camera on the unit, the mini map refreshed, the camera no longer following
        const GETransformation& theTransformation = sHotKeyUnits[inA]->mPhysicalModel.GetLocation();
        GE3DPoint theCenter;
        theCenter.SetX(theTransformation.GetX());
        theCenter.SetY(theTransformation.GetY());
        theCenter.SetZ(theTransformation.GetZ());
        gWorld.mCameraControl.SetChanged();
        gWorld.mCameraControl.SetLocation(theCenter, false, true);
        gMiniMap->Refresh();
        gCameraFollowing = false;
        break;
    }
    case EE_SCREENS_GAME_HK_ALARM:
        if (EESoundObject::Get(kSoundHotKeyAlarm))
            gSoundManager->Play(EESoundObject::Get(kSoundHotKeyAlarm)->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
        break;
    }
    return 0;
}

// ---- the chat forms' (chat.rs): inForm is the chat form or the chat log form (NULL for the log's posted close)
static uint32_t EEScreensChatCall(UIForm* inForm, uint32_t inOp, uint32_t inA, uint32_t inB, uint32_t inC)
{
    switch (inOp) {
    case EE_SCREENS_GAME_CH_CHAT_TYPE:
        gCommunications->ProcessLocalUserInput(new EEMSelectPlayerChatType((long)inA), kSlotNone);
        break;
    case EE_SCREENS_OP_CH_SEND_KEYS: {
        // Enter: one event for the hot key and the edit box's Enter
        UIHotKey theHotKey;
        UIEvent* theEvent = EECreateScreenEvent(inA);
        theHotKey.SetKey(DIK_RETURN);
        theHotKey.SetJustPressedEvent(theEvent);
        inForm->GetControl(kChatTextEditBox)->SetTriggerEvent(kControlTriggerEnterPressed, theEvent);
        theEvent->Dereference();
        inForm->AddHotKey(theHotKey, 0);
        break;
    }
    case EE_SCREENS_OP_CH_HOT_KEY:
        inForm->AddHotKey(inA, inB, kKeyNone, kKeyNone, EECreateScreenEvent(inC), NULL, NULL, NULL, 0, false);
        break;
    case EE_SCREENS_GAME_CH_OBSERVER:
        return gWorld.GetCurrentPlayer()->mObserverMode != false;
    case EE_SCREENS_GAME_CH_PLAYER_TYPE:
        return gWorld.GetCurrentPlayer()->mChatType;
    case EE_SCREENS_OP_CH_TABBED_TO:
        inForm->GetControl(inA)->TabbedTo();
        break;
    case EE_SCREENS_OP_CH_KEYBOARD_EVENT: {
        UIEvent* theEvent = EECreateScreenEvent(inA);
        inForm->SetFormKeyboardCapturedEvent(theEvent);
        theEvent->Dereference();
        break;
    }
    case EE_SCREENS_OP_CH_KEYBOARD_CAPTURED:
        return inForm->GetIsKeyboardCaptured();
    case EE_SCREENS_OP_CH_TAB_STOP:
        inForm->SetTabStop(inA);
        break;
    case EE_SCREENS_GAME_CH_CLOSE:
        return (uint32_t)gUserInterface->CloseForm(gUIManager->mMainGameForm);
    case EE_SCREENS_OP_CH_MOUSE_INSIDE:
        return inForm->GetRectangle().Contains(inForm->GetMouseLocation());
    case EE_SCREENS_GAME_CH_CHEATS:
        return gWorld.GetCheatsEnabled();
    case EE_SCREENS_OP_CL_BEGIN: {
        UITextLabel* theLabel = static_cast<UITextLabel*>(gUIManager->GetFormByID(kFormChatLog)->GetControl(kChatLogChatLogTextBox));
        theLabel->DeleteArray();
        theLabel->SetAlignment(kTextAlignLeft);
        break;
    }
    case EE_SCREENS_GAME_CL_COUNT:
        return EEUIECLEFormManager::Get()->mEntries.size();
    case EE_SCREENS_OP_CL_CENTER:
        static_cast<UITextLabel*>(gUIManager->GetFormByID(kFormChatLog)->GetControl(kChatLogChatLogTextBox))->SetAlignment(kTextAlignCenter);
        break;
    case EE_SCREENS_OP_CL_END: {
        UITextLabel* theLabel = static_cast<UITextLabel*>(gUIManager->GetFormByID(kFormChatLog)->GetControl(kChatLogChatLogTextBox));
        theLabel->SetStartingDisplayLine(theLabel->GetLineCount() - 1);
        break;
    }
    case EE_SCREENS_GAME_CL_UNPAUSE:
        gWorld.UnpauseGame();
        break;
    default:
        return EEScreensHotKeyCall(inForm, inOp, inA, inB, inC);
    }
    return 0;
}

// ---- the main game form's events' (main_game.rs): inForm is the main game form (or the editor's, for the media
// dump); the behaviour menu's calls are on the manager's main game form, as the original's
static uint32_t EEScreensMainGameCall(EEUIFMainGame* inForm, uint32_t inOp, uint32_t inA, uint32_t inB, uint32_t inC)
{
    switch (inOp) {
    case EE_SCREENS_GAME_MG_UI_STATE:
        return (uint32_t)inForm->mUIState;
    case EE_SCREENS_GAME_MG_GHOSTS:
        return gWorld.GetNumGhostUnits();
    case EE_SCREENS_GAME_MG_CLEAR_GHOSTS:
        gWorld.mGhostUnitsLock->Enter();
        gWorld.ClearGhostUnits();
        gWorld.mGhostUnitsLock->Leave();
        break;
    case EE_SCREENS_GAME_MG_CLEAR_GATE_GHOSTS:
        inForm->mBuildGate.ClearGhostUnits();
        break;
    case EE_SCREENS_GAME_MG_SET_UI_STATE:
        inForm->SetUIState((long)(int32_t)inA);
        break;
    case EE_SCREENS_GAME_MG_POINTER:
        gMouse->mPointer = (long)(int32_t)inA;
        break;
    case EE_SCREENS_GAME_MG_CURSOR_LOCK:
        inForm->mIsCursorLocked = inA != 0;
        break;
    case EE_SCREENS_GAME_MG_REFRESH_GROUP:
        if (gUIManager->mMainGameForm)
            gUIManager->mMainGameForm->mRefreshGroup = true;
        break;
    case EE_SCREENS_GAME_MG_DUMP_MEDIA: {
        // Media Dump.txt in the startup folder (replaced when inA), the loaded media (and leaks when inB) appended
        FSFileSpec theFile;
        theFile.SetVolume(gStartupVolume);
        theFile.SetDirectory(gStartupDirectory);
        theFile.SetFile("Media Dump.txt");
        if (inA)
            theFile.Delete();
        theFile.Create();
        long theResult = theFile.FOpen("a+");
        if (theResult == 0) {
            gUserInterface->AddChatLogEntry("Dumped Media Information.", theResult);
            gWorld.DumpMedia(theFile, inB != 0);
            theFile.Close();
        }
        return (uint32_t)theResult;
    }
    case EE_SCREENS_GAME_MG_SHOW_DIALOG:
        return (uint32_t)gPhysics->PostShowDialogMessage();
    case EE_SCREENS_GAME_MG_CURRENT_GROUP:
        inForm->mCurrentGroup = inA;
        break;
    case EE_SCREENS_GAME_MG_ENTITY_GROUP:
        inForm->SetEntityGroup((long)inA);
        break;
    case EE_SCREENS_GAME_MG_REFRESH_FORM:
        inForm->FnE123D();
        break;
    case EE_SCREENS_GAME_MG_CIV_BUILDER:
        return (uint32_t)gUserInterface->PostDisplayCivBuilder();
    case EE_SCREENS_GAME_MG_HAS_PLAYER:
        return gWorld.GetCurrentPlayer() != NULL;
    case EE_SCREENS_GAME_MG_SHIFT:
        if (inA)
            gCommunications->ProcessLocalUserInput(new EEMEnterShiftMode(gPhysics->mGameTime), kSlotNone);
        else
            gCommunications->ProcessLocalUserInput(new EEMExitShiftMode(gPhysics->mGameTime), kSlotNone);
        break;
    case EE_SCREENS_GAME_MG_AIRPORT_SELECTED: {
        EESelectedGroup* theGroup = gWorld.GetCurrentPlayer()->mSelectedGroup;
        for (unsigned long i = 0; i < theGroup->GetNumUnits(); i++) {
            EEComplexUnit* theUnit = theGroup->GetUnit(i);
            if (theUnit && theUnit->mUnitClass->mUnitType == kUnitTypeAirport)
                return 1;
        }
        return 0;
    }
    case EE_SCREENS_GAME_MG_RALLY:
        gCommunications->ProcessLocalUserInput(new EEMIncrementRallyTimer((unsigned char)inA), kSlotNone);
        break;
    case EE_SCREENS_GAME_MG_HOT_KEY_MODE:
        return gUIManager->mMainGameForm->mHotKeyMode;
    case EE_SCREENS_GAME_MG_TO_EDITOR: {
        // back from a test game to the scenario editor: the ghosts cleared, the player to the editor slot, the
        // simulation stopped, the editor switch and the pause posted
        NEProtocol* theProtocol = NULL;
        gWorld.mGhostUnitsLock->Enter();
        gWorld.ClearGhostUnits();
        gWorld.mGhostUnitsLock->Leave();
        NEProtocol::GetProtocol(0, theProtocol);
        gWorld.MovePlayerToSlot(0xfe, gWorld.GetCurrentPlayer()->GetSlot(), 1, theProtocol->GetLocalAddress(gNetworkAdapter), 0);
        gPhysics->mThread->Post(0x402, NULL, NULL, true);
        long theResult = gUserInterface->PostEnterScenarioEditor(gUIManager->GetInScenarioEditor());
        gUserInterface->PostPauseInformation(true);
        return (uint32_t)theResult;
    }
    case EE_SCREENS_GAME_MG_QUICK_SAVE: {
        // the timestamped quick save (remembered in the settings) to every player
        UWideString theName;
        TMTime theTime;
        NESynchronousServer* theServer = dynamic_cast<NESynchronousServer*>(gCommunications);
        TMGetTimeOfDay(theTime);
        sprintf(theName, "%s %4ld.%02ld.%02ld - %02ldh.%02ldm.%02lds%s", "QuickSave", theTime.mYear, theTime.mMonth + 1,
                theTime.mDay, theTime.mHour, theTime.mMinute, theTime.mSecond, (const char*)gSavedGameExtension);
        gGamePreferences.mSettings.mLastQuickSave = theName;
        gGamePreferences.Save();
        theServer->SendToAllAndWait(new EEMLoadOrSaveGame(theName, TMGetTime(), 0, 1,
                                                          gCommunications->GetNumberOfCTsProcessedInPhysics(), 0),
                                    15000, 12);
        break;
    }
    case EE_SCREENS_GAME_MG_DIALOG_SHOWN: {
        UIControl* theMessage = inForm->GetControl(kMainGameDialogMessage);
        return (theMessage != NULL && theMessage->GetDisplayMode() == kDisplayModeShown) || gWorld.mInCinematic;
    }
    case EE_SCREENS_GAME_MG_CLOCK:
        return (uint32_t)inForm->mClockMode;
    case EE_SCREENS_GAME_MG_SET_CLOCK:
        inForm->mClockMode = (long)inA;
        break;
    case EE_SCREENS_GAME_MG_KILL:
        return (uint32_t)gCommunications->ProcessLocalUserInput(new EEMKillUnit(inA != 0), kSlotNone);
    case EE_SCREENS_GAME_MG_ATTACK:
        return (uint32_t)gCommunications->ProcessLocalUserInput(new EEMAttackPlayers(gWorld.mCurrentPlayer, inA != 0),
                                                                kSlotNone);
    case EE_SCREENS_GAME_MG_HIDDEN_MODE:
        return (uint32_t)gHiddenUnitsMode;
    case EE_SCREENS_GAME_MG_SET_HIDDEN_MODE:
        gHiddenUnitsMode = (long)inA;
        break;
    case EE_SCREENS_GAME_MG_MARK_HIDDEN_MOVED:
        for (EEUnit* theUnit = (EEUnit*)gWorld.mUnits.mFirst; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mTileNode->mNext)) {
            EEComplexUnit* theComplex = theUnit->mComplexUnit;
            if (theComplex && theComplex->mCanBeHidden)
                theComplex->mHiddenFlags |= kHiddenFlagMoved;
        }
        break;
    case EE_SCREENS_GAME_MG_FOLLOW_LEAD: {
        EEUnit* theUnit = gWorld.GetUnit(EEGetLeadUnitID(gWorld.GetCurrentPlayer()));
        gWorld.mCameraControl.Follow(theUnit, inA != 0, 1.0f, 0.0f, 0.0f);
        if (inA == 0)
            gWorld.mCameraControl.SetChanged();
        break;
    }
    case EE_SCREENS_GAME_MG_CAMERA_MODE:
        return (uint32_t)gWorld.mCameraControl.mMode;
    case EE_SCREENS_GAME_MG_SET_CAMERA_MODE:
        gWorld.mCameraControl.SetMode((long)inA);
        break;
    case EE_SCREENS_GAME_MG_ZOOM_STYLE:
        gGamePreferences.mSettings.mZoomStyle = gWorld.mCameraControl.mMode;
        break;
    case EE_SCREENS_GAME_MG_LOCK_SPEED:
        return gWorld.mGameOptions.GetLockSpeed() != false;
    case EE_SCREENS_GAME_MG_GAME_SPEED:
        return Bits(gWorld.GetGameSpeed());
    case EE_SCREENS_GAME_MG_SPEED_TABLE:
        return Bits(inA == 0   ? gWorldTable.GetFloatValue(kWorldGameSpeedStep)
                    : inA == 1 ? gWorldTable.GetFloatValue(kWorldGameSpeedRange)
                               : gWorldTable.GetFloatValue2(kWorldGameSpeedRange));
    case EE_SCREENS_GAME_MG_SEND_SPEED:
        gCommunications->ProcessLocalUserInput(new EEMGameSpeed(Float(inA)), kSlotNone);
        break;
    case EE_SCREENS_GAME_MG_SELECTED_COMMAND: {
        EEUIFMGMainGroup* theGroup = static_cast<EEUIFMGMainGroup*>(gUIManager->mMainGameForm->mGroups[0]);
        EEUnit* theUnit = theGroup->GetSelectedUnit();
        if (!theUnit || !theUnit->mComplexUnit || !theUnit->mUnitClass)
            return 0;
        return theUnit->mUnitClass->mCommands[inA];
    }
    case EE_SCREENS_GAME_MG_BEHAVIOR:
        gCommunications->ProcessLocalUserInput(new EEMSetUnitBehavior((long)inA), kSlotNone);
        break;
    case EE_SCREENS_GAME_MG_CLOSE_BEHAVIORS:
        gUIManager->mMainGameForm->GetEntityGroup(9)->Disable();
        gUIManager->mMainGameForm->GetBackgroundByID(0x97)->SetVisibility(false);
        break;
    case EE_SCREENS_GAME_MG_COPY_BITMAPS: {
        UIButton* theButton = static_cast<UIButton*>(gUIManager->mMainGameForm->GetControl(inA));
        UIButton* theSource = static_cast<UIButton*>(gUIManager->mMainGameForm->GetControl(inB));
        theButton->SetNormalBitmap(theSource->GetNormalBitmap());
        theButton->SetPressedBitmap(theSource->GetPressedBitmap());
        break;
    }
    default:
        return EEScreensChatCall(inForm, inOp, inA, inB, inC);
    }
    return 0;
}

// ---- the civilization builder's (civ_builder.rs): inForm is the builder's form or one of its dialogs

static UWideString sCivBuilderText;   // the typed name or the load list's selected file (EE_SCREENS_TEXT_CB_NAME)

// ---- the camera, scroll and zoom events' (camera.rs): inForm is the main game form or the scenario editor's

static UIEvent* sGrabSaved[2];   // each form's grab scroll: the mouse move event it saved (referenced) while grabbing

UIEvent* EECreateGrabScroll(unsigned long inForm)
{
    EEReleaseGrabScroll(inForm);
    return EECreateScreenEvent(inForm ? EE_SCREENS_CAM_GRAB_EDITOR : EE_SCREENS_CAM_GRAB_MAIN);
}

void EEReleaseGrabScroll(unsigned long inForm)
{
    if (sGrabSaved[inForm] != NULL) {
        sGrabSaved[inForm]->Dereference();
        sGrabSaved[inForm] = NULL;
    }
    ee_screens_camera_grab_reset(inForm);
}

// the mouse's X axis, Y axis, middle button and wheel (EE_SCREENS_CAM_AXIS_*)
static ISEntity* EEScreensMouseEntity(uint32_t inAxis)
{
    static const unsigned long kAxes[] = {kMouseXAxis, kMouseYAxis, kMouseMiddleButton, kMouseZAxis};
    ISEntity* theEntity = NULL;
    gISMouse->GetEntity(kAxes[inAxis & 3], theEntity);
    return theEntity;
}

static uint32_t EEScreensCameraCall(UIForm* inForm, uint32_t inOp, uint32_t inA, uint32_t inB, uint32_t inC)
{
    EECameraControl& theCamera = gWorld.mCameraControl;
    switch (inOp) {
    case EE_SCREENS_GAME_CAM_FREEZE:
        gMouse->mIsFrozen = inA != 0;
        break;
    case EE_SCREENS_GAME_CAM_MOUSE_PRESENT:
        return EEScreensMouseEntity(inA) != NULL;
    case EE_SCREENS_GAME_CAM_MOUSE_VALUE: {
        ISEntity* theEntity = EEScreensMouseEntity(inA);
        return (uint32_t)(inB ? theEntity->GetNormalizedValue() : theEntity->GetValue());
    }
    case EE_SCREENS_GAME_CAM_FREE:
        return gFreeCamera;
    case EE_SCREENS_GAME_CAM_REVOLVE:
        theCamera.Revolve(GE3DPoint(Float(inA), Float(inB), 0.0f));
        break;
    case EE_SCREENS_GAME_CAM_ROTATE:
        theCamera.Rotate(GE3DPoint(Float(inA), Float(inB), 0.0f));
        break;
    case EE_SCREENS_GAME_CAM_ZOOM:
        return Bits(theCamera.GetZoom());
    case EE_SCREENS_GAME_CAM_GRAB_SPEED:
        return Bits(gGamePreferences.mSettings.GetGrabScrollSpeed());
    case EE_SCREENS_GAME_CAM_SCROLL:
        theCamera.Scroll(Float(inA), Float(inB));
        break;
    case EE_SCREENS_GAME_CAM_ZOOM_ENABLED:
        return inA ? gZoomEnabled : gGamePreferences.mSettings.mZoomEnabled;
    case EE_SCREENS_GAME_CAM_CINEMATIC:
        return gWorld.mInCinematic;
    case EE_SCREENS_GAME_CAM_RESET_ZOOM:
        theCamera.ResetZoom();
        break;
    case EE_SCREENS_GAME_CAM_CHANGED:
        theCamera.SetChanged();
        break;
    case EE_SCREENS_GAME_CAM_SET_ZOOM:
        theCamera.SetZoom(Float(inA));
        break;
    case EE_SCREENS_GAME_CAM_MAX_ZOOM:
        return Bits(gCameraMaxZoom);
    case EE_SCREENS_GAME_CAM_COS:
        return Bits(UMCos(Float(inA)));
    case EE_SCREENS_OP_CAM_GRAB_BEGIN: {
        // the form's mouse move event saved; the grab its mouse move event while the button is down
        sGrabSaved[inA] = inForm->GetMouseMoveEvent();
        if (sGrabSaved[inA])
            sGrabSaved[inA]->Reference();
        UIEvent* theGrab = EECreateScreenEvent(inA ? EE_SCREENS_CAM_GRAB_EDITOR : EE_SCREENS_CAM_GRAB_MAIN);
        inForm->SetMouseMoveEvent(theGrab);
        theGrab->Dereference();
        break;
    }
    case EE_SCREENS_OP_CAM_GRAB_END:
        inForm->SetMouseMoveEvent(sGrabSaved[inA]);
        if (sGrabSaved[inA]) {
            sGrabSaved[inA]->Dereference();
            sGrabSaved[inA] = NULL;
        }
        break;
    case EE_SCREENS_GAME_MG_SET_FORMATION: {
        // the selected group's formation, posted; the formation menu closed
        EEFormation* theFormation = EEFormation::Create((long)inA);
        gCommunications->ProcessLocalUserInput(new EEMChangeFormation((long)inA), kSlotNone);
        gUIManager->mMainGameForm->GetEntityGroup(8)->Disable();
        gUIManager->mMainGameForm->GetBackgroundByID(0x98)->SetVisibility(false);
        theFormation->mB5C = true;
        gWorld.mPlayers[gWorld.mCurrentPlayer]->mSelectedGroup->SetFormation(theFormation);
        theFormation->Dereference();
        break;
    }
    default:
        return EEScreensMainGameCall(static_cast<EEUIFMainGame*>(inForm), inOp, inA, inB, inC);
    }
    return 0;
}

// ---- the diplomacy form's (diplomacy.rs): inForm is the diplomacy form

// player inIndex of the world as the diplomacy form reads it; false for an empty entry
extern "C" bool ee_screenscpp_diplomacy_player(uint32_t inIndex, EeScreensDiplomacyPlayer* outPlayer)
{
    EEPlayer* thePlayer = gWorld.GetPlayer(inIndex);
    if (thePlayer == NULL)
        return false;
    outPlayer->player_id = thePlayer->GetPlayerID();
    outPlayer->game_result = thePlayer->GetGameResult();
    outPlayer->observer = thePlayer->GetObserverMode();
    outPlayer->human = thePlayer->IsHuman();
    outPlayer->slot = thePlayer->GetSlot();
    for (long r = 0; r < 5; r++)
        outPlayer->resources[r] = thePlayer->GetResource(kResourceFood + r);
    for (unsigned long q = 0; q < 17; q++) {
        outPlayer->enemies[q] = thePlayer->GetEnemy(q);
        outPlayer->send_chat[q] = thePlayer->GetSendChatMessage(q);
    }
    outPlayer->tribute_fee = thePlayer->GetTributeFee();
    return true;
}

// a tribute to player inTo (food, wood, gold, stone, iron) to the local input
extern "C" void ee_screenscpp_diplomacy_tribute(uint32_t inTo, const uint32_t* inAmounts)
{
    gCommunications->ProcessLocalUserInput(new EEMPayTribute((unsigned char)inTo, inAmounts[0], inAmounts[1], inAmounts[2],
                                                             inAmounts[3], inAmounts[4]), kSlotNone);
}

static uint32_t EEScreensDiplomacyCall(UIForm* inForm, uint32_t inOp, uint32_t inA, uint32_t inB, uint32_t inC,
                                       uint32_t inD)
{
    switch (inOp) {
    case EE_SCREENS_GAME_DS_PLAYERS:
        return gWorld.GetNumPlayers();
    case EE_SCREENS_GAME_DS_CURRENT:
        return gWorld.mCurrentPlayer;
    case EE_SCREENS_GAME_DS_TEAMS_LOCKED:
        return gWorld.GetTeamsLocked();
    case EE_SCREENS_OP_DS_UV: {
        GETextureCoordinate theTopLeft, theTopRight, theBottomRight, theBottomLeft;
        theTopLeft.mU = Float(inB);
        theTopLeft.mV = 0.0f;
        theTopRight.mU = Float(inC);
        theTopRight.mV = 0.0f;
        theBottomRight.mU = Float(inB);
        theBottomRight.mV = Float(inD);
        theBottomLeft.mU = Float(inC);
        theBottomLeft.mV = Float(inD);
        static_cast<UIBitmap*>(inForm->GetControl(inA))->GetBitmap()->SetTextureCoordinates(theTopLeft, theTopRight,
                                                                                            theBottomRight, theBottomLeft);
        break;
    }
    case EE_SCREENS_OP_DS_BG:
        inForm->GetControl(inA)->SetBgColor(GEColor(Float(inB), Float(inC), Float(inD)));
        break;
    case EE_SCREENS_OP_DS_FG: {
        UIControl* theControl = inForm->GetControl(inA);
        GEText* theText = inC ? static_cast<UIButton*>(theControl)->GetGEText()
                              : static_cast<UITextLabel*>(theControl)->GetGEText();
        theText->SetFgColor(gWorld.GetPlayer(inB)->GetColor());
        break;
    }
    case EE_SCREENS_OP_DS_DISABLE:
        static_cast<UICheckBox*>(inForm->GetControl(inA))->SetDisabled(true);
        break;
    case EE_SCREENS_OP_DS_BUTTON_NUMBER:
        return (uint32_t)atoi(static_cast<UIButton*>(inForm->GetControl(inA))->GetGEText()->GetText());
    case EE_SCREENS_OP_DS_LABEL_NUMBER:
        return (uint32_t)static_cast<UITextLabel*>(inForm->GetControl(inA))->GetTextAsInt(0);
    case EE_SCREENS_GAME_DS_RESOURCES:
        gCommunications->ProcessLocalUserInput(new EEMAddRemoveResources(inA, (long)inB, inC, (unsigned char)inD), kSlotNone);
        break;
    case EE_SCREENS_GAME_DS_STANCE:
        gCommunications->ProcessLocalUserInput(new EEMSetDiplomaticStance((long)inA, (long)inB), kSlotNone);
        break;
    case EE_SCREENS_GAME_DS_CHAT:
        gCommunications->ProcessLocalUserInput(new EEMSelectCustomPlayerForChat((long)inA, inB != 0), kSlotNone);
        break;
    default:
        return EEScreensCameraCall(inForm, inOp, inA, inB, inC);
    }
    return 0;
}

static uint32_t EEScreensCivBuilderCall(UIForm* inForm, UIEntity* inEntity, uint32_t inOp, uint32_t inA, uint32_t inB,
                                        uint32_t inC, uint32_t inD)
{
    switch (inOp) {
    case EE_SCREENS_GAME_CB_SHOW_NAME_DIALOG:
        return (uint32_t)CivBuilderShowNameDialog();
    case EE_SCREENS_GAME_CB_TYPED:
        sCivBuilderText = UWideString();
        return (uint32_t)(inA ? CivBuilderGetTypedFileName(sCivBuilderText) : CivBuilderGetTypedName(sCivBuilderText));
    case EE_SCREENS_GAME_CB_ACCEPT_NAME:
        return (uint32_t)CivBuilderAcceptName();
    case EE_SCREENS_GAME_CB_CLOSE:
        return (uint32_t)CivBuilderClose(inA != 0);
    case EE_SCREENS_GAME_CB_SAVE_AS:
        return (uint32_t)CivBuilderSaveAs();
    case EE_SCREENS_GAME_CB_SHOW_LOAD:
        return (uint32_t)CivBuilderShowLoad();
    case EE_SCREENS_GAME_CB_SHOW_SAVE:
        return (uint32_t)CivBuilderShowSave();
    case EE_SCREENS_GAME_CB_SELECTION: {
        // the load list (the builder's helper finds it) or the event form's save list; the load list's file kept
        UIFileListBox* theList = inA ? static_cast<UIFileListBox*>(inForm->GetControlById(kCivBuilderIdSaveCivFileListBox))
                                     : CivBuilderGetLoadFileList();
        if (theList == NULL)
            return EE_SCREENS_CB_NO_LIST;
        unsigned long theSelection;
        long theResult = theList->GetCurrentSelection(theSelection);
        if (theResult == kErrorOK && !inA)
            sCivBuilderText = theList->GetData(theSelection);
        return (uint32_t)theResult;
    }
    case EE_SCREENS_GAME_CB_SET_NAME_FROM_CIVILIZATION:
        return (uint32_t)CivBuilderSetName(*gEditableCivilization->GetName());
    case EE_SCREENS_GAME_CB_CLOSE_FORM:
        return (uint32_t)CivBuilderCloseForm();
    case EE_SCREENS_GAME_CB_UNCHANGED:
        return gEditableCivilization->GetIsUnchanged();
    case EE_SCREENS_GAME_CB_SAVE_NAME_SELECTED:
        return (uint32_t)CivBuilderSaveNameSelected();
    case EE_SCREENS_GAME_CB_CAN_CHANGE:
        return CivBuilderCanChange();
    case EE_SCREENS_GAME_CB_RESET_AVAILABLE:
        return (uint32_t)CivBuilderResetAvailableSelection(inForm);
    case EE_SCREENS_GAME_CB_RESET_BOUGHT:
        return (uint32_t)CivBuilderResetBoughtSelection(inForm);
    case EE_SCREENS_GAME_CB_LOCKED:
        return gB565566;
    case EE_SCREENS_GAME_CB_BUY:
        return (uint32_t)CivBuilderBuyUpgrade(0);
    case EE_SCREENS_GAME_CB_RETURN:
        return (uint32_t)CivBuilderReturnUpgrade();
    case EE_SCREENS_GAME_CB_POINTS_LEFT:
        return (uint32_t)gEditableCivilization->GetPointsLeft();
    case EE_SCREENS_GAME_CB_ASK_ACCEPT:
        return (uint32_t)CivBuilderAskAccept(inForm);
    case EE_SCREENS_GAME_CB_ACCEPT_CHANGES:
        CivBuilderAcceptChanges(NULL);
        break;
    case EE_SCREENS_GAME_CB_FORM_ID:
        return inForm->GetFormID();
    case EE_SCREENS_GAME_CB_ASK_DELETE:
        return (uint32_t)CivBuilderAskDelete(inForm, inEntity);
    case EE_SCREENS_GAME_CB_CLEAN_UP:
        delete gEditableCivilization;
        gEditableCivilization = NULL;
        break;
    case EE_SCREENS_OP_CB_SHOW:
        gUIManager->GetFormByID(kFormCivBuilder)->GetControl(inA >> 1)->SetDisplayMode(inA & 1 ? kDisplayModeShown
                                                                                                 : kDisplayModeHidden);
        break;
    case EE_SCREENS_GAME_CB_HIDE_POINTS:
        CivBuilderHidePoints();
        break;
    default:
        return EEScreensDiplomacyCall(inForm, inOp, inA, inB, inC, inD);
    }
    return 0;
}

// ---- the civilization choice form's (civ_choice.rs): the form 35 by the manager, or the event's form
static std::vector<EECivilizationEntry> sPdcsCivs;   // EE_SCREENS_GAME_PDCS_CIVS's, for the names and files
static UWideString sPdcsName;                        // the last EE_SCREENS_TEXT_PDCS_LOAD's civilization
static std::vector<EECivUpgrade> sPdcsUpgrades;

static EECBUIListBox* EEScreensPdcsControl(unsigned long inIndex)
{
    return static_cast<EECBUIListBox*>(gUIManager->GetFormByID(kFormPredefinedCiv)->GetControl(inIndex));
}

static uint32_t EEScreensCivChoiceCall(UIForm* inForm, UIEntity* inEntity, uint32_t inOp, uint32_t inA, uint32_t inB,
                                       uint32_t inC, uint32_t inD)
{
    switch (inOp) {
    case EE_SCREENS_OP_PDCS_STYLE: {
        EECBUIListBox* theList = static_cast<EECBUIListBox*>(inForm->GetControl(inA));
        theList->SetAutoSelect(false);
        if (inB & 2) {
            theList->mShowPoints = false;
            theList->mSelectUpgradesOnly = false;
        }
        theList->SetSortLines((inB & 1) != 0);
        theList->mC13C = GEColor(Float(inC), Float(inC), Float(inD));
        break;
    }
    case EE_SCREENS_GAME_PDCS_FILL:
        gCivilizationManager.FillCivilizationList(static_cast<EECBUIListBox*>(inForm->GetControl(inA)));
        break;
    case EE_SCREENS_OP_PDCS_HAS_SELECTION:
        return static_cast<EECBUIListBox*>(inForm->GetControl(inA))->GetHasSelection();
    case EE_SCREENS_GAME_PDCS_PAUSE:
        gWorld.PauseGame();
        break;
    case EE_SCREENS_GAME_PDCS_CIVS:
        sPdcsCivs.clear();
        gCivilizationManager.GetCivilizations(sPdcsCivs);
        return (uint32_t)sPdcsCivs.size();
    case EE_SCREENS_OP_PDCS_CLEAR_STATS:
        EEScreensPdcsControl(4)->DeleteAllEntries();
        break;
    case EE_SCREENS_OP_PDCS_SELECTION: {
        unsigned long theSelection;
        return EEScreensPdcsControl(3)->GetCurrentSelection(theSelection) != kErrorOK ? 0 : theSelection + 1;
    }
    case EE_SCREENS_OP_PDCS_CONTROLS:
        return EEScreensPdcsControl(3) && EEScreensPdcsControl(4) && EEScreensPdcsControl(5);
    case EE_SCREENS_OP_PDCS_SET_UPGRADES:
        EEScreensPdcsControl(4)->SetCivilizationUpgrades(sPdcsUpgrades);
        break;
    case EE_SCREENS_OP_PDCS_HIDE_LABEL:
        EEScreensPdcsControl(5)->SetDisplayMode(kDisplayModeHidden);
        break;
    case EE_SCREENS_OP_PDCS_RESET:
        EEScreensPdcsControl(3)->ResetSelection();
        break;
    case EE_SCREENS_GAME_PDCS_REQUEST: {
        NEMessage* theRequest = new EEMCivUpgradesRequested(gWorld.mCurrentPlayer, sPdcsUpgrades, sPdcsName, 0);
        if (gCommunications->GetIsMultiplayerSession())
            gCommunications->ProcessLocalUserInput(theRequest, kSlotServer);
        else {
            theRequest->Execute();
            theRequest->Dereference();
        }
        break;
    }
    default:
        return EEScreensCivBuilderCall(inForm, inEntity, inOp, inA, inB, inC, inD);
    }
    return 0;
}

// ---- the debug sound panel's (debug_sound.rs): the first selected unit's sound of a kind
static EESoundObject* sDebugSound;   // EE_SCREENS_GAME_DSU_SOUND's, for EE_SCREENS_GAME_DSU_VOLUME

static uint32_t EEScreensDebugSoundCall(UIForm* inForm, UIEntity* inEntity, uint32_t inOp, uint32_t inA, uint32_t inB,
                                        uint32_t inC, uint32_t inD)
{
    switch (inOp) {
    case EE_SCREENS_GAME_DSU_SOUND: {
        EEComplexUnit* theUnit = NULL;
        sDebugSound = NULL;
        if (!EEGetFirstSelectedUnit(theUnit) || theUnit->mComplexUnit == NULL)
            return 0;
        EEUnitClass* theClass = theUnit->mComplexUnit->mUnitClass;
        EEModel* theModel = theClass->mModel;
        switch (inA) {
        case 1: sDebugSound = theClass->mAcknowledgeSounds[0]; break;
        case 2: sDebugSound = theClass->mAcknowledgeSounds[1]; break;
        case 3: sDebugSound = theClass->mCreationSound; break;
        case 4: sDebugSound = theClass->mDeathSound; break;
        case 5: sDebugSound = theClass->mSelectionSounds[0]; break;
        case 6: sDebugSound = theClass->mSelectionSounds[1]; break;
        case 7: sDebugSound = theModel->mSounds1[0]; break;
        case 8: sDebugSound = theModel->mSounds2[0]; break;
        case 9: sDebugSound = theModel->mSounds1[13]; break;
        case 10: sDebugSound = theModel->mSounds2[13]; break;
        }
        return sDebugSound ? 2 : 1;
    }
    case EE_SCREENS_OP_DSU_SLIDER:
        return dynamic_cast<UISlider*>(inEntity) != NULL;
    case EE_SCREENS_GAME_DSU_VOLUME:
        sDebugSound->SetVolume(Float(inA));
        break;
    default:
        return EEScreensCivChoiceCall(inForm, inEntity, inOp, inA, inB, inC, inD);
    }
    return 0;
}

// the toolkit's calls (EE_SCREENS_OP_*) and the game's (EE_SCREENS_GAME_*) on the event's form and entity
extern "C" uint32_t ee_screenscpp_call(void* inForm, void* inEntity, uint32_t inOp, uint32_t inA, uint32_t inB,
                                       uint32_t inC, uint32_t inD)
{
    UIForm* theForm = static_cast<UIForm*>(inForm);
    // (a posted event runs with no form: the game's manager)
    EEUIManager* theManager = theForm ? static_cast<EEUIManager*>(theForm->GetManager()) : gUIManager;
    auto Ctl = [&]() { return theForm->GetControl(inA); };   // a control op's control
    switch (inOp) {
    case EE_SCREENS_OP_PUSH:
        theManager->Push(theManager->GetFormByID(inA));
        break;
    case EE_SCREENS_OP_POP:
        theManager->Pop();
        break;
    case EE_SCREENS_OP_SWITCH:
        gUserInterface->SwitchToForm(theManager->GetFormByID(inA));
        break;
    case EE_SCREENS_OP_OPEN_EVENT:
    case EE_SCREENS_OP_CLOSE_EVENT:
    case EE_SCREENS_OP_TRIGGER: {
        UIEvent* theEvent = EECreateScreenEvent(inOp == EE_SCREENS_OP_TRIGGER ? inD : inB);
        if (inOp == EE_SCREENS_OP_OPEN_EVENT)
            theManager->GetFormByID(inA)->SetFormOpenEvent(theEvent);
        else if (inOp == EE_SCREENS_OP_CLOSE_EVENT)
            theManager->GetFormByID(inA)->SetFormCloseEvent(theEvent);
        else
            theManager->GetFormByID(inA)->GetControl(inB)->SetTriggerEvent(inC, theEvent);
        if (theEvent)
            theEvent->Dereference();
        break;
    }
    case EE_SCREENS_OP_OPEN_FORM:
        return (uint32_t)gUserInterface->OpenForm(theManager->GetFormByID(inA));
    case EE_SCREENS_OP_ENTITY_INDEX:
        return static_cast<UIControl*>(inEntity)->GetIndex();
    case EE_SCREENS_OP_SHOW:
        if (UIControl* c = Ctl())
            c->SetDisplayMode(inB);
        break;
    // the settings form's (settings_form.rs): a missing control answers 0 and takes nothing
    case EE_SCREENS_OP_GROUP: {
        UIForm* theTarget = inC ? theManager->GetFormByID(inC - 1) : theForm;
        if (inB)
            theTarget->GetEntityGroup(inA)->Enable();
        else
            theTarget->GetEntityGroup(inA)->Disable();
        break;
    }
    case EE_SCREENS_OP_BACKGROUND:
        (inC ? theManager->GetFormByID(inC - 1) : theForm)->GetBackgroundByID(inA)->SetVisibility(inB != 0);
        break;
    case EE_SCREENS_OP_CHECKBOX:
        return Ctl() && static_cast<UICheckBox*>(Ctl())->GetIsChecked();
    case EE_SCREENS_OP_SET_CHECKBOX:
        if (UIControl* c = Ctl())
            static_cast<UICheckBox*>(c)->SetChecked(inB != 0);
        break;
    case EE_SCREENS_OP_RADIO:
        return Ctl() && static_cast<UIRadioButton*>(Ctl())->GetIsChecked();
    case EE_SCREENS_OP_CHECK_RADIO:
        if (UIControl* c = Ctl())
            static_cast<UIRadioButton*>(c)->Check();
        break;
    case EE_SCREENS_OP_RANGE:
        return Ctl() ? static_cast<UISlider*>(Ctl())->GetCurrentRangeValue() : 0;
    case EE_SCREENS_OP_SET_RANGE:
        if (UIControl* c = Ctl())
            static_cast<UISlider*>(c)->SetCurrentRangeValue((int32_t)inB);
        break;
    case EE_SCREENS_OP_ENTITY_RANGE:
        return static_cast<UISlider*>(inEntity)->GetCurrentRangeValue();
    case EE_SCREENS_OP_COMBO_SELECT:
        if (UIControl* c = Ctl())
            static_cast<UIComboBox*>(c)->SetCurrentSelection(inB);
        break;
    case EE_SCREENS_OP_LIST_SELECT:
        if (UIControl* c = Ctl())
            static_cast<UILBString*>(c)->ChangeSelectionIndex(inB, false, true);
        break;
    case EE_SCREENS_OP_COMBO_CLEAR:
        if (UIControl* c = Ctl())
            static_cast<UIComboBox*>(c)->DeleteAllEntries();
        break;
    case EE_SCREENS_OP_LIST_CLEAR:
        if (UIControl* c = Ctl())
            static_cast<UILBString*>(c)->DeleteAllEntries();
        break;
    case EE_SCREENS_OP_RELEASE_MOUSE:
        if (UIControl* c = Ctl())
            c->ReleaseCapturedMouse();
        break;
    case EE_SCREENS_OP_LABEL_NUMBER:
        if (UIControl* c = Ctl())
            static_cast<UITextLabel*>(c)->SetText((long)(int32_t)inB);
        break;
    case EE_SCREENS_OP_ENTITY_ID:
        return inEntity ? static_cast<UIControl*>(inEntity)->GetId() : 0xffffffffu;
    case EE_SCREENS_OP_CONTROL_ID:
        return Ctl() ? Ctl()->GetId() : 0;
    case EE_SCREENS_OP_TRIGGER_HERE: {
        UIEvent* theEvent = EECreateScreenEvent(inC);
        Ctl()->SetTriggerEvent(inB, theEvent);
        if (theEvent)
            theEvent->Dereference();
        break;
    }
    case EE_SCREENS_OP_SLIDER_LABEL: {
        UIESliderChange* theEvent = new UIESliderChange(static_cast<UITextLabel*>(theForm->GetControl(inB)), 1.0f, 0);
        Ctl()->SetTriggerEvent(kControlTriggerRangeChanged, theEvent);
        if (inC) {
            UWideString theString;
            gResource->GetString(inC, theString);
            theEvent->AddString(0.0f, theString);
        }
        theEvent->Dereference();
        break;
    }
    case EE_SCREENS_OP_CLOSE_FORM:
        return (uint32_t)gUserInterface->CloseForm(NULL);
    case EE_SCREENS_OP_DISPLAY_CHANGED:
        gUserInterface->PostDisplayChanged(false);
        break;
    case EE_SCREENS_OP_RESIZE_MENU:
        gUserInterface->ResizeGameScreen(EEMenuWidth(), EEMenuHeight(), true, false);
        break;
    case EE_SCREENS_OP_SRAND_TIME:
        srand(TMGetTime());
        break;
    case EE_SCREENS_OP_RAND:
        return rand();
    case EE_SCREENS_GAME_CONDUCTOR_VOLUME:
        return gConductor->GetConductor().SetVolume(Float(inA));
    case EE_SCREENS_GAME_GET_CONDUCTOR_VOLUME:
        return Bits(gConductor->GetConductor().GetVolume());
    case EE_SCREENS_GAME_REVERB:
        gConductor->GetConductor().SetReverb(inA);
        break;
    case EE_SCREENS_GAME_CONDUCTOR_STATE:
        gConductor->GetConductor().SetConductorState(inA);
        break;
    case EE_SCREENS_GAME_GET_CONDUCTOR_STATE:
        return gConductor->GetConductor().GetConductorState();
    case EE_SCREENS_GAME_UI_MUSIC_VOLUME:
        gUIManager->SetMusicVolume(Float(inA));
        break;
    case EE_SCREENS_GAME_MASTER_VOLUME:
        return gSoundManager->SetMasterVolume(Float(inA));
    case EE_SCREENS_GAME_GET_MASTER_VOLUME:
        return Bits(gSoundManager->GetMasterVolume());
    case EE_SCREENS_GAME_GET_CHANNELS:
        return gSoundManager->GetMaxChannels();
    case EE_SCREENS_GAME_CHANNELS:
        gSoundManager->SetMaxChannels(inA);
        break;
    case EE_SCREENS_GAME_MUSIC_START:
        gMusicManager->Start();
        break;
    case EE_SCREENS_GAME_MUSIC_STOP:
        gMusicManager->Stop();
        break;
    case EE_SCREENS_GAME_MOUSE_SENSITIVITY:
        gMouse->SetSensitivity(Float(inA));
        break;
    case EE_SCREENS_GAME_LEFT_HANDED:
        UIManager::SetMouseLeftHanded(inA != 0);
        break;
    case EE_SCREENS_GAME_IS_SERVER:
        return gCommunications->IsServer() == true;
    case EE_SCREENS_GAME_PAUSE:
        gWorld.PauseGame();
        break;
    case EE_SCREENS_GAME_CURRENT_RASTERIZER:
        for (unsigned long i = 0; i < GERasterizer::GetRasterizerCount(); i++)
            if (GERasterizer::GetRasterizer(i) == gRasterizer)
                return i;
        return 0;
    case EE_SCREENS_GAME_CHANGE_RASTERIZER:
        gUserInterface->PostChangeRasterizer(inA, inB);
        break;
    case EE_SCREENS_GAME_APPLY_PORT:
        EEApplyPortSettings(gGamePreferences.GetSettings());
        break;
    case EE_SCREENS_GAME_VIEW: {
        // the game view rectangle the settings reset
        GERectangle theRectangle;
        theRectangle.SetBottom(1085.0f);
        theRectangle.SetLeft(0.0f);
        theRectangle.SetRight(1600.0f);
        theRectangle.SetTop(35.0f);
        EESetGameViewRectangle(theRectangle);
        break;
    }
    case EE_SCREENS_GAME_MIP_FILTER:
        gTextureManager->UpdateMIPFilterMode(inA);
        break;
    case EE_SCREENS_GAME_TEXTURE_RESOLUTION:
        EESetTextureResolution((int32_t)inA);
        break;
    case EE_SCREENS_GAME_AMBIENT:
        gAmbientSoundManager.SetEnabled(inA != 0);
        break;
    case EE_SCREENS_GAME_SAVE:
        return gGamePreferences.Save();
    case EE_SCREENS_GAME_NOT_KOREAN:
        return gGamePreferences.IsNotKorean();
    case EE_SCREENS_GAME_TEST_SOUND:
        if (EESoundObject* theSound = EESoundObject::Get(inA)) {
            theSound->LoadMedia();
            gSoundManager->Play(theSound->GetSampleID(), 1.0f, 0.0f, 1.0f, false, NULL);
        }
        break;
    case EE_SCREENS_GAME_WINDOW_MODE: {
        // the settings' window mode on the game window (the caller resizes the game)
        PlatWindow* theWindow = gGameWindow ? gGameWindow->GetOSRepresentation() : NULL;
        if (!theWindow)
            break;
        if (gGamePreferences.GetSettings().mWindowMode == 0) {
            PlatWindowSetFixedSize(theWindow, 0);
            PlatWindowSetFullscreen(theWindow, 1);
        } else {
            int32_t theWidth = 0, theHeight = 0;
            PlatWindowSetFullscreen(theWindow, 0);
            PlatWindowSetFixedSize(theWindow, 0);
            if (PlatWindowGetDisplayLogicalSize(NULL, &theWidth, &theHeight) && theWidth > 0)
                PlatWindowSetSize(theWindow, theWidth * 4 / 5, theHeight * 4 / 5);
            PlatWindowSetFixedSize(theWindow, 1);
        }
        break;
    }
    // the custom campaign form's (custom_campaign.rs): a list box of form inA
    case EE_SCREENS_OP_FORM_LIST_CLEAR:
        static_cast<UIListBox<UWideString>*>(theManager->GetFormByID(inA)->GetControl(inB))->DeleteAllEntries();
        break;
    case EE_SCREENS_OP_FORM_LIST_COUNT:
        return static_cast<UIListBox<UWideString>*>(theManager->GetFormByID(inA)->GetControl(inB))->GetLineCount();
    case EE_SCREENS_OP_FORM_LIST_SELECT:
        static_cast<UIListBox<UWideString>*>(theManager->GetFormByID(inA)->GetControl(inB))->ChangeSelectionIndex(inC, true, false);
        break;
    case EE_SCREENS_OP_FORM_LIST_SELECTION: {
        unsigned long theSelection = 0;
        static_cast<UIListBox<UWideString>*>(theManager->GetFormByID(inA)->GetControl(inB))->GetCurrentSelection(theSelection);
        return theSelection;
    }
    case EE_SCREENS_GAME_RASTERIZER_COUNT:
        return (uint32_t)gDisplayRasterizers.size();
    case EE_SCREENS_GAME_SCREEN_COUNT:
        return inA < gDisplayRasterizers.size() ? (uint32_t)gDisplayRasterizers[inA].mScreens.size() : 0;
    case EE_SCREENS_OP_HOT_KEY:
        theForm->AddHotKey(inA, inB, inC, kKeyNone, NULL, NULL, EECreateScreenEvent(inD), NULL, 0, false);
        break;
    case EE_SCREENS_GAME_QUIT:
        gUserInterface->PostQuit();
        break;
    case EE_SCREENS_GAME_EXTERNAL_LOBBY:
        EEStartExternalLobby();
        break;
    case EE_SCREENS_GAME_EDITOR_MODE_OFF:
        theManager->mInScenarioEditor = false;
        break;
    case EE_SCREENS_GAME_SINGLE:
    case EE_SCREENS_GAME_MULTI:
        gGameState.mGameOptions.mIsMultiplayer = inOp == EE_SCREENS_GAME_MULTI;
        break;
    case EE_SCREENS_GAME_DELETE_EDITOR:
        theManager->DeleteScenarioEditor();
        break;
    case EE_SCREENS_GAME_DELETE_WON:
        if (gWonNet != NULL) {
            EEWonNet* theWonNet = gWonNet;
            gWonNet = NULL;
            delete theWonNet;
        }
        break;
    case EE_SCREENS_GAME_CREATE_EDITOR:
        theManager->CreateScenarioEditor();
        break;
    case EE_SCREENS_GAME_START_EDITOR:
        theManager->StartScenarioEditor();
        break;
    case EE_SCREENS_GAME_CIV_BUILDER:
        sCivilization.mPurchasedUpgrades.clear();
        sCivilization.mAvailablePoints = sCivilization.mMaxPoints;
        sCivilization.SetPointsAndMax(gWorldTable.GetLongValue(kWorldCivilizationBuilderPoints));
        gUserInterface->OpenCivilizationBuilder(&sCivilization, false);
        break;
    case EE_SCREENS_GAME_FORGET_MAIN_FORM:
        gSavedMainGameForm = NULL;
        break;
    case EE_SCREENS_GAME_LOBBY_BEGIN:
        gWorld.GetLobbyOptions().mLock->WriteLock();
        gGameOptions = gGamePreferences.GetLastGameOptions();
        break;
    case EE_SCREENS_GAME_LOBBY_END:
        gWorld.GetLobbyOptions().mLock->WriteUnlock();
        break;
    case EE_SCREENS_GAME_SHOW_SINGLE_PLAYER:
        EEShowSinglePlayerScreen();
        break;
    case EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN:
        EECampaignFormManager::Get()->mAllScenariosOpen = inA != 0;
        break;
    // the load and save screens' (save_load.rs)
    case EE_SCREENS_OP_SWAP_FORM:
        theManager->SwapForm(theManager->GetFormByID(inA));
        break;
    case EE_SCREENS_OP_CURRENT_FORM_IS:
        return theManager->GetCurrentForm() == theManager->GetFormByID(inA);
    case EE_SCREENS_OP_POST_EVENT: {
        UIEvent* theEvent = EECreateScreenEvent(inA);
        gUserInterface->PostEvent(theEvent, NULL);
        theEvent->Dereference();
        break;
    }
    case EE_SCREENS_OP_FORM_LIST_SELECTED: {
        UIForm* theTarget = inA ? theManager->GetFormByID(inA - 1) : theForm;
        UILBString* theList = static_cast<UILBString*>(theTarget->GetControl(inB));
        unsigned long theSelection = 0;
        if (theList && theList->GetCurrentSelection(theSelection) == kErrorOK)
            return (uint32_t)theSelection;
        return 0xffffffffu;
    }
    case EE_SCREENS_GAME_FREE_CAMERA:
        return gFreeCamera;
    case EE_SCREENS_GAME_MULTIPLAYER_SESSION:
        return gCommunications->GetIsMultiplayerSession();
    case EE_SCREENS_GAME_SCREEN_ACTIVE:
        return gGameScreenActive;
    case EE_SCREENS_GAME_IN_EDITOR:
        return theManager->mInScenarioEditor;
    case EE_SCREENS_GAME_PAUSE_SERVER: {
        // (the save screen's original did not check the cast: a client crashed there)
        EEServer* theServer = dynamic_cast<EEServer*>(gCommunications);
        if (theServer && theServer->GetIsGamePaused() == false) {
            theServer->PauseGame();
            gUserInterface->PostPauseInformation(true);
        }
        break;
    }
    case EE_SCREENS_GAME_CTS_PROCESSED:
        return gCommunications->GetNumberOfCTsProcessedInPhysics();
    case EE_SCREENS_GAME_SAVE_SEND:
        EEScreensSendSave(sSaveObject);
        break;
    case EE_SCREENS_GAME_SAVE_POST:
        EEScreensPostSave(sSaveObject);
        break;
    case EE_SCREENS_GAME_SAVED_MAIN_FORM:
        return gSavedMainGameForm != NULL;
    case EE_SCREENS_GAME_OPEN_SAVED_MAIN_FORM:
        gUserInterface->OpenForm(gSavedMainGameForm);
        break;
    // the campaign scenario select and scenario intro forms' (scenario_screens.rs)
    case EE_SCREENS_OP_REMOVE_BACKGROUND:
        if (UIBackground* theBackground = theForm->GetBackgroundByID((long)(int32_t)inA))
            theForm->RemoveBackground(theBackground);
        break;
    case EE_SCREENS_OP_TAB_STOP:
        theForm->SetTabStop(inA);
        break;
    case EE_SCREENS_OP_LABEL_CLEAR:
        static_cast<UITextLabel*>(Ctl())->DeleteArray();
        break;
    case EE_SCREENS_OP_LABEL_LINE_COUNT:
        return static_cast<UITextLabel*>(Ctl())->GetLineCount();
    case EE_SCREENS_OP_LABEL_START_LINE:
        static_cast<UITextLabel*>(Ctl())->SetStartingDisplayLine(inB);
        break;
    case EE_SCREENS_OP_ADD_TRIGGER: {
        UIEvent* theEvent = EECreateScreenEvent(inA);
        theManager->AddEventToTrigger(theEvent, theForm, NULL);
        theEvent->Dereference();
        break;
    }
    case EE_SCREENS_OP_SLEEP:
        PlatSleep(inA);
        break;
    case EE_SCREENS_OP_OPEN_MAIN_GAME_FORM:
        gUserInterface->OpenForm(gUIManager->mMainGameForm);
        break;
    case EE_SCREENS_GAME_SI_FLAGS: {
        EEUISCIFormManager* theIntro = EEUISCIFormManager::Get();
        return (theIntro->mIsInGame ? EE_SCREENS_SI_FLAG_IN_GAME : 0) |
               (theIntro->mIsLoadingFromGame ? EE_SCREENS_SI_FLAG_LOADING : 0) |
               (theIntro->mIsCampaign ? EE_SCREENS_SI_FLAG_CAMPAIGN : 0) |
               (theIntro->mScrollBriefingToEnd ? EE_SCREENS_SI_FLAG_SCROLL : 0);
    }
    case EE_SCREENS_GAME_SI_SET_FLAG: {
        EEUISCIFormManager* theIntro = EEUISCIFormManager::Get();
        bool* const theFlags[4] = {&theIntro->mIsInGame, &theIntro->mIsLoadingFromGame, &theIntro->mIsCampaign,
                                   &theIntro->mScrollBriefingToEnd};
        for (unsigned long i = 0; i < 4; i++)
            if (inA == (1u << i))
                *theFlags[i] = inB != 0;
        break;
    }
    case EE_SCREENS_GAME_SI_BLINK_OFF:
        EEUISCIFormManager::Get()->SetBlinking(false);
        break;
    case EE_SCREENS_GAME_SOUND_STOP:
        gSoundManager->Stop(inA, false);
        break;
    case EE_SCREENS_GAME_SOUND_LATENCY:
        gSoundManager->SetLatency(inA);
        break;
    case EE_SCREENS_GAME_SI_OPEN_MAP:
        return EEUISCIFormManager::Get()->OpenMap();
    case EE_SCREENS_GAME_SI_OPEN_MOVIE:
        return EEUISCIFormManager::Get()->OpenMovie();
    case EE_SCREENS_GAME_SI_OPEN_SOUND:
        return EEUISCIFormManager::Get()->OpenSound();
    case EE_SCREENS_GAME_SI_PLAY_MOVIE: {
        FSFileSpec theMovie;
        UIEvent* theEvent = EECreateScreenEvent(inA);
        EEUISCIFormManager::Get()->GetMovie(theMovie);
        gUserInterface->PostPlayMovie(theMovie, theEvent);
        theEvent->Dereference();
        break;
    }
    case EE_SCREENS_GAME_SI_PLAY_SOUND: {
        // the briefing sound once (below Sounds\ unless its file names the folder)
        FSFileSpec theSound;
        UString theFile;
        UString theLower;
        UString theDirectory;
        unsigned long theSample;
        unsigned long theID = inA;
        EEUISCIFormManager::Get()->GetSound(theSound);
        theFile = theSound.GetFile();
        theLower = theFile;
        theLower.ToLower();
        theDirectory = UWideString("Sounds\\");
        theDirectory.ToLower();
        if (theLower.Find(theDirectory, 0) == theLower.GetLength())
            theFile = UString("Sounds\\") + theFile;
        gSoundManager->SetLatency(500);
        EELoadSound(theFile, 0x31, theSample, theID, 0);
        gSoundManager->RegisterOneShotSound(theSample, theID);
        return (uint32_t)theID;
    }
    case EE_SCREENS_GAME_SI_MAP_BACKDROP: {
        // the scenario's map picture as the form's backdrop (-2), unless it already is
        FSFileSpec theMap;
        UString theFile;
        UString theLower;
        UString theDirectory;
        GERectangle theRectangle;
        GETextureDescription theDescription;
        theDescription.mPixelDepth = 16;
        theDescription.mNumMipLevelsToDrop = 0;
        theDescription.mSurfaceVolatility = 1;
        theDescription.mFrameFormat = 0;
        theDescription.mIs3DRenderable = false;
        theDescription.mIsTransparencyEnabled = false;
        theDescription.mHasAlpha = true;
        theDescription.mCreateMIPMaps = true;
        UILayoutUtility theLayout;
        EEUISCIFormManager* theIntro = EEUISCIFormManager::Get();
        UIBackground* theBackground = theForm->GetBackgroundByID(-2);
        theIntro->GetMap(theMap);
        theFile = theMap.GetFile();
        theLower = theFile;
        theLower.ToLower();
        theDirectory = UWideString("Scenarios\\");
        theDirectory.ToLower();
        if (theLower.Find(theDirectory, 0) == theLower.GetLength())
            theFile = UString("Scenarios\\") + theFile;
        if (theBackground == NULL || strcmp(static_cast<UITextureBackground*>(theBackground)->GetTextureFileName(), theFile) != 0) {
            if (theBackground)
                theForm->RemoveBackground(theBackground);
            if (theIntro->OpenMap()) {
                theForm->LockDrawBackground();
                UIBackground* theBackdrop = new EEUIBMapBackground(-2, theFile, theDescription, theRectangle, theLayout,
                                                                   theForm, 2, 0, 0, true);
                theBackdrop->Open();
                theForm->UnlockDrawBackground();
            }
        }
        break;
    }
    case EE_SCREENS_GAME_SI_GET_SCENARIO:
        return EEUISCIFormManager::Get()->GetScenario(EEScreensScenario());
    case EE_SCREENS_GAME_SI_POST_LOAD:
        gUserInterface->PostLoadSavedGame(EEScreensScenario());
        break;
    case EE_SCREENS_GAME_SI_START_GAME: {
        // a single player game of the scenario (inA: the campaign's), a new local server, start loading
        gGameOptions.mLock->WriteLock();
        gGameOptions.SetDefaults();
        if (inA) {
            EECampaignFormManager* theCampaign = EECampaignFormManager::Get();
            gGameOptions.mLoadType = 4;
            gGameOptions.mDifficulty = theCampaign->mDifficultyLevel;
            gGameOptions.mCampaignName = theCampaign->mCampaignName;
            gGameOptions.mCampaignChecksum = theCampaign->mChecksum;
            gGameOptions.mScenarioName = theCampaign->mScenarioName;
            gGameOptions.mMapName = theCampaign->mFirstScenario;
        } else {
            gGameOptions.mLoadType = 1;
            gLoadScenarioName = EEScreensScenario().GetFile().GetIndexedSubstring(0, EEScreensScenario().GetFile().GetLength() - 5);
        }
        gGameOptions.SetMapType((const char*)EEScreensScenario().GetFile());
        gGameOptions.mIsMultiplayer = false;
        gGameOptions.mLock->WriteUnlock();
        if (gCommunications)
            delete gCommunications;
        gCommunications = new EEServer(UWideString("EEName"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, true, true);
        static_cast<EEServer*>(gCommunications)->StartThreads();
        static_cast<EEServer*>(gCommunications)->mNumLoadResponses = false;
        static_cast<EEServer*>(gCommunications)->StopPing();
        static_cast<EEServer*>(gCommunications)->mLoadState = true;
        NEMessage* theMessage = new EEMStartLoading(1, TMGetTime());
        gLocalPlayerOut = false;
        gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
        break;
    }
    case EE_SCREENS_GAME_CAMPAIGN_UPDATE_CHECKSUM:
        EECampaignFormManager::Get()->UpdateChecksum();
        break;
    case EE_SCREENS_GAME_CAMPAIGN_CHECKSUM:
        return EECampaignFormManager::Get()->mChecksum;
    case EE_SCREENS_GAME_CS_MAP_BACKDROP: {
        // the campaign's background image (a file of Scenarios) as the form's backdrop (-2)
        GERectangle theRectangle;
        GETextureDescription theDescription;
        theDescription.mPixelDepth = 16;
        theDescription.mNumMipLevelsToDrop = 0;
        theDescription.mSurfaceVolatility = 1;
        theDescription.mFrameFormat = 0;
        theDescription.mIs3DRenderable = false;
        theDescription.mIsTransparencyEnabled = false;
        theDescription.mHasAlpha = true;
        theDescription.mCreateMIPMaps = true;
        UILayoutUtility theLayout;
        FSFileSpec* theMapSpec = NULL;
        UWideString theMapName = EECampaignFormManager::Get()->mCampaign.GetBackgroundImage();
        UString theMapFile = UString("Scenarios\\" + gU52A280 + theMapName);
        if (gDataPath->FindFile(theMapFile, theMapSpec) == kErrorOK) {
            theForm->LockDrawBackground();
            UIBackground* theMap = new EEUIBMapBackground(-2, theMapFile, theDescription, theRectangle, theLayout, theForm,
                                                          2, 0, 0, true);
            theMap->Open();
            theForm->UnlockDrawBackground();
            if (theMapSpec)
                delete theMapSpec;
        }
        break;
    }
    case EE_SCREENS_GAME_DIFFICULTY:
        return (uint32_t)gDifficultyLevel;
    case EE_SCREENS_GAME_CS_SET_DIFFICULTY:
        EECampaignFormManager::Get()->mDifficultyLevel = (long)inA;
        gDifficultyLevel = (long)inA;
        gDifficultyLevel2 = (long)inA;
        gGameOptions.mLock->WriteLock();
        gGameOptions.mDifficulty = inA;
        gGameOptions.mLock->WriteUnlock();
        break;
    case EE_SCREENS_GAME_CAMPAIGN_ALL_OPEN_GET:
        return EECampaignFormManager::Get()->mAllScenariosOpen;
    // the lobby's and its load screens' (lobby_form.rs)
    case EE_SCREENS_GAME_LE_LOCK:
        switch (inA) {
        case EE_SCREENS_LE_READ_LOCK: gGameOptions.mLock->ReadLock(); break;
        case EE_SCREENS_LE_READ_UNLOCK: gGameOptions.mLock->ReadUnlock(); break;
        case EE_SCREENS_LE_WRITE_LOCK: gGameOptions.mLock->WriteLock(); break;
        default: gGameOptions.mLock->WriteUnlock(); break;
        }
        break;
    case EE_SCREENS_GAME_LE_IS_CLIENT:
        return gCommunications->IsClient() == true;
    case EE_SCREENS_GAME_LE_PLAYER_INFO:
        gCommunications->ProcessGuaranteedLocalUserInput(
            new EEMPlayerInformation((long)inA, (long)inB, (long)(int32_t)inC, 0.0f), kSlotNone);
        break;
    case EE_SCREENS_GAME_LE_SEND_OPTION:
        gCommunications->ProcessGuaranteedLocalUserInput(
            new EEMGameOptions((long)(int32_t)inA, (long)(int32_t)inB, UWideString("")), kSlotNone);
        break;
    case EE_SCREENS_GAME_LE_SET_OPTION:
        EEScreensLobbySetOption(inA, (long)(int32_t)inB);
        break;
    case EE_SCREENS_GAME_LE_DIFFICULTY_LEVELS:
        gDifficultyLevel = (long)(int32_t)inA;
        gDifficultyLevel2 = (long)(int32_t)inA;
        break;
    case EE_SCREENS_OP_LE_COMBO_DELETE_LINE:
        static_cast<UIComboBox*>(Ctl())->DeleteLine(inB);
        break;
    case EE_SCREENS_OP_LE_COMBO_LINES:
        return (uint32_t)static_cast<UIComboBox*>(Ctl())->GetListBox()->GetLineCount();
    case EE_SCREENS_OP_LE_TYPE_SELECT: {
        // LobbySelectPlayerType: the line without firing the box's change event
        UIComboBox* theBox = static_cast<UIComboBox*>(Ctl());
        UIEvent* theEvent = theBox->GetTriggerEvent(kControlTriggerSelectionChanged);
        if (theEvent) {
            theEvent->Reference();
            theBox->SetTriggerEvent(kControlTriggerSelectionChanged, NULL);
        }
        if (inB)
            theBox->SetCurrentSelection(inC);
        if (theEvent) {
            theBox->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
            theEvent->Dereference();
        }
        break;
    }
    case EE_SCREENS_OP_LE_HAS_TRIGGER:
        return Ctl()->GetTriggerEvent(inB) != NULL;
    case EE_SCREENS_OP_LE_PLAYER_COLOUR:
        Ctl()->SetBgColor(EEGetPlayerColor(inB));
        break;
    case EE_SCREENS_OP_LE_OPACITY:
        Ctl()->SetBgOpacity(1.0f);
        break;
    case EE_SCREENS_OP_LE_BITMAP:
        static_cast<UIBitmap*>(Ctl())->SetBitmap(inB ? gUIManager->GetCheckDownOn() : gUIManager->GetCheckDownOff());
        break;
    case EE_SCREENS_OP_LE_NAME_COLOUR: {
        // a slot's name colour: loaded game, observer, normal, the local player's (the type box's label too)
        static const GEColor kColours[4] = {GEColor(1.0f, 0.0f, 0.0f), GEColor(1.0f, 1.0f, 0.0f),
                                            GEColor(1.0f, 1.0f, 1.0f), GEColor(0.0f, 1.0f, 0.0f)};
        if (inC)
            static_cast<UIComboBox*>(Ctl())->GetTextLabel()->GetGEText()->SetFgColor(kColours[inB & 3]);
        else
            static_cast<UITextLabel*>(Ctl())->GetGEText()->SetFgColor(kColours[inB & 3]);
        break;
    }
    case EE_SCREENS_OP_LE_DIRTY:
        Ctl()->SetDirty(true);
        break;
    case EE_SCREENS_OP_LE_PING:
        static_cast<EEUICPing*>(Ctl())->SetPing(Float(inB));
        break;
    case EE_SCREENS_OP_LE_FORM_VISIBLE:
        return theManager->IsFormVisible(theManager->GetFormByID(inA));
    case EE_SCREENS_OP_LE_KEYBOARD_EVENT: {
        UIEvent* theEvent = EECreateScreenEvent(inA);
        theForm->SetFormKeyboardCapturedEvent(theEvent);
        if (theEvent)
            theEvent->Dereference();
        break;
    }
    case EE_SCREENS_OP_LE_KEYBOARD_CAPTURED:
        return theForm->GetIsKeyboardCaptured();
    case EE_SCREENS_OP_LE_BLINKING:
        static_cast<UIBlinkingButton*>(Ctl())->SetIsBlinking(true);
        break;
    case EE_SCREENS_OP_LE_EDIT_DELETE:
        static_cast<UIEditBox*>(Ctl())->DeleteText();
        break;
    case EE_SCREENS_GAME_LE_DISCONNECT:
        gCommunications->ProcessGuaranteedLocalUserInput(new NEMClientDisconnectionNotification(false), kSlotNone);
        break;
    case EE_SCREENS_GAME_LE_REMOVE_HOST: {
        EEAction* theAction = new EEARemovePlayer(0xfe);
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
        break;
    }
    case EE_SCREENS_GAME_LE_WON_REJOIN: {
        // the lobby's Cancel: back to WON's room (the form closes unless that failed)
        bool theClose = true;
        if (gWonNet) {
            gWonNet->ClearGameList();
            theClose = gWonNet->RejoinRoom(true);
        }
        return theClose;
    }
    case EE_SCREENS_GAME_LE_WON_LEAVE:
        if (gWonNet)
            gWonNet->LeaveRoom();
        break;
    case EE_SCREENS_GAME_LE_START_SERVER: {
        // Start: the server closed to new players (WON's game told the humans and players), its transfers reset
        EEServer* theServer = static_cast<EEServer*>(gCommunications);
        EEWonNetGameInfo theInfo;
        sLobbyStartTime = TMGetTime();
        theServer->AllowNewPlayers(false);
        theServer->SetGameState(2);
        theServer->mNumLoadResponses = false;
        if (gWonNet) {
            gWonNet->GetGameInfo(theInfo);
            theInfo.mFlags &= ~1;
            theInfo.mPlayers = (unsigned char)inB;
            theInfo.mHumans = (unsigned char)inA;
            gWonNet->SetGameInfo(theInfo);
        }
        theServer->StopPing();
        theServer->ResetXfer();
        theServer->mLoadState = true;
        theServer->AllowNewPlayers(false);
        break;
    }
    case EE_SCREENS_GAME_LE_START_LOADING:
        gCommunications->ProcessGuaranteedLocalUserInput(new EEMStartLoading((unsigned char)inA, (long)(int32_t)inB),
                                                         kSlotNone);
        break;
    case EE_SCREENS_GAME_LE_SAVE_LAST_OPTIONS:
        gGamePreferences.SetLastGameOptions(gGameOptions);
        break;
    case EE_SCREENS_GAME_LE_START_PING:
        static_cast<EEServer*>(gCommunications)->StartPing();
        break;
    case EE_SCREENS_GAME_LE_FULL_PLAYER:
        if (inA < 16)
            gCommunications->ProcessGuaranteedLocalUserInput(new EEMFullPlayerInformation(gGameOptions.mPlayers[inA]),
                                                             kSlotNone);
        break;
    case EE_SCREENS_GAME_LE_RULE:
        return EEScreensLobbyRule(inA, inB);
    case EE_SCREENS_GAME_LE_FIND_GENERATOR: {
        unsigned long theLine = 0;
        bool theFound = EERMFindGenerator(EEScreensLobbyOptions(inA)->GetMapType(), theLine) == 0;
        return (uint32_t)(theLine & 0x7fffffff) | (theFound ? 0 : 0x80000000u);
    }
    case EE_SCREENS_GAME_LE_BOOT_FREE:
        delete sLobbyBoot;
        sLobbyBoot = NULL;
        break;
    case EE_SCREENS_GAME_LE_BOOT_REFUSE: {
        // the boot answered yes: WON's counts, the slot's client refused (the copy, reset)
        EEServer* theServer = dynamic_cast<EEServer*>(gCommunications);
        EEWonNetGameInfo theInfo;
        if (gWonNet)
            gWonNet->GetGameInfo(theInfo);
        if (inA != 0)
            theInfo.mHumans--;
        if (inA == 4)
            theInfo.mPlayers--;
        if (theServer && sLobbyBoot)
            theServer->RefuseClient(*sLobbyBoot);
        if (gWonNet)
            gWonNet->SetGameInfo(theInfo);
        break;
    }
    case EE_SCREENS_GAME_LE_XFER: {
        // a joined human's transfer of the loaded file: the old one aborted, the new one's id
        unsigned long theID = inB;
        if (!sLobbyHeader || !sLobbyFile)
            return 0;
        static_cast<NESynchronousServer*>(gCommunications)->AbortXfer(theID);
        theID = 0;
        static_cast<NESynchronousServer*>(gCommunications)->XferFileToClient(*sLobbyFile, sLobbyHeader->mChecksum,
            static_cast<EEServer*>(gCommunications)->GetSendToSlot((unsigned char)inA), theID, false);
        return (uint32_t)theID;
    }
    case EE_SCREENS_GAME_LE_HEADER_PLAYERS:
        return sLobbyHeader ? (uint32_t)sLobbyHeader->mNumPlayers : 0;
    case EE_SCREENS_GAME_LE_APPLY_SAVED: {
        // LobbyApplySavedGameOptions: the loaded header's options (a copy) into the lobby options, sent
        if (!sLobbyHeader)
            break;
        EEGameOptions theOptions(sLobbyHeader->mGameOptions);
        gGameOptions.mMapSize = theOptions.mMapSize;
        gGameOptions.mResources = theOptions.mResources;
        gGameOptions.mEpochStart = theOptions.mEpochStart;
        gGameOptions.mEpochEnd = theOptions.mEpochEnd;
        gGameOptions.mUnitLimit = theOptions.mUnitLimit;
        gGameOptions.mWondersForVictory = theOptions.mWondersForVictory;
        gGameOptions.mVariant = theOptions.mVariant;
        gGameOptions.mDifficulty = theOptions.mDifficulty;
        gGameOptions.mGameSpeed = theOptions.mGameSpeed;
        gGameOptions.mRecordGames = theOptions.mRecordGames;
        gGameOptions.mAllowCustomCivs = theOptions.mAllowCustomCivs;
        gGameOptions.mRevealMap = theOptions.mRevealMap;
        gGameOptions.mLockTeams = theOptions.mLockTeams;
        gGameOptions.mLockSpeed = theOptions.mLockSpeed;
        gGameOptions.mCheatCodes = theOptions.mCheatCodes;
        gGameOptions.mVictory = theOptions.mVictory;
        gCommunications->ProcessGuaranteedLocalUserInput(new EEMFullGameOptions(gGameOptions), kSlotNone);
        break;
    }
    case EE_SCREENS_GAME_LE_RANDOM_MAP_OPTIONS:
        // the game type's random map: the options copied (the assignment checks each value) with load type 0, and back
        if (!sLobbyCopy)
            sLobbyCopy = new EEGameOptions;
        *sLobbyCopy = gGameOptions;
        sLobbyCopy->mLoadType = 0;
        gGameOptions = *sLobbyCopy;
        break;
    // the in-game options menu's (main_game_options.rs)
    case EE_SCREENS_GAME_GO_RESIGN:
        gCommunications->ProcessLocalUserInput(new EEMResign, kSlotNone);
        break;
    case EE_SCREENS_GAME_GO_RESTART:
        gUserInterface->PostRestartGame();
        break;
    case EE_SCREENS_GAME_GO_TO_EDITOR:
        // back to the scenario editor from its single player test game: the simulation stopped, the editor posted
        gPhysics->mThread->Post(0x402, NULL, NULL, true);
        gUserInterface->PostEnterScenarioEditor(gUIManager->mInScenarioEditor);
        gUserInterface->PostPauseInformation(true);
        break;
    case EE_SCREENS_GAME_GO_USER_PAUSE: {
        // (1: our reference dropped, as the main game form's pause did; the options menu's original kept it)
        EEAction* theAction = new EEAUserPauseGame;
        gUserInterface->PostAction(theAction);
        if (inA)
            theAction->Dereference();
        break;
    }
    case EE_SCREENS_GAME_GO_REQUEST_PAUSE:
        gCommunications->ProcessLocalUserInput(new NEMClientRequestPause, kSlotNone);
        break;
    case EE_SCREENS_GAME_GO_FORM_EXISTS:
        return gUIManager->GetFormByID(inA) != NULL;
    case EE_SCREENS_GAME_GO_TO_SCENARIO_LOAD: {
        // Play Scenario: the scenario load form swapped in for the menu (keeping its music), the game paused, the main
        // game form remembered
        UWideString theMusic;
        EEUISCIFormManager* theIntro = EEUISCIFormManager::Get();
        theMusic = gUIManager->GetFormByID(kFormScenarioLoad)->GetMusicFileName();
        gUIManager->GetFormByID(kFormScenarioLoad)->SetMusicFileName("");
        gUIManager->Pop();
        gUIManager->SwapForm(gUIManager->GetFormByID(kFormScenarioLoad));
        gUIManager->GetFormByID(kFormScenarioLoad)->SetMusicFileName(theMusic);
        dynamic_cast<EEServer*>(gCommunications)->PauseGame();
        gUserInterface->PostPauseInformation(true);
        theIntro->mIsInGame = false;
        theIntro->mIsLoadingFromGame = true;
        gSavedMainGameForm = gUIManager->mMainGameForm;
        break;
    }
    case EE_SCREENS_GAME_GO_BRIEFING:
        EEDisplayScenarioIntro();
        break;
    case EE_SCREENS_GAME_GO_CANNOT_EXIT:
        return gCommunications->IsServer() ? static_cast<NESynchronousServer*>(gCommunications)->IsMigrating()
                                           : static_cast<NESynchronousClient*>(gCommunications)->IsPlaybackRecordingSession();
    case EE_SCREENS_GAME_GO_EXIT:
        EEExitGame();
        break;
    case EE_SCREENS_GAME_GO_OPTIONS_BLINK_OFF:
        static_cast<UIBlinkingButton*>(gUIManager->mMainGameForm->GetControl(kMainGameIngameOptions))->SetIsBlinking(false);
        break;
    case EE_SCREENS_GAME_GO_USER_PAUSED:
        return gWorld.mUserPaused == true;
    case EE_SCREENS_OP_GO_REMOVE_HOT_KEYS:
        theForm->RemoveAllHotKeys();
        break;
    case EE_SCREENS_OP_GO_HOT_KEY:
        theForm->AddHotKey(inA, kKeyNone, kKeyNone, kKeyNone, EECreateScreenEvent(inB), NULL, NULL, NULL, 0, false);
        break;
    case EE_SCREENS_OP_GO_STRING_ID:
        Ctl()->SetStringTableID(inB);
        break;
    case EE_SCREENS_GAME_GO_HIDE_RESIGN:
        return gWorld.GetCurrentPlayer()->GetObserverMode() == true || gWorld.GetGameOptions().GetIsScenarioGame() == true;
    default:
        return EEScreensDebugSoundCall(theForm, static_cast<UIEntity*>(inEntity), inOp, inA, inB, inC, inD);
    }
    return 0;
}

// the manager's form of an id (scenario_screens.rs: a call on a named form)
extern "C" void* ee_screenscpp_form(uint32_t inID)
{
    return gUIManager->GetFormByID(inID);
}

// a text of the game (EE_SCREENS_TEXT_*) into outText (up to inCapacity bytes); its length in bytes
extern "C" size_t ee_screenscpp_text(void* inForm, uint32_t inWhich, uint32_t inArg, uint8_t* outText, size_t inCapacity)
{
    UWideString theText;
    UIForm* theForm = static_cast<UIForm*>(inForm);
    std::string theList;   // the lists' texts, each ended by a 0 byte
    switch (inWhich) {
    // the civilization choice form's (civ_choice.rs)
    case EE_SCREENS_TEXT_PDCS_LINE: {
        unsigned long theSelection;
        if (EEScreensPdcsControl(3)->GetCurrentSelection(theSelection) != kErrorOK)
            return (size_t)-1;
        theText = EEScreensPdcsControl(3)->GetData(theSelection);
        break;
    }
    case EE_SCREENS_TEXT_PDCS_CIV_NAME:
        gResource->GetString(sPdcsCivs[inArg].mNameID, theText);
        break;
    case EE_SCREENS_TEXT_PDCS_CIV_FILE:
        theText = sPdcsCivs[inArg].GetFileName();
        break;
    case EE_SCREENS_TEXT_MUSIC_FILE:
        theText = theForm->GetMusicFileName();
        break;
    case EE_SCREENS_TEXT_CL_NAME:
        theText = EEUIECLEFormManager::Get()->mEntries[inArg].mName;
        break;
    case EE_SCREENS_TEXT_CL_TEXT:
        theText = EEUIECLEFormManager::Get()->mEntries[inArg].mText;
        break;
    case EE_SCREENS_TEXT_CL_PLAYER:
        theText = gWorld.GetCurrentPlayer()->mName;
        break;
    case EE_SCREENS_TEXT_DS_NAME:
        theText = gWorld.GetPlayer(inArg)->mName;
        break;
    case EE_SCREENS_TEXT_DS_CIVILIZATION:
        theText = UWideString(gWorld.GetPlayer(inArg)->mCivilization.mName);
        break;
    case EE_SCREENS_TEXT_CB_NAME:
        theText = sCivBuilderText;
        break;
    case EE_SCREENS_TEXT_RASTERIZER_SETTING:
        theText = (inArg ? EEGamePreferences::sDefaultSettings : gGamePreferences.GetSettings()).mRasterizerName;
        break;
    case EE_SCREENS_TEXT_AUDIO_DEVICE:
        theText = (inArg ? EEGamePreferences::sDefaultSettings : gGamePreferences.GetSettings()).mAudioDevice;
        break;
    case EE_SCREENS_TEXT_DISPLAY_RASTERIZER:
        if (inArg < gDisplayRasterizers.size())
            theText = gDisplayRasterizers[inArg].mName;
        break;
    case EE_SCREENS_TEXT_DISPLAY_SCREEN:
        if ((inArg >> 16) < gDisplayRasterizers.size() &&
            (inArg & 0xffff) < gDisplayRasterizers[inArg >> 16].mScreens.size())
            theText = gDisplayRasterizers[inArg >> 16].mScreens[inArg & 0xffff].mName;
        break;
    case EE_SCREENS_TEXT_RASTERIZER_NAME:
        if (inArg < GERasterizer::GetRasterizerCount())
            theText = GERasterizer::GetRasterizer(inArg)->GetName();
        break;
    case EE_SCREENS_TEXT_LIST_DATA: {
        UILBString* theBox = static_cast<UILBString*>(theForm->GetControl(inArg >> 16));
        if ((inArg & 0xffff) < theBox->GetLineCount())
            theText = theBox->GetData(inArg & 0xffff);
        break;
    }
    case EE_SCREENS_TEXT_COMBO_DATA: {
        const UILBString* theBox = static_cast<UIComboBox*>(theForm->GetControl(inArg >> 16))->GetListBox();
        if ((inArg & 0xffff) < theBox->GetLineCount())
            theText = theBox->GetData(inArg & 0xffff);
        break;
    }
    case EE_SCREENS_TEXT_FORM_LIST_DATA: {
        UIListBox<UWideString>* theBox =
            static_cast<UIListBox<UWideString>*>(gUIManager->GetFormByID(inArg >> 24)->GetControl((inArg >> 16) & 0xff));
        if ((inArg & 0xffff) < theBox->GetLineCount())
            theText = theBox->GetData(inArg & 0xffff);
        break;
    }
    case EE_SCREENS_TEXT_CAMPAIGN_FILES: {
        // the files of data\Campaigns with the campaign extension (a set: in its order)
        std::set<UString> theFiles;
        FSFileSpec theDirectory;
        FSPath* thePath = new FSPath;
        theDirectory.SetVolume(gStartupVolume);
        theDirectory.SetDirectory(gStartupDirectory + "data\\" + "Campaigns\\");
        thePath->AddDirectory(theDirectory, true);
        UString thePattern = UString("*") + gCampaignExtension;
        thePath->ListFiles(thePattern, theFiles);
        thePath->Dereference();
        for (std::set<UString>::iterator it = theFiles.begin(); it != theFiles.end(); it++)
            theList.append((const char*)*it).push_back('\0');
        break;
    }
    case EE_SCREENS_TEXT_CAMPAIGN_EXTENSION:
        theText = gCampaignExtension;
        break;
    case EE_SCREENS_TEXT_CAMPAIGN_ERROR:
        theText = sCampaignError;
        break;
    case EE_SCREENS_TEXT_ADAPTERS: {
        NEProtocol* theProtocol = NULL;
        std::vector<UWideString> theAdapters;
        NEProtocol::GetProtocol(0, theProtocol);
        if (theProtocol)
            theProtocol->EnumerateNetworkAdapters(theAdapters);
        for (size_t i = 0; i < theAdapters.size(); i++)
            theList.append((const char*)theAdapters[i], theAdapters[i].GetTCHARLength()).push_back('\0');
        break;
    }
    case EE_SCREENS_TEXT_OUTPUT_DEVICES: {
        char theNames[16][128];
        unsigned long theCount = SPIMGetOutputDevices(theNames, 16);
        for (unsigned long i = 0; i < theCount && i < 16; i++)
            theList.append(theNames[i]).push_back('\0');
        break;
    }
    case EE_SCREENS_TEXT_VERSION:
        sprintf(theText, "%s", (const char*)gGameVersion);
        break;
    case EE_SCREENS_TEXT_PLAYER:
        theText = gPlayerName;
        break;
    case EE_SCREENS_TEXT_EDIT:
        theText = static_cast<UIEditBox*>(static_cast<UIForm*>(inForm)->GetControl(inArg))->GetText(0);
        break;
    case EE_SCREENS_TEXT_STRING:
        gResource->GetString(inArg, theText);
        break;
    case EE_SCREENS_TEXT_STRING_CHECKED:
        if (gResource->GetString(inArg, theText) != 0)
            return (size_t)-1;
        break;
    case EE_SCREENS_TEXT_CAMPAIGN: {
        const UWideString* const kCampaigns[4] = {&gCampaignAOCRoman, &gCampaignAOCPacific, &gCampaignAOCAsian,
                                                  &gCampaignEELearning};
        theText = *kCampaigns[inArg & 3];
        break;
    }
    case EE_SCREENS_TEXT_CAMPAIGN_LOADED:
        theText = EECampaignFormManager::Get()->mCampaign.GetDescription();
        break;
    case EE_SCREENS_TEXT_CAMPAIGN_CIV:
        theText = EECampaignFormManager::Get()->mCampaign.GetRequiredCiv();
        break;
    case EE_SCREENS_TEXT_SAVED_EXTENSION:
        theText = gSavedGameExtension;
        break;
    case EE_SCREENS_TEXT_SCENARIO_EXTENSION:
        theText = gScenarioExtension;
        break;
    case EE_SCREENS_TEXT_SI_BRIEFING:
        theText = EEUISCIFormManager::Get()->GetBriefing();
        break;
    case EE_SCREENS_TEXT_SI_HINTS:
        theText = EEUISCIFormManager::Get()->GetHints();
        break;
    case EE_SCREENS_TEXT_SI_HISTORY:
        theText = EEUISCIFormManager::Get()->GetHistory();
        break;
    case EE_SCREENS_TEXT_CS_CAMPAIGN_NAME:
        theText = EECampaignFormManager::Get()->mCampaignName;
        break;
    case EE_SCREENS_TEXT_CS_DESCRIPTION:
        theText = EECampaignFormManager::Get()->mDescription;
        break;
    case EE_SCREENS_TEXT_CS_SCENARIOS: {
        std::vector<UWideString> theScenarios = EECampaignFormManager::Get()->mCampaign.GetScenarios();
        for (size_t i = 0; i < theScenarios.size(); i++)
            theList.append((const char*)theScenarios[i], theScenarios[i].GetTCHARLength()).push_back('\0');
        break;
    }
    case EE_SCREENS_TEXT_CS_COMPLETED: {
        // the campaign progress of the default user: its completed scenarios (none when it does not load)
        EECampaignProgress theProgress;
        std::vector<UWideString> theCompleted;
        EECampaignFormManager* theManager = EECampaignFormManager::Get();
        if (theProgress.Load(UWideString(sCampaignDefaultUser), theManager->mCampaignName, theManager->GetChecksum(),
                             false) == kErrorOK)
            theProgress.GetCompletedScenarios(theCompleted);
        for (size_t i = 0; i < theCompleted.size(); i++)
            theList.append((const char*)theCompleted[i], theCompleted[i].GetTCHARLength()).push_back('\0');
        break;
    }
    case EE_SCREENS_TEXT_CS_PROGRESS: {
        UWideString theString;
        gResource->GetString(IDS_CAMPAIGN_IN_PROGRESS_OF_COMPLETED, theString);
        sprintf(theText, theString, (size_t)(inArg >> 16), (size_t)(inArg & 0xffff));   // (the sizes, as the original)
        break;
    }
    case EE_SCREENS_TEXT_LOAD_SAVED_NAME:
        theText = gLoadSavedGameName;
        break;
    case EE_SCREENS_TEXT_LOAD_SCENARIO_NAME:
        theText = gLoadScenarioName;
        break;
    // the lobby's (lobby_form.rs)
    case EE_SCREENS_TEXT_LE_MAP_TYPE:
        theText = EEScreensLobbyOptions(inArg)->mMapType;
        break;
    case EE_SCREENS_TEXT_LE_GENERATOR_DISPLAY:
        theText = UWideString((const char*)EERMGetGeneratorDisplayName((unsigned long)inArg));
        break;
    case EE_SCREENS_TEXT_LE_MAP_DISPLAY:
        theText = EERMGetGeneratorDisplayName(EEScreensLobbyOptions(inArg)->GetMapType());
        break;
    case EE_SCREENS_TEXT_LE_GENERATOR_NAME:
        theText = EERMGetGeneratorName(inArg);
        break;
    case EE_SCREENS_TEXT_LE_ENGINE: {
        // an engine choice (ee/EECPEngine.h): the strings Classic and Random, an AI's manifest name
        const EECPAIEntry* theAI = EECPAIByChoice(inArg);
        if (theAI != NULL)
            theText = UWideString(theAI->mName.c_str());
        else
            gResource->GetString(kLobbyEngineClassicString + (inArg == kEECPChoiceRandom ? 1 : 0), theText);
        break;
    }
    case EE_SCREENS_TEXT_LE_MISSING:
        theText = sLobbyMissing;
        break;
    case EE_SCREENS_TEXT_LE_CHECK_LABEL: {
        UICheckBox* theBox = static_cast<UICheckBox*>(theForm->GetControl(inArg));
        if (!theBox || !theBox->GetTextLabel())
            return (size_t)-1;
        theText = theBox->GetTextLabel()->GetText(0);
        break;
    }
    }
    if (!theList.empty()) {
        memcpy(outText, theList.data(), theList.size() < inCapacity ? theList.size() : inCapacity);
        return theList.size();
    }
    size_t theLength = theText.GetTCHARLength();
    memcpy(outText, (const char*)theText, theLength < inCapacity ? theLength : inCapacity);
    return theLength;
}

// a text into the game (EE_SCREENS_TEXT_*); 1 for a campaign that loads
extern "C" uint32_t ee_screenscpp_put_text(void* inForm, uint32_t inWhich, uint32_t inArg, const uint8_t* inText,
                                           size_t inLength)
{
    UWideString theText = Text(inText, inLength);
    UIForm* theForm = static_cast<UIForm*>(inForm);
    switch (inWhich) {
    case EE_SCREENS_TEXT_PLAYER:
        gPlayerName = theText;
        break;
    case EE_SCREENS_TEXT_MUSIC_FILE:
        theForm->SetMusicFileName(theText);
        break;
    case EE_SCREENS_TEXT_RASTERIZER_SETTING:
        gGamePreferences.GetSettings().mRasterizerName = std::string((const char*)inText, inLength).c_str();
        break;
    case EE_SCREENS_TEXT_AUDIO_DEVICE: {
        EEGameSettings& theSettings = gGamePreferences.GetSettings();
        snprintf(theSettings.mAudioDevice, sizeof(theSettings.mAudioDevice), "%s",
                 std::string((const char*)inText, inLength).c_str());
        break;
    }
    case EE_SCREENS_TEXT_COMBO_APPEND:
        if (UIComboBox* theBox = static_cast<UIComboBox*>(theForm->GetControl(inArg)))
            theBox->AppendLine(theText);
        break;
    case EE_SCREENS_TEXT_COMBO_INSERT:
        if (UIComboBox* theBox = static_cast<UIComboBox*>(theForm->GetControl(inArg >> 16)))
            theBox->InsertLine(theText, inArg & 0xffff);
        break;
    case EE_SCREENS_TEXT_LIST_APPEND:
        if (UILBString* theBox = static_cast<UILBString*>(theForm->GetControl(inArg)))
            theBox->AppendLine(theText);
        break;
    case EE_SCREENS_TEXT_FORM_LIST_APPEND:
        static_cast<UIListBox<UWideString>*>(gUIManager->GetFormByID(inArg >> 16)->GetControl(inArg & 0xffff))->AppendLine(theText);
        break;
    case EE_SCREENS_TEXT_CAMPAIGN_BUILT_IN:
        return EECampaignFormManager::GetBuiltInCampaign(theText);   // (its number: the checksum it must have)
    case EE_SCREENS_TEXT_MESSAGE_NONE_FOUND:
        gUIManager->DisplayMessage(theText, true, NULL, EEScreensNoCustomCampaigns, true);
        break;
    case EE_SCREENS_TEXT_MESSAGE_CLOSE_CAMPAIGN:
        gUIManager->DisplayMessage(theText, true, NULL, EEScreensCloseCampaign, true);
        break;
    case EE_SCREENS_TEXT_CS_ARCHIVE: {
        // the campaign's file on the data path; a valid one's archive added to it
        FSFileSpec* theArchiveFile = NULL;
        FSArchive theArchive;
        long theResult = gDataPath->FindFile(Parts(inText, inLength)[0], theArchiveFile);
        bool theAdded = theResult == kErrorOK && inArg != 0;
        if (theAdded) {
            theArchive.SetArchive(*theArchiveFile);
            gDataPath->AddArchive(theArchive, true);
            EESetDataPaths();
        }
        if (theArchiveFile)
            delete theArchiveFile;
        return theAdded;
    }
    case EE_SCREENS_TEXT_CS_SCENARIO_HEADER:
    case EE_SCREENS_TEXT_CS_START: {
        // a campaign scenario's header: for Start, its media strings (the campaign's prefix on the sound and map) and
        // file into the intro form's manager (2; 1: the header does not load, 0: no file)
        EEPSaveGameHeader theHeader;
        FSFileSpec* theFile = NULL;
        if (gDataPath->FindFile(UString("Scenarios\\" + gU52A280 + theText + gScenarioExtension), theFile) != kErrorOK) {
            if (theFile)
                delete theFile;
            return 0;
        }
        if (theHeader.LoadFromFile(*theFile) != kErrorOK) {
            delete theFile;
            return inWhich == EE_SCREENS_TEXT_CS_START ? 1 : 0;
        }
        if (inWhich == EE_SCREENS_TEXT_CS_SCENARIO_HEADER) {
            delete theFile;
            return 1;
        }
        EESMMediaStrings theMediaStrings;
        theMediaStrings = theHeader.mMediaStrings;
        EEUISCIFormManager* theInfo = EEUISCIFormManager::Get();
        EEScreensMedia::Prefix(theMediaStrings);
        EECampaignFormManager::Get()->UpdateChecksum();
        theInfo->mMediaStrings = theMediaStrings;
        theInfo->SetFile(theFile);
        theInfo->mIsCampaign = true;
        return 2;
    }
    case EE_SCREENS_TEXT_CS_SCENARIO_NAME:
        EECampaignFormManager::Get()->mScenarioName = theText;
        break;
    case EE_SCREENS_TEXT_QUESTION:
        gUIManager->DisplayQuestion(theText, true, theForm, EEScreensRestoreDefaultYes, NULL);
        break;
    case EE_SCREENS_TEXT_EDIT:
        static_cast<UIEditBox*>(theForm->GetControl(inArg))->SetText(theText);
        break;
    case EE_SCREENS_TEXT_LABEL:
        static_cast<UITextLabel*>(theForm->GetControl(inArg))->SetText(theText);
        break;
    case EE_SCREENS_TEXT_MESSAGE:
        gUIManager->DisplayMessage(theText, true, NULL, 0, inArg != 0);
        break;
    case EE_SCREENS_TEXT_MG_CHAT_LOG:
        gUserInterface->AddChatLogEntry(theText, 0);
        break;
    case EE_SCREENS_TEXT_CH_CHEAT: {
        long theCode = 0;
        long theParameter = 0;
        gCheatCodes->ParseCheatCode(theText, theCode, theParameter);
        if (theCode != 0)
            gCommunications->ProcessLocalUserInput(new EEMCheatCode(theCode, theParameter), kSlotNone);
        break;
    }
    case EE_SCREENS_TEXT_CH_CHAT:
        gCommunications->ProcessLocalUserInput(new EEMInGameChat(theText), kSlotNone);
        break;
    case EE_SCREENS_TEXT_CL_APPEND:
        static_cast<UITextLabel*>(gUIManager->GetFormByID(kFormChatLog)->GetControl(kChatLogChatLogTextBox))->AppendText(theText);
        break;
    case EE_SCREENS_TEXT_DS_BUTTON: {
        UIButton* theButton = static_cast<UIButton*>(theForm->GetControl(inArg));
        theButton->SetText(theText, theButton->GetGEText()->GetFont());
        break;
    }
    case EE_SCREENS_TEXT_CB_CIVILIZATION_NAME:
        CivBuilderSetCivilizationName(theText);
        break;
    case EE_SCREENS_TEXT_CB_SET_NAME:
        return (uint32_t)CivBuilderSetName(theText);
    case EE_SCREENS_TEXT_CB_LOAD:
        return (uint32_t)CivBuilderLoadCivilization(UString(std::string((const char*)inText, inLength).c_str()));
    case EE_SCREENS_TEXT_MG_QUICK_LOAD: {
        // the last quick save's (the auto save's when inArg) load; theText shown when its file is not found
        UWideString theName = inArg ? gGamePreferences.mSettings.mLastAutoSave : gGamePreferences.mSettings.mLastQuickSave;
        FSFileSpec* theFile = new FSFileSpec;
        long theResult = EEOpenSaveGameFile((const char*)theName, "Saved Games\\", theFile);
        if (theResult == kErrorOK) {
            theFile->SetFile((const char*)theName);
            gUserInterface->PostLoadSavedGame(*theFile);
        } else
            gUIManager->DisplayMessage(theText, 1, NULL, 0, true);
        if (theFile)
            delete theFile;
        break;
    }
    case EE_SCREENS_TEXT_LABEL_LINES: {
        UITextLabel* theLabel = static_cast<UITextLabel*>(gUIManager->GetFormByID(inArg >> 16)->GetControl(inArg & 0xffff));
        theLabel->DeleteArray();
        theLabel->AppendText(theText);
        break;
    }
    case EE_SCREENS_TEXT_CAMPAIGN_NAME:
        EECampaignFormManager::Get()->SetCampaignName(theText);
        break;
    case EE_SCREENS_TEXT_CAMPAIGN_DESCRIPTION:
        EECampaignFormManager::Get()->SetDescription(theText);
        break;
    case EE_SCREENS_TEXT_CAMPAIGN_FIRST:
        EECampaignFormManager::Get()->SetFirstScenario(theText);
        break;
    case EE_SCREENS_TEXT_CAMPAIGN_LOAD: {
        UWideString theFile;
        UWideString theError;
        theFile = "Campaigns\\" + theText + gCampaignExtension;
        bool theLoaded = EECampaignFormManager::Get()->mCampaign.Load(theFile, theError, false);
        sCampaignError = theError;
        return theLoaded ? 1 : 0;
    }
    case EE_SCREENS_TEXT_SERVER:
        // the random map's local server (the old communications deleted) and the human's connection request
        if (gCommunications)
            delete gCommunications;
        gCommunications = new EEServer(UWideString("EEName"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, true, true);
        static_cast<EEServer*>(gCommunications)->StartThreads();
        static_cast<EEServer*>(gCommunications)->mNumLoadResponses = false;
        gCommunications->ProcessLocalUserInput(new NEMClientConnectionRequest(theText, gGameVersion, gBinaryChecksum, false),
                                               kSlotNone);
        break;
    // the load and save screens' (save_load.rs)
    case EE_SCREENS_TEXT_LOAD_SAVED_NAME:
        gGamePreferences.mSettings.SetLoadSavedGameName(Parts(inText, inLength)[0]);
        break;
    case EE_SCREENS_TEXT_LOAD_SCENARIO_NAME:
        gGamePreferences.mSettings.SetLoadScenarioName(Parts(inText, inLength)[0]);
        break;
    case EE_SCREENS_TEXT_FILE_LIST: {
        // a file list from a directory below the startup one with an extension, the name selected when listed
        std::vector<UString> theParts = Parts(inText, inLength);
        UIFileListBox* theList = static_cast<UIFileListBox*>(gUIManager->GetFormByID(inArg >> 16)->GetControl(inArg & 0xffff));
        if (theList == NULL)
            break;
        FSFileSpec theDirectory;
        FSPath* thePath = new FSPath;
        UWideString theName(theParts[2]);
        unsigned long theIndex = 0;
        theList->AddValidExtension(theParts[0]);
        theDirectory.SetVolume(gStartupVolume);
        theDirectory.SetDirectory(gStartupDirectory + theParts[1]);
        thePath->AddDirectory(theDirectory, true);
        theList->SetSearchPaths(thePath);
        theList->CreateEntries(1);
        if (theList->FindString(theName, theIndex) == true) {
            theList->ChangeSelectionIndex(theIndex, true, false);
            theList->SetScrollBarCurrentRange(theIndex);
        }
        thePath->Dereference();
        break;
    }
    case EE_SCREENS_TEXT_QUESTION_DELETE:
        gUIManager->DisplayQuestion(theText, true, reinterpret_cast<UIForm*>(static_cast<UIFileListBox*>(theForm->GetControl(inArg))),
                                    EEScreensDeleteSelectedFile, 0);
        break;
    case EE_SCREENS_TEXT_LOAD_POST: {
        std::vector<UString> theParts = Parts(inText, inLength);
        FSFileSpec* theFile = NULL;
        if (EEOpenSaveGameFile(theParts[0], theParts[1], theFile) != kErrorOK)
            return 0;
        gUserInterface->PostLoadSavedGame(*theFile);
        if (theFile)
            delete theFile;
        return 1;
    }
    case EE_SCREENS_TEXT_SAVE_MESSAGE:
        sSaveObject = reinterpret_cast<UIForm*>(static_cast<NEMessage*>(new EEMLoadOrSaveGame(
            theText, TMGetTime(), 0, 0, gCommunications->GetNumberOfCTsProcessedInPhysics(), 0)));
        break;
    case EE_SCREENS_TEXT_SAVE_ACTION: {
        std::vector<UString> theParts = Parts(inText, inLength);
        sSaveObject = reinterpret_cast<UIForm*>(static_cast<EEAction*>(new EEASaveGame(theParts[0], true, theParts[1])));
        break;
    }
    case EE_SCREENS_TEXT_FILE_EXISTS: {
        std::vector<UString> theParts = Parts(inText, inLength);
        FSFileSpec theSpec;
        theSpec.SetFile(theParts[0]);
        theSpec.SetDirectory(gStartupDirectory + theParts[1]);
        if (theSpec.Open(kFileAccessRead) != kErrorOK)
            return 0;
        theSpec.Close();
        return 1;
    }
    case EE_SCREENS_TEXT_SAVE_QUESTION:
        gUIManager->DisplayQuestion(theText, true, sSaveObject, inArg ? EEScreensPostSave : EEScreensSendSave, EEScreensDropSave);
        break;
    case EE_SCREENS_TEXT_LOAD_SAVED_GAME: {
        // the single player load: the saved game validated, the lobby options and a new local server set up for
        // it, its loading started; an invalid file shows the reason (the file is freed on every path)
        std::vector<UString> theParts = Parts(inText, inLength);
        UWideString theMessage;
        FSFileSpec* theFile = NULL;
        if (EEOpenSaveGameFile(theParts[0], theParts[1], theFile) == kErrorOK && theFile->Open(kFileAccessRead) == kErrorOK) {
            if (!EEValidateSaveGameFile(*theFile, theMessage)) {
                gUIManager->DisplayMessage(theMessage, true, NULL, 0, true);
                delete theFile;
                return 0;
            }
            gWorld.GetLobbyOptions().GetLock()->WriteLock();
            gWorld.GetLobbyOptions().SetDefaults();
            gWorld.GetLobbyOptions().SetLoadType(2);
            gWorld.GetLobbyOptions().SetMapType((const char*)theFile->GetFile());
            gWorld.GetLobbyOptions().GetLock()->WriteUnlock();
            gLoadSavedGameName = theFile->GetFile().GetIndexedSubstring(0, theFile->GetFile().GetLength() - 5);
            delete theFile;
            gGameState.mGameOptions.SetIsMultiplayer(false);
            if (gCommunications)
                delete gCommunications;
            gCommunications = new EEServer(UWideString("EEName"), gGamePreferences.mSettings.GetNetworkAdapter(), 0, 0, true, true);
            static_cast<EEServer*>(gCommunications)->StartThreads();
            static_cast<EEServer*>(gCommunications)->SetNumLoadResponses(false);
            gGameStateLoadType = 2;
            static_cast<EEServer*>(gCommunications)->StopPing();
            static_cast<EEServer*>(gCommunications)->SetLoadState(true);
            gCommunications->ProcessLocalUserInput(new EEMStartLoading(1, TMGetTime()), kSlotNone);
            return 1;
        }
        if (theFile != NULL)
            delete theFile;
        return 0;
    }
    case EE_SCREENS_TEXT_SCENARIO_INTRO: {
        // a scenario's header into the scenario intro's form manager (2; the caller opens the form), the scenario
        // the last loaded one; 1: the file is not a scenario, 0: not found
        std::vector<UString> theParts = Parts(inText, inLength);
        EEPSaveGameHeader theHeader;
        FSFileSpec* theFile = NULL;
        if (EEOpenSaveGameFile(theParts[0], theParts[1], theFile) != kErrorOK)
            return 0;
        theHeader.SetMajorVersion(kSaveGameMajorVersion);
        theHeader.SetMinorVersion(kSaveGameMinorVersion);
        if (theHeader.LoadFromFile(*theFile) != kErrorOK) {
            if (theFile)
                delete theFile;
            return 1;
        }
        EEUISCIFormManager* theManager = EEUISCIFormManager::Get();
        theManager->SetMediaStrings(theHeader.GetMediaStrings());
        theManager->SetFile(theFile);
        theManager->SetIsCampaign(false);
        gLoadScenarioName = theFile->GetFile().GetIndexedSubstring(0, theFile->GetFile().GetLength() - 5);
        return 2;
    }
    case EE_SCREENS_TEXT_FORM_EDIT:
        static_cast<UIEditBox*>(gUIManager->GetFormByID(inArg >> 16)->GetControl(inArg & 0xffff))->SetText(theText);
        break;
    // the lobby's (lobby_form.rs)
    case EE_SCREENS_TEXT_LE_MAP_TYPE:
        gGameOptions.mMapType = theText;
        break;
    case EE_SCREENS_TEXT_LE_SEND_MAP_TYPE:
        gCommunications->ProcessGuaranteedLocalUserInput(new EEMGameOptions(2, (long)inArg, theText), kSlotNone);
        break;
    case EE_SCREENS_TEXT_LE_CHAT:
        gCommunications->ProcessGuaranteedLocalUserInput(new EEMChat(0, theText), kSlotNone);
        break;
    case EE_SCREENS_TEXT_LE_CHECK_LOAD: {
        // the start's check of the loaded game's file (a header of its own)
        std::vector<UString> theParts = Parts(inText, inLength);
        EEPSaveGameHeader theHeader;
        FSFileSpec* theStream = NULL;
        return EEScreensLobbyLoad(theParts[0], theParts[1], theHeader, theStream, true, 0);
    }
    case EE_SCREENS_TEXT_LE_OPEN_HEADER: {
        // the load screen's: the header kept for the transfers, the options and the slots
        std::vector<UString> theParts = Parts(inText, inLength);
        if (sLobbyHeader)
            delete sLobbyHeader;
        sLobbyHeader = new EEPSaveGameHeader;
        sLobbyFile = NULL;
        return EEScreensLobbyLoad(theParts[0], theParts[1], *sLobbyHeader, sLobbyFile, false, inArg);
    }
    case EE_SCREENS_TEXT_LE_BOOT_QUESTION:
        gUIManager->DisplayQuestion(theText, true, reinterpret_cast<UIForm*>(sLobbyBoot), EEScreensBootYes,
                                    EEScreensBootNo);
        break;
    // the in-game options menu's (main_game_options.rs)
    case EE_SCREENS_TEXT_GO_QUESTION: {
        EEUIManager* theManager = theForm ? static_cast<EEUIManager*>(theForm->GetManager()) : gUIManager;
        EEUIFErrorCallback theYes = inArg == EE_SCREENS_GO_RESIGN_YES    ? EEScreensResignYes
                                  : inArg == EE_SCREENS_GO_RESTART_YES   ? EEScreensRestartYes
                                  : inArg == EE_SCREENS_GO_RETURN_TO_EDITOR_YES ? EEScreensToEditorYes
                                                                          : EEScreensExitYes;
        theManager->DisplayQuestion(theText, true, NULL, theYes, 0);
        break;
    }
    case EE_SCREENS_TEXT_GO_BUTTON: {
        UIButton* theButton = static_cast<UIButton*>(theForm->GetControl(inArg));
        theButton->SetText(theText, theButton->GetGEText()->GetFont());
        break;
    }
    // the debug sound panel's (debug_sound.rs)
    case EE_SCREENS_TEXT_DSU_LABEL:
        if (UITextLabel* theLabel = dynamic_cast<UITextLabel*>(gUIManager->GetFormByID(kFormDebugSound)->GetControlById(inArg)))
            theLabel->SetText(theText);
        break;
    // the civilization choice form's (civ_choice.rs)
    case EE_SCREENS_TEXT_PDCS_LOAD:
        sPdcsName = UWideString();
        sPdcsUpgrades.clear();
        return (uint32_t)EELoadCivilization(UString(theText), EEGetCivilizationDirectory(false), sPdcsName, sPdcsUpgrades);
    case EE_SCREENS_TEXT_PDCS_LABEL:
        static_cast<UITextLabel*>(static_cast<UIControl*>(EEScreensPdcsControl(5)))->SetText(theText);
        break;
    }
    return 0;
}

// the random map's lobby options inside the write lock (EE_SCREENS_GAME_LOBBY_BEGIN): single player, the slots from
// the table (the human's and the computer's with their names, the others a new slot's with its slot number), then
// the unlock
extern "C" void ee_screenscpp_lobby_fill(const EeScreensSinglePlayerSlot* inSlots, const uint8_t* inHuman,
                                         size_t inHumanLength, const uint8_t* inComputer, size_t inComputerLength)
{
    EEGameOptionsPlayer theComputer;
    EEGameOptionsPlayer theHuman;
    EEGameOptionsPlayer theClosed;
    gWorld.GetLobbyOptions().mIsMultiplayer = false;
    for (unsigned long i = 0; i < 16; i++) {
        const EeScreensSinglePlayerSlot& theSlot = inSlots[i];
        EEGameOptionsPlayer& thePlayer = theSlot.source == EE_SCREENS_SP_HUMAN ? theHuman
                                       : theSlot.source == EE_SCREENS_SP_COMPUTER ? theComputer : theClosed;
        if (theSlot.source != EE_SCREENS_SP_CLOSED) {
            thePlayer.mType = theSlot.kind;
            thePlayer.mColor = theSlot.color;
            thePlayer.mIsLocal = theSlot.local;
            thePlayer.mIsReady = theSlot.ready;
            thePlayer.mName = theSlot.source == EE_SCREENS_SP_HUMAN ? Text(inHuman, inHumanLength)
                                                                    : Text(inComputer, inComputerLength);
        }
        thePlayer.mSlot = theSlot.slot;
        gWorld.GetLobbyOptions().mPlayers[i] = thePlayer;
    }
    gWorld.GetLobbyOptions().mLock->WriteUnlock();
}

// ---- the settings form's records (settings_form.rs)

// a control's GetCurrentSelection (a combo box's, else a list box's): its result, the selection into outSelection
extern "C" int32_t ee_screenscpp_selection(void* inForm, uint32_t inControl, bool inList, uint32_t* outSelection)
{
    UIControl* theControl = static_cast<UIForm*>(inForm)->GetControl(inControl);
    unsigned long theSelection = 0;
    long theResult = kErrorFailed;
    if (theControl && inList)
        theResult = static_cast<UILBString*>(theControl)->GetCurrentSelection(theSelection);
    else if (theControl)
        theResult = static_cast<UIComboBox*>(theControl)->GetCurrentSelection(theSelection);
    *outSelection = theSelection;
    return theResult;
}

// the fields of EEGameSettings the form reads and writes: (record field, settings member)
#define EE_SCREENS_SETTINGS_FIELDS(X)                                                                                  \
    X(mouse_sensitivity, mMouseSensitivity) X(side_scroll_speed, mSideScrollSpeed)                                     \
    X(grab_scroll_speed, mGrabScrollSpeed) X(sound_volume, mSoundVolume) X(music_volume, mMusicVolume)                 \
    X(max_number_of_sounds, mMaxNumberOfSounds) X(game_bit_depth, mGameBitDepth)                                       \
    X(game_window_height, mGameWindowHeight) X(game_window_width, mGameWindowWidth)                                    \
    X(mip_levels_to_drop, mMIPLevelsToDrop) X(model_detail, mModelDetail) X(effect_detail, mEffectDetail)              \
    X(network_adapter, mNetworkAdapter) X(screen_id, mScreenID) X(terrain_mip_levels_to_drop, mTerrainMIPLevelsToDrop) \
    X(texture_bit_depth, mTextureBitDepth) X(number_of_auto_saves, mNumberOfAutoSaves)                                 \
    X(auto_save_in_milliseconds, mAutoSaveInMilliseconds) X(texture_resolution, mTextureResolution)                    \
    X(ui_scale, mUIScale) X(render_resolution, mRenderResolution) X(window_mode, mWindowMode)                          \
    X(frame_rate_limit, mFrameRateLimit) X(texture_filtering, mTextureFiltering) X(brightness, mBrightness)            \
    X(master_volume, mMasterVolume) X(maximum_zoom_out, mMaximumZoomOut) X(zoom_enabled, mZoomEnabled)                 \
    X(display_clouds, mDisplayClouds) X(display_shadows, mDisplayShadows)                                              \
    X(display_battlefield_ambients, mDisplayBattlefieldAmbients) X(trilinear_filter, mTrilinearFilter)                 \
    X(full_screen, mFullScreen) X(reverb_enabled, mReverbEnabled) X(wait_for_vsync, mWaitForVSync)                     \
    X(ambient_sounds, mAmbientSounds) X(show_blood, mShowBlood) X(music_enabled, mMusicEnabled)                        \
    X(right_hand_mouse_orientation, mRightHandMouseOrientation) X(unit_lighting, mUnitLighting)                        \
    X(display_weather_effects, mDisplayWeatherEffects) X(anti_aliasing, mAntiAliasing)                                 \
    X(mute_in_background, mMuteInBackground)

// the settings (the defaults when inDefaults) into outRecord
extern "C" void ee_screenscpp_settings(bool inDefaults, EeScreensSettingsRecord* outRecord)
{
    const EEGameSettings& theSettings = inDefaults ? EEGamePreferences::sDefaultSettings : gGamePreferences.GetSettings();
#define EE_SCREENS_GET(field, member) outRecord->field = theSettings.member;
    EE_SCREENS_SETTINGS_FIELDS(EE_SCREENS_GET)
#undef EE_SCREENS_GET
}

// a record into the settings
extern "C" void ee_screenscpp_put_settings(const EeScreensSettingsRecord* inRecord)
{
    EEGameSettings& theSettings = gGamePreferences.GetSettings();
#define EE_SCREENS_PUT(field, member) theSettings.member = inRecord->field;
    EE_SCREENS_SETTINGS_FIELDS(EE_SCREENS_PUT)
#undef EE_SCREENS_PUT
}

// FillModes' modes, unsorted (the first screen of index inScreen in the first rasterizer of index inRasterizer that
// has it): up to inCapacity into outModes; their count
extern "C" size_t ee_screenscpp_modes(uint32_t inRasterizer, uint32_t inScreen, EeScreensVideoMode* outModes,
                                      size_t inCapacity)
{
    for (size_t r = 0; r < gDisplayRasterizers.size(); r++) {
        if (gDisplayRasterizers[r].mIndex != inRasterizer)
            continue;
        const std::vector<EEDisplayScreen>& theScreens = gDisplayRasterizers[r].mScreens;
        for (size_t s = 0; s < theScreens.size(); s++) {
            if (theScreens[s].mIndex != inScreen)
                continue;
            const std::vector<EEDisplayMode>& theModes = theScreens[s].mModes;
            for (size_t m = 0; m < theModes.size() && m < inCapacity; m++) {
                outModes[m].width = theModes[m].mMode.mWidth;
                outModes[m].height = theModes[m].mMode.mHeight;
                outModes[m].refresh_rate = theModes[m].mMode.mRefreshRate;
                outModes[m].bit_count = theModes[m].mMode.mBitCount;
            }
            return theModes.size();
        }
    }
    return 0;
}

// the current mode of gRasterizer's screen inScreen
extern "C" void ee_screenscpp_current_mode(uint32_t inScreen, EeScreensVideoMode* outMode)
{
    const GEVideoMode& theMode = gRasterizer->GetScreen(inScreen)->GetCurrentMode();
    outMode->width = theMode.mWidth;
    outMode->height = theMode.mHeight;
    outMode->refresh_rate = theMode.mRefreshRate;
    outMode->bit_count = theMode.mBitCount;
}

// GXGameSize
extern "C" void ee_screenscpp_game_size(uint32_t inPercent, bool inWindowed, uint32_t* outWidth, uint32_t* outHeight)
{
    unsigned long theWidth = 0, theHeight = 0;
    GXGameSize(inPercent, inWindowed, &theWidth, &theHeight);
    *outWidth = theWidth;
    *outHeight = theHeight;
}

// ---- the lobby's records (lobby_form.rs)

// a slot of a source (EE_SCREENS_LE_SOURCE_*) into outPlayer, its name into outName (up to inCapacity bytes); the
// name's length (a missing slot: zeros, 0)
extern "C" size_t ee_screenscpp_lobby_player(uint32_t inSource, uint32_t inIndex, EeScreensLobbyPlayerRecord* outPlayer,
                                             uint8_t* outName, size_t inCapacity)
{
    const EEGameOptionsPlayer* thePlayer = EEScreensLobbySlot(inSource, inIndex);
    if (!thePlayer) {
        memset(outPlayer, 0, sizeof(*outPlayer));
        return 0;
    }
    outPlayer->kind = thePlayer->mType;
    outPlayer->loaded_type = thePlayer->mLoadedType;
    outPlayer->ping = thePlayer->mPing;
    outPlayer->slot = thePlayer->mSlot;
    outPlayer->color = thePlayer->mColor;
    outPlayer->team = thePlayer->mTeam;
    outPlayer->citizens = thePlayer->mStartingCitizens;
    outPlayer->transfer = thePlayer->mTransferID;
    outPlayer->is_local = thePlayer->mIsLocal;
    outPlayer->is_ready = thePlayer->mIsReady;
    outPlayer->b36 = thePlayer->mB36;
    outPlayer->engine = thePlayer->mEngine;
    size_t theLength = thePlayer->mName.GetTCHARLength();
    memcpy(outName, (const char*)thePlayer->mName, theLength < inCapacity ? theLength : inCapacity);
    return theLength;
}

// a slot of a source from a record and a name (the boot question's copy made when there is none)
extern "C" void ee_screenscpp_put_lobby_player(uint32_t inSource, uint32_t inIndex,
                                               const EeScreensLobbyPlayerRecord* inPlayer, const uint8_t* inName,
                                               size_t inLength)
{
    if (inSource == EE_SCREENS_LE_SOURCE_BOOT && !sLobbyBoot)
        sLobbyBoot = new EEGameOptionsPlayer;
    EEGameOptionsPlayer* thePlayer = EEScreensLobbySlot(inSource, inIndex);
    if (!thePlayer)
        return;
    thePlayer->mType = inPlayer->kind;
    thePlayer->mLoadedType = inPlayer->loaded_type;
    thePlayer->mName = Text(inName, inLength);
    thePlayer->mPing = inPlayer->ping;
    thePlayer->mSlot = inPlayer->slot;
    thePlayer->mColor = inPlayer->color;
    thePlayer->mTeam = inPlayer->team;
    thePlayer->mStartingCitizens = inPlayer->citizens;
    thePlayer->mTransferID = inPlayer->transfer;
    thePlayer->mIsLocal = inPlayer->is_local;
    thePlayer->mIsReady = inPlayer->is_ready;
    thePlayer->mB36 = inPlayer->b36;
    thePlayer->mEngine = inPlayer->engine;
}

// the options of a source into outOptions
extern "C" void ee_screenscpp_lobby_options(uint32_t inSource, EeScreensLobbyOptionsRecord* outOptions)
{
    const EEGameOptions& o = *EEScreensLobbyOptions(inSource);
    outOptions->load_type = o.mLoadType;
    outOptions->map_size = o.mMapSize;
    outOptions->resources = o.mResources;
    outOptions->epoch_start = o.mEpochStart;
    outOptions->epoch_end = o.mEpochEnd;
    outOptions->unit_limit = o.mUnitLimit;
    outOptions->wonders = o.mWondersForVictory;
    outOptions->variant = o.mVariant;
    outOptions->difficulty = o.mDifficulty;
    outOptions->speed = o.mGameSpeed;
    outOptions->multiplayer = o.mIsMultiplayer;
    outOptions->record_games = o.mRecordGames;
    outOptions->custom_civs = o.mAllowCustomCivs;
    outOptions->reveal_map = o.mRevealMap;
    outOptions->lock_teams = o.mLockTeams;
    outOptions->lock_speed = o.mLockSpeed;
    outOptions->cheat_codes = o.mCheatCodes;
    outOptions->victory = o.mVictory;
}
