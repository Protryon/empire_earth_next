// C:\EEX\Empire Earth\Calamities\EEAreaEffectFunctor.cpp (file name inferred: no __FILE__; the class name, as the
// sibling functor units). The area effects' functor base (RTTI EEAreaEffectFunctor, vtable 0x43c624): 0xac991..0xacb75:
// the ctor, the base's inline defaults (EEContagionFunctor's Update/Cure, CanInfectType, Virtual1C, ??_G), Infect,
// Apply, then EEUnitSetManager::Contains (the ICF-shared CanInfectType's callee). Its other overrides are folded with
// the sibling functors' bodies (ICF). EECalamity.cpp starts at 0xacb75. Unit flags: the exe module set.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEEffectManager.h"
#include "ee/EECalamity.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"

extern EEEffectManager*& gEffectManager;
extern EEWorld gWorld;
extern EEUnitSetManager gUnitSetManager;

// FUNCTION: EXE 0x4ac991
// Creates an area effect functor: target and hit effects, damage per update, update time, unit set, mode
EEAreaEffectFunctor::EEAreaEffectFunctor(EEGraphicEffect* inEffect2, EEGraphicEffect* inEffect3, long inDamage,
                                         long inUpdateTime, unsigned long inValue, long inMode)
{
    this->mUpdates = 0;
    this->mEffect2 = inEffect2;
    this->mEffect3 = inEffect3;
    this->mDamage = inDamage;
    this->mUpdateTime = inUpdateTime;
    this->mValue = inValue;
    this->mMode = inMode;
}

// TEMPLATE: EXE 0x4ac9c8
// ?Update@EEContagionFunctor@@UAEXMPAVEECalamity@@@Z (the base's inline default; also its Infect/Cure)

// FUNCTION: EXE 0x4ac9cb
// Destroys the functor and frees it when asked (scalar deleting destructor)

// FUNCTION: EXE 0x4ac9e7
// Destroys the functor (nothing of its own to free)
EEAreaEffectFunctor::~EEAreaEffectFunctor()
{
}

// TEMPLATE: EXE 0x4ac9ee
// ?CanInfectType@EEContagionFunctor@@UAE_NJJ@Z (the base's inline default)

// TEMPLATE: EXE 0x4ac9f3
// ?Virtual1C@EEContagionFunctor@@UAE_NXZ (the base's inline default)

// TEMPLATE: EXE 0x4ac9f6
// ??_GEEContagionFunctor@@UAEPAXI@Z

bool EEAreaEffectFunctor::CanInfectType(long inCategory, long inUnitType)
{
    bool theResult = false;
    if (gUnitSetManager.Contains(inUnitType, this->GetUnitSet()))
        theResult = true;
    return theResult;
}

unsigned long EEAreaEffectFunctor::GetUnitSet()
{
    return this->mValue;
}

bool EEAreaEffectFunctor::SkipsSpread()
{
    return true;
}

void EEAreaEffectFunctor::UpdateUnits(std::vector<EEUnit*>* inUnits)
{
}

void EEAreaEffectFunctor::Update(float inElapsedTime, EECalamity* inCalamity)
{
    inCalamity->mDiscreteUpdateTime = this->mUpdateTime;
    inCalamity->mTimeElapsedSinceLastUpdate += inElapsedTime;
    this->mUpdates = inCalamity->GetDiscreteUpdates();
}

// FUNCTION: EXE 0x4aca13
// Plays the target effect at the calamity's target on the ground (at height 0 on water terrain types 1 and 10)
void EEAreaEffectFunctor::Infect(EEUnit* inUnit, EECalamity* inCalamity)
{
    GE3DPoint theLocation;
    inCalamity->GetTargetLocation(theLocation);
    long theTerrainType = ::gWorld.mMap->GetTile(UMFloor(theLocation.GetX()), UMFloor(theLocation.GetY()))->mTerrainType;
    if (theTerrainType != 1 && theTerrainType != 10)
        theLocation.SetZ(::gWorld.mMap->mTerrainMesh.GetMeshZ(theLocation.GetX(), theLocation.GetY()));
    else
        theLocation.SetZ(0.0f);
    if (this->mEffect2 != NULL)
        gEffectManager->CreateEffect(inUnit, theLocation, this->mEffect2, NULL, 0);
}

// FUNCTION: EXE 0x4acad4
// Hits the unit for damage x updates due x the unit's own attack stat, with the hit effect;
// totals of 1 or less do nothing
void EEAreaEffectFunctor::Apply(EEUnit* inUnit, float inElapsedTime, EECalamity* inCalamity)
{
    EEUnit* theCaster = inCalamity->mCasterUnit;
    if (!(this->mUpdates > 0))
        return;
    float theValue = (float)(this->mDamage * this->mUpdates) * inUnit->mStats->GetAttack();
    long theDamage = UMFloatToLong(theValue);
    if (theDamage <= 1)
        return;
    if (this->mEffect3 != NULL)
        gEffectManager->CreateUnitEffect(inUnit, this->mEffect3, NULL, false, 0, 0);
    inUnit->TakeHit(theCaster, theDamage, inCalamity->mPlayerNumber, this->mMode);
}

// FUNCTION: EXE 0x4acb52
// Returns whether a unit type is in the given unit set (false for an unused set)
// ?Contains@EEUnitSetManager@@QAE_NJK@Z
// (declared out of line in ee/EEUnitSetManager.h; the original's COMDAT sits here, after its first user)
bool EEUnitSetManager::Contains(long inUnitType, unsigned long inSet)
{
    if (this->mSets[inSet] != NULL)
        return this->mSets[inSet]->Contains(inUnitType);
    return false;
}
