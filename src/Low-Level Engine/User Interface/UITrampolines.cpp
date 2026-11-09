// The UI toolkit's trampolines into Rust (rust/crates/ee_ui, docs/RUST_PLAN.md R6): the members of the classes whose
// whole unit moved forward to `extern "C"` functions with the C++ object as a pinned record. New unit, no original
// address (tools/units.py UI_TRAMPOLINES).
#include <windows.h>
#include <stddef.h>

#include "rust/ee_rust.h"
#include "lle/LowLevelEngine.h"
#include "lle/UILayoutUtility.h"

// --- GERectangle: the pinned record every UI seam passes ------------------------------------------------------------

static_assert(sizeof(GERectangle) == sizeof(EeUiRect), "ee_ui/src/rect.rs");
static_assert(offsetof(GERectangle, mBottom) == offsetof(EeUiRect, bottom), "ee_ui/src/rect.rs");
static_assert(offsetof(GERectangle, mLeft) == offsetof(EeUiRect, left), "ee_ui/src/rect.rs");
static_assert(offsetof(GERectangle, mRight) == offsetof(EeUiRect, right), "ee_ui/src/rect.rs");
static_assert(offsetof(GERectangle, mTop) == offsetof(EeUiRect, top), "ee_ui/src/rect.rs");

static const EeUiRect* UIRect(const GERectangle& inRectangle) { return reinterpret_cast<const EeUiRect*>(&inRectangle); }
static EeUiRect* UIRect(GERectangle& inRectangle) { return reinterpret_cast<EeUiRect*>(&inRectangle); }

// --- UILayoutUtility (was UILayoutUtility.cpp; ee_ui/src/layout.rs) ------------------------------------------------

UILayoutUtility::UILayoutUtility()
{
    static_assert(sizeof(UILayoutUtility) == sizeof(EeUiLayout), "ee_ui/src/layout.rs");
    static_assert(offsetof(UILayoutUtility, mLeftOffset) == offsetof(EeUiLayout, left_offset), "ee_ui/src/layout.rs");
    static_assert(offsetof(UILayoutUtility, mRightOffset) == offsetof(EeUiLayout, right_offset), "ee_ui/src/layout.rs");
    static_assert(offsetof(UILayoutUtility, mTopOffset) == offsetof(EeUiLayout, top_offset), "ee_ui/src/layout.rs");
    static_assert(offsetof(UILayoutUtility, mBottomOffset) == offsetof(EeUiLayout, bottom_offset), "ee_ui/src/layout.rs");
    static_assert(offsetof(UILayoutUtility, mLeftOffsetType) == offsetof(EeUiLayout, left_type), "ee_ui/src/layout.rs");
    static_assert(offsetof(UILayoutUtility, mRightOffsetType) == offsetof(EeUiLayout, right_type), "ee_ui/src/layout.rs");
    static_assert(offsetof(UILayoutUtility, mTopOffsetType) == offsetof(EeUiLayout, top_type), "ee_ui/src/layout.rs");
    static_assert(offsetof(UILayoutUtility, mBottomOffsetType) == offsetof(EeUiLayout, bottom_type), "ee_ui/src/layout.rs");
    ee_ui_layout_init(reinterpret_cast<EeUiLayout*>(this));
}

void UILayoutUtility::CalculateRectangle(const GERectangle& inParent, GERectangle& outRectangle, bool inExclusive) const
{
    ee_ui_layout_calculate(reinterpret_cast<const EeUiLayout*>(this), UIRect(inParent), UIRect(outRectangle), inExclusive);
}

void UILayoutUtility::OffsetBottom(long inOffset, unsigned long inSize)
{
    ee_ui_layout_offset(reinterpret_cast<EeUiLayout*>(this), EeUiEdge::Bottom, inOffset, inSize);
}

void UILayoutUtility::OffsetLeft(long inOffset, unsigned long inSize)
{
    ee_ui_layout_offset(reinterpret_cast<EeUiLayout*>(this), EeUiEdge::Left, inOffset, inSize);
}

void UILayoutUtility::OffsetTop(long inOffset, unsigned long inSize)
{
    ee_ui_layout_offset(reinterpret_cast<EeUiLayout*>(this), EeUiEdge::Top, inOffset, inSize);
}

void UILayoutUtility::OffsetRight(long inOffset, unsigned long inSize)
{
    ee_ui_layout_offset(reinterpret_cast<EeUiLayout*>(this), EeUiEdge::Right, inOffset, inSize);
}
