// C:\EEX\Low-Level Engine\Utilities\UGraph.h (RECONSTRUCTION). Requires lle/LowLevelEngine.h, lle/U2DPoint.h,
// lle/GEColor.h. A set of named series of integer points (UIGraph/UILineGraph draw it). UGraph.cpp
// (0x1005726b..0x10058d38) is the first includer: the U2DPoint key chains, then these classes' inline members in
// declaration order (getters/setters, then the implicit copy ctors and dtors; most getters are ICF-folded into
// earlier twins), then the out-of-line members.
#ifndef LLE_UGRAPH_H
#define LLE_UGRAPH_H

class LLE_API UGraphSeries {                // sizeof 0x30 (UGraph::GetSeriesCount divides by 0x30)
public:
    UGraphSeries();
    // implicit copy ctor (0x572f1) and dtor (0x57331); operator= is user-defined
    UGraphSeries& operator=(const UGraphSeries& inSeries);

    void AddDataPoint(const U2DPoint<long>& inPoint);
    void ClearAllDataPoints();
    const GEColor& GetColor() const { return this->mColor; }
    const U2DPoint<long>& GetDataPoint(unsigned long inIndex) const;
    const unsigned long GetDataPointCount() const { return this->mDataPoints.size(); }
    const bool GetIsEnabled() const { return this->mIsEnabled; }
    long GetMaxX() const { return this->mMaxX; }
    long GetMaxY() const { return this->mMaxY; }
    long GetMinX() const { return this->mMinX; }
    long GetMinY() const { return this->mMinY; }
    void SetColor(const GEColor& inColor) { this->mColor = inColor; }
    void SetIsEnabled(bool inIsEnabled) { this->mIsEnabled = inIsEnabled; }

private:
    void RecomputeMinMaxValues();

    std::vector<U2DPoint<long> > mDataPoints;   // +0x00 sorted by x
    GEColor mColor;                         // +0x10
    long mMinX;                             // +0x1c
    long mMaxX;                             // +0x20
    long mMinY;                             // +0x24
    long mMaxY;                             // +0x28
    bool mIsEnabled;                        // +0x2c
};

class LLE_API UGraph {                      // sizeof 0x5c
public:
    UGraph();
    // implicit copy ctor (0x5738d) and dtor (0x573de); operator= is user-defined
    UGraph& operator=(const UGraph& inGraph);

    void AddSeries(const UGraphSeries& inSeries);
    void ClearAllSeries();
    void CombineGraph(const UGraph& inGraph, UGraph& outGraph);
    long GetMaxX() const { return this->mMaxX; }
    long GetMaxY() const { return this->mMaxY; }
    long GetMinX() const { return this->mMinX; }
    long GetMinY() const { return this->mMinY; }
    const UGraphSeries& GetSeries(unsigned long inIndex) const;
    const unsigned long GetSeriesCount() const { return this->mSeries.size(); }
    const UWideString& GetTitle() const { return this->mTitle; }
    const UWideString& GetXLabel() const { return this->mXLabel; }
    const UWideString& GetYLabel() const { return this->mYLabel; }
    void PruneGraph();
    void SetIsSeriesEnabled(unsigned long inIndex, bool inIsEnabled);
    void SetTitle(const UWideString& inTitle) { this->mTitle = inTitle; }
    void SetXLabel(const UWideString& inLabel) { this->mXLabel = inLabel; }
    void SetYLabel(const UWideString& inLabel) { this->mYLabel = inLabel; }

private:
    void RecomputeMinMaxValues();

    std::vector<UGraphSeries> mSeries;      // +0x00
    UWideString mTitle;                     // +0x10
    UWideString mXLabel;                    // +0x24
    UWideString mYLabel;                    // +0x38
    long mMinX;                             // +0x4c
    long mMaxX;                             // +0x50
    long mMinY;                             // +0x54
    long mMaxY;                             // +0x58
};

#endif
