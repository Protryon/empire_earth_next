// C:\EEX\Empire Earth\Graphics Thread\EEOrbital.cpp (file name inferred: no __FILE__; named after the class whose members
// it holds, RTTI EEOrbital, declared in ee/EEGraphicObject.h). 0x208c37..0x208e6b (unit map [C], after EESightRings.cpp,
// before EESavedGroup.cpp; edge: two globals chains): the orbiting-part name prefixes, EEOrbital::IsOrbitalModel,
// Init and Update. Unit flags: the exe's module set (provisional names).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEModel.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicObject.h"

// degrees to radians: written as a literal (0x446094 is its __real constant; a named const float loads first)

// SYNTHETIC: EXE 0x608c37   (wrapper)
// SYNTHETIC: EXE 0x608c41   (ctor)
// SYNTHETIC: EXE 0x608c52   (registrar)
// SYNTHETIC: EXE 0x608c5e   (dtor)
UString gOrbitalClockwisePrefix("orbital_cw_");          // 0x9634b8
// SYNTHETIC: EXE 0x608c69   (wrapper)
// SYNTHETIC: EXE 0x608c73   (ctor)
// SYNTHETIC: EXE 0x608c84   (registrar)
// SYNTHETIC: EXE 0x608c90   (dtor)
UString gOrbitalCounterClockwisePrefix("orbital_ccw_");  // 0x9634a8

// FUNCTION: EXE 0x608c9b
// Returns whether a model's name starts with orbital_cw_ or orbital_ccw_ (ignoring case)
bool EEOrbital::IsOrbitalModel(GEModel* inModel)
{
    const UString& theName = inModel->GetName();
    if (theName.EqualsIgnoreCase(gOrbitalClockwisePrefix, gOrbitalClockwisePrefix.GetLength()) ||
        theName.EqualsIgnoreCase(gOrbitalCounterClockwisePrefix, gOrbitalCounterClockwisePrefix.GetLength()))
        return true;
    return false;
}

// FUNCTION: EXE 0x608ce6
// Attaches the orbiting part to a model, speed from the name in degrees per second (orbital_cw_<deg>_... clockwise,
// orbital_ccw_ counterclockwise); the prefix test ignores case
void EEOrbital::Init(GEModel* inModel)
{
    UString theField(0x40);
    float theDegrees;
    this->mSpeed = 0.0f;
    this->mModel = inModel;
    // Match the prefixes ignoring case, as IsOrbitalModel does.
    if (inModel->GetName().EqualsIgnoreCase(gOrbitalClockwisePrefix, gOrbitalClockwisePrefix.GetLength())) {
        unsigned long theEnd = inModel->GetName().FindBackwards("_", (unsigned long)-1);
        unsigned long theCount = theEnd - gOrbitalClockwisePrefix.GetLength();
        if (theCount != 0 && theCount < theEnd) {
            theField.Copy(inModel->GetName(), gOrbitalClockwisePrefix.GetLength(), theCount, 0, false);
            sscanf(theField, "%f", &theDegrees);
            theDegrees = -(theDegrees < 0.0f ? -theDegrees : theDegrees);
            this->mSpeed = theDegrees * 0.017453292f;
        }
    } else if (inModel->GetName().EqualsIgnoreCase(gOrbitalCounterClockwisePrefix,
                                                   gOrbitalCounterClockwisePrefix.GetLength())) {
        unsigned long theEnd = inModel->GetName().FindBackwards("_", (unsigned long)-1);
        unsigned long theCount = theEnd - gOrbitalCounterClockwisePrefix.GetLength();
        if (theCount != 0 && theCount < theEnd) {
            theField.Copy(inModel->GetName(), gOrbitalCounterClockwisePrefix.GetLength(), theCount, 0, false);
            sscanf(theField, "%f", &theDegrees);
            if (theDegrees < 0.0f)
                theDegrees = -theDegrees;
            this->mSpeed = theDegrees * 0.017453292f;
        }
    }
}

// FUNCTION: EXE 0x608e28
// Advances the orbiting part's angle by its speed over the tick, wrapped to -pi..pi
void EEOrbital::Update()
{
    float theAngle = gPhysics->GetTickSeconds() * this->mSpeed + this->mAngle;
    while (theAngle < -3.1415927f)
        theAngle += 6.2831855f;
    while (theAngle > 3.1415927f)
        theAngle -= 6.2831855f;
    this->mAngle = theAngle;
}
