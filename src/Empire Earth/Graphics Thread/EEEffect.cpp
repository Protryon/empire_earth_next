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
#include "ee/EERenderSnapshot.h"
#include "rust/ee_ge.h"   // ee_gfx effects.rs (RUST_PLAN R4.4)
#include "rust/ee_gfx.h"
#include "ee/EEEffectMesh.h"

// SYNTHETIC: EXE 0x50251d
// SYNTHETIC: EXE 0x502527
// SYNTHETIC: EXE 0x502538
// SYNTHETIC: EXE 0x502544
const UString EEEffect::sData("EEEffect-Data");

// SYNTHETIC: EXE 0x502560
// SYNTHETIC: EXE 0x502565
// the effects' random numbers (EEEffect::GetRandom, GetEffectRandomFloat, GetEffectRandomLong; no dtor) (provisional name)
static URandom sEffectRandom;

// The two quad batches (one draw call per 0x800 vertices; provisional names): the camera-facing quads of AddGlowSprite
// (0x4e8db8) and the flat/heightened ones of AddGroundSprite/AddGroundQuad/AddBlendedSprite/AddGroundStrip (0x4e8da8)
struct EEEffectBatch {
    GEVertexArray* mVertices;               // +0x00
    GETexture* mTexture;                    // +0x04 (referenced twice by BeginEffectBatches)
    float mAlphaReference;                  // +0x08 (the smallest reference value of the batch; 1 when empty)
    unsigned long mCount;                   // +0x0c vertices
};
static EEEffectBatch sEffectGroundBatch;          // 0x4e8da8
static EEEffectBatch sEffectBatch;                // 0x4e8db8
static GEPolygonArray* sEffectPolygons;           // 0x4e8df0 (0x400 polygons: two per quad)
GEModel* gU4E8DF4;                          // 0x4e8df4
GEModel* gEffect2DModel;                    // 0x4e8df8

void FlushEffectBatch();
// The quads' corners, colours, normals and opacities are ee_gfx effects.rs (RUST_PLAN R4.4); the GE value classes
// pass as their #[repr(C)] mirrors (the same storage)
static_assert(sizeof(GEColor) == sizeof(EeGfxColor) && sizeof(GETextureCoordinate) == sizeof(EeGfxTexCoord) &&
              sizeof(GEVector) == sizeof(EeGeVector) && sizeof(GE3DPoint) == sizeof(EeGe3DPoint), "GE mirrors");
// Sets a quad's four vertices from index inIndex on and counts them (provisional name)
static void AddEffectQuad(EEEffectBatch& ioBatch, const EeGfxEffectQuad& inQuad, unsigned long inIndex)
{
    const GEColor& theColor = *reinterpret_cast<const GEColor*>(&inQuad.color);
    const GEVector& theNormal = *reinterpret_cast<const GEVector*>(&inQuad.normal);
    for (unsigned long i = 0; i < 4; i++)
        ioBatch.mVertices->SetVertex(theColor, *reinterpret_cast<const GE3DPoint*>(&inQuad.points[i]), theNormal,
                                     *reinterpret_cast<const GETextureCoordinate*>(&inQuad.coords[i]),
                                     inQuad.opacities[i], inIndex + i);
    ioBatch.mCount += 4;
}
// the camera plane (a, b, c, d) the fog distance is measured from (also RTSGraphics.cpp 0x11a130; read through
// inline accessors: `fld; fld; fmulp`; a GEQuaternion by its x, y, z, w accessors) (provisional name; type inferred)
void FlushEffectGroundBatch();

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
}

// FUNCTION: EXE 0x50269c
// Creates the quad batches' shared polygon array (two triangles per quad, 0x200 quads) and both vertex arrays
// (provisional name)
void CreateEffectBatchBuffers()
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
void BeginEffectBatches(GETexture* inTexture)
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

// A shared quad model (unit square, two triangles, white and opaque): standing facing -y or on the ground facing up;
// its mesh is ee_gfx effects.rs's (RUST_PLAN R4.4) (provisional name)
static GEModel* CreatePlaneModel(bool inStanding)
{
    GEModel* theModel = gModelManager->CreateModel(2, 4, 1, 0, 1, 0, "EEEffect::sHorizontalPlane", 1);
    theModel->Reference();
    EEEffectMesh theMesh(4, 2);
    ee_gfx_effect_plane(inStanding, theMesh.Vertices(), theMesh.Triangles());
    theMesh.Store(theModel);
    theModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    theModel->GetMaterial(0)->SetPolygonRangeLength(2, 0);
    theModel->GetMaterial(0)->SetVertexStartingIndex(0);
    theModel->GetMaterial(0)->SetVertexRangeLength(4);
    theModel->ComputeBoundingVolumes();
    return theModel;
}

// FUNCTION: EXE 0x50281f
// Creates the shared horizontal quad model gU4E8DF4 (unit square in the ground plane, facing up) (provisional name)
void CreateHorizontalPlaneModel()
{
    if (gU4E8DF4) {
        gU4E8DF4->Dereference();
        gU4E8DF4 = NULL;
    }
    gU4E8DF4 = CreatePlaneModel(false);
}

// FUNCTION: EXE 0x502b1c
// Creates the shared upright quad model gEffect2DModel (unit square facing -y, which the 2D effects turn to the
// camera) (provisional name)
void CreateStandingPlaneModel()
{
    if (gEffect2DModel) {
        gEffect2DModel->Dereference();
        gEffect2DModel = NULL;
    }
    gEffect2DModel = CreatePlaneModel(true);
}

// FUNCTION: EXE 0x502e1b
// Deletes the quad batches' polygon array and vertex arrays (provisional name)
void DeleteEffectBatchBuffers()
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
}

// FUNCTION: EXE 0x502ef0
// Releases the shared horizontal quad model
void ReleaseHorizontalPlaneModel()
{
    if (gU4E8DF4)
        gU4E8DF4->Dereference();
    gU4E8DF4 = NULL;
}

// FUNCTION: EXE 0x502f08
// Releases the shared upright quad model gEffect2DModel
void ReleaseStandingPlaneModel()
{
    if (gEffect2DModel)
        gEffect2DModel->Dereference();
    gEffect2DModel = NULL;
}

// FUNCTION: EXE 0x502f20
// Drops the two texture references BeginEffectBatches took (both through the first batch's texture)
void EndEffectBatches()
{
    if (sEffectBatch.mTexture) {
        sEffectBatch.mTexture->Dereference();
        sEffectBatch.mTexture->Dereference();
    }
}

// FUNCTION: EXE 0x502f3d
// Returns a random float between the minimum and the maximum from the effects' random generator
float GetEffectRandomFloat(float inMinimum, float inMaximum)
{
    return sEffectRandom.Generate(inMinimum, inMaximum);
}

// FUNCTION: EXE 0x502f57
// Returns a random long from the minimum to the maximum (clamped to the maximum) (provisional name)
long GetEffectRandomLong(long inMinimum, long inMaximum)
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
void CreatePlaneModels()
{
    CreateHorizontalPlaneModel();
    CreateStandingPlaneModel();
}

// FUNCTION: EXE 0x5030b5
// Loads the graphic effect's media (on demand or now)
void EEEffect::LoadMedia(bool inIsOnDemand)
{
    if (this->mGraphicEffect)
        this->mGraphicEffect->Load(inIsOnDemand);
}

// NEAR: block layout of the three returns; fld1 and fldz where ours loads the literals
// FUNCTION: EXE 0x5030c8
// Returns the fog visibility of a point: 1 unless the fog mode is linear, else fades from 1 at the fog start to 0
// at the fog end, by the distance along the camera (row 1 of the inverse)
float GetEffectFogFactor(const GE3DPoint& inPoint, unsigned long inFog)
{
    GERFogAttributes theAttributes;
    if (inFog == 1)
        gRasterizer->GetFogAttributes(theAttributes, gScreenIndex);
    EeGfxEffectFog theFog;
    theFog.state = inFog == 1 ? 1 : 0;
    theFog.start = theAttributes.mStart;
    theFog.end = theAttributes.mEnd;
    theFog.scale = theAttributes.mScale;
    return ee_gfx_effect_fog_factor(reinterpret_cast<const EeGe3DPoint*>(&inPoint), &theFog,
                                    reinterpret_cast<const EeGeTransformation*>(&gU4E8F08.GetCameraInverse()));
}

// TEMPLATE: EXE 0x503154
// (GERFogAttributes' inline ctor, emitted after its first user)

// NEAR: register allocation (ours hoists the Apply(GE3DPoint) import into esi; the original keeps the index
// in esi and inPosition in edi) and the GEVector stores' order
// FUNCTION: EXE 0x50317d
// Adds a camera-facing additive quad to the camera batch (colour dimmed by fog, alpha and brightness), drawing the
// batch first when it is full
void AddGlowSprite(const GE3DPoint& inPosition, const GEColor& inColor, float inRotation, float inSize, float inAlpha,
              float inBrightness, float inAlphaReference)
{
    float theFog = GetEffectFogFactor(inPosition, gRasterizer->GetFog(gScreenIndex));
    unsigned long theIndex = sEffectBatch.mCount;
    if (theIndex == 0x800) {
        FlushEffectBatch();
        theIndex = 0;
    }
    if (inAlphaReference < sEffectBatch.mAlphaReference)
        sEffectBatch.mAlphaReference = inAlphaReference;
    EeGfxEffectQuad theQuad = ee_gfx_effect_glow_sprite(reinterpret_cast<const EeGe3DPoint*>(&inPosition), reinterpret_cast<const EeGfxColor*>(&inColor), inRotation, inSize,
                                                        inAlpha, inBrightness, theFog, gRenderSnapshot.mCameraYaw,
                                                        gRenderSnapshot.mCameraPitch);
    AddEffectQuad(sEffectBatch, theQuad, theIndex);
}

// FUNCTION: EXE 0x5033f1
// Adds a flat ground quad turned by the rotation to the ground batch with one opacity, drawing the batch first when
// it is full
void AddGroundSprite(const GE3DPoint& inPosition, const GEColor& inColor, float inRotation, float inSize, float inAlpha,
              float inBrightness, float inAlphaReference)
{
    unsigned long theIndex = sEffectGroundBatch.mCount;
    if (theIndex == 0x800) {
        FlushEffectGroundBatch();
        theIndex = 0;
    }
    if (inAlphaReference < sEffectGroundBatch.mAlphaReference)
        sEffectGroundBatch.mAlphaReference = inAlphaReference;
    float theOpacities[4] = {inAlpha, inAlpha, inAlpha, inAlpha};
    EeGfxEffectQuad theQuad = ee_gfx_effect_ground_sprite(reinterpret_cast<const EeGe3DPoint*>(&inPosition), reinterpret_cast<const EeGfxColor*>(&inColor), inRotation, inSize,
                                                          theOpacities, inBrightness);
    AddEffectQuad(sEffectGroundBatch, theQuad, theIndex);
}

// FUNCTION: EXE 0x503634
// Adds a flat ground quad turned by the yaw to the ground batch with a separate opacity per corner, drawing the
// batch first when it is full
void AddGroundQuad(const GE3DPoint& inCenter, const GEColor& inColor, float inYaw, float inSize, float* inHeights,
              float inBrightness, float inAlphaReference)
{
    unsigned long theIndex = sEffectGroundBatch.mCount;
    if (theIndex == 0x800) {
        FlushEffectGroundBatch();
        theIndex = 0;
    }
    if (inAlphaReference < sEffectGroundBatch.mAlphaReference)
        sEffectGroundBatch.mAlphaReference = inAlphaReference;
    EeGfxEffectQuad theQuad = ee_gfx_effect_ground_sprite(reinterpret_cast<const EeGe3DPoint*>(&inCenter), reinterpret_cast<const EeGfxColor*>(&inColor), inYaw, inSize, inHeights,
                                                          inBrightness);
    AddEffectQuad(sEffectGroundBatch, theQuad, theIndex);
}

// FUNCTION: EXE 0x50387b
// Adds a camera-facing quad to the alpha-blended ground batch, drawing the batch first when it is full
void AddBlendedSprite(const GE3DPoint& inPosition, const GEColor& inColor, float inRotation, float inSize, float inAlpha,
              float inBrightness, float inAlphaReference)
{
    unsigned long theIndex = sEffectGroundBatch.mCount;
    if (theIndex == 0x800) {
        FlushEffectGroundBatch();
        theIndex = 0;
    }
    if (inAlphaReference < sEffectGroundBatch.mAlphaReference)
        sEffectGroundBatch.mAlphaReference = inAlphaReference;
    EeGfxEffectQuad theQuad = ee_gfx_effect_blended_sprite(reinterpret_cast<const EeGe3DPoint*>(&inPosition), reinterpret_cast<const EeGfxColor*>(&inColor), inRotation, inSize,
                                                           inAlpha, inBrightness, gRenderSnapshot.mCameraYaw,
                                                           gRenderSnapshot.mCameraPitch);
    AddEffectQuad(sEffectGroundBatch, theQuad, theIndex);
}

// FUNCTION: EXE 0x503ac8
// Adds an upright quad from inFrom to inTo, inHeight tall above the ground line between them, to the ground batch,
// textured from inOrigin over inSize (EEE3DEnergyWave) (provisional names)
void AddGroundStrip(const GE3DPoint& inFrom, const GE3DPoint& inTo, float inHeight, const GEColor& inColor,
              const GETextureCoordinate& inOrigin, const GETextureCoordinate& inSize, float inAlpha, float inBrightness,
              float inAlphaReference)
{
    unsigned long theIndex = sEffectGroundBatch.mCount;
    if (theIndex == 0x800) {
        FlushEffectGroundBatch();
        theIndex = 0;
    }
    if (inAlphaReference < sEffectGroundBatch.mAlphaReference)
        sEffectGroundBatch.mAlphaReference = inAlphaReference;
    EeGfxEffectQuad theQuad = ee_gfx_effect_ground_strip(
        reinterpret_cast<const EeGe3DPoint*>(&inFrom), reinterpret_cast<const EeGe3DPoint*>(&inTo), inHeight, reinterpret_cast<const EeGfxColor*>(&inColor),
        reinterpret_cast<const EeGfxTexCoord*>(&inOrigin), reinterpret_cast<const EeGfxTexCoord*>(&inSize), inAlpha,
        inBrightness);
    AddEffectQuad(sEffectGroundBatch, theQuad, theIndex);
}

// FUNCTION: EXE 0x503db7
// Draws and empties the camera batch: additive, no fog, no culling, z test only (provisional name)
void FlushEffectBatch()
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
    SetWhiteLighting();
    gRasterizer->Draw(*sEffectPolygons, sEffectBatch.mCount / 2, *sEffectBatch.mVertices, sEffectBatch.mCount, sEffectBatch.mTexture, theScreen,
                      gOrthographicView);
    gRasterizer->SetFog(theFog, theScreen);
    gRasterizer->SetCulling(theCulling);
    sEffectBatch.mCount = 0;
    sEffectBatch.mAlphaReference = 1.0f;
}

// FUNCTION: EXE 0x503ea6
// Draws and empties the ground batch: alpha-blended, no culling, z test only (provisional name)
void FlushEffectGroundBatch()
{
    unsigned long theScreen = gScreenIndex;
    unsigned long theCulling = gRasterizer->GetCulling();
    gRasterizer->SetCulling(kCullNone);
    gRasterizer->SetAlphaTestingRefValue(sEffectGroundBatch.mAlphaReference, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetShadingColorSource(1, theScreen);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    SetPrelitLighting();
    gRasterizer->Draw(*sEffectPolygons, sEffectGroundBatch.mCount / 2, *sEffectGroundBatch.mVertices, sEffectGroundBatch.mCount,
                      sEffectGroundBatch.mTexture, theScreen, gOrthographicView);
    gRasterizer->SetCulling(theCulling);
    sEffectGroundBatch.mCount = 0;
    sEffectGroundBatch.mAlphaReference = 1.0f;
}

// FUNCTION: EXE 0x503f64
// Draws whatever the ground batch and then the camera batch still hold (provisional name)
void FlushEffectBatches()
{
    if (sEffectGroundBatch.mCount != 0)
        FlushEffectGroundBatch();
    if (sEffectBatch.mCount != 0)
        FlushEffectBatch();
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
        EEEffect* theEffect2 = this;
        theList = theTable->GetRegion(this->mTileX, this->mTileY);
        if (theList != NULL)
            theList->push_back(theEffect2);
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
void ReleasePlaneModels()
{
    ReleaseHorizontalPlaneModel();
    ReleaseStandingPlaneModel();
}

// FUNCTION: EXE 0x50427f
// Unloads the graphic effect's media
void EEEffect::UnloadMedia()
{
    if (this->mGraphicEffect)
        this->mGraphicEffect->Unload();
}

// ICF-FOLDED: EXE 0x5ad67e
// (no address of its own: folded into the shared `xor eax, eax; ret` body the vtable slot points at)
long EEEffect::DrawPass2()
{
    return 0;
}

// ICF-FOLDED: EXE 0x5ad67e
// (no address of its own: folded into the shared `xor eax, eax; ret` body the vtable slot points at)
long EEEffect::DrawPass3()
{
    return 0;
}

// ICF-FOLDED: EXE 0x5ad67e
// (no address of its own: folded into the shared `xor eax, eax; ret` body the vtable slot points at)
long EEEffect::DrawPass4()
{
    return 0;
}

// ICF-FOLDED: EXE 0x5ad67e
// (no address of its own: folded into the shared `xor eax, eax; ret` body the vtable slot points at)
long EEEffect::DrawPass5()
{
    return 0;
}

// ICF-FOLDED: EXE 0x5ad67e
// (no address of its own: folded into the shared `xor eax, eax; ret` body the vtable slot points at)
long EEEffect::DrawPass6()
{
    return 0;
}
