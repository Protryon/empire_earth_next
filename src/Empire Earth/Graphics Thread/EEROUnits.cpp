// C:\EEX\Empire Earth\Graphics Thread\EEROUnits.cpp (__FILE__ 0x4be760). The units' render object (vtable 0x441bbc):
// 0x114296..0x118741 (the player-colour key and eleven colour constants' init chains, ctor, ??_G, dtor, Virtual00,
// the ring/bar/model passes, the labels, Virtual08, Virtual0C, the media release, the screen projection GetScreenPoint);
// EEROUserInterface.cpp's init chain starts at 0x118741. Unit flags: the exe's module set.
// The passes (the rings, the models, the placements, the bars, the labels), the media's textures and models and the
// colour constants are ee_gfx ro_units.rs's (RUST_PLAN R4.4): the C++ hands the frame's render lists over as records
// and keeps the bar rhombi and the line batch (LoadMedia 0x516a97's asserts carry their original __LINE__ 1823/1824).
#include <windows.h>

#include <vector>

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
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GEPolygonArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GELine.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "lle/GEText.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "GEWindow.h"
#include "rust/base/math.h"   // ee_gfx units.rs (RUST_PLAN R4.4)
#include "rust/render/ge.h"
#include "rust/render/gfx.h"
#include "rust/base/math.h"
#include "rust/render/ge.h"
#include "rust/render/gfx.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EERenderLists.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEROUnits.h"
#include "ee/EERenderSnapshot.h"

extern bool& gShowCollisionSpheres;          // 0x5318dc (the units' collision spheres are drawn; provisional name)
extern bool& gInterpolateFrames;             // 0x5308cf (the frames are interpolated; provisional name)
extern long& gModelDetail;                   // 0x530898 (0: full opacity, 1: half, else opaque; provisional name)
extern bool gUnitPanelActive;               // 0x5297a0 (EEROUserInterface.cpp)
extern GEFont* gU531994;                    // 0x531994 (the labels' default font; before gFontTahoma10; provisional name)
// 0x4e5b98: an empty string (EEHumanPlayer's ctors assign it too); zero bytes in the original's initialised .data.
// Size: one dword (the next named datum is 8 bytes on, 0x4e5ba0; the dword between is unreferenced)
char gEmptyString[4] = "";
extern EERenderUnit* gU529044;              // 0x529044 (the unit with the debug label; EERenderObject.cpp; provisional name)
extern UWideString gU529030;                // 0x529030 (that label; EERenderObject.cpp; provisional name)
// the rasterizer as the Rust draws call it (EERenderObject.cpp)
extern const EeGfxRaster gEERustRaster;

// the bars' rhombi (0x529508 the fill, 0x52950c its back) and the line batch (0x529478, new[]) (provisional names)
static GERhombus* sBars[2];
static GELine* sLines;
long gCollisionSphereMode;                  // 0x529510 (ee/EmpireEarth.h; read-only: EERenderObject, EEMessage. .bss after this
                                            // unit's statics, before EEROUserInterface's gUIViewport 0x529518: either unit)

// The GE value classes pass as their #[repr(C)] mirrors (the same storage)
static_assert(sizeof(GE2DPoint) == sizeof(EeGe2DPoint) && sizeof(GE3DPoint) == sizeof(EeGe3DPoint) &&
              sizeof(GERectangle) == sizeof(EeGeRectangle) && sizeof(GETransformation) == sizeof(EeGeTransformation) &&
              sizeof(GEViewport) == sizeof(EeGeViewport) && sizeof(GEColor) == sizeof(EeGfxColor), "GE mirrors");

// Creates the units' render object
EEROUnits::EEROUnits()
{
}

// Destroys the units' render object
EEROUnits::~EEROUnits()
{
}

// Fills the render lists of units and objects for this frame
void EEROUnits::FrameStarting()
{
    this->FillRenderLists();
}

// the raster table's entries for the bars, the placement lines and the labels (EERenderObject.cpp's table)
void EEROUnitsBarSet(uint32_t inWhich, const EeGeRectangle* inRect, const EeGfxColor* inColors)
{
    if (inRect != NULL)
        sBars[inWhich]->SetRectangle(*reinterpret_cast<const GERectangle*>(inRect));
    if (inColors != NULL)
        sBars[inWhich]->SetColorPerVertex(*reinterpret_cast<const GEColor*>(&inColors[0]),
                                          *reinterpret_cast<const GEColor*>(&inColors[1]),
                                          *reinterpret_cast<const GEColor*>(&inColors[2]),
                                          *reinterpret_cast<const GEColor*>(&inColors[3]));
}
int32_t EEROUnitsBarDraw(uint32_t inWhich)
{
    return gRasterizer->Draw(sBars[inWhich], gScreenIndex);
}
int32_t EEROUnitsScreenLines(const EeGfxScreenLine* inLines, uint32_t inCount, float inStartU, float inEndU)
{
    GETextureCoordinate theStart;
    GETextureCoordinate theEnd;
    theStart.SetU(inStartU);
    theEnd.SetU(inEndU);
    for (uint32_t i = 0; i < inCount; i++) {
        sLines[i].SetTextureCoordinates(theStart, theEnd);
        sLines[i].SetColor(*reinterpret_cast<const GEColor*>(&inLines[i].color));
        sLines[i].SetStartPoint(*reinterpret_cast<const GE2DPoint*>(&inLines[i].start));
        sLines[i].SetEndPoint(*reinterpret_cast<const GE2DPoint*>(&inLines[i].end));
    }
    return gRasterizer->Draw(sLines, inCount, gScreenIndex);
}
void EEROUnitsLabel(const uint8_t* inText, float inX, float inY, EeGfxColor inColor, uint32_t inFont,
                    const EeGeRectangle* inView, const EeGeRectangle* inStrip, float inBottom)
{
    GEText theText(gU531994);
    theText.SetFgColor(GEColor(inColor.red, inColor.green, inColor.blue));
    theText.SetIsTransparent(true);
    theText.SetAlignment(kTextAlignCenter);
    if (inFont == 2)
        theText.SetFont(gFontTahoma16);
    else if (inFont == 1)
        theText.SetFont(gFontTahoma12);
    theText.SetText(UWideString(reinterpret_cast<const char*>(inText)));
    GE2DPoint thePoint;
    thePoint.SetX(inX);
    thePoint.SetY(inY);
    theText.SetLocation(thePoint);
    const GERectangle& theView = *reinterpret_cast<const GERectangle*>(inView);
    float theTextHeight = gFreeCamera ? 0.0f : theText.GetTextExtent().GetHeight();
    uint32_t theTargets = ee_gfx_units_label_targets(inY, theTextHeight, inBottom, gFreeCamera);
    if (theTargets & 1)
        gUIManager->GetMainGameForm()->DrawClippedText(theView, &theText, NULL);
    if (theTargets & 2)
        gUIManager->GetMainGameForm()->DrawClippedText(*reinterpret_cast<const GERectangle*>(inStrip), &theText, NULL);
    if (theTargets & 4)
        gUIManager->GetScenarioEditorForm()->DrawClippedText(theView, &theText, NULL);
}

namespace {
// one render unit as its passes read it (its graphic objects' models, textures and flags, its class's)
EeGfxRenderUnit UnitRecord(EERenderUnit& inUnit)
{
    EeGfxRenderUnit r = {};
    r.transformation = *reinterpret_cast<const EeGeTransformation*>(&inUnit.mTransformation);
    r.player_color = {inUnit.mPlayerColor.mRed, inUnit.mPlayerColor.mGreen, inUnit.mPlayerColor.mBlue};
    r.tint_color = {inUnit.mTintColor.mRed, inUnit.mTintColor.mGreen, inUnit.mTintColor.mBlue};
    EEGraphicObject* theObject = inUnit.mSource;
    GEModel* theModel = theObject->mModels[inUnit.mAnimation];
    r.object = theObject;
    r.model = theModel;
    r.texture = theObject->mTextures[inUnit.mAnimation];
    if (theModel != NULL)
        r.center = *reinterpret_cast<const EeGe3DPoint*>(&theModel->GetCenterPoint());
    r.bar = *reinterpret_cast<const EeGe3DPoint*>(&theObject->mBar);
    r.drawn = theObject->mIsDrawn;
    r.alpha_ref = theObject->mAlphaReference;
    r.object_opacity = theObject->mOpacity;
    if (inUnit.mSource2 != NULL) {
        r.object2 = inUnit.mSource2;
        r.model2 = inUnit.mSource2->mModel;
        r.texture2 = inUnit.mSource2->mTexture;
        r.drawn2 = inUnit.mSource2->mIsDrawn;
    }
    r.health_bars = inUnit.mHealthBars.empty() ? NULL : &inUnit.mHealthBars[0];
    r.health_bar_count = inUnit.mHealthBars.size();
    r.turret_angles = inUnit.mTurretAngles.empty() ? NULL : &inUnit.mTurretAngles[0];
    r.turret_models = inUnit.mTurretModels.empty() ? NULL : reinterpret_cast<void* const*>(&inUnit.mTurretModels[0]);
    r.turret_count = min(inUnit.mTurretAngles.size(), inUnit.mTurretModels.size());
    r.orbital_angles = inUnit.mOrbitalAngles.empty() ? NULL : &inUnit.mOrbitalAngles[0];
    r.orbital_models = inUnit.mOrbitalModels.empty() ? NULL : reinterpret_cast<void* const*>(&inUnit.mOrbitalModels[0]);
    r.orbital_count = min(inUnit.mOrbitalAngles.size(), inUnit.mOrbitalModels.size());
    r.wheel_angles = inUnit.mWheelAngles.empty() ? NULL : &inUnit.mWheelAngles[0];
    r.wheel_models = inUnit.mWheelModels.empty() ? NULL : reinterpret_cast<void* const*>(&inUnit.mWheelModels[0]);
    r.wheel_count = min(inUnit.mWheelAngles.size(), inUnit.mWheelModels.size());
    r.name = reinterpret_cast<const uint8_t*>((const char*)inUnit.mName);
    r.scale = inUnit.mScale;
    r.selection_size = inUnit.mSelectionSize;
    r.opacity = inUnit.mOpacity;
    r.health = inUnit.mHealth;
    r.fraction = inUnit.mFraction;
    r.sub_model_offset_scale = inUnit.mSubModelOffsetScale;
    r.fog_scale = inUnit.mFogScale;
    r.inner_collision_sphere = inUnit.mInfo->GetInnerCollisionSphere();
    r.category = (int32_t)inUnit.mCategory;
    r.command_type = (int32_t)inUnit.mCommandType;
    r.animation = (int32_t)inUnit.mAnimation;
    r.current_frame = (uint32_t)inUnit.mCurrentFrame;
    r.next_frame = (uint32_t)inUnit.mNextFrame;
    r.area_effect_strength = (uint32_t)inUnit.mAreaEffectStrength;
    r.air = (inUnit.mInfo->mDomains & kMovementAir) != 0;
    r.waypoint = inUnit.mUnitClassIndex == kUnitTypeWaypoint;
    r.fogged = inUnit.mIsFogged;
    r.tinted = inUnit.mIsTinted;
    r.has_sub_model_offset = inUnit.mHasSubModelOffset;
    r.second_full_opacity = inUnit.mSecondFullOpacity;
    return r;
}

// one placed map object as its passes read it
EeGfxRenderObject ObjectRecord(const EERenderShadow& inObject)
{
    EeGfxRenderObject r = {};
    r.location = *reinterpret_cast<const EeGe3DPoint*>(&inObject.mLocation);
    r.model = inObject.mObject->mModels[inObject.mModelIndex];
    r.texture = inObject.mObject->mTextures[inObject.mModelIndex];
    r.bar = *reinterpret_cast<const EeGe3DPoint*>(&inObject.mObject->mBar);
    r.alpha_ref = inObject.mObject->mAlphaReference;
    r.object_opacity = inObject.mObject->mOpacity;
    r.model_scale = inObject.mModelScale;
    r.selection_size = inObject.mSelectionSize;
    r.health = inObject.mHealth;
    r.opacity = inObject.mOpacity;
    r.fraction = inObject.mFraction;
    r.current_frame = (uint32_t)inObject.mCurrentFrame;
    r.next_frame = (uint32_t)inObject.mNextFrame;
    r.fogged = inObject.mIsFogged;
    return r;
}

// the effects' units pass: queue 3, batched per texture
void UnitsEffects(void*)
{
    for (EEEffectQueue::iterator i = gEffectQueues[3].begin(); i != gEffectQueues[3].end(); i++) {
        BeginEffectBatches(i->first);
        for (std::deque<EEEffect*>::iterator theEffect = i->second.begin(); theEffect != i->second.end(); theEffect++)
            (*theEffect)->DrawPass5();
        FlushEffectBatches();
        EndEffectBatches();
    }
}

// the frame's render lists as records and indices (ee_gfx ro_units.rs)
struct UnitsFrame {
    std::vector<EeGfxRenderUnit> mUnits;
    std::vector<EeGfxRenderObject> mObjects;
    std::vector<uint32_t> mLists[6];
    std::vector<EeGfxGhostRecord> mGhosts;
    EeGfxUnitsScene mScene;

    template <class M> void Units(M& inMap, std::vector<uint32_t>& outList)
    {
        for (typename M::iterator i = inMap.begin(); i != inMap.end(); i++)
            outList.push_back((uint32_t)(i->second - EERenderLists::sInstance.mRenderUnits));
    }
    template <class M> void Objects(M& inMap, std::vector<uint32_t>& outList)
    {
        for (typename M::iterator i = inMap.begin(); i != inMap.end(); i++)
            outList.push_back((uint32_t)(i->second - EERenderLists::sInstance.mRenderShadows));
    }
    UnitsFrame()
    {
        EERenderLists& l = EERenderLists::sInstance;
        for (unsigned long i = 0; i < l.mRenderUnitCount; i++)
            this->mUnits.push_back(UnitRecord(l.mRenderUnits[i]));
        for (unsigned long i = 0; i < l.mRenderShadowCount; i++)
            this->mObjects.push_back(ObjectRecord(l.mRenderShadows[i]));
        this->Units(l.mUnits0, this->mLists[0]);
        this->Units(l.mUnits1, this->mLists[1]);
        for (unsigned long i = 0; i < l.mSelectedUnits.size(); i++)
            this->mLists[2].push_back((uint32_t)(l.mSelectedUnits[i] - l.mRenderUnits));
        this->Objects(l.mShadows0, this->mLists[3]);
        this->Objects(l.mShadows1, this->mLists[4]);
        for (unsigned long i = 0; i < l.mSelectedObjects.size(); i++)
            this->mLists[5].push_back((uint32_t)(l.mSelectedObjects[i] - l.mRenderShadows));
        for (unsigned long i = 0; i < l.mGhostRecords.size(); i++) {
            EERenderUnitRecord& theRecord = l.mGhostRecords[i];
            EeGfxGhostRecord g;
            g.unit = UnitRecord(theRecord.mUnit);
            static_assert(sizeof(g.heights) == sizeof(theRecord.mHeights) && sizeof(g.cells) == sizeof(theRecord.mCells),
                          "the grid");
            memcpy(g.heights, theRecord.mHeights, sizeof(g.heights));
            memcpy(g.cells, theRecord.mCells, sizeof(g.cells));
            g.tile_x = (int32_t)theRecord.mTileX;
            g.tile_y = (int32_t)theRecord.mTileY;
            g.grid_size = (int32_t)theRecord.mUnit.mGridSize;
            g.can_place = theRecord.mUnit.mCanPlace;
            g.land_building = theRecord.mUnit.mInfo->IsLandBuilding();
            this->mGhosts.push_back(g);
        }
        EeGfxUnitsScene& s = this->mScene;
        s.context = NULL;
        s.viewport = reinterpret_cast<const EeGeViewport*>(&gU4E8F08);
        s.orthographic = gOrthographicView;
        s.cinematic = gRenderSnapshot.mInCinematic;
        s.collision_spheres = gShowCollisionSpheres;
        s.interpolate_frames = gInterpolateFrames;
        s.model_detail = (int32_t)gModelDetail;
        s.free_camera = gFreeCamera;
        s.panel_active = gUnitPanelActive;
        s.grid_phase = gU4E9330;
        s.camera_yaw = gRenderEnvironment.mCameraYaw;
        s.window_width = (uint32_t)gGameWindow->GetWidth();
        s.window_height = (uint32_t)gGameWindow->GetHeight();
        s.units = this->mUnits.empty() ? NULL : &this->mUnits[0];
        s.unit_count = this->mUnits.size();
        s.objects = this->mObjects.empty() ? NULL : &this->mObjects[0];
        s.object_count = this->mObjects.size();
        const uint32_t** theLists[6] = {&s.units0, &s.units1, &s.selected_units, &s.objects0, &s.objects1,
                                        &s.selected_objects};
        uintptr_t* theCounts[6] = {&s.units0_count, &s.units1_count, &s.selected_unit_count, &s.objects0_count,
                                   &s.objects1_count, &s.selected_object_count};
        for (int i = 0; i < 6; i++) {
            *theLists[i] = this->mLists[i].empty() ? NULL : &this->mLists[i][0];
            *theCounts[i] = this->mLists[i].size();
        }
        s.selected_unit = l.mSelectedUnit != NULL ? (int32_t)(l.mSelectedUnit - l.mRenderUnits) : -1;
        s.selected_object = l.mSelectedObject != NULL ? (int32_t)(l.mSelectedObject - l.mRenderShadows) : -1;
        s.ghosts = this->mGhosts.empty() ? NULL : &this->mGhosts[0];
        s.ghost_count = this->mGhosts.size();
        s.debug_unit = gU529044 != NULL ? (int32_t)(gU529044 - l.mRenderUnits) : -1;
        s.debug_text = reinterpret_cast<const uint8_t*>((const char*)gU529030);
        s.draw_effects = UnitsEffects;
    }
};
}

// Loads the units' media: bar rhombi, command and dotted-line textures, the line buffer, the area-effect
// ring model and the three selection ring models (segment count by detail level) (ee_gfx ro_units.rs)
void EEROUnits::LoadMedia()
{
    GETextureCoordinate theLineStart;
    GETextureCoordinate theLineEnd(3.0f, 0.0f);
    sBars[1] = new GERhombus;
    sBars[0] = new GERhombus;
    ESVERIFY_AT(sBars[1] != NULL, 1823);
    ESVERIFY_AT(sBars[0] != NULL, 1824);
    ee_gfx_ro_units_load(&gEERustRaster, (int32_t)gModelDetail);
    sLines = new GELine[0x40];
    for (unsigned long i = 0; i < 0x40; i++) {
        sLines[i].SetColor(GEColor(0.0f, 1.0f, 0.0f));
        sLines[i].SetTexture(static_cast<GETexture*>(ee_gfx_ro_units_line_texture()), gScreenIndex);
        sLines[i].SetTextureCoordinates(theLineStart, theLineEnd);
    }
}

// Draws the selections and rings (not in cinematics), the object and unit models, the buildings being
// placed, the air selection rings and bars, then the effects' pass 5 (ee_gfx ro_units.rs)
void EEROUnits::RenderScene()
{
    UnitsFrame theFrame;
    ee_gfx_ro_units_render(&gEERustRaster, &theFrame.mScene);
}

// Draws the unit labels (not in cinematics; ee_gfx ro_units.rs) and the effects' pass 3
void EEROUnits::FrameDone()
{
    EEEffectQueue::iterator theIterator;
    std::deque<EEEffect*>::iterator theEffect;
    if (!gRenderSnapshot.mInCinematic) {
        UnitsFrame theFrame;
        ee_gfx_ro_units_labels(&gEERustRaster, &theFrame.mScene);
    }
    for (theIterator = gEffectQueues[1].begin(); theIterator != gEffectQueues[1].end(); theIterator++) {
        for (theEffect = theIterator->second.begin(); theEffect != theIterator->second.end(); theEffect++)
            (*theEffect)->DrawPass3();
    }
}

// Releases the units' media: bar rhombi, line buffer, textures, ring and selection models (ee_gfx ro_units.rs)
void EEROUnits::ReleaseMedia()
{
    delete sBars[1];
    delete sBars[0];
    if (sLines != NULL)
        delete[] sLines;
    sLines = NULL;
    ee_gfx_ro_units_release(&gEERustRaster);
}

// Projects a world point to window coordinates in the game view (ee_gfx units.rs)
void GetScreenPoint(const GE3DPoint& inPoint, GE2DPoint& outPoint)
{
    ee_gfx_units_screen_point(reinterpret_cast<const EeGeViewport*>(&gU4E8F08), gOrthographicView,
                              reinterpret_cast<const EeGe3DPoint*>(&inPoint), reinterpret_cast<EeGe2DPoint*>(&outPoint));
}

void EEROUnits::SceneStarting()
{
}
