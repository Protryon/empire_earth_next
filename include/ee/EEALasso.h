// C:\EEX\Empire Earth\...\EEALasso.h (RECONSTRUCTION; file path inferred). The lasso (rubber band)
// selection action (RTTI EEALasso > EEAction; vtable 0x438c74, sizeof 0x10; ctor inline, dtor 0x289f out of line):
// holds its selection message (+0x0c, referenced; an EEMSelectUnits or a Select Patrol Group record). Requires ee/EEAction.h, ee/EEMessage.h.
#ifndef EE_EEALASSO_H
#define EE_EEALASSO_H

class EEALasso : public EEAction {
public:
    EEALasso() { this->mMessage = NULL; }
    virtual ~EEALasso();
    virtual void Virtual4();                // +0x04 (0x28c4)
    virtual void Virtual8();                // +0x08 (the shared empty slot 0x1720c6)
    virtual void SelectInRectangle(const GERectangle& inRectangle, long inArg2) {}  // +0x0c (0xac9c8 `ret 8`)
    // 0x28e5 (EEALasso.cpp; called from 0x1973e8): builds the selection message for a lasso over inRectangle (inFlags
    // 2: add and remove, 1: add; unit type 0xdf: select the patrol group inPatrolGroup) and posts the action
    void SelectUnits(const GERectangle& inRectangle, long inFlags, unsigned long inUnitType, long inPatrolGroup);

    EEMessage* mMessage;                    // +0x0c (the original's EEMSelectUnits*, also for the patrol group's)
};

#endif
