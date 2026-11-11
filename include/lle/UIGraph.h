// C:\EEX\Low-Level Engine\User Interface\UIGraph.h (RECONSTRUCTION). A control that draws a UGraph: a grid, grid
// value labels, a title and axis labels, zoomed by a scale. Abstract (FormatXValues/FormatYValues are pure; slots
// 0x9c/0xa0 are __purecall in the vtable 0x10099634); UILineGraph draws the series. sizeof 0x230 (copy ctor).
// The inline members are emitted first by UIControlsTable.cpp (0x10066acb.., declaration order: GetGraphRectangle ..
// SetZoomIncrement, then the implicit copy ctor 0x10066b74 and operator= 0x10066d98). UIGraph.cpp
// (0x10075b7c..0x10076f27: key chains, then the ctor 0x10075c9c) holds the rest.
// Requires lle/TMPoll.h, lle/UGraph.h, lle/UIControl.h.
#ifndef LLE_UIGRAPH_H
#define LLE_UIGRAPH_H

class GEFont;
class GEText;

// Provisional name (not exported; a free inline). UIGraph rounds grid division counts up and uses the result as a
// float: `fistp r; fild r; fcomp v; jae; inc r`, then `fild r` at every use (a float return: the long UMCeil alone
// gives `fidiv` and integer loop compares). Wrapping UMCeil or repeating its asm body compiles the same.
inline float UMCeilFloat(float inValue)
{
    return UMCeil(inValue);
}

class LLE_API UIGraph : public UIControl {
    friend struct UIGraphRust;              // UIGraph.cpp: the Rust layout's and draws' view (ee_ui graph.rs)
    friend struct UILineGraphRust;          // UILineGraph.cpp: the Rust series draws' view
public:
    UIGraph(unsigned long inId, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
            unsigned long inBorderThickness);
    virtual ~UIGraph();
    virtual long CopyData();
    virtual long DrawPolygons();
    virtual long DrawTextA();
    // new virtuals in vtable order (slots 0x88..0xb4)
    virtual long DrawXAxisText();
    virtual long DrawYAxisText();
    virtual long DrawGridText();
    virtual long DrawTitleText();
    virtual const UGraph& GetGraph() const { return this->mGraph; }  // ICF-folded (0x100596b2)
    virtual void FormatYValues(float inValue, UWideString& outString) = 0;  // slot 0x9c
    virtual void FormatXValues(float inValue, UWideString& outString) = 0;  // slot 0xa0
    const GERectangle& GetGraphRectangle() { return this->mGraphRectangle; }
    float GetLineOpacity() { return this->mLineOpacity; }
    const GE2DPoint& GetZoomLocation() { return this->mZoomLocation; }
    void SetDrawGrid(bool inDrawGrid) { this->mDrawGrid = inDrawGrid; }
    virtual void SetFont(GEFont* inFont);
    void SetFontColor(const GEColor& inColor);
    virtual void SetGraph(const UGraph& inGraph) { this->mGraph = inGraph; }
    void SetLabelGrid(bool inLabelGrid) { this->mLabelGrid = inLabelGrid; }
    void SetLabelXAxis(bool inLabelXAxis) { this->mLabelXAxis = inLabelXAxis; }
    void SetLabelYAxis(bool inLabelYAxis) { this->mLabelYAxis = inLabelYAxis; }
    void SetLabelGraph(bool inLabelGraph) { this->mLabelGraph = inLabelGraph; }
    void SetLineOpacity(float inOpacity) { this->mLineOpacity = inOpacity; }
    void SetNumberOfGridDivisionsX(unsigned long inDivisions) { this->mNumberOfGridDivisionsX = inDivisions; }
    void SetNumberOfGridDivisionsY(unsigned long inDivisions) { this->mNumberOfGridDivisionsY = inDivisions; }
    void SetZoomScale(float inScale) { this->mZoomScale = inScale; }
    void SetZoomIncrement(float inIncrement) { this->mZoomIncrement = inIncrement; }
    virtual long UpdateMouse(const GE2DPoint& inLocation);       // slot 0xac (hides UIControl::UpdateMouse)
    virtual void ZoomIn(const GE2DPoint& inLocation);
    virtual void ZoomOut();
    // UIControl's pure handlers; the inline bodies are ICF-folded (0x10032e79)
    virtual long MouseLButtonDown() { return kErrorOK; }
    virtual long MouseLButtonUp() { return kErrorOK; }
    virtual long MouseMove(const GE2DPoint& inLocation);
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void TabbedTo();
    virtual void TabbedFrom();
    virtual long UpdateKeyboard() { return kErrorOK; }
protected:
    TMPoll mPoll;                           // +0xa0 (2000, 0, false); not used by UIGraph.cpp
    UGraph mGraph;                          // +0xc0
    UGraph mCopiedGraph;                    // +0x11c
    GEText* mTitleText;                     // +0x178 (owned; created by SetFont)
    GEText* mCopiedTitleText;               // +0x17c
    GEText* mXAxisText;                     // +0x180
    GEText* mCopiedXAxisText;               // +0x184
    GEText* mYAxisText;                     // +0x188
    GEText* mCopiedYAxisText;               // +0x18c
    GEText* mGridText;                      // +0x190 (grid value labels; no copy)
    float mZoomScale;                       // +0x194
    float mCopiedZoomScale;                 // +0x198
    unsigned long mNumberOfGridDivisionsX;  // +0x19c
    unsigned long mNumberOfGridDivisionsY;  // +0x1a0
    long mMinX;                             // +0x1a4 the displayed range (the graph's range over the zoom scale)
    long mMaxX;                             // +0x1a8
    long mMinY;                             // +0x1ac
    long mMaxY;                             // +0x1b0
    GE2DPoint mZoomLocation;                // +0x1b4
    GEColor mGridColor;                     // +0x1bc (the title's foreground colour as of CopyData)
    GERectangle mGraphRectangle;            // +0x1c8
    GERectangle mCopiedGraphRectangle;      // +0x1d8
    GERectangle mTitleRectangle;            // +0x1e8
    GERectangle mXAxisRectangle;            // +0x1f8
    GERectangle mYAxisRectangle;            // +0x208
    float mLineOpacity;                     // +0x218
    float mCopiedLineOpacity;               // +0x21c
    float mZoomIncrement;                   // +0x220
    bool mDrawGrid;                         // +0x224
    bool mCopiedDrawGrid;                   // +0x225
    bool mLabelGrid;                        // +0x226
    bool mCopiedLabelGrid;                  // +0x227
    bool mLabelXAxis;                       // +0x228
    bool mCopiedLabelXAxis;                 // +0x229
    bool mLabelYAxis;                       // +0x22a
    bool mCopiedLabelYAxis;                 // +0x22b
    bool mLabelGraph;                       // +0x22c
    bool mCopiedLabelGraph;                 // +0x22d
};

#endif
