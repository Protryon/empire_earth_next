// ee/EESMTEffectWrapper.h: layout only as far as the verifier-exact
// functions that include it prove it (member names by offset are placeholders).
#ifndef EE_EESMTEFFECTWRAPPER_H
#define EE_EESMTEFFECTWRAPPER_H

#include "ee/EEScenarioIDs.h"

class EESMTEffectWrapper {
public:
    // inline (the trigger's ctor 0x1c97b7 and 0x1ca3c1 expand it)
    EESMTEffectWrapper() { this->mDelay = 0; this->mEffectIndex = 0; this->mOperator = kSMTriggerOperatorEnd; }
    // +0x00 (ret 4; not a dtor): the cdecl 0x1b9fcf(&mL0C, inArg) (provisional names)
    long mOperator;                         // +0x04
    unsigned long mDelay;                   // +0x08 (unsigned: 0x5c9ab5 zero-extends it for fild qword)
    unsigned long mEffectIndex;             // +0x0c
};

#endif
