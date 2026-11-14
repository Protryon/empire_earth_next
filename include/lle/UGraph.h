// C:\EEX\Low-Level Engine\Utilities\UGraph.h (RECONSTRUCTION). Requires lle/LowLevelEngine.h, lle/U2DPoint.h,
// lle/GEColor.h. A set of named series of integer points (UIGraph/UILineGraph draw it).
//
// Whole in Rust (rust/crates/ee_util graph.rs, LLE lane round 7): a series is an EeUtilSeries and a graph's series and
// range an EeUtilGraph; these classes hold the pointer and forward. GetSeries hands out the graph's slot for a series
// (a UGraphSeries is that one pointer), GetDataPoint and GetColor the Rust values' addresses (laid out as U2DPoint<long>
// and GEColor): each lasts until its container changes, as the C++ vectors' did. The title and labels stay here.
#ifndef LLE_UGRAPH_H
#define LLE_UGRAPH_H

#include "rust/ee_util.h"

class LLE_API UGraphSeries {                // one pointer (was sizeof 0x30)
public:
    UGraphSeries() : mSeries(ee_util_series_new()) {}
    UGraphSeries(const UGraphSeries& inSeries) : mSeries(ee_util_series_clone(inSeries.mSeries)) {}
    ~UGraphSeries() { ee_util_series_free(this->mSeries); }
    UGraphSeries& operator=(const UGraphSeries& inSeries)
        { ee_util_series_assign(this->mSeries, inSeries.mSeries); return *this; }

    // replaces the last point if it has the same x, keeps the points sorted by x then y, recomputes min/max
    void AddDataPoint(const U2DPoint<long>& inPoint) { ee_util_series_add_point(this->mSeries, inPoint.mX, inPoint.mY); }
    void ClearAllDataPoints() { ee_util_series_clear(this->mSeries); }
    const GEColor& GetColor() const { return *reinterpret_cast<const GEColor*>(ee_util_series_color(this->mSeries)); }
    const U2DPoint<long>& GetDataPoint(unsigned long inIndex) const
        { return *reinterpret_cast<const U2DPoint<long>*>(ee_util_series_point(this->mSeries, inIndex)); }
    const unsigned long GetDataPointCount() const { return ee_util_series_count(this->mSeries); }
    const bool GetIsEnabled() const { return ee_util_series_enabled(this->mSeries); }
    long GetMaxX() const { return ee_util_series_range(this->mSeries)->max_x; }
    long GetMaxY() const { return ee_util_series_range(this->mSeries)->max_y; }
    long GetMinX() const { return ee_util_series_range(this->mSeries)->min_x; }
    long GetMinY() const { return ee_util_series_range(this->mSeries)->min_y; }
    // min/max are not updated
    void RemoveDataPoint(unsigned long inIndex) { ee_util_series_remove_point(this->mSeries, inIndex); }
    void SetColor(const GEColor& inColor)
        { ee_util_series_set_color(this->mSeries, reinterpret_cast<const EeUtilColor*>(&inColor)); }
    void SetIsEnabled(bool inIsEnabled) { ee_util_series_set_enabled(this->mSeries, inIsEnabled); }

private:
    friend class UGraph;
    EeUtilSeries* mSeries;
};

class LLE_API UGraph {                      // a pointer and the three strings (was sizeof 0x5c)
public:
    UGraph() : mGraph(ee_util_graph_new())
    {
        this->mXLabel = "";
        this->mYLabel = "";
        this->mTitle = "";
    }
    UGraph(const UGraph& inGraph)
        : mGraph(ee_util_graph_clone(inGraph.mGraph)), mTitle(inGraph.mTitle), mXLabel(inGraph.mXLabel),
          mYLabel(inGraph.mYLabel) {}
    ~UGraph() { ee_util_graph_free(this->mGraph); }
    UGraph& operator=(const UGraph& inGraph)
    {
        if (&inGraph != this) {
            ee_util_graph_assign(this->mGraph, inGraph.mGraph);
            this->mTitle = inGraph.mTitle;
            this->mXLabel = inGraph.mXLabel;
            this->mYLabel = inGraph.mYLabel;
        }
        return *this;
    }

    // adds a copy of the series and widens min/max to include it
    void AddSeries(const UGraphSeries& inSeries) { ee_util_graph_add_series(this->mGraph, inSeries.mSeries); }
    void ClearAllSeries() { ee_util_graph_clear(this->mGraph); }
    // adds to outGraph, per series index of both, the sum of their values at every x either has
    void CombineGraph(const UGraph& inGraph, UGraph& outGraph)
        { ee_util_graph_combine(this->mGraph, inGraph.mGraph, outGraph.mGraph); }
    long GetMaxX() const { return ee_util_graph_range(this->mGraph)->max_x; }
    long GetMaxY() const { return ee_util_graph_range(this->mGraph)->max_y; }
    long GetMinX() const { return ee_util_graph_range(this->mGraph)->min_x; }
    long GetMinY() const { return ee_util_graph_range(this->mGraph)->min_y; }
    const UGraphSeries& GetSeries(unsigned long inIndex) const
        { return *reinterpret_cast<const UGraphSeries*>(ee_util_graph_series(this->mGraph, inIndex)); }
    const unsigned long GetSeriesCount() const { return ee_util_graph_count(this->mGraph); }
    const UWideString& GetTitle() const { return this->mTitle; }
    const UWideString& GetXLabel() const { return this->mXLabel; }
    const UWideString& GetYLabel() const { return this->mYLabel; }
    // reduces every series to one point per x, keeping the highest y
    void PruneGraph() { ee_util_graph_prune(this->mGraph); }
    // min/max are not updated
    void RemoveSeries(unsigned long inIndex) { ee_util_graph_remove_series(this->mGraph, inIndex); }
    void SetIsSeriesEnabled(unsigned long inIndex, bool inIsEnabled)
        { ee_util_graph_set_enabled(this->mGraph, inIndex, inIsEnabled); }
    void SetTitle(const UWideString& inTitle) { this->mTitle = inTitle; }
    void SetXLabel(const UWideString& inLabel) { this->mXLabel = inLabel; }
    void SetYLabel(const UWideString& inLabel) { this->mYLabel = inLabel; }

private:
    EeUtilGraph* mGraph;                    // the series and min/max
    UWideString mTitle;
    UWideString mXLabel;
    UWideString mYLabel;
};

#endif
