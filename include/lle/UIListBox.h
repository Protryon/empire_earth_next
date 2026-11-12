// C:\EEX\Low-Level Engine\User Interface\UIListBox.h (RECONSTRUCTION). The list box control template (control type 5)
// and its insertion predicates. Every member of the exported instantiation UIListBox<UWideString> is emitted by
// UIComboBox.cpp, the first includer in link order: the implicit copy members with the class (0x10060b91..), the rest
// after the unit's own code in declaration order (0x100629a3..0x10063f1e), the InsertLine member template after the
// vector COMDATs (0x1006452c). Vtable 0x1009920c (UIControl's 34 slots, then AppendLine 0x88 .. ProcessUp 0xb8).
// sizeof 0x134 (UILBString's members start at +0x134). Field names are provisional.
// The selection and scrolling are Rust (rust/crates/ee_ui/src/list.rs, docs/RUST_PLAN.md R6): the members below forward
// to it through UIListBoxRust<T>; the data, drawing, key repeat and the sort's algorithm (std::sort, with the order
// from ee_ui/src/text.rs) stay here.
// Requires lle/UIControl.h, lle/UIScrollBar.h, lle/TMPoll.h, lle/GERhombus.h, <vector>, <algorithm>.
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
        return ee_ui_text_less_no_case(inLeft, inRight);   // ee_ui/src/text.rs
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
template <class T> struct UIListBoxRust;
template <class T> class UILISTBOX_API UIListBox : public UIControl {
    friend class UIComboBox;                // OpenListBox reads mLineHeight
    friend struct UIListBoxRust<T>;         // the Rust algorithms' host calls (below)
public:
    UIListBox(unsigned long inId, std::vector<T>& inData, unsigned long inLineHeight, bool inAutoSelect,
              const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
              unsigned long inBorderThickness);
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
    virtual void AppendLine(const T& inLine);
    virtual void AppendStringArray(std::vector<T>& inLines);
    virtual void ChangeSelectionIndex(unsigned long inIndex, bool inFireEvent, bool inScrollTo);
    void ChangeWidth(unsigned long inWidth);
    virtual void ClearSelection() { this->mHasSelection = false; }
    virtual long CopyData();
    virtual void DeleteAllEntries();
    virtual void DeleteLine(unsigned long inLine);
    virtual long DrawPolygons();
    virtual long DrawTextA();
    long GetCurrentSelection(unsigned long& outSelection) const;
    unsigned long GetDisplayLine() const
    {
        if (this->mScrollBar)
            return this->mScrollBar->GetSlider()->GetCurrentRangeValue();
        return 0;
    }
    GERhombus* GetSelectedBackground() const { return this->mSelectedBackground; }
    unsigned long GetLineCount() const { return this->mData.size(); }
    unsigned long GetNumberLinesDisplayed() const { return this->mNumberLinesDisplayed; }
    const UIScrollBar* GetScrollBar() const { return this->mScrollBar; }
    bool GetScrollBarCaptured() const { return this->mScrollBarCaptured; }
    const T& GetData(unsigned long inIndex) const { return this->mData[inIndex]; }
    const std::vector<T>& GetDataArray() const { return this->mData; }
    bool GetHasSelection() const { return this->mHasSelection; }
    bool FindString(const T& inString, unsigned long& outIndex) const;
    void InsertLine(const T& inLine, unsigned long inIndex)
    {
        UILBNoSort<T> theSort;
        this->InsertLine(inLine, inIndex, theSort);
    }
    template <class P> void InsertLine(const T& inLine, unsigned long inIndex, P& inSort)
    {
        std::vector<T>::iterator theWhere = this->mData.begin() + inIndex;
        theWhere = std::lower_bound(theWhere, this->mData.end(), inLine, inSort);
        // Fixed: the index was taken after the insert, from an iterator a reallocation had invalidated
        unsigned long theIndex = theWhere - this->mData.begin();
        if (theWhere != this->mData.end())
            this->mData.insert(theWhere, inLine);
        else
            this->mData.push_back(inLine);
        ee_ui_list_inserted(typename UIListBoxRust<T>::Refs(this), theIndex);
    }
    std::vector<T>::const_iterator Begin() const { return this->mData.begin(); }
    std::vector<T>::iterator Begin() { return this->mData.begin(); }
    std::vector<T>::const_iterator End() const { return this->mData.end(); }
    std::vector<T>::iterator End() { return this->mData.end(); }
    virtual long MouseWheelTurned(long inDelta)
    {
        if (this->mScrollBar) {
            this->mScrollBar->IncrementBy(inDelta);
            return kErrorOK;
        }
        return kErrorFailed;
    }
    void ReplaceLine(const T& inLine, unsigned long inIndex)
    {
        this->mData[inIndex] = inLine;
        this->SetDirty(true);
    }
    bool ScrollBarExists() { return this->mScrollBar ? true : false; }
    void SetAutoSelect(bool inAutoSelect) { this->mAutoSelect = inAutoSelect; }
    void SetFocusedRectColor(const GEColor& inColor) { this->mFocusedRectColor = inColor; }
    virtual void SetLocation(const GE2DPoint& inLocation);
    void SetScrollBarBgOpacity(float inOpacity);
    void SetScrollBarBgColor(const GEColor& inColor);
    void SetScrollBarCurrentRange(unsigned long inValue)
    {
        if (this->mScrollBar)
            this->mScrollBar->SetCurrentRangeValue(inValue);
    }
    void SetStartingDisplayLine(unsigned long inLine)
    {
        if (this->mScrollBar)
            this->mScrollBar->SetCurrentRangeValue(inLine);
    }
    void Sort();
    virtual long UpdateKeyboard();
protected:
    virtual void ButtonDown();              // slot 0xa0
    virtual void ButtonUp() { this->FireTriggerEvent(kControlTriggerButtonUp); }
    long DrawScrollBarText()
    {
        long theResult = kErrorOK;
        if (this->mCopiedScrollBarVisible)
            theResult = this->mScrollBar->DrawTextA();
        return theResult;
    }
    const std::vector<T>& GetCopyLineArray() const { return this->mCopyLineArray; }
    const GERectangle& GetCopyTextRectangle() const { return this->mCopyTextRectangle; }
    unsigned long GetDrawCount() const { return this->mDrawLineCount; }
    virtual unsigned long GetNumLines() const { return this->mData.size(); }  // slot 0xa8
    virtual long MouseLButtonDown();
    virtual long MouseLButtonUp();
    virtual long MouseLButtonDoublePressed();
    virtual long MouseMove(const GE2DPoint& inLocation);
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void ProcessDown();
    virtual void ProcessEnd();
    virtual void ProcessHome();
    virtual void ProcessUp();
    virtual void TabbedFrom();
    virtual void TabbedTo() { this->SetDirty(true); }
    void CheckScrollBar();

    std::vector<T> mData;                   // +0x0a0
    std::vector<T> mCopyLineArray;          // +0x0b0 the visible lines as of the last CopyData
    GERectangle mTextRectangle;             // +0x0c0
    GERectangle mCopyTextRectangle;         // +0x0d0
    GEColor mScrollBarBgColor;              // +0x0e0
    GEColor mFocusedRectColor;              // +0x0ec
    GEColor mCopiedFocusedRectColor;        // +0x0f8
    GERhombus* mSelectedBackground;         // +0x104
    GERhombus* mCopiedSelectedBackground;   // +0x108
    UIScrollBar* mScrollBar;                // +0x10c
    TMPoll* mRepeatPoll;                    // +0x110 key repeat rate (100 ms)
    TMPoll* mRepeatDelayPoll;               // +0x114 key repeat delay (500 ms)
    float mScrollBarBgOpacity;              // +0x118
    unsigned long mNumberLinesDisplayed;    // +0x11c
    unsigned long mCurrentSelection;        // +0x120
    unsigned long mDrawLineCount;           // +0x124 lines copied by the last CopyData
    unsigned long mLineHeight;              // +0x128
    bool mAutoSelect;                       // +0x12c select on mouse over
    bool mButtonDown;                       // +0x12d
    bool mScrollBarCaptured;                // +0x12e
    bool mCopiedHasFocus;                   // +0x12f
    bool mCopiedHasSelection;               // +0x130
    bool mScrollBarRemoved;                 // +0x131 the scroll bar is to be deleted by the next CopyData
    bool mCopiedScrollBarVisible;           // +0x132
    bool mHasSelection;                     // +0x133
};


// The Rust algorithms' view of a UIListBox<T> (rust/crates/ee_ui/src/list.rs): the object, its members from
// mScrollBarBgOpacity on as the pinned record EeUiListFields, and a table of host functions instantiated per T. The
// virtual members stay virtual.
template <class T> struct UIListBoxRust {
    typedef UIListBox<T> Box;
    static Box* B(void* inList) { return static_cast<Box*>(inList); }
    static unsigned long NumLines(void* inList) { return B(inList)->GetNumLines(); }
    static void ChangeSelection(void* inList, unsigned long inIndex, bool inFire, bool inScrollTo)
    {
        B(inList)->ChangeSelectionIndex(inIndex, inFire, inScrollTo);
    }
    static void ClearSelection(void* inList) { B(inList)->ClearSelection(); }
    static bool HasScrollBar(void* inList) { return B(inList)->mScrollBar != NULL; }
    static long ScrollValue(void* inList) { return B(inList)->mScrollBar->GetSlider()->GetCurrentRangeValue(); }
    static long ScrollMax(void* inList) { return B(inList)->mScrollBar->GetSlider()->GetMaxRange(); }
    static void SetScrollValue(void* inList, unsigned long inValue) { B(inList)->mScrollBar->SetCurrentRangeValue(inValue); }
    static void SetScrollRange(void* inList, unsigned long inMin, unsigned long inMax)
    {
        B(inList)->mScrollBar->SetRange(inMin, inMax);
    }
    static void ScrollSetRectangle(void* inList, const EeUiRect* inRectangle)
    {
        B(inList)->mScrollBar->SetRectangle(*reinterpret_cast<const GERectangle*>(inRectangle));
    }
    static void CreateScrollBar(void* inList, float inX, float inY, float inLength, unsigned long inPage,
                                unsigned long inValue)
    {
        Box* theList = B(inList);
        theList->mScrollBar = new UIScrollBar(theList->mId, GE2DPoint(inX, inY), inLength, 1, theList->mForm,
                                              theList->mIndex, theList->mBorderThickness);
        theList->mScrollBar->SetPageSize(inPage);
        theList->mScrollBar->SetCurrentRangeValue(inValue);
        theList->mScrollBar->SetBgColor(theList->mScrollBarBgColor);
        theList->mScrollBar->SetBgOpacity(theList->mScrollBarBgOpacity);
    }
    static void SetDirty(void* inList) { B(inList)->SetDirty(true); }
    static void Fire(void* inList, unsigned long inEvent) { B(inList)->FireTriggerEvent(inEvent); }
    static void SetBgRectangle(void* inList, const EeUiRect* inRectangle)
    {
        B(inList)->SetBgRectangle(*reinterpret_cast<const GERectangle*>(inRectangle));
    }
    static void BaseSetLocation(void* inList, float inX, float inY) { B(inList)->UIControl::SetLocation(GE2DPoint(inX, inY)); }
    static void MouseLocation(void* inList, EeUiPoint* outLocation)
    {
        const GE2DPoint& theMouse = B(inList)->GetMouseLocation();
        outLocation->x = theMouse.GetX();
        outLocation->y = theMouse.GetY();
    }
    static const EeUiListHost* Table()
    {
        static const EeUiListHost sTable = {NumLines, ChangeSelection, ClearSelection, HasScrollBar, ScrollValue,
                                            ScrollMax, SetScrollValue, SetScrollRange, ScrollSetRectangle,
                                            CreateScrollBar, SetDirty, Fire, SetBgRectangle, BaseSetLocation,
                                            MouseLocation};
        return &sTable;
    }
    struct Refs {
        EeUiListRefs mRefs;
        Refs(const Box* inList)
        {
            // the record's offsets from mScrollBarBgOpacity (ee_ui/src/list.rs ListFields)
#define EE_UI_LIST_FIELD(member, field)                                                                               \
    static_assert(offsetof(Box, member) - offsetof(Box, mScrollBarBgOpacity) == offsetof(EeUiListFields, field),      \
                  "ee_ui/src/list.rs")
            EE_UI_LIST_FIELD(mNumberLinesDisplayed, number_lines_displayed);
            EE_UI_LIST_FIELD(mCurrentSelection, current_selection);
            EE_UI_LIST_FIELD(mDrawLineCount, draw_line_count);
            EE_UI_LIST_FIELD(mLineHeight, line_height);
            EE_UI_LIST_FIELD(mAutoSelect, auto_select);
            EE_UI_LIST_FIELD(mButtonDown, button_down);
            EE_UI_LIST_FIELD(mScrollBarCaptured, scroll_bar_captured);
            EE_UI_LIST_FIELD(mCopiedHasFocus, copied_has_focus);
            EE_UI_LIST_FIELD(mCopiedHasSelection, copied_has_selection);
            EE_UI_LIST_FIELD(mScrollBarRemoved, scroll_bar_removed);
            EE_UI_LIST_FIELD(mCopiedScrollBarVisible, copied_scroll_bar_visible);
            EE_UI_LIST_FIELD(mHasSelection, has_selection);
#undef EE_UI_LIST_FIELD
            static_assert(sizeof(Box) - offsetof(Box, mScrollBarBgOpacity) >= sizeof(EeUiListFields), "ee_ui/src/list.rs");
            Box* theList = const_cast<Box*>(inList);
            this->mRefs.list = theList;
            this->mRefs.host = Table();
            this->mRefs.fields = reinterpret_cast<EeUiListFields*>(&theList->mScrollBarBgOpacity);
            this->mRefs.text = reinterpret_cast<EeUiRect*>(&theList->mTextRectangle);
            this->mRefs.rect = reinterpret_cast<const EeUiRect*>(&theList->mRectangle);
            this->mRefs.border = &theList->mBorderThickness;
        }
        operator const EeUiListRefs*() const { return &this->mRefs; }
    };
    static EeUiPoint Point(const GE2DPoint& inPoint) { return EeUiPoint{inPoint.GetX(), inPoint.GetY()}; }
};

template <class T>
UIListBox<T>::UIListBox(unsigned long inId, std::vector<T>& inData, unsigned long inLineHeight, bool inAutoSelect,
                        const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                        unsigned long inBorderThickness)
    : UIControl(kControlTypeListBox, inId, inRectangle, inForm, inIndex, inBorderThickness), mFocusedRectColor(1.0f, 1.0f, 0.0f),
      mCopiedFocusedRectColor(1.0f, 1.0f, 0.0f), mAutoSelect(inAutoSelect)
{
    this->mScrollBar = NULL;
    this->mCurrentSelection = 0;
    this->mDrawLineCount = 0;
    this->mLineHeight = inLineHeight;
    this->mCopiedHasFocus = false;
    this->mCopiedHasSelection = false;
    this->mCopiedScrollBarVisible = false;
    this->mButtonDown = false;
    this->mScrollBarCaptured = false;
    this->mScrollBarRemoved = false;
    this->mHasSelection = true;
    this->mBorderType = 0;
    this->mRepeatPoll = new TMPoll(100, 0, false);
    this->mRepeatDelayPoll = new TMPoll(500, 0, false);
    this->mSelectedBackground = new GERhombus;
    this->mCopiedSelectedBackground = new GERhombus;
    this->mTextRectangle.mLeft = this->mRectangle.GetLeft() + inBorderThickness;
    this->mTextRectangle.mTop = this->mRectangle.GetTop() + inBorderThickness;
    this->mTextRectangle.mBottom = this->mRectangle.GetBottom() - inBorderThickness;
    this->mTextRectangle.mRight = this->mRectangle.GetRight() - inBorderThickness;
    this->mNumberLinesDisplayed = UMFloor(inRectangle.GetHeight() - 2 * inBorderThickness) / this->mLineHeight;
    if (this->mNumberLinesDisplayed > inRectangle.GetHeight())
        this->mNumberLinesDisplayed = 1;
    this->mCopyLineArray.resize(this->mNumberLinesDisplayed);
    this->mData = inData;
    this->mScrollBarBgColor = GEColor(0.0f, 0.0f, 0.0f);
    this->mScrollBarBgOpacity = 0.0f;
    this->CheckScrollBar();
}

template <class T> UIListBox<T>::~UIListBox()
{
    this->DeleteAllEntries();
    delete this->mScrollBar;
    delete this->mRepeatPoll;
    delete this->mRepeatDelayPoll;
    delete this->mSelectedBackground;
    delete this->mCopiedSelectedBackground;
}

template <class T> void UIListBox<T>::AppendLine(const T& inLine)
{
    this->mData.push_back(inLine);
    this->SetDirty(true);
    this->CheckScrollBar();
}

template <class T> void UIListBox<T>::AppendStringArray(std::vector<T>& inLines)
{
    for (unsigned long i = 0; i < inLines.size(); i++)
        this->mData.push_back(inLines[i]);
    this->CheckScrollBar();
    this->SetDirty(true);
}

template <class T> void UIListBox<T>::ChangeSelectionIndex(unsigned long inIndex, bool inFireEvent, bool inScrollTo)
{
    ee_ui_list_change_selection_index(typename UIListBoxRust<T>::Refs(this), inIndex, inFireEvent, inScrollTo);
}

template <class T> void UIListBox<T>::ChangeWidth(unsigned long inWidth)
{
    GERectangle theRectangle;
    theRectangle = this->mRectangle;
    theRectangle.SetRight(theRectangle.GetLeft() + inWidth - 1.0f);
    UIControl::SetRectangle(theRectangle);
}

template <class T> long UIListBox<T>::CopyData()
{
    GERectangle theRectangle;
    unsigned long theIndex = 0;
    long theResult = UIControl::CopyData();
    this->mCopiedHasFocus = false;
    this->mCopiedHasSelection = false;
    this->mCopyTextRectangle = this->mTextRectangle;
    if (this->mScrollBarRemoved) {
        delete this->mScrollBar;
        this->mScrollBar = NULL;
        this->mScrollBarRemoved = false;
        this->mScrollBarCaptured = false;
    }
    // the scroll bar's window of lines, or every line (ee_ui draw.rs)
    bool theScroll = this->mScrollBar != NULL;
    EeUiListRange theRange = ee_ui_list_line_range(theScroll, theScroll ? this->mScrollBar->GetSlider()->GetCurrentRangeValue() : 0,
                                                   this->mNumberLinesDisplayed, this->GetNumLines());
    unsigned long theFirst = theRange.first;
    unsigned long theLast = theRange.end;
    this->mDrawLineCount = theRange.count;
    if (theScroll)
        theResult = this->mScrollBar->CopyData();
    this->mCopiedScrollBarVisible = theScroll;
    if (this->mNumberLinesDisplayed > this->mCopyLineArray.size())
        this->mCopyLineArray.resize(this->mNumberLinesDisplayed);
    for (unsigned long i = theFirst; i < theLast; i++, theIndex++)
        this->mCopyLineArray[theIndex] = this->mData[i];
    // the selected line's rectangle when it is copied (ee_ui draw.rs)
    EeUiRect theText = {this->mCopyTextRectangle.mBottom, this->mCopyTextRectangle.mLeft, this->mCopyTextRectangle.mRight,
                        this->mCopyTextRectangle.mTop};
    EeUiRect theSelection;
    if (ee_ui_list_selection(this->GetNumLines(), this->mHasSelection, this->mCurrentSelection, theFirst, theLast, theText,
                             this->mLineHeight, &theSelection)) {
        this->mCopiedHasSelection = true;
        theRectangle = GERectangle(theSelection.left, theSelection.top, theSelection.right, theSelection.bottom);
        this->mSelectedBackground->SetRectangle(theRectangle);
        *this->mCopiedSelectedBackground = *this->mSelectedBackground;
        if (this->mForm->GetTabbingOn() && this->mForm->GetTabStop() == this->mIndex) {
            this->mCopiedHasFocus = true;
            this->mCopiedFocusedRectColor = this->mFocusedRectColor;
        }
    }
    return theResult;
}

template <class T> void UIListBox<T>::DeleteAllEntries()
{
    this->mData.clear();
    ee_ui_list_cleared(typename UIListBoxRust<T>::Refs(this));
}

template <class T> void UIListBox<T>::DeleteLine(unsigned long inLine)
{
    this->mData.erase(this->mData.begin() + inLine);
    ee_ui_list_deleted_line(typename UIListBoxRust<T>::Refs(this), inLine);
}

template <class T> long UIListBox<T>::DrawPolygons()
{
    GE2DPoint thePoint;
    GE2DPoint theTemp;
    GELine theLine(theTemp, thePoint, this->mCopiedFocusedRectColor, this->mCopiedFocusedRectColor, 1.0f);
    long theResult = UIControl::DrawPolygons();
    if (this->mCopiedScrollBarVisible)
        theResult = this->mScrollBar->DrawPolygons();
    if (this->mCopiedHasSelection) {
        theResult = this->GetRasterizer()->Draw(this->mCopiedSelectedBackground, this->GetScreenIndex());
        if (this->mCopiedHasFocus) {
            // the focus rectangle: top, left, bottom, right (ee_ui draw.rs)
            const GERectangle& theRectangle = this->mCopiedSelectedBackground->GetRectangle();
            EeUiRect theFocus = {theRectangle.mBottom, theRectangle.mLeft, theRectangle.mRight, theRectangle.mTop};
            EeUiPoint theEnds[8];
            ee_ui_list_focus_lines(theFocus, theEnds);
            for (int k = 0; k < 4; k++) {
                theLine.SetStartPoint(GE2DPoint(theEnds[2 * k].x, theEnds[2 * k].y));
                theLine.SetEndPoint(GE2DPoint(theEnds[2 * k + 1].x, theEnds[2 * k + 1].y));
                theResult = this->GetRasterizer()->Draw(&theLine, this->GetScreenIndex());
            }
        }
    }
    return theResult;
}

template <class T> long UIListBox<T>::DrawTextA()
{
    this->DrawScrollBarText();
    this->mDrawCount--;
    return kErrorOK;
}

template <class T> long UIListBox<T>::GetCurrentSelection(unsigned long& outSelection) const
{
    outSelection = this->mCurrentSelection;
    if (this->mData.size() > 0 && this->mHasSelection)
        return kErrorOK;
    return 0xd;
}

template <class T> bool UIListBox<T>::FindString(const T& inString, unsigned long& outIndex) const
{
    bool theFound = false;
    for (unsigned long i = 0; i < this->GetNumLines(); i++)
        if (this->mData[i] == inString) {
            theFound = true;
            outIndex = i;
            break;
        }
    return theFound;
}

template <class T> void UIListBox<T>::SetLocation(const GE2DPoint& inLocation)
{
    ee_ui_list_set_location(typename UIListBoxRust<T>::Refs(this), UIListBoxRust<T>::Point(inLocation));
}

template <class T> void UIListBox<T>::SetScrollBarBgOpacity(float inOpacity)
{
    if (this->mScrollBar)
        this->mScrollBar->SetBgOpacity(inOpacity);
    this->mScrollBarBgOpacity = inOpacity;
}

template <class T> void UIListBox<T>::SetScrollBarBgColor(const GEColor& inColor)
{
    if (this->mScrollBar)
        this->mScrollBar->SetBgColor(inColor);
    this->mScrollBarBgColor = inColor;
}

template <class T> void UIListBox<T>::Sort()
{
    std::sort(this->mData.begin(), this->mData.end(), UWSNoCaseAlphaSort());
}

template <class T> long UIListBox<T>::UpdateKeyboard()
{
    bool theHandled = false;
    bool theRepeat = false;
    if (this->mRepeatDelayPoll->GetIsRunning()) {
        if (this->mRepeatDelayPoll->Poll()) {
            this->mRepeatDelayPoll->Stop();
            theRepeat = true;
            this->mRepeatPoll->Start();
        }
    } else if (this->mRepeatPoll->GetIsRunning()) {
        if (this->mRepeatPoll->Poll())
            theRepeat = true;
    } else {
        this->mRepeatDelayPoll->Start();
        theRepeat = true;
    }
    if (UIControl::DownKeyPressed()) {
        theHandled = true;
        if (theRepeat)
            this->ProcessDown();
    }
    if (UIControl::UpKeyPressed()) {
        theHandled = true;
        if (theRepeat)
            this->ProcessUp();
    }
    if (UIControl::HomeKeyPressed()) {
        theHandled = true;
        this->ProcessHome();
    }
    if (UIControl::EndKeyPressed()) {
        theHandled = true;
        this->ProcessEnd();
    }
    if (UIControl::EnterKeyPressed()) {
        theHandled = true;
        this->FireTriggerEvent(kControlTriggerEnterPressed);
    }
    if (!theHandled) {
        this->mRepeatDelayPoll->Stop();
        this->mRepeatPoll->Stop();
    }
    return kErrorOK;
}

template <class T> void UIListBox<T>::ButtonDown()
{
    ee_ui_list_button_down(typename UIListBoxRust<T>::Refs(this));
}

template <class T> long UIListBox<T>::MouseLButtonDown()
{
    long theResult = kErrorOK;
    if (this->mForm->GetTabStop() != this->mIndex && this->GetTabable())
        this->mForm->SetTabStop(this->mIndex);
    if (this->mScrollBar && this->mScrollBar->GetRectangle().Contains(this->GetMouseLocation())) {
        theResult = this->mScrollBar->MouseLButtonDown();
        this->CaptureMouse();
        this->mScrollBarCaptured = true;
    } else if (this->mTextRectangle.Contains(this->GetMouseLocation())) {
        this->ButtonDown();
        this->CaptureMouse();
    }
    return theResult;
}

template <class T> long UIListBox<T>::MouseLButtonUp()
{
    long theResult = kErrorOK;
    if (this->mScrollBarCaptured)
        theResult = this->mScrollBar->MouseLButtonUp();
    else if (ee_ui_list_up_on_selection(typename UIListBoxRust<T>::Refs(this)))
        this->ButtonUp();
    this->ReleaseCapturedMouse();
    this->mScrollBarCaptured = false;
    this->mButtonDown = false;
    return theResult;
}

template <class T> long UIListBox<T>::MouseLButtonDoublePressed()
{
    long theResult = kErrorOK;
    if (!ee_ui_list_double_pressed(typename UIListBoxRust<T>::Refs(this)))
        theResult = UIControl::MouseLButtonDoublePressed();
    return theResult;
}

template <class T> long UIListBox<T>::MouseMove(const GE2DPoint& inLocation)
{
    long theResult = kErrorOK;
    if (this->mScrollBarCaptured)
        theResult = this->mScrollBar->MouseMove(inLocation);
    else
        ee_ui_list_mouse_move(typename UIListBoxRust<T>::Refs(this), UIListBoxRust<T>::Point(inLocation));
    return theResult;
}

template <class T> void UIListBox<T>::ProcessDown()
{
    ee_ui_list_down(typename UIListBoxRust<T>::Refs(this));
}

template <class T> void UIListBox<T>::ProcessEnd()
{
    ee_ui_list_end(typename UIListBoxRust<T>::Refs(this));
}

template <class T> void UIListBox<T>::ProcessHome()
{
    ee_ui_list_home(typename UIListBoxRust<T>::Refs(this));
}

template <class T> void UIListBox<T>::ProcessUp()
{
    ee_ui_list_up(typename UIListBoxRust<T>::Refs(this));
}

template <class T> void UIListBox<T>::TabbedFrom()
{
    this->ReleaseCapturedMouse();
    this->ReleaseCapturedKeyboard();
    this->mButtonDown = false;
    this->mScrollBarCaptured = false;
    this->SetDirty(true);
}

template <class T> void UIListBox<T>::CheckScrollBar()
{
    ee_ui_list_check_scroll_bar(typename UIListBoxRust<T>::Refs(this));
}

#endif
