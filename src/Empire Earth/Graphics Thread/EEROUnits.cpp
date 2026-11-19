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
#include "ee/EERenderCapture.h"
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

extern std::vector<unsigned long> gU4E9230;   // EERenderObject.cpp: the rectangle's picked unit ids
extern EERenderUnit* gU529044;               // the picked enemy's render unit
extern UWideString gU529030;                 // its owner's name

namespace {

// a texture map in its order, as rfil.rs reads it
template <class Map> EeGfxRfilPairs Flatten(const Map& inMap, std::vector<EeGfxRfilPair>& outPairs)
{
    for (typename Map::const_iterator it = inMap.begin(); it != inMap.end(); ++it)
        outPairs.push_back({it->first, it->second});
    return {outPairs.data(), outPairs.size()};
}

template <class T> EeGfxRfilSpan Span(const std::vector<T>& inValues) { return {inValues.data(), inValues.size()}; }

EeGfxRfilLayout MakeLayout()
{
    static_assert(sizeof(EERenderShadow) == sizeof(EeGfxFillObject) && offsetof(EERenderUnitRecord, mUnit) == 0,
                  "rfil.rs reads the shadows as EeGfxFillObject, a record's unit first");
    EeGfxRfilLayout l = {};
    l.size = sizeof(EeGfxRfilLayout);
    l.unit_stride = sizeof(EERenderUnit);
    l.unit_health_bars = offsetof(EERenderUnit, mHealthBars);
    l.unit_turret_angles = offsetof(EERenderUnit, mTurretAngles);
    l.unit_turret_models = offsetof(EERenderUnit, mTurretModels);
    l.unit_orbital_angles = offsetof(EERenderUnit, mOrbitalAngles);
    l.unit_orbital_models = offsetof(EERenderUnit, mOrbitalModels);
    l.unit_wheel_angles = offsetof(EERenderUnit, mWheelAngles);
    l.unit_wheel_models = offsetof(EERenderUnit, mWheelModels);
    l.unit_name = offsetof(EERenderUnit, mName);
    l.record_stride = sizeof(EERenderUnitRecord);
    l.record_heights = offsetof(EERenderUnitRecord, mHeights);
    l.record_cells = offsetof(EERenderUnitRecord, mCells);
    l.record_tile_x = offsetof(EERenderUnitRecord, mTileX);
    l.record_tile_y = offsetof(EERenderUnitRecord, mTileY);
    l.line_stride = sizeof(EERenderLine);
    l.line_points = offsetof(EERenderLine, mPoints);
    l.line_texture = offsetof(EERenderLine, mTexture);
    l.line_width = offsetof(EERenderLine, mWidth);
    return l;
}

}   // namespace

// The render lists as ee_gfx reads them in place (rfil.rs, units_lists.rs): the pools and vectors, the multimaps
// flattened into ioPairs (which must outlive the frame's use)
void EERenderListsView(EeGfxRfilFrame& outFrame, std::vector<EeGfxRfilPair> (&ioPairs)[6])
{
    static const EeGfxRfilLayout kLayout = MakeLayout();
    const EERenderLists& l = EERenderLists::sInstance;
    EeGfxRfilFrame& f = outFrame;
    f.layout = &kLayout;
    f.units = l.mRenderUnits;
    f.units_used = l.mRenderUnitCount;
    f.shadows = reinterpret_cast<const EeGfxFillObject*>(l.mRenderShadows);
    f.shadows_used = l.mRenderShadowCount;
    f.maps[0] = Flatten(l.mUnits0, ioPairs[0]);
    f.maps[1] = Flatten(l.mUnits1, ioPairs[1]);
    f.maps[2] = Flatten(l.mUnits2, ioPairs[2]);
    f.maps[3] = Flatten(l.mShadows0, ioPairs[3]);
    f.maps[4] = Flatten(l.mShadows1, ioPairs[4]);
    f.maps[5] = Flatten(l.mShadows2, ioPairs[5]);
    f.selected_units = Span(l.mSelectedUnits);
    f.selected_objects = Span(l.mSelectedObjects);
    f.selected_unit = l.mSelectedUnit;
    f.selected_object = l.mSelectedObject;
    f.label_unit = gU529044;
    f.label_name = (const char*)gU529030;
    f.picked_ids = Span(gU4E9230);
    f.ghosts = Span(l.mGhostRecords);
    f.lines = Span(gU529050);
}

namespace {
// the effects' units pass: queue 3, batched per texture
void UnitsEffects(void*)
{
    ee_gfx_effect_queues_draw(5);
}

// the frame's render lists drawn by ee_gfx ro_units.rs (the records and indices: units_lists.rs, over the lists'
// view) with the scene's settings
void DrawUnits(bool inLabels)
{
    std::vector<EeGfxRfilPair> thePairs[6];
    EeGfxRfilFrame theFrame = {};
    EERenderListsView(theFrame, thePairs);
    EeGfxUnitsScene s = {};
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
    s.draw_effects = UnitsEffects;
    ee_gfx_ro_units_draw_lists(&gEERustRaster, &theFrame, &s, inLabels);
}
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
    DrawUnits(false);
}

// Draws the unit labels (not in cinematics; ee_gfx ro_units.rs) and the effects' pass 3
void EEROUnits::FrameDone()
{
    if (!gRenderSnapshot.mInCinematic) {
        DrawUnits(true);
    }
    ee_gfx_effect_queues_draw(3);
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
