// C:\EEX\Empire Earth\Graphics Thread\EEEffect.cpp (no __FILE__; name from the RTTI class and its key
// "EEEffect-Data"). The graphics thread's effects (vtable 0x43f9fc): 0x10251d..0x10428c (the key chain, the file's
// URandom, the ctor .. Virtual20, the shared effect models and the two quad batches the 2D effects draw into), since
// R4.4's twenty-fifth round with what was EEEffect2D.cpp (0x10428c..0x1050fb) and EEEffect3D.cpp (0x1050fb..0x1054ec)
// and the leaves' lifetimes: one class for every kind (ee/EEEffect.h); its constructors and the world's answers to its
// Rust calls are EEEffectHost.cpp's.
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
#include "lle/GEModelManager.h"
#include "lle/GEPolygonArray.h"
#include "lle/GEQuaternion.h"
#include "ee/EEWorld.h"
#include "ee/EERenderSnapshot.h"
#include "rust/ee_ge.h"   // ee_gfx effects.rs (RUST_PLAN R4.4)
#include "rust/ee_gfx.h"
#include "ee/EEEffectMesh.h"
#include "lle/SESoundManager.h"
#include "ee/EEGameSound.h"
#include "lle/SESMInstanceNode.h"
#include "lle/U2DPoint.h"
#include "ee/EESoundObject.h"

const UString EEEffect::sData("EEEffect-Data");

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

// Starts both quad batches empty with the given texture, referencing it once per batch (provisional name)
void BeginEffectBatches(GETexture* inTexture) { ee_gfx_effect_batches_begin(&gEERustRaster, inTexture); }


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

// Drops the two texture references BeginEffectBatches took (both through the first batch's texture)
void EndEffectBatches() { ee_gfx_effect_batches_end(&gEERustRaster); }

// Creates the two shared quad models (provisional name)
void CreatePlaneModels() { ee_gfx_effect_planes_create(&gEERustRaster); }

// Draws whatever the ground batch and then the camera batch still hold (provisional name)
void FlushEffectBatches() { ee_gfx_effect_batches_flush(&gEERustRaster); }

// Releases the two shared quad models (provisional name)
void ReleasePlaneModels() { ee_gfx_effect_planes_release(&gEERustRaster); }

// The original base constructor's part (EEEffect 0x10258e: Init, both pointers referenced, the kind, the keeps-unit
// flag, the media load), then the kind's class part
void EEEffect::Attach(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind, bool inKeepsUnit)
{
    ee_gfx_effect_init(this->mRecord);
    this->mUnit = inUnit;
    if (this->mUnit)
        this->mUnit->Reference();
    this->mGraphicEffect = inGraphicEffect;
    if (this->mGraphicEffect)
        this->mGraphicEffect->Reference();
    this->mKind = inKind;
    this->mKeepsUnit = inKeepsUnit;
    if (this->mGraphicEffect)
        this->mGraphicEffect->Load(gLoadingSavedGame == false);
    this->Begin(inKind);
}

// The kind's class: a 2D kind (EEEffect2D's constructor 0xdf44e) references its shared plane model, the ground plane
// for the shockwave's, confirmation's, pulse wave's, ripple's, vortex's and decal's (0x4e8df4), else the standing one
// (0x4e8df8); its bounding radius 0 (a new record's)
void EEEffect::Begin(long inKind)
{
    this->mKind = inKind;
    this->mIs2D = ee_gfx_effect_is_2d((int32_t)inKind);
    if (!this->mIs2D)
        return;
    bool theGround = inKind == 0x12 || inKind == 0x41 || inKind == 0x2f || inKind == 0x22 || inKind == 0x31 || inKind == 0x32;
    this->mModel = static_cast<GEModel*>(ee_gfx_effect_plane_model(!theGround));
    if (this->mModel)
        this->mModel->Reference();
    if (inKind == 0x2e)   // the particle's motion, age, look and timing cleared (EEE2DParticle::Clear 0xe4be3)
        ee_gfx_effect_particle_clear(this->mRecord);
}

// for loading (the leaves' load-time constructors: the record's kind; its new record holds Init's values)
EEEffect::EEEffect(long inKind)
{
    this->Begin(inKind);
}

// Destroys the effect: the leaves' parts (the marching line's regions, the sounds, the rain's sound media, the storms'
// and the farm's and earthquake's models, the kept graphic objects, the tracer beam's target, the marching line's
// ends), the 2D parts and plane model (EEEffect2D 0x1042cf), then the base's: the graphic effect unloaded and
// released, the map region left, the unit released
EEEffect::~EEEffect()
{
    if (this->mKind == 0x2d)
        this->RemoveRegions();
    SESMInstanceNode** theSounds[2] = {&this->mSound, &this->mRumble};
    for (int i = 0; i < 2; i++) {
        if (*theSounds[i]) {
            gSoundManager->Stop(*theSounds[i], false);
            (*theSounds[i])->Dereference();
            *theSounds[i] = NULL;
        }
    }
    if (this->mKind == 0x2a) {   // the rain's three sound objects' media (0xf74a0)
        for (int i = 0; i < 3; i++) {
            EESoundObject* theSound = EESoundObject::Get(this->mRecord->storm.sounds[i]);
            if (theSound)
                theSound->UnloadMedia();
        }
    }
    void* theModels[5] = {NULL, NULL, NULL, NULL, NULL};
    if (this->mKind == 0x2a || this->mKind == 0x3d)
        theModels[0] = this->mRecord->storm.model;
    if (this->mKind == 0x29) {   // the farm's ground model and draw copies (0xee775)
        theModels[1] = this->mRecord->farm.ground;
        theModels[2] = this->mRecord->farm.ground_copy;
        theModels[3] = this->mRecord->farm.crops_copy;
    }
    if (this->mKind == 0x1a)   // the earthquake's model (0xec88f)
        theModels[4] = this->mRecord->quake.model;
    EEGraphicObject** theObjects[2] = {&this->mGraphicObject, &this->mUnitModel};
    for (int i = 0; i < 2; i++) {
        if (theModels[0] && i == 0)
            static_cast<GEModel*>(theModels[0])->Dereference();
        if (*theObjects[i]) {
            (*theObjects[i])->UnloadMedia();
            *theObjects[i] = NULL;
        }
    }
    if (this->mTarget != NULL && this->mTarget != (EEUnit*)-1)
        this->mTarget->Dereference();
    // the marching line's units (not referenced by a repair: as the original); still saved ids after a load that
    // failed before the repair (mUnitIsSavedID): no units
    if (this->mFromUnit && !this->mUnitIsSavedID)
        this->mFromUnit->Dereference();
    if (this->mToUnit && !this->mUnitIsSavedID)
        this->mToUnit->Dereference();
    if (this->mVolcanoModel)
        this->mVolcanoModel->Dereference();
    for (int i = 1; i < 5; i++) {
        if (theModels[i])
            static_cast<GEModel*>(theModels[i])->Dereference();
    }
    for (unsigned long i = 0; i < this->mParts.size(); i++)
        this->mParts[i].mGraphicObject->UnloadMedia();
    if (this->mModel) {
        this->mModel->Dereference();
        this->mModel = NULL;
    }
    this->ReleaseBase();
    ee_gfx_effect_record_free(this->mRecord);
    this->mRecord = NULL;
}

// The base's release (EEEffect's destructor 0x102600 and ReleaseResources 0x102e61): the graphic effect unloaded and
// released, the map region left, the unit released
void EEEffect::ReleaseBase()
{
    if (this->mGraphicEffect != NULL) {
        this->mGraphicEffect->Unload();
        this->mGraphicEffect->Dereference();
        this->mGraphicEffect = NULL;
    }
    if (gWorld.mMap != NULL && this->mTileX != -1 && this->mTileY != -1) {
        EEEffect* theEffect = this;
        RTSRegion<EEEffect*>* theList = gWorld.mMap->mEffectRegions->GetRegion(this->mTileX, this->mTileY);
        if (theList != NULL)
            theList->Remove(theEffect);
    }
    if (this->mUnit != NULL && !this->mUnitIsSavedID)   // (a failed load's effect: the id is no unit)
        this->mUnit->Dereference();
    this->mUnit = NULL;
    this->mUnitIsSavedID = false;
}

// Releases what the destructor releases without destroying the effect: the particles' graphic object (0xf4f30,
// 0xe4b2b), the marching line's regions (0xf3882), the 2D parts and plane model (0x1048b1), the base's
void EEEffect::ReleaseResources()
{
    if ((this->mKind == 0x39 || this->mKind == 0x2e) && this->mGraphicObject) {
        this->mGraphicObject->UnloadMedia();
        this->mGraphicObject = NULL;
    }
    if (this->mKind == 0x2d)
        this->RemoveRegions();
    if (this->mIs2D) {
        for (unsigned long i = 0; i < this->mParts.size(); i++)
            this->mParts[i].mGraphicObject->UnloadMedia();
        this->mParts.clear();
        if (this->mModel) {
            this->mModel->Dereference();
            this->mModel = NULL;
        }
    }
    this->ReleaseBase();
    this->mTileX = -1;
    this->mTileY = -1;
}

// Returns the unit's position (physical model, else its tile centre at the tile's height; the origin without a
// unit) in a shared static point
const GE3DPoint& EEEffect::GetUnitLocation()
{
    static GE3DPoint sLocation;
    EEUnit* theUnit = this->mUnit;
    sLocation = GE3DPoint(0.0f, 0.0f, 0.0f);
    if (theUnit && theUnit->mComplexUnit) {
        const GETransformation& theLocation = theUnit->mComplexUnit->mPhysicalModel.GetLocation();
        sLocation = GE3DPoint(theLocation.GetX(), theLocation.GetY(), theLocation.GetZ());
    } else if (theUnit) {
        sLocation = GE3DPoint(theUnit->mTileX + 0.5f, theUnit->GetTileY() + 0.5f, theUnit->mTile ? theUnit->mTile->mHeight : 0.0f);
    }
    return sLocation;
}

// The base queues: 10 (skip) for an effect shown only to another player; with inInView also off the map, on a tile
// the current player does not see (the 3D kinds' IsTileVisible) unless the camera is free, or with its bounding
// sphere out of view (EEEffect2D::Queue 0x104332 at its position, EEEffect3D::Queue 0x10513e at its translation)
long EEEffect::BaseQueue(bool inInView)
{
    if (this->mIsLocalOnly && this->mPlayer != gWorld.mCurrentPlayer)
        return 10;
    if (!inInView)
        return 0;
    GE3DPoint theLocation = this->mIs2D ? this->mPosition
                                        : GE3DPoint(this->mTransformation.GetX(), this->mTransformation.GetY(),
                                                    this->mTransformation.GetZ());
    EEMapTile* theTile = gWorld.mMap->GetTile(UMFloor(theLocation.GetX()), UMFloor(theLocation.GetY()));
    if (theTile == NULL)
        return 10;
    bool theSeen = this->mIs2D ? theTile->IsSeenBy(gWorld.GetCurrentPlayer()) : this->IsTileVisible(theTile);
    if (!theSeen && !gFreeCamera)
        return 10;
    if (!ee_gfx_effect_in_view(reinterpret_cast<const EeGeViewport*>(&gViewport), gOrthographicView,
                               reinterpret_cast<const EeGe3DPoint*>(&theLocation), this->mBoundingRadius))
        return 10;
    return 0;
}

// Returns a random float between the minimum and the maximum from the effects' random generator
float GetEffectRandomFloat(float inMinimum, float inMaximum)
{
    return ee_gfx_effect_random_float(inMinimum, inMaximum);
}

// Returns a random long from the minimum to the maximum (clamped to the maximum) (provisional name)
long GetEffectRandomLong(long inMinimum, long inMaximum)
{
    return ee_gfx_effect_random_long((int32_t)inMinimum, (int32_t)inMaximum);
}

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

// Returns the graphic object (media) of the effect's graphic effect, or null without one
EEGraphicObject* EEEffect::GetGraphicObject()
{
    return this->mGraphicEffect ? static_cast<EEGraphicObject*>(this->mGraphicEffect->GetMedia()) : 0;
}

// The 2D parts as ee_gfx effect_state.rs's Load and Save use them (the context: the effect): a loaded part takes the
// effect's graphic object and is kept when its media loads
struct EEEffectPartOps {
    static EEEffect& Effect(void* inContext) { return *static_cast<EEEffect*>(inContext); }
    static int32_t Push(void* inContext, const EeGfxEffectPart* inPart)
    {
        EEEffect2DPart thePart = {*reinterpret_cast<const GE3DPoint*>(&inPart->position), Effect(inContext).GetGraphicObject(),
                                  inPart->tag, inPart->frame, 0, inPart->time, inPart->done};
        long theResult = thePart.mGraphicObject->LoadMedia();
        if (theResult == 0)
            Effect(inContext).mParts.push_back(thePart);
        return (int32_t)theResult;
    }
    static uint32_t Count(void* inContext) { return (uint32_t)Effect(inContext).mParts.size(); }
    static void Get(void* inContext, uint32_t inIndex, EeGfxEffectPart* outPart)
    {
        const EEEffect2DPart& thePart = Effect(inContext).mParts[inIndex];
        outPart->position = *reinterpret_cast<const EeGe3DPoint*>(&thePart.mPosition);
        outPart->tag = (uint32_t)thePart.mTag;
        outPart->frame = (uint32_t)thePart.mFrame;
        outPart->time = thePart.mTime;
        outPart->done = thePart.mIsDone;
    }
};

// Reads the base's state (the graphic effect id, whose media loads, the region, the saved unit id, the handle and the
// keeps-unit flag; ee_gfx effect_state.rs: its result ignored by every class), then the 3D kinds' transformation and
// radius (0x105484) or the 2D kinds' position, radius and parts (0x104985), then the kind's own (ee_gfx); the command
// confirmation is never saved (0 without the base's), the explosion has no state of its own, the damage controller its
// children's handles; returns the first error, as each original
long EEEffect::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    if (this->mKind == 0x41)
        return this->Call(EeGfxEffect3DOp::Load);
    EeGfxEffectFile theFile = EEEffectFile(inFile);
    EeGfxEffectLoaded theLoaded;
    ee_gfx_effect_load(this->mRecord, &theFile, &theLoaded);
    if (theLoaded.has_graphic_effect) {
        this->mGraphicEffect = EEGraphicEffect::Get(theLoaded.graphic_effect_id);
        this->mGraphicEffect->Reference();
        this->mGraphicEffect->Load(false);
    }
    if (theLoaded.has_unit)
        this->mUnit = reinterpret_cast<EEUnit*>((uintptr_t)theLoaded.unit_id);
    this->mUnitIsSavedID = true;   // (until RepairPointers: the kinds' saved unit ids too)
    if (!this->mIs2D) {
        long theBase = ee_gfx_effect_load_3d(this->mRecord, &theFile);
        if (this->mKind == 0x40)
            this->mTarget = (EEUnit*)-1;   // the tracer beam's: no target saved (RepairPointers)
        return this->Call(EeGfxEffect3DOp::Load, NULL, &theFile, theBase, inMajorVersion, inMinorVersion);
    }
    EeGfxEffectParts theParts = {this, EEEffectPartOps::Push, EEEffectPartOps::Count, EEEffectPartOps::Get};
    long theBase = ee_gfx_effect_load_2d(this->mRecord, &theFile, &theParts);
    if (this->mKind == 0x3e)   // the children's handles and, from 0.249 on, the finished flag (0x1020e2)
        return ee_gfx_effect_load_damaged(this->mRecord, &theFile, (uint32_t)inMajorVersion, (uint32_t)inMinorVersion);
    if (this->mKind == 4)
        return theBase;
    return this->Call(EeGfxEffect3DOp::Load, NULL, &theFile, 0, inMajorVersion, inMinorVersion);
}

// Writes the base's state (the graphic effect id, the region, the unit id or -1, the handle, the keeps-unit flag;
// its result ignored by every class), then the 3D kinds' transformation and radius (0x1054b8) or the 2D kinds'
// position, radius and parts (0x104a8a; the particle where it was drawn, 0xe517c), then the kind's own (ee_gfx; the
// marching icons stop at the 2D part's error); the command confirmation is never saved
long EEEffect::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    if (this->mKind == 0x41)
        return this->Call(EeGfxEffect3DOp::Save);
    GE3DPoint& theDrawn = *reinterpret_cast<GE3DPoint*>(&this->mRecord->particle.draw_position);
    if (this->mKind == 0x2e && this->mPosition != theDrawn)
        this->SetPosition(theDrawn);
    EeGfxEffectFile theFile = EEEffectFile(outFile);
    ee_gfx_effect_save(this->mRecord, &theFile, (int32_t)this->mGraphicEffect->mID,
                       this->mUnit ? (uint32_t)this->mUnit->mUnitID : 0xffffffffu);
    if (!this->mIs2D) {
        long theBase = ee_gfx_effect_save_3d(this->mRecord, &theFile);
        return this->Call(EeGfxEffect3DOp::Save, NULL, &theFile, theBase, inMajorVersion, inMinorVersion);
    }
    EeGfxEffectParts theParts = {this, EEEffectPartOps::Push, EEEffectPartOps::Count, EEEffectPartOps::Get};
    long theBase = ee_gfx_effect_save_2d(this->mRecord, &theFile, &theParts);
    if (this->mKind == 0x3e)
        return ee_gfx_effect_save_damaged(this->mRecord, &theFile, this->mGraphicEffect->mLinkedEffect2 != NULL);
    if (this->mKind == 4 || ((this->mKind == 0xf || this->mKind == 0x10) && theBase != 0))
        return theBase;
    return this->Call(EeGfxEffect3DOp::Save, NULL, &theFile, 0, inMajorVersion, inMinorVersion);
}

// After a load: the saved unit id back to the unit (7 when it is gone) and the map region joined (the base's; not
// the command confirmation's), then the tracer beam's target (0xfd000), the marching line's ends (0xf3ee8), the kind's
// own (ee_gfx), or the light glow's set-up again (0xe25a1)
long EEEffect::RepairPointers()
{
    if (this->mKind == 0x41)
        return this->Call(EeGfxEffect3DOp::Repair);
    long theResult = 0;
    this->mUnitIsSavedID = false;
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
    RTSRegion<EEEffect*>* theList = gWorld.mMap->mEffectRegions->GetRegion(this->mTileX, this->mTileY);
    if (theList != NULL)
        theList->push_back(theEffect);
    if (this->mIs2D)
        return this->mKind == 0xd && theResult == 0 ? this->SetUp() : theResult;
    if (this->mKind == 0x40) {
        if (theResult == 0 && this->mTarget != (EEUnit*)-1) {
            this->mTarget = dynamic_cast<EEUnit*>(gWorld.GetUnit(USavedID(this->mTarget)));
            if (this->mTarget == NULL)
                theResult = 7;
            else
                this->mTarget->Reference();
        } else
            this->mTarget = NULL;
        return theResult;
    }
    if (this->mKind == 0x2d && theResult == 0) {
        if (USavedID(this->mFromUnit) > 0)
            this->mFromUnit = static_cast<EEUnit*>(::gWorld.GetUnit(USavedID(this->mFromUnit)));
        if (USavedID(this->mToUnit) > 0)
            this->mToUnit = static_cast<EEUnit*>(::gWorld.GetUnit(USavedID(this->mToUnit)));
    } else if (this->mKind == 0x2d) {   // port: a failed repair drops the effect; its ends are ids, not units
        this->mFromUnit = NULL;
        this->mToUnit = NULL;
    }
    return theResult != 0 ? theResult : this->Call(EeGfxEffect3DOp::Repair);
}

// Moves the effect to the map region under a point (the marching line: its regions along it again, 0xf3e29)
void EEEffect::MoveToPoint(const GE3DPoint& inLocation)
{
    if (this->mKind == 0x2d)
        this->RemoveRegions();
    this->MoveToRegion(inLocation.GetX(), inLocation.GetY());
    if (this->mKind == 0x2d)
        this->AddRegions(inLocation, *reinterpret_cast<const GE3DPoint*>(&this->mRecord->march.to));
}

// Moves the effect to the map region under a transformation's translation (the marching line's: 0xf3e52)
void EEEffect::MoveToTransformation(const GETransformation& inTransformation)
{
    this->MoveToPoint(GE3DPoint(inTransformation.GetX(), inTransformation.GetY(), inTransformation.GetZ()));
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

void EEEffect::SetPosition(const GE3DPoint& inPosition)
{
    this->mPosition = inPosition;
    this->MoveToPoint(inPosition);
}

void EEEffect::SetTransformation(const GETransformation& inTransformation)
{
    this->mTransformation = inTransformation;
    this->MoveToTransformation(inTransformation);
}

// Moves the effect from the old unit to the new one (when it is on the old unit and does not keep its unit); the
// marching line's ends (0xf4017) and the tracer beam's target unless it keeps its unit (0xfd10f); the farm's is empty
void EEEffect::ReplaceUnit(EEUnit* inOldUnit, EEUnit* inNewUnit)
{
    if (this->mKind == 0x29)
        return;
    if (this->mUnit == inOldUnit && !this->mKeepsUnit) {
        this->mUnit->Dereference();
        this->mUnit = inNewUnit;
        this->mUnit->Reference();
    }
    EEUnit** theEnds[3] = {&this->mFromUnit, &this->mToUnit, &this->mTarget};
    for (int i = 0; i < 3; i++) {
        bool theMoves = i < 2 ? this->mKind == 0x2d : this->mKind == 0x40 && !this->mKeepsUnit;
        if (theMoves && inOldUnit == *theEnds[i]) {
            (*theEnds[i])->Dereference();
            *theEnds[i] = inNewUnit;
            inNewUnit->Reference();
        }
    }
}
