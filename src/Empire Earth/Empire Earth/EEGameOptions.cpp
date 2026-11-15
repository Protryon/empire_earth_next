// C:\EEX\Empire Earth\Empire Earth\EEGameOptions.cpp (__FILE__ 0x4bf9c4, the ctors' CreateReadWriteLock asserts).
// 0x12f4b2..0x1304e4: the empty player slot's `$E` chain, EEGameOptionsPlayer, EEGameOptions and
// the two table-size helpers; EEGameState::Clear 0x1304e4 starts the next unit. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/UWideString.h"
#include "ee/EESMDialogMessage.h"
#include "lle/FSFile.h"
#include <vector>
#include "ee/EEAreaUtilities.h"
#include "ee/EEScenarioManager.h"
#include "ee/EESMCondition.h"
#include <algorithm>
#include <mbstring.h>
#include <stdlib.h>
#include "ee/EESMEffect.h"
#include "ee/EESMObject.h"
#include "ee/EESMArea.h"
#include "ee/EESMTrigger.h"
#include "ee/EEDeleteVectorContents.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEUGRust.h"
#include "ee/EEPhysics.h"
#include "ee/EEAIRandom.h"
#include "ee/EEDebugTrace.h"
#include "lle/SESoundManager.h"
#include "lle/FSFileSpec.h"
#include "GEWindow.h"
#include "ee/EECameraControl.h"
#include "ee/EEGraphics.h"
#include "ee/EEMouse.h"
#include "ee/EEUserInterface.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEBitmap.h"
#include "lle/GEVertexArray.h"
#include "lle/GERhombus.h"
#include "lle/ISEntity.h"
#include "lle/UIEntity.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GERasterizer.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEGameSound.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/EECommunications.h"
#include "lle/TSScheduler.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEGameVariant.h"
#include "ee/EERandomMapGenerators.h"
#include "ee/EEGameOptions.h"
#include "ee/EERules.h"
#include "ee/EERulesWorld.h"

extern char gEmptyString[];                 // 0x4e5b98 (pooled "")
extern EEGameOptions& gLobbyGameOptions;     // 0x53030c (SetDefaults starts from it)
unsigned long EEGetNumberOfResourceLevels();

// 0x529b38: the empty slot every constructor and SetDefaults copies into the sixteen players
static EEGameOptionsPlayer sEmptyPlayer;

#include "ee/EEStartingResources.h"
#include "ee/EECPEngine.h"

// The defaults the constructors read (the rules and the epoch count only: the global options are made before the
// tables, the variants and the AI manifest exist)
static EeAppOptionsLimits EEGameOptionsDefaults()
{
    const EEGameOptionRules& theRules = EEGetRules().mGameOptions;
    EeAppOptionsLimits theLimits = {};
    theLimits.epochs = EEEpochCount();
    theLimits.map_size_default = theRules.mMapSizeDefault;
    theLimits.speed_default = theRules.mSpeedDefault;
    theLimits.wonders_default = theRules.mWondersDefault;
    return theLimits;
}

// The limits the Rust bodies check against: gameoptions.yaml's lists and numbers, the tables' sizes (the engine
// choices only for a load, inLoad)
static EeAppOptionsLimits EEGameOptionsLimits(bool inLoad = false)
{
    const EEGameOptionRules& theRules = EEGetRules().mGameOptions;
    EeAppOptionsLimits theLimits;
    theLimits.load_types = kLoadTypeCount;
    theLimits.resource_levels = EEGetNumberOfResourceLevels();
    theLimits.variants = EEGetNumberOfGameVariants();
    theLimits.epochs = EEEpochCount();
    theLimits.map_sizes = theRules.mMapSizes.size();
    theLimits.map_size_default = theRules.mMapSizeDefault;
    theLimits.difficulties = theRules.mDifficulties.size();
    theLimits.speeds = theRules.mSpeeds.size();
    theLimits.speed_default = theRules.mSpeedDefault;
    theLimits.wonders_default = theRules.mWondersDefault;
    theLimits.wonders_maximum = EERuleLong(theRules.mWondersMaximum);
    theLimits.unit_limit_minimum = EERuleLong(theRules.mUnitLimitMinimum);
    theLimits.unit_limit_maximum = EERuleLong(theRules.mUnitLimitMaximum);
    theLimits.unit_limit_default = EERuleLong(theRules.mUnitLimitDefault);
    theLimits.engine_choices = inLoad ? EECPEngineChoiceCount() : 0;
    return theLimits;
}

// the saved game's host: the file's primitives (EESavePrimitive; the records are the same) and the strings
static_assert(EE_APP_SAVE_READ_ULONG == EE_UNITS_PLAYER_SAVE_READ_ULONG && EE_APP_SAVE_READ_LONG == EE_UNITS_PLAYER_SAVE_READ_LONG &&
                  EE_APP_SAVE_READ_FLOAT == EE_UNITS_PLAYER_SAVE_READ_FLOAT && EE_APP_SAVE_READ_BOOL == EE_UNITS_PLAYER_SAVE_READ_BOOL &&
                  EE_APP_SAVE_WRITE_ULONG == EE_UNITS_PLAYER_SAVE_WRITE_ULONG && EE_APP_SAVE_WRITE_LONG == EE_UNITS_PLAYER_SAVE_WRITE_LONG &&
                  EE_APP_SAVE_WRITE_FLOAT == EE_UNITS_PLAYER_SAVE_WRITE_FLOAT && EE_APP_SAVE_WRITE_BOOL == EE_UNITS_PLAYER_SAVE_WRITE_BOOL &&
                  sizeof(EeAppSaveIo) == sizeof(EeUnitsPlayerSaveIo) && offsetof(EeAppSaveIo, f) == offsetof(EeUnitsPlayerSaveIo, f) &&
                  offsetof(EeAppSaveIo, b) == offsetof(EeUnitsPlayerSaveIo, b),
              "ee_app save_io.rs mirrors EESavePrimitive's operations");
struct EEGameOptionsFile {
    EEGameOptions* mOptions;
    FSFile* mFile;
};
static int32_t EEGameOptionsIo(void* inContext, int32_t inOp, uint32_t inIndex, EeAppSaveIo* io)
{
    EEGameOptionsFile* theContext = static_cast<EEGameOptionsFile*>(inContext);
    EEGameOptions* theOptions = theContext->mOptions;
    FSFile& theFile = *theContext->mFile;
    UWideString* theStrings[3] = {&theOptions->mCampaignName, &theOptions->mScenarioName, &theOptions->mMapName};
    switch (inOp) {
    case EE_APP_OPTIONS_NAME_LOAD: return theOptions->mPlayers[inIndex].mName.Load(theFile);
    case EE_APP_OPTIONS_NAME_SAVE: return theOptions->mPlayers[inIndex].mName.Save(theFile);
    case EE_APP_OPTIONS_STRING_LOAD: return theStrings[inIndex]->Load(theFile);
    case EE_APP_OPTIONS_STRING_SAVE: return theStrings[inIndex]->Save(theFile);
    case EE_APP_OPTIONS_VARIANT_KNOWN: return gGameVariants[inIndex] != NULL;
    }
    return EESavePrimitive(theFile, inOp, reinterpret_cast<EeUnitsPlayerSaveIo*>(io));
}
static void EEGameOptionsSlots(EEGameOptions* inOptions, EeAppOptionsPlayerFields* outSlots[16])
{
    for (unsigned long i = 0; i < 16; i++)
        outSlots[i] = &inOptions->mPlayers[i];
}

// Creates an empty, closed player slot with no name, colour, team or starting citizens
EEGameOptionsPlayer::EEGameOptionsPlayer()
{
    ee_app_options_player_default(this);
    this->mName = gEmptyString;
}

// Creates default game options (random epochs, normal speed and map size, unit limit 50, three wonders to win)
// with sixteen empty player slots
EEGameOptions::EEGameOptions()
{
    ESVERIFY_AT(TSScheduler::CreateReadWriteLock(this->mLock) == keseOK, 304);
    EeAppOptionsLimits theLimits = EEGameOptionsDefaults();
    ee_app_options_default(this, &theLimits);
    this->mMapType = gEmptyString;
    this->mCampaignName = gEmptyString;
    this->mScenarioName = gEmptyString;
    this->mMapName = gEmptyString;
    for (unsigned long i = 0; i < 16; i++)
        this->mPlayers[i] = sEmptyPlayer;
}

// Creates a copy of other game options, with its own lock (the copy keeps the defaults' mB407: operator= leaves it)
EEGameOptions::EEGameOptions(const EEGameOptions& inOther)
{
    ESVERIFY_AT(TSScheduler::CreateReadWriteLock(this->mLock) == keseOK, 349);
    EeAppOptionsLimits theLimits = EEGameOptionsDefaults();
    ee_app_options_default(this, &theLimits);
    *this = inOther;
}

// Destroys the game options, releasing their lock
EEGameOptions::~EEGameOptions()
{
    TSScheduler::DestroyReadWriteLock(this->mLock);
}

// Copies other game options, replacing each out-of-range setting (load type, resources, variant, epochs, map,
// size, difficulty, speed, wonders, unit limit) with a default (ee_app game_options.rs assign); copies names, flags
// and all sixteen slots as they are. A new game's map type must be a known generator's, else the first one's.
EEGameOptions& EEGameOptions::operator=(const EEGameOptions& inOther)
{
    unsigned long theIndex;
    EeAppOptionsLimits theLimits = EEGameOptionsLimits();
    ee_app_options_assign(this, &inOther, &theLimits);
    if (inOther.mLoadType == 0) {
        if (EERMFindGenerator(inOther.mMapType, theIndex) == 0)
            this->mMapType = inOther.mMapType;
        else
            this->mMapType = EERMGetNumberOfGenerators() >= 1 ? EERMGetGeneratorName(0) : UWideString(gEmptyString);
    } else {
        this->mMapType = inOther.mMapType;
    }
    this->mCampaignName = inOther.mCampaignName;
    this->mScenarioName = inOther.mScenarioName;
    this->mMapName = inOther.mMapName;
    for (unsigned long i = 0; i < 16; i++)
        this->mPlayers[i] = inOther.mPlayers[i];
    return *this;
}

// Returns the map's width and height in tiles for the map size setting (0 x 0 for an unknown size)
// (gameoptions.yaml map_sizes: 42, 58, 75, 105, 160, 220 in the original)
void EEGameOptions::GetMapDimensions(long& outWidth, long& outHeight)
{
    const std::vector<EEMapSizeRule>& theSizes = EEGetRules().mGameOptions.mMapSizes;
    outWidth = this->mMapSize < theSizes.size() ? theSizes[this->mMapSize].mTiles : 0;
    outHeight = outWidth;
}

// Returns the campaign scenario's map name when the options name a campaign with a checksum and a map, else ""
bool EEGameOptions::GetCampaignMapName(UWideString& outName)
{
    bool theResult = this->mCampaignName.GetLength() > 0 && this->mCampaignChecksum > 0 && this->mMapName.GetLength() > 0;
    outName = theResult ? this->mMapName : UWideString(gEmptyString);
    return theResult;
}

// Returns whether civilizations may be bought: not in the scenario editor and not in a scenario game
bool EEGameOptions::CanBuyCivilization()
{
    return !gUIManager->mInScenarioEditor && !this->mIsScenarioGame;
}

// Resets the options to the lobby's defaults (all resources, random epochs, first map type, cheats on, victory off,
// the preferred difficulty) with sixteen empty player slots
void EEGameOptions::SetDefaults()
{
    *this = gLobbyGameOptions;
    EeAppOptionsLimits theLimits = EEGameOptionsLimits();
    ee_app_options_set_defaults(this, &theLimits, (unsigned long)gDifficultyLevel);
    this->mMapType = EERMGetNumberOfGenerators() > 0 ? EERMGetGeneratorName(0) : UWideString(gEmptyString);
    this->mCampaignName = gEmptyString;
    this->mScenarioName = gEmptyString;
    this->mMapName = gEmptyString;
    for (unsigned long i = 0; i < 16; i++)
        this->mPlayers[i] = sEmptyPlayer;
}

// Reads the game options and the sixteen slots from a saved game (ee_app game_options.rs load); returns the first
// read error
long EEGameOptions::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEGameOptionsFile theContext = {this, &inFile};
    EeAppOptionsPlayerFields* theSlots[16];
    EEGameOptionsSlots(this, theSlots);
    EeAppOptionsLimits theLimits = EEGameOptionsLimits(true);
    return ee_app_options_load(this, theSlots, 16, inMajorVersion, inMinorVersion, &theLimits, &theContext, EEGameOptionsIo);
}

// Writes the sixteen player slots and the game options to a saved game (ee_app game_options.rs save); returns the
// first write error
long EEGameOptions::Save(FSFile& outFile)
{
    EEGameOptionsFile theContext = {this, &outFile};
    EeAppOptionsPlayerFields* theSlots[16];
    EEGameOptionsSlots(this, theSlots);
    return ee_app_options_save(this, theSlots, 16, &theContext, EEGameOptionsIo);
}

// Returns the number of game variants
unsigned long EEGetNumberOfGameVariants()
{
    return gGameVariants.size();
}

// Returns the number of starting resource levels
unsigned long EEGetNumberOfResourceLevels()
{
    return EEStartingResources::sTable.size();
}
