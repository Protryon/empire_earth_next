// New (port; docs/RUST_PLAN.md R5b.4 leaf 6, the twelfth round): a unit class's Update as a Rust sequence
// (rust/crates/ee_units/src/update.rs): which of the unit's per-tick leaves run, in which order, decided over the unit
// store, each leaf a call of this host (EEUCLand.cpp). inKind: ee_units' EE_UNITS_UPDATE_*; inResourceUnit: the
// class's UpdateResourceUnit (EEUCNaval's, for its fish), NULL for none.
#ifndef EE_EEUNITUPDATE_H
#define EE_EEUNITUPDATE_H

class EEUnitClass;
class EEComplexUnit;

void EEUnitClassUpdate(int inKind, EEUnitClass* inClass, EEComplexUnit* inUnit,
                       void (*inResourceUnit)(EEUnitClass*, EEComplexUnit*));

#endif
