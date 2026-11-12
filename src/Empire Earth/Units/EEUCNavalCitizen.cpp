// C:\EEX\Empire Earth\Units\EEUCNavalCitizen.cpp (file name inferred: no __FILE__; the class name). The naval citizen's
// unit class (a fishing boat): 0x218490..0x21880c (after EEUCNavalBuilding.cpp; EEUCNavalPriest.cpp's out-of-line
// ctor 0x21880c follows). Every slot of ee/EEUCNavalCitizen.h in that range is here: +0x08 (ICF-shared with
// EEUCNaval/EEUCNavalBuilding/EEUCNavalTransport), +0x0c, +0x30 (ICF-shared), +0x74, +0xa0, +0xb8. The ctor is
// inline (EEUnitClass.cpp's factory). Unit flags: the exe's module set.
#include <windows.h>
#include <stdio.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEBoundingBox.h"
#include "lle/GEModel.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEPhysicsModel.h"
#include "ee/EEUnitAction.h"
#include "ee/EEUCBasicCitizen.h"
#include "ee/EEUCNavalCitizen.h"

// FUNCTION: EXE 0x618490
// Returns the tile under a location along the camera line, at sea level
void EEUCNavalCitizen::LocationToTile(const GE3DPoint& inLocation, long& outX, long& outY)
{
    GE3DPoint thePoint;
    GE3DLine theLine(::gWorld.mLine);
    theLine.SetStart(inLocation);
    float theT;
    theLine.SolveForZ(0.0f, theT, thePoint);
    outX = UMFloor(thePoint.GetX());
    outY = UMFloor(thePoint.GetY());
}

// FUNCTION: EXE 0x618539
// Thins the boat's path to every n-th point and both ends when inSmooth is set (n: inner collision sphere, min 1)
void EEUCNavalCitizen::AdjustPath(EEComplexUnit* inUnit, std::vector<GE3DPoint>& ioPath, bool inSmooth)
{
    std::vector<GE3DPoint> thePath;
    long theCount = ioPath.size();
    long theStep = UMFloatToLong(this->mInnerCollisionSphere);
    if (inSmooth != true)
        return;
    thePath.reserve(ioPath.size());
    if (theStep <= 0)
        theStep = 1;
    for (long i = 0; i < theCount; i++) {
        if (i == theCount - 1 || i == 0 || i % theStep == 0)
            thePath.push_back(ioPath[i]);
    }
    ioPath = thePath;
}

// FUNCTION: EXE 0x618605
// Creates a fishing boat on a tile at sea level
EEUnit* EEUCNavalCitizen::CreateUnit(long inX, long inY, long inPlayer, bool inIsFoundation, EEUnit* inCreator)
{
    GETransformation theTransformation;
    theTransformation.SetX((float)inX);
    theTransformation.SetY((float)inY);
    theTransformation.SetZ(0.0f);
    return EEUnitClass::CreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
}

// FUNCTION: EXE 0x618641
// Hands every action change to the basic citizen's handler (the dying test changes nothing)
// The original still loads the command's type and compares it with 0x1770 with no branch after it: both arms make the
// same base call and VC6 merges them after the compare (an empty if, a switch or a bool local drop the compare)
void EEUCNavalCitizen::ActionChanged(EEComplexUnit* inUnit)
{
    if (inUnit->GetCommandType() == kActionDying)
        EEUCBasicCitizen::ActionChanged(inUnit);
    else
        EEUCBasicCitizen::ActionChanged(inUnit);
}

// FUNCTION: EXE 0x618664
// Answers dying done: true once the boat's transformed bounding box lies wholly below the water line
long EEUCNavalCitizen::EvaluateCondition(EEComplexUnit* inUnit, unsigned long inEvent)
{
    GE3DPoint thePosition;
    GEBoundingBox theBox;
    GEBoundingBox theTransformedBox;
    EEComplexUnit* theUnit = inUnit;
    const GEPhysicalModel& thePhysicalModel = theUnit->mPhysicalModel;
    GEModel* theModel = thePhysicalModel.GetModel();
    long theResult = 0;
    if (inEvent != kTriggerDyingAnimationFinished)
        theResult = EEUCBasicCitizen::EvaluateCondition(theUnit, inEvent);
    else {
        theUnit->GetPosition(thePosition);
        GETransformation theTransformation(false);
        thePhysicalModel.GetLocation().Apply(theModel->GetInternalTransform(thePhysicalModel.GetFrame()),
                                             theTransformation);
        float theScale = thePhysicalModel.GetScale();
        theTransformation.Scale(theScale, theScale, theScale);
        theTransformation.SetX(thePosition.GetX());
        theTransformation.SetY(thePosition.GetY());
        theTransformation.SetZ(thePosition.GetZ());
        theBox = theModel->GetBoundingBox(theUnit->mPhysicalModel.GetFrame());
        theBox.Transform(theTransformation, theTransformedBox);
        if (theTransformedBox.GetTopRightBack().GetZ() < 0.0f)
            theResult = 1;
    }
    return theResult;
}

// FUNCTION: EXE 0x618767
// Updates a fishing boat each tick: physics, effects, camouflage, animation, sound, rotting, then its citizen work
void EEUCNavalCitizen::Update(EEComplexUnit* inUnit)
{
    inUnit->SetLastHitPoints(inUnit->mHitPoints);
    if (this->mPhysicsModel != NULL && inUnit->mTileX != -10000)
        this->mPhysicsModel->Step(inUnit);
    this->UpdateEffects(inUnit);
    this->UpdateCamouflageTimer(inUnit);
    this->Animate(inUnit);
    this->RefreshSound(inUnit);
    unsigned long theCommand = inUnit->mCommand ? inUnit->mCommand->mType : 0;
    if (theCommand == kActionUnitRotting) {
        inUnit->UpdateRottingAwayTime();
    }
    this->UpdateCitizen(inUnit);
}

// ICF-FOLDED: EXE 0x618909
// Creates a priest ship at a placement on the water plane (z 0)
// (no address of its own: the same body as EEUCNavalPriest::CreateUnitAt, which ICF kept)
EEUnit* EEUCNavalCitizen::CreateUnitAt(GETransformation& inTransformation, long inPlayer, bool inIsFoundation,
                                      EEUnit* inCreator)
{
    GETransformation theTransformation(inTransformation);
    theTransformation.SetX(theTransformation.GetX());
    theTransformation.SetZ(0.0f);
    return EEUnitClass::CreateUnitAt(theTransformation, inPlayer, inIsFoundation, inCreator);
}

// ICF-FOLDED: EXE 0x618a45
// Raises a location below the water plane to z 0
// (no address of its own: the same body as EEUCNavalTransport::GroundLocation, which ICF kept)
void EEUCNavalCitizen::GroundLocation(GE3DPoint& ioLocation)
{
    if (ioLocation.GetZ() < 0.0f)
        ioLocation.SetZ(0.0f);
}
