// C:\EEX\Low-Level Engine\User Interface\UIListBox.h (RECONSTRUCTION). The list box control template (control type 5)
// and its insertion predicates. Every member of the exported instantiation UIListBox<UWideString> is emitted by
// UIComboBox.cpp, the first includer in link order: the implicit copy members with the class (0x10060b91..), the rest
// after the unit's own code in declaration order (0x100629a3..0x10063f1e), the InsertLine member template after the
// vector COMDATs (0x1006452c). Vtable 0x1009920c (UIControl's 34 slots, then AppendLine 0x88 .. ProcessUp 0xb8).
// The original's sizeof was 0x134 (UILBString's members started at +0x134).
//
// The template is whole in Rust (rust/crates/ee_ui/src/listobj.rs, R6 ui round 15): the text rectangles, colours,
// selection record, scroll bar, key-repeat polls and selection rhombi, every method, and (UI round 15) the lines
// themselves: mData and its drawn copy mCopyLineArray are Rust-owned boxed Ts, made, copied and freed through
// UILineOps<T>; here the members are one call each and mData/mCopyLineArray are views (UILines<T>: size() and [i]).
// UIListBoxRust<T>::Op answers the virtuals (a subclass's override runs); the rest go to ee_ui_list_host.
// Requires lle/UIControl.h, lle/UIScrollBar.h, lle/GERhombus.h, <vector>.
#ifndef LLE_UILISTBOX_H
#define LLE_UILISTBOX_H
#include "platform/PlatString.h"
#include "rust/ui/listobj.h"
#include "rust/ui/lle_hosts.h"
#include "rust/ui/obj.h"
#include <stddef.h>

#include <vector>
#include "lle/UIControl.h"

// The default InsertLine predicate: never "less", so lower_bound keeps the insertion index.
template <class T> class UILBNoSort {
public:
    bool operator()(const T& inLeft, const T& inRight) const { return false; }
};

// (the original's UWSNoCaseAlphaSort, UIFileComboBox's case-insensitive order: ee_ui_text_less_no_case, listobj.rs)

// A line type's members for the Rust list (EeUiLineOps): a UWideString line's text both ways, nothing for another type
inline const char* UILineText(const UWideString* inLine) { return *inLine; }
inline const char* UILineText(const void* inLine) { return NULL; }
inline void* UILineFromText(const UWideString*, const char* inText) { return new UWideString(inText); }
inline void* UILineFromText(const void*, const char* inText) { return NULL; }
template <class T> struct UILineOps {
    static void* Make() { return new T; }
    static void* Clone(const void* inLine) { return new T(*static_cast<const T*>(inLine)); }
    static void Free(void* inLine) { delete static_cast<T*>(inLine); }
    static void Assign(void* inTo, const void* inFrom) { *static_cast<T*>(inTo) = *static_cast<const T*>(inFrom); }
    static const char* Text(const void* inLine) { return UILineText(static_cast<const T*>(inLine)); }
    static void* FromText(const char* inText) { return UILineFromText(static_cast<const T*>(NULL), inText); }
    static bool Equal(const void* a, const void* b) { return *static_cast<const T*>(a) == *static_cast<const T*>(b); }
    template <class P> static bool Less(void* inSort, const void* a, const void* b)
    {
        return (*static_cast<P*>(inSort))(*static_cast<const T*>(a), *static_cast<const T*>(b));
    }
    static constexpr EeUiLineOps kOps = {Make, Clone, Free, Assign, Text, FromText};
};

// mData / mCopyLineArray: a view of the Rust list's lines (inWhich 0, 1)
template <class T> class UILines {
public:
    UILines(EeUiList* inList, uint32_t inWhich) : mList(inList), mWhich(inWhich) {}
    size_t size() const { return ee_ui_list_lines(this->mList, this->mWhich); }
    T& operator[](size_t inIndex) const { return *static_cast<T*>(ee_ui_list_line(this->mList, this->mWhich, inIndex)); }
    EeUiList* mList;
    uint32_t mWhich;
};

#include "lle/LLEForward.h"
template <class T> class UIListBox : public UIControl {
public:
    typedef EeUiListCall UIListBoxCall;
    intptr_t Call(UIListBoxCall inCall, intptr_t a = 0, intptr_t b = 0, float* f = NULL) const { return ee_ui_list_call(this->mRust, inCall, a, b, f); }
    intptr_t CallF(UIListBoxCall inCall, float f0, float f1 = 0.0f, float f2 = 0.0f) const { return ee_ui_list_call(this->mRust, inCall, 0, 0, UIF4(f0, f1, f2)); }
    GEColor Color(UIListBoxCall inCall) const { return UIOut(ee_ui_list_call, this->mRust, inCall, (intptr_t)0, (intptr_t)0).Color(); }
    UIListBox(unsigned long inId, const std::vector<T>& inData, unsigned long inLineHeight, bool inAutoSelect,
              const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness)
        : UIControl(kControlTypeListBox, inId, inRectangle, inForm, inIndex, inBorderThickness),
          mRust(ee_ui_list_new(this, UIListBoxRust<T>::Op, &UILineOps<T>::kOps, inData.data(), inData.size(), sizeof(T),
                               inLineHeight, inAutoSelect, inBorderThickness)),
          mData(mRust, 0), mCopyLineArray(mRust, 1)
    {
        this->CheckScrollBar();
    }
    virtual ~UIListBox() { ee_ui_list_free(this->mRust); }
    virtual void AppendLine(const T& inLine) { ee_ui_list_append(this->mRust, &inLine); }
    virtual void AppendStringArray(std::vector<T>& inLines) { ee_ui_list_append_n(this->mRust, inLines.data(), inLines.size(), sizeof(T)); }
    virtual void ChangeSelectionIndex(unsigned long inIndex, bool inFireEvent, bool inScrollTo)
    {
        this->Call(UIListBoxCall::ChangeSelectionIndex, inIndex, (inFireEvent ? 1 : 0) | (inScrollTo ? 2 : 0));
    }
    virtual void ClearSelection() { this->Call(UIListBoxCall::ClearSelection); }
    virtual long CopyData() { return (long)this->Call(UIListBoxCall::CopyData); }
    virtual void DeleteAllEntries() { ee_ui_list_delete_all(this->mRust); }
    virtual void DeleteLine(unsigned long inLine) { ee_ui_list_delete_line(this->mRust, inLine); }
    virtual long DrawPolygons() { return (long)this->Call(UIListBoxCall::DrawPolygons); }
    virtual long DrawTextA() { return (long)this->Call(UIListBoxCall::DrawText); }
    long GetCurrentSelection(unsigned long& outSelection) const { return ee_ui_list_current_selection(this->mRust, &outSelection); }
    unsigned long GetDisplayLine() const { return (unsigned long)this->Call(UIListBoxCall::DisplayLine); }
    GERhombus* GetSelectedBackground() const { return reinterpret_cast<GERhombus*>(this->Call(UIListBoxCall::SelectedBackground)); }
    unsigned long GetLineCount() const { return this->mData.size(); }
    unsigned long GetNumberLinesDisplayed() const { return (unsigned long)this->Call(UIListBoxCall::NumberLinesDisplayed); }
    const UIScrollBar* GetScrollBar() const { return reinterpret_cast<const UIScrollBar*>(this->Call(UIListBoxCall::ScrollBar)); }
    bool GetScrollBarCaptured() const { return this->Call(UIListBoxCall::ScrollBarCaptured) != 0; }
    const T& GetData(unsigned long inIndex) const { return this->mData[inIndex]; }
    bool GetHasSelection() const { return this->Call(UIListBoxCall::HasSelection) != 0; }
    bool FindString(const T& inString, unsigned long& outIndex) const { return ee_ui_list_find(this->mRust, &inString, UILineOps<T>::Equal, &outIndex); }
    void InsertLine(const T& inLine, unsigned long inIndex) { ee_ui_list_insert(this->mRust, &inLine, inIndex, NULL, NULL); }
    template <class P> void InsertLine(const T& inLine, unsigned long inIndex, const P& inSort)
    {
        ee_ui_list_insert(this->mRust, &inLine, inIndex, const_cast<P*>(&inSort), UILineOps<T>::template Less<P>);
    }
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
    virtual long UpdateKeyboard() { return (long)this->Call(UIListBoxCall::UpdateKeyboard); }
    virtual void ButtonDown() { this->Call(UIListBoxCall::ButtonDown); }   // slot 0xa0
    virtual void ButtonUp() { this->FireTriggerEvent(kControlTriggerButtonUp); }
    long DrawScrollBarText() { return (long)this->Call(UIListBoxCall::DrawScrollBarText); }
    const GERectangle& GetCopyTextRectangle() const { return *reinterpret_cast<const GERectangle*>(this->Call(UIListBoxCall::CopyTextRectangle)); }
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
    GERhombus* GetCopiedSelectedBackground() const { return reinterpret_cast<GERhombus*>(this->Call(UIListBoxCall::CopiedSelectedBackground)); }
    virtual unsigned long GetNumLines() const { return this->mData.size(); }  // slot 0xa8
    virtual long MouseLButtonDown() { return (long)this->Call(UIListBoxCall::MouseLButtonDown); }
    virtual long MouseLButtonUp() { return (long)this->Call(UIListBoxCall::MouseLButtonUp); }
    virtual long MouseLButtonDoublePressed() { return (long)this->Call(UIListBoxCall::MouseDoublePressed); }
    virtual long MouseMove(const GE2DPoint& inLocation) { return (long)this->Call(UIListBoxCall::MouseMove, 0, 0, UIF4(inLocation)); }
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void ProcessDown() { this->Call(UIListBoxCall::ProcessDown); }
    virtual void ProcessUp() { this->Call(UIListBoxCall::ProcessUp); }
    virtual void TabbedFrom() { this->Call(UIListBoxCall::TabbedFrom); }
    virtual void TabbedTo() { this->SetDirty(true); }
    void CheckScrollBar() { this->Call(UIListBoxCall::CheckScrollBar); }

    EeUiList* mRust;                        // the state and the lines (listobj.rs; first: the views are made from it)
    UILines<T> mData;                       // the lines
    UILines<T> mCopyLineArray;              // the visible lines as of the last CopyData
};

// The Rust list box's ops that need the instantiation: the virtuals (a subclass's override runs); the rest go to
// ee_ui_list_host (lle_hosts.rs)
template <class T> struct UIListBoxRust {
    static intptr_t Op(void* inList, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d, float* f)
    {
        UIListBox<T>* l = static_cast<UIListBox<T>*>(inList);
        switch (inOp) {
        case EeUiOp::ListNumLines: return l->GetNumLines();
        case EeUiOp::ListChangeSelection: l->ChangeSelectionIndex((unsigned long)a, b != 0, c != 0); break;
        case EeUiOp::ListClearSelection: l->ClearSelection(); break;
        case EeUiOp::ListProcessDown: l->ProcessDown(); break;
        case EeUiOp::ListProcessUp: l->ProcessUp(); break;
        case EeUiOp::ButtonDown: l->ButtonDown(); break;
        case EeUiOp::ButtonUp: l->ButtonUp(); break;
        case EeUiOp::ListAppendLine: l->AppendLine(*reinterpret_cast<const T*>(a)); break;
        case EeUiOp::ListDeleteAll: l->DeleteAllEntries(); break;
        default: return ee_ui_list_host(l, inOp, a, b, c, d, f);
        }
        return 0;
    }
};


#endif
