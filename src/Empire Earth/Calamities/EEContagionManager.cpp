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
#include "lle/GETerrainMesh.h"
#include "lle/USingleton.h"

#include "ee/EEEffectManager.h"
#include "ee/EECalamity.h"
#include "ee/EEVectorIO.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEContagionManager.h"
#include "rust/game/gamecpp.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EEAreaUtilities.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
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

// (CanInfect, the update's periods, Infect, Cure and Apply are gamecpp calamity_objects.rs', called by path)

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

// (the calamities' addresses as the Rust's 64-bit handles: every target is 64-bit)
static_assert(sizeof(EECalamity*) == sizeof(uint64_t), "the calamities' handles");
long EEContagionManager::RestorePointers(std::vector<EECalamity*>& inCalamities)
{
    return ee_contagion_manager_restore(this->mRust, reinterpret_cast<const uint64_t*>(inCalamities.data()), inCalamities.size());
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

// ------------------------------------------------------------------------- round 9, lane B: the hosts' compound cases' C++

int64_t EEPWorldOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

static_assert(kContagionNew == 4, "gamecpp calamity_hosts.rs");
static_assert(sizeof(EETileArea) == 4 * sizeof(long), "gamecpp calamity_hosts.rs: the area by address");

#define P64(x) ((int64_t)(intptr_t)(x))


// ------------------------------------------------------------------------- round 11, lane C: the calamities' C++

#define CAL ((EECalamity*)p)
#define FNC ((EEContagionFunctor*)p)
#define FSF (*(FSFile*)a[0])

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
    case O::CgCalamityIndex: return EECalamityManager::GetInstance()->GetCalamityIndex((EECalamity*)p, *(long*)a[0]);
    case O::CgWriteClassId: return ((FSFile*)a[0])->Write((unsigned long)((EECalamity*)p)->mClass->mID);
    case O::CgFileReadLong: return ((FSFile*)p)->Read(*(long*)a[0]);
    case O::CgFileWriteLong: return ((FSFile*)p)->Write((long)a[0]);
    case O::CgSetClass: ((EECalamity*)p)->mClass = EECalamityManager::GetInstance()->GetCalamityClass((long)a[0]);
        break;
    case O::CgTileAddr: return P64(a[0] != 0 ? &((EECalamity*)p)->mTilePlaceHolder1.mY : &((EECalamity*)p)->mTilePlaceHolder1.mX);
    // (EECore11OpC5) the C++ calls and members of the calamity record's I/O and of the contagion world (gamecpp calamity_io.rs; p the
    // calamity or the functor; EEMessage.cpp's EECore11OpC4 default): one statement each
    // the calamity record's parts (a0 the file)
    case O::C11CalRectLoad: return CAL->mRectangle.Load(FSF);
    case O::C11CalRectSave: return CAL->mRectangle.Save(FSF);
    case O::C11CalTargetLoad: return CAL->mTargetLocation.Load(FSF);
    case O::C11CalTargetSave: return CAL->mTargetLocation.Save(FSF);
    case O::C11CalShakeLoad: return CAL->mShakeLocation.Load(FSF);
    case O::C11CalShakeSave: return CAL->mShakeLocation.Save(FSF);
    case O::C11CalGenerationLoad: return FSLoadVector(FSF, CAL->mEffectGeneration, EECalamity::mEffectGenerationStringID);
    case O::C11CalGenerationSave: return FSSaveVector(FSF, CAL->mEffectGeneration, EECalamity::mEffectGenerationStringID);
    case O::C11CalTargetId: return CAL->mTargetUnit != NULL ? CAL->mTargetUnit->GetUnitID() : -1;
    case O::C11CalCasterId: return CAL->mCasterUnit != NULL ? CAL->mCasterUnit->GetUnitID() : -1;
    case O::C11CalHandleRead: return FSF.Read(CAL->mEffectHandle.mValue);
    case O::C11CalHandleWrite: return FSF.Write(CAL->mEffectHandle.mValue);
    // the contagion world (a0, a1 the unit and the calamity unless noted)
    case O::C11CgChecksum: return CAL->GetOutOfSynchChecksum();
    case O::C11CgFunctor: return P64(CAL->mClass->GetContagionFunctor());
    case O::C11CgDump: CAL->DumpOutOfSynch((FILE*)a[0]);
        break;
    case O::C11CgText: fputs((const char*)a[0], (FILE*)p);
        break;
    case O::C11CgUnknown: ESVERIFY_AT(false, 1);
        break;
    default: return EEPWorldOp(inOp, p, a, f);
    }
    return 0;
}

// the float answers: a random draw, two complex units' squared distance (p, a0)
double EECore11OpC5F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::C11CgRandom: return gPhysics->mRandom.Generate();
    case O::C11CgDistance: return ((EEComplexUnit*)p)->mPhysicalModel.GetLocation().DistanceSquaredTo(((EEComplexUnit*)a[0])->mPhysicalModel.GetLocation());
    default: ESVERIFY_AT(inOp == EeGameCpp::CxEffectHeight, 1);   // the end of the chain
    }
    return 0.0;
}
