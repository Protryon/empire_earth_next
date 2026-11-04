// Low-Level Engine U2DPolygon header, as seen by its includers (inferred header). Requires lle/LowLevelEngine.h.
#ifndef LLE_U2DPOLYGON_H
#define LLE_U2DPOLYGON_H

#include "lle/U2DRectangle.h"

#ifndef LLE_EXE_HEADERS   // the exe has one U2DPolygon-PointCount key chain (0x1160, its first unit), not one per
                          // includer (EECPILocation.cpp has only its own chain)
const UString kU2DPolygonPointCountKey("U2DPolygon-PointCount");
#endif

// Engine templates (GETerrainMesh.cpp instantiates U2DPolygon<float>/U2DConvexHull<float>: vtables 0x10098010,
// 0x1009804c, RTTI .?AV?$U2DPolygon@M@@ / .?AV?$U2DConvexHull@M@@). Slot order from the vtables; member names
// are provisional (no export names them). Needs <map>, <math.h> and lle/U2DRectangle.h at the point of use.
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_EXE_HEADERS)
// The exe instantiates U2DPolygon<long>/U2DConvexHull<long> itself (EEAIArea::mHull; U2DPolygon<long> ctor 0x11251,
// Contains 0xd16e): the templates are visible to the exe too. The std::less
// specialisation stays engine-only.
class GE3DLine;

#ifdef LOWLEVELENGINE_EXPORTS

// Inferred: an explicit std::less for the point type. U2DPolygon<T>::Sort's sort(.., less<U2DPoint<T> >()) family
// expands this comparison inline (0x10031ebe/0x10032015/0x100321ac/0x1003278e: fld;fld;fcompp accessor loads), while
// the bare sort(F, L) family calls U2DPoint::operator< out of line (0x10031459) with the same expression; a plain
// less<> forwarding to operator< is refused there like the direct calls.
namespace std {
template<> struct less<U2DPoint<float> > {   // (binary_function base dropped: removed from C++17)
    bool operator()(const U2DPoint<float>& inA, const U2DPoint<float>& inB) const
        { return inA.GetY() < inB.GetY() || (inA.GetY() == inB.GetY() && inA.GetX() > inB.GetX()); }
};
}
#endif

// Provisional name: U2DPolygon<T>::Sort's predicate (lower y first, then larger x), a type of its own with the compare
// inline. exe: the U2DPolygon<long>::Sort family (sort 0x413afd, _Sort 0x41428d, _Unguarded_partition 0x414995,
// _Unguarded_insert 0x414ab0, _Adjust_heap 0x41513e, _Push_heap 0x41536b) expands this compare (y: jl/jne, x: jle),
// while EEUCGate's std::less<U2DPoint<long> > family (_Sort 0x6120d9, ..) calls U2DPoint::operator< out of line:
// two COMDAT families, so two predicate types
template <class T> struct U2DPointLess {
    bool operator()(const U2DPoint<T>& inA, const U2DPoint<T>& inB) const
        { return inA.GetY() < inB.GetY() || (inA.GetY() == inB.GetY() && inA.GetX() > inB.GetX()); }
};

template <class T> class U2DPolygon {      // sizeof 0x2c
public:
    U2DPolygon() {}                         // 0x1002dc4a (out of line)
    virtual ~U2DPolygon() {}                // ??_G 0x1002d9c1
#ifdef LLE_EXE_HEADERS
    virtual float ComputeArea() = 0;        // slot 1 (exe: a float, U2DConvexHull<long> 0x41214b returns it with fld)
#else
    virtual T ComputeArea() = 0;            // slot 1
#endif
    virtual void SetPoints(const std::vector<U2DPoint<T> >& inPoints, bool inIsSorted) = 0;   // slot 2
    virtual void Validate()                 // slot 3 (0x1002e82f): the checks are compiled out
    {
        for (unsigned long i = 0; i < this->mPoints.size(); i++)
            ;
    }
    // slot 4 (0x1002e84a): inPoint projected onto the line inStart..inEnd, scaled by inScale
#ifdef LLE_EXE_HEADERS
    // exe (U2DPolygon<long> 0x11343): the scale is a float and the distance DistanceToF (0xe92e, out of line)
    virtual U2DPoint<T> ProjectPoint(const U2DPoint<T>& inPoint, float inScale, const U2DPoint<T>& inStart,
                                     const U2DPoint<T>& inEnd)
    {
        GEVector theDirection;
        GEVector theToPoint;
        U2DPoint<T> theResult = inPoint;
        if (inStart != inEnd && inPoint != inStart && inPoint != inEnd) {
            theToPoint.SetX((float)inPoint.mX - (float)inStart.mX);
            theToPoint.SetY((float)inPoint.mY - (float)inStart.mY);
            theToPoint.Normalize();
            theDirection.SetX((float)inEnd.mX - (float)inStart.mX);
            theDirection.SetY((float)inEnd.mY - (float)inStart.mY);
            theDirection.Normalize();
            float theDistance = inStart.DistanceToF(inPoint) * inScale;
            theResult.mX = (T)(theDirection.GetX() * theDistance + inStart.mX);
            theResult.mY = (T)(theDirection.GetY() * theDistance + inStart.mY);
        }
        return theResult;
    }
#else
    virtual U2DPoint<T> ProjectPoint(const U2DPoint<T>& inPoint, T inScale, const U2DPoint<T>& inStart,
                                     const U2DPoint<T>& inEnd)
    {
        T theY = inPoint.mY;
        U2DPoint<T> theResult;
        GEVector theDirection;
        GEVector theToPoint;
        theResult.mX = inPoint.mX;
        if (inStart != inEnd && inPoint != inStart && inPoint != inEnd) {
            theToPoint.SetX(inPoint.GetX() - inStart.GetX());
            theToPoint.SetY(inPoint.GetY() - inStart.GetY());
            theToPoint.Normalize();
            theDirection.SetX(inEnd.GetX() - inStart.GetX());
            theDirection.SetY(inEnd.GetY() - inStart.GetY());
            theDirection.Normalize();
            T theDistance = inStart.DistanceTo(inPoint) * inScale;
            theResult.mX = theDirection.GetX() * theDistance + inStart.GetX();
            theY = theDirection.GetY() * theDistance + inStart.GetY();
        }
        theResult.mY = theY;
        return theResult;
    }
#endif
    // slot 5 (0x1002e931): the point of the outline nearest inPoint. One result, one return (every path copies
    // theResult at 0x2ef13); the 3+ point case is the fall-through (cmp 2; jbe to the two-point case); the else of
    // `inPoint != mPoints[i]` is a second theFound store merged with the first (je 0x2ed3f); the two
    // `theResult = mPoints[theIndex]` statements share their tail (0x2ed5e); locals in zero-init order (edge before
    // to-point). Still open: the original refuses the last copies (return, = inPoint, front()) for budget it spends
    // earlier (about 16 EE_LOST_DEBUG units at the top reproduce it; no source for it found).
    virtual U2DPoint<T> GetClosestOutlinePoint(const U2DPoint<T>& inPoint)
    {
        U2DPoint<T> theResult;
        U2DPoint<T> theCandidate;
        GEVector theEdge;
        GEVector theToPoint;
#ifdef LLE_EXE_HEADERS
        // exe 0x411421: the dot product and the best distance are floats (fst [ebp-0x10]; fcomp 0.0; the projection
        // gets the float dot; the distances are DistanceToSquaredF 0xe964)
        float theBest = 0.0f;
        float theDot;
#else
        T theBest = 0.0f;
        T theDot;
#endif
        long theIndex = this->GetClosestPointIndex(inPoint);
        long thePrevious = theIndex > 0 ? theIndex - 1 : (long)this->mPoints.size() - 1;
        long theNext = (theIndex < (long)this->mPoints.size() - 1) ? theIndex + 1 : 0;
        bool theFound = false;
        if (theIndex >= 0 && theIndex < (long)this->mPoints.size() && this->mPoints[theIndex] != inPoint) {
            if (this->mPoints.size() > 1) {
                if (this->mPoints.size() > 2) {
                    if (theNext != thePrevious) {
                        if (inPoint != this->mPoints[theIndex]) {
                            theToPoint.SetX(inPoint.GetX() - this->mPoints[theIndex].GetX());
                            theToPoint.SetY(inPoint.GetY() - this->mPoints[theIndex].GetY());
                            theToPoint.Normalize();
                            theEdge.SetX(this->mPoints[thePrevious].GetX() - this->mPoints[theIndex].GetX());
                            theEdge.SetY(this->mPoints[thePrevious].GetY() - this->mPoints[theIndex].GetY());
                            theEdge.Normalize();
                            theDot = theEdge.DotProduct(theToPoint);
                            if (theDot >= 0.0f) {
                                theResult = this->ProjectPoint(inPoint, theDot, this->mPoints[theIndex], this->mPoints[thePrevious]);
#ifdef LLE_EXE_HEADERS
                                theBest = inPoint.DistanceToSquaredF(theResult);
#else
                                theBest = inPoint.DistanceToSquared(theResult);
#endif
                                theFound = true;
                            }
                            theEdge.SetX(this->mPoints[theNext].GetX() - this->mPoints[theIndex].GetX());
                            theEdge.SetY(this->mPoints[theNext].GetY() - this->mPoints[theIndex].GetY());
                            theEdge.Normalize();
                            theDot = theEdge.DotProduct(theToPoint);
                            if (theDot >= 0.0f) {
                                theCandidate = this->ProjectPoint(inPoint, theDot, this->mPoints[theIndex], this->mPoints[theNext]);
#ifdef LLE_EXE_HEADERS
                                if (theBest == 0.0f || inPoint.DistanceToSquaredF(theCandidate) < theBest) {
#else
                                if (theBest == 0.0f || inPoint.DistanceToSquared(theCandidate) < theBest) {
#endif
                                    theResult = theCandidate;
                                    theFound = true;
                                }
                            }
                        } else
                            theFound = true;
                        if (!theFound)
                            theResult = this->mPoints[theIndex];
                    } else
                        theResult = this->mPoints[theIndex];
                } else {
                    theToPoint.SetX(inPoint.GetX() - this->mPoints[theIndex].GetX());
                    theToPoint.SetY(inPoint.GetY() - this->mPoints[theIndex].GetY());
                    theToPoint.Normalize();
                    theEdge.SetX(this->mPoints[thePrevious].GetX() - this->mPoints[theIndex].GetX());
                    theEdge.SetY(this->mPoints[thePrevious].GetY() - this->mPoints[theIndex].GetY());
                    theEdge.Normalize();
                    theDot = theEdge.DotProduct(theToPoint);
                    if (theDot >= 0.0f) {
                        theResult = this->ProjectPoint(inPoint, theDot, this->mPoints[theIndex], this->mPoints[thePrevious]);
#ifdef LLE_EXE_HEADERS
                        inPoint.DistanceToSquaredF(theResult);
#else
                        inPoint.DistanceToSquared(theResult);
#endif
                    } else
                        theResult = this->mPoints[theIndex];
                }
            } else
                theResult = this->mPoints.front();
        } else
            theResult = inPoint;
        return theResult;
    }
    // slot 6 (0x1002ef29): the closest pair of points between this outline and inPolygon's. make_pair twice (the
    // pair<long,long> and pair<float,pair> ctors are out of line, 0x1002fe2b/0x100308a7, then the converting copy);
    // one begin() read into two new block locals (second.first stays in a fresh slot, second.second in edi); the
    // `<` test keeps the swapped pair in the fall-through. Still open: the original inlines begin() and refuses the
    // else branch's operator[]/operator= (about 4 units of budget short, as in GetClosestOutlinePoint).
    virtual void GetClosestPoints(U2DPolygon& inPolygon, U2DPoint<T>& outPoint, U2DPoint<T>& outOtherPoint)
    {
        // float keys for every T (exe <long>: fldz, DistanceToSquaredF 0xe964, the tree's fld/fcomp insert 0x412af4)
        std::multimap<float, std::pair<long, long> > theDistances;
        U2DPoint<T> theOther;
        U2DPoint<T> theThis;
        float theDistance = 0.0f;
        long theIndex;
        long theOtherIndex;
        outPoint.mX = 0;
        outPoint.mY = 0;
        outOtherPoint.mX = 0;
        outOtherPoint.mY = 0;
        for (theIndex = this->mPoints.size() - 1; theIndex >= 0; theIndex--) {
            theOtherIndex = inPolygon.GetClosestPointIndex(this->mPoints[theIndex]);
            if (theOtherIndex < (long)inPolygon.mPoints.size())
#ifdef LLE_EXE_HEADERS
                theDistance = inPolygon.mPoints[theOtherIndex].DistanceToSquaredF(this->mPoints[theIndex]);
#else
                theDistance = inPolygon.mPoints[theOtherIndex].DistanceToSquared(this->mPoints[theIndex]);
#endif
            theDistances.insert(std::make_pair(theDistance, std::make_pair(theIndex, theOtherIndex)));
        }
        if (theDistances.size() > 0) {
            std::multimap<float, std::pair<long, long> >::iterator theBest = theDistances.begin();
            long theBestIndex = theBest->second.first;
            long theBestOtherIndex = theBest->second.second;
            theOther = inPolygon.GetClosestOutlinePoint(this->mPoints[theBestIndex]);
            theThis = this->GetClosestOutlinePoint(inPolygon.mPoints[theBestOtherIndex]);
#ifdef LLE_EXE_HEADERS
            theDistance = this->mPoints[theBestIndex].DistanceToSquaredF(theOther);
            if (inPolygon.mPoints[theBestOtherIndex].DistanceToSquaredF(theThis) < theDistance) {
#else
            theDistance = this->mPoints[theBestIndex].DistanceToSquared(theOther);
            if (inPolygon.mPoints[theBestOtherIndex].DistanceToSquared(theThis) < theDistance) {
#endif
                outPoint = theThis;
                outOtherPoint = inPolygon.mPoints[theBestOtherIndex];
            } else {
                outPoint = this->mPoints[theBestIndex];
                outOtherPoint = theOther;
            }
        }
    }
    // slot 7 (0x1002f1bd): the vertex nearest inPoint (`size() > 0`: sar; je). Still open: the original refuses the
    // two front `end() - 1` operator- calls and inlines the loop's operator[] (ours the reverse); back() and
    // rbegin() are refused whole instead.
    virtual U2DPoint<T> GetClosestPoint(const U2DPoint<T>& inPoint)
    {
        U2DPoint<T> theResult;
        T theBest;
        T theDistance;
        long i;
        if (this->mPoints.size() > 0) {
            theBest = inPoint.DistanceToSquared(*(this->mPoints.end() - 1));
            theResult = *(this->mPoints.end() - 1);
            for (i = this->mPoints.size() - 2; i >= 0; i--) {
                theDistance = inPoint.DistanceToSquared(this->mPoints[i]);
                if (theDistance < theBest) {
                    theBest = theDistance;
                    theResult = this->mPoints[i];
                }
            }
        }
        return theResult;
    }
    // slot 8 (0x1002f28d)
    virtual long GetClosestPointIndex(const U2DPoint<T>& inPoint)
    {
        long theResult = -1;
        T theBest;
        T theDistance;
        long i;
        if (this->mPoints.size() > 0) {
            theBest = inPoint.DistanceToSquared(this->mPoints.back());
            theResult = this->mPoints.size() - 1;
            for (i = this->mPoints.size() - 2; i >= 0; i--) {
                theDistance = inPoint.DistanceToSquared(this->mPoints[i]);
                if (theDistance < theBest) {
                    theBest = theDistance;
                    theResult = i;
                }
            }
        }
        return theResult;
    }
    virtual long Load(FSFile& inFile)       // slot 9 (0x1002f318)
    {
        long theResult;
        long i;
        long theCount = 0;
        if (inFile.GetIsOpen()) {
            theResult = inFile.Read(theCount);
            if (theResult == 0) {
                this->mPoints.resize(theCount);
                for (i = 0; i < theCount; i++) {
                    theResult = this->mPoints[i].Load(inFile);
                    if (theResult != 0)
                        return theResult;
                }
                theResult = this->mBounds.Load(inFile);
            }
        } else
            theResult = 4;
        return theResult;
    }
    virtual long Save(FSFile& inFile)       // slot 10 (0x1002f37e)
    {
        long theResult;
        long i;
        long theCount = this->mPoints.size();
        if (inFile.GetIsOpen()) {
            theResult = inFile.Write(theCount);
            if (theResult == 0) {
                for (i = 0; i < theCount; i++) {
                    U2DPoint<T>& thePoint = this->mPoints[i];  // the element address before the push
                    theResult = thePoint.Save(inFile);
                    if (theResult != 0)
                        return theResult;
                }
                theResult = this->mBounds.Save(inFile);
            }
        } else
            theResult = 4;
        return theResult;
    }
    // slot 11 (0x1002f3f1): crossing-number containment test. The accessor/SetXY shape sets the inline budget (the
    // original refuses begin() at the first three mPoints[] sites only); thePreviousY is its own statement (its
    // begin() temporary dies before the x site: one shared slot, frame 0x54).
    virtual bool Contains(const U2DPoint<T>& inPoint)
    {
        U2DPoint<T> theDelta;
        U2DPoint<T> thePreviousDelta;
        long theRightCrossings = 0;
        long theLeftCrossings = 0;
        long theCount = this->mPoints.size();
        long thePrevious;
        long i;
        bool theResult = false;
        bool theRightStraddle;
        bool theLeftStraddle;
        float theX;   // the crossing on the x87 (exe 0x411ecb fild/fidiv)
        if (theCount != 0) {
            for (i = 0; i < theCount; i++) {
                if (theDelta == this->mPoints[i]) {
                    theResult = true;
                    break;
                }
                theDelta.SetXY(this->mPoints[i].GetX() - inPoint.GetX(), this->mPoints[i].GetY() - inPoint.GetY());
                thePrevious = (i + theCount - 1) % theCount;
                T thePreviousY = this->mPoints[thePrevious].GetY() - inPoint.GetY();
                thePreviousDelta.SetXY(this->mPoints[thePrevious].GetX() - inPoint.GetX(), thePreviousY);
                theRightStraddle = (theDelta.mY > 0) != (thePreviousDelta.mY > 0);
                theLeftStraddle = (theDelta.mY < 0) != (thePreviousDelta.mY < 0);
                if (theRightStraddle || theLeftStraddle) {
                    theX = (float)(thePreviousDelta.GetY() * theDelta.GetX() - theDelta.GetY() * thePreviousDelta.GetX()) /
                           (thePreviousDelta.GetY() - theDelta.GetY());
                    if (theRightStraddle && theX > 0.0f)
                        theRightCrossings++;
                    if (theLeftStraddle && theX < 0.0f)
                        theLeftCrossings++;
                }
            }
            if (theRightCrossings % 2 != theLeftCrossings % 2 || theRightCrossings % 2 == 1)
                theResult = true;
        }
        return theResult;
    }
#ifdef LLE_EXE_HEADERS
    // exe 0x41209a (U2DPolygon<long>): the game's revision accumulates in float and converts once at the end
    // (fild/fdiv/fadd per point, two __ftol); the engine's own 0x1002f5ef keeps the U2DPoint<T> form below.
    // Integer division per point truncated every AI hull centre.
    virtual void ComputeCenter()            // slot 12
    {
        float theX = 0.0f;
        float theY = 0.0f;
        long theCount = this->mPoints.size();
        for (long i = theCount - 1; i >= 0; i--) {
            theX += this->mPoints[i].GetX() / (float)theCount;
            theY += this->mPoints[i].GetY() / (float)theCount;
        }
        this->mCenter.SetX(theX);
        this->mCenter.SetY(theY);
    }
#else
    virtual void ComputeCenter()            // slot 12 (0x1002f5ef)
    {
        U2DPoint<T> theCenter;
        long theCount = this->mPoints.size();
        for (long i = theCount - 1; i >= 0; i--) {
            theCenter.SetX(theCenter.GetX() + this->mPoints[i].GetX() / (T)theCount);
            theCenter.SetY(theCenter.GetY() + this->mPoints[i].GetY() / (T)theCount);
        }
        this->mCenter = theCenter;
    }
#endif
    virtual void Sort();                    // slot 13 (0x1002f649, below U2DGrahamsAlgorithm)
    // 0x10030746 (provisional name): takes the predicate by reference; Sort() passes an uninitialised temporary
#ifdef LLE_EXE_HEADERS
    void Sort(const U2DPointLess<T>& inPredicate) { std::sort(this->mPoints.begin(), this->mPoints.end(), inPredicate); }
#else
    void Sort(const std::less<U2DPoint<T> >& inPredicate) { std::sort(this->mPoints.begin(), this->mPoints.end(), inPredicate); }
#endif
    U2DPoint<T>* GetPoint(long inIndex) { return &this->mPoints[inIndex]; }  // inline (0x1002feb7 is vector::operator[])
    unsigned long GetPointCount() { return this->mPoints.size(); }      // 0x1002ffe4
    const U2DPoint<T>& GetCenter() const { return this->mCenter; }  // exe (EECPDefenseCommander reads a town's) (provisional name)

protected:
    std::vector<U2DPoint<T> > mPoints;      // +0x04
    U2DRectangle<T> mBounds;                // +0x14
    U2DPoint<T> mCenter;                    // +0x24
};

// Graham's scan over a point set (vtable 0x100980ac, RTTI .?AV?$U2DGrahamsAlgorithm@M@@). (provisional names)
template <class T> class U2DGrahamsAlgorithm {
public:
    struct GAPoint {                        // sizeof 0x10
        GAPoint() : mIndex(-1), mIsDeleted(false) {}   // resize(n) 0x100308c3: or [+8], -1; and [+0xc], 0
        U2DPoint<T> mPoint;                 // +0x00
        long mIndex;                        // +0x08
        bool mIsDeleted;                    // +0x0c
    };
    // The sort predicate: angle order around the pivot (mPoints[0]).
    class GAAngleCompare {
    public:
        GAAngleCompare(U2DGrahamsAlgorithm* inAlgorithm) : mAlgorithm(inAlgorithm)   // 0x100308e5
            { this->mPivot = inAlgorithm->mPoints[0].mPoint; }
        bool operator()(GAPoint& inA, GAPoint& inB)
        {
            // T(0): the exe's <long> compares the side as an integer (0x414c48: test eax; jle/jge), the engine's <float>
            // as a float
            bool theResult;
            long theSide = inB.mPoint.GetSide(this->mPivot, inA.mPoint);
            if (theSide > T(0))
                theResult = true;
            else if (theSide < T(0))
                theResult = false;
            else
                theResult = this->IsNearer(inA, inB);
            return theResult;
        }
        // 0x1003232b (provisional name): collinear tie-break, the point nearer the pivot (per axis) is dropped
        bool IsNearer(GAPoint& inA, GAPoint& inB)
        {
            T theDX = UMAbs(inA.mPoint.GetX() - this->mPivot.GetX()) - UMAbs(inB.mPoint.GetX() - this->mPivot.GetX());
            T theDY = UMAbs(inA.mPoint.GetY() - this->mPivot.GetY()) - UMAbs(inB.mPoint.GetY() - this->mPivot.GetY());
            if (theDX < T(0) || theDY < T(0)) {     // T(0): integer compares in the exe's <long> (0x414c7d)
                inA.mIsDeleted = true;
                return true;
            }
            if (theDX > T(0) || theDY > T(0)) {
                inB.mIsDeleted = true;
                return false;
            }
            if (inA.mIndex > inB.mIndex)
                inB.mIsDeleted = true;
            else if (inA.mIndex != inB.mIndex)
                inA.mIsDeleted = true;
            return false;
        }
        U2DGrahamsAlgorithm* mAlgorithm;    // +0x00
        U2DPoint<T> mPivot;                 // +0x04
    };

    U2DGrahamsAlgorithm() {}
    virtual ~U2DGrahamsAlgorithm() {}       // ??_G 0x10030022
    void Compute(const std::vector<U2DPoint<T> >& inPoints, std::vector<U2DPoint<T> >& outHull)   // 0x10030621
    {
        if (inPoints.size() >= 3) {
            this->mPoints.resize(inPoints.size());
            for (long i = inPoints.size() - 1; i >= 0; i--) {
                this->mPoints[i].mPoint = inPoints[i];
                this->mPoints[i].mIndex = i;
                this->mPoints[i].mIsDeleted = false;
            }
            this->MoveLowestToFront();
            std::sort(this->mPoints.begin() + 1, this->mPoints.end(), GAAngleCompare(this));
            this->RemoveDeleted();
            this->Scan(outHull);
        }
    }

    void MoveLowestToFront()                // 0x10030cd5
    {
        unsigned long theLowest = 0;
        unsigned long theCount = this->mPoints.size();
        for (unsigned long i = 1; i < theCount; i++) {
            if (this->mPoints[i].mPoint < this->mPoints[theLowest].mPoint)
                theLowest = i;
        }
        std::swap(this->mPoints[0], this->mPoints[theLowest]);
    }
    void RemoveDeleted()                    // 0x10030d47
    {
        unsigned long theKept = 0;
        for (unsigned long i = 0; i < this->mPoints.size(); i++) {
            if (!this->mPoints[i].mIsDeleted) {
                this->mPoints[theKept] = this->mPoints[i];
                theKept++;
            }
        }
        if (theKept < this->mPoints.size())
            this->mPoints.erase(this->mPoints.begin() + theKept, this->mPoints.end());
    }
    void Scan(std::vector<U2DPoint<T> >& outHull)   // 0x10030e32
    {
        std::vector<GAPoint> theStack;
        long i;
        if (this->mPoints.size() >= 2) {
            theStack.push_back(this->mPoints[0]);
            theStack.push_back(this->mPoints[1]);
            for (i = 2; i < (long)this->mPoints.size();) {
                if (this->mPoints[i].mPoint.GetSide(theStack[theStack.size() - 2].mPoint,
                                              theStack[theStack.size() - 1].mPoint) > 0) {
                    theStack.push_back(this->mPoints[i]);
                    i++;
                } else
                    theStack.pop_back();
            }
        }
        outHull.resize(theStack.size());
        for (i = theStack.size() - 1; i >= 0; i--)
            outHull[i] = theStack[i].mPoint;
    }

    std::vector<GAPoint> mPoints;           // +0x04
};

template <class T> void U2DPolygon<T>::Sort()
{
    U2DGrahamsAlgorithm<T> theAlgorithm;
#ifdef LLE_EXE_HEADERS
    this->Sort(U2DPointLess<T>());
#else
    this->Sort(std::less<U2DPoint<T> >());
#endif
    theAlgorithm.Compute(this->mPoints, this->mPoints);
}

// The convex hull of a point set (vtable 0x1009804c). (provisional names)
template <class T> class U2DConvexHull : public U2DPolygon<T> {   // sizeof 0x30
public:
    U2DConvexHull() {}                      // mArea is not zeroed: GETerrainMesh::GetTileRowsInVolume 0x1002ac2d, exe EEAIArea 0x110cb
    void Clear()                            // 0x1002ffb4
    {
        this->mBounds.Clear();
        this->mPoints.clear();
        this->mArea = 0;
        this->mCenter.mX = 0;
        this->mCenter.mY = 0;
    }
#ifdef LLE_EXE_HEADERS
    // exe (U2DConvexHull<long> 0x41214b): the area is a float (fldz/fstp, fadd/fstp into +0x2c, the size<3 branch
    // `fild qword`, returned with fld) and the first point is copied as floats (fild/fstp)
    virtual float ComputeArea()
    {
        float theFirstX;
        float theFirstY;
        long i;
        long j;
        this->mArea = 0;
        if (this->mPoints.size() >= 3) {
            theFirstX = this->mPoints.front().GetX();
            theFirstY = this->mPoints.front().GetY();
            for (i = this->mPoints.size() - 1; i >= 2; i--) {
                j = i - 1;
                this->mArea = ((this->mPoints[i].GetX() - theFirstX) * (this->mPoints[j].GetY() - theFirstY) -
                         (this->mPoints[j].GetX() - theFirstX) * (this->mPoints[i].GetY() - theFirstY)) * 0.5f + this->mArea;
            }
        } else
            this->mArea = this->mPoints.size();
        return this->mArea;
    }
#else
    virtual T ComputeArea()                 // slot 1 (0x1002f692)
    {
        U2DPoint<T> theFirst;
        long i;
        long j;
        this->mArea = 0;
        if (this->mPoints.size() >= 3) {
            theFirst = this->mPoints.front();
            for (i = this->mPoints.size() - 1; i >= 2; i--) {
                j = i - 1;
                this->mArea = ((this->mPoints[i].GetX() - theFirst.GetX()) * (this->mPoints[j].GetY() - theFirst.GetY()) -
                         (this->mPoints[j].GetX() - theFirst.GetX()) * (this->mPoints[i].GetY() - theFirst.GetY())) * 0.5f + this->mArea;
            }
        } else
            this->mArea = (T)this->mPoints.size();
        return this->mArea;
    }
#endif
    virtual void SetPoints(const std::vector<U2DPoint<T> >& inPoints, bool inIsSorted)   // slot 2 (0x1002f795)
    {
        long i;
        this->Clear();
        this->mPoints.resize(inPoints.size());
        if (inPoints.size() != 0) {
            this->mBounds.mTop = (*(inPoints.end() - 1)).mY;
            this->mBounds.mLeft = (*(inPoints.end() - 1)).mX;
            this->mBounds.mBottom = (*(inPoints.end() - 1)).mY;
            this->mBounds.mRight = (*(inPoints.end() - 1)).mX;
            this->mPoints[this->mPoints.size() - 1] = inPoints[this->mPoints.size() - 1];
        }
        for (i = inPoints.size() - 2; i >= 0; i--) {
            this->mPoints[i] = inPoints[i];
            this->mBounds.Extend(*this->GetPoint(i));
        }
        this->Compute(inIsSorted);
        this->ComputeArea();
    }
    virtual long Load(FSFile& inFile) { return U2DPolygon<T>::Load(inFile); }   // slot 9 (0x1002fc0a)
    virtual long Save(FSFile& inFile) { return U2DPolygon<T>::Save(inFile); }   // slot 10 (0x1002fd14)
#ifdef LLE_EXE_HEADERS
    // exe only (engine token stream unchanged): COMDAT 0xd16e (ret 4; in EECPASGUProphet.cpp's range): the bounds test
    // U2DRectangle<long>::Contains 0xd190 first, then slot 11 (EEAIArea/EETown/EESMArea callers).
    // (provisional name)
    bool BoundedContains(const U2DPoint<T>& inPoint)
    {
        bool theResult = this->mBounds.Contains(inPoint);
        if (theResult)
            theResult = this->Contains(inPoint);
        return theResult;
    }
    // exe only: the bounds (EETown::GetRandomPoint 0xac203 reads the hull's +0x14 rectangle) (provisional name)
    const U2DRectangle<T>& GetBounds() const { return this->mBounds; }
#endif
    virtual bool Contains(const U2DPoint<T>& inPoint)   // slot 11 (0x1002fd20)
    {
        long theCount = this->mPoints.size();
        bool theResult = theCount != 0;
        for (long i = 0; theResult && i < theCount; i++) {
            const U2DPoint<T>& theEnd = this->mPoints[(i + theCount - 1) % theCount];  // both addresses before the pushes
            const U2DPoint<T>& theStart = this->mPoints[i];
            theResult = inPoint.GetSide(theStart, theEnd) <= 0;
        }
        return theResult;
    }
    // slot 14 (0x1002f917): the tile rows the outline covers, row -> (first x, last x)
    virtual void GetScanLines(std::map<long, std::pair<long, long> >& outRows)
    {
#if defined(EE_LOSTDEBUG_H) && defined(LOWLEVELENGINE_EXPORTS)   // NOT original source (docs/LOST_DEBUG.md)
        // include-order dependent: active only where LostDebug.h is included BEFORE this header (GETerrainMesh.cpp,
        // line 255; bytes otherwise identical); everywhere else it compiles to nothing
#endif
        GE3DLine theEdge;
        U2DPoint<T> theStart;
        U2DPoint<T> theEnd;
        unsigned long theCount = this->GetPointCount();
        for (unsigned long i = 0; i < theCount; i++) {
            theStart = *this->GetPoint(i);
            theEnd = *this->GetPoint((i + 1) % theCount);
            theEdge.SetStart(GE3DPoint(theStart.GetX(), theStart.GetY(), 0.0f));
            theEdge.SetDirection(GEVector(theEnd.GetX() - theStart.GetX(), theEnd.GetY() - theStart.GetY(), 0.0f));
            this->AddScanLine(theEdge, outRows);
        }
    }
    // slot 15 (0x1002f9eb): walks the tiles under one edge
    virtual void AddScanLine(const GE3DLine& inEdge, std::map<long, std::pair<long, long> >& ioRows)
    {
        GE3DLine theCrossing(GE3DPoint(0.0f, 0.0f, 0.0f), GEVector(0.0f, 1.0f, 0.0f));
        GE3DPoint theEnd;
        std::map<long, std::pair<long, long> >::iterator theRow;
        long theStartX;
        long theStartY;
        long theEndX;
        long theEndY;
        long theStepX;
        long theStepY;
        long theX;
        long theY;
        long theDX;
        long theDY;
        float theOffset;
        float theT;
        float theEdgeT;
        if (inEdge.GetStart().GetX() > 0.0f)
            theStartX = (long)inEdge.GetStart().GetX();
        else
            theStartX = (long)inEdge.GetStart().GetX() - 1;
        if (inEdge.GetStart().GetY() > 0.0f)
            theStartY = (long)inEdge.GetStart().GetY();
        else
            theStartY = (long)inEdge.GetStart().GetY() - 1;
        inEdge.GetPointAt(1.0f, theEnd);
        if (theEnd.GetX() > 0.0f)
            theEndX = (long)theEnd.GetX();
        else
            theEndX = (long)theEnd.GetX() - 1;
        if (theEnd.GetY() > 0.0f)
            theEndY = (long)theEnd.GetY();
        else
            theEndY = (long)theEnd.GetY() - 1;
        theEndX -= theStartX;
        if (theEndX < 0)
            theEndX = -theEndX;
        theEndY -= theStartY;
        if (theEndY < 0)
            theEndY = -theEndY;
        if (inEdge.GetDirection().GetX() < 0.0f) {
            theStepX = -1;
            theOffset = 0.0f;
        } else {
            theStepX = 1;
            theOffset = 1.0f;
        }
        if (inEdge.GetDirection().GetY() < 0.0f)
            theStepY = -1;
        else
            theStepY = 1;
        theX = theStartX;
        theY = theStartY;
        for (;;) {
            theDX = theX - theStartX;
            if (theDX < 0)
                theDX = -theDX;
            theDY = theY - theStartY;
            if (theDY < 0)
                theDY = -theDY;
            if (theDX > theEndX || theDY > theEndY)
                break;
            theRow = ioRows.find(theY);
            if (theRow == ioRows.end())
                ioRows.insert(std::map<long, std::pair<long, long> >::value_type(theY, std::pair<long, long>(theX, theX)));
            else {
                (*theRow).second.first = min((*theRow).second.first, theX);
                (*theRow).second.second = max((*theRow).second.second, theX);
            }
            theCrossing.SetStart(GE3DPoint((float)theX + theOffset, (float)theY, 0.0f));
            if (inEdge.IntersectsIn2D(theCrossing, theT, theEdgeT) && theEdgeT >= -0.00001f && theEdgeT <= 1.0001f)
                theX += theStepX;
            else
                theY += theStepY;
        }
    }
    // slot 16 (0x1002fc16): sorts, removes duplicates and wraps the points
    virtual void Compute(bool inIsSorted)
    {
        U2DGrahamsAlgorithm<T> theAlgorithm;
        std::vector<U2DPoint<T> >::iterator theEnd;
        TMGetTime();
#if defined(EE_LOSTDEBUG_H) && (defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_EXE_HEADERS))   // NOT original source (docs/LOST_DEBUG.md)
        // include-order dependent: active only where LostDebug.h is included BEFORE this header (GETerrainMesh.cpp,
        // line 255; bytes otherwise identical); everywhere else it compiles to nothing
        // n; makes the out-of-line vector<GAPoint>() call (0x10030249) as the original
#endif
        if (!inIsSorted) {
            std::sort(this->mPoints.begin(), this->mPoints.end());
            theEnd = std::unique(this->mPoints.begin(), this->mPoints.end());
            if (theEnd != this->mPoints.end())
                this->mPoints.erase(theEnd, this->mPoints.end());
        }
        if (this->mPoints.size() >= 3) {
            theAlgorithm.Compute(this->mPoints, this->mPoints);
            TMGetTime();
        }
        this->ComputeCenter();
    }

protected:
#ifdef LLE_EXE_HEADERS
    float mArea;                            // +0x2c (exe: a float, Clear 0x412922 fldz/fstp)
#else
    T mArea;                                // +0x2c
#endif
};
#endif

#endif
