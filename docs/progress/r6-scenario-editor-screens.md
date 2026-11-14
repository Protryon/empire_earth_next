# R6 scenario editor screens

The lane's state, as the other lanes' files in this folder. Each round rewrites this file to the current state;
the round's story goes in PORT_LEARNINGS.md.

**Rounds 1-2** (`ee_screens`, branches `rust-editor1`, `rust-editor2`; PORT_LEARNINGS "R6: the scenario editor
screens, round 1" and "round 2"): `src/Empire Earth/Scenario Editor` (46 units, 23.3k lines at the start) measured
per unit. Whole in Rust: the player tab (`editor_players.rs`: `EESEPlayerScreen` with its rows and game options
page), the trigger tab's object screen (`editor_object.rs`: `EESETSObject`) and trigger screen
(`editor_trigger.rs`: `EESETSTrigger`); each control a record of one shared maker (`EeScreensSeplCtl`, the host
`SeplMake` in EESERBUnitTypes.cpp), each screen's world calls one host function (`ee_screenscpp_sepl`, `_seob`,
`_setr`, split by op number). Net C++: -1,321, then -1,306. The editor gate (`tools/sim_editor.py`) dumps every
control of the form, the players, the open scenario object and trigger, and drives the player tab, the object and
trigger screens and the unit tab (6,559 lines); controls are named by form index (`%N`). Next: the unit tab
(`EESEUnit`, gated, its placement state read from outside first), the player effects (`EESETSETPlayer`), the editor
form (`EEUIFScenarioEditor`), `EESETriggerScreen`.
