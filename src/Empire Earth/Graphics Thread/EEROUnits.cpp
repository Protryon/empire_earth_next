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
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GEText.h"
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
#include "ee/EEPlayer.h"
#include "ee/EEGraphics.h"
#include "ee/EERenderObject.h"
#include "ee/EERenderLists.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEROUnits.h"
#include "ee/EERenderSnapshot.h"
#include "rust/game/gamecpp.h"
#include <utility>

extern bool& gShowCollisionSpheres;          // 0x5318dc (the units' collision spheres are drawn; provisional name)
extern bool& gInterpolateFrames;             // 0x5308cf (the frames are interpolated; provisional name)
extern long& gModelDetail;                   // 0x530898 (0: full opacity, 1: half, else opaque; provisional name)
extern GEFont* gU531994;                    // 0x531994 (the labels' default font; before gFontTahoma10; provisional name)
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

// the raster table's bar draw (EERenderObject.cpp's table; the bars' set, the screen lines and the labels are gamecpp
// ro_units.rs')
int32_t EEROUnitsBarDraw(uint32_t inWhich)
{
    return gRasterizer->Draw(sBars[inWhich], gScreenIndex);
}
extern std::vector<unsigned long> gU4E9230;   // EERenderObject.cpp: the rectangle's picked unit ids
extern EERenderUnit* gU529044;               // the picked enemy's render unit
extern UWideString gU529030;                 // its owner's name

namespace {

// a texture map in its order, as rfil.rs reads it
template <class Map> EeGfxRfilPairs Flatten(const Map& inMap, std::vector<EeGfxRfilPair>& outPairs)
{
    outPairs.clear();   // (the pairs' storage is reused from view to view: sDrawPairs)
    for (typename Map::const_iterator it = inMap.begin(); it != inMap.end(); ++it)
        outPairs.push_back({it->first, it->second});
    return {outPairs.data(), outPairs.size()};
}

template <class T> EeGfxRfilSpan Span(const std::vector<T>& inValues) { return {inValues.data(), inValues.size()}; }

EeGfxRfilLayout MakeLayout()
{
    static_assert(offsetof(EERenderUnitRecord, mUnit) == 0, "rfil.rs reads a record's unit first");
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

// the media, the frame's draws and the labels: gamecpp ro_units.rs
void EEROUnits::LoadMedia()
{
    ee_gamecpp_ru_load_media();
}

void EEROUnits::RenderScene()
{
    ee_gamecpp_ru_draw(false);
}

void EEROUnits::FrameDone()
{
    ee_gamecpp_ru_frame_done();
}

void EEROUnits::ReleaseMedia()
{
    ee_gamecpp_ru_release_media();
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

// ------------------------------------------------------------------------- round 10, lane A: this file's C++ (gamecpp ro_units.rs)

int64_t EERoViewsOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);
double EERoViewsOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

// the render lists' pairs of the units' draws (valid until the next view)
static std::vector<EeGfxRfilPair> sDrawPairs[6];

#define RU_P64(x) ((int64_t)(intptr_t)(x))
#define RU_TEXT ((GEText*)p)
#define RU_COLOR(k) (*reinterpret_cast<const GEColor*>(&((const EeGfxColor*)a[1])[k]))

// the C++ calls and members of gamecpp ro_units.rs (EECore10OpA's default): one statement each
int64_t EERoUnitsOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::RuRaster: return RU_P64(&gEERustRaster);
    case O::RuListsView: EERenderListsView(*(EeGfxRfilFrame*)a[0], sDrawPairs);
        break;
    case O::RuMainViewport: return RU_P64(&gU4E8F08);
    case O::RuCinematic: return gRenderSnapshot.mInCinematic;
    case O::RuCollisionSpheres: return gShowCollisionSpheres;
    case O::RuInterpolateFrames: return gInterpolateFrames;
    case O::RuModelDetail: return gModelDetail;
    case O::RuWindowWidth: return gGameWindow->GetWidth();
    case O::RuWindowHeight: return gGameWindow->GetHeight();
    case O::RuBarNew: sBars[a[0]] = new GERhombus;
        break;
    case O::RuBarPresent: return sBars[a[0]] != NULL;
    case O::RuBarDelete: delete sBars[a[0]];
        break;
    case O::RuBarSetRect: sBars[a[0]]->SetRectangle(*(const GERectangle*)a[1]);
        break;
    case O::RuBarSetColors: sBars[a[0]]->SetColorPerVertex(RU_COLOR(0), RU_COLOR(1), RU_COLOR(2), RU_COLOR(3));
        break;
    case O::RuLinesNew: sLines = new GELine[a[0]];
        break;
    case O::RuLinesDelete: delete[] std::exchange(sLines, nullptr);
        break;
    case O::RuLineInitColor: sLines[a[0]].SetColor(GEColor(0.0f, 1.0f, 0.0f));
        break;
    case O::RuLineInitTexture: sLines[a[0]].SetTexture((GETexture*)a[1], gScreenIndex);
        break;
    case O::RuLineInitCoordinates: sLines[a[0]].SetTextureCoordinates(GETextureCoordinate(), GETextureCoordinate(3.0f, 0.0f));
        break;
    case O::RuLineCoordinates: sLines[a[0]].SetTextureCoordinates(GETextureCoordinate(f[0], 0.0f), GETextureCoordinate(f[1], 0.0f));
        break;
    case O::RuLineColor: sLines[a[0]].SetColor(*(const GEColor*)a[1]);
        break;
    case O::RuLineStart: sLines[a[0]].SetStartPoint(*(const GE2DPoint*)a[1]);
        break;
    case O::RuLineEnd: sLines[a[0]].SetEndPoint(*(const GE2DPoint*)a[1]);
        break;
    case O::RuLinesDraw: return gRasterizer->Draw(sLines, (unsigned long)a[0], gScreenIndex);
    case O::RuTextNew: return RU_P64(new GEText(gU531994));
    case O::RuTextFont16: RU_TEXT->SetFont(gFontTahoma16);
        break;
    case O::RuTextFont12: RU_TEXT->SetFont(gFontTahoma12);
        break;
    default: return EERoViewsOp(inOp, p, a, f);
    }
    return 0;
}

double EERoUnitsOpF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::RuTextHeight: return RU_TEXT->GetTextExtent().GetHeight();
    default: return EERoViewsOpF(inOp, p, a, f);
    }
}
