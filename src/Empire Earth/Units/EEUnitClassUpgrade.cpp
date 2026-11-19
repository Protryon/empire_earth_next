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

#include "lle/LowLevelEngine.h"

#include "ee/EEUnit.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEButtonObject.h"
#include "ee/EEAEManager.h"
#include "EEDatabase.h"

// Second unguarded lock chain at 0x23b664; independent technology-media readers use 0x564458.
static EEDatabaseLock sUnitClassUpgradeLock;

static unsigned long sModel74Index;         // 0x564460 (GetAcknowledgeSound's round robin; provisional name)
static unsigned long sModel7CIndex;         // 0x564464 (GetSelectionSound's; provisional name)

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

// Destroys the copy, unloading its sounds and model while its media is loaded
EEUnitClassUpgrade::~EEUnitClassUpgrade()
{
    if (this->mMediaCount > 0) {
        long i;
        for (i = 0; i < 2; i++) {
            if (this->mAcknowledgeSounds[i] != NULL)
                this->mAcknowledgeSounds[i]->UnloadMedia();
        }
        for (i = 0; i < 2; i++) {
            if (this->mSelectionSounds[i] != NULL)
                this->mSelectionSounds[i]->UnloadMedia();
        }
        if (this->mDeathSound != NULL)
            this->mDeathSound->UnloadMedia();
        if (this->mCreationSound != NULL)
            this->mCreationSound->UnloadMedia();
        if (this->mModel != NULL)
            this->mModel->UnloadMedia();
    }
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
    if (inIndex == -1) {
        inIndex = sModel74Index;
        sModel74Index = (inIndex - 1) & 1;
    }
    return this->mAcknowledgeSounds[inIndex];
}

// Returns the copy's improvement levels
EEUnitClassImprovement* EEUnitClassUpgrade::GetImprovement()
{
    return &this->mImprovement;
}

// Returns selection sound inIndex; -1 alternates between the two (one turn shared by every class)
EESoundObject* EEUnitClassUpgrade::GetSelectionSound(long inIndex)
{
    if (inIndex == -1) {
        inIndex = sModel7CIndex;
        sModel7CIndex = (inIndex - 1) & 1;
    }
    return this->mSelectionSounds[inIndex];
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
    sUnitClassUpgradeLock.mLock->Enter();
    if (this->mButtonLoadCount == 0) {
        this->mButtonLoadCount = 1;
        this->mIsLoadingButtons = true;
        sUnitClassUpgradeLock.mLock->Leave();
        this->mUnitClass->LoadButtons(inPlayer);
        sUnitClassUpgradeLock.mLock->Enter();
        this->mIsLoadingButtons = false;
    } else {
        this->mButtonLoadCount++;
        sUnitClassUpgradeLock.mLock->Leave();
        while (this->mIsLoadingButtons == true)
            PlatSleep(1);
        sUnitClassUpgradeLock.mLock->Enter();
    }
    sUnitClassUpgradeLock.mLock->Leave();
}

// On the first request loads the sounds and model chosen by inMediaFlags (dropping sounds that fail), the button,
// the area-effect templates and the class's media; later requests are counted and wait while the first one loads
void EEUnitClassUpgrade::LoadMedia(EEPlayer* inPlayer, long inMediaFlags)
{
    sDatabaseLock.mLock->Enter();
    if (this->mMediaCount == 0) {
        this->mMediaCount = 1;
        this->mIsLoadingMedia = true;
        sDatabaseLock.mLock->Leave();
        if ((inMediaFlags & kMediaAcknowledgeSound1) != 0 && this->mAcknowledgeSounds[0] != NULL && this->mAcknowledgeSounds[0]->LoadMedia() != 0)
            this->mAcknowledgeSounds[0] = NULL;
        if ((inMediaFlags & kMediaAcknowledgeSound2) != 0 && this->mAcknowledgeSounds[1] != NULL && this->mAcknowledgeSounds[1]->LoadMedia() != 0)
            this->mAcknowledgeSounds[1] = NULL;
        if ((inMediaFlags & kMediaSelectionSound1) != 0 && this->mSelectionSounds[0] != NULL && this->mSelectionSounds[0]->LoadMedia() != 0)
            this->mSelectionSounds[0] = NULL;
        if ((inMediaFlags & kMediaSelectionSound2) != 0 && this->mSelectionSounds[1] != NULL && this->mSelectionSounds[1]->LoadMedia() != 0)
            this->mSelectionSounds[1] = NULL;
        if ((inMediaFlags & kMediaDeathSound) != 0 && this->mDeathSound != NULL && this->mDeathSound->LoadMedia() != 0)
            this->mDeathSound = NULL;
        if ((inMediaFlags & kMediaCreationSound) != 0 && this->mCreationSound != NULL && this->mCreationSound->LoadMedia() != 0)
            this->mCreationSound = NULL;
        if ((inMediaFlags & kMediaModel) != 0 && this->mModel != NULL)
            this->mModel->LoadMedia();
        this->mButton->Acquire();
        for (long i = 0; i < 4; i++)
            gAEManager.LoadTemplateMedia(this->mAreaEffectTemplates[i]);
        this->mUnitClass->LoadMedia(inPlayer);
        sDatabaseLock.mLock->Enter();
        this->mIsLoadingMedia = false;
    } else {
        this->mMediaCount++;
        sDatabaseLock.mLock->Leave();
        while (this->mIsLoadingMedia == true)
            PlatSleep(1);
        sDatabaseLock.mLock->Enter();
    }
    sDatabaseLock.mLock->Leave();
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
    while (this->mIsLoadingMedia == true)
        PlatSleep(1);
    sDatabaseLock.mLock->Enter();
    if (this->mMediaCount > 0 && this->mAcknowledgeSounds[inIndex] != NULL)
        this->mAcknowledgeSounds[inIndex]->UnloadMedia();
    this->mAcknowledgeSounds[inIndex] = inSound;
    if (this->mMediaCount > 0 && inSound != NULL)
        inSound->LoadMedia();
    sDatabaseLock.mLock->Leave();
}

// Replaces the creation sound; while media is loaded, unloads the old sound and loads the new one
void EEUnitClassUpgrade::SetCreationSound(EESoundObject* inSound)
{
    while (this->mIsLoadingMedia == true)
        PlatSleep(1);
    sDatabaseLock.mLock->Enter();
    if (this->mMediaCount > 0 && this->mCreationSound != NULL)
        this->mCreationSound->UnloadMedia();
    this->mCreationSound = inSound;
    if (this->mMediaCount > 0 && inSound != NULL)
        inSound->LoadMedia();
    sDatabaseLock.mLock->Leave();
}

// Replaces the death sound; while media is loaded, unloads the old sound and loads the new one
void EEUnitClassUpgrade::SetDeathSound(EESoundObject* inSound)
{
    while (this->mIsLoadingMedia == true)
        PlatSleep(1);
    sDatabaseLock.mLock->Enter();
    if (this->mMediaCount > 0 && this->mDeathSound != NULL)
        this->mDeathSound->UnloadMedia();
    this->mDeathSound = inSound;
    if (this->mMediaCount > 0 && inSound != NULL)
        inSound->LoadMedia();
    sDatabaseLock.mLock->Leave();
}

// Replaces the model; while media is loaded, unloads the old model and loads the new one
void EEUnitClassUpgrade::SetModel(EEModel* inModel)
{
    while (this->mIsLoadingMedia == true)
        PlatSleep(1);
    sDatabaseLock.mLock->Enter();
    if (this->mMediaCount > 0 && this->mModel != NULL)
        this->mModel->UnloadMedia();
    this->mModel = inModel;
    if (this->mMediaCount > 0 && inModel != NULL)
        inModel->LoadMedia();
    sDatabaseLock.mLock->Leave();
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
    while (this->mIsLoadingMedia == true)
        PlatSleep(1);
    sDatabaseLock.mLock->Enter();
    if (this->mMediaCount > 0 && this->mButton != NULL)
        this->mButton->Release();
    this->mButton = inButton;
    if (this->mMediaCount > 0 && inButton != NULL)
        inButton->Acquire();
    sDatabaseLock.mLock->Leave();
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
    while (this->mIsLoadingMedia == true)
        PlatSleep(1);
    sDatabaseLock.mLock->Enter();
    if (this->mMediaCount > 0 && this->mSelectionSounds[inIndex] != NULL)
        this->mSelectionSounds[inIndex]->UnloadMedia();
    this->mSelectionSounds[inIndex] = inSound;
    if (this->mMediaCount > 0 && inSound != NULL)
        inSound->LoadMedia();
    sDatabaseLock.mLock->Leave();
}

// Drops one button request; the last one unloads the class's buttons
void EEUnitClassUpgrade::UnloadButtons(EEPlayer* inPlayer)
{
    while (this->mIsLoadingButtons == true)
        PlatSleep(1);
    sUnitClassUpgradeLock.mLock->Enter();
    if (this->mButtonLoadCount == 1) {
        this->mButtonLoadCount = 0;
        this->mUnitClass->UnloadButtons(inPlayer);
    } else
        this->mButtonLoadCount--;
    sUnitClassUpgradeLock.mLock->Leave();
}

// Drops one media request; the last one unloads the sounds, model, button, area-effect templates and class media
void EEUnitClassUpgrade::UnloadMedia(EEPlayer* inPlayer)
{
    while (this->mIsLoadingMedia == true)
        PlatSleep(1);
    sDatabaseLock.mLock->Enter();
    if (this->mMediaCount != 0) {
        if (this->mMediaCount == 1) {
            this->mMediaCount = 0;
            long i;
            for (i = 0; i < 2; i++) {
                if (this->mAcknowledgeSounds[i] != NULL)
                    this->mAcknowledgeSounds[i]->UnloadMedia();
            }
            for (i = 0; i < 2; i++) {
                if (this->mSelectionSounds[i] != NULL)
                    this->mSelectionSounds[i]->UnloadMedia();
            }
            if (this->mDeathSound != NULL)
                this->mDeathSound->UnloadMedia();
            if (this->mCreationSound != NULL)
                this->mCreationSound->UnloadMedia();
            if (this->mModel != NULL)
                this->mModel->UnloadMedia();
            this->mButton->Release();
            for (i = 0; i < 4; i++)
                gAEManager.UnloadTemplateMedia(this->mAreaEffectTemplates[i]);
            this->mUnitClass->UnloadMedia(inPlayer);
        } else
            this->mMediaCount--;
    }
    sDatabaseLock.mLock->Leave();
}
