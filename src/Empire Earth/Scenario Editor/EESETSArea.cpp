// C:\EEX\Empire Earth\Scenario Editor\EESETSArea.cpp (file name inferred: no __FILE__; RTTI class EESETSArea).
// The trigger screen's area sub-screen: 0x17ccf3..0x17daa8 (ctor, ??_G, dtor, the slots in vtable order of their
// bodies with the COMDATs of their first uses: EESMArea::SetRectangle/SetAnchor, EEUIFScenarioEditor::SetPaintArea,
// EESEAPaintArea's ctor, EEScenarioManager::GetArea, then the ??_G of the base and of the two events). After
// EESETriggerScreen.cpp, before the condition controls' unit (EESETSCComboBox.cpp 0x17daa8). Unit flags: the exe
// module set.
//
// The sub-screen whole in Rust (ee_screens editor_area.rs): EESETSArea holds its object and forwards its slots with
// the base's selection. This unit is its host (ee_screenscpp_sear): the scenario's areas, the trigger tab's boxes,
// the editor's paint area, the mouse, the map's picking and the paint action.
#include <windows.h>
#include <string>
#include "lle/LowLevelEngine.h"
#include "GEWindow.h"
#include "lle/GEViewport.h"
#include "lle/GERhombus.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEMouse.h"
#include "ee/EEARust.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EESMArea.h"


// Sets the map rectangle the editor highlights
// (defined in this unit: every caller calls it out of line)
void EEUIFScenarioEditor::SetPaintArea(const GERectangle& inRectangle) { this->GetBrushGroup()->mRectangle = inRectangle; }

// ---- the sub-screen's host (editor_area.rs: the ops from 600; editor_trigx.rs composes the sequences)

namespace {

EEUIFScenarioEditor* SearEditor() { return gUIManager->GetScenarioEditorForm(); }

UWideString SearBytes(const void* inBytes, int32_t inLength)
{
    return UWideString(std::string(static_cast<const char*>(inBytes), (size_t)(inLength > 0 ? inLength : 0)).c_str());
}


// the point a pick or ground read takes, its tile's coordinates, the screen line and its ground location (the
// originals' locals)
GE2DPoint sSearPoint;
long sSearX = 0;
long sSearY = 0;
GE3DLine sSearLine;
GE3DPoint sSearLocation;
float sSearT = 0.0f;

}   // namespace


// the sub-screen's single statements (editor_trigx.rs EE_SCREENS_SEARX_*, 500..: p0 the area or the action, p2 the
// out word of a found or made one or a text's buffer (capacity i3), a text p1's i0 bytes)
extern "C" int32_t ee_screenscpp_searx(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    void** theOut = static_cast<void**>(p[2]);
    EESMArea* theArea = static_cast<EESMArea*>(p[0]);
    switch (inOp) {
    case EE_SCREENS_SEAR_OP_NAMES: gWorld.mScenarioManager->GetNames(EE_SCENARIO_NAMES_USER_AREAS, *static_cast<std::vector<UWideString>*>(p[0])); return 0;
    case EE_SCREENS_SEAR_OP_HIGHLIGHT: SearEditor()->GetBrushGroup()->SetIsHighlightShown(i[0] != 0); return 0;
    case EE_SCREENS_SEAR_OP_PAINT_AREA: SearEditor()->SetPaintArea(GERectangle(f[0], f[1], f[2], f[3])); return 0;
    case EE_SCREENS_SEAR_OP_UI_HIDDEN: return SearEditor()->GetIsUIHidden();
    case EE_SCREENS_SEAR_OP_IN_VIEW: return gViewport.GetViewRectangle().Contains(i[0] ? gMouse->mPosition : GE2DPoint(f[0], f[1]));
    case EE_SCREENS_SEAR_OP_PAINT_CORNER: ee_screens_sew_area_corner(static_cast<EESEARust*>(p[0])->mAction, i[0], i[1]); return 0;
    case EE_SCREENS_SEAR_OP_PAINT_RELEASE: static_cast<EESEARust*>(p[0])->Dereference(); return 0;
    case EE_SCREENS_SEARX_AREA: *theOut = gWorld.mScenarioManager->GetArea((unsigned long)i[0]); return 0;
    case EE_SCREENS_SEARX_NAME: return EESEWrite(p[2], i[3], theArea->GetName());
    case EE_SCREENS_SEARX_TEXT: return EESEWrite(p[2], i[3], theArea->GetText());
    case EE_SCREENS_SEARX_RECT + 0: f[0] = theArea->GetRect().GetLeft(); return 0;
    case EE_SCREENS_SEARX_RECT + 1: f[0] = theArea->GetRect().GetTop(); return 0;
    case EE_SCREENS_SEARX_RECT + 2: f[0] = theArea->GetRect().GetRight(); return 0;
    case EE_SCREENS_SEARX_RECT + 3: f[0] = theArea->GetRect().GetBottom(); return 0;
    case EE_SCREENS_SEARX_ANCHOR_X: return (int32_t)theArea->GetAnchor().GetX();
    case EE_SCREENS_SEARX_ANCHOR_Y: return (int32_t)theArea->GetAnchor().GetY();
    case EE_SCREENS_SEARX_HAS: return (theArea->GetHasRectangle() ? 1 : 0) | (theArea->GetHasAnchor() ? 2 : 0);
    case EE_SCREENS_SEARX_SELECT_MODE: return (int32_t)theArea->GetSelectMode();
    case EE_SCREENS_SEARX_SET_NAME: theArea->mName = SearBytes(p[1], i[0]); return 0;
    case EE_SCREENS_SEARX_SET_TEXT: theArea->mText = SearBytes(p[1], i[0]); return 0;
    case EE_SCREENS_SEARX_GET_SELECTION: static_cast<UIComboBox*>(p[1])->GetCurrentSelection(theArea->mSelectMode); return 0;
    case EE_SCREENS_SEARX_SET_RECT: theArea->SetRectangle(GERectangle(f[0], f[1], f[2], f[3])); return 0;
    case EE_SCREENS_SEARX_SET_ANCHOR: theArea->SetAnchor(U2DPoint<long>(i[0], i[1])); return 0;
    case EE_SCREENS_SEARX_POINT: sSearPoint = i[0] != 0 ? gMouse->mPosition : GE2DPoint(f[0], f[1]); return 0;
    case EE_SCREENS_SEARX_XY_RESET: sSearX = sSearY = 0; return 0;
    case EE_SCREENS_SEARX_LOCATION_RESET: sSearLocation = GE3DPoint(); return 0;
    case EE_SCREENS_SEARX_PICK: return gWorld.mMap->PickScreenTile(sSearPoint, sSearX, sSearY, true) != NULL;
    case EE_SCREENS_SEARX_TILE_AT: *theOut = gWorld.mMap->GetTileAt(sSearPoint, sSearX, sSearY); return 0;
    case EE_SCREENS_SEARX_SCREEN_LINE: gWorld.mMap->GetScreenLine(sSearPoint, sSearLine); return 0;
    case EE_SCREENS_SEARX_SOLVE: sSearLine.SolveForZ(static_cast<EETile*>(p[0])->GetHeight(), sSearT, sSearLocation); return 0;
    case EE_SCREENS_SEARX_GROUND_X: sSearX = (long)sSearLocation.GetX(); return 0;
    case EE_SCREENS_SEARX_GROUND_Y: sSearY = (long)sSearLocation.GetY(); return 0;
    case EE_SCREENS_SEARX_X: return (int32_t)sSearX;
    case EE_SCREENS_SEARX_Y: return (int32_t)sSearY;
    case EE_SCREENS_SEARX_LBUTTON_DOWN: return UIManager::GetLMouseButton()->GetValue() > 0;
    case EE_SCREENS_SEARX_LBUTTON_PRESSED: return UIManager::GetLMouseButton()->JustPressed();
    case EE_SCREENS_SEARX_LBUTTON_UNPRESSED: return UIManager::GetLMouseButton()->JustUnpressed();
    case EE_SCREENS_SEARX_NEW_PAINT: *theOut = new EESEARust(EE_SCREENS_SEW_PAINT_AREA, NULL, i[0], i[1]); return 0;
    case EE_SCREENS_SEARX_BOUNDS_VIEW: gMouse->SetBounds(gViewport.GetViewRectangle()); return 0;
    case EE_SCREENS_SEARX_VIEW_LEFT: f[0] = gViewport.GetViewRectangle().GetLeft();
        return 0;
    case EE_SCREENS_SEARX_VIEW_TOP: f[0] = gViewport.GetViewRectangle().GetTop();
        return 0;
    case EE_SCREENS_SEARX_VIEW_RIGHT: f[0] = gViewport.GetViewRectangle().GetRight();
        return 0;
    case EE_SCREENS_SEARX_VIEW_BOTTOM: f[0] = gViewport.GetViewRectangle().GetBottom();
        return 0;
    case EE_SCREENS_SEARX_SET_EDGES: SearEditor()->GetEdgeScroll()->mEdges = GERectangle(f[0], f[1], f[2], f[3]);
        return 0;
    case EE_SCREENS_SEARX_BOUNDS_WINDOW: gMouse->SetBounds(GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight())); return 0;
    case EE_SCREENS_SEARX_WINDOW_WIDTH: f[0] = (float)gGameWindow->GetWidth();
        return 0;
    case EE_SCREENS_SEARX_WINDOW_HEIGHT: f[0] = (float)gGameWindow->GetHeight();
        return 0;
    case EE_SCREENS_SEARX_PAINT_AREA:   // (left, top, right, bottom)
        ee_screens_sew_area(static_cast<EESEARust*>(p[0])->mAction, i);
        return 0;
    case EE_SCREENS_SEARX_PAINT_RELEASE: static_cast<EESEARust*>(p[0])->Dereference(); return 0;
    }
    return 0;
}
