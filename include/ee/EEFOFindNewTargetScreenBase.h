// EEFOFindNewTargetScreenBase.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names).
// Classes: EEFOFindNewTargetScreenBase.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: -.
#ifndef EE_EEFOFINDNEWTARGETSCREENBASE_H
#define EE_EEFOFINDNEWTARGETSCREENBASE_H

class EEComplexUnit;

// vtable 0x449ab8; bases EEFOFindNewTargetScreenBase; sizeof ?; ctor ?; ??_G 0x239ec5 (dtor inline)
class EEFOFindNewTargetScreenBase {
public:
    EEFOFindNewTargetScreenBase() {}        // inline
    virtual ~EEFOFindNewTargetScreenBase() {} // +0x00 (inline: ??_G 0x239ec5)
    // +0x04/+0x08: a unit passes the screen (the finder 0x245dd6 calls them on its candidates)
    virtual bool Virtual04(EEComplexUnit*) { return true; }   // +0x04 0x220a18 NEW trivial: mov al, 1; ret 4 (ICF: Component@WONAPI, EEFOLocationScreenBase...)
    virtual bool Virtual08(EEComplexUnit*) { return true; }   // +0x08 0x220a18 NEW trivial: mov al, 1; ret 4 (ICF: Component@WONAPI, EEFOLocationScreenBase...)
};

#endif
