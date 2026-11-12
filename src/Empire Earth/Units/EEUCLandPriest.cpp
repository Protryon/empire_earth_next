// C:\EEX\Empire Earth\Units\EEUCLandPriest.cpp (file name inferred: no __FILE__; the class name).
// The land priest's unit
// class: 0x216d57..0x216de3 (EEUCLandTransport.cpp's slots follow; the unit map's 0x216d57..0x21880c block is one
// unit per class in alphabetical order: LandPriest, LandTransport, MiningBuilding, Missile, Naval, NavalBuilding,
// NavalCitizen). Unit flags: the exe's module set. Its ctor is followed by EEUnitClass's inline +0x24/+0x68/+0x6c
// (every unit class's vtable points at these copies), then its own slots. vtable 0x447060.
#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUCBasicPriest.h"
#include "ee/EEUCLandPriest.h"

// FUNCTION: EXE 0x616d57
// Creates the land priest's unit class from its record, with the first physics model
EEUCLandPriest::EEUCLandPriest(EEUnitClassRecord* inRecord)
    : EEUCBasicPriest(inRecord, gPhysicsModels[0])
{
}

// TEMPLATE: EXE 0x616d77
// EEUnitClass::GetHeightAt (inline in ee/EEUnit.h)

// TEMPLATE: EXE 0x616d81
// EEUnitClass::Virtual68 (inline in ee/EEUnit.h)

// TEMPLATE: EXE 0x616d8d
// EEUCLandPriest::Virtual6C (inline in ee/EEUCLandPriest.h)

// FUNCTION: EXE 0x616d94
// Gets the sound parameters the common way; silent when the priest is not seen
void EEUCLandPriest::GetSoundParameters(EEUnit* inUnit, EESoundObject* inSound, float* outVolume, float* outPan,
                               float* outPitch)
{
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    EEUnitClass::GetSoundParameters(inUnit, inSound, outVolume, outPan, outPitch);
    if (theUnit != NULL && !EEIsSeen(theUnit))
        *outVolume = 0.0f;
}

// FUNCTION: EXE 0x616dcb
// Sets up a new land priest as a priest
void EEUCLandPriest::UnitCreated(EEComplexUnit* inUnit)
{
    EEUCBasicPriest::UnitCreated(inUnit);
}

