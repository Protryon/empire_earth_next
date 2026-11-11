// C:\EEX\Empire Earth\Calamities\EECCConscription.cpp (file name inferred: no __FILE__). The conscription calamity
// class: 0xafdd3..0xafef9. Unit flags: the exe's module set. Stop and Reset are ICF-folded (0x157269).
// Data (.rdata): the vtable 0x43c8ac (COL 0x43c8a8).
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
#include "ee/EEUnitCalamityClass.h"
#include "ee/EECCConscription.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"

// FUNCTION: EXE 0x4afdd3
// Creates the conscription calamity class
EECCConscription::EECCConscription()
{
}

// SYNTHETIC: EXE 0x4afde5
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4afe01
// Destroys the calamity class (nothing of its own to free)
EECCConscription::~EECCConscription()
{
}

// FUNCTION: EXE 0x4afe0c
// Conscripts a living target: when the caster's player can afford the conscription technology, pays it
// and runs unit effect 0x138d on the target. Always returns true
bool EECCConscription::Start(EECalamity* inCalamity)
{
    EEUnit* theTargetUnit = NULL;
    if (!inCalamity->GetTargetUnit(theTargetUnit))
        return true;
    EEComplexUnit* theTarget = theTargetUnit->mComplexUnit;
    if (!(theTarget->mHitPoints > 0))
        return true;
    EEUnit* theCaster = inCalamity->mCasterUnit;
    EETechnology* theTechnology = theCaster->mPlayer->mTechnologies->Get(kTechConscriptionSpell);
    if (!theCaster->mPlayer->CanAfford(theTechnology, theCaster->mUnitClass->GetUnitType(), true))
        return true;
    theCaster->mPlayer->Pay(theTechnology, theCaster->mUnitClass->GetUnitType(), 1.0f);
    theTarget->PlayEffect(0x138d);
    EEPlayUnitEffect(theTarget, 0x138d);
    return true;
}

// ICF-FOLDED: EXE 0x557269
// (empty: folded into a shared empty function)
void EECCConscription::Stop(EECalamity* inCalamity)
{
}

// FUNCTION: EXE 0x4afea3
// Reads the calamity data (the base part only)
void EECCConscription::Initialize(EECalamityData* inData)
{
    EECalamityClass::Initialize(inData);
}

// ICF-FOLDED: EXE 0x557269
// (empty: folded into a shared empty function)
void EECCConscription::Reset(EECalamity* inCalamity)
{
}

// FUNCTION: EXE 0x4afeaf
// Returns 7: done at once (Rust)
long EECCConscription::Update(EECalamity* inCalamity, float inElapsedTime)
{
    // the update is Rust (ee_tech calamity_update.rs; ee/EECalamityUpdate.h)
    EeCalamityParams theParams = EECalamityBaseParams(this, NULL, NULL);
    return EECalamityRunUpdate(this, inCalamity, inElapsedTime, theParams, NULL, NULL, NULL);
}

// FUNCTION: EXE 0x4afeb5
// Accepts another unit of the caster's own player that is on the map and in the calamity's unit set
bool EECCConscription::IsValidTarget(EEComplexUnit* inUnit, EEUnit* inTargetUnit)
{
    return inTargetUnit->mTileNode != NULL && inTargetUnit != inUnit && inTargetUnit->mPlayer == inUnit->mPlayer
        && gUnitSetManager.Contains(inTargetUnit->mUnitClass->GetUnitType(), this->GetUnitSet());
}
