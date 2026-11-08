// C:\EEX\Empire Earth\Graphics Thread\EEE3DEarthquake.cpp (no __FILE__; name from the RTTI class and its key
// "EEE3DEarthquake-Data"). 0xec6e4..0xed606: the key chain, both ctors .. Virtual4C. Unit flags: the exe's module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "lle/GEModelManager.h"
#include "lle/GEPolygonArray.h"
#include "lle/SESoundManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEMedia.h"
#include "ee/EEGlobalConstants.h"
#include "ee/EEGameSound.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EEEffect2D.h"
#include "ee/EEEffect3D.h"
#include "ee/EEE3DEarthquake.h"
#include "ee/EEEffectKindIDs.h"

// SYNTHETIC: EXE 0x4ec6e4
// SYNTHETIC: EXE 0x4ec6ee
// SYNTHETIC: EXE 0x4ec6ff
// SYNTHETIC: EXE 0x4ec70b
const UString EEE3DEarthquake::sData("EEE3DEarthquake-Data");

// FUNCTION: EXE 0x4ec727
// Creates an earthquake crack from one ground point towards another: one segment per unit of length, growing at
// a speed that covers the length in the effect's duration
EEE3DEarthquake::EEE3DEarthquake(GE3DPoint& inFrom, GE3DPoint& inTo, EEGraphicEffect* inGraphicEffect)
    : EEEffect3D(0, inGraphicEffect, kEffect3DEarthquake)
{
    GEVector theDirection(inTo.GetX() - inFrom.GetX(), inTo.GetY() - inFrom.GetY(), 0.0f);
    this->mGraphicObject = NULL;
    this->mDistance = 0.0f;
    this->mLength = theDirection.GetMagnitude();
    this->mSpeed = this->mLength / this->mGraphicEffect->mDuration;
    this->mSegmentCount = (float)UMCeil(this->mLength);  // UMCeilFloat (lle/UIGraph.h) shape: float ceil then __ftol
    this->mClosing = false;
    this->mGrowing = true;
    theDirection.Normalize();
    float theYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
    GETransformation theTransformation(false);
    theTransformation.SetPosition(inFrom.GetX(), inFrom.GetY(), 0.0f);
    theTransformation.SetOrientationYPR(theYaw, 0.0f, 0.0f);
    theTransformation.SetElement(3, 0, 0.0f);
    theTransformation.SetElement(3, 1, 0.0f);
    theTransformation.SetElement(3, 2, 0.0f);
    theTransformation.SetElement(3, 3, 1.0f);
    this->SetTransformation(theTransformation);
    this->SetBoundingRadius(this->mLength);
}

// SYNTHETIC: EXE 0x4ec85f
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4ec87b
// Creates an empty earthquake for loading from a saved game
EEE3DEarthquake::EEE3DEarthquake()
    : EEEffect3D(kEffect3DEarthquake)
{
}

// FUNCTION: EXE 0x4ec88f
// Destroys the earthquake, unloading its graphic and releasing its model (not null-checked)
EEE3DEarthquake::~EEE3DEarthquake()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
    this->mModel->Dereference();
}

// FUNCTION: EXE 0x4ec8bc
// Builds the crack's mesh: five ground-hugging vertices across per segment, eight triangles per segment, tiled
// texture coordinates; textures it and returns 0
// NEAR: same statements; frame slots of the two points swapped, the map kept in ebx where ours keeps the Apply import
long EEE3DEarthquake::BuildModel()
{
    GETransformation theTransformation(this->mTransformation);
    GE3DPoint thePoint;
    EEMap* theMap = gWorld.mMap;
    unsigned long theVertexCount = (this->mSegmentCount + 1) * 5;
    GE3DPoint theGround;
    float theV = 0.0f;
    this->mModel = gModelManager->CreateModel(this->mSegmentCount * 8, theVertexCount, 1, 0, 1, 0, "earthquake model", 1);
    this->mModel->Reference();
    this->mModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    this->mModel->GetMaterial(0)->SetPolygonRangeLength(this->mSegmentCount * 8, 0);
    this->mModel->GetMaterial(0)->SetVertexStartingIndex(0);
    this->mModel->GetMaterial(0)->SetVertexRangeLength(theVertexCount);
    thePoint.SetX(-2.0f);
    theTransformation.Apply(thePoint, theGround);
    theMap->GetGroundPoint(theGround, theGround);
    thePoint.SetZ(theGround.GetZ());
    this->mModel->GetVertexArray(0)->SetPoint(thePoint, 0);
    thePoint.SetX(-1.0f);
    theTransformation.Apply(thePoint, theGround);
    theMap->GetGroundPoint(theGround, theGround);
    thePoint.SetZ(theGround.GetZ());
    this->mModel->GetVertexArray(0)->SetPoint(thePoint, 1);
    thePoint.SetX(0.0f);
    theTransformation.Apply(thePoint, theGround);
    theMap->GetGroundPoint(theGround, theGround);
    thePoint.SetZ(theGround.GetZ());
    this->mModel->GetVertexArray(0)->SetPoint(thePoint, 2);
    thePoint.SetX(1.0f);
    theTransformation.Apply(thePoint, theGround);
    theMap->GetGroundPoint(theGround, theGround);
    thePoint.SetZ(theGround.GetZ());
    this->mModel->GetVertexArray(0)->SetPoint(thePoint, 3);
    thePoint.SetX(2.0f);
    theTransformation.Apply(thePoint, theGround);
    theMap->GetGroundPoint(theGround, theGround);
    thePoint.SetZ(theGround.GetZ());
    this->mModel->GetVertexArray(0)->SetPoint(thePoint, 4);
    GETextureCoordinate theCoordinate(0.0f, 0.0f);
    this->mModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate, 0);
    theCoordinate.SetU(0.25f);
    theCoordinate.SetV(0.0f);
    this->mModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate, 1);
    theCoordinate.SetU(0.5f);
    theCoordinate.SetV(0.0f);
    this->mModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate, 2);
    theCoordinate.SetU(0.75f);
    theCoordinate.SetV(0.0f);
    this->mModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate, 3);
    theCoordinate.SetU(1.0f);
    theCoordinate.SetV(0.0f);
    this->mModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate, 4);
    thePoint.SetX(0.0f);
    thePoint.SetY(0.0f);
    thePoint.SetZ(0.0f);
    for (unsigned long i = 5; i < theVertexCount; i++)
        this->mModel->GetVertexArray(0)->SetPoint(thePoint, i);
    for (unsigned long theSegment = 0; theSegment < this->mSegmentCount; theSegment++) {
        unsigned long thePolygon = theSegment * 8;
        unsigned long theVertex = theSegment * 5;
        this->mModel->GetPolygonArray(0)->SetVertexIndex1(thePolygon, theVertex + 5);
        this->mModel->GetPolygonArray(0)->SetVertexIndex2(thePolygon, theVertex);
        this->mModel->GetPolygonArray(0)->SetVertexIndex3(thePolygon, theVertex + 6);
        this->mModel->GetPolygonArray(0)->SetVertexIndex1(thePolygon + 1, theVertex + 6);
        this->mModel->GetPolygonArray(0)->SetVertexIndex2(thePolygon + 1, theVertex);
        this->mModel->GetPolygonArray(0)->SetVertexIndex3(thePolygon + 1, theVertex + 1);
        this->mModel->GetPolygonArray(0)->SetVertexIndex1(thePolygon + 2, theVertex + 6);
        this->mModel->GetPolygonArray(0)->SetVertexIndex2(thePolygon + 2, theVertex + 1);
        this->mModel->GetPolygonArray(0)->SetVertexIndex3(thePolygon + 2, theVertex + 7);
        this->mModel->GetPolygonArray(0)->SetVertexIndex1(thePolygon + 3, theVertex + 7);
        this->mModel->GetPolygonArray(0)->SetVertexIndex2(thePolygon + 3, theVertex + 1);
        this->mModel->GetPolygonArray(0)->SetVertexIndex3(thePolygon + 3, theVertex + 2);
        this->mModel->GetPolygonArray(0)->SetVertexIndex1(thePolygon + 4, theVertex + 7);
        this->mModel->GetPolygonArray(0)->SetVertexIndex2(thePolygon + 4, theVertex + 2);
        this->mModel->GetPolygonArray(0)->SetVertexIndex3(thePolygon + 4, theVertex + 8);
        this->mModel->GetPolygonArray(0)->SetVertexIndex1(thePolygon + 5, theVertex + 8);
        this->mModel->GetPolygonArray(0)->SetVertexIndex2(thePolygon + 5, theVertex + 2);
        this->mModel->GetPolygonArray(0)->SetVertexIndex3(thePolygon + 5, theVertex + 3);
        this->mModel->GetPolygonArray(0)->SetVertexIndex1(thePolygon + 6, theVertex + 8);
        this->mModel->GetPolygonArray(0)->SetVertexIndex2(thePolygon + 6, theVertex + 3);
        this->mModel->GetPolygonArray(0)->SetVertexIndex3(thePolygon + 6, theVertex + 9);
        this->mModel->GetPolygonArray(0)->SetVertexIndex1(thePolygon + 7, theVertex + 9);
        this->mModel->GetPolygonArray(0)->SetVertexIndex2(thePolygon + 7, theVertex + 3);
        this->mModel->GetPolygonArray(0)->SetVertexIndex3(thePolygon + 7, theVertex + 4);
        this->mModel->GetVertexArray(0)->SetTextureCoordinate(GETextureCoordinate(0.0f, theV), theVertex + 5);
        this->mModel->GetVertexArray(0)->SetTextureCoordinate(GETextureCoordinate(0.25f, theV), theVertex + 6);
        this->mModel->GetVertexArray(0)->SetTextureCoordinate(GETextureCoordinate(0.5f, theV), theVertex + 7);
        this->mModel->GetVertexArray(0)->SetTextureCoordinate(GETextureCoordinate(0.75f, theV), theVertex + 8);
        this->mModel->GetVertexArray(0)->SetTextureCoordinate(GETextureCoordinate(1.0f, theV), theVertex + 9);
        theV += 0.25f;
    }
    this->mModel->ComputeModelNormals();
    this->mModel->ComputeBoundingVolumes();
    this->mModel->SetGraphicsPostModelTransform(this->mTransformation);
    this->mModel->PrepareTransforms();
    this->mModel->GetMaterial(0)->SetTexture(this->mGraphicObject->mTextures[0], gScreenIndex);
    return 0;
}

// FUNCTION: EXE 0x4ecf53
// Reshapes the crack's segments over the ground up to its current length and queues it in render queue 0; a
// growing crack that reaches a space tile stops and holds
long EEE3DEarthquake::Queue(EEEffectQueue* inQueues)
{
    GETransformation theTransformation(this->mTransformation);
    GE3DPoint thePoint;
    EEMap* theMap = gWorld.mMap;
    bool theOverSpace = false;
    float theV = 0.25f;
    GE3DPoint theGround;
    float theEnd = 1.0f;
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        for (unsigned long theSegment = 0; theSegment < this->mSegmentCount; theSegment++) {
            unsigned long theVertex = theSegment * 5 + 5;
            float theFrontV;
            float theY;
            if (this->mDistance < theEnd) {
                theY = this->mDistance;
                theFrontV = (1.0f - (theEnd - this->mDistance)) * 0.25f + theV - 0.25f;
            } else {
                theY = theEnd;
                theFrontV = theV;
            }
            thePoint.SetY(theY);
            thePoint.SetZ(0.0f);
            thePoint.SetX(-2.0f);
            theTransformation.Apply(thePoint, theGround);
            theMap->GetGroundPoint(theGround, theGround);
            thePoint.SetZ(theGround.GetZ() + kHeightEpsilon);
            this->mModel->GetVertexArray(0)->SetPoint(thePoint, theVertex);
            thePoint.SetZ(0.0f);
            thePoint.SetX(-1.0f);
            theTransformation.Apply(thePoint, theGround);
            theMap->GetGroundPoint(theGround, theGround);
            thePoint.SetZ(theGround.GetZ() + kHeightEpsilon);
            this->mModel->GetVertexArray(0)->SetPoint(thePoint, theVertex + 1);
            thePoint.SetX(0.0f);
            thePoint.SetZ(0.0f);
            theTransformation.Apply(thePoint, theGround);
            theMap->GetGroundPoint(theGround, theGround);
            thePoint.SetZ(theGround.GetZ() + kHeightEpsilon);
            this->mModel->GetVertexArray(0)->SetPoint(thePoint, theVertex + 2);
            if (theMap->mTerrainMesh.GetIsSpaceTile(UMFloorUnsigned(theGround.GetX()), UMFloorUnsigned(theGround.GetY())))
                theOverSpace = true;
            thePoint.SetX(1.0f);
            thePoint.SetZ(0.0f);
            theTransformation.Apply(thePoint, theGround);
            theMap->GetGroundPoint(theGround, theGround);
            thePoint.SetZ(theGround.GetZ() + kHeightEpsilon);
            this->mModel->GetVertexArray(0)->SetPoint(thePoint, theVertex + 3);
            thePoint.SetZ(0.0f);
            thePoint.SetX(2.0f);
            theTransformation.Apply(thePoint, theGround);
            theMap->GetGroundPoint(theGround, theGround);
            thePoint.SetZ(theGround.GetZ() + kHeightEpsilon);
            this->mModel->GetVertexArray(0)->SetPoint(thePoint, theVertex + 4);
            this->mModel->GetVertexArray(0)->SetTextureCoordinate(GETextureCoordinate(0.0f, theFrontV), theVertex);
            this->mModel->GetVertexArray(0)->SetTextureCoordinate(GETextureCoordinate(0.25f, theFrontV), theVertex + 1);
            this->mModel->GetVertexArray(0)->SetTextureCoordinate(GETextureCoordinate(0.5f, theFrontV), theVertex + 2);
            this->mModel->GetVertexArray(0)->SetTextureCoordinate(GETextureCoordinate(0.75f, theFrontV), theVertex + 3);
            this->mModel->GetVertexArray(0)->SetTextureCoordinate(GETextureCoordinate(1.0f, theFrontV), theVertex + 4);
            theEnd += 1.0f;
            theV += 0.25f;
        }
        if (this->mGrowing && theOverSpace) {
            this->mHoldTime = (this->mLength - this->mDistance) / this->mSpeed * 2.0f + 15000.0f;
            this->mGrowing = false;
        }
        this->mModel->ComputeModelNormals();
        this->mModel->ComputeBoundingVolumes();
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : NULL;
        inQueues[0][theTexture].push_back(this);
    }
    return theResult;
}

// FUNCTION: EXE 0x4ed365
// Loads the graphic and builds the crack's model; returns the first error
long EEE3DEarthquake::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    if (theResult == 0)
        theResult = this->BuildModel();
    return theResult;
}

// FUNCTION: EXE 0x4ed384
// Draws the crack's model alpha blended with z test only; returns the draw result
long EEE3DEarthquake::DrawPass2()
{
    unsigned long theScreen = gScreenIndex;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    SetTerrainLighting();
    bool theSorted = gOrthographicView;
    return gRasterizer->Draw(this->mModel, theScreen, 1.0f, theSorted);
}

long EEE3DEarthquake::DrawPass4()
{
    return 0;
}

long EEE3DEarthquake::DrawPass5()
{
    return 0;
}

long EEE3DEarthquake::DrawPass6()
{
    return 0;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DEarthquake::PrepareQueue()
{
}

// FUNCTION: EXE 0x4ed424
// Reloads the graphic and reads the crack's length, segments, hold time and phase flags; recomputes its speed
long EEE3DEarthquake::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    long theResult = inFile.Read(this->mDistance);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mLength);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mSegmentCount);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mHoldTime);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mGrowing);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mClosing);
    this->mSpeed = this->mLength / this->mGraphicEffect->mDuration;
    return theResult;
}

// FUNCTION: EXE 0x4ed4d4
// Relinks the effect's pointers after a load and rebuilds the crack's model; returns the first error
long EEE3DEarthquake::RepairPointers()
{
    long theResult = EEEffect::RepairPointers();
    if (theResult == 0)
        theResult = this->BuildModel();
    return theResult;
}

// FUNCTION: EXE 0x4ed4e9
// Writes the base 3D effect (result ignored) and the crack's length, segments, hold time and phase flags
long EEE3DEarthquake::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = outFile.Write(this->mDistance);
    if (theResult == 0)
        theResult = outFile.Write(this->mLength);
    if (theResult == 0)
        theResult = outFile.Write(this->mSegmentCount);
    if (theResult == 0)
        theResult = outFile.Write(this->mHoldTime);
    if (theResult == 0)
        theResult = outFile.Write(this->mGrowing);
    if (theResult == 0)
        theResult = outFile.Write(this->mClosing);
    return theResult;
}

// FUNCTION: EXE 0x4ed56c
// Grows the crack to its full length, holds it 15 s, then closes it; returns 7 (remove) once fully closed
long EEE3DEarthquake::Step()
{
    long theResult = 0;
    if (this->mGrowing) {
        this->mDistance += gPhysics->GetTickTime() * this->mSpeed;
        if (this->mDistance > this->mLength) {
            this->mGrowing = false;
            this->mHoldTime = 15000.0f;
        }
    } else if (this->mClosing) {
        this->mDistance -= gPhysics->GetTickTime() * this->mSpeed;
        if (this->mDistance < 0.0f)
            theResult = 7;
    } else {
        this->mHoldTime -= gPhysics->GetTickTime();
        if (this->mHoldTime < 0.0f)
            this->mClosing = true;
    }
    return theResult;
}
