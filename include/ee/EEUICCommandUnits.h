// C:\EEX\Empire Earth\User Interface Thread\EEUICCommandUnits.h (RECONSTRUCTION; file name inferred). The main game
// form's right-drag command control: a right drag on the map turns a flat arrow model ("Unit Direction Model",
// "textures\Unit Facing Arrow") to the facing the selected units take; the release hands target and facing to the
// form's command mouse click action (RTTI EEUICCommandUnits > UIControl; vtable 0x449e8c, 35 slots: UIControl's 34 +
// StartCommand; the scenario editor's EESEUICCommandUnits derives from it; EEUICCommandUnits.cpp 0x257881..0x258464).
// Whole in Rust (ee_screens command_units.rs, round 29): the press, the map point, the facing and their snapshot are
// the record EeScreensCommandUnitsFields (the editor's subclass reads mFacing, mCanFace, mHasFacing); the action, the
// model and the texture stay C++. Requires lle/UIControl.h, ee/EEAction.h, ee/EEAMouseClick.h.
#ifndef EE_EEUICCOMMANDUNITS_H
#define EE_EEUICCOMMANDUNITS_H

#include "ee/EEUIRustControl.h"

class EEAMouseClick;
class GEModel;
class GETexture;

class EEUICCommandUnits : public EEUIRustControl, protected EeScreensCommandUnitsFields {
public:
    virtual bool GetTabable() const { return false; }
    // 0x257881 (the user-defined control factory 0x274931; EESEUICCommandUnits 0x197274): a 2x2 rectangle at
    // gViewport's view rectangle top-left; references inListener
    EEUICCommandUnits(UIForm* inForm, unsigned long inId, unsigned long inIndex, EEAMouseClick* inListener);
    virtual ~EEUICCommandUnits();           // 0x257971
    virtual long CopyData() { return this->Rust(EE_SCREENS_CU_DO_COPY_DATA); }   // 0x2579e5
    virtual long DrawPolygons() { return this->Rust(EE_SCREENS_CU_DO_DRAW); }   // 0x257c4d
    // +0x88 0x25829c: a right button press at inLocation (EEUIFMainGame; EESEUICCommandUnits 0x197383)
    virtual void StartCommand(const GE2DPoint& inLocation) { this->Rust(EE_SCREENS_CU_DO_START, &inLocation); }
protected:
    virtual long MouseLButtonDoublePressed() { return 0; }
    virtual long MouseMove(const GE2DPoint& inLocation) { return this->Rust(EE_SCREENS_CU_DO_MOUSE_MOVE, &inLocation); }
    virtual long MouseRButtonUp() { return this->Rust(EE_SCREENS_CU_DO_MOUSE_R_UP); }   // 0x258170
    virtual int64_t RustOp(uint32_t inOp, int64_t inA, int64_t inB, float* ioF);
    long Rust(uint32_t inWhich, const GE2DPoint* inLocation = NULL);

    EEAMouseClick* mListener;               // referenced (EEUIFMainGame's mouse click action)
    GEModel* mModel;                        // referenced
    GETexture* mTexture;                    // referenced
};

#endif
