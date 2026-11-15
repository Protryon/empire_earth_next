// C:\EEX\Empire Earth\AI Thread\EECPASGUMechPriest.h (RECONSTRUCTION; file name inferred). An
// attack sub group unit kind (RTTI EECPASGUMechPriest > EECPASGUPriest > EECPAttackSubGroupUnit; vtable 0x439404;
// sizeof 0x84; mType 7; EECPASGUMechPriest.cpp 0x6547..0x7c45, which also holds the TimeWarp/Teleport/ShieldBattery
// analyzer bodies and two screens). Requires ee/EECPASGUPriest.h.
#ifndef EE_EECPASGUMECHPRIEST_H
#define EE_EECPASGUMECHPRIEST_H

class EECPASGUMechPriest : public EECPASGUPriest {
public:
    EECPASGUMechPriest(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup);  // 0x6547
    EECPASGUMechPriest(EECPAttackSubGroup* inSubGroup); // 0x6586
    virtual ~EECPASGUMechPriest();          // 0x65a5
    // +0x6c 0xd21f: only the base's (ICF: EECPASGUSea's, the kept copy)
    virtual void Update(unsigned long inTime);
};

#endif
