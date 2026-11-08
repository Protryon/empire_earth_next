// The WONAPI GUI's graphics context on the platform layer (WONLobbyPlatform.h): draws into the display context's
// software surface (WONSurface.h) with the results the GDI backend (MSGraphics.cpp) had: lines include both end
// points (Polyline with an extra one-pixel segment), rectangles cover x..x+width-1 (Rectangle), fills are solid,
// FillRectBlend is AlphaBlend with a constant alpha, polygons fill by the alternate rule with the pen's outline,
// ellipses lie in the box x..x+width-1, and the clip is the surface's (a DC's clip region).
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <math.h>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPlatform.h"

namespace WONAPI {

PlatGraphics::PlatGraphics() : mSurface(NULL) {}

PlatGraphics::~PlatGraphics() { this->Detach(); }

// Attaches to the context's surface; the colour starts black (MSGraphics' black pen and brush)
void PlatGraphics::Attach(DisplayContext* inContext)
{
    Graphics::Attach(inContext);
    this->mSurface = inContext->mSurface;
    this->mColor = 0;
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
    DWORD thePixel = this->mSurface->GetPixel(x, y);
    return ((thePixel & 0xff) << 16) | (thePixel & 0xff00) | ((thePixel >> 16) & 0xff);
}

void PlatGraphics::SetPixelHook(int x, int y, DWORD inColor)
{
    if (this->mSurface != NULL && this->mSurface->GetPixelsBase() != NULL)
        this->mSurface->SetPixel(x, y, inColor & 0x00FFFFFF);
}

// A line with both end points (MSGraphics: Polyline through x2+1, y2)
void PlatGraphics::DrawLineHook(int x1, int y1, int x2, int y2)
{
    if (SurfaceBase* theSurface = this->PrepareSurface())
        theSurface->DrawLine(x1, y1, x2, y2);
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
    theSurface->DrawRect(x, y, width, height);
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
    theSurface->FillRect(x, y, width, height);
}

bool PlatGraphics::BlendSupported() { return true; }

// The current colour over the rectangle at inBlendAmount / 255 (AlphaBlend, SourceConstantAlpha, no source alpha)
void PlatGraphics::FillRectBlendHook(int x, int y, int width, int height, int inBlendAmount)
{
    if (this->mSurface == NULL || this->mSurface->GetPixelsBase() == NULL || width <= 0 || height <= 0)
        return;
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
        theSurface->DrawLine(a.x, a.y, b.x, b.y);
    }
}

// A filled polygon: the pixels whose centres lie inside by the alternate (even-odd) rule, then the pen's outline
void PlatGraphics::FillPolygonHook(POINT* inPoints, int inCount)
{
    SurfaceBase* theSurface = this->PrepareSurface();
    if (theSurface == NULL || inCount < 2)
        return;
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
    this->DrawPolygonHook(inPoints, inCount);
}

void PlatGraphics::DrawEllipseHook(int x, int y, int width, int height)
{
    SurfaceBase* theSurface = this->PrepareSurface();
    if (theSurface == NULL || !NormalizeRect(x, y, width, height))
        return;
    theSurface->DrawEllipse(x, y, width, height);
}

void PlatGraphics::FillEllipseHook(int x, int y, int width, int height)
{
    SurfaceBase* theSurface = this->PrepareSurface();
    if (theSurface == NULL || !NormalizeRect(x, y, width, height))
        return;
    theSurface->FillEllipse(x, y, width, height);
}

// An elliptical arc from inStartAngle over inArcAngle degrees, counter-clockwise (Arc)
void PlatGraphics::DrawArcHook(int x, int y, int width, int height, int inStartAngle, int inArcAngle)
{
    SurfaceBase* theSurface = this->PrepareSurface();
    if (theSurface == NULL || !NormalizeRect(x, y, width, height))
        return;
    theSurface->DrawArc(x, y, width, height, inStartAngle, inArcAngle);
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
    int theX1, theY1, theX2, theY2;
    this->mSurface->GetClipRect(theX1, theY1, theX2, theY2);
    int theLeft = (std::max)(x, theX1), theTop = (std::max)(y, theY1);
    int theRight = (std::min)(x + width - 1, theX2), theBottom = (std::min)(y + height - 1, theY2);
    this->mSurface->SetClipRect(theLeft, theTop, theRight - theLeft + 1, theBottom - theTop + 1);
}

} // namespace WONAPI
