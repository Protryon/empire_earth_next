# R6 scenario editor screens

The lane's state, as the other lanes' files in this folder. Each round rewrites this file to the current state;
the round's story goes in PORT_LEARNINGS.md.

**Rounds 1-8** (`ee_screens`, branches `rust-editor1` .. `rust-editor8`; PORT_LEARNINGS "R6: the scenario editor
screens, round 1" .. "round 8"): `src/Empire Earth/Scenario Editor` (46 units, 23.3k lines at the start; 35 units
now, 9,636 lines with the address markers gone). Whole in Rust: the editor form (`editor_form.rs`), the terrain tab
(round 8, `editor_terrain.rs`: `EEUIFSETerrain`, its lists' events, the paint radios' `Check`, the paint area's
`CreatePaintAction`; the two list boxes' draws and the terrain name sort stay in its host), the brush group's
controls and highlight square (round 8, `editor_brush.rs`; the group itself stays a C++ record the paint areas and
tabs read), the elevation tab (`editor_elevation.rs`), the player tab (`editor_players.rs`), the unit tab
(`editor_unit.rs`; its build tools and small control classes stay C++), the trigger tab (`editor_tscreen.rs`) with
its five sub-screens (`editor_trigger.rs`, `editor_object.rs`, `editor_subscreens.rs`, `editor_area.rs`), the
condition and effect types (`editor_conditions.rs`, `editor_effects*.rs`), the controls' tables
(`editor_controls.rs`), the map tab (`editor_map.rs`) and the instructions tab (`editor_instructions.rs`). Each
control a record of one shared maker (`EeScreensSeplCtl`, the host `SeplMake` in EESERBUnitTypes.cpp); each screen's
world one host function split by op number (`ee_screenscpp_sepl` below 100, `_seob` 100.., `_setr` 200.., `_seun`
300.., `_sets` 400.., `_semp` 500.., `_sear` 600.., `_sein` 700.., `_sefm` 800.., `_seel` 900.., round 8 `_setn`
1000.. (the terrain tab and the brush radios, EEUIFSETerrain.cpp); a screen's Rust host routes an op to whichever
host owns its range, so the terrain tab reuses the elevation host's mouse and highlight ops and the map host's
control by id). Net C++ by round: -1,321, -1,306, -564, about -5,700, -1,177, about -1,210, -1,287, then -86 (round 8: the terrain tab -42, the brush group -44; the remaining units' unused includes -977 apart; the gate's and the fixture's TEST ONLY lines +155).

The editor gate (`tools/sim_editor.py`, 14,448 lines) dumps every control of the form, the players, the open
scenario object and trigger, every effect's and condition's numbers and texts, the area screen and the scenario's
areas, the form's own state and the trigger-only save's report, round 8 the terrain tab (`terrainstate`: its
terrains and colour types, both lists, the preview's texture and corners, the radios, the display modes, the paint
area's centring, the brush group) and the tech lists (`techstate`: ids, lines, CopyData's marks), and drives every
tab (round 8: the terrain lists' clicks, `CreatePaintAction`, the mouse, `SetHighlight`, a tech list's `Fill`).

The harness: `tools/harness/fixture_game.py --exercise editor` (round 8, TEST ONLY) opens the editor from the menus
(the fixture's `clickevent`), captures its first frame and closes it without saving (`screenevent`): the editor's
first frame and its destructor in a real session.

Left, by what each still holds (PORT_LEARNINGS round 8 "The measure first"): the tech list box's
`Fill`/`Rebuild`/`CopyData` (about 135 lines of decisions; `Rebuild`'s `std::sort` ties), the paint areas' mouse
(EEUIFSEPaint, EEUIFSEPaintElevation, about 200 lines of tile and brush math), the unit tab's small classes
(EEUIFSEBRotation's steps and angles, the unit and cargo lists' sorted inserts), the trigger tab's combo boxes'
right-click menus (EESESCComboBox, EESETSTrigger), EESETSUICBPlayer, EESEBSelect, EESEIAExport, the load thread's
dispatch. Everything else is the hosts' world statements: a whole move now frees little C++ (the terrain tab -42,
the brush group -44).
