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

#include "dx7/DX7Rasterizer.h"
#include "lle/GEModelManager.h"
#include "lle/GERhombus.h"
#include "lle/UIControl.h"

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
#include "ee/EEWheel.h"
#include "ee/EEAreaEffectTemplate.h"
#include "rust/game/gamecpp.h"
#include "rust/game/screens.h"

// the 3D lines (0x529050) and a name (0x529030): one guard byte (0x529048) (provisional names)
std::vector<EERenderLine> gU529050;
UWideString gU529030;

// the unit picker and the environment (gamecpp render_object.rs' RENDER_ENV): one guard byte (0x528d70)
EEUnitPicker gUnitPicker(0x600);

EERenderLists EERenderLists::sInstance;

// 0x4e9348 (17 colours) and its init chain: EEInitialiseScene's key frames, ee_gfx scene.rs initial_tables
GEColor gU4E9338(1.0f, 1.0f, 1.0f);
// (0x4e9330, the grid's phase: gU4E9330, gamecpp globals.rs)
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
// the letterbox's zoom gF4BE374 (0x4be374) and the unit's other .bss globals gB529065, gB529066, gCameraFollowing,
// gB529068 (declared in ee/EEGraphics.h, ee/EmpireEarth.h): gamecpp globals.rs

// The scene, the frame's lists, the lighting and the posts: gamecpp render_object.rs (EECore9OpA's cases below)
void EEPostGraphicsAll() { ee_gamecpp_ro_post_render_objects(true); }

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

// GetUnitOpacity (ee_gfx fill.rs over its EeGfxOpacityHost) and the rasterizer table the Rust draws call (was
// gEERustRaster): gamecpp ro_raster.rs (the Rr cases below)

// EEROUnits.cpp's screen point (0x118676; the goal's countdown)
void GetScreenPoint(const GE3DPoint& inPoint, GE2DPoint& outPoint);

// ------------------------------------------------------------------------- round 9, lane A: this file's bodies' C++

// the unit picker's cases (EEUnitPicker.cpp), then lane B's
int64_t EEPickerOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EEPickerOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

// the mirrors and constants of gamecpp render_object.rs
static_assert(offsetof(EERenderFrameInfo, mYaw) == sizeof(void*) && offsetof(EERenderFrameInfo, mFrame) == sizeof(void*) + 8
              && offsetof(EERenderFrameInfo, mAmbient) == sizeof(void*) + 8 + sizeof(long), "render_object.rs FrameInfo");
static_assert(offsetof(GETextureDescription, mIs3DRenderable) == 4 * sizeof(long)
              && offsetof(GETextureDescription, mUseTexturePack) == 4 * sizeof(long) + 4, "render_object.rs TextureDescription");
// (its constants are checked against the headers by gamecpp cpp_consts_tests.rs)

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

// the C++ calls and members of this file's bodies in Rust (gamecpp render_object.rs; EeGameCpp's lane A section): one
// statement each
int64_t EECore9OpA(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    // the render globals
    case O::RoSceneFade: return RO_P64(&sSceneFade);
    case O::RoAmbientColor: return RO_P64(&gU4E9338);
    case O::RoFogColor: return RO_P64(&gU4E9220);
    case O::RoSunDirection: return RO_P64(&gU4E9210);
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
    case O::RoLineSetTexture: RO_LINE->mTexture = (GETexture*)a[0];
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
    case O::RoUnitAreaEffectStrength: return (long)ee_screens_ae_strength((uintptr_t)RO_CX->mAffectingAreaEffects.mItems[a[0]]);
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
    case O::RoPickUnits: ee_gamecpp_pz_box(&gUnitPicker, &(((EEUICSelectUnits*)p)->GetSelectionRectangle()), false, (EEPlayer*)a[0], 0, &gU4E9230);
        break;
    case O::RoPickUnitsInRectangle: ee_gamecpp_pz_box(&gUnitPicker, &(((EEUICSelectUnits*)p)->GetSelectionRectangle()), true, NULL, 0, &gU4E9230);
        break;
    case O::RoPickTriggerUnitsInRectangle: ee_gamecpp_pz_box(&gUnitPicker, &(((EESEUICTriggerSelectUnits*)p)->GetSelectionRectangle()), true, NULL, 0, &gU4E9230);
        break;
    case O::RoAmbientCountUnit: ee_gamecpp_ambient_count_unit(&gAmbientSoundManager.mAnalyzer, (EEComplexUnit*)p);
        break;
    case O::RoAmbientCountTree: ee_gamecpp_ambient_count_tree(&gAmbientSoundManager.mAnalyzer, RO_UN);
        break;
    case O::RoAmbientFrameCountIncrement: gAmbientSoundManager.mFrameCount++;
        break;
    case O::RoAmbientUpdateDue: return gAmbientSoundManager.IsUpdateDue();
    case O::RoAmbientUpdate: gAmbientSoundManager.Update();
        break;
    // the terrain, the viewport, the rasterizer
    case O::RoMeshSetCliffTextures: gWorld.mMap->mTerrainMesh.SetSpaceCliffTextures(gU4E8EF0);
        break;
    case O::RoCliffTexturesCount: return (int64_t)gU4E8EF0.size();
    case O::RoCliffTexturesPush: gU4E8EF0.push_back(RO_TEX);
        break;
    case O::RoShowAllBackgrounds: gUIManager->GetMainGameForm()->ShowAllBackgrounds();
        break;
    case O::RoHideAllBackgrounds: gUIManager->GetMainGameForm()->HideAllBackgrounds();
        break;
    case O::RoScreenPoint: GetScreenPoint(*(const GE3DPoint*)a[0], *(GE2DPoint*)a[1]);
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
    // round 11, lane A: the raster table and the opacity host (gamecpp ro_raster.rs)
    case O::RrModelDraw: return (int32_t)gRasterizer->Draw((GEModel*)p, gScreenIndex, f[0], gOrthographicView);
    case O::RrCreateModel: return RO_P64(gModelManager->CreateModel((uint32_t)a[0], (uint32_t)a[1], 1, 0, 1, (uint32_t)a[2], UString((const char*)a[3]), (uint32_t)a[4]));
    case O::RrCreateSubModel: return RO_P64(gModelManager->CreateSubModel((uint32_t)a[0], (uint32_t)a[1], 1, 0, 1, 0, 0, 0, (GEModel*)a[2]));
    case O::RrDrawSky: return (int32_t)gRasterizer->DrawSky((GEModel*)p, gScreenIndex, 1.0f, a[0] != 0);
    case O::RrTexture32: return gRasterizer->GetScreen(gScreenIndex)->Get32BitTextureSupport();
    case O::RrScreen: return (uint32_t)gScreenIndex;
    case O::RrObjects: return RO_P64(GERasterizer::RustObjects(gRasterizer, (void**)a[0]));
    case O::RrOpacityOwn: return ee_units_ck_body(EE_UNITS_CK_DO_TRANSLUCENT, (uint64_t)(uintptr_t)(EEComplexUnit*)p, (uint64_t)a[1], a[0] != 0, 0) != 0;
    case O::RrModelCenter: *(GE3DPoint*)a[1] = RO_UN->GetModel()->mModels[a[0]]->GetCenterPoint();
        break;
    case O::RrPhysicalFrame: return ((EEComplexUnit*)p)->mPhysicalModel.GetFrame();
    case O::RrCinematic: return gWorld.mInCinematic;
    // round 11, lane A: EEOrbital::Init (gamecpp ro_misc.rs)
    case O::OvSetSpeed: ((EEOrbital*)p)->mSpeed = f[0];
        break;
    case O::OvSetModel: ((EEOrbital*)p)->mModel = (GEModel*)a[0];
        break;
    case O::OvModelName: return RO_P64((const char*)((GEModel*)p)->GetName());
    default: return EEPickerOp(inOp, p, a, f);
    }
    return 0;
}

double EECore9OpAF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::RoEnvironmentWind: return gWorld.mEnvironmentManager->mWind;
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
    // round 11, lane A (gamecpp ro_raster.rs)
    case O::RrModelRadius: return RO_UN->GetModel()->mModels[a[0]]->GetBoundingRadius((unsigned long)a[1]);
    case O::RrPhysicalScale: return ((EEComplexUnit*)p)->mPhysicalModel.GetScale();
    default: return EEPickerOpF(inOp, p, a, f);
    }
}
