// C:\EEX\Empire Earth\Scenario Editor\EESEInstructions.cpp (__FILE__ in the ctor's ESVERIFYs). 0x1705a2..0x172125: the text
// import answer and button, the instructions tab and the ??_G of its events, with the functions of
// EEABuild.cpp folded in. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEABuild.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERhombus.h"
#include "lle/GEText.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EEUIFileComboBox.h"
#include "ee/EESEIAExport.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EESEInstructions.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEStringIDs.h"
#include "rust/ee_screens.h"
extern GEFont* gU531938;                    // 0x531938 (Empire Earth.cpp's fonts)
extern GEFont* gSeplScreenFont;   // the screen's font while a tab makes its controls (EESERBUnitTypes.cpp)
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
// Creates the instructions tab: map/movie/sound file combos (directories created when missing), the text boxes and
// labels, and the text import/export buttons. History reuses the hints label/box ids (editor_instructions.rs)
EESEInstructions::EESEInstructions(unsigned long& ioIndex, UIForm* inForm, GEFont* inFont)
{
    gSeplScreenFont = inFont;
    uint32_t theIndex = (uint32_t)ioIndex;
    this->mRust = ee_screens_sein_new(inForm, &theIndex);
    ioIndex = theIndex;
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
// Destroys the instructions tab and every control it created (editor_instructions.rs)
EESEInstructions::~EESEInstructions()
{
    ee_screens_sein_free(this->mRust);
}

// FUNCTION: EXE 0x571ce3
// Fills the tab from the scenario's media strings (a file missing from a list goes into its combo's label) and shows
// the .lcl name in the import box when that file exists (editor_instructions.rs; the transfers are the host's)
void EESEInstructions::Load()
{
    ee_screens_sein_call(this->mRust, EE_SCREENS_SEIN_LOAD);
}

// FUNCTION: EXE 0x571f62
// Rescans the movie, sound and map file lists when the tab is entered (editor_instructions.rs)
void EESEInstructions::Enter()
{
    ee_screens_sein_call(this->mRust, EE_SCREENS_SEIN_ENTER);
}

// FUNCTION: EXE 0x571f8b
// Stores the chosen map, movie and sound files and the four texts into the scenario's media strings
// (editor_instructions.rs; the transfers are the host's)
void EESEInstructions::Store()
{
    ee_screens_sein_call(this->mRust, EE_SCREENS_SEIN_STORE);
}

// ---- the tab's host (editor_instructions.rs: the ops from 700)

int32_t EESEInstructions::Host(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    switch (inOp) {
    case EE_SCREENS_SEIN_OP_PATHS: {
        const char* theNames[3] = {"Scenarios\\", "Sounds\\", "Movies\\"};
        FSFileSpec theDirectories[3];
        FSPath** thePaths = static_cast<FSPath**>(p[0]);
        for (int k = 0; k < 3; k++) {
            theDirectories[k].SetVolume(gStartupVolume);
            theDirectories[k].SetDirectory(gStartupDirectory + "data\\" + theNames[k]);
            thePaths[k] = new FSPath;
            thePaths[k]->AddDirectory(theDirectories[k], true);
        }
        for (int k = 0; k < 3; k++)
            if (!theDirectories[k].IsDirectory())
                ESVERIFY_AT(theDirectories[k].CreateDirectory() == keseOK, 536 + 3 * k);
        return 0;
    }
    case EE_SCREENS_SEIN_OP_PATHS_RELEASE:
        for (int k = 0; k < 3; k++)
            static_cast<FSPath**>(p[0])[k]->Dereference();
        return 0;
    case EE_SCREENS_SEIN_OP_MAKE_FILES: {
        // the scenarios' *.jpg, the movies' *.mpg, the sounds' *.mp3 (the paths: scenarios, sounds, movies)
        FSPath* const* thePaths = static_cast<FSPath* const*>(p[1]);
        static const unsigned long kIDs[3] = {0x44d, 0x44f, 0x451};
        FSPath* thePath = thePaths[i[0] == EE_SCREENS_SEIN_FILES_MAP ? 0 : i[0] == EE_SCREENS_SEIN_FILES_MOVIE ? 2 : 1];
        UString thePattern = i[0] == EE_SCREENS_SEIN_FILES_MAP     ? UString("*") + ".jpg"
                             : i[0] == EE_SCREENS_SEIN_FILES_MOVIE ? UString("*") + ".mpg"
                                                                   : UString("*.mp3");
        EEUIFileComboBox* t = new EEUIFileComboBox(thePath, thePattern, kIDs[i[0]], 1, std::vector<UWideString>(),
                                                   gSeplScreenFont, GE2DPoint(f[0], f[1]), 150, 400,
                                                   static_cast<UIForm*>(p[2]), (unsigned long)i[1], 0, 2,
                                                   i[0] != EE_SCREENS_SEIN_FILES_MAP);
        GEText* theGEText = t->GetGEText();
        theGEText->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
        theGEText->SetIsTransparent(true);
        t->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
        t->SetTextBoxBgOpacity(0.4f);
        static_cast<UIForm*>(p[2])->AddControl(t, 7);
        *static_cast<UIControl**>(p[0]) = t;
        return 0;
    }
    case EE_SCREENS_SEIN_OP_BUTTON_EVENT: {
        UIEvent* theEvent = i[0] == 0 ? static_cast<UIEvent*>(new EESEIEImport) : new EESEIEExport;
        static_cast<UIControl*>(p[0])->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
        theEvent->Dereference();
        return 0;
    }
    case EE_SCREENS_SEIN_OP_FILE_ENTRIES: static_cast<EEUIFileComboBox*>(p[0])->CreateEntries(); return 0;
    case EE_SCREENS_SEIN_OP_LOAD: {
        UIControl* const* c = static_cast<UIControl* const*>(p[0]);
        EEUIFileComboBox* theMap = static_cast<EEUIFileComboBox*>(c[EE_SCREENS_SEIN_C_MAP]);
        EEUIFileComboBox* theMovie = static_cast<EEUIFileComboBox*>(c[EE_SCREENS_SEIN_C_MOVIE]);
        EEUIFileComboBox* theSound = static_cast<EEUIFileComboBox*>(c[EE_SCREENS_SEIN_C_SOUND]);
        UIEditBox* theLocalization = static_cast<UIEditBox*>(c[EE_SCREENS_SEIN_C_LOCALIZATION]);
        FSFileSpec theFile;
        UWideString theText;
        UWideString theName;
        EEScenarioManager* theManager = ::gWorld.mScenarioManager;
        theText = theManager->mMediaStrings.mMapFile;
        if (theMap->SelectFile(theText) && theText.GetLength() != 0)
            theMap->GetTextLabel()->SetText(theText);
        theText = theManager->mMediaStrings.mMovieFile;
        theMovie->SelectFile(theText);
        if (theMovie->SelectFile(theText) && theText.GetLength() != 0)
            theMovie->GetTextLabel()->SetText(theText);
        theText = theManager->mMediaStrings.mSoundFile;
        if (theSound->SelectFile(theText) && theText.GetLength() != 0)
            theSound->GetTextLabel()->SetText(theText);
        static_cast<UIEditBox*>(c[EE_SCREENS_SEIN_C_INSTRUCTIONS])->SetText(theManager->mMediaStrings.mInstructions);
        static_cast<UIEditBox*>(c[EE_SCREENS_SEIN_C_AFTERMATH])->SetText(theManager->mMediaStrings.mAftermath);
        static_cast<UIEditBox*>(c[EE_SCREENS_SEIN_C_HINTS])->SetText(theManager->mMediaStrings.mHints);
        static_cast<UIEditBox*>(c[EE_SCREENS_SEIN_C_HISTORY])->SetText(theManager->mMediaStrings.mHistory);
        theText = theManager->mExportFileName;
        if (theText.GetLength() > 0) {
            theFile.SetVolume(gStartupVolume);
            theFile.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\");
            theFile.SetFile((const char*)theText);
            if (theFile.FOpen("r") == 0) {
                theName.CopyString(theText, theText.GetLength() - 4);
                theLocalization->SetText(theName);
                theFile.Close();
            } else
                theLocalization->SetText(gEmptyString);
        } else
            theLocalization->SetText(gEmptyString);
        return 0;
    }
    case EE_SCREENS_SEIN_OP_STORE: {
        UIControl* const* c = static_cast<UIControl* const*>(p[0]);
        UWideString theText;
        EESMMediaStrings& theMedia = ::gWorld.mScenarioManager->mMediaStrings;
        theMedia.mMapFile = static_cast<UIComboBox*>(c[EE_SCREENS_SEIN_C_MAP])->GetTextLabel()->GetText(0);
        theMedia.mMovieFile = static_cast<UIComboBox*>(c[EE_SCREENS_SEIN_C_MOVIE])->GetTextLabel()->GetText(0);
        theMedia.mSoundFile = static_cast<UIComboBox*>(c[EE_SCREENS_SEIN_C_SOUND])->GetTextLabel()->GetText(0);
        static_cast<UIEditBox*>(c[EE_SCREENS_SEIN_C_INSTRUCTIONS])->GetAllText(theText);
        theMedia.mInstructions = theText;
        static_cast<UIEditBox*>(c[EE_SCREENS_SEIN_C_AFTERMATH])->GetAllText(theText);
        theMedia.mAftermath = theText;
        static_cast<UIEditBox*>(c[EE_SCREENS_SEIN_C_HINTS])->GetAllText(theText);
        theMedia.mHints = theText;
        static_cast<UIEditBox*>(c[EE_SCREENS_SEIN_C_HISTORY])->GetAllText(theText);
        theMedia.mHistory = theText;
        return 0;
    }
    }
    return 0;
}

extern "C" int32_t ee_screenscpp_sein(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    return EESEInstructions::Host(inOp, p, i, f);
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
