// C:\EEX\Empire Earth\Calamities\EEContagionManager.cpp (file name inferred: no __FILE__; the class name). The world's
// contagion helpers: 0xb662f..0xb6bd1 (the three save keys' init chains first, then EEContagionFunctor::CanInfect);
// with the six contagion functors as one (ee/EEContagionFunctor.h; their units EE*Functor.cpp are gone).
// Unit flags: the exe's module set. The manager and its helpers are Rust's (ee/EEContagionManager.h); here are the
// functors and the world under the Rust (ee_contagion_world).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/USingleton.h"

#include "ee/EEEffectManager.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEContagionManager.h"
#include "rust/game/gamecpp.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEEffectKindIDs.h"
#include "ee/EEMap.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EEAreaUtilities.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EETechCapture.h"
#include "ee/EESaveGameFiles.h"


// New (port): the six functor classes' constructors (0xb6bd1, 0xb6e58, 0xb6fb1, 0xac991, 0xac7b9, 0xac891) as one:
// the numbers from the class's record (ee_tech contagion_functor.rs init), the effects each kind kept
EEContagionFunctor::EEContagionFunctor(EECalamityClass* inClass, uint32_t inKind)
{
    ee_gamecpp_functor_construct(this, inClass, inKind);   // (ee_game gamecpp calamity_objects.rs)
}

// Destroys the functor, releasing the fire's burning effect
EEContagionFunctor::~EEContagionFunctor()
{
    if (this->mKind == EE_CALAMITY_FUNCTOR_FIRE)
        this->mEffect->Dereference();
}

// Accepts a unit type in the functor's unit set (every functor's CanInfectType, by ICF)
bool EEContagionFunctor::CanInfectType(long inCategory, long inUnitType)
{
    return gUnitSetManager.Contains(inUnitType, this->mUnitSet);
}

// Tells whether the functor can infect the unit's category and type
bool EEContagionFunctor::CanInfect(EEUnit* inUnit)
{
    return this->CanInfectType(inUnit->mUnitClass->GetCategory(), inUnit->mUnitClass->GetUnitType());
}

// Counts the whole discrete update periods elapsed on the calamity for this update's damage
void EEContagionFunctor::Update(float inElapsedTime, EECalamity* inCalamity)
{
    ee_gamecpp_functor_update(this, inElapsedTime, inCalamity);
}

void EEContagionFunctor::Infect(EEUnit* inUnit, EECalamity* inCalamity)
{
    ee_gamecpp_functor_infect(this, inUnit, inCalamity);
}

void EEContagionFunctor::Cure(EEUnit* inUnit, EECalamity* inCalamity)
{
    ee_gamecpp_functor_cure(this, inUnit, inCalamity);
}

// Damages the unit per elapsed period (ee_tech contagion_functor.rs apply_damage): the fire and the plague keep it
// from being deleted (the fire before, unless it is carried; the plague after), the area effect shows its hit effect
void EEContagionFunctor::Apply(EEUnit* inUnit, float inElapsedTime, EECalamity* inCalamity)
{
    ee_gamecpp_functor_apply(this, inUnit, inElapsedTime, inCalamity);
}

// Reads the helpers and their calamity indexes from a saved game (the last helper id from 0.273 on: ee/
// EESaveGameFiles.h EESaveHasContagionCounter); the first read error (contagion_store.rs EeContagionManager::load)
long EEContagionManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    return ee_contagion_manager_load(this->mRust, (uint64_t)(uintptr_t)&inFile, inMajorVersion, inMinorVersion,
                                     EESaveHasContagionCounter(inMajorVersion, inMinorVersion));
}

// Writes each helper with its calamity's index; 9 if a calamity is not active, else the first write error
long EEContagionManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    return ee_contagion_manager_save(this->mRust, (uint64_t)(uintptr_t)&outFile,
                                     EESaveHasContagionCounter(inMajorVersion, inMinorVersion));
}

long EEContagionManager::RestorePointers(std::vector<EECalamity*>& inCalamities)
{
    std::vector<uint64_t> theCalamities;
    for (EECalamity* theCalamity : inCalamities)
        theCalamities.push_back((uint64_t)(uintptr_t)theCalamity);
    return ee_contagion_manager_restore(this->mRust, theCalamities.data(), theCalamities.size());
}

bool EEContagionHelperAddUnit(EEContagionHelper* inHelper, EEUnit* inUnit, bool inNotify)
{
    return ee_contagion_helper_add_unit(inHelper, (uint64_t)(uintptr_t)inUnit, inNotify);
}

void EEContagionHelperUpdate(EEContagionHelper* inHelper, float inElapsedTime)
{
    EECalamity* theCalamity = reinterpret_cast<EECalamity*>((uintptr_t)ee_contagion_helper_calamity(inHelper));
    ee_contagion_helper_update(inHelper, theCalamity->mClass->mIsBlockedByImmunity, inElapsedTime);
}

namespace {

template<class T> T* Object(uint64_t inHandle) { return reinterpret_cast<T*>((uintptr_t)inHandle); }

uint32_t FloatBits(float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    return theBits;
}

}   // namespace

// New (port): the world under the Rust contagion manager and helpers (contagion_store.rs EE_CONTAGION_WORLD_*): each
// case the original helper's or manager's statement on the units, functors and calamities by address
extern "C" int64_t ee_contagion_world(uint32_t inOp, uint64_t inA, uint64_t inB, uint64_t inC, float inX, void* ioValue)
{
    switch (inOp) {
    case EE_CONTAGION_WORLD_REFERENCE: return ee_gamecpp_contagion_reference(inA, inB);
    case EE_CONTAGION_WORLD_MARK: return ee_gamecpp_contagion_mark(inA, inB, inC);
    case EE_CONTAGION_WORLD_INFECT: Object<EEContagionFunctor>(inA)->Infect(Object<EEUnit>(inB), Object<EECalamity>(inC)); return 0;
    case EE_CONTAGION_WORLD_CURE: Object<EEContagionFunctor>(inA)->Cure(Object<EEUnit>(inB), Object<EECalamity>(inC)); return 0;
    case EE_CONTAGION_WORLD_APPLY:
        Object<EEContagionFunctor>(inA)->Apply(Object<EEUnit>(inB), inX, Object<EECalamity>(inC));
        return 0;
    case EE_CONTAGION_WORLD_FUNCTOR_UPDATE: Object<EEContagionFunctor>(inA)->Update(inX, Object<EECalamity>(inC)); return 0;
    case EE_CONTAGION_WORLD_NEAR: return ee_gamecpp_contagion_near(inA, inX, ioValue);   // GetUnitsInArea
    case EE_CONTAGION_WORLD_CAN_INFECT:
        return Object<EEContagionFunctor>(inA)->CanInfectType(Object<EEUnit>(inB)->mUnitClass->GetCategory(), Object<EEUnit>(inB)->mUnitClass->GetUnitType());
    case EE_CONTAGION_WORLD_IMMUNE: return ee_gamecpp_contagion_immune(inA);
    case EE_CONTAGION_WORLD_RANDOM: return FloatBits(gPhysics->mRandom.Generate());
    case EE_CONTAGION_WORLD_DISTANCE:
        return FloatBits(Object<EEComplexUnit>(inA)->mPhysicalModel.GetLocation().DistanceSquaredTo(
            Object<EEComplexUnit>(inB)->mPhysicalModel.GetLocation()));
    case EE_CONTAGION_WORLD_UNIT: return (int64_t)(uintptr_t)gWorld.GetUnit((unsigned long)inA);
    case EE_CONTAGION_WORLD_CALAMITY_CHECKSUM: return Object<EECalamity>(inA)->GetOutOfSynchChecksum();
    case EE_CONTAGION_WORLD_CALAMITY_INDEX: return ee_gamecpp_contagion_calamity_index(inA);
    case EE_CONTAGION_WORLD_CALAMITY_FUNCTOR:
        return (int64_t)(uintptr_t)Object<EECalamity>(inA)->mClass->GetContagionFunctor();
    case EE_CONTAGION_WORLD_CALAMITY_DUMP: Object<EECalamity>(inA)->DumpOutOfSynch(Object<FILE>(inB)); return 0;
    case EE_CONTAGION_WORLD_TEXT: fputs(static_cast<const char*>(ioValue), Object<FILE>(inB)); return 0;
    case EE_CONTAGION_WORLD_FILE: {
        FSFile& theFile = *Object<FSFile>(inB);
        switch (inA) {
        case EE_CONTAGION_IO_READ_ULONG: return theFile.Read(*static_cast<unsigned long*>(ioValue));
        case EE_CONTAGION_IO_READ_LONG: return theFile.Read(*static_cast<long*>(ioValue));
        case EE_CONTAGION_IO_READ_FLOAT: return theFile.Read(*static_cast<float*>(ioValue));
        case EE_CONTAGION_IO_WRITE_ULONG: return theFile.Write(*static_cast<const unsigned long*>(ioValue));
        case EE_CONTAGION_IO_WRITE_LONG: return theFile.Write(*static_cast<const long*>(ioValue));
        case EE_CONTAGION_IO_WRITE_FLOAT: return theFile.Write(*static_cast<const float*>(ioValue));
        }
        return kErrorFailed;
    }
    case EE_CONTAGION_WORLD_CAPTURE:   // CONT (TEST ONLY)
        EECalamityCaptureContagionEvent((int32_t)inA, (int32_t)inB, (int32_t)inC, inX, *static_cast<float*>(ioValue));
        return 0;
    }
    ESVERIFY_AT(false, 1);
    return 0;
}

// ------------------------------------------------------------------------- round 9, lane B: the hosts' compound cases' C++

int64_t EECore9OpC(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EECore9OpCF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

static_assert(kContagionNew == 4, "gamecpp calamity_hosts.rs");
static_assert(sizeof(EETileArea) == 4 * sizeof(long), "gamecpp calamity_hosts.rs: the area by address");

#define P64(x) ((int64_t)(intptr_t)(x))

// The C++ calls and members of the contagion world's and the calamity record's compound cases in Rust (gamecpp
// calamity_hosts.rs; EeGameCpp's lane B section, after EETile.cpp's EECore9OpB4): one statement each
int64_t EECore9OpB5(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::CgUnitReference: ((EEUnit*)p)->Reference();
        break;
    case O::CgUnitDereference: ((EEUnit*)p)->Dereference();
        break;
    case O::CgClassFlags: return ((EECalamity*)p)->mClass->mClassFlags;
    case O::CgUnitsNew: return P64(new std::vector<EEComplexUnit*>);
    case O::CgUnitsDelete: delete (std::vector<EEComplexUnit*>*)p;
        break;
    case O::CgUnitsCount: return (int64_t)((std::vector<EEComplexUnit*>*)p)->size();
    case O::CgUnitsAt: return P64((*(std::vector<EEComplexUnit*>*)p)[a[0]]);
    case O::CgUnitsInArea: EEGetUnitsInRectangle(*(const EETileArea*)a[0], *(std::vector<EEComplexUnit*>*)p, EEAnyUnit(), true);
        break;
    case O::CgCaptureContagion: EECalamityCaptureContagionEvent((long)a[0], (long)a[1], (long)a[2]);   // CONT (TEST ONLY)
        break;
    case O::CgCalamityIndex: return EECalamityManager::GetInstance()->GetCalamityIndex((EECalamity*)p, *(long*)a[0]);
    case O::CgWriteClassId: return ((FSFile*)a[0])->Write((unsigned long)((EECalamity*)p)->mClass->mID);
    case O::CgFileReadLong: return ((FSFile*)p)->Read(*(long*)a[0]);
    case O::CgFileWriteLong: return ((FSFile*)p)->Write((long)a[0]);
    case O::CgSetClass: ((EECalamity*)p)->mClass = EECalamityManager::GetInstance()->GetCalamityClass((long)a[0]);
        break;
    case O::CgTileAddr: return P64(a[0] != 0 ? &((EECalamity*)p)->mTilePlaceHolder1.mY : &((EECalamity*)p)->mTilePlaceHolder1.mX);
    default: return EECore9OpC(inOp, p, a, f);
    }
    return 0;
}

double EECore9OpB5F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    return EECore9OpCF(inOp, p, a, f);
}
