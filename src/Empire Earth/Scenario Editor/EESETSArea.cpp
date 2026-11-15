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
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/TMPoll.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/Input System.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UICheckBox.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include <vector>
#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETriggerScreen.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEMouse.h"
#include "ee/EEAScroll.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EESEARust.h"
#include "ee/EESMArea.h"
#include "ee/EESETSArea.h"
#include "rust/ee_screens.h"

extern GEViewport gViewport;
extern GEFont* gSeplScreenFont;   // the screen's font while a screen makes its controls (EESERBUnitTypes.cpp)

// FUNCTION: EXE 0x57ccf3
// Creates the area sub-screen: the red 'no area selected' label, the area selection mode combo and the center
// camera button, in rectangle mode with no area drawn (editor_area.rs)
EESETSArea::EESETSArea(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex)
{
    gSeplScreenFont = inFont;
    uint32_t theIndex = (uint32_t)ioIndex;
    this->mRust = ee_screens_sear_new(inForm, &theIndex);
    ioIndex = theIndex;
}

// SYNTHETIC: EXE 0x57d07f
// (EESETSArea's scalar deleting destructor)

// FUNCTION: EXE 0x57d09b
// Destroys the area sub-screen's controls and releases a rectangle still being drawn (editor_area.rs)
EESETSArea::~EESETSArea()
{
    ee_screens_sear_free(this->mRust);
}

// The slots (editor_area.rs), each with the base's selection: 0x57d0e0 Sort, 0x57d0eb Update, 0x57d1d5 MoveDown,
// 0x57d1e7 MoveUp, 0x57d1f9 Delete, 0x57d21d Deactivate, 0x57d22e Duplicate, 0x57d240 Activate, 0x57d272 Store,
// 0x57d3f8 Hide, 0x57d41b Load, 0x57d5ba New, 0x57d5c5 SetAreaMode, 0x57d667 Show
void EESETSArea::Call(uint32_t inWhat, int32_t inArgument)
{
    ee_screens_sear_call(this->mRust, inWhat, (int32_t)this->mSelection, inArgument);
}

void EESETSArea::Sort() { this->Call(EE_SCREENS_SEAR_SORT); }
void EESETSArea::Update() { this->Call(EE_SCREENS_SEAR_UPDATE); }
void EESETSArea::MoveDown() { this->Call(EE_SCREENS_SEAR_MOVE_DOWN); }
void EESETSArea::MoveUp() { this->Call(EE_SCREENS_SEAR_MOVE_UP); }
void EESETSArea::Delete() { this->Call(EE_SCREENS_SEAR_DELETE); }
void EESETSArea::Deactivate() { this->Call(EE_SCREENS_SEAR_DEACTIVATE); }
void EESETSArea::Duplicate() { this->Call(EE_SCREENS_SEAR_DUPLICATE); }
void EESETSArea::Activate() { this->Call(EE_SCREENS_SEAR_ACTIVATE); }
void EESETSArea::Store() { this->Call(EE_SCREENS_SEAR_STORE); }
void EESETSArea::Hide() { this->Call(EE_SCREENS_SEAR_HIDE); }
void EESETSArea::Load() { this->Call(EE_SCREENS_SEAR_LOAD); }
void EESETSArea::New() { this->Call(EE_SCREENS_SEAR_NEW); }
void EESETSArea::Show() { this->Call(EE_SCREENS_SEAR_SHOW); }
void EESETSArea::SetAreaMode(bool inRectangle) { this->Call(EE_SCREENS_SEAR_SET_MODE, inRectangle ? 1 : 0); }

// FUNCTION: EXE 0x57d6a9
// Draws the area with the left button: drags a rectangle (mouse confined to the view) or sets the anchor point on
// release, then highlights it (editor_area.rs)
void EESETSArea::MouseMove(const GE2DPoint& inLocation)
{
    ee_screens_sear_mouse_move(this->mRust, inLocation.GetX(), inLocation.GetY());
}

// FUNCTION: EXE 0x57d3aa
// Returns the center of the drawn rectangle (or the anchor point in point mode), false if none (editor_area.rs)
bool EESETSArea::GetCenter(GE2DPoint& outCenter)
{
    float theCenter[2];
    if (!ee_screens_sear_center(this->mRust, theCenter))
        return false;
    outCenter = GE2DPoint(theCenter[0], theCenter[1]);
    return true;
}

// the screen's state (the editor gate; no original address)
void EESETSArea::GetState(GERectangle& outRectangle, GE2DPoint& outPoint, unsigned long& outFlags) const
{
    float theRectangle[4];
    float thePoint[2];
    outFlags = ee_screens_sear_state(this->mRust, theRectangle, thePoint);
    outRectangle = GERectangle(theRectangle[0], theRectangle[1], theRectangle[2], theRectangle[3]);
    outPoint = GE2DPoint(thePoint[0], thePoint[1]);
}

// FUNCTION: EXE 0x57d3e6
// Returns the names of the scenario's user areas (editor_area.rs)
void EESETSArea::GetNames(std::vector<UWideString>& outNames)
{
    ee_screens_sear_names(this->mRust, &outNames);
}

// FUNCTION: EXE 0x57d34c
// Sets the area's rectangle and marks it as having one (EESMArea::SetRectangle, inline)

// FUNCTION: EXE 0x57d381
// Sets the area's anchor tile and marks it as having one (EESMArea::SetAnchor, inline)

// FUNCTION: EXE 0x57d597
// Sets the map rectangle the editor highlights
// (defined in this unit: every caller calls it out of line)
void EEUIFScenarioEditor::SetPaintArea(const GERectangle& inRectangle)
{
    this->GetBrushGroup()->mRectangle = inRectangle;
}

// FUNCTION: EXE 0x57d991
// (the rectangle drawing action's ctor: ee_screens editor_whole.rs, an EESEARust)

// FUNCTION: EXE 0x57da08
// Returns the scenario's area by index (EEScenarioManager::GetArea, inline)

// SYNTHETIC: EXE 0x57da35
// (EESETSBaseObject's scalar deleting destructor)

// SYNTHETIC: EXE 0x57da7d
// (EESEECenterCameraOnArea's scalar deleting destructor)

// ---- the sub-screen's host (editor_area.rs: the ops from 600)

namespace {

EEUIFScenarioEditor* SearEditor()
{
    return gUIManager->mScenarioEditorForm;
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
        SearEditor()->GetTriggerScreen()->HideText();
        SearEditor()->GetTriggerScreen()->ShowAlphaSort();
        return 0;
    case EE_SCREENS_SEAR_OP_LOAD: {
        EESMArea* theArea = theManager->GetArea((unsigned long)i[0]);
        if (theArea == NULL)
            return 0;
        EESETriggerScreen* theScreen = SearEditor()->GetTriggerScreen();
        theScreen->ShowText();
        theScreen->GetNameEditBox()->SetText(theArea->GetName());
        theScreen->GetTextEditBox()->SetText(theArea->GetText());
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
        theArea->mName = UWideString(SearEditor()->GetTriggerScreen()->GetName());
        SearEditor()->GetTriggerScreen()->GetTextEditBox()->GetAllText(theText);
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
    case EE_SCREENS_SEAR_OP_CENTER: {
        GE2DPoint theCenter;
        GERectangle(f[0], f[1], f[2], f[3]).GetCenter(theCenter);
        f[0] = theCenter.GetX();
        f[1] = theCenter.GetY();
        return 0;
    }
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
