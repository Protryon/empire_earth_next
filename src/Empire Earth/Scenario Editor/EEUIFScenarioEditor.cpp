// C:\EEX\Empire Earth\Scenario Editor\EEUIFScenarioEditor.cpp (file name inferred: no __FILE__; RTTI class
// EEUIFScenarioEditor). The scenario editor's main form: 0x19cc39..0x1a0315 (its events' handlers, the ctor, the
// members, then the inline event classes; the parts once kept as EEUIEGrabScroll.cpp 0x19e2d2 and the head of
// EEUIEOpenCivBuilder.cpp 0x19eba9 are one unit with it). After EESEUnit.cpp. Unit flags: the exe module set.
//
// The form whole in Rust (ee_screens editor_form.rs): EEUIFScenarioEditor holds its object (the controls, events,
// tabs, state and selection as slots) and forwards; the trigger-only save (Ctrl+S, once EEUIESaveScenarioTriggersOnly)
// is an EESEERust whose report Rust writes over the scenario's records. This unit is the form's host
// (ee_screenscpp_sefm): the controls only the form makes and the world it reads and posts to.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/Input System.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NEMessage.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFileComboBox.h"
#include "lle/UResourceFile.h"
#include "lle/UICheckBox.h"
#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESEUnit.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EESMEffect.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETriggerScreen.h"
#include "lle/USingleton.h"
#include "ee/EEMedia.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMouse.h"
#include "ee/EEGraphics.h"
#include "ee/EEAScroll.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EESMArea.h"
#include "ee/EEUnit.h"
#include "ee/EESMObject.h"
#include "ee/EESEPlayerScreen.h"
#include "ee/EEMessage.h"
#include "ee/EEALasso.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EESMTrigger.h"
#include "ee/EESMCondition.h"
#include "ee/EEUIMiniMap.h"
#include "ee/EEScreenEvent.h"
#include "ee/EEUIFSEMainButton.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEStringIDs.h"
#include "ee/EESEARust.h"
#include "lle/UIFullBackground.h"
#include "ee/EEUIFSEUndo.h"
#include "ee/EEUIFSERedo.h"
#include "ee/EEGamePreferences.h"
#include "ee/EESEAChangeLoadText.h"
#include "ee/EESELoadThread.h"
#include "ee/EEUIFSEElevation.h"
#include "ee/EERenderObject.h"
#include "ee/EEROTerrain.h"
#include "ee/EEMiniMap.h"
#include "ee/EEAmbientSoundManager.h"
#include "ee/EEMusicManager.h"
#include "ee/EEUIFSEPaint.h"
#include "ee/EEAQuickSaveLoad.h"
#include "ee/EEUIFSEMap.h"
#include "ee/EEUIFSETerrain.h"
#include "ee/EESEInstructions.h"
#include "ee/EESEAQuickSaveLoad.h"
#include "rust/ee_screens.h"
extern char gEmptyString[];                 // 0x4e5b98 (pooled "")
extern unsigned long& gTexturePixelDepth;
extern GEFont* gU531930;                    // 0x531930 (Empire Earth.cpp's fonts)
extern GEFont* gU531934;
extern GEFont* gU531938;
extern RTSRenderObject* gU56534C;           // the game's base render object (EERenderObject.cpp)
extern RTSRenderObject* gU56535C;           // the terrain render object (an EEROTerrain newed in Empire Earth.cpp)
extern bool gSightUpdatesEnabled;
extern GEViewport gViewport;
void EEPostGraphicsAll();                   // EERenderObject.cpp: every render object
void EEPreviewPlacement(const GE2DPoint& inLocation);  // 0x204aba (EEGhostUnit.cpp: the ghost at the mouse)

// TEST ONLY (tools/sim_editor.py `trigreport`): the trigger-only save as its hot key (Ctrl+S) runs it
long EESETestSaveTriggersOnly(UIForm* inForm)
{
    UIEvent* theEvent = new EESEERust(EE_SCREENS_SEE_SAVE_TRIGGERS_ONLY);
    long theResult = theEvent->ProcessEvent(inForm, NULL);
    theEvent->Dereference();
    return theResult;
}

// Creates the editor form (id 1000, 15 control groups): button texture, edge scroll, mini map, player combo box,
// labels, tab groups, tool bar, hide-UI check box, camera and trigger save/load and debug hot keys; map tab on
// (editor_form.rs)
EEUIFScenarioEditor::EEUIFScenarioEditor(UIManager* inManager)
    : UIForm(1000, 0xf, inManager, -1, true)
{
    UWideString theText;
    this->SetRectangle(GERectangle(0.0f, 0.0f, 1023.0f, 767.0f));
    this->mRust = ee_screens_sefm_new(this);
    ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_BUILD, 0, NULL, NULL);
    this->mC1A0.SetPointsAndMax(gWorldTable.GetLongValue(kWorldCivilizationBuilderPoints));
    gResource->GetString(IDS_SE_MAP_NONE, theText);
    this->SetMusicFileName(theText);
}

// Destroys the editor form, releasing its events, textures and edge scroll and deleting its tabs and controls
// (editor_form.rs)
EEUIFScenarioEditor::~EEUIFScenarioEditor()
{
    ee_screens_sefm_free(this->mRust);
}

// Adds a unit to the editor selection, showing the cargo panel for a placed carrier and the status of a lone unit
void EEUIFScenarioEditor::SelectUnit(EEUnit* inUnit)
{
    ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_SELECT_UNIT, 0, inUnit, NULL);
}

// Closes the editor form, restoring the game camera mode, the game render list and the full-window mouse bounds
void EEUIFScenarioEditor::Close()
{
    gFreeCamera = false;
    ::gWorld.mCameraControl.SetMode(gGamePreferences.mSettings.mZoomStyle);
    ::gWorld.mCameraControl.SetChanged();
    ((UIFullBackground*)this->GetSlot(EE_SCREENS_SEFM_FULL_BACKGROUND))->UnloadBackground();
    gGraphics->PostClearRenderObjects();
    gGraphics->PostPushRenderObject(gU56534C);
    gMouse->SetBounds(GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight()));
    gB529065 = false;
    UIForm::Close();
}

// Prepares the frame: dirties the game view (unless full view) and mini map, moves the placement ghost, updates the
// current tab group and the edge scroll (unless full view or a control holds the mouse) (editor_form.rs)
long EEUIFScenarioEditor::CopyData()
{
    ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_COPY_DATA, 0, NULL, NULL);
    return UIForm::CopyData();
}

// Draws the full-view background or the bottom panel, then the current tab group; returns the full view's result
long EEUIFScenarioEditor::DrawBackground(const GERectangle& inRectangle)
{
    float theRectangle[4] = {inRectangle.GetLeft(), inRectangle.GetTop(), inRectangle.GetRight(), inRectangle.GetBottom()};
    return ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_DRAW_BACKGROUND, 0, NULL, theRectangle);
}

// Draws the form's polygons (no editor additions)
long EEUIFScenarioEditor::DrawPolygons()
{
    return UIForm::DrawPolygons();
}

// Leaves the current tab: hides the brush highlight, leaves its group (unless switching), disables its controls
void EEUIFScenarioEditor::LeaveTab()
{
    ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_LEAVE_TAB, 0, NULL, NULL);
}

// Switches to a tab: enters or leaves full view as needed, enables its controls, enters and loads its group
void EEUIFScenarioEditor::SelectTab(unsigned char inFlag, long inTab)
{
    ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_SELECT_TAB, (int32_t)(inFlag | (inTab << 8)), NULL, NULL);
}

// Leaves the editor through the load thread, to test the scenario or to quit, back on the map tab; suspends the form
void EEUIFScenarioEditor::ExitEditor(bool inTestScenario)
{
    ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_EXIT_EDITOR, inTestScenario ? 1 : 0, NULL, NULL);
}

// Resumes the editor after a load and reloads the current tab group
void EEUIFScenarioEditor::ResumeCurrentGroup()
{
    ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_RESUME_GROUP, 0, NULL, NULL);
}

// Suspends the editor and passes an event to the load thread, which posts it to the user interface
void EEUIFScenarioEditor::PostLoadEvent(UReferenceCounter* inObject)
{
    ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_POST_LOAD_EVENT, 0, inObject, NULL);
}

// Opens the editor: editor camera, backgrounds, paused music and ambient sound, editor game view and mini map,
// enters the current tab; posts the exit action if a save asked to exit afterwards (editor_form.rs)
void EEUIFScenarioEditor::Open()
{
    ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_OPEN, 0, NULL, NULL);
}

// Suspends the editor and passes a loaded object to the load thread to show
void EEUIFScenarioEditor::PostShowLoadedObject(UReferenceCounter* inObject)
{
    ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_POST_SHOW_LOADED, 0, inObject, NULL);
}

// Deselects every selected unit and hides the cargo panel and the unit status
void EEUIFScenarioEditor::ClearSelection()
{
    ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_CLEAR_SELECTION, 0, NULL, NULL);
}

// Removes the first unit from the editor selection, updating the cargo panel and the unit status
void EEUIFScenarioEditor::DeselectFirstUnit()
{
    ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_DESELECT_FIRST, 0, NULL, NULL);
}

// Removes a unit from the editor selection, updating the cargo panel, and the unit status if it was first
void EEUIFScenarioEditor::DeselectUnit(EEUnit* inUnit)
{
    ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_DESELECT_UNIT, 0, inUnit, NULL);
}

// Makes the current tab group store its settings
void EEUIFScenarioEditor::RefreshCurrentGroup()
{
    ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_STORE_GROUP, 0, NULL, NULL);
}

// Sets the editor's game view rectangle and disables the current tab group while the UI is hidden
void EEUIFScenarioEditor::SetGameView(const GERectangle& inRectangle, bool inIsUIHidden)
{
    float theRectangle[4] = {inRectangle.GetLeft(), inRectangle.GetTop(), inRectangle.GetRight(), inRectangle.GetBottom()};
    ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_SET_GAME_VIEW, inIsUIHidden ? 1 : 0, NULL, theRectangle);
}

// Shows the given loading text in the top label, or hides the label for an empty text
void EEUIFScenarioEditor::SetLoadText(const UWideString& inText)
{
    ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_SET_LOAD_TEXT, inText.GetLength() > 0 ? 1 : 0,
                         const_cast<UWideString*>(&inText), NULL);
}

// Enters full view: resets the game render list to its base object, hides the mini map and redraws all controls
void EEUIFScenarioEditor::EnterFullView()
{
    ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_ENTER_FULL_VIEW, 0, NULL, NULL);
}

// Leaves full view: restores the full game render list, shows the mini map and redraws all controls
void EEUIFScenarioEditor::LeaveFullView()
{
    ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_LEAVE_FULL_VIEW, 0, NULL, NULL);
}

// Handles the editor keys (; quick save, / quick load, T / Shift+T grid mode, 1-8 and Alt+1-8 pick a player),
// else the tab group's and the form's keys; nothing while suspended (editor_form.rs)
long EEUIFScenarioEditor::UpdateKeyboard()
{
    if (ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_UPDATE_KEYBOARD, 0, NULL, NULL) == 0)
        return 0;
    return UIForm::UpdateKeyboard();
}

// Passes the mouse to the current tab group unless a control captured it; does nothing while suspended
long EEUIFScenarioEditor::UpdateMouse()
{
    if (ee_screens_sefm_call(this->mRust, EE_SCREENS_SEFM_UPDATE_MOUSE, 0, NULL, NULL) == 0)
        return 0;
    return UIForm::UpdateMouse();
}

void EEUIFScenarioEditor::FnE123D()
{
}

// ---- the form's host (editor_form.rs: the ops from 800)

static_assert(EEUIFScenarioEditor::kSlotButtonTexture == EE_SCREENS_SEFM_BUTTON_TEXTURE &&
                  EEUIFScenarioEditor::kSlotEdgeScroll == EE_SCREENS_SEFM_EDGE_SCROLL &&
                  EEUIFScenarioEditor::kSlotPanelBackground == EE_SCREENS_SEFM_PANEL_BACKGROUND &&
                  EEUIFScenarioEditor::kSlotFullBackground == EE_SCREENS_SEFM_FULL_BACKGROUND &&
                  EEUIFScenarioEditor::kSlotPlayerComboBox == EE_SCREENS_SEFM_PLAYER_COMBO &&
                  EEUIFScenarioEditor::kSlotBrushGroup == EE_SCREENS_SEFM_BRUSH_GROUP &&
                  EEUIFScenarioEditor::kSlotGroup == EE_SCREENS_SEFM_GROUP &&
                  EEUIFScenarioEditor::kSlotTabButton == EE_SCREENS_SEFM_TAB_BUTTON &&
                  EEUIFScenarioEditor::kSlotQuickSaveButton == EE_SCREENS_SEFM_QUICK_SAVE_BUTTON &&
                  EEUIFScenarioEditor::kSlotLoadThread == EE_SCREENS_SEFM_LOAD_THREAD &&
                  EEUIFScenarioEditor::kSlotUIHidden == EE_SCREENS_SEFM_UI_HIDDEN &&
                  EEUIFScenarioEditor::kSlotWasUIHidden == EE_SCREENS_SEFM_WAS_UI_HIDDEN &&
                  EEUIFScenarioEditor::kSlotExitAfterSave == EE_SCREENS_SEFM_EXIT_AFTER_SAVE,
              "editor_form.rs's slots");
static_assert(kSMConditionObject == 1 && kSMConditionPlayerAttribute == 2 && kSMEffectNone == 0 &&
                  kSMEffectObject == 1 && kSMEffectPlayer == 2 && kSMObjectActionClassName == 10 &&
                  kSMObjectActionClassAttribute == 12 && kSMPlayerActionModifyTechTree == 5 &&
                  kSMTriggerOperatorAnd == 1 && kSMTriggerOperatorOr == 2 && kSEAExit == 3 && kSEEHideUI == 1 &&
                  kSEEQuickLoad == 3 && kSEEQuickSave == 4 && kToolbarButtonExit == 15 &&
                  IDS_CIVILIZATION_BUILDER_TOOL == EE_SCREENS_SEFM_IDS_CIVILIZATION_BUILDER_TOOL,
              "editor_form.rs's numbers");

namespace {

// the keys the form reads or binds (editor_form.rs EE_SCREENS_SEFM_KEY_*)
const unsigned long sSefmKeys[EE_SCREENS_SEFM_KEY_COUNT] = {
    kKeySemicolon, kKeySlash, kKeyT, kKeyShift, kKeyAlt, kKey1, kKey2, kKey3, kKey4, kKey5, kKey6, kKey7, kKey8,
    kKeyRBracket, kKeyLBracket, kKeyGrave, kKeyHome, kKeyS, kKeyL, kKeyTab, kKeyD, kKeyControl, kKeyNone};

GETextureDescription SefmDescription(unsigned long inPixelDepth, bool inHasAlpha)
{
    GETextureDescription theDescription;
    theDescription.mPixelDepth = inPixelDepth;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = inHasAlpha;
    theDescription.mCreateMIPMaps = true;
    return theDescription;
}

void SefmColor(GEText* inText, const GEColor& inColor)
{
    inText->SetFgColor(inColor);
    inText->SetIsTransparent(true);
}

const GEColor kSefmYellow(0.99607843f, 0.96862745f, 0.1254902f);

// a scenario record's name or description into the caller's buffer: its length
int32_t SefmText(const UWideString& inText, void* outBuffer, int32_t inCapacity)
{
    int32_t theLength = (int32_t)inText.GetTCHARLength();
    memcpy(outBuffer, (const char*)inText, theLength < inCapacity ? theLength : inCapacity);
    return theLength;
}

}   // namespace

extern "C" int32_t ee_screenscpp_sefm(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    return EEUIFScenarioEditor::Host(inOp, p, i, f);
}

int32_t EEUIFScenarioEditor::Host(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    EEUIFScenarioEditor* theForm = static_cast<EEUIFScenarioEditor*>(p[1]);
    void** theOut = static_cast<void**>(p[0]);
    UWideString theText;
    switch (inOp) {
    case EE_SCREENS_SEFM_OP_BASICS: {   // the ctor's texture, scroll and poll, then CreateBackground
        GETextureDescription theDescription = SefmDescription(gTexturePixelDepth, true);
        GETexture* theTexture = gRasterizer->GetTextureManager(theForm->GetManager()->GetScreen())
                                    ->LoadTexture("textures\\SE Buttons", theDescription);
        theTexture->Reference();
        theOut[0] = theTexture;
        theOut[1] = new EEAScroll;
        TMPoll* thePoll = new TMPoll(500, 0, false);
        thePoll->Start();
        theOut[2] = thePoll;
        theDescription = SefmDescription(16, false);
        GERasterizer* theRasterizer = GERasterizer::GetActiveRasterizer();
        GEScreen* theScreen = NULL;
        for (unsigned long k = 0; k < theRasterizer->GetScreenCount(); k++) {
            if (theRasterizer->GetIsScreenActivated(k)) {
                theScreen = theRasterizer->GetScreen(k);
                break;
            }
        }
        GETextureManager* theManager = theScreen->GetTextureManager();
        theTexture = theManager->LoadTexture("textures\\scenpart", theDescription);
        theTexture->SetFrame(0);
        theTexture->SetStageAlphaMode(kStageOpTexture, -1);
        theTexture->SetStageColorMode(kStageOpTexture, -1);
        theTexture->SetStageMinFilterMode(0, -1);
        theTexture->SetStageMagFilterMode(0, -1);
        theTexture->SetStageMipFilterMode(kMipFilterNone, -1);
        theTexture->SetStageMipMapBias(-1.0f, -1);
        theTexture->SetStageTextureAddressingMode(kAddressingWrap, -1);
        GEBitmap* thePanel = new GEBitmap;
        thePanel->SetTexture(theTexture, gScreenIndex);
        theOut[3] = thePanel;
        theOut[4] = new UIFullBackground("textures\\Scenfull", 4, 3, theForm, 1024, 768);
        return 0;
    }
    case EE_SCREENS_SEFM_OP_MINI_MAP: {
        EEUIMiniMap* t = new EEUIMiniMap(GERectangle(GE2DPoint(f[0], f[1]), f[2], f[2]), theForm, (unsigned long)i[0], 8, 3);
        theForm->AddControl(t, 0);
        t->SetDisplayMode(kDisplayModeHidden);
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SEFM_OP_PLAYER_COMBO: {
        UIComboBox* t = new UIComboBox(0x149, 1, std::vector<UWideString>(), gU531938,
                                       GERectangle(GE2DPoint(0.0f, 0.0f), 140.0f, 20.0f), 400, theForm,
                                       (unsigned long)i[0], 0, 2);
        t->SetTextBoxBgOpacity(0.4f);
        SefmColor(t->GetGEText(), kSefmYellow);
        t->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
        t->SetBgOpacity(1.0f);
        theForm->AddControl(t, 0);
        t->SetDisplayMode(kDisplayModeHidden);
        UIEvent* theEvent = EECreateScreenEvent(EE_SCREENS_SE_NEW_PLAYER);
        t->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
        *static_cast<void**>(p[2]) = theEvent;
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SEFM_OP_LABEL: {
        UITextLabel* t;
        if (i[0] == 0) {   // the top text label
            t = new UITextLabel(UWideString(gEmptyString), gU531930, 9,
                                GERectangle(GE2DPoint(500.0f, 0.0f), 300.0f, (float)gU531930->GetHeight()), theForm,
                                0, (unsigned long)i[1], 0);
            t->SetAlignment(kTextAlignRight);
            t->SetBgOpacity(0.4f);
            SefmColor(t->GetGEText(), kSefmYellow);
        } else {   // the help label, the form's tool tips
            t = new UITextLabel(UWideString(gEmptyString), gU531934, 0x10,
                                GERectangle(GE2DPoint(0.0f, 0.0f), 400.0f, 400.0f), theForm, 1, (unsigned long)i[1], 0);
            t->SetAlignment(kTextAlignLeft);
            t->SetBgOpacity(1.0f);
            t->SetBgColor(GEColor());
            theForm->SetUseToolTips(true);
            theForm->SetHelpTextTimeDelay(600);
            SefmColor(t->GetGEText(), GEColor(0.0f, 0.0f, 0.0f));
        }
        theForm->AddControl(t, 0);
        t->SetDisplayMode(kDisplayModeHidden);
        t->SetAlwaysDirty(true);
        t->SetFitRectToText(true);
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SEFM_OP_BRUSH_GROUP: {
        unsigned long theIndex = *static_cast<uint32_t*>(p[2]);
        *theOut = new EEUIFSEBrushGroup(theIndex, GE2DPoint(f[0], f[1]), theForm, gU531930);
        *static_cast<uint32_t*>(p[2]) = (uint32_t)theIndex;
        return 0;
    }
    case EE_SCREENS_SEFM_OP_UNDO_REDO: {
        UIButton* t;
        if (i[0] == 0) {
            t = new EEUIFSEUndo(0, 0, 0, 0, GE2DPoint(f[0], f[1]), 65, 25, theForm, (unsigned long)i[1]);
            gResource->GetString(IDS_UNDO, theText);
        } else {
            t = new EEUIFSERedo(1, 0, 0, 0, GE2DPoint(f[0], f[1]), 65, 25, theForm, (unsigned long)i[1]);
            gResource->GetString(IDS_REDO, theText);
        }
        t->SetText(theText, gU531930);
        SefmColor(t->GetGEText(), GEColor(1.0f, 1.0f, 1.0f));
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SEFM_OP_TAB_GROUP: {   // CreateTabGroup
        unsigned long theIndex = *static_cast<uint32_t*>(p[2]);
        EEUIFSEMainGroup* t = NULL;
        switch (i[0]) {
        case kEditorTabMap: t = new EEUIFSEMap(theIndex, theForm, gU531930); break;
        case kEditorTabElevation:
            t = new EEUIFSEElevation(theForm->GetEdgeScroll(), theForm->GetBrushGroup(), theIndex, theForm, gU531930);
            break;
        case kEditorTabTerrain:
            t = new EEUIFSETerrain(theForm->GetEdgeScroll(), theForm->GetBrushGroup(), theIndex, theForm, gU531930);
            break;
        case kEditorTabUnits: t = new EESEUnit(theIndex, theForm, gU531938, theForm->GetPlayerComboBox()); break;
        case kEditorTabTriggers: t = new EESETriggerScreen(theIndex, theForm, gU531938); break;
        case kEditorTabPlayers: t = new EESEPlayerScreen(theIndex, theForm, gU531938, theForm->GetPlayerComboBox()); break;
        case kEditorTabInstructions: t = new EESEInstructions(theIndex, theForm, gU531938); break;
        }
        *static_cast<uint32_t*>(p[2]) = (uint32_t)theIndex;
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SEFM_OP_BUTTON: {
        const EeScreensSefmButton& b = *static_cast<const EeScreensSefmButton*>(p[2]);
        UIButton* t;
        if (b.kind == 0) {
            t = new EEUIFSEMainButton(b.id, (unsigned char)b.flag, theForm->GetButtonTexture(),
                                      GE2DPoint(b.normal[0], b.normal[1]), GE2DPoint(b.pressed[0], b.pressed[1]),
                                      GE2DPoint(b.rect[0], b.rect[1]), theForm, b.tab, b.index, b.checked != 0);
            t->SetStringTableID(b.help);
            theForm->AddControl(t, 0);
            *theOut = t;
            return 0;
        }
        unsigned long theScreen = theForm->GetManager()->GetScreen();
        GERectangle theDestination(b.dest[0], b.dest[1], b.dest[2], b.dest[3]);
        GEBitmap* theNormal = new GEBitmap;
        GEBitmap* thePressed = new GEBitmap;
        theNormal->SetTexture(theForm->GetButtonTexture(), theScreen);
        thePressed->SetTexture(theForm->GetButtonTexture(), theScreen);
        theNormal->ComputeStretchedCoordsAndScale(theDestination, theDestination,
            GERectangle(b.normal[0], b.normal[1], b.normal[0] + 32.0f, b.normal[1] + 32.0f), theScreen);
        thePressed->ComputeStretchedCoordsAndScale(theDestination, theDestination,
            GERectangle(b.pressed[0], b.pressed[1], b.pressed[0] + 32.0f, b.pressed[1] + 32.0f), theScreen);
        t = new UIButton(b.id, theNormal, NULL, thePressed, GERectangle(b.rect[0], b.rect[1], b.rect[2], b.rect[3]),
                         theForm, b.index, 0);
        t->SetStringTableID(b.help);
        theForm->AddControl(t, 0);
        UIEvent* theEvent = b.object != 0 ? new EESEERust(b.event, (void*)b.object) : EECreateScreenEvent(b.event);
        t->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
        theEvent->Dereference();
        theNormal->Dereference();
        thePressed->Dereference();
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SEFM_OP_QUICK_ACTION:
        *theOut = static_cast<EEAction*>(new EESEAQuickSaveLoad(gScenarioExtension));
        return 0;
    case EE_SCREENS_SEFM_OP_HIDE_UI: {
        UICheckBox* t = new UICheckBox(kScenarioEditorHideUICheckBox, UWideString(gEmptyString), gU531934,
                                       GERectangle(GE2DPoint(849.0f, 718.0f), 20.0f, 20.0f), theForm,
                                       (unsigned long)i[0], 0, false);
        t->SetStringTableID(IDS_HIDE_UI);
        t->SetChecked(true);
        EESEERust* theHideUI = new EESEERust(kSEEHideUI);
        t->SetTriggerEvent(kControlTriggerChecked, theHideUI);
        t->SetTriggerEvent(kControlTriggerUnchecked, theHideUI);
        theHideUI->Dereference();
        theForm->AddControl(t, 0);
        t->SetDisplayMode(kDisplayModeHidden);
        *theOut = t;
        return 0;
    }
    case EE_SCREENS_SEFM_OP_GROUP_ENABLE:
        if (i[1])
            theForm->GetEntityGroup((unsigned long)i[0])->Enable();
        else
            theForm->GetEntityGroup((unsigned long)i[0])->Disable();
        return 0;
    case EE_SCREENS_SEFM_OP_SCREEN_EVENT: *theOut = EECreateScreenEvent((unsigned long)i[0]); return 0;
    case EE_SCREENS_SEFM_OP_SEE_EVENT: *theOut = new EESEERust((uint32_t)i[0], p[2]); return 0;
    case EE_SCREENS_SEFM_OP_MOUSE_EVENT: {
        UIEvent* theEvent = static_cast<UIEvent*>(p[0]);
        if (i[0] == 0)
            theForm->SetMouseWheelMoveEvent(theEvent);
        else {
            theForm->SetMouseMiddleButtonPressedEvent(theEvent);
            theForm->SetMouseMiddleButtonUnpressedEvent(theEvent);
        }
        return 0;
    }
    case EE_SCREENS_SEFM_OP_HOT_KEY: {
        UIEvent* const* theEvents = static_cast<UIEvent* const*>(p[0]);
        UIHotKey theHotKey;
        theHotKey.SetKey(sSefmKeys[i[0]]);
        theHotKey.SetJustPressedEvent(theEvents[0]);
        theHotKey.SetPressedEvent(theEvents[1]);
        theHotKey.SetJustUnpressedEvent(theEvents[2]);
        theForm->AddHotKey(theHotKey, 0);
        return 0;
    }
    case EE_SCREENS_SEFM_OP_ADD_HOT_KEY: {
        UIEvent* theEvents[4] = {NULL, NULL, NULL, NULL};
        theEvents[i[3] & 3] = static_cast<UIEvent*>(p[0]);
        theForm->AddHotKey(sSefmKeys[i[0]], sSefmKeys[i[1]], sSefmKeys[i[2]], kKeyNone, theEvents[0], theEvents[1],
                           theEvents[2], theEvents[3], (unsigned long)(i[3] >> 4), false);
        return 0;
    }
    case EE_SCREENS_SEFM_OP_GRAB_SCROLL: *theOut = EECreateGrabScroll(1); return 0;
    case EE_SCREENS_SEFM_OP_RELEASE_GRAB_SCROLL: EEReleaseGrabScroll(1); return 0;
    case EE_SCREENS_SEFM_OP_TAB_CHECK:
        if (i[0])
            static_cast<EEUIFSEMainButton*>(p[0])->CheckDirect();
        else
            static_cast<EEUIFSEMainButton*>(p[0])->Check();
        return 0;
    case EE_SCREENS_SEFM_OP_DEREFERENCE:
        switch (i[0]) {
        case 0: static_cast<UIEvent*>(p[0])->Dereference(); break;
        case 1: static_cast<GETexture*>(p[0])->Dereference(); break;
        case 2: static_cast<GEBitmap*>(p[0])->Dereference(); break;
        case 3: static_cast<EEAction*>(p[0])->Dereference(); break;
        case 4: static_cast<EEAScroll*>(p[0])->Dereference(); break;
        }
        return 0;
    case EE_SCREENS_SEFM_OP_DELETE:
        switch (i[0]) {
        case 0: delete static_cast<UIControl*>(p[0]); break;
        case 1: delete static_cast<EEUIFSEMainGroup*>(p[0]); break;
        case 2: delete static_cast<TMPoll*>(p[0]); break;
        case 3: delete static_cast<EEUIFSEBrushGroup*>(p[0]); break;
        case 4: delete static_cast<UIFullBackground*>(p[0]); break;
        }
        return 0;
    case EE_SCREENS_SEFM_OP_GROUP_CALL: {
        EEUIFSEMainGroup* g = static_cast<EEUIFSEMainGroup*>(p[0]);
        switch (i[0]) {
        case EE_SCREENS_SEFM_GROUP_UPDATE: g->Update(); break;
        case EE_SCREENS_SEFM_GROUP_LEAVE: g->Leave(); break;
        case EE_SCREENS_SEFM_GROUP_DRAW: g->Draw(GERectangle(f[0], f[1], f[2], f[3])); break;
        case EE_SCREENS_SEFM_GROUP_DISABLE: g->Disable(); break;
        case EE_SCREENS_SEFM_GROUP_LOAD: g->Load(); break;
        case EE_SCREENS_SEFM_GROUP_ENTER: g->Enter(); break;
        case EE_SCREENS_SEFM_GROUP_STORE: g->Store(); break;
        case EE_SCREENS_SEFM_GROUP_ENABLE: g->Enable(); break;
        case EE_SCREENS_SEFM_GROUP_MOUSE_MOVE: g->MouseMove(GE2DPoint(f[0], f[1])); break;
        case EE_SCREENS_SEFM_GROUP_UPDATE_KEYS: g->UpdateKeys(); break;
        }
        return 0;
    }
    case EE_SCREENS_SEFM_OP_RENDER_LIST:
        if (i[0]) {
            gGraphics->PostClearRenderObjects();
            gGraphics->PostPushRenderObject(gU56534C);
        } else
            EEPostGraphicsAll();
        return 0;
    case EE_SCREENS_SEFM_OP_REDRAW_ALL:
        gUIManager->RedrawAll();
        gUIManager->GetCurrentForm()->SetAllControlsDirty();
        return 0;
    case EE_SCREENS_SEFM_OP_HIGHLIGHT: static_cast<EEUIFSEBrushGroup*>(p[0])->mIsHighlightShown = i[0] != 0; return 0;
    case EE_SCREENS_SEFM_OP_RESOLVE: {
        EEUnit* theUnit;
        EEUnitHandleResolve((uint32_t)i[0], theUnit);
        *theOut = theUnit;
        return 0;
    }
    case EE_SCREENS_SEFM_OP_UNIT_ID: return (int32_t)EEUnitHandle<EEUnit>(static_cast<EEUnit*>(p[0])).GetSavedID();
    case EE_SCREENS_SEFM_OP_UNIT_SELECT: {
        EEUnit* theUnit = static_cast<EEUnit*>(p[0]);
        if (theUnit == NULL)
            return 0;
        theUnit->mIsEditorSelected = i[0] != 0;
        if (i[1] > 0)
            theUnit->Reference();
        else if (i[1] < 0)
            theUnit->Dereference();
        return 0;
    }
    case EE_SCREENS_SEFM_OP_UNIT_CARGO: {
        EEUnit* theUnit = static_cast<EEUnit*>(p[0]);
        return theUnit->GetTileNode() != NULL && (theUnit->GetComplexUnit()->mCargoRoom > 0 ||
                                                  theUnit->GetComplexUnit()->GetNumCargoUnits() > 0);
    }
    case EE_SCREENS_SEFM_OP_SHOW_CARGO: static_cast<EESEUnit*>(p[0])->ShowCargo(i[0] != 0); return 0;
    case EE_SCREENS_SEFM_OP_SHOW_STATUS: static_cast<EESEUnit*>(p[0])->ShowUnitStatus(static_cast<EEUnit*>(p[1])); return 0;
    case EE_SCREENS_SEFM_OP_GAME_VIEW: EESetGameViewRectangle(GERectangle(f[0], f[1], f[2], f[3])); return 0;
    case EE_SCREENS_SEFM_OP_LOAD_THREAD: {
        EESELoadThread* theThread = static_cast<EESELoadThread*>(p[1]);
        switch (i[0]) {
        case 0: *theOut = new EESELoadThread(static_cast<EEUIFScenarioEditor*>(p[2]), 0); break;
        case EE_SCREENS_SEFM_POST_401: theThread->Post401(); break;
        case EE_SCREENS_SEFM_POST_404: theThread->Post404(); break;
        case EE_SCREENS_SEFM_POST_405: theThread->Post405(); break;
        case EE_SCREENS_SEFM_POST_40B: theThread->Post40B(static_cast<UReferenceCounter*>(p[2])); break;
        case EE_SCREENS_SEFM_POST_40C: theThread->Post40C(static_cast<UReferenceCounter*>(p[2])); break;
        }
        return 0;
    }
    case EE_SCREENS_SEFM_OP_TEST_PREPARE:
        gWorld.RebuildActiveUnits();
        return gUIManager->mInScenarioEditor == true;
    case EE_SCREENS_SEFM_OP_OPEN_CAMERA: {
        GETransformation theTransformation;
        gWorld.mCameraControl.SetMode(2);
        gWorld.RefreshUnits();
        gSightUpdatesEnabled = false;
        theTransformation.SetOrientationYPR(0.7853981852531433f, -0.6108652353286743f, 0.0f);
        gWorld.mCameraControl.SetOffsetTurn(theTransformation, false);
        gWorld.mCameraControl.SetChanged();
        return 0;
    }
    case EE_SCREENS_SEFM_OP_LOAD_BACKGROUND:
        static_cast<UIFullBackground*>(p[0])->LoadBackground(SefmDescription(gTexturePixelDepth, true));
        return 0;
    case EE_SCREENS_SEFM_OP_OPEN_WORLD: {
        gGraphics->PostFadeIn(0);
        static_cast<EESEPlayerScreen*>(p[0])->Refresh();
        gMusicManager->Pause();
        gAmbientSoundManager.StopSounds();
        gAmbientSoundManager.Pause();
        static_cast<EEAScroll*>(p[1])->mEdges =
            GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth() - 1.0f, (float)gGameWindow->GetHeight() - 1.0f);
        gFreeCamera = true;
        gB529065 = true;
        const GERectangle& theView = gViewport.GetViewRectangle();
        f[0] = theView.GetLeft();
        f[1] = theView.GetTop();
        f[2] = theView.GetRight();
        f[3] = theView.GetBottom();
        EESetGameViewRectangle(GERectangle(0.0f, 0.0f, 1024.0f, 606.0f));
        gB529066 = false;
        gCameraFollowing = false;
        gB529068 = false;
        if (gMiniMap != NULL) gMiniMap->RefreshView();
        EEUIMiniMap::Update(gMiniMap);
        return 0;
    }
    case EE_SCREENS_SEFM_OP_OPEN_FORM:
        gWorld.mScenarioManager->mIsMovingCamera = false;
        gMiniMap->Redraw();
        theForm->UIForm::Open();
        return 0;
    case EE_SCREENS_SEFM_OP_HELP_LABEL: theForm->SetHelpTextLabel(static_cast<UITextLabel*>(p[0])); return 0;
    case EE_SCREENS_SEFM_OP_POST_EXIT: {
        EESEARust* theAction = new EESEARust(kSEAExit);
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
        return 0;
    }
    case EE_SCREENS_SEFM_OP_COPY_WORLD:
        if (i[0] == 2) {
            EEPreviewPlacement(gMouse->mPosition);
            return 0;
        }
        if (i[0] == 1) {
            GERectangle theRectangle = gViewport.GetViewRectangle();
            theRectangle.SetRight(theRectangle.GetRight() - 1.0f);
            theRectangle.SetBottom(theRectangle.GetBottom() - 1.0f);
            theForm->GetManager()->AddDirtyRectangle(theRectangle, theForm);
        }
        static_cast<UIControl*>(p[0])->SetDirty(true);
        return 0;
    case EE_SCREENS_SEFM_OP_CAPTURED: return i[0] ? theForm->GetIsKeyboardCaptured() : theForm->GetIsMouseCaptured();
    case EE_SCREENS_SEFM_OP_EDGE_SCROLL: static_cast<EEAScroll*>(p[0])->Update(gMouse->mPosition); return 0;
    case EE_SCREENS_SEFM_OP_DRAW_FULL:
        return (int32_t) static_cast<UIFullBackground*>(p[0])->DrawBackground(GERectangle(f[0], f[1], f[2], f[3]));
    case EE_SCREENS_SEFM_OP_DRAW_PANEL: {
        const float* r = static_cast<const float*>(p[2]);
        theForm->UIForm::DrawBackground(static_cast<GEBitmap*>(p[0]), GERectangle(f[0], f[1], f[2], f[3]),
                                        GERectangle(0.0f, 0.0f, 256.0f, 256.0f), GERectangle(r[0], r[1], r[2], r[3]));
        return 0;
    }
    case EE_SCREENS_SEFM_OP_KEY: {
        ISEntity* theKey;
        gISKeyboard->GetEntity(sSefmKeys[i[0]], theKey);
        return (theKey->JustPressed() ? 1 : 0) | (theKey->GetValue() != 0 ? 2 : 0);
    }
    case EE_SCREENS_SEFM_OP_FIRE: {
        UIControl* theControl = theForm->GetControlById((unsigned long)i[0]);
        if (theControl != NULL) theControl->FireTriggerEvent(kControlTriggerButtonUp);
        return 0;
    }
    case EE_SCREENS_SEFM_OP_GRID: static_cast<EEROTerrain*>(gU56535C)->StepGridMode(i[0] != 0); return 0;
    case EE_SCREENS_SEFM_OP_PLAYER_COUNT: return (int32_t) static_cast<EESEPlayerScreen*>(p[0])->GetPlayerCount();
    case EE_SCREENS_SEFM_OP_COMBO_SELECT: static_cast<UIComboBox*>(p[0])->SetCurrentSelection((unsigned long)i[0]); return 0;
    case EE_SCREENS_SEFM_OP_LABEL_TEXT: static_cast<UITextLabel*>(p[0])->SetText(*static_cast<const UWideString*>(p[1])); return 0;
    case EE_SCREENS_SEFM_OP_MOUSE: {
        const GE2DPoint& theMouse = theForm->GetMouseLocation();
        f[0] = theMouse.GetX();
        f[1] = theMouse.GetY();
        return 0;
    }
    }
    // the trigger-only save's records (the scenario manager's lists)
    EEScenarioManager* m = gWorld.mScenarioManager;
    unsigned long k = (unsigned long)i[1];
    switch (inOp) {
    case EE_SCREENS_SEFM_OP_SM_COUNT:
        switch (i[0]) {
        case EE_SCREENS_SEFM_LIST_TRIGGERS: return (int32_t)m->mTriggers.size();
        case EE_SCREENS_SEFM_LIST_CONDITIONS: return (int32_t)m->mConditions.size();
        case EE_SCREENS_SEFM_LIST_EFFECTS: return (int32_t)m->mEffects.size();
        case EE_SCREENS_SEFM_LIST_OBJECTS: return (int32_t)m->mObjects.size();
        }
        return (int32_t)m->mAreas.size();
    case EE_SCREENS_SEFM_OP_SM_RECORD:
        switch (i[0]) {
        case EE_SCREENS_SEFM_LIST_TRIGGERS:
            *theOut = k < m->mTriggers.size() ? static_cast<EeScenarioTriggerFields*>(m->GetTrigger(k)) : NULL;
            return 0;
        case EE_SCREENS_SEFM_LIST_CONDITIONS: *theOut = static_cast<EeScenarioConditionFields*>(m->GetCondition(k)); return 0;
        case EE_SCREENS_SEFM_LIST_EFFECTS: *theOut = static_cast<EeScenarioEffectFields*>(m->GetEffect(k)); return 0;
        case EE_SCREENS_SEFM_LIST_OBJECTS: *theOut = static_cast<EeScenarioObjectFields*>(m->GetScenarioObject(k)); return 0;
        }
        *theOut = NULL;
        return 0;
    case EE_SCREENS_SEFM_OP_SM_TEXT:
        switch (i[0]) {
        case EE_SCREENS_SEFM_LIST_TRIGGERS:
            if (k >= m->mTriggers.size()) return -1;
            return SefmText(i[2] ? m->GetTrigger(k)->mText : m->GetTrigger(k)->mName, p[0], i[3]);
        case EE_SCREENS_SEFM_LIST_CONDITIONS:
            return m->GetCondition(k) == NULL ? -1 : SefmText(m->GetCondition(k)->mName, p[0], i[3]);
        case EE_SCREENS_SEFM_LIST_EFFECTS: return m->GetEffect(k) == NULL ? -1 : SefmText(m->GetEffect(k)->mName, p[0], i[3]);
        case EE_SCREENS_SEFM_LIST_OBJECTS:
            return m->GetScenarioObject(k) == NULL ? -1 : SefmText(m->GetScenarioObject(k)->mName, p[0], i[3]);
        }
        return m->GetArea(k) == NULL ? -1 : SefmText(m->GetArea(k)->mName, p[0], i[3]);
    case EE_SCREENS_SEFM_OP_SM_VEC_LEN:
        if (i[0] == EE_SCREENS_SEFM_VEC_OBJECTS)
            return m->GetEffect(k) == NULL ? 0 : (int32_t)m->GetEffect(k)->mObjects.size();
        if (k >= m->mTriggers.size()) return 0;
        return (int32_t)(i[0] == EE_SCREENS_SEFM_VEC_CONDITIONS ? m->GetTrigger(k)->mConditions.size()
                                                                 : m->GetTrigger(k)->mEffects.size());
    case EE_SCREENS_SEFM_OP_SM_VEC_AT: {
        unsigned long n = (unsigned long)i[2];
        if (i[0] == EE_SCREENS_SEFM_VEC_OBJECTS) {
            i[0] = (int32_t)m->GetEffect(k)->mObjects[n];
        } else if (i[0] == EE_SCREENS_SEFM_VEC_CONDITIONS) {
            EESMTConditionWrapper* w = m->GetTrigger(k)->GetCondition(n);
            i[0] = w->mIsNot ? 1 : 0;
            i[1] = (int32_t)w->mValue;
            i[2] = (int32_t)w->mOperator;
        } else {
            EESMTEffectWrapper* w = m->GetTrigger(k)->GetEffect(n);
            i[0] = (int32_t)w->mDelay;
            i[1] = (int32_t)w->mEffectIndex;
            i[2] = (int32_t)w->mOperator;
        }
        return 0;
    }
    case EE_SCREENS_SEFM_OP_REPORT_SAVE: {
        FSFileSpec theFile;
        theFile.SetVolume(gStartupVolume);
        theFile.SetDirectory(gStartupDirectory);
        theFile.SetFile("ScenarioTriggers.scn");
        theFile.Create();
        long theResult = theFile.Open(kFileAccessWrite);
        if (theResult == kErrorOK)
            m->Save(kSaveGameMajorVersion, kSaveGameMinorVersion, theFile);
        theFile.Close();
        return (int32_t)theResult;
    }
    case EE_SCREENS_SEFM_OP_REPORT_WRITE: {
        UString thePath(260);
        thePath = gStartupVolume + gStartupDirectory + "ScenarioTriggers.txt";
        FILE* theText = PlatFOpen(thePath, "w");
        if (theText) {
            fwrite(p[0], 1, (size_t)i[0], theText);
            fclose(theText);
        }
        return 0;
    }
    }
    return 0;
}
