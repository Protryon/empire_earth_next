// C:\EEX\Low-Level Engine\User Interface\UIComboBox.h (RECONSTRUCTION). A text label or edit box with a drop button and
// a UILBString drop-down list (control type 3). UIComboBox.cpp (0x100602e4..0x10064ed6) holds everything: the inline
// blocks of the headers it is the first includer of (UWSNoCaseAlphaSort, UIEditBox, UIListBox<UWideString>'s copy
// members, UILBString, then this class 0x100610e9..), the U2D key chains, the ctor and members, the UIListBox members
// and the vector/algorithm COMDATs. Vtable 0x1009938c (UIControl's 34 slots, then AddString 0x88 .. ProcessUp 0x9c).
// The original's sizeof was 0xe0.
//
// The class is whole in Rust (rust/crates/ee_ui/src/comboobj.rs, R6 ui round 15): its record, its parts (held by
// address) and the focus highlight are the Rust object mRust; UIComboBox.cpp holds the constructor's part making, the
// forwarders and the host (UIComboBox::Rust::Op). Requires lle/UIEditBox.h, lle/UILBString.h, lle/UITextLabel.h,
// lle/GERhombus.h.
#ifndef LLE_UICOMBOBOX_H
#define LLE_UICOMBOBOX_H
#include "rust/ee_ui.h"

class UIButton;
class UIEditBox;
class UITextLabel;

class LLE_API UIComboBox : public UIControl {
    friend class EESEPlayerRow; // original direct field/method caller17a3f0..17a3f6; access policy only
    intptr_t Call(EeUiComboCall inCall, intptr_t a = 0, intptr_t b = 0, float* f = NULL) const
    {
        return ee_ui_combo_call(this->mRust, inCall, a, b, f);
    }
    UIControl* PartOf(int inPart) const { return reinterpret_cast<UIControl*>(this->Call(EeUiComboCall::Part, inPart)); }
public:
    struct Rust;                            // the Rust object's host (UIComboBox.cpp; a member: the class's friendships)
    UIComboBox(unsigned long inId, unsigned long inTextBoxType, std::vector<UWideString> inData, GEFont* inFont,
               const GERectangle& inRectangle, unsigned long inMaxListBoxHeight, UIForm* inForm, unsigned long inIndex,
               unsigned long inMaxChars, unsigned long inBorderThickness);
    UIComboBox(const UIComboBox&) = delete;
    UIComboBox& operator=(const UIComboBox&) = delete;
    virtual ~UIComboBox();
    void AppendLine(UWideString& inLine);
#ifdef __clang__
    // VC6 bound temporaries to the non-const reference parameters above; clang does not. These const overloads
    // forward to them (clang build only; remove once the parameters themselves are const).
    void AppendLine(const UWideString& inLine) { AppendLine(const_cast<UWideString&>(inLine)); }
#endif
    void AppendStringArray(std::vector<UWideString>& inLines);
    void ChangeWidth(unsigned long inWidth) { this->Call(EeUiComboCall::ChangeWidth, inWidth); }
    virtual long CopyData() { return (long)this->Call(EeUiComboCall::CopyData); }
    void DeleteAllEntries() { this->Call(EeUiComboCall::DeleteAllEntries); }
    void DeleteLine(unsigned long inLine) { this->Call(EeUiComboCall::DeleteLine, inLine); }
    virtual long DrawPolygons() { return (long)this->Call(EeUiComboCall::DrawPolygons); }
    virtual long DrawTextA() { return (long)this->Call(EeUiComboCall::DrawText); }
    long GetCurrentSelection(unsigned long& outSelection) const
    {
        outSelection = this->GetSelectionIndex();
        return outSelection != -1 ? kErrorOK : kErrorFailed;
    }
    GEText* GetGEText() const
    {
        if (this->GetTextLabel())
            return this->GetTextLabel()->GetGEText();
        return reinterpret_cast<UIEditBox*>(this->PartOf(3))->GetGEText();
    }
    bool GetIsListBoxOpen() const { return this->Call(EeUiComboCall::IsListBoxOpen) != 0; }
    const UILBString* GetListBox() const { return this->GetList(); }
    GERhombus* GetSelectedBackground() const { return this->GetList()->GetSelectedBackground(); }
#ifdef LLE_EXE_HEADERS   // the game sets the label's text (EESEInstructions::Virtual14 0x171ce3 calls
                         // UITextLabel::SetText on +0xa0): non-const like UICheckBox/UIRadioButton (owner decision 2)
    UITextLabel* GetTextLabel() const { return reinterpret_cast<UITextLabel*>(this->PartOf(4)); }
#else
    const UITextLabel* GetTextLabel() const { return reinterpret_cast<const UITextLabel*>(this->PartOf(4)); }
#endif
    void InsertLine(const UWideString& inLine, unsigned long inIndex)
    {
        this->Call(EeUiComboCall::InsertLine, (intptr_t)(const char*)inLine, inIndex);
    }
    virtual long MouseWheelTurned(long inDelta) { return (long)this->Call(EeUiComboCall::MouseWheelTurned, inDelta); }
    virtual void SetBgColor(const GEColor& inColor) {}
    virtual void SetBgOpacity(float inOpacity) {}
    void SetCurrentSelection(unsigned long inIndex) { this->Call(EeUiComboCall::SetCurrentSelection, inIndex); }
    void SetListBoxAlwaysDirty(bool inAlwaysDirty) { this->Call(EeUiComboCall::SetListBoxAlwaysDirty, inAlwaysDirty); }
    void SetListBoxFocusedRectColor(const GEColor& inColor) { this->GetList()->SetFocusedRectColor(inColor); }
    void SetListBoxSelectedBgColor(const GEColor& inColor)
    {
        this->GetList()->GetSelectedBackground()->SetColor(inColor);
        float f[4] = {inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue(), 0.0f};
        this->Call(EeUiComboCall::SetSelectedColor, 0, 0, f);
    }
    virtual void SetLocation(const GE2DPoint& inLocation)
    {
        float f[4] = {inLocation.GetX(), inLocation.GetY(), 0.0f, 0.0f};
        this->Call(EeUiComboCall::SetLocation, 0, 0, f);
    }
    void SetScrollBarBgColor(const GEColor& inColor) { this->GetList()->SetScrollBarBgColor(inColor); }
    void SetTextBoxBgColor(const GEColor& inColor) { this->PartOf(0)->SetBgColor(inColor); }
    void SetTextBoxBgOpacity(float inOpacity) { this->PartOf(0)->SetBgOpacity(inOpacity); }
    virtual long UpdateKeyboard() { return (long)this->Call(EeUiComboCall::UpdateKeyboard); }
    virtual long UpdateTranslatedKey(const char* inKey, short inRepeat)
    {
        return (long)this->Call(EeUiComboCall::UpdateTranslatedKey, (intptr_t)inKey, inRepeat);
    }
protected:
    virtual long AddString() { return (long)this->Call(EeUiComboCall::AddString); }   // slot 0x88
    virtual void ChangeSelection() { this->Call(EeUiComboCall::ChangeSelection); }
    virtual void CloseListBox() { this->Call(EeUiComboCall::CloseListBox); }
    virtual void HideControl() { this->Call(EeUiComboCall::HideControl); }
    virtual long MouseLButtonDown() { return (long)this->Call(EeUiComboCall::MouseLButtonDown); }
    virtual long MouseLButtonUp() { return (long)this->Call(EeUiComboCall::MouseLButtonUp); }
    virtual long MouseMove(const GE2DPoint& inLocation)
    {
        float f[4] = {inLocation.GetX(), inLocation.GetY(), 0.0f, 0.0f};
        return (long)this->Call(EeUiComboCall::MouseMove, 0, 0, f);
    }
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void OpenListBox() { this->Call(EeUiComboCall::OpenListBox); }   // slot 0x94
    virtual void ProcessDown() { this->Call(EeUiComboCall::ProcessArrow); }   // slot 0x98 (ICF-folded with ProcessUp)
    virtual void ProcessUp() { this->Call(EeUiComboCall::ProcessArrow); }
    virtual void TabbedTo() { this->Call(EeUiComboCall::Tabbed, 1); }
    virtual void TabbedFrom() { this->Call(EeUiComboCall::Tabbed, 0); }
    // the subclasses' reads of the members (R6 ui round 15's census)
    UILBString* GetList() const { return reinterpret_cast<UILBString*>(this->PartOf(2)); }
    unsigned long GetSelectionIndex() const { return (unsigned long)this->Call(EeUiComboCall::Selection); }
private:
    EeUiCombo* mRust;                       // the state (comboobj.rs)
};

#endif
