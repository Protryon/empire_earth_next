// C:\EEX\Low-Level Engine\User Interface\UIListBox.h (RECONSTRUCTION). The list box control template (control type 5)
// and its insertion predicates. Every member of the exported instantiation UIListBox<UWideString> is emitted by
// UIComboBox.cpp, the first includer in link order: the implicit copy members with the class (0x10060b91..), the rest
// after the unit's own code in declaration order (0x100629a3..0x10063f1e), the InsertLine member template after the
// vector COMDATs (0x1006452c). Vtable 0x1009920c (UIControl's 34 slots, then AppendLine 0x88 .. ProcessUp 0xb8).
// sizeof 0x134 (UILBString's members start at +0x134). Field names are provisional.
// Requires lle/UIControl.h, lle/UIScrollBar.h, lle/TMPoll.h, lle/GERhombus.h, <vector>, <algorithm>.
#ifndef LLE_UILISTBOX_H
#define LLE_UILISTBOX_H

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
        return _mbsicmp(reinterpret_cast<const unsigned char*>((const char*)inLeft), reinterpret_cast<const unsigned char*>((const char*)inRight)) < 0;
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
template <class T> class UILISTBOX_API UIListBox : public UIControl {
    friend class UIComboBox;                // OpenListBox reads mLineHeight
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
        if (theWhere != this->mData.end())
            this->mData.insert(theWhere, inLine);
        else
            this->mData.push_back(inLine);
        unsigned long theIndex = theWhere - this->mData.begin();
        this->SetDirty(true);
        if (this->mCurrentSelection >= theIndex && this->GetNumLines() > 1)
            this->mCurrentSelection++;
        this->CheckScrollBar();
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
    unsigned long theCount = this->GetNumLines();
    if (theCount > 0 && inIndex >= this->GetNumLines())
        inIndex = theCount - 1;
    if (!this->mHasSelection || this->mCurrentSelection != inIndex) {
        this->mCurrentSelection = inIndex;
        this->SetDirty(true);
        if (inFireEvent)
            this->FireTriggerEvent(kControlTriggerSelectionChanged);
        this->mHasSelection = true;
    }
    if (inScrollTo && this->mScrollBar) {
        this->CheckScrollBar();
        if (this->mCurrentSelection < (unsigned long)this->mScrollBar->GetSlider()->GetCurrentRangeValue())
            this->mScrollBar->SetCurrentRangeValue(this->mCurrentSelection);
        else if (this->mCurrentSelection >= this->mScrollBar->GetSlider()->GetCurrentRangeValue() + this->mNumberLinesDisplayed)
            this->mScrollBar->SetCurrentRangeValue(this->mCurrentSelection - this->mNumberLinesDisplayed + 1);
    }
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
    unsigned long theFirst;
    unsigned long theLast;
    if (this->mScrollBar != NULL) {
        theFirst = this->mScrollBar->GetSlider()->GetCurrentRangeValue();
        this->mDrawLineCount = this->mNumberLinesDisplayed;
        theLast = this->mNumberLinesDisplayed + theFirst;
        theResult = this->mScrollBar->CopyData();
        this->mCopiedScrollBarVisible = true;
    } else {
        theFirst = 0;
        theLast = this->GetNumLines();
        this->mDrawLineCount = this->GetNumLines();
        this->mCopiedScrollBarVisible = false;
    }
    if (this->mNumberLinesDisplayed > this->mCopyLineArray.size())
        this->mCopyLineArray.resize(this->mNumberLinesDisplayed);
    for (unsigned long i = theFirst; i < theLast; i++, theIndex++)
        this->mCopyLineArray[theIndex] = this->mData[i];
    if (this->GetNumLines() > 0 && this->mHasSelection && this->mCurrentSelection >= theFirst && this->mCurrentSelection < theLast) {
        this->mCopiedHasSelection = true;
        theRectangle.mLeft = this->mCopyTextRectangle.mLeft;
        theRectangle.mRight = this->mCopyTextRectangle.mRight;
        theRectangle.mTop = (float)((this->mCurrentSelection - theFirst) * this->mLineHeight) + this->mCopyTextRectangle.mTop;
        theRectangle.mBottom = theRectangle.mTop + this->mLineHeight - 1.0f;
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
    this->mCurrentSelection = 0;
    this->ClearSelection();
    this->SetDirty(true);
    this->CheckScrollBar();
}

template <class T> void UIListBox<T>::DeleteLine(unsigned long inLine)
{
    this->mData.erase(this->mData.begin() + inLine);
    this->SetDirty(true);
    if (this->mHasSelection && (this->mCurrentSelection > inLine || this->GetNumLines() > 0 && this->mCurrentSelection == this->GetNumLines()))
        this->mCurrentSelection--;
    this->CheckScrollBar();
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
            GERectangle theRectangle = this->mCopiedSelectedBackground->GetRectangle();
            thePoint = GE2DPoint(theRectangle.GetLeft(), theRectangle.GetTop());
            theLine.SetStartPoint(thePoint);
            thePoint = GE2DPoint(theRectangle.GetRight(), theRectangle.GetTop());
            theLine.SetEndPoint(thePoint);
            theResult = this->GetRasterizer()->Draw(&theLine, this->GetScreenIndex());
            thePoint = GE2DPoint(theRectangle.GetLeft(), theRectangle.GetBottom());
            theLine.SetEndPoint(thePoint);
            theResult = this->GetRasterizer()->Draw(&theLine, this->GetScreenIndex());
            thePoint = GE2DPoint(theRectangle.GetRight(), theRectangle.GetBottom());
            theLine.SetStartPoint(thePoint);
            theResult = this->GetRasterizer()->Draw(&theLine, this->GetScreenIndex());
            thePoint = GE2DPoint(theRectangle.GetRight(), theRectangle.GetTop());
            theLine.SetEndPoint(thePoint);
            theResult = this->GetRasterizer()->Draw(&theLine, this->GetScreenIndex());
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
    UIControl::SetLocation(inLocation);
    this->mTextRectangle.SetLeft(this->GetRectangle().GetLeft() + this->GetBorderThickness());
    this->mTextRectangle.SetTop(this->GetRectangle().GetTop() + this->GetBorderThickness());
    this->mTextRectangle.SetBottom(this->GetRectangle().GetBottom() - this->mBorderThickness);
    this->mTextRectangle.SetRight(this->GetRectangle().GetRight() - this->mBorderThickness);
    if (this->mScrollBar != NULL && !this->mScrollBarRemoved) {
        this->mTextRectangle.SetRight(this->GetRectangle().GetRight() - kScrollBarWidth);
        GE2DPoint theLocation;
        theLocation = this->mTextRectangle.GetTopRight();
        theLocation.SetX(theLocation.GetX() + 1.0f);
        this->mScrollBar->SetRectangle(GERectangle(theLocation, GE2DPoint(this->mRectangle.GetRight(), this->mTextRectangle.mBottom)));
    }
    this->mNumberLinesDisplayed = UMFloor(this->mRectangle.mBottom - this->mRectangle.mTop - 2 * this->mBorderThickness) /
                            this->mLineHeight;
    this->SetBgRectangle(this->mTextRectangle);
    this->CheckScrollBar();
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
    if (!this->mButtonDown && !this->mAutoSelect) {
        long theFirst;
        if (this->mScrollBar)
            theFirst = this->mScrollBar->GetSlider()->GetCurrentRangeValue();
        else
            theFirst = 0;
        unsigned long theLine = UMFloor((this->GetMouseLocation().GetY() - this->mTextRectangle.GetTop()) / this->mLineHeight + theFirst);
        if (theLine < this->GetNumLines())
            this->ChangeSelectionIndex(theLine, true, false);
        this->FireTriggerEvent(kControlTriggerButtonDown);
        this->mButtonDown = true;
    }
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
    GERectangle theRectangle;
    long theResult = kErrorOK;
    if (this->mScrollBarCaptured)
        theResult = this->mScrollBar->MouseLButtonUp();
    else if (this->mTextRectangle.Contains(this->GetMouseLocation())) {
        theRectangle = this->mTextRectangle;
        theRectangle.SetTop((float)(this->mCurrentSelection - this->GetDisplayLine()) * (float)this->mLineHeight + theRectangle.GetTop());
        theRectangle.SetBottom(theRectangle.GetTop() + this->mLineHeight);
        if (theRectangle.Contains(this->GetMouseLocation()))
            this->ButtonUp();
    }
    this->ReleaseCapturedMouse();
    this->mScrollBarCaptured = false;
    this->mButtonDown = false;
    return theResult;
}

template <class T> long UIListBox<T>::MouseLButtonDoublePressed()
{
    long theResult = kErrorOK;
    if (this->mTextRectangle.Contains(this->GetMouseLocation())) {
        long theFirst;
        if (this->mScrollBar)
            theFirst = this->mScrollBar->GetSlider()->GetCurrentRangeValue();
        else
            theFirst = 0;
        unsigned long theLine = UMFloor((this->GetMouseLocation().GetY() - this->mTextRectangle.GetTop()) / this->mLineHeight + theFirst);
        if (theLine < this->GetNumLines())
            this->ChangeSelectionIndex(theLine, true, false);
        this->FireTriggerEvent(kControlTriggerDoubleClicked);
    } else
        theResult = UIControl::MouseLButtonDoublePressed();
    return theResult;
}

template <class T> long UIListBox<T>::MouseMove(const GE2DPoint& inLocation)
{
    long theResult = kErrorOK;
    if (this->mScrollBarCaptured)
        theResult = this->mScrollBar->MouseMove(inLocation);
    else if (this->mAutoSelect && this->mTextRectangle.Contains(inLocation)) {
        unsigned long theFirst = 0;
        if (this->mScrollBar)
            theFirst = this->mScrollBar->GetSlider()->GetCurrentRangeValue();
        long theLine = (unsigned long)UMFloor(this->GetMouseLocation().GetY() - this->mTextRectangle.GetTop()) / this->mLineHeight + theFirst;
        if (theLine >= (long)(this->mNumberLinesDisplayed + theFirst))
            theLine = this->mNumberLinesDisplayed + theFirst - 1;
        if (theLine < (long)this->GetNumLines())
            this->ChangeSelectionIndex(theLine, true, false);
    }
    return theResult;
}

template <class T> void UIListBox<T>::ProcessDown()
{
    if (this->mCurrentSelection + 1 < this->GetNumLines()) {
        this->ChangeSelectionIndex(this->mCurrentSelection + 1, true, false);
        if (this->mScrollBar) {
            if ((long)this->mCurrentSelection < this->mScrollBar->GetSlider()->GetCurrentRangeValue())
                this->mScrollBar->SetCurrentRangeValue(this->mCurrentSelection);
            else if (this->mCurrentSelection >= this->mScrollBar->GetSlider()->GetCurrentRangeValue() + this->mNumberLinesDisplayed)
                this->mScrollBar->SetCurrentRangeValue(this->mCurrentSelection - this->mNumberLinesDisplayed + 1);
        }
        this->SetDirty(true);
    }
}

template <class T> void UIListBox<T>::ProcessEnd()
{
    if (this->mCurrentSelection < this->GetNumLines()) {
        if (this->mScrollBar)
            this->mScrollBar->SetCurrentRangeValue(this->mScrollBar->GetSlider()->GetMaxRange());
        this->ChangeSelectionIndex(this->GetNumLines() - 1, true, false);
        this->SetDirty(true);
    }
}

template <class T> void UIListBox<T>::ProcessHome()
{
    if (this->mCurrentSelection > 0) {
        if (this->mScrollBar)
            this->mScrollBar->SetCurrentRangeValue(0);
        this->ChangeSelectionIndex(0, true, false);
        this->SetDirty(true);
    }
}

template <class T> void UIListBox<T>::ProcessUp()
{
    if (this->mCurrentSelection != 0) {
        this->ChangeSelectionIndex(this->mCurrentSelection - 1, true, false);
        if (this->mScrollBar) {
            if (this->mCurrentSelection < (unsigned long)this->mScrollBar->GetSlider()->GetCurrentRangeValue())
                this->mScrollBar->SetCurrentRangeValue(this->mCurrentSelection);
            else if (this->mCurrentSelection >= this->mScrollBar->GetSlider()->GetCurrentRangeValue() + this->mNumberLinesDisplayed)
                this->mScrollBar->SetCurrentRangeValue(this->mCurrentSelection - this->mNumberLinesDisplayed + 1);
        }
        this->SetDirty(true);
    }
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
    if (this->mScrollBar != NULL && !this->mScrollBarRemoved) {
        if (this->GetNumLines() <= this->mNumberLinesDisplayed) {
            this->mScrollBarRemoved = true;
            this->SetDirty(true);
            this->mTextRectangle.SetRight(this->GetRectangle().GetRight() - this->GetBorderThickness());
            this->SetBgRectangle(this->mTextRectangle);
        } else
            this->mScrollBar->SetRange(0, this->GetNumLines() - this->mNumberLinesDisplayed);
    } else if (this->GetNumLines() > this->mNumberLinesDisplayed) {
        if (this->mScrollBar == NULL) {
            this->mScrollBar = new UIScrollBar(this->mId, GE2DPoint(this->mRectangle.GetRight() - kScrollBarWidth, this->mTextRectangle.mTop),
                                         this->mTextRectangle.mBottom - this->mTextRectangle.mTop, 1, this->mForm, this->mIndex,
                                         this->mBorderThickness);
            this->mScrollBar->SetPageSize(this->mNumberLinesDisplayed > 1 ? this->mNumberLinesDisplayed - 1 : this->mNumberLinesDisplayed);
            this->mScrollBar->SetCurrentRangeValue(this->mCurrentSelection);
            this->mScrollBar->SetBgColor(this->mScrollBarBgColor);
            this->mScrollBar->SetBgOpacity(this->mScrollBarBgOpacity);
        }
        this->mTextRectangle.mRight = this->mRectangle.GetRight() - kScrollBarWidth;
        this->mScrollBar->SetRange(0, this->GetNumLines() - this->mNumberLinesDisplayed);
        this->SetBgRectangle(this->mTextRectangle);
        this->mScrollBarRemoved = false;
    }
}

#endif
