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
      EeScreensSelectUnitsFields(), mClick(inClick)
{
    this->StoreDisplayMode(kDisplayModeHidden);
    this->mClick->Reference();
    this->mLasso = inLasso;
    this->mLasso->Reference();
    this->SetAlwaysDirty(true);
}

// Destroys the selection box, releasing its click and lasso handlers
EEUICSelectUnits::~EEUICSelectUnits()
{
    this->mClick->Dereference();
    this->mLasso->Dereference();
}

// CopyData, DrawPolygons, the double click, the release, the drag and StartSelection: ee_screens select_units.rs
// (EE_SCREENS_SU_DO_*) over the picker's and actions' ops below (the lines and the unit's reads: hud_ctl_ffi.rs)
long EEUICSelectUnits::Rust(uint32_t inWhich, const GE2DPoint* inLocation)
{
    return (long)ee_screens_select_units(this, this->Host(), inWhich, inLocation ? inLocation->GetX() : 0.0f,
                                         inLocation ? inLocation->GetY() : 0.0f);
}

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
