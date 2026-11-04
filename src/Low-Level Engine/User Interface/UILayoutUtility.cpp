// C:\EEX\Low-Level Engine\User Interface\UILayoutUtility.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10077f75..0x10078251: the ctor, then the members alphabetical (private Get*Point among them), Offset* last in
// header order. No key chains, no COMDATs (the inline setters are emitted by UIBackground.cpp). UILBString.cpp's key
// chains start at 0x10078251.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/UILayoutUtility.h"

// FUNCTION: LLE 0x10077f75
// Creates a layout that fills its parent (left/top from the start, right/bottom from the end, zero offsets)
UILayoutUtility::UILayoutUtility()
{
    this->mLeftOffset = 0.0f;
    this->mLeftOffsetType = kLayoutFromStart;
    this->mTopOffsetType = kLayoutFromStart;
    this->mRightOffset = 0.0f;
    this->mRightOffsetType = kLayoutFromEnd;
    this->mTopOffset = 0.0f;
    this->mBottomOffsetType = kLayoutFromEnd;
    this->mBottomOffset = 0.0f;
}

// FUNCTION: LLE 0x10077f9c
// Computes the rectangle inside a parent from the four edge offsets, rounded; exclusive mode pulls right/bottom in by 1
void UILayoutUtility::CalculateRectangle(const GERectangle& inParent, GERectangle& outRectangle, bool inExclusive) const
{
    outRectangle.SetTop(this->GetVerticalPoint(inParent, this->mTopOffsetType, this->mTopOffset));
    outRectangle.SetLeft(this->GetHorizontalPoint(inParent, this->mLeftOffsetType, this->mLeftOffset));
    outRectangle.SetRight(this->GetHorizontalPoint(inParent, this->mRightOffsetType, this->mRightOffset));
    outRectangle.SetBottom(this->GetVerticalPoint(inParent, this->mBottomOffsetType, this->mBottomOffset));
    outRectangle.RoundToNearest();
    if (inExclusive == true) {
        outRectangle.SetRight(outRectangle.GetRight() - 1.0f);
        outRectangle.SetBottom(outRectangle.GetBottom() - 1.0f);
    }
}

// FUNCTION: LLE 0x10078023
// Returns an x coordinate in the parent for an offset type (start, end, centre, proportional, after the left edge)
float UILayoutUtility::GetHorizontalPoint(const GERectangle& inParent, unsigned long inType, float inOffset) const
{
    float theResult = 0.0f;
    switch (inType) {
    case kLayoutFromStartEdge:
        theResult = this->GetHorizontalPoint(inParent, this->mLeftOffsetType, this->mLeftOffset) + inOffset;
        break;
    case kLayoutProportional:
        theResult = inParent.GetLeft() + inParent.GetWidth() * inOffset;
        break;
    case kLayoutFromEnd:
        theResult = inParent.GetRight() - inOffset;
        break;
    case kLayoutBeforeCenter:
        theResult = inParent.GetLeft() + (inParent.GetWidth() * 0.5f - inOffset);
        break;
    case kLayoutAfterCenter:
        theResult = inParent.GetLeft() + (inParent.GetWidth() * 0.5f + inOffset);
        break;
    case kLayoutFromStart:
        theResult = inParent.GetLeft() + inOffset;
        break;
    }
    return theResult;
}

// FUNCTION: LLE 0x100780b1
// Returns a y coordinate in the parent for an offset type (start, end, centre, proportional, after the top edge)
float UILayoutUtility::GetVerticalPoint(const GERectangle& inParent, unsigned long inType, float inOffset) const
{
    float theResult = 0.0f;
    switch (inType) {
    case kLayoutFromStartEdge:
        theResult = this->GetVerticalPoint(inParent, this->mTopOffsetType, this->mTopOffset) + inOffset;
        break;
    case kLayoutProportional:
        theResult = inParent.GetTop() + inParent.GetHeight() * inOffset;
        break;
    case kLayoutFromEnd:
        theResult = inParent.GetBottom() - inOffset;
        break;
    case kLayoutBeforeCenter:
        theResult = inParent.GetTop() + (inParent.GetHeight() * 0.5f - inOffset);
        break;
    case kLayoutAfterCenter:
        theResult = inParent.GetTop() + (inParent.GetHeight() * 0.5f + inOffset);
        break;
    case kLayoutFromStart:
        theResult = inParent.GetTop() + inOffset;
        break;
    }
    return theResult;
}

// FUNCTION: LLE 0x1007813c
// Moves the bottom edge by a pixel offset in its offset type's direction (proportional: a fraction of inSize - 1)
void UILayoutUtility::OffsetBottom(long inOffset, unsigned long inSize)
{
    switch (this->mBottomOffsetType) {
    case kLayoutFromStart:
    case kLayoutAfterCenter:
    case kLayoutFromStartEdge:
        this->mBottomOffset += inOffset;
        break;
    case kLayoutBeforeCenter:
    case kLayoutFromEnd:
        this->mBottomOffset -= inOffset;
        break;
    case kLayoutProportional:
        this->mBottomOffset += (float)inOffset / (inSize - 1);
        break;
    }
}

// FUNCTION: LLE 0x10078182
// Moves the left edge by a pixel offset in its offset type's direction (proportional: a fraction of inSize - 1)
void UILayoutUtility::OffsetLeft(long inOffset, unsigned long inSize)
{
    switch (this->mLeftOffsetType) {
    case kLayoutFromStart:
    case kLayoutAfterCenter:
    case kLayoutFromStartEdge:
        this->mLeftOffset += inOffset;
        break;
    case kLayoutBeforeCenter:
    case kLayoutFromEnd:
        this->mLeftOffset -= inOffset;
        break;
    case kLayoutProportional:
        this->mLeftOffset += (float)inOffset / (inSize - 1);
        break;
    }
}

// FUNCTION: LLE 0x100781c5
// Moves the top edge by a pixel offset in its offset type's direction (proportional: a fraction of inSize - 1)
void UILayoutUtility::OffsetTop(long inOffset, unsigned long inSize)
{
    switch (this->mTopOffsetType) {
    case kLayoutFromStart:
    case kLayoutAfterCenter:
    case kLayoutFromStartEdge:
        this->mTopOffset += inOffset;
        break;
    case kLayoutBeforeCenter:
    case kLayoutFromEnd:
        this->mTopOffset -= inOffset;
        break;
    case kLayoutProportional:
        this->mTopOffset += (float)inOffset / (inSize - 1);
        break;
    }
}

// FUNCTION: LLE 0x1007820b
// Moves the right edge by a pixel offset, in the direction of the right edge's offset type
void UILayoutUtility::OffsetRight(long inOffset, unsigned long inSize)
{
    switch (this->mRightOffsetType) {   // Fixed: used to switch on the left edge's offset type
    case kLayoutFromStart:
    case kLayoutAfterCenter:
    case kLayoutFromStartEdge:
        this->mRightOffset += inOffset;
        break;
    case kLayoutBeforeCenter:
    case kLayoutFromEnd:
        this->mRightOffset -= inOffset;
        break;
    case kLayoutProportional:
        this->mRightOffset += (float)inOffset / (inSize - 1);
        break;
    }
}
