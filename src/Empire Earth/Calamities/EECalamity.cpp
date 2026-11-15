// C:\EEX\Empire Earth\Calamities\EECalamity.cpp (file name inferred: no __FILE__; the class's save keys, ctors, members
// and the vector load/save templates it instantiates first). One active calamity: 0xacb75..0xadcd9 (init chains of
// the 21 save keys 0xacb75..0xad0f4). Unit flags: the exe's module set.
// Data (.rdata, in order): -10000.0f 0x43c690 (not referenced by the unit's code), the vtable 0x43c698 (COL
// 0x43c694). (.data): the key strings 0x4b90d8.., type descriptors, the dump formats 0x4b9324..; (.bss): the keys
// 0x4e5f58..0x4e5fa0 and their guard bytes.
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/USingleton.h"

#include "ee/EEEffectManager.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEPlayer.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEUnit.h"
#include "ee/EEVectorIO.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "rust/ee_tech.h"
#include "lle/FSFile.h"

const UString EECalamity::mEffectGenerationStringID("EffectGenerationStringID");

// (the 21 keys' init chains 0x4acb75..0x4ad0f4: registrar, ctor, atexit registrar, guarded dtor each)

// FUNCTION: EXE 0x4ad0f4
// Creates an active calamity of the class for the caster (referenced) and loads the class's media for the caster's
// player (nature without a caster)
EECalamity::EECalamity(EECalamityClass* inClass, EEUnit* inCasterUnit)
    : mEffectGeneration(6), mTilePlaceHolder1(0, 0)
{
    this->mTargetUnit = NULL;
    this->mCasterUnit = NULL;
    this->mSound = NULL;
    this->mSecondarySound = NULL;
    this->Reset();
    this->mClass = inClass;
    this->SetCasterUnit(inCasterUnit);
    this->mClass->LoadMedia(inCasterUnit != NULL ? inCasterUnit->mPlayer : ::gWorld.mPlayers[0], true);
}

// SYNTHETIC: EXE 0x4ad191
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4ad1ad
// Creates an empty calamity with no class, target or caster (for loading)
EECalamity::EECalamity()
    : mEffectGeneration(6), mTilePlaceHolder1(0, 0)
{
    this->mTargetUnit = NULL;
    this->mCasterUnit = NULL;
    this->mSound = NULL;
    this->mSecondarySound = NULL;
    this->Reset();
}

// FUNCTION: EXE 0x4ad218
// Destroys the calamity: stops its sounds, unloads its class's media unless it has a caster id but no caster unit,
// and releases its units
EECalamity::~EECalamity()
{
    this->StopSound();
    this->StopSecondarySound();
    if (UIsNone32(this->mOriginatingUnitID) || this->mCasterUnit != NULL)
        this->mClass->UnloadMedia(this->mCasterUnit != NULL ? this->mCasterUnit->mPlayer : ::gWorld.mPlayers[0]);
    this->Reset();
}

// FUNCTION: EXE 0x4ad267
// Stops the calamity's main sound and releases it
void EECalamity::StopSound()
{
    if (this->mSound != NULL) {
        gSoundManager->Stop(this->mSound, false);
        this->ReleaseSound();
    }
}

// FUNCTION: EXE 0x4ad289
// Releases the calamity's main sound without stopping it
void EECalamity::ReleaseSound()
{
    if (this->mSound != NULL)
        this->mSound->Dereference();
    this->mSound = NULL;
}

// ICF-FOLDED: EXE 0x4dfd80
// Stops the calamity's secondary sound and releases it (folded with EEE2DFire::StopSound in the original, whose
// mSound sat at the same offset)
void EECalamity::StopSecondarySound()
{
    if (this->mSecondarySound) {
        gSoundManager->Stop(this->mSecondarySound, false);
        this->ReleaseSecondarySound();
    }
}

// FUNCTION: EXE 0x4ad2ad
// Releases the calamity's secondary sound without stopping it
void EECalamity::ReleaseSecondarySound()
{
    if (this->mSecondarySound != NULL)
        this->mSecondarySound->Dereference();
    this->mSecondarySound = NULL;
}

// FUNCTION: EXE 0x4ad2d1
// Clears the calamity: no class, target or caster (both released), sounds released, the effect generations and
// handle zeroed; every timer, flag and placeholder as a new calamity's (ee_tech calamity_record.rs reset)
void EECalamity::Reset()
{
    this->mClass = NULL;
    if (this->mTargetUnit != NULL) {
        this->mTargetUnit->Dereference();
        this->mTargetUnit = NULL;
    }
    if (this->mCasterUnit != NULL) {
        this->mCasterUnit->Dereference();
        this->mCasterUnit = NULL;
    }
    for (long i = 0; i < 6; i++)
        *(this->mEffectGeneration.begin() + i) = 0;
    this->ReleaseSound();
    this->ReleaseSecondarySound();
    this->mEffectHandle.mValue = 0;
    ee_calamity_record_reset(this);
}

// FUNCTION: EXE 0x4ad377
// Writes the calamity's state to the out-of-sync dump
void EECalamity::DumpOutOfSynch(FILE* inFile)
{
    fprintf(inFile, "\nOUT OF SYNCH DUMP:  Calamity Class is %ld\n", this->mClass->mClassEnum);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Target Location X: %6.6f\n", this->mTargetLocation.GetX());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Target Location Y: %6.6f\n", this->mTargetLocation.GetY());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Target Location Z: %6.6f\n", this->mTargetLocation.GetZ());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Tile Int PlaceHolder1 X: %ld\n", this->mTilePlaceHolder1.mX);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Tile Int PlaceHolder1 Y: %ld\n", this->mTilePlaceHolder1.mY);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Integer PlaceHolder1 %ld\n", this->mIntPlaceHolder1);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Integer PlaceHolder2 %ld\n", this->mIntPlaceHolder2);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Target Unit ID #%08lx\n", this->mTargetUnitID);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Caster Unit ID #%08lx\n", this->mOriginatingUnitID);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Player Number %ld\n", this->mPlayerNumber);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Bool PlaceHolder 1 has a value of %d\n", this->mBoolPlaceHolder1 == true);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Bool PlaceHolder 2 has a value of %d\n", this->mBoolPlaceHolder2 == true);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Bool Initialized has a value of %d\n", this->mInitialized == true);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Bool HasTargetLocation has a value of %d\n",
            this->mHasTargetLocation == true);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Bool HasTargetUnit has a value of %d\n", this->mHasTargetUnit == true);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Float PlaceHolder 1 is %6.6f\n", this->mFloatPlaceHolder1);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Float PlaceHolder 2 is %6.6f\n", this->mFloatPlaceHolder2);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Height is %6.6f\n", this->mHeight);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Discrete Update Time is %6.6f\n", this->mDiscreteUpdateTime);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Time Elapsed Since Last Update is %6.6f\n",
            this->mTimeElapsedSinceLastUpdate);
}

// ICF-FOLDED: EXE 0x557269
// (empty: folded into a shared empty function)
void EECalamity::Virtual18(unsigned long)
{
}

// FUNCTION: EXE 0x4ad50f
// Returns a sync checksum: the sum of the calamity's float bit patterns, ids, placeholders and flags (ee_tech
// calamity_record.rs checksum)
long EECalamity::GetOutOfSynchChecksum()
{
    return ee_calamity_record_checksum(this, this->mTargetLocation.GetX(), this->mTargetLocation.GetY(),
                                       this->mTargetLocation.GetZ(), this->mTilePlaceHolder1.mX, this->mTilePlaceHolder1.mY);
}

// FUNCTION: EXE 0x4ad5bb
// Returns how many whole discrete update periods have elapsed and keeps the remainder
unsigned long EECalamity::GetDiscreteUpdates()
{
    // the periods and the time left are Rust (ee_tech contagion.rs discrete_updates)
    EeCalamityDiscrete theDiscrete = ee_calamity_discrete_updates(this->mTimeElapsedSinceLastUpdate, this->mDiscreteUpdateTime);
    this->mTimeElapsedSinceLastUpdate = theDiscrete.elapsed;
    return theDiscrete.updates;
}

// FUNCTION: EXE 0x4ad5fc
// Returns the target unit through the argument (NULL if none) and whether there is one
bool EECalamity::GetTargetUnit(EEUnit*& outUnit)
{
    bool theResult = false;
    if (this->mTargetUnit != NULL) {
        outUnit = this->mTargetUnit;
        theResult = true;
    } else
        outUnit = NULL;
    return theResult;
}

namespace {

// New (port): the file and the calamity's C++ members for its Rust Load and Save (ee_tech calamity_record.rs
// EE_CALAMITY_IO_*): the numbers through the FSFile overload the original called, the members by part
struct EECalamityIo {
    EECalamity* mCalamity;
    FSFile* mFile;
    bool mLoading;
};

long EECalamityIoCall(void* inContext, uint32_t inOp, void* ioValue)
{
    EECalamityIo& theIo = *static_cast<EECalamityIo*>(inContext);
    EECalamity* theCalamity = theIo.mCalamity;
    FSFile& theFile = *theIo.mFile;
    switch (inOp) {
    case EE_CALAMITY_IO_READ_ULONG: return theFile.Read(*static_cast<unsigned long*>(ioValue));
    case EE_CALAMITY_IO_READ_LONG: return theFile.Read(*static_cast<long*>(ioValue));
    case EE_CALAMITY_IO_READ_FLOAT: return theFile.Read(*static_cast<float*>(ioValue));
    case EE_CALAMITY_IO_READ_BOOL: return theFile.Read(*static_cast<bool*>(ioValue));
    case EE_CALAMITY_IO_WRITE_ULONG: return theFile.Write(*static_cast<const unsigned long*>(ioValue));
    case EE_CALAMITY_IO_WRITE_LONG: return theFile.Write(*static_cast<const long*>(ioValue));
    case EE_CALAMITY_IO_WRITE_FLOAT: return theFile.Write(*static_cast<const float*>(ioValue));
    case EE_CALAMITY_IO_WRITE_BOOL: return theFile.Write(*static_cast<const bool*>(ioValue));
    case EE_CALAMITY_IO_RECTANGLE:
        return theIo.mLoading ? theCalamity->mRectangle.Load(theFile) : theCalamity->mRectangle.Save(theFile);
    case EE_CALAMITY_IO_TARGET_LOCATION:
        return theIo.mLoading ? theCalamity->mTargetLocation.Load(theFile) : theCalamity->mTargetLocation.Save(theFile);
    case EE_CALAMITY_IO_SHAKE_LOCATION:
        return theIo.mLoading ? theCalamity->mShakeLocation.Load(theFile) : theCalamity->mShakeLocation.Save(theFile);
    case EE_CALAMITY_IO_EFFECT_GENERATION:
        if (theIo.mLoading)
            return FSLoadVector(theFile, theCalamity->mEffectGeneration, EECalamity::mEffectGenerationStringID);
        return FSSaveVector(theFile, theCalamity->mEffectGeneration, EECalamity::mEffectGenerationStringID);
    case EE_CALAMITY_IO_CLASS: {
        if (!theIo.mLoading)
            return theFile.Write((unsigned long)theCalamity->mClass->mID);
        long theClassID = 0;
        long theResult = theFile.Read(theClassID);
        theCalamity->mClass = EECalamityManager::GetInstance()->GetCalamityClass(theClassID);
        return theResult;
    }
    case EE_CALAMITY_IO_TARGET_ID:
        *static_cast<unsigned long*>(ioValue) = theCalamity->mTargetUnit != NULL ? theCalamity->mTargetUnit->GetUnitID() : -1;
        return kErrorOK;
    case EE_CALAMITY_IO_CASTER_ID:
        *static_cast<unsigned long*>(ioValue) = theCalamity->mCasterUnit != NULL ? theCalamity->mCasterUnit->GetUnitID() : -1;
        return kErrorOK;
    case EE_CALAMITY_IO_TILE_X:
    case EE_CALAMITY_IO_TILE_Y: {
        unsigned long& theTile = inOp == EE_CALAMITY_IO_TILE_X ? theCalamity->mTilePlaceHolder1.mX : theCalamity->mTilePlaceHolder1.mY;
        if (!theIo.mLoading)
            return theFile.Write((long)theTile);
        long theValue = 0;
        long theResult = theFile.Read(theValue);
        theTile = theValue;
        return theResult;
    }
    case EE_CALAMITY_IO_EFFECT_HANDLE:
        return theIo.mLoading ? theFile.Read(theCalamity->mEffectHandle.mValue) : theFile.Write(theCalamity->mEffectHandle.mValue);
    }
    return kErrorFailed;
}

}   // namespace

// FUNCTION: EXE 0x4ad618
// Reads the calamity from a saved game (the class by id; target and caster only as ids); returns the first read error
// (ee_tech calamity_record.rs load)
long EECalamity::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EECalamityIo theIo = {this, &inFile, true};
    EeCalamityIoFfi theFfi = {&theIo, &EECalamityIoCall};
    return ee_calamity_record_load(this, theFfi);
}

// FUNCTION: EXE 0x4ad85e
// Writes the calamity to the saved game (target and caster as ids, -1 if none); returns the first write error
// (ee_tech calamity_record.rs save)
long EECalamity::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EECalamityIo theIo = {this, &outFile, false};
    EeCalamityIoFfi theFfi = {&theIo, &EECalamityIoCall};
    return ee_calamity_record_save(this, theFfi);
}

// FUNCTION: EXE 0x4adaa3
// Plays the calamity's sound through its class
void EECalamity::PlaySound()
{
    if (this->mClass != NULL)
        this->mClass->PlaySound(this);
}

// FUNCTION: EXE 0x4adab3
// Sets the caster unit, moving the reference and the originating unit id to it (-1 when cleared)
void EECalamity::SetCasterUnit(EEUnit* inUnit)
{
    if (this->mCasterUnit != NULL) {
        this->mCasterUnit->Dereference();
        this->mOriginatingUnitID = -1;
    }
    this->mCasterUnit = inUnit;
    if (inUnit != NULL) {
        this->mCasterUnit->Reference();
        this->mOriginatingUnitID = this->mCasterUnit->mUnitID;
    }
}

// FUNCTION: EXE 0x4adae7
// Stops the current main sound and keeps a referenced new one
void EECalamity::SetSound(SESMInstanceNode* inSound)
{
    this->StopSound();
    this->mSound = inSound;
    if (inSound != NULL)
        inSound->Reference();
}

// FUNCTION: EXE 0x4adb01
// Stops the current secondary sound and keeps a referenced new one
void EECalamity::SetSecondarySound(SESMInstanceNode* inSound)
{
    this->StopSecondarySound();
    this->mSecondarySound = inSound;
    if (inSound != NULL)
        inSound->Reference();
}

// FUNCTION: EXE 0x4adb1b
// Sets the target unit, moving the reference and the target id to it, and whether the calamity has a target unit
void EECalamity::SetTargetUnit(EEUnit* inUnit)
{
    if (this->mTargetUnit != NULL) {
        this->mTargetUnit->Dereference();
        this->mTargetUnitID = -1;
    }
    this->mTargetUnit = inUnit;
    if (inUnit != NULL) {
        this->mTargetUnit->Reference();
        this->mTargetUnitID = this->mTargetUnit->mUnitID;
    }
    if (this->mTargetUnit != NULL)
        this->mHasTargetUnit = true;
    else
        this->mHasTargetUnit = false;
}

// Compiler-generated for this unit (template COMDATs; names in analysis/exe/names.txt):
// TEMPLATE: EXE 0x4adb65
// TEMPLATE: EXE 0x4adb9f
// TEMPLATE: EXE 0x4adc3d

