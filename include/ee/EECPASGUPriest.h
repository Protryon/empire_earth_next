// C:\EEX\Empire Earth\AI Thread\EECPASGUPriest.h (RECONSTRUCTION; file name inferred). An
// attack sub group unit kind (RTTI EECPASGUPriest > EECPAttackSubGroupUnit; vtable 0x4396bc; sizeof 0x84; mType 6;
// ctors 0x8de3/0x8e22, dtor 0x8e41, inside EECPASGUPCTargetIsUnitSetScreenBase.cpp's mapped range 0x908a..: the
// name sorts after EECPASGUPCTargetIsUnitSetScreen*, so 0x8de3..0x908a may be its own unit). The base of
// EECPASGUMechPriest. Requires ee/EECPAttackSubGroupUnit.h.
#ifndef EE_EECPASGUPRIEST_H
#define EE_EECPASGUPRIEST_H

class EECPASGUPriest : public EECPAttackSubGroupUnit {
public:
    EECPASGUPriest(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup);  // 0x8de3
    EECPASGUPriest(EECPAttackSubGroup* inSubGroup);     // 0x8e22
    virtual ~EECPASGUPriest();              // 0x8e41
    // +0x10 0x8e6f: the base's choice, kept only when 0x242542(mUnit, its unit) holds
    virtual EECPASTargetUnit* ChooseTarget(bool inTakeFirst, EETileCoordinate& outTile);
    virtual bool Retarget();                // +0x14 0x8ea0
    virtual bool CanAttack(EECPASTargetUnit* inTarget);    // +0x2c 0x8f37
    virtual float GetTargetDamage(EECPASTargetUnit* inTarget);  // +0x64 0x8e4c (min(1, hit points / maximum))
};

#endif
