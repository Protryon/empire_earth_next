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
#include "ee/EECCAreaEffect.h"
#include "ee/EECCAEExtraDamage.h"
#include "ee/EECCAEMindControl.h"
#include "ee/EECCCloaking.h"
#include "ee/EECCConscription.h"
#include "ee/EECCDropObject.h"
#include "ee/EECCEarthquake.h"
#include "ee/EECCFire.h"
#include "ee/EECCHurricane.h"
#include "ee/EECCLeprosy.h"
#include "ee/EECCLineOfSight.h"
#include "ee/EECCLogicBomb.h"
#include "ee/EECCMeteorStorm.h"
#include "ee/EECCMorale.h"
#include "ee/EECCPlague.h"
#include "ee/EECCSpecialAttack.h"
#include "ee/EECCSpecialLocation.h"
#include "ee/EECCTeleport.h"
#include "ee/EECCTimeWarp.h"
#include "ee/EECCVolcano.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEContagionHelper.h"
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

void Functor(EESimCaptureRecord& ioRecord, EEContagionFunctor* inFunctor)
{
    if (inFunctor == NULL) {
        ioRecord.U32(0);
        return;
    }
    if (EEAEExtraDamageFunctor* f = dynamic_cast<EEAEExtraDamageFunctor*>(inFunctor)) {
        ioRecord.U32(5);
        ioRecord.U32(f->mFlag ? 1 : 0);
    } else if (EEAEMindControlFunctor* f = dynamic_cast<EEAEMindControlFunctor*>(inFunctor)) {
        ioRecord.U32(6);
        ioRecord.I32(EffectIndex(f->mEffect));
    } else if (dynamic_cast<EEAreaEffectFunctor*>(inFunctor) != NULL)
        ioRecord.U32(4);
    else if (EEFireFunctor* f = dynamic_cast<EEFireFunctor*>(inFunctor)) {
        ioRecord.U32(1);
        ioRecord.I32(EffectIndex(f->mEffect));
        ioRecord.F32(f->mRadius);
        ioRecord.F32(f->mChance);
        ioRecord.I32(f->mBurn);
        ioRecord.I32(f->mDamage);
        ioRecord.U32(f->mSpreadTime);
        ioRecord.U32(f->mUnitSet);
        ioRecord.I32(f->mMode);
    } else if (EELeprosyFunctor* f = dynamic_cast<EELeprosyFunctor*>(inFunctor)) {
        ioRecord.U32(2);
        ioRecord.I32(f->mDamage);
        ioRecord.U32(f->mSpreadTime);
        ioRecord.U32(f->mUnitSet);
        ioRecord.I32(f->mMode);
    } else if (EEPlagueFunctor* f = dynamic_cast<EEPlagueFunctor*>(inFunctor)) {
        ioRecord.U32(3);
        ioRecord.F32(f->mRadius);
        ioRecord.F32(f->mChance);
        ioRecord.I32(f->mDamage);
        ioRecord.U32(f->mDamageTime);
        ioRecord.U32(f->mUnitSet);
        ioRecord.I32(f->mMode);
    } else
        ioRecord.U32(7);
    if (EEAreaEffectFunctor* f = dynamic_cast<EEAreaEffectFunctor*>(inFunctor)) {
        ioRecord.I32(EffectIndex(f->mEffect2));
        ioRecord.I32(EffectIndex(f->mEffect3));
        ioRecord.I32(f->mDamage);
        ioRecord.I32(f->mUpdateTime);
        ioRecord.U32(f->mValue);
        ioRecord.I32(f->mMode);
    }
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

// the classes' protected members, read through a derived type
struct AreaPeek : EECCAreaEffect {
    static void Rec(EESimCaptureRecord& r, EECalamityClass* c, long e2t)
    {
        AreaPeek* p = static_cast<AreaPeek*>(c);
        Media(r, p->mGraphicEffect1, p->mGraphicEffect2, p->mGraphicEffect3, NULL, p->mSound, NULL, p->mFunctor);
        r.I32(p->mDamageFactor);
        r.U32(p->mEffect1Duration);
        r.U32(p->mRadiusTiles);
        r.I32(e2t);
    }
};
struct ExtraPeek : EECCAEExtraDamage {
    static long Time(EECalamityClass* c) { return static_cast<ExtraPeek*>(c)->mEffect2Time; }
};
struct MindPeek : EECCAEMindControl {
    static long Time(EECalamityClass* c) { return static_cast<MindPeek*>(c)->mEffect2Time; }
};
struct CloakPeek : EECCCloaking {
    static void Rec(EESimCaptureRecord& r, EECalamityClass* c)
    {
        CloakPeek* p = static_cast<CloakPeek*>(c);
        Media(r, p->mGraphicEffect1, NULL, NULL, NULL, p->mSound, NULL, NULL);
    }
};
struct DropPeek : EECCDropObject {
    static void Rec(EESimCaptureRecord& r, EECalamityClass* c)
    {
        DropPeek* p = static_cast<DropPeek*>(c);
        Media(r, p->mGraphicEffect1, p->mGraphicEffect2, p->mGraphicEffect3, NULL, p->mSound, NULL, NULL);
        r.I32(p->mUnitClassIndex);
    }
};
struct QuakePeek : EECCEarthquake {
    static void Rec(EESimCaptureRecord& r, EECalamityClass* c)
    {
        QuakePeek* p = static_cast<QuakePeek*>(c);
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
};
struct FirePeek : EECCFire {
    static void Rec(EESimCaptureRecord& r, EECalamityClass* c)
    {
        FirePeek* p = static_cast<FirePeek*>(c);
        Media(r, p->mGraphicEffect1, p->mGraphicEffect2, NULL, NULL, p->mSound, NULL, p->mFunctor);
        r.I32(p->mBurn);
        r.I32(p->mDamage);
        r.F32(p->mRadius);
        r.F32(p->mChance);
    }
};
struct StormPeek : EECCHurricane {
    static void Rec(EESimCaptureRecord& r, EECalamityClass* c)
    {
        StormPeek* p = static_cast<StormPeek*>(c);
        Media(r, p->mGraphicEffect1, p->mGraphicEffect2, p->mGraphicEffect3, NULL, p->mSound, NULL, NULL);
        r.I32(p->mDamage);
        r.U32(p->mTickTime);
        r.I32(p->mArea);
        r.I32(p->mUnitClassIndex);
        r.F32(p->mRadius);
        r.U32(p->mOverWater ? 1 : 0);
    }
};
struct LeprosyPeek : EECCLeprosy {
    static void Rec(EESimCaptureRecord& r, EECalamityClass* c)
    {
        LeprosyPeek* p = static_cast<LeprosyPeek*>(c);
        Media(r, p->mGraphicEffect1, NULL, NULL, NULL, p->mSound, NULL, p->mFunctor);
        r.I32(p->mDamage);
        r.U32(p->mEffect1Duration);
        r.U32(p->mRadiusTiles);
    }
};
struct SightPeek : EECCLineOfSight {
    static void Rec(EESimCaptureRecord& r, EECalamityClass* c)
    {
        SightPeek* p = static_cast<SightPeek*>(c);
        Media(r, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        r.F32(p->mSightBonus);
    }
};
struct BombPeek : EECCLogicBomb {
    static void Rec(EESimCaptureRecord& r, EECalamityClass* c)
    {
        BombPeek* p = static_cast<BombPeek*>(c);
        Media(r, p->mGraphicEffect1, p->mGraphicEffect2, p->mGraphicEffect3, NULL, p->mSound, NULL, NULL);
        r.U32(p->mEffectTime2);
        r.U32(p->mEffectTime3);
    }
};
struct MeteorPeek : EECCMeteorStorm {
    static void Rec(EESimCaptureRecord& r, EECalamityClass* c)
    {
        MeteorPeek* p = static_cast<MeteorPeek*>(c);
        Media(r, NULL, NULL, NULL, NULL, p->mSound, NULL, NULL);
        r.F32(p->mMinimumInterval);
        r.F32(p->mMaximumInterval);
        r.F32(p->mRadius);
        r.I32(p->mUnitClassIndex);
    }
};
struct MoralePeek : EECCMorale {
    static void Rec(EESimCaptureRecord& r, EECalamityClass* c)
    {
        MoralePeek* p = static_cast<MoralePeek*>(c);
        Media(r, p->mGraphicEffect1, NULL, NULL, NULL, p->mSound, p->mSecondarySound, NULL);
        r.I32(p->mEffectDuration);
        r.U32(p->mPositive ? 1 : 0);
    }
};
struct PlaguePeek : EECCPlague {
    static void Rec(EESimCaptureRecord& r, EECalamityClass* c)
    {
        PlaguePeek* p = static_cast<PlaguePeek*>(c);
        Media(r, p->mGraphicEffect1, NULL, NULL, NULL, p->mSound, p->mSecondarySound, p->mFunctor);
        r.I32(p->mDamage);
    }
};
struct TeleportPeek : EECCTeleport {
    static void Rec(EESimCaptureRecord& r, EECalamityClass* c)
    {
        TeleportPeek* p = static_cast<TeleportPeek*>(c);
        Media(r, p->mGraphicEffect1, p->mGraphicEffect2, NULL, NULL, p->mSound, p->mSecondarySound, NULL);
        r.F32(p->mDamageShare);
        r.I32(p->mPlacementRange);
        r.I32(p->mHoldTime);
    }
};
struct WarpPeek : EECCTimeWarp {
    static void Rec(EESimCaptureRecord& r, EECalamityClass* c)
    {
        WarpPeek* p = static_cast<WarpPeek*>(c);
        Media(r, p->mGraphicEffect1, p->mGraphicEffect2, p->mGraphicEffect3, p->mGraphicEffect4, p->mSound,
              p->mSecondarySound, NULL);
        r.I32(p->mPlacementRange);
        r.I32(p->mGraphicEffect1 != NULL ? p->mDepartTime : 0);   // (not set without the first effect)
        r.I32(p->mAwayTime);
    }
};
struct VolcanoPeek : EECCVolcano {
    static void Rec(EESimCaptureRecord& r, EECalamityClass* c)
    {
        VolcanoPeek* p = static_cast<VolcanoPeek*>(c);
        Media(r, p->mGraphicEffect1, p->mGraphicEffect2, p->mGraphicEffect3, p->mGraphicEffect4, p->mSound, NULL, NULL);
        r.I32(p->mL3C);
        r.I32(p->mL40);
        r.I32(p->mUnitClassIndex);
        r.I32(p->mL48);
        r.F32(p->mMaximumRadius);
        r.F32(p->mMinimumRadius);
    }
};

void ClassRecord(EESimCaptureRecord& r, EECalamityClass* c)
{
    switch (c->mClassEnum) {
    case kCalamityEarthquake: QuakePeek::Rec(r, c); break;
    case kCalamityHurricane: StormPeek::Rec(r, c); break;
    case kCalamityPlague: PlaguePeek::Rec(r, c); break;
    case kCalamityLeprosy: LeprosyPeek::Rec(r, c); break;
    case kCalamityVolcano: VolcanoPeek::Rec(r, c); break;
    case kCalamityFire: FirePeek::Rec(r, c); break;
    case kCalamityMorale: MoralePeek::Rec(r, c); break;
    case kCalamityTimeWarp: WarpPeek::Rec(r, c); break;
    case kCalamityTeleport: TeleportPeek::Rec(r, c); break;
    case kCalamityAreaEffect: AreaPeek::Rec(r, c, 0); break;
    case kCalamityAEExtraDamage: AreaPeek::Rec(r, c, ExtraPeek::Time(c)); break;
    case kCalamityAEMindControl: AreaPeek::Rec(r, c, MindPeek::Time(c)); break;
    case kCalamityLogicBomb: BombPeek::Rec(r, c); break;
    case kCalamityDropObject: DropPeek::Rec(r, c); break;
    case kCalamityCloaking: CloakPeek::Rec(r, c); break;
    case kCalamityMeteorStorm: MeteorPeek::Rec(r, c); break;
    case kCalamityLineOfSight: SightPeek::Rec(r, c); break;
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
    EEContagionManager* theContagion = ::gWorld.GetContagionManager();
    EESimCaptureRecord r(EECaptureTag("CALK"));
    r.U32(1);
    r.U32((unsigned int)theContagion->GetOutOfSynchChecksum());
    r.U32((unsigned int)theContagion->mHelpers.size());
    for (size_t i = 0; i < theContagion->mHelpers.size(); i++)
        r.U32((unsigned int)theContagion->mHelpers[i]->GetOutOfSynchChecksum());
    FSMemoryFile theFile;
    r.I32(theContagion->Save(0, kSaveGameMinorVersion, theFile));
    FileBytes(r, theFile);
    theFile.SetPosition(0, kFileOriginBegin, false);
    EEContagionManager* theCopy = new EEContagionManager;
    r.I32(theCopy->Load(0, kSaveGameMinorVersion, theFile));
    r.I32(theCopy->mNextHelperID);
    r.U32((unsigned int)theCopy->mHelpers.size());
    for (size_t i = 0; i < theCopy->mHelpers.size(); i++) {
        EEContagionHelper* h = theCopy->mHelpers[i];
        r.U32(theCopy->mCalamityIndexes[i]);
        r.I32(h->mHelperID);
        r.F32(h->mTimeSinceLastSpread);
        r.F32(h->mTimeSinceLastUpdate);
        r.U32((unsigned int)h->mUnitIDs.size());
        for (size_t j = 0; j < h->mUnitIDs.size(); j++)
            r.U32(h->mUnitIDs[j]);
        r.U32((unsigned int)h->mUnits.size());
    }
    // a helper loaded at the oldest version's layout (no update timer) from the first helper's bytes
    if (!theContagion->mHelpers.empty()) {
        FSMemoryFile theHelperFile;
        r.I32(theContagion->mHelpers[0]->Save(0, kSaveGameMinorVersion, theHelperFile));
        theHelperFile.SetPosition(0, kFileOriginBegin, false);
        EEContagionHelper theOld;
        r.I32(theOld.Load(0, 0xe6, theHelperFile));
        r.I32(theOld.mHelperID);
        r.F32(theOld.mTimeSinceLastSpread);
        r.F32(theOld.mTimeSinceLastUpdate);
        r.U32((unsigned int)theOld.mUnitIDs.size());
    }
    DeleteVectorContents(theCopy->mHelpers);
    delete theCopy;
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
