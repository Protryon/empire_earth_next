// ee/EESMTConditionWrapper.h: layout only as far as the verifier-exact
// functions that include it prove it (member names by offset are placeholders).
#ifndef EE_EESMTCONDITIONWRAPPER_H
#define EE_EESMTCONDITIONWRAPPER_H

#include "ee/EESMIDMap.h"
#include "ee/EEScenarioIDs.h"

class EESMTConditionWrapper {
public:
    // inline (the trigger's ctor 0x1c97b7 and 0x1ca2c3 expand it)
    EESMTConditionWrapper() { this->mIsNot = false; this->mValue = 0; this->mOperator = kSMTriggerOperatorEnd; }
    // +0x00 (ret 4; not a dtor): the cdecl 0x1b9fcf(&mValue, inArg) (provisional names)
    virtual void Virtual0(EESMIDMap& inMap);   // (EESMIDMap&: EESMTrigger +0x04/+0x0c forward a map)
    virtual void Virtual4(unsigned long inOld, unsigned long inNew);
    unsigned long mValue;                   // +0x04
    long mOperator;                         // +0x08 (0 at the `new` sites 0x1c985b/0x1ca2ff)
    bool mIsNot;                            // +0x0c (false there)
};

#endif
