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
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFileSpec.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"

#include "ee/EmpireEarth.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEButtonObject.h"
#include "ee/EEAEManager.h"
#include "EEDatabase.h"

// Second unguarded lock chain at 0x23b664; independent technology-media readers use 0x564458.
static EEDatabaseLock sUnitClassUpgradeLock;

// FUNCTION: EXE 0x63b61a
// FUNCTION: EXE 0x63b624
// FUNCTION: EXE 0x63b64a
// FUNCTION: EXE 0x63b656
// (EEDatabase.h's sDatabaseLock: wrapper, ctor, registrar, dtor)

// FUNCTION: EXE 0x63b664
// The second unguarded EEDatabaseLock chain (0x564458): generated wrapper (10 bytes).
// FUNCTION: EXE 0x63b66e
// Generated ctor (38 bytes).
// FUNCTION: EXE 0x63b694
// Generated registrar (12 bytes).
// FUNCTION: EXE 0x63b6a0
// Generated dtor (14 bytes).

static unsigned long sModel74Index;         // 0x564460 (GetAcknowledgeSound's round robin; provisional name)
static unsigned long sModel7CIndex;         // 0x564464 (GetSelectionSound's; provisional name)

// FUNCTION: EXE 0x63b6ae
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

// SYNTHETIC: EXE 0x63b70d
// (EEUnitClassUpgrade's scalar deleting destructor)

// FUNCTION: EXE 0x63b729
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

// FUNCTION: EXE 0x63b7a4
// Raises another copy's improvements by this copy's levels above 1, moving its stats
void EEUnitClassUpgrade::CopyUpgradeLevels(EEUnitClassUpgrade* inOther)
{
    this->mImprovement.CopyLevelsTo(inOther, inOther->GetImprovement());
}

// FUNCTION: EXE 0x63b7c1
// Writes the class name and media count to the dump file when the copy's media is still loaded
void EEUnitClassUpgrade::DumpMediaCount(FSFileSpec& inFile)
{
    UWideString theName;
    if (this->mMediaCount > 0)
        fprintf(inFile, "\t%s has a media count of %d\n", (const char*)this->mUnitClass->mName, this->mMediaCount);
}

// FUNCTION: EXE 0x63b806
// Returns acknowledge sound inIndex; -1 alternates between the two (one turn shared by every class)
EESoundObject* EEUnitClassUpgrade::GetAcknowledgeSound(long inIndex)
{
    if (inIndex == -1) {
        inIndex = sModel74Index;
        sModel74Index = (inIndex - 1) & 1;
    }
    return this->mAcknowledgeSounds[inIndex];
}

// FUNCTION: EXE 0x63b827
// Returns the copy's improvement levels
EEUnitClassImprovement* EEUnitClassUpgrade::GetImprovement()
{
    return &this->mImprovement;
}

// FUNCTION: EXE 0x63b82b
// Returns selection sound inIndex; -1 alternates between the two (one turn shared by every class)
EESoundObject* EEUnitClassUpgrade::GetSelectionSound(long inIndex)
{
    if (inIndex == -1) {
        inIndex = sModel7CIndex;
        sModel7CIndex = (inIndex - 1) & 1;
    }
    return this->mSelectionSounds[inIndex];
}

// FUNCTION: EXE 0x63b84c
// Builds the upgrade button's " + n" help text for an improvement's next level
void EEUnitClassUpgrade::GetImprovementGainText(UWideString& outText, long inImprovement, long inLevel)
{
    this->mImprovement.GetGainText(this, inLevel, inImprovement, outText);
}

// FUNCTION: EXE 0x63b864
// Returns an improvement's display name for this unit class
void EEUnitClassUpgrade::GetImprovementName(UWideString& outName, long inImprovement)
{
    this->mImprovement.GetName(this->mUnitClass, inImprovement, outName);
}

// FUNCTION: EXE 0x63b87a
// Raises or lowers an improvement by inDelta levels, moving this copy's stats
void EEUnitClassUpgrade::ChangeLevel(long inImprovement, long inDelta)
{
    this->mImprovement.ChangeLevel(this->mUnitClass, this, inImprovement, inDelta);
}

// ICF-FOLDED: EXE 0x5f1f81
// Returns the unit class this copy belongs to (the original folded it with EEComplexDoppelganger::GetTileX, the same
// `mov eax,[ecx+0x60]`; written out so the member is read at this build's offset)
EEUnitClass* EEUnitClassUpgrade::GetUnitClass()
{
    return this->mUnitClass;
}

// FUNCTION: EXE 0x63b891
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
    for (i = 0; i < 7; i++)
        this->mGatherTimes[i] = this->mUnitClass->mGatherTimes[i];
    this->mScale = this->mUnitClass->GetScale();
    this->SetRange(this->mUnitClass->mRange);
    this->mAreaEffect = this->mUnitClass->GetAreaEffect();
    this->mLineOfSight = this->mUnitClass->GetLineOfSight();
    this->mAcceleration = this->mUnitClass->GetAcceleration();
    this->mMaxVelocity = this->mUnitClass->GetMaxVelocity();
    this->mAngularAcceleration = this->mUnitClass->GetAngularAcceleration();
    this->mMaxAngularVelocity = this->mUnitClass->GetMaxAngularVelocity();
    this->mTurretMaxAngularVelocity = this->mUnitClass->GetTurretMaxAngularVelocity();
    // the class's armour array (+0x1f0..+0x204) copied in a loop, as the original
    for (i = 0; i < 6; i++)
        this->mArmor[i] = (float)this->mUnitClass->mArmor[i];
    this->mReloadTime = this->mUnitClass->mReloadTime;
    this->mAttack = (float)this->mUnitClass->mAttack;
    this->mWorkRate = this->mUnitClass->mWorkRate;
    this->mFlightTime = this->mUnitClass->mFlightTime;
    this->mPowerRegeneration = this->mUnitClass->mPowerRegeneration;
    this->mLeadership = (float)this->mUnitClass->mLeadership;
    this->mMaxObjectCarry = this->mUnitClass->mMaxObjectCarry;
    this->mRepairRate = this->mUnitClass->mRepairRate;
    this->mMaxHitPoints = this->mUnitClass->mHitPoints;
    this->mCarryAmount = this->mUnitClass->mCarryAmount;
    this->mHealingPoints = this->mUnitClass->mHealingPoints;
    this->mCanFireOverWalls = this->mUnitClass->mCanFireOverWalls;
    this->mBuildsOnElevation = this->mUnitClass->mBuildsOnElevation;
    this->mConvertsBuildings = this->mUnitClass->mConvertsBuildings;
    this->mConvertsPriests = this->mUnitClass->mConvertsPriests;
    this->mConversionResistance = this->mUnitClass->GetConversionResistance();
    this->mConversionRate = this->mUnitClass->GetConversionRate();
    this->mCostScale = this->mUnitClass->mCostScale;
    this->mBuildTimeScale = this->mUnitClass->mBuildTimeScale;
    this->mAreaEffectScale = this->mUnitClass->mAreaEffectScale;
    this->mPower = this->mUnitClass->mPower;
    this->mRegeneration = this->mUnitClass->mRegeneration;
    if (this->mAcceleration < this->mMaxVelocity)
        this->mBC = this->mAcceleration;
    else
        this->mBC = this->mMaxVelocity;
    this->mInverseAcceleration = 1.0f / this->mAcceleration;
    this->mInverseAngularAcceleration = 1.0f / this->mAngularAcceleration;
    this->mInverseMaxAngularVelocity = 1.0f / this->mMaxAngularVelocity;
    this->mInverseMaxVelocity = 1.0f / this->mMaxVelocity;
    this->mInverseTurretMaxAngularVelocity = 1.0f / this->mTurretMaxAngularVelocity;
    this->mImprovement.Reapply(this->mUnitClass, this);
    // the four area-effect templates (+0xfc / the class's +0x2bc) copied in a loop, as the original
    for (i = 0; i < 4; i++)
        this->mAreaEffectTemplates[i] = this->mUnitClass->mAreaEffectTemplates[i];
    this->mWonderUnitSet = this->mUnitClass->mWonderUnitSet;
    this->mIsInitialized = true;
    this->mCameraFocus = 0.5f;              // original +0xf8 IEEE float 0x3f000000
    this->mTerrainMask = 0;
    for (unsigned long j = 0; j < 0x11; j++) {
        if (!this->mUnitClass->mTerrainTypes[j])
            continue;
        unsigned long theBit = 1 << j;
        if ((theBit & this->mTerrainMask) == 0)
            this->mTerrainMask |= theBit;
    }
    if (this->mUnitClass->mCanCrossForest && ((this->mTerrainMask >> 0xe) & 1) == 0)
        this->mTerrainMask |= 0x4000;
}

// FUNCTION: EXE 0x63bbf9
// Reads the copy's name and improvement levels (applied to its stats) from a saved game; returns the first read error
long EEUnitClassUpgrade::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = this->mName.Load(inFile);
    if (theResult == 0)
        theResult = this->mImprovement.Load(this->mUnitClass, this, inMajorVersion, inMinorVersion, inFile);
    return theResult;
}

// FUNCTION: EXE 0x63bc29
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

// FUNCTION: EXE 0x63bcc3
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

// FUNCTION: EXE 0x63be43
// Writes the copy's name and improvement levels to the saved game; returns the first write error
long EEUnitClassUpgrade::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = this->mName.Save(outFile);
    if (theResult == 0)
        theResult = this->mImprovement.Save(inMajorVersion, inMinorVersion, outFile);
    return theResult;
}

// FUNCTION: EXE 0x63be6f
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

// FUNCTION: EXE 0x63bed9
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

// FUNCTION: EXE 0x63bf3d
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

// FUNCTION: EXE 0x63bfa1
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

// FUNCTION: EXE 0x63c005
// Sets the top speed and scales acceleration and turn rates (turret too) by its ratio to the class's; updates the
// inverses
void EEUnitClassUpgrade::SetMaxVelocity(float inValue)
{
    float theRatio = inValue / this->mUnitClass->GetMaxVelocity();
    this->mMaxVelocity = inValue;
    this->mAcceleration = this->mUnitClass->GetAcceleration() * theRatio;
    this->mAngularAcceleration = this->mUnitClass->GetAngularAcceleration() * theRatio;
    this->mMaxAngularVelocity = this->mUnitClass->GetMaxAngularVelocity() * theRatio;
    this->mTurretMaxAngularVelocity = this->mUnitClass->GetTurretMaxAngularVelocity() * theRatio;
    this->mInverseAcceleration = 1.0f / this->mAcceleration;
    this->mInverseAngularAcceleration = 1.0f / this->mAngularAcceleration;
    this->mInverseMaxAngularVelocity = 1.0f / this->mMaxAngularVelocity;
    this->mInverseMaxVelocity = 1.0f / inValue;
    this->mInverseTurretMaxAngularVelocity = 1.0f / this->mTurretMaxAngularVelocity;
}

// FUNCTION: EXE 0x63c09d
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

// FUNCTION: EXE 0x63c101
// Sets the range and the projectile's launch speed to reach it at 45 degrees (a bomb: its top speed; a Roman
// Legionnaire aims for its heal range; no projectile: 0)
void EEUnitClassUpgrade::SetRange(float inValue)
{
    float theCosine = UMCos(0.78539819f);
    this->mRange = inValue;
    EEUnitClass* theProjectile = this->mUnitClass->mProjectileClass;
    if (theProjectile != NULL) {
        if (theProjectile->mCategory == kUnitCategoryBomb)
            this->mLaunchSpeed = theProjectile->GetMaxVelocity();
        else {
            if (this->mUnitClass->mUnitType == kUnitTypeX04RomanLegionnaire)
                inValue = this->mUnitClass->mHealRange;
            this->mLaunchSpeed = (float)sqrt(theProjectile->GetAcceleration() * inValue / (theCosine * theCosine * 2.0f));
        }
    } else
        this->mLaunchSpeed = 0.0f;
}

// FUNCTION: EXE 0x63c185
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

// FUNCTION: EXE 0x63c1ef
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

// FUNCTION: EXE 0x63c24f
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
