// C:\EEX\Empire Earth\Civilization Builder\EEAreaEffect.cpp (name from RTTI EEAreaEffect; folder inferred: no
// __FILE__; the area-effect units sit between EEWonNetAvailableGameFormManager.cpp and the civilization builder's).
// 0x27bf4b..0x27c0f3: the base area effect (vtable 0x44be24): ctor, ??_G, dtor, the slots it implements and the
// scale helper. Its empty slots are ICF-folded elsewhere (+0x04/+0x38/+0x44 `ret 4` 0x557269.., +0x18 0xfca98,
// +0x2c..+0x34 0x27acf9, +0x3c 0x5720c6, +0x48 0x4ac9c8); EEAreaEffectTemplate.cpp starts at 0x27c0f3.
// Unit flags: the exe's module set.
#include <windows.h>
#include <stdio.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"

#include "ee/EEUnit.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEAreaEffectTemplate.h"
#include "ee/EEEffectManager.h"

// FUNCTION: EXE 0x67bf4b
// Creates an area effect that holds a reference to its template, with no unit or player yet, and loads its media
EEAreaEffect::EEAreaEffect(EEAreaEffectTemplate* inTemplate)
{
    this->mTemplate = inTemplate;
    this->mTemplate->Reference();
    this->mUnit = NULL;
    this->mPlayer = 0;
    this->LoadMedia(false);
}

// SYNTHETIC: EXE 0x67bf83
// (the scalar deleting destructor; 0x1c bytes)

// FUNCTION: EXE 0x67bf9f
// Destroys the area effect, releasing its template and unit and unloading its media
EEAreaEffect::~EEAreaEffect()
{
    this->mTemplate->Dereference();
    if (this->mUnit != NULL)
        this->mUnit->Dereference();
    this->UnloadMedia();
}

// Does nothing when applied to a unit (ICF-folded empty `ret 4`)
void EEAreaEffect::AppliedTo(EEComplexUnit* inUnit)
{
}

// FUNCTION: EXE 0x67bfcd
// Tells whether the template's area, scaled by the unit's area-effect scale, covers the unit
bool EEAreaEffect::Covers(unsigned long inPlayer, EEComplexUnit* inUnit, const GE2DPoint& inPoint)
{
    return this->mTemplate->CoversUnit(inPlayer, inUnit, inPoint, this->GetScale());
}

// FUNCTION: EXE 0x67bff1
// Plays the template's graphic effect on the unit, if it has one
void EEAreaEffect::ShowOn(EEUnit* inUnit)
{
    EEGraphicEffect* theEffect = this->mTemplate->mGraphicEffect;
    if (theEffect != NULL)
        gEffectManager->CreateUnitEffect(inUnit, theEffect, NULL, false, 0, 0);
}

// FUNCTION: EXE 0x67c014
// Writes the effect's location, owning unit id and player to the out-of-sync dump
void EEAreaEffect::Dump(FILE* inFile)
{
    fprintf(inFile, "OUT OF SYNCH DUMP:  Area Effect Position Location X: %6.6f\n", (double)this->mLocation.mX);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Area Effect Position Location Y: %6.6f\n", (double)this->mLocation.mY);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Area Effect Owning Unit ID #%08lx\n", this->mUnit != NULL ? this->mUnit->mUnitID : -1);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Area Effect Player Number %ld\n", this->mPlayer);
}

// FUNCTION: EXE 0x67c073
// Returns a sync checksum of the location's float bits, the player and the owning unit id
long EEAreaEffect::Checksum()
{
    float theValue = this->mLocation.mX;
    long theSum = *reinterpret_cast<long*>(&theValue);
    theValue = this->mLocation.mY;
    theSum += *reinterpret_cast<long*>(&theValue);
    return this->mPlayer + (this->mUnit != NULL ? this->mUnit->mUnitID : 0) + theSum;
}

// Returns 0 as the base strength (ICF-folded `xor eax, eax; ret` 0xfca98)
long EEAreaEffect::GetStrength()
{
    return 0;
}

// FUNCTION: EXE 0x67c0a0
// Returns the template's radius times the effect's scale
float EEAreaEffect::GetRadius()
{
    float theScale = this->GetScale();
    float theRadius = this->mTemplate->mRadius;
    theRadius *= theScale;
    return theRadius;
}

// Tells that the base effect gives no immunity (ICF-folded `xor al, al; ret` 0x27c0b4)
bool EEAreaEffect::IsImmunity()
{
    return false;
}

// FUNCTION: EXE 0x67c0b7
// Returns the owning unit's area-effect scale, or 1 without a unit
float EEAreaEffect::GetScale()
{
    return this->mUnit != NULL ? this->mUnit->mStats->mAreaEffectScale : 1.0f;
}

// FUNCTION: EXE 0x67c0cb
// Sets the owning unit, taking a reference to it
void EEAreaEffect::SetUnit(EEComplexUnit* inUnit)
{
    this->mUnit = inUnit;
    if (inUnit != NULL)
        inUnit->Reference();
}

// FUNCTION: EXE 0x67c0dc
// Loads the template's media
void EEAreaEffect::LoadMedia(bool inIsOnDemand)
{
    this->mTemplate->Load(inIsOnDemand);
}

// (ICF-folded `xor al, al; ret 0xc` 0x27acf9)
bool EEAreaEffect::Virtual2C(long inKind, long inValue, long& outValue)
{
    return false;
}

// (ICF-folded 0x27acf9)
bool EEAreaEffect::Modify(long inKind, long inValue, long& outValue)
{
    return false;
}

// (ICF-folded 0x27acf9)
bool EEAreaEffect::Virtual34(long inKind, long inValue, long& outValue)
{
    return false;
}

// (ICF-folded empty `ret 4`)
void EEAreaEffect::RemovedFrom(EEComplexUnit* inUnit)
{
}

// (ICF-folded empty `ret` 0x5720c6)
void EEAreaEffect::MarkDirty()
{
}

// FUNCTION: EXE 0x67c0eb
// Unloads the template's media
void EEAreaEffect::UnloadMedia()
{
    this->mTemplate->Unload();
}

// (ICF-folded empty `ret 4`)
void EEAreaEffect::Recount(long inArg)
{
}

// (ICF-folded empty `ret 8` 0x4ac9c8)
void EEAreaEffect::Tick(EEComplexUnit* inUnit, float inElapsed)
{
}

// The ICF-shared copy LINK kept in this unit's range
// SYNTHETIC: EXE 0x67c0b4
