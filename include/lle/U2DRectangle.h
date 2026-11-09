// Low-Level Engine U2DRectangle header, as seen by its includers (inferred header). Requires lle/LowLevelEngine.h.
#ifndef LLE_U2DRECTANGLE_H
#define LLE_U2DRECTANGLE_H

#include "lle/U2DPoint.h"

#ifndef LLE_EXE_HEADERS   // the exe has one U2DRectangle key chain (its first unit), not one per includer
                          // (EEContagionHelper.cpp, EECPAttackSubGroup.cpp: none)
const UString kU2DRectangleLeftKey("U2DRectangle-Left");
const UString kU2DRectangleTopKey("U2DRectangle-Top");
const UString kU2DRectangleRightKey("U2DRectangle-Right");
const UString kU2DRectangleBottomKey("U2DRectangle-Bottom");
#endif

// Engine template (U2DPolygon<float>'s bounds, GETerrainMesh.cpp). Members alphabetical like GERectangle (+0 bottom,
// +4 left, +8 right, +0xc top: U2DConvexHull<float>::SetPoints 0x1002f795, Load 0x1003057d). (provisional names)
template <class T> class U2DRectangle {
public:
    U2DRectangle()
    {
        this->mTop = 0;
        this->mLeft = 0;
        this->mBottom = 0;
        this->mRight = 0;
    }
#ifdef LLE_EXE_HEADERS
    // exe: user-declared empty dtor: a returned rectangle is then built in place, no temporary and movsd copy
    // (EEGetUnitsInRectangle's GetRegions 0xa50a6, 0x46bace 93 -> 99.66, 0x609847 91 -> 99.63, as U2DPoint's)
    ~U2DRectangle() {}
    // exe: user-declared (0x6c5ce, ICF RTSMapVertexColors::operator=: +0c, +04, +00, +08, the ctor's order): it makes
    // pair<U2DRectangle<long>, unsigned long>'s implicit operator= an out-of-line call (EECPBuildManager), and fill /
    // copy_backward copy member by member (EESelectedGroup 0x209b5c / 0x209b8a)
    U2DRectangle& operator=(const U2DRectangle& inOther)
    {
        this->mTop = inOther.mTop;
        this->mLeft = inOther.mLeft;
        this->mBottom = inOther.mBottom;
        this->mRight = inOther.mRight;
        return *this;
    }
    // exe: tile (inX, inY) lies inside (EEAIAStarBoundedPathGoal::AcceptTile 0xf4bc: Contains' compare order)
    // (provisional name; was EEAIAStarBounds)
    bool Contains(T inX, T inY) const { return inX >= this->mLeft && inX <= this->mRight && inY >= this->mTop && inY <= this->mBottom; }
    // exe: the rectangle of one point (EEAIAStarPathfindingGoalBase::SetStart 0x19c77 / SetGoal 0x19ca1 store in the
    // ctor's order) provisional name (was EEAIAStarBounds)
    void Set(const U2DPoint<T>& inPoint)
    {
        this->mTop = inPoint.mY;
        this->mLeft = inPoint.mX;
        this->mBottom = inPoint.mY;
        this->mRight = inPoint.mX;
    }
    // exe: the edges move out by inMargin, clamped at 0 (out of line 0x19f9d; defined below the class) (provisional
    // name)
    // name (was EEAIAStarBounds)
    void Grow(T inMargin, bool inClamp);
    // exe: the overlap of the two rectangles, false when they do not meet (0x7669d, ret 8; inline COMDAT emitted in
    // EECPCombatManager.cpp after its first user Update 0x47219a) (provisional name; was EEAIAStarBounds)
    bool Intersect(const U2DRectangle& inOther, U2DRectangle& outOverlap) const
    {
        bool theResult = false;
        if (this->mLeft <= inOther.mRight && this->mRight >= inOther.mLeft && this->mTop <= inOther.mBottom && this->mBottom >= inOther.mTop) {
            theResult = true;
            if (this->mLeft < inOther.mLeft)
                outOverlap.mLeft = inOther.mLeft;
            else
                outOverlap.mLeft = this->mLeft;
            if (this->mRight > inOther.mRight)
                outOverlap.mRight = inOther.mRight;
            else
                outOverlap.mRight = this->mRight;
            if (this->mTop < inOther.mTop)
                outOverlap.mTop = inOther.mTop;
            else
                outOverlap.mTop = this->mTop;
            if (this->mBottom > inOther.mBottom)
                outOverlap.mBottom = inOther.mBottom;
            else
                outOverlap.mBottom = this->mBottom;
        }
        return theResult;
    }
    // exe: the middle of the rectangle (0x61340, ret 4, hidden result; out of line, defined below the class)
    // (provisional name; was EEAIAStarBounds)
    void GetCenter(U2DPoint<T>& outCenter) const;
    // exe: the corner nearest inPoint, the first one on ties (inline COMDAT 0x208164, out of line after
    // EEGroup::MakeClusters, its only user; defined below the class) provisional name (was EEAIAStarBounds)
    U2DPoint<T> GetClosestCorner(const U2DPoint<T>& inPoint) const;
    // exe: an inline COMDAT first expanded out of line by the AI (0xd190, U2DPolygon<long>::Contains 0xd16e calls it
    // on its bounds; RTSMap 0x1238bd): inside, edges included (bool: push 1; pop eax) provisional name. Exe view only
    // so the engine's token stream stays unchanged
    bool Contains(const U2DPoint<T>& inPoint) const
        { return inPoint.GetX() >= this->mLeft && inPoint.GetX() <= this->mRight && inPoint.GetY() >= this->mTop && inPoint.GetY() <= this->mBottom; }
    // inline corner getters (exe 0x24c8bc rebuilds both corners for every loop test: the pair-shaped ctor, then the
    // out-of-line copy ctor of the returned value; GetClosestCorner 0x803f4 calls the COMDATs 0x80804 / 0x807f3 /
    // 0x8070d / 0x8071e out of line) provisional names (bottom-left/top-right)
    // (named local, then the return copy: the same code without the exe view's dtor; with one it is the only form that
    // keeps the original's temporary)
    U2DPoint<T> GetTopLeft() const { U2DPoint<T> thePoint(this->mLeft, this->mTop); return thePoint; }
    U2DPoint<T> GetBottomRight() const { U2DPoint<T> thePoint(this->mRight, this->mBottom); return thePoint; }
    U2DPoint<T> GetBottomLeft() const { U2DPoint<T> thePoint(this->mLeft, this->mBottom); return thePoint; }
    U2DPoint<T> GetTopRight() const { U2DPoint<T> thePoint(this->mRight, this->mTop); return thePoint; }
    // exe: the corner closest to inPoint and its two neighbours (inline COMDAT 0x803f4, ret 0x10, first expanded out of
    // line by EECPIArmy) (provisional name)
    U2DPoint<T> GetClosestCorner(const U2DPoint<T>& inPoint, U2DPoint<T>& outFirst, U2DPoint<T>& outSecond) const;
    // exe: the rectangle's point closest to inPoint: the nearer of the closest points on the two edges at the closest
    // corner (COMDAT 0x80347, ret 8, called by EECPIArmy::IsNear 0x80296; defined below the class) (provisional name)
    // (body)
    U2DPoint<T> GetClosestPoint(const U2DPoint<T>& inPoint) const;
#endif
    void Clear()
    {
        this->mTop = 0;
        this->mLeft = 0;
        this->mBottom = 0;
        this->mRight = 0;
    }
    void Extend(const U2DPoint<T>& inPoint)   // ICF-folded with GERectangle::Extend (0x1000e586)
    {
        this->mLeft = min(this->mLeft, inPoint.GetX());
        this->mRight = max(this->mRight, inPoint.GetX());
        this->mTop = min(this->mTop, inPoint.GetY());
        this->mBottom = max(this->mBottom, inPoint.GetY());
    }
    long Load(FSFile& inFile)               // 0x1003057d
    {
        long theResult;
        if (inFile.GetIsOpen()) {
            theResult = inFile.Read(this->mLeft);
            if (theResult == 0) {
                theResult = inFile.Read(this->mTop);
                if (theResult == 0) {
                    theResult = inFile.Read(this->mRight);
                    if (theResult == 0)
                        theResult = inFile.Read(this->mBottom);
                }
            }
        } else
            theResult = 4;
        return theResult;
    }
    long Save(FSFile& inFile) const         // 0x100305cf
    {
        long theResult;
        if (inFile.GetIsOpen()) {
            theResult = inFile.Write(this->mLeft);
            if (theResult == 0) {
                theResult = inFile.Write(this->mTop);
                if (theResult == 0) {
                    theResult = inFile.Write(this->mRight);
                    if (theResult == 0)
                        theResult = inFile.Write(this->mBottom);
                }
            }
        } else
            theResult = 4;
        return theResult;
    }

    T mBottom;                              // +0x00
    T mLeft;                                // +0x04
    T mRight;                               // +0x08
    T mTop;                                 // +0x0c
};

#ifdef LLE_EXE_HEADERS
// exe view only: out of line (never expanded; 0x19f9d, EEAIAStarPathfindingGoalBase's range)
template <class T>
void U2DRectangle<T>::Grow(T inMargin, bool inClamp)
{
    this->mLeft -= inMargin;
    this->mTop -= inMargin;
    this->mRight += inMargin;
    this->mBottom += inMargin;
    if (inClamp) {
        this->mLeft = max(this->mLeft, 0);
        this->mTop = max(this->mTop, 0);
    }
}
// exe view only: out of line (never expanded; 0x61340, EECPAttackSubGroup's range)
template <class T>
void U2DRectangle<T>::GetCenter(U2DPoint<T>& outCenter) const
{
    outCenter.mX = (this->mRight - this->mLeft) / 2 + this->mLeft;
    outCenter.mY = (this->mBottom - this->mTop) / 2 + this->mTop;
}
template <class T>
inline U2DPoint<T> U2DRectangle<T>::GetClosestCorner(const U2DPoint<T>& inPoint) const
{
    U2DPoint<T> theCorner(this->mLeft, this->mTop);
    U2DPoint<T> theClosest = theCorner;
    float theBest = inPoint.DistanceToSquaredF(theCorner);
    theCorner = U2DPoint<T>(this->mLeft, this->mBottom);
    float theDistance = inPoint.DistanceToSquaredF(theCorner);
    if (theDistance < theBest) {
        theBest = theDistance;
        theClosest = theCorner;
    }
    theCorner = U2DPoint<T>(this->mRight, this->mTop);
    theDistance = inPoint.DistanceToSquaredF(theCorner);
    if (theDistance < theBest) {
        theBest = theDistance;
        theClosest = theCorner;
    }
    theCorner = U2DPoint<T>(this->mRight, this->mBottom);
    theDistance = inPoint.DistanceToSquaredF(theCorner);
    if (theDistance < theBest) {
        theBest = theDistance;
        theClosest = theCorner;
    }
    return theClosest;
}
// exe view only: out of line (never expanded; COMDATs 0x803f4 / 0x80347, EECPIArmy's range)
template <class T>
U2DPoint<T> U2DRectangle<T>::GetClosestCorner(const U2DPoint<T>& inPoint, U2DPoint<T>& outFirst, U2DPoint<T>& outSecond) const
{
    U2DPoint<T> theCorner = this->GetTopLeft();
    U2DPoint<T> theClosest = theCorner;
    float theBest = inPoint.DistanceToSquaredF(theCorner);
    outFirst = this->GetBottomLeft();
    outSecond = this->GetTopRight();
    theCorner = this->GetBottomLeft();
    float theDistance = inPoint.DistanceToSquaredF(theCorner);
    if (theDistance < theBest) {
        theBest = theDistance;
        theClosest = theCorner;
        outFirst = this->GetBottomRight();
        outSecond = this->GetTopLeft();
    }
    theCorner = this->GetTopRight();
    theDistance = inPoint.DistanceToSquaredF(theCorner);
    if (theDistance < theBest) {
        theClosest = theCorner;
        theBest = theDistance;
        outFirst = this->GetTopLeft();
        outSecond = this->GetBottomRight();
    }
    theCorner = this->GetBottomRight();
    if (inPoint.DistanceToSquaredF(theCorner) < theBest) {
        theClosest = theCorner;
        outFirst = this->GetBottomLeft();
        outSecond = this->GetTopRight();
    }
    return theClosest;
}
template <class T>
U2DPoint<T> U2DRectangle<T>::GetClosestPoint(const U2DPoint<T>& inPoint) const
{
    U2DPoint<T> theFirst;
    U2DPoint<T> theSecond;
    U2DPoint<T> theResult;
    U2DPoint<T> theCorner = this->GetClosestCorner(inPoint, theFirst, theSecond);
    U2DPoint<T> theOther = theCorner;
    theResult = inPoint.GetClosestPointOnSegment(theCorner, theFirst);
    T theDistance = theResult.DistanceToSquared(inPoint);
    theOther = inPoint.GetClosestPointOnSegment(theCorner, theSecond);
    // the conditional assignment, then one return copy (0x80347 exact)
    if (theDistance < theOther.DistanceToSquared(inPoint))
        theOther = theResult;
    return theOther;
}
#endif

#endif
