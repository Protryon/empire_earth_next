// won_wonnet_test's screens' classes (R6 WON round 32): the WON front end's events, its three list boxes and the
// game room form's manager as their headers declare them (ee/EEUIEWonNet*Events.h, ee/EEUICWonNet*Box.h,
// ee/EEWonNetAvailableGameFormManager.h), over won_wonnet_world.h's FE stand-ins. No include guard: included inside
// namespace FE::Ori (the deleted C++ defines the members) and FE::Rus (the forwards to the Rust define them).

#define FE_EVENT(C) \
    class C : public UIEvent { \
    public: \
        C() : UIEvent(#C) {} \
        virtual long ProcessEvent(UIForm* inForm, UIEntity* inEntity); \
    };
FE_EVENT(EEUIEWNAGActivateForm)
FE_EVENT(EEUIEWNAGBack)
FE_EVENT(EEUIEWNAGCheckPassword)
FE_EVENT(EEUIEWNAGClickGameFilter)
FE_EVENT(EEUIEWNAGCloseForm)
FE_EVENT(EEUIEWNAGDeactivateForm)
FE_EVENT(EEUIEWNAGDoubleclickUser)
FE_EVENT(EEUIEWNAGHostOrJoinGame)
FE_EVENT(EEUIEWNAGOpenScreen)
FE_EVENT(EEUIEWNAGSendChat)
FE_EVENT(EEUIEWNAGToggleHostScreen)
FE_EVENT(EEUIEWNAGToggleJoinScreen)
FE_EVENT(EEUIEWNAGUpdateForm)
FE_EVENT(EEUIEWNARActivateForm)
FE_EVENT(EEUIEWNARBack)
FE_EVENT(EEUIEWNARClickUpdateAccount)
FE_EVENT(EEUIEWNARDeactivateForm)
FE_EVENT(EEUIEWNARDoubleClickRoom)
FE_EVENT(EEUIEWNARJoinRoom)
FE_EVENT(EEUIEWNAROpenScreen)
FE_EVENT(EEUIEWNARUpdateForm)
FE_EVENT(EEUIEWNMUACancel)
FE_EVENT(EEUIEWNMUAOpenScreen)
FE_EVENT(EEUIEWNMUASubmit)
FE_EVENT(EEUIEWNSSCancel)
FE_EVENT(EEUIEWNSSOk)
FE_EVENT(EEUIEWNSSOpenScreen)
FE_EVENT(EEUIEWNSCancel)
FE_EVENT(EEUIEWNSOpen)
#undef FE_EVENT

class EEUICWonNetGamesBox : public UIListBox<EEUICWNGBLine> {
public:
    EEUICWonNetGamesBox(unsigned long inId, GEFont* inFont, const GE2DPoint& inLocation, unsigned long inWidth,
                        unsigned long inHeight, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness);
    virtual ~EEUICWonNetGamesBox();
    long CopyData();
    long DrawPolygons();
    virtual long DrawTextA();
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
    // the screens' host's reads of the protected base (R6 WON round 32: ee/EEWonFrontHost.inl)
    long FrontScrollBarText() { return this->DrawScrollBarText(); }
    const GERectangle& FrontCopyRect() const { return this->GetCopyTextRectangle(); }
    unsigned long FrontDrawCount() const { return this->GetDrawCount(); }
    unsigned long FrontLineHeight() const { return this->GetLineHeight(); }
    unsigned long FrontCopyCount() const { return (unsigned long)this->mCopyLineArray.size(); }
    const EEUICWNGBLine& FrontCopyLine(unsigned long inIndex) const { return this->mCopyLineArray[inIndex]; }
    void FrontClearCopiedSelection() { this->SetCopiedHasSelection(false); }
    UWideString mPasswordLabel;
    UWideString mPrivateLabel;
    UWideString mInProgressLabel;
    float mPasswordWidth;
    float mPrivateWidth;
    float mInProgressWidth;
    GEText* mCopiedText;
    GEText* mText;
};

class EEUICWonNetRoomsBox : public UIListBox<EEUICWNRBLine> {
public:
    EEUICWonNetRoomsBox(unsigned long inId, GEFont* inFont, const GE2DPoint& inLocation, unsigned long inWidth,
                        unsigned long inHeight, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness);
    virtual ~EEUICWonNetRoomsBox();
    long CopyData();
    virtual long DrawTextA();
    // the screens' host's reads of the protected base (R6 WON round 32: ee/EEWonFrontHost.inl)
    long FrontScrollBarText() { return this->DrawScrollBarText(); }
    const GERectangle& FrontCopyRect() const { return this->GetCopyTextRectangle(); }
    unsigned long FrontDrawCount() const { return this->GetDrawCount(); }
    unsigned long FrontLineHeight() const { return this->GetLineHeight(); }
    unsigned long FrontCopyCount() const { return (unsigned long)this->mCopyLineArray.size(); }
    const EEUICWNRBLine& FrontCopyLine(unsigned long inIndex) const { return this->mCopyLineArray[inIndex]; }
    void FrontClearCopiedSelection() { this->SetCopiedHasSelection(false); }
    GEText* mCopiedText;
    GEText* mText;
};

class EEUICWonNetStatBox : public UIListBox<EEUICWNSBStat> {
public:
    EEUICWonNetStatBox(unsigned long inID, GEFont* inFont, const GE2DPoint& inLocation, unsigned long inWidth,
                       unsigned long inHeight, UIForm* inForm, unsigned long inIndex, unsigned long inBorder);
    virtual ~EEUICWonNetStatBox();
    long CopyData();
    virtual long DrawTextA();
    virtual void Refresh();
    // the screens' host's reads of the protected base (R6 WON round 32: ee/EEWonFrontHost.inl)
    long FrontScrollBarText() { return this->DrawScrollBarText(); }
    const GERectangle& FrontCopyRect() const { return this->GetCopyTextRectangle(); }
    unsigned long FrontDrawCount() const { return this->GetDrawCount(); }
    unsigned long FrontLineHeight() const { return this->GetLineHeight(); }
    unsigned long FrontCopyCount() const { return (unsigned long)this->mCopyLineArray.size(); }
    const EEUICWNSBStat& FrontCopyLine(unsigned long inIndex) const { return this->mCopyLineArray[inIndex]; }
    void FrontClearCopiedSelection() { this->SetCopiedHasSelection(false); }
    GEText* mText;
    std::vector<EEUICWNSBStat> mStats;
    unsigned long mRowHeight;
};

class EEWonNetAvailableGameFormManager : public UIFormManager {
public:
    EEWonNetAvailableGameFormManager();
    virtual ~EEWonNetAvailableGameFormManager();
    static EEWonNetAvailableGameFormManager* Get();
    TMPoll* GetPoll() const { return this->mPoll; }
    TMPoll* mPoll;
};

// the test's events by number (kFrontEvents in the driver names them) and the form each runs on
static UIEvent* FrontMakeEvent(int inKind)
{
    switch (inKind) {
    case 0: return new EEUIEWNAGActivateForm;
    case 1: return new EEUIEWNAGBack;
    case 2: return new EEUIEWNAGCheckPassword;
    case 3: return new EEUIEWNAGClickGameFilter;
    case 4: return new EEUIEWNAGCloseForm;
    case 5: return new EEUIEWNAGDeactivateForm;
    case 6: return new EEUIEWNAGDoubleclickUser;
    case 7: return new EEUIEWNAGHostOrJoinGame;
    case 8: return new EEUIEWNAGOpenScreen;
    case 9: return new EEUIEWNAGSendChat;
    case 10: return new EEUIEWNAGToggleHostScreen;
    case 11: return new EEUIEWNAGToggleJoinScreen;
    case 12: return new EEUIEWNAGUpdateForm;
    case 13: return new EEUIEWNARActivateForm;
    case 14: return new EEUIEWNARBack;
    case 15: return new EEUIEWNARClickUpdateAccount;
    case 16: return new EEUIEWNARDeactivateForm;
    case 17: return new EEUIEWNARDoubleClickRoom;
    case 18: return new EEUIEWNARJoinRoom;
    case 19: return new EEUIEWNAROpenScreen;
    case 20: return new EEUIEWNARUpdateForm;
    case 21: return new EEUIEWNMUACancel;
    case 22: return new EEUIEWNMUAOpenScreen;
    case 23: return new EEUIEWNMUASubmit;
    case 24: return new EEUIEWNSSCancel;
    case 25: return new EEUIEWNSSOk;
    case 26: return new EEUIEWNSSOpenScreen;
    case 27: return new EEUIEWNSCancel;
    default: return new EEUIEWNSOpen;
    }
}
