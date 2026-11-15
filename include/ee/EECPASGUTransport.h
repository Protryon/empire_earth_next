// C:\EEX\Empire Earth\AI Thread\EECPASGUTransport.h (RECONSTRUCTION; file name inferred). An
// attack sub group unit kind (RTTI EECPASGUTransport > EECPAttackSubGroupUnit; vtable 0x439ad4; sizeof 0x84; mType 4;
// EECPASGUTransport.cpp 0xd5d8..0xe9a1). Requires ee/EECPAttackSubGroupUnit.h.
#ifndef EE_EECPASGUTRANSPORT_H
#define EE_EECPASGUTRANSPORT_H

class EECPASGUTransport : public EECPAttackSubGroupUnit {
public:
    EECPASGUTransport(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup);   // 0xd5d8
    EECPASGUTransport(EECPAttackSubGroup* inSubGroup);  // 0xd617
    virtual ~EECPASGUTransport();           // 0xd636
    virtual bool Retarget();                // +0x14 0x27c0b4 (`xor al, al`, ICF-shared)
    virtual void SetState(long inState, unsigned long inTime);    // +0x28 0xd641
    virtual bool CanAttack(EECPASTargetUnit* inTarget);    // +0x2c 0xbb118 (`xor al, al; ret 4`, ICF-shared)
    virtual bool IsInDanger();              // +0x30 0xd651
    virtual bool CheckCondition(long inState, unsigned long inTime);  // +0x50 0xd6d3
    virtual void Update(unsigned long inTime);      // +0x6c 0xd21f (only the base's; ICF: EECPASGUSea's)
};

#endif
