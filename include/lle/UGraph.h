// C:\EEX\Low-Level Engine\Utilities\UGraph.h (RECONSTRUCTION). Requires lle/LowLevelEngine.h, lle/U2DPoint.h,
// lle/GEColor.h. A set of named series of integer points (UIGraph/UILineGraph draw it).
//
// Whole in Rust (rust/crates/ee_base/src/util graph.rs, LLE lane round 7): a series is an EeUtilSeries and a graph's series and
// range an EeUtilGraph; these classes hold the pointer and forward. GetSeries hands out the graph's slot for a series
// (a UGraphSeries is that one pointer), GetDataPoint and GetColor the Rust values' addresses (laid out as U2DPoint<long>
// and GEColor): each lasts until its container changes, as the C++ vectors' did. The title and axis labels, which
// nothing read (UIGraph has its own), are gone (LLE round 13).
#ifndef LLE_UGRAPH_H
#define LLE_UGRAPH_H

#include "rust/base/util.h"

class LLE_API UGraphSeries {                // one pointer (was sizeof 0x30)
public:
    UGraphSeries() : mSeries(ee_util_series_new()) {}
    UGraphSeries(const UGraphSeries& inSeries) : mSeries(ee_util_series_clone(inSeries.mSeries)) {}
    ~UGraphSeries() { ee_util_series_free(this->mSeries); }
    UGraphSeries& operator=(const UGraphSeries& inSeries)
        { ee_util_series_assign(this->mSeries, inSeries.mSeries); return *this; }

    const GEColor& GetColor() const { return *reinterpret_cast<const GEColor*>(ee_util_series_color(this->mSeries)); }

    EeUtilSeries* mSeries;
};

class LLE_API UGraph {                      // a pointer (was sizeof 0x5c)
public:
    UGraph() : mGraph(ee_util_graph_new()) {}
    UGraph(const UGraph& inGraph) : mGraph(ee_util_graph_clone(inGraph.mGraph)) {}
    ~UGraph() { ee_util_graph_free(this->mGraph); }
    UGraph& operator=(const UGraph& inGraph)
        { ee_util_graph_assign(this->mGraph, inGraph.mGraph); return *this; }

    EeUtilGraph* mGraph;                    // the series and min/max
};

#endif
