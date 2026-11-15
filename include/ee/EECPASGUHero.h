// C:\EEX\Empire Earth\AI Thread\EECPASGUHero.h (RECONSTRUCTION; file name inferred). An attack
// sub group unit kind (RTTI EECPASGUHero > EECPASGUProphet > EECPAttackSubGroupUnit; vtable 0x43936c; sizeof 0x94;
// mType 9; EECPASGUHero.cpp 0x6391..0x6547). Requires ee/EECPASGUProphet.h.
#ifndef EE_EECPASGUHERO_H
#define EE_EECPASGUHERO_H

class EECPASGUHero : public EECPASGUProphet {
public:
    EECPASGUHero(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup);    // 0x6391
    EECPASGUHero(EECPAttackSubGroup* inSubGroup);       // 0x63d0
    virtual ~EECPASGUHero();                // 0x63ef
    virtual bool Retarget();                // +0x14 0x7efc (ICF: EECPASGUMechProphet's, the kept copy)
    virtual void SetState(long inState, unsigned long inTime);    // +0x28 0x63fa (only EECPASGUProphet's)
    virtual bool IsInDanger();              // +0x30 0x640a (the base's, else Virtual74())
    virtual bool CheckCondition(long inState, unsigned long inTime);  // +0x50 0x64fd
    virtual void Update(unsigned long inTime);      // +0x6c 0x653b (only EECPASGUProphet's)
    virtual bool ShouldRetreat();           // +0x74 0x641f
};

#endif
