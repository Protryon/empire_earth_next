// C:\EEX\Empire Earth\Units\EEUCNavalPriest.cpp (file name inferred: no __FILE__; the class name). The naval priest's
// unit class: 0x21880c..0x218a45 (after EEUCNavalCitizen's slots; EEUCNavalTransport.cpp's +0x04 0x218a45 follows).
// Unit flags: the exe's module set. Its ctor is out of line (vtable 0x44724c), followed by the ??_G every priest class
// shares (EEUCBasicPriest's inline dtor), then its own slots +0x0c, +0x34, +0xa0.
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
#include "lle/GEModel.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUCBasicPriest.h"
#include "ee/EEUCNavalPriest.h"

// FUNCTION: EXE 0x61880c
// Creates the naval priest class from its record, with physics model 4
EEUCNavalPriest::EEUCNavalPriest(EEUnitClassRecord* inRecord)
    : EEUCBasicPriest(inRecord, gPhysicsModels[4])
{
}

// SYNTHETIC: EXE 0x61882c
// EEUCNavalPriest::`scalar deleting destructor' (EEUCBasicPriest's inline dtor; ICF: every priest class)

// FUNCTION: EXE 0x61884e
// Thins the priest ship's path to every n-th point and both ends when inSmooth is set (n: its inner collision sphere)
void EEUCNavalPriest::AdjustPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath, bool inSmooth)
{
    std::vector<GE3DPoint> thePath;
    long theCount = ioPath.size();
    long theStep = UMFloatToLong(this->mInnerCollisionSphere);
    if (inSmooth != true)
        return;
    thePath.reserve(ioPath.size());
    // a collision sphere that rounds to 0 keeps every point (it used to divide by zero); as in
    // EEUCNavalCitizen::AdjustPath
    if (theStep == 0)
        theStep = 1;
    for (long i = 0; i < theCount; i++) {
        if (i == theCount - 1 || i == 0 || i % theStep == 0)
            thePath.push_back(ioPath[i]);
    }
    ioPath = thePath;
}

// FUNCTION: EXE 0x618909
// Creates a priest ship at a placement on the water plane (z 0)
EEUnit* EEUCNavalPriest::CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation,
                                      EEUnit* inCreator)
{
    GETransformation theTransformation(inTransformation);
    theTransformation.SetX(theTransformation.GetX());
    theTransformation.SetZ(0.0f);
    return EEUnitClass::CreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
}

// FUNCTION: EXE 0x618942
// Answers dying done: true once the ship's transformed bounding box lies wholly below the water line
long EEUCNavalPriest::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    GE3DPoint thePosition(0.0f, 0.0f, 0.0f);
    GEBoundingBox theBox;
    GEBoundingBox theTransformedBox;
    EEComplexUnit* theUnit = inUnit;
    GEPhysicalModel& theModel = theUnit->mPhysicalModel;
    GEModel* theGEModel = theModel.GetModel();
    long theResult = 0;
    if (inEvent != kTriggerDyingAnimationFinished) {
        theResult = EEUCBasicPriest::EvaluateCondition(theUnit, inEvent);
    } else {
        GETransformation theTransformation(false);
        theUnit->GetPosition(thePosition);
        theModel.GetLocation().Apply(theGEModel->GetInternalTransform(theModel.GetFrame()), theTransformation);
        theTransformation.Scale(theModel.GetScale());
        theTransformation.SetX(thePosition.GetX());
        theTransformation.SetY(thePosition.GetY());
        theTransformation.SetZ(thePosition.GetZ());
        theBox = theGEModel->GetBoundingBox(theUnit->mPhysicalModel.GetFrame());
        theBox.Transform(theTransformation, theTransformedBox);
        if (theTransformedBox.GetTopRightBack().GetZ() < 0.0f)
            theResult = 1;
    }
    return theResult;
}
