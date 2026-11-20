// C:\EEX\Empire Earth\Units\EEUnitClassUpgrade.cpp (file name inferred: no __FILE__; the class name). A player's copy
// of a unit class's stats (EEPlayer::mUnitClassUpgrades): 0x23b61a..0x23c332 (after EEUnitClassImprovement.cpp; EEUnitGoal.cpp
// follows). Unit flags: the exe's module set. Two independent lock chains open it (EEDatabase.h's 0x56445c, then
// this unit's 0x564458: unguarded EEDatabaseLock instances), then the ctor, ??_G, dtor, Virtual04 (with
// GetImprovement's COMDAT), the dump line, the media selectors, the improvement forwarders, Init, Load,
// LoadButtons/LoadMedia (media load), Save, the media setters, SetMaxVelocity (speed), SetRange (range), UnloadButtons/UnloadMedia
// (media unload).
#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <stddef.h>

#include "lle/LowLevelEngine.h"

#include "ee/EEUnit.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEButtonObject.h"
#include "ee/EEAEManager.h"
#include "EEDatabase.h"
#include "platform/PlatTime.h"
#include "rust/sim/units.h"

#if defined(__clang__)
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#endif

// Second unguarded lock chain at 0x23b664; independent technology-media readers use 0x564458.
static EEDatabaseLock sUnitClassUpgradeLock;

// (the sounds' round robins 0x564460, 0x564464 are Rust's: upgrade_media.rs)

// Creates a player's copy of a unit class's stats, unnamed, with no media or buttons loaded
EEUnitClassUpgrade::EEUnitClassUpgrade(EEUnitClass* inUnitClass)
{
    this->mUnitClass = inUnitClass;
    this->mName = "";
    this->mMediaCount = 0;
    this->mButtonLoadCount = 0;
    this->mIsLoadingMedia = false;
    this->mIsLoadingButtons = false;
    this->mTerrainMask = 0;
    this->Init();
}

// port (the groups lane, round 1): the media bodies are Rust (ee_units upgrade_media.rs) over the eight media members
// (mButton .. mSelectionSounds[1], contiguous), the busy flags and the record's counts; each C++ call is one case of
// ee_units_upgrade_host
static_assert(offsetof(EEUnitClassUpgrade, mSelectionSounds) + sizeof(void*) == offsetof(EEUnitClassUpgrade, mButton) + 7 * sizeof(void*),
              "the eight media members are contiguous");
static_assert(offsetof(EEUnitClassUpgrade, mIsLoadingButtons) == offsetof(EEUnitClassUpgrade, mIsLoadingMedia) + 1,
              "the busy flags are adjacent");

static uint64_t EEUpgradeMedia(EEUnitClassUpgrade* inUpgrade, int32_t inOp, long inArg, EEPlayer* inPlayer, void* inValue)
{
    EeUnitsUpgradeMedia theMedia = {reinterpret_cast<void**>(&inUpgrade->mButton), const_cast<bool*>(&inUpgrade->mIsLoadingMedia),
                                    static_cast<EeUnitsUnitClassUpgradeFields*>(inUpgrade), inUpgrade, inPlayer};
    return ee_units_upgrade_media(&theMedia, inOp, inArg, inValue);
}

// The C++ the media bodies call (ee_units upgrade_media.rs; EE_UNITS_UPGRADE_OP_*)
extern "C" long ee_units_upgrade_host(int32_t inOp, void* inUpgrade, void* inObject, void* inPlayer)
{
    EEUnitClassUpgrade* theUpgrade = static_cast<EEUnitClassUpgrade*>(inUpgrade);
    switch (inOp) {
    case EE_UNITS_UPGRADE_OP_SOUND_LOAD: return static_cast<EESoundObject*>(inObject)->LoadMedia();
    case EE_UNITS_UPGRADE_OP_SOUND_UNLOAD: static_cast<EESoundObject*>(inObject)->UnloadMedia(); return 0;
    case EE_UNITS_UPGRADE_OP_MODEL_LOAD: static_cast<EEModel*>(inObject)->LoadMedia(); return 0;
    case EE_UNITS_UPGRADE_OP_MODEL_UNLOAD: static_cast<EEModel*>(inObject)->UnloadMedia(); return 0;
    case EE_UNITS_UPGRADE_OP_BUTTON_ACQUIRE: static_cast<EEButtonObject*>(inObject)->Acquire(); return 0;
    case EE_UNITS_UPGRADE_OP_BUTTON_RELEASE: static_cast<EEButtonObject*>(inObject)->Release(); return 0;
    case EE_UNITS_UPGRADE_OP_TEMPLATE_LOAD: gAEManager.LoadTemplateMedia((unsigned long)(uintptr_t)inObject); return 0;
    case EE_UNITS_UPGRADE_OP_TEMPLATE_UNLOAD: gAEManager.UnloadTemplateMedia((unsigned long)(uintptr_t)inObject); return 0;
    case EE_UNITS_UPGRADE_OP_CLASS_LOAD_MEDIA: theUpgrade->mUnitClass->LoadMedia(static_cast<EEPlayer*>(inPlayer)); return 0;
    case EE_UNITS_UPGRADE_OP_CLASS_UNLOAD_MEDIA: theUpgrade->mUnitClass->UnloadMedia(static_cast<EEPlayer*>(inPlayer)); return 0;
    case EE_UNITS_UPGRADE_OP_CLASS_LOAD_BUTTONS: theUpgrade->mUnitClass->LoadButtons(static_cast<EEPlayer*>(inPlayer)); return 0;
    case EE_UNITS_UPGRADE_OP_CLASS_UNLOAD_BUTTONS: theUpgrade->mUnitClass->UnloadButtons(static_cast<EEPlayer*>(inPlayer)); return 0;
    case EE_UNITS_UPGRADE_OP_DATABASE_ENTER: sDatabaseLock.mLock->Enter(); return 0;
    case EE_UNITS_UPGRADE_OP_DATABASE_LEAVE: sDatabaseLock.mLock->Leave(); return 0;
    case EE_UNITS_UPGRADE_OP_UPGRADE_ENTER: sUnitClassUpgradeLock.mLock->Enter(); return 0;
    case EE_UNITS_UPGRADE_OP_UPGRADE_LEAVE: sUnitClassUpgradeLock.mLock->Leave(); return 0;
    case EE_UNITS_UPGRADE_OP_SLEEP: PlatSleep(1); return 0;
    }
    return 0;
}

// Destroys the copy, unloading its sounds and model while its media is loaded
EEUnitClassUpgrade::~EEUnitClassUpgrade()
{
    EEUpgradeMedia(this, EE_UNITS_UPGRADE_MEDIA_DESTROY, 0, NULL, NULL);
}

// Raises another copy's improvements by this copy's levels above 1, moving its stats
void EEUnitClassUpgrade::CopyUpgradeLevels(EEUnitClassUpgrade* inOther)
{
    this->mImprovement.CopyLevelsTo(inOther, inOther->GetImprovement());
}

// Writes the class name and media count to the dump file when the copy's media is still loaded
void EEUnitClassUpgrade::DumpMediaCount(FSFileSpec& inFile)
{
    UWideString theName;
    if (this->mMediaCount > 0)
        fprintf(inFile, "\t%s has a media count of %ld\n", (const char*)this->mUnitClass->mName, this->mMediaCount);
}

// Returns acknowledge sound inIndex; -1 alternates between the two (one turn shared by every class)
EESoundObject* EEUnitClassUpgrade::GetAcknowledgeSound(long inIndex)
{
    return reinterpret_cast<EESoundObject*>(EEUpgradeMedia(this, EE_UNITS_UPGRADE_MEDIA_ACKNOWLEDGE_SOUND, inIndex, NULL, NULL));
}

// Returns the copy's improvement levels
EEUnitClassImprovement* EEUnitClassUpgrade::GetImprovement()
{
    return &this->mImprovement;
}

// Returns selection sound inIndex; -1 alternates between the two (one turn shared by every class)
EESoundObject* EEUnitClassUpgrade::GetSelectionSound(long inIndex)
{
    return reinterpret_cast<EESoundObject*>(EEUpgradeMedia(this, EE_UNITS_UPGRADE_MEDIA_SELECTION_SOUND, inIndex, NULL, NULL));
}

// Builds the upgrade button's " + n" help text for an improvement's next level
void EEUnitClassUpgrade::GetImprovementGainText(UWideString& outText, long inImprovement, long inLevel)
{
    this->mImprovement.GetGainText(this, inLevel, inImprovement, outText);
}

// Returns an improvement's display name for this unit class
void EEUnitClassUpgrade::GetImprovementName(UWideString& outName, long inImprovement)
{
    this->mImprovement.GetName(this->mUnitClass, inImprovement, outName);
}

// Raises or lowers an improvement by inDelta levels, moving this copy's stats
void EEUnitClassUpgrade::ChangeLevel(long inImprovement, long inDelta)
{
    this->mImprovement.ChangeLevel(this->mUnitClass, this, inImprovement, inDelta);
}

// Returns the unit class this copy belongs to (the original folded it with EEComplexDoppelganger::GetTileX, the same
// `mov eax,[ecx+0x60]`; written out so the member is read at this build's offset)
EEUnitClass* EEUnitClassUpgrade::GetUnitClass()
{
    return this->mUnitClass;
}

// Resets the copy to its unit class: media, button, stats and inverse speeds, area-effect templates and terrain mask
// (flag 0x4000 added when the class's +0x315 is set), then reapplies the improvement levels
void EEUnitClassUpgrade::Init()
{
    long i;
    this->mButton = this->mUnitClass->mButton;
    this->SetModel(this->mUnitClass->mModel);
    for (i = 0; i < 2; i++)
        this->SetAcknowledgeSound(this->mUnitClass->mAcknowledgeSounds[i], i);
    this->SetCreationSound(this->mUnitClass->mCreationSound);
    this->SetDeathSound(this->mUnitClass->mDeathSound);
    for (i = 0; i < 2; i++)
        this->SetSelectionSound(this->mUnitClass->mSelectionSounds[i], i);
    // New (R5b.4, round 28): the copies from the class's record are Rust (ee_units stats.rs init, init_finish), the
    // original's statements in order around SetRange (which writes none of them) and Reapply
    this->SetRange(this->mUnitClass->mRange);
    ee_units_upgrade_init(this, this->mUnitClass);
    this->mImprovement.Reapply(this->mUnitClass, this);
    ee_units_upgrade_init_finish(this, this->mUnitClass);
}

// Reads the copy's name and improvement levels (applied to its stats) from a saved game; returns the first read error
long EEUnitClassUpgrade::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = this->mName.Load(inFile);
    if (theResult == 0)
        theResult = this->mImprovement.Load(this->mUnitClass, this, inMajorVersion, inMinorVersion, inFile);
    return theResult;
}

// Loads the class's buttons on the first request; later requests are counted and wait while the first one loads
void EEUnitClassUpgrade::LoadButtons(EEPlayer* inPlayer)
{
    EEUpgradeMedia(this, EE_UNITS_UPGRADE_MEDIA_LOAD_BUTTONS, 0, inPlayer, NULL);
}

// On the first request loads the sounds and model chosen by inMediaFlags (dropping sounds that fail), the button,
// the area-effect templates and the class's media; later requests are counted and wait while the first one loads
void EEUnitClassUpgrade::LoadMedia(EEPlayer* inPlayer, long inMediaFlags)
{
    EEUpgradeMedia(this, EE_UNITS_UPGRADE_MEDIA_LOAD, inMediaFlags, inPlayer, NULL);
}

// Writes the copy's name and improvement levels to the saved game; returns the first write error
long EEUnitClassUpgrade::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = this->mName.Save(outFile);
    if (theResult == 0)
        theResult = this->mImprovement.Save(inMajorVersion, inMinorVersion, outFile);
    return theResult;
}

// Replaces acknowledge sound inIndex; while media is loaded, unloads the old sound and loads the new one
void EEUnitClassUpgrade::SetAcknowledgeSound(EESoundObject* inSound, long inIndex)
{
    EEUpgradeMedia(this, EE_UNITS_UPGRADE_MEDIA_SET, EE_UNITS_MEDIA_ACKNOWLEDGE + inIndex, NULL, inSound);
}

// Replaces the creation sound; while media is loaded, unloads the old sound and loads the new one
void EEUnitClassUpgrade::SetCreationSound(EESoundObject* inSound)
{
    EEUpgradeMedia(this, EE_UNITS_UPGRADE_MEDIA_SET, EE_UNITS_MEDIA_CREATION, NULL, inSound);
}

// Replaces the death sound; while media is loaded, unloads the old sound and loads the new one
void EEUnitClassUpgrade::SetDeathSound(EESoundObject* inSound)
{
    EEUpgradeMedia(this, EE_UNITS_UPGRADE_MEDIA_SET, EE_UNITS_MEDIA_DEATH, NULL, inSound);
}

// Replaces the model; while media is loaded, unloads the old model and loads the new one
void EEUnitClassUpgrade::SetModel(EEModel* inModel)
{
    EEUpgradeMedia(this, EE_UNITS_UPGRADE_MEDIA_SET, EE_UNITS_MEDIA_MODEL, NULL, inModel);
}

// Sets the top speed and scales acceleration and turn rates (turret too) by its ratio to the class's; updates the
// inverses
void EEUnitClassUpgrade::SetMaxVelocity(float inValue)
{
    ee_units_upgrade_set_max_velocity(this, this->mUnitClass, inValue);   // New (R5b.4, round 28): ee_units stats.rs
}

// Replaces the button; while media is loaded, releases the old button and acquires the new one
void EEUnitClassUpgrade::SetButton(EEButtonObject* inButton)
{
    EEUpgradeMedia(this, EE_UNITS_UPGRADE_MEDIA_SET, EE_UNITS_MEDIA_BUTTON, NULL, inButton);
}

// Sets the range and the projectile's launch speed to reach it at 45 degrees (a bomb: its top speed; a Roman
// Legionnaire aims for its heal range; no projectile: 0)
void EEUnitClassUpgrade::SetRange(float inValue)
{
    ee_units_upgrade_set_range(this, this->mUnitClass, this->mUnitClass->mProjectileClass, inValue);   // New (R5b.4, round 28): ee_units improve.rs
}

// Replaces selection sound inIndex; while media is loaded, unloads the old sound and loads the new one
void EEUnitClassUpgrade::SetSelectionSound(EESoundObject* inSound, long inIndex)
{
    EEUpgradeMedia(this, EE_UNITS_UPGRADE_MEDIA_SET, EE_UNITS_MEDIA_SELECTION + inIndex, NULL, inSound);
}

// Drops one button request; the last one unloads the class's buttons
void EEUnitClassUpgrade::UnloadButtons(EEPlayer* inPlayer)
{
    EEUpgradeMedia(this, EE_UNITS_UPGRADE_MEDIA_UNLOAD_BUTTONS, 0, inPlayer, NULL);
}

// Drops one media request; the last one unloads the sounds, model, button, area-effect templates and class media
void EEUnitClassUpgrade::UnloadMedia(EEPlayer* inPlayer)
{
    EEUpgradeMedia(this, EE_UNITS_UPGRADE_MEDIA_UNLOAD, 0, inPlayer, NULL);
}
