# R6 scenario editor screens

The lane's state, as the other lanes' files in this folder. Each round rewrites this file to the current state;
the round's story goes in PORT_LEARNINGS.md.

**Rounds 1-7** (`ee_screens`, branches `rust-editor1` .. `rust-editor7`; PORT_LEARNINGS "R6: the scenario editor
screens, round 1" .. "round 7"): `src/Empire Earth/Scenario Editor` (46 units, 23.3k lines at the start; 35 units,
about 11.8k lines now) measured per unit. Whole in Rust: the editor form (round 7, `editor_form.rs`:
`EEUIFScenarioEditor`, its controls, tabs, flags and selection as one slot array, the trigger-only save's report over
the scenario's records), the elevation tab (round 7, `editor_elevation.rs`), the player tab (`editor_players.rs`), the
unit tab (`editor_unit.rs`; its build tools and small control classes stay C++), the trigger tab (`editor_tscreen.rs`)
with its five sub-screens (`editor_trigger.rs`, `editor_object.rs`, `editor_subscreens.rs` for the condition and effect
screens, `editor_area.rs` for `EESETSArea`), the condition and effect types (`editor_conditions.rs`,
`editor_effects*.rs`), the condition and effect controls' constructors and destructors as tables
(`editor_controls.rs`), the map tab (`editor_map.rs`) and the instructions tab (`editor_instructions.rs`). Each
control a record of one shared maker (`EeScreensSeplCtl`, the host `SeplMake` in EESERBUnitTypes.cpp); each screen's
world one host function split by op number (`ee_screenscpp_sepl` below 100, `_seob` 100.., `_setr` 200.., `_seun`
300.., `_sets` 400.., `_semp` 500.., `_sear` 600.., `_sein` 700.., round 7 `_sefm` 800.. (the form,
EEUIFScenarioEditor.cpp, a static member `EEUIFScenarioEditor::Host` for UIForm's protected calls) and `_seel` 900..
(the elevation tab); the condition and effect screens share the template `SeetOp` over `ee_screenscpp_seet` /
`_sect`). Net C++: -1,321, -1,306, -564, about -5,700, -1,177, about -1,210, then -1,287 (round 7: the form -525, the elevation tab -123, the
hosts' unused includes -639; the gate's TEST ONLY lines +141).

The form's members other units read are accessors now (`GetTabGroup`, `GetLoadThread`, `GetBrushGroup`,
`GetButtonTexture`, `GetEdgeScroll`, `GetPanelBackground`, `GetPlayerComboBox`, `GetIsUIHidden`, `GetWasUIHidden`,
`SetExitAfterSave`, `GetSelectedUnit`, `GetSlot` for the gate); the class keeps only its civilization (`mC1A0`,
the builder's) and its Rust object.

The editor gate (`tools/sim_editor.py`, 14,260 lines) dumps every control of the form, the players, the open
scenario object and trigger, every effect's and condition's numbers and texts, the area screen and the scenario's
areas, the labels' alignments, the scenario's media texts, round 7 the form's own state (`formstate`) and the
trigger-only save's report (`trigreport`), and drives every tab but the terrain tab's controls (its paint actions
are driven directly), the form's selection members (`fsel`), the load text, the game view, a store and a resume.

Left, by size: the terrain tab (`EEUIFSETerrain.cpp`, 857: its lists draw textures; keep the terrain sort, a
`std::sort` of localized names, in the host), the tech list box (`EESEUITechListBox.cpp`, 595: draws; its
`Fill`/`Rebuild` and sorts could move), the load thread (385), the events' host (`EESEEvents.cpp`), the brush group
(`EEUIEOpenCivBuilder.cpp`, its radios a custom class), the terrain paint radios' unit (now the events' host), the
unit tab's small control classes (`EEUIFSEBRotation`, the two lists), the hosts of the moved screens
(EESERBUnitTypes.cpp, EESEUnit.cpp, EESETSEffect.cpp, EESETSObject.cpp, EEUIFScenarioEditor.cpp). A whole move frees
about half of a unit: the hosts' world calls stay C++.
