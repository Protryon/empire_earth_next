// C:\EEX\Empire Earth\Calamities\EELocationCalamityClass.h (RECONSTRUCTION; file name inferred). The calamity
// classes cast at a location (RTTI: EECCAreaEffect > EELocationCalamityClass > EECalamityClass; its sibling is
// EEGlobalCalamityClass). EELocationCalamityClass.cpp is 0xb6f21..0xb6fb1. Its vtable is not in the image (an
// abstract class whose implicit ctor/dtor are inline: their vptr stores are dead). Requires ee/EECalamityClass.h.
#ifndef EE_EELOCATIONCALAMITYCLASS_H
#define EE_EELOCATIONCALAMITYCLASS_H

class EELocationCalamityClass : public EECalamityClass {
public:
    // 0xb6f21 (EECalamityManager 0xae64a calls it): a new calamity of this class at inLocation, Reset (provisional
    // name)
    EECalamity* CreateCalamity(GE3DPoint& inLocation, EEUnit* inCasterUnit);
    virtual long GetTargetKind();           // +0x14 0xb6f6a: 2
    // +0x3c 0xb6f6e: EECalamityClass's, then 1 when Virtual4C refuses the location
    virtual long CanCastAt(EEComplexUnit* inUnit, GE3DPoint& inLocation, unsigned long inX, unsigned long inY, bool inCheck,
                           EEUnit* inTargetUnit);
    // +0x4c (the last slot): the shared rule with the class's own fields (ee_tech calamity_cast.rs, calamity_class.rs)
    virtual bool IsValidLocation(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY);
};

#endif
