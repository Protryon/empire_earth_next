// C:\EEX\Empire Earth\Scenario Editor\EESEUICTriggerSelectUnits.h (RECONSTRUCTION; file name inferred). The
// scenario editor's selection control (RTTI EESEUICTriggerSelectUnits > EEUICSelectUnits): the right button drives
// the left-button handlers with mRightButton set (a right drag or click removes units from the selection), a release
// adds, a double click selects a type. Its overrides are EEUICSelectUnits' own Rust (ee_screens select_units.rs, the
// record's mTrigger and mRightButton). Requires ee/EEUICSelectUnits.h.
#ifndef EE_EESEUICTRIGGERSELECTUNITS_H
#define EE_EESEUICTRIGGERSELECTUNITS_H

class EESEUICTriggerSelectUnits : public EEUICSelectUnits {
public:
    EESEUICTriggerSelectUnits(UIForm* inForm, unsigned long inId, unsigned long inIndex, EEAMouseClick* inClick,
                              EEALasso* inLasso)
        : EEUICSelectUnits(inForm, inId, inIndex, inClick, inLasso)
    {
        this->mTrigger = true;
        this->mRightButton = false;
    }
protected:
    virtual long MouseRButtonDown() { return this->Rust(EE_SCREENS_SU_DO_R_DOWN); }
    virtual long MouseRButtonUp() { return this->Rust(EE_SCREENS_SU_DO_R_UP); }
};

#endif
