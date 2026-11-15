// C:\EEX\Empire Earth\AI Thread\EECPASGUAirBomber.h (RECONSTRUCTION; file name inferred). An
// attack sub group unit kind (RTTI EECPASGUAirBomber > EECPAttackSubGroupUnit; vtable 0x439160; sizeof 0x84; mType 1;
// EECPASGUAirBomber.cpp 0x5d47..0x6132): both ctors set the kind in the body; the dtor is user-declared (own vptr,
// then a jump to the base dtor). Requires ee/EECPAttackSubGroupUnit.h.
#ifndef EE_EECPASGUAIRBOMBER_H
#define EE_EECPASGUAIRBOMBER_H

class EECPASGUAirBomber : public EECPAttackSubGroupUnit {
public:
    EECPASGUAirBomber(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup);   // 0x5d47
    EECPASGUAirBomber(EECPAttackSubGroup* inSubGroup);  // 0x5d86
    virtual ~EECPASGUAirBomber();           // 0x5da5
    virtual bool Retarget();                // +0x14 0x5db0
    virtual void Flee();                    // +0x1c 0xe123d (`ret`, ICF-shared)
    // +0x34 0x631c: only the base's (ICF: EECPASGUAirFighter's, the kept copy)
    virtual void ApplyPrejudices(EECPASTargetUnit* inTarget);
    virtual bool IsTargetValid();           // +0x58 0x60b1 (`mov al, 1`, ICF-shared)
    virtual void Update(unsigned long inTime);      // +0x6c 0x5f58
};

#endif
