# R6 scenario editor screens

The lane's state, as the other lanes' files in this folder. Each round rewrites this file to the current state;
the round's story goes in PORT_LEARNINGS.md.

**Rounds 1-4** (`ee_screens`, branches `rust-editor1` .. `rust-editor4`; PORT_LEARNINGS "R6: the scenario editor
screens, round 1" .. "round 4"): `src/Empire Earth/Scenario Editor` (46 units, 23.3k lines at the start, 36 units
now) measured per unit. Whole in Rust: the player tab (`editor_players.rs`: `EESEPlayerScreen` with its rows and
game options page), the trigger tab's object screen (`editor_object.rs`: `EESETSObject`) and trigger screen
(`editor_trigger.rs`: `EESETSTrigger`), the unit tab (`editor_unit.rs`: `EESEUnit`; its build tools and small
control classes stay C++), the trigger tab itself (`editor_tscreen.rs`: `EESETriggerScreen`) and, round 4, the
effect screen's seven effect types with their 44 actions (`editor_effects.rs`, `editor_effects_object.rs`,
`editor_effects_player.rs`, `editor_effects_more.rs`: `EESETSEffectType` and every `EESETSET*` class; the ten
`EESETSET*.cpp` units deleted). Each control a record of one shared maker (`EeScreensSeplCtl`, the host `SeplMake`
in EESERBUnitTypes.cpp), each screen's world one host function split by op number (`ee_screenscpp_sepl` below 100,
`_seob` 100.., `_setr` 200.., `_seun` 300.., `_sets` 400..); the effect types have one generic host,
`ee_screenscpp_seet` (EESETSEffect.cpp), which addresses `EESETSEffectControls` by array and index. Net C++:
-1,321, -1,306, -564, then about -5,700.

The editor gate (`tools/sim_editor.py`, 11,673 lines) dumps every control of the form, the players, the open
scenario object and trigger, every effect's numbers and texts (`effectstate`), and drives the player tab, the object
and trigger screens, the unit tab's lists and placement state, the trigger tab's list and sub-tabs (with a stored
selection past its list), and every effect type and action shown, stored and loaded back; controls are named by
form index (`%N`).

Next: the condition types (`EESETSConditionType.cpp`, 888 lines: the same shape as the effect types over
`EESETSConditionControls`, so the generic host's pattern applies), then the effect and condition screens
(`EESETSEffect`, `EESETSCondition`) and the area screen (`EESETSArea`); the editor form (`EEUIFScenarioEditor`,
1,497 lines; accessors first); the unit tab's small control classes (`EEUIFSEBRotation`, the two lists).
