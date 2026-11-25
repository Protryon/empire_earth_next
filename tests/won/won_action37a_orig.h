// EEAWonNet as it was before R6 WON lobby and network round 37 (include/ee/EEAWonNet.h's class and
// src/Empire Earth/Actions/EEAWonNet.cpp's three constructors, destructor and Virtual4 at 0b96ea220), for
// won_action37a_test.cpp: the text is the game's but Virtual4's function statics, which are the driver's file
// statics here (sSelectedGame .. sListDisplayLine) so it can print them and start every sequence alike.
class EEAWonNet : public EEAction {
public:
    // 0x530f (EEWonNet 0xcfdab..: type 4, the game room's player list)
    EEAWonNet(long inType, UIListBox<UWideString>* inList, const UWideString& inText, bool inHasPassword);
    // 0x5418 (EEWonNet 0xcfb27..: type 3, one game line)
    EEAWonNet(long inType, const UWideString& inText, long inHumans, long inPlayers, bool inHasPassword, bool inIsPrivate, bool inIsInProgress);
    // 0x5523 (EEWonNet 0xd05ca/0xd0c6b: types 6/7, a chat line)
    EEAWonNet(long inType, UITextLabel* inChat, const UWideString& inPrefix, const UWideString& inText, bool inIsFromMe);
    virtual ~EEAWonNet();
    virtual void Virtual4();
    virtual void Virtual8();                // +0x08 (the shared empty slot 0x1720c6)

    UIListBox<UWideString>* mList;          // +0x0c (type 4)
    UITextLabel* mChat;                     // +0x10 (types 6/7)
    long mType;                             // +0x14 (Virtual4's switch: 2 rooms, 3 games, 4 list, 6/7 chat, 8 room labels)
    UWideString mPrefix;                    // +0x18 (chat: the speaker)
    UWideString mText2C;                    // +0x2c
    UWideString mRoomName;                  // +0x40 (type 2: the rooms box line's name; empty clears the box)
    UWideString mServerName;                // +0x54 (type 8: label 16)
    UWideString mText;                      // +0x68 ("@@@@" / "@@@" bracket a refill)
    char _gap7C[4];
    long mL80;                              // +0x80 (only the chat ctor clears it)
    long mHumans;                           // +0x84
    long mPlayers;                          // +0x88
    long mL8C;                              // +0x8c
    bool mHasPassword;                      // +0x90 (the games line's +0x1c)
    bool mIsFromMe;                         // +0x91 (chat: do not scroll to the end)
    bool mIsInProgress;                     // +0x92 (the games line's +0x1e)
    bool mIsPrivate;                        // +0x93 (the games line's +0x1d)
};


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

EEAWonNet::~EEAWonNet()
{
}

void EEAWonNet::Virtual4()
{
    bool theAtEnd;
    unsigned long theSelection;
    EEUICWNRBLine theRoom;
    EEUICWNGBLine theGame;
    UWideString theLabel;
    UWideString theFormat;
    switch (this->mType) {
    case 8:
        if (gUIManager->GetCurrentForm() == gUIManager->GetFormByID(kFormAvailableRooms)) {
            gResource->GetString(IDS_SETUP_PLAYERS, theFormat);
            sprintf(theLabel, "%ld  (%ld ", this->mL8C, this->mPlayers);
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
                        sSelectedGame = theGames->GetData(theSelection).mName;
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

void EEAWonNet::Virtual8()
{
}
