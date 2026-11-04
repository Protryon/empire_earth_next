// C:\EEX\Empire Earth\Scenario Editor\EESEIAExport.cpp (file name inferred: no __FILE__; RTTI EESEIAExport). The trigger
// text export/import actions: 0x16ff5f..0x1705a2 (EESEIAExport::Virtual4, the export question's answer, EESEIAExport's
// ??_G, EESEIEExport::ProcessEvent, EESEIAImport::Virtual4). After EESEEvents.cpp (0x16f892..0x16ff5f), before
// EESEInstructions.cpp (0x1705a2). Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
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
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIFileComboBox.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UISlider.h"
#include "lle/UICheckBox.h"

#include "lle/UIRadioButton.h"
#include "lle/UITriStateCheckBox.h"
#include <vector>
#include <deque>
#include "ee/EEStringIDs.h"
#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESMEffect.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETriggerScreen.h"
#include "ee/EESEEChangeEffectAction.h"
#include "ee/EESEEGCPlace.h"
#include "ee/EESEEPlace.h"
#include "ee/EESEEvents.h"
#include "lle/USingleton.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESETSUICBPlayer.h"
#include "ee/EESETSEffectControls.h"
#include "ee/EESETSEffectType.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEMouse.h"
#include "ee/EEGraphics.h"
#include "ee/EEAScroll.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EESMArea.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EESMObject.h"
#include "ee/EESEPlayerScreen.h"
#include "ee/EEMessage.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EEALasso.h"
#include "ee/EEAMouseClick.h"
#include "ee/EESEALasso.h"
#include "ee/EESEAMouseClick.h"
#include "ee/EESETSAMouseClick.h"
#include "ee/EESETSALasso.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EESEUICTriggerSelectUnits.h"
#include "ee/EESETSOUnitList.h"
#include "ee/EESETSAComboBox.h"
#include "ee/EESETSAEditBox.h"
#include "ee/EESETSObject.h"
#include "lle/UIESliderChange.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "lle/FSFileSpec.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EESEIAExport.h"
unsigned long gSETextVersion = 1;           // 0x4c2cb8 (.data = 1; ee/EESEIAExport.h)

extern char gU4E5B98[];                     // 0x4e5b98 (pooled "")
// 0x1705a2, in EESEInstructions.cpp (the import question's answer)
void EESEImportTextAnswer(UIForm* inInfo);

// FUNCTION: EXE 0x56ff5f
// Writes the scenario's text to the chosen .lcl file under a {Version_1} header and reports success
// (noting a changed file name) or a file error
void EESEIAExport::Virtual4()
{
    UWideString theName;
    UWideString theError;
    UWideString theMessage;

    if (this->mFile.FOpen("w") == 0) {
        gUIManager->mScenarioEditorForm->RefreshCurrentGroup();
        if ((unsigned long)fprintf((FILE*)this->mFile, "{Version_%d}\n\n", gSETextVersion) > 0) {
            if (gWorld.mScenarioManager->ExportText(this->mFile) != 0) {
                gResource->GetString(IDS_FILE_ERROR, theError);
                gUIManager->DisplayMessage(theError, 1, NULL, NULL, true);
            } else {
                gResource->GetString(IDS_FILE_EXPORTED_SUCCESSFULLY, theMessage);
                theName = this->mFile.GetFile();
                if (theName != gWorld.mScenarioManager->mExportFileName) {
                    gResource->GetString(IDS_EXPORTED_FILE_NAME_HAS_CHANGED, theError);
                    theMessage += "\n";
                    theMessage += theError;
                }
                gWorld.mScenarioManager->mExportFileName = theName;
                gUIManager->DisplayMessage(theMessage, 1, NULL, NULL, true);
            }
            this->mFile.Close();
            return;
        }
    }
    gResource->GetString(IDS_FILE_ERROR, theError);
    gUIManager->DisplayMessage(theError, 1, NULL, NULL, true);
}

// FUNCTION: EXE 0x5700e9
// Posts the export action for the given file spec and frees the spec (the replace question's yes)
static void EESEExportTextAnswer(UIForm* inFile)
{
    FSFileSpec* theFile = reinterpret_cast<FSFileSpec*>(inFile);
    gUserInterface->PostAction(new EESEIAExport(*theFile));
    delete theFile;
}

// FUNCTION: EXE 0x57013d
// Destroys the export action and frees it when asked (EESEIAExport's scalar deleting destructor)

// FUNCTION: EXE 0x570170
// Exports the scenario text to data\Scenarios\<name>.lcl, asking first when the file exists; complains
// when no name is entered; returns the open result (nonzero when the file was new)
long EESEIEExport::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
{
    FSFileSpec theFile;
    UWideString theName;
    UWideString theMessage;
    EEUIManager* theManager = static_cast<EEUIManager*>(inForm->GetManager());
    long theResult;

    UIEditBox* theBox = static_cast<UIEditBox*>(inForm->GetControlById(kSEInstructionsLocalizationEditBox));
    theResult = 0;
    theName = theBox->GetText(0);
    if (theName.GetLength() > 0) {
        theName += ".lcl";
        theFile.SetVolume(gStartupVolume);
        theFile.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\");
        theFile.SetFile((const char*)theName);
        theResult = theFile.FOpen("r");
        if (theResult == 0) {
            theFile.Close();
            gResource->GetString(IDS_REPLACE_FILE_ON_DISK, theMessage);
            theManager->DisplayQuestion(theMessage, 1, (UIForm*)new FSFileSpec(theFile), EESEExportTextAnswer, NULL);
        } else
            EESEExportTextAnswer((UIForm*)new FSFileSpec(theFile));
    } else {
        gResource->GetString(IDS_YOU_MUST_ENTER_FILE_NAME_BEFORE, theMessage);
        theManager->DisplayMessage(theMessage, 1, NULL, NULL, true);
    }
    return theResult;
}

// FUNCTION: EXE 0x570345
// Imports trigger text from the .lcl file after checking its {Version_n} header; a validation run lists the
// errors and asks to import anyway; the file is closed on every path
void EESEIAImport::Virtual4()
{
    UWideString theUnused;
    UWideString theLine;
    UWideString theMessage;
    char theBuffer[0x200];
    unsigned long thePosition;

    if (this->mFile.FOpen("r") == 0 && !feof((FILE*)this->mFile)) {
        theLine = fgets(theBuffer, 0x200, (FILE*)this->mFile);
        thePosition = theLine.Find("{Version_", 0);
        if (thePosition <= theLine.GetLength()) {
            theLine = (const char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)theLine), thePosition + 9);
            thePosition = theLine.Find("}", 0);
            if (thePosition <= theLine.GetLength()) {
                char* theEnd = (char*)_mbsninc(reinterpret_cast<const unsigned char*>((const char*)theLine), thePosition);
                strcpy(theEnd, gU4E5B98);
                unsigned long theVersion = atoi(theLine);
                if (theVersion <= gSETextVersion) {
                    EEScenarioManager* theManager = gWorld.mScenarioManager;
                    if (theManager->ImportTriggerText(this->mFile, theVersion, this->mValidate)) {
                        if (this->mValidate == true) {
                            gResource->GetString(IDS_FOLLOWING_ERRORS_OCCURRED_WHILE_IMPORTING, theLine);
                            theLine += "\n\n";
                            for (unsigned long i = 0; i < gWorld.mScenarioManager->mImportErrors.size(); i++) {
                                theLine += gWorld.mScenarioManager->mImportErrors[i];
                                theLine += "\n\n";
                            }
                            EESEImportInfo* theInfo = new EESEImportInfo;
                            theInfo->mFile = this->mFile;
                            theInfo->mValidate = false;
                            gUIManager->DisplayQuestion(theLine, 1, reinterpret_cast<UIForm*>(theInfo), EESEImportTextAnswer, NULL);
                        }
                    } else {
                        gResource->GetString(IDS_FILE_IMPORTED_SUCCESSFULLY, theMessage);
                        gUIManager->DisplayMessage(theMessage, 1, NULL, NULL, true);
                    }
                    gUIManager->mScenarioEditorForm->ResumeCurrentGroup();
                    this->mFile.Close();
                    return;
                }
            }
        }
        gResource->GetString(IDS_LCL_FILE_HAS_INVALID_VERSION, theLine);
    } else
        gResource->GetString(IDS_FILE_ERROR, theLine);
    this->mFile.Close();    // an empty file or a bad version left the file open (Close does nothing when it is not)
    gUIManager->DisplayMessage(theLine, 1, NULL, NULL, true);
}
