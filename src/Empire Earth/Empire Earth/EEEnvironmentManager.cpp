// C:\EEX\Empire Earth\Empire Earth\EEEnvironmentManager.cpp (name from RTTI EEEnvironmentManager; no __FILE__).
// 0x12ed4e..0x12f4b2: the weather manager (ctor, ??_G, dtor, the storm test, CreateDefault, the update, Load,
// RepairPointers, Save, the effect switch, the storm list, the wind). EEEMStorm.cpp ends at 0x12ed4e, EEGameOptions.cpp
// starts at 0x12f4b2. Unit flags: the exe's module set (provisional names).
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/FSFile.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEEMStorm.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEPersistentTag.h"
#include "ee/EEStormType.h"

// the wind's limits (unit-local .rdata; the first two sit before the vtable, the step limit after it)
static const float kWindStepMaximum = 1.3333333e-06f;
static const float kWindMinimum = 1.25e-06f;
static const float kWindMaximum = 1.4285714e-06f;

// FUNCTION: EXE 0x52ed4e
// Creates the weather manager, disabled, with no storms, calm wind and at most three storms at a time
EEEnvironmentManager::EEEnvironmentManager()
    : FSPersistent(kPersistentEnvironment), mState(0)
{
    this->mWind = kWindMinimum;
    this->mWindTarget = kWindMinimum;
    this->mWindRate = kWindStepMaximum;
    this->mNextStep = 0;
    this->mIsEnabled = false;
    this->mInterval = 5000;
    this->mMaxStorms = 3;
    this->mF40 = 0.25f;
    this->mF44 = 0.75f;
}

// Compiler-generated: the ??_G of the virtual dtor (calls it out of line):
// TEMPLATE: EXE 0x52ede0

// FUNCTION: EXE 0x52edfc
// Destroys the weather manager, releasing and deleting every storm
EEEnvironmentManager::~EEEnvironmentManager()
{
    this->DeleteStorms();
}

// FUNCTION: EXE 0x52ee1b
// Returns whether a tile lies inside a storm: rain or hurricane storms and/or snow or blizzard storms, as asked
bool EEEnvironmentManager::IsInStorm(const U2DPoint<long>& inPoint, bool inTypes13, bool inTypes24)
{
    bool theResult = false;
    for (long i = this->mStorms.size() - 1; i >= 0 && !theResult; i--) {
        if (inTypes13) {
            if (this->mStorms[i]->GetType() == kStormRain || this->mStorms[i]->GetType() == kStormHurricane)
                theResult = this->mStorms[i]->Contains(inPoint);
        }
        if (inTypes24) {
            if (this->mStorms[i]->GetType() == kStormSnow || this->mStorms[i]->GetType() == kStormBlizzard)
                theResult = this->mStorms[i]->Contains(inPoint);
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x52ef03
// Creates a new default weather manager (the persistent-object factory)
FSPersistent* EEEnvironmentManager::CreateDefault() const
{
    return new EEEnvironmentManager;
}

// FUNCTION: EXE 0x52ef1a
// Advances the storms and the wind, clears a finished state and schedules the next weather step by game speed
void EEEnvironmentManager::Step(unsigned long inTime)
{
    float theTimeOfDay = gWorld.GetTimeOfDay();
    this->UpdateStorms(theTimeOfDay, inTime);
    this->UpdateWind(theTimeOfDay, inTime);
    if (this->mState == 2)
        this->mState = 0;
    this->mNextStep = UMFloatToLong(gWorld.GetGameSpeed() * this->mInterval) + inTime;
}

// FUNCTION: EXE 0x52ef75
// Reads the storms, state, wind and timing from a saved game; returns the first read error (it used to return 0
// whatever the reads returned)
long EEEnvironmentManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theCount = 0;
    long theResult = inFile.Read(theCount);
    if (theResult != kErrorOK)
        return theResult;
    this->mStorms.resize(theCount, 0);
    for (unsigned long i = 0; i < theCount; i++) {
        this->mStorms[i] = new EEEMStorm(kStormUnknown, this);
        if (theResult == kErrorOK)
            theResult = this->mStorms[i]->Load(inMajorVersion, inMinorVersion, inFile);
    }
    if (theResult != kErrorOK)
        return theResult;
    if ((theResult = inFile.Read(this->mState)) == 0 && (theResult = inFile.Read(this->mWind)) == 0
        && (theResult = inFile.Read(this->mWindTarget)) == 0 && (theResult = inFile.Read(this->mWindRate)) == 0
        && (theResult = inFile.Read(this->mNextStep)) == 0 && (theResult = inFile.Read(this->mInterval)) == 0
        && (theResult = this->mC24.Load(inFile)) == 0 && (theResult = this->mC18.Load(inFile)) == 0)
        theResult = inFile.Read(this->mL50);
    return theResult;
}

// FUNCTION: EXE 0x52f083
// Reconnects the loaded storms after a load, deleting the ones that fail to restore; returns 0
long EEEnvironmentManager::RepairPointers(unsigned long inFirst, unsigned long inSecond)
{
    for (long i = this->mStorms.size() - 1; i >= 0; i--) {
        if (this->mStorms[i]->Restore() != 0) {
            delete this->mStorms[i];
            this->mStorms.erase(this->mStorms.begin() + i);
        }
    }
    return 0;
}

// FUNCTION: EXE 0x52f0ff
// Writes the storms, state, wind and timing to a saved game; returns the first write error (it used to return
// success whatever the writes returned)
long EEEnvironmentManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theCount = this->mStorms.size();
    long theResult = outFile.Write(theCount);
    if (theResult != 0)
        return theResult;
    for (unsigned long i = 0; i < theCount; i++) {
        if (theResult == 0) {
            EEEMStorm*& theStorm = this->mStorms[i];
            theResult = theStorm->Save(inMajorVersion, inMinorVersion, outFile);
        }
    }
    if (theResult != 0)
        return theResult;
    if ((theResult = outFile.Write(this->mState)) == 0 && (theResult = outFile.Write(this->mWind)) == 0
        && (theResult = outFile.Write(this->mWindTarget)) == 0 && (theResult = outFile.Write(this->mWindRate)) == 0
        && (theResult = outFile.Write(this->mNextStep)) == 0 && (theResult = outFile.Write(this->mInterval)) == 0
        && (theResult = this->mC24.Save(outFile)) == 0 && (theResult = this->mC18.Save(outFile)) == 0)
        theResult = outFile.Write(this->mL50);
    return theResult;
}

// FUNCTION: EXE 0x52f1fb
// Turns the weather on or off: off deletes every storm and unloads the rain and snow effects, on loads them
void EEEnvironmentManager::SetEnabled(bool inEnabled)
{
    if (this->mIsEnabled == inEnabled)
        return;
    this->mIsEnabled = inEnabled;
    if (!inEnabled) {
        this->DeleteStorms();
        EEGraphicEffect::Get(kGraphicEffectRain)->Unload();
        EEGraphicEffect::Get(kGraphicEffectSnow)->Unload();
    } else {
        EEGraphicEffect::Get(kGraphicEffectRain)->Load(false);
        EEGraphicEffect::Get(kGraphicEffectSnow)->Load(false);
    }
}

// FUNCTION: EXE 0x52f25b
// Releases and deletes every storm and empties the storm list
void EEEnvironmentManager::DeleteStorms()
{
    for (long i = this->mStorms.size() - 1; i >= 0; i--) {
        this->mStorms[i]->Release();
        delete this->mStorms[i];
    }
    this->mStorms.clear();
}

// FUNCTION: EXE 0x52f2a6
// Steps every storm and deletes the ones that are over; then rolls for a new storm below the limit but never creates
// one (the roll and the rain effect lookup have no further effect)
void EEEnvironmentManager::UpdateStorms(float inTimeOfDay, unsigned long inTime)
{
    for (long i = this->mStorms.size() - 1; i >= 0; i--) {
        this->mStorms[i]->Step(inTime);
        if (!(*(this->mStorms.begin() + i))->IsOver())
            continue;
        EEEMStorm* theStorm = *(this->mStorms.begin() + i);
        theStorm->Release();
        this->mStorms.erase(this->mStorms.begin() + i);
        delete theStorm;
    }
    if (this->mStorms.size() < (unsigned long)this->mMaxStorms && gPhysics->GetRandom(0.0001f, 1.0f) <= 0.2f) {
        gPhysics->mRandom.Generate();
        EEGraphicEffect* theEffect = EEGraphicEffect::Get(kGraphicEffectRain);
    }
}

static const float kWindStepMinimum = 1e-08f;

// FUNCTION: EXE 0x52f3d6
// Moves the wind one step toward its target; once there, sets the wind to a random strength and a step rate that
// brings it back to the (unchanging) target in about fifteen steps
void EEEnvironmentManager::UpdateWind(float inTimeOfDay, unsigned long inTime)
{
    if (this->mWind != this->mWindTarget) {
        if (this->mWind < this->mWindTarget) {
            this->mWind += this->mWindRate;
            if (this->mWind > this->mWindTarget)
                this->mWind = this->mWindTarget;
        } else {
            this->mWind -= this->mWindRate;
            if (this->mWind < this->mWindTarget)
                this->mWind = this->mWindTarget;
        }

    } else {
        this->mWind = gPhysics->GetRandom(kWindMinimum, kWindMaximum);
        if (this->mWind != this->mWindTarget) {
            float theDelta = this->mWindTarget - this->mWind;
            float theStep = (theDelta < 0.0f ? -theDelta : theDelta) / 15.0;


            this->mWindRate = min(max(theStep, kWindStepMinimum), kWindStepMaximum);
        }
    }
}
