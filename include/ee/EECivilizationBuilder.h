// EECivilizationBuilder.h (RECONSTRUCTION; file name inferred; provisional names). The civilization builder's entry
// points: Civilization Builder/EECivilizationBuilder.cpp (0x27efca..0x27fdfe) and the unit after it,
// EECBCivilizationFiles.cpp (0x27fdfe..0x2813f4). The builder is Rust's whole (ee_screens civ_whole.rs: the forms'
// helpers, the timer, the edited civilization, the files' writing, the builder's events' ops); these forward.
#ifndef EE_EECIVILIZATIONBUILDER_H
#define EE_EECIVILIZATIONBUILDER_H
#include "ee/EECivUpgrade.h"
#include "rust/game/screens.h"   // the builder (civ_whole.rs)

class EECivilization;
class UString;


inline long CivBuilderEditCivilization() { return ee_screens_cb_edit_civilization(); }   // 0x280faf (pauses, then opens the builder for the current player)
inline long CivBuilderCreateCivilization(EECivilization* inCivilization, bool inReload) { return ee_screens_cb_create_civilization(inCivilization, inReload); }
long CivBuilderSaveCivilization(const UString& inName, bool inOverwrite);  // 0x28015d (the edited civilization as inName)
inline void CivBuilderEditCurrentCivilization() { ee_screens_cb_edit_current(); }   // 0x280bb1 (the current player's civilization edited)
inline void CivBuilderFreeCivilization() { ee_screens_cb_free_civilization(); }   // the edited civilization deleted (delete gEditableCivilization)
// EEProcessTimerExpired: ee/EEUserInterface.h; EESetCivBuilderButtonBlinking: ee/EmpireEarth.h

#endif
