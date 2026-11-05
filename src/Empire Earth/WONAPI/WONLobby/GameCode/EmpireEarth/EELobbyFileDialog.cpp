// C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\EELobbyFileDialog.cpp (__FILE__ 0x8d0540, InitComponent's
// lines 37..40). The game room's saved-game dialog: the files of a directory with one extension, loaded or deleted
// with its buttons: 0x290e89..0x291463 (GUIString's size helpers follow their first user; WONFile's implicit dtor
// closes it; the SendFileOp unit follows). Module flags (r12-misc-h7, round 12).
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
EELobbyFileDialog::EELobbyFileDialog()
{
}

// FUNCTION: EXE 0x690efc
EELobbyFileDialog::~EELobbyFileDialog()
{
}

// FUNCTION: EXE 0x690f66
void EELobbyFileDialog::InitComponent(ComponentInit& inInit)
{
    if (inInit.mType == 2) {
        ComponentConfig* theConfig = ((ComponentConfigInit&)inInit).mConfig;
        SafeGetComponent(theConfig, mFileList, "FileList", __FILE__, __LINE__);
        SafeGetComponent(theConfig, mLoadButton, "LoadButton", __FILE__, __LINE__);
        SafeGetComponent(theConfig, mCancelButton, "CancelButton", __FILE__, __LINE__);
        SafeGetComponent(theConfig, mDeleteButton, "DeleteButton", __FILE__, __LINE__);
    }
}

// FUNCTION: EXE 0x690fd9
void EELobbyFileDialog::ListFiles(const std::string& inDirectory, const std::string& inExtension)
{
    mFileList->Fn2FAA00();
    mFileList->Fn2FAA50(true);
    mDirectory = inDirectory;
    mExtension = inExtension;
    if (mDirectory.size() != 0 && mDirectory[mDirectory.size() - 1] != '/' && mDirectory[mDirectory.size() - 1] != '\\')
        mDirectory += '\\';
    std::string thePattern = mDirectory + '*' + inExtension;
    PlatFindData theData;
    PlatFind* theFind = PlatFindFirst(thePattern.c_str(), &theData);
    if (theFind != NULL) {
        do {
            GUIString theName(theData.mName);
            if (theName.length() > inExtension.size()) {
                theName.resize(theName.length() - inExtension.size());
                mFileList->InsertString(theName, -1);
            }
        } while (PlatFindNext(theFind, &theData) == PLAT_FILE_OK);
        PlatFindClose(theFind);
    }
    mFileList->Fn2FAA10();
}

// FUNCTION: EXE 0x69115e
GUIString::GUIString(const char* inText) : mData(new GUIStringData(inText))
{
}

// FUNCTION: EXE 0x69119a
void GUIString::resize(int inLength)
{
    CopyOnWrite();
    GUIStringData* theData = mData.mPtr;
    theData->Reserve(inLength, false);
    theData->mLength = inLength;
    theData->mText[theData->mLength] = 0;
}

// FUNCTION: EXE 0x6911c9
void GUIStringData::Reserve(int inLength, bool inGrow)
{
    inLength++;
    if (mCapacity < inLength) {
        wchar_t* theOld = mText;
        if (inGrow)
            inLength += inLength;
        mText = new wchar_t[inLength];
        if (theOld != NULL) {
            memcpy(mText, theOld, (mLength + 1) * sizeof(wchar_t));
            delete[] theOld;
        }
        mCapacity = inLength;
    }
}

// FUNCTION: EXE 0x69121b
void GUIString::CopyOnWrite()
{
    if (mData->GetRefCount() > 1)
        mData = new GUIStringData(*mData.mPtr);
}

// FUNCTION: EXE 0x691249
void EELobbyFileDialog::LoadSelectedFile()
{
    std::string theFile = GetSelectedFile();
    if (theFile.size() != 0 && EELobbyLoadSavedGame(theFile.c_str()))
        GetRoot()->Fn32B0A0(100001, this);
}

// FUNCTION: EXE 0x69129a
std::string EELobbyFileDialog::GetSelectedFile()
{
    ListItem* theItem = mFileList->mSelItem;
    if (theItem == NULL)
        return "";
    std::string theText = ((TextListItem*)theItem)->mText.mData->GetString();
    std::string theName = theText;
    return mDirectory + theName + mExtension;
}

// FUNCTION: EXE 0x69136a
void EELobbyFileDialog::HandleComponentEvent(ComponentEvent* inEvent)
{
    if (inEvent->mType == 15) {
        if (inEvent->mComponent == mDeleteButton.mPtr) {
            if (LobbyMessageBoxYesNo(gEELobbyFileDialog_DeleteTitle, gEELobbyFileDialog_DeleteDetails, true) == 100001) {
                WONFile theFile(GetSelectedFile());
                theFile.Delete(true);
                mFileList->RemoveItem(mFileList->mSelItem);
                return;
            }
        } else if (inEvent->mComponent == mLoadButton.mPtr) {
            LoadSelectedFile();
            return;
        }
    } else if (inEvent->mType == 23) {
        ListItemClickedEvent* theEvent = (ListItemClickedEvent*)inEvent;
        if (theEvent->mDoubleClick && theEvent->mParam == 0)
            LoadSelectedFile();
        return;
    }
    Dialog::HandleComponentEvent(inEvent);
}

// The unit's compiler-generated functions:
// SYNTHETIC: EXE 0x690ee0
// (??_G EELobbyFileDialog)
// SYNTHETIC: EXE 0x69141e
// (WONFile's implicit dtor)
