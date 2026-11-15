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
// (the effects' random numbers, the original's file-static URandom: ee_gfx effect_state.rs)

// The effects' records (ee_gfx effect_state.rs) hold the GE value classes as their mirrors
static_assert(sizeof(GETransformation) == sizeof(EeGeTransformation), "GE mirrors");

// An FSFile as effect_state.rs's typed reads and writes (each the FSFile call of that type)
struct EEEffectFileOps {
    static FSFile& File(void* inContext) { return *static_cast<FSFile*>(inContext); }
    static bool IsOpen(void* inContext) { return File(inContext).GetIsOpen(); }
    static int32_t ReadU32(void* inContext, uint32_t* outValue) { return (int32_t)File(inContext).Read(*outValue); }
    static int32_t ReadI32(void* inContext, int32_t* outValue) { return (int32_t)File(inContext).Read(*outValue); }
    static int32_t ReadF32(void* inContext, float* outValue) { return (int32_t)File(inContext).Read(*outValue); }
    static int32_t ReadBool(void* inContext, bool* outValue) { return (int32_t)File(inContext).Read(*outValue); }
    static int32_t WriteU32(void* inContext, uint32_t inValue) { return (int32_t)File(inContext).Write(inValue); }
    static int32_t WriteI32(void* inContext, int32_t inValue) { return (int32_t)File(inContext).Write(inValue); }
    static int32_t WriteF32(void* inContext, float inValue) { return (int32_t)File(inContext).Write(inValue); }
    static int32_t WriteBool(void* inContext, bool inValue) { return (int32_t)File(inContext).Write(inValue); }
};

// The terrain mesh as the particles' move asks it
struct EEParticleGroundOps {
    static GETerrainMesh& Mesh() { return gWorld.mMap->mTerrainMesh; }
    static bool IsValidTile(void*, uint32_t inX, uint32_t inY) { return Mesh().GetIsValidTile(inX, inY); }
    static bool IsSpaceTile(void*, uint32_t inX, uint32_t inY) { return Mesh().GetIsSpaceTile(inX, inY); }
    static float MeshZ(void*, float inX, float inY) { return Mesh().GetMeshZ(inX, inY); }
    static void MeshNormal(void*, float inX, float inY, EeGe3DPoint* outNormal)
    {
        GEVector theNormal;
        Mesh().GetMeshNormal(inX, inY, theNormal);
        outNormal->x = theNormal.mX;
        outNormal->y = theNormal.mY;
        outNormal->z = theNormal.mZ;
    }
};

EeGfxParticleGround EEParticleGround()
{
    EeGfxParticleGround theGround = {NULL, EEParticleGroundOps::IsValidTile, EEParticleGroundOps::IsSpaceTile,
                                     EEParticleGroundOps::MeshZ, EEParticleGroundOps::MeshNormal};
    return theGround;
}

// The move's world reads (both particles) (provisional name; from the deleted EEE2DParticle.cpp)
EeGfxParticleTick EEParticleTick(EEGraphicEffect* inEffect, const GE3DPoint& inPosition)
{
    EeGfxParticleTick theTick;
    const GEVector& theWind = gWorld.mWindDirection;
    theTick.now = gPhysics->mGameClock;
    theTick.game_over = gPhysics->mGameOver;
    theTick.wind.x = theWind.GetX();
    theTick.wind.y = theWind.GetY();
    theTick.wind.z = theWind.GetZ();
    theTick.position = *reinterpret_cast<const EeGe3DPoint*>(&inPosition);
    theTick.duration = (uint32_t)inEffect->mDuration;
    theTick.param4 = inEffect->mParam4;
    theTick.param8 = inEffect->mParam8;
    theTick.param9 = inEffect->GetParam9();
    theTick.param10 = inEffect->GetParam10();
    theTick.param11 = inEffect->mParam11;
    theTick.flag2 = inEffect->mFlag2;
    return theTick;
}

EeGfxEffectFile EEEffectFile(FSFile& inFile)
{
    EeGfxEffectFile theFile = {&inFile, EEEffectFileOps::IsOpen, EEEffectFileOps::ReadU32, EEEffectFileOps::ReadI32,
                               EEEffectFileOps::ReadF32, EEEffectFileOps::ReadBool, EEEffectFileOps::WriteU32,
                               EEEffectFileOps::WriteI32, EEEffectFileOps::WriteF32, EEEffectFileOps::WriteBool};
    return theFile;
}

// The quad batches' arrays (the camera batch's vertices 0x4e8db8, the ground batch's 0x4e8da8 and their polygons
// 0x4e8df0: two triangles a quad); their textures, counts, alpha references, quads, flushes and draws are ee_gfx
// effect_batch.rs's, the shared plane models (0x4e8df4, 0x4e8df8) and GetEffectFogFactor (0x1030c8) too (RUST_PLAN
// R4.4). The GE value classes pass as their #[repr(C)] mirrors (the same storage).
static GEVertexArray* sEffectVertices[2];
static GEPolygonArray* sEffectPolygons;
extern const EeGfxRaster gEERustRaster;
static_assert(sizeof(GEColor) == sizeof(EeGfxColor) && sizeof(GETextureCoordinate) == sizeof(EeGfxTexCoord) &&
              sizeof(GEVector) == sizeof(EeGeVector) && sizeof(GE3DPoint) == sizeof(EeGe3DPoint), "GE mirrors");

// A batch's quad: its four vertices from inIndex on (the raster table's batch_quad; AddEffectQuad)
void EEEffectBatchQuad(uint32_t inWhich, const EeGfxEffectQuad* inQuad, uint32_t inIndex)
{
    const GEColor& theColor = *reinterpret_cast<const GEColor*>(&inQuad->color);
    const GEVector& theNormal = *reinterpret_cast<const GEVector*>(&inQuad->normal);
    for (unsigned long i = 0; i < 4; i++)
        sEffectVertices[inWhich]->SetVertex(theColor, *reinterpret_cast<const GE3DPoint*>(&inQuad->points[i]), theNormal,
                                            *reinterpret_cast<const GETextureCoordinate*>(&inQuad->coords[i]),
                                            inQuad->opacities[i], inIndex + i);
}

// A batch's draw: inCount vertices, half as many polygons (the raster table's batch_draw)
int32_t EEEffectBatchDraw(uint32_t inWhich, uint32_t inCount, void* inTexture)
{
    return (int32_t)gRasterizer->Draw(*sEffectPolygons, inCount / 2, *sEffectVertices[inWhich], inCount,
                                      static_cast<GETexture*>(inTexture), gScreenIndex, gOrthographicView);
}

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
    this->mKind = inKind;
    this->mKeepsUnit = inKeepsUnit;
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
    ee_gfx_effect_record_free(this->mRecord);
    this->mRecord = NULL;
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
    sEffectVertices[EE_GFX_BATCH_CAMERA] = gRasterizer->CreateVertexArray(1, 0x800);
    sEffectVertices[EE_GFX_BATCH_GROUND] = gRasterizer->CreateVertexArray(1, 0x800);
}

// FUNCTION: EXE 0x502724
// Starts both quad batches empty with the given texture, referencing it once per batch (provisional name)
void BeginEffectBatches(GETexture* inTexture) { ee_gfx_effect_batches_begin(&gEERustRaster, inTexture); }

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

// FUNCTION: EXE 0x502e1b
// Deletes the quad batches' polygon array and vertex arrays (provisional name)
void DeleteEffectBatchBuffers()
{
    if (sEffectPolygons)
        delete sEffectPolygons;
    for (int i = 0; i < 2; i++) {
        if (sEffectVertices[i])
            delete sEffectVertices[i];
    }
    sEffectPolygons = NULL;
    sEffectVertices[0] = sEffectVertices[1] = NULL;
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

// FUNCTION: EXE 0x502f20
// Drops the two texture references BeginEffectBatches took (both through the first batch's texture)
void EndEffectBatches() { ee_gfx_effect_batches_end(&gEERustRaster); }

// FUNCTION: EXE 0x502f3d
// Returns a random float between the minimum and the maximum from the effects' random generator
float GetEffectRandomFloat(float inMinimum, float inMaximum)
{
    return ee_gfx_effect_random_float(inMinimum, inMaximum);
}

// FUNCTION: EXE 0x502f57
// Returns a random long from the minimum to the maximum (clamped to the maximum) (provisional name)
long GetEffectRandomLong(long inMinimum, long inMaximum)
{
    return ee_gfx_effect_random_long((int32_t)inMinimum, (int32_t)inMaximum);
}

// FUNCTION: EXE 0x502f8b
// Returns the next random fraction from the effects' random generator
float EEEffect::GetRandom()
{
    return ee_gfx_effect_random();
}

// TEST ONLY (tools/sim_capture.py EFST): the effects' random seed
unsigned long EEEffectRandomSeed()
{
    return ee_gfx_effect_random_seed();
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
    ee_gfx_effect_init(this->mRecord);
}

// FUNCTION: EXE 0x503027
// Reads the graphic effect id (and loads its media), region, saved unit id, handle and keeps-unit flag; returns
// the first read error
// (the reads are ee_gfx effect_state.rs's; the graphic effect is resolved after them: its media load reads no file)
long EEEffect::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EeGfxEffectFile theFile = EEEffectFile(inFile);
    EeGfxEffectLoaded theLoaded;
    long theResult = ee_gfx_effect_load(this->mRecord, &theFile, &theLoaded);
    if (theLoaded.has_graphic_effect) {
        this->mGraphicEffect = EEGraphicEffect::Get(theLoaded.graphic_effect_id);
        this->mGraphicEffect->Reference();
        this->LoadMedia(false);
    }
    if (theLoaded.has_unit)
        this->mUnit = reinterpret_cast<EEUnit*>((uintptr_t)theLoaded.unit_id);
    return theResult;
}

// FUNCTION: EXE 0x5030ab
// Creates the two shared quad models (provisional name)
void CreatePlaneModels() { ee_gfx_effect_planes_create(&gEERustRaster); }

// FUNCTION: EXE 0x5030b5
// Loads the graphic effect's media (on demand or now)
void EEEffect::LoadMedia(bool inIsOnDemand)
{
    if (this->mGraphicEffect)
        this->mGraphicEffect->Load(inIsOnDemand);
}

// FUNCTION: EXE 0x503f64
// Draws whatever the ground batch and then the camera batch still hold (provisional name)
void FlushEffectBatches() { ee_gfx_effect_batches_flush(&gEERustRaster); }

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
    EeGfxEffectFile theFile = EEEffectFile(outFile);
    return ee_gfx_effect_save(this->mRecord, &theFile, (int32_t)this->mGraphicEffect->mID,
                              this->mUnit ? (uint32_t)this->mUnit->mUnitID : 0xffffffffu);
}

// FUNCTION: EXE 0x50409b
// Moves the effect to the map region under a point when that region changed (mTileX/mTileY hold region
// coordinates)
void EEEffect::MoveToPoint(const GE3DPoint& inLocation)
{
    this->MoveToRegion(inLocation.GetX(), inLocation.GetY());
}

// Moves the effect to the map region under (inX, inY) when that region changed: leaves the old region's list, joins
// the new one (the region is ee_gfx effect_state.rs's) (provisional name)
void EEEffect::MoveToRegion(float inX, float inY)
{
    RTSRegionTable<EEEffect*>* theTable = gWorld.mMap->mEffectRegions;
    int32_t theX, theY;
    if (ee_gfx_effect_region_changed(this->mRecord, inX, inY, (int32_t)theTable->mRegionSize, &theX, &theY)) {
        EEEffect* theEffect = this;
        RTSRegion<EEEffect*>* theList = theTable->GetRegion(this->mTileX, this->mTileY);
        if (theList != NULL)
            theList->Remove(theEffect);
        this->mTileX = theX;
        this->mTileY = theY;
        theList = theTable->GetRegion(this->mTileX, this->mTileY);
        if (theList != NULL)
            theList->push_back(theEffect);
    }
}

// FUNCTION: EXE 0x504175
// Moves the effect to the map region under a transformation's translation when that region changed
void EEEffect::MoveToTransformation(const GETransformation& inTransformation)
{
    this->MoveToRegion(inTransformation.GetX(), inTransformation.GetY());
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
void ReleasePlaneModels() { ee_gfx_effect_planes_release(&gEERustRaster); }

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
