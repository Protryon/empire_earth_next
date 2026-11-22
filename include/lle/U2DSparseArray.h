// C:\EEX\Low-Level Engine\Utilities\U2DSparseArray.h (RECONSTRUCTION). Requires lle/LowLevelEngine.h.
// A set of integer points stored as one sorted run list per x column (U2DSparseArray.cpp, 0x100549b6..0x10056369).
//
// Port: the points live in Rust (ee_base::sparse, rust/base/sparse.h; docs/RUST_PLAN.md R5b.1): the class keeps its
// name, vtable and members' API and owns a handle to them; the run node class, the column list header and their node
// pool are gone (nothing outside the class used them). The members are inline forwarders to the handle
// (U2DSparseArray.cpp is gone); rust/base/sparse.h names nothing else, so the 60 units that include this header do not
// see the AI's A* interface.
#ifndef LLE_U2DSPARSEARRAY_H
#define LLE_U2DSPARSEARRAY_H

#include <stdint.h>
#include <vector>

#include "lle/U2DPoint.h"
#include "rust/base/sparse.h"

class LLE_API U2DSparseArrayPointContainer {   // was sizeof 0x1c (vptr, the column array, offset, count)
public:
    U2DSparseArrayPointContainer() : mPoints(ee_sparse_new()) {}
    // The original shared the column pointers with the source (and freed them twice when both died); the copy owns
    // its own points
    U2DSparseArrayPointContainer(const U2DSparseArrayPointContainer& inContainer)
        : mPoints(ee_sparse_clone(inContainer.mPoints))
    {
    }
    // no vtable (port): the AI's areas are Rust objects that hold the handle where this class is (ee_sim::astar
    // objects.rs)
    ~U2DSparseArrayPointContainer() { ee_sparse_free(this->mPoints); }
    // exported 0xd5a1 (ICF-folded `mov eax, ecx; ret 4`): copies nothing
    U2DSparseArrayPointContainer& operator=(const U2DSparseArrayPointContainer& inContainer) { return *this; }

    // Rebuilds the list from every column's points (every inStep-th y), last column first
    void BuildPointList(std::vector<U2DPoint<long> >& outList, long inStep) { List(false, inStep, outList); }
    // Deletes every column and empties the set
    void Clear() { ee_sparse_clear(this->mPoints); }
    // Returns whether the set holds the point
    bool Contains(const U2DPoint<long>& inPoint) const
    {
        return ee_sparse_contains(this->mPoints, (int32_t)inPoint.mX, (int32_t)inPoint.mY);
    }
    // Rebuilds the set from a point list, sizing the column array from the list's x range
    void Create(const std::vector<U2DPoint<long> >& inPoints) { InsertList(inPoints, true); }
    // Returns the point itself if it is in the set, else the nearest point of the set (squared distance)
    U2DPoint<long> GetClosestPoint(const U2DPoint<long>& inPoint)
    {
        return Point(ee_sparse_closest_point(this->mPoints, (int32_t)inPoint.mX, (int32_t)inPoint.mY));
    }
    // Rebuilds the list from the lowest and highest point of each column
    void GetOnlyExtremeEdgePoints(std::vector<U2DPoint<long> >& outList) { List(true, 1, outList); }
    // Returns a random point (16 tries at a random column; then the first point; default point when empty)
    U2DPoint<long> GetRandomPoint(URandom& inRandom)
    {
        return Point(ee_sparse_random_point(this->mPoints, Generate, &inRandom));
    }
    // Adds a point, growing the column array; a point left of the array with x <= 0 is silently ignored
    void Insert(const U2DPoint<long>& inPoint)
    {
        ee_sparse_insert(this->mPoints, (int32_t)inPoint.mX, (int32_t)inPoint.mY);
    }
    // Adds every point of the list (from the last)
    void Insert(const std::vector<U2DPoint<long> >& inPoints) { InsertList(inPoints, false); }
    // Removes a point, deleting its column when it becomes empty; false when it was not there
    bool Remove(const U2DPoint<long>& inPoint)
    {
        return ee_sparse_remove(this->mPoints, (int32_t)inPoint.mX, (int32_t)inPoint.mY);
    }
    // Returns the point count
    long Size() const { return ee_sparse_size(this->mPoints); }
    // Returns the point at the given index over all columns in order (default point when out of range)
    U2DPoint<long> operator[](long inIndex) const { return Point(ee_sparse_get(this->mPoints, (int32_t)inIndex)); }
    // port (save version 0.263, the continent manager's regions in a saved game): the set as it is (empty columns
    // too: GetRandomPoint draws a column), as numbers: the offset, the count, the column count, then per column 0 (none)
    // or 1, its size, its run count and its runs' ends (ee_sparse_raw); SetRaw makes the set from them (false, the set
    // cleared, when they are not such a list)
    void GetRaw(std::vector<long>& outRaw) const
    {
        size_t theCount = 0;
        const int32_t* theRaw = ee_sparse_raw(this->mPoints, &theCount);
        outRaw.assign(theRaw, theRaw + theCount);
    }
    bool SetRaw(const std::vector<long>& inRaw)
    {
        std::vector<int32_t> theRaw(inRaw.begin(), inRaw.end());
        return ee_sparse_set_raw(this->mPoints, theRaw.empty() ? NULL : &theRaw[0], theRaw.size());
    }
    EeSparseArray* GetHandle() const { return this->mPoints; }   // New (port): for the AI area's record (ee_astar area.rs)
    // New (port): where the handle is (ee_sim::astar cm_io.rs reads an area's tiles from its object)
    EeSparseArray* const* GetHandleSlot() const { return &this->mPoints; }

private:
    static U2DPoint<long> Point(EePoint inPoint) { return U2DPoint<long>(inPoint.x, inPoint.y); }
    static float Generate(void* inRandom) { return static_cast<URandom*>(inRandom)->Generate(); }
    // The handle's point list (BuildPointList / GetOnlyExtremeEdgePoints) into outList, cleared first
    void List(bool inExtremes, long inStep, std::vector<U2DPoint<long> >& outList)
    {
        size_t theCount = 0;
        const EePoint* thePoints = ee_sparse_points(this->mPoints, inExtremes, (int32_t)inStep, &theCount);
        outList.clear();
        outList.reserve(theCount);
        for (size_t i = 0; i < theCount; i++)
            outList.push_back(Point(thePoints[i]));
    }
    // Copies a point list into the handle's (int32) points: Create (inCreate) or Insert
    void InsertList(const std::vector<U2DPoint<long> >& inPoints, bool inCreate)
    {
        std::vector<EePoint> thePoints(inPoints.size());
        for (size_t i = 0; i < inPoints.size(); i++) {
            thePoints[i].x = (int32_t)inPoints[i].mX;
            thePoints[i].y = (int32_t)inPoints[i].mY;
        }
        ee_sparse_insert_list(this->mPoints, thePoints.empty() ? NULL : &thePoints[0], thePoints.size(), inCreate);
    }

    EeSparseArray* mPoints;                 // the points (ee_sparse_new)
};
static_assert(sizeof(EePoint) == 8, "EePoint");

#endif
