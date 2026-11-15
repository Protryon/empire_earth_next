// C:\EEX\Empire Earth\Scenario Editor\EESEUICTriggerSelectUnits.h (RECONSTRUCTION; file name inferred). The
// scenario editor's selection control (RTTI EESEUICTriggerSelectUnits > EEUICSelectUnits; vtable 0x4449e0; ctor
// 0x197393, ??_G 0x1973c1, dtor 0x1973dd): the right button drives the left-button handlers with
// mRightButton set (a right drag removes units from the selection). Requires ee/EEUICSelectUnits.h.
#ifndef EE_EESEUICTRIGGERSELECTUNITS_H
#define EE_EESEUICTRIGGERSELECTUNITS_H

class EESEUICTriggerSelectUnits : public EEUICSelectUnits {
public:
    EESEUICTriggerSelectUnits(UIForm* inForm, unsigned long inId, unsigned long inIndex, EEAMouseClick* inClick,
                              EEALasso* inLasso);
    virtual ~EESEUICTriggerSelectUnits();  // 0x1973dd
protected:
    virtual long MouseLButtonUp();          // +0x38 0x197491
    virtual long MouseLButtonDoublePressed();   // +0x3c 0x1973e8
    virtual long MouseRButtonDown();        // +0x44 0x197624
    virtual long MouseRButtonUp();          // +0x48 0x197630

    bool mRightButton;                      // +0xd4
};

#endif
