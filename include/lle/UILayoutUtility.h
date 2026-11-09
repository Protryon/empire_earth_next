// C:\EEX\Low-Level Engine\User Interface\UILayoutUtility.h (RECONSTRUCTION). Anchoring of a rectangle's four edges
// to its parent (offset + offset type per edge). The ctor and the out-of-line members (once UILayoutUtility.cpp,
// 0x10077f75..) forward to Rust (rust/crates/ee_ui/src/layout.rs, through User Interface/UITrampolines.cpp: the
// object is the pinned record EeUiLayout); the inline setters and operator= are emitted first by UIBackground.cpp
// (0x10058d44..). sizeof 0x20 (operator= is `rep movsd` x8).
#ifndef LLE_UILAYOUTUTILITY_H
#define LLE_UILAYOUTUTILITY_H

class LLE_API UILayoutUtility {
public:
    UILayoutUtility();
    ~UILayoutUtility() {}                   // exported, ICF-folded `ret` (0x1000c16a)
    void CalculateRectangle(const GERectangle& inParent, GERectangle& outRectangle, bool inExclusive) const;
    float GetBottomOffset() const { return this->mBottomOffset; }
    unsigned long GetBottomOffsetType() const { return this->mBottomOffsetType; }
    float GetLeftOffset() const { return this->mLeftOffset; }
    unsigned long GetLeftOffsetType() const { return this->mLeftOffsetType; }
    float GetRightOffset() const { return this->mRightOffset; }
    unsigned long GetRightOffsetType() const { return this->mRightOffsetType; }
    float GetTopOffset() const { return this->mTopOffset; }
    unsigned long GetTopOffsetType() const { return this->mTopOffsetType; }
    void OffsetBottom(long inOffset, unsigned long inType);
    void OffsetLeft(long inOffset, unsigned long inType);
    void OffsetTop(long inOffset, unsigned long inType);
    void OffsetRight(long inOffset, unsigned long inType);
    void SetBottom(unsigned long inType, float inOffset)
    {
        this->mBottomOffset = inOffset;
        this->mBottomOffsetType = inType;
    }
    void SetLeft(unsigned long inType, float inOffset)
    {
        this->mLeftOffset = inOffset;
        this->mLeftOffsetType = inType;
    }
    void SetTop(unsigned long inType, float inOffset)
    {
        this->mTopOffset = inOffset;
        this->mTopOffsetType = inType;
    }
    void SetRight(unsigned long inType, float inOffset)
    {
        this->mRightOffset = inOffset;
        this->mRightOffsetType = inType;
    }
private:
    float mLeftOffset;                      // +0x00
    float mRightOffset;                     // +0x04
    float mTopOffset;                       // +0x08
    float mBottomOffset;                    // +0x0c
    unsigned long mLeftOffsetType;          // +0x10
    unsigned long mRightOffsetType;         // +0x14 (3 at construction)
    unsigned long mTopOffsetType;           // +0x18
    unsigned long mBottomOffsetType;        // +0x1c (3 at construction)
};

// UILayoutUtility edge offset types (GetHorizontalPoint/GetVerticalPoint switch); the database layouts store them
enum UILayoutType {
    kLayoutFromStart = 0,    // UILayoutUtility::GetHorizontalPoint: left/top + offset (provisional name)
    kLayoutAfterCenter = 1,  // UILayoutUtility::GetHorizontalPoint: centre + offset (provisional name)
    kLayoutBeforeCenter = 2,  // UILayoutUtility::GetHorizontalPoint: centre - offset (provisional name)
    kLayoutFromEnd = 3,      // UILayoutUtility::GetHorizontalPoint: right/bottom - offset (provisional name)
    kLayoutProportional = 4,  // UILayoutUtility::GetHorizontalPoint: left/top + size x offset (fraction) (provisional name)
    kLayoutFromStartEdge = 5,  // UILayoutUtility::GetHorizontalPoint: the left/top edge point + offset (a width/height) (provisional name)
};

#endif
