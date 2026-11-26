// C:\EEX\Empire Earth\User Interface Thread\EEUICSelectUnits.cpp (file name inferred: no __FILE__; RTTI class
// EEUICSelectUnits). The main game form's left-drag selection control: 0x25952f..0x259f7f (the two GEColor `$E`
// chains, ctor, ??_G, dtor, then the members in alphabetical order; EEUICUnitIconBar's ctor at 0x259f7f). Unit flags:
// the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEViewport.h"
#include "lle/GERhombus.h"
#include "lle/UIControl.h"
#include "ee/EEUnit.h"
#include "ee/EmpireEarth.h"
#include "ee/EEGraphics.h"
#include "ee/EEMouse.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEARust.h"
#include "ee/EEUICSelectUnits.h"

// Creates the main game view's drag-selection box (its eight lines: ee_screens hud_draw.rs), hidden
EEUICSelectUnits::EEUICSelectUnits(UIForm* inForm, unsigned long inId, unsigned long inIndex, EEAMouseClick* inClick,
                                   EEALasso* inLasso)
    : EEUIRustControl(inId, GERectangle(GE2DPoint(gViewport.GetViewRectangle().GetTopLeft()), 2.0f, 2.0f), inForm, inIndex, 0),
      EeScreensSelectUnitsFields(), EeScreensSelectUnitsObjects{inClick, inLasso}
{
    ee_screens_su_construct(this);   // (hud_ctl_ffi.rs: hidden, the handlers referenced, always dirty)
}

// the constructor's and destructor's calls (hud_ctl_ffi.rs)
extern "C" void ee_screenscpp_su_hide(EEUICSelectUnits* inBox) { EEUICSelectUnits::Hide(inBox); }
extern "C" void ee_screenscpp_su_click_reference(EEUICSelectUnits* inBox) { static_cast<EEAMouseClick*>(inBox->mClick)->Reference(); }
extern "C" void ee_screenscpp_su_lasso_reference(EEUICSelectUnits* inBox) { static_cast<EEALasso*>(inBox->mLasso)->Reference(); }
extern "C" void ee_screenscpp_su_always_dirty(EEUICSelectUnits* inBox) { inBox->SetAlwaysDirty(true); }
extern "C" void ee_screenscpp_su_click_release(EEUICSelectUnits* inBox) { static_cast<EEAMouseClick*>(inBox->mClick)->Dereference(); }
extern "C" void ee_screenscpp_su_lasso_release(EEUICSelectUnits* inBox) { static_cast<EEALasso*>(inBox->mLasso)->Dereference(); }

#include "ee/EEScreensShims.h"
#include "rust/game/screens.h"
// the class's ops (select_units_ops.rs; its RustOp until UI lane round 16)
#define SUC(x) reinterpret_cast<EEUICSelectUnits*>(x)
extern "C" uintptr_t ee_screenscpp_su_pick() { return (uintptr_t)gUnitPicker.PickUnit(gMouse->mPosition, 0); }
extern "C" void ee_screenscpp_su_lasso_type(uintptr_t inBox, uintptr_t inUnit, int64_t inMode)
{ ee_screens_ga_select_units(static_cast<EEALasso*>(SUC(inBox)->mLasso), &(gU4E8F08.GetViewRectangle()), (int32_t)((long)inMode), (uint32_t)(reinterpret_cast<EEUnit*>(inUnit)->GetUnitClass()->mUnitType), (int32_t)(reinterpret_cast<EEUnit*>(inUnit)->mUnitID)); }
extern "C" void ee_screenscpp_su_unit_release(uintptr_t inUnit) { reinterpret_cast<EEUnit*>(inUnit)->Dereference(); }
extern "C" void ee_screenscpp_su_lasso_box(uintptr_t inBox, float inA, float inB, float inC, float inD, int64_t inMode)
{ ee_screens_ga_select_units(static_cast<EEALasso*>(SUC(inBox)->mLasso), &(GERectangle(inA, inB, inC, inD)), (int32_t)((long)inMode), (uint32_t)kAnyUnitType, (int32_t)0); }
extern "C" void ee_screenscpp_su_click(uintptr_t inBox, uint32_t inMode) { ee_screens_ga_select_at_mouse(static_cast<EEARust*>(static_cast<EEAMouseClick*>(SUC(inBox)->mClick)), (uint32_t)((unsigned long)inMode)); }
// the trigger tab's (EESEUICTriggerSelectUnits): the point under the mouse into the unit hack's scratch point
extern "C" void ee_screenscpp_su_point_at(uintptr_t inBox)
{ EEPointUnitOrTile(SUC(inBox)->GetMouseLocation(), EEScratch<EETile*>(kScratchShimTile), EEScratch<GE3DPoint>(kScratchUHPoint), false); }
extern "C" void ee_screenscpp_su_lasso_in_rect(uintptr_t inBox, float inA, float inB, float inC, float inD, int64_t inMode)
{ static_cast<EEALasso*>(SUC(inBox)->mLasso)->SelectInRectangle(GERectangle(inA, inB, inC, inD), (long)inMode); }
extern "C" void ee_screenscpp_su_click_execute(uintptr_t inBox, float inX, float inY)
{ static_cast<EEAMouseClick*>(SUC(inBox)->mClick)->Execute(inX, inY, true, 1, false, 0.0f, false); }
extern "C" int64_t ee_screenscpp_su_left_down(uintptr_t inBox) { return static_cast<UIControl*>(SUC(inBox))->MouseLButtonDown(); }
