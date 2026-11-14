// C:\EEX\Low-Level Engine\User Interface\UIListBox.h (RECONSTRUCTION). The list box control template (control type 5)
// and its insertion predicates. Every member of the exported instantiation UIListBox<UWideString> is emitted by
// UIComboBox.cpp, the first includer in link order: the implicit copy members with the class (0x10060b91..), the rest
// after the unit's own code in declaration order (0x100629a3..0x10063f1e), the InsertLine member template after the
// vector COMDATs (0x1006452c). Vtable 0x1009920c (UIControl's 34 slots, then AppendLine 0x88 .. ProcessUp 0xb8).
// The original's sizeof was 0x134 (UILBString's members started at +0x134).
//
// The template is whole in Rust but for what depends on T (rust/crates/ee_ui/src/listobj.rs, R6 ui round 15): the
// text rectangles, colours, selection record, scroll bar, key-repeat polls and selection rhombi, and every method
// that does not touch the lines, are the Rust object mRust; the lines (mData), their drawn copy (mCopyLineArray) and
// the data methods stay here. UIListBoxRust<T>::Op answers the ops that need the instantiation (the virtuals, the
// copy of the lines); the rest go to UIListBoxHostOp (UILBString.cpp).
// Requires lle/UIControl.h, lle/UIScrollBar.h, lle/GERhombus.h, <vector>, <algorithm>.
#ifndef LLE_UILISTBOX_H
#define LLE_UILISTBOX_H
#include "platform/PlatString.h"
#include "rust/ee_ui.h"
#include <stddef.h>

#include <vector>
#include <algorithm>
#include "lle/UIControl.h"

// The default InsertLine predicate: never "less", so lower_bound keeps the insertion index.
template <class T> class UILBNoSort {
public:
    bool operator()(const T& inLeft, const T& inRight) const { return false; }
};

// Case-insensitive alphabetical order (UIListBox<UWideString>::Sort, UIFileComboBox's InsertLine).
class LLE_API UWSNoCaseAlphaSort {
public:
    UWSNoCaseAlphaSort() {}                 // user-declared: exported ??0UWSNoCaseAlphaSort@@QAE@XZ (ICF-folded, 0x1093c)
    bool operator()(const UWideString& inLeft, const UWideString& inRight) const
    {
        return ee_ui_text_less_no_case(inLeft, inRight);   // ee_ui/src/listobj.rs
    }
};

#ifdef __clang__
// clang applies an exported/imported class template's attribute to every instantiation (the modern MSVC rule), so
// the exe would import members of UIListBox<EESEUITTData> etc. that the engine never exports. VC6 instantiated those
// where they were used; only UIListBox<UWideString> crosses the DLL boundary (lle/UILBString.h's explicit
// instantiation carries LLE_API).
#define UILISTBOX_API
#else
#define UILISTBOX_API LLE_API
#endif
class GERhombus;
class UIScrollBar;
// The Rust list box's ops that do not depend on T (UILBString.cpp: UIControl's, the scroll bar's, polls', rhombi's,
// the focus line's, the keys')
LLE_API intptr_t UIListBoxHostOp(UIControl* inList, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f);

template <class T> struct UIListBoxRust;
template <class T> class UILISTBOX_API UIListBox : public UIControl {
    friend class UIComboBox;                // OpenListBox reads the line height
    friend struct UIListBoxRust<T>;         // the Rust object's per-T ops (below)
    typedef EeUiListCall UIListBoxCall;
    intptr_t Call(UIListBoxCall inCall, intptr_t a = 0, intptr_t b = 0, float* f = NULL) const
    {
        return ee_ui_list_call(this->mRust, inCall, a, b, f);
    }
    intptr_t CallF(UIListBoxCall inCall, float f0, float f1 = 0.0f, float f2 = 0.0f) const
    {
        float f[4] = {f0, f1, f2, 0.0f};
        return ee_ui_list_call(this->mRust, inCall, 0, 0, f);
    }
    GEColor Color(UIListBoxCall inCall) const
    {
        float f[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        ee_ui_list_call(this->mRust, inCall, 0, 0, f);
        return GEColor(f[0], f[1], f[2]);
    }
public:
    UIListBox(unsigned long inId, std::vector<T>& inData, unsigned long inLineHeight, bool inAutoSelect,
              const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
              unsigned long inBorderThickness);
    UIListBox(const UIListBox&) = delete;
    UIListBox& operator=(const UIListBox&) = delete;
    virtual ~UIListBox();
#ifdef __clang__
    // VC6 bound temporaries to the non-const reference parameters above; clang does not. These const overloads
    // forward to them (clang build only; remove once the parameters themselves are const).
    UIListBox(unsigned long inId, const std::vector<T>& inData, unsigned long inLineHeight, bool inAutoSelect,
              const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness)
        : UIListBox(inId, const_cast<std::vector<T>&>(inData), inLineHeight, inAutoSelect, inRectangle, inForm,
                    inIndex, inBorderThickness) {}
    template <class P> void InsertLine(const T& inLine, unsigned long inIndex, const P& inSort)
        { this->InsertLine(inLine, inIndex, const_cast<P&>(inSort)); }
#endif
    virtual void AppendLine(const T& inLine)
    {
        this->mData.push_back(inLine);
        this->SetDirty(true);
        this->CheckScrollBar();
    }
    virtual void AppendStringArray(std::vector<T>& inLines)
    {
        for (unsigned long i = 0; i < inLines.size(); i++)
            this->mData.push_back(inLines[i]);
        this->CheckScrollBar();
        this->SetDirty(true);
    }
    virtual void ChangeSelectionIndex(unsigned long inIndex, bool inFireEvent, bool inScrollTo)
    {
        this->Call(UIListBoxCall::ChangeSelectionIndex, inIndex, (inFireEvent ? 1 : 0) | (inScrollTo ? 2 : 0));
    }
    void ChangeWidth(unsigned long inWidth) { this->Call(UIListBoxCall::ChangeWidth, inWidth); }
    virtual void ClearSelection() { this->Call(UIListBoxCall::ClearSelection); }
    virtual long CopyData() { return (long)this->Call(UIListBoxCall::CopyData); }
    virtual void DeleteAllEntries()
    {
        this->mData.clear();
        this->Call(UIListBoxCall::Cleared);
    }
    virtual void DeleteLine(unsigned long inLine)
    {
        this->mData.erase(this->mData.begin() + inLine);
        this->Call(UIListBoxCall::DeletedLine, inLine);
    }
    virtual long DrawPolygons() { return (long)this->Call(UIListBoxCall::DrawPolygons); }
    virtual long DrawTextA() { return (long)this->Call(UIListBoxCall::DrawText); }
    long GetCurrentSelection(unsigned long& outSelection) const
    {
        outSelection = this->GetSelectionIndex();
        if (this->mData.size() > 0 && this->GetHasSelection())
            return kErrorOK;
        return 0xd;
    }
    unsigned long GetDisplayLine() const { return (unsigned long)this->Call(UIListBoxCall::DisplayLine); }
    GERhombus* GetSelectedBackground() const { return reinterpret_cast<GERhombus*>(this->Call(UIListBoxCall::SelectedBackground)); }
    unsigned long GetLineCount() const { return this->mData.size(); }
    unsigned long GetNumberLinesDisplayed() const { return (unsigned long)this->Call(UIListBoxCall::NumberLinesDisplayed); }
    const UIScrollBar* GetScrollBar() const { return reinterpret_cast<const UIScrollBar*>(this->Call(UIListBoxCall::ScrollBar)); }
    bool GetScrollBarCaptured() const { return this->Call(UIListBoxCall::ScrollBarCaptured) != 0; }
    const T& GetData(unsigned long inIndex) const { return this->mData[inIndex]; }
    const std::vector<T>& GetDataArray() const { return this->mData; }
    bool GetHasSelection() const { return this->Call(UIListBoxCall::HasSelection) != 0; }
    bool FindString(const T& inString, unsigned long& outIndex) const
    {
        for (unsigned long i = 0; i < this->GetNumLines(); i++)
            if (this->mData[i] == inString) {
                outIndex = i;
                return true;
            }
        return false;
    }
    void InsertLine(const T& inLine, unsigned long inIndex)
    {
        UILBNoSort<T> theSort;
        this->InsertLine(inLine, inIndex, theSort);
    }
    template <class P> void InsertLine(const T& inLine, unsigned long inIndex, P& inSort)
    {
        typename std::vector<T>::iterator theWhere = this->mData.begin() + inIndex;
        theWhere = std::lower_bound(theWhere, this->mData.end(), inLine, inSort);
        // Fixed: the index was taken after the insert, from an iterator a reallocation had invalidated
        unsigned long theIndex = theWhere - this->mData.begin();
        if (theWhere != this->mData.end())
            this->mData.insert(theWhere, inLine);
        else
            this->mData.push_back(inLine);
        this->Call(UIListBoxCall::Inserted, theIndex);
    }
    typename std::vector<T>::const_iterator Begin() const { return this->mData.begin(); }
    typename std::vector<T>::iterator Begin() { return this->mData.begin(); }
    typename std::vector<T>::const_iterator End() const { return this->mData.end(); }
    typename std::vector<T>::iterator End() { return this->mData.end(); }
    virtual long MouseWheelTurned(long inDelta) { return (long)this->Call(UIListBoxCall::MouseWheelTurned, inDelta); }
    bool ScrollBarExists() { return this->Call(UIListBoxCall::ScrollBar) != 0; }
    void SetAutoSelect(bool inAutoSelect) { this->Call(UIListBoxCall::SetAutoSelect, inAutoSelect); }
    void SetFocusedRectColor(const GEColor& inColor)
    {
        this->CallF(UIListBoxCall::SetFocusedRectColor, inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue());
    }
    virtual void SetLocation(const GE2DPoint& inLocation) { this->CallF(UIListBoxCall::SetLocation, inLocation.GetX(), inLocation.GetY()); }
    void SetScrollBarBgOpacity(float inOpacity) { this->CallF(UIListBoxCall::SetScrollBarBgOpacity, inOpacity); }
    void SetScrollBarBgColor(const GEColor& inColor)
    {
        this->CallF(UIListBoxCall::SetScrollBarBgColor, inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue());
    }
    void SetScrollBarCurrentRange(unsigned long inValue) { this->Call(UIListBoxCall::SetScrollValue, inValue); }
    void SetStartingDisplayLine(unsigned long inLine) { this->Call(UIListBoxCall::SetScrollValue, inLine); }
    void Sort() { std::sort(this->mData.begin(), this->mData.end(), UWSNoCaseAlphaSort()); }
    virtual long UpdateKeyboard() { return (long)this->Call(UIListBoxCall::UpdateKeyboard); }
protected:
    virtual void ButtonDown() { this->Call(UIListBoxCall::ButtonDown); }   // slot 0xa0
    virtual void ButtonUp() { this->FireTriggerEvent(kControlTriggerButtonUp); }
    long DrawScrollBarText() { return (long)this->Call(UIListBoxCall::DrawScrollBarText); }
    const GERectangle& GetCopyTextRectangle() const
    {
        return *reinterpret_cast<const GERectangle*>(this->Call(UIListBoxCall::CopyTextRectangle));
    }
    unsigned long GetDrawCount() const { return (unsigned long)this->Call(UIListBoxCall::DrawCount); }
    // the subclasses' reads and writes of the members (R6 ui round 15's census)
    unsigned long GetLineHeight() const { return (unsigned long)this->Call(UIListBoxCall::LineHeight); }
    unsigned long GetSelectionIndex() const { return (unsigned long)this->Call(UIListBoxCall::SelectionIndex); }
    bool GetAutoSelect() const { return this->Call(UIListBoxCall::AutoSelect) != 0; }
    bool GetCopiedHasSelection() const { return this->Call(UIListBoxCall::CopiedHasSelection) != 0; }
    void SetCopiedHasSelection(bool inValue) { this->Call(UIListBoxCall::SetCopiedHasSelection, inValue); }
    void SetCopiedHasFocus(bool inValue) { this->Call(UIListBoxCall::SetCopiedHasFocus, inValue); }
    GEColor GetFocusedRectColor() const { return this->Color(UIListBoxCall::FocusedRectColor); }
    GEColor GetCopiedFocusedRectColor() const { return this->Color(UIListBoxCall::CopiedFocusedRectColor); }
    void SetCopiedFocusedRectColor(const GEColor& inColor)
    {
        this->CallF(UIListBoxCall::SetCopiedFocusedRectColor, inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue());
    }
    GERhombus* GetCopiedSelectedBackground() const
    {
        return reinterpret_cast<GERhombus*>(this->Call(UIListBoxCall::CopiedSelectedBackground));
    }
    virtual unsigned long GetNumLines() const { return this->mData.size(); }  // slot 0xa8
    virtual long MouseLButtonDown() { return (long)this->Call(UIListBoxCall::MouseLButtonDown); }
    virtual long MouseLButtonUp() { return (long)this->Call(UIListBoxCall::MouseLButtonUp); }
    virtual long MouseLButtonDoublePressed() { return (long)this->Call(UIListBoxCall::MouseDoublePressed); }
    virtual long MouseMove(const GE2DPoint& inLocation)
    {
        float f[4] = {inLocation.GetX(), inLocation.GetY(), 0.0f, 0.0f};
        return (long)this->Call(UIListBoxCall::MouseMove, 0, 0, f);
    }
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void ProcessDown() { this->Call(UIListBoxCall::ProcessDown); }
    virtual void ProcessEnd() { this->Call(UIListBoxCall::ProcessEnd); }
    virtual void ProcessHome() { this->Call(UIListBoxCall::ProcessHome); }
    virtual void ProcessUp() { this->Call(UIListBoxCall::ProcessUp); }
    virtual void TabbedFrom() { this->Call(UIListBoxCall::TabbedFrom); }
    virtual void TabbedTo() { this->SetDirty(true); }
    void CheckScrollBar() { this->Call(UIListBoxCall::CheckScrollBar); }

    std::vector<T> mData;                   // the lines
    std::vector<T> mCopyLineArray;          // the visible lines as of the last CopyData
private:
    EeUiList* mRust;                        // the state (listobj.rs)
};

// The Rust list box's ops that need the instantiation: the virtuals (a subclass's override runs) and the copy of the
// visible lines; the rest go to UIListBoxHostOp
template <class T> struct UIListBoxRust {
    static intptr_t Op(void* inList, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f)
    {
        UIListBox<T>* l = static_cast<UIListBox<T>*>(inList);
        switch (inOp) {
        case EeUiOp::ListNumLines: return l->GetNumLines();
        case EeUiOp::ListChangeSelection: l->ChangeSelectionIndex((unsigned long)a, b != 0, c != 0); break;
        case EeUiOp::ListClearSelection: l->ClearSelection(); break;
        case EeUiOp::ListCopyLines:
            if ((unsigned long)a > l->mCopyLineArray.size())
                l->mCopyLineArray.resize((unsigned long)a);
            for (unsigned long i = (unsigned long)b, j = 0; i < (unsigned long)c; i++, j++)
                l->mCopyLineArray[j] = l->mData[i];
            break;
        case EeUiOp::ListProcess:
            if (a == 0)
                l->ProcessDown();
            else if (a == 1)
                l->ProcessUp();
            else if (a == 2)
                l->ProcessHome();
            else
                l->ProcessEnd();
            break;
        case EeUiOp::ButtonDown: l->ButtonDown(); break;
        case EeUiOp::ButtonUp: l->ButtonUp(); break;
        default: return UIListBoxHostOp(l, inOp, a, b, c, d, f);
        }
        return 0;
    }
};

template <class T>
UIListBox<T>::UIListBox(unsigned long inId, std::vector<T>& inData, unsigned long inLineHeight, bool inAutoSelect,
                        const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                        unsigned long inBorderThickness)
    : UIControl(kControlTypeListBox, inId, inRectangle, inForm, inIndex, inBorderThickness)
{
    this->mRust = ee_ui_list_new(static_cast<UIListBox<T>*>(this), UIListBoxRust<T>::Op, inLineHeight, inAutoSelect,
                                 inBorderThickness);
    this->mCopyLineArray.resize(this->GetNumberLinesDisplayed());
    this->mData = inData;
    this->CheckScrollBar();
}

template <class T> UIListBox<T>::~UIListBox()
{
    this->mData.clear();
    ee_ui_list_free(this->mRust);
}

#endif
