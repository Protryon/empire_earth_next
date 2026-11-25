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
static_assert(offsetof(EEUnitClassUpgrade, mIsLoadingButtons) == offsetof(EEUnitClassUpgrade, mIsLoadingMedia) + 1,
              "the busy flags are adjacent");

// The C++ the media bodies call (ee_units upgrade_media.rs; EE_UNITS_UPGRADE_OP_*)
extern "C" long ee_units_upgrade_host(int32_t inOp, void* inUpgrade, void* inObject, void* inPlayer)
{
    EEUnitClassUpgrade* theUpgrade = static_cast<EEUnitClassUpgrade*>(inUpgrade);
    switch (inOp) {
    case EE_UNITS_UPGRADE_OP_SOUND_LOAD: return static_cast<EESoundObject*>(inObject)->LoadMedia();
    case EE_UNITS_UPGRADE_OP_SOUND_UNLOAD: static_cast<EESoundObject*>(inObject)->UnloadMedia();
        return 0;
    case EE_UNITS_UPGRADE_OP_MODEL_LOAD: static_cast<EEModel*>(inObject)->LoadMedia();
        return 0;
    case EE_UNITS_UPGRADE_OP_MODEL_UNLOAD: static_cast<EEModel*>(inObject)->UnloadMedia();
        return 0;
    case EE_UNITS_UPGRADE_OP_BUTTON_ACQUIRE: static_cast<EEButtonObject*>(inObject)->Acquire();
        return 0;
    case EE_UNITS_UPGRADE_OP_BUTTON_RELEASE: static_cast<EEButtonObject*>(inObject)->Release();
        return 0;
    case EE_UNITS_UPGRADE_OP_TEMPLATE_LOAD: gAEManager.LoadTemplateMedia((unsigned long)(uintptr_t)inObject);
        return 0;
    case EE_UNITS_UPGRADE_OP_TEMPLATE_UNLOAD: gAEManager.UnloadTemplateMedia((unsigned long)(uintptr_t)inObject);
        return 0;
    case EE_UNITS_UPGRADE_OP_CLASS_LOAD_MEDIA: theUpgrade->mUnitClass->LoadMedia(static_cast<EEPlayer*>(inPlayer));
        return 0;
    case EE_UNITS_UPGRADE_OP_CLASS_UNLOAD_MEDIA: theUpgrade->mUnitClass->UnloadMedia(static_cast<EEPlayer*>(inPlayer));
        return 0;
    case EE_UNITS_UPGRADE_OP_CLASS_LOAD_BUTTONS: theUpgrade->mUnitClass->LoadButtons(static_cast<EEPlayer*>(inPlayer));
        return 0;
    case EE_UNITS_UPGRADE_OP_CLASS_UNLOAD_BUTTONS: theUpgrade->mUnitClass->UnloadButtons(static_cast<EEPlayer*>(inPlayer));
        return 0;
    case EE_UNITS_UPGRADE_OP_DATABASE_ENTER: sDatabaseLock.mLock->Enter();
        return 0;
    case EE_UNITS_UPGRADE_OP_DATABASE_LEAVE: sDatabaseLock.mLock->Leave();
        return 0;
    case EE_UNITS_UPGRADE_OP_UPGRADE_ENTER: sUnitClassUpgradeLock.mLock->Enter();
        return 0;
    case EE_UNITS_UPGRADE_OP_UPGRADE_LEAVE: sUnitClassUpgradeLock.mLock->Leave();
        return 0;
    case EE_UNITS_UPGRADE_OP_SLEEP: PlatSleep(1);
        return 0;
    }
    return 0;
}

// Raises another copy's improvements by this copy's levels above 1, moving its stats

// Writes the class name and media count to the dump file when the copy's media is still loaded
void EEUnitClassUpgrade::DumpMediaCount(FSFileSpec& inFile)
{
    if (this->mMediaCount > 0)
        fprintf(inFile, "\t%s has a media count of %ld\n", (const char*)this->mUnitClass->mName, this->mMediaCount);
}

// Returns acknowledge sound inIndex; -1 alternates between the two (one turn shared by every class)
// Returns the copy's improvement levels
// Returns selection sound inIndex; -1 alternates between the two (one turn shared by every class)
// Builds the upgrade button's " + n" help text for an improvement's next level

// Returns an improvement's display name for this unit class

// Raises or lowers an improvement by inDelta levels, moving this copy's stats

// Returns the unit class this copy belongs to (the original folded it with EEComplexDoppelganger::GetTileX, the same
// `mov eax,[ecx+0x60]`; written out so the member is read at this build's offset)
// Resets the copy to its unit class: media, button, stats and inverse speeds, area-effect templates and terrain mask
// (flag 0x4000 added when the class's +0x315 is set), then reapplies the improvement levels
// Reads the copy's name and improvement levels (applied to its stats) from a saved game; returns the first read error

// Loads the class's buttons on the first request; later requests are counted and wait while the first one loads
// On the first request loads the sounds and model chosen by inMediaFlags (dropping sounds that fail), the button,
// the area-effect templates and the class's media; later requests are counted and wait while the first one loads
// Writes the copy's name and improvement levels to the saved game; returns the first write error

// Replaces acknowledge sound inIndex; while media is loaded, unloads the old sound and loads the new one
// Replaces the creation sound; while media is loaded, unloads the old sound and loads the new one
// Replaces the death sound; while media is loaded, unloads the old sound and loads the new one
// Replaces the model; while media is loaded, unloads the old model and loads the new one
// Sets the top speed and scales acceleration and turn rates (turret too) by its ratio to the class's; updates the
// inverses
void EEUnitClassUpgrade::SetMaxVelocity(float inValue)
{
    ee_units_upgrade_set_max_velocity(this, this->mUnitClass, inValue);   // New (R5b.4, round 28): ee_units stats.rs
}

// Replaces the button; while media is loaded, releases the old button and acquires the new one
// Sets the range and the projectile's launch speed to reach it at 45 degrees (a bomb: its top speed; a Roman
// Legionnaire aims for its heal range; no projectile: 0)
void EEUnitClassUpgrade::SetRange(float inValue)
{
    ee_units_upgrade_set_range(this, this->mUnitClass, this->mUnitClass->mProjectileClass, inValue);   // New (R5b.4, round 28): ee_units improve.rs
}

// Replaces selection sound inIndex; while media is loaded, unloads the old sound and loads the new one
// Drops one button request; the last one unloads the class's buttons
// Drops one media request; the last one unloads the sounds, model, button, area-effect templates and class media