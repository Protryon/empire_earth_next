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
    // Returns whether the set holds the point
    bool Contains(const U2DPoint<long>& inPoint) const { return ee_sparse_contains(this->mPoints, (int32_t)inPoint.mX, (int32_t)inPoint.mY); }
    // Returns the point itself if it is in the set, else the nearest point of the set (squared distance)
    U2DPoint<long> GetClosestPoint(const U2DPoint<long>& inPoint)
    {
        return Point(ee_sparse_closest_point(this->mPoints, (int32_t)inPoint.mX, (int32_t)inPoint.mY));
    }
    // Returns the point count
    long Size() const { return ee_sparse_size(this->mPoints); }
    // Returns the point at the given index over all columns in order (default point when out of range)
    U2DPoint<long> operator[](long inIndex) const { return Point(ee_sparse_get(this->mPoints, (int32_t)inIndex)); }
    // New (port): where the handle is (ee_sim::astar cm_io.rs reads an area's tiles from its object)
    EeSparseArray* const* GetHandleSlot() const { return &this->mPoints; }

    static U2DPoint<long> Point(EePoint inPoint) { return U2DPoint<long>(inPoint.x, inPoint.y); }
    // The handle's point list (BuildPointList / GetOnlyExtremeEdgePoints) into outList, cleared first
    void List(bool inExtremes, long inStep, std::vector<U2DPoint<long> >& outList)
        { ee_sparse_points_into(this->mPoints, inExtremes, (int32_t)inStep, &outList, Resize<U2DPoint<long> >); }
    // a vector Rust fills (ee_sparse_points_into): resized, its data as longs
    template <class T> static long* Resize(void* inVector, size_t inCount)
        { return reinterpret_cast<long*>((static_cast<std::vector<T>*>(inVector)->resize(inCount), static_cast<std::vector<T>*>(inVector)->data())); }
    EeSparseArray* mPoints;                 // the points (ee_sparse_new)
};
static_assert(sizeof(EePoint) == 8, "EePoint");
static_assert(sizeof(U2DPoint<long>) == 2 * sizeof(long), "a U2DPoint<long> is two longs (ee_sparse_points_into)");

#endif
