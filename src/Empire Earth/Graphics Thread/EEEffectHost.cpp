// The effects' world (docs/RUST_PLAN.md R4.4; the graphics + LLE cluster's second round): the effects and their
// manager are Rust objects (ee_gfx effect_obj.rs, ffi_manager.rs); this unit answers what they ask of the C++ world.
// The effects' constructors' weapon points (the units' EECombatEffectWeaponPoint), the C++
// objects an effect holds (its unit, graphic effect, models, graphic objects and sounds: their references, loads and
// releases), the map's tiles and doppelgangers, the answers of the 3D kinds (EeGfxEffectAnswers3D: ee_gfx
// effect_rising.rs and its kinds' modules) and of the 2D ones (EeGfxEffectAnswers2D: effect_2d.rs) that call the
// units, sounds, area effects and the map, the damage controller's tick facts, the goal markers' questions, the TEST
// ONLY captures' file and records, the effects' file over an FSFile, the particles' ground, the quad batches' arrays
// and the battlefield decals' entry points. Since R4.4's twenty-fifth round one file for what was EEE3DRust.cpp and
// EEE2DRust.cpp (the original's 60 effect leaves, deleted round by round), since the second cluster round also for
// EEEffect.cpp, EEEffectManager.cpp and EEGraphicalGoalManager.cpp.
#include <windows.h>
#include <math.h>
#include <utility>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "lle/SESoundManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEGraphics.h"
#include "ee/EEMedia.h"
#include "ee/EEGameSound.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEGraphicObject.h"
#include "ee/EECombatFormulas.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEDoppelgangerManager.h"
#include "ee/EEUnitUtilities.h"
#include "ee/EEGamePreferences.h"
#include "lle/GEPolygonArray.h"
#include "lle/GEModelManager.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEEffectMesh.h"
#include "ee/EEDoppelganger.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EESoundObject.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEAEManager.h"
#include "ee/EESaveGameFiles.h"
#include "rust/base/math.h"
#include "rust/render/gfx.h"
#include "rust/sim/units.h"
#include "rust/game/gamecpp.h"

class EEComplexUnit;
// 0x23cee2 (EERenderLists.h): the unit's draw opacity (the farm's)
bool Fn23CEE2(EEComplexUnit* inUnit, bool inTranslucent, float& ioOpacity);

// the classes are not standard-layout; offsetof of a non-virtual base's member is the member's place in every object
#if defined(__clang__)
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#endif

// The effects' layout, world and manager world: gamecpp fx_world.rs (EECore9OpB's round 11 cases below) registers them before
// main; the physical model's scale and frame are GEPhysicalModel's friend's (here)
void EEGfxObjectLayoutRegister()
{
    ee_gamecpp_fw_register(offsetof(EEComplexUnit, mPhysicalModel) + offsetof(GEPhysicalModel, mScale), offsetof(EEComplexUnit, mPhysicalModel) + offsetof(GEPhysicalModel, mFrame));
}

namespace {
const bool kRegistered = (EEGfxObjectLayoutRegister(), true);
}

// the effects' file, the manager's Load, Save and entry points, an effect's local flag: gamecpp fx_world.rs
EeGfxEffectFile EEEffectFile(FSFile& inFile) { return ee_gamecpp_fw_file(&inFile); }
long EEEffectManager::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_gamecpp_fw_load(this->mRust, &inFile, inMajorVersion, inMinorVersion); }
long EEEffectManager::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return ee_gamecpp_fw_save(this->mRust, &outFile, inMajorVersion, inMinorVersion); }
long EEEffectManager::CreateUnitEffect(EEUnit* inUnit, EEGraphicEffect* inEffect, EEEffectHandle* outHandle, bool inKeepsUnit, long inTagPoint, long inGoalIndex) { return ee_gamecpp_fw_unit_effect(this->mRust, inUnit, inEffect, reinterpret_cast<uint32_t*>(outHandle), inKeepsUnit, inTagPoint, inGoalIndex); }
long EEEffectManager::CreateEffectBetweenUnits(EEUnit* inFrom, EEUnit* inTo, EEGraphicEffect* inEffect, EEEffectHandle* outHandle) { return ee_gamecpp_fw_between_units(this->mRust, inFrom, inTo, inEffect, reinterpret_cast<uint32_t*>(outHandle)); }
long EEEffectManager::CreateEffect(EEUnit* inUnit, const GE3DPoint& inLocation, EEGraphicEffect* inEffect, EEEffectHandle* outHandle, long inGoalIndex) { return ee_gamecpp_fw_unit_to_point(this->mRust, inUnit, reinterpret_cast<const EeGe3DPoint*>(&inLocation), inEffect, reinterpret_cast<uint32_t*>(outHandle), inGoalIndex); }
long EEEffectManager::CreatePlacedEffect(const GE3DPoint& inLocation, EEGraphicEffect* inEffect, EEEffectHandle* outHandle) { return ee_gamecpp_fw_placed(this->mRust, reinterpret_cast<const EeGe3DPoint*>(&inLocation), inEffect, reinterpret_cast<uint32_t*>(outHandle)); }
long EEEffectManager::CreateTransformationEffect(const GETransformation& inTransformation, EEGraphicEffect* inEffect, EEEffectHandle* outHandle) { return ee_gamecpp_fw_transformation(this->mRust, reinterpret_cast<const EeGeTransformation*>(&inTransformation), inEffect, reinterpret_cast<uint32_t*>(outHandle)); }
long EEEffectManager::CreateEffectBetween(const GE3DPoint& inFrom, const GE3DPoint& inTo, EEGraphicEffect* inEffect, EEEffectHandle* outHandle) { return ee_gamecpp_fw_between(this->mRust, reinterpret_cast<const EeGe3DPoint*>(&inFrom), reinterpret_cast<const EeGe3DPoint*>(&inTo), inEffect, reinterpret_cast<uint32_t*>(outHandle)); }
long EEEffectManager::CreateWeatherEffect(const std::vector<EETileCoordinate>& inTiles, EEGraphicEffect* inEffect, EEEffectHandle* outHandle) { return ee_gamecpp_fw_weather(this->mRust, reinterpret_cast<const long*>(inTiles.data()), inTiles.size(), inEffect, reinterpret_cast<uint32_t*>(outHandle)); }
void EEEffect::SetLocalTo(long inPlayer) { ee_gamecpp_fw_set_local(this->mRecord, inPlayer); }

// the weather storms' (EEEMStorm's): every tile one step towards its place around the new centre, the storm
// recentred on the terrain there; the tiles' bounds (gamecpp ro_views.rs over ee_gfx effect_storm.rs)
void EEEffect::Move(const U2DPoint<long>& inCenter, U2DRectangle<long>& outBounds)
{
    ee_gamecpp_fx_storm_move(this, inCenter.mX, inCenter.mY, reinterpret_cast<EeGameCppTileBounds*>(&outBounds));
}

void EEEffect::GetBounds(U2DRectangle<long>& outBounds)
{
    ee_gamecpp_fx_storm_bounds(this->mRecord, reinterpret_cast<EeGameCppTileBounds*>(&outBounds));
}

U2DPoint<long> EEEffect::GetCenterPoint() const
{
    return U2DPoint<long>(this->mRecord->storm.center_x, this->mRecord->storm.center_y);
}

void EEEffectCaptureSweep()
{
    ee_gfx_effects_sweep(gEffectManager != NULL ? gEffectManager->mRust : NULL);
}

// The quad batches' arrays (the camera batch's vertices 0x4e8db8, the ground batch's 0x4e8da8 and their polygons
// 0x4e8df0: two triangles a quad); their textures, counts, alpha references, quads, flushes and draws are ee_gfx
// effect_batch.rs's, the shared plane models (0x4e8df4, 0x4e8df8) too. The GE value classes pass as their mirrors.
static GEVertexArray* sEffectVertices[2];
static GEPolygonArray* sEffectPolygons;

// Creates the quad batches' shared polygon array (two triangles per quad, 0x200 quads) and both vertex arrays
// (provisional name)
void CreateEffectBatchBuffers()
{
    ee_gamecpp_fx_batch_create();
}

// Deletes the quad batches' polygon array and vertex arrays (provisional name)
void DeleteEffectBatchBuffers()
{
    ee_gamecpp_fx_batch_delete();
}

// Creates and releases the two shared quad models (provisional names)
void CreatePlaneModels() { ee_gfx_effect_planes_create(ee_gamecpp_ro_raster()); }
void ReleasePlaneModels() { ee_gfx_effect_planes_release(ee_gamecpp_ro_raster()); }

// Places up to N shell-hole decals of the unit's class around a point (ambient effects on; ee_gfx
// effect_placement.rs over the manager's list); the first one renews a decal already on that spot instead, unless
// its scatter roll moves it
void EEPlaceImpactDecal(EEComplexUnit* inUnit, GE3DPoint& inLocation)
{
    ee_gamecpp_fx_place_impact_decal(inUnit, reinterpret_cast<const EeGe3DPoint*>(&inLocation));
}

// Places the unit class's blood-stain decal under the unit if ambient effects and blood are on and its roll passes
// (moved forward along the heading for giraffes and horses)
void EEPlaceTrackDecal(EEComplexUnit* inUnit)
{
    ee_gamecpp_fx_place_track_decal(inUnit);
}

// ------------------------------------------------------------------------- round 9, lane B: this file's bodies' C++

// the map's cases (EEMap.cpp), then lane C's
int64_t EECore9OpB2(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

// the C++ calls and members of this file's bodies in Rust (gamecpp; EeGameCpp's lane B section): one statement each
// the constants the Rust names (gamecpp effect_host.rs)
static_assert(kTerrainDeepWater == 1 && kTerrainShallowWater == 10 && kUnitCategoryGate == 0x19 && kUnitTypeWallSpace == 0x2fe &&
                  kUnitTypeGateSpace == 0x2ff && kUnitTypeTownCenter == 0x23 && kUnitTypeCapitol == 0x29 &&
                  kUnitTypeResGiraffe == 0x124 && kUnitTypeResHorse == 0x175 && kGoalLocation == 1 && kGoalAttackUnit == 2 &&
                  kGoalUnloadTransport == 8 && kGoalLoadGarrison == 12 && kTechEpochAtomicWW2 == 10 &&
                  kPixelFormatA8R8G8B8 == 0 && kSurfaceSystemMemory == 4,
              "gamecpp effect_host.rs");
static_assert(sizeof(GEColor) == sizeof(EeGfxColor) && sizeof(GEVector) == sizeof(EeGeVector) &&
                  sizeof(EETileCoordinate) == 2 * sizeof(long) && sizeof(GESurfaceDescription) == 9 * sizeof(unsigned long),
              "gamecpp effect_host.rs: the values the Rust passes by address");

#define P64(x) ((int64_t)(intptr_t)(x))
#define FXPT(i) (*(GE3DPoint*)a[i])
#define FXUN ((EEUnit*)p)
#define FXCX ((EEComplexUnit*)p)
#define FXQUAD ((const EeGfxEffectQuad*)a[1])
#define M ((EEMap*)p)

// round 11, lane A: the effects' world tables' layout (gamecpp fx_world.rs)

// where Base sits in a Derived object (a cast of a made-up address: nothing is read)
#define FW_BASE(D, B) ((int64_t)(reinterpret_cast<uintptr_t>(static_cast<B*>(reinterpret_cast<D*>((uintptr_t)0x10000))) - 0x10000))
#define FW_GOBJ(m) offsetof(EEGraphicObject, m)
static_assert(sizeof(std::vector<EETurretPart*>) == 3 * sizeof(void*), "fx_world.rs: a vector is three words");
static_assert(offsetof(EEGraphicEffect, mLinkedEffect3) == offsetof(EEGraphicEffect, mLinkedEffect1) + 2 * sizeof(void*), "the links in order");
static_assert(sizeof(EEEffectHandle) == sizeof(uint32_t) && sizeof(U2DPoint<long>) == 2 * sizeof(long), "fx_world.rs: the handles and tiles");

int64_t EECore9OpB(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    // the storms' map, the terrain under a volcano and a farm
    case O::FxTileTerrainType: return ((EEMapTile*)p)->mTerrainType;
    case O::FxNextTile: EEGetNextTile(*(const EETileCoordinate*)a[0], *(EETileCoordinate*)a[1], (long)a[2], 1);
        break;
    case O::FxLandRipple: return P64(EEGraphicEffect::Get(kGraphicEffectLandRipple));
    case O::FxWorldPlacedEffect: return gWorld.GetEffectManager()->CreatePlacedEffect(FXPT(0), (EEGraphicEffect*)a[1], NULL);
    case O::FxTerrainColor: *(GEColor*)a[2] = EETerrainTable::Get(gWorld.mMap->mTerrainMesh.GetTextureSetID((unsigned long)a[0], (unsigned long)a[1]))->mColor;
        break;
    case O::FxVertexColor: *(GEColor*)a[3] = gWorld.mMap->GetVertexColor((long)a[0], (long)a[1], (long)a[2]);
        break;
    case O::FxVertexNormal: *(GEVector*)a[2] = *gWorld.mMap->mTerrainMesh.GetVertexNormal((unsigned long)a[0], (unsigned long)a[1]);
        break;
    // the sounds
    case O::FxComputeSound: EEComputeSoundParameters(f[0], FXPT(0), *(float*)a[1], *(float*)a[2], *(float*)a[3], f[1]);
        break;
    case O::FxSetSoundParameters: gSoundManager->SetSoundParameters((SESMInstanceNode*)p, f[0], f[1], f[2]);
        break;
    case O::FxPlayAnimationSound: return P64(PlayAnimationSound(FXPT(0), (EEGraphicObject*)a[1], 0, (SESMInstanceNode*)a[2], (unsigned long)a[3], a[4] != 0));
    case O::FxPlayNodeNamed: return P64(gSoundManager->PlayAndReturnReferencedNode(((EESoundObject*)p)->GetSampleID(), ((EESoundObject*)p)->GetVolume(), 0.0f, 1.0f, ((EESoundObject*)p)->GetLooping(), (char*)a[0]));
    case O::FxSoundStop: gSoundManager->Stop((SESMInstanceNode*)p, false);
        break;
    case O::FxSoundObjectUnload: ((EESoundObject*)p)->UnloadMedia();
        break;
    case O::FxSoundObjectLoad: ((EESoundObject*)p)->LoadMedia();
        break;
    // the units, their classes, players and goals
    case O::FxCxGetClass: return P64(FXCX->GetClass());
    case O::FxGatePart: return P64(EEUnitClassOfKind<EE_CLASS_KIND_GATE>((EEUnitClass*)p)->GetPart((EEComplexUnit*)a[0], (long)a[1]));
    case O::FxDynComplex: return P64(dynamic_cast<EEComplexUnit*>(FXUN));
    case O::FxIsFoundation: return FXCX->IsFoundation();
    case O::FxCxIsSidePart: return FXCX->GetClass()->IsSidePart(FXCX);
    case O::FxCxTileX: return FXCX->GetTileX();
    case O::FxCxTileY: return FXCX->GetTileY();
    case O::FxUnitTile: return P64(FXUN->mTile);
    case O::FxDoppelgangerGet: return P64(gDoppelgangerManager.GetDoppelganger(static_cast<EETile*>(((EEUnit*)a[0])->mTile), (EEUnit*)a[0], (unsigned long)a[1]));
    case O::FxDoppelgangerIsBuilt: return ((EEDoppelganger*)p)->IsBuilt();
    case O::FxHasDoppelganger: return gDoppelgangerManager.HasDoppelganger((EEMapTile*)p, (EEUnit*)a[0], gWorld.mCurrentPlayer);
    case O::FxFreeCamera: return gFreeCamera;
    case O::FxUnitOpacity: Fn23CEE2(FXCX, false, *(float*)a[0]);
        break;
    case O::FxGeffObject: return P64(((EEGraphicEffect*)p)->mGraphicObject);
    case O::FxGeffLinked2: return P64(((EEGraphicEffect*)p)->GetLinkedEffect2());
    case O::FxGeffLinked1TagKind: return ((EEGraphicEffect*)p)->GetLinkedEffect1()->mTagPointKind;
    case O::FxQueueGoalCount: return (int64_t)FXCX->mGoalQueues[a[0]].mGoals.size();
    case O::FxQueueGoalAt: return P64(FXCX->mGoalQueues[a[0]].mGoals[a[1]]);
    case O::FxQueueGoalGetAt: return P64(FXCX->mGoalQueues[a[0]].GetGoalAt((unsigned long)a[1]));
    case O::FxLocationGoalCast: return P64(EELocationGoalCast((EEUnitGoal*)p));
    case O::FxLoadGarrisonCast: return P64(EELoadGarrisonGoalCast((EEUnitGoal*)p));
    case O::FxAttackUnitCast: return P64(EEAttackUnitCast((EEUnitGoal*)p));
    case O::FxGoalForcedMove: return ((EEUGRust*)p)->GetIsForcedMove();
    case O::FxGoalAutoWaypoint: return EELocationGoalCast((EEUnitGoal*)p)->GetIsAutoWaypoint();
    case O::FxGoalLocation: ((EEUnitGoal*)p)->GetGoalLocation(FXPT(0));
        break;
    case O::FxGoalLoadContainer: return P64(((EEUGRust*)p)->GetLoadContainer());
    case O::FxGoalAttackTarget: return P64(((EEUGRust*)p)->GetAttackTarget());
    case O::FxCxActiveGoal: return P64(FXCX->mActiveGoal);
    case O::FxCxFnE123D: FXCX->FnE123D();
        break;
    case O::FxCxGetAnimation: return FXCX->GetAnimation();
    case O::FxCxIsCloaked: return FXCX->GetIsCloaked();
    case O::FxCxGetPlayer: return P64(FXCX->GetPlayer());
    case O::FxUnGetHitPoints: return FXUN->GetHitPoints();
    case O::FxUnMaxHitPoints: return FXUN->GetStats()->GetMaxHitPoints();
    case O::FxTileVisibleTo: return ((EEMapTile*)p)->IsVisibleTo((EEPlayer*)a[0]);
    case O::FxTileSeenBy: return ((EEMapTile*)p)->IsSeenBy((EEPlayer*)a[0]);
    case O::FxPlIsAlly: return ((EEPlayer*)p)->IsAlly((EEPlayer*)a[0]);
    case O::FxPlSelectedGroup: return P64(((EEPlayer*)p)->mSelectedGroup);
    case O::FxPlCurrentEpoch: return ((EEPlayer*)p)->mTechnologies->mCurrentEpoch;
    case O::FxGroupHasType: return ((EESelectedGroup*)p)->HasUnitType((unsigned long)a[0]);
    case O::FxGroupUnit: return P64(((EESelectedGroup*)p)->GetUnit((unsigned long)a[0]));
    case O::FxGroupNumUnits: return ((EESelectedGroup*)p)->GetNumUnits();
    case O::FxVisibleBottomLeft: gWorld.GetMap()->mTerrainMesh.GetTerrainVisibleBottomLeftIndices(*(unsigned long*)a[0], *(unsigned long*)a[1]);
        break;
    case O::FxVisibleDimensions: gWorld.GetMap()->mTerrainMesh.GetTerrainVisibleTileDimensions(*(unsigned long*)a[0], *(unsigned long*)a[1]);
        break;
    // the models, their tag points, the weapon point
    case O::FxTagPoints: return P64(((EEModel*)p)->GetTagPoints((unsigned long)a[0], (long)a[1]));
    case O::FxModelTagList: return P64(((EEModel*)p)->mTagPoints[a[0]]);
    case O::FxTagCount: return ((EETagPointList*)p)->GetNumTagPoints();
    case O::FxWeaponPoint: EECombatEffectWeaponPoint((long)a[0], FXCX, (EEModel*)a[1], (EETagPointList*)a[2], a[3] != 0, FXPT(4));
        break;
    // the volcano's image and model
    case O::FxCreateSurface: return gRasterizer->GetScreen(gScreenIndex)->CreateSurface(*(GESurfaceDescription*)a[0], (GESurface**)a[1]);
    case O::FxFindTexture: return gTextureManager->GetTexturePath()->FindFile(UString((const char*)a[0]), *(FSFileSpec**)a[1]);
    case O::FxSurfaceLoadPng: ((GESurface*)p)->Load(*(FSFileSpec*)a[0], kImageFilePNG);
        break;
    case O::FxFileSpecDelete: delete (FSFileSpec*)p;
        break;
    case O::FxSurfaceLock: ((GESurface*)p)->Lock();
        break;
    case O::FxSurfaceMemory: return P64(((GESurface*)p)->GetMemoryAddress());
    case O::FxSurfaceBytesPerRow: return ((GESurface*)p)->GetBytesPerRow();
    case O::FxSurfaceUnlock: ((GESurface*)p)->Unlock();
        break;
    case O::FxSurfaceDereference: ((GESurface*)p)->Dereference();
        break;
    case O::FxCreateVolcanoModel: return P64(gModelManager->CreateModel((unsigned long)a[0], (unsigned long)a[1], 1, 0, 1, 0, "volcano model", 1));
    case O::FxModelReference: ((GEModel*)p)->Reference();
        break;
    case O::FxMaterialPolygonStart: ((GEModel*)p)->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
        break;
    case O::FxMaterialPolygonLength: ((GEModel*)p)->GetMaterial(0)->SetPolygonRangeLength((unsigned long)a[0], 0);
        break;
    case O::FxMaterialVertexStart: ((GEModel*)p)->GetMaterial(0)->SetVertexStartingIndex(0);
        break;
    case O::FxMaterialVertexLength: ((GEModel*)p)->GetMaterial(0)->SetVertexRangeLength((unsigned long)a[0]);
        break;
    case O::FxStoreMesh: EEStoreMesh((GEModel*)p, (const EeGfxMeshVertex*)a[0], (size_t)a[1], (const EeGfxMeshTriangle*)a[2], (size_t)a[3]);
        break;
    case O::FxModelComputeNormals: ((GEModel*)p)->ComputeModelNormals();
        break;
    case O::FxModelComputeBounds: ((GEModel*)p)->ComputeBoundingVolumes();
        break;
    case O::FxModelSetPostTransform: ((GEModel*)p)->SetGraphicsPostModelTransform(*(GETransformation*)a[0]);
        break;
    case O::FxModelPrepare: ((GEModel*)p)->PrepareTransforms();
        break;
    case O::FxModelCenter: FXPT(0) = ((GEModel*)p)->GetCenterPoint();
        break;
    case O::FxModelSetTexture: ((GEModel*)p)->GetMaterial(0)->SetTexture(((EEGraphicObject*)a[0])->mTextures[0], gScreenIndex);
        break;
    // the manager's world, the memory files
    case O::FxSetEffectDetail: gEffectDetail = (unsigned long)a[0];
        break;
    case O::FxEffectDetail: return gEffectDetail;
    case O::FxMemoryLength: ((FSMemoryFile*)p)->GetLength(*(unsigned long*)a[0]);
        break;
    case O::FxMemoryData: return P64(((FSMemoryFile*)p)->GetData());
    // the quad batches' arrays
    case O::FxBatchSetVertex: sEffectVertices[a[0]]->SetVertex(*reinterpret_cast<const GEColor*>(&FXQUAD->color), *reinterpret_cast<const GE3DPoint*>(&FXQUAD->points[a[2]]), *reinterpret_cast<const GEVector*>(&FXQUAD->normal), *reinterpret_cast<const GETextureCoordinate*>(&FXQUAD->coords[a[2]]), FXQUAD->opacities[a[2]], (unsigned long)a[3]);
        break;
    case O::FxBatchPolygonsNew: return P64(sEffectPolygons = gRasterizer->CreatePolygonArray((unsigned long)a[0]));
    case O::FxPolygonSetIndices: ((GEPolygonArray*)p)->SetVertexIndices((unsigned long)a[0], (unsigned long)a[1], (unsigned long)a[2], (unsigned long)a[3]);
        break;
    case O::FxBatchVerticesNew: sEffectVertices[a[0]] = gRasterizer->CreateVertexArray(1, 0x800);
        break;
    case O::FxBatchPolygonsDelete: delete std::exchange(sEffectPolygons, nullptr);
        break;
    case O::FxBatchVerticesDelete: delete std::exchange(sEffectVertices[a[0]], nullptr);
        break;
    // the decals
    case O::FxAmbientEffects: return gAmbientEffectsEnabled;
    case O::FxDrawBlood: return gDrawBlood;
    case O::FxShellHoleEffect: return P64(FXCX->mUnitClass->mShellHoleEffect);
    case O::FxBloodStainEffect: return P64(FXCX->mUnitClass->mBloodStainEffect);
    // round 11, lane A: the effects' world tables (gamecpp fx_world.rs)
    // the layout ee_gfx effect_reads.rs and gobj_media.rs read
    case O::FwLayGobjFields: return FW_BASE(EEGraphicObject, EeDataGraphicObjectFields);
    case O::FwLayGobjModels: return FW_GOBJ(mModels);
    case O::FwLayGobjTextures: return FW_GOBJ(mTextures);
    case O::FwLayGobjTagPoints: return FW_GOBJ(mTagPoints);
    case O::FwLayGobjTags: return FW_GOBJ(mTags);
    case O::FwLayGobjModel: return FW_GOBJ(mModel);
    case O::FwLayGobjDefaultPoint: return FW_GOBJ(mDefaultPoint);
    case O::FwLayGobjTopPoint: return FW_GOBJ(mTopPoint);
    case O::FwLayGobjTurretParts: return FW_GOBJ(mTurretParts);
    case O::FwLayGobjOrbitals: return FW_GOBJ(mOrbitals);
    case O::FwLayTurretPartModel: return offsetof(EETurretPart, mModel);
    case O::FwLayOrbitalModel: return offsetof(EEOrbital, mModel);
    case O::FwLayOrbitalSpeed: return offsetof(EEOrbital, mSpeed);
    case O::FwLayOrbitalAngle: return offsetof(EEOrbital, mAngle);
    case O::FwLayGeffectFields: return FW_BASE(EEGraphicEffect, EeDataGraphicEffectFields);
    case O::FwLayGeffectObject: return offsetof(EEGraphicEffect, mGraphicObject);
    case O::FwLayGeffectLinked: return offsetof(EEGraphicEffect, mLinkedEffect1);
    case O::FwLayComplexTurretAngles: return offsetof(EEComplexUnit, mTurretAngles);
    case O::FwLayStatsModel: return offsetof(EEUnitClassUpgrade, mModel);
    case O::FwLayGobjFileName: return FW_GOBJ(mFileName);
    case O::FwLayGobjTextureName: return FW_GOBJ(mTextureName);
    case O::FwLayGobjShadowModelName: return FW_GOBJ(mShadowModelName);
    case O::FwLayGobjShadowTextureName: return FW_GOBJ(mShadowTextureName);
    case O::FwLayGobjModelNames: return FW_GOBJ(mModelNames);
    case O::FwLayGobjTextureNames: return FW_GOBJ(mTextureNames);
    case O::FwLayGobjTexture: return FW_GOBJ(mTexture);
    case O::FwLayGobjShadowModel: return FW_GOBJ(mShadowModel);
    case O::FwLayGobjShadowTexture: return FW_GOBJ(mShadowTexture);
    case O::FwLayGobjSounds1: return FW_GOBJ(mSounds1);
    case O::FwLayGobjSounds2: return FW_GOBJ(mSounds2);
    case O::FwLayGobjBar: return FW_GOBJ(mBar);
    case O::FwLayGobjMediaUse: return FW_BASE(EEGraphicObject, EeDataMediaUse);
    case O::FwLayGeffectNumLoads: return offsetof(EEGraphicEffect, mNumLoads);
    // the effects' world: the units, graphic effects, models and media
    case O::FwUnitRef: ((EEUnit*)p)->Reference();
        break;
    case O::FwUnitDeref: ((EEUnit*)p)->Dereference();
        break;
    case O::FwGeffRef: ((EEGraphicEffect*)p)->Reference();
        break;
    case O::FwGeffDeref: ((EEGraphicEffect*)p)->Dereference();
        break;
    case O::FwGeffLoad: ((EEGraphicEffect*)p)->Load(a[0] != 0);
        break;
    case O::FwGeffUnload: ((EEGraphicEffect*)p)->Unload();
        break;
    case O::FwGeffMedia: return P64(static_cast<EEGraphicObject*>(((EEGraphicEffect*)p)->GetMedia()));
    case O::FwGeffGet: return P64(EEGraphicEffect::Get((uint32_t)a[0]));
    case O::FwGeffCount: return (uint32_t)EEGraphicEffect::GetCount();
    case O::FwModelDeref: ((GEModel*)p)->Dereference();
        break;
    case O::FwMediaLoad: return (int32_t)((EEGraphicObject*)p)->LoadMedia();
    case O::FwMediaUnload: ((EEGraphicObject*)p)->UnloadMedia();
        break;
    case O::FwUnitDamaged: ((EEUnit*)p)->mUnitClass->UnitDamaged(((EEUnit*)p)->mComplexUnit, NULL, 0);
        break;
    case O::FwLoadingSaved: return (bool)gLoadingSavedGame;
    case O::FwMapEffectRegions: return P64(((EEMap*)p)->mEffectRegions);
    case O::FwViewport: return P64(&gViewport);
    case O::FwGameTime: return (int64_t)gPhysics->mGameTime;
    case O::FwGameOver: return gPhysics->mGameOver;
    case O::FwWind: *(EeGe3DPoint*)a[0] = EeGe3DPoint{gWorld.mWindDirection.mX, gWorld.mWindDirection.mY, gWorld.mWindDirection.mZ};
        break;
    case O::FwScreen: return (uint32_t)(a[0] != 0 ? gGamePreferences.mSettings.GetScreenID() : gScreenIndex);
    // the answers of the 3D and 2D kinds
    case O::FwDoppelgangerAny: return gDoppelgangerManager.HasDoppelganger(static_cast<EETile*>(((EEUnit*)p)->mTile), (EEUnit*)p) == true;
    case O::FwCxSetScaledModel: ((EEComplexUnit*)p)->SetScaledModel((GEModel*)a[0]);
        break;
    case O::FwTileBlocked: return gWorld.IsTileBlocked((int32_t)a[0], (int32_t)a[1]);
    case O::FwAreaEffect: return gAEManager.HasEffect((int32_t)a[0], f[0], f[1], (uint32_t)a[1], false, &((EEComplexUnit*)p)->mAreaEffects);
    case O::FwGlowRows: return (uint32_t)gWorld.GetMap()->mTerrainMesh.GetNumVisibleTileRows();
    // the manager's world, the memory files, the effects' file
    case O::FwManagerRust: return P64(gEffectManager != NULL ? gEffectManager->mRust : (EeGfxEffectManager*)NULL);
    case O::FwCurrentPlayerIndex: return (uint32_t)::gWorld.mCurrentPlayer;
    case O::FwNumPlayers: return (uint32_t)::gWorld.mNumPlayers;
    case O::FwMemoryNew: return P64(static_cast<FSFile*>(new FSMemoryFile));
    case O::FwMemoryRewind: static_cast<FSMemoryFile*>((FSFile*)p)->SetPosition(0, kFileOriginBegin, false);
        break;
    case O::FwMemoryDelete: delete static_cast<FSMemoryFile*>((FSFile*)p);
        break;
    case O::FwSaveMajor: return kSaveGameMajorVersion;
    case O::FwSaveMinor: return kSaveGameMinorVersion;
    case O::FwReadU32: return ((FSFile*)p)->Read(*(uint32_t*)a[0]);
    case O::FwReadI32: return ((FSFile*)p)->Read(*(int32_t*)a[0]);
    case O::FwWriteU32: return ((FSFile*)p)->Write(*(const uint32_t*)a[0]);
    case O::FwWriteI32: return ((FSFile*)p)->Write(*(const int32_t*)a[0]);
    case O::RrBatchDraw: return (int32_t)gRasterizer->Draw(*sEffectPolygons, (uint32_t)a[1] / 2, *sEffectVertices[a[0]], (uint32_t)a[1], (GETexture*)p, gScreenIndex, gOrthographicView);
    default: return EECore9OpB2(inOp, p, a, f);
    }
    return 0;
}

double EECore9OpB5F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EECore9OpBF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::FxGeffSize: return ((EEGraphicEffect*)p)->mSize;
    case O::FxGeffParam2: return ((EEGraphicEffect*)p)->GetParam2();
    // round 11, lane A (gamecpp fx_world.rs)
    case O::FwTickSeconds: return gPhysics->GetTickSeconds();
    case O::FwTickTime: return gPhysics->GetTickTime();
    case O::FwPhysicsSpeed: return gPhysics->mSpeed;
    case O::FwWorldSpeed: return gWorld.GetGameSpeed();
    case O::FwGameClock: return gPhysics->GetGameClock();
    case O::FwMeshGetZ: return ((EEMap*)p)->mTerrainMesh.GetZ((int32_t)a[0], (int32_t)a[1]);
    case O::MpMeshZ: return M->mTerrainMesh.GetMeshZ(f[0], f[1]);
    case O::MpTerrainWaterDepth: return ((EETerrainTable*)p)->mWaterDepth;
    case O::MpViewHalfWidth: return gViewport.GetHalfWidth();
    case O::MpViewXCenter: return gViewport.GetXCenter();
    case O::MpViewYCenter: return gViewport.GetYCenter();
    case O::MpViewFrontClip: return gViewport.GetFrontClippingDistance();
    case O::MpViewRearClip: return gViewport.GetRearClippingDistance();
    case O::TlHeight: return ((EETile*)p)->GetHeight();
    case O::TlMapTileSize: return gMapTileSize;
    case O::TlMeshZ: return ((EEMap*)p)->mTerrainMesh.GetZ((long)a[0], (long)a[1]);
    default: return EECore9OpB5F(inOp, p, a, f);
    }
}
