// C:\EEX\Low-Level Engine\User Interface\UILineGraph.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100785cd..0x10078a70: the U2D key init chains, then the ctor 0x100786ed, dtor, DrawPolygons, DrawSeries and
// FormatXValues/FormatYValues (ICF-folded). No template COMDATs; the implicit copy members and ??_E are emitted by
// UIControlsTable.cpp. UILBString.cpp ends at 0x100785cd; UILRTiledBackground.cpp starts at 0x10078a70.
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
#include "lle/UILineGraph.h"

// FUNCTION: LLE 0x100786ed
// Creates a line graph control on the form (plain graph setup)
UILineGraph::UILineGraph(unsigned long inId, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                         unsigned long inBorderThickness)
    : UIGraph(inId, inRectangle, inForm, inIndex, inBorderThickness)
{
}

// FUNCTION: LLE 0x10078714
// Destroys the line graph (members only)
UILineGraph::~UILineGraph()
{
}

// FUNCTION: LLE 0x1007871f
// Draws the graph frame, then every enabled series with at least two points; returns the last draw result
long UILineGraph::DrawPolygons()
{
    long theResult = UIGraph::DrawPolygons();
    for (unsigned long theIndex = 0; theIndex < this->mCopiedGraph.GetSeriesCount(); theIndex++)
        if (this->mCopiedGraph.GetSeries(theIndex).GetIsEnabled() &&
            this->mCopiedGraph.GetSeries(theIndex).GetDataPointCount() > 1)
            theResult = this->DrawSeries(this->mCopiedGraph.GetSeries(theIndex));
    return theResult;
}

// FUNCTION: LLE 0x10078793
// Clips the segment (x0, y0)-(x1, y1) to the box [inMinX, inMaxX] x [inMinY, inMaxY] (Liang-Barsky); false when
// nothing of it is inside
static bool UILGClipSegment(float& ioX0, float& ioY0, float& ioX1, float& ioY1, float inMinX, float inMaxX,
                            float inMinY, float inMaxY)
{
    float theDX = ioX1 - ioX0;
    float theDY = ioY1 - ioY0;
    float theP[4] = { -theDX, theDX, -theDY, theDY };
    float theQ[4] = { ioX0 - inMinX, inMaxX - ioX0, ioY0 - inMinY, inMaxY - ioY0 };
    float theT0 = 0.0f;
    float theT1 = 1.0f;
    for (int i = 0; i < 4; i++) {
        if (theP[i] == 0.0f) {
            if (theQ[i] < 0.0f)
                return false;
        } else {
            float theT = theQ[i] / theP[i];
            if (theP[i] < 0.0f) {
                if (theT > theT1)
                    return false;
                if (theT > theT0)
                    theT0 = theT;
            } else {
                if (theT < theT0)
                    return false;
                if (theT < theT1)
                    theT1 = theT;
            }
        }
    }
    float theX0 = ioX0;
    float theY0 = ioY0;
    ioX0 = theX0 + theT0 * theDX;
    ioY0 = theY0 + theT0 * theDY;
    ioX1 = theX0 + theT1 * theDX;
    ioY1 = theY0 + theT1 * theDY;
    return true;
}

// Draws a series as line segments scaled into the graph area, clipped to the displayed range
// (skips a vertical segment at or left of the series' minimum X)
long UILineGraph::DrawSeries(const UGraphSeries& inSeries)
{
    GE2DPoint theStart;
    GE2DPoint theEnd;
    GELine theLine;
    GERectangle theBounds(this->mMinX, this->mMaxY, this->mMaxX, this->mMinY);
    float theLeft = this->mGraphRectangle.GetLeft();
    float theBottom = this->mGraphRectangle.GetBottom();
    long theResult = kErrorOK;
    unsigned long theRangeX = this->mCopiedGraph.GetMaxX() - this->mCopiedGraph.GetMinX();
    if (theRangeX == 0)
        theRangeX = 1;
    unsigned long theRangeY = this->mCopiedGraph.GetMaxY() - this->mCopiedGraph.GetMinY();
    if (theRangeY == 0)
        theRangeY = 1;
    float theScaleX = this->mGraphRectangle.GetWidth() / theRangeX;
    float theScaleY = this->mGraphRectangle.GetHeight() / theRangeY;
    theLine.SetColor(inSeries.GetColor());
    for (unsigned long theIndex = 1; theIndex < inSeries.GetDataPointCount(); theIndex++) {
        unsigned long theStartX = inSeries.GetDataPoint(theIndex - 1).GetX();
        unsigned long theStartY = inSeries.GetDataPoint(theIndex - 1).GetY();
        unsigned long theEndX = inSeries.GetDataPoint(theIndex).GetX();
        unsigned long theEndY = inSeries.GetDataPoint(theIndex).GetY();
        theStart = GE2DPoint((float)theStartX, (float)theStartY);
        theEnd = GE2DPoint((float)theEndX, (float)theEndY);
        if (theStartX == theEndX && !(theStartX > inSeries.GetMinX()))
            continue;
        // Fixed: the three clip tests here had empty bodies, so segments outside the displayed range were drawn
        // outside the graph; the segment is now clipped to the range (and skipped when wholly outside it)
        float theX0 = (float)(long)theStartX;
        float theY0 = (float)(long)theStartY;
        float theX1 = (float)(long)theEndX;
        float theY1 = (float)(long)theEndY;
        if (!UILGClipSegment(theX0, theY0, theX1, theY1, (float)this->mMinX, (float)this->mMaxX, (float)this->mMinY,
                             (float)this->mMaxY))
            continue;
        float theMinX = (float)(long)this->mCopiedGraph.GetMinX();
        float theMinY = (float)(long)this->mCopiedGraph.GetMinY();
        theLine.SetStartPoint(GE2DPoint((theX0 - theMinX) * theScaleX + theLeft, theBottom - (theY0 - theMinY) * theScaleY));
        theLine.SetEndPoint(GE2DPoint((theX1 - theMinX) * theScaleX + theLeft, theBottom - (theY1 - theMinY) * theScaleY));
        theResult = this->GetRasterizer()->Draw(&theLine, this->GetScreenIndex());
    }
    return theResult;
}

// FUNCTION: LLE 0x10078a53
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

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x100785cd
// SYNTHETIC: LLE 0x100785d7
// SYNTHETIC: LLE 0x100785e7
// SYNTHETIC: LLE 0x100785f3
// SYNTHETIC: LLE 0x100785fd
// SYNTHETIC: LLE 0x10078607
// SYNTHETIC: LLE 0x10078617
// SYNTHETIC: LLE 0x10078623
// SYNTHETIC: LLE 0x1007862d
// SYNTHETIC: LLE 0x10078637
// SYNTHETIC: LLE 0x10078647
// SYNTHETIC: LLE 0x10078653
// SYNTHETIC: LLE 0x1007865d
// SYNTHETIC: LLE 0x10078667
// SYNTHETIC: LLE 0x10078677
// SYNTHETIC: LLE 0x10078683
// SYNTHETIC: LLE 0x1007868d
// SYNTHETIC: LLE 0x10078697
// SYNTHETIC: LLE 0x100786a7
// SYNTHETIC: LLE 0x100786b3
// SYNTHETIC: LLE 0x100786bd
// SYNTHETIC: LLE 0x100786c7
// SYNTHETIC: LLE 0x100786d7
// SYNTHETIC: LLE 0x100786e3
