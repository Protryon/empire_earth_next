// C:\EEX\Empire Earth\Scenario Editor\EESEInstructions.cpp (__FILE__ in the ctor's ESVERIFYs). 0x1705a2..0x172125: the text
// import answer and button, the instructions tab and the ??_G of its events, with the functions of
// EEABuild.cpp folded in. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/ESChecksum.h"
#include "lle/FSFile.h"
#include "lle/FSBasicFile.h"
#include "lle/FSFileSpec.h"
#include "lle/FSMemoryFile.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DPoint.h"
#include "lle/GEOrientation.h"
#include "lle/GERectangle.h"
#include "lle/GETransformation.h"
#include "lle/GEVector.h"
#include "lle/PIMModule.h"
#include "lle/NEProtocolConfiguration.h"
#include "lle/TSCriticalSection.h"
#include "lle/SEConductor.h"
#include "lle/SEMixer.h"
#include "lle/SESample.h"
#include "lle/TSEvent.h"
#include "lle/TSReadWriteLock.h"
#include "lle/TSThread.h"
#include "lle/TSScheduler.h"
#include "lle/UMFastMath.h"
#include "lle/URandom.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "ee/EEABuild.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/UIEntity.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIScrollBar.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EEUIFileComboBox.h"
#include "ee/EESEIAExport.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EESEInstructions.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEStringIDs.h"
#include "ee/EEScreenEvent.h"
#include "rust/ee_screens.h"
extern GEFont* gU531938;                    // 0x531938 (Empire Earth.cpp's fonts)
extern char gEmptyString[];                 // 0x4e5b98 (pooled "")

// FUNCTION: EXE 0x5705a2
// Posts the import action for the file the question carries (the import question's yes) and frees
// the question's import info
void EESEImportTextAnswer(UIForm* inInfo)
{
    EESEImportInfo* theInfo = reinterpret_cast<EESEImportInfo*>(inInfo);
    gUserInterface->PostAction(new EESEIAImport(theInfo->mFile, theInfo->mValidate));
    delete theInfo;
}

// SYNTHETIC: EXE 0x570605
// (the scalar deleting destructor, emitted with the vtable; slot EESEIAImport+0x0, stores EESEIAImport vtable, stores EEAction vtable)

// FUNCTION: EXE 0x570638
// Starts importing data\Scenarios\<name>.lcl: asks to confirm, then validates first; complains when no
// name is entered or the file is missing; returns the open result
long EESEIEImport::ProcessEvent(UIForm* inForm, UIEntity* inEntity)
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
            EESEImportInfo* theInfo = new EESEImportInfo;
            theInfo->mFile = theFile;
            theInfo->mValidate = true;
            gResource->GetString(IDS_IMPORTING_FILE_REPLACES_ALL_TEXT, theMessage);
            theManager->DisplayQuestion(theMessage, 1, reinterpret_cast<UIForm*>(theInfo), EESEImportTextAnswer, NULL);
            theFile.Close();
        } else {
            gResource->GetString(IDS_FILE_NAME_NOT_FOUND, theMessage);
            theManager->DisplayMessage(theMessage, 1, NULL, NULL, true);
        }
    } else {
        gResource->GetString(IDS_YOU_MUST_ENTER_FILE_NAME_BEFORE, theMessage);
        theManager->DisplayMessage(theMessage, 1, NULL, NULL, true);
    }
    return theResult;
}

// FUNCTION: EXE 0x5707ff
// Creates the instructions tab: map/movie/sound file combos (directories created when missing), the text
// boxes and labels, and the text import/export buttons. History reuses the hints label/box ids
EESEInstructions::EESEInstructions(unsigned long& ioIndex, UIForm* inForm, GEFont* inFont)
{
    FSFileSpec theScenDirectory;
    FSFileSpec theSoundsDirectory;
    FSFileSpec theMoviesDirectory;
    GE2DPoint theLocation;
    UWideString theText;
    UIEvent* theFindText = EECreateScreenEvent(EE_SCREENS_SE_FIND_TEXT);
    GEText* theGEText;
    theScenDirectory.SetVolume(gStartupVolume);
    theScenDirectory.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\");
    FSPath* theScenPath = new FSPath;
    theScenPath->AddDirectory(theScenDirectory, true);
    theSoundsDirectory.SetVolume(gStartupVolume);
    theSoundsDirectory.SetDirectory(gStartupDirectory + "data\\" + "Sounds\\");
    FSPath* theSoundsPath = new FSPath;
    theSoundsPath->AddDirectory(theSoundsDirectory, true);
    theMoviesDirectory.SetVolume(gStartupVolume);
    theMoviesDirectory.SetDirectory(gStartupDirectory + "data\\" + "Movies\\");
    FSPath* theMoviesPath = new FSPath;
    theMoviesPath->AddDirectory(theMoviesDirectory, true);
    if (!theScenDirectory.IsDirectory())
        ESVERIFY_AT(theScenDirectory.CreateDirectory() == keseOK, 536);

    if (!theSoundsDirectory.IsDirectory())
        ESVERIFY_AT(theSoundsDirectory.CreateDirectory() == keseOK, 539);

    if (!theMoviesDirectory.IsDirectory())
        ESVERIFY_AT(theMoviesDirectory.CreateDirectory() == keseOK, 542);

    gResource->GetString(IDS_SCEN_INST, theText);
    theLocation.SetXY(20.0f, 2.0f);
    this->mMapLabel = new UITextLabel(theText, inFont, 0x44c, GERectangle(theLocation, 150.0f, (float)inFont->GetHeight()),
                           inForm, 0, ioIndex, 0);
    this->mMapLabel->SetAlignment(kTextAlignCenter);
    this->mMapLabel->SetBgOpacity(0.4f);
    theGEText = this->mMapLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mMapLabel, 7);
    ioIndex++;
    gResource->GetString(IDS_MOVIE, theText);
    theLocation.SetX(437.0f);
    this->mMovieLabel = new UITextLabel(theText, inFont, 0x44e, GERectangle(theLocation, 150.0f, (float)inFont->GetHeight()),
                           inForm, 0, ioIndex, 0);
    this->mMovieLabel->SetAlignment(kTextAlignCenter);
    this->mMovieLabel->SetBgOpacity(0.4f);
    theGEText = this->mMovieLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mMovieLabel, 7);
    ioIndex++;
    gResource->GetString(IDS_INST_SOUND_OVER, theText);
    theLocation.SetX(854.0f);
    this->mSoundLabel = new UITextLabel(theText, inFont, 0x450, GERectangle(theLocation, 150.0f, (float)inFont->GetHeight()),
                           inForm, 0, ioIndex, 0);
    this->mSoundLabel->SetAlignment(kTextAlignCenter);
    this->mSoundLabel->SetBgOpacity(0.4f);
    theGEText = this->mSoundLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mSoundLabel, 7);
    ioIndex++;
    gResource->GetString(IDS_SCENARIO_INSTRUCTIONS, theText);
    theLocation.SetXY(20.0f, 62.0f);
    this->mInstructionsLabel = new UITextLabel(theText, inFont, 0x452, GERectangle(theLocation, 984.0f, (float)inFont->GetHeight()),
                           inForm, 0, ioIndex, 0);
    this->mInstructionsLabel->SetAlignment(kTextAlignCenter);
    this->mInstructionsLabel->SetBgOpacity(0.4f);
    theGEText = this->mInstructionsLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mInstructionsLabel, 7);
    ioIndex++;
    gResource->GetString(IDS_AFTERMATH, theText);
    theLocation.SetY(386.0f);
    this->mAftermathLabel = new UITextLabel(theText, inFont, 0x454, GERectangle(theLocation, 984.0f, (float)inFont->GetHeight()),
                           inForm, 0, ioIndex, 0);
    this->mAftermathLabel->SetAlignment(kTextAlignCenter);
    this->mAftermathLabel->SetBgOpacity(0.4f);
    theGEText = this->mAftermathLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mAftermathLabel, 7);
    ioIndex++;
    this->mAftermathLabel->SetDisplayMode(kDisplayModeHidden);
    gResource->GetString(IDS_SE_HINTS, theText);
    theLocation.SetY(202.0f);
    this->mHintsLabel = new UITextLabel(theText, inFont, 0x456, GERectangle(theLocation, 984.0f, (float)inFont->GetHeight()),
                           inForm, 0, ioIndex, 0);
    this->mHintsLabel->SetAlignment(kTextAlignCenter);
    this->mHintsLabel->SetBgOpacity(0.4f);
    theGEText = this->mHintsLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mHintsLabel, 7);
    ioIndex++;
    gResource->GetString(IDS_SE_HISTORY, theText);
    theLocation.SetY(342.0f);
    this->mHistoryLabel = new UITextLabel(theText, inFont, 0x456, GERectangle(theLocation, 984.0f, (float)inFont->GetHeight()),
                           inForm, 0, ioIndex, 0);
    this->mHistoryLabel->SetAlignment(kTextAlignCenter);
    this->mHistoryLabel->SetBgOpacity(0.4f);
    theGEText = this->mHistoryLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mHistoryLabel, 7);
    ioIndex++;
    theLocation.SetY(80.0f);
    this->mInstructionsEditBox = new UIEditBox(0x453, inFont, 0, false, GERectangle(theLocation, 984.0f, 120.0f), inForm, ioIndex, 2, true,
                         false);
    this->mInstructionsEditBox->SetBgOpacity(0.4f);
    this->mInstructionsEditBox->SetTriggerEvent(kControlTriggerEnterPressed, theFindText);
    theGEText = this->mInstructionsEditBox->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mInstructionsEditBox, 7);
    ioIndex++;
    theLocation.SetY(404.0f);
    this->mAftermathEditBox = new UIEditBox(0x455, inFont, 0, false, GERectangle(theLocation, 984.0f, 120.0f), inForm, ioIndex, 2, true,
                         false);
    this->mAftermathEditBox->SetBgOpacity(0.4f);
    this->mAftermathEditBox->SetTriggerEvent(kControlTriggerEnterPressed, theFindText);
    theGEText = this->mAftermathEditBox->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mAftermathEditBox, 7);
    ioIndex++;
    this->mAftermathEditBox->SetDisplayMode(kDisplayModeHidden);
    theLocation.SetY(220.0f);
    this->mHintsEditBox = new UIEditBox(0x457, inFont, 0, false, GERectangle(theLocation, 984.0f, 120.0f), inForm, ioIndex, 2, true,
                         false);
    this->mHintsEditBox->SetBgOpacity(0.4f);
    this->mHintsEditBox->SetTriggerEvent(kControlTriggerEnterPressed, theFindText);
    theGEText = this->mHintsEditBox->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mHintsEditBox, 7);
    ioIndex++;
    theLocation.SetY(360.0f);
    this->mHistoryEditBox = new UIEditBox(0x457, inFont, 0, false, GERectangle(theLocation, 984.0f, 120.0f), inForm, ioIndex, 2, true,
                         false);
    this->mHistoryEditBox->SetBgOpacity(0.4f);
    this->mHistoryEditBox->SetTriggerEvent(kControlTriggerEnterPressed, theFindText);
    theGEText = this->mHistoryEditBox->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mHistoryEditBox, 7);
    ioIndex++;

    // the file combo boxes
    theLocation.SetXY(20.0f, 22.0f);
    this->mMapComboBox = new EEUIFileComboBox(theScenPath, UString("*") + ".jpg", 0x44d, 1, std::vector<UWideString>(), inFont,
                                theLocation, 150, 400, inForm, ioIndex, 0, 2, false);
    theGEText = this->mMapComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    this->mMapComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mMapComboBox->SetTextBoxBgOpacity(0.4f);
    inForm->AddControl(this->mMapComboBox, 7);
    ioIndex++;
    theLocation.SetX(437.0f);
    this->mMovieComboBox = new EEUIFileComboBox(theMoviesPath, UString("*") + ".mpg", 0x44f, 1, std::vector<UWideString>(), inFont,
                                theLocation, 150, 400, inForm, ioIndex, 0, 2, true);
    theGEText = this->mMovieComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    this->mMovieComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mMovieComboBox->SetTextBoxBgOpacity(0.4f);
    inForm->AddControl(this->mMovieComboBox, 7);
    ioIndex++;
    theLocation.SetX(854.0f);
    this->mSoundComboBox = new EEUIFileComboBox(theSoundsPath, "*.mp3", 0x451, 1, std::vector<UWideString>(), inFont,
                                theLocation, 150, 400, inForm, ioIndex, 0, 2, true);
    theGEText = this->mSoundComboBox->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    this->mSoundComboBox->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
    this->mSoundComboBox->SetTextBoxBgOpacity(0.4f);
    inForm->AddControl(this->mSoundComboBox, 7);
    ioIndex++;
    theFindText->Dereference();

    // the text import and export
    theLocation.SetXY(20.0f, 494.0f);
    this->mImportButton = new UIButton(0x458, NULL, NULL, NULL, GERectangle(theLocation, 150.0f, 20.0f), inForm, ioIndex, 2);
    gResource->GetString(IDS_IMPORT, theText);
    this->mImportButton->SetText(theText, gU531938);
    this->mImportButton->SetStringTableID(IDS_IMPORT_LOCALIZED_TEXT_FILE_INTO_SCENARIO);
    this->mImportButton->SetBgOpacity(0.4f);
    theGEText = this->mImportButton->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    UIEvent* theEvent = new EESEIEImport;
    this->mImportButton->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    inForm->AddControl(this->mImportButton, 7);
    ioIndex++;
    theLocation.SetXY(20.0f, 516.0f);
    this->mExportButton = new UIButton(0x459, NULL, NULL, NULL, GERectangle(theLocation, 150.0f, 20.0f), inForm, ioIndex, 2);
    gResource->GetString(IDS_EXPORT, theText);
    this->mExportButton->SetText(theText, gU531938);
    this->mExportButton->SetStringTableID(IDS_EXPORT_SCENARIO_TO_TEXT_FILE);
    this->mExportButton->SetBgOpacity(0.4f);
    theEvent = new EESEIEExport;
    this->mExportButton->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    theGEText = this->mExportButton->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mExportButton, 7);
    ioIndex++;
    theLocation.SetXY(200.0f, 494.0f);
    gResource->GetString(IDS_LOCALIZATION_TEXT_FILE_NAME, theText);
    this->mLocalizationLabel = new UITextLabel(theText, inFont, 0x45a, GERectangle(theLocation, 400.0f, 20.0f), inForm, 0, ioIndex, 0);
    this->mLocalizationLabel->SetAlignment(kTextAlignCenter);
    this->mLocalizationLabel->SetBgOpacity(0.4f);
    theGEText = this->mLocalizationLabel->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mLocalizationLabel, 7);
    ioIndex++;
    theLocation.SetY(516.0f);
    this->mLocalizationEditBox = new UIEditBox(kSEInstructionsLocalizationEditBox, inFont, 0x100, false, GERectangle(theLocation, 400.0f, 20.0f), inForm, ioIndex, 2,
                         false, false);
    this->mLocalizationEditBox->SetBgOpacity(0.4f);
    theGEText = this->mLocalizationEditBox->GetGEText();
    theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mLocalizationEditBox, 7);
    ioIndex++;
    theScenPath->Dereference();
    theSoundsPath->Dereference();
    theMoviesPath->Dereference();
}

// FUNCTION: EXE 0x571b49
// Destroys the export button's event and frees it when asked (EESEIEExport's scalar deleting destructor)

// FUNCTION: EXE 0x571b74
// Destroys the import button's event and frees it when asked (EESEIEImport's scalar deleting destructor)

// SYNTHETIC: EXE 0x571b9f
// (the scalar deleting destructor, emitted with the vtable; slot EESEInstructions+0x0)

// SYNTHETIC: EXE 0x571bbb
// (the scalar deleting destructor, emitted with the vtable; slot UITextLabel+0x0)

// SYNTHETIC: EXE 0x571bd8
// (the scalar deleting destructor, emitted with the vtable; slot UIButton+0x0)

// FUNCTION: EXE 0x571bf5
// Destroys the instructions tab and every control it created
EESEInstructions::~EESEInstructions()
{
    delete this->mMapLabel;
    delete this->mMovieLabel;
    delete this->mSoundLabel;
    delete this->mMapComboBox;
    delete this->mMovieComboBox;
    delete this->mSoundComboBox;
    delete this->mInstructionsLabel;
    delete this->mInstructionsEditBox;
    delete this->mAftermathLabel;
    delete this->mAftermathEditBox;
    delete this->mHintsLabel;
    delete this->mHistoryLabel;
    delete this->mHintsEditBox;
    delete this->mHistoryEditBox;
    delete this->mImportButton;
    delete this->mExportButton;
    delete this->mLocalizationLabel;
    delete this->mLocalizationEditBox;
}

// FUNCTION: EXE 0x571ce3
// Fills the tab from the scenario's media strings (a file missing from a list goes into its combo's
// label) and shows the .lcl name in the import box when that file exists
void EESEInstructions::Load()
{
    FSFileSpec theFile;
    UWideString theText;
    UWideString theName;
    EEScenarioManager* theManager = ::gWorld.mScenarioManager;
    theText = theManager->mMediaStrings.mMapFile;
    if (this->mMapComboBox->SelectFile(theText) && theText.GetLength() != 0)
        this->mMapComboBox->GetTextLabel()->SetText(theText);
    theText = theManager->mMediaStrings.mMovieFile;
    this->mMovieComboBox->SelectFile(theText);
    if (this->mMovieComboBox->SelectFile(theText) && theText.GetLength() != 0)
        this->mMovieComboBox->GetTextLabel()->SetText(theText);
    theText = theManager->mMediaStrings.mSoundFile;
    if (this->mSoundComboBox->SelectFile(theText) && theText.GetLength() != 0)
        this->mSoundComboBox->GetTextLabel()->SetText(theText);
    theText = theManager->mMediaStrings.mInstructions;
    this->mInstructionsEditBox->SetText(theText);
    theText = theManager->mMediaStrings.mAftermath;
    this->mAftermathEditBox->SetText(theText);
    theText = theManager->mMediaStrings.mHints;
    this->mHintsEditBox->SetText(theText);
    theText = theManager->mMediaStrings.mHistory;
    this->mHistoryEditBox->SetText(theText);
    theText = theManager->mExportFileName;
    if (theText.GetLength() > 0) {
        theFile.SetVolume(gStartupVolume);
        theFile.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\");
        theFile.SetFile((const char*)theText);
        if (theFile.FOpen("r") == 0) {
            theName.CopyString(theText, theText.GetLength() - 4);
            this->mLocalizationEditBox->SetText(theName);
            theFile.Close();
        } else {
            this->mLocalizationEditBox->SetText(gEmptyString);
        }
    } else {
        this->mLocalizationEditBox->SetText(gEmptyString);
    }
}

// FUNCTION: EXE 0x571f62
// Rescans the movie, sound and map file lists when the tab is entered
void EESEInstructions::Enter()
{
    if (this->mMovieComboBox != NULL)
        this->mMovieComboBox->CreateEntries();
    if (this->mSoundComboBox != NULL)
        this->mSoundComboBox->CreateEntries();
    if (this->mMapComboBox != NULL)
        this->mMapComboBox->CreateEntries();
}

// FUNCTION: EXE 0x571f8b
// Stores the chosen map, movie and sound files and the four texts into the scenario's media strings
void EESEInstructions::Store()
{
    UWideString theText;
    EEScenarioManager* theManager = ::gWorld.mScenarioManager;
    theText = this->mMapComboBox->GetTextLabel()->GetText(0);
    theManager->mMediaStrings.mMapFile = theText;
    theText = this->mMovieComboBox->GetTextLabel()->GetText(0);
    theManager->mMediaStrings.mMovieFile = theText;
    theText = this->mSoundComboBox->GetTextLabel()->GetText(0);
    theManager->mMediaStrings.mSoundFile = theText;
    this->mInstructionsEditBox->GetAllText(theText);
    theManager->mMediaStrings.mInstructions = theText;
    this->mAftermathEditBox->GetAllText(theText);
    theManager->mMediaStrings.mAftermath = theText;
    this->mHintsEditBox->GetAllText(theText);
    theManager->mMediaStrings.mHints = theText;
    this->mHistoryEditBox->GetAllText(theText);
    theManager->mMediaStrings.mHistory = theText;
}

// FUNCTION: EXE 0x5720c6
// Does nothing (the shared empty body; EESETSBaseObject::RefreshControls and EEPhysicsTrace bind here too)
void EEABuild::Virtual8()
{
}

// FUNCTION: EXE 0x5720c7
// Destroys the editor's main group and frees it when asked (EEUIFSEMainGroup's scalar deleting destructor)

// FUNCTION: EXE 0x57210f
// Creates an empty list of wide strings (vector<UWideString> default constructor, template COMDAT)

// ICF-FOLDED: EXE 0x556dd8
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EESEInstructions::MouseMove(class GE2DPoint const &)
{
}
