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

// where Base sits in a Derived object (a cast of a made-up address: nothing is read)
template <class Derived, class Base> static uint64_t EEBaseOffset()
{
    Derived* theObject = reinterpret_cast<Derived*>(static_cast<uintptr_t>(0x10000));
    return reinterpret_cast<uintptr_t>(static_cast<Base*>(theObject)) - 0x10000;
}

// The members ee_gfx effect_reads.rs reads in place (the graphic objects, the graphic effects' records and links, the
// complex units' physical models and turret angles) and gobj_media.rs loads and releases (the graphic objects' media)
void EEGfxObjectLayoutRegister()
{
    static_assert(sizeof(std::vector<EETurretPart*>) == 3 * sizeof(void*), "a vector is three words");
    EeGfxObjectLayout theLayout = {};
    theLayout.size = sizeof(EeGfxObjectLayout);
    theLayout.gobj_fields = EEBaseOffset<EEGraphicObject, EeDataGraphicObjectFields>();
    theLayout.gobj_models = offsetof(EEGraphicObject, mModels);
    theLayout.gobj_textures = offsetof(EEGraphicObject, mTextures);
    theLayout.gobj_tag_points = offsetof(EEGraphicObject, mTagPoints);
    theLayout.gobj_tags = offsetof(EEGraphicObject, mTags);
    theLayout.gobj_model = offsetof(EEGraphicObject, mModel);
    theLayout.gobj_default_point = offsetof(EEGraphicObject, mDefaultPoint);
    theLayout.gobj_top_point = offsetof(EEGraphicObject, mTopPoint);
    theLayout.gobj_turret_parts = offsetof(EEGraphicObject, mTurretParts);
    theLayout.gobj_orbitals = offsetof(EEGraphicObject, mOrbitals);
    theLayout.turret_part_model = offsetof(EETurretPart, mModel);
    theLayout.orbital_model = offsetof(EEOrbital, mModel);
    theLayout.orbital_speed = offsetof(EEOrbital, mSpeed);
    theLayout.orbital_angle = offsetof(EEOrbital, mAngle);
    theLayout.geffect_fields = EEBaseOffset<EEGraphicEffect, EeDataGraphicEffectFields>();
    theLayout.geffect_object = offsetof(EEGraphicEffect, mGraphicObject);
    theLayout.geffect_linked = offsetof(EEGraphicEffect, mLinkedEffect1);
    static_assert(offsetof(EEGraphicEffect, mLinkedEffect3) == offsetof(EEGraphicEffect, mLinkedEffect1) + 2 * sizeof(void*),
                  "the links in order");
    theLayout.complex_scale = offsetof(EEComplexUnit, mPhysicalModel) + offsetof(GEPhysicalModel, mScale);
    theLayout.complex_frame = offsetof(EEComplexUnit, mPhysicalModel) + offsetof(GEPhysicalModel, mFrame);
    theLayout.complex_turret_angles = offsetof(EEComplexUnit, mTurretAngles);
    theLayout.stats_model = offsetof(EEUnitClassUpgrade, mModel);
    theLayout.gobj_file_name = offsetof(EEGraphicObject, mFileName);
    theLayout.gobj_texture_name = offsetof(EEGraphicObject, mTextureName);
    theLayout.gobj_shadow_model_name = offsetof(EEGraphicObject, mShadowModelName);
    theLayout.gobj_shadow_texture_name = offsetof(EEGraphicObject, mShadowTextureName);
    theLayout.gobj_model_names = offsetof(EEGraphicObject, mModelNames);
    theLayout.gobj_texture_names = offsetof(EEGraphicObject, mTextureNames);
    theLayout.gobj_texture = offsetof(EEGraphicObject, mTexture);
    theLayout.gobj_shadow_model = offsetof(EEGraphicObject, mShadowModel);
    theLayout.gobj_shadow_texture = offsetof(EEGraphicObject, mShadowTexture);
    theLayout.gobj_sounds1 = offsetof(EEGraphicObject, mSounds1);
    theLayout.gobj_sounds2 = offsetof(EEGraphicObject, mSounds2);
    theLayout.gobj_bar = offsetof(EEGraphicObject, mBar);
    theLayout.gobj_media_use = EEBaseOffset<EEGraphicObject, EeDataMediaUse>();
    theLayout.geffect_num_loads = offsetof(EEGraphicEffect, mNumLoads);
    ee_gfx_object_layout_set(&theLayout);
}

namespace {
EeGfxEffectHead* H(EEEffect* inEffect) { return reinterpret_cast<EeGfxEffectHead*>(inEffect); }
EEEffect* Fx(EeGfxEffectHead* inEffect) { return reinterpret_cast<EEEffect*>(inEffect); }
uint64_t U(const void* inObject) { return (uint64_t)(uintptr_t)inObject; }
template <class T> T* P(uint64_t inObject) { return reinterpret_cast<T*>((uintptr_t)inObject); }
EeGe3DPoint R(const GE3DPoint& inPoint) { return EeGe3DPoint{inPoint.mX, inPoint.mY, inPoint.mZ}; }

// the map a weather storm reads and calls into (ee_gfx effect_storm.rs; the storms' and EEEMStorm's moves)
struct EEStormHostOps {
    static bool OnMap(void*, int32_t inX, int32_t inY) { return gWorld.GetMap()->GetTile(inX, inY) != NULL; }
    static float GroundZ(void*, int32_t inX, int32_t inY) { return gWorld.GetMap()->mTerrainMesh.GetZ(inX, inY); }
};

// The C++ answers of one call (the call's EeGfxEffectAsk: its effect, its operation, a constructor's argument; the
// reads of the graphic objects, tag points and physical models and the effects' own answers are ee_gfx's)
struct Ask {
    static EEEffect* E(void* inAsk) { return Fx(static_cast<EeGfxEffectAsk*>(inAsk)->effect); }
    // a unit's graphic (a virtual: a foundation shows its player's class graphic)
    static uint64_t UnitGraphic(void*, uint64_t inUnit) { return U(P<EEUnit>(inUnit)->GetModel()); }
    static void FarmSetVariant(void* inAsk, int32_t inVariant) { E(inAsk)->mUnit->mComplexUnit->mCloaked = inVariant; }
    static bool FarmDoppelgangerAny(void* inAsk)
    {
        EEUnit* theUnit = E(inAsk)->mUnit;
        return gDoppelgangerManager.HasDoppelganger(static_cast<EETile*>(theUnit->mTile), theUnit) == true;
    }
    static void SetScaledModel(void* inAsk, void* inModel)
    {
        E(inAsk)->mUnit->mComplexUnit->SetScaledModel(static_cast<GEModel*>(inModel));
    }
    static bool DrawBlood(void*) { return gDrawBlood; }
    static bool TileBlocked(void*, int32_t inX, int32_t inY) { return gWorld.IsTileBlocked(inX, inY); }
    static bool AreaEffect(void* inAsk, int32_t inKind, float inX, float inY, uint32_t inPlayer)
    {
        return gAEManager.HasEffect(inKind, inX, inY, inPlayer, false, &E(inAsk)->mUnit->mComplexUnit->mAreaEffects);
    }
    static int32_t AreaEffects(void* inAsk) { return (int32_t)E(inAsk)->mUnit->mComplexUnit->mNumAreaEffects; }
    static uint32_t GlowRows(void*) { return (uint32_t)gWorld.GetMap()->mTerrainMesh.GetNumVisibleTileRows(); }
};

// The goal markers' questions (goal_markers.rs EeGfxGoalHost; the selection is the last player asked; the markers'
// effects are ee_gfx's)
struct EEGoalOps {
    static uint32_t CurrentPlayer(void*) { return (uint32_t)::gWorld.mCurrentPlayer; }
    static uint32_t PlayerCount(void*) { return (uint32_t)::gWorld.mNumPlayers; }
};

// The effects' file over an FSFile (each the FSFile call of that type) and the TEST ONLY captures' memory files
struct EEFileOps {
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
    static EeGfxEffectFile Memory() { return EEEffectFile(*new FSMemoryFile); }
    static void MemoryRewind(void* inContext) { static_cast<FSMemoryFile*>(inContext)->SetPosition(0, kFileOriginBegin, false); }
    static void MemoryFree(void* inContext) { delete static_cast<FSMemoryFile*>(inContext); }
};

// The world the effects and their manager call into (ee_gfx effect_obj.rs EeGfxEffectWorld, ffi_manager.rs
// EeGfxManagerWorld), registered before main
template <class T> void Deref(uint64_t inObject) { P<T>(inObject)->Dereference(); }
template <class T> void Ref(uint64_t inObject) { P<T>(inObject)->Reference(); }

EeGfxEffectWorld EEMakeWorld()
{
    EeGfxEffectWorld w = {};
    w.unit_reference = Ref<EEUnit>;
    w.unit_release = Deref<EEUnit>;
    w.unit_by_id = [](uint32_t inID) { return U(gWorld.GetUnit(inID)); };
    w.graphic_effect_reference = Ref<EEGraphicEffect>;
    w.graphic_effect_release = Deref<EEGraphicEffect>;
    w.graphic_effect_load = [](uint64_t inEffect, bool inOnDemand) { P<EEGraphicEffect>(inEffect)->Load(inOnDemand); };
    w.graphic_effect_unload = [](uint64_t inEffect) { P<EEGraphicEffect>(inEffect)->Unload(); };
    w.graphic_effect_media = [](uint64_t inEffect) {
        return U(static_cast<EEGraphicObject*>(P<EEGraphicEffect>(inEffect)->GetMedia()));
    };
    w.graphic_effect_by_id = [](uint32_t inID) { return U(EEGraphicEffect::Get(inID)); };
    w.graphic_effect_count = []() { return (uint32_t)EEGraphicEffect::GetCount(); };
    w.model_reference = Ref<GEModel>;
    w.model_release = Deref<GEModel>;
    w.media_load = [](uint64_t inObject) { return (int32_t)P<EEGraphicObject>(inObject)->LoadMedia(); };
    w.media_unload = [](uint64_t inObject) { P<EEGraphicObject>(inObject)->UnloadMedia(); };
    w.sound_release = ee_gamecpp_fx_sound_release;
    w.sound_object_unload = ee_gamecpp_fx_sound_object_unload;
    w.sound_object_load = ee_gamecpp_fx_sound_object_load;
    w.unit_damaged = [](uint64_t inUnit) { P<EEUnit>(inUnit)->mUnitClass->UnitDamaged(P<EEUnit>(inUnit)->mComplexUnit, NULL, 0); };
    w.loading_saved_game = []() { return (bool)gLoadingSavedGame; };
    w.regions = []() { return gWorld.mMap != NULL ? gWorld.mMap->mEffectRegions : (EeGfxEffectRegions*)NULL; };
    w.tile_seen = ee_gamecpp_fx_tile_seen;
    w.doppelganger = ee_gamecpp_fx_doppelganger;
    w.in_view = [](const EeGe3DPoint* inPoint, float inRadius) {
        return ee_gfx_effect_in_view(reinterpret_cast<const EeGeViewport*>(&gViewport), gOrthographicView, inPoint, inRadius);
    };
    w.tick = []() {
        return EeGfxEffectTick{gPhysics->GetTickSeconds(), gPhysics->GetTickTime(), (uint32_t)(unsigned long)gPhysics->mGameTime,
                               gFreeCamera, gPhysics->mSpeed, gWorld.GetGameSpeed(), (int64_t)gPhysics->mGameTime,
                               gPhysics->GetGameClock(), gPhysics->mGameOver,
                               EeGe3DPoint{gWorld.mWindDirection.mX, gWorld.mWindDirection.mY, gWorld.mWindDirection.mZ}};
    };
    w.screen = [](bool inPreferences) {
        return (uint32_t)(inPreferences ? gGamePreferences.mSettings.GetScreenID() : gScreenIndex);
    };
    // a shooter's weapon point (ee_combat's weapon_point with its CBEW capture): with the tag points at its
    // animation when inTestList finds them, else assumed
    w.weapon_point = ee_gamecpp_fx_weapon_point;
    w.height = [](const EeGe3DPoint* inPoint) { return gWorld.mMap->GetHeight(*reinterpret_cast<const GE3DPoint*>(inPoint)); };
    w.mesh = []() { return (const EeGeTerrainMesh*)gWorld.mMap->mTerrainMesh.GetRust(); };
    w.raster = &gEERustRaster;
    w.unit_graphic = Ask::UnitGraphic;
    w.storm = EeGfxStormHost{NULL, EEStormHostOps::OnMap, ee_gamecpp_fx_storm_is_water, EEStormHostOps::GroundZ,
                             ee_gamecpp_fx_storm_step_toward, ee_gamecpp_fx_storm_ripple};
    w.answers3d = EeGfxEffectAnswers3D{ee_gamecpp_fx_animation_sound, ee_gamecpp_fx_wall_gate_posts, ee_gamecpp_fx_wall_piece,
                                       ee_gamecpp_fx_volcano_build, ee_gamecpp_fx_farm_terrain, ee_gamecpp_fx_farm_drop_site,
                                       Ask::FarmSetVariant, Ask::FarmDoppelgangerAny, ee_gamecpp_fx_farm_doppelganger,
                                       ee_gamecpp_fx_farm_opacity, Ask::SetScaledModel, ee_gamecpp_fx_ground_point,
                                       ee_gamecpp_fx_rain_sounds, ee_gamecpp_fx_emitter_sound};
    w.answers2d = EeGfxEffectAnswers2D{ee_gamecpp_fx_confirm_goal, ee_gamecpp_fx_part_sound, Ask::DrawBlood, Ask::TileBlocked,
                                       Ask::AreaEffect, Ask::AreaEffects, ee_gamecpp_fx_set_area_effects,
                                       ee_gamecpp_fx_icons_selected, ee_gamecpp_fx_ground_at, ee_gamecpp_fx_glow_area,
                                       Ask::GlowRows};
    return w;
}

EeGfxManagerWorld EEMakeManagerWorld()
{
    EeGfxManagerWorld w = {};
    w.damaged_tick = ee_gamecpp_fx_damaged_tick;
    w.effect_detail = ee_gamecpp_fx_effect_detail;
    w.current = []() { return gEffectManager != NULL ? gEffectManager->mRust : (EeGfxEffectManager*)NULL; };
    w.goals = EeGfxGoalHost{NULL, EEGoalOps::CurrentPlayer, EEGoalOps::PlayerCount, ee_gamecpp_fx_goal_player,
                            ee_gamecpp_fx_goal_facts, ee_gamecpp_fx_goal_count, ee_gamecpp_fx_goal_step,
                            ee_gamecpp_fx_goal_location, ee_gamecpp_fx_goal_unit_location, ee_gamecpp_fx_goal_target,
                            ee_gamecpp_fx_goal_ground, NULL, NULL, NULL, NULL};
    w.capture = ee_gamecpp_fx_capture;
    w.memory_file = EEFileOps::Memory;
    w.memory_file_data = ee_gamecpp_fx_memory_file_data;
    w.memory_file_rewind = EEFileOps::MemoryRewind;
    w.memory_file_free = EEFileOps::MemoryFree;
    w.save_major = kSaveGameMajorVersion;
    w.save_minor = kSaveGameMinorVersion;
    return w;
}

const EeGfxEffectWorld kWorld = EEMakeWorld();
const EeGfxManagerWorld kManagerWorld = EEMakeManagerWorld();
const bool kRegistered = (EEGfxObjectLayoutRegister(), ee_gfx_effect_world_set(&kWorld), ee_gfx_manager_world_set(&kManagerWorld), true);
}

EeGfxEffectFile EEEffectFile(FSFile& inFile)
{
    EeGfxEffectFile theFile = {&inFile, EEFileOps::IsOpen, EEFileOps::ReadU32, EEFileOps::ReadI32, EEFileOps::ReadF32,
                               EEFileOps::ReadBool, EEFileOps::WriteU32, EEFileOps::WriteI32, EEFileOps::WriteF32,
                               EEFileOps::WriteBool};
    return theFile;
}

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

// rain or snow over a set of tiles (their coordinates as x, y pairs)
long EEEffectManager::CreateWeatherEffect(const std::vector<EETileCoordinate>& inTiles, EEGraphicEffect* inEffect,
                                          EEEffectHandle* outHandle)
{
    std::vector<int32_t> theTiles;
    for (size_t i = 0; i < inTiles.size(); i++) {
        theTiles.push_back((int32_t)inTiles[i].mX);
        theTiles.push_back((int32_t)inTiles[i].mY);
    }
    EeGfxEffectArgs a = Args(EeGfxEffectEntry::Weather, inEffect);
    a.tiles = theTiles.empty() ? NULL : &theTiles[0];
    a.tile_count = (uint32_t)inTiles.size();
    return this->Create(a, outHandle);
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

// A batch's quad: its four vertices from inIndex on (the raster table's batch_quad; AddEffectQuad)
void EEEffectBatchQuad(uint32_t inWhich, const EeGfxEffectQuad* inQuad, uint32_t inIndex)
{
    ee_gamecpp_fx_batch_quad(inWhich, inQuad, inIndex);
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
    ee_gamecpp_fx_batch_create();
}

// Deletes the quad batches' polygon array and vertex arrays (provisional name)
void DeleteEffectBatchBuffers()
{
    ee_gamecpp_fx_batch_delete();
}

// Creates and releases the two shared quad models (provisional names)
void CreatePlaneModels() { ee_gfx_effect_planes_create(&gEERustRaster); }
void ReleasePlaneModels() { ee_gfx_effect_planes_release(&gEERustRaster); }

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
double EECore9OpB2F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

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
    default: return EECore9OpB2(inOp, p, a, f);
    }
    return 0;
}

double EECore9OpBF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::FxGeffSize: return ((EEGraphicEffect*)p)->mSize;
    case O::FxGeffParam2: return ((EEGraphicEffect*)p)->GetParam2();
    default: return EECore9OpB2F(inOp, p, a, f);
    }
}
