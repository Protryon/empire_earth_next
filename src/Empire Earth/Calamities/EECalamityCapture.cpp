// New (port), TEST ONLY: the calamity lane's records of the simulation capture (ee/EECalamityCapture.h;
// tools/sim_capture.py --profile calamity). Reads the world; writes nothing to it.
#include <windows.h>
#include <algorithm>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"
#include "lle/FSMemoryFile.h"
#include "lle/USingleton.h"

#include "ee/EEEffectManager.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamityCapture.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEGlobalCalamityClass.h"
#include "ee/EELocationCalamityClass.h"
#include "ee/EEUnitCalamityClass.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEContagionManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EESoundObject.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

namespace {

void Effect(EESimCaptureRecord& ioRecord, EEGraphicEffect* inEffect)
{
    if (inEffect == NULL) {
        ioRecord.I32(-1);
        return;
    }
    std::vector<EEGraphicEffect*>::iterator theFound =
        std::find(EEGraphicEffect::sEffects.begin(), EEGraphicEffect::sEffects.end(), inEffect);
    ioRecord.I32((int)(theFound - EEGraphicEffect::sEffects.begin()));
    ioRecord.U32(inEffect->mDuration);
    ioRecord.F32(inEffect->GetSize());
    ioRecord.U32(inEffect->mTime1);
}

void Sound(EESimCaptureRecord& ioRecord, EESoundObject* inSound)
{
    if (inSound == NULL) {
        ioRecord.I32(-1);
        return;
    }
    std::vector<EESoundObject*>::iterator theFound = std::find(gSoundObjects.begin(), gSoundObjects.end(), inSound);
    ioRecord.I32((int)(theFound - gSoundObjects.begin()));
}

int EffectIndex(EEGraphicEffect* inEffect)
{
    if (inEffect == NULL)
        return -1;
    return (int)(std::find(EEGraphicEffect::sEffects.begin(), EEGraphicEffect::sEffects.end(), inEffect) -
                 EEGraphicEffect::sEffects.begin());
}

void Functor(EESimCaptureRecord& ioRecord, EEContagionFunctor* f)
{
    // the record each of the six classes wrote (its tag: 1 fire, 2 leprosy, 3 plague, 4 area, 5 extra damage, 6 mind)
    static const unsigned int kTags[7] = {7, 4, 5, 6, 1, 2, 3};   // by EE_CALAMITY_FUNCTOR_*
    if (f == NULL) {
        ioRecord.U32(0);
        return;
    }
    ioRecord.U32(kTags[f->mKind]);
    switch (f->mKind) {
    case EE_CALAMITY_FUNCTOR_EXTRA_DAMAGE: ioRecord.U32(f->mFlag ? 1 : 0); break;
    case EE_CALAMITY_FUNCTOR_MIND_CONTROL: ioRecord.I32(EffectIndex(f->mEffect)); break;
    case EE_CALAMITY_FUNCTOR_FIRE:
        ioRecord.I32(EffectIndex(f->mEffect));
        ioRecord.F32(f->mRadius);
        ioRecord.F32(f->mChance);
        ioRecord.I32(f->mBurn);
        ioRecord.I32(f->mDamage);
        ioRecord.U32(f->mSpreadTime);
        ioRecord.U32(f->mUnitSet);
        ioRecord.I32(f->mMode);
        return;
    case EE_CALAMITY_FUNCTOR_LEPROSY:
        ioRecord.I32(f->mDamage);
        ioRecord.U32(f->mSpreadTime);
        ioRecord.U32(f->mUnitSet);
        ioRecord.I32(f->mMode);
        return;
    case EE_CALAMITY_FUNCTOR_PLAGUE:
        ioRecord.F32(f->mRadius);
        ioRecord.F32(f->mChance);
        ioRecord.I32(f->mDamage);
        ioRecord.U32(f->mSpreadTime);
        ioRecord.U32(f->mUnitSet);
        ioRecord.I32(f->mMode);
        return;
    }
    // the area effects' base
    ioRecord.I32(EffectIndex(f->mEffect2));
    ioRecord.I32(EffectIndex(f->mEffect3));
    ioRecord.I32(f->mDamage);
    ioRecord.I32((long)f->mSpreadTime);
    ioRecord.U32(f->mUnitSet);
    ioRecord.I32(f->mMode);
}

// the media slots in one order for every class: effects 1-4, the sound and the secondary sound (a class without a
// slot: -1), then the functor
void Media(EESimCaptureRecord& ioRecord, EEGraphicEffect* inEffect1, EEGraphicEffect* inEffect2,
           EEGraphicEffect* inEffect3, EEGraphicEffect* inEffect4, EESoundObject* inSound,
           EESoundObject* inSecondarySound, EEContagionFunctor* inFunctor)
{
    Effect(ioRecord, inEffect1);
    Effect(ioRecord, inEffect2);
    Effect(ioRecord, inEffect3);
    Effect(ioRecord, inEffect4);
    Sound(ioRecord, inSound);
    Sound(ioRecord, inSecondarySound);
    Functor(ioRecord, inFunctor);
}

// each class's own members (the base's, by the class's enum)
void AreaRec(EESimCaptureRecord& r, EECalamityClass* p, long e2t)
{
    Media(r, p->mGraphicEffect1, p->mGraphicEffect2, p->mGraphicEffect3, NULL, p->mSound, NULL, p->mFunctor);
    r.I32(p->mDamageFactor);
    r.U32(p->mEffect1Duration);
    r.U32(p->mRadiusTiles);
    r.I32(e2t);
}
void CloakRec(EESimCaptureRecord& r, EECalamityClass* p)
{
    Media(r, p->mGraphicEffect1, NULL, NULL, NULL, p->mSound, NULL, NULL);
}
void DropRec(EESimCaptureRecord& r, EECalamityClass* p)
{
    Media(r, p->mGraphicEffect1, p->mGraphicEffect2, p->mGraphicEffect3, NULL, p->mSound, NULL, NULL);
    r.I32(p->mUnitClassIndex);
}
void QuakeRec(EESimCaptureRecord& r, EECalamityClass* p)
{
    Media(r, p->mGraphicEffect1, p->mGraphicEffect2, NULL, NULL, p->mSound, NULL, NULL);
    r.I32(p->mDamage);
    r.I32(p->mTickTime);
    r.I32(p->mShake);
    r.I32(p->mCrackCount);
    r.F32(p->mDuration);
    r.F32(p->mRadius);
    r.F32(p->mShakeInterval);
    r.F32(p->mVolume);
    r.F32(p->mQuakeVolume);
    r.F32(p->mMinimumFactor);
    r.F32(p->mQuakeMinimumFactor);
    r.F32(p->mSpeed);
    r.F32(p->mSourceSpeed);
    r.F32(p->mFalloff);
}
void FireRec(EESimCaptureRecord& r, EECalamityClass* p)
{
    Media(r, p->mGraphicEffect1, p->mGraphicEffect2, NULL, NULL, p->mSound, NULL, p->mFunctor);
    r.I32(p->mBurn);
    r.I32(p->mDamage);
    r.F32(p->mRadius);
    r.F32(p->mChance);
}
void StormRec(EESimCaptureRecord& r, EECalamityClass* p)
{
    Media(r, p->mGraphicEffect1, p->mGraphicEffect2, p->mGraphicEffect3, NULL, p->mSound, NULL, NULL);
    r.I32(p->mDamage);
    r.U32(p->mTickTime);
    r.I32(p->mArea);
    r.I32(p->mUnitClassIndex);
    r.F32(p->mRadius);
    r.U32(p->mOverWater ? 1 : 0);
}
void LeprosyRec(EESimCaptureRecord& r, EECalamityClass* p)
{
    Media(r, p->mGraphicEffect1, NULL, NULL, NULL, p->mSound, NULL, p->mFunctor);
    r.I32(p->mDamage);
    r.U32(p->mEffect1Duration);
    r.U32(p->mRadiusTiles);
}
void SightRec(EESimCaptureRecord& r, EECalamityClass* p)
{
    Media(r, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    r.F32(p->mSightBonus);
}
void BombRec(EESimCaptureRecord& r, EECalamityClass* p)
{
    Media(r, p->mGraphicEffect1, p->mGraphicEffect2, p->mGraphicEffect3, NULL, p->mSound, NULL, NULL);
    r.U32(p->mEffectTime2);
    r.U32(p->mEffectTime3);
}
void MeteorRec(EESimCaptureRecord& r, EECalamityClass* p)
{
    Media(r, NULL, NULL, NULL, NULL, p->mSound, NULL, NULL);
    r.F32(p->mMinimumInterval);
    r.F32(p->mMaximumInterval);
    r.F32(p->mRadius);
    r.I32(p->mUnitClassIndex);
}
void MoraleRec(EESimCaptureRecord& r, EECalamityClass* p)
{
    Media(r, p->mGraphicEffect1, NULL, NULL, NULL, p->mSound, p->mSecondarySound, NULL);
    r.I32(p->mEffectDuration);
    r.U32(p->mPositive ? 1 : 0);
}
void PlagueRec(EESimCaptureRecord& r, EECalamityClass* p)
{
    Media(r, p->mGraphicEffect1, NULL, NULL, NULL, p->mSound, p->mSecondarySound, p->mFunctor);
    r.I32(p->mDamage);
}
void TeleportRec(EESimCaptureRecord& r, EECalamityClass* p)
{
    Media(r, p->mGraphicEffect1, p->mGraphicEffect2, NULL, NULL, p->mSound, p->mSecondarySound, NULL);
    r.F32(p->mDamageShare);
    r.I32(p->mPlacementRange);
    r.I32(p->mHoldTime);
}
void WarpRec(EESimCaptureRecord& r, EECalamityClass* p)
{
    Media(r, p->mGraphicEffect1, p->mGraphicEffect2, p->mGraphicEffect3, p->mGraphicEffect4, p->mSound,
          p->mSecondarySound, NULL);
    r.I32(p->mPlacementRange);
    r.I32(p->mGraphicEffect1 != NULL ? p->mDepartTime : 0);   // (not set without the first effect)
    r.I32(p->mAwayTime);
}
void VolcanoRec(EESimCaptureRecord& r, EECalamityClass* p)
{
    Media(r, p->mGraphicEffect1, p->mGraphicEffect2, p->mGraphicEffect3, p->mGraphicEffect4, p->mSound, NULL, NULL);
    r.I32(p->mL3C);
    r.I32(p->mL40);
    r.I32(p->mUnitClassIndex);
    r.I32(p->mL48);
    r.F32(p->mMaximumRadius);
    r.F32(p->mMinimumRadius);
}

void ClassRecord(EESimCaptureRecord& r, EECalamityClass* c)
{
    switch (c->mClassEnum) {
    case kCalamityEarthquake: QuakeRec(r, c); break;
    case kCalamityHurricane: StormRec(r, c); break;
    case kCalamityPlague: PlagueRec(r, c); break;
    case kCalamityLeprosy: LeprosyRec(r, c); break;
    case kCalamityVolcano: VolcanoRec(r, c); break;
    case kCalamityFire: FireRec(r, c); break;
    case kCalamityMorale: MoraleRec(r, c); break;
    case kCalamityTimeWarp: WarpRec(r, c); break;
    case kCalamityTeleport: TeleportRec(r, c); break;
    case kCalamityAreaEffect: AreaRec(r, c, 0); break;
    case kCalamityAEExtraDamage: AreaRec(r, c, c->mEffect2Time); break;
    case kCalamityAEMindControl: AreaRec(r, c, c->mEffect2Time); break;
    case kCalamityLogicBomb: BombRec(r, c); break;
    case kCalamityDropObject: DropRec(r, c); break;
    case kCalamityCloaking: CloakRec(r, c); break;
    case kCalamityMeteorStorm: MeteorRec(r, c); break;
    case kCalamityLineOfSight: SightRec(r, c); break;
    default: Media(r, NULL, NULL, NULL, NULL, NULL, NULL, NULL); break;   // special attack, conscription, location
    }
}

// a calamity's numeric state as its Load leaves it (no pointers)
void LoadedState(EESimCaptureRecord& r, EECalamity* k)
{
    r.F32(k->mRectangle.GetLeft());
    r.F32(k->mRectangle.GetTop());
    r.F32(k->mRectangle.GetRight());
    r.F32(k->mRectangle.GetBottom());
    r.F32(k->mTargetLocation.mX);
    r.F32(k->mTargetLocation.mY);
    r.F32(k->mTargetLocation.mZ);
    r.F32(k->mShakeLocation.mX);
    r.F32(k->mShakeLocation.mY);
    r.F32(k->mShakeLocation.mZ);
    r.U32((unsigned int)k->mEffectGeneration.size());
    for (size_t i = 0; i < k->mEffectGeneration.size(); i++)
        r.I32(k->mEffectGeneration[i]);
    r.U32(k->mTargetUnitID);
    r.I32(k->mClass != NULL ? k->mClass->mClassEnum : -1);
    r.F32(k->mEpicenterFactor);
    r.F32(k->mFloatPlaceHolder1);
    r.F32(k->mFloatPlaceHolder2);
    r.F32(k->mHeight);
    r.I32(k->mIntPlaceHolder1);
    r.I32(k->mIntPlaceHolder2);
    r.F32(k->mTimeAlive);
    r.U32(k->mOriginatingUnitID);
    r.U32(k->mPlayerNumber);
    r.F32(k->mDiscreteUpdateTime);
    r.F32(k->mTimeElapsedSinceLastUpdate);
    r.U32((unsigned int)k->mTilePlaceHolder1.mX);
    r.U32((unsigned int)k->mTilePlaceHolder1.mY);
    r.U32(k->mEffectHandle.mValue);
    r.U32((k->mBoolPlaceHolder1 ? 1u : 0u) | (k->mBoolPlaceHolder2 ? 2u : 0u) | (k->mInitialized ? 4u : 0u) |
          (k->mHasTargetLocation ? 8u : 0u) | (k->mHasTargetUnit ? 16u : 0u));
    r.U32(k->mTargetUnit != NULL || k->mCasterUnit != NULL || k->mSound != NULL || k->mSecondarySound != NULL);
}

void FileBytes(EESimCaptureRecord& r, FSMemoryFile& inFile)
{
    long theLength = 0;
    inFile.GetPosition(theLength);
    r.U32((unsigned int)theLength);
    if (theLength > 0)
        r.Bytes(inFile.GetData(), (unsigned int)theLength);
}

}   // namespace

void EECalamityCaptureClasses()
{
    if (!EESimCaptureRecord::Active())
        return;
    EECalamityManager* theManager = EECalamityManager::GetInstance();
    for (unsigned long i = 0; i < theManager->mCalamityClasses.size(); i++) {
        EECalamityClass* c = theManager->mCalamityClasses[i];
        EESimCaptureRecord r(EECaptureTag("CALI"));
        r.U32(i);
        r.U32(c != NULL ? 1 : 0);
        if (c != NULL) {
            r.F32(c->mLifeTime);
            r.I32(c->mClassEnum);
            r.U32(c->mClassFlags);
            r.U32(c->mUnitSet);
            r.I32(c->mNameID);
            r.I32((long)c->mID);
            r.I32(c->mRange);
            r.I32(c->mCost);
            r.U32((c->mB24 ? 1u : 0u) | (c->mIsBlockedByImmunity ? 2u : 0u) | (c->mIsCastByAllSelected ? 4u : 0u));
            r.I32(c->GetTargetKind());
            r.I32(c->GetCost());
            r.U32(c->GetUnitSet());
            r.U32(c->GetContagionFunctor() != NULL ? 1 : 0);
            ClassRecord(r, c);
        }
        r.Commit();
    }
}

void EECalamityCaptureSave(EECalamity* k, unsigned long inIndex)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord r(EECaptureTag("CALK"));
    r.U32(0);
    r.U32(inIndex);
    r.I32(k->mClass->mClassEnum);
    r.U32((unsigned int)k->GetOutOfSynchChecksum());
    FSMemoryFile theFile;
    r.I32(k->Save(0, kSaveGameMinorVersion, theFile));
    FileBytes(r, theFile);
    theFile.SetPosition(0, kFileOriginBegin, false);
    EECalamity* theCopy = new EECalamity;
    r.I32(theCopy->Load(0, kSaveGameMinorVersion, theFile));
    LoadedState(r, theCopy);
    r.U32((unsigned int)theCopy->GetOutOfSynchChecksum());
    theCopy->mOriginatingUnitID = 0;   // (no caster unit and an id: the destructor leaves the class's media alone)
    delete theCopy;
    r.Commit();
}

void EECalamityCaptureContagionSave()
{
    if (!EESimCaptureRecord::Active())
        return;
    EeContagionManager* theContagion = ::gWorld.GetContagionManager()->mRust;
    EESimCaptureRecord r(EECaptureTag("CALK"));
    r.U32(1);
    r.U32((unsigned int)ee_contagion_manager_checksum(theContagion));
    r.U32((unsigned int)ee_contagion_manager_count(theContagion));
    for (size_t i = 0; i < ee_contagion_manager_count(theContagion); i++)
        r.U32((unsigned int)ee_contagion_helper_checksum(ee_contagion_manager_at(theContagion, i)));
    FSMemoryFile theFile;
    r.I32(ee_contagion_manager_save(theContagion, (uint64_t)(uintptr_t)&theFile, true));
    FileBytes(r, theFile);
    theFile.SetPosition(0, kFileOriginBegin, false);
    EeContagionManager* theCopy = ee_contagion_manager_new();
    r.I32(ee_contagion_manager_load(theCopy, (uint64_t)(uintptr_t)&theFile, 0, kSaveGameMinorVersion, true));
    r.I32(ee_contagion_manager_next_id(theCopy));
    r.U32((unsigned int)ee_contagion_manager_count(theCopy));
    for (size_t i = 0; i < ee_contagion_manager_count(theCopy); i++) {
        EeContagionHelper* h = ee_contagion_manager_at(theCopy, i);
        EeContagionHelperFields theFields = ee_contagion_helper_fields(h);
        r.U32(ee_contagion_manager_calamity_index(theCopy, i));
        r.I32(theFields.mHelperID);
        r.F32(theFields.mTimeSinceLastSpread);
        r.F32(theFields.mTimeSinceLastUpdate);
        r.U32((unsigned int)ee_contagion_helper_count(h, true));
        for (size_t j = 0; j < ee_contagion_helper_count(h, true); j++)
            r.U32((unsigned int)ee_contagion_helper_unit(h, true, j));
        r.U32((unsigned int)ee_contagion_helper_count(h, false));
    }
    // a helper loaded at the oldest version's layout (no update timer) from the first helper's bytes
    if (ee_contagion_manager_count(theContagion) > 0) {
        FSMemoryFile theHelperFile;
        r.I32(ee_contagion_helper_save(ee_contagion_manager_at(theContagion, 0), (uint64_t)(uintptr_t)&theHelperFile));
        theHelperFile.SetPosition(0, kFileOriginBegin, false);
        EeContagionHelperFields theOld;
        size_t theOldIDs = 0;
        r.I32(ee_contagion_helper_load_probe((uint64_t)(uintptr_t)&theHelperFile, 0, 0xe6, &theOld, &theOldIDs));
        r.I32(theOld.mHelperID);
        r.F32(theOld.mTimeSinceLastSpread);
        r.F32(theOld.mTimeSinceLastUpdate);
        r.U32((unsigned int)theOldIDs);
    }
    ee_contagion_manager_delete_helpers(theCopy);
    ee_contagion_manager_free(theCopy);
    r.Commit();
}

void EECalamityCaptureSaves()
{
    if (!EESimCaptureRecord::Active())
        return;
    EECalamityManager* theManager = EECalamityManager::GetInstance();
    for (unsigned long i = 0; i < theManager->mCalamities.size(); i++)
        EECalamityCaptureSave(theManager->mCalamities[i], i);
    EECalamityCaptureContagionSave();
}

void EECalamityCaptureAged(EECalamity* inCalamity, unsigned long inIndex, float inBefore)
{
    // every five seconds of a calamity's life (its time alive crossing a multiple of 5000)
    if (!EESimCaptureRecord::Active() || (long)(inBefore / 5000.0f) == (long)(inCalamity->mTimeAlive / 5000.0f))
        return;
    EECalamityCaptureSave(inCalamity, inIndex);
    EECalamityCaptureContagionSave();
}
