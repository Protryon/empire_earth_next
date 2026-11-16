// C:\EEX\Empire Earth\Scenario Editor\EESEUICCommandUnits.h (RECONSTRUCTION; file name inferred). The scenario
// editor's right-drag control (RTTI EESEUICCommandUnits > EEUICCommandUnits): a drag rotates the selected units
// through a rotation action (an EESEARust), a right click without a drag deletes the unit under the mouse. Its
// overrides are EEUICCommandUnits' own Rust (ee_screens command_units.rs, the record's mEditor); the class keeps the
// rotation action and the ops that reach it (Scenario Editor Actions/EESEARust.cpp). Requires ee/EEUICCommandUnits.h,
// ee/EESEARust.h.
#ifndef EE_EESEUICCOMMANDUNITS_H
#define EE_EESEUICCOMMANDUNITS_H

class EESEUICCommandUnits : public EEUICCommandUnits {
public:
    EESEUICCommandUnits(UIForm* inForm, unsigned long inId, unsigned long inIndex, EEAMouseClick* inListener);
    virtual ~EESEUICCommandUnits();
protected:
    virtual int64_t RustOp(uint32_t inOp, int64_t inA, int64_t inB, float* ioF);

    EESEARust* mRotateAction;              // new in the ctor, dereferenced by the dtor
};

#endif
