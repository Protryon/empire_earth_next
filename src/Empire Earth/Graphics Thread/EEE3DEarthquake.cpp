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
#include "rust/ee_rust.h"   // ee_gfx quake.rs (RUST_PLAN R4.4)
#include <vector>
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
    this->mModel = gModelManager->CreateModel(this->mSegmentCount * 8, theVertexCount, 1, 0, 1, 0, "earthquake model", 1);
    this->mModel->Reference();
    this->mModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    this->mModel->GetMaterial(0)->SetPolygonRangeLength(this->mSegmentCount * 8, 0);
    this->mModel->GetMaterial(0)->SetVertexStartingIndex(0);
    this->mModel->GetMaterial(0)->SetVertexRangeLength(theVertexCount);
    // the first row on the ground; the strip's triangles and texture rows: ee_gfx quake.rs (RUST_PLAN R4.4)
    unsigned long i;
    for (i = 0; i < 5; i++) {
        thePoint.SetX((float)i - 2.0f);
        theTransformation.Apply(thePoint, theGround);
        theMap->GetGroundPoint(theGround, theGround);
        thePoint.SetZ(theGround.GetZ());
        this->mModel->GetVertexArray(0)->SetPoint(thePoint, i);
    }
    for (i = 0; i < 5; i++)
        this->mModel->GetVertexArray(0)->SetTextureCoordinate(GETextureCoordinate(EE_GFX_QUAKE_ROW_U[i], 0.0f), i);
    thePoint.SetX(0.0f);
    thePoint.SetY(0.0f);
    thePoint.SetZ(0.0f);
    for (i = 5; i < theVertexCount; i++)
        this->mModel->GetVertexArray(0)->SetPoint(thePoint, i);
    std::vector<float> theRows(this->mSegmentCount);
    ee_gfx_quake_build_rows(theRows.data(), theRows.size());
    for (unsigned long theSegment = 0; theSegment < this->mSegmentCount; theSegment++) {
        uint32_t thePolygons[24];
        ee_gfx_quake_segment_polygons((uint32_t)theSegment, thePolygons);
        for (unsigned long k = 0; k < 8; k++) {
            this->mModel->GetPolygonArray(0)->SetVertexIndex1(theSegment * 8 + k, thePolygons[k * 3]);
            this->mModel->GetPolygonArray(0)->SetVertexIndex2(theSegment * 8 + k, thePolygons[k * 3 + 1]);
            this->mModel->GetPolygonArray(0)->SetVertexIndex3(theSegment * 8 + k, thePolygons[k * 3 + 2]);
        }
        for (i = 0; i < 5; i++)
            this->mModel->GetVertexArray(0)->SetTextureCoordinate(
                GETextureCoordinate(EE_GFX_QUAKE_ROW_U[i], theRows[theSegment]), theSegment * 5 + 5 + i);
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
    GE3DPoint theGround;
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        // each row's front and texture v: ee_gfx quake.rs; the five vertices across it on the ground
        std::vector<EeGfxQuakeRow> theRows(this->mSegmentCount);
        ee_gfx_quake_queue_rows(this->mDistance, theRows.data(), theRows.size());
        for (unsigned long theSegment = 0; theSegment < this->mSegmentCount; theSegment++) {
            unsigned long theVertex = theSegment * 5 + 5;
            thePoint.SetY(theRows[theSegment].y);
            for (unsigned long i = 0; i < 5; i++) {
                thePoint.SetZ(0.0f);
                thePoint.SetX((float)i - 2.0f);
                theTransformation.Apply(thePoint, theGround);
                theMap->GetGroundPoint(theGround, theGround);
                thePoint.SetZ(theGround.GetZ() + kHeightEpsilon);
                this->mModel->GetVertexArray(0)->SetPoint(thePoint, theVertex + i);
                if (i == 2 && theMap->mTerrainMesh.GetIsSpaceTile(UMFloorUnsigned(theGround.GetX()),
                                                                  UMFloorUnsigned(theGround.GetY())))
                    theOverSpace = true;
            }
            for (unsigned long i = 0; i < 5; i++)
                this->mModel->GetVertexArray(0)->SetTextureCoordinate(
                    GETextureCoordinate(EE_GFX_QUAKE_ROW_U[i], theRows[theSegment].front_v), theVertex + i);
        }
        if (this->mGrowing && theOverSpace) {
            this->mHoldTime = ee_gfx_quake_hold_time(this->mLength, this->mDistance, this->mSpeed);
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
