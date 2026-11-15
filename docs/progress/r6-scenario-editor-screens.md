# R6 scenario editor screens

The lane's state, as the other lanes' files in this folder. Each round rewrites this file to the current state;
the round's story goes in PORT_LEARNINGS.md.

**Rounds 1-6** (`ee_screens`, branches `rust-editor1` .. `rust-editor6`; PORT_LEARNINGS "R6: the scenario editor
screens, round 1" .. "round 6"): `src/Empire Earth/Scenario Editor` (46 units, 23.3k lines at the start; 35 units,
13.1k lines now) measured per unit. Whole in Rust: the player tab (`editor_players.rs`), the unit tab
(`editor_unit.rs`; its build tools and small control classes stay C++), the trigger tab (`editor_tscreen.rs`) with
its five sub-screens (`editor_trigger.rs`, `editor_object.rs`, `editor_subscreens.rs` for the condition and effect
screens, round 6 `editor_area.rs` for `EESETSArea`), the condition and effect types (`editor_conditions.rs`,
`editor_effects*.rs`), round 6 the condition and effect controls' constructors and destructors as tables
(`editor_controls.rs`), the map tab (`editor_map.rs`: `EEUIFSEMap`) and the instructions tab
(`editor_instructions.rs`: `EESEInstructions`). Each control a record of one shared maker (`EeScreensSeplCtl`,
the host `SeplMake` in EESERBUnitTypes.cpp); each screen's world one host function split by op number
(`ee_screenscpp_sepl` below 100, `_seob` 100.., `_setr` 200.., `_seun` 300.., `_sets` 400.., `_semp` 500..,
`_sear` 600.., `_sein` 700..; the condition and effect screens share the template `SeetOp` over
`ee_screenscpp_seet` / `_sect`). Net C++: -1,321, -1,306, -564, about -5,700, -1,177, then about -1,210 (round 6,
the gate's TEST ONLY lines aside).

The editor gate (`tools/sim_editor.py`, 14,049 lines) dumps every control of the form, the players, the open
scenario object and trigger, every effect's and condition's numbers and texts, the area screen and the scenario's
areas (`areastate`), the labels' alignments the types set (`align`, a TEST ONLY record in the types' host), the
scenario's media texts (`media`), and drives every tab above but the elevation and terrain tabs' controls (their
paint actions are driven directly).

Left, by size: the editor form (`EEUIFScenarioEditor.cpp`, 1,495 lines: its members are read across 16 units —
`mTabGroups`, `mBrushGroup`, `mLoadThread`, `mIsUIHidden`, `mEdgeScroll` ...: accessors first; its trigger-only
report (Ctrl+S) reads the scenario manager's internals and is better written over ee_scenario's records once the
scenario manager lane has them), the terrain tab (`EEUIFSETerrain.cpp`, 857: its lists draw textures), the tech list
box (`EESEUITechListBox.cpp`, 595), the load thread (385), the events' host (`EESEEvents.cpp`), the terrain paint
radios (`EEUIFSETRBPaintType.cpp`), the elevation tab, the unit tab's small control classes (`EEUIFSEBRotation`, the
two lists), the hosts of the moved screens (EESERBUnitTypes.cpp, EESEUnit.cpp, EESETSEffect.cpp, EESETSObject.cpp).
