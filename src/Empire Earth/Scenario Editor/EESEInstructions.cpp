// C:\EEX\Empire Earth\Scenario Editor\EESEInstructions.cpp (__FILE__ in the ctor's ESVERIFYs). 0x1705a2..0x172125: the text
// import answer and button (now in EESEIAExport.cpp with the other import and export classes), the instructions tab
// and the ??_G of its events, with the functions of EEABuild.cpp folded in. Unit flags: the exe module set.
#include <windows.h>
#include <string>
#include "lle/LowLevelEngine.h"
#include "lle/GERhombus.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUIShell.h"
#include "ee/EESEARust.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"

// ---- the tab's host (editor_instructions.rs: the ops from 700; editor_trigx.rs composes the sequences over the
// single statements below, EE_SCREENS_SEINX_*)

// (a struct for EESMMediaStrings' friendship: Load's and Store's transfers)
struct EESEInstructionsHost {
    static UWideString& Media(int32_t inString);
    static int32_t Statement(uint32_t inOp, void* const* p, int32_t* i, float* f);
};

namespace {

const char* const kSeinDirectories[3] = {"Scenarios\\", "Sounds\\", "Movies\\"};
const unsigned long kSeinFileIDs[3] = {0x44d, 0x44f, 0x451};

// the scenarios' *.jpg, the movies' *.mpg, the sounds' *.mp3
UString SeinPattern(int32_t inFiles)
{
    return inFiles == EE_SCREENS_SEIN_FILES_MAP     ? UString("*") + ".jpg"
           : inFiles == EE_SCREENS_SEIN_FILES_MOVIE ? UString("*") + ".mpg"
                                                    : UString("*.mp3");
}

// the originals' locals: the three directories, the export file, the text and the name moved between the media
// strings and the controls
FSFileSpec sSeinDirectories[3];
FSFileSpec sSeinFile;
UWideString sSeinText;
UWideString sSeinName;

}   // namespace

// the media string i (EE_SCREENS_SEINX_M_*)
UWideString& EESEInstructionsHost::Media(int32_t inString)
{
    EESMMediaStrings& m = ::gWorld.mScenarioManager->mMediaStrings;
    switch (inString) {
    case EE_SCREENS_SEINX_M_MAP: return m.mMapFile;
    case EE_SCREENS_SEINX_M_MOVIE: return m.mMovieFile;
    case EE_SCREENS_SEINX_M_SOUND: return m.mSoundFile;
    case EE_SCREENS_SEINX_M_INSTRUCTIONS: return m.mInstructions;
    case EE_SCREENS_SEINX_M_AFTERMATH: return m.mAftermath;
    case EE_SCREENS_SEINX_M_HINTS: return m.mHints;
    }
    return m.mHistory;
}

int32_t EESEInstructionsHost::Statement(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    void** theOut = static_cast<void**>(p[2]);
    switch (inOp) {
    case EE_SCREENS_SEIN_OP_FILE_ENTRIES: EEUIFileComboEntries(static_cast<UIControl*>(p[0])); return 0;
    case EE_SCREENS_SEINX_DIR_RESET: sSeinDirectories[i[0]] = FSFileSpec(); return 0;
    case EE_SCREENS_SEINX_DIR_VOLUME: sSeinDirectories[i[0]].SetVolume(gStartupVolume); return 0;
    case EE_SCREENS_SEINX_DIR_DIRECTORY:
        sSeinDirectories[i[0]].SetDirectory(gStartupDirectory + "data\\" + kSeinDirectories[i[0]]);
        return 0;
    case EE_SCREENS_SEINX_NEW_PATH: *theOut = new FSPath; return 0;
    case EE_SCREENS_SEINX_ADD_DIRECTORY:
        static_cast<FSPath*>(p[0])->AddDirectory(sSeinDirectories[i[0]], true);
        return 0;
    case EE_SCREENS_SEINX_IS_DIRECTORY: return sSeinDirectories[i[0]].IsDirectory();
    case EE_SCREENS_SEINX_CREATE_DIRECTORY:
        ESVERIFY_AT(sSeinDirectories[i[0]].CreateDirectory() == keseOK, 536 + 3 * i[0]);
        return 0;
    case EE_SCREENS_SEINX_PATH_RELEASE: static_cast<FSPath*>(p[0])->Dereference(); return 0;
    case EE_SCREENS_SEINX_NEW_FILE_COMBO:
        *theOut = EEUIShellNewFileCombo(static_cast<FSPath*>(p[0]), SeinPattern(i[0]), kSeinFileIDs[i[0]], gSeplScreenFont,
                                        GE2DPoint(f[0], f[1]), 150, static_cast<UIForm*>(p[1]), (unsigned long)i[1]);
        return 0;
    case EE_SCREENS_SEINX_COMBO_GETEXT: *theOut = static_cast<UIComboBox*>(p[0])->GetGEText(); return 0;
    case EE_SCREENS_SEINX_YELLOW:
        static_cast<GEText*>(p[0])->SetFgColor(GEColor(0.99607843f, 0.96862745f, 0.1254902f));
        return 0;
    case EE_SCREENS_SEINX_LIST_BLUE:
        static_cast<UIComboBox*>(p[0])->SetListBoxSelectedBgColor(GEColor(0.0f, 0.0f, 1.0f));
        return 0;
    case EE_SCREENS_SEINX_BOX_OPACITY: static_cast<UIComboBox*>(p[0])->SetTextBoxBgOpacity(0.4f); return 0;
    case EE_SCREENS_SEINX_ADD_CONTROL:
        static_cast<UIForm*>(p[0])->AddControl(static_cast<UIComboBox*>(p[1]), 7);
        return 0;
    case EE_SCREENS_SEINX_NEW_IMPORT:
        *theOut = static_cast<UIEvent*>(new EESEERust(EE_SCREENS_SEE_TEXT_IMPORT));
        return 0;
    case EE_SCREENS_SEINX_NEW_EXPORT:
        *theOut = static_cast<UIEvent*>(new EESEERust(EE_SCREENS_SEE_TEXT_EXPORT));
        return 0;
    case EE_SCREENS_SEINX_SET_BUTTON_UP:
        static_cast<UIControl*>(p[0])->SetTriggerEvent(kControlTriggerButtonUp, static_cast<UIEvent*>(p[1]));
        return 0;
    case EE_SCREENS_SEINX_EVENT_RELEASE: static_cast<UIEvent*>(p[0])->Dereference(); return 0;
    case EE_SCREENS_SEINX_TEXT_MEDIA: sSeinText = Media(i[0]); return 0;
    case EE_SCREENS_SEINX_TEXT_EXPORT: sSeinText = ::gWorld.mScenarioManager->mExportFileName; return 0;
    case EE_SCREENS_SEINX_FILE_SELECT: return EEUIFileComboSelect(static_cast<UIComboBox*>(p[0]), sSeinText);
    case EE_SCREENS_SEINX_TEXT_LENGTH: return (int32_t)sSeinText.GetLength();
    case EE_SCREENS_SEINX_LABEL_TEXT: static_cast<UIComboBox*>(p[0])->GetTextLabel()->SetText(sSeinText); return 0;
    case EE_SCREENS_SEINX_EDIT_MEDIA: static_cast<UIEditBox*>(p[0])->SetText(Media(i[0])); return 0;
    case EE_SCREENS_SEINX_EDIT_ALL_TEXT: static_cast<UIEditBox*>(p[0])->GetAllText(sSeinText); return 0;
    case EE_SCREENS_SEINX_SET_MEDIA: Media(i[0]) = sSeinText; return 0;
    case EE_SCREENS_SEINX_SET_MEDIA_LABEL:
        Media(i[0]) = static_cast<UIComboBox*>(p[0])->GetTextLabel()->GetText(0);
        return 0;
    case EE_SCREENS_SEINX_FILE_RESET: sSeinFile = FSFileSpec(); return 0;
    case EE_SCREENS_SEINX_FILE_VOLUME: sSeinFile.SetVolume(gStartupVolume); return 0;
    case EE_SCREENS_SEINX_FILE_DIRECTORY:
        sSeinFile.SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\");
        return 0;
    case EE_SCREENS_SEINX_FILE_NAME: sSeinFile.SetFile((const char*)sSeinText); return 0;
    case EE_SCREENS_SEINX_FILE_OPEN: return sSeinFile.FOpen("r") == 0;
    case EE_SCREENS_SEINX_FILE_CLOSE: sSeinFile.Close(); return 0;
    case EE_SCREENS_SEINX_NAME_STEM: sSeinName.CopyString(sSeinText, sSeinText.GetLength() - 4); return 0;
    case EE_SCREENS_SEINX_EDIT_STEM: static_cast<UIEditBox*>(p[0])->SetText(sSeinName); return 0;
    case EE_SCREENS_SEINX_EDIT_EMPTY: static_cast<UIEditBox*>(p[0])->SetText(gEmptyString); return 0;
    }
    return 0;
}


extern "C" int32_t ee_screenscpp_seinx(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    return EESEInstructionsHost::Statement(inOp, p, i, f);
}
