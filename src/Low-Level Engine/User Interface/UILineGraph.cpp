// C:\EEX\Low-Level Engine\User Interface\UILineGraph.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100785cd..0x10078a70: the U2D key init chains, then the ctor 0x100786ed, dtor, DrawPolygons, DrawSeries and
// FormatXValues/FormatYValues (ICF-folded). No template COMDATs; the implicit copy members and ??_E are emitted by
// UIControlsTable.cpp. UILBString.cpp ends at 0x100785cd; UILRTiledBackground.cpp starts at 0x10078a70.
//
// Which series draw and each segment's skip, clip and scale are Rust (rust/crates/ee_ui/src/graph.rs,
// docs/RUST_PLAN.md R6); the points, the line and the draws stay here.
#include <windows.h>

#include "rust/ee_ui.h"
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GELine.h"
#include "lle/GEText.h"
#include "lle/TMPoll.h"
#include "lle/UGraph.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIGraph.h"
#include "lle/UILineGraph.h"

// The Rust line graph's questions (ee_ui graph.rs): the copied graph's series, the virtual DrawSeries, the points
struct UILineGraphRust {
    static UILineGraph* Graph(void* inGraph) { return static_cast<UILineGraph*>(inGraph); }
    static const UGraph& Copied(void* inGraph) { return Graph(inGraph)->mCopiedGraph; }
    static EeUiGraphRange Range(const UGraph& inGraph)
    {
        return EeUiGraphRange{inGraph.GetMinX(), inGraph.GetMaxX(), inGraph.GetMinY(), inGraph.GetMaxY()};
    }
    static EeUiGraphRange Displayed(const UIGraph* inGraph)
    {
        return EeUiGraphRange{inGraph->mMinX, inGraph->mMaxX, inGraph->mMinY, inGraph->mMaxY};
    }
    static long DrawSeries(UILineGraph* inGraph, const UGraphSeries& inSeries)
    {
        GELine theLine;
        const GERectangle& r = inGraph->mGraphRectangle;
        theLine.SetColor(inSeries.GetColor());
        return ee_ui_linegraph_draw_series(inGraph, &theLine, &inSeries, inSeries.GetDataPointCount(), inSeries.GetMinX(),
                                           Range(inGraph->mCopiedGraph), Displayed(inGraph),
                                           EeUiRect{r.GetBottom(), r.GetLeft(), r.GetRight(), r.GetTop()});
    }
};

extern "C" unsigned long ee_uicpp_linegraph_series(void* inGraph, unsigned long inIndex, int inWhich)
{
    const UGraphSeries& theSeries = UILineGraphRust::Copied(inGraph).GetSeries(inIndex);
    return inWhich == 0 ? (unsigned long)theSeries.GetIsEnabled() : theSeries.GetDataPointCount();
}
extern "C" long ee_uicpp_linegraph_draw_series(void* inGraph, unsigned long inIndex)
{
    return UILineGraphRust::Graph(inGraph)->DrawSeries(UILineGraphRust::Copied(inGraph).GetSeries(inIndex));
}
extern "C" void ee_uicpp_linegraph_point(const void* inSeries, unsigned long inIndex, long* outX, long* outY)
{
    const U2DPoint<long>& thePoint = static_cast<const UGraphSeries*>(inSeries)->GetDataPoint(inIndex);
    *outX = thePoint.GetX();
    *outY = thePoint.GetY();
}

// Creates a line graph control on the form (plain graph setup)
UILineGraph::UILineGraph(unsigned long inId, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                         unsigned long inBorderThickness)
    : UIGraph(inId, inRectangle, inForm, inIndex, inBorderThickness)
{
}

// Destroys the line graph (members only)
UILineGraph::~UILineGraph()
{
}

// Draws the graph frame, then every enabled series with at least two points; returns the last draw result
long UILineGraph::DrawPolygons()
{
    long theResult = UIGraph::DrawPolygons();
    return ee_ui_linegraph_draw_list(this, theResult, this->mCopiedGraph.GetSeriesCount());
}

// Draws a series as line segments scaled into the graph area, clipped to the displayed range
// (skips a vertical segment at or left of the series' minimum X)
long UILineGraph::DrawSeries(const UGraphSeries& inSeries)
{
    return UILineGraphRust::DrawSeries(this, inSeries);
}

// Formats an axis value as text with two decimals (FormatXValues shares this body)
void UILineGraph::FormatYValues(float inValue, UWideString& outString)
{
    sprintf(outString, "%.2f", inValue);
}

// FormatXValues is identical and ICF-folded into FormatYValues (both exports 0x10078a53)
void UILineGraph::FormatXValues(float inValue, UWideString& outString)
{
    sprintf(outString, "%.2f", inValue);
}
