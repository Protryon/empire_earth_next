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
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/TMPoll.h"
#include "lle/GEVertexArray.h"
#include "lle/GERhombus.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UICheckBox.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
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
#include "ee/EESEARust.h"
#include "ee/EESMArea.h"
#include "rust/game/screens.h"
#include "rust/sim/scenario.h"

extern GEViewport gViewport;
extern GEFont* gSeplScreenFont;   // the screen's font while a screen makes its controls (EESERBUnitTypes.cpp)

// Sets the map rectangle the editor highlights
// (defined in this unit: every caller calls it out of line)
void EEUIFScenarioEditor::SetPaintArea(const GERectangle& inRectangle)
{
    this->GetBrushGroup()->mRectangle = inRectangle;
}

// ---- the sub-screen's host (editor_area.rs: the ops from 600)

namespace {

EEUIFScenarioEditor* SearEditor()
{
    return gUIManager->GetScenarioEditorForm();
}

// the tile under the point (the mouse's if inMouse): whether the map picks one, its coordinates
bool SearPick(bool inMouse, float inX, float inY, long& outX, long& outY)
{
    GE2DPoint thePoint = inMouse ? gMouse->mPosition : GE2DPoint(inX, inY);
    return gWorld.mMap->PickScreenTile(thePoint, outX, outY, true) != NULL;
}

// the ground point under the point: the screen line solved at its tile's height, truncated
void SearGround(bool inMouse, float inX, float inY, long& outX, long& outY)
{
    GE2DPoint thePoint = inMouse ? gMouse->mPosition : GE2DPoint(inX, inY);
    GE3DPoint theLocation;
    GE3DLine theLine;
    float theT;
    EETile* theTile = gWorld.mMap->GetTileAt(thePoint, outX, outY);
    gWorld.mMap->GetScreenLine(thePoint, theLine);
    theLine.SolveForZ(theTile->GetHeight(), theT, theLocation);
    outX = (long)theLocation.GetX();
    outY = (long)theLocation.GetY();
}

}   // namespace

extern "C" int32_t ee_screenscpp_sear(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    EEScenarioManager* theManager = gWorld.mScenarioManager;
    switch (inOp) {
    case EE_SCREENS_SEAR_OP_LIST:
        switch (i[0]) {
        case EE_SCREENS_SEOB_SM_SORT: theManager->SortList(EE_SCENARIO_LIST_AREAS); break;
        case EE_SCREENS_SEOB_SM_DOWN: theManager->MoveEntry(EE_SCENARIO_LIST_AREAS, (unsigned long)i[1], false); break;
        case EE_SCREENS_SEOB_SM_UP: theManager->MoveEntry(EE_SCENARIO_LIST_AREAS, (unsigned long)i[1], true); break;
        case EE_SCREENS_SEOB_SM_DELETE: theManager->DeleteEntry(EE_SCENARIO_LIST_AREAS, (unsigned long)i[1]); break;
        case EE_SCREENS_SEOB_SM_DUPLICATE: theManager->DuplicateEntry(EE_SCENARIO_LIST_AREAS, (unsigned long)i[1]); break;
        default: theManager->NewEntry(EE_SCENARIO_LIST_AREAS); break;
        }
        return 0;
    case EE_SCREENS_SEAR_OP_NAMES:
        theManager->GetNames(EE_SCENARIO_NAMES_USER_AREAS, *static_cast<std::vector<UWideString>*>(p[0]));
        return 0;
    case EE_SCREENS_SEAR_OP_HIGHLIGHT: SearEditor()->GetBrushGroup()->SetIsHighlightShown(i[0] != 0); return 0;
    case EE_SCREENS_SEAR_OP_PAINT_AREA: SearEditor()->SetPaintArea(GERectangle(f[0], f[1], f[2], f[3])); return 0;
    case EE_SCREENS_SEAR_OP_UI_HIDDEN: return SearEditor()->GetIsUIHidden();
    case EE_SCREENS_SEAR_OP_ACTIVATE:
        SearEditor()->TriggerCall(EE_SCREENS_SETS_HIDE_TEXT);
        SearEditor()->TriggerCall(EE_SCREENS_SETS_SHOW_ALPHA_SORT);
        return 0;
    case EE_SCREENS_SEAR_OP_LOAD: {
        EESMArea* theArea = theManager->GetArea((unsigned long)i[0]);
        if (theArea == NULL)
            return 0;
        SearEditor()->TriggerCall(EE_SCREENS_SETS_SHOW_TEXT);
        SearEditor()->GetTriggerBox(false)->SetText(theArea->GetName());
        SearEditor()->GetTriggerBox(true)->SetText(theArea->GetText());
        const GERectangle& r = theArea->GetRect();
        f[0] = r.GetLeft();
        f[1] = r.GetTop();
        f[2] = r.GetRight();
        f[3] = r.GetBottom();
        i[1] = (int32_t)theArea->GetAnchor().GetX();
        i[2] = (int32_t)theArea->GetAnchor().GetY();
        i[3] = (theArea->GetHasRectangle() ? 1 : 0) | (theArea->GetHasAnchor() ? 2 : 0);
        static_cast<UIComboBox*>(p[0])->SetCurrentSelection(theArea->GetSelectMode());
        return 1;
    }
    case EE_SCREENS_SEAR_OP_STORE: {
        // the original reads a missing area: nothing here (also for the two below)
        EESMArea* theArea = theManager->GetArea((unsigned long)i[0]);
        if (theArea == NULL)
            return 0;
        UWideString theText;
        theArea->mName = UWideString(SearEditor()->GetTriggerBox(false)->GetText(0));
        SearEditor()->GetTriggerBox(true)->GetAllText(theText);
        theArea->mText = theText;
        static_cast<UIComboBox*>(p[0])->GetCurrentSelection(theArea->mSelectMode);
        return 0;
    }
    case EE_SCREENS_SEAR_OP_SET_RECT:
        if (EESMArea* theArea = theManager->GetArea((unsigned long)i[0]))
            theArea->SetRectangle(GERectangle(f[0], f[1], f[2], f[3]));
        return 0;
    case EE_SCREENS_SEAR_OP_SET_ANCHOR:
        if (EESMArea* theArea = theManager->GetArea((unsigned long)i[0]))
            theArea->SetAnchor(U2DPoint<long>(i[1], i[2]));
        return 0;
    case EE_SCREENS_SEAR_OP_IN_VIEW:
        return gViewport.GetViewRectangle().Contains(i[0] ? gMouse->mPosition : GE2DPoint(f[0], f[1]));
    case EE_SCREENS_SEAR_OP_PICK: {
        long theX = 0, theY = 0;
        bool thePicked = SearPick(i[0] != 0, f[0], f[1], theX, theY);
        i[1] = (int32_t)theX;
        i[2] = (int32_t)theY;
        return thePicked;
    }
    case EE_SCREENS_SEAR_OP_GROUND: {
        long theX = 0, theY = 0;
        SearGround(i[0] != 0, f[0], f[1], theX, theY);
        i[1] = (int32_t)theX;
        i[2] = (int32_t)theY;
        return 0;
    }
    case EE_SCREENS_SEAR_OP_LBUTTON: {
        ISEntity* theButton = UIManager::GetLMouseButton();
        return (theButton->GetValue() > 0 ? 1 : 0) | (theButton->JustPressed() ? 2 : 0) |
               (theButton->JustUnpressed() ? 4 : 0);
    }
    case EE_SCREENS_SEAR_OP_PAINT_BEGIN: {
        *static_cast<EESEARust**>(p[0]) = new EESEARust(kSEAPaintArea, NULL, i[0], i[1]);
        gMouse->SetBounds(gViewport.GetViewRectangle());
        SearEditor()->GetEdgeScroll()->mEdges = GERectangle(gViewport.GetViewRectangle().GetLeft(),
                                                        gViewport.GetViewRectangle().GetTop(),
                                                        gViewport.GetViewRectangle().GetRight() - 1.0f,
                                                        gViewport.GetViewRectangle().GetBottom() - 1.0f);
        return 0;
    }
    case EE_SCREENS_SEAR_OP_PAINT_CORNER:
        ee_screens_sew_area_corner(static_cast<EESEARust*>(p[0])->mAction, i[0], i[1]);
        return 0;
    case EE_SCREENS_SEAR_OP_PAINT_END: {
        EESEARust* theAction = static_cast<EESEARust*>(p[0]);
        gMouse->SetBounds(GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight()));
        SearEditor()->GetEdgeScroll()->mEdges = GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth() - 1.0f,
                                                        (float)gGameWindow->GetHeight() - 1.0f);
        int32_t theArea[4];   // left, top, right, bottom
        ee_screens_sew_area(theAction->mAction, theArea);
        f[0] = (float)theArea[0];
        f[1] = (float)theArea[1];
        f[2] = (float)theArea[2];
        f[3] = (float)theArea[3];
        theAction->Dereference();
        return 0;
    }
    case EE_SCREENS_SEAR_OP_PAINT_RELEASE: static_cast<EESEARust*>(p[0])->Dereference(); return 0;
    }
    return 0;
}
