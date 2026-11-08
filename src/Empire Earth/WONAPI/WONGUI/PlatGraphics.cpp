// The WONAPI GUI's graphics context on the platform layer (WONLobbyPlatform.h): draws into the display context's
// software surface (WONSurface.h) with the results the GDI backend (MSGraphics.cpp) had: lines include both end
// points (Polyline with an extra one-pixel segment), rectangles cover x..x+width-1 (Rectangle), fills are solid,
// FillRectBlend is AlphaBlend with a constant alpha, polygons fill by the alternate rule with the pen's outline,
// ellipses lie in the box x..x+width-1, and the clip is the surface's (a DC's clip region). On a surface with a pixel
// scale k (DisplayContext::mPixelScale, WONLobbyPlatform.h) the same shapes cover the pixels of their logical
// pixels: a logical pixel x is the surface's pixels PlatGUIPixels(x, k) .. PlatGUIPixels(x + 1, k) - 1, lines and
// outlines are one logical pixel (about k pixels) wide; at k = 1 every result is GDI's as before.
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <math.h>
#include <stdlib.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPlatform.h"

namespace WONAPI {

PlatGraphics::PlatGraphics() : mSurface(NULL), mScale(1.0f) {}

PlatGraphics::~PlatGraphics() { this->Detach(); }

// Attaches to the context's surface; the colour starts black (MSGraphics' black pen and brush)
void PlatGraphics::Attach(DisplayContext* inContext)
{
    Graphics::Attach(inContext);
    this->mSurface = inContext->mSurface;
    this->mScale = inContext->mPixelScale > 0.0f ? inContext->mPixelScale : 1.0f;
    this->mColor = 0;
}

int PlatGraphics::PX(int v) const { return PlatGUIPixels(v, this->mScale); }

// The pixels of the logical rectangle (a width or height <= 0 covers nothing), within the clip
void PlatGraphics::FillLogicalRect(SurfaceBase* inSurface, int x, int y, int width, int height)
{
    if (width <= 0 || height <= 0)
        return;
    int theLeft = this->PX(x), theTop = this->PX(y);
    inSurface->FillRect(theLeft, theTop, this->PX(x + width) - theLeft, this->PX(y + height) - theTop);
}

// A line from logical pixel (x1, y1) to (x2, y2), both included, one logical pixel wide: straight lines are the
// rectangles of their pixels; another steps along its longer axis in surface pixels, each step a span of one
// logical pixel's size across it, centred on the line between the end pixels' centres
void PlatGraphics::ScaledLine(SurfaceBase* inSurface, int x1, int y1, int x2, int y2)
{
    if (y1 == y2) {
        this->FillLogicalRect(inSurface, (std::min)(x1, x2), y1, abs(x2 - x1) + 1, 1);
        return;
    }
    if (x1 == x2) {
        this->FillLogicalRect(inSurface, x1, (std::min)(y1, y2), 1, abs(y2 - y1) + 1);
        return;
    }
    float k = this->mScale;
    bool theXMajor = abs(x2 - x1) >= abs(y2 - y1);
    // along the major axis a (a1 -> a2), the minor axis b (b1 -> b2)
    int a1 = theXMajor ? x1 : y1, a2 = theXMajor ? x2 : y2, b1 = theXMajor ? y1 : x1, b2 = theXMajor ? y2 : x2;
    if (a2 < a1) {
        std::swap(a1, a2);
        std::swap(b1, b2);
    }
    int theStart = this->PX(a1), theEnd = this->PX(a2 + 1);
    for (int p = theStart; p < theEnd; p++) {
        float t = ((p + 0.5f) / k - 0.5f - (float)a1) / (float)(a2 - a1);
        t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
        float b = (float)b1 + t * (float)(b2 - b1);   // the logical pixel's top (or left) at this step
        int theFrom = (int)floorf(b * k + 0.5f), theTo = (int)floorf((b + 1.0f) * k + 0.5f);
        if (theTo <= theFrom)
            theTo = theFrom + 1;
        if (theXMajor)
            inSurface->FillRect(p, theFrom, 1, theTo - theFrom);
        else
            inSurface->FillRect(theFrom, p, theTo - theFrom, 1);
    }
}

void PlatGraphics::Detach()
{
    Graphics::Detach();
    if (this->mSurface == NULL)
        return;
    this->mSurface = NULL;
    this->mDisplayContext = NULL;
}

// The surface with the current colour (0xRRGGBB: GDI drops the top byte of a plain colour); NULL without one
SurfaceBase* PlatGraphics::PrepareSurface()
{
    if (this->mSurface == NULL || this->mSurface->GetPixelsBase() == NULL)
        return NULL;
    this->mSurface->SetColor(this->mColor & 0x00FFFFFF);
    return this->mSurface;
}

void PlatGraphics::SetColorHook() {}

// The pixel as a COLORREF (0x00BBGGRR): GDI's GetPixel, not swapped back, as the original returned it
DWORD PlatGraphics::GetPixelHook(int x, int y)
{
    if (this->mSurface == NULL)
        return 0;
    DWORD thePixel = this->mSurface->GetPixel(this->PX(x), this->PX(y));
    return ((thePixel & 0xff) << 16) | (thePixel & 0xff00) | ((thePixel >> 16) & 0xff);
}

void PlatGraphics::SetPixelHook(int x, int y, DWORD inColor)
{
    if (this->mSurface == NULL || this->mSurface->GetPixelsBase() == NULL)
        return;
    if (this->mScale == 1.0f) {
        this->mSurface->SetPixel(x, y, inColor & 0x00FFFFFF);
        return;
    }
    this->mSurface->SetColor(inColor & 0x00FFFFFF);
    this->FillLogicalRect(this->mSurface, x, y, 1, 1);
}

// A line with both end points (MSGraphics: Polyline through x2+1, y2)
void PlatGraphics::DrawLineHook(int x1, int y1, int x2, int y2)
{
    SurfaceBase* theSurface = this->PrepareSurface();
    if (theSurface == NULL)
        return;
    if (this->mScale == 1.0f)
        theSurface->DrawLine(x1, y1, x2, y2);
    else
        this->ScaledLine(theSurface, x1, y1, x2, y2);
}

// GDI's Rectangle(x, y, x+width, y+height) normalised: right and bottom exclusive
static bool NormalizeRect(int& x, int& y, int& width, int& height)
{
    if (width < 0) { x += width; width = -width; }
    if (height < 0) { y += height; height = -height; }
    return width > 0 && height > 0;
}

// A rectangle outline (Rectangle with the hollow brush)
void PlatGraphics::DrawRectHook(int x, int y, int width, int height)
{
    SurfaceBase* theSurface = this->PrepareSurface();
    if (theSurface == NULL || !NormalizeRect(x, y, width, height))
        return;
    if (this->mScale == 1.0f) {
        theSurface->DrawRect(x, y, width, height);
        return;
    }
    this->FillLogicalRect(theSurface, x, y, width, 1);
    if (height > 1)
        this->FillLogicalRect(theSurface, x, y + height - 1, width, 1);
    if (height > 2) {
        this->FillLogicalRect(theSurface, x, y + 1, 1, height - 2);
        if (width > 1)
            this->FillLogicalRect(theSurface, x + width - 1, y + 1, 1, height - 2);
    }
}

void PlatGraphics::DrawDottedRectHook(int x, int y, int width, int height)
{
    Graphics::DrawDottedRectHook(x, y, width, height);
}

// A filled rectangle (Rectangle with the pen and brush of the same colour)
void PlatGraphics::FillRectHook(int x, int y, int width, int height)
{
    SurfaceBase* theSurface = this->PrepareSurface();
    if (theSurface == NULL || !NormalizeRect(x, y, width, height))
        return;
    if (this->mScale == 1.0f)
        theSurface->FillRect(x, y, width, height);
    else
        this->FillLogicalRect(theSurface, x, y, width, height);
}

bool PlatGraphics::BlendSupported() { return true; }

// The current colour over the rectangle at inBlendAmount / 255 (AlphaBlend, SourceConstantAlpha, no source alpha)
void PlatGraphics::FillRectBlendHook(int x, int y, int width, int height, int inBlendAmount)
{
    if (this->mSurface == NULL || this->mSurface->GetPixelsBase() == NULL || width <= 0 || height <= 0)
        return;
    if (this->mScale != 1.0f) {
        int theLeft = this->PX(x), theTop = this->PX(y);
        width = this->PX(x + width) - theLeft;
        height = this->PX(y + height) - theTop;
        x = theLeft;
        y = theTop;
    }
    int theClipX1, theClipY1, theClipX2, theClipY2;
    this->mSurface->GetClipRect(theClipX1, theClipY1, theClipX2, theClipY2);
    int theX1 = (std::max)(x, theClipX1), theY1 = (std::max)(y, theClipY1);
    int theX2 = (std::min)(x + width - 1, theClipX2), theY2 = (std::min)(y + height - 1, theClipY2);
    if (theX2 < theX1 || theY2 < theY1)
        return;
    unsigned theAlpha = (unsigned)inBlendAmount & 0xff, theInverse = 255 - theAlpha;
    unsigned theRed = (this->mColor >> 16) & 0xff, theGreen = (this->mColor >> 8) & 0xff, theBlue = this->mColor & 0xff;
    uint32_t* thePixels = (uint32_t*)this->mSurface->GetPixelsBase();
    int thePitch = this->mSurface->GetPixelsPerRow();
    for (int j = theY1; j <= theY2; j++) {
        uint32_t* theRow = thePixels + (size_t)j * thePitch;
        for (int i = theX1; i <= theX2; i++) {
            uint32_t thePixel = theRow[i];
            unsigned r = (theRed * theAlpha + ((thePixel >> 16) & 0xff) * theInverse + 127) / 255;
            unsigned g = (theGreen * theAlpha + ((thePixel >> 8) & 0xff) * theInverse + 127) / 255;
            unsigned b = (theBlue * theAlpha + (thePixel & 0xff) * theInverse + 127) / 255;
            theRow[i] = (thePixel & 0xff000000) | (r << 16) | (g << 8) | b;
        }
    }
}

// A closed polygon outline (Polygon with the hollow brush)
void PlatGraphics::DrawPolygonHook(POINT* inPoints, int inCount)
{
    SurfaceBase* theSurface = this->PrepareSurface();
    if (theSurface == NULL || inCount < 2)
        return;
    for (int i = 0; i < inCount; i++) {
        const POINT& a = inPoints[i];
        const POINT& b = inPoints[(i + 1) % inCount];
        if (this->mScale == 1.0f)
            theSurface->DrawLine(a.x, a.y, b.x, b.y);
        else
            this->ScaledLine(theSurface, a.x, a.y, b.x, b.y);
    }
}

// A filled polygon: the pixels whose centres lie inside by the alternate (even-odd) rule, then the pen's outline
void PlatGraphics::FillPolygonHook(POINT* inPoints, int inCount)
{
    SurfaceBase* theSurface = this->PrepareSurface();
    if (theSurface == NULL || inCount < 2)
        return;
    // at a pixel scale the fill runs over the surface's pixels with the corners scaled (the points are logical)
    std::vector<POINT> theScaled;
    POINT* theOutline = inPoints;
    if (this->mScale != 1.0f) {
        theScaled.assign(inPoints, inPoints + inCount);
        for (int i = 0; i < inCount; i++) {
            theScaled[i].x = this->PX(inPoints[i].x);
            theScaled[i].y = this->PX(inPoints[i].y);
        }
        inPoints = theScaled.data();
    }
    int theMinY = inPoints[0].y, theMaxY = inPoints[0].y;
    for (int i = 1; i < inCount; i++) {
        theMinY = (std::min)(theMinY, (int)inPoints[i].y);
        theMaxY = (std::max)(theMaxY, (int)inPoints[i].y);
    }
    theMinY = (std::max)(theMinY, theSurface->GetClipY1());
    theMaxY = (std::min)(theMaxY, theSurface->GetClipY2());
    std::vector<double> theCrossings;
    for (int y = theMinY; y <= theMaxY; y++) {
        double theCentre = y + 0.5;
        theCrossings.clear();
        for (int i = 0; i < inCount; i++) {
            const POINT& a = inPoints[i];
            const POINT& b = inPoints[(i + 1) % inCount];
            if (a.y == b.y)
                continue;
            double theLow = (std::min)(a.y, b.y), theHigh = (std::max)(a.y, b.y);
            if (theCentre < theLow || theCentre >= theHigh)
                continue;
            theCrossings.push_back(a.x + (theCentre - a.y) * (double)(b.x - a.x) / (double)(b.y - a.y));
        }
        std::sort(theCrossings.begin(), theCrossings.end());
        for (size_t i = 0; i + 1 < theCrossings.size(); i += 2) {
            int theLeft = (int)ceil(theCrossings[i] - 0.5), theRight = (int)ceil(theCrossings[i + 1] - 0.5) - 1;
            if (theRight >= theLeft)
                theSurface->DrawHorzLine(theLeft, theRight, y);
        }
    }
    this->DrawPolygonHook(theOutline, inCount);
}

// The outline width of an ellipse or arc at a pixel scale: as many boxes, each inset by one more pixel, as make one
// logical pixel
static int ScaledOutlineWidth(float inScale)
{
    int theWidth = (int)floorf(inScale + 0.5f);
    return theWidth < 1 ? 1 : theWidth;
}

void PlatGraphics::DrawEllipseHook(int x, int y, int width, int height)
{
    SurfaceBase* theSurface = this->PrepareSurface();
    if (theSurface == NULL || !NormalizeRect(x, y, width, height))
        return;
    if (this->mScale == 1.0f) {
        theSurface->DrawEllipse(x, y, width, height);
        return;
    }
    int theLeft = this->PX(x), theTop = this->PX(y);
    int theWidth = this->PX(x + width) - theLeft, theHeight = this->PX(y + height) - theTop;
    for (int i = 0, n = ScaledOutlineWidth(this->mScale); i < n && theWidth - 2 * i > 0 && theHeight - 2 * i > 0; i++)
        theSurface->DrawEllipse(theLeft + i, theTop + i, theWidth - 2 * i, theHeight - 2 * i);
}

void PlatGraphics::FillEllipseHook(int x, int y, int width, int height)
{
    SurfaceBase* theSurface = this->PrepareSurface();
    if (theSurface == NULL || !NormalizeRect(x, y, width, height))
        return;
    if (this->mScale == 1.0f) {
        theSurface->FillEllipse(x, y, width, height);
        return;
    }
    int theLeft = this->PX(x), theTop = this->PX(y);
    theSurface->FillEllipse(theLeft, theTop, this->PX(x + width) - theLeft, this->PX(y + height) - theTop);
}

// An elliptical arc from inStartAngle over inArcAngle degrees, counter-clockwise (Arc)
void PlatGraphics::DrawArcHook(int x, int y, int width, int height, int inStartAngle, int inArcAngle)
{
    SurfaceBase* theSurface = this->PrepareSurface();
    if (theSurface == NULL || !NormalizeRect(x, y, width, height))
        return;
    if (this->mScale == 1.0f) {
        theSurface->DrawArc(x, y, width, height, inStartAngle, inArcAngle);
        return;
    }
    int theLeft = this->PX(x), theTop = this->PX(y);
    int theWidth = this->PX(x + width) - theLeft, theHeight = this->PX(y + height) - theTop;
    for (int i = 0, n = ScaledOutlineWidth(this->mScale); i < n && theWidth - 2 * i > 0 && theHeight - 2 * i > 0; i++)
        theSurface->DrawArc(theLeft + i, theTop + i, theWidth - 2 * i, theHeight - 2 * i, inStartAngle, inArcAngle);
}

void PlatGraphics::ClearClipRectHook()
{
    if (this->mSurface != NULL)
        this->mSurface->ClearClipRect();
}

// Narrows the clip to the rectangle (Graphics::PerformClip clears it first, so this sets it)
void PlatGraphics::SetClipRectHook(int x, int y, int width, int height)
{
    if (this->mSurface == NULL)
        return;
    if (this->mScale != 1.0f) {
        int theLeft = this->PX(x), theTop = this->PX(y);
        width = this->PX(x + width) - theLeft;
        height = this->PX(y + height) - theTop;
        x = theLeft;
        y = theTop;
    }
    int theX1, theY1, theX2, theY2;
    this->mSurface->GetClipRect(theX1, theY1, theX2, theY2);
    int theLeft = (std::max)(x, theX1), theTop = (std::max)(y, theY1);
    int theRight = (std::min)(x + width - 1, theX2), theBottom = (std::min)(y + height - 1, theY2);
    this->mSurface->SetClipRect(theLeft, theTop, theRight - theLeft + 1, theBottom - theTop + 1);
}

} // namespace WONAPI
