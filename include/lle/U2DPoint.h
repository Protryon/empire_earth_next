// Low-Level Engine U2DPoint header, as seen by its includers. Inferred: only what an includer's codegen
// proves. Requires lle/LowLevelEngine.h.
#ifndef LLE_U2DPOINT_H
#define LLE_U2DPOINT_H

// LLE exports use U2DPoint<long> (?$U2DPoint@J@@) and U2DPoint<float>; no member is exported (all inline).
// Default.dll EEClumps keeps a vector<U2DPoint<long> > (8-byte elements, x then y, trivial dtor: 0x8e6d).
template <class T> class U2DPoint {
public:
    U2DPoint() : mX(0), mY(0) {}           // zero-filled: GETerrainMesh.cpp static arrays 0x1002c1ba (and dword, 0),
                                            // AddDenormalizedPointToList 0x1002ab5d (U2DPoint<float>: fldz)
#ifdef LLE_EXE_HEADERS   // the exe's U2DPoint.h takes the coordinates by const reference (AddGraphSeries 0x2764e0
                         // loads the first argument first through both element addresses). The engine and
                         // Default.dll views keep by-value (each loses a row)
    U2DPoint(const T& inX, const T& inY) : mX(inX), mY(inY) {}
#else
    U2DPoint(T inX, T inY) : mX(inX), mY(inY) {}
#endif
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_EXE_HEADERS)
    // exe view too (owner decision, net +32 exact with 18 known regressions: the returned U2DPoint is then built in
    // place, U2DRectangle<T>::GetClosestPoint 0x80347/0x1f056a).
    // Inferred engine-only: GETerrainMesh.cpp's static U2DPoint<long>[2] arrays register an (empty) atexit dtor
    // (0x1002c1d0/0x1002c1dc), so the engine's header declares one; Default.dll's units match only without it
    // (775 -> 772 exact), i.e. the plug-in was built against a header without the dtor.
    ~U2DPoint() {}
#endif
#if defined(LOWLEVELENGINE_EXPORTS) || defined(LLE_EXE_HEADERS)
    // Inferred engine + exe: user-declared copy ctor and operator=. The engine copies U2DPoint elements and by-value
    // arguments field by field through references (GETerrainMesh's sort/heap COMDATs 0x100311c5.., _Adjust_heap
    // 0x1003252e; the out-of-line operator= is 0x1002fe8b, ICF-folded with GE2DPoint's), while Default.dll copies
    // U2DPoint<float> with rep movsd (0x131bd): the plug-ins were probably built against an older U2DPoint.h.
    // The exe calls an out-of-line copy ctor 0x4d28b (479 callers); with the pair in the exe view allscores gained
    // 3 exact, 0 lost (EEAIArea 0xda77, EEUGLoadGarrison 0x226ba4, EEUGUnloadTransport 0x228883).
    // (The engine's empty dtor is in the exe view too: EEAIArea 0xe98e is one of its known regressions.)
    U2DPoint(const U2DPoint& inPoint) : mX(inPoint.mX), mY(inPoint.mY) {}
    U2DPoint& operator=(const U2DPoint& inPoint) { this->mX = inPoint.mX; this->mY = inPoint.mY; return *this; }
#endif
    // Engine template members (instantiated by GETerrainMesh.cpp's U2DPolygon<float> code). (provisional names)
    bool operator<(const U2DPoint& inPoint) const   // 0x10031459: lower y first, then larger x (through the accessors:
                                                    // refused inline at direct sites, fld;fld;fcompp loads)
        { return GetY() < inPoint.GetY() || (GetY() == inPoint.GetY() && GetX() > inPoint.GetX()); }
    bool operator==(const U2DPoint& inPoint) const { return this->mX == inPoint.mX && this->mY == inPoint.mY; }
    bool operator!=(const U2DPoint& inPoint) const { return this->mX != inPoint.mX || this->mY != inPoint.mY; }
    T GetX() const { return this->mX; }
    T GetY() const { return this->mY; }
    void SetX(T inX) { this->mX = inX; }
    void SetY(T inY) { this->mY = inY; }
    void SetXY(T inX, T inY) { this->mX = inX; this->mY = inY; }  // exe: EEUGUnloadTransport's (bool) ctor 0x23c685 (as GE2DPoint's)
    // exe (out-of-line COMDAT 0xe964, an AI unit's): the squared distance as a float (EEUGUnloadTransport 0x228683)
    // (provisional name)
    // Inline with the doubled differences: 0xe964 exact, and the float copy is ICF-identical to
    // DistanceToSquared<float> 0x1f0eb5; its inline cost puts GetClosestCorner's budget where the original's is
#ifdef LLE_EXE_HEADERS
    // exe: the squared distance (out-of-line COMDAT ICF-identical to EETileDistanceLess::DistanceSquared 0x8a3f; the no-/Og
    // header sort functor 0x408fd7 calls it on GetTile()'s result). Inline: VC6's /Os post-check undoes it at every
    // site; at a no-/Og site its locals and the getter temps keep 4 caller frame slots (r37_load family note)
    T DistanceSquared(const U2DPoint& inPoint) const
    {
        T theDY = this->mY - inPoint.GetY();
        T theDX = this->mX - inPoint.GetX();
        theDX *= theDX;
        theDY *= theDY;
        return theDX + theDY;
    }
    float DistanceToSquaredF(const U2DPoint& inPoint) const
    {
        return (float)((this->mX - inPoint.GetX()) * (this->mX - inPoint.GetX())) +
               (this->mY - inPoint.GetY()) * (this->mY - inPoint.GetY());
    }
#else
    float DistanceToSquaredF(const U2DPoint& inPoint) const;
#endif
#ifdef LLE_EXE_HEADERS
    // exe (out-of-line COMDAT 0xe92e, an AI unit's): the distance as a float (sqrt of the squared distance; EECPIArmy's
    // ProjectOnSegment 0x80730) (provisional name)
    float DistanceToF(const U2DPoint& inPoint) const;
    // exe view only (COMDAT 0x80730, EECPIArmy's range; defined below the class): the point inDistance of the way
    // (inDot times this point's distance from inStart) along inStart..inEnd; the offset vector is normalized but
    // unused. GEVector must be complete where it is instantiated. (provisional name)
    U2DPoint ProjectOnSegment(float inDot, const U2DPoint& inStart, const U2DPoint& inEnd) const;
    // exe view only (COMDAT 0x805ff, EECPIArmy's range; defined below the class): the point of inStart..inEnd
    // closest to this one (the projection when the angle at inStart is not obtuse, else the nearer end) (provisional
    // name)
    // name
    U2DPoint GetClosestPointOnSegment(const U2DPoint& inStart, const U2DPoint& inEnd) const;
    // exe view only: the heading from this point to inPoint (thiscall COMDAT 0x1efd40, first expanded out of line by
    // EEActiveGroup::Fn1EBF85 for the unload goal's destination) (provisional name)
    float GetHeadingTo(const U2DPoint& inPoint) const
        { float theY = (float)inPoint.mY - this->mY; float theX = -((float)inPoint.mX - this->mX); return UMArcTan2(theX, theY); }  // Y first (fild/fisub [+4]; 0x5efd40)
    // exe view only (owner decision 2): the tiles a line from this tile to inTo crosses, one step of
    // inStep tiles at a time, inTo last (thiscall COMDAT 0xf40d8, EEE3DMarchingLine.cpp; also EEUGGather 0x222ff0).
    // GE3DPoint/GE3DLine/GEVector must be complete where it is instantiated. (provisional name)
    void GetLineTiles(const U2DPoint& inTo, float inStep, std::vector<U2DPoint>& outTiles) const
    {
        GE3DPoint theStart((float)this->mX, (float)this->mY, 0.0f);
        GE3DPoint theEnd((float)inTo.mX, (float)inTo.mY, 0.0f);
        GE3DPoint thePoint;
        long theLastX = 0;
        long theLastY = 0;
        U2DPoint theTile;
        GE3DLine theLine;
        GEVector theDirection;
        float theDistance = 0.0f;
        float theLength;
        outTiles.clear();
        if (!(this->mX == inTo.mX && this->mY == inTo.mY)) {
            theDirection.SetX((float)inTo.mX - (float)this->mX);
            theDirection.SetY((float)inTo.mY - (float)this->mY);
            theDirection.Normalize();
            theDirection.SetX(theDirection.GetX() * inStep);
            theDirection.SetY(theDirection.GetY() * inStep);
            theLine.SetStart(theStart);
            theLine.SetDirection(theDirection);
            if (!theLine.SolveForX(theEnd.GetX(), theLength))
                theLine.SolveForY(theEnd.GetY(), theLength);
            while (theDistance < theLength) {
                theLine.GetPointAt(theDistance, thePoint);
                theTile.mX = (long)thePoint.GetX();
                theTile.mY = (long)thePoint.GetY();
                if (theLastX != theTile.mX || theLastY != theTile.mY || outTiles.size() == 0)
                    outTiles.push_back(theTile);
                theDistance += inStep;
                theLastX = theTile.mX;
                theLastY = theTile.mY;
            }
        }
        outTiles.push_back(inTo);
    }
#endif
#ifdef LLE_EXE_HEADERS
    // exe: the squares are summed in the FPU (fild dx2; fiadd dy2; EECPResourceCenter COMDAT 0x48d421), not as T
    T DistanceTo(const U2DPoint& inPoint) const
    {
        T theDY = this->mY - inPoint.GetY();
        T theDX = this->mX - inPoint.GetX();
        theDX *= theDX;
        theDY *= theDY;
        return (T)sqrt((float)theDX + theDY);
    }
#else
    T DistanceTo(const U2DPoint& inPoint) const
    {
        T theDistance = (this->mX - inPoint.GetX()) * (this->mX - inPoint.GetX()) + (this->mY - inPoint.GetY()) * (this->mY - inPoint.GetY());
        return (T)sqrt(theDistance);
    }
#endif
    // each difference written out twice (a squaring macro's shape): the one form that gives both the float copy
    // (GETerrainMesh.cpp 0x1002fe6c: y difference first, the squares from st0/st2) and the long copy
    // (U2DSparseArray.cpp 0x10055899: x and y kept in eax/edx, inPoint in esi)
    T DistanceToSquared(const U2DPoint& inPoint) const
        { return (this->mX - inPoint.GetX()) * (this->mX - inPoint.GetX()) + (this->mY - inPoint.GetY()) * (this->mY - inPoint.GetY()); }
    long Load(FSFile& inFile)
    {
        long theResult;
        if (inFile.GetIsOpen()) {
            theResult = inFile.Read(this->mX);
            if (theResult == 0)
                theResult = inFile.Read(this->mY);
        } else
            theResult = 4;
        return theResult;
    }
    long Save(FSFile& inFile) const
    {
        long theResult;
        if (inFile.GetIsOpen()) {
            theResult = inFile.Write(this->mX);
            if (theResult == 0)
                theResult = inFile.Write(this->mY);
        } else
            theResult = 4;
        return theResult;
    }

    T mX;                                   // +0x00
    T mY;                                   // +0x04
};

#ifdef LLE_EXE_HEADERS
// exe: out of line (COMDAT 0xe92e; never expanded by its 4 callers). Spelled out: through DistanceToSquaredF its
// callers expand it
template <class T>
float U2DPoint<T>::DistanceToF(const U2DPoint& inPoint) const
{
    return (float)sqrt((float)((this->mX - inPoint.mX) * (this->mX - inPoint.mX)) + (this->mY - inPoint.mY) * (this->mY - inPoint.mY));
}
// exe view only: out of line (never expanded; COMDATs 0x80730 / 0x805ff, EECPIArmy's range)
template <class T>
U2DPoint<T> U2DPoint<T>::ProjectOnSegment(float inDot, const U2DPoint& inStart, const U2DPoint& inEnd) const
{
    GEVector theEdge;
    GEVector theOffset;
    U2DPoint<T> theResult = *this;
    if (inStart != inEnd) {
        theOffset.SetX((float)this->mX - (float)inStart.mX);
        theOffset.SetY((float)this->mY - (float)inStart.mY);
        theOffset.Normalize();
        theEdge.SetX((float)inEnd.mX - (float)inStart.mX);
        theEdge.SetY((float)inEnd.mY - (float)inStart.mY);
        theEdge.Normalize();
        float theLength = inStart.DistanceToF(theResult) * inDot;
        theResult.mX = (T)(theEdge.GetX() * theLength + inStart.mX);
        theResult.mY = (T)(theEdge.GetY() * theLength + inStart.mY);
    }
    return theResult;
}
template <class T>
U2DPoint<T> U2DPoint<T>::GetClosestPointOnSegment(const U2DPoint& inStart, const U2DPoint& inEnd) const
{
    GEVector theEdge;
    GEVector theOffset;
    T theX = this->mX;
    T theY = this->mY;
    U2DPoint<T> theResult(theX, theY);
    if (!(*this == inStart || *this == inEnd)) {
        if (inStart == inEnd)
            theResult = inStart;
        else {
            theOffset.SetX((float)theX - (float)inStart.mX);
            theOffset.SetY((float)theY - (float)inStart.mY);
            theOffset.Normalize();
            theEdge.SetX((float)inEnd.mX - (float)inStart.mX);
            theEdge.SetY((float)inEnd.mY - (float)inStart.mY);
            theEdge.Normalize();
            float theDot = theOffset.DotProduct(theEdge);
            if (theDot >= 0.0f)
                theResult = this->ProjectOnSegment(theDot, inStart, inEnd);
            else
                theResult = this->DistanceToSquaredF(inStart) < this->DistanceToSquaredF(inEnd) ? inStart : inEnd;
        }
    }
    return theResult;
}
#endif

// LLE 0x10055900 (template COMDAT of U2DSparseArray.cpp; provisional name): the mean of the points, optionally moved
// to the closest point of the list when the mean itself is not one of them.
template <class T> U2DPoint<T> U2DComputeCenterPoint(const std::vector<U2DPoint<T> >& inPoints, bool inMustBeInList)
{
    float theX = 0.0f;
    float theY = 0.0f;
    U2DPoint<T> theCenter;
    U2DPoint<T> theClosest;
    long theCount = inPoints.size();
    if (inPoints.size() > 0) {   // size() > 0: the original expands this size() and calls back() out of line
        long i;
        for (i = theCount - 1; i >= 0; i--) {
            theX += inPoints[i].GetX() / (float)theCount;
            theY += inPoints[i].GetY() / (float)theCount;
        }
        theCenter = U2DPoint<T>((T)theX, (T)theY);
        if (inMustBeInList && std::find(inPoints.begin(), inPoints.end(), theCenter) == inPoints.end()) {
            theClosest = inPoints.back();
            T theBest = theCenter.DistanceToSquared(theClosest);
            for (i = inPoints.size() - 2; i >= 0; i--) {
                T theDistance = theCenter.DistanceToSquared(inPoints[i]);
                if (theDistance <= theBest) {
                    theClosest = inPoints[i];
                    theBest = theDistance;
                }
            }
            theCenter = theClosest;
        }
    }
    return theCenter;
}

#endif
