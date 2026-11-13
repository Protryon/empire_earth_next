// C:\EEX\Empire Earth\Scenario Editor\EEUIFSETRBPaintType.cpp (file name inferred: no __FILE__). 0x1ac48b..0x1afa90,
// with the functions of EEUIESLLoadScreen.cpp folded in (the editor's tool bar events, EESMDialogMessage's
// Load/Save; the load screen event is a screen code, ee_screens save_load.rs). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GEVertexArray.h"
#include "lle/GERhombus.h"
#include "lle/ISEntity.h"
#include "lle/TSCriticalSection.h"
#include "lle/UILayoutUtility.h"
#include "lle/UIEntity.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIEvent.h"
#include "lle/UString.h"

#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EESEARust.h"
#include "lle/TSEvent.h"
#include "lle/TSThread.h"
#include "lle/UResourceFile.h"
#include "ee/EEUserInterface.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DPlane.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/GEViewport.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GERasterizer.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"

// These are genuine original RTTI classes. Their original header placement is provisional;
// first construction is in the editor toolbar builder 0x19f0b1 (CreateToolbarButtons, EEUIFScenarioEditor.cpp).
// All three are sizeof(UIEvent) = 0x0c, with no additional fields.





#include <mbstring.h>
#include <stdio.h>
#include <algorithm>
#include "GEWindow.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/GETerrainMesh.h"
#include <vector>
#include <deque>
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEHumanPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEGraphics.h"
#include "ee/RTSGraphics.h"
#include "ee/EEMiniMap.h"
#include "ee/EEUIMiniMap.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEGameOptions.h"
#include "lle/UIButton.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERandomMapClass.h"
#include "ee/EERandomMapGenerators.h"
#include "lle/UIScrollBar.h"
#include "lle/UIListBox.h"
#include "lle/UIRadioButton.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/GEBitmap.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EEUIFSEPaint.h"
#include "ee/EESEAPaint.h"
#include "ee/EESEAPaintTerrain.h"
#include "ee/EESEAPaintMoutains.h"
#include "lle/UIBitmap.h"
#include "ee/EEUIFSETerrain.h"
#include "ee/EESEAChangeLoadText.h"
#include "ee/EESELoadThread.h"
#include "ee/EEMouse.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "lle/UISlider.h"
#include "ee/EEUIFSEMap.h"
#include "ee/EESEUnit.h"
#include "ee/EETechTreeTechID.h"
#include "lle/UIEditBox.h"
#include <string.h>


#include "ee/EEButtonObject.h"

#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"
#include "lle/UICheckBox.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UITextLabel.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESETSEffectType.h"
#include "ee/EESETSETAI.h"
#include "ee/EESETSETMedia.h"
#include "ee/EEAQuickSaveLoad.h"
#include "ee/EEStringIDs.h"
// 0x1ac48b..0x1af2a9 (the EEUIFSETRBPaintType ctor, EEUIFSETerrain's members and the unit's template COMDATs)
// moved to EEUIFSETerrain.cpp (one translation unit; its UIListBox<vector<GEColor> > /
// UIListBox<EESEUITTData> COMDATs are what that file compiles).

// FUNCTION: EXE 0x5af47b
// Shows the unit limit per player (slider value / non-gaia players) and updates the slider's own label
// (EESEEGameUnitLimit; ee_screens editor_events.rs)

// FUNCTION: EXE 0x5af4c1
// (EESEEHideUI::ProcessEvent: ee_screens editor_events.rs)

// FUNCTION: EXE 0x5af55a
// (EESEEChangeViewport::ProcessEvent: ee_screens editor_events.rs)

// FUNCTION: EXE 0x5af58e
// (EESEEQuickLoad::ProcessEvent: ee_screens editor_events.rs)

// FUNCTION: EXE 0x5af874
// (EESEEQuickSave::ProcessEvent: ee_screens editor_events.rs)

// The host of the editor form's events whole in Rust (EE_SCREENS_SEE_OP_*; EESEERust in ee/EESEARust.h)
void FillScenarioComboBox(EESETSEScenarioComboBox* inComboBox, UITextLabel* inLabel, bool inAreas);   // EESEEvents.cpp
void FillAreaObjectComboBox(UITextLabel* inLabel, EESETSEScenarioComboBox* inComboBox, bool inAreas);   // EESETSETPlayer.cpp

static_assert(kSEEHideUI == EE_SCREENS_SEE_HIDE_UI && kSEEChangeViewport == EE_SCREENS_SEE_CHANGE_VIEWPORT &&
                  kSEEQuickLoad == EE_SCREENS_SEE_QUICK_LOAD && kSEEQuickSave == EE_SCREENS_SEE_QUICK_SAVE &&
                  kSEESliderChange == EE_SCREENS_SEE_SLIDER_CHANGE && kSEECheckDisplay == EE_SCREENS_SEE_CHECK_DISPLAY &&
                  kSEEUseObjectArea == EE_SCREENS_SEE_USE_OBJECT_AREA &&
                  kSEECAMGModifyObject == EE_SCREENS_SEE_CAMG_MODIFY_OBJECT &&
                  kSEEMSDCameraScroll == EE_SCREENS_SEE_MSD_CAMERA_SCROLL && kSEESAAAttribute == EE_SCREENS_SEE_SAA_ATTRIBUTE &&
                  kSEEUseAreaObject == EE_SCREENS_SEE_USE_AREA_OBJECT && kSEEFaceAreaObject == EE_SCREENS_SEE_FACE_AREA_OBJECT &&
                  kSEEMapType == EE_SCREENS_SEE_MAP_TYPE && kSEESortUnitList == EE_SCREENS_SEE_SORT_UNIT_LIST &&
                  kSEEStartEpoch == EE_SCREENS_SEE_START_EPOCH && kSEEUnitLimit == EE_SCREENS_SEE_UNIT_LIMIT &&
                  EE_SCREENS_SEE_PER_PLAYER_LABEL_ID == kSERBUnitTypesPerPlayerCountLabel,
              "ee/EESEARust.h's event kinds");

// p: three pointers, i: four integers, f: four floats (EE_SCREENS_SEE_OP_*)
extern "C" int32_t ee_screenscpp_see(uint32_t inOp, void* const* inPointers, int32_t* i, float* f)
{
    void* p = inPointers[0];
    void* q = inPointers[1];
    void* r = inPointers[2];
    switch (inOp) {
    case EE_SCREENS_SEE_OP_REFERENCE:
        static_cast<EEAction*>(p)->Reference();
        break;
    case EE_SCREENS_SEE_OP_DEREFERENCE:
        static_cast<EEAction*>(p)->Dereference();
        break;
    case EE_SCREENS_SEE_OP_CHECKED:
        return static_cast<UICheckBox*>(static_cast<UIEntity*>(p))->GetIsChecked() ? 1 : 0;
    case EE_SCREENS_SEE_OP_POST_VIEWPORT: {
        EESEERust* theEvent = new EESEERust(kSEEChangeViewport);
        ee_screens_see_viewport(theEvent->mEvent, f, i[0] != 0 ? 1 : 0);
        long theResult = gUserInterface->PostEvent(theEvent, NULL);
        theEvent->Dereference();
        return (int32_t)theResult;
    }
    case EE_SCREENS_SEE_OP_HAS_EDITOR:
        return gUIManager->mScenarioEditorForm != NULL;
    case EE_SCREENS_SEE_OP_SET_GAME_VIEW:
        gUIManager->mScenarioEditorForm->SetGameView(GERectangle(f[0], f[1], f[2], f[3]), i[0] != 0);
        break;
    case EE_SCREENS_SEE_OP_REDRAW_ALL:
        gUIManager->RedrawAll();
        break;
    case EE_SCREENS_SEE_OP_SHOW_LOADED:
        gUIManager->mScenarioEditorForm->PostShowLoadedObject(static_cast<EEAction*>(p));
        break;
    case EE_SCREENS_SEE_OP_QUICK_SAVE: {
        EEAction* theAction = new EESEARust(kSEAQuickSave, static_cast<EEAction*>(p));
        gUserInterface->PostAction(theAction);
        theAction->Dereference();
        break;
    }
    case EE_SCREENS_SEE_OP_RANGE:
        return (int32_t)static_cast<UISlider*>(static_cast<UIEntity*>(p))->GetCurrentRangeValue();
    case EE_SCREENS_SEE_OP_SLIDER_TEXT: {
        // the original's sprintf of its UWideString format: the float value passed as a double through `...`
        UWideString theText;
        sprintf(theText, static_cast<const char*>(q), (double)f[0]);
        static_cast<UITextLabel*>(p)->SetText(theText);
        break;
    }
    case EE_SCREENS_SEE_OP_SET_DISPLAY:
        static_cast<UIControl*>(p)->SetDisplayMode((unsigned long)i[0]);
        break;
    case EE_SCREENS_SEE_OP_FILL_SCENARIO:
        FillScenarioComboBox(static_cast<EESETSEScenarioComboBox*>(p), static_cast<UITextLabel*>(q), i[0] != 0);
        break;
    case EE_SCREENS_SEE_OP_SET_SELECTION:
        static_cast<UIComboBox*>(static_cast<EESETSEScenarioComboBox*>(p))->SetCurrentSelection((unsigned long)i[0]);
        break;
    case EE_SCREENS_SEE_OP_ENTITY_ID:
        return (int32_t)static_cast<UIControl*>(static_cast<UIEntity*>(p))->GetId();
    case EE_SCREENS_SEE_OP_LIST_SELECTION: {
        unsigned long theSelection;
        return static_cast<UILBString*>(static_cast<UIEntity*>(p))->GetCurrentSelection(theSelection) == kErrorOK
                   ? (int32_t)theSelection
                   : -1;
    }
    case EE_SCREENS_SEE_OP_CAMG_ADD:
        static_cast<EESETSETCreateAIMilitaryGroup*>(p)->AddObject((unsigned long)i[0], static_cast<UILBString*>(q),
                                                                  static_cast<UILBString*>(r));
        break;
    case EE_SCREENS_SEE_OP_CAMG_REMOVE:
        static_cast<EESETSETCreateAIMilitaryGroup*>(p)->RemoveObject((unsigned long)i[0], static_cast<UILBString*>(q),
                                                                     static_cast<UILBString*>(r));
        break;
    case EE_SCREENS_SEE_OP_MSD_SHOW:
        static_cast<EESETSETMediaSendDialog*>(p)->ShowCameraScrollSpeed(static_cast<EESETSEffectControls*>(q));
        break;
    case EE_SCREENS_SEE_OP_COMBO_SELECTION: {
        unsigned long theSelection;
        return static_cast<UIComboBox*>(static_cast<UIEntity*>(p))->GetCurrentSelection(theSelection) == kErrorOK
                   ? (int32_t)theSelection
                   : -1;
    }
    case EE_SCREENS_SEE_OP_SAA_SHOW:
        static_cast<EESETSETSetAIAttribute*>(p)->ShowAttributeRanges((unsigned long)i[0],
                                                                     static_cast<EESETSEffectControls*>(q));
        break;
    case EE_SCREENS_SEE_OP_FILL_AREA_OBJECT: {
        UIForm* theForm = static_cast<UIForm*>(p);
        FillAreaObjectComboBox(static_cast<UITextLabel*>(theForm->GetControlById(i[0])),
                               static_cast<EESETSEScenarioComboBox*>(theForm->GetControlById(i[1])), i[2] != 0);
        break;
    }
    case EE_SCREENS_SEE_OP_MAP_ELEVATION:
        static_cast<EEUIFSEMap*>(p)->ShowElevationControls(i[0] != 0);
        break;
    case EE_SCREENS_SEE_OP_REFRESH_UNIT_LIST:
        ((EESEUnit*)(static_cast<EEUIFScenarioEditor*>(static_cast<UIForm*>(p)))->mTabGroups[kEditorTabUnits])
            ->RefreshUnitList();
        break;
    case EE_SCREENS_SEE_OP_EPOCH_COUNT:
        return (int32_t)EEEpochCount();
    case EE_SCREENS_SEE_OP_LINE_COUNT:
        return (int32_t)static_cast<UIComboBox*>(p)->GetListBox()->GetLineCount();
    case EE_SCREENS_SEE_OP_FILL_EPOCHS: {
        UWideString theText;
        std::vector<UWideString> theNames;
        for (unsigned long k = (unsigned long)i[0]; k < (unsigned long)EEEpochCount(); k++) {
            gResource->GetString(EEEpochTitleID(k), theText);
            theNames.push_back(theText);
        }
        static_cast<UIComboBox*>(p)->DeleteAllEntries();
        static_cast<UIComboBox*>(p)->AppendStringArray(theNames);
        break;
    }
    case EE_SCREENS_SEE_OP_MAX_PLAYERS:
        return (int32_t)gWorld.mMaxPlayers;
    case EE_SCREENS_SEE_OP_LABEL_NUMBER:
        static_cast<UITextLabel*>(static_cast<UIForm*>(p)->GetControlById(i[0]))->SetText((unsigned long)(uint32_t)i[1]);
        break;
    }
    return 0;
}

// FUNCTION: EXE 0x5af8ad
// Creates a quick save action referencing the quick save/load state (inline ctor, out of line after its first user)

// SYNTHETIC: EXE 0x5af8cd
// (the scalar deleting destructor, emitted with the vtable)

// SYNTHETIC: EXE 0x5af900
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x5af92b
// Reads the dialog message (text, unit type, sound, duration, unit, scroll speed, and from version 0.200 on
// the cinematic flag) from the saved scenario; returns the first read error
long EESMDialogMessage::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult;
    if (inMajorVersion <= 0 && inMinorVersion <= 199) {
        if ((theResult = this->mText.Load(inFile)) != 0)
            return theResult;
        if ((theResult = inFile.Read(this->mUnitType)) != 0)
            return theResult;
        if ((theResult = this->mSound.Load(inFile)) != 0)
            return theResult;
        if ((theResult = inFile.Read(this->mDuration)) != 0)
            return theResult;
        if ((theResult = inFile.Read(this->mUnitID)) != 0)
            return theResult;
        return inFile.Read(this->mScrollSpeed);
    }
    if ((theResult = this->mText.Load(inFile)) != 0)
        return theResult;
    if ((theResult = inFile.Read(this->mUnitType)) != 0)
        return theResult;
    if ((theResult = this->mSound.Load(inFile)) != 0)
        return theResult;
    if ((theResult = inFile.Read(this->mDuration)) != 0)
        return theResult;
    if ((theResult = inFile.Read(this->mUnitID)) != 0)
        return theResult;
    if ((theResult = inFile.Read(this->mScrollSpeed)) != 0)
        return theResult;
    return inFile.Read(this->mIsCinematic);
}

// FUNCTION: EXE 0x5afa14
// Writes the dialog message, cinematic flag included, to the saved scenario; returns the first write error
long EESMDialogMessage::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult;
    if ((theResult = this->mText.Save(outFile)) != 0)
        return theResult;
    if ((theResult = outFile.Write(this->mUnitType)) != 0)
        return theResult;
    if ((theResult = this->mSound.Save(outFile)) != 0)
        return theResult;
    if ((theResult = outFile.Write(this->mDuration)) != 0)
        return theResult;
    if ((theResult = outFile.Write(this->mUnitID)) != 0)
        return theResult;
    if ((theResult = outFile.Write(this->mScrollSpeed)) != 0)
        return theResult;
    return outFile.Write(this->mIsCinematic);
}
