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
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GERasterizer.h"
#include "lle/UIForm.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEGraphics.h"
#include "ee/EERenderLists.h"
#include "ee/EEUIFMainGame.h"
#include "lle/GESurface.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EEROTerrain.h"
#include "ee/EEROUserInterface.h"
#include <utility>

extern bool& gShowCollisionSpheres;          // 0x5318dc (the units' collision spheres are drawn; provisional name)
extern bool& gInterpolateFrames;             // 0x5308cf (the frames are interpolated; provisional name)
extern long& gModelDetail;                   // 0x530898 (0: full opacity, 1: half, else opaque; provisional name)
extern GEFont* gU531994;                    // 0x531994 (the labels' default font; before gFontTahoma10; provisional name)

// the bars' rhombi (0x529508 the fill, 0x52950c its back) and the line batch (0x529478, new[]) (provisional names)
static GERhombus* sBars[2];
static GELine* sLines;
long gCollisionSphereMode;                  // 0x529510 (ee/EmpireEarth.h; read-only: EERenderObject, EEMessage. .bss after this
                                            // unit's statics, before EEROUserInterface's gUIViewport 0x529518: either unit)

// The GE value classes pass as their #[repr(C)] mirrors (the same storage)
static_assert(sizeof(GE2DPoint) == sizeof(EeGe2DPoint) && sizeof(GE3DPoint) == sizeof(EeGe3DPoint) &&
              sizeof(GERectangle) == sizeof(EeGeRectangle) && sizeof(GETransformation) == sizeof(EeGeTransformation) &&
              sizeof(GEViewport) == sizeof(EeGeViewport) && sizeof(GEColor) == sizeof(EeGfxColor), "GE mirrors");

extern std::vector<unsigned long> gU4E9230;   // EERenderObject.cpp: the rectangle's picked unit ids
extern EERenderUnit* gU529044;               // the picked enemy's render unit
extern UWideString gU529030;                 // its owner's name

// Projects a world point to window coordinates in the game view (ee_gfx units.rs)
void GetScreenPoint(const GE3DPoint& inPoint, GE2DPoint& outPoint)
{
    ee_gfx_units_screen_point(reinterpret_cast<const EeGeViewport*>(&gU4E8F08), gOrthographicView,
                              reinterpret_cast<const EeGe3DPoint*>(&inPoint), reinterpret_cast<EeGe2DPoint*>(&outPoint));
}

// ------------------------------------------------------------------------- round 10, lane A: this file's C++ (gamecpp ro_units.rs)

int64_t EECore10OpB5(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEDataTextFile.cpp
extern unsigned long& gTextureMipLevelsToDrop;
extern bool& gDrawClouds;                    // 0x5308d3
extern bool& gTrilinearFilter;               // 0x5308da
static_assert(sizeof(RTSColoredRectangle) == 5 * sizeof(long) + 4 * sizeof(float) && sizeof(RTSMapTileRun) == 4 * sizeof(long), "ro_views.rs ColoredRectangle, grid runs");

#define RU_P64(x) ((int64_t)(intptr_t)(x))
#define RU_TEXT ((GEText*)p)
#define RU_MAP ((EEMap*)p)
#define RU_COLOR(k) (*reinterpret_cast<const GEColor*>(&((const EeGfxColor*)a[1])[k]))

static_assert(offsetof(EERenderUnitRecord, mUnit) == 0, "rfil.rs reads a record's unit first");

// the C++ calls and members of gamecpp ro_units.rs, ro_lists.rs and ro_views.rs (EECommandLine.cpp's EECommandLineOp default): one statement each
int64_t EERoUnitsOp(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::RrBarDraw: return gRasterizer->Draw(sBars[a[0]], gScreenIndex);
    case O::RuMainViewport: return RU_P64(&gU4E8F08);
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
    // round 11, lane A: the render lists' view (gamecpp ro_lists.rs)
    case O::RlLayUnitStride: return sizeof(EERenderUnit);
    case O::RlLayUnitHealthBars: return offsetof(EERenderUnit, mHealthBars);
    case O::RlLayUnitTurretAngles: return offsetof(EERenderUnit, mTurretAngles);
    case O::RlLayUnitTurretModels: return offsetof(EERenderUnit, mTurretModels);
    case O::RlLayUnitOrbitalAngles: return offsetof(EERenderUnit, mOrbitalAngles);
    case O::RlLayUnitOrbitalModels: return offsetof(EERenderUnit, mOrbitalModels);
    case O::RlLayUnitWheelAngles: return offsetof(EERenderUnit, mWheelAngles);
    case O::RlLayUnitWheelModels: return offsetof(EERenderUnit, mWheelModels);
    case O::RlLayUnitName: return offsetof(EERenderUnit, mName);
    case O::RlLayRecordStride: return sizeof(EERenderUnitRecord);
    case O::RlLayRecordHeights: return offsetof(EERenderUnitRecord, mHeights);
    case O::RlLayRecordCells: return offsetof(EERenderUnitRecord, mCells);
    case O::RlLayRecordTileX: return offsetof(EERenderUnitRecord, mTileX);
    case O::RlLayRecordTileY: return offsetof(EERenderUnitRecord, mTileY);
    case O::RlLayLineStride: return sizeof(EERenderLine);
    case O::RlLayLinePoints: return offsetof(EERenderLine, mPoints);
    case O::RlLayLineTexture: return offsetof(EERenderLine, mTexture);
    case O::RlLayLineWidth: return offsetof(EERenderLine, mWidth);
    case O::RlUnits: return RU_P64(EERenderLists::sInstance.mRenderUnits);
    case O::RlShadows: return RU_P64(EERenderLists::sInstance.mRenderShadows);
    case O::RlLabelUnit: return RU_P64(gU529044);
    case O::RlLabelName: return RU_P64((const char*)gU529030);
    case O::RlPickedIds: return RU_P64(gU4E9230.data());
    case O::RlPickedCount: return (int64_t)gU4E9230.size();
    case O::RlGhosts: return RU_P64(EERenderLists::sInstance.mGhostRecords.data());
    case O::RlLines: return RU_P64(gU529050.data());
    case O::RlLinesCount: return (int64_t)gU529050.size();
    // the other render objects (gamecpp ro_views.rs)
    // the UI's render object
    case O::GuCopyData: ((EEROUserInterface*)p)->mUIManager->CopyData();
        break;
    case O::GuSelectedGroup: return RU_P64(((EEPlayer*)p)->mSelectedGroup);
    case O::GuGroupFirstUnit: return RU_P64(((EESelectedGroup*)p)->GetFirstUnit());
    case O::GuCurrentViewedUnit: return RU_P64(gWorld.GetCurrentPlayer()->mSelectedViewedUnit);
    case O::GuMainUiState: return gUIManager->GetMainGameForm()->mUIState;
    case O::GuTargetWidth: return gRasterizer->GetRenderTarget(gScreenIndex)->GetWidth();
    case O::GuTargetHeight: return gRasterizer->GetRenderTarget(gScreenIndex)->GetHeight();
    case O::GuDrawPolygons: ((EEROUserInterface*)p)->mUIManager->DrawPolygons();
        break;
    // the ground shadows
    case O::GsDrawGroundShadows: return gDrawGroundShadows;
    case O::GsShadowModel: return RU_P64(((EEGraphicObject*)p)->mShadowModel);
    case O::GsShadowTexture: return RU_P64(((EEGraphicObject*)p)->mShadowTexture);
    // the terrain
    case O::TeMipLevelsToDrop: return gTextureMipLevelsToDrop;
    case O::TeTrilinear: return gTrilinearFilter;
    case O::TeDrawClouds: return gDrawClouds;
    case O::TeMapSize: return gWorld.mMap->mSize;
    case O::TeMapGridRuns: return (*(const RTSMapTileRun**)a[0] = gWorld.mMap->mU1955DC.data(), (int64_t)gWorld.mMap->mU1955DC.size());
    case O::TeGridMode: return ((EEROTerrain*)p)->mGridMode;
    case O::TeDraw3DLine: gRasterizer->Draw3DLine(gU529050[a[0]].mPoints, gU529050[a[0]].mTexture, gU529050[a[0]].mWidth, gScreenIndex, false);
        break;
    // the snapshot
    case O::SnMapColoredCount: return (int64_t)RU_MAP->mColoredRectangles.size();
    case O::SnMapColoredAt: return RU_P64(&RU_MAP->mColoredRectangles[a[0]]);
    case O::SnMapColoredLock: RU_MAP->mCriticalSectionForColoredRectangles->Enter();
        break;
    case O::SnMapColoredUnlock: RU_MAP->mCriticalSectionForColoredRectangles->Leave();
        break;
    case O::SnMapColoredErase: RU_MAP->GetColoredRectangles().erase(RU_MAP->GetColoredRectangles().begin() + a[0]);
        break;
    case O::SnEditorPresent: return gUIManager->GetScenarioEditorForm() != NULL;
    case O::SnBrushRectangle: *(GERectangle*)a[0] = gUIManager->GetScenarioEditorForm()->GetBrushGroup()->mRectangle;
        break;
    case O::SnBrushHighlightShown: return gUIManager->GetScenarioEditorForm()->GetBrushGroup()->mIsHighlightShown;
    // RFIL
    case O::RcPhysicsPresent: return gPhysics != NULL;
    default: return EECore10OpB5(inOp, p, a, f);
    }
    return 0;
}

