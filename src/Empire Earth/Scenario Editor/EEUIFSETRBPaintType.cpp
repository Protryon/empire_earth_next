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
#include "ee/EESEASaveScenario.h"
#include "ee/EESEATestScenario.h"
#include "ee/EESEAExitScenario.h"
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
#include "ee/EESEEGameUnitLimit.h"
#include "lle/UIEditBox.h"
#include <string.h>


#include "ee/EEButtonObject.h"

#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"
#include "lle/UICheckBox.h"
#include "ee/EESEEHideUI.h"
#include "ee/EESEEQuickLoad.h"
#include "ee/EESEEQuickSave.h"
#include "ee/EEAQuickSaveLoad.h"
#include "ee/EESEAQuickSaveScenario.h"
#include "ee/EEStringIDs.h"
// 0x1ac48b..0x1af2a9 (the EEUIFSETRBPaintType ctor, EEUIFSETerrain's members and the unit's template COMDATs)
// moved to EEUIFSETerrain.cpp (one translation unit; its UIListBox<vector<GEColor> > /
// UIListBox<EESEUITTData> COMDATs are what that file compiles).

// FUNCTION: EXE 0x5af47b
// Shows the unit limit per player (slider value / non-gaia players) and updates the slider's own label
long EESEEGameUnitLimit::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UITextLabel* theLabel = static_cast<UITextLabel*>(inForm->GetControlById(kSERBUnitTypesPerPlayerCountLabel));
    UISlider* theSlider = static_cast<UISlider*>(inEntity);
    unsigned long thePlayerCount = gWorld.mMaxPlayers - 1;
    theLabel->SetText((unsigned long)theSlider->GetCurrentRangeValue() / thePlayerCount);
    return UIESliderChange::ProcessEvent(inForm, inEntity);
}

// FUNCTION: EXE 0x5af4c1
// Posts a viewport change to the full view or the framed view, as the hide-UI check box is checked or not
long EESEEHideUI::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    UICheckBox* theBox = static_cast<UICheckBox*>(inEntity);
    GERectangle theRectangle;
    if (theBox->GetIsChecked())
        theRectangle = this->mR1C;
    else
        theRectangle = this->mR0C;
    UIEvent* theEvent = new EESEEChangeViewport(theRectangle, theBox->GetIsChecked());
    long theResult = gUserInterface->PostEvent(theEvent, NULL);
    theEvent->Dereference();
    return theResult;
}

// FUNCTION: EXE 0x5af55a
// Sets the scenario editor's game view rectangle and UI visibility, then redraws everything
long EESEEChangeViewport::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    if (gUIManager->mScenarioEditorForm != NULL) {
        gUIManager->mScenarioEditorForm->SetGameView(this->mRectangle, this->mHidden == false);
        gUIManager->RedrawAll();
    }
    return kErrorOK;
}

// FUNCTION: EXE 0x5af58e
// Asks the editor to show the quick-loaded scenario
long EESEEQuickLoad::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    static_cast<EEUIFScenarioEditor*>(inForm)->PostShowLoadedObject(this->mOwner);
    return kErrorOK;
}

// FUNCTION: EXE 0x5af874
// Posts a quick save of the scenario
long EESEEQuickSave::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    EEAction* theAction = new EESEAQuickSaveScenario(this->mOwner);
    gUserInterface->PostAction(theAction);
    theAction->Dereference();
    return kErrorOK;
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
