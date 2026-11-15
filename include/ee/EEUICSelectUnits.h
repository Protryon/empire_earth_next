// C:\EEX\Empire Earth\User Interface Thread\EEUICSelectUnits.h (RECONSTRUCTION; file name inferred). The main game
// form's left-drag selection control (RTTI EEUICSelectUnits > UIControl; vtable 0x44a038; EEUICSelectUnits.cpp
// 0x25952f..0x259f7f): a drag draws a box (eight GELines: four black shadow lines, four white) and hands it to the
// lasso action, a click to the mouse click action. Whole in Rust (ee_screens select_units.rs, round 29): the box, the
// drag's points and flags are the record EeScreensSelectUnitsFields (the scenario editor's EESEUICTriggerSelectUnits
// reads it); the lines and the two actions stay C++. Requires lle/UIControl.h, ee/EEAction.h, ee/EEAMouseClick.h,
// ee/EEALasso.h.
#ifndef EE_EEUICSELECTUNITS_H
#define EE_EEUICSELECTUNITS_H

#include "ee/EEUIRustControl.h"

class GELine;

class EEUICSelectUnits : public EEUIRustControl, protected EeScreensSelectUnitsFields {
public:
    virtual bool GetTabable() const { return false; }
    // 0x25958f (the user-defined control factory 0x2748f7; EESEUICTriggerSelectUnits 0x1973a8): a 2x2 rectangle at
    // gViewport's view rectangle top-left; references inClick and inLasso
    EEUICSelectUnits(UIForm* inForm, unsigned long inId, unsigned long inIndex, EEAMouseClick* inClick,
                     EEALasso* inLasso);
    virtual ~EEUICSelectUnits();
    virtual long CopyData() { return this->Rust(EE_SCREENS_SU_DO_COPY_DATA); }
    virtual long DrawPolygons() { return this->Rust(EE_SCREENS_SU_DO_DRAW); }
    // 0x259ec5: a left press at inLocation starts a selection (EEUIFMainGame, the scenario editor)
    void StartSelection(const GE2DPoint& inLocation) { this->Rust(EE_SCREENS_SU_DO_START, &inLocation); }
    // inline accessors (EERenderObject::FillRenderLists picks the units in the drawn box; provisional names)
    bool GetIsDrawn() const { return this->mD1; }
    const GERectangle& GetSelectionRectangle() const { return *reinterpret_cast<const GERectangle*>(&this->mRA0); }
    const GE2DPoint& GetDragStart() const { return *reinterpret_cast<const GE2DPoint*>(&this->mDragStart); }
    // New (port), TEST ONLY readers for the UI fixture's box selection (EETestFixture.cpp)
    bool GetIsDragging() const { return this->mDragging; }
    const GE2DPoint& GetDragEnd() const { return *reinterpret_cast<const GE2DPoint*>(&this->mDragEnd); }
protected:
    virtual long MouseLButtonUp() { return this->Rust(EE_SCREENS_SU_DO_MOUSE_L_UP); }
    virtual long MouseLButtonDoublePressed() { return this->Rust(EE_SCREENS_SU_DO_DOUBLE_CLICK); }
    virtual long MouseMove(const GE2DPoint& inLocation) { return this->Rust(EE_SCREENS_SU_DO_MOUSE_MOVE, &inLocation); }
    virtual int64_t RustOp(uint32_t inOp, int64_t inA, int64_t inB, float* ioF);
    long Rust(uint32_t inWhich, const GE2DPoint* inLocation = NULL);

    GELine* mLines;                         // new GELine[8]
    EEAMouseClick* mClick;                  // referenced; the mouse click action
    EEALasso* mLasso;                       // referenced
};

#endif
