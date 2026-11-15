// C:\EEX\Empire Earth\Calamities\EELocationCalamityClass.cpp (file name inferred: no __FILE__; sorts between
// EELeprosyFunctor.cpp and EEPlagueFunctor.cpp). The calamity classes cast at a location: 0xb6f21..0xb6fb1. Unit
// flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"

#include "ee/EEEffectManager.h"
#include "ee/EECalamityUpdate.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EELocationCalamityClass.h"
#include "ee/EEMap.h"
#include "ee/EEPhysics.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

// FUNCTION: EXE 0x4b6f21
// Creates a calamity of the class at the location for the caster and resets it for its start
EECalamity* EELocationCalamityClass::CreateCalamity(GE3DPoint& inLocation, EEUnit* inCasterUnit)
{
    EECalamity* theCalamity = new EECalamity(this, inCasterUnit);
    theCalamity->mTargetLocation = inLocation;
    this->Reset(theCalamity);
    return theCalamity;
}

// FUNCTION: EXE 0x4b6f6a
// Returns 2: cast at a location
long EELocationCalamityClass::GetTargetKind()
{
    return 2;
}

// FUNCTION: EXE 0x4b6f6e
// Checks the base cast rules, then refuses (1) a tile that is not a valid location for the calamity
long EELocationCalamityClass::CanCastAt(EEComplexUnit* inUnit, GE3DPoint& inLocation, unsigned long inX, unsigned long inY,
                                        bool inCheck, EEUnit* inTargetUnit)
{
    // the rules are Rust (ee_tech calamity_cast.rs; ee/EECalamityUpdate.h)
    EECalamityCanCastScope theCapture;   // CCAS (TEST ONLY)
    return theCapture.Record(this, inUnit, inLocation, inX, inY, inCheck, inTargetUnit,
                             EECalamityCanCastAt(this, EeCalamityCastMode::Location, inUnit, inX, inY, inCheck, inTargetUnit));
}

// FUNCTION: EXE 0x4b4e74 (EECCVolcano::GetRandomLocation)
// Picks a random point on the map between the minimum and maximum radius around the centre, at the tile's height
void EELocationCalamityClass::GetRandomLocation(const GE3DPoint& inCenter, GE3DPoint& outLocation)
{
    EEMapTile* theTile;
    do {
        float theAngle = gPhysics->mRandom.Generate() * 6.2831855f - 3.1415927f;
        float theFraction = gPhysics->mRandom.Generate();
        float theCos = UMCos(theAngle);
        float theSin = UMCos(1.5707964f - theAngle);
        float theDistance = (1.0f - theFraction) * this->mMinimumRadius + theFraction * this->mMaximumRadius;
        outLocation.SetX(theCos * theDistance);
        outLocation.SetY(theSin * theDistance);
        outLocation += inCenter;
        theTile = ::gWorld.mMap->GetTile(UMFloor(outLocation.GetX()), UMFloor(outLocation.GetY()));
    } while (theTile == NULL);
    outLocation.SetZ(theTile->mHeight);
}

// The class's IsValidLocation: the shared rule over the class's radius, water flag, map area, space terrain and placed
// unit class (ee_tech calamity_cast.rs, calamity_class.rs cast_fields)
bool EELocationCalamityClass::IsValidLocation(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY)
{
    EeCalamityCast theCast = EECalamityCastOf(this, inUnit, inX, inY);
    long theUnitClass = ee_calamity_class_cast_fields(this, &theCast, kTerrainSpace);
    return EECalamityValidLocation(this, theCast, inUnit, theUnitClass);
}
