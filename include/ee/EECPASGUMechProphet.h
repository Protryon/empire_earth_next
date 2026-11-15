// C:\EEX\Empire Earth\AI Thread\EECPASGUMechProphet.h (RECONSTRUCTION; file name inferred). An
// attack sub group unit kind (RTTI EECPASGUMechProphet > EECPASGUProphet > EECPAttackSubGroupUnit; vtable 0x4394f0;
// sizeof 0x94; mType 0xa; EECPASGUMechProphet.cpp 0x7c45..0x908a). Requires ee/EECPASGUProphet.h.
#ifndef EE_EECPASGUMECHPROPHET_H
#define EE_EECPASGUMECHPROPHET_H

class EECPASGUMechProphet : public EECPASGUProphet {
public:
    EECPASGUMechProphet(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup); // 0x7c45
    EECPASGUMechProphet(EECPAttackSubGroup* inSubGroup);    // 0x7c84
    virtual ~EECPASGUMechProphet();         // 0x7ca3
    virtual bool Retarget();                // +0x14 0x7efc (ICF: EECPASGUHero's)
    virtual void SetState(long inState, unsigned long inTime);    // +0x28 0x7f2d
    virtual bool CheckCondition(long inState, unsigned long inTime);  // +0x50 0x7f41
    virtual void Update(unsigned long inTime);      // +0x6c 0x7f51
    // +0x84 0x7cae: its own analyzer set (Teleport, ShieldBattery, ...)
    virtual EECPASGUProphetCalamityAnalyzerBase* CreateAnalyzer(long inTechID, unsigned long inType);
};

#endif
