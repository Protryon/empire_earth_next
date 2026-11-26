// C:\EEX\Low-Level Engine\Graphics Engine\GERectangle.h (RECONSTRUCTION). Requires GE2DPoint.h and windows.h (tagRECT). The members below the getters
// are emitted at 0x1000e39e.. by the unit after GE3DPoint.cpp (src/.../GEBitmap.cpp).
#ifndef LLE_GERECTANGLE_H
#define LLE_GERECTANGLE_H

#include <vector>
#include "lle/GERust.h"

#include "lle/LLEForward.h"

class GERectangle : public EeGeRectangle {   // the Rust record (mBottom, mLeft, mRight, mTop; rust/base/math.h)
public:
    float GetBottom() const { return this->mBottom; }
    float GetLeft() const { return this->mLeft; }
    float GetRight() const { return this->mRight; }
    float GetTop() const { return this->mTop; }
    float GetHeight() const { return this->mBottom - this->mTop; }
    float GetWidth() const { return this->mRight - this->mLeft; }
    void SetBottom(float inBottom) { this->mBottom = inBottom; }
    void SetLeft(float inLeft) { this->mLeft = inLeft; }
    void SetRight(float inRight) { this->mRight = inRight; }
    void SetTop(float inTop) { this->mTop = inTop; }
    void SetBottom(long inBottom) { this->mBottom = (float)inBottom; }
    void SetLeft(long inLeft) { this->mLeft = (float)inLeft; }
    void SetRight(long inRight) { this->mRight = (float)inRight; }
    void SetTop(long inTop) { this->mTop = (float)inTop; }

    GERectangle() : EeGeRectangle{0.0f, 0.0f, 0.0f, 0.0f} {}
    GERectangle(const GE2DPoint& inTopLeft, const GE2DPoint& inBottomRight)
        : EeGeRectangle{inBottomRight.mY, inTopLeft.mX, inBottomRight.mX, inTopLeft.mY} {}
    GERectangle(const GE2DPoint& inPoint) : EeGeRectangle{inPoint.mY, inPoint.mX, inPoint.mX, inPoint.mY} {}
    GERectangle(const tagRECT& inRect)   // a Windows RECT
        : EeGeRectangle{(float)inRect.bottom, (float)inRect.left, (float)inRect.right, (float)inRect.top} {}
    GERectangle(long inLeft, long inTop, long inRight, long inBottom)
        : EeGeRectangle{(float)inBottom, (float)inLeft, (float)inRight, (float)inTop} {}
    GERectangle(float inLeft, float inTop, float inRight, float inBottom) : EeGeRectangle{inBottom, inLeft, inRight, inTop} {}
    // a width and height counting the first row and column (the right and bottom edges inside)
    GERectangle(const GE2DPoint& inTopLeft, float inWidth, float inHeight)
        : EeGeRectangle{inTopLeft.mY + (inHeight - 1.0f), inTopLeft.mX, inTopLeft.mX + (inWidth - 1.0f), inTopLeft.mY} {}
    bool operator==(const GERectangle& inRectangle) const
    {
        return this->mTop == inRectangle.mTop && this->mLeft == inRectangle.mLeft && this->mRight == inRectangle.mRight &&
               this->mBottom == inRectangle.mBottom;
    }
    bool operator!=(const GERectangle& inRectangle) const   // 0xe4c0; UITextLabel::ComputeTextLabelRect expands it
    {
        return this->mTop != inRectangle.mTop || this->mLeft != inRectangle.mLeft || this->mRight != inRectangle.mRight ||
               this->mBottom != inRectangle.mBottom;
    }
    bool Contains(const GE2DPoint& inPoint) const   // edges included
        { return inPoint.mX >= this->mLeft && inPoint.mX <= this->mRight && inPoint.mY >= this->mTop && inPoint.mY <= this->mBottom; }
    bool Contains(const GERectangle& inRectangle) const
    {
        return inRectangle.mLeft >= this->mLeft && inRectangle.mRight <= this->mRight && inRectangle.mTop >= this->mTop &&
               inRectangle.mBottom <= this->mBottom;
    }
    void Extend(const GE2DPoint& inPoint)   // just enough to contain the point (each edge reads only its own)
    {
        *this = GERectangle(this->mLeft < inPoint.mX ? this->mLeft : inPoint.mX, this->mTop < inPoint.mY ? this->mTop : inPoint.mY,
                            this->mRight > inPoint.mX ? this->mRight : inPoint.mX,
                            this->mBottom > inPoint.mY ? this->mBottom : inPoint.mY);
    }
    GE2DPoint GetBottomLeft() const { return GE2DPoint(this->mLeft, this->mBottom); }
    GE2DPoint GetBottomRight() const { return GE2DPoint(this->mRight, this->mBottom); }
    void GetCenter(GE2DPoint& outCenter) const
        { outCenter = GE2DPoint((this->mRight - this->mLeft) * 0.5f + this->mLeft, (this->mBottom - this->mTop) * 0.5f + this->mTop); }
    GE2DPoint GetTopLeft() const { return GE2DPoint(this->mLeft, this->mTop); }
    GE2DPoint GetTopRight() const { return GE2DPoint(this->mRight, this->mTop); }
    bool Intersects(const GERectangle& inRectangle) const
    {
        return this->mLeft <= inRectangle.mRight && this->mRight >= inRectangle.mLeft && this->mTop <= inRectangle.mBottom &&
               this->mBottom >= inRectangle.mTop;
    }
    void RoundToNearest()   // each edge to the nearest whole value
    {
        *this = GERectangle((float)UMFloor(this->mLeft + 0.5f), (float)UMFloor(this->mTop + 0.5f),
                            (float)UMFloor(this->mRight + 0.5f), (float)UMFloor(this->mBottom + 0.5f));
    }
    // clipped to another rectangle (or the overlap into outRectangle); false, nothing changed, when they do not overlap
    bool And(const GERectangle& inRectangle) { return ee_ge_rect_and_in_place(GERust(*this), GERust(inRectangle)); }
    bool And(const GERectangle& inRectangle, GERectangle& outRectangle) const
        { return ee_ge_rect_and(GERust(*this), GERust(inRectangle), GERust(outRectangle)); }
    // left, top, right and bottom from (to) an open file; kErrorNotOpen when it is not open, else the first error
    template <class F> long Load(F& inFile) { return ee_ge_rect_load(GERust(*this), GERustFile(inFile)); }
    template <class F> long Save(F& inFile) const { return ee_ge_rect_save(GERust(*this), GERustFile(inFile)); }
};

#endif
