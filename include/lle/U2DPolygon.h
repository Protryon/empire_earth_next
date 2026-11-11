// Low-Level Engine U2DPolygon header, as seen by its includers (inferred header). Requires lle/LowLevelEngine.h.
#ifndef LLE_U2DPOLYGON_H
#define LLE_U2DPOLYGON_H

#include "lle/U2DRectangle.h"

#ifndef LLE_EXE_HEADERS   // the exe has one U2DPolygon-PointCount key chain (0x1160, its first unit), not one per
                          // includer (EECPILocation.cpp has only its own chain)
const UString kU2DPolygonPointCountKey("U2DPolygon-PointCount");
#endif

// Engine templates in the original (GETerrainMesh.cpp instantiated U2DPolygon<float>/U2DConvexHull<float>: vtables
// 0x10098010, 0x1009804c; the exe U2DPolygon<long>/U2DConvexHull<long>: EEAIArea::mHull, U2DPolygon<long> ctor
// 0x11251, Contains 0xd16e). Slot order from the vtables; member names are provisional (no export names them).
//
// Port: the float instantiation is gone (the terrain volume's hull is ee_ge hull.rs / terrain_volume.rs), so the
// templates are the exe's only, for T = long: the class keeps its members and its virtual slots, and its algorithms
// are Rust (ee_astar hull.rs over ee_ge hull.rs; docs/RUST_PLAN.md R5b.1, branch rust-r5b-cont3). Deleted with the
// move: U2DGrahamsAlgorithm, U2DPointLess and the engine's std::less<U2DPoint<float> >, the polygon's crossing-number
// Contains (never called: its only subclass overrides it; the slot is pure now), and the hull's AddScanLine and
// Compute slots (only SetPoints and GetScanLines called them).
#ifdef LLE_EXE_HEADERS
#include <map>
#include <vector>
#include "rust/ee_rust.h"

// New (port), TEST ONLY (EE_SIM_CAPTURE, tools/sim_capture.py; defined in EEAIContinentCapture.cpp): the records of
// the hulls' geometry (HULL: kind 1 SetPoints, 2 GetClosestOutlinePoint, 3 GetClosestPoints, 4 GetClosestPoint,
// 6 Contains, 7 GetScanLines) and of the areas' (AREA, ee/EEAIArea.h). Only the outermost captured call of a thread
// is recorded: a scope made while another is alive is not outer, so the calls an algorithm makes inside another (and
// that leave the C++ with it) never reach the digests.
enum {
    kGeomCaptureHull = 0x4c4c5548,          // "HULL"
    kGeomCaptureArea = 0x41455241,          // "AREA"
};
class EEGeomCaptureScope {
public:
    EEGeomCaptureScope();                   // mOuter: the capture is on and no other scope is alive on this thread
    ~EEGeomCaptureScope();
    bool mOuter;
};
void EEGeomCaptureBegin(unsigned int inTag, unsigned int inKind);
void EEGeomCaptureValue(long inValue);
void EEGeomCaptureFloat(float inValue);
void EEGeomCapturePoint(const U2DPoint<long>& inPoint);
// inWhole: the count and every point; else the count and an FNV-1a hash of the points (a hull's identity)
void EEGeomCapturePoints(const std::vector<U2DPoint<long> >& inPoints, bool inWhole);
void EEGeomCaptureRows(const std::map<long, std::pair<long, long> >& inRows);
void EEGeomCaptureCommit();

// New (port): the seam's point type is U2DPoint<long>'s layout, so a point vector passes in place
static_assert(sizeof(U2DPoint<long>) == sizeof(EeHullPoint), "EeHullPoint");
inline const EeHullPoint* U2DHullPoints(const std::vector<U2DPoint<long> >& inPoints)
{
    return inPoints.empty() ? NULL : reinterpret_cast<const EeHullPoint*>(&inPoints[0]);
}
inline EeHullPoint U2DHullPointOf(const U2DPoint<long>& inPoint)
{
    EeHullPoint thePoint;
    thePoint.x = inPoint.mX;
    thePoint.y = inPoint.mY;
    return thePoint;
}
inline U2DPoint<long> U2DHullPoint(EeHullPoint inPoint)
{
    return U2DPoint<long>(inPoint.x, inPoint.y);
}

template <class T> class U2DPolygon {      // sizeof 0x2c
public:
    U2DPolygon() {}                         // 0x1002dc4a (out of line)
    virtual ~U2DPolygon() {}                // ??_G 0x1002d9c1
    virtual float ComputeArea() = 0;        // slot 1 (exe: a float, U2DConvexHull<long> 0x41214b returns it with fld)
    virtual void SetPoints(const std::vector<U2DPoint<T> >& inPoints, bool inIsSorted) = 0;   // slot 2
    virtual void Validate()                 // slot 3 (0x1002e82f): the checks are compiled out
    {
    }
    // slot 4 (exe U2DPolygon<long> 0x11343): inPoint moved to inScale times its distance from inStart along
    // inStart..inEnd (Rust: hull.rs project_point)
    virtual U2DPoint<T> ProjectPoint(const U2DPoint<T>& inPoint, float inScale, const U2DPoint<T>& inStart,
                                     const U2DPoint<T>& inEnd)
    {
        return U2DHullPoint(ee_hull_project_point(U2DHullPointOf(inPoint), inScale, U2DHullPointOf(inStart),
                                                  U2DHullPointOf(inEnd)));
    }
    // slot 5 (exe 0x411421): the point of the outline nearest inPoint (Rust: closest_outline_point)
    virtual U2DPoint<T> GetClosestOutlinePoint(const U2DPoint<T>& inPoint)
    {
        EEGeomCaptureScope theScope;
        U2DPoint<T> theResult = U2DHullPoint(
            ee_hull_closest_outline_point(U2DHullPoints(this->mPoints), this->mPoints.size(), U2DHullPointOf(inPoint)));
        if (theScope.mOuter) {   // TEST ONLY: HULL 2
            EEGeomCaptureBegin(kGeomCaptureHull, 2);
            EEGeomCapturePoints(this->mPoints, false);
            EEGeomCapturePoint(inPoint);
            EEGeomCapturePoint(theResult);
            EEGeomCaptureCommit();
        }
        return theResult;
    }
    // slot 6 (0x1002ef29): the closest pair of points between this outline and inPolygon's (Rust: closest_points)
    virtual void GetClosestPoints(U2DPolygon& inPolygon, U2DPoint<T>& outPoint, U2DPoint<T>& outOtherPoint)
    {
        EEGeomCaptureScope theScope;
        EeHullPoint thePoint;
        EeHullPoint theOtherPoint;
        ee_hull_closest_points(U2DHullPoints(this->mPoints), this->mPoints.size(), U2DHullPoints(inPolygon.mPoints),
                               inPolygon.mPoints.size(), &thePoint, &theOtherPoint);
        outPoint = U2DHullPoint(thePoint);
        outOtherPoint = U2DHullPoint(theOtherPoint);
        if (theScope.mOuter) {   // TEST ONLY: HULL 3
            EEGeomCaptureBegin(kGeomCaptureHull, 3);
            EEGeomCapturePoints(this->mPoints, false);
            EEGeomCapturePoints(inPolygon.mPoints, false);
            EEGeomCapturePoint(outPoint);
            EEGeomCapturePoint(outOtherPoint);
            EEGeomCaptureCommit();
        }
    }
    // slot 7 (0x1002f1bd): the vertex nearest inPoint, (0, 0) without points (Rust: closest_point)
    virtual U2DPoint<T> GetClosestPoint(const U2DPoint<T>& inPoint)
    {
        EEGeomCaptureScope theScope;
        U2DPoint<T> theResult = U2DHullPoint(
            ee_hull_closest_point(U2DHullPoints(this->mPoints), this->mPoints.size(), U2DHullPointOf(inPoint)));
        if (theScope.mOuter) {   // TEST ONLY: HULL 4
            EEGeomCaptureBegin(kGeomCaptureHull, 4);
            EEGeomCapturePoints(this->mPoints, false);
            EEGeomCapturePoint(inPoint);
            EEGeomCapturePoint(theResult);
            EEGeomCaptureCommit();
        }
        return theResult;
    }
    // slot 8 (0x1002f28d): the index of the vertex nearest inPoint, -1 without points (Rust: closest_point_index)
    virtual long GetClosestPointIndex(const U2DPoint<T>& inPoint)
    {
        return ee_hull_closest_point_index(U2DHullPoints(this->mPoints), this->mPoints.size(), U2DHullPointOf(inPoint));
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
    // slot 11: the containment test (the polygon's crossing-number test was never called: pure now)
    virtual bool Contains(const U2DPoint<T>& inPoint) = 0;
    // slot 12 (exe 0x41209a): the float mean of the points, truncated once (Rust: center)
    virtual void ComputeCenter()
    {
        this->mCenter = U2DHullPoint(ee_hull_center(U2DHullPoints(this->mPoints), this->mPoints.size()));
    }
    // slot 13 (0x1002f649): sorted (duplicates kept) and Graham's scan (Rust: sort)
    virtual void Sort()
    {
        if (!this->mPoints.empty())
            this->mPoints.resize(ee_hull_sort(reinterpret_cast<EeHullPoint*>(&this->mPoints[0]), this->mPoints.size()));
    }
    U2DPoint<T>* GetPoint(long inIndex) { return &this->mPoints[inIndex]; }  // inline (0x1002feb7 is vector::operator[])
    unsigned long GetPointCount() { return this->mPoints.size(); }      // 0x1002ffe4
    const U2DPoint<T>& GetCenter() const { return this->mCenter; }  // exe (EECPDefenseCommander reads a town's) (provisional name)
    const std::vector<U2DPoint<T> >& GetPoints() const { return this->mPoints; }   // New (port)

protected:
    std::vector<U2DPoint<T> > mPoints;      // +0x04
    U2DRectangle<T> mBounds;                // +0x14
    U2DPoint<T> mCenter;                    // +0x24
};

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
    // slot 1 (exe U2DConvexHull<long> 0x41214b): the float area (Rust: area)
    virtual float ComputeArea()
    {
        this->mArea = ee_hull_area(U2DHullPoints(this->mPoints), this->mPoints.size());
        return this->mArea;
    }
    // slot 2 (0x1002f795): the input's bounds, the hull (sorted and made unique unless inIsSorted, then Graham's
    // scan), its center and its area (Rust: set_points)
    virtual void SetPoints(const std::vector<U2DPoint<T> >& inPoints, bool inIsSorted)
    {
        EEGeomCaptureScope theScope;
        EeHullShape theShape;
        this->Clear();
        this->mPoints.resize(inPoints.size());
        if (!inPoints.empty()) {
            this->mPoints.resize(ee_hull_set_points(U2DHullPoints(inPoints), inPoints.size(), inIsSorted,
                                                    reinterpret_cast<EeHullPoint*>(&this->mPoints[0]), &theShape));
            this->mBounds.mBottom = theShape.bottom;
            this->mBounds.mLeft = theShape.left;
            this->mBounds.mRight = theShape.right;
            this->mBounds.mTop = theShape.top;
            this->mCenter = U2DHullPoint(theShape.center);
            this->mArea = theShape.area;
        }
        if (theScope.mOuter) {   // TEST ONLY: HULL 1, the input and the whole hull
            EEGeomCaptureBegin(kGeomCaptureHull, 1);
            EEGeomCaptureValue(inIsSorted);
            EEGeomCapturePoints(inPoints, true);
            EEGeomCapturePoints(this->mPoints, true);
            EEGeomCaptureValue(this->mBounds.mBottom);
            EEGeomCaptureValue(this->mBounds.mLeft);
            EEGeomCaptureValue(this->mBounds.mRight);
            EEGeomCaptureValue(this->mBounds.mTop);
            EEGeomCapturePoint(this->mCenter);
            EEGeomCaptureFloat(this->mArea);
            EEGeomCaptureCommit();
        }
    }
    virtual long Load(FSFile& inFile) { return U2DPolygon<T>::Load(inFile); }   // slot 9 (0x1002fc0a)
    virtual long Save(FSFile& inFile) { return U2DPolygon<T>::Save(inFile); }   // slot 10 (0x1002fd14)
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
    // slot 11 (0x1002fd20): inPoint on no outer side of an edge (Rust: contains)
    virtual bool Contains(const U2DPoint<T>& inPoint)
    {
        EEGeomCaptureScope theScope;
        bool theResult = ee_hull_contains(U2DHullPoints(this->mPoints), this->mPoints.size(), U2DHullPointOf(inPoint));
        if (theScope.mOuter) {   // TEST ONLY: HULL 6
            EEGeomCaptureBegin(kGeomCaptureHull, 6);
            EEGeomCapturePoints(this->mPoints, false);
            EEGeomCapturePoint(inPoint);
            EEGeomCaptureValue(theResult);
            EEGeomCaptureCommit();
        }
        return theResult;
    }
    // slot 14 (0x1002f917): the tile rows the outline covers, row -> (first x, last x), merged into outRows (Rust:
    // scan_lines)
    virtual void GetScanLines(std::map<long, std::pair<long, long> >& outRows)
    {
        ee_hull_scan_lines(U2DHullPoints(this->mPoints), this->mPoints.size(), &outRows, U2DConvexHull::AddRow);
        if (EEGeomCaptureScope().mOuter) {   // TEST ONLY: HULL 7, the rows after the call
            EEGeomCaptureBegin(kGeomCaptureHull, 7);
            EEGeomCapturePoints(this->mPoints, false);
            EEGeomCaptureRows(outRows);
            EEGeomCaptureCommit();
        }
    }

protected:
    // GetScanLines' rows: a new row, or the row widened (the original's AddScanLine merge)
    static void AddRow(void* inRows, long inY, long inFirst, long inLast)
    {
        std::map<long, std::pair<long, long> >& theRows = *static_cast<std::map<long, std::pair<long, long> >*>(inRows);
        std::map<long, std::pair<long, long> >::iterator theRow = theRows.find(inY);
        if (theRow == theRows.end())
            theRows.insert(std::map<long, std::pair<long, long> >::value_type(inY, std::pair<long, long>(inFirst, inLast)));
        else {
            (*theRow).second.first = min((*theRow).second.first, inFirst);
            (*theRow).second.second = max((*theRow).second.second, inLast);
        }
    }
    float mArea;                            // +0x2c (exe: a float, Clear 0x412922 fldz/fstp)
};
#endif

#endif
