// EEUIPGSLineGraph.h (RECONSTRUCTION; file name inferred; member names provisional). The
// post-game summary's line graph (control 0 of its form): RTTI EEUIPGSLineGraph > UILineGraph, vtable 0x44b95c; ctor
// 0x276442 (UILineGraph's, then 31 UGraph ctors at +0x230), ??_G 0x276496. Four new virtuals after UILineGraph's
// DrawSeries (0xb8): 0xbc 0x2764e0 (a series from the points, added to graph n with the colour), 0xc0 0x276485
// (&mGraphs[n]), 0xc4 0x2765eb (mGraph = mGraphs[n], SetDirty(true)), 0xc8 0x276616 (mGraphs[n] = graph). The
// summary also copies UIGraph::mGraph in and out directly (0x1e3cd4, 0x1e3cf5): inline accessors here. Requires
// lle/UILineGraph.h; the .cpp needs ee/EEPlayer.h (EEStatisticsPoint).
#ifndef EE_EEUIPGSLINEGRAPH_H
#define EE_EEUIPGSLINEGRAPH_H

struct EEStatisticsPoint;                   // ee/EEPlayer.h

class EEUIPGSLineGraph : public UILineGraph {
public:
    EEUIPGSLineGraph(unsigned long inId, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                     unsigned long inBorderThickness);
    // GetGraphAt is inline (its body 0x276485 sits between the ctor and ??_G, emitted with the vtable)
    // the overrides of UILineGraph's slots follow the dtor in EEUIPGSLineGraph.cpp
    virtual void AddGraphSeries(const std::vector<EEStatisticsPoint>& inPoints, unsigned long inGraph,
                                GEColor inColor);         // +0xbc
    virtual UGraph& GetGraphAt(unsigned long inGraph) { return this->mGraphs[inGraph]; }  // +0xc0
    virtual void ShowGraph(unsigned long inGraph);       // +0xc4
    virtual void SetGraphAt(const UGraph& inGraph, unsigned long inIndex);   // +0xc8
    virtual ~EEUIPGSLineGraph();
    virtual long DrawPolygons();                         // +0x18 (calls UILineGraph's)
    virtual long DrawSeries(const UGraphSeries& inSeries);   // +0xb8 (calls UILineGraph's)
    virtual void FormatYValues(float inValue, UWideString& outString);   // +0x9c "%d", rounded
    virtual void FormatXValues(float inValue, UWideString& outString);   // +0xa0 "%d:%.2d" game time
    const UGraph& GetShownGraph() const { return this->mGraph; }

    UGraph mGraphs[31];                     // +0x230
};

#endif
