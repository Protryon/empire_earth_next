// C:\EEX\Empire Earth\Calamities\EEUnitCalamityClass.h (RECONSTRUCTION; file name inferred). The calamity
// classes cast on a unit (RTTI EECCConscription > EEUnitCalamityClass > EECalamityClass). EEUnitCalamityClass.cpp is
// 0xb70cc..0xb71b5 (after EEPlagueFunctor.cpp). Its vtable is not in the image (abstract, inline ctor/dtor).
// Requires ee/EECalamityClass.h.
#ifndef EE_EEUNITCALAMITYCLASS_H
#define EE_EEUNITCALAMITYCLASS_H

class EEUnitCalamityClass : public EECalamityClass {
public:
    // 0xb70cc (provisional name): a new calamity of this class on inTargetUnit, at its tile, Reset
    EECalamity* CreateCalamity(EEUnit* inTargetUnit, EEUnit* inCasterUnit);
    virtual long GetTargetKind();           // +0x14 (ICF-folded 0x23c6cd: 3)
    // +0x18 0xb712a: the id of the unit under the mouse, -1 if none
    virtual long GetDefaultTarget(GE3DPoint&, unsigned long, unsigned long, bool);
    // +0x3c 0xb7157: EECalamityClass's, then 1 unless Virtual4C accepts the target
    virtual long CanCastAt(EEComplexUnit* inUnit, GE3DPoint& inLocation, unsigned long inX, unsigned long inY,
                           bool inCheck, EEUnit* inTargetUnit);
    // +0x4c: the class's functor's CanInfect for the fire and the plague, else the shared rule (ee_tech
    // calamity_cast.rs)
    virtual bool IsValidTarget(EEComplexUnit* inUnit, EEUnit* inTargetUnit);
};

#endif
