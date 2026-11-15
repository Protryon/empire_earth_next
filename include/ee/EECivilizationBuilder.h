// EECivilizationBuilder.h (RECONSTRUCTION; file name inferred; provisional names). The civilization builder's entry
// points: Civilization Builder/EECivilizationBuilder.cpp (0x27efca..0x27fdfe) and the unit after it,
// EECBCivilizationFiles.cpp (0x27fdfe..0x2813f4). The builder is Rust's whole (ee_screens civ_whole.rs: the forms'
// helpers, the timer, the edited civilization, the files' writing, the builder's events' ops); these forward.
#ifndef EE_EECIVILIZATIONBUILDER_H
#define EE_EECIVILIZATIONBUILDER_H
#include "ee/EECivUpgrade.h"

class EECivilization;
class UString;

extern bool& gAllowCustomCivs;              // 0x531256 (a scenario game: the timer opens the builder itself)
extern bool& gIsScenarioGame;               // 0x531259 (with gAllowCustomCivs: no premade-civilization check)

long CivBuilderEditCivilization();          // 0x280faf (pauses, then opens the builder for the current player)
long CivBuilderCreateCivilization(EECivilization* inCivilization, bool inReload);
long CivBuilderSaveCivilization(const UString& inName, bool inOverwrite);  // 0x28015d (the edited civilization as inName)
void CivBuilderEditCurrentCivilization();   // 0x280bb1 (the current player's civilization edited)
void CivBuilderFreeCivilization();          // the edited civilization deleted (delete gEditableCivilization)
// EEProcessTimerExpired: ee/EEUserInterface.h; EESetCivBuilderButtonBlinking: ee/EmpireEarth.h

#endif
