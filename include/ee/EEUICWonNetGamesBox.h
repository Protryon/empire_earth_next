// C:\EEX\Empire Earth\User Interface Thread\EEUICWonNetGamesBox.h (RECONSTRUCTION; file name inferred). The WON game
// room's list of games (RTTI EEUICWonNetGamesBox > UIListBox<EEUICWNGBLine>; vtable 0x44a154; control 13 of form
// 0x20). Field and member names are provisional. Requires lle/UIListBox.h.
#ifndef EE_EEUICWONNETGAMESBOX_H
#define EE_EEUICWONNETGAMESBOX_H

// one line of the games box (sizeof 0x20: the accessors index mData with << 5)
struct EEUICWNGBLine {
    UWideString mName;                      // +0x00
    long mHumans;                           // +0x14 (EEAWonNet::Virtual4 0x564a: the action's mL84)
    long mPlayers;                          // +0x18 (likewise mL88)
    bool mHasPassword;                      // +0x1c (joining asks for the password first)
    bool mIsPrivate;                        // +0x1d
    bool mIsInProgress;                     // +0x1e (joining shows message 0x8023)
};

class EEUICWonNetGamesBox : public UIListBox<EEUICWNGBLine> {
public:
    // 0x25b2ce (ret 0x20): as EEUICWonNetRoomsBox's ctor, then the three bracketed column headers
    // (resource strings 0x8020..0x8022) and their widths
    EEUICWonNetGamesBox(unsigned long inId, GEFont* inFont, const GE2DPoint& inLocation, unsigned long inWidth,
                        unsigned long inHeight, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness);
    virtual ~EEUICWonNetGamesBox();         // 0x25b63a (??_G 0x25b61e)
    virtual long CopyData();                // +0x14 0x25b68e
    virtual long DrawPolygons();            // +0x18 0x25b6b0 (the base's)
    virtual long DrawTextA();               // +0x1c 0x25b6b5 (six columns; full or empty games greyed)
    // inline; emitted out of line after their first user (EEUIEWNAGHostOrJoinGame::ProcessEvent 0x1ea2fb) in
    // declaration order: 0x1ea583, 0x1ea5e7, 0x1ea618
    UWideString GetGameName(unsigned long inIndex)
    {
        return inIndex < this->mData.size() ? this->mData[inIndex].mName : UWideString("");
    }
    bool GetHasPassword(unsigned long inIndex)
    {
        return inIndex < this->mData.size() ? this->mData[inIndex].mHasPassword : false;
    }
    bool GetIsInProgress(unsigned long inIndex)
    {
        return inIndex < this->mData.size() ? this->mData[inIndex].mIsInProgress : false;
    }

    // members from the ctor 0x25b2ce (ret 0x20: UIListBox's ctor 0x25bb37, then these, then vptr 0x44a154) and the dtor
    // 0x25b63a (??_G 0x25b61e: both texts' ??_G, the three strings in reverse, then ~UIListBox 0x25bdb1). Names
    // (provisional name)
    UWideString mPasswordLabel;             // +0x134
    UWideString mPrivateLabel;              // +0x148
    UWideString mInProgressLabel;           // +0x15c
    float mPasswordWidth;                   // +0x170 (ctor: mU134's width in mCopiedText + 4)
    float mPrivateWidth;                    // +0x174 (likewise mPrivateLabel)
    float mInProgressWidth;                 // +0x178 (likewise mInProgressLabel)
    GEText* mCopiedText;                    // +0x17c (ctor: new GEText(argument 2, the font))
    GEText* mText;                          // +0x180 (likewise)
    long mReserved184[6];   // never accessed (gbscan.py); sizeof 0x19c from new 0x19c in EEUIManager 0x274647
    // (owner-approved reserved tail, docs/LOST_DEBUG.md: neither the ctor, the dtor, this class's members
    // 0x25b68e..0x25bb37 nor any caller touches +0x184..+0x19c)
};

#endif
