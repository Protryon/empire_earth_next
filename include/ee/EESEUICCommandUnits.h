// C:\EEX\Empire Earth\Scenario Editor\EESEUICCommandUnits.h (RECONSTRUCTION; file name inferred). The scenario
// editor's right-drag control (RTTI EESEUICCommandUnits > EEUICCommandUnits; vtable 0x444950; ctor 0x197261,
// ??_G 0x1972ac, dtor 0x1972c8): a drag rotates the selected units through an
// rotation action (+0xd0, an EESEARust), a right click deletes the unit under the mouse. Requires ee/EEUICCommandUnits.h,
// ee/EESEARust.h.
#ifndef EE_EESEUICCOMMANDUNITS_H
#define EE_EESEUICCOMMANDUNITS_H

class EESEUICCommandUnits : public EEUICCommandUnits {
public:
    EESEUICCommandUnits(UIForm* inForm, unsigned long inId, unsigned long inIndex, EEAMouseClick* inListener);
    virtual ~EESEUICCommandUnits();         // 0x1972c8
    virtual void StartCommand(const GE2DPoint& inLocation);   // +0x88 0x19737c
protected:
    virtual long MouseMove(const GE2DPoint& inLocation);    // +0x40 0x1972e6
    virtual long MouseRButtonUp();          // +0x48 0x197316

    EESEARust* mRotateAction;              // +0xd0 (new in the ctor, dereferenced by the dtor)
};

#endif
