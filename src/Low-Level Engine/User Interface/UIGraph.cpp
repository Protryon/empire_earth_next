// C:\EEX\Low-Level Engine\User Interface\UIGraph.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10075b7c..0x10076f27: the U2D key init chains, then the ctor 0x10075c9c and the out-of-line members in source
// order (no template COMDATs). The inline members, the implicit copy ctor/operator= and ??_E are emitted by
// UIControlsTable.cpp. UIHotKey.cpp starts at 0x10076f27.
// Not exact: DrawPolygons (98%: the original packs the per-line offset temp into theResult's slot [ebp-8], ours
// into the loop's UMCeilFloat parameter slot); DrawGridText (70%: the original refuses GetTopLeft's GE2DPoint copy
// ctor in the column loop, like UIFullBackground::DrawBackground, plus frame-slot differences).
//
// The arithmetic and the decisions are Rust (rust/crates/ee_ui/src/graph.rs, docs/RUST_PLAN.md R6): CopyData's
// layout and range, the zoom steps, the grid lines, the inner border, the grid values' places and which labels draw.
// The texts (the virtual formats), the line, every draw and the copies stay here (UIGraphRust below).
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

const float kDefaultZoomIncrement = 1.0f;
const float kDefaultLineOpacity = 0.25f;

// GERectangle <-> the Rust record (EeUiRect: bottom, left, right, top)
static inline EeUiRect UIGraphRect(const GERectangle& r) { return EeUiRect{r.GetBottom(), r.GetLeft(), r.GetRight(), r.GetTop()}; }
static inline GERectangle UIGraphRect(const EeUiRect& r) { return GERectangle(r.left, r.top, r.right, r.bottom); }

// The Rust graph's questions and draws (ee_ui graph.rs): the texts, the grid values, the lines and the virtual draws
struct UIGraphRust {
    static UIGraph* Graph(void* inGraph) { return static_cast<UIGraph*>(inGraph); }
    static GEText* Text(UIGraph* inGraph, int inWhich)
    {
        return inWhich == 0 ? inGraph->mTitleText : inWhich == 1 ? inGraph->mYAxisText : inGraph->mXAxisText;
    }
    static const UWideString& Label(UIGraph* inGraph, int inWhich)
    {
        return inWhich == 0 ? inGraph->mGraph.GetTitle() : inWhich == 1 ? inGraph->mGraph.GetYLabel() : inGraph->mGraph.GetXLabel();
    }
    static EeUiGraphRange Range(const UGraph& inGraph)
    {
        return EeUiGraphRange{inGraph.GetMinX(), inGraph.GetMaxX(), inGraph.GetMinY(), inGraph.GetMaxY()};
    }
    static EeUiGraphRange Displayed(const UIGraph* inGraph)
    {
        return EeUiGraphRange{inGraph->mMinX, inGraph->mMaxX, inGraph->mMinY, inGraph->mMaxY};
    }
    static void SetDisplayed(UIGraph* inGraph, const EeUiGraphRange& inRange)
    {
        inGraph->mMaxX = inRange.max_x;
        inGraph->mMaxY = inRange.max_y;
        inGraph->mMinX = inRange.min_x;
        inGraph->mMinY = inRange.min_y;
    }
    static void Zoom(UIGraph* inGraph, bool inIn)
    {
        EeUiGraphRange theRange;
        inGraph->mZoomScale = ee_ui_graph_zoom(inIn, inGraph->mZoomScale, inGraph->mZoomIncrement, Range(inGraph->mGraph), &theRange);
        SetDisplayed(inGraph, theRange);
        inGraph->SetDirty(true);
    }
    static void GridValue(UIGraph* inGraph, int inAxis, float inValue, float* outWidth, float* outHeight)
    {
        UWideString theString;
        if (inAxis == 0)
            inGraph->FormatXValues(inValue, theString);
        else
            inGraph->FormatYValues(inValue, theString);
        inGraph->mGridText->SetText(theString);
        *outWidth = inGraph->mGridText->GetTextExtent().GetWidth();
        *outHeight = inGraph->mGridText->GetTextExtent().GetHeight();
    }
    static long DrawGridValue(UIGraph* inGraph, const GERectangle& inRectangle, float inX, float inY)
    {
        inGraph->mGridText->SetLocation(inX, inY);
        return inGraph->GetForm()->DrawClippedText(inRectangle, inGraph->mGridText, inGraph);
    }
    static long Border(UIGraph* inGraph, const GERectangle& inRectangle, unsigned long inThickness)
    {
        return inGraph->DrawBorder(inRectangle, inThickness);
    }
    static long DrawText(UIGraph* inGraph, int inWhich)
    {
        switch (inWhich) {
        case 0:
            return inGraph->DrawTitleText();
        case 1:
            return inGraph->DrawXAxisText();
        case 2:
            return inGraph->DrawYAxisText();
        default:
            return inGraph->DrawGridText();
        }
    }
};

extern "C" void ee_uicpp_graph_text(void* inGraph, int inWhich, float* outWidth, float* outHeight)
{
    UIGraph* theGraph = UIGraphRust::Graph(inGraph);
    GEText* theText = UIGraphRust::Text(theGraph, inWhich);
    theText->SetText(UIGraphRust::Label(theGraph, inWhich));
    *outWidth = theText->GetTextExtent().GetWidth();
    *outHeight = theText->GetTextExtent().GetHeight();
}
extern "C" void ee_uicpp_graph_grid_value(void* inGraph, int inAxis, float inValue, float* outWidth, float* outHeight)
{
    UIGraphRust::GridValue(UIGraphRust::Graph(inGraph), inAxis, inValue, outWidth, outHeight);
}
extern "C" long ee_uicpp_graph_draw_grid_value(void* inGraph, const EeUiRect* inRectangle, float inX, float inY)
{
    return UIGraphRust::DrawGridValue(UIGraphRust::Graph(inGraph), UIGraphRect(*inRectangle), inX, inY);
}
extern "C" long ee_uicpp_graph_line(void* inGraph, void* inLine, float inX0, float inY0, float inX1, float inY1)
{
    UIGraph* theGraph = UIGraphRust::Graph(inGraph);
    GELine* theLine = static_cast<GELine*>(inLine);
    GE2DPoint theStart(inX0, inY0);
    theLine->SetStartPoint(theStart);
    GE2DPoint theEnd(inX1, inY1);
    theLine->SetEndPoint(theEnd);
    return theGraph->GetRasterizer()->Draw(theLine, theGraph->GetScreenIndex());
}
extern "C" long ee_uicpp_graph_border(void* inGraph, const EeUiRect* inRectangle, unsigned long inThickness)
{
    return UIGraphRust::Border(UIGraphRust::Graph(inGraph), UIGraphRect(*inRectangle), inThickness);
}
extern "C" long ee_uicpp_graph_draw_text(void* inGraph, int inWhich) { return UIGraphRust::DrawText(UIGraphRust::Graph(inGraph), inWhich); }

// FUNCTION: LLE 0x10075c9c
// Creates a graph control with a 15 x 10 grid, every label on, zoom 1 and no fonts yet
UIGraph::UIGraph(unsigned long inId, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                 unsigned long inBorderThickness)
    : UIControl(kControlTypeGraph, inId, inRectangle, inForm, inIndex, inBorderThickness), mPoll(2000, 0, false)
{
    this->mTitleText = NULL;
    this->mCopiedTitleText = NULL;
    this->mXAxisText = NULL;
    this->mCopiedXAxisText = NULL;
    this->mYAxisText = NULL;
    this->mCopiedYAxisText = NULL;
    this->mGridText = NULL;
    this->mGraphRectangle = inRectangle;
    this->mCopiedGraphRectangle = inRectangle;
    this->mLineOpacity = kDefaultLineOpacity;
    this->mCopiedLineOpacity = kDefaultLineOpacity;
    this->mZoomScale = 1.0f;
    this->mCopiedZoomScale = 1.0f;
    this->mZoomIncrement = kDefaultZoomIncrement;
    this->mNumberOfGridDivisionsX = 15;
    this->mNumberOfGridDivisionsY = 10;
    this->mMinX = 0;
    this->mMaxX = 10;
    this->mMinY = 0;
    this->mMaxY = 10;
    this->mDrawGrid = true;
    this->mCopiedDrawGrid = true;
    this->mLabelGrid = true;
    this->mCopiedLabelGrid = true;
    this->mLabelXAxis = true;
    this->mCopiedLabelXAxis = true;
    this->mLabelYAxis = true;
    this->mCopiedLabelYAxis = true;
    this->mLabelGraph = true;
    this->mCopiedLabelGraph = true;
}

// FUNCTION: LLE 0x10075eb8
// Destroys the graph's title, axis and grid texts
UIGraph::~UIGraph()
{
    if (this->mTitleText) {
        delete this->mTitleText;
        delete this->mCopiedTitleText;
    }
    if (this->mXAxisText) {
        delete this->mXAxisText;
        delete this->mCopiedXAxisText;
    }
    if (this->mYAxisText) {
        delete this->mYAxisText;
        delete this->mCopiedYAxisText;
    }
    if (this->mGridText)
        delete this->mGridText;
}

// FUNCTION: LLE 0x10075f8a
// Lays out the title, axis labels and grid-value margins around the plot area and copies the graph state for
// drawing; needs SetFont first (dereferences the texts unchecked)
long UIGraph::CopyData()
{
    this->mGraphRectangle = this->mRectangle;
    EeUiGraphTexts theTexts = {this->mLabelGraph, this->mTitleText != NULL, this->mLabelGrid, this->mGridText != NULL,
                               this->mLabelYAxis, this->mYAxisText != NULL, this->mLabelXAxis, this->mXAxisText != NULL};
    EeUiGraphLayout theLayout = ee_ui_graph_copy_layout(this, UIGraphRect(this->mRectangle), theTexts, this->mMaxX,
                                                        this->mMaxY);
    this->mTitleRectangle = UIGraphRect(theLayout.title);
    this->mYAxisRectangle = UIGraphRect(theLayout.y_axis);
    this->mXAxisRectangle = UIGraphRect(theLayout.x_axis);
    this->mGridColor = this->mTitleText->GetFgColor();
    this->mGraphRectangle = UIGraphRect(theLayout.graph);
    this->SetBgRectangle(this->mGraphRectangle);
    UIGraphRust::SetDisplayed(this, ee_ui_graph_copy_range(this->mZoomScale, UIGraphRust::Displayed(this),
                                                           UIGraphRust::Range(this->mGraph)));
    *this->mCopiedTitleText = *this->mTitleText;
    *this->mCopiedXAxisText = *this->mXAxisText;
    *this->mCopiedYAxisText = *this->mYAxisText;
    this->mCopiedGraph = this->mGraph;
    this->mCopiedGraphRectangle = this->mGraphRectangle;
    this->mCopiedZoomScale = this->mZoomScale;
    this->mCopiedLineOpacity = this->mLineOpacity;
    this->mCopiedDrawGrid = this->mDrawGrid;
    this->mCopiedLabelGrid = this->mLabelGrid;
    this->mCopiedLabelXAxis = this->mLabelXAxis;
    this->mCopiedLabelYAxis = this->mLabelYAxis;
    this->mCopiedLabelGraph = this->mLabelGraph;
    return UIControl::CopyData();
}

// FUNCTION: LLE 0x10076430
// Draws the x-axis label
long UIGraph::DrawXAxisText()
{
    this->mCopiedXAxisText->SetLocation(this->mXAxisRectangle.GetTopLeft());
    return this->GetForm()->DrawClippedText(this->mXAxisRectangle, this->mCopiedXAxisText, this);
}

// FUNCTION: LLE 0x10076472
// Draws the y-axis label
long UIGraph::DrawYAxisText()
{
    this->mCopiedYAxisText->SetLocation(this->mYAxisRectangle.GetTopLeft());
    return this->GetForm()->DrawClippedText(this->mYAxisRectangle, this->mCopiedYAxisText, this);
}

// FUNCTION: LLE 0x100764b4
// Draws the grid values below each column line and left of each row line (the bottom row stays unlabelled)
long UIGraph::DrawGridText()
{
    return ee_ui_graph_draw_grid_text(this, UIGraphRect(this->mCopiedGraphRectangle), this->mNumberOfGridDivisionsX,
                                      this->mNumberOfGridDivisionsY, this->mCopiedZoomScale, UIGraphRust::Displayed(this));
}

// FUNCTION: LLE 0x10076822
// Draws the graph title
long UIGraph::DrawTitleText()
{
    this->mCopiedTitleText->SetLocation(this->mTitleRectangle.GetTopLeft());
    return this->GetForm()->DrawClippedText(this->mTitleRectangle, this->mCopiedTitleText, this);
}

// FUNCTION: LLE 0x10076864
// Draws the control, an inner border around the plot area and the grid lines
long UIGraph::DrawPolygons()
{
    GELine theLine;
    unsigned long theThickness = this->mBorderThickness;
    long theResult = UIControl::DrawPolygons();
    theLine.SetColor(this->mGridColor);
    theLine.SetOpacity(this->mCopiedLineOpacity);
    return ee_ui_graph_draw_grid(this, &theLine, theResult, theThickness, UIGraphRect(this->mCopiedGraphRectangle),
                                 this->mCopiedDrawGrid, this->mNumberOfGridDivisionsX, this->mNumberOfGridDivisionsY,
                                 this->mCopiedZoomScale);
}

// FUNCTION: LLE 0x10076acc
// Draws the title, axis labels and grid values that are switched on
long UIGraph::DrawTextA()
{
    this->mDrawCount--;
    return ee_ui_graph_draw_texts(this, this->mCopiedLabelGraph, this->mCopiedLabelXAxis, this->mCopiedLabelYAxis,
                                  this->mCopiedLabelGrid);
}

// FUNCTION: LLE 0x10076b22
// Sets the font of every graph text, creating the texts on first use, and marks the graph dirty
void UIGraph::SetFont(GEFont* inFont)
{
    if (this->mTitleText == NULL) {
        this->mTitleText = new GEText(inFont);
        this->mCopiedTitleText = new GEText(inFont);
    } else {
        this->mTitleText->SetFont(inFont);
        this->mCopiedTitleText->SetFont(inFont);
    }
    if (this->mXAxisText == NULL) {
        this->mXAxisText = new GEText(inFont);
        this->mCopiedXAxisText = new GEText(inFont);
    } else {
        this->mXAxisText->SetFont(inFont);
        this->mCopiedXAxisText->SetFont(inFont);
    }
    if (this->mYAxisText == NULL) {
        this->mYAxisText = new GEText(inFont);
        this->mCopiedYAxisText = new GEText(inFont);
    } else {
        this->mYAxisText->SetFont(inFont);
        this->mCopiedYAxisText->SetFont(inFont);
    }
    if (this->mGridText == NULL)
        this->mGridText = new GEText(inFont);
    else
        this->mGridText->SetFont(inFont);
    this->SetDirty(true);
}

// FUNCTION: LLE 0x10076c78
// Sets the colour of every graph text (transparent background)
void UIGraph::SetFontColor(const GEColor& inColor)
{
    this->mTitleText->SetIsTransparent(true);
    this->mTitleText->SetFgColor(inColor);
    this->mXAxisText->SetIsTransparent(true);
    this->mXAxisText->SetFgColor(inColor);
    this->mYAxisText->SetIsTransparent(true);
    this->mYAxisText->SetFgColor(inColor);
    this->mGridText->SetIsTransparent(true);
    this->mGridText->SetFgColor(inColor);
}

// FUNCTION: LLE 0x10076d1c
// Marks the graph dirty
void UIGraph::TabbedTo()
{
    this->SetDirty(true);
}

// FUNCTION: LLE 0x10076d24
// Releases the mouse and keyboard and marks the graph dirty
void UIGraph::TabbedFrom()
{
    this->ReleaseCapturedMouse();
    this->ReleaseCapturedKeyboard();
    this->SetDirty(true);
}

// FUNCTION: LLE 0x10076d3e
// Ignores the mouse (MouseMove is the same code)
long UIGraph::UpdateMouse(const GE2DPoint& inLocation)
{
    return kErrorOK;
}

// MouseMove is identical and ICF-folded into UpdateMouse (both exports 0x10076d3e)
long UIGraph::MouseMove(const GE2DPoint& inLocation)
{
    return kErrorOK;
}

// FUNCTION: LLE 0x10076d43
// Zooms in one step (at most 4x) and shrinks the visible value range; the zoom location is stored but unused
void UIGraph::ZoomIn(const GE2DPoint& inLocation)
{
    this->mZoomLocation = inLocation;
    UIGraphRust::Zoom(this, true);
}

// FUNCTION: LLE 0x10076e44
// Zooms out one step (at least 1x) and widens the visible value range
void UIGraph::ZoomOut()
{
    UIGraphRust::Zoom(this, false);
}

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x10075b7c
// SYNTHETIC: LLE 0x10075b86
// SYNTHETIC: LLE 0x10075b96
// SYNTHETIC: LLE 0x10075ba2
// SYNTHETIC: LLE 0x10075bac
// SYNTHETIC: LLE 0x10075bb6
// SYNTHETIC: LLE 0x10075bc6
// SYNTHETIC: LLE 0x10075bd2
// SYNTHETIC: LLE 0x10075bdc
// SYNTHETIC: LLE 0x10075be6
// SYNTHETIC: LLE 0x10075bf6
// SYNTHETIC: LLE 0x10075c02
// SYNTHETIC: LLE 0x10075c0c
// SYNTHETIC: LLE 0x10075c16
// SYNTHETIC: LLE 0x10075c26
// SYNTHETIC: LLE 0x10075c32
// SYNTHETIC: LLE 0x10075c3c
// SYNTHETIC: LLE 0x10075c46
// SYNTHETIC: LLE 0x10075c56
// SYNTHETIC: LLE 0x10075c62
// SYNTHETIC: LLE 0x10075c6c
// SYNTHETIC: LLE 0x10075c76
// SYNTHETIC: LLE 0x10075c86
// SYNTHETIC: LLE 0x10075c92
