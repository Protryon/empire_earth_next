// C:\EEX\Low-Level Engine\User Interface\UILayoutUtility.h (RECONSTRUCTION). Anchoring of a rectangle's four edges
// to its parent (offset + offset type per edge). The ctor and CalculateRectangle (once UILayoutUtility.cpp,
// 0x10077f75..) are inline forwarders to Rust (rust/crates/ee_ui/src/layout.rs; the object is the pinned record
// EeUiLayout, a GERectangle the record EeUiRect; UITrampolines.cpp, their unit, is gone); the inline setters and
// operator= are emitted first by UIBackground.cpp (0x10058d44..). sizeof 0x20 (operator= is `rep movsd` x8).
// Requires lle/LowLevelEngine.h.
#ifndef LLE_UILAYOUTUTILITY_H
#define LLE_UILAYOUTUTILITY_H

#include <stddef.h>

#include "rust/ui/layout_ffi.h"

// GERectangle: the pinned record every UI seam passes
static_assert(sizeof(GERectangle) == sizeof(EeUiRect), "ee_ui/src/rect.rs");
static_assert(offsetof(GERectangle, mBottom) == offsetof(EeUiRect, bottom), "ee_ui/src/rect.rs");
static_assert(offsetof(GERectangle, mLeft) == offsetof(EeUiRect, left), "ee_ui/src/rect.rs");
static_assert(offsetof(GERectangle, mRight) == offsetof(EeUiRect, right), "ee_ui/src/rect.rs");
static_assert(offsetof(GERectangle, mTop) == offsetof(EeUiRect, top), "ee_ui/src/rect.rs");

class LLE_API UILayoutUtility {
public:
    UILayoutUtility() { ee_ui_layout_init(reinterpret_cast<EeUiLayout*>(this)); }
    ~UILayoutUtility() {}                   // exported, ICF-folded `ret` (0x1000c16a)
    void CalculateRectangle(const GERectangle& inParent, GERectangle& outRectangle, bool inExclusive) const
    {
        ee_ui_layout_calculate(reinterpret_cast<const EeUiLayout*>(this), reinterpret_cast<const EeUiRect*>(&inParent),
                               reinterpret_cast<EeUiRect*>(&outRectangle), inExclusive);
    }
    float GetBottomOffset() const { return this->mBottomOffset; }
    unsigned long GetBottomOffsetType() const { return this->mBottomOffsetType; }
    float GetLeftOffset() const { return this->mLeftOffset; }
    unsigned long GetLeftOffsetType() const { return this->mLeftOffsetType; }
    float GetRightOffset() const { return this->mRightOffset; }
    unsigned long GetRightOffsetType() const { return this->mRightOffsetType; }
    float GetTopOffset() const { return this->mTopOffset; }
    unsigned long GetTopOffsetType() const { return this->mTopOffsetType; }
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

    static void CheckLayout()               // the record EeUiLayout (ee_ui/src/layout.rs)
    {
        static_assert(sizeof(UILayoutUtility) == sizeof(EeUiLayout), "ee_ui/src/layout.rs");
        static_assert(offsetof(UILayoutUtility, mLeftOffset) == offsetof(EeUiLayout, left_offset), "layout.rs");
        static_assert(offsetof(UILayoutUtility, mRightOffset) == offsetof(EeUiLayout, right_offset), "layout.rs");
        static_assert(offsetof(UILayoutUtility, mTopOffset) == offsetof(EeUiLayout, top_offset), "layout.rs");
        static_assert(offsetof(UILayoutUtility, mBottomOffset) == offsetof(EeUiLayout, bottom_offset), "layout.rs");
        static_assert(offsetof(UILayoutUtility, mLeftOffsetType) == offsetof(EeUiLayout, left_type), "layout.rs");
        static_assert(offsetof(UILayoutUtility, mRightOffsetType) == offsetof(EeUiLayout, right_type), "layout.rs");
        static_assert(offsetof(UILayoutUtility, mTopOffsetType) == offsetof(EeUiLayout, top_type), "layout.rs");
        static_assert(offsetof(UILayoutUtility, mBottomOffsetType) == offsetof(EeUiLayout, bottom_type), "layout.rs");
    }
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
