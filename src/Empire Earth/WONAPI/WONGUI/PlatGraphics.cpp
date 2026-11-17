// The WONAPI GUI's graphics context on the platform layer (WONLobbyPlatform.h): draws into the display context's
// software surface (WONSurface.h) with the results the GDI backend (MSGraphics.cpp) had, at the surface's pixel
// scale. R6 WON round 40: every hook is ee_won's platgfx.rs (rust/crates/ee_won_gui/src/platgfx.rs); the class keeps the
// borrowed surface and the scale, Graphics the colour.
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPlatform.h"
#include "rust/ee_won.h"

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

void PlatGraphics::Detach()
{
    Graphics::Detach();
    if (this->mSurface == NULL)
        return;
    this->mSurface = NULL;
    this->mDisplayContext = NULL;
}

DWORD PlatGraphics::Hook(int inOp, int a, int b, int c, int d, int e, int f)
{
    return ee_won_plat_gfx(this->mSurface != NULL ? this->mSurface->GetRust() : NULL, this->mScale, (uint32_t)this->mColor,
                           (EeWonPlatGfx)inOp, a, b, c, d, e, f);
}

void PlatGraphics::Polygon(POINT* inPoints, int inCount, bool inFill)
{
    std::vector<int32_t> theXY;
    for (int i = 0; i < inCount; i++) {
        theXY.push_back((int32_t)inPoints[i].x);
        theXY.push_back((int32_t)inPoints[i].y);
    }
    ee_won_plat_gfx_polygon(this->mSurface != NULL ? this->mSurface->GetRust() : NULL, this->mScale, (uint32_t)this->mColor,
                            inFill, theXY.data(), inCount);
}

void PlatGraphics::SetColorHook() {}
DWORD PlatGraphics::GetPixelHook(int x, int y) { return this->Hook((int)EeWonPlatGfx::GetPixel, x, y); }
void PlatGraphics::SetPixelHook(int x, int y, DWORD inColor) { this->Hook((int)EeWonPlatGfx::SetPixel, x, y, (int)inColor); }
void PlatGraphics::DrawLineHook(int x1, int y1, int x2, int y2) { this->Hook((int)EeWonPlatGfx::Line, x1, y1, x2, y2); }
void PlatGraphics::DrawRectHook(int x, int y, int width, int height) { this->Hook((int)EeWonPlatGfx::Rect, x, y, width, height); }
void PlatGraphics::DrawDottedRectHook(int x, int y, int width, int height) { Graphics::DrawDottedRectHook(x, y, width, height); }
void PlatGraphics::FillRectHook(int x, int y, int width, int height) { this->Hook((int)EeWonPlatGfx::FillRect, x, y, width, height); }
bool PlatGraphics::BlendSupported() { return true; }
void PlatGraphics::FillRectBlendHook(int x, int y, int width, int height, int inBlendAmount)
{ this->Hook((int)EeWonPlatGfx::FillRectBlend, x, y, width, height, inBlendAmount); }
void PlatGraphics::DrawPolygonHook(POINT* inPoints, int inCount) { this->Polygon(inPoints, inCount, false); }
void PlatGraphics::FillPolygonHook(POINT* inPoints, int inCount) { this->Polygon(inPoints, inCount, true); }
void PlatGraphics::DrawEllipseHook(int x, int y, int width, int height) { this->Hook((int)EeWonPlatGfx::Ellipse, x, y, width, height); }
void PlatGraphics::FillEllipseHook(int x, int y, int width, int height) { this->Hook((int)EeWonPlatGfx::FillEllipse, x, y, width, height); }
void PlatGraphics::DrawArcHook(int x, int y, int width, int height, int inStartAngle, int inArcAngle)
{ this->Hook((int)EeWonPlatGfx::Arc, x, y, width, height, inStartAngle, inArcAngle); }
void PlatGraphics::ClearClipRectHook() { this->Hook((int)EeWonPlatGfx::ClearClip); }
void PlatGraphics::SetClipRectHook(int x, int y, int width, int height) { this->Hook((int)EeWonPlatGfx::SetClip, x, y, width, height); }

} // namespace WONAPI
