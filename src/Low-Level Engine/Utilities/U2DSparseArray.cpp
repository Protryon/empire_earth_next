// C:\EEX\Low-Level Engine\Utilities\U2DSparseArray.cpp (file name inferred: sorts between U2DFractal.cpp and
// UDataCompression.cpp)
//
// Port: the sparse point set is Rust (ee_astar sparse.rs, docs/RUST_PLAN.md R5b.1, branch rust-r5b-cont2); these
// members forward to the handle the class owns. The original's run lists, their node pool (SetPoolSize,
// DumpPoolStats) and the column array are gone; ComputeCenterPoint keeps the U2DComputeCenterPoint template.
#include <windows.h>
#include <algorithm>
#include <typeinfo>

#include "lle/LowLevelEngine.h"
#include "lle/U2DPoint.h"
#include "lle/U2DSparseArray.h"
#include "lle/URandom.h"
#include "rust/ee_astar.h"

static_assert(sizeof(EePoint) == 8, "EePoint");

static U2DPoint<long> U2DSparsePoint(EePoint inPoint)
{
    return U2DPoint<long>(inPoint.x, inPoint.y);
}

// Copies a point list into the handle's (int32) points
static void U2DSparsePoints(const std::vector<U2DPoint<long> >& inPoints, std::vector<EePoint>& outPoints)
{
    outPoints.resize(inPoints.size());
    for (size_t i = 0; i < inPoints.size(); i++) {
        outPoints[i].x = (int32_t)inPoints[i].mX;
        outPoints[i].y = (int32_t)inPoints[i].mY;
    }
}

// The handle's point list (BuildPointList / GetOnlyExtremeEdgePoints) into outList, cleared first
static void U2DSparseList(EeSparseArray* inPoints, bool inExtremes, long inStep, std::vector<U2DPoint<long> >& outList)
{
    size_t theCount = 0;
    const EePoint* thePoints = ee_sparse_points(inPoints, inExtremes, (int32_t)inStep, &theCount);
    outList.clear();
    outList.reserve(theCount);
    for (size_t i = 0; i < theCount; i++)
        outList.push_back(U2DSparsePoint(thePoints[i]));
}

static float U2DSparseGenerate(void* inRandom)
{
    return static_cast<URandom*>(inRandom)->Generate();
}

U2DSparseArrayPointContainer::U2DSparseArrayPointContainer() : mPoints(ee_sparse_new())
{
}

U2DSparseArrayPointContainer::U2DSparseArrayPointContainer(const U2DSparseArrayPointContainer& inContainer)
    : mPoints(ee_sparse_clone(inContainer.mPoints))
{
}

U2DSparseArrayPointContainer::~U2DSparseArrayPointContainer()
{
    ee_sparse_free(this->mPoints);
}

// Rebuilds the list from every column's points (every inStep-th y), last column first
void U2DSparseArrayPointContainer::BuildPointList(std::vector<U2DPoint<long> >& outList, long inStep)
{
    U2DSparseList(this->mPoints, false, inStep, outList);
}

// Deletes every column and empties the set
void U2DSparseArrayPointContainer::Clear()
{
    ee_sparse_clear(this->mPoints);
}

// Returns the center point of the set's points (optionally one that is in the set)
U2DPoint<long> U2DSparseArrayPointContainer::ComputeCenterPoint(bool inMustBeInList)
{
    std::vector<U2DPoint<long> > thePoints;
    this->BuildPointList(thePoints, 1);
    U2DPoint<long> theResult;
    theResult = U2DComputeCenterPoint(thePoints, inMustBeInList);
    return theResult;
}

// Returns whether the set holds the point
bool U2DSparseArrayPointContainer::Contains(const U2DPoint<long>& inPoint) const
{
    return ee_sparse_contains(this->mPoints, (int32_t)inPoint.mX, (int32_t)inPoint.mY);
}

// Returns the number of runs over all columns
long U2DSparseArrayPointContainer::CountContainerNodes()
{
    return ee_sparse_run_count(this->mPoints);
}

// Rebuilds the set from a point list, sizing the column array from the list's x range
void U2DSparseArrayPointContainer::Create(const std::vector<U2DPoint<long> >& inPoints)
{
    std::vector<EePoint> thePoints;
    U2DSparsePoints(inPoints, thePoints);
    ee_sparse_insert_list(this->mPoints, thePoints.empty() ? NULL : &thePoints[0], thePoints.size(), true);
}

// Returns the point itself if it is in the set, else the nearest point of the set (squared distance)
U2DPoint<long> U2DSparseArrayPointContainer::GetClosestPoint(const U2DPoint<long>& inPoint)
{
    return U2DSparsePoint(ee_sparse_closest_point(this->mPoints, (int32_t)inPoint.mX, (int32_t)inPoint.mY));
}

// Returns a random point (16 tries at a random column; then the first point; default point when empty)
U2DPoint<long> U2DSparseArrayPointContainer::GetRandomPoint(URandom& inRandom)
{
    return U2DSparsePoint(ee_sparse_random_point(this->mPoints, U2DSparseGenerate, &inRandom));
}

// Rebuilds the list from the lowest and highest point of each column
void U2DSparseArrayPointContainer::GetOnlyExtremeEdgePoints(std::vector<U2DPoint<long> >& outList)
{
    U2DSparseList(this->mPoints, true, 1, outList);
}

// Adds a point, growing the column array; a point left of the array with x <= 0 is silently ignored
void U2DSparseArrayPointContainer::Insert(const U2DPoint<long>& inPoint)
{
    ee_sparse_insert(this->mPoints, (int32_t)inPoint.mX, (int32_t)inPoint.mY);
}

// Adds every point of the list (from the last)
void U2DSparseArrayPointContainer::Insert(const std::vector<U2DPoint<long> >& inPoints)
{
    std::vector<EePoint> thePoints;
    U2DSparsePoints(inPoints, thePoints);
    ee_sparse_insert_list(this->mPoints, thePoints.empty() ? NULL : &thePoints[0], thePoints.size(), false);
}

// Removes a point, deleting its column when it becomes empty; false when it was not there
bool U2DSparseArrayPointContainer::Remove(const U2DPoint<long>& inPoint)
{
    return ee_sparse_remove(this->mPoints, (int32_t)inPoint.mX, (int32_t)inPoint.mY);
}

// Returns the point count
long U2DSparseArrayPointContainer::Size() const
{
    return ee_sparse_size(this->mPoints);
}

// Returns the point at the given index over all columns in order (default point when out of range)
U2DPoint<long> U2DSparseArrayPointContainer::operator[](long inIndex) const
{
    return U2DSparsePoint(ee_sparse_get(this->mPoints, (int32_t)inIndex));
}

// port (save version 0.263): the set as numbers, and the set from them (ee_astar sparse.rs raw / set_raw)
void U2DSparseArrayPointContainer::GetRaw(std::vector<long>& outRaw) const
{
    size_t theCount = 0;
    const int32_t* theRaw = ee_sparse_raw(this->mPoints, &theCount);
    outRaw.assign(theRaw, theRaw + theCount);
}

bool U2DSparseArrayPointContainer::SetRaw(const std::vector<long>& inRaw)
{
    std::vector<int32_t> theRaw(inRaw.begin(), inRaw.end());
    return ee_sparse_set_raw(this->mPoints, theRaw.empty() ? NULL : &theRaw[0], theRaw.size());
}
