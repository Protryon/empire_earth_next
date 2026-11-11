// C:\EEX\Low-Level Engine\Utilities\U2DSparseArray.h (RECONSTRUCTION). Requires lle/LowLevelEngine.h, lle/U2DPoint.h.
// A set of integer points stored as one sorted run list per x column (U2DSparseArray.cpp, 0x100549b6..0x10056369).
//
// Port: the points live in Rust (ee_astar sparse.rs, docs/RUST_PLAN.md R5b.1): the class keeps its name, vtable and
// members' API and owns a handle to them; the run node class, the column list header and their node pool are gone
// (nothing outside the class used them). U2DSparseArray.cpp forwards to the handle.
#ifndef LLE_U2DSPARSEARRAY_H
#define LLE_U2DSPARSEARRAY_H

#include <vector>

struct EeSparseArray;                       // rust/ee_rust.h (ee_astar)
class URandom;

class LLE_API U2DSparseArrayPointContainer {   // was sizeof 0x1c (vptr, the column array, offset, count)
public:
    U2DSparseArrayPointContainer();
    // The original shared the column pointers with the source (and freed them twice when both died); the copy owns
    // its own points
    U2DSparseArrayPointContainer(const U2DSparseArrayPointContainer& inContainer);
    virtual ~U2DSparseArrayPointContainer();
    // exported 0xd5a1 (ICF-folded `mov eax, ecx; ret 4`): copies nothing
    U2DSparseArrayPointContainer& operator=(const U2DSparseArrayPointContainer& inContainer) { return *this; }

    void BuildPointList(std::vector<U2DPoint<long> >& outList, long inStep);
    void Clear();
    U2DPoint<long> ComputeCenterPoint(bool inMustBeInList);
    bool Contains(const U2DPoint<long>& inPoint) const;
    long CountContainerNodes();
    void Create(const std::vector<U2DPoint<long> >& inPoints);
    U2DPoint<long> GetClosestPoint(const U2DPoint<long>& inPoint);
    void GetOnlyExtremeEdgePoints(std::vector<U2DPoint<long> >& outList);
    U2DPoint<long> GetRandomPoint(URandom& inRandom);
    void Insert(const U2DPoint<long>& inPoint);
    void Insert(const std::vector<U2DPoint<long> >& inPoints);
    bool Remove(const U2DPoint<long>& inPoint);
    long Size() const;
    U2DPoint<long> operator[](long inIndex) const;
    // port (save version 0.263, the continent manager's regions in a saved game): the set as it is (empty columns
    // too: GetRandomPoint draws a column), as numbers: the offset, the count, the column count, then per column 0 (none)
    // or 1, its size, its run count and its runs' ends (ee_sparse_raw); SetRaw makes the set from them (false, the set
    // cleared, when they are not such a list)
    void GetRaw(std::vector<long>& outRaw) const;
    bool SetRaw(const std::vector<long>& inRaw);

private:
    EeSparseArray* mPoints;                 // the points (ee_sparse_new)
};

#endif
