// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEElevation.cpp (file name inferred: no __FILE__; RTTI class
// EEUIFSEElevation). The editor's elevation tab: 0x1a098e..0x1a1397 (the ctor, EEUIEElevationTool's ??_G, the tab's
// ??_G, dtor and slots). After the form's unit (0x1a0315..: EEUIFSEBrushGroup), before EEUIFSEESElevation.cpp
// (0x1a1397). Unit flags: the exe module set.
//
// The tab whole in Rust (ee_screens editor_elevation.rs): its group editor_groups_ffi.rs holds it (the class
// EEUIFSEElevation is gone). This unit
// is the tab's host (ee_screenscpp_seel): the controls only the tab makes and the world it reads.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEViewport.h"
#include "lle/GERhombus.h"
#include "lle/UIForm.h"


#include "ee/EmpireEarth.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEMouse.h"
#include "lle/UIESliderChange.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EEUIShell.h"
#include "rust/game/screens.h"

// ---- the tab's host (editor_elevation.rs: the ops from 900)


namespace {

UISlider* SeelxSlider(void* const* p) { return static_cast<UISlider*>(static_cast<UIControl*>(p[0])); }
EEUIFSEBrushGroup* SeelxBrushes(void* const* p) { return static_cast<EEUIFScenarioEditor*>(static_cast<UIForm*>(p[0]))->GetBrushGroup(); }
EETile* sSeelxTile;      // EE_SCREENS_SEELX_POINT's and _TILE's
GE3DPoint sSeelxPoint;   // EE_SCREENS_SEELX_POINT's
long sSeelxX;            // EE_SCREENS_SEELX_TILE's
long sSeelxY;

}   // namespace

// the tab's host whole in Rust (ee_screens editor_tabsx.rs composes the EE_SCREENS_SEEL_OP_* ops of these single
// statements, EE_SCREENS_SEELX_*)
extern "C" int32_t ee_screenscpp_seelx(uint32_t inOp, void* const* p, int32_t* i, float* f)
{
    UIForm* theForm = static_cast<UIForm*>(p[1]);
    switch (inOp) {
    case EE_SCREENS_SEELX_NEW_PAINT: return EESEOut(i, static_cast<UIControl*>(new EEUIShellControl(EE_SCREENS_SK_PAINT_ELEVATION, {(int64_t)static_cast<void* const*>(p[2])[0], (int64_t)static_cast<void* const*>(p[2])[1]}, EE_SCREENS_SE_ELEVATION_PAINT, GERectangle(GE2DPoint(0.0f, 0.0f), 1024.0f, 606.0f), theForm, (unsigned long)i[0], 0)));
    case EE_SCREENS_SEELX_NEW_ELEVATION_SLIDER: return EESEOut(i, static_cast<UIControl*>(new EEUIShellSlider(EE_SCREENS_SK_ELEVATION_SLIDER, {}, EE_SCREENS_SE_ELEVATION_ELEVATION_SLIDER, GERectangle(GE2DPoint(f[0], f[1]), 20.0f, (float)(unsigned long)f[2]), 1, theForm, (unsigned long)i[0], 2)));
    case EE_SCREENS_SEELX_NEW_STRENGTH_SLIDER: return EESEOut(i, static_cast<UIControl*>(new UISlider(EE_SCREENS_SE_ELEVATION_STRENGTH_SLIDER, GERectangle(GE2DPoint(f[0], f[1]), 20.0f, f[2]), 1, theForm, (unsigned long)i[0], 2)));
    case EE_SCREENS_SEELX_SLIDER_RANGE: SeelxSlider(p)->SetRange(0, i[0]);
        return 0;
    case EE_SCREENS_SEELX_SLIDER_MAX: return (int32_t)SeelxSlider(p)->GetMaxRange();
    case EE_SCREENS_SEELX_SLIDER_SET: SeelxSlider(p)->SetCurrentRangeValue(i[0]);
        return 0;
    case EE_SCREENS_SEELX_SLIDER_VALUE: return (int32_t)SeelxSlider(p)->GetCurrentRangeValue();
    case EE_SCREENS_SEELX_SLIDER_PAGE: SeelxSlider(p)->SetPageSize(1);
        return 0;
    case EE_SCREENS_SEELX_NEW_SLIDER_CHANGE: return EESEOut(i, static_cast<UIEvent*>(new UIESliderChange(static_cast<UITextLabel*>(p[2]), -4.0f, 0x28)));
    case EE_SCREENS_SEELX_MOUSE_X: f[0] = gMouse->mPosition.GetX();
        return 0;
    case EE_SCREENS_SEELX_MOUSE_Y: f[0] = gMouse->mPosition.GetY();
        return 0;
    case EE_SCREENS_SEELX_IS_CURRENT: return static_cast<UIForm*>(p[0]) == static_cast<UIForm*>(p[0])->GetManager()->GetCurrentForm();
    case EE_SCREENS_SEELX_VIEW_HOLDS: return gViewport.GetViewRectangle().Contains(GE2DPoint(f[0], f[1]));
    case EE_SCREENS_SEELX_HIGHLIGHT_SHOWN: SeelxBrushes(p)->mIsHighlightShown = i[0] != 0;
        return 0;
    case EE_SCREENS_SEELX_SET_HIGHLIGHT: ee_screens_sebr_group_highlight(reinterpret_cast<EeScreensSeBrushGroup*>(SeelxBrushes(p)), f[0], f[1], i[0] != 0);
        return 0;
    case EE_SCREENS_SEELX_POINT: EEPointUnitOrTile(GE2DPoint(f[0], f[1]), sSeelxTile, sSeelxPoint, false);
        return 0;
    case EE_SCREENS_SEELX_POINT_X: f[0] = sSeelxPoint.GetX();
        return 0;
    case EE_SCREENS_SEELX_POINT_Y: f[0] = sSeelxPoint.GetY();
        return 0;
    case EE_SCREENS_SEELX_TILE: EEPointUnitOrTileXY(GE2DPoint(f[0], f[1]), sSeelxTile, sSeelxX, sSeelxY, false);
        return 0;
    case EE_SCREENS_SEELX_TILE_X: return (int32_t)sSeelxX;
    case EE_SCREENS_SEELX_TILE_Y: return (int32_t)sSeelxY;
    case EE_SCREENS_SEELX_FONT_HEIGHT: return (int32_t)gSeplScreenFont->GetHeight();
    }
    return 0;
}
