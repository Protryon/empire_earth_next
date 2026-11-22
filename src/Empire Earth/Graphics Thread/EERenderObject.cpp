// C:\EEX\Empire Earth\Graphics Thread\EERenderObject.cpp (no __FILE__; name from the RTTI class). The render
// objects' base (RTTI EERenderObject > RTSRenderObject, vtable 0x441378) and the per-frame render lists:
// 0x10aa88..0x10e899. It opens with the init chains of three classes' statics (guard bytes 0x529048, 0x528d70,
// 0x4e9454: the lines vector and a UWideString, the unit picker and the environment, the render lists; owner
// classes inferred, defined as globals here), then the unit's own globals' chains (the effect queues, colours, the
// main viewport 0x4e8f08), the ctor, ??_G, RTSRenderObject's inline dtor, and the list builders. The chains
// 0x10aa88..0x10ac30 might instead be a unit of their own (no __FILE__ either way). Unit flags: the exe's module set.
// FillUnit (0x10d3ad, 4 KB) and FillDoppelgangerUnit (0x10e3c8, the same for a doppelganger) are declared in
// ee/EERenderObject.h and called by FillRenderLists. The tail
// 0x10e899..0x111d3d (the space-cliff textures, the frame builders, the lighting set-ups, the template COMDATs and the
// inline ctors) follows the ghost-unit pass.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "rust/base/math.h"
#include "rust/render/ge.h"
#include "rust/render/gfx.h"
#include "rust/sim/units.h"
#include "dx7/DX7Rasterizer.h"
#include "lle/GEModelManager.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEGraphics.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderLists.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffectMesh.h"        // the table's store_mesh
#include "ee/EERenderSnapshot.h"     // gRenderSnapshot (the Rust draws' camera)
#include "ee/EEUnitPicker.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEDoppelganger.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEAmbientSoundManager.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EESEUICTriggerSelectUnits.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEMusicManager.h"
#include "ee/EEPhysics.h"
#include "ee/EEMouse.h"
#include "ee/EEAreaEffectKindIDs.h"
#include "ee/EERenderCapture.h"     // RFIL (TEST ONLY)
#include "ee/EEWheel.h"
#include "ee/EEUnitClassKinds.h"
#include "ee/EEAreaEffectTemplate.h"
#include "lle/GEPhysicalModel.h"
#include "rust/game/gamecpp.h"

// the 3D lines (0x529050) and a name (0x529030): one guard byte (0x529048) (provisional names)
std::vector<EERenderLine> gU529050;
UWideString gU529030;

// the unit picker and the environment (gamecpp render_object.rs' RENDER_ENV): one guard byte (0x528d70)
EEUnitPicker gUnitPicker(0x600);

EERenderLists EERenderLists::sInstance;

// 0x4e9348 (17 colours) and its init chain: EEInitialiseScene's key frames, ee_gfx scene.rs initial_tables
GEColor gU4E9338(1.0f, 1.0f, 1.0f);
float gU4E9330;                            // 0x4e9330 (no init chain)
// the three per-time-of-day tables (0x4e92ec, 0x4e92a8, 0x4e9264; 17 entries): ee_gfx scene.rs initial_tables
// 0x4413ac: EEInitialiseScene's 0.549f is a literal (__real COMDAT; the fst chain of a literal, not a static const reload)
// Fn10C036's constants (.rdata 0x441334..0x4413c4: the view band, the clocks' rates, the moon's hours, the fill share):
// ee_gfx scene.rs; the sway clock 0x529084 (and its last time 0x529094), the music check's last time 0x529090, the
// frame counter 0x52908c, the cliff textures' names 0x4be37c, EEInitialiseScene's flag 0x529064 and bitmap 0x529060:
// gamecpp render_object.rs
// 0x441344 (degrees to radians), 0x441354 (the objects' size step): ee_gfx capture.rs
EERenderUnit* gU529044;                     // 0x529044 (the picked enemy's render unit: FillUnit; EEROUnits.cpp reads it; provisional name)

// 0x4e9240 (the three fill lights) and its init chain: ee_gfx scene.rs initial_tables
std::vector<unsigned long> gU4E9230;

GEColor gU4E9220(1.0f, 1.0f, 1.0f);

GEVector gU4E9210;                         // 0x4e9210 (the sun direction: Fn10C036, SetTerrainLighting)

GELight gU4E9198;
unsigned long gU4E9190;                     // 0x4e9190 (gU4E9198's index in the rasterizer: AddLight/UpdateLight)

GEViewport gU4E8F08;

// the space-cliff textures (0x4e8ef0; loaded once by LoadCliffTextures, handed to the terrain mesh) (provisional name)
std::vector<GETexture*> gU4E8EF0;

// the light fade (0x4be368 sLightFade, 0x4be36c its start, 0x529070.. its direction, rate and time) and the
// letterbox (0x4be370 its direction, 0x4be378 its start, 0x52907c.. its rate and time, 0x529088 the bars to clear
// again): ee_gfx scene.rs (RUST_PLAN R4.4), which steps them; the C++ keeps the storage
static EeGfxSceneFade sSceneFade = {1.0f, 1.0f, 0, 0.0f, 0.0f, 1, 1.0f, 0.0f, 0.0f, false};
float gF4BE374 = 1.0f;                     // 0x4be374 the letterbox's zoom (EEScenarioManager.cpp reads it)
// the unit's other .bss globals (declared in ee/EERenderLists.h, ee/EEGraphics.h, ee/EmpireEarth.h; their address
// neighbours and references are this unit's) (provisional names)
bool gB529065;
bool gB529066;
bool gCameraFollowing;
bool gB529068;

// Creates a game render object
EERenderObject::EERenderObject()
{
}

// Destroys a render object (RTSRenderObject's inline destructor)
RTSRenderObject::~RTSRenderObject()
{
}

// The scene, the frame's lists, the lighting and the posts: gamecpp render_object.rs (EECore9OpA's cases below)
void EEInitialiseScene() { ee_gamecpp_ro_initialise_scene(); }
void EEReleaseScene() { ee_gamecpp_ro_release_scene(); }
void EEStartLetterboxOut(unsigned long inFrames) { ee_gamecpp_ro_letterbox(true, inFrames); }
void EEStartLetterboxIn(unsigned long inFrames) { ee_gamecpp_ro_letterbox(false, inFrames); }
void EEStartLightFadeIn(unsigned long inFrames) { ee_gamecpp_ro_light_fade(false, inFrames); }
void EEStartLightFadeOut(unsigned long inFrames) { ee_gamecpp_ro_light_fade(true, inFrames); }
void EERenderObject::QueueEffects() { ee_gamecpp_ro_queue_effects(this); }
void EERenderObject::Fn10C036() { ee_gamecpp_ro_fn10c036(this); }
void EERenderObject::Fn10E8D1() { ee_gamecpp_ro_fn10e8d1(this); }
void EERenderObject::FillRenderLists() { ee_gamecpp_ro_fill_render_lists(this); }
void SetSceneFog() { ee_gamecpp_ro_lighting((uint32_t)EeGfxLighting::SceneFog); }
void SetLightingOff() { ee_gamecpp_ro_lighting((uint32_t)EeGfxLighting::Off); }
void SetTerrainLighting() { ee_gamecpp_ro_lighting((uint32_t)EeGfxLighting::Terrain); }
void EEPostGraphicsAll() { ee_gamecpp_ro_post_render_objects(true); }
void EEResetGraphicsValues() { ee_gamecpp_ro_post_render_objects(false); }

// Returns the ghost unit's model through its player's upgrade of its unit class
// (inline in the real header)
EEModel* EEGhostUnit::GetModel()
{
    return this->mPlayer->mUnitClassUpgrades[this->mUnitClass->GetUnitType()]->mModel;
}

// Returns the ghost unit's scale through its player's upgrade of its unit class
float EEGhostUnit::GetScale()
{
    return this->mPlayer->mUnitClassUpgrades[this->mUnitClass->GetUnitType()]->GetScale();
}

// EERenderShadow is ee_gfx fill.rs's EeGfxFillObject and EERenderUnit's head its EeGfxFillUnit (ee/EERenderLists.h)

// GetUnitOpacity's calls (ee_gfx fill.rs EeGfxOpacityHost)
static EEUnit* EEOpacityUnit(uint64_t inUnit)
{
    return reinterpret_cast<EEUnit*>((uintptr_t)inUnit);
}
static bool EEOpacityOwn(void*, uint64_t inUnit, bool inTranslucent, float* ioOpacity)
{
    return Fn23CEE2(reinterpret_cast<EEComplexUnit*>((uintptr_t)inUnit), inTranslucent, *ioOpacity);
}
static bool EEOpacityLandBuilding(void*, uint64_t inClass)
{
    return reinterpret_cast<EEUnitClass*>((uintptr_t)inClass)->IsLandBuilding();
}
static EeGe3DPoint EEOpacityModelCenter(void*, uint64_t inUnit, unsigned long inIndex)
{
    const GE3DPoint& theCenter = EEOpacityUnit(inUnit)->GetModel()->mModels[inIndex]->GetCenterPoint();
    return EeGe3DPoint{theCenter.mX, theCenter.mY, theCenter.mZ};
}
static float EEOpacityModelRadius(void*, uint64_t inUnit, unsigned long inIndex, unsigned long inFrame)
{
    return EEOpacityUnit(inUnit)->GetModel()->mModels[inIndex]->GetBoundingRadius(inFrame);
}
static float EEOpacityPhysicalScale(void*, uint64_t inUnit)
{
    return reinterpret_cast<EEComplexUnit*>((uintptr_t)inUnit)->mPhysicalModel.GetScale();
}
static unsigned long EEOpacityPhysicalFrame(void*, uint64_t inUnit)
{
    return reinterpret_cast<EEComplexUnit*>((uintptr_t)inUnit)->mPhysicalModel.GetFrame();
}
static float EEOpacityMeshZ(void*, float inX, float inY)
{
    return gWorld.mMap->mTerrainMesh.GetMeshZ(inX, inY);
}
static const EeGfxOpacityHost* EEOpacityHost()
{
    static const EeGfxOpacityHost sHost = {NULL, EEOpacityOwn, EEOpacityLandBuilding, EEOpacityModelCenter,
                                           EEOpacityModelRadius, EEOpacityPhysicalScale, EEOpacityPhysicalFrame, EEOpacityMeshZ};
    return &sHost;
}

// EEROUnits.cpp's screen point (0x118676; the goal's countdown)
void GetScreenPoint(const GE3DPoint& inPoint, GE2DPoint& outPoint);

// the rasterizer as the Rust draws call it (ee_gfx raster.rs), on the current screen
extern const EeGfxRaster gEERustRaster;
// the raster table's models (ee_gfx raster.rs; the farm's and the ground shadows'): made through the model manager
static void* EERasterCreateModel(uint32_t inPolygons, uint32_t inVertices, uint32_t inSubModels, const uint8_t* inName,
                                 uint32_t inFlags)
{
    return gModelManager->CreateModel(inPolygons, inVertices, 1, 0, 1, inSubModels,
                                      UString(reinterpret_cast<const char*>(inName)), inFlags);
}
static void* EERasterCreateSubModel(uint32_t inPolygons, uint32_t inVertices, void* inParent)
{
    return gModelManager->CreateSubModel(inPolygons, inVertices, 1, 0, 1, 0, 0, 0, static_cast<GEModel*>(inParent));
}
static void EERasterStoreMesh(void* inModel, const EeGfxMeshVertex* inVertices, size_t inVertexCount,
                              const EeGfxMeshTriangle* inTriangles, size_t inTriangleCount)
{
    EEStoreMesh(static_cast<GEModel*>(inModel), inVertices, inVertexCount, inTriangles, inTriangleCount);
}

// EEEffect.cpp's quad batches' quad and draw
void EEEffectBatchQuad(uint32_t inWhich, const EeGfxEffectQuad* inQuad, uint32_t inIndex);
int32_t EEEffectBatchDraw(uint32_t inWhich, uint32_t inCount, void* inTexture);
// EEROUnits.cpp's bars (their set, the placement lines and the labels: gamecpp ro_units.rs)
int32_t EEROUnitsBarDraw(uint32_t inWhich);

// Computes a unit's opacity this frame (see-through modes, its own fade, and buildings fading in front of the near
// clipping plane); returns whether it is drawn translucent (ee_gfx fill.rs over the world views; the calls here)
static bool EEGetUnitOpacity(EEUnit* inUnit, long inShowHiddenUnits, float& outOpacity)
{
    return ee_gfx_fill_unit_opacity((uint64_t)(uintptr_t)inUnit, inShowHiddenUnits, gWorld.mInCinematic,
                                    gOrthographicView, reinterpret_cast<const EeGeViewport*>(&gViewport), EEOpacityHost(),
                                    &outOpacity);
}

const EeGfxRaster gEERustRaster = {
    [](EeGfxLighting inKind) { ee_gamecpp_ro_lighting((uint32_t)inKind); },
    [](const EeGe3DPoint* inLocation, uint32_t inFog) { return ee_gamecpp_ro_fog_factor(&inLocation->x, inFog); },
    [](void* inModel, float inOpacity) { return (int32_t)gRasterizer->Draw(static_cast<GEModel*>(inModel), gScreenIndex, inOpacity, gOrthographicView); },
    []() { return *reinterpret_cast<const EeGeTransformation*>(&gRenderSnapshot.mCamera); },
    ee_gamecpp_ro_depth_range,
    []() { return gRenderSnapshot.mCameraYaw; },
    [](const EeGe3DPoint* inPoint, float* outX, float* outY) { ee_gamecpp_ro_screen_point(&inPoint->x, outX, outY); },
    // EEE3DGoal::DrawPass3's text (0xf13db)
    [](float inX, float inY, const uint8_t* inText, EeGfxColor inColor) { ee_gamecpp_ro_draw_text(inX, inY, inText, inColor.red, inColor.green, inColor.blue); },
    EERasterCreateModel,
    EERasterCreateSubModel,
    EERasterStoreMesh,
    [](const uint8_t* inName, const EeGfxTextureLoad* inLoad) { return ee_gamecpp_ro_load_texture(inName, inLoad); },
    // EEE3DMarchingLine::DrawPass4's line (0xf3c68)
    ee_gamecpp_ro_screen_line,
    ee_gamecpp_ro_rear_clipping,
    [](void* inModel, bool inSorted) { return (int32_t)gRasterizer->DrawSky(static_cast<GEModel*>(inModel), gScreenIndex, 1.0f, inSorted); },
    []() { return gRasterizer->GetScreen(gScreenIndex)->Get32BitTextureSupport(); },
    ee_gamecpp_ru_bar_set,
    EEROUnitsBarDraw,
    ee_gamecpp_ru_screen_lines,
    ee_gamecpp_ru_label,
    // the effects' quad batches (ee_gfx effect_batch.rs; EEEffect.cpp's arrays)
    []() { return *reinterpret_cast<const EeGeTransformation*>(&gU4E8F08.GetCameraInverse()); },
    []() { return gRenderSnapshot.mCameraPitch; },
    EEEffectBatchQuad,
    EEEffectBatchDraw,
    // the screen the models' textures are set for (raster_direct.rs: the model, material and texture calls are Rust's)
    []() { return (uint32_t)gScreenIndex; },
    // gRasterizer's Rust objects (raster_direct.rs: the state calls the lambdas made on it)
    [](void** outBase) { return DX7Rasterizer::RustObjects(gRasterizer, outBase); },
};

// ------------------------------------------------------------------------- round 9, lane A: this file's bodies' C++

// the unit picker's cases (EEUnitPicker.cpp), then lane B's
int64_t EEPickerOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EEPickerOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

// the mirrors and constants of gamecpp render_object.rs
static_assert(offsetof(EERenderFrameInfo, mYaw) == sizeof(void*) && offsetof(EERenderFrameInfo, mFrame) == sizeof(void*) + 8
              && offsetof(EERenderFrameInfo, mAmbient) == sizeof(void*) + 8 + sizeof(long), "render_object.rs FrameInfo");
static_assert(offsetof(GETextureDescription, mIs3DRenderable) == 4 * sizeof(long)
              && offsetof(GETextureDescription, mUseTexturePack) == 4 * sizeof(long) + 4, "render_object.rs TextureDescription");
static_assert(kUnitTypeRoboticFarm == 0x33a && kUnitTypeSpaceTurret == 0x302 && kUnitTypeOrbitalSpaceStation == 0x31f
              && kAreaEffectMorale == 2 && kAreaEffectMoraleFromBuildings == 5 && kAreaEffectMoraleFromMilitaryHeroes == 6
              && kStageOpModulate == 2 && (int)EeGfxLighting::DimUnit == 10, "render_object.rs constants");

#define RO_P64(x) ((int64_t)(intptr_t)(x))
#define RS (EERenderLists::sInstance)
#define RU ((EERenderUnit*)p)
#define RO_UN ((EEUnit*)p)
#define RO_CX ((EEUnit*)p)->GetComplexUnit()
#define RO_UC ((EEUnitClass*)p)
#define RO_DG ((EEDoppelganger*)p)
#define RO_GH ((EEGhostUnit*)p)
#define RO_GOBJ ((EEGraphicObject*)p)
#define RO_LINE ((EERenderLine*)p)
#define RO_TEX ((GETexture*)p)
#define RO_TEXT ((GEText*)p)
#define RO_GELINE ((GELine*)p)
#define RO_SHADOWS(k) ((k) == 0 ? RS.mShadows0 : (k) == 1 ? RS.mShadows1 : RS.mShadows2)
#define RO_UNITS(k) ((k) == 0 ? RS.mUnits0 : (k) == 1 ? RS.mUnits1 : RS.mUnits2)

// the C++ calls and members of this file's bodies in Rust (gamecpp render_object.rs; EeGameCpp's lane A section): one
// statement each
int64_t EECore9OpA(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    // the render globals
    case O::RoSceneFade: return RO_P64(&sSceneFade);
    case O::RoGridPhase: return RO_P64(&gU4E9330);
    case O::RoLetterboxZoom: return RO_P64(&gF4BE374);
    case O::RoAmbientColor: return RO_P64(&gU4E9338);
    case O::RoFogColor: return RO_P64(&gU4E9220);
    case O::RoSunDirection: return RO_P64(&gU4E9210);
    case O::RoFlag529065: return RO_P64(&gB529065);
    case O::RoFlag529066: return RO_P64(&gB529066);
    case O::RoFlag529068: return RO_P64(&gB529068);
    case O::RoCameraFollowing: return RO_P64(&gCameraFollowing);
    case O::RoWorldTransformation: return RO_P64(&gWorld.mTransformation);
    case O::RoWindDirection: return RO_P64(&gWorld.mWindDirection);
    case O::RoOrthographic: return gOrthographicView;
    case O::RoUnitLighting: return gUnitLighting;
    case O::RoHiddenUnitsMode: return gHiddenUnitsMode;
    case O::RoPhysicsTicks: return gPhysics->mTicksRun;
    case O::RoMusicFollowsBattle: return gMusicFollowsBattle;
    case O::RoMusicIntensity: return gMusicManager->mIntensity;
    case O::RoCollisionSphereMode: return gCollisionSphereMode;
    case O::RoLightAttenuation0: gU4E9198.SetAttenuation0(f[0]);
        break;
    case O::RoLightAttenuation1: gU4E9198.SetAttenuation1(f[0]);
        break;
    case O::RoLightAttenuation2: gU4E9198.SetAttenuation2(f[0]);
        break;
    case O::RoLightStyle: gU4E9198.SetStyle((unsigned long)a[0]);
        break;
    case O::RoNewBitmap: return RO_P64(new GEBitmap);
    case O::RoBitmapDereference: ((GEBitmap*)p)->Dereference();
        break;
    // the render lists
    case O::RoUnitCount: return RO_P64(&RS.mRenderUnitCount);
    case O::RoObjectCount: return RO_P64(&RS.mRenderShadowCount);
    case O::RoRenderUnit: return RO_P64(&RS.mRenderUnits[a[0]]);
    case O::RoRenderObject: return RO_P64(&RS.mRenderShadows[a[0]]);
    case O::RoUnitBarsResize: RU->mHealthBars.resize((size_t)a[0], 0.0f);
        break;
    case O::RoUnitBarsCount: return (int64_t)RU->mHealthBars.size();
    case O::RoUnitBarSet: RU->mHealthBars[a[0]] = f[0];
        break;
    case O::RoUnitBarsClear: RU->mHealthBars.clear();
        break;
    case O::RoUnitPartsClear: RU->mTurretAngles.clear(), RU->mTurretModels.clear(), RU->mOrbitalAngles.clear(), RU->mOrbitalModels.clear(), RU->mWheelAngles.clear(), RU->mWheelModels.clear();
        break;
    case O::RoUnitTurretsResize: RU->mTurretAngles.resize((size_t)a[0], 0.0f), RU->mTurretModels.resize((size_t)a[0], 0);
        break;
    case O::RoUnitOrbitalsResize: RU->mOrbitalAngles.resize((size_t)a[0], 0.0f), RU->mOrbitalModels.resize((size_t)a[0], 0);
        break;
    case O::RoUnitWheelsResize: RU->mWheelAngles.resize((size_t)a[0], 0.0f), RU->mWheelModels.resize((size_t)a[0], 0);
        break;
    case O::RoUnitTurretSet: RU->mTurretAngles[a[1]] = ((EEUnit*)a[0])->GetComplexUnit()->mTurretAngles[a[1]], RU->mTurretModels[a[1]] = ((EEGraphicObject*)RU->source)->mTurretParts[a[1]]->mModel;
        break;
    case O::RoUnitOrbitalSet: RU->mOrbitalAngles[a[1]] = ((EEUnit*)a[0])->GetComplexUnit()->mOrbitals[a[1]]->mAngle, RU->mOrbitalModels[a[1]] = ((EEUnit*)a[0])->GetComplexUnit()->mOrbitals[a[1]]->mModel;
        break;
    case O::RoUnitWheelSet: RU->mWheelAngles[a[1]] = ((EEUnit*)a[0])->GetComplexUnit()->mWheels[a[1]]->mAngle, RU->mWheelModels[a[1]] = ((EEUnit*)a[0])->GetComplexUnit()->mWheels[a[1]]->mModel;
        break;
    case O::RoUnitNameClear: RU->mName.Clear();
        break;
    case O::RoUnitNameFromStats: RU->mName = ((EEUnit*)a[0])->GetComplexUnit()->mStats->mName;
        break;
    case O::RoObjectsClear: RO_SHADOWS(a[0]).clear();
        break;
    case O::RoUnitsClear: RO_UNITS(a[0]).clear();
        break;
    case O::RoObjectsInsert: RO_SHADOWS(a[0]).insert(EERenderShadowMap::value_type((GETexture*)a[1], (EERenderShadow*)p));
        break;
    case O::RoUnitsInsert: RO_UNITS(a[0]).insert(EERenderUnitMap::value_type((GETexture*)a[1], RU));
        break;
    case O::RoSelectedObjectsClear: RS.mSelectedObjects.clear();
        break;
    case O::RoSelectedUnitsClear: RS.mSelectedUnits.clear();
        break;
    case O::RoSelectedObjectsPush: RS.mSelectedObjects.push_back((EERenderShadow*)p);
        break;
    case O::RoSelectedUnitsPush: RS.mSelectedUnits.push_back(RU);
        break;
    case O::RoSetSelectedUnit: RS.mSelectedUnit = RU;
        break;
    case O::RoSelectedClear: RS.mSelectedUnit = NULL, RS.mSelectedObject = NULL;
        break;
    case O::RoSetLabelUnit: gU529044 = RU;
        break;
    case O::RoObjectUnloadMedia: ((EEGraphicObject*)((EERenderShadow*)p)->object)->UnloadMedia();
        break;
    case O::RoGhostRecordsCount: return (int64_t)RS.mGhostRecords.size();
    case O::RoGhostRecordsBackSource: return RO_P64(RS.mGhostRecords.back().mUnit.source);
    case O::RoGhostRecordsPop: RS.mGhostRecords.pop_back();
        break;
    case O::RoGhostRecordsPush: RS.mGhostRecords.push_back(*(EERenderUnitRecord*)p);
        break;
    case O::RoGhostRecordNew: return RO_P64(new EERenderUnitRecord());
    case O::RoGhostRecordDelete: delete (EERenderUnitRecord*)p;
        break;
    case O::RoGhostRecordCopy: ((EERenderUnitRecord*)p)->mTileX = ((EEGhostUnit*)a[0])->mTileX, ((EERenderUnitRecord*)p)->mTileY = ((EEGhostUnit*)a[0])->mTileY, memcpy(((EERenderUnitRecord*)p)->mCells, ((EEGhostUnit*)a[0])->mFootprint, sizeof(((EERenderUnitRecord*)p)->mCells)), memcpy(((EERenderUnitRecord*)p)->mHeights, ((EEGhostUnit*)a[0])->mHeights, sizeof(((EERenderUnitRecord*)p)->mHeights));
        break;
    case O::RoPickedIds: return (*(const unsigned long**)a[0] = gU4E9230.empty() ? NULL : &gU4E9230[0], (int64_t)gU4E9230.size());
    case O::RoPickedClear: gU4E9230.clear();
        break;
    // the 3D lines
    case O::RoLinesCount: return (int64_t)gU529050.size();
    case O::RoLineDereference: gU529050[a[0]].mTexture->Dereference();
        break;
    case O::RoLinesClear: gU529050.clear();
        break;
    case O::RoLinesPushBack: gU529050.push_back(EERenderLine());
        break;
    case O::RoLinesBack: return RO_P64(&gU529050[gU529050.size() - 1]);
    case O::RoLineSetWidth: RO_LINE->mWidth = gU4E9330;
        break;
    case O::RoLineSetTexture: RO_LINE->mTexture = gAreaEffectTexture;
        break;
    case O::RoAreaEffectTextureReference: gAreaEffectTexture->Reference();
        break;
    case O::RoLinePointsResize: RO_LINE->mPoints.resize((size_t)a[0], std::pair<GE3DPoint, float>());
        break;
    case O::RoLinePointSet: RO_LINE->mPoints[a[0]] = std::pair<GE3DPoint, float>(*(const GE3DPoint*)a[1], f[0]);
        break;
    // the units, classes, players, tiles and graphic objects
    case O::RoUnitId: return RO_UN->GetUnitID();
    case O::RoUnitIsTree: return RO_UN->GetUnitClass()->mIsTree;
    case O::RoUnitTileSize: return RO_UN->GetUnitClass()->mTileSize;
    case O::RoUnitIsObstacle: return RO_UN->GetUnitClass()->mIsObstacle;
    case O::RoUnitComplexIsFoundation: return RO_UN->mComplexUnit->IsFoundation();
    case O::RoUnitNextPart: return RO_P64(RO_UN->mComplexUnit->mNextPart);
    case O::RoUnitHitPoints: return RO_UN->GetHitPoints();
    case O::RoUnitHitPointsIsOne: return RO_UN->mHitPoints == 1;
    case O::RoUnitPhysicalFrame: return RO_CX->mPhysicalModel.GetFrame();
    case O::RoUnitTint: *(GEColor*)a[0] = RO_CX->mColor;
        break;
    case O::RoUnitSidePart: return EEUnitClassOfKind<EE_CLASS_KIND_GATE>(RO_CX->mUnitClass)->IsSidePart(RO_CX);
    case O::RoTurretPartCount: return (int64_t)RO_CX->GetModel()->mTurretParts.size();
    case O::RoOrbitalCount: return RO_CX->mNumOrbitals;
    case O::RoWheelCount: return RO_CX->mNumWheels;
    case O::RoUnitAreaEffectCount: return (int64_t)RO_CX->mAffectingAreaEffects.mItems.size();
    case O::RoUnitAreaEffectKind: return RO_CX->mAffectingAreaEffects.mItems[a[0]]->mTemplate->mKind;
    case O::RoUnitAreaEffectStrength: return RO_CX->mAffectingAreaEffects.mItems[a[0]]->GetStrength();
    case O::RoComplexInView: return RO_UN->mComplexUnit->mPhysicalModel.Intersects(gViewport, gOrthographicView);
    case O::RoComplexLocation: return RO_P64(&((EEComplexUnit*)p)->mPhysicalModel.GetLocation());
    case O::RoComplexIsFoundation: return ((EEComplexUnit*)p)->IsFoundation();
    case O::RoWorldUnit: return RO_P64(static_cast<EEUnit*>(gWorld.GetUnit((unsigned long)a[0])));
    case O::RoFindWaypoint: return ((EEComplexUnit*)p)->FindWaypointAtTile((long)a[0], (long)a[1], *(const EEComplexUnitWaypoint**)a[2]);
    case O::RoWaypointLocation: *(GE3DPoint*)a[0] = ((const EEComplexUnitWaypoint*)p)->mLocation;
        break;
    case O::RoClassCategory: return RO_UC->mCategory;
    case O::RoClassTileSize: return RO_UC->mTileSize;
    case O::RoClassUnitType: return RO_UC->GetUnitType();
    case O::RoClassFamily: return RO_UC->mFamily;
    case O::RoClassFacesCamera: return RO_UC->mFacesCamera == true;
    case O::RoScaffoldingModel: return RO_UC->mScaffoldingClass == NULL ? 0 : RO_P64(gWorld.mPlayers[a[0]]->GetUpgrade(RO_UC->mScaffoldingClass->GetUnitType())->mModel);
    case O::RoGroupContains: return ((EEPlayer*)p)->mSelectedGroup->Contains(((EEUnit*)a[0])->GetComplexUnit());
    case O::RoCivilizationUnchanged: return ((EEPlayer*)p)->mCivilization.GetUnchanged();
    case O::RoLabelCivilizationName: gU529030 = ((EEPlayer*)p)->mCivilization.mName;
        break;
    case O::RoLabelPlayerName: gU529030 = ((EEPlayer*)p)->mName;
        break;
    case O::RoTileFirstNode: return RO_P64(((EETile*)p)->mUnits);
    case O::RoTileSeenBy: return ((EETile*)p)->IsSeenBy((EEPlayer*)a[0]);
    case O::RoTileDoppelgangerCount: return ((EETile*)p)->GetNumDoppelgangers();
    case O::RoTileDoppelganger: return RO_P64(((EETile*)p)->mDoppelgangers[a[0]]);
    case O::RoNodeNext: return RO_P64(((RTSTileUnitNode*)p)->mNext);
    case O::RoNodeUnit: return RO_P64(((RTSTileUnitNode*)p)->mUnit);
    case O::RoNodeUnitIsTree: return ((RTSTileUnitNode*)p)->mUnit->mUnitClass->mIsTree;
    case O::RoDoppelUnit: return RO_P64(RO_DG->mUnit);
    case O::RoDoppelModel: return RO_P64(RO_DG->GetModel());
    case O::RoDoppelAnimation: return RO_DG->GetAnimation();
    case O::RoDoppelFrame: return RO_DG->GetFrame();
    case O::RoDoppelHitPoints: return RO_DG->GetHitPoints();
    case O::RoDoppelMaxHitPoints: return RO_DG->GetPlayer()->GetUpgrade(RO_DG->mUnit->mUnitClass->GetUnitType())->mMaxHitPoints;
    case O::RoDoppelIsBuilt: return RO_DG->IsBuilt();
    case O::RoDoppelIsComplex: return RO_DG->IsComplex() == true;
    case O::RoDoppelIsFoundation: return RO_DG->IsFoundation();
    case O::RoDoppelTransformation: RO_DG->GetTransformation(*(GETransformation*)a[0]);
        break;
    case O::RoDoppelPlayerColor: *(GEColor*)a[0] = RO_DG->GetPlayer()->mColor;
        break;
    case O::RoDoppelPlayer: return RO_P64(RO_DG->GetPlayer());
    case O::RoDoppelPlayerMember: return RO_P64(RO_DG->mPlayer);
    case O::RoGhostLock: gWorld.mGhostUnitsLock->Enter();
        break;
    case O::RoGhostUnlock: gWorld.mGhostUnitsLock->Leave();
        break;
    case O::RoGhostUnitsChanged: return gGhostUnitsChanged;
    case O::RoGhostCount: return (int64_t)gWorld.GetNumGhostUnits();
    case O::RoGhostAt: return RO_P64(gWorld.mGhostUnits[a[0]]);
    case O::RoGhostModel: return RO_P64(RO_GH->GetModel());
    case O::RoGhostFrame: return RO_GH->mFrame;
    case O::RoGhostAnimation: return RO_GH->mAnimation;
    case O::RoGhostCanPlace: return RO_GH->mCanPlace;
    case O::RoGhostClass: return RO_P64(RO_GH->mUnitClass);
    case O::RoGhostPlayerColor: *(GEColor*)a[0] = RO_GH->mPlayer->mColor;
        break;
    case O::RoGhostTransformation: *(GETransformation*)a[0] = RO_GH->mTransformation;
        break;
    case O::GobjLoadMedia: RO_GOBJ->LoadMedia();
        break;
    case O::GobjUnloadMedia: RO_GOBJ->UnloadMedia();
        break;
    case O::RoGobjFrameCount: return RO_GOBJ->mModels[a[0]]->GetFrameCount();
    case O::RoGobjModel: return RO_P64(RO_GOBJ->mModels[a[0]]);
    case O::RoGobjTexture: return RO_P64(RO_GOBJ->mTextures[a[0]]);
    case O::RoGobjShadowTexture: return RO_P64(RO_GOBJ->mShadowTexture);
    case O::RoMeshWaterTile: return gWorld.mMap->mTerrainMesh.GetIsWaterTile((long)a[0], (long)a[1]);
    case O::RoModelInView: return GEPhysicalModel((GEModel*)p, *(const GETransformation*)a[0], f[0]).Intersects(gViewport, gOrthographicView);
    case O::RoUnitOpacity: return EEGetUnitOpacity(RO_UN, (long)a[1], *(float*)a[0]);
    // the picker, the selection, the ambient sounds
    case O::RoPickerAdd: gUnitPicker.AddUnit(RO_UN, *(const GETransformation*)a[0], f[0]);
        break;
    case O::RoPickerContains: return gUnitPicker.Contains(RO_UN);
    case O::RoPickUnitUnreferenced: return RO_P64(gUnitPicker.PickUnitUnreferenced(gMouse->mPosition, 1));
    case O::RoMainSelectUnits: return RO_P64(dynamic_cast<EEUICSelectUnits*>(gUIManager->GetMainGameForm()->GetControl(kMainGameSelectUnits)));
    case O::RoEditorSelectUnits: return RO_P64(gUIManager->GetScenarioEditorForm() != NULL ? gUIManager->GetScenarioEditorForm()->GetSelectUnits() : NULL);
    case O::RoTriggerSelectUnits: return RO_P64(dynamic_cast<EESEUICTriggerSelectUnits*>(gUIManager->GetScenarioEditorForm()->GetControlById(EE_SCREENS_SE_TS_OBJECT_SELECT_UNITS)));
    case O::RoSelectionIsDrawn: return ((EEUICSelectUnits*)p)->GetIsDrawn();
    case O::RoTriggerSelectionIsDrawn: return ((EESEUICTriggerSelectUnits*)p)->GetIsDrawn();
    case O::RoPickUnits: gUnitPicker.PickUnits(((EEUICSelectUnits*)p)->GetSelectionRectangle(), (EEPlayer*)a[0], 0, gU4E9230);
        break;
    case O::RoPickUnitsInRectangle: gUnitPicker.PickUnitsInRectangle(((EEUICSelectUnits*)p)->GetSelectionRectangle(), 0, gU4E9230);
        break;
    case O::RoPickTriggerUnitsInRectangle: gUnitPicker.PickUnitsInRectangle(((EESEUICTriggerSelectUnits*)p)->GetSelectionRectangle(), 0, gU4E9230);
        break;
    case O::RoAmbientCountUnit: gAmbientSoundManager.mAnalyzer.CountUnit((EEComplexUnit*)p);
        break;
    case O::RoAmbientCountTree: gAmbientSoundManager.mAnalyzer.CountTree(RO_UN);
        break;
    case O::RoAmbientFrameCountIncrement: gAmbientSoundManager.mFrameCount++;
        break;
    case O::RoAmbientUpdateDue: return gAmbientSoundManager.IsUpdateDue();
    case O::RoAmbientUpdate: gAmbientSoundManager.Update();
        break;
    // the terrain, the viewport, the rasterizer
    case O::RoVisibleRowCount: return gWorld.mMap->mTerrainMesh.GetNumVisibleTileRows();
    case O::RoVisibleRow: gWorld.mMap->mTerrainMesh.GetVisibleTileRow((unsigned long)a[0], ((long*)a[1])[0], ((long*)a[1])[1], ((long*)a[1])[2]);
        break;
    case O::RoMeshCliffTextureCount: return gWorld.mMap->mTerrainMesh.NumSpaceCliffTextures();
    case O::RoMeshSetCliffTextures: gWorld.mMap->mTerrainMesh.SetSpaceCliffTextures(gU4E8EF0);
        break;
    case O::RoCliffTexturesCount: return (int64_t)gU4E8EF0.size();
    case O::RoCliffTexturesPush: gU4E8EF0.push_back(RO_TEX);
        break;
    case O::RoUpdateVisibleTerrain: gWorld.mMap->mTerrainMesh.UpdateVisibleTerrain(gWorld.mTransformation, *(GEViewport*)p, 2, 1, 3, gOrthographicView);
        break;
    case O::RoTransformToCamera: gRasterizer->TransformToCamera(gWorld.mMap->mTerrainMesh, *(GEViewport*)p, f[0], gCameraFollowing, gOrthographicView);
        break;
    case O::RoRastViewport: return RO_P64(&gRasterizer->GetViewport(gScreenIndex));
    case O::RoRastLightMesh: gRasterizer->Light(gWorld.mMap->mTerrainMesh, gScreenIndex);
        break;
    case O::RoRastClearBlack: gRasterizer->Clear(GEColor(0.0f, 0.0f, 0.0f), *(const GERectangle*)a[0], gScreenIndex);
        break;
    case O::RoShowAllBackgrounds: gUIManager->GetMainGameForm()->ShowAllBackgrounds();
        break;
    case O::RoHideAllBackgrounds: gUIManager->GetMainGameForm()->HideAllBackgrounds();
        break;
    case O::RoViewportSetCamera: gViewport.SetCamera(gWorld.mTransformation);
        break;
    case O::RoRastSetViewport: gRasterizer->SetViewport(gViewport, gScreenIndex);
        break;
    case O::RoCopyViewport: gU4E8F08 = gViewport;
        break;
    case O::RoMainViewportDepthRange: gU4E8F08.SetDepthRange(f[0], f[1]);
        break;
    case O::RoRastSetMainViewport: gRasterizer->SetViewport(gU4E8F08, gScreenIndex);
        break;
    case O::RoMainViewportSetRearClipping: gU4E8F08.SetRearClippingDistance(f[0]);
        break;
    case O::RoRastViewRectangle: *(GERectangle*)a[0] = gRasterizer->GetViewport(gScreenIndex).GetViewRectangle();
        break;
    case O::RoScreenPoint: GetScreenPoint(*(const GE3DPoint*)a[0], *(GE2DPoint*)a[1]);
        break;
    case O::RoRastFog: gRasterizer->SetFog((unsigned long)a[0], gScreenIndex);
        break;
    case O::RoRastFogColor: gRasterizer->SetFogColor(GEColor(f[0], f[1], f[2]), gScreenIndex);
        break;
    case O::RoRastFogRange: gRasterizer->SetFogRange(f[0], f[1], gScreenIndex);
        break;
    case O::RoRastFogType: gRasterizer->SetFogType((unsigned long)a[0], gScreenIndex);
        break;
    case O::RoRastAmbientColor: gRasterizer->SetAmbientColor(GEColor(f[0], f[1], f[2]), gScreenIndex);
        break;
    case O::RoRastAmbientIntensity: gRasterizer->SetAmbientIntensity(f[0], gScreenIndex);
        break;
    case O::RoRastRemoveLights: gRasterizer->RemoveAllLights(gScreenIndex);
        break;
    case O::RoRastLighting: gRasterizer->SetLighting((unsigned long)a[0], gScreenIndex);
        break;
    case O::RoLightColor: gU4E9198.SetColor(GEColor(f[0], f[1], f[2]));
        break;
    case O::RoLightIntensity: gU4E9198.SetIntensity(f[0]);
        break;
    case O::RoLightDirection: gU4E9198.SetDirection(GEVector(f[0], f[1], f[2]));
        break;
    case O::RoRastAddLight: gRasterizer->AddLight(gU4E9198, gScreenIndex, gU4E9190);
        break;
    case O::RoRastUpdateLight: gRasterizer->UpdateLight(gU4E9190, gU4E9198, gScreenIndex);
        break;
    // the textures, the text and the screen line the raster table makes
    case O::RoLoadTexture: return RO_P64(gTextureManager->LoadTexture(UString((const char*)a[0]), *(const GETextureDescription*)a[1]));
    case O::RoTexReference: RO_TEX->Reference();
        break;
    case O::RoTexAlphaMode: RO_TEX->SetStageAlphaMode((unsigned long)a[0], -1);
        break;
    case O::RoTexColorMode: RO_TEX->SetStageColorMode((unsigned long)a[0], -1);
        break;
    case O::RoTexMinFilter: RO_TEX->SetStageMinFilterMode((unsigned long)a[0], -1);
        break;
    case O::RoTexMagFilter: RO_TEX->SetStageMagFilterMode((unsigned long)a[0], -1);
        break;
    case O::RoTexMipFilter: RO_TEX->SetStageMipFilterMode((unsigned long)a[0], -1);
        break;
    case O::RoTexMipBias: RO_TEX->SetStageMipMapBias(f[0], -1);
        break;
    case O::RoTexAddressing: RO_TEX->SetStageTextureAddressingMode((unsigned long)a[0], -1);
        break;
    case O::RoTextNew: return RO_P64(new GEText(gFontTahoma10));
    case O::RoTextDelete: delete RO_TEXT;
        break;
    case O::RoTextSetTransparent: RO_TEXT->SetIsTransparent(true);
        break;
    case O::RoTextSetColor: RO_TEXT->SetFgColor(GEColor(f[0], f[1], f[2]));
        break;
    case O::RoTextSetCentred: RO_TEXT->SetAlignment(kTextAlignCenter);
        break;
    case O::RoTextSetText: RO_TEXT->SetText(UWideString((const char*)a[0]));
        break;
    case O::RoTextSetLocation: RO_TEXT->SetLocation(GE2DPoint(f[0], f[1]));
        break;
    case O::RoMainGameDrawText: gUIManager->GetMainGameForm()->DrawClippedText(*(GERectangle*)a[0], RO_TEXT, NULL);
        break;
    case O::RoEditorDrawText: gUIManager->GetScenarioEditorForm()->DrawClippedText(*(GERectangle*)a[0], RO_TEXT, NULL);
        break;
    case O::RoGeLineNew: return RO_P64(new GELine);
    case O::RoGeLineDelete: delete RO_GELINE;
        break;
    case O::RoGeLineStart: RO_GELINE->SetStartPoint(GE2DPoint(f[0], f[1]));
        break;
    case O::RoGeLineEnd: RO_GELINE->SetEndPoint(GE2DPoint(f[0], f[1]));
        break;
    case O::RoGeLineOpacity: RO_GELINE->SetOpacityPerVertex(f[0], f[1]);
        break;
    case O::RoGeLineDraw: return gRasterizer->Draw(RO_GELINE, 1, gScreenIndex);
    // RFIL's world key (TEST ONLY)
    case O::RoPlayerId: return ((EEPlayer*)p)->GetPlayerID();
    case O::RoViewedUnitId: return ((EEPlayer*)p)->mViewedUnit != NULL ? ((EEPlayer*)p)->mViewedUnit->GetUnitID() : 0;
    case O::RoSelectedViewedUnitId: return ((EEPlayer*)p)->mSelectedViewedUnit != NULL ? ((EEPlayer*)p)->mSelectedViewedUnit->GetUnitID() : 0;
    case O::RoGroupContainsComplex: return ((EEPlayer*)p)->mSelectedGroup->Contains((EEComplexUnit*)a[0]);
    case O::RoUnitHitPointsValue: return RO_UN->mHitPoints;
    case O::RoUnitResourceAmount: return RO_UN->mResourceAmount;
    case O::RoCxFrame: return ((EEComplexUnit*)p)->mPhysicalModel.GetFrame();
    case O::RoCxNextFrame: return ((EEComplexUnit*)p)->mNextFrame;
    case O::RoCxAnimation: return ((EEComplexUnit*)p)->mAnimation;
    case O::RoCxCommandType: return ((EEComplexUnit*)p)->mCommand != NULL ? ((EEComplexUnit*)p)->mCommand->mType : 0;
    case O::RoCxIsBuilt: return ((EEComplexUnit*)p)->mIsBuilt;
    case O::RoPostClearRenderObjects: gGraphics->PostClearRenderObjects();
        break;
    case O::RoPostPushRenderObject: gGraphics->PostPushRenderObject(a[0] == 0 ? gU565360 : a[0] == 1 ? gU56535C : a[0] == 2 ? gU565358 : a[0] == 3 ? gU565354 : a[0] == 4 ? gU565350 : gU56534C);
        break;
    default: return EEPickerOp(inOp, p, a, f);
    }
    return 0;
}

double EECore9OpAF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::RoEnvironmentWind: return gWorld.mEnvironmentManager->mWind;
    case O::RoViewportRearClipping: return gViewport.GetRearClippingDistance();
    case O::RoMainViewportRearClipping: return gU4E8F08.GetRearClippingDistance();
    case O::RoZoomLock: return gZoomLock;
    case O::RoSpaceTurretHeight: return gSpaceTurretHeight;
    case O::RoSpaceStationHeight: return gSpaceStationHeight;
    case O::RoUnitStatsScale: return RO_UN->mStats->GetScale();
    case O::RoUnitStatsRange: return RO_UN->mStats->mRange;
    case O::RoUnitPhysicalScale: return RO_CX->mPhysicalModel.GetScale();
    case O::RoClassScale: return RO_UC->mScale;
    case O::RoClassShadowScale: return RO_UC->GetShadowScale();
    case O::RoClassF170: return RO_UC->GetF170();
    case O::RoDoppelScale: return RO_DG->GetScale();
    case O::RoGhostScale: return RO_GH->GetScale();
    case O::RoCxAnimationFraction: return ((EEComplexUnit*)p)->mAnimationFraction;
    default: return EEPickerOpF(inOp, p, a, f);
    }
}
