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
#include "lle/GELine.h"
#include "ee/EEUICSelectUnits.h"

static GEColor sSelectBoxColor(1.0f, 1.0f, 1.0f);   // 0x564580
static GEColor sSelectShadowColor(0.0f, 0.0f, 0.0f);    // 0x564570

// SYNTHETIC: EXE 0x65952f
// SYNTHETIC: EXE 0x659539
// SYNTHETIC: EXE 0x659552
// SYNTHETIC: EXE 0x65955e
// SYNTHETIC: EXE 0x65955f
// SYNTHETIC: EXE 0x659569
// SYNTHETIC: EXE 0x659582
// SYNTHETIC: EXE 0x65958e

// FUNCTION: EXE 0x65958f
// Creates the main game view's drag-selection box (white lines over black shadow lines), hidden
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
    this->mLines = new GELine[8];
    for (int i = 0; i < 8; i++)
        this->mLines[i].SetColor(i < 4 ? sSelectShadowColor : sSelectBoxColor);
}

// SYNTHETIC: EXE 0x65971d
// (the scalar deleting destructor, emitted with the vtable; slot EEUICSelectUnits+0x0)

// FUNCTION: EXE 0x659739
// Destroys the selection box, releasing its click and lasso handlers
EEUICSelectUnits::~EEUICSelectUnits()
{
    this->mClick->Dereference();
    this->mLasso->Dereference();
    delete[] this->mLines;
}

// FUNCTION: EXE 0x659775
// FUNCTION: EXE 0x6597e2
// FUNCTION: EXE 0x659af1
// FUNCTION: EXE 0x659bda
// FUNCTION: EXE 0x659cfc
// FUNCTION: EXE 0x659ec5
// CopyData, DrawPolygons, the double click, the release, the drag and StartSelection: ee_screens select_units.rs
// (EE_SCREENS_SU_DO_*) over the lines', picker's and actions' ops below
long EEUICSelectUnits::Rust(uint32_t inWhich, const GE2DPoint* inLocation)
{
    return (long)ee_screens_select_units(this, this->Host(), inWhich, inLocation ? inLocation->GetX() : 0.0f,
                                         inLocation ? inLocation->GetY() : 0.0f);
}

int64_t EEUICSelectUnits::RustOp(uint32_t inOp, int64_t inA, int64_t inB, float* ioF)
{
    EEUnit* theUnit = reinterpret_cast<EEUnit*>(inA);
    switch (inOp) {
    case EE_SCREENS_SU_DRAW_LINES:
        for (int i = 0; i < 8; i++) {
            this->mLines[i].SetStartPoint(GE2DPoint(ioF[i * 4], ioF[i * 4 + 1]));
            this->mLines[i].SetEndPoint(GE2DPoint(ioF[i * 4 + 2], ioF[i * 4 + 3]));
        }
        return gRasterizer->Draw(this->mLines, 8, this->GetScreenIndex());
    case EE_SCREENS_SU_PICK: return (int64_t)gUnitPicker.PickUnit(gMouse->mPosition, 0);
    case EE_SCREENS_SU_OWN_LIVE: return theUnit->GetHitPoints() > 0 && theUnit->GetPlayer() == gWorld.GetCurrentPlayer();
    case EE_SCREENS_SU_LASSO_TYPE:
        this->mLasso->SelectUnits(gU4E8F08.GetViewRectangle(), (long)inB, theUnit->GetUnitClass()->mUnitType, theUnit->mUnitID);
        break;
    case EE_SCREENS_SU_DEREF: theUnit->Dereference(); break;
    case EE_SCREENS_SU_LASSO_BOX:
        this->mLasso->SelectUnits(GERectangle(ioF[0], ioF[1], ioF[2], ioF[3]), (long)inA, kAnyUnitType, 0);
        break;
    case EE_SCREENS_SU_CLICK: this->mClick->SelectAtMouse((unsigned long)inA); break;
    }
    return 0;
}
