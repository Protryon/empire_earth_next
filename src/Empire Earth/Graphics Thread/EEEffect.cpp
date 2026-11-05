// C:\EEX\Empire Earth\Graphics Thread\EEEffect.cpp (no __FILE__; name from the RTTI class and its key
// "EEEffect-Data"). The base of every graphics-thread effect (vtable 0x43f9fc): 0x10251d..0x10428c (the key chain,
// the file's URandom, the ctor .. Virtual20, the shared effect models and the two quad batches the 2D effects draw
// into). EEEffect2D.cpp follows.
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

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EEEffect2D.h"
#include "lle/URandom.h"
#include "lle/GEModelManager.h"
#include "lle/GEPolygonArray.h"
#include "lle/GEQuaternion.h"
#include "ee/EEWorld.h"

// SYNTHETIC: EXE 0x50251d
// SYNTHETIC: EXE 0x502527
// SYNTHETIC: EXE 0x502538
// SYNTHETIC: EXE 0x502544
const UString EEEffect::sData("EEEffect-Data");

// SYNTHETIC: EXE 0x502560
// SYNTHETIC: EXE 0x502565
// the effects' random numbers (EEEffect::GetRandom, Fn102F3D, Fn102F57; no dtor) (provisional name)
static URandom sEffectRandom;

// The two quad batches (one draw call per 0x800 vertices; provisional names): the camera-facing quads of Fn10317D
// (0x4e8db8) and the flat/heightened ones of Fn1033F1/Fn103634/Fn10387B/Fn103AC8 (0x4e8da8)
struct EEEffectBatch {
    GEVertexArray* mVertices;               // +0x00
    GETexture* mTexture;                    // +0x04 (referenced twice by Fn102724)
    float mAlphaReference;                  // +0x08 (the smallest reference value of the batch; 1 when empty)
    unsigned long mCount;                   // +0x0c vertices
};
static EEEffectBatch sEffectGroundBatch;          // 0x4e8da8
static EEEffectBatch sEffectBatch;                // 0x4e8db8
static GEPolygonArray* sEffectPolygons;           // 0x4e8df0 (0x400 polygons: two per quad)
GEModel* gU4E8DF4;                          // 0x4e8df4
GEModel* gEffect2DModel;                    // 0x4e8df8

void Fn103DB7();
// the camera plane (a, b, c, d) the fog distance is measured from (also RTSGraphics.cpp 0x11a130; read through
// inline accessors: `fld; fld; fmulp`; a GEQuaternion by its x, y, z, w accessors) (provisional name; type inferred)
void Fn103EA6();

// FUNCTION: EXE 0x50258e
// Creates an effect on a unit (or none) with a graphic effect, referencing both, in no region yet, and loads its media
EEEffect::EEEffect(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind, bool inKeepsUnit)
{
    this->Init();
    this->mUnit = inUnit;
    if (this->mUnit)
        this->mUnit->Reference();
    this->mGraphicEffect = inGraphicEffect;
    if (this->mGraphicEffect)
        this->mGraphicEffect->Reference();
    this->mTileX = -1;
    this->mTileY = -1;
    this->mKind = inKind;
    this->mKeepsUnit = inKeepsUnit;
    this->mIsLocalOnly = false;
    this->mPlayer = 0;
    this->mIsSaved = true;
    this->mHandle = kNoEffectHandle;
    this->LoadMedia(gLoadingSavedGame == false);
}

// FUNCTION: EXE 0x502600
// Destroys the effect: unloads and releases its graphic effect, leaves its map region and releases its unit
EEEffect::~EEEffect()
{
    if (this->mGraphicEffect != NULL) {
        this->UnloadMedia();
        this->mGraphicEffect->Dereference();
        this->mGraphicEffect = NULL;
    }
    if (gWorld.mMap != NULL && this->mTileX != -1 && this->mTileY != -1) {
        EEEffect* theEffect = this;
        RTSRegion<EEEffect*>* theList = gWorld.mMap->mEffectRegions->GetRegion(this->mTileX, this->mTileY);
        if (theList != NULL)
            theList->Remove(theEffect);
    }
    if (this->mUnit != NULL) {
        this->mUnit->Dereference();
        this->mUnit = NULL;
    }
    this->mTileX = -1;
    this->mTileY = -1;
    this->mNext = NULL;
    this->mPrevious = NULL;
}

// FUNCTION: EXE 0x50269c
// Creates the quad batches' shared polygon array (two triangles per quad, 0x200 quads) and both vertex arrays
// (provisional name)
void Fn10269C()
{
    sEffectPolygons = gRasterizer->CreatePolygonArray(0x400);
    unsigned long j = 0;
    for (unsigned long i = 0; i < 0x200; i++) {
        sEffectPolygons->SetVertexIndices(j, i * 4, i * 4 + 1, i * 4 + 2);
        sEffectPolygons->SetVertexIndices(j + 1, i * 4, i * 4 + 2, i * 4 + 3);
        j += 2;
    }
    sEffectBatch.mVertices = gRasterizer->CreateVertexArray(1, 0x800);
    sEffectGroundBatch.mVertices = gRasterizer->CreateVertexArray(1, 0x800);
}

// FUNCTION: EXE 0x502724
// Starts both quad batches empty with the given texture, referencing it once per batch (provisional name)
void Fn102724(GETexture* inTexture)
{
    sEffectBatch.mTexture = inTexture;
    sEffectGroundBatch.mTexture = inTexture;
    if (inTexture) {
        inTexture->Reference();
        inTexture->Reference();
    }
    sEffectBatch.mCount = 0;
    sEffectGroundBatch.mCount = 0;
    sEffectBatch.mAlphaReference = 1.0f;
    sEffectGroundBatch.mAlphaReference = 1.0f;
}

// FUNCTION: EXE 0x502759
// Returns the unit's position (physical model, else its tile centre at the tile's height; the origin without a
// unit) in a shared static point
const GE3DPoint& EEEffect::GetUnitLocation()
{
    static GE3DPoint sLocation;
    EEUnit* theUnit = this->mUnit;
    if (theUnit) {
        EEComplexUnit* theComplexUnit = theUnit->mComplexUnit;
        if (theComplexUnit) {
            sLocation.SetX(theComplexUnit->mPhysicalModel.GetLocation().GetX());
            sLocation.SetY(theComplexUnit->mPhysicalModel.GetLocation().GetY());
            sLocation.SetZ(theComplexUnit->mPhysicalModel.GetLocation().GetZ());
        } else {
            sLocation.SetX(theUnit->mTileX + 0.5f);
            sLocation.SetY(theUnit->GetTileY() + 0.5f);
            if (theUnit->mTile)
                sLocation.SetZ(theUnit->mTile->mHeight);
            else
                sLocation.SetZ(0.0f);
        }
    } else {
        sLocation.SetX(0.0f);
        sLocation.SetY(0.0f);
        sLocation.SetZ(0.0f);
    }
    return sLocation;
}

// SYNTHETIC: EXE 0x502806
// (sLocation's atexit dtor: GE3DPoint's empty user dtor)

// FUNCTION: EXE 0x502807
// Returns 10 (skip) for an effect shown only to another player, else 0
long EEEffect::Queue(EEEffectQueue* inQueues)
{
    long theResult = 0;
    if (this->mIsLocalOnly && this->mPlayer != gWorld.mCurrentPlayer)
        theResult = 10;
    return theResult;
}

// FUNCTION: EXE 0x50281f
// Creates the shared horizontal quad model gU4E8DF4 (unit square in the ground plane, facing up) (provisional name)
void Fn10281F()
{
    GEColor theColor(1.0f, 1.0f, 1.0f);
    GE3DPoint thePoint1(-0.5f, 0.5f, 0.0f);
    GE3DPoint thePoint2(0.5f, 0.5f, 0.0f);
    GE3DPoint thePoint3(-0.5f, -0.5f, 0.0f);
    GE3DPoint thePoint4(0.5f, -0.5f, 0.0f);
    GETextureCoordinate theCoordinate1(0.0f, 0.0f);
    GETextureCoordinate theCoordinate2(1.0f, 0.0f);
    GETextureCoordinate theCoordinate3(0.0f, 1.0f);
    GETextureCoordinate theCoordinate4(1.0f, 1.0f);
    GEVector theNormal(0.0f, 0.0f, 1.0f);
    if (gU4E8DF4) {
        gU4E8DF4->Dereference();
        gU4E8DF4 = NULL;
    }
    GEModel* theModel = gModelManager->CreateModel(2, 4, 1, 0, 1, 0, "EEEffect::sHorizontalPlane", 1);
    gU4E8DF4 = theModel;
    theModel->Reference();
    theModel->GetVertexArray(0)->SetPoint(thePoint1, 0);
    theModel->GetVertexArray(0)->SetPoint(thePoint2, 1);
    theModel->GetVertexArray(0)->SetPoint(thePoint3, 2);
    theModel->GetVertexArray(0)->SetPoint(thePoint4, 3);
    theModel->GetVertexArray(0)->SetNormal(theNormal, 0);
    theModel->GetVertexArray(0)->SetNormal(theNormal, 1);
    theModel->GetVertexArray(0)->SetNormal(theNormal, 2);
    theModel->GetVertexArray(0)->SetNormal(theNormal, 3);
    theModel->GetVertexArray(0)->SetColor(theColor, 0);
    theModel->GetVertexArray(0)->SetColor(theColor, 1);
    theModel->GetVertexArray(0)->SetColor(theColor, 2);
    theModel->GetVertexArray(0)->SetColor(theColor, 3);
    theModel->GetVertexArray(0)->SetOpacity(1.0f, 0);
    theModel->GetVertexArray(0)->SetOpacity(1.0f, 1);
    theModel->GetVertexArray(0)->SetOpacity(1.0f, 2);
    theModel->GetVertexArray(0)->SetOpacity(1.0f, 3);
    theModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    theModel->GetMaterial(0)->SetPolygonRangeLength(2, 0);
    theModel->GetMaterial(0)->SetVertexStartingIndex(0);
    theModel->GetMaterial(0)->SetVertexRangeLength(4);
    theModel->GetPolygonArray(0)->SetVertexIndex1(0, 0);
    theModel->GetPolygonArray(0)->SetVertexIndex2(0, 1);
    theModel->GetPolygonArray(0)->SetVertexIndex3(0, 2);
    theModel->GetPolygonArray(0)->SetVertexIndex1(1, 2);
    theModel->GetPolygonArray(0)->SetVertexIndex2(1, 1);
    theModel->GetPolygonArray(0)->SetVertexIndex3(1, 3);
    theModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate1, 0);
    theModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate2, 1);
    theModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate3, 2);
    theModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate4, 3);
    theModel->ComputeBoundingVolumes();
}

// FUNCTION: EXE 0x502b1c
// Creates the shared upright quad model gEffect2DModel (unit square facing -y, which the 2D effects turn to the
// camera) (provisional name)
void Fn102B1C()
{
    GEColor theColor(1.0f, 1.0f, 1.0f);
    GE3DPoint thePoint1(-0.5f, 0.0f, 0.5f);
    GE3DPoint thePoint2(0.5f, 0.0f, 0.5f);
    GE3DPoint thePoint3(-0.5f, 0.0f, -0.5f);
    GE3DPoint thePoint4(0.5f, 0.0f, -0.5f);
    GETextureCoordinate theCoordinate1(0.0f, 0.0f);
    GETextureCoordinate theCoordinate2(1.0f, 0.0f);
    GETextureCoordinate theCoordinate3(0.0f, 1.0f);
    GETextureCoordinate theCoordinate4(1.0f, 1.0f);
    GEVector theNormal(0.0f, -1.0f, 0.0f);
    if (gEffect2DModel) {
        gEffect2DModel->Dereference();
        gEffect2DModel = NULL;
    }
    GEModel* theModel = gModelManager->CreateModel(2, 4, 1, 0, 1, 0, "EEEffect::sHorizontalPlane", 1);
    gEffect2DModel = theModel;
    theModel->Reference();
    theModel->GetVertexArray(0)->SetPoint(thePoint1, 0);
    theModel->GetVertexArray(0)->SetPoint(thePoint2, 1);
    theModel->GetVertexArray(0)->SetPoint(thePoint3, 2);
    theModel->GetVertexArray(0)->SetPoint(thePoint4, 3);
    theModel->GetVertexArray(0)->SetNormal(theNormal, 0);
    theModel->GetVertexArray(0)->SetNormal(theNormal, 1);
    theModel->GetVertexArray(0)->SetNormal(theNormal, 2);
    theModel->GetVertexArray(0)->SetNormal(theNormal, 3);
    theModel->GetVertexArray(0)->SetColor(theColor, 0);
    theModel->GetVertexArray(0)->SetColor(theColor, 1);
    theModel->GetVertexArray(0)->SetColor(theColor, 2);
    theModel->GetVertexArray(0)->SetColor(theColor, 3);
    theModel->GetVertexArray(0)->SetOpacity(1.0f, 0);
    theModel->GetVertexArray(0)->SetOpacity(1.0f, 1);
    theModel->GetVertexArray(0)->SetOpacity(1.0f, 2);
    theModel->GetVertexArray(0)->SetOpacity(1.0f, 3);
    theModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    theModel->GetMaterial(0)->SetPolygonRangeLength(2, 0);
    theModel->GetMaterial(0)->SetVertexStartingIndex(0);
    theModel->GetMaterial(0)->SetVertexRangeLength(4);
    theModel->GetPolygonArray(0)->SetVertexIndex1(0, 0);
    theModel->GetPolygonArray(0)->SetVertexIndex2(0, 1);
    theModel->GetPolygonArray(0)->SetVertexIndex3(0, 2);
    theModel->GetPolygonArray(0)->SetVertexIndex1(1, 2);
    theModel->GetPolygonArray(0)->SetVertexIndex2(1, 1);
    theModel->GetPolygonArray(0)->SetVertexIndex3(1, 3);
    theModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate1, 0);
    theModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate2, 1);
    theModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate3, 2);
    theModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate4, 3);
    theModel->ComputeBoundingVolumes();
}

// FUNCTION: EXE 0x502e1b
// Deletes the quad batches' polygon array and vertex arrays (provisional name)
void Fn102E1B()
{
    if (sEffectPolygons)
        delete sEffectPolygons;
    if (sEffectBatch.mVertices)
        delete sEffectBatch.mVertices;
    if (sEffectGroundBatch.mVertices)
        delete sEffectGroundBatch.mVertices;
    sEffectPolygons = NULL;
    sEffectBatch.mVertices = NULL;
    sEffectGroundBatch.mVertices = NULL;
}

// FUNCTION: EXE 0x502e61
// Releases what the destructor releases (graphic effect, map region, unit) without destroying the effect
void EEEffect::ReleaseResources()
{
    if (this->mGraphicEffect != NULL) {
        this->UnloadMedia();
        this->mGraphicEffect->Dereference();
        this->mGraphicEffect = NULL;
    }
    if (gWorld.mMap != NULL && this->mTileX != -1 && this->mTileY != -1) {
        EEEffect* theEffect = this;
        RTSRegion<EEEffect*>* theList = gWorld.mMap->mEffectRegions->GetRegion(this->mTileX, this->mTileY);
        if (theList != NULL)
            theList->Remove(theEffect);
    }
    if (this->mUnit != NULL) {
        this->mUnit->Dereference();
        this->mUnit = NULL;
    }
    this->mTileX = -1;
    this->mTileY = -1;
    this->mNext = NULL;
    this->mPrevious = NULL;
}

// FUNCTION: EXE 0x502ef0
// Releases the shared horizontal quad model
void Fn102EF0()
{
    if (gU4E8DF4)
        gU4E8DF4->Dereference();
    gU4E8DF4 = NULL;
}

// FUNCTION: EXE 0x502f08
// Releases the shared upright quad model gEffect2DModel
void Fn102F08()
{
    if (gEffect2DModel)
        gEffect2DModel->Dereference();
    gEffect2DModel = NULL;
}

// FUNCTION: EXE 0x502f20
// Drops the two texture references Fn102724 took (both through the first batch's texture)
void Fn102F20()
{
    if (sEffectBatch.mTexture) {
        sEffectBatch.mTexture->Dereference();
        sEffectBatch.mTexture->Dereference();
    }
}

// FUNCTION: EXE 0x502f3d
// Returns a random float between the minimum and the maximum from the effects' random generator
float Fn102F3D(float inMinimum, float inMaximum)
{
    return sEffectRandom.Generate(inMinimum, inMaximum);
}

// FUNCTION: EXE 0x502f57
// Returns a random long from the minimum to the maximum (clamped to the maximum) (provisional name)
long Fn102F57(long inMinimum, long inMaximum)
{
    long theResult = UMFloatToLong(sEffectRandom.Generate() * (inMaximum - inMinimum) + inMinimum);
    if (theResult > inMaximum)
        theResult = inMaximum;
    return theResult;
}

// FUNCTION: EXE 0x502f8b
// Returns the next random fraction from the effects' random generator
float EEEffect::GetRandom()
{
    return sEffectRandom.Generate();
}

// FUNCTION: EXE 0x502f96
// Returns the graphic object (media) of the effect's graphic effect, or null without one
EEGraphicObject* EEEffect::GetGraphicObject()
{
    return this->mGraphicEffect ? static_cast<EEGraphicObject*>(this->mGraphicEffect->GetMedia()) : 0;
}

// FUNCTION: EXE 0x502fa6
// Resets a pooled effect and attaches it to a new unit and graphic effect, referencing both, then loads its media
void EEEffect::Reinit(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, bool inKeepsUnit)
{
    this->Init();
    this->mUnit = inUnit;
    if (this->mUnit)
        this->mUnit->Reference();
    this->mGraphicEffect = inGraphicEffect;
    if (this->mGraphicEffect)
        this->mGraphicEffect->Reference();
    this->mKeepsUnit = inKeepsUnit;
    this->LoadMedia(gLoadingSavedGame == false);
}

// FUNCTION: EXE 0x502fe7
// Resets a pooled effect to the empty state with the given keeps-unit flag
void EEEffect::Reinit(bool inKeepsUnit)
{
    this->Init();
    this->mKeepsUnit = inKeepsUnit;
}

// FUNCTION: EXE 0x502ffa
// Clears the effect's unit, graphic effect, links, handle, region and flags (shown to everyone, saved)
void EEEffect::Init()
{
    this->mUnit = NULL;
    this->mGraphicEffect = NULL;
    this->mNext = NULL;
    this->mPrevious = NULL;
    this->mHandle = kNoEffectHandle;
    this->mTileX = -1;
    this->mTileY = -1;
    this->mPlayer = 0;
    this->mKeepsUnit = false;
    this->mIsSaved = true;
    this->mIsLocalOnly = false;
}

// FUNCTION: EXE 0x503027
// Reads the graphic effect id (and loads its media), region, saved unit id, handle and keeps-unit flag; returns
// the first read error
long EEEffect::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theValue;
    long theResult = inFile.Read(theValue);
    if (theResult != kErrorOK)
        return theResult;
    this->mGraphicEffect = EEGraphicEffect::Get(theValue);
    this->mGraphicEffect->Reference();
    this->LoadMedia(false);
    theResult = inFile.Read(this->mTileX);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mTileY);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(theValue);
    this->mUnit = reinterpret_cast<EEUnit*>(theValue);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mHandle.mValue);
    if (theResult == kErrorOK)
        theResult = inFile.Read(this->mKeepsUnit);
    return theResult;
}

// FUNCTION: EXE 0x5030ab
// Creates the two shared quad models (provisional name)
void Fn1030AB()
{
    Fn10281F();
    Fn102B1C();
}

// FUNCTION: EXE 0x5030b5
// Loads the graphic effect's media (on demand or now)
void EEEffect::LoadMedia(bool inIsOnDemand)
{
    if (this->mGraphicEffect)
        this->mGraphicEffect->Load(inIsOnDemand);
}

// NEAR: block layout of the three returns, and `GetX() * gU4E8F08.GetCameraInverse().GetElement(1, 0)` folds into `fmul [mem]` in ours
// FUNCTION: EXE 0x5030c8
// Returns the fog visibility of a point: 1 unless the fog mode is linear, else fades from 1 at the fog start to 0
// at the fog end, by the distance to the camera plane 0x4e9008
float Fn1030C8(const GE3DPoint& inPoint, unsigned long inFog)
{
    GERFogAttributes theAttributes;
    if (inFog != 1)
        return 1.0f;
    gRasterizer->GetFogAttributes(theAttributes, gScreenIndex);
    float theDistance = inPoint.GetY() * gU4E8F08.GetCameraInverse().GetElement(1, 1) + inPoint.GetZ() * gU4E8F08.GetCameraInverse().GetElement(1, 2) +
                        inPoint.GetX() * gU4E8F08.GetCameraInverse().GetElement(1, 0) + gU4E8F08.GetCameraInverse().GetElement(1, 3);
    if (theDistance <= theAttributes.mStart)
        return 1.0f;
    if (theDistance <= theAttributes.mEnd) {
        float theFactor = 1.0f - (theDistance - theAttributes.mStart) * theAttributes.mScale;
        if (theFactor >= 0.0f)
            return theFactor;
    }
    return 0.0f;
    return 1.0f;
}

// TEMPLATE: EXE 0x503154
// (GERFogAttributes' inline ctor, emitted after its first user)

// NEAR: register allocation (ours hoists the Apply(GE3DPoint) import into esi; the original keeps the index
// in esi and inPosition in edi) and the GEVector stores' order
// FUNCTION: EXE 0x50317d
// Adds a camera-facing additive quad to the camera batch (colour dimmed by fog, alpha and brightness), drawing the
// batch first when it is full
void Fn10317D(const GE3DPoint& inPosition, const GEColor& inColor, float inRotation, float inSize, float inAlpha,
              float inBrightness, float inAlphaReference)
{
    GE3DPoint theCorner;
    GE3DPoint thePoint;
    GEVector theUp(0.0f, -1.0f, 0.0f);
    GEVector theNormal;
    GETransformation theTransformation(false);
    float theAlpha = Fn1030C8(inPosition, gRasterizer->GetFog(gScreenIndex));
    theAlpha *= inAlpha * inBrightness;
    unsigned long theIndex = sEffectBatch.mCount;
    float theHalfSize = inSize * 0.5f;
    GEColor theColor(inColor.GetRed() * theAlpha, inColor.GetGreen() * theAlpha, inColor.GetBlue() * theAlpha);
    if (theIndex == 0x800) {
        Fn103DB7();
        theIndex = 0;
    }
    if (inAlphaReference < sEffectBatch.mAlphaReference)
        sEffectBatch.mAlphaReference = inAlphaReference;
    theTransformation.SetOrientationYPR(gWorld.mCameraYaw, gWorld.mCameraPitch, inRotation);
    theTransformation.SetPosition(inPosition);
    theTransformation.SetElement(3, 0, 0.0f);
    theTransformation.SetElement(3, 1, 0.0f);
    theTransformation.SetElement(3, 2, 0.0f);
    theTransformation.SetElement(3, 3, 1.0f);
    theTransformation.Apply(theUp, theNormal);
    float theNegativeHalfSize = -theHalfSize;
    theCorner.SetX(theNegativeHalfSize);
    theCorner.SetZ(theHalfSize);
    theTransformation.Apply(theCorner, thePoint);
    GETextureCoordinate theCoordinate(0.0f, 0.0f);
    sEffectBatch.mVertices->SetVertex(theColor, thePoint, theNormal, theCoordinate, 1.0f, theIndex);
    theCorner.SetX(theHalfSize);
    theCorner.SetZ(theHalfSize);
    theIndex++;
    theTransformation.Apply(theCorner, thePoint);
    theCoordinate.SetU(1.0f);
    theCoordinate.SetV(0.0f);
    sEffectBatch.mVertices->SetVertex(theColor, thePoint, theNormal, theCoordinate, 1.0f, theIndex);
    theCorner.SetX(theHalfSize);
    theCorner.SetZ(theNegativeHalfSize);
    theIndex++;
    theTransformation.Apply(theCorner, thePoint);
    theCoordinate.SetU(1.0f);
    theCoordinate.SetV(1.0f);
    sEffectBatch.mVertices->SetVertex(theColor, thePoint, theNormal, theCoordinate, 1.0f, theIndex);
    theCorner.SetX(theNegativeHalfSize);
    theCorner.SetZ(theNegativeHalfSize);
    theTransformation.Apply(theCorner, thePoint);
    theCoordinate.SetU(0.0f);
    theCoordinate.SetV(1.0f);
    theIndex++;
    sEffectBatch.mVertices->SetVertex(theColor, thePoint, theNormal, theCoordinate, 1.0f, theIndex);
    sEffectBatch.mCount += 4;
}

// FUNCTION: EXE 0x5033f1
// Adds a flat ground quad turned by the rotation to the ground batch with one opacity, drawing the batch first when
// it is full
void Fn1033F1(const GE3DPoint& inPosition, const GEColor& inColor, float inRotation, float inSize, float inAlpha,
              float inBrightness, float inAlphaReference)
{
    GE3DPoint theCorner;
    GE3DPoint thePoint;
    GEVector theUp(0.0f, 0.0f, 1.0f);
    GEVector theNormal;
    GETransformation theTransformation(false);
    unsigned long theIndex = sEffectGroundBatch.mCount;
    GEColor theColor(inColor.GetRed() * inBrightness, inColor.GetGreen() * inBrightness, inColor.GetBlue() * inBrightness);
    float theHalfSize = inSize * 0.5f;
    if (theIndex == 0x800) {
        Fn103EA6();
        theIndex = 0;
    }
    if (inAlphaReference < sEffectGroundBatch.mAlphaReference)
        sEffectGroundBatch.mAlphaReference = inAlphaReference;
    theTransformation.SetOrientationYPR(inRotation, 0.0f, 0.0f);
    theTransformation.SetPosition(inPosition);
    theTransformation.SetElement(3, 0, 0.0f);
    theTransformation.SetElement(3, 1, 0.0f);
    theTransformation.SetElement(3, 2, 0.0f);
    theTransformation.SetElement(3, 3, 1.0f);
    theTransformation.Apply(theUp, theNormal);
    float theNegativeHalfSize = -theHalfSize;
    theCorner.SetX(theNegativeHalfSize);
    theCorner.SetY(theHalfSize);
    theTransformation.Apply(theCorner, thePoint);
    GETextureCoordinate theCoordinate(0.0f, 0.0f);
    sEffectGroundBatch.mVertices->SetVertex(theColor, thePoint, theNormal, theCoordinate, inAlpha, theIndex);
    theCorner.SetX(theHalfSize);
    theCorner.SetY(theHalfSize);
    theIndex++;
    theTransformation.Apply(theCorner, thePoint);
    theCoordinate.SetU(1.0f);
    theCoordinate.SetV(0.0f);
    sEffectGroundBatch.mVertices->SetVertex(theColor, thePoint, theNormal, theCoordinate, inAlpha, theIndex);
    theCorner.SetX(theHalfSize);
    theCorner.SetY(theNegativeHalfSize);
    theIndex++;
    theTransformation.Apply(theCorner, thePoint);
    theCoordinate.SetU(1.0f);
    theCoordinate.SetV(1.0f);
    sEffectGroundBatch.mVertices->SetVertex(theColor, thePoint, theNormal, theCoordinate, inAlpha, theIndex);
    theCorner.SetX(theNegativeHalfSize);
    theCorner.SetY(theNegativeHalfSize);
    theTransformation.Apply(theCorner, thePoint);
    theCoordinate.SetU(0.0f);
    theCoordinate.SetV(1.0f);
    theIndex++;
    sEffectGroundBatch.mVertices->SetVertex(theColor, thePoint, theNormal, theCoordinate, inAlpha, theIndex);
    sEffectGroundBatch.mCount += 4;
}

// FUNCTION: EXE 0x503634
// Adds a flat ground quad turned by the yaw to the ground batch with a separate opacity per corner, drawing the
// batch first when it is full
void Fn103634(const GE3DPoint& inCenter, const GEColor& inColor, float inYaw, float inSize, float* inHeights,
              float inBrightness, float inAlphaReference)
{
    GE3DPoint theCorner;
    GE3DPoint thePoint;
    GEVector theUp(0.0f, 0.0f, 1.0f);
    GEVector theNormal;
    GETransformation theTransformation(false);
    unsigned long theIndex = sEffectGroundBatch.mCount;
    GEColor theColor(inColor.GetRed() * inBrightness, inColor.GetGreen() * inBrightness, inColor.GetBlue() * inBrightness);
    float theHalfSize = inSize * 0.5f;
    if (theIndex == 0x800) {
        Fn103EA6();
        theIndex = 0;
    }
    if (inAlphaReference < sEffectGroundBatch.mAlphaReference)
        sEffectGroundBatch.mAlphaReference = inAlphaReference;
    theTransformation.SetOrientationYPR(inYaw, 0.0f, 0.0f);
    theTransformation.SetPosition(inCenter);
    theTransformation.SetElement(3, 0, 0.0f);
    theTransformation.SetElement(3, 1, 0.0f);
    theTransformation.SetElement(3, 2, 0.0f);
    theTransformation.SetElement(3, 3, 1.0f);
    theTransformation.Apply(theUp, theNormal);
    float theNegativeHalfSize = -theHalfSize;
    theCorner.SetX(theNegativeHalfSize);
    theCorner.SetY(theHalfSize);
    theTransformation.Apply(theCorner, thePoint);
    GETextureCoordinate theCoordinate(0.0f, 0.0f);
    sEffectGroundBatch.mVertices->SetVertex(theColor, thePoint, theNormal, theCoordinate, inHeights[0], theIndex);
    theCorner.SetX(theHalfSize);
    theCorner.SetY(theHalfSize);
    theIndex++;
    theTransformation.Apply(theCorner, thePoint);
    theCoordinate.SetU(1.0f);
    theCoordinate.SetV(0.0f);
    sEffectGroundBatch.mVertices->SetVertex(theColor, thePoint, theNormal, theCoordinate, inHeights[1], theIndex);
    theCorner.SetX(theHalfSize);
    theCorner.SetY(theNegativeHalfSize);
    theIndex++;
    theTransformation.Apply(theCorner, thePoint);
    theCoordinate.SetU(1.0f);
    theCoordinate.SetV(1.0f);
    sEffectGroundBatch.mVertices->SetVertex(theColor, thePoint, theNormal, theCoordinate, inHeights[2], theIndex);
    theCorner.SetX(theNegativeHalfSize);
    theCorner.SetY(theNegativeHalfSize);
    theTransformation.Apply(theCorner, thePoint);
    theCoordinate.SetU(0.0f);
    theCoordinate.SetV(1.0f);
    theIndex++;
    sEffectGroundBatch.mVertices->SetVertex(theColor, thePoint, theNormal, theCoordinate, inHeights[3], theIndex);
    sEffectGroundBatch.mCount += 4;
}

// FUNCTION: EXE 0x50387b
// Adds a camera-facing quad to the alpha-blended ground batch, drawing the batch first when it is full
void Fn10387B(const GE3DPoint& inPosition, const GEColor& inColor, float inRotation, float inSize, float inAlpha,
              float inBrightness, float inAlphaReference)
{
    GE3DPoint theCorner;
    GE3DPoint thePoint;
    GEVector theUp(0.0f, -1.0f, 0.0f);
    GEVector theNormal;
    GETransformation theTransformation(false);
    unsigned long theIndex = sEffectGroundBatch.mCount;
    GEColor theColor(inColor.GetRed() * inBrightness, inColor.GetGreen() * inBrightness, inColor.GetBlue() * inBrightness);
    float theHalfSize = inSize * 0.5f;
    if (theIndex == 0x800) {
        Fn103EA6();
        theIndex = 0;
    }
    if (inAlphaReference < sEffectGroundBatch.mAlphaReference)
        sEffectGroundBatch.mAlphaReference = inAlphaReference;
    theTransformation.SetOrientationYPR(gWorld.GetCameraYaw(), gWorld.GetCameraPitch(), inRotation);
    theTransformation.SetPosition(inPosition);
    theTransformation.SetElement(3, 0, 0.0f);
    theTransformation.SetElement(3, 1, 0.0f);
    theTransformation.SetElement(3, 2, 0.0f);
    theTransformation.SetElement(3, 3, 1.0f);
    theTransformation.Apply(theUp, theNormal);
    float theNegativeHalfSize = -theHalfSize;
    theCorner.SetX(theNegativeHalfSize);
    theCorner.SetZ(theHalfSize);
    theTransformation.Apply(theCorner, thePoint);
    GETextureCoordinate theCoordinate(0.0f, 0.0f);
    sEffectGroundBatch.mVertices->SetVertex(theColor, thePoint, theNormal, theCoordinate, inAlpha, theIndex);
    theCorner.SetX(theHalfSize);
    theCorner.SetZ(theHalfSize);
    theIndex++;
    theTransformation.Apply(theCorner, thePoint);
    theCoordinate.SetU(1.0f);
    theCoordinate.SetV(0.0f);
    sEffectGroundBatch.mVertices->SetVertex(theColor, thePoint, theNormal, theCoordinate, inAlpha, theIndex);
    theCorner.SetX(theHalfSize);
    theCorner.SetZ(theNegativeHalfSize);
    theIndex++;
    theTransformation.Apply(theCorner, thePoint);
    theCoordinate.SetU(1.0f);
    theCoordinate.SetV(1.0f);
    sEffectGroundBatch.mVertices->SetVertex(theColor, thePoint, theNormal, theCoordinate, inAlpha, theIndex);
    theCorner.SetX(theNegativeHalfSize);
    theCorner.SetZ(theNegativeHalfSize);
    theTransformation.Apply(theCorner, thePoint);
    theCoordinate.SetU(0.0f);
    theCoordinate.SetV(1.0f);
    theIndex++;
    sEffectGroundBatch.mVertices->SetVertex(theColor, thePoint, theNormal, theCoordinate, inAlpha, theIndex);
    sEffectGroundBatch.mCount += 4;
}

// FUNCTION: EXE 0x503ac8
// Adds an upright quad from inFrom to inTo, inHeight tall above the ground line between them, to the ground batch,
// textured from inOrigin over inSize (EEE3DEnergyWave) (provisional names)
void Fn103AC8(const GE3DPoint& inFrom, const GE3DPoint& inTo, float inHeight, const GEColor& inColor,
              const GETextureCoordinate& inOrigin, const GETextureCoordinate& inSize, float inAlpha, float inBrightness,
              float inAlphaReference)
{
    GE3DPoint theCorner;
    GEVector theUp(0.0f, -1.0f, 0.0f);
    GE3DPoint thePoint;
    GEVector theNormal;
    GETransformation theTransformation(false);
    GEColor theColor(inColor.GetRed() * inBrightness, inColor.GetGreen() * inBrightness, inColor.GetBlue() * inBrightness);
    unsigned long theIndex = sEffectGroundBatch.mCount;
    if (theIndex == 0x800) {
        Fn103EA6();
        theIndex = 0;
    }
    if (inAlphaReference < sEffectGroundBatch.mAlphaReference)
        sEffectGroundBatch.mAlphaReference = inAlphaReference;
    float theDX = inTo.GetX() - inFrom.GetX();
    float theDY = inTo.GetY() - inFrom.GetY();
    float theHalfDX = theDX * 0.5f;
    float theHalfDY = theDY * 0.5f;
    GE3DPoint theCenter(inFrom.GetX() + theHalfDX, inFrom.GetY() + theHalfDY, inFrom.GetZ());
    theTransformation.SetOrientationYPR(0.0f, 0.0f, 0.0f);
    theTransformation.SetPosition(theCenter);
    theTransformation.SetElement(3, 0, 0.0f);
    theTransformation.SetElement(3, 1, 0.0f);
    theTransformation.SetElement(3, 2, 0.0f);
    theTransformation.SetElement(3, 3, 1.0f);
    theTransformation.Apply(theUp, theNormal);
    theCorner.SetX(theHalfDX);
    theCorner.SetY(theHalfDY);
    theCorner.SetZ(inTo.GetZ() - inFrom.GetZ() + inHeight);
    theTransformation.Apply(theCorner, thePoint);
    GETextureCoordinate theCoordinate(inOrigin.GetU(), inOrigin.GetV());
    sEffectGroundBatch.mVertices->SetVertex(theColor, thePoint, theNormal, theCoordinate, inAlpha, theIndex);
    theIndex++;
    float theBackX = theDX * -0.5f;
    float theBackY = theDY * -0.5f;
    theCorner.SetX(theBackX);
    theCorner.SetY(theBackY);
    theCorner.SetZ(inHeight);
    theTransformation.Apply(theCorner, thePoint);
    theCoordinate.SetU(inOrigin.GetU() + inSize.GetU());
    theCoordinate.SetV(inOrigin.GetV());
    sEffectGroundBatch.mVertices->SetVertex(theColor, thePoint, theNormal, theCoordinate, inAlpha, theIndex);
    theIndex++;
    theCorner.SetX(theBackX);
    theCorner.SetY(theBackY);
    theCorner.SetZ(0.0f);
    theTransformation.Apply(theCorner, thePoint);
    theCoordinate.SetU(inOrigin.GetU() + inSize.GetU());
    theCoordinate.SetV(inOrigin.GetV() + inSize.GetV());
    sEffectGroundBatch.mVertices->SetVertex(theColor, thePoint, theNormal, theCoordinate, inAlpha, theIndex);
    theCorner.SetX(theHalfDX);
    theCorner.SetY(theHalfDY);
    theCorner.SetZ(inTo.GetZ() - inFrom.GetZ());
    theTransformation.Apply(theCorner, thePoint);
    theCoordinate.SetU(inOrigin.GetU());
    theCoordinate.SetV(inOrigin.GetV() + inSize.GetV());
    theIndex++;
    sEffectGroundBatch.mVertices->SetVertex(theColor, thePoint, theNormal, theCoordinate, inAlpha, theIndex);
    sEffectGroundBatch.mCount += 4;
}

// FUNCTION: EXE 0x503db7
// Draws and empties the camera batch: additive, no fog, no culling, z test only (provisional name)
void Fn103DB7()
{
    unsigned long theScreen = gScreenIndex;
    unsigned long theCulling = gRasterizer->GetCulling();
    gRasterizer->SetCulling(kCullNone);
    unsigned long theFog = gRasterizer->GetFog(theScreen);
    gRasterizer->SetFog(0, theScreen);
    gRasterizer->SetAlphaTestingRefValue(sEffectBatch.mAlphaReference, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetShadingColorSource(1, theScreen);
    gRasterizer->SetBlending(kBlendOne, kBlendOne, theScreen);
    Fn10F7C8();
    gRasterizer->Draw(*sEffectPolygons, sEffectBatch.mCount / 2, *sEffectBatch.mVertices, sEffectBatch.mCount, sEffectBatch.mTexture, theScreen,
                      gOrthographicView);
    gRasterizer->SetFog(theFog, theScreen);
    gRasterizer->SetCulling(theCulling);
    sEffectBatch.mCount = 0;
    sEffectBatch.mAlphaReference = 1.0f;
}

// FUNCTION: EXE 0x503ea6
// Draws and empties the ground batch: alpha-blended, no culling, z test only (provisional name)
void Fn103EA6()
{
    unsigned long theScreen = gScreenIndex;
    unsigned long theCulling = gRasterizer->GetCulling();
    gRasterizer->SetCulling(kCullNone);
    gRasterizer->SetAlphaTestingRefValue(sEffectGroundBatch.mAlphaReference, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetShadingColorSource(1, theScreen);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    Fn10F71C();
    gRasterizer->Draw(*sEffectPolygons, sEffectGroundBatch.mCount / 2, *sEffectGroundBatch.mVertices, sEffectGroundBatch.mCount,
                      sEffectGroundBatch.mTexture, theScreen, gOrthographicView);
    gRasterizer->SetCulling(theCulling);
    sEffectGroundBatch.mCount = 0;
    sEffectGroundBatch.mAlphaReference = 1.0f;
}

// FUNCTION: EXE 0x503f64
// Draws whatever the ground batch and then the camera batch still hold (provisional name)
void Fn103F64()
{
    if (sEffectGroundBatch.mCount != 0)
        Fn103EA6();
    if (sEffectBatch.mCount != 0)
        Fn103DB7();
}

// FUNCTION: EXE 0x503f81
// After a load, turns the saved unit id back into the unit (returns 7 when it is gone) and joins the effect's
// map region
long EEEffect::RepairPointers()
{
    long theResult = 0;
    if (USavedID(this->mUnit) == 0xffffffff)
        this->mUnit = NULL;
    else {
        this->mUnit = dynamic_cast<EEUnit*>(static_cast<EEUnit*>(gWorld.GetUnit(USavedID(this->mUnit))));
        if (this->mUnit == NULL)
            theResult = 7;
        else
            this->mUnit->Reference();
    }
    EEEffect* theEffect = this;
    RTSRegionTable<EEEffect*>* theTable = gWorld.mMap->mEffectRegions;
    RTSRegion<EEEffect*>* theList = theTable->GetRegion(this->mTileX, this->mTileY);
    if (theList != NULL)
        theList->push_back(theEffect);
    return theResult;
}

// FUNCTION: EXE 0x50400a
// Writes the graphic effect id, region, unit id (-1 without a unit), handle and keeps-unit flag; returns the first
// write error
long EEEffect::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theUnitID = (unsigned long)-1;
    long theIndex = this->mGraphicEffect->mID;
    long theResult = outFile.Write(theIndex);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mTileX);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mTileY);
    if (theResult != 0)
        return theResult;
    if (this->mUnit)
        theUnitID = this->mUnit->mUnitID;
    theResult = outFile.Write(theUnitID);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mHandle.mValue);
    if (theResult == 0)
        theResult = outFile.Write(this->mKeepsUnit);
    return theResult;
}

// FUNCTION: EXE 0x50409b
// Moves the effect to the map region under a point when that region changed (mTileX/mTileY hold region
// coordinates)
// NEAR: the two theEffect temporaries' frame slots are swapped (ebp-8 / ebp-4)
void EEEffect::MoveToPoint(const GE3DPoint& inLocation)
{
    RTSRegionTable<EEEffect*>* theTable = gWorld.mMap->mEffectRegions;
    long theX = UMFloor(inLocation.GetX());
    long theY = UMFloor(inLocation.GetY());
    theX /= theTable->mRegionSize;
    theY /= theTable->mRegionSize;
    if (theX != this->mTileX || theY != this->mTileY) {
        EEEffect* theEffect = this;
        RTSRegion<EEEffect*>* theList = theTable->GetRegion(this->mTileX, this->mTileY);
        if (theList != NULL)
            theList->Remove(theEffect);
        this->mTileX = theX;
        this->mTileY = theY;
        theEffect = this;
        theList = theTable->GetRegion(this->mTileX, this->mTileY);
        if (theList != NULL)
            theList->push_back(theEffect);
    }
}

// FUNCTION: EXE 0x504175
// Moves the effect to the map region under a transformation's translation when that region changed
// NEAR: as MoveToPoint (swapped frame slots)
void EEEffect::MoveToTransformation(const GETransformation& inTransformation)
{
    RTSRegionTable<EEEffect*>* theTable = gWorld.mMap->mEffectRegions;
    long theX = UMFloor(inTransformation.GetX());
    long theY = UMFloor(inTransformation.GetY());
    theX /= theTable->mRegionSize;
    theY /= theTable->mRegionSize;
    if (theX != this->mTileX || theY != this->mTileY) {
        EEEffect* theEffect = this;
        RTSRegion<EEEffect*>* theList = theTable->GetRegion(this->mTileX, this->mTileY);
        if (theList != NULL)
            theList->Remove(theEffect);
        this->mTileX = theX;
        this->mTileY = theY;
        theEffect = this;
        theList = theTable->GetRegion(this->mTileX, this->mTileY);
        if (theList != NULL)
            theList->push_back(theEffect);
    }
}

// FUNCTION: EXE 0x504250
// Moves the effect from the old unit to the new one (when it is on the old unit and does not keep its unit)
void EEEffect::ReplaceUnit(EEUnit* inOldUnit, EEUnit* inNewUnit)
{
    if (this->mUnit != inOldUnit || this->mKeepsUnit)
        return;
    this->mUnit->Dereference();
    this->mUnit = inNewUnit;
    this->mUnit->Reference();
}

// FUNCTION: EXE 0x504275
// Releases the two shared quad models (provisional name)
void Fn104275()
{
    Fn102EF0();
    Fn102F08();
}

// FUNCTION: EXE 0x50427f
// Unloads the graphic effect's media
void EEEffect::UnloadMedia()
{
    if (this->mGraphicEffect)
        this->mGraphicEffect->Unload();
}
