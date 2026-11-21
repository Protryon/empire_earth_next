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
#include "lle/SESoundManager.h"
#include "lle/USingleton.h"

#include "ee/EEEffectManager.h"
#include "ee/EECalamity.h"
#include "rust/game/gamecpp.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEUnit.h"
#include "ee/EEVectorIO.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

const UString EECalamity::mEffectGenerationStringID("EffectGenerationStringID");

// (the 21 keys' init chains 0x4acb75..0x4ad0f4: registrar, ctor, atexit registrar, guarded dtor each)

// Creates an active calamity of the class for the caster (referenced) and loads the class's media for the caster's
// player (nature without a caster)
EECalamity::EECalamity(EECalamityClass* inClass, EEUnit* inCasterUnit)
    : mEffectGeneration(6), mTilePlaceHolder1(0, 0)
{
    ee_gamecpp_calamity_construct(this, inClass, inCasterUnit, true);   // (ee_game gamecpp calamity_objects.rs)
}

// Creates an empty calamity with no class, target or caster (for loading)
EECalamity::EECalamity()
    : mEffectGeneration(6), mTilePlaceHolder1(0, 0)
{
    ee_gamecpp_calamity_construct(this, NULL, NULL, false);
}

// Destroys the calamity: stops its sounds, unloads its class's media unless it has a caster id but no caster unit,
// and releases its units
EECalamity::~EECalamity()
{
    ee_gamecpp_calamity_destruct(this);
}

// Stops the calamity's main sound and releases it
void EECalamity::StopSound()
{
    ee_gamecpp_calamity_stop_sound(this, false);
}

// Releases the calamity's main sound without stopping it
void EECalamity::ReleaseSound()
{
    ee_gamecpp_calamity_release_sound(this, false);
}

// Stops the calamity's secondary sound and releases it (folded with EEE2DFire::StopSound in the original, whose
// mSound sat at the same offset)
void EECalamity::StopSecondarySound()
{
    ee_gamecpp_calamity_stop_sound(this, true);
}

// Releases the calamity's secondary sound without stopping it
void EECalamity::ReleaseSecondarySound()
{
    ee_gamecpp_calamity_release_sound(this, true);
}

// Clears the calamity: no class, target or caster (both released), sounds released, the effect generations and
// handle zeroed; every timer, flag and placeholder as a new calamity's (ee_tech calamity_record.rs reset)
void EECalamity::Reset()
{
    ee_gamecpp_calamity_reset(this);
}

// Writes the calamity's state to the out-of-sync dump
void EECalamity::DumpOutOfSynch(FILE* inFile)
{
    ee_gamecpp_calamity_dump(this, inFile);
}

// (empty: folded into a shared empty function)
void EECalamity::Virtual18(unsigned long)
{
}

// Returns a sync checksum: the sum of the calamity's float bit patterns, ids, placeholders and flags (ee_tech
// calamity_record.rs checksum)
long EECalamity::GetOutOfSynchChecksum()
{
    return ee_gamecpp_calamity_checksum(this);
}

// Returns how many whole discrete update periods have elapsed and keeps the remainder
unsigned long EECalamity::GetDiscreteUpdates()
{
    return ee_gamecpp_calamity_discrete_updates(this);
}

// Returns the target unit through the argument (NULL if none) and whether there is one
bool EECalamity::GetTargetUnit(EEUnit*& outUnit)
{
    return (outUnit = this->mTargetUnit) != NULL;
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

// Reads the calamity from a saved game (the class by id; target and caster only as ids); returns the first read error
// (ee_tech calamity_record.rs load)
long EECalamity::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EECalamityIo theIo = {this, &inFile, true};
    EeCalamityIoFfi theFfi = {&theIo, &EECalamityIoCall};
    return ee_calamity_record_load(this, theFfi);
}

// Writes the calamity to the saved game (target and caster as ids, -1 if none); returns the first write error
// (ee_tech calamity_record.rs save)
long EECalamity::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EECalamityIo theIo = {this, &outFile, false};
    EeCalamityIoFfi theFfi = {&theIo, &EECalamityIoCall};
    return ee_calamity_record_save(this, theFfi);
}

// Plays the calamity's sound through its class
void EECalamity::PlaySound()
{
    ee_gamecpp_calamity_play_sound(this);
}

// Sets the caster unit, moving the reference and the originating unit id to it (-1 when cleared)
void EECalamity::SetCasterUnit(EEUnit* inUnit)
{
    ee_gamecpp_calamity_set_unit(this, inUnit, false);
}

// Stops the current main sound and keeps a referenced new one
void EECalamity::SetSound(SESMInstanceNode* inSound)
{
    ee_gamecpp_calamity_set_sound(this, inSound, false);
}

// Stops the current secondary sound and keeps a referenced new one
void EECalamity::SetSecondarySound(SESMInstanceNode* inSound)
{
    ee_gamecpp_calamity_set_sound(this, inSound, true);
}

// Sets the target unit, moving the reference and the target id to it, and whether the calamity has a target unit
void EECalamity::SetTargetUnit(EEUnit* inUnit)
{
    ee_gamecpp_calamity_set_unit(this, inUnit, true);
}

