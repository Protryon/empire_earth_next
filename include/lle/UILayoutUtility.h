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
#include "rust/ui/ids.h"

// GERectangle: the pinned record every UI seam passes
static_assert(sizeof(GERectangle) == sizeof(EeUiRect), "ee_ui/src/rect.rs");
static_assert(offsetof(GERectangle, mBottom) == offsetof(EeUiRect, bottom), "ee_ui/src/rect.rs");
static_assert(offsetof(GERectangle, mLeft) == offsetof(EeUiRect, left), "ee_ui/src/rect.rs");
static_assert(offsetof(GERectangle, mRight) == offsetof(EeUiRect, right), "ee_ui/src/rect.rs");
static_assert(offsetof(GERectangle, mTop) == offsetof(EeUiRect, top), "ee_ui/src/rect.rs");

class LLE_API UILayoutUtility : public EeUiLayout {
public:   // the record is Rust's (layout.rs; UI round 13)
    UILayoutUtility() { ee_ui_layout_init(this); }
    ~UILayoutUtility() {}                   // exported, ICF-folded `ret` (0x1000c16a)
    void CalculateRectangle(const GERectangle& inParent, GERectangle& outRectangle, bool inExclusive) const
    {
        ee_ui_layout_calculate(this, reinterpret_cast<const EeUiRect*>(&inParent), reinterpret_cast<EeUiRect*>(&outRectangle),
                               inExclusive);
    }
    float GetBottomOffset() const { return this->bottom_offset; }
    unsigned long GetBottomOffsetType() const { return this->bottom_type; }
    float GetLeftOffset() const { return this->left_offset; }
    unsigned long GetLeftOffsetType() const { return this->left_type; }
    float GetRightOffset() const { return this->right_offset; }
    unsigned long GetRightOffsetType() const { return this->right_type; }
    float GetTopOffset() const { return this->top_offset; }
    unsigned long GetTopOffsetType() const { return this->top_type; }
    void SetBottom(unsigned long inType, float inOffset) { ee_ui_layout_set(this, EeUiEdge::Bottom, inType, inOffset); }
    void SetLeft(unsigned long inType, float inOffset) { ee_ui_layout_set(this, EeUiEdge::Left, inType, inOffset); }
    void SetTop(unsigned long inType, float inOffset) { ee_ui_layout_set(this, EeUiEdge::Top, inType, inOffset); }
    void SetRight(unsigned long inType, float inOffset) { ee_ui_layout_set(this, EeUiEdge::Right, inType, inOffset); }
};

// the edge offset types (UILayoutType): rust/ui/ids.h (ee_ui ids.rs)
#endif
