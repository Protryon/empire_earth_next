// C:\EEX\Empire Earth\Calamities\EECCLogicBomb.cpp (file name inferred: no __FILE__; the class name). The logic bomb
// calamity class: 0xb2034..0xb2634. Unit flags: the exe's module set. Reset and Virtual2C are ICF-folded (0x157269),
// Virtual28/Virtual34 into the area effects' (0xaf5f2/0xaf632).
// Data (.rdata, in order): kNoEffectHandle 0x43cb18, kCalamitySoundFalloff 0x43cb20, the vtable 0x43cb28 (COL
// 0x43cb24).
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
#include "lle/GEModel.h"
#include "ee/EEEffectManager.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EEUnitCalamityClass.h"
#include "ee/EECCLogicBomb.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEGameSound.h"
#include "ee/EEMap.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EESoundObject.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

// FUNCTION: EXE 0x4b2034
// Creates the logic bomb calamity class with no effects, sound or stage times
EECCLogicBomb::EECCLogicBomb()
    : mGraphicEffect1(NULL), mGraphicEffect2(NULL), mGraphicEffect3(NULL), mSound(NULL), mEffectTime2(0), mEffectTime3(0)
{
}

// SYNTHETIC: EXE 0x4b205a
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4b2076
// Destroys the calamity class, releasing its three effects
EECCLogicBomb::~EECCLogicBomb()
{
    if (this->mGraphicEffect1 != NULL)
        this->mGraphicEffect1->Dereference();
    if (this->mGraphicEffect2 != NULL)
        this->mGraphicEffect2->Dereference();
    if (this->mGraphicEffect3 != NULL)
        this->mGraphicEffect3->Dereference();
}

static void AdjustLocation(EECalamity* inCalamity, GE3DPoint* ioLocation);

// FUNCTION: EXE 0x4b20af
// Starts a logic bomb unless the target is protected: sends the launch effect from the caster to the target and plays
// the sound there (at the origin when no launch effect was sent)
bool EECCLogicBomb::Start(EECalamity* inCalamity)
{
    GE3DPoint theLocation;
    EEUnit* theTargetUnit = NULL;
    EEEffectHandle theEffectHandle;
    float theVolume;
    float thePan;
    float thePitch;

    if (inCalamity->GetTargetUnit(theTargetUnit) && theTargetUnit->mComplexUnit->IsProtected())
        return false;
    if (this->mGraphicEffect1 != NULL && inCalamity->mCasterUnit != NULL) {
        theLocation = inCalamity->mTargetLocation;
        AdjustLocation(inCalamity, &theLocation);
        gEffectManager->CreateEffect(inCalamity->mCasterUnit, theLocation, this->mGraphicEffect1, &theEffectHandle, 0);
    }
    EEComputeSoundParameters(this->mSound->GetVolume(), theLocation, theVolume, thePan, thePitch, kCalamitySoundFalloff);
    inCalamity->SetSound(gSoundManager->PlayAndReturnReferencedNode(this->mSound->GetSampleID(), theVolume, thePan,
                                                                    thePitch, this->mSound->GetLooping(), NULL));
    if (inCalamity->mSound != NULL)
        inCalamity->mSound->Dereference();
    return true;
}

// FUNCTION: EXE 0x4b21c2
// Raises the location by the target model's first-part LOD-0 bounding radius (provisional name)
static void AdjustLocation(EECalamity* inCalamity, GE3DPoint* ioLocation)
{
    EEUnit* theTargetUnit;
    if (!inCalamity->GetTargetUnit(theTargetUnit))
        return;
    EEModel* theModel = theTargetUnit->GetModel();
    if (theModel == NULL)
        return;
    GEModel* thePart = theModel->mModels[0];
    if (thePart == NULL)
        return;
    // raise the point by the radius (as the disable effect in Step uses it as a height), not shift it sideways
    ioLocation->SetZ(thePart->GetBoundingRadius(0) + ioLocation->GetZ());
}

// FUNCTION: EXE 0x4b221e
// Re-enables the target unit (giving a wonder back to its player), removes the disable effect and stops the sounds
void EECCLogicBomb::Stop(EECalamity* inCalamity)
{
    EEUnit* theTargetUnit = NULL;
    if (inCalamity->GetTargetUnit(theTargetUnit)) {
        theTargetUnit->mComplexUnit->SetDisabled(0, 0);
        theTargetUnit->mUnitClass->ActionChanged(theTargetUnit->mComplexUnit);
        EETechTreeNode* theNode = theTargetUnit->GetTechTreeNode();
        if (theNode != NULL && theNode->mData->mKind == 4)
            theTargetUnit->mPlayer->AddWonder(theTargetUnit->mComplexUnit, false);
    }
    if (inCalamity->mEffectHandle != kNoEffectHandle) {
        gEffectManager->DestroyEffect(inCalamity->mEffectHandle);
        inCalamity->mEffectHandle = kNoEffectHandle;
    }
    inCalamity->StopSound();
    inCalamity->StopSecondarySound();
}

// FUNCTION: EXE 0x4b22ad
// Reads the calamity data: sound and three effects; the stage times follow the first two effects' durations
void EECCLogicBomb::Initialize(EECalamityData* inData)
{
    EECalamityClass::Initialize(inData);
    if (inData->mSoundID != 0)
        this->mSound = EESoundObject::Get(inData->mSoundID);
    if (inData->mGraphicEffectID1 != 0) {
        this->mGraphicEffect1 = EEGraphicEffect::Get(inData->mGraphicEffectID1);
        this->mGraphicEffect1->Reference();
        this->mEffectTime2 = this->mGraphicEffect1->mDuration;
    } else
        this->mEffectTime2 = 600;
    if (inData->mGraphicEffectID2 != 0) {
        this->mGraphicEffect2 = EEGraphicEffect::Get(inData->mGraphicEffectID2);
        this->mGraphicEffect2->Reference();
        this->mEffectTime3 = this->mGraphicEffect2->mDuration + this->mEffectTime2;
    }
    if (inData->mGraphicEffectID3 != 0) {
        this->mGraphicEffect3 = EEGraphicEffect::Get(inData->mGraphicEffectID3);
        this->mGraphicEffect3->Reference();
    }
}

// ICF-FOLDED: EXE 0x557269
// (empty: folded into a shared empty function)
void EECCLogicBomb::Reset(EECalamity* inCalamity)
{
}

// ICF-FOLDED: EXE 0x4af5f2
// (identical to EECCAreaEffect::LoadMedia)
void EECCLogicBomb::LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand)
{
    if (this->mGraphicEffect1 != NULL)
        this->mGraphicEffect1->Load(inIsOnDemand);
    if (this->mGraphicEffect2 != NULL)
        this->mGraphicEffect2->Load(inIsOnDemand);
    if (this->mGraphicEffect3 != NULL)
        this->mGraphicEffect3->Load(inIsOnDemand);
    if (this->mSound != NULL)
        this->mSound->LoadMedia();
}

// ICF-FOLDED: EXE 0x557269
// (empty: folded into a shared empty function)
void EECCLogicBomb::PlaySound(EECalamity* inCalamity)
{
}

// ICF-FOLDED: EXE 0x4af632
// (identical to EECCAreaEffect::UnloadMedia)
void EECCLogicBomb::UnloadMedia(EEPlayer* inPlayer)
{
    if (this->mGraphicEffect1 != NULL)
        this->mGraphicEffect1->Unload();
    if (this->mGraphicEffect2 != NULL)
        this->mGraphicEffect2->Unload();
    if (this->mGraphicEffect3 != NULL)
        this->mGraphicEffect3->Unload();
    if (this->mSound != NULL)
        this->mSound->UnloadMedia();
}

// FUNCTION: EXE 0x4b2337
// Ages the calamity: after the launch places the ground effect; after the second stage disables the live target (a
// wonder stops counting, a gate opens, others halt); returns 7 once the target is dead
long EECCLogicBomb::Update(EECalamity* inCalamity, float inElapsedTime)
{
    GE3DPoint theLocation;
    EEEffectHandle theEffectHandle;
    long theResult = EECalamityClass::UpdateTimeAlive(inCalamity, inElapsedTime, -1);
    if (this->mSound != NULL)
        this->UpdateSound(inCalamity, this->mSound, -1.0f, 15.0f, true);
    if (inCalamity->GetTimeAlive() >= this->mEffectTime2 && !inCalamity->GetBoolPlaceHolder1()) {
        inCalamity->SetBoolPlaceHolder1(true);
        if (this->mGraphicEffect2 != NULL) {
            inCalamity->GetTargetLocation(theLocation);
            AdjustLocation(inCalamity, &theLocation);
            theLocation.SetZ(::gWorld.mMap->mTerrainMesh.GetMeshZ(theLocation.GetX(), theLocation.GetY()) + 0.05f);
            gEffectManager->CreatePlacedEffect(theLocation, this->mGraphicEffect2, NULL);
        }
    }
    if (inCalamity->GetTimeAlive() >= this->mEffectTime3 && !inCalamity->GetBoolPlaceHolder2()) {
        inCalamity->SetBoolPlaceHolder2(true);
        EEUnit* theTargetUnit = NULL;
        if (inCalamity->GetTargetUnit(theTargetUnit)) {
            EEComplexUnit* theUnit = theTargetUnit->GetComplexUnit();
            if (theUnit->GetHitPoints() > 0) {
                if (this->mGraphicEffect3 != NULL) {
                    float theHeight = theUnit->GetModel()->mModels[0]->GetBoundingRadius(0);
                    GE3DPoint thePosition;
                    theUnit->GetPosition(thePosition);
                    thePosition.SetZ(thePosition.GetZ() + theHeight * 1.5f);
                    gEffectManager->CreatePlacedEffect(thePosition, this->mGraphicEffect3, &theEffectHandle);
                    inCalamity->SetEffectHandle(theEffectHandle);
                }
                theUnit->SetDisabled(1, 0x1c);
                EETechTreeNode* theNode = theUnit->GetTechTreeNode();
                if (theNode != NULL && theNode->mData->mKind == 4)
                    theUnit->GetPlayer()->RemoveWonder(theUnit);
                if (theTargetUnit->GetUnitClass()->GetCategory() != kUnitCategoryGate)
                    theUnit->Halt();
                else {
                    theUnit->StartAction(kActionOpen);
                    theUnit->GetUnitClass()->ActionChanged(theUnit);
                    EEUnitClass* theClass = theTargetUnit->GetUnitClass();
                    theUnit->SetOpenFraction(1.0f);
                    theClass->GetLastLinkedCarrier(theUnit)->SetOpenFraction(1.0f);
                    theClass->GetLastLinked(theUnit)->SetOpenFraction(1.0f);
                }
            }
        }
    }
    EEUnit* theTargetUnit;
    if (inCalamity->GetTargetUnit(theTargetUnit) && theTargetUnit->GetComplexUnit() != NULL
        && theTargetUnit->GetComplexUnit()->GetHitPoints() <= 0)
        theResult = 7;
    return theResult;
}

// FUNCTION: EXE 0x4b25b4
// Accepts a built, enabled enemy unit on the map that is not a foundation or volcano and is in the class's unit set
bool EECCLogicBomb::IsValidTarget(EEComplexUnit* inUnit, EEUnit* inTargetUnit)
{
    return inTargetUnit->mTileNode != NULL && inTargetUnit->mPlayer != inUnit->mPlayer
        && inUnit->mPlayer->mEnemies[inTargetUnit->mPlayer->mPlayerID] != 0 && !inTargetUnit->IsFoundation()
        && inTargetUnit->mComplexUnit->mIsBuilt && !inTargetUnit->mComplexUnit->mIsDisabled
        && inTargetUnit->mUnitClass->GetUnitType() != kUnitTypeVolcano
        && gUnitSetManager.Contains(inTargetUnit->mUnitClass->GetUnitType(), this->GetUnitSet());
}
