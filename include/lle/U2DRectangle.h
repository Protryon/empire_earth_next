// Low-Level Engine U2DRectangle header, as seen by its includers (inferred header). Requires lle/LowLevelEngine.h.
#ifndef LLE_U2DRECTANGLE_H
#define LLE_U2DRECTANGLE_H

#include "lle/U2DPoint.h"
#include <type_traits>
#include "rust/base/math.h"

// Engine template (U2DPolygon<float>'s bounds, GETerrainMesh.cpp). Members alphabetical like GERectangle (+0 bottom,
// +4 left, +8 right, +0xc top: U2DConvexHull<float>::SetPoints 0x1002f795, Load 0x1003057d). (provisional names)
template <class T> class U2DRectangle {
public:
    U2DRectangle() : mBottom(0), mLeft(0), mRight(0), mTop(0) {}
    U2DRectangle(T inLeft, T inTop, T inRight, T inBottom) : mBottom(inBottom), mLeft(inLeft), mRight(inRight), mTop(inTop) {}
#ifdef LLE_EXE_HEADERS
    // exe: tile (inX, inY) lies inside (EEAIAStarBoundedPathGoal::AcceptTile 0xf4bc: Contains' compare order)
    // (provisional name; was EEAIAStarBounds)
    bool Contains(T inX, T inY) const { return inX >= this->mLeft && inX <= this->mRight && inY >= this->mTop && inY <= this->mBottom; }
    // exe: the rectangle of one point (EEAIAStarPathfindingGoalBase::SetStart 0x19c77 / SetGoal 0x19ca1 store in the
    // ctor's order) provisional name (was EEAIAStarBounds)
    void Set(const U2DPoint<T>& inPoint) { *this = U2DRectangle(inPoint.mX, inPoint.mY, inPoint.mX, inPoint.mY); }
    // exe: the middle of the rectangle (EEARust, the command units' bounds)
    void GetCenter(U2DPoint<T>& outCenter) const
        { outCenter = U2DPoint<T>((this->mRight - this->mLeft) / 2 + this->mLeft, (this->mBottom - this->mTop) / 2 + this->mTop); }
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
    U2DPoint<T> GetTopLeft() const { return U2DPoint<T>(this->mLeft, this->mTop); }
    U2DPoint<T> GetTopRight() const { return U2DPoint<T>(this->mRight, this->mTop); }
    // exe: the rectangle's point closest to inPoint: the nearer of the closest points on the two edges at the closest
    // corner (U2DRectangle<float> only: ee_base math u2d.rs)
    U2DPoint<T> GetClosestPoint(const U2DPoint<T>& inPoint) const
    {
        U2DPoint<T> thePoint;
        ee_u2d_rect_closest_point(reinterpret_cast<const float(*)[4]>(&this->mBottom), reinterpret_cast<const float(*)[2]>(&inPoint.mX),
                                  reinterpret_cast<float(*)[2]>(&thePoint.mX));
        return thePoint;
    }
#endif
    void Clear() { *this = U2DRectangle(); }
    // left, top, right and bottom from (to) an open file, each through its typed call; kErrorNotOpen when it is not
    // open, else the first error (ee_base math u2d.rs; T is long or float)
    long Load(FSFile& inFile) { return ee_u2d_rect_io(&inFile, inFile.GetIsOpen(), &this->mBottom, std::is_floating_point<T>::value, false); }
    long Save(FSFile& inFile) const
    {
        return ee_u2d_rect_io(&inFile, inFile.GetIsOpen(), const_cast<T*>(&this->mBottom), std::is_floating_point<T>::value,
                              true);
    }

    T mBottom;                              // +0x00
    T mLeft;                                // +0x04
    T mRight;                               // +0x08
    T mTop;                                 // +0x0c
};


#endif
