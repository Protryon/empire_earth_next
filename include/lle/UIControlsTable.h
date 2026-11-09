// C:\EEX\Low-Level Engine\User Interface\UIControlsTable.h (RECONSTRUCTION). One control record of the UI database,
// loaded by Create into the static sUIControlsArray (0x1012e3c0); UIForm::Open creates the controls from them.
// UIControlsTable.cpp (0x10065f69..0x10069cc7). Vtable 0x10099430 (the dtor only). sizeof 0x158 (??_E stride
// 0x100666cb). Layout proven by the implicit copy ctor 0x10066153 (every member copied on its own: no arrays) and
// the UIDbControl ctor 0x1006796f. The inline members are all exported; the ones emitted at 0x10065f69..0x10066153
// are in declaration order, the others are ICF-folded with earlier getters (their position here is a guess).
// Requires lle/UIForm.h (UIEventData), lle/UILayoutUtility.h, lle/GEColor.h, <vector>.
#ifndef LLE_UICONTROLSTABLE_H
#define LLE_UICONTROLSTABLE_H

#include <vector>

class FSFileSpec;
class GEBitmap;
class GEFont;
class GEText;
class UIControl;
class UIForm;
class UIManager;
struct UIDbControl;

class LLE_API UIControlsTable {
public:
    UIControlsTable();
    UIControlsTable(const UIDbControl& inControl);
    virtual ~UIControlsTable();
    void AddEvent(const UIEventData& inEvent) { this->mEventData.push_back(inEvent); }
    static long Create(FSFileSpec& inFile, UIManager* inManager);
    // port: adds a control the database does not have (after Create), described as a copy of the database control
    // inTemplateID (its type, font, colours, sounds and offset types; not its events) with its own id, form index,
    // group, rectangle (fractions of the window: offset type 4), string range and help string; registered with the
    // template's form as Create registers a record. -> kErrorOK; kErrorNotFound without the template or when the
    // id or the form index is taken
    static long AddControlLike(UIManager* inManager, long inTemplateID, long inID, long inFormIndex,
                               unsigned long inGroup, float inLeft, float inRight, float inTop, float inBottom,
                               long inStringTableBegin, long inStringTableEnd, unsigned long inHelpStringID);
    // port: moves the form inFormID's controls with form index inFirstIndex .. inLastIndex horizontally (after
    // Create, before the form makes its controls): inLeftDelta and inRightDelta are added to their left and right
    // offsets, which must be fractions of the window (offset type 4; others are left alone). -> the number moved
    static long MoveControlsHorizontally(long inFormID, long inFirstIndex, long inLastIndex, float inLeftDelta,
                                         float inRightDelta);
    GEBitmap* CreateBitmap(UIManager* inManager, const GERectangle& inRectangle, const UString& inFileName,
                           bool inHasAlpha, const GERectangle& inTextureRectangle) const;
    UIControl* CreateControl(UIManager* inManager);
    void CreateControlEvent(UIForm* inForm);
    GEBitmap* CreateFocusedBitmap(UIManager* inManager, const GERectangle& inRectangle) const
        { return this->CreateBitmap(inManager, inRectangle, this->mFocusedFileName, this->mFocusedHasAlpha, this->mFocusedTextureRect); }
    GEBitmap* CreateNormalBitmap(UIManager* inManager, const GERectangle& inRectangle) const
        { return this->CreateBitmap(inManager, inRectangle, this->mNormalFileName, this->mNormalHasAlpha, this->mNormalTextureRect); }
    GEBitmap* CreatePressedBitmap(UIManager* inManager, const GERectangle& inRectangle) const
        { return this->CreateBitmap(inManager, inRectangle, this->mPressedFileName, this->mPressedHasAlpha, this->mPressedTextureRect); }
    UIControl* CreateMovie(UIManager* inManager, GERectangle inRectangle) const;
    static void DeleteAll();
    unsigned long GetFontTableIndex() const { return this->mFontTableIndex; }
#ifdef LOWLEVELENGINE_EXPORTS                   // reads the guarded static below (the exe imports neither)
    static UIControlsTable* GetUIControlsTable(unsigned long inIndex) { return UIControlsTable::sUIControlsArray[inIndex]; }
#endif
    GEColor GetBackground() const { return this->mBackground; }
    GEColor GetTextForeground() const { return this->mTextForeground; }
    GEColor GetSeletedListBox() const { return this->mSelectedListBox; }
    GEColor GetFocusedLineColor() const { return this->mFocusedLineColor; }
    UString GetNormalFileName() const { return this->mNormalFileName; }
    UString GetPressedFileName() const { return this->mPressedFileName; }
    UString GetFocusedFileName() const { return this->mFocusedFileName; }
    GEFont* GetFont() const { return this->mFont; }
    GEBitmap* GetNormal() const { return this->mNormal; }
    GEBitmap* GetPressed() const { return this->mPressed; }
    GEBitmap* GetFocused() const { return this->mFocused; }
    float GetBackgroundOpacity() const { return this->mBackgroundOpacity; }
    float GetTextBoxOpacity() const { return this->mTextBoxOpacity; }
    float GetScrollBarBgOpacity() const { return this->mScrollBarBgOpacity; }
    long GetID() const { return this->mID; }
    long GetFormID() const { return this->mFormID; }
    unsigned long GetGroup() const { return this->mGroup; }
    long GetGroupID() const { return this->mGroup; }
    unsigned long GetControlType() const { return this->mControlType; }
    long GetFormIndex() const { return this->mFormIndex; }
    long GetBorderThickness() const { return this->mBorderThickness; }
    unsigned long GetStringTableID() const { return this->mStringTableID; }
    long GetStringTableBegin() const { return this->mStringTableBegin; }
    long GetStringTableEnd() const { return this->mStringTableEnd; }
    long GetTextAlignment() const { return this->mTextAlignment; }
    long GetMaxChars() const { return this->mMaxChars; }
    long GetStartCurrentSelection() const { return this->mStartCurrentSelection; }
    long GetStartRange() const { return this->mStartRange; }
    long GetEndRange() const { return this->mEndRange; }
    long GetCurrentRange() const { return this->mCurrentRange; }
    const UILayoutUtility& GetLayout() const { return this->mLayout; }
    const GERectangle& GetNormalTextureRect() const { return this->mNormalTextureRect; }
    const GERectangle& GetPressedTextureRect() const { return this->mPressedTextureRect; }
    const GERectangle& GetFocusedTextureRect() const { return this->mFocusedTextureRect; }
    bool GetIsVisible() const { return this->mIsVisible; }
    bool GetStartChecked() const { return this->mStartChecked; }
    bool GetListBoxAlwaysDirty() const { return this->mListBoxAlwaysDirty; }
    bool GetSortListBox() const { return this->mSortListBox; }
    bool GetFocusedHasAlpha() const { return this->mFocusedHasAlpha; }
    bool GetPressedHasAlpha() const { return this->mPressedHasAlpha; }
    bool GetNormalHasAlpha() const { return this->mNormalHasAlpha; }
    bool GetAlwaysDirty() const { return this->mAlwaysDirty; }
    bool GetLoopMovie() const { return this->mLoopMovie; }
    void RelayoutControl(UIControl* inControl, UIManager* inManager);
    void SetupFont(UIManager* inManager, const GERectangle& inRectangle, bool inScaleToWindow);
    void SetupGEText(GEText* inText) const;
    void SetupStringArray(UIManager* inManager, std::vector<UWideString>& outStrings) const;
private:
    void CalculateColor(GEColor& outColor, unsigned long inRed, unsigned long inGreen, unsigned long inBlue) const
    {
        outColor.mRed = inRed * (1.0f / 255.0f);
        outColor.mGreen = inGreen * (1.0f / 255.0f);
        outColor.mBlue = inBlue * (1.0f / 255.0f);
    }
    UIControl* CreateBitmap(UIManager* inManager, const GERectangle& inRectangle);
    UIControl* CreateButton(UIManager* inManager, const GERectangle& inRectangle);
    UIControl* CreateCheckBox(UIManager* inManager, const GERectangle& inRectangle);
    UIControl* CreateComboBox(UIManager* inManager, const GERectangle& inRectangle);
    UIControl* CreateEditBox(UIManager* inManager, const GERectangle& inRectangle);
    UIControl* CreateFileComboBox(UIManager* inManager, const GERectangle& inRectangle);
    UIControl* CreateFileListBox(UIManager* inManager, const GERectangle& inRectangle);
    UIControl* CreateGraph(UIManager* inManager, const GERectangle& inRectangle);
    UIControl* CreateListBox(UIManager* inManager, const GERectangle& inRectangle);
    UIControl* CreateRadioButton(UIManager* inManager, const GERectangle& inRectangle);
    UIControl* CreateScrollBar(UIManager* inManager, const GERectangle& inRectangle);
    UIControl* CreateScrollingTextLabel(UIManager* inManager, const GERectangle& inRectangle);
    UIControl* CreateSlider(UIManager* inManager, const GERectangle& inRectangle);
    UIControl* CreateStatusBar(UIManager* inManager, const GERectangle& inRectangle);
    UIControl* CreateTextLabel(UIManager* inManager, const GERectangle& inRectangle);

    std::vector<UIEventData> mEventData;    // +0x004 (AddEvent)
    UILayoutUtility mLayout;                // +0x014
    GERectangle mNormalTextureRect;         // +0x034
    GERectangle mPressedTextureRect;        // +0x044
    GERectangle mFocusedTextureRect;        // +0x054
    GEColor mBackground;                    // +0x064
    GEColor mTextForeground;                // +0x070
    GEColor mSelectedListBox;               // +0x07c
    GEColor mFocusedLineColor;              // +0x088
    UString mNormalFileName;                // +0x094
    UString mPressedFileName;               // +0x0a4
    UString mFocusedFileName;               // +0x0b4
    UString mButtonDownSound;               // +0x0c4
    UString mButtonUpSound;                 // +0x0d4
    GEBitmap* mNormal;                      // +0x0e4
    GEBitmap* mPressed;                     // +0x0e8
    GEBitmap* mFocused;                     // +0x0ec
    GEFont* mFont;                          // +0x0f0 SetupFont; released after each CreateControl/RelayoutControl
    float mBackgroundOpacity;               // +0x0f4
    float mTextBoxOpacity;                  // +0x0f8
    float mScrollBarBgOpacity;              // +0x0fc
    long mUnknown100;                       // +0x100 (provisional name; copied from the record, never read here)
    long mID;                               // +0x104 (Create skips records with a negative id)
    long mFormID;                           // +0x108
    unsigned long mGroup;                   // +0x10c
    unsigned long mControlType;             // +0x110 CreateControl's switch; >= 1000: UIManager::CreateUserDefinedControl
    long mFormIndex;                        // +0x114
    long mBorderThickness;                  // +0x118
    unsigned long mStringTableID;           // +0x11c (negative record values become 0)
    unsigned long mFontTableIndex;          // +0x120 0: no font
    long mBlinkRate;                        // +0x124 provisional name (UIBlinkingButton's last argument)
    long mStringTableBegin;                 // +0x128
    long mStringTableEnd;                   // +0x12c
    long mTextAlignment;                    // +0x130
    long mMaxChars;                         // +0x134
    long mStartCurrentSelection;            // +0x138
    long mStartRange;                       // +0x13c
    long mEndRange;                         // +0x140
    long mCurrentRange;                     // +0x144
    long mPageSize;                         // +0x148 provisional name (UISlider +0xd4, clamped below the range)
    bool mIsVisible;                        // +0x14c
    bool mStartChecked;                     // +0x14d
    bool mListBoxAlwaysDirty;               // +0x14e
    bool mSortListBox;                      // +0x14f
    bool mFocusedHasAlpha;                  // +0x150
    bool mPressedHasAlpha;                  // +0x151
    bool mNormalHasAlpha;                   // +0x152
    bool mAlwaysDirty;                      // +0x153
    bool mFitRectToText;                 // +0x154 provisional name (UITextLabel::SetFitRectToText)
    bool mScaleToControl;                 // +0x155 provisional name (scaled fonts; check boxes/radio buttons)
    bool mLoopMovie;                 // +0x156 (UIMovie: loop)
#ifdef LOWLEVELENGINE_EXPORTS
    // Inferred guard (see LowLevelEngine.h: a static data member costs every includer a $E number)
    static std::vector<UIControlsTable*> sUIControlsArray;   // 0x1012e3c0
#endif
};

// UI database control type (UIControlsTable::CreateControl; >= 1000: EEUIManager::CreateUserDefinedControl)
enum UIDbControlType {
    kDbControlBitmap = 0,                     // CreateBitmap (provisional name)
    kDbControlButton = 1,                     // CreateButton: new UIButton (provisional name)
    kDbControlCheckBox = 2,                   // CreateCheckBox (provisional name)
    kDbControlEditableComboBox = 3,           // CreateComboBox: UIComboBox text box type 0 (an edit box) (provisional name)
    kDbControlComboBox = 4,                   // CreateComboBox: UIComboBox text box type 1 (a text label; db GS Video Card Combo Box) (provisional name)
    kDbControlNumberMultiLineEditBox = 5,     // CreateEditBox: UIEditBox numbers only + multi-line; text set from the number (provisional name)
    kDbControlNumberEditBox = 6,              // CreateEditBox: UIEditBox numbers only, single line (provisional name)
    kDbControlMultiLineEditBox = 7,           // CreateEditBox: UIEditBox multi-line (db CE Campaign Descrip. Edit Box) (provisional name)
    kDbControlEditBox = 8,                    // CreateEditBox: UIEditBox single line (db Chat Text Edit Box) (provisional name)
    kDbControlListBox = 9,                    // CreateListBox: UILBString (provisional name)
    kDbControlAutoSelectListBox = 10,         // CreateListBox: UILBString with inAutoSelect (select on mouse over) (provisional name)
    kDbControlRadioButton = 11,               // CreateRadioButton (provisional name)
    kDbControlRadioTab = 12,                  // CreateRadioButton with inIsPushButton (db: GS Video Tab, MPS Join By IP Tab) (provisional name)
    kDbControlStatusBar = 13,                 // CreateStatusBar (provisional name)
    kDbControlHorizontalScrollBar = 14,       // CreateScrollBar style 0 (provisional name)
    kDbControlTextLabel = 15,                 // CreateTextLabel: label type 0 (single line) (provisional name)
    kDbControlMultiLineTextLabel = 16,        // CreateTextLabel: label type 1 (multi-line with scroll bar) (provisional name)
    kDbControlHorizontalSlider = 0x11,        // CreateSlider style 0 (provisional name)
    kDbControlVerticalSlider = 0x12,          // CreateSlider style 1 (provisional name)
    kDbControlVerticalScrollBar = 0x13,       // CreateScrollBar style 1 (provisional name)
    kDbControlNumberTextLabel = 0x14,         // CreateTextLabel single line, text = the number in the string-table field (db CB Civ Pts Left (#)) (provisional name)
    kDbControlNumberMultiLineTextLabel = 0x15,  // CreateTextLabel multi-line, number text (provisional name)
    kDbControlFileListBox = 0x16,             // CreateFileListBox (provisional name)
    kDbControlHelpTextLabel = 0x18,           // UIForm::Open takes it as mHelpTextLabel; single line (db Main Screen Tool Tips) (provisional name)
    kDbControlMultiLineHelpTextLabel = 0x19,  // UIForm::Open help text label, multi-line (db GS Help Roll Over Text Display) (provisional name)
    kDbControlScrollingTextLabel = 0x1a,      // CreateScrollingTextLabel: UITLScrolling (provisional name)
    kDbControlBlinkingButton = 0x1b,          // CreateButton: new UIBlinkingButton (provisional name)
    kDbControlScalableButton = 0x1c,          // CreateButton: new UIScalableButton (provisional name)
    kDbControlMovie = 0x1d,                   // CreateMovie: UIMovie (provisional name)
    kDbControlLineGraph = 0x1e,               // CreateGraph: new UILineGraph (provisional name)
    kDbControlDualButton = 0x1f,              // CreateButton: new UIDualButton (provisional name)
    kDbControlFileComboBox = 0x20,            // CreateFileComboBox: UIFileComboBox (provisional name)
    kDbControlStatusButton = 0x3e8,           // CreateUserDefinedControl: new EEUIFMGStatusButton (RTTI)
    kDbControlMiniMap = 0x3e9,                // CreateUserDefinedControl: new EEUIMiniMap (RTTI)
    kDbControlErrorLog = 0x3eb,               // CreateUserDefinedControl: new EEUIFMGErrorLog (RTTI)
    kDbControlAttribute = 0x3ed,              // CreateUserDefinedControl: new EEUIFMGAttribute (RTTI)
    kDbControlMainGameStatusBar = 0x3ee,      // CreateUserDefinedControl: new EEUIFMGStatusBar (RTTI)
    kDbControlSelectUnits = 0x3ef,            // CreateUserDefinedControl: new EEUICSelectUnits (RTTI)
    kDbControlCommandUnits = 0x3f0,           // CreateUserDefinedControl: new EEUICCommandUnits (RTTI)
    kDbControlPortrait = 0x3f1,               // CreateUserDefinedControl: new EEUICPortrait (RTTI)
    kDbControlChatLine = 0x3f2,               // CreateUserDefinedControl: new EEUICChatLine (RTTI)
    kDbControlCBListBox = 0x3f3,              // CreateUserDefinedControl: new EECBUIListBox (RTTI)
    kDbControlWonNetStatBox = 0x3f5,          // CreateUserDefinedControl: new EEUICWonNetStatBox (RTTI)
    kDbControlUnitIconBar = 0x3f6,            // CreateUserDefinedControl: new EEUICUnitIconBar (RTTI)
    kDbControlSavedGroup = 0x3f7,             // CreateUserDefinedControl: new EEUIBSavedGroup (RTTI)
    kDbControlPGSLineGraph = 0x3f8,           // CreateUserDefinedControl: new EEUIPGSLineGraph (RTTI)
    kDbControlPing = 0x3f9,                   // CreateUserDefinedControl: new EEUICPing (RTTI)
    kDbControlWonNetRoomsBox = 0x3fa,         // CreateUserDefinedControl: new EEUICWonNetRoomsBox (RTTI)
    kDbControlWonNetGamesBox = 0x3fb,         // CreateUserDefinedControl: new EEUICWonNetGamesBox (RTTI)
};

#endif
