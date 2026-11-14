// C:\EEX\Empire Earth\Calamities\EECalamityClass.cpp (file name inferred: no __FILE__; the class's members in vtable
// order). The base of every calamity class: 0xadcd9..0xadf97. Unit flags: the exe's module set.
// Data (.rdata, in order): -10000.0f 0x43c6bc (not referenced by the unit's code), the vtable 0x43c6c4 (COL 0x43c6c0).
#include <windows.h>

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

#include "ee/EEEffectManager.h"
#include "ee/EECalamityUpdate.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EEAEManager.h"
#include "ee/EEContagionHelper.h"
#include "ee/EEContagionManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEMedia.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EESoundObject.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "rust/ee_tech.h"
#include "ee/EETechCapture.h"
#include "ee/EEGameVariant.h"
#include "lle/USingleton.h"
#include "ee/EECalamityManager.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEGlobalCalamityClass.h"
#include "ee/EELocationCalamityClass.h"
#include "ee/EEUnitCalamityClass.h"

static_assert(sizeof(EECalamityData) == sizeof(EeCalamityData), "the db record as ee_tech reads it");

// FUNCTION: EXE 0x4adcd9
// Creates a calamity class with no numbers, effects, sounds or functor
EECalamityClass::EECalamityClass()
    : EeCalamityClassFields(), mGraphicEffect1(NULL), mGraphicEffect2(NULL), mGraphicEffect3(NULL),
      mGraphicEffect4(NULL), mSound(NULL), mSecondarySound(NULL), mFunctor(NULL)
{
}

// FUNCTION: EXE 0x4adceb
// Returns the power cost of casting the calamity
long EECalamityClass::GetCost()
{
    return this->mCost;
}

// FUNCTION: EXE 0x4adcef
// Returns -1: no default target
long EECalamityClass::GetDefaultTarget(GE3DPoint&, unsigned long, unsigned long, bool)
{
    return -1;
}

// FUNCTION: EXE 0x4adcf5
// Returns the unit set the calamity affects
unsigned long EECalamityClass::GetUnitSet()
{
    return this->mUnitSet;
}

// SYNTHETIC: EXE 0x4adcf9
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4add15
// Destroys the calamity class: its class's releases in its order (effects dereferenced, the functor deleted, the
// volcano's sample unloaded; ee_tech calamity_class.rs media_step)
EECalamityClass::~EECalamityClass()
{
    EEGraphicEffect* theEffects[4] = {this->mGraphicEffect1, this->mGraphicEffect2, this->mGraphicEffect3,
                                      this->mGraphicEffect4};
    for (uint32_t i = 0;; i++) {
        uint32_t theStep = ee_calamity_class_media_step((int32_t)this->mClassEnum, EE_CALAMITY_MEDIA_DESTROY, i);
        if (theStep == EE_CALAMITY_STEP_END)
            break;
        if (theStep >= EE_CALAMITY_STEP_EFFECT1 && theStep < EE_CALAMITY_STEP_EFFECT1 + 4) {
            if (theEffects[theStep - EE_CALAMITY_STEP_EFFECT1] != NULL)
                theEffects[theStep - EE_CALAMITY_STEP_EFFECT1]->Dereference();
        } else if (theStep == EE_CALAMITY_STEP_FUNCTOR) {
            delete this->mFunctor;
            this->mFunctor = NULL;
        } else if (theStep == EE_CALAMITY_STEP_SAMPLE && this->mSound != NULL)
            gSoundManager->Unload(this->mSound->GetSampleID());
    }
}

// FUNCTION: EXE 0x4add1c
// Reads the calamity data: the common fields (class, lifetime, unit set, cost, range, immunity and
// cast-by-all-selected flags, name and id), then the class's own: its effects and sounds fetched as it fetched them,
// its numbers, its contagion functor (ee_tech calamity_class.rs)
void EECalamityClass::Initialize(EECalamityData* inData)
{
    const EeCalamityData* theData = reinterpret_cast<const EeCalamityData*>(inData);
    ee_calamity_class_initialize_base(this, theData);
    const int32_t theIDs[6] = {inData->mGraphicEffectID1, inData->mGraphicEffectID2, inData->mGraphicEffectID3,
                               inData->mGraphicEffectID4, inData->mSoundID, inData->mSecondarySoundID};
    EEGraphicEffect** theEffects[4] = {&this->mGraphicEffect1, &this->mGraphicEffect2, &this->mGraphicEffect3,
                                       &this->mGraphicEffect4};
    for (uint32_t i = 0; i < 6; i++) {
        if (!ee_calamity_class_fetches(ee_calamity_class_get_mode(inData->mClassEnum, i), theIDs[i]))
            continue;
        if (i < 4) {
            *theEffects[i] = EEGraphicEffect::Get(theIDs[i]);
            if (*theEffects[i] != NULL)
                (*theEffects[i])->Reference();
        } else if (i == 4)
            this->mSound = EESoundObject::Get(theIDs[i]);
        else
            this->mSecondarySound = EESoundObject::Get(theIDs[i]);
    }
    EeCalamityClassMedia theMedia;
    this->GetMedia(theMedia);
    ee_calamity_class_fill(this, theData, &theMedia);
    uint32_t theKind = ee_calamity_class_functor_kind(inData->mClassEnum);
    if (theKind != EE_CALAMITY_FUNCTOR_NONE)
        this->mFunctor = new EEContagionFunctor(this, theKind);
}

// New (port): the class's effects (whether set, duration, size, delay) and sounds for the Rust tables
void EECalamityClass::GetMedia(EeCalamityClassMedia& outMedia)
{
    outMedia = EeCalamityClassMedia();
    EEGraphicEffect* theEffects[4] = {this->mGraphicEffect1, this->mGraphicEffect2, this->mGraphicEffect3,
                                      this->mGraphicEffect4};
    for (uint32_t i = 0; i < 4; i++) {
        if (theEffects[i] == NULL)
            continue;
        outMedia.present |= EE_CALAMITY_MEDIA_EFFECT1 << i;
        outMedia.duration[i] = (uint32_t)theEffects[i]->mDuration;
        outMedia.size[i] = theEffects[i]->GetSize();
        outMedia.time1[i] = (uint32_t)theEffects[i]->mTime1;
    }
    if (this->mSound != NULL)
        outMedia.present |= EE_CALAMITY_MEDIA_SOUND;
    if (this->mSecondarySound != NULL)
        outMedia.present |= EE_CALAMITY_MEDIA_SECONDARY_SOUND;
}

// The class's Start: its parameters and sounds (ee_tech calamity_class.rs run), the decisions in Rust (ee_tech
// calamity_start.rs; ee/EECalamityUpdate.h), the world steps the class's (EECalamityUpdate.cpp)
bool EECalamityClass::Start(EECalamity* inCalamity)
{
    EeCalamityClassMedia theMedia;
    this->GetMedia(theMedia);
    EeCalamityClassRun theRun = ee_calamity_class_run(this, &theMedia, EE_CALAMITY_RUN_START);
    return EECalamityRunStart(this, inCalamity, theRun.params, (theRun.sounds & EE_CALAMITY_RUN_SOUND) ? this->mSound : NULL,
                              (theRun.sounds & EE_CALAMITY_RUN_SECONDARY_SOUND) ? this->mSecondarySound : NULL);
}

// The class's Stop (none for the classes whose Stop was empty), as Start
void EECalamityClass::Stop(EECalamity* inCalamity)
{
    EeCalamityClassMedia theMedia;
    this->GetMedia(theMedia);
    EeCalamityClassRun theRun = ee_calamity_class_run(this, &theMedia, EE_CALAMITY_RUN_STOP);
    if (!theRun.runs)
        return;
    EECalamityRunStop(this, inCalamity, theRun.params, (theRun.sounds & EE_CALAMITY_RUN_SOUND) ? this->mSound : NULL,
                      (theRun.sounds & EE_CALAMITY_RUN_SECONDARY_SOUND) ? this->mSecondarySound : NULL);
}

// The class's Update: its parameters (ee_tech calamity_class.rs run; the game variant's earthquake damage factor),
// the update in Rust (ee_tech calamity_update.rs; ee/EECalamityUpdate.h), the class's world steps; 7 once
// the calamity is over
long EECalamityClass::Update(EECalamity* inCalamity, float inElapsedTime)
{
    EeCalamityClassMedia theMedia;
    this->GetMedia(theMedia);
    theMedia.quake_damage_factor = EEGameVariant::GetGameVariant(::gWorld.mGameOptions.mVariant)->GetEarthquakeDamageFactor();
    EeCalamityClassRun theRun = ee_calamity_class_run(this, &theMedia, EE_CALAMITY_RUN_UPDATE);
    return EECalamityRunUpdate(this, inCalamity, inElapsedTime, theRun.params,
                               (theRun.sounds & EE_CALAMITY_RUN_SOUND) ? this->mSound : NULL,
                               (theRun.sounds & EE_CALAMITY_RUN_SECONDARY_SOUND) ? this->mSecondarySound : NULL);
}

// The class's Reset of a new calamity: its placeholders (ee_tech calamity_class.rs reset), the time warp's target
// tile, the fire's and the plague's contagion helper on the target
void EECalamityClass::Reset(EECalamity* inCalamity)
{
    EeCalamityReset theReset = EeCalamityReset();
    EEUnit* theTargetUnit = NULL;
    theReset.has_target = inCalamity->GetTargetUnit(theTargetUnit);
    if (theReset.has_target && theTargetUnit->mComplexUnit != NULL) {
        theReset.target_tile_x = theTargetUnit->mComplexUnit->mTileX;
        theReset.target_tile_y = theTargetUnit->mComplexUnit->mTileY;
        theReset.target_hit_points = theTargetUnit->mComplexUnit->mHitPoints;
    }
    theReset.bool1 = inCalamity->mBoolPlaceHolder1;
    theReset.bool2 = inCalamity->mBoolPlaceHolder2;
    theReset.float1 = inCalamity->mFloatPlaceHolder1;
    theReset.int1 = inCalamity->mIntPlaceHolder1;
    theReset.int2 = inCalamity->mIntPlaceHolder2;
    theReset.tile_x = (long)inCalamity->mTilePlaceHolder1.mX;
    theReset.tile_y = (long)inCalamity->mTilePlaceHolder1.mY;
    ee_calamity_class_reset((int32_t)this->mClassEnum, &theReset);
    inCalamity->mBoolPlaceHolder1 = theReset.bool1;
    inCalamity->mBoolPlaceHolder2 = theReset.bool2;
    inCalamity->mFloatPlaceHolder1 = theReset.float1;
    inCalamity->mIntPlaceHolder1 = theReset.int1;
    inCalamity->mIntPlaceHolder2 = theReset.int2;
    inCalamity->mTilePlaceHolder1.SetXY(theReset.tile_x, theReset.tile_y);
    if (theReset.create_helper)
        inCalamity->mIntPlaceHolder1 = ::gWorld.GetContagionManager()->CreateContagionHelper(this->mFunctor, inCalamity,
                                                                                             theTargetUnit);
}

// FUNCTION: EXE 0x4add8a
// Ages the calamity (and its contagion helper, if given); returns 7 once its lifetime is over, else 0
long EECalamityClass::UpdateTimeAlive(EECalamity* inCalamity, float inElapsedTime, long inHelperID)
{
    // the age and the lifetime test: ee_tech calamity.rs (computed before the helper's update, as the C++ did)
    EeCalamityAge theAge = ee_calamity_update_time_alive(inCalamity->GetTimeAlive(), inElapsedTime, this->mLifeTime);
    if (inHelperID != -1) {
        EEContagionHelper* theHelper = ::gWorld.GetContagionManager()->GetContagionHelper(inHelperID);
        EECalamityCaptureEvent(kCalamityOpHelperUpdate, inHelperID, theHelper != NULL);   // CALW (TEST ONLY)
        if (theHelper != NULL)
            EECalamityCaptureContagion(theHelper, inElapsedTime);   // CONT (TEST ONLY): theHelper->Update
    }
    inCalamity->mTimeAlive = theAge.time_alive;
    return theAge.done ? 7 : 0;
}

// FUNCTION: EXE 0x4adde9
// Updates the calamity's main or secondary sound for the target location with the given falloff; a volume other than
// -1 replaces the computed one (clamped to 0..1)
void EECalamityClass::UpdateSound(EECalamity* inCalamity, EESoundObject* inSound, float inVolume, float inFalloff,
                                  bool inPrimary)
{
    GE3DPoint theLocation;
    theLocation = inCalamity->mTargetLocation;
    SESMInstanceNode* theNode;
    if (inPrimary)
        theNode = inCalamity->mSound;
    else
        theNode = inCalamity->mSecondarySound;
    if (theNode == NULL)
        return;
    float theVolume;
    float thePan;
    float thePitch;
    EEComputeSoundParameters(inSound->GetVolume(), theLocation, theVolume, thePan, thePitch, inFalloff);
    if (inVolume != -1.0f)
        theVolume = (1.0f < inVolume) ? 1.0f : max(0.0f, inVolume);
    gSoundManager->SetSoundParameters(theNode, theVolume, thePan, thePitch);
}

// FUNCTION: EXE 0x4ade97
// Checks whether the calamity can be cast at the tile with no caster
long EECalamityClass::CanCast(EEUnit* inTargetUnit, unsigned long inX, unsigned long inY, bool inCheck)
{
    GE3DPoint theLocation;
    return this->CanCastAt(NULL, theLocation, inX, inY, inCheck, inTargetUnit);
}

// FUNCTION: EXE 0x4adec7
// Returns 0 if the caster (or nature) is alive with enough power and sees the tile, 2 if immunity blocks it there,
// else 1; always 1 without the check flag
long EECalamityClass::CanCastAt(EEComplexUnit* inUnit, GE3DPoint& inLocation, unsigned long inX, unsigned long inY,
                                bool inCheck, EEUnit* inTargetUnit)
{
    // the rules are Rust (ee_tech calamity_cast.rs; ee/EECalamityUpdate.h)
    EECalamityCanCastScope theCapture;   // CCAS (TEST ONLY)
    return theCapture.Record(this, inUnit, inLocation, inX, inY, inCheck, inTargetUnit,
                             EECalamityCanCastAt(this, EeCalamityCastMode::Base, inUnit, inX, inY, inCheck, inTargetUnit));
}

// (the base's: every class's own, by tables keyed by its enum)
// Returns the class's contagion functor (none but for the fire, plague, leprosy and area effects)
class EEContagionFunctor * EECalamityClass::GetContagionFunctor()
{
    return this->mFunctor;
}

namespace {

// runs one of the class's media lists (ee_tech calamity_class.rs media_step) for LoadMedia or UnloadMedia
void MediaSteps(EECalamityClass* inClass, uint32_t inList, EEPlayer* inPlayer, bool inIsOnDemand)
{
    EEGraphicEffect* theEffects[4] = {inClass->mGraphicEffect1, inClass->mGraphicEffect2, inClass->mGraphicEffect3,
                                      inClass->mGraphicEffect4};
    bool theLoad = inList == EE_CALAMITY_MEDIA_LOAD;
    for (uint32_t i = 0;; i++) {
        uint32_t theStep = ee_calamity_class_media_step((int32_t)inClass->mClassEnum, inList, i);
        if (theStep == EE_CALAMITY_STEP_END)
            break;
        if (theStep >= EE_CALAMITY_STEP_EFFECT1 && theStep < EE_CALAMITY_STEP_EFFECT1 + 4) {
            EEGraphicEffect* theEffect = theEffects[theStep - EE_CALAMITY_STEP_EFFECT1];
            if (theEffect != NULL && theLoad)
                theEffect->Load(inIsOnDemand);
            else if (theEffect != NULL)
                theEffect->Unload();
        } else if (theStep == EE_CALAMITY_STEP_SOUND || theStep == EE_CALAMITY_STEP_SECONDARY_SOUND) {
            EESoundObject* theSound = theStep == EE_CALAMITY_STEP_SOUND ? inClass->mSound : inClass->mSecondarySound;
            if (theSound != NULL && theLoad)
                theSound->LoadMedia();
            else if (theSound != NULL)
                theSound->UnloadMedia();
        } else if (theStep == EE_CALAMITY_STEP_UNIT_MEDIA && theLoad) {
            if (inIsOnDemand == true)
                inPlayer->LoadUnitClassMedia(inClass->mUnitClassIndex);
            inPlayer->SetUnitClassMedia(inClass->mUnitClassIndex, 0x7f);
        } else if (theStep == EE_CALAMITY_STEP_UNIT_MEDIA)
            inPlayer->UnloadUnitClassMedia(inClass->mUnitClassIndex);
        else if (theStep == EE_CALAMITY_STEP_UNIT_BUTTONS && theLoad)
            inPlayer->LoadUnitClassButtons(inClass->mUnitClassIndex);
        else if (theStep == EE_CALAMITY_STEP_UNIT_BUTTONS)
            inPlayer->UnloadUnitClassButtons(inClass->mUnitClassIndex);
    }
}

}   // namespace

// (the base's: every class's own, by tables keyed by its enum)
// Loads the class's effects and sounds (and the unit class it places) in its order
void EECalamityClass::LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand)
{
    MediaSteps(this, EE_CALAMITY_MEDIA_LOAD, inPlayer, inIsOnDemand);
}

// ICF-FOLDED: EXE 0x6907a3 (the base's empty body); EECCVolcano::Place 0xb5137
// The volcano makes units step aside from its site at the tile; nothing for the other classes
void EECalamityClass::Place(EEUnit* inUnit, unsigned long inX, unsigned long inY)
{
    if (this->mClassEnum != kCalamityVolcano)
        return;
    EETileCoordinate theTile(inX, inY);
    EEStepAsideFromSite(NULL, gUnitClassTable[this->mUnitClassIndex], inUnit->mPlayer, theTile, 1);
}

// FUNCTION: EXE 0x4b0e20
// Returns the game variant with the given index
// Probably inline in the real header (an out-of-line COMDAT right after its first user, EECCEarthquake's Update);
// defined here, with the earthquake's update parameters (Update above)
EEGameVariant* EEGameVariant::GetGameVariant(long inIndex)
{
    return gGameVariants[inIndex];
}

// (the base's: every class's own, by tables keyed by its enum)
// Starts the calamity's sound: the earthquake's, hurricane's and volcano's as its looping main sound, the meteor
// storm's once at the target (ee_tech calamity_class.rs sound_mode); nothing for the others
void EECalamityClass::PlaySound(EECalamity* inCalamity)
{
    uint32_t theMode = ee_calamity_class_sound_mode((int32_t)this->mClassEnum);
    if (theMode == EE_CALAMITY_PLAY_NODE) {
        inCalamity->SetSound(gSoundManager->PlayAndReturnReferencedNode(this->mSound->GetSampleID(), this->mSound->GetVolume(),
                                                                        0.0f, 1.0f, this->mSound->GetLooping(), NULL));
        if (inCalamity->mSound != NULL)
            inCalamity->mSound->Dereference();
    } else if (theMode == EE_CALAMITY_PLAY_AT_TARGET) {
        GE3DPoint theLocation;
        float theVolume;
        float thePan;
        float thePitch;
        theLocation = inCalamity->mTargetLocation;
        EEComputeSoundParameters(this->mSound->GetVolume(), theLocation, theVolume, thePan, thePitch, kCalamitySoundFalloff);
        gSoundManager->Play(this->mSound->GetSampleID(), theVolume, thePan, thePitch, false, NULL);
    }
}

// Unloads the class's effects and sounds (and the unit class it places) in its order
void EECalamityClass::UnloadMedia(EEPlayer* inPlayer)
{
    MediaSteps(this, EE_CALAMITY_MEDIA_UNLOAD, inPlayer, false);
}
