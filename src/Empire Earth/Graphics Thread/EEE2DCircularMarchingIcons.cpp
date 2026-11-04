// C:\EEX\Empire Earth\Graphics Thread\EEE2DCircularMarchingIcons.cpp (no __FILE__; name from the RTTI class). Marching
// icons on a circle: 0xdf57c..0xdf68d (EEE2DBattlefieldAmbient.cpp ends at 0xdf57c, EEE2DExhaustTrail.cpp's key chains
// start at 0xdf68d). Its ??_G 0xdf590 (implicit dtor) is the copy ICF kept for the whole marching-icons family. Unit
// flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"

#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEEffect.h"
#include "ee/EEEffect2D.h"
#include "ee/EEE2DMarchingIcons.h"
#include "ee/EEE2DCircularMarchingIcons.h"

// FUNCTION: EXE 0x4df57c
// Creates an empty ring of marching icons for loading from a saved game (its own kind, as the unit constructor)
EEE2DCircularMarchingIcons::EEE2DCircularMarchingIcons()
    : EEE2DMarchingIcons(0x10)
{
}

// SYNTHETIC: EXE 0x4df590
// (the scalar deleting destructor, emitted with the vtable; the implicit dtor calls ~EEE2DMarchingIcons)

// FUNCTION: EXE 0x4df5ac
// Creates a ring of marching icons around a unit, its radius the effect's parameter 1
EEE2DCircularMarchingIcons::EEE2DCircularMarchingIcons(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect)
    : EEE2DMarchingIcons(inUnit, inGraphicEffect, 0x10)
{
    this->SetBoundingRadius(inGraphicEffect->mParam1);
}

// FUNCTION: EXE 0x4df5d3
// Adds the icon offset at an angle on the ring to the list if that icon is shown
void EEE2DCircularMarchingIcons::AddIcon(float inAngle, std::vector<GE3DPoint>& outPoints)
{
    GE3DPoint thePoint;
    float theRadius = this->mBoundingRadius;
    thePoint.SetX(UMCos(inAngle) * theRadius);
    // the exe's spelling of pi/2 (__real 0x3fc90fdb): not UMSin's 1.5707963f
    thePoint.SetY(UMCos(1.5707964f - inAngle) * theRadius);
    if (this->IsIconShown(thePoint))
        outPoints.push_back(thePoint);
}

// FUNCTION: EXE 0x4df63d
// Lists the shown icon offsets spaced evenly around the ring, starting at the given angle
void EEE2DCircularMarchingIcons::ComputeOffsets(float inAngle, std::vector<GE3DPoint>& outPoints)
{
    long theCount = this->GetIconCount();
    if (theCount <= 0)
        return;
    float theStep = 6.2831855f / theCount;
    for (long i = 0; i < theCount; i++)
        this->AddIcon(inAngle + i * theStep, outPoints);
}
