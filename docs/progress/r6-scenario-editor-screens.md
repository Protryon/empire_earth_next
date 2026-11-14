# R6 scenario editor screens

The lane's state, as the other lanes' files in this folder. Each round rewrites this file to the current state;
the round's story goes in PORT_LEARNINGS.md.

**Rounds 1-3** (`ee_screens`, branches `rust-editor1`, `rust-editor2`, `rust-editor3`; PORT_LEARNINGS "R6: the
scenario editor screens, round 1", "round 2" and "round 3"): `src/Empire Earth/Scenario Editor` (46 units, 23.3k
lines at the start) measured per unit. Whole in Rust: the player tab (`editor_players.rs`: `EESEPlayerScreen` with
its rows and game options page), the trigger tab's object screen (`editor_object.rs`: `EESETSObject`) and trigger
screen (`editor_trigger.rs`: `EESETSTrigger`), the unit tab (`editor_unit.rs`: `EESEUnit`; its build tools and small
control classes stay C++) and the trigger tab itself (`editor_tscreen.rs`: `EESETriggerScreen`; its five
sub-screens stay C++ objects of the shell). Each control a record of one shared maker (`EeScreensSeplCtl`, the host
`SeplMake` in EESERBUnitTypes.cpp), each screen's world one host function split by op number (`ee_screenscpp_sepl`
below 100, `_seob` 100.., `_setr` 200.., `_seun` 300.., `_sets` 400..). Net C++: -1,321, -1,306, then -564. The
editor gate (`tools/sim_editor.py`) dumps every control of the form, the players, the open scenario object and
trigger, and drives the player tab, the object and trigger screens, the unit tab's lists and placement state and the
trigger tab's list (6,823 lines); controls are named by form index (`%N`). Next: the gate's gap (a sub-tab entered
with its stored selection past its list), the effect types (`EESETSETPlayer` and the other `EESETSET*` units: their
Show/Load/Store reconfigure `EESETSEffectControls`, so a generic effect-controls host first), the editor form
(`EEUIFScenarioEditor`), the unit tab's small control classes (`EEUIFSEBRotation`, the two lists).
