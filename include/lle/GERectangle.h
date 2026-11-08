// C:\EEX\Low-Level Engine\Graphics Engine\GERectangle.h (RECONSTRUCTION). Requires GE2DPoint.h and windows.h (tagRECT). The members below the getters
// are emitted at 0x1000e39e.. by the unit after GE3DPoint.cpp (src/.../GEBitmap.cpp).
#ifndef LLE_GERECTANGLE_H
#define LLE_GERECTANGLE_H

#include <vector>

class FSFile;

class UString;
struct tagRECT;

class LLE_API GERectangle {                 // proven layout by LLE's corner getters (GetTopLeft 0xe63d = (+4, +0xc),
public:                                     // GetBottomLeft 0xe5e4 = (+4, +0)); sizeof 0x10
    // inline (exported bodies ICF-folded with GE2DPoint::GetX/GetY): values, not memory operands
    // (Default.dll EEFlatClumps 0x90ca: `fld rect; fld point; faddp`)
    float GetBottom() const { return this->mBottom; }
    float GetLeft() const { return this->mLeft; }
    float GetRight() const { return this->mRight; }
    float GetTop() const { return this->mTop; }
    float GetHeight() const { return this->mBottom - this->mTop; }  // inline (exported 0xe637; Default.dll 0x41ca)
    float GetWidth() const { return this->mRight - this->mLeft; }  // inline (exported 0xe669)
    // inline float setters (exported bodies ICF-folded with GE2DPoint::SetX/SetY: SetLeft 0xc56c, SetRight 0xc576,
    // SetTop 0xe773, SetBottom 0x54ae4). UIButton::SetLocation 0x1005ed7a expands them: the unsigned border
    // thickness is re-read and narrowed (`fiadd dword`) per call.
    void SetBottom(float inBottom) { this->mBottom = inBottom; }
    void SetLeft(float inLeft) { this->mLeft = inLeft; }
    void SetRight(float inRight) { this->mRight = inRight; }
    void SetTop(float inTop) { this->mTop = inTop; }
    // long overloads (exported ?SetLeft@GERectangle@@QAEXJ@Z ..; bodies ICF-folded with GE2DPoint::SetX/SetY(long))
    void SetBottom(long inBottom) { this->mBottom = (float)inBottom; }
    void SetLeft(long inLeft) { this->mLeft = (float)inLeft; }
    void SetRight(long inRight) { this->mRight = (float)inRight; }
    void SetTop(long inTop) { this->mTop = (float)inTop; }

    // All seven ctors are inline in the real header (their exported COMDATs sit in GEBitmap.cpp's GERectangle
    // block, 0x1000e39e..0x1000e47f). Inline here: (), (float x4), (long x4) and (GE2DPoint, GE2DPoint), plus
    // Contains(GERectangle)/Intersects/SetTopLeft/SetBottomRight (GERhombus, RemoveIntersectingRectangle, the UI expand
    // them). (const GE2DPoint&), (const tagRECT&) and (GE2DPoint, float, float) still have their bodies in GEBitmap.cpp.
    // Default.dll's _Tree::_Dec pair (0x12ee6/0x14d30) is a function-count canary, not a header test: which copy
    // VC6 jump-threads depends on how many functions the unit compiles (LEARNINGS "unit-size effect").
    GERectangle()
    {
        this->mTop = 0.0f;
        this->mLeft = 0.0f;
        this->mBottom = 0.0f;
        this->mRight = 0.0f;
    }
    GERectangle(const GE2DPoint& inTopLeft, const GE2DPoint& inBottomRight)
    {
        this->mTop = inTopLeft.GetY();
        this->mLeft = inTopLeft.GetX();
        this->mBottom = inBottomRight.GetY();
        this->mRight = inBottomRight.GetX();
    }
    GERectangle(const GE2DPoint& inPoint);
    GERectangle(const tagRECT& inRect);
    GERectangle(long inLeft, long inTop, long inRight, long inBottom)   // 0xe413; UIBackgroundTable expands it
    {
        this->mLeft = (float)inLeft;
        this->mTop = (float)inTop;
        this->mRight = (float)inRight;
        this->mBottom = (float)inBottom;
    }
    GERectangle(float inLeft, float inTop, float inRight, float inBottom)
    {
        this->mLeft = inLeft;
        this->mTop = inTop;
        this->mRight = inRight;
        this->mBottom = inBottom;
    }
    GERectangle(const GE2DPoint& inTopLeft, float inWidth, float inHeight)   // 0xe453; UIComboBox's ctor expands it
    {
        this->mLeft = inTopLeft.GetX();
        this->mRight = this->mLeft + (inWidth - 1.0f);
        this->mTop = inTopLeft.GetY();
        this->mBottom = this->mTop + (inHeight - 1.0f);
    }
    // inline + empty: exported ??1GERectangle@@QAE@XZ (ICF 0xc16a); vector<GERectangle>::insert 0x24b50 keeps the
    // dead stores of its destroy loop and refuses the second _Ufill
    ~GERectangle() {}
    // inline: exported 0x1000e4f8 is emitted by GEBitmap.cpp, and GEViewport::SetOrthographicRectangle expands it
    // (store order top, left, bottom, right)
    GERectangle& operator=(const GERectangle& inRectangle)
    {
        this->mTop = inRectangle.mTop;
        this->mLeft = inRectangle.mLeft;
        this->mBottom = inRectangle.mBottom;
        this->mRight = inRectangle.mRight;
        return *this;
    }
    bool operator==(const GERectangle& inRectangle) const;
    bool operator!=(const GERectangle& inRectangle) const   // 0xe4c0; UITextLabel::ComputeTextLabelRect expands it
    {
        return this->mTop != inRectangle.mTop || this->mLeft != inRectangle.mLeft || this->mRight != inRectangle.mRight ||
               this->mBottom != inRectangle.mBottom;
    }
    bool Contains(const GE2DPoint& inPoint) const;
    bool Contains(const GERectangle& inRectangle) const
    {
        return inRectangle.mLeft >= this->mLeft && inRectangle.mRight <= this->mRight && inRectangle.mTop >= this->mTop &&
               inRectangle.mBottom <= this->mBottom;
    }
    void Extend(const GE2DPoint& inPoint);
    GE2DPoint GetBottomLeft() const
    {
#ifdef LLE_EXE_HEADERS
        GE2DPoint thePoint(this->mLeft, this->mBottom);
#else
        GE2DPoint thePoint(this->GetLeft(), this->GetBottom());
#endif
        return thePoint;
    }
    GE2DPoint GetBottomRight() const
    {
#ifdef LLE_EXE_HEADERS
        GE2DPoint thePoint(this->mRight, this->mBottom);
#else
        GE2DPoint thePoint(this->GetRight(), this->GetBottom());
#endif
        return thePoint;
    }
    void GetCenter(GE2DPoint& outCenter) const;
    GE2DPoint GetTopLeft() const
    {
#ifdef LLE_EXE_HEADERS
        GE2DPoint thePoint(this->mLeft, this->mTop);
#else
        GE2DPoint thePoint(this->GetLeft(), this->GetTop());
#endif
        return thePoint;
    }
    GE2DPoint GetTopRight() const
    {
#ifdef LLE_EXE_HEADERS
        GE2DPoint thePoint(this->mRight, this->mTop);
#else
        GE2DPoint thePoint(this->GetRight(), this->GetTop());
#endif
        return thePoint;
    }
    bool Intersects(const GERectangle& inRectangle) const
    {
        return this->mLeft <= inRectangle.mRight && this->mRight >= inRectangle.mLeft && this->mTop <= inRectangle.mBottom &&
               this->mBottom >= inRectangle.mTop;
    }
    void RoundToNearest();
    void SetBottomRight(const GE2DPoint& inPoint)
    {
        this->mBottom = inPoint.GetY();
        this->mRight = inPoint.GetX();
    }
    void SetTopLeft(const GE2DPoint& inPoint)
    {
        this->mTop = inPoint.GetY();
        this->mLeft = inPoint.GetX();
    }
    // GERectangle.cpp (0x100247ab..0x10024ad4)
    bool And(const GERectangle& inRectangle);
    bool And(const GERectangle& inRectangle, GERectangle& outRectangle) const;
    long Load(FSFile& inFile);
    void RemoveIntersectingRectangle(const GERectangle& inRectangle, std::vector<GERectangle>& outRectangles) const;
    long Save(FSFile& inFile) const;

    float mBottom;                          // +0x00
    float mLeft;                            // +0x04
    float mRight;                           // +0x08
    float mTop;                             // +0x0c
#ifdef LOWLEVELENGINE_EXPORTS
    // Inferred guard (class members, exported ?s..ID@GERectangle@@0VUString@@B): keeps the plug-ins' $E numbering
private:
    static const UString sLeftID;
    static const UString sTopID;
    static const UString sRightID;
    static const UString sBottomID;
#endif
};

#endif
