// C:\EEX\Low-Level Engine\User Interface\UIGraph.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10075b7c..0x10076f27: the U2D key init chains, then the ctor 0x10075c9c and the out-of-line members in source
// order (no template COMDATs). The inline members, the implicit copy ctor/operator= and ??_E are emitted by
// UIControlsTable.cpp. UIHotKey.cpp starts at 0x10076f27.
// Not exact: DrawPolygons (98%: the original packs the per-line offset temp into theResult's slot [ebp-8], ours
// into the loop's UMCeilFloat parameter slot); DrawGridText (70%: the original refuses GetTopLeft's GE2DPoint copy
// ctor in the column loop, like UIFullBackground::DrawBackground, plus frame-slot differences).
#include <windows.h>

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
const float kMaximumZoomScale = 4.0f;
const float kTextSpacing = 5.0f;
const float kDefaultLineOpacity = 0.25f;

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
    UWideString theString;
    float theValueHeight = 0.0f;
    float theValueWidth = 0.0f;
    float theWidth;
    float theHeight;
    this->mGraphRectangle = this->mRectangle;
    float theCenterX = this->mGraphRectangle.GetWidth() * 0.5f + this->mGraphRectangle.GetLeft();
    float theCenterY = this->mGraphRectangle.GetHeight() * 0.5f + this->mGraphRectangle.GetTop();
    if (this->mLabelGraph && this->mTitleText) {
        this->mTitleText->SetText(this->mGraph.GetTitle());
        theWidth = this->mTitleText->GetTextExtent().GetWidth();
        theHeight = this->mTitleText->GetTextExtent().GetHeight();
        this->mTitleRectangle.SetLeft(theCenterX - theWidth * 0.5f);
        this->mTitleRectangle.SetRight(this->mTitleRectangle.GetLeft() + theWidth);
        this->mTitleRectangle.SetTop(this->mGraphRectangle.GetTop() + kTextSpacing);
        this->mTitleRectangle.SetBottom(this->mTitleRectangle.GetTop() + theHeight);
    } else
        this->mTitleRectangle = GERectangle();
    if (this->mLabelGrid && this->mGridText) {
        // Fixed: the margins used to be measured from FormatYValues(mMaxX) and FormatXValues(mMaxY), axes and
        // formatters swapped
        this->FormatXValues((float)this->mMaxX, theString);
        this->mGridText->SetText(theString);
        theValueHeight = this->mGridText->GetTextExtent().GetHeight();
        this->FormatYValues((float)this->mMaxY, theString);
        this->mGridText->SetText(theString);
        theValueWidth = this->mGridText->GetTextExtent().GetWidth() + kTextSpacing * 2;
    }
    if (this->mLabelYAxis && this->mYAxisText) {
        this->mYAxisText->SetText(this->mGraph.GetYLabel());
        theWidth = this->mYAxisText->GetTextExtent().GetWidth();
        theHeight = this->mYAxisText->GetTextExtent().GetHeight();
        this->mYAxisRectangle.SetLeft(this->mGraphRectangle.GetLeft() + kTextSpacing);
        this->mYAxisRectangle.SetRight(this->mYAxisRectangle.GetLeft() + theWidth);
        this->mYAxisRectangle.SetTop(theCenterY - theHeight * 0.5f);
        this->mYAxisRectangle.SetBottom(this->mYAxisRectangle.GetTop() + theHeight);
    } else
        this->mYAxisRectangle = GERectangle();
    if (this->mLabelXAxis && this->mXAxisText) {
        this->mXAxisText->SetText(this->mGraph.GetXLabel());
        theWidth = this->mXAxisText->GetTextExtent().GetWidth();
        theHeight = this->mXAxisText->GetTextExtent().GetHeight();
        this->mXAxisRectangle.SetLeft(theCenterX - theWidth * 0.5f);
        this->mXAxisRectangle.SetRight(this->mXAxisRectangle.GetLeft() + theWidth);
        this->mXAxisRectangle.SetBottom(this->mGraphRectangle.GetBottom() - kTextSpacing);
        this->mXAxisRectangle.SetTop(this->mXAxisRectangle.GetBottom() - theHeight);
    } else
        this->mXAxisRectangle = GERectangle();
    this->mGridColor = this->mTitleText->GetFgColor();
    this->mGraphRectangle.SetLeft(this->mRectangle.GetLeft() + (this->mYAxisRectangle.GetWidth() + kTextSpacing * 2 + theValueWidth));
    this->mGraphRectangle.SetRight(this->mRectangle.GetRight() - (kTextSpacing * 2 + theValueWidth));
    this->mGraphRectangle.SetTop(this->mRectangle.GetTop() + (this->mTitleRectangle.GetHeight() + kTextSpacing * 2));
    this->mGraphRectangle.SetBottom(this->mRectangle.GetBottom() -
                              (this->mXAxisRectangle.GetHeight() + kTextSpacing * 2 + theValueHeight));
    this->SetBgRectangle(this->mGraphRectangle);
    if (UMFloor(this->mZoomScale) == 1) {
        this->mMaxX = this->mGraph.GetMaxX();
        this->mMaxY = this->mGraph.GetMaxY();
        this->mMinX = this->mGraph.GetMinX();
        this->mMinY = this->mGraph.GetMinY();
    }
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
    GERectangle theRectangle;
    UWideString theString;
    long theResult = kErrorOK;
    float theStep = this->mCopiedGraphRectangle.GetWidth() / UMCeilFloat(this->mNumberOfGridDivisionsX / this->mCopiedZoomScale);
    float theValueStep = (this->mMaxX - this->mMinX) / (this->mNumberOfGridDivisionsX / this->mCopiedZoomScale);
    float theValue = 0.0f;
    for (unsigned long theColumn = 0; theColumn <= UMCeilFloat(this->mNumberOfGridDivisionsX / this->mCopiedZoomScale);
         theColumn++) {
        float theLabel = this->mMinX + theValue;
        if (theLabel > this->mMaxX)
            theLabel = this->mMaxX;
        theValue += theValueStep;
        this->FormatXValues(theLabel, theString);
        this->mGridText->SetText(theString);
        theRectangle = GERectangle(GE2DPoint(theColumn * theStep + this->mCopiedGraphRectangle.GetLeft(),
                                             this->mCopiedGraphRectangle.GetBottom()),
                                   this->mGridText->GetTextExtent().GetWidth(), this->mGridText->GetTextExtent().GetHeight());
        this->mGridText->SetLocation(theRectangle.GetTopLeft());
        theResult = this->GetForm()->DrawClippedText(theRectangle, this->mGridText, this);
    }
    theStep = this->mCopiedGraphRectangle.GetHeight() / UMCeilFloat(this->mNumberOfGridDivisionsY / this->mCopiedZoomScale);
    theValueStep = (this->mMaxY - this->mMinY) / (this->mNumberOfGridDivisionsY / this->mCopiedZoomScale);
    theValue = 0.0f;
    for (unsigned long theRow = 0; theRow < UMCeilFloat(this->mNumberOfGridDivisionsY / this->mCopiedZoomScale); theRow++) {
        float theLabel = this->mMaxY - theValue;
        if (theLabel > this->mMaxY)
            theLabel = this->mMaxY;
        theValue += theValueStep;
        this->FormatYValues(theLabel, theString);
        this->mGridText->SetText(theString);
        float theX = this->mCopiedGraphRectangle.GetLeft() - this->mGridText->GetTextExtent().GetWidth();
        float theOffset = theRow * theStep;
        float theY = this->mCopiedGraphRectangle.GetTop() + theOffset;
        theRectangle = GERectangle(theX, theY, this->mCopiedGraphRectangle.GetLeft(),
                                   this->mCopiedGraphRectangle.GetTop() + this->mGridText->GetTextExtent().GetHeight() + theOffset);
        this->mGridText->SetLocation(theX, theY);
        theResult = this->GetForm()->DrawClippedText(theRectangle, this->mGridText, this);
    }
    return theResult;
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
    if (theThickness > 1)
        theResult = this->DrawBorder(this->mCopiedGraphRectangle, theThickness - 1);
    if (this->mCopiedDrawGrid) {
        theLine.SetColor(this->mGridColor);
        theLine.SetOpacity(this->mCopiedLineOpacity);
        float theStep = this->mCopiedGraphRectangle.GetWidth() / UMCeilFloat(this->mNumberOfGridDivisionsX / this->mCopiedZoomScale);
        for (unsigned long theColumn = 0; theColumn <= UMCeilFloat(this->mNumberOfGridDivisionsX / this->mCopiedZoomScale);
             theColumn++) {
            float theX = theColumn * theStep;
            GE2DPoint theStart(this->mCopiedGraphRectangle.GetLeft() + theX, this->mCopiedGraphRectangle.GetTop());
            theLine.SetStartPoint(theStart);
            GE2DPoint theEnd(this->mCopiedGraphRectangle.GetLeft() + theX, this->mCopiedGraphRectangle.GetBottom());
            theLine.SetEndPoint(theEnd);
            theResult = this->GetRasterizer()->Draw(&theLine, this->GetScreenIndex());
        }
        theStep = this->mCopiedGraphRectangle.GetHeight() / UMCeilFloat(this->mNumberOfGridDivisionsY / this->mCopiedZoomScale);
        for (unsigned long theRow = 0; theRow <= UMCeilFloat(this->mNumberOfGridDivisionsY / this->mCopiedZoomScale);
             theRow++) {
            GE2DPoint theStart(this->mCopiedGraphRectangle.GetLeft(), this->mCopiedGraphRectangle.GetTop() + theRow * theStep);
            theLine.SetStartPoint(theStart);
            GE2DPoint theEnd(this->mCopiedGraphRectangle.GetRight(), this->mCopiedGraphRectangle.GetTop() + theRow * theStep);
            theLine.SetEndPoint(theEnd);
            theResult = this->GetRasterizer()->Draw(&theLine, this->GetScreenIndex());
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x10076acc
// Draws the title, axis labels and grid values that are switched on
long UIGraph::DrawTextA()
{
    this->mDrawCount--;
    long theResult = kErrorOK;
    if (this->mCopiedLabelGraph)
        theResult = this->DrawTitleText();
    if (this->mCopiedLabelXAxis)
        theResult = this->DrawXAxisText();
    if (this->mCopiedLabelYAxis)
        theResult = this->DrawYAxisText();
    if (this->mCopiedLabelGrid)
        theResult = this->DrawGridText();
    return theResult;
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
    this->mZoomScale = this->mZoomIncrement + this->mZoomScale;
    this->mZoomLocation = inLocation;
    if (this->mZoomScale > kMaximumZoomScale)
        this->mZoomScale = kMaximumZoomScale;
    this->mMaxX = UMFloor(this->mGraph.GetMaxX() / this->mZoomScale);
    this->mMaxY = UMFloor(this->mGraph.GetMaxY() / this->mZoomScale);
    this->mMinX = UMFloor(this->mGraph.GetMinX() / this->mZoomScale);
    this->mMinY = UMFloor(this->mGraph.GetMinY() / this->mZoomScale);
    this->SetDirty(true);
}

// FUNCTION: LLE 0x10076e44
// Zooms out one step (at least 1x) and widens the visible value range
void UIGraph::ZoomOut()
{
    this->mZoomScale -= this->mZoomIncrement;
    if (this->mZoomScale < 1.0f)
        this->mZoomScale = 1.0f;
    this->mMaxX = UMFloor(this->mGraph.GetMaxX() / this->mZoomScale);
    this->mMaxY = UMFloor(this->mGraph.GetMaxY() / this->mZoomScale);
    this->mMinX = UMFloor(this->mGraph.GetMinX() / this->mZoomScale);
    this->mMinY = UMFloor(this->mGraph.GetMinY() / this->mZoomScale);
    this->SetDirty(true);
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
