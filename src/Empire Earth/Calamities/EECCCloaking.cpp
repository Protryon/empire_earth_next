// C:\EEX\Empire Earth\Calamities\EECCCloaking.cpp (file name inferred: no __FILE__; the class's members in vtable
// order, then its two helpers). The cloaking calamity class: 0xaf8c0..0xafdd3. Unit flags: the exe's module set.
// Reset and Virtual34 are ICF-folded into other units' copies (0x157269, 0xb1c2f).
// Data (.rdata, in order): the vtable 0x43c854 (COL 0x43c850), -10000.0f 0x43c8a4.
#include <windows.h>
#include <math.h>

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

#include "ee/EEUnitKind.h"
#include "ee/EEEffectManager.h"
#include "ee/EECalamityUpdate.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EEGlobalCalamityClass.h"
#include "ee/EECCCloaking.h"
#include "ee/EEAEManager.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEGameSound.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

// FUNCTION: EXE 0x4af8c0
// Creates the cloaking calamity class with no effect or sound
EECCCloaking::EECCCloaking()
{
    this->mGraphicEffect = NULL;
    this->mSound = NULL;
}

// SYNTHETIC: EXE 0x4af8da
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4af8f6
// Destroys the calamity class, releasing its effect
EECCCloaking::~EECCCloaking()
{
    if (this->mGraphicEffect != NULL)
        this->mGraphicEffect->Dereference();
}

// FUNCTION: EXE 0x4af915
// Cloaks the caster: gives it the cloak area effect, plays the sound and the effect on it; a capitol
// also makes the capitols grouped around it cast cloaking. Returns true
bool EECCCloaking::Start(EECalamity* inCalamity)
{
    // the decisions are Rust (ee_tech calamity_start.rs; ee/EECalamityUpdate.h); the world steps are StartStep's
    EeCalamityParams theParams = EECalamityBaseParams(this, this->mSound, NULL);
    return EECalamityRunStart(this, inCalamity, theParams, this->mSound, NULL, &EECCCloaking::StartStep);
}

// FUNCTION: EXE 0x4afa26
// Uncloaks the caster: removes its area effect of kind 8 and stops the sound
void EECCCloaking::Stop(EECalamity* inCalamity)
{
    // the decisions are Rust (ee_tech calamity_start.rs; ee/EECalamityUpdate.h); the world steps are StartStep's
    EeCalamityParams theParams = EECalamityBaseParams(this, this->mSound, NULL);
    EECalamityRunStop(this, inCalamity, theParams, this->mSound, NULL, &EECCCloaking::StartStep);
}

// FUNCTION: EXE 0x4afa68
// Reads the calamity data: sound and effect; the effect's duration becomes the cloak's lifetime
void EECCCloaking::Initialize(EECalamityData* inData)
{
    EECalamityClass::Initialize(inData);
    if (inData->mSoundID != 0)
        this->mSound = EESoundObject::Get(inData->mSoundID);
    if (inData->mGraphicEffectID1 == 0)
        return;
    this->mGraphicEffect = EEGraphicEffect::Get(inData->mGraphicEffectID1);
    this->mGraphicEffect->Reference();
    this->mLifeTime = this->mGraphicEffect->mDuration;
}

// ICF-FOLDED: EXE 0x557269
// (empty: folded into a shared empty function)
void EECCCloaking::Reset(EECalamity* inCalamity)
{
}

// FUNCTION: EXE 0x4afabe
// Loads the cloak's effect and sound
void EECCCloaking::LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand)
{
    if (this->mGraphicEffect != NULL)
        this->mGraphicEffect->Load(inIsOnDemand);
    if (this->mSound != NULL)
        this->mSound->LoadMedia();
}

// ICF-FOLDED: EXE 0x4b1c2f
// (folded into a later unit's identical copy)
void EECCCloaking::UnloadMedia(EEPlayer* inPlayer)
{
    if (this->mGraphicEffect != NULL)
        this->mGraphicEffect->Unload();
    if (this->mSound != NULL)
        this->mSound->UnloadMedia();
}

// FUNCTION: EXE 0x4afae1
// Ages the cloak and updates its sound; returns 7 at the end of its lifetime (Rust)
long EECCCloaking::Update(EECalamity* inCalamity, float inElapsedTime)
{
    // the update is Rust (ee_tech calamity_update.rs; ee/EECalamityUpdate.h)
    EeCalamityParams theParams = EECalamityBaseParams(this, this->mSound, NULL);
    return EECalamityRunUpdate(this, inCalamity, inElapsedTime, theParams, this->mSound, NULL, NULL);
}

// ICF-FOLDED: EXE 0x6135bc
// (`return true`: folded into a later unit's identical function)
bool EECCCloaking::Virtual4C(unsigned long, unsigned long)
{
    return true;
}

// FUNCTION: EXE 0x4afb38
// Collects the player's capitols chained within twice the cloak radius of the caster and makes each
// other one cast cloaking too (a capitol near several members is listed more than once)
void EECCCloaking::CloakGroup(EECalamity* inCalamity, EEComplexUnit* inUnit, EEAreaEffect* inEffect)
{
    std::vector<EEUnit*> theCandidates;
    std::vector<EEUnit*> theGroup;
    EEGetPlayerUnitsOfType(kUnitTypeCapitol, inUnit->mPlayer->mPlayerID, theCandidates);
    float theRange = inEffect->GetRadius() * 2.0f;
    theGroup.push_back(inUnit);
    bool theAdded;
    do {
        theAdded = false;
        for (unsigned long i = 0; i < theCandidates.size(); i++) {
            EEUnit* theCandidate = EEUnitCast<EEComplexUnit>(theCandidates[i]);
            bool theFound = false;
            for (unsigned long j = 0; j < theGroup.size(); j++) {
                if (theCandidate == theGroup[j]) {
                    theFound = true;
                    break;
                }
            }
            if (theFound)
                continue;
            GE3DPoint theCandidateLocation;
            theCandidate->GetPosition(theCandidateLocation);
            unsigned long theGroupSize = theGroup.size();
            for (unsigned long k = 0; k < theGroupSize; k++) {
                GE3DPoint theMemberLocation;
                theGroup[k]->GetPosition(theMemberLocation);
                float theDistanceX = fabs(theCandidateLocation.GetX() - theMemberLocation.GetX());
                float theDistanceY = fabs(theCandidateLocation.GetY() - theMemberLocation.GetY());
                if (theDistanceX <= theRange && theDistanceY <= theRange) {
                    theGroup.push_back(theCandidate);
                    theAdded = true;
                }
            }
        }
    } while (theAdded);
    for (unsigned long i = 0; i < theGroup.size(); i++) {
        EEComplexUnit* theMember = EEUnitCast<EEComplexUnit>(theGroup[i]);
        if (theMember != inUnit)
            this->CloakUnit(inCalamity, theMember);
    }
}

// FUNCTION: EXE 0x4afd37
// Makes a living capitol that is not already casting cast cloaking on itself, topping its power up to
// the cost first
void EECCCloaking::CloakUnit(EECalamity* inCalamity, EEComplexUnit* inUnit)
{
    if (!(inUnit->mHitPoints > 0) || (inUnit->mGoalState != NULL && inUnit->mGoalState->mKind == kGoalCastCalamity))
        return;
    if (inUnit->GetPower() < inCalamity->mClass->GetCost()) {
        long thePower = inCalamity->mClass->GetCost();
        if (inUnit->mPower != thePower)
            inUnit->mPower = thePower;
    }
    inUnit->SetGoal(EEUGRust::NewCastCalamity(inUnit, 0, inUnit->GetTileX(), inUnit->GetTileY(), 0x43d, true), 0, 0);
    inUnit->StartAction(kActionCalamityPreCast);
    inUnit->mUnitClass->ActionChanged(inUnit);
}

// New (port): EECCCloaking's own steps for its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
bool EECCCloaking::StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply)
{
    EECCCloaking* theClass = static_cast<EECCCloaking*>(ioHost.mClass);
    EECalamity* theCalamity = ioHost.mCalamity;
    (void)theClass;
    if (inCall.op != EeCalamityOp::ClassStep)
        return false;
    switch (inCall.index) {
    case 1: {
        // the cloak's area effect on the caster, the target its position, the sound, the cloak flag; a capitol's group
        EEComplexUnit* theUnit = EEUnitCast<EEComplexUnit>(theCalamity->GetCasterUnit());
        EEAreaEffect* theEffect = gAEManager.CreateAreaEffect(0x2e, theUnit);
        theUnit->mAreaEffects.Add(theEffect);
        theEffect->Dereference();
        GE3DPoint theLocation;
        theUnit->GetPosition(theLocation);
        theCalamity->mTargetLocation = theLocation;
        theCalamity->SetSound(gSoundManager->PlayAndReturnReferencedNode(theClass->mSound->GetSampleID(), theClass->mSound->GetVolume(),
                                                                         0.0f, 1.0f, theClass->mSound->GetLooping(), NULL));
        if (theCalamity->mSound != NULL)
            theCalamity->mSound->Dereference();
        theUnit->mCloaked = 1;
        ioHost.mPending = theEffect;
        outReply.ok = theUnit->mUnitClass->mUnitType == kUnitTypeCapitol;
        return true;
    }
    case 2:
        theClass->CloakGroup(theCalamity, EEUnitCast<EEComplexUnit>(theCalamity->GetCasterUnit()),
                             static_cast<EEAreaEffect*>(ioHost.mPending));
        return true;
    case 3:
        gEffectManager->CreateUnitEffect(theCalamity->mCasterUnit, theClass->mGraphicEffect, NULL, 0, 0, 0);
        return true;
    case 4:
        EEUnitCast<EEComplexUnit>(theCalamity->GetCasterUnit())->mAreaEffects.Remove(8);
        return true;
    case 5:
        EEUnitCast<EEComplexUnit>(theCalamity->GetCasterUnit())->mCloaked = 0;
        return true;
    default:
        return false;
    }
}
