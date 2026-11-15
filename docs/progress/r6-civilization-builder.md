# R6 civilization builder

The lane's state, as the other lanes' files in this folder. Each round rewrites this file to the current state;
the round's story goes in PORT_LEARNINGS.md.

**Round 1** (`ee_screens`, branch `rust-civbuilder1`; PORT_LEARNINGS "R6: the civilization builder, round 1"):
`src/Empire Earth/Civilization Builder` without the area effects (the calamities lane's): 7 units, 3,761 lines at
the start (EECivilizationBuilder 617, EECBUIListBox 835, EECBCivilizationFiles 848, EECBEditableCivilization 197,
EECBCivilizationCreator 78, EECivPowersManager 271, EECivilizationManager 915) and their headers. The builder's
events were already Rust (`civ_builder.rs`, game screens round 9). Whole in Rust now (`civ_whole.rs` with
`civ_edit.rs` and `civ_list.rs`, one host `ee_screenscpp_cb` in EECivilizationBuilder.cpp): every builder helper
(`CivBuilder*`, `EEShowCivBuilderPoints`, `EESetCivBuilderButtonBlinking`, `EEProcessTimerExpired`), the builder's
state, the edited civilization (`EECBEditableCivilization`, `EECBCivilizationCreator`: units and headers deleted),
the civilization file's writing (keyed text, else the legacy binary form), the list box's logic (`EECBUIListBox`:
the class keeps its base, members and one-line forwarders). The builder's events' C++ host cases in EEScreens.cpp
are gone (their ops route to `civ_whole::event_call`). Net C++ (src, include/ee): the gate +433 (TEST ONLY), the
move -1,806 (the builder -1,819). The folder's units now: EECivilizationBuilder (the host, 506), EECBUIListBox
(239), EECBCivilizationFiles (291, the simulation's half), EECivilizationManager, EECivPowersManager.

**Stays C++**: the simulation's half of EECBCivilizationFiles.cpp (`EELoadCivilization`, which the AI reaches through
`EEApplyCivilizationFile`, `EECBRequestUpgrades`, the timer limits, the civilizations' directory),
`EECivilizationManager` (the categories, the premade civilizations, the upgrade ids and keys, `ApplyUpgradeLevel`'s
stat changes: simulation) and `EECivPowersManager` (`ApplyPower`: simulation). Both are data plus simulation; a
move needs the oracle and the capture profiles, not this lane's gate.

**The gate**: `python -I tools/sim_editor.py --civbuilder` (EE-SIM's scripted editor, `tools/sim_editor/civbuilder.txt`
against `civbuilder_expected.txt`, the "cb" steps in `src/Empire Earth/Empire Earth/EESimCivBuilder.cpp`). It opens
the builder as a person does (the editor's builder button: the creator, unlimited and with points; the game's edit
of the current player's civilization; the civilization timer: the predefined civilizations' choice, or the builder
with custom civilizations) and drives forms 7, 15, 16, 17, 35 and the message boxes; it digests every control of a
form (the lists' every line, flags, selection), the form stack, the editor's and players' civilizations, the edited
civilization and the civilization files' bytes. Recorded on the unmodified builder (twice identical, 3,913 lines);
the move passes it unchanged; 5 of 6 Rust mutations change it (the sixth, the overwrite answer's close flag, is not
observable in EE-SIM). The ai-game gate opens and closes the builder in the real game (form 7, Exit).

**A person only**: the list's drawing (`DrawTextA`: EE-SIM draws nothing; the ai-game gate opens and dismisses the
builder in the real game), the mouse's clicks on the lists (the gate fires their events), multiplayer's paths (the
request to the server, `CanChange`'s multiplayer rule, the not-spent-all question), a campaign's map name.

**Next**: `EECivilizationManager`'s data half into a Rust record (the categories, premade civilizations, keys:
`ee_data`/`ee_tech` own the tables), then its `ApplyUpgradeLevel` and `EECivPowersManager::ApplyPower` with the
simulation lane's gates (oracle, captures); `EELoadCivilization`'s byte parsing beside `civ_whole::legacy_file`.
