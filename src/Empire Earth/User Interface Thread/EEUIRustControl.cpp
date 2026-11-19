// New (port): the host of the game's controls whose logic is Rust (ee/EEUIRustControl.h; ee_screens hud_ctl.rs):
// any control's ops here, the class's own in its RustOp.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "GEWindow.h"
#include "lle/GEViewport.h"
#include "lle/GERhombus.h"
#include "lle/Input System.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EmpireEarth.h"
#include "ee/EEGraphics.h"
#include "ee/EEMouse.h"
#include "ee/EEUIRustControl.h"

extern "C" int64_t ee_screenscpp_hud_ctl(void* inControl, uint32_t inOp, int64_t inA, int64_t inB, float* ioF)
{
    return EEUIRustControl::HostOp(static_cast<EEUIRustControl*>(inControl), inOp, inA, inB, ioF);
}

// a rectangle from the op's floats (left, top, right, bottom) and back
static GERectangle EEHudRect(const float* inF)
{
    return GERectangle(inF[0], inF[1], inF[2], inF[3]);
}

static void EEHudPutRect(const GERectangle& inRect, float* outF)
{
    outF[0] = inRect.GetLeft();
    outF[1] = inRect.GetTop();
    outF[2] = inRect.GetRight();
    outF[3] = inRect.GetBottom();
}

int64_t EEUIRustControl::HostOp(EEUIRustControl* inControl, uint32_t inOp, int64_t inA, int64_t inB, float* ioF)
{
    switch (inOp) {
    case EE_SCREENS_HC_SET_RECTANGLE: inControl->SetRectangle(EEHudRect(ioF)); break;
    case EE_SCREENS_HC_SET_BG_RECTANGLE: inControl->SetBgRectangle(EEHudRect(ioF)); break;
    case EE_SCREENS_HC_DRAW_BASE: return inControl->UIControl::DrawPolygons();
    case EE_SCREENS_HC_DOUBLE_BASE: return inControl->UIControl::MouseLButtonDoublePressed();
    case EE_SCREENS_HC_UPDATE_MOUSE: return inControl->UpdateMouse(GE2DPoint(ioF[0], ioF[1]));
    case EE_SCREENS_HC_VIEW_RECT: EEHudPutRect(gViewport.GetViewRectangle(), ioF); break;
    case EE_SCREENS_HC_MOUSE_BOUNDS: gMouse->SetBounds(EEHudRect(ioF)); break;
    case EE_SCREENS_HC_MOUSE_BOUNDS_WINDOW:
        gMouse->SetBounds(GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight()));
        break;
    case EE_SCREENS_HC_VIEW_HAS_MOUSE: return gViewport.GetViewRectangle().Contains(inControl->GetMouseLocation());
    case EE_SCREENS_HC_WINDOW_WIDTH: return gGameWindow->GetWidth();
    default: return inControl->RustOp(inOp, inA, inB, ioF);
    }
    return 0;
}
