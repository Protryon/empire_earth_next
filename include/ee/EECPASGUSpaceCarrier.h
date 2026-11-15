// C:\EEX\Empire Earth\AI Thread\EECPASGUSpaceCarrier.h (RECONSTRUCTION; file name inferred). An
// attack sub group unit kind (RTTI EECPASGUSpaceCarrier > EECPAttackSubGroupUnit; vtable 0x439a50; sizeof 0x84;
// mType 0xd; EECPASGUSpaceCarrier.cpp 0xd3d6..0xd5d8). Requires ee/EECPAttackSubGroupUnit.h.
#ifndef EE_EECPASGUSPACECARRIER_H
#define EE_EECPASGUSPACECARRIER_H

class EECPASGUSpaceCarrier : public EECPAttackSubGroupUnit {
public:
    EECPASGUSpaceCarrier(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup);    // 0xd3d6
    EECPASGUSpaceCarrier(EECPAttackSubGroup* inSubGroup);   // 0xd415
    virtual ~EECPASGUSpaceCarrier();        // 0xd434
    virtual bool Retarget();                // +0x14 0xd43f
};

#endif
