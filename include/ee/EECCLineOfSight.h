// C:\EEX\Empire Earth\Calamities\EECCLineOfSight.h (RECONSTRUCTION; file name inferred). The line-of-sight
// Layout proven: EECCLineOfSight sizeof 0x2c = its new-site size, ctor 0xb1f4f exact in EECCLineOfSight.cpp. Member
// names are provisional.
// calamity class: raises the caster's sight while it lasts (vtable 0x43cacc). EECCLineOfSight.cpp is
// 0xb1f4f..0xb2034. sizeof 0x2c. Requires ee/EEGlobalCalamityClass.h.
#ifndef EE_EECCLINEOFSIGHT_H
#define EE_EECCLINEOFSIGHT_H

class EECCLineOfSight : public EEGlobalCalamityClass {
public:
    // +0x3c 0xb202f: 0 (the cloaking class's slot is folded into this copy)
    virtual long CanCastAt(EEComplexUnit* inUnit, GE3DPoint& inLocation, unsigned long inX, unsigned long inY,
                           bool inCheck, EEUnit* inTargetUnit);
};

#endif
