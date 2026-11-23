// C:\EEX\Empire Earth\User Interface Thread\EEUICSelectUnits.cpp (file name inferred: no __FILE__; RTTI class
// EEUICSelectUnits). The main game form's left-drag selection control: 0x25952f..0x259f7f (the two GEColor `$E`
// chains, ctor, ??_G, dtor, then the members in alphabetical order; EEUICUnitIconBar's ctor at 0x259f7f). Unit flags:
// the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/UIControl.h"
#include "lle/NEMessage.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EmpireEarth.h"
#include "ee/EEGraphics.h"
#include "ee/EEMouse.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEAction.h"
#include "ee/EEARust.h"
#include "ee/EEUICSelectUnits.h"

// Creates the main game view's drag-selection box (its eight lines: ee_screens hud_draw.rs), hidden
EEUICSelectUnits::EEUICSelectUnits(UIForm* inForm, unsigned long inId, unsigned long inIndex, EEAMouseClick* inClick,
                                   EEALasso* inLasso)
    : EEUIRustControl(inId, GERectangle(GE2DPoint(gViewport.GetViewRectangle().GetTopLeft()), 2.0f, 2.0f), inForm, inIndex, 0),
      EeScreensSelectUnitsFields(), mClick(inClick), mLasso(inLasso)
{
    ee_screens_su_construct(this);   // (hud_ctl_ffi.rs: hidden, the handlers referenced, always dirty)
}

// the constructor's and destructor's calls (hud_ctl_ffi.rs)
extern "C" void ee_screenscpp_su_hide(EEUICSelectUnits* inBox) { EEUICSelectUnits::Hide(inBox); }
extern "C" void ee_screenscpp_su_click_reference(EEUICSelectUnits* inBox) { inBox->mClick->Reference(); }
extern "C" void ee_screenscpp_su_lasso_reference(EEUICSelectUnits* inBox) { inBox->mLasso->Reference(); }
extern "C" void ee_screenscpp_su_always_dirty(EEUICSelectUnits* inBox) { inBox->SetAlwaysDirty(true); }
extern "C" void ee_screenscpp_su_click_release(EEUICSelectUnits* inBox) { inBox->mClick->Dereference(); }
extern "C" void ee_screenscpp_su_lasso_release(EEUICSelectUnits* inBox) { inBox->mLasso->Dereference(); }

int64_t EEUICSelectUnits::RustOp(uint32_t inOp, int64_t inA, int64_t inB, float* ioF)
{
    EEUnit* theUnit = reinterpret_cast<EEUnit*>(inA);
    switch (inOp) {
    case EE_SCREENS_SU_PICK: return (int64_t)gUnitPicker.PickUnit(gMouse->mPosition, 0);
    case EE_SCREENS_SU_LASSO_TYPE:
        this->mLasso->SelectUnits(gU4E8F08.GetViewRectangle(), (long)inB, theUnit->GetUnitClass()->mUnitType, theUnit->mUnitID);
        break;
    case EE_SCREENS_SU_DEREF: theUnit->Dereference(); break;
    case EE_SCREENS_SU_LASSO_BOX:
        this->mLasso->SelectUnits(GERectangle(ioF[0], ioF[1], ioF[2], ioF[3]), (long)inA, kAnyUnitType, 0);
        break;
    case EE_SCREENS_SU_CLICK: this->mClick->SelectAtMouse((unsigned long)inA); break;
    // the trigger tab's (EESEUICTriggerSelectUnits)
    case EE_SCREENS_SU_POINT_AT: {
        GE3DPoint thePoint;
        EETile* theLocation;
        EEPointUnitOrTile(this->GetMouseLocation(), theLocation, thePoint, false);
        ioF[0] = thePoint.GetX();
        ioF[1] = thePoint.GetY();
        break;
    }
    case EE_SCREENS_SU_LASSO_IN_RECT:
        this->mLasso->SelectInRectangle(GERectangle(ioF[0], ioF[1], ioF[2], ioF[3]), (long)inA);
        break;
    case EE_SCREENS_SU_CLICK_EXECUTE: this->mClick->Execute(ioF[0], ioF[1], true, 1, false, 0.0f, false); break;
    case EE_SCREENS_SU_L_DOWN: return this->MouseLButtonDown();
    }
    return 0;
}
