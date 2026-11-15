# R6 scenario editor screens

The lane's state, as the other lanes' files in this folder. Each round rewrites this file to the current state;
the round's story goes in PORT_LEARNINGS.md.

**Rounds 1-5** (`ee_screens`, branches `rust-editor1` .. `rust-editor5`; PORT_LEARNINGS "R6: the scenario editor
screens, round 1" .. "round 5"): `src/Empire Earth/Scenario Editor` (46 units, 23.3k lines at the start, 35 units
now) measured per unit. Whole in Rust: the player tab (`editor_players.rs`: `EESEPlayerScreen` with its rows and
game options page), the trigger tab's object screen (`editor_object.rs`: `EESETSObject`) and trigger screen
(`editor_trigger.rs`: `EESETSTrigger`), the unit tab (`editor_unit.rs`: `EESEUnit`; its build tools and small
control classes stay C++), the trigger tab itself (`editor_tscreen.rs`: `EESETriggerScreen`), round 4 the effect
screen's seven effect types with their 44 actions (`editor_effects*.rs`: `EESETSEffectType` and every `EESETSET*`
class), round 5 the condition screen's seven condition types (`editor_conditions.rs`: `EESETSConditionType` and
every `EESETSCT*` class) and the condition and effect screens themselves (`editor_subscreens.rs`: `EESETSCondition`,
`EESETSEffect`). Each control a record of one shared maker (`EeScreensSeplCtl`, the host `SeplMake` in
EESERBUnitTypes.cpp), each screen's world one host function split by op number (`ee_screenscpp_sepl` below 100,
`_seob` 100.., `_setr` 200.., `_seun` 300.., `_sets` 400..); the condition and effect types and screens share one
generic host template (`SeetOp` in EESETSEffect.cpp, over `EESETSConditionControls` or `EESETSEffectControls` by
array and index; `ee_screenscpp_sect` and `ee_screenscpp_seet` add the entry's own ops). Net C++: -1,321, -1,306,
-564, about -5,700, then -1,177.

The editor gate (`tools/sim_editor.py`, 12,723 lines) dumps every control of the form, the players, the open
scenario object and trigger, every effect's and condition's numbers and texts (`effectstate`, `conditionstate`), and
drives the player tab, the object and trigger screens, the unit tab's lists and placement state, the trigger tab's
list and sub-tabs (with a stored selection past its list), every effect type and action and every condition type
shown, stored and loaded back, the AI attribute's float slider, the condition and effect screens' list buttons and
texts; controls are named by form index (`%N`).

Next: the area screen (`EESETSArea`, 484 lines; its mouse drawing is world and input calls only a person reaches:
give the gate a way to set an area's rectangle first), the condition and effect controls' constructors
(`EESETSCComboBox.cpp`, `EESETSEComboBox.cpp`: about 30 controls each, a table for the shared maker with a few new
kinds: the condition and effect combo box classes, a slider without range, a two-sided list box), the editor form
(`EEUIFScenarioEditor`, 1,495 lines; accessors first); the unit tab's small control classes (`EEUIFSEBRotation`, the
two lists).
