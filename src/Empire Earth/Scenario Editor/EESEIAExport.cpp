// C:\EEX\Empire Earth\Scenario Editor\EESEIAExport.cpp (file name inferred: no __FILE__; RTTI EESEIAExport). The trigger
// text export/import actions: 0x16ff5f..0x1705a2 (EESEIAExport::Virtual4, the export question's answer, EESEIAExport's
// ??_G, EESEIEExport::ProcessEvent, EESEIAImport::Virtual4). After EESEEvents.cpp (0x16f892..0x16ff5f), before
// EESEInstructions.cpp (0x1705a2). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERhombus.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NEMessage.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFileComboBox.h"
#include "lle/UResourceFile.h"
#include "lle/UICheckBox.h"

#include "ee/EEStringIDs.h"
#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESETSBaseObject.h"
#include "lle/USingleton.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMessage.h"
#include "ee/EEARust.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EESEIAExport.h"
unsigned long gSETextVersion = 1;           // 0x4c2cb8 (.data = 1; ee/EESEIAExport.h)

extern char gEmptyString[];                 // 0x4e5b98 (pooled "")
// 0x1705a2, in EESEInstructions.cpp (the import question's answer)
void EESEImportTextAnswer(UIForm* inInfo);

// Writes the scenario's text to the chosen .lcl file under a {Version_1} header and reports success
// (noting a changed file name) or a file error
void EESEIAExport::Virtual4()
{
    UWideString theName;
    UWideString theError;
    UWideString theMessage;

    if (this->mFile.FOpen("w") == 0) {
        gUIManager->mScenarioEditorForm->RefreshCurrentGroup();
        if ((unsigned long)fprintf((FILE*)this->mFile, "{Version_%ld}\n\n", gSETextVersion) > 0) {
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

// Posts the export action for the given file spec and frees the spec (the replace question's yes)
static void EESEExportTextAnswer(UIForm* inFile)
{
    FSFileSpec* theFile = reinterpret_cast<FSFileSpec*>(inFile);
    gUserInterface->PostAction(new EESEIAExport(*theFile));
    delete theFile;
}

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
            theLine = (const char*)PlatMbsNInc(reinterpret_cast<const unsigned char*>((const char*)theLine), thePosition + 9);
            thePosition = theLine.Find("}", 0);
            if (thePosition <= theLine.GetLength()) {
                char* theEnd = (char*)PlatMbsNInc(reinterpret_cast<const unsigned char*>((const char*)theLine), thePosition);
                strcpy(theEnd, gEmptyString);
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
