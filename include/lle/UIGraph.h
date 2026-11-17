// C:\EEX\Low-Level Engine\User Interface\UIGraph.h (RECONSTRUCTION). A control that draws a UGraph: a grid, grid
// value labels, a title and axis labels, zoomed by a scale, and each enabled series as line segments (control type
// 13). The original's three classes, UIGraph (abstract), UILineGraph and the post-game summary's EEUIPGSLineGraph
// (31 stored graphs, its own value formats), are this one class (R6 game screens B, round 3): the kind says which.
//
// Whole in Rust (rust/crates/ee_ui/src/graphobj.rs): every member (the texts, rectangles, zoom, flags, the shown
// graph, its copy and the summary's stored graphs) and every method are the Rust object mRust; UIGraph.cpp holds
// the forwarders and the host. The screens' Rust reaches the summary's graphs through ee_ui's direct path.
// Requires lle/UIControl.h.
#ifndef LLE_UIGRAPH_H
#define LLE_UIGRAPH_H

#include "rust/ui/graphobj.h"

class GEFont;

class LLE_API UIGraph : public UIControl {
    friend struct UIGraphHost;              // UIGraph.cpp: the Rust object's calls out
public:
    UIGraph(unsigned long inId, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
            unsigned long inBorderThickness, EeUiGraphKind inKind = EeUiGraphKind::Line,
            EeUiGraphFormatFn inFormat = NULL);
    UIGraph(const UIGraph&) = delete;
    UIGraph& operator=(const UIGraph&) = delete;
    virtual ~UIGraph();
    virtual long CopyData();
    virtual long DrawPolygons();
    virtual long DrawTextA();
    void SetFont(GEFont* inFont);
    void SetFontColor(const GEColor& inColor);
    // the summary's: a series of the inCount (time, value) pairs in a colour, added to stored graph inGraph
    void AddGraphSeries(const long* inPoints, unsigned long inCount, unsigned long inGraph, const GEColor& inColor);
    virtual long UpdateMouse(const GE2DPoint& inLocation) { return kErrorOK; }
    virtual long UpdateKeyboard() { return kErrorOK; }
protected:
    virtual long MouseLButtonDown() { return kErrorOK; }
    virtual long MouseLButtonUp() { return kErrorOK; }
    virtual long MouseMove(const GE2DPoint& inLocation) { return kErrorOK; }
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void TabbedTo();
    virtual void TabbedFrom();

    EeUiGraph* mRust;                       // the state (graphobj.rs)
};

#endif
