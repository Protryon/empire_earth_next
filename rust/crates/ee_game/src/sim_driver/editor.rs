//! EE-SIM's scenario editor script (TEST ONLY; EE-SIM.exe --editor-script, tools/sim_editor.py): the runner and the
//! script's language. EE-SIM opens the scenario editor as a person does (main menu -> Editors -> Scenario Editor: a
//! local server, load type 3, the editor form over a default map), then this runs a script: one editor action per
//! line, built and posted as the editor's tabs build and post them, and digests of the world in between. Each line is
//! a step, run by the host (EESimEditor.cpp `EESimEditorStep`: an action of its own in the UI thread's action queue,
//! then a marker; the clock pinned by step). The physics never runs: the editor's actions are its only writes to the
//! world. The output is "step <n> <the line>" and the step's own lines.
//!
//! Script lines ('#' starts a comment; tiles and units are the world's; a unit is its id or "@X,Y", the last
//! player's unit on that tile):
//!   tab N                                 the editor's tab N (the tab button's CheckDirect)
//!   blank WIDTH HEIGHT TERRAIN            EESEACreateBlankMap (the map tab's elevation file: none)
//!   random SIZE STYLE                     EESEACreateRandomMap
//!   terrain TERRAIN LEFT TOP RIGHT BOTTOM EESEAPaintTerrain (the terrain's own colour type) over an area
//!   bulldoze LEFT TOP RIGHT BOTTOM        EESEAPaintMoutains (the terrain tab's bulldozer: the units there removed)
//!   elevation raise|hills|level LEVEL LEFT TOP RIGHT BOTTOM   EESEAPaintElevation, the elevation tab's radio checked
//!   area X0 Y0 X1 Y1                      EESEAPaintArea (the trigger areas' rectangle)
//!   place PLAYER TYPE X Y YAW [ANIMATION] EESEAPlaceUnit with one entry (what Place makes of a placeable ghost)
//!   select|toggle|add UNIT, clear         EESEAMouseClick with the select, toggle, always-add, unselect-all message
//!   mouseclick X Y [ROTATING]             EESEAMouseClick::Execute with the mouse at the screen point (X, Y)
//!   lasso select|add|addremove L T R B [TYPE]   EESEALasso with that EEMSelectUnits over the screen rectangle
//!   lassorect L T R B                     EESEALasso::SelectInRectangle (EE-SIM draws nothing: no unit is picked)
//!   selection                             the editor's selected units
//!   units                                 every unit's id, type, player and tile (the world flushed)
//!   rotate YAW                            EESEARotateUnits (the selection)
//!   delete UNIT | delete PLAYER group|first   EESEADeleteUnit
//!   pickup UNIT, putdown X Y YAW          EESEAMoveUnit: PickUp, then PutDown over its ghosts put on the tile, turned
//!   wall PLAYER TECHNOLOGY X0 Y0 X1 Y1    EESEAPlaceWall::Place
//!   gate PLAYER TECHNOLOGY X Y DIRECTION  EESEAPlaceGate::Place (no ghosts: placed)
//!   quicksave NAME, savefile NAME         EESEAQuickSaveScenario over EESEAQuickSaveLoad; the written file's chunks
//!   elevfile NAME W H WCUT WDEPTH LHEIGHT LCUT   an elevation file pair NAME.dat and NAME.txt (W x H heights, a
//!                                         fixed pattern of land and water) written to data\Scenarios\Elevation Maps,
//!                                         the map tab's list refilled and NAME picked, its four boxes set (the next
//!                                         blank map is made from it); "elevfile -" picks none
//!   ghost PLAYER TYPE X Y YAW             the placement ghosts (EEGhostUnit::Create) put on the tile (X, Y) as
//!                                         EEPreviewPlacement puts them under the mouse (FillSquare: placeable or not)
//!   ghostat X Y                           the ghosts there are moved to the tile (X, Y) (a picked-up unit's ghost)
//!   placeghost PLAYER [UNIT]              EESEAPlaceUnit::Place over the ghosts (UNIT: the unit tab's unit under the
//!                                         mouse, a transport to board)
//!   drop [UNIT]                           EESEAMoveUnit::PutDown over the ghosts (UNIT as placeghost's)
//!   cancelmove                            EESEAMoveUnit::Cancel (the picked-up unit deleted)
//!   check ID 0|1                          the editor's check box ID set (UICheckBox::SetChecked: its trigger events
//!                                         fire, e.g. the hide-UI box's EESEEHideUI)
//!   view                                  the game view's rectangle, the editor's UI hidden flag
//!   click quicksave                       the toolbar's quick save button released (its EESEEQuickSave), then the
//!                                         written file's chunks (data\Scenarios\EEQuickSave.scn's)
//!   digest LABEL                          the map's tiles and mesh, every unit, the selection, the paint area, and
//!                                         the game's save chunks (the header, which holds the date, left out;
//!                                         four chunks differ by run: ChunkByRun)
//!   sm OP LIST [INDEX] | sm fill | sm state  the scenario manager's lists: an editor list operation (new, sort, up,
//!                                         down, delete, dup), index references set on every element, every element's
//!                                         references and the name lists (ScenarioListStep)
//!   sm text | sm export | sm import FILE KIND VALIDATE   the trigger text file (EESEIAExport, EESEIAImport): the
//!                                         effects' and conditions' texts set, ExportText's file, ImportTriggerText
//!                                         over a fixed file (ScenarioTextStep)
//!   enter ID                              an edit box's Enter key (its ProcessEnterKey); an ID "%N" anywhere is the
//!                                         form's control N (the trigger screens repeat ids)
//!   tscreen select N|new|dup|delete|up|down|sort|name   the trigger tab's list and its buttons (EESETriggerScreen)
//!   tsclick select|add UNIT               the object screen's click (EESETSAMouseClick: the selection, then its list)
//!   object                                the trigger tab's open scenario object (ObjectDigest)
//!   triggerstate                          the trigger tab's open trigger (TriggerDigest)
//!   fire ID                               a control's button-up event processed
//!   areaset AREA L T R B | areapoint AREA X Y   a scenario area's rectangle or anchor set (as the area screen's Store)
//!   areastate                             the area screen's state and centre, the paint area, the scenario's areas
//!   align c|e                             the condition's or effect's labels' alignments (- never set)
//!   seed                                  the user interface's random seed (the map tab's CreateMap)
//!   media                                 the scenario's media texts (their lengths, first 60 bytes), the export file
//!   terrainstate | terrainpick LINE | terraincolor LINE   round 8: the terrain tab (TerrainDigest), a list's line
//!                                         clicked (its event fires)
//!   terrainpaint L T R B                  the terrain paint area's CreatePaintAction over an area, posted
//!   terrainmouse X Y | terrainmouse update   the terrain tab's MouseMove / Update; brushhl X Y CENTERED SetHighlight
//!   techfill ID FIRST LAST | techstate    a tech list's Fill; the two tech lists (TechDigest: ids, lines, CopyData's marks)
//! Out of the script's reach (a person's): the mouse's picking (EEPreviewPlacement's screen point, the lasso's and
//! the click's picking), the save and test screens (EESEASaveScenario, EESEATestScenario, EESEAExitScenario), the tech
//! lists (EESEATechTree), the load thread's texts (EESEAChangeLoadText).

/// A script line's words (blanks, tabs and carriage returns between them).
pub fn words(line: &str) -> Vec<String> {
    line.split([' ', '\t', '\r']).filter(|w| !w.is_empty()).map(str::to_string).collect()
}

/// The script's lines (split at each new line; a last line without one kept when not empty).
pub fn lines(script: &[u8]) -> Vec<String> {
    let mut out: Vec<String> = script.split(|&b| b == b'\n').map(|l| String::from_utf8_lossy(l).into_owned()).collect();
    if out.last().is_some_and(|l| l.is_empty()) {
        out.pop();
    }
    out
}

/// What a step's host answered.
#[derive(Debug, PartialEq)]
pub enum Step {
    /// done: its output lines
    Done(Vec<String>),
    /// not a command
    Unknown,
    /// not finished within a minute
    Unfinished,
}

/// The script `path` run: each line's step through `step` (the comment from '#' cut, blank lines skipped), the
/// output through `out` -> `Err((code, message))` (2 unreadable or unknown, 3 unfinished; the messages the C++'s).
pub fn run(
    path: &str,
    step: &mut dyn FnMut(&[String]) -> Step,
    out: &mut dyn FnMut(&[u8]),
) -> Result<(), (i32, String)> {
    let script = std::fs::read(path).map_err(|_| (2, format!("cannot read {path}")))?;
    for (k, raw) in lines(&script).iter().enumerate() {
        let w = words(raw.split('#').next().unwrap_or(""));
        if w.is_empty() {
            continue;
        }
        match step(&w) {
            Step::Unknown => return Err((2, format!("{path}:{}: unknown command {}", k + 1, w[0]))),
            Step::Unfinished => return Err((3, format!("{path}:{}: '{}' did not finish", k + 1, w[0]))),
            Step::Done(output) => {
                let mut text = format!("step {} {raw}\n", k + 1);
                for l in output {
                    text += &l;
                    text.push('\n');
                }
                out(text.as_bytes());
            }
        }
    }
    Ok(())
}
