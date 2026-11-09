// C:\EEX\Low-Level Engine\User Interface\UIComboBox.h (RECONSTRUCTION). A text label or edit box with a drop button and
// a UILBString drop-down list (control type 3). UIComboBox.cpp (0x100602e4..0x10064ed6) holds everything: the inline
// blocks of the headers it is the first includer of (UWSNoCaseAlphaSort, UIEditBox, UIListBox<UWideString>'s copy
// members, UILBString, then this class 0x100610e9..), the U2D key chains, the ctor and members, the UIListBox members
// and the vector/algorithm COMDATs. Vtable 0x1009938c (UIControl's 34 slots, then AddString 0x88 .. ProcessUp 0x9c).
// sizeof 0xe0. Field names are provisional.
// Requires lle/UIEditBox.h, lle/UILBString.h, lle/UITextLabel.h, lle/GERhombus.h.
#ifndef LLE_UICOMBOBOX_H
#define LLE_UICOMBOBOX_H

class UIButton;
class UIEditBox;
class UITextLabel;

class LLE_API UIComboBox : public UIControl {
    friend class EESEPlayerRow; // original direct field/method caller17a3f0..17a3f6; access policy only
public:
    struct Rust;                            // the Rust logic's view and host calls (UIComboBox.cpp; a member: the class's friendships)
    UIComboBox(unsigned long inId, unsigned long inTextBoxType, std::vector<UWideString> inData, GEFont* inFont,
               const GERectangle& inRectangle, unsigned long inMaxListBoxHeight, UIForm* inForm, unsigned long inIndex,
               unsigned long inMaxChars, unsigned long inBorderThickness);
    virtual ~UIComboBox();
    void AppendLine(UWideString& inLine);
#ifdef __clang__
    // VC6 bound temporaries to the non-const reference parameters above; clang does not. These const overloads
    // forward to them (clang build only; remove once the parameters themselves are const).
    void AppendLine(const UWideString& inLine) { AppendLine(const_cast<UWideString&>(inLine)); }
#endif
    void AppendStringArray(std::vector<UWideString>& inLines);
    void ChangeWidth(unsigned long inWidth);
    virtual long CopyData();
    void DeleteAllEntries();
    void DeleteLine(unsigned long inLine);
    virtual long DrawPolygons();
    virtual long DrawTextA();
    long GetCurrentSelection(unsigned long& outSelection) const
    {
        outSelection = this->mCurrentSelection;
        return this->mCurrentSelection != -1 ? kErrorOK : kErrorFailed;
    }
    const UIEditBox* GetEditBox() const { return this->mEditBox; }
    GEText* GetGEText() const
    {
        if (this->mTextLabel)
            return this->mTextLabel->GetGEText();
        return this->mEditBox->GetGEText();
    }
    bool GetIsListBoxOpen() const { return this->mIsListBoxOpen; }
    const UILBString* GetListBox() const { return this->mListBox; }
    GERhombus* GetSelectedBackground() const { return this->mListBox->GetSelectedBackground(); }
#ifdef LLE_EXE_HEADERS   // the game sets the label's text (EESEInstructions::Virtual14 0x171ce3 calls
                         // UITextLabel::SetText on +0xa0): non-const like UICheckBox/UIRadioButton (owner decision 2)
    UITextLabel* GetTextLabel() const { return this->mTextLabel; }
#else
    const UITextLabel* GetTextLabel() const { return this->mTextLabel; }
#endif
    unsigned long GetTextBoxType() const { return this->mTextBoxType; }
    void InsertLine(const UWideString& inLine, unsigned long inIndex);
    virtual long MouseWheelTurned(long inDelta);
    virtual void SetBgColor(const GEColor& inColor) {}
    virtual void SetBgOpacity(float inOpacity) {}
    void SetCurrentSelection(unsigned long inIndex);
    void SetListBoxAlwaysDirty(bool inAlwaysDirty) { this->mListBoxAlwaysDirty = inAlwaysDirty; }
    void SetListBoxBgColor(const GEColor& inColor) { this->mListBox->SetBgColor(inColor); }
    void SetListBoxFocusedRectColor(const GEColor& inColor) { this->mListBox->SetFocusedRectColor(inColor); }
    void SetListBoxSelectedBgColor(const GEColor& inColor)
    {
        this->mListBox->GetSelectedBackground()->SetColor(inColor);
        this->mSelectedBackground.SetColor(inColor);
    }
    virtual void SetLocation(const GE2DPoint& inLocation);
    void SetScrollBarBgColor(const GEColor& inColor) { this->mListBox->SetScrollBarBgColor(inColor); }
    void SetTextBoxBgColor(const GEColor& inColor) { this->mTextBox->SetBgColor(inColor); }
    void SetTextBoxBgOpacity(float inOpacity) { this->mTextBox->SetBgOpacity(inOpacity); }
    virtual long UpdateKeyboard();
    virtual long UpdateTranslatedKey(const char* inKey, short inRepeat);
protected:
    virtual long AddString();               // slot 0x88
    virtual void ChangeSelection();
    virtual void CloseListBox();
    virtual void HideControl();
    virtual long MouseLButtonDown();
    virtual long MouseLButtonUp();
    virtual long MouseMove(const GE2DPoint& inLocation);
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void OpenListBox();             // slot 0x94
    virtual void ProcessDown();             // slot 0x98 (ICF-folded with ProcessUp)
    virtual void ProcessUp();
    virtual void TabbedTo();
    virtual void TabbedFrom();

    UITextLabel* mTextLabel;                // +0x0a0 text box type 1
    UIControl* mDropButton;                 // +0x0a4 a UIButton
    UILBString* mListBox;                   // +0x0a8
    UIEditBox* mEditBox;                    // +0x0ac the other text box types
    GERhombus mSelectedBackground;          // +0x0b0 focus highlight of the text box
    UIControl* mTextBox;                    // +0x0c8 mTextLabel or mEditBox
    unsigned long mTextBoxType;             // +0x0cc
    unsigned long mCurrentSelection;        // +0x0d0 -1: none
    unsigned long mMaxListBoxHeight;        // +0x0d4
    bool mIsListBoxOpen;                    // +0x0d8
    bool mReleasedOnce;                     // +0x0d9 the first button release after opening is ignored
    bool mCopiedListBoxOpen;                // +0x0da
    bool mListBoxAlwaysDirty;               // +0x0db
    bool mCopiedHasFocus;                   // +0x0dc
};

#endif
