// C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\EELobbyFileDialog.cpp (__FILE__ 0x8d0540, InitComponent's
// lines 37..40). The game room's saved-game dialog: the files of a directory with one extension, loaded or deleted
// with its buttons: 0x690e89..0x691463 (GUIString's size helpers follow their first user; WONFile's implicit dtor
// closes it; the SendFileOp unit follows).
#include <windows.h>
#include <string>
#include <list>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "platform/PlatFile.h"

#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/EELobbyGameCode.h"

using namespace WONAPI;

extern GUIString gEELobbyFileDialog_DeleteTitle;       // 0x965938 (EELobbyResource.cpp)
extern GUIString gEELobbyFileDialog_DeleteDetails;     // 0x96599c (EELobbyResource.cpp)

// FUNCTION: EXE 0x690e89
// Creates the saved-game dialog (members only)
EELobbyFileDialog::EELobbyFileDialog()
{
}

// FUNCTION: EXE 0x690efc
// Destroys the saved-game dialog (members only)
EELobbyFileDialog::~EELobbyFileDialog()
{
}

// FUNCTION: EXE 0x690f66
// Looks up the file list and the load, cancel and delete buttons from the dialog's layout
void EELobbyFileDialog::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType == 2) {
        ComponentConfig* theConfig = ((ComponentConfigInit&)inInit).mConfig;
        SafeGetComponent(theConfig, this->mFileList, "FileList", __FILE__, 37);
        SafeGetComponent(theConfig, this->mLoadButton, "LoadButton", __FILE__, 38);
        SafeGetComponent(theConfig, this->mCancelButton, "CancelButton", __FILE__, 39);
        SafeGetComponent(theConfig, this->mDeleteButton, "DeleteButton", __FILE__, 40);
    }
}

// FUNCTION: EXE 0x690fd9
// Lists the files of a directory with the given extension, shown without the extension
void EELobbyFileDialog::ListFiles(const std::string& inDirectory, const std::string& inExtension)
{
    this->mFileList->BeginUpdate();
    this->mFileList->Clear(true);
    this->mDirectory = inDirectory;
    this->mExtension = inExtension;
    if (this->mDirectory.size() != 0 && this->mDirectory[this->mDirectory.size() - 1] != '/' && this->mDirectory[this->mDirectory.size() - 1] != '\\')
        this->mDirectory += '\\';
    std::string thePattern = this->mDirectory + '*' + inExtension;
    PlatFindData theData;
    PlatFind* theFind = PlatFindFirst(thePattern.c_str(), &theData);
    if (theFind != NULL) {
        do {
            GUIString theName(theData.mName);
            if (theName.length() > inExtension.size()) {
                theName.resize(theName.length() - inExtension.size());
                this->mFileList->InsertString(theName, -1);
            }
        } while (PlatFindNext(theFind, &theData) == PLAT_FILE_OK);
        PlatFindClose(theFind);
    }
    this->mFileList->EndUpdate();
}

// FUNCTION: EXE 0x691249
// Loads the selected saved game and closes the dialog with OK when that succeeds
void EELobbyFileDialog::LoadSelectedFile()
{
    std::string theFile = this->GetSelectedFile();
    if (theFile.size() != 0 && EELobbyLoadSavedGame(theFile.c_str()))
        this->GetRoot()->EndDialog(100001, this);
}

// FUNCTION: EXE 0x69129a
// Returns the full path of the selected saved game, or an empty string with nothing selected
std::string EELobbyFileDialog::GetSelectedFile()
{
    ListItem* theItem = this->mFileList->GetSelItem();
    if (theItem == NULL)
        return "";
    std::string theText = static_cast<TextListItem*>(theItem)->mText.mData->GetString();
    std::string theName = theText;
    return this->mDirectory + theName + this->mExtension;
}

// FUNCTION: EXE 0x69136a
// Deletes the selected file after a yes/no prompt, or loads it on Load or a double click
void EELobbyFileDialog::HandleComponentEvent(ComponentEvent* inEvent)
{
    if (inEvent->mType == ComponentEvent_ButtonPressed) {
        if (inEvent->mComponent == this->mDeleteButton.mPtr) {
            if (LobbyMessageBoxYesNo(gEELobbyFileDialog_DeleteTitle, gEELobbyFileDialog_DeleteDetails, true) == 100001) {
                WONFile theFile(this->GetSelectedFile());
                theFile.Delete(true);
                this->mFileList->RemoveItem(this->mFileList->GetSelItem());
                return;
            }
        } else if (inEvent->mComponent == this->mLoadButton.mPtr) {
            this->LoadSelectedFile();
            return;
        }
    } else if (inEvent->mType == ComponentEvent_ListItemClicked) {
        ListItemClickedEvent* theEvent = static_cast<ListItemClickedEvent*>(inEvent);
        if (theEvent->mDoubleClick && theEvent->mButton == 0)
            this->LoadSelectedFile();
        return;
    }
    Dialog::HandleComponentEvent(inEvent);
}

// The unit's compiler-generated functions:
// SYNTHETIC: EXE 0x690ee0
// (??_G EELobbyFileDialog)
// SYNTHETIC: EXE 0x69141e
// (WONFile's implicit dtor)
