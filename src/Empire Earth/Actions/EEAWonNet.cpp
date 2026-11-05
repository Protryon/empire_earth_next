// C:\EEX\Empire Earth\Actions\EEAWonNet.cpp (file name inferred: no __FILE__; RTTI class EEAWonNet, vtable
// 0x43914c). The WON lobby's UI-thread action: 0x52f3..0x5d47 (EEAUserPauseGame's Virtual4 ends at 0x52f3; the
// EECPASGU* group starts at 0x5d47). ??_G, three ctors (called from EEWonNet.cpp), the dtor, Virtual4 (feeds the room
// list and game room forms) with its two static strings' atexit dtors, and the games box's GetCurrentSelection and
// GetDisplayLine (first user).
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <string>
#include <list>

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
#include "lle/UISlider.h"
#include "lle/UIScrollBar.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EEUICWonNetGamesBox.h"
#include "ee/EEUICWonNetRoomsBox.h"
#include "ee/EEAction.h"
#include "ee/EEAWonNet.h"
#include "ee/EEStringIDs.h"
// SYNTHETIC: EXE 0x4052f3
// (the scalar deleting destructor)

// FUNCTION: EXE 0x40530f
// Creates a WON lobby action for a list box line (room or user list)
EEAWonNet::EEAWonNet(long inType, UIListBox<UWideString>* inList, const UWideString& inText, bool inHasPassword)
{
    this->mType = inType;
    this->mPrefix = UWideString("");
    this->mText2C = UWideString("");
    this->mRoomName = UWideString("");
    this->mList = inList;
    this->mChat = NULL;
    this->mText = inText;
    this->mIsFromMe = false;
    this->mHumans = 0;
    this->mHasPassword = inHasPassword;
    this->mServerName = UWideString("");
    this->mL8C = 0;
    this->mPlayers = 0;
    this->mIsInProgress = false;
    this->mIsPrivate = false;
}

// FUNCTION: EXE 0x405418
// Creates a WON lobby action for a game list line (players, password, private and in-progress flags)
EEAWonNet::EEAWonNet(long inType, const UWideString& inText, long inHumans, long inPlayers, bool inHasPassword, bool inIsPrivate,
                     bool inIsInProgress)
{
    this->mType = inType;
    this->mPrefix = UWideString("");
    this->mText2C = UWideString("");
    this->mRoomName = UWideString("");
    this->mList = NULL;
    this->mChat = NULL;
    this->mText = inText;
    this->mIsFromMe = false;
    this->mHasPassword = inHasPassword;
    this->mHumans = inHumans;
    this->mServerName = UWideString("");
    this->mL8C = 0;
    this->mPlayers = inPlayers;
    this->mIsInProgress = inIsInProgress;
    this->mIsPrivate = inIsPrivate;
}

// FUNCTION: EXE 0x405523
// Creates a WON lobby action for a chat line (prefix, text, whether it is ours)
EEAWonNet::EEAWonNet(long inType, UITextLabel* inChat, const UWideString& inPrefix, const UWideString& inText,
                     bool inIsFromMe)
{
    this->mType = inType;
    this->mPrefix = inPrefix;
    this->mText2C = UWideString("");
    this->mRoomName = UWideString("");
    this->mList = NULL;
    this->mChat = inChat;
    this->mText = inText;
    this->mL80 = 0;
    this->mIsFromMe = inIsFromMe;
    this->mServerName = UWideString("");
    this->mL8C = 0;
    this->mPlayers = 0;
    this->mIsInProgress = false;
    this->mIsPrivate = false;
}

// FUNCTION: EXE 0x405611
// Destroys the WON lobby action
EEAWonNet::~EEAWonNet()
{
}

// NEAR: the original refuses more inlines (UIListBox<UWideString>::GetDisplayLine via the import, the games box's
// GetDisplayLine 0x5d2d and _Ptrit::operator+ 0x28a137 out of line), so theGames lives at [ebp-8] and the frame is 8
// bytes larger; source shape otherwise follows the original's block order.
// FUNCTION: EXE 0x40564a
// Applies a WON lobby update on the UI thread: room and user lists, games box lines and selection, chat text
void EEAWonNet::Virtual4()
{
    bool theAtEnd;
    unsigned long theSelection;
    static UWideString sSelectedGame("");
    static UWideString sUnused("");
    static unsigned long sGameCount;
    static unsigned long sGameSelection;
    static unsigned long sGameDisplayLine;
    static unsigned long sListDisplayLine;
    EEUICWNRBLine theRoom;
    EEUICWNGBLine theGame;
    UWideString theLabel;
    UWideString theFormat;
    switch (this->mType) {
    case 8:
        if (gUIManager->GetCurrentForm() == gUIManager->GetFormByID(kFormAvailableRooms)) {
            gResource->GetString(IDS_SETUP_PLAYERS, theFormat);
            sprintf(theLabel, "%d  (%d ", this->mL8C, this->mPlayers);
            theLabel += theFormat;
            theLabel += UWideString(")");
            static_cast<UITextLabel*>(gUIManager->GetFormByID(kFormAvailableRooms)->GetControl(kAvailableRoomsTotalGamesNumber))->SetText(theLabel);
            static_cast<UITextLabel*>(gUIManager->GetFormByID(kFormAvailableRooms)->GetControl(kAvailableRoomsServersNameLabel))->SetText(this->mServerName);
        }
        break;
    case 7:
        if (gUIManager->GetCurrentForm() != gUIManager->GetFormByID(kFormLobby))
            break;
        this->mChat = static_cast<UITextLabel*>(gUIManager->GetCurrentForm()->GetControl(8));
    case 6: {
        if (!this->mIsFromMe && this->mChat->GetScrollBar() &&
            this->mChat->GetScrollBar()->GetSlider()->GetMaxRange() !=
                this->mChat->GetScrollBar()->GetSlider()->GetCurrentRangeValue())
            theAtEnd = false;
        else
            theAtEnd = true;
        theLabel = this->mPrefix;
        theLabel += this->mText;
        this->mChat->AppendText(theLabel);
        if (theAtEnd == true && this->mChat->GetScrollBar())
            this->mChat->SetStartingDisplayLine(this->mChat->GetScrollBar()->GetSlider()->GetMaxRange());
        break;
    }
    case 4:
        if (gUIManager->GetCurrentForm() == gUIManager->GetFormByID(kFormAvailableGames))
            this->mList = (UIListBox<UWideString>*)gUIManager->GetCurrentForm()->GetControl(8);
        else
            this->mList = NULL;
        if (this->mList) {
            if (this->mText == UWideString("@@@@")) {
                sListDisplayLine = this->mList->GetDisplayLine();
                this->mList->DeleteAllEntries();
            } else if (this->mText == UWideString("@@@")) {
                if (this->mList->GetLineCount() > sListDisplayLine)
                    this->mList->SetStartingDisplayLine(sListDisplayLine);
            } else {
                this->mList->AppendLine(this->mText);
            }
        }
        break;
    case 3:
        if (gUIManager->GetCurrentForm() == gUIManager->GetFormByID(kFormAvailableGames)) {
            EEUICWonNetGamesBox* theGames =
                dynamic_cast<EEUICWonNetGamesBox*>(gUIManager->GetFormByID(kFormAvailableGames)->GetControl(kAvailableGamesAvalGamesListBox));
            if (this->mText == UWideString("@@@@")) {
                if (theGames->GetLineCount() > 0) {
                    sGameDisplayLine = theGames->GetDisplayLine();
                    if (theGames->GetCurrentSelection(theSelection) == kErrorOK)
                        sSelectedGame = (theGames->Begin() + theSelection)->mName;
                    else
                        sSelectedGame = "";
                    theGames->DeleteAllEntries();
                } else {
                    sGameDisplayLine = 0;
                    sSelectedGame = "";
                }
                sGameCount = 0;
            } else if (this->mText == UWideString("@@@")) {
                if (theGames->GetLineCount() > sGameDisplayLine) {
                    theGames->SetStartingDisplayLine(sGameDisplayLine);
                    theGames->ChangeSelectionIndex(sGameSelection, true, false);
                }
                sGameCount = 0;
                sGameSelection = 0;
            } else {
                theGame.mName = this->mText;
                theGame.mIsInProgress = this->mIsInProgress;
                theGame.mPlayers = this->mPlayers;
                theGame.mHumans = this->mHumans;
                theGame.mHasPassword = this->mHasPassword;
                theGame.mIsPrivate = this->mIsPrivate;
                theGames->AppendLine(theGame);
                if (sSelectedGame != "") {
                    if (sSelectedGame == this->mText)
                        sGameSelection = sGameCount;
                } else {
                    theGames->ChangeSelectionIndex(0, true, false);
                }
                sGameCount++;
            }
        }
        break;
    case 2:
        if (gUIManager->GetCurrentForm() == gUIManager->GetFormByID(kFormAvailableRooms)) {
            EEUICWonNetRoomsBox* theRooms =
                dynamic_cast<EEUICWonNetRoomsBox*>(gUIManager->GetFormByID(kFormAvailableRooms)->GetControl(kAvailableRoomsAvailRoomsListBox));
            if (this->mRoomName.GetLength() == 0) {
                theRooms->DeleteAllEntries();
            } else {
                theRoom.mNumPlayers = this->mHumans;
                theRoom.mDescription = this->mText;
                theRoom.mName = this->mRoomName;
                theRoom.mNumGames = this->mL8C;
                theRooms->AppendLine(theRoom);
            }
        }
        break;
    }
}

// FUNCTION: EXE 0x405ce1
// Destroys Virtual4's unused static string at exit
// (Virtual4's sUnused atexit dtor)

// FUNCTION: EXE 0x405cec
// Destroys Virtual4's static selected-game string at exit
// (Virtual4's sSelectedGame atexit dtor)

// FUNCTION: EXE 0x405cf7
// Returns the games box's current selection
// (UIListBox<EEUICWNGBLine>::GetCurrentSelection: template member, first user)

// NEAR (missing): inline in our build (see Virtual4)
// FUNCTION: EXE 0x405d2d
// Returns the games box's first displayed line
// (UIListBox<EEUICWNGBLine>::GetDisplayLine: inline, first user)

// ICF-FOLDED: EXE 0x5720c6
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEAWonNet::Virtual8()
{
}
