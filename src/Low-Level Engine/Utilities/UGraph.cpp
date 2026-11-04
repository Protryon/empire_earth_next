// C:\EEX\Low-Level Engine\Utilities\UGraph.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1005726b..0x10058d38: the U2DPoint key chains, the UGraphSeries/UGraph header-inline members (at the point
// lle/UGraph.h is included), the out-of-line members (UGraphSeries first, then UGraph), then the template COMDATs
// (vector<U2DPoint<long> >/vector<UGraphSeries>/vector<unsigned long> members, the data point sort). The library
// __ftol object sits between UEncryption.cpp and this unit (0x10057218). UIBackground.cpp starts at 0x10058d38.
#include <windows.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/U2DPoint.h"
#include "lle/GEColor.h"
#include "lle/UGraph.h"

// Orders data points by x, then y (the sort predicate of AddDataPoint; RTTI name UUGraphSeriesDataPointSorter).
// The sort COMDATs compare x, then y on a tie (cmp; jne; cmp; setl: _Unguarded_insert 0x58a6d).
class UUGraphSeriesDataPointSorter {
public:
    virtual ~UUGraphSeriesDataPointSorter() {}
    bool operator()(const U2DPoint<long>& inPoint1, const U2DPoint<long>& inPoint2) const
    {
        if (inPoint1.GetX() != inPoint2.GetX())
            return inPoint1.GetX() < inPoint2.GetX();
        return inPoint1.GetY() < inPoint2.GetY();
    }
};

static bool UGGetValueAt(const UGraphSeries& inSeries, long inX, long& outY);

// Header-inline members emitted here (names in analysis/lle/names.txt)
// FUNCTION: LLE 0x100572cb
// FUNCTION: LLE 0x100572cf
// FUNCTION: LLE 0x100572e7
// FUNCTION: LLE 0x100572f1
// FUNCTION: LLE 0x10057331
// FUNCTION: LLE 0x10057336
// FUNCTION: LLE 0x1005733a
// FUNCTION: LLE 0x1005733e
// FUNCTION: LLE 0x10057342
// FUNCTION: LLE 0x10057358
// FUNCTION: LLE 0x1005735c
// FUNCTION: LLE 0x10057360
// FUNCTION: LLE 0x1005736f
// FUNCTION: LLE 0x1005737e
// FUNCTION: LLE 0x1005738d
// FUNCTION: LLE 0x100573de

// ---- UGraphSeries

// FUNCTION: LLE 0x10057402
// Creates an empty, enabled, white graph series
UGraphSeries::UGraphSeries()
{
    this->ClearAllDataPoints();
    this->mColor = GEColor(1.0f, 1.0f, 1.0f);
    this->mIsEnabled = true;
}

// FUNCTION: LLE 0x10057441
// Adds a data point (replacing the last point if it has the same x), keeps the points sorted, updates min/max
void UGraphSeries::AddDataPoint(const U2DPoint<long>& inPoint)
{
    if (this->GetDataPointCount() > 0 && this->mDataPoints.back().GetX() == inPoint.GetX())
        this->mDataPoints.pop_back();
    this->mDataPoints.push_back(inPoint);
    std::sort(this->mDataPoints.begin(), this->mDataPoints.end(), UUGraphSeriesDataPointSorter());
    this->RecomputeMinMaxValues();
}

// FUNCTION: LLE 0x100574dc
// Removes every data point and resets the min/max values
void UGraphSeries::ClearAllDataPoints()
{
    this->mDataPoints.clear();
    this->mMinY = this->mMinX = 0x7fffffff;
    this->mMaxY = this->mMaxX = 0x80000000;
}

// FUNCTION: LLE 0x100574f8
// Returns the data point at the given index
const U2DPoint<long>& UGraphSeries::GetDataPoint(unsigned long inIndex) const
{
    return this->mDataPoints[inIndex];
}

// FUNCTION: LLE 0x10057505
// Removes the data point at the given index (min/max are not updated)
void UGraphSeries::RemoveDataPoint(unsigned long inIndex)
{
    this->mDataPoints.erase(this->mDataPoints.begin() + inIndex);
}

// FUNCTION: LLE 0x1005753b
// Recomputes the series' min/max x and y from its points
void UGraphSeries::RecomputeMinMaxValues()
{
    this->mMinY = this->mMinX = 0x7fffffff;
    this->mMaxY = this->mMaxX = 0x80000000;
    for (std::vector<U2DPoint<long> >::iterator i = this->mDataPoints.begin(); i != this->mDataPoints.end(); i++) {
        if (i->mX < this->mMinX)
            this->mMinX = i->mX;
        if (i->mX > this->mMaxX)
            this->mMaxX = i->mX;
        if (i->mY < this->mMinY)
            this->mMinY = i->mY;
        if (i->mY > this->mMaxY)
            this->mMaxY = i->mY;
    }
}

// FUNCTION: LLE 0x10057589
// Copies another series' points, color, min/max and enabled flag
UGraphSeries& UGraphSeries::operator=(const UGraphSeries& inSeries)
{
    if (&inSeries != this) {
        this->ClearAllDataPoints();
        this->mDataPoints.resize(inSeries.GetDataPointCount());
        for (long i = inSeries.GetDataPointCount() - 1; i >= 0; i--)
            this->mDataPoints[i] = inSeries.mDataPoints[i];
        this->SetColor(inSeries.GetColor());
        this->mMaxX = inSeries.mMaxX;
        this->mMaxY = inSeries.mMaxY;
        this->mMinX = inSeries.mMinX;
        this->mMinY = inSeries.mMinY;
        this->mIsEnabled = inSeries.mIsEnabled;
    }
    return *this;
}

// ---- UGraph

// FUNCTION: LLE 0x10057629
// Creates an empty graph with blank title and axis labels
UGraph::UGraph()
{
    this->ClearAllSeries();
    this->mXLabel = "";
    this->mYLabel = "";
    this->mTitle = "";
}

// FUNCTION: LLE 0x10057686
// Adds a copy of a series and widens the graph's min/max to include it
void UGraph::AddSeries(const UGraphSeries& inSeries)
{
    this->mSeries.push_back(inSeries);
    if (inSeries.GetMinX() < this->mMinX)
        this->mMinX = inSeries.GetMinX();
    if (inSeries.GetMaxX() > this->mMaxX)
        this->mMaxX = inSeries.GetMaxX();
    if (inSeries.GetMinY() < this->mMinY)
        this->mMinY = inSeries.GetMinY();
    if (inSeries.GetMaxY() > this->mMaxY)
        this->mMaxY = inSeries.GetMaxY();
}

// FUNCTION: LLE 0x100576c5
// Removes every series and resets the min/max values
void UGraph::ClearAllSeries()
{
    this->mSeries.clear();
    this->mMinY = this->mMinX = 0x7fffffff;
    this->mMaxY = this->mMaxX = 0x80000000;
}

// FUNCTION: LLE 0x100576e1
// Adds to outGraph, per series index, the sum of both graphs' values at every x either of them has
void UGraph::CombineGraph(const UGraph& inGraph, UGraph& outGraph)
{
    U2DPoint<long> thePoint;
    UGraphSeries theXValues;
    UGraphSeries theCombined;
    unsigned long theCount = min(this->GetSeriesCount(), inGraph.GetSeriesCount());
    for (unsigned long i = 0; i < theCount; i++) {
        theXValues.ClearAllDataPoints();
        theCombined.ClearAllDataPoints();
        unsigned long j;
        unsigned long k;
        bool theFound;
        for (j = 0; j < this->GetSeries(i).GetDataPointCount(); j++) {
            thePoint = this->GetSeries(i).GetDataPoint(j);
            theFound = false;
            for (k = 0; k < theXValues.GetDataPointCount(); k++) {
                if (theXValues.GetDataPoint(k).mX == thePoint.mX) {
                    theFound = true;
                    break;
                }
            }
            if (!theFound)
                theXValues.AddDataPoint(thePoint);
        }
        for (j = 0; j < inGraph.GetSeries(i).GetDataPointCount(); j++) {
            thePoint = inGraph.GetSeries(i).GetDataPoint(j);
            theFound = false;
            for (k = 0; k < theXValues.GetDataPointCount(); k++) {
                if (theXValues.GetDataPoint(k).mX == thePoint.mX) {
                    theFound = true;
                    break;
                }
            }
            if (!theFound)
                theXValues.AddDataPoint(thePoint);
        }
        for (k = 0; k < theXValues.GetDataPointCount(); k++) {
            thePoint = theXValues.GetDataPoint(k);
            long theY1;
            long theY2;
            if (!UGGetValueAt(this->GetSeries(i), thePoint.mX, theY1))
                theY1 = 0;
            if (!UGGetValueAt(inGraph.GetSeries(i), thePoint.mX, theY2))
                theY2 = 0;
            thePoint.mY = theY1 + theY2;
            theCombined.AddDataPoint(thePoint);
        }
        outGraph.AddSeries(theCombined);
    }
    outGraph.RecomputeMinMaxValues();
}

// FUNCTION: LLE 0x10057919
// Returns the series value at x: the y of the last point at or before x (false when empty or x below the minimum)
static bool UGGetValueAt(const UGraphSeries& inSeries, long inX, long& outY)
{
    U2DPoint<long> theNext;
    bool theResult = false;
    if (inSeries.GetDataPointCount() > 0 && inX >= inSeries.GetMinX()) {
        unsigned long i = 0;
        bool theFound = false;
        while (i < inSeries.GetDataPointCount()) {
            U2DPoint<long> thePoint = inSeries.GetDataPoint(i);
            i++;
            if (i < inSeries.GetDataPointCount())
                theNext = inSeries.GetDataPoint(i);
            else
                theNext = thePoint;
            if (inX >= thePoint.mX && inX < theNext.mX) {
                outY = thePoint.mY;
                theFound = true;
                break;
            }
        }
        // Fixed: the found point's value used to be overwritten here by the next point's
        if (!theFound)
            outY = theNext.mY;
        theResult = true;
    }
    return theResult;
}

// FUNCTION: LLE 0x100579b1
// Returns the series at the given index
const UGraphSeries& UGraph::GetSeries(unsigned long inIndex) const
{
    return this->mSeries[inIndex];
}

// FUNCTION: LLE 0x100579c3
// Reduces every series to one point per x, keeping the highest y
void UGraph::PruneGraph()
{
    UGraph theGraph;
    UGraphSeries theSeries;
    U2DPoint<long> thePoint;
    std::vector<long> theXValues;
    theGraph = *this;
    theGraph.ClearAllSeries();
    for (unsigned long i = 0; i < this->GetSeriesCount(); i++) {
        theSeries = this->GetSeries(i);
        theSeries.ClearAllDataPoints();
        theXValues.clear();
        for (unsigned long j = 0; j < this->GetSeries(i).GetDataPointCount(); j++) {
            thePoint = this->GetSeries(i).GetDataPoint(j);
            std::vector<long>::iterator theFound = std::find(theXValues.begin(), theXValues.end(), thePoint.GetX());
            if (theFound != theXValues.end())
                continue;
            for (unsigned long k = 0; k < this->GetSeries(i).GetDataPointCount(); k++) {
                if (thePoint.GetX() == this->GetSeries(i).GetDataPoint(k).GetX()
                    && thePoint.GetY() < this->GetSeries(i).GetDataPoint(k).GetY())
                    thePoint = this->GetSeries(i).GetDataPoint(k);
            }
            theXValues.push_back(thePoint.GetX());
            theSeries.AddDataPoint(thePoint);
        }
        theGraph.AddSeries(theSeries);
    }
    *this = theGraph;
    this->RecomputeMinMaxValues();
}

// FUNCTION: LLE 0x10057bc5
// Removes the series at the given index (min/max are not updated)
void UGraph::RemoveSeries(unsigned long inIndex)
{
    this->mSeries.erase(this->mSeries.begin() + inIndex);
}

// FUNCTION: LLE 0x10057bfb
// Shows or hides the series at the given index
void UGraph::SetIsSeriesEnabled(unsigned long inIndex, bool inIsEnabled)
{
    this->mSeries[inIndex].SetIsEnabled(inIsEnabled);
}

// FUNCTION: LLE 0x10057c13
// Recomputes the graph's min/max x and y from its series
void UGraph::RecomputeMinMaxValues()
{
    this->mMinY = this->mMinX = 0x7fffffff;
    this->mMaxY = this->mMaxX = 0x80000000;
    for (std::vector<UGraphSeries>::iterator i = this->mSeries.begin(); i != this->mSeries.end(); i++) {
        if (i->GetMinX() < this->mMinX)
            this->mMinX = i->GetMinX();
        if (i->GetMaxX() > this->mMaxX)
            this->mMaxX = i->GetMaxX();
        if (i->GetMinY() < this->mMinY)
            this->mMinY = i->GetMinY();
        if (i->GetMaxY() > this->mMaxY)
            this->mMaxY = i->GetMaxY();
    }
}

// FUNCTION: LLE 0x10057c63
// Copies another graph's series, title, axis labels and min/max
UGraph& UGraph::operator=(const UGraph& inGraph)
{
    if (&inGraph != this) {
        this->ClearAllSeries();
        this->mSeries.resize(inGraph.mSeries.size());
        for (long i = inGraph.mSeries.size() - 1; i >= 0; i--)
            this->mSeries[i] = inGraph.mSeries[i];
        this->mTitle = inGraph.mTitle;
        this->mXLabel = inGraph.mXLabel;
        this->mYLabel = inGraph.mYLabel;
        this->mMaxX = inGraph.mMaxX;
        this->mMaxY = inGraph.mMaxY;
        this->mMinX = inGraph.mMinX;
        this->mMinY = inGraph.mMinY;
    }
    return *this;
}

// Compiler-generated: the U2DPoint key init chains, the sorter's ??_G, template COMDATs (names in names.txt)
// SYNTHETIC: LLE 0x1005726b
// SYNTHETIC: LLE 0x10057275
// SYNTHETIC: LLE 0x10057285
// SYNTHETIC: LLE 0x10057291
// SYNTHETIC: LLE 0x1005729b
// SYNTHETIC: LLE 0x100572a5
// SYNTHETIC: LLE 0x100572b5
// SYNTHETIC: LLE 0x100572c1
// SYNTHETIC: LLE 0x100574bf
// SYNTHETIC: LLE 0x10057d1b
// SYNTHETIC: LLE 0x10057d32
// SYNTHETIC: LLE 0x10057d66
// SYNTHETIC: LLE 0x10057dca
// SYNTHETIC: LLE 0x10057e14
// SYNTHETIC: LLE 0x10057e30
// SYNTHETIC: LLE 0x10057e5c
// SYNTHETIC: LLE 0x10057ecf
// SYNTHETIC: LLE 0x10057f0a
// SYNTHETIC: LLE 0x10057f1f
// SYNTHETIC: LLE 0x10057f4b
// SYNTHETIC: LLE 0x10058007
// SYNTHETIC: LLE 0x10058061
// SYNTHETIC: LLE 0x100580ca
// SYNTHETIC: LLE 0x10058104
// SYNTHETIC: LLE 0x1005813c
// SYNTHETIC: LLE 0x10058175
// SYNTHETIC: LLE 0x10058450
// SYNTHETIC: LLE 0x1005847b
// SYNTHETIC: LLE 0x10058492
// SYNTHETIC: LLE 0x10058574
// SYNTHETIC: LLE 0x10058582
// SYNTHETIC: LLE 0x100585ac
// SYNTHETIC: LLE 0x100585d8
// SYNTHETIC: LLE 0x10058601
// SYNTHETIC: LLE 0x10058625
// SYNTHETIC: LLE 0x10058642
// SYNTHETIC: LLE 0x1005866e
// SYNTHETIC: LLE 0x10058680
// SYNTHETIC: LLE 0x1005868b
// SYNTHETIC: LLE 0x100586bd
// SYNTHETIC: LLE 0x100587ac
// SYNTHETIC: LLE 0x100587f0
// SYNTHETIC: LLE 0x100588b9
// SYNTHETIC: LLE 0x100588fc
// SYNTHETIC: LLE 0x10058943
// SYNTHETIC: LLE 0x10058a6d
// SYNTHETIC: LLE 0x10058aa7
// SYNTHETIC: LLE 0x10058b06
// SYNTHETIC: LLE 0x10058b38
// SYNTHETIC: LLE 0x10058b5d
// SYNTHETIC: LLE 0x10058bf5
// SYNTHETIC: LLE 0x10058c6c
// SYNTHETIC: LLE 0x10058c8d
// SYNTHETIC: LLE 0x10058cea
