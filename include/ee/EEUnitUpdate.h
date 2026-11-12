// New (port; docs/RUST_PLAN.md R5b.4 leaf 6, the twelfth and thirteenth rounds): a unit class's Update as a Rust
// sequence (rust/crates/ee_units/src/update.rs): which of the unit's per-tick leaves run, in which order, decided over
// the unit store, each leaf a call of this host (EEUnitUpdate.cpp). inKind: ee_units' EE_UNITS_UPDATE_*.
#ifndef EE_EEUNITUPDATE_H
#define EE_EEUNITUPDATE_H

class EEUnitClass;
class EEComplexUnit;

void EEUnitClassUpdate(int inKind, EEUnitClass* inClass, EEComplexUnit* inUnit);
// a class's ActionChanged as a Rust decision (rust/crates/ee_units/src/actions.rs; inKind: EE_UNITS_ACTION_*)
void EEUnitClassActionChanged(int inKind, EEUnitClass* inClass, EEComplexUnit* inUnit);

#endif
