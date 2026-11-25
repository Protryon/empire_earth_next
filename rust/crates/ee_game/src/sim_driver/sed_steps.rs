//! TEST ONLY: the scripted scenario editor's steps (EE-SIM.exe --editor-script; was EESimEditor.cpp's
//! `EESimEditorStep`): one script line's command built and run as a step on the UI thread (the host's
//! `ee_simcpp_sed_run`: an action of its own in the UI thread's action queue, so under the UI copy lock and the
//! world's write lock, as every editor action runs, then a marker; the clock pinned by step). The language is
//! [`super::editor`]'s; the C++ objects are reached through [`super::sed_host`].
#![allow(clippy::unnecessary_cast)] // c_long and c_ulong are 32 bits on Windows, 64 on LP64

use crate::gamecpp as gc;
use std::ffi::{c_long, c_ulong};
use std::sync::atomic::{AtomicUsize, Ordering};

use ee_base::math::transform::RotationOrder;
use ee_base::math::types::GeTransformation;
use ee_render::gx::cformat::{atof, strtoul_0};

use super::editor::Step;
use super::options::c_atol;
use super::sed_digest::{self as d, g, head_of, rect_text};
use super::sed_host::{self as x, h, hf, hs, ht, sel, EeSimDriverSed as O};
use crate::screens::editor_form::{
    EE_SCREENS_SEFM_CLEAR_SELECTION, EE_SCREENS_SEFM_DESELECT_FIRST, EE_SCREENS_SEFM_DESELECT_UNIT,
    EE_SCREENS_SEFM_RESUME_GROUP, EE_SCREENS_SEFM_SELECT_UNIT, EE_SCREENS_SEFM_SET_GAME_VIEW, EE_SCREENS_SEFM_STORE_GROUP,
};
use crate::screens::editor_tscreen::EE_SCREENS_SETS_SCREEN_REFRESH;
use crate::screens::hud_ctl::EeScreensPoint;
use crate::screens::mouse_modes::{EeScreensBuildGateFields, EeScreensBuildWallFields};
use crate::screens::shells::EE_SCREENS_SKOP_SET_SELECTED_PLAYER;

// the editor's long-lived actions (the tabs keep one of each and post it again)
static ROTATE: AtomicUsize = AtomicUsize::new(0);
static MOVE: AtomicUsize = AtomicUsize::new(0);
static PLACE: AtomicUsize = AtomicUsize::new(0);

/// The action in `slot`, made of `kind` the first time.
fn kept(slot: &AtomicUsize, kind: u32) -> usize {
    if slot.load(Ordering::Relaxed) == 0 {
        slot.store(x::sea_new(kind, 0, &[]), Ordering::Relaxed);
    }
    slot.load(Ordering::Relaxed)
}

// the editor's whole actions' kinds (ee/EESEARust.h kSEA*)
const SEA_QUICK_SAVE: u32 = 4;
const SEA_ROTATE: u32 = 6;
const SEA_PAINT_AREA: u32 = 7;
const SEA_BLANK_MAP: u32 = 8;
const SEA_RANDOM_MAP: u32 = 9;
const SEA_PLACE_WALL: u32 = 10;
const SEA_PLACE_GATE: u32 = 11;
const SEA_PLACE_UNIT: u32 = 12;
const SEA_MOVE_UNIT: u32 = 13;
const SEA_DELETE_UNIT: u32 = 14;
// the paints (kSEAPaint*)
const PAINT_TERRAIN: i64 = 0;
const PAINT_ELEVATION: i64 = 1;
const PAINT_BULLDOZER: i64 = 2;
// the messages (ee/EEMessageID.h)
const MESSAGE_SELECT_UNIT: i64 = 0x48;
const MESSAGE_ADD_UNIT_TO_SELECTED_GROUP: i64 = 0x49;
const MESSAGE_UNSELECT_ALL_UNITS: i64 = 0x4b;
const MESSAGE_SELECT_UNITS: i64 = 0x4e;
const MESSAGE_ADD_UNITS_TO_SELECTED_GROUP: i64 = 0x4f;
const MESSAGE_ADD_AND_REMOVE_UNITS: i64 = 0x50;
const MESSAGE_ALWAYS_ADD_UNIT_TO_SELECTED_GROUP: i64 = 0x76;
// the trigger tab's members (editor_tscreen_ffi.rs EE_SCREENS_SETS_*)
const SETS_ALPHA_SORT: u32 = 1;
const SETS_SELECT_ENTRY: u32 = 2;
const SETS_DEC_PRIORITY: u32 = 5;
const SETS_DELETE_ENTRY: u32 = 6;
const SETS_DUPLICATE_ENTRY: u32 = 7;
const SETS_SHOW_TAB: u32 = 8;
const SETS_INC_PRIORITY: u32 = 16;
const SETS_NEW_ENTRY: u32 = 18;
const SETS_SELECT_NAME: u32 = 20;
const SETS_LIST: u32 = 1;
const SETS_CURRENT_SCREEN: u32 = 5;
// the unit tab's (editor_unit_ffi.rs EE_SCREENS_SEUN_*)
const SEUN_SELECT: u32 = 10;
const SEUN_GET_UNDER_MOUSE: u32 = 22;
const SEUN_GET_FLAGS: u32 = 23;
const SEUN_GET_LISTED_MODE: u32 = 24;
const SEUN_GET_LISTED_SORT: u32 = 25;
const SEUN_GET_LISTED_LAYER: u32 = 26;
// the sub-screens' (EE_SCREENS_SESC_*)
const SESC_CHANGE_TYPE: u32 = 13;
const SESC_CHANGE_ACTION: u32 = 14;

/// An identity transformation turned by `yaw` (SetOrientationYPR) and put at (x, y, z) (SetPosition).
fn placed(yaw: f32, x: f32, y: f32, z: f32) -> GeTransformation {
    let mut t = GeTransformation::IDENTITY;
    t.set_orientation(RotationOrder::Ypr, yaw, 0.0, 0.0);
    t.m[0][3] = x;
    t.m[1][3] = y;
    t.m[2][3] = z;
    t
}

/// A tile's height, 0 off the map.
fn tile_height(tx: c_long, ty: c_long) -> f32 {
    let tile = h(O::MapTile, 0, &[tx as i64, ty as i64]) as usize;
    if tile != 0 {
        gc::hd(gc::O::TileHeight, tile, &[], &[])
    } else {
        0.0
    }
}

/// `sscanf(text, "%ld,%ld")`: both numbers.
pub fn two_longs(text: &str) -> Option<(c_long, c_long)> {
    fn number(b: &[u8], mut i: usize) -> Option<(c_long, usize)> {
        while i < b.len() && (b[i] == b' ' || (9..=13).contains(&b[i])) {
            i += 1;
        }
        let start = i;
        if i < b.len() && (b[i] == b'-' || b[i] == b'+') {
            i += 1;
        }
        let digits = i;
        while i < b.len() && b[i].is_ascii_digit() {
            i += 1;
        }
        if i == digits {
            return None;
        }
        Some((std::str::from_utf8(&b[start..i]).ok()?.parse::<i64>().ok()? as c_long, i))
    }
    let b = text.as_bytes();
    let (x, i) = number(b, 0)?;
    if b.get(i) != Some(&b',') {
        return None;
    }
    let (y, _) = number(b, i + 1)?;
    Some((x, y))
}

/// A unit by its id, or "@X,Y": the last unit (a player's, not the world's) on that tile.
pub fn unit_ref(word: &str) -> c_ulong {
    let Some(rest) = word.strip_prefix('@') else { return strtoul_0(word) as c_ulong };
    let Some((tx, ty)) = two_longs(rest) else { return 0xffff_ffff };
    if gc::h(gc::O::EcHasMap, 0, &[]) == 0 {
        return 0xffff_ffff;
    }
    let tile = h(O::MapTile, 0, &[tx as i64, ty as i64]) as usize;
    let mut id: c_ulong = 0xffff_ffff;
    let mut n = if tile != 0 { gc::h(gc::O::TileUnits, tile, &[]) as usize } else { 0 };
    while n != 0 {
        let u = gc::h(gc::O::MmNodeUnit, n, &[]) as usize;
        if u != 0 {
            let p = h(O::NodeUnitPlayer, u, &[]) as usize;
            if p != 0 && gc::h(gc::O::TcPlayerId, p, &[]) != 0 {
                id = gc::h(gc::O::RoUnitId, u, &[]) as c_ulong;
            }
        }
        n = gc::h(gc::O::MmNodeNext, n, &[]) as usize;
    }
    id
}

/// The ghosts put on the tile (X, Y) as EEPreviewPlacement puts them under the mouse over the map: each on the tile
/// (a farm ghost on its slot), at the tile's centre (an even size: its corner) at the mesh's height, FillSquare
/// deciding whether it can be placed, its tile from its class.
fn put_ghosts(tx: c_long, ty: c_long) {
    gc::h(gc::O::RoGhostLock, 0, &[]);
    h(O::GhostsChanged, 0, &[]);
    let n = h(O::GhostCount, 0, &[]) as c_ulong;
    for i in 0..n {
        let ghost = h(O::GhostAt, 0, &[i as i64]) as usize;
        h(O::GhostSetTileX, ghost, &[tx as i64]);
        h(O::GhostSetTileY, ghost, &[ty as i64]);
        let kind = h(O::GhostUnitType, ghost, &[]) as c_long;
        // kUnitTypeFarm, kUnitTypeRoboticFarm
        if (kind == 0x2a || kind == 0x33a) && h(O::GhostCount, 0, &[]) != 1 {
            h(O::GhostSetFarmSlot, ghost, &[i as i64]);
        }
        let half = if (h(O::GhostTileSize, ghost, &[]) & 1) == 0 { 0.0f32 } else { 0.5f32 };
        let (gx, gy) = (h(O::GhostTileX, ghost, &[]) as c_long, h(O::GhostTileY, ghost, &[]) as c_long);
        let px = gx as f32 + half;
        let py = gy as f32 + half;
        let mut pz = 0.0f32;
        if h(O::MapGetTile, 0, &[gx as i64, gy as i64]) != 0 {
            let mesh = h(O::MapMesh, 0, &[]) as usize as *const ee_render::ge::tmobj::EeGeTerrainMesh;
            // SAFETY: the map's live mesh; the tile under the point exists
            pz = unsafe { ee_render::ge::tmobj::ee_ge_tm_mesh_z(mesh, px, py) };
            if pz < 0.0 {
                pz = 0.0;
            }
        }
        hf(O::GhostSetPosition, ghost, &[], &[px, py, pz]);
        h(O::GhostFillSquare, ghost, &[]);
        hf(O::GhostLocationToTile, ghost, &[], &[px, py, pz]);
        let (gx, gy) = (h(O::GhostTileX, ghost, &[]), h(O::GhostTileY, ghost, &[]));
        if h(O::MapGetTile, 0, &[gx, gy]) == 0 {
            h(O::GhostSetCanPlace, ghost, &[0]);
        }
    }
    gc::h(gc::O::RoGhostUnlock, 0, &[]);
}

/// The ghosts' tiles and whether each can be placed (the step's output).
fn ghost_text() -> String {
    let mut s = "  ghosts".to_string();
    gc::h(gc::O::RoGhostLock, 0, &[]);
    for i in 0..h(O::GhostCount, 0, &[]) {
        let ghost = h(O::GhostAt, 0, &[i]) as usize;
        s += &format!(
            " {}@{},{}{}",
            h(O::GhostUnitType, ghost, &[]) as c_long,
            h(O::GhostTileX, ghost, &[]) as c_long,
            h(O::GhostTileY, ghost, &[]) as c_long,
            if gc::h(gc::O::RoGhostCanPlace, ghost, &[]) != 0 { "+" } else { "-" }
        );
    }
    gc::h(gc::O::RoGhostUnlock, 0, &[]);
    s
}

/// The ghosts cleared (the move's and the placement's end).
fn clear_ghosts() {
    gc::h(gc::O::RoGhostLock, 0, &[]);
    #[cfg(not(test))]
    crate::gamecpp::world::ee_gamecpp_world_clear_ghosts(
        core::ptr::addr_of_mut!(crate::gamecpp::global_objects::gWorld).cast(),
    );
    gc::h(gc::O::RoGhostUnlock, 0, &[]);
}

/// An elevation file's heights: a ridge of land rising to 300 across the middle, water down to -200 at the two
/// ends (row by row, as shorts).
pub fn elevation_heights(width: c_long, height: c_long) -> Vec<i16> {
    let mut data = Vec::with_capacity((width * height).max(0) as usize);
    for yy in 0..height {
        for xx in 0..width {
            data.push((300 - ((2 * yy - height).abs() * 500) / height + (xx * 37) % 41 - 20) as i16);
        }
    }
    data
}

/// An elevation file pair: NAME.txt holds "W H", NAME.dat W x H shorts.
fn write_elevation_file(name: &str, width: c_long, height: c_long) {
    const DIRECTORY: &str = "data\\Scenarios\\Elevation Maps\\";
    let dir = x::spec_in(DIRECTORY);
    gc::h(gc::O::FileSpecCreateDirectory, dir, &[]);
    gc::h(gc::O::DxDelete, dir, &[]);
    let data: Vec<u8> = elevation_heights(width, height).iter().flat_map(|v| v.to_le_bytes()).collect();
    let info = format!("{width} {height}\n");
    for (extension, bytes) in [(".dat", data.as_slice()), (".txt", info.as_bytes())] {
        let file = x::spec_in(DIRECTORY);
        hs(O::SpecSetFile, file, &format!("{name}{extension}"));
        gc::h(gc::O::UsgCreate, file, &[]);
        let mut written: c_ulong = 0;
        if h(O::SpecOpenWrite, file, &[]) == 0 {
            h(O::SpecWrite, file, &[bytes.as_ptr() as i64, bytes.len() as i64, &mut written as *mut c_ulong as i64]);
            gc::h(gc::O::FileClose, file, &[]);
        }
        gc::h(gc::O::DxDelete, file, &[]);
    }
}

/// A control's event `trigger` processed by the editor form (none: nothing).
fn fire(control: usize, trigger: c_ulong) {
    let e = x::trigger_event(control, trigger);
    if e != 0 {
        x::process_event(e, control);
    }
}

/// A paint action over an area, posted when it paints anything.
fn paint(a: usize, area: [c_long; 4]) {
    if h(O::PaintSetArea, a, &area.map(|v| v as i64)) != 0 {
        x::post_action(a);
    }
    x::deref(a);
}

/// The text of a word ('_' a space).
fn spaced(word: Option<&String>) -> String {
    word.map_or_else(String::new, |w| w.replace('_', " "))
}

type Code<'a> = x::StepCode<'a>;

/// One script line's command (its words) -> its output lines (`Step::Done`), `Unknown`, or `Unfinished` (the step
/// did not finish within a minute).
pub fn step(w: &[String]) -> Step {
    let l = |i: usize| if i < w.len() { c_atol(&w[i]) } else { 0 };
    let f = |i: usize| if i < w.len() { atof(&w[i]) as f32 } else { 0.0 };
    let c = w[0].as_str();
    let w1 = w.get(1).map_or("", |s| s.as_str());
    let code: Code = match c {
        "tab" => Box::new(|_: &mut Vec<String>| {
            x::shell_op(x::slot(19 + l(1) as u32), crate::screens::shells_editor::EE_SCREENS_SKOP_CHECK_DIRECT, 0, 0);
        }),
        "blank" => Box::new(|_: &mut Vec<String>| x::deref(x::sea_new(SEA_BLANK_MAP, 0, &[l(1), l(2), l(3)]))),
        "random" => Box::new(|_: &mut Vec<String>| x::deref(x::sea_new(SEA_RANDOM_MAP, 0, &[l(1), l(2)]))),
        "terrain" => Box::new(|_: &mut Vec<String>| {
            let color = h(O::TerrainColorType, 0, &[l(1) as i64]);
            let a = h(O::PaintNew, 0, &[PAINT_TERRAIN, l(1) as i64, color]) as usize;
            paint(a, [l(2), l(3), l(4), l(5)]);
        }),
        "bulldoze" => Box::new(|_: &mut Vec<String>| {
            let a = h(O::PaintNew, 0, &[PAINT_BULLDOZER, 0, 0]) as usize;
            paint(a, [l(1), l(2), l(3), l(4)]);
        }),
        "elevation" => Box::new(|_: &mut Vec<String>| {
            // kSEElevationRaiseButton, kSEElevationLowerButton, kSEElevationLevelButton
            let button = match w1 {
                "raise" => 0x96,
                "hills" => 0x97,
                _ => 0xa0,
            };
            h(O::RadioCheck, x::control_by_id(button), &[]);
            let a = h(O::PaintNew, 0, &[PAINT_ELEVATION, l(2) as i8 as i64, 0]) as usize;
            paint(a, [l(3), l(4), l(5), l(6)]);
        }),
        "area" => Box::new(|_: &mut Vec<String>| {
            let a = x::sea_new(SEA_PAINT_AREA, 0, &[l(1), l(2)]);
            // SAFETY: the action's Rust record
            unsafe { x::ee_screens_sew_area_corner(x::sea_action(a), l(3) as i32, l(4) as i32) };
            x::deref(a);
        }),
        "place" => Box::new(|_: &mut Vec<String>| {
            // place PLAYER TYPE X Y YAW [ANIMATION]
            let a = x::sea_new(SEA_PLACE_UNIT, 0, &[]);
            let t = placed(f(5), f(3), f(4), tile_height(f(3) as c_long, f(4) as c_long));
            let animation = if w.len() > 6 { l(6) } else { 7 };
            // SAFETY: the action's Rust record, sixteen floats
            unsafe {
                x::ee_screens_sew_place_push(
                    x::sea_action(a),
                    l(1) as i32,
                    l(2) as i32,
                    t.m.as_ptr() as *const f32,
                    animation as i32,
                )
            };
            x::post_action(a);
            x::deref(a);
        }),
        "select" | "toggle" | "add" | "clear" => Box::new(|_: &mut Vec<String>| {
            let a = h(O::ClickNew, 0, &[]) as usize;
            let id = if c == "clear" { 0 } else { unit_ref(w1) };
            let m = match c {
                "select" => x::record(MESSAGE_SELECT_UNIT, Some(id)),
                "toggle" => x::record(MESSAGE_ADD_UNIT_TO_SELECTED_GROUP, Some(id)),
                "add" => x::record(MESSAGE_ALWAYS_ADD_UNIT_TO_SELECTED_GROUP, Some(id)),
                _ => x::record(MESSAGE_UNSELECT_ALL_UNITS, None),
            };
            h(O::ClickSetMessage, a, &[m as i64]);
            x::post_action(a);
            x::deref(a);
        }),
        "mouseclick" => Box::new(|_: &mut Vec<String>| {
            // mouseclick X Y [ROTATING]: EESEAMouseClick::Execute with the mouse at (X, Y)
            hf(O::MouseSetPosition, 0, &[], &[f(1), f(2)]);
            let a = h(O::ClickNew, 0, &[]) as usize;
            hf(O::ClickExecute, a, &[(l(3) != 0) as i64], &[f(1), f(2)]);
            x::deref(a);
        }),
        "lasso" => Box::new(|_: &mut Vec<String>| {
            // lasso select|add|addremove L T R B [TYPE]: EESEALasso with that EEMSelectUnits
            let kind = match w1 {
                "select" => MESSAGE_SELECT_UNITS,
                "add" => MESSAGE_ADD_UNITS_TO_SELECTED_GROUP,
                _ => MESSAGE_ADD_AND_REMOVE_UNITS,
            };
            let a = h(O::LassoNew, 0, &[]) as usize;
            hf(O::LassoSetSelect, a, &[l(6) as i64, kind], &[f(2), f(3), f(4), f(5)]);
            x::post_action(a);
            x::deref(a);
        }),
        "lassorect" => Box::new(|_: &mut Vec<String>| {
            // lassorect L T R B: EESEALasso::SelectInRectangle (deselects, posts)
            let a = h(O::LassoNew, 0, &[]) as usize;
            hf(O::LassoSelectInRectangle, a, &[], &[f(1), f(2), f(3), f(4)]);
            x::deref(a);
        }),
        "units" => Box::new(|out: &mut Vec<String>| {
            // every unit's id, type, player and tile (no hit points: they follow the epochs)
            crate::physics::ee_physics_flush_world(crate::gamecpp::globals::get!(gPhysics));
            for i in 0..gc::h(gc::O::MmNumPlayers, 0, &[]) {
                let p = gc::h(gc::O::SsWorldPlayer, 0, &[i]) as usize;
                if p == 0 {
                    continue;
                }
                let mut u = gc::h(gc::O::PlFirstUnit, p, &[]) as usize;
                while u != 0 {
                    out.push(format!(
                        "  unit {} type={} player={i} tile={},{}",
                        gc::h(gc::O::RoUnitId, u, &[]) as c_ulong,
                        super::sim_ops::h(super::sim_ops::EeSimDriverSim::DmUnitTypeRaw, u, &[]) as c_long,
                        gc::h(gc::O::DkTileX, u, &[]) as c_long,
                        gc::h(gc::O::DkTileY, u, &[]) as c_long
                    ));
                    u = gc::h(gc::O::UnNext, u, &[]) as usize;
                }
            }
        }),
        "selection" => Box::new(|out: &mut Vec<String>| {
            let e = x::editor();
            let ids: String = (0..x::selected_count(e))
                .map(|i| format!(" {}", h(O::SelectedUnitId, e, &[i]) as c_ulong))
                .collect();
            out.push(format!("  selection{ids}"));
        }),
        "rotate" => Box::new(|_: &mut Vec<String>| {
            let a = kept(&ROTATE, SEA_ROTATE);
            // SAFETY: the action's Rust record
            unsafe { x::ee_screens_sew_rotate(x::sea_action(a), f(1)) };
        }),
        "delete" => Box::new(|_: &mut Vec<String>| {
            // delete UNIT | delete PLAYER group|first (the player's selection)
            if w.len() > 2 {
                x::deref(x::sea_new(SEA_DELETE_UNIT, 0, &[-1, l(1), (w[2] == "group") as c_long]));
            } else {
                x::deref(x::sea_new(SEA_DELETE_UNIT, 0, &[unit_ref(w1) as i32 as c_long]));
            }
        }),
        "pickup" => Box::new(|_: &mut Vec<String>| {
            // pickup UNIT: the move's first half (lifted off the map, a ghost at the mouse)
            let a = kept(&MOVE, SEA_MOVE_UNIT);
            // SAFETY: the action's Rust record
            unsafe { x::ee_screens_sew_pick_up(x::sea_action(a), unit_ref(w1) as u32) };
        }),
        "putdown" => Box::new(|_: &mut Vec<String>| {
            // putdown X Y YAW: the picked-up unit's ghosts on the tile, turned, then PutDown
            let a = MOVE.load(Ordering::Relaxed);
            if a == 0 {
                return;
            }
            gc::h(gc::O::RoGhostLock, 0, &[]);
            for k in 0..h(O::GhostCount, 0, &[]) {
                let ghost = h(O::GhostAt, 0, &[k]) as usize;
                let t = placed(f(3), l(1) as f32 + 0.5, l(2) as f32 + 0.5, tile_height(l(1), l(2)));
                h(O::GhostSetTransformation, ghost, &[&t as *const GeTransformation as i64]);
                h(O::GhostSetTileX, ghost, &[l(1) as i64]);
                h(O::GhostSetTileY, ghost, &[l(2) as i64]);
                h(O::GhostSetCanPlace, ghost, &[1]);
            }
            gc::h(gc::O::RoGhostUnlock, 0, &[]);
            // SAFETY: the unit tab's and the action's Rust objects
            unsafe {
                x::ee_screens_seun_set_under_mouse(x::unit_tab(), -1);
                x::ee_screens_sew_put_down(x::sea_action(a));
            }
        }),
        "wall" => Box::new(|_: &mut Vec<String>| {
            // wall PLAYER TECHNOLOGY X0 Y0 X1 Y1
            let a = x::sea_new(SEA_PLACE_WALL, 0, &[]);
            // SAFETY: the action's Rust record
            unsafe { x::ee_screens_sew_wall(x::sea_action(a), f(3), f(4), f(5), f(6), l(1) as i32, l(2) as i32) };
            x::deref(a);
        }),
        "gate" => Box::new(|_: &mut Vec<String>| {
            // gate PLAYER TECHNOLOGY X Y DIRECTION
            let a = x::sea_new(SEA_PLACE_GATE, 0, &[]);
            // SAFETY: the action's Rust record
            unsafe {
                x::ee_screens_sew_gate(
                    x::sea_action(a),
                    l(3) as i32,
                    l(4) as i32,
                    l(1) as i32,
                    l(2) as i32,
                    l(5) as i32,
                )
            };
            x::deref(a);
        }),
        "quicksave" => Box::new(|_: &mut Vec<String>| {
            // quicksave NAME: the toolbar's quick save (written to data\Scenarios)
            let save = hs(O::QuickSaveNew, 0, w1) as usize;
            let a = x::sea_new(SEA_QUICK_SAVE, save, &[]);
            x::post_action(a);
            x::deref(a);
            x::deref(save);
        }),
        "savefile" => Box::new(|out: &mut Vec<String>| {
            // savefile NAME: the body of the quick save NAME
            let name = x::hts(O::SaveTextName, 0, 1, &format!("EEQuickSave{w1}"));
            out.push(format!("savefile {w1}"));
            d::file_digest(&name, out);
        }),
        "elevfile" => Box::new(|out: &mut Vec<String>| {
            // elevfile NAME W H WCUT WDEPTH LHEIGHT LCUT | elevfile -
            let list = x::control_by_id(0x75); // kSEMapElevationFile
            if w1 == "-" {
                h(O::ComboSetSelection, list, &[0]);
                return;
            }
            write_elevation_file(w1, l(2), l(3));
            x::shell_op(list, crate::screens::shells_files::EE_SCREENS_SKOP_FILE_ENTRIES, 0, 0);
            let found = hs(O::FileComboSelect, list, w1) as c_long;
            // kSEMapWaterCutoff, kSEMapWaterDepth, kSEMapLandHeight, kSEMapLandCutoff
            for (i, id) in [0x7d, 0x7c, 0x79, 0x78].into_iter().enumerate() {
                h(O::EditSetNumber, x::control_by_id(id), &[l(4 + i) as c_ulong as i64]);
            }
            out.push(format!("  elevation file {w1} {}", if found == 0 { "listed" } else { "missing" }));
        }),
        "ghost" => Box::new(|out: &mut Vec<String>| {
            // ghost PLAYER TYPE X Y YAW
            hf(O::GhostCreate, 0, &[l(2) as i64, l(1) as i64], &[l(3) as f32 + 0.5, l(4) as f32 + 0.5, f(5)]);
            put_ghosts(l(3), l(4));
            out.push(ghost_text());
        }),
        "ghostat" => Box::new(|out: &mut Vec<String>| {
            put_ghosts(l(1), l(2));
            out.push(ghost_text());
        }),
        "placeghost" | "drop" => Box::new(|out: &mut Vec<String>| {
            // placeghost PLAYER [UNIT] | drop [UNIT]
            crate::physics::ee_physics_flush_world(crate::gamecpp::globals::get!(gPhysics)); // the units placed before are in the world, as a person finds them
            let screen = x::unit_tab();
            let word = if c == "drop" { 1 } else { 2 };
            let under = if w.len() > word { unit_ref(&w[word]) as i32 } else { -1 };
            // SAFETY: the unit tab's and the actions' Rust objects
            unsafe { x::ee_screens_seun_set_under_mouse(screen, under) };
            let mut placed = true;
            if c == "drop" {
                let a = MOVE.load(Ordering::Relaxed);
                if a != 0 {
                    unsafe { x::ee_screens_sew_put_down(x::sea_action(a)) };
                }
            } else {
                let a = kept(&PLACE, SEA_PLACE_UNIT);
                placed = unsafe { x::ee_screens_sew_place_unit(x::sea_action(a), l(1) as i32) } != 0;
            }
            unsafe { x::ee_screens_seun_set_under_mouse(screen, -1) };
            clear_ghosts();
            out.push(format!("  placed {}", if placed { "1" } else { "0" }));
        }),
        "cancelmove" => Box::new(|_: &mut Vec<String>| {
            let a = MOVE.load(Ordering::Relaxed);
            if a != 0 {
                // SAFETY: the action's Rust record
                unsafe { x::ee_screens_sew_cancel(x::sea_action(a)) };
            }
            clear_ghosts();
        }),
        // check ID 0|1: SetChecked, which fires the box's checked or unchecked event. The hide-UI box (0x12) shows
        // the UI while checked: EESEEHideUI posts the check state as EESEEChangeViewport's flag, which calls
        // SetGameView(view, flag == false), so checked -> GetIsUIHidden() false (the original's 0x5af4c1, 0x5af55a)
        "check" => Box::new(|_: &mut Vec<String>| {
            h(O::CheckSetChecked, x::editor_control(w1), &[(l(2) != 0) as i64]);
        }),
        "view" => Box::new(|out: &mut Vec<String>| {
            let r = x::rect(h(O::ViewportRect, 0, &[]) as usize);
            out.push(format!("  view {} hidden={}", rect_text(r), (x::slot(52) != 0) as i32));
        }),
        "click" => Box::new(|_: &mut Vec<String>| {
            // click quicksave
            let button = x::slot(31); // kSlotQuickSaveButton
            x::process_event(x::trigger_event(button, x::TRIGGER_BUTTON_UP), button);
        }),
        "editorsave" => Box::new(|out: &mut Vec<String>| {
            // the editor's own quick save file (EEQuickSave + the scenario extension)
            out.push("editorsave".into());
            d::file_digest(&ht(O::EditorSaveName, 0, 0), out);
        }),
        "trigger" => Box::new(|_: &mut Vec<String>| x::trigger_call(SETS_SHOW_TAB, (0x7d0 + l(1)) as u32)),
        "newentry" => Box::new(|_: &mut Vec<String>| x::trigger_call(SETS_NEW_ENTRY, 0)),
        "ctype" => Box::new(|_: &mut Vec<String>| {
            // ctype TYPE: the condition's type combo box (types from 2), as a person picks it
            // SAFETY: the condition screen's Rust object
            let combo = unsafe { x::ee_screens_secd_handle(x::trigger_sub(1), 1) };
            h(O::ComboSetSelection, combo, &[(l(1) - 2) as c_ulong as i64]);
            x::trigger_sub_op(1, SESC_CHANGE_TYPE);
        }),
        "etype" => Box::new(|_: &mut Vec<String>| {
            // etype TYPE ACTION: the effect's type combo box (types from 1), then its action combo
            // SAFETY: the effect screen's Rust object
            let combo = unsafe { x::ee_screens_sefx_handle(x::trigger_sub(2), 1) };
            h(O::ComboSetSelection, combo, &[(l(1) - 1) as c_ulong as i64]);
            x::trigger_sub_op(2, SESC_CHANGE_TYPE);
            h(O::ComboSetSelection, x::effect_controls().combos[0], &[l(2) as c_ulong as i64]);
            x::trigger_sub_op(2, SESC_CHANGE_ACTION);
        }),
        "ctl" if w.len() > 2 && (w[2] == "list" || w[2] == "combo") => Box::new(|_: &mut Vec<String>| {
            // ctl e list|combo INDEX LINE: an effect list box's line clicked (its selection, then its button-up
            // event) or a combo box's line picked (its selection, then its selection-changed event)
            let t = x::effect_controls();
            let (control, trigger) = if w[2] == "list" {
                let list = t.lists[l(3) as usize];
                h(O::ListChangeSelection, list, &[l(4) as c_ulong as i64, 0, 0]);
                (list, x::TRIGGER_BUTTON_UP)
            } else {
                let combo = t.combos[l(3) as usize];
                h(O::ComboSetSelection, combo, &[l(4) as c_ulong as i64]);
                (combo, x::TRIGGER_SELECTION_CHANGED)
            };
            fire(control, trigger);
        }),
        "ctl" => Box::new(|_: &mut Vec<String>| {
            // ctl c|e check|slide|radio INDEX [VALUE]: a condition (c) or effect (e) control
            let i = l(3) as usize;
            let w2 = w.get(2).map_or("", |s| s.as_str());
            let (check, slider, radio) = if w1 == "c" {
                let t = x::condition_controls();
                match w2 {
                    "check" => (t.checks[i], 0, 0),
                    "slide" => (0, t.slider, 0),
                    _ => (0, 0, t.radios[i]),
                }
            } else {
                let t = x::effect_controls();
                match w2 {
                    "check" => (t.checks[i], 0, 0),
                    "slide" => (0, t.sliders[i], 0),
                    _ => (0, 0, t.radios[i]),
                }
            };
            if check != 0 {
                h(O::CheckSetChecked, check, &[(l(4) != 0) as i64]); // fires the box's checked or unchecked event
            } else if slider != 0 {
                // a person's drag: the value set, then the range-changed event the slider fires
                h(O::SliderSetValue, slider, &[l(4) as i64]);
                fire(slider, x::TRIGGER_RANGE_CHANGED);
            } else if radio != 0 {
                h(O::RadioCheck, radio, &[]); // fires the checked event unless it was checked
            }
        }),
        "ectl" => Box::new(|_: &mut Vec<String>| {
            // ectl edit|edit2|text|edit48 INDEX TEXT | player|scen|ecombo INDEX LINE: an effect control's text typed
            // ('_' a space) or its line set (the player picker's player, a scenario or effect combo box's line;
            // no event: the next Store reads them)
            let t = x::effect_controls();
            let i = l(2) as usize;
            let text = spaced(w.get(3));
            match w1 {
                "edit" | "edit2" | "text" | "edit48" => {
                    let b = match w1 {
                        "edit" => t.edits[i],
                        "edit2" => t.edits2[i],
                        "text" => t.text_edit,
                        _ => t.edit48,
                    };
                    hs(O::EditSetText, b, &text);
                }
                "player" => {
                    x::shell_op(t.players[i], EE_SCREENS_SKOP_SET_SELECTED_PLAYER, l(3) as u32 as i64, 0);
                }
                "scen" => {
                    h(O::ComboSetSelection, t.scenario[i], &[l(3) as c_ulong as i64]);
                }
                _ => {
                    h(O::ComboSetSelection, t.effect_combos[i], &[l(3) as c_ulong as i64]);
                }
            }
        }),
        "cctl" => Box::new(|_: &mut Vec<String>| {
            // cctl edit|text INDEX TEXT | player|combo|scen INDEX LINE: a condition control's text typed ('_' a
            // space) or its line set (no event: the next Store reads them)
            let t = x::condition_controls();
            let i = l(2) as usize;
            let text = spaced(w.get(3));
            match w1 {
                "edit" | "text" => {
                    hs(O::EditSetText, if w1 == "edit" { t.edits[i] } else { t.text_edit }, &text);
                }
                "player" => {
                    x::shell_op(t.players[i], EE_SCREENS_SKOP_SET_SELECTED_PLAYER, l(3) as u32 as i64, 0);
                }
                "scen" => {
                    h(O::ComboSetSelection, t.scenario[i], &[l(3) as c_ulong as i64]);
                }
                _ => {
                    h(O::ComboSetSelection, t.combos[i], &[l(3) as c_ulong as i64]);
                }
            }
        }),
        "crefresh" => Box::new(|_: &mut Vec<String>| x::trigger_sub_slot(1, EE_SCREENS_SETS_SCREEN_REFRESH)),
        "conditionstate" => Box::new(|out: &mut Vec<String>| d::condition_state(out)),
        "areaset" | "areapoint" => Box::new(|_: &mut Vec<String>| {
            // areaset AREA L T R B | areapoint AREA X Y: a scenario area's rectangle or anchor set (EESMArea::
            // SetRectangle, SetAnchor), as the area screen's Store sets them
            let a = h(O::SmGetArea, x::sm(), &[l(1) as c_ulong as i64]) as usize;
            if c == "areaset" {
                hf(O::AreaSetRectangle, a, &[], &[f(2), f(3), f(4), f(5)]);
            } else {
                h(O::AreaSetAnchor, a, &[l(2) as i64, l(3) as i64]);
            }
        }),
        "areastate" => Box::new(|out: &mut Vec<String>| d::area_digest(out)),
        "align" => Box::new(|out: &mut Vec<String>| {
            // align c|e: the condition's or effect's labels' text alignments
            let labels =
                if w1 == "c" { x::condition_controls().labels.to_vec() } else { x::effect_controls().labels.to_vec() };
            let mut s = "  align".to_string();
            for label in labels {
                s += &if h(O::AlignmentHas, label, &[]) != 0 {
                    format!(" {}", h(O::AlignmentGet, label, &[]) as c_ulong)
                } else {
                    " -".to_string()
                };
            }
            out.push(s);
        }),
        "media" => Box::new(|out: &mut Vec<String>| {
            // the scenario's media texts (hints, aftermath, history, instructions) and export file
            let m = x::sm();
            for (k, name) in ["hints", "aftermath", "history", "instructions"].into_iter().enumerate() {
                let text = x::htb(O::SmMediaSection, m, k as i64);
                let head: Vec<u8> = head_of(&text, 60).iter().map(|&b| if b == b'\n' { b'|' } else { b }).collect();
                out.push(format!("  {name} {} '{}'", text.len(), String::from_utf8_lossy(&head)));
            }
            out.push(format!("  export '{}'", ht(O::SmExportFileName, m, 0)));
        }),
        "seed" => Box::new(|out: &mut Vec<String>| {
            out.push(format!("  seed {}", gc::h(gc::O::UiRandomSeed, 0, &[]) as c_long))
        }),
        "erefresh" => Box::new(|_: &mut Vec<String>| x::trigger_sub_slot(2, EE_SCREENS_SETS_SCREEN_REFRESH)),
        "effectstate" => Box::new(|out: &mut Vec<String>| d::effect_state(out)),
        "ui" => Box::new(|out: &mut Vec<String>| d::controls_digest(w1 == "c", out)),
        "combo" | "slider" | "radio" => Box::new(|_: &mut Vec<String>| {
            // combo|slider ID VALUE | radio ID: an editor control by id: a combo box's line picked and its
            // selection-changed event processed, a slider moved and its range-changed event processed, a radio
            // checked (it fires unless it was checked)
            let control = x::editor_control(w1);
            if c == "radio" {
                h(O::RadioCheck, control, &[]);
                return;
            }
            let trigger = if c == "combo" {
                h(O::ComboSetSelection, control, &[l(2) as c_ulong as i64]);
                x::TRIGGER_SELECTION_CHANGED
            } else {
                h(O::SliderSetValue, control, &[l(2) as i64]);
                x::TRIGGER_RANGE_CHANGED
            };
            fire(control, trigger);
        }),
        "controls" => Box::new(|out: &mut Vec<String>| {
            // controls ID...: editor controls' display modes (and a label's first line)
            for word in &w[1..] {
                let control = x::editor_control(word);
                let mut s = format!("  {word} mode={}", x::display_mode(control));
                let kind = h(O::ControlType, control, &[]) as c_ulong;
                if kind == 11 {
                    // kControlTypeTextLabel
                    let first = if h(O::LabelLineCount, control, &[]) > 0 {
                        ht(O::LabelText, control, 0)
                    } else {
                        String::new()
                    };
                    s += &format!(" '{first}'");
                }
                if kind == 12 {
                    // kControlTypeTriStateCheckBox
                    s += &format!(" state={}", h(O::TriStateCheckState, control, &[]) as c_ulong);
                }
                out.push(s);
            }
        }),
        "unitlist" => Box::new(|out: &mut Vec<String>| {
            // the unit tab's unit list (0x3f1): its line count and first lines
            let list = x::editor_control("0x3f1");
            let n = x::line_count(list);
            let mut s = format!("  units {n}:");
            for k in 0..n.min(6) {
                s += &format!(" '{}'", ht(O::ListData, list, k as i64));
            }
            out.push(s);
        }),
        "unittab" => Box::new(|out: &mut Vec<String>| {
            // the unit tab's placement state, its list state, rotation, cargo and ghosts
            let s = x::unit_tab();
            // SAFETY: the unit tab's Rust object
            let get = |what: u32| unsafe { x::ee_screens_seun_get(s, what) } as c_long;
            let angle = unsafe { x::ee_screens_seun_angle(s) };
            let mut line = format!(
                "  unit tab flags={} under={} list={}/{}/{} angle={:.4} ghosts={}",
                get(SEUN_GET_FLAGS),
                get(SEUN_GET_UNDER_MOUSE),
                get(SEUN_GET_LISTED_MODE),
                get(SEUN_GET_LISTED_SORT),
                get(SEUN_GET_LISTED_LAYER),
                angle as f64,
                h(O::GhostCount, 0, &[]) as c_ulong
            );
            let cargo = x::editor_control("0x3f2");
            let n = x::line_count(cargo);
            line += &format!(" cargo {n}:");
            for k in 0..n {
                line += &format!(" '{}'", ht(O::ListData, cargo, k as i64));
            }
            if n > 0 {
                if let Some(selected) = sel(O::ListSelection, cargo) {
                    line += &format!(" selected {selected}");
                }
            }
            out.push(line);
        }),
        "unitpick" => Box::new(|out: &mut Vec<String>| {
            // unitpick NAME: the unit list's first line starting with NAME ('_' a space) picked
            let list = x::editor_control("0x3f1");
            let name = spaced(w.get(1));
            for k in 0..x::line_count(list) {
                let line = x::htb(O::ListData, list, k as i64);
                if !line.starts_with(name.as_bytes()) {
                    continue;
                }
                h(O::ListChangeSelection, list, &[k as i64, 1, 0]);
                x::unit_call(SEUN_SELECT); // as the list's ButtonUp
                out.push(format!("  picked {k} '{}'", String::from_utf8_lossy(&line)));
                return;
            }
            out.push(format!("  no line {name}"));
        }),
        "unitbuild" => Box::new(|_: &mut Vec<String>| {
            // unitbuild wall X0 Y0 X1 Y1 | gate X Y DIRECTION | cancel: the tab's build tools
            // SAFETY: the unit tab group's parts (the EESEUnit)
            let s = unsafe { x::ee_screens_segr_parts(x::tab_group(3)) };
            let flags = unsafe { x::ee_screens_seun_get(x::unit_tab(), SEUN_GET_FLAGS) };
            match w1 {
                // SAFETY (these): the EESEUnit's wall and gate placements, live while the tab is
                "wall" => unsafe {
                    let wall = h(O::UnitBuildWall, s, &[]) as usize as *mut EeScreensBuildWallFields;
                    (*wall).start = EeScreensPoint { x: f(2), y: f(3) };
                    (*wall).end = EeScreensPoint { x: f(4), y: f(5) };
                    x::ee_screens_wall_build(wall); // PlaceWall
                },
                "gate" => unsafe {
                    let gate = h(O::UnitBuildGate, s, &[]) as usize as *mut EeScreensBuildGateFields;
                    (*gate).tile_x = l(2) as i32;
                    (*gate).tile_y = l(3) as i32;
                    (*gate).direction = l(4) as i32;
                    x::ee_screens_gate_build(gate, f(2), f(3), l(4) as i32); // PlaceGate
                },
                _ => unsafe {
                    if flags & 2 != 0 {
                        x::ee_screens_wall_cancel(h(O::UnitBuildWall, s, &[]) as usize as *mut _); // EndWall
                    }
                    if x::ee_screens_seun_get(x::unit_tab(), SEUN_GET_FLAGS) & 4 != 0 {
                        x::ee_screens_gate_cancel(h(O::UnitBuildGate, s, &[]) as usize as *mut _); // EndGate
                    }
                },
            }
        }),
        "epoch" => Box::new(|out: &mut Vec<String>| {
            // epoch ROW START END: a player row's end epoch set, then its start epoch picked
            use crate::screens::editor_players::{EE_SCREENS_SEPL_ROW_END_EPOCH, EE_SCREENS_SEPL_ROW_START_EPOCH};
            let screen = x::player_tab();
            // SAFETY: the player tab's Rust object
            let start = unsafe { x::ee_screens_sepl_control(screen, l(1) as u32, EE_SCREENS_SEPL_ROW_START_EPOCH) };
            let end = unsafe { x::ee_screens_sepl_control(screen, l(1) as u32, EE_SCREENS_SEPL_ROW_END_EPOCH) };
            h(O::ComboSetSelection, end, &[l(3) as c_ulong as i64]);
            h(O::ComboSetSelection, start, &[l(2) as c_ulong as i64]);
            fire(start, x::TRIGGER_SELECTION_CHANGED);
            let selected = sel(O::ComboSelection, end);
            out.push(format!(
                "  end epochs {} selection {}",
                x::line_count(h(O::ComboList, end, &[]) as usize),
                selected.map_or_else(|| "none".to_string(), |v| v.to_string())
            ));
        }),
        "form" => Box::new(|out: &mut Vec<String>| {
            // form FIRST LAST [GROUP]: the editor form's controls by index
            d::form_digest(l(1) as c_ulong, l(2) as c_ulong, if w.len() > 3 { l(3) } else { -1 }, out);
        }),
        "players" => Box::new(|out: &mut Vec<String>| d::players_digest(out)),
        "edit" => Box::new(|_: &mut Vec<String>| {
            // edit ID TEXT: an editor edit box's text set ("-" for none)
            let w2 = w.get(2).map_or("", |s| s.as_str());
            hs(O::EditSetText, x::editor_control(w1), if w2 == "-" { "" } else { w2 });
        }),
        "buttonup" => Box::new(|_: &mut Vec<String>| {
            h(O::ButtonUp, x::editor_control(w1), &[]);
        }),
        "changesel" => Box::new(|_: &mut Vec<String>| {
            // changesel ID LINE: a combo box's line chosen (its ChangeSelection)
            let b = x::editor_control(w1);
            h(O::ComboSetSelection, b, &[l(2) as c_ulong as i64]);
            h(O::ComboChange, b, &[]);
        }),
        "enter" => Box::new(|_: &mut Vec<String>| {
            h(O::EditEnter, x::editor_control(w1), &[]);
        }),
        "tscreen" => Box::new(|_: &mut Vec<String>| {
            // tscreen select N|new|dup|delete|up|down|sort|name: the trigger tab's list buttons
            match w1 {
                "select" => {
                    // SAFETY: the trigger tab's Rust object
                    let list = unsafe { x::ee_screens_sets_control(x::trigger_tab(), SETS_LIST, 0) };
                    h(O::ListChangeSelection, list, &[l(2) as c_ulong as i64, 0, 0]);
                    x::trigger_call(SETS_SELECT_ENTRY, 0);
                }
                "new" => x::trigger_call(SETS_NEW_ENTRY, 0),
                "dup" => x::trigger_call(SETS_DUPLICATE_ENTRY, 0),
                "delete" => x::trigger_call(SETS_DELETE_ENTRY, 0),
                "up" => x::trigger_call(SETS_INC_PRIORITY, 0),
                "down" => x::trigger_call(SETS_DEC_PRIORITY, 0),
                "sort" => x::trigger_call(SETS_ALPHA_SORT, 0),
                _ => x::trigger_call(SETS_SELECT_NAME, 0),
            }
        }),
        "tsclick" => Box::new(|_: &mut Vec<String>| {
            // tsclick select|add UNIT: the object screen's click (EESETSAMouseClick) posted
            let a = h(O::TsClickNew, 0, &[]) as usize;
            let id = unit_ref(w.get(2).map_or("", |s| s.as_str()));
            let kind = if w1 == "select" { MESSAGE_SELECT_UNIT } else { MESSAGE_ALWAYS_ADD_UNIT_TO_SELECTED_GROUP };
            h(O::ClickSetMessage, a, &[x::record(kind, Some(id)) as i64]);
            x::post_action(a);
            x::deref(a);
        }),
        "fire" => Box::new(|_: &mut Vec<String>| fire(x::editor_control(w1), x::TRIGGER_BUTTON_UP)),
        "tscreenstate" => Box::new(|out: &mut Vec<String>| {
            // the trigger tab's list, open sub-screen and the sub-screens' selections
            let s = x::trigger_tab();
            // SAFETY: the trigger tab's Rust object
            let list = unsafe { x::ee_screens_sets_control(s, SETS_LIST, 0) };
            let screen = unsafe { x::ee_screens_sets_control(s, SETS_CURRENT_SCREEN, 0) } as isize as c_long;
            let n = x::line_count(list);
            let mut line = format!("  tscreen screen={screen} storing={} list {n}:", h(O::TriggerStoring, 0, &[]));
            for k in 0..n.min(6) {
                line += &format!(" '{}'", ht(O::ListData, list, k as i64));
            }
            match sel(O::ListSelection, list) {
                Some(v) => line += &format!(" selected {v}"),
                None => line += " none",
            }
            for k in 0..5u32 {
                let mut selection = 0i32;
                let mut has = false;
                unsafe { x::ee_screens_sets_sub(s, k, &mut selection, &mut has) };
                line += &format!(" s{k}={selection}{}", if has { "+" } else { "-" });
            }
            out.push(line);
        }),
        "triggerstate" => Box::new(|out: &mut Vec<String>| d::trigger_digest(out)),
        "object" => Box::new(|out: &mut Vec<String>| d::object_digest(out)),
        "sm" => Box::new(|out: &mut Vec<String>| d::scenario_list_step(w, out)),
        "formstate" => Box::new(|out: &mut Vec<String>| d::form_state(out)),
        "loadtext" => Box::new(|_: &mut Vec<String>| {
            // loadtext TEXT|-: the form's SetLoadText ('_' a space)
            let text = if w1 == "-" { String::new() } else { spaced(w.get(1)) };
            hs(O::FormSetLoadText, x::editor(), &text);
        }),
        "fsel" => Box::new(|_: &mut Vec<String>| {
            // fsel add|remove UNIT | fsel first | fsel clear: the form's selection members
            let e = x::editor();
            match w1 {
                "first" => {
                    x::form_call(e, EE_SCREENS_SEFM_DESELECT_FIRST, 0, 0, None);
                }
                "clear" => {
                    x::form_call(e, EE_SCREENS_SEFM_CLEAR_SELECTION, 0, 0, None);
                }
                _ => {
                    let u = gc::h(gc::O::EcResolve, 0, &[unit_ref(w.get(2).map_or("", |s| s.as_str())) as u32 as i64])
                        as usize;
                    if u != 0 && w1 == "add" {
                        x::form_call(e, EE_SCREENS_SEFM_SELECT_UNIT, 0, u, None);
                    } else if u != 0 {
                        x::form_call(e, EE_SCREENS_SEFM_DESELECT_UNIT, 0, u, None);
                    }
                }
            }
        }),
        "formcall" => Box::new(|_: &mut Vec<String>| {
            // formcall store|resume: the form's RefreshCurrentGroup / ResumeCurrentGroup
            let what = if w1 == "store" { EE_SCREENS_SEFM_STORE_GROUP } else { EE_SCREENS_SEFM_RESUME_GROUP };
            x::form_call(x::editor(), what, 0, 0, None);
        }),
        "gameview" => Box::new(|_: &mut Vec<String>| {
            // gameview L T R B HIDDEN: the form's SetGameView
            x::form_call(x::editor(), EE_SCREENS_SEFM_SET_GAME_VIEW, i32::from(l(5) != 0), 0, Some(&[f(1), f(2), f(3), f(4)]));
        }),
        "trigreport" => Box::new(|out: &mut Vec<String>| d::trigger_report(w1 == "setup", out)),
        "terrainstate" => Box::new(|out: &mut Vec<String>| d::terrain_digest(out)),
        "terrainpick" | "terraincolor" => Box::new(|_: &mut Vec<String>| {
            // terrainpick|terraincolor LINE: a list's line clicked (its event fires)
            if c == "terrainpick" {
                h(O::TerrainTypePick, x::control_by_id(0xcc), &[l(1) as c_ulong as i64]);
            } else {
                h(O::TerrainColorPick, x::control_by_id(0xcf), &[l(1) as c_ulong as i64]);
            }
        }),
        "terrainpaint" => Box::new(|out: &mut Vec<String>| {
            // terrainpaint L T R B: the paint area's CreatePaintAction over an area, posted
            use crate::screens::shells_paint::{EE_SCREENS_SKOP_PAINT_CENTERED, EE_SCREENS_SKOP_PAINT_CREATE};
            let area = x::control_by_id(205);
            let a = x::shell_op(area, EE_SCREENS_SKOP_PAINT_CREATE, 0, 0) as usize;
            let centered = x::shell_op(area, EE_SCREENS_SKOP_PAINT_CENTERED, 0, 0) != 0;
            out.push(format!(
                "  paint {} centered={}",
                if a != 0 { "action" } else { "none" },
                if centered { "1" } else { "0" }
            ));
            if a != 0 {
                paint(a, [l(1), l(2), l(3), l(4)]);
            }
        }),
        "terrainmouse" => Box::new(|_: &mut Vec<String>| {
            // terrainmouse X Y | terrainmouse update: the tab's MouseMove or Update
            use crate::screens::editor_form::{EE_SCREENS_SEFM_GROUP_MOUSE_MOVE, EE_SCREENS_SEFM_GROUP_UPDATE};
            let t = x::tab_group(2);
            // SAFETY: the terrain tab group's Rust object
            unsafe {
                if w1 == "update" {
                    x::ee_screens_segr_call(t, EE_SCREENS_SEFM_GROUP_UPDATE, 0.0, 0.0, 0);
                } else {
                    x::ee_screens_segr_call(t, EE_SCREENS_SEFM_GROUP_MOUSE_MOVE, f(1), f(2), 0);
                }
            }
        }),
        "brushhl" => Box::new(|_: &mut Vec<String>| {
            hf(O::BrushSetHighlight, x::brush_group(), &[(l(3) != 0) as i64], &[f(1), f(2)]);
        }),
        "techfill" => Box::new(|_: &mut Vec<String>| {
            x::shell_op(
                x::editor_control(w1),
                crate::screens::shells_tech::EE_SCREENS_SKOP_TECH_FILL,
                l(2) as i64,
                l(3) as i64,
            );
        }),
        "techstate" => Box::new(|out: &mut Vec<String>| d::tech_digest(out)),
        "cb" => Box::new(|out: &mut Vec<String>| {
            // cb ...: the civilization builder's steps (sed_civ.rs)
            if !super::sed_civ::step(w, out) {
                out.push(format!("  cb: unknown {}", w.get(1).map_or("", |s| s.as_str())));
            }
        }),
        "digest" => Box::new(|out: &mut Vec<String>| {
            // the world flushed first: the physics thread flushes it on its own clock otherwise
            crate::physics::ee_physics_flush_world(crate::gamecpp::globals::get!(gPhysics));
            d::digest(w1, out);
        }),
        _ => return Step::Unknown,
    };
    // the load thread drained before the step is queued: its texts reach the UI thread as actions
    // (EESEAChangeLoadText, one object posted again with each text), so a sync inside the step (FormState's) leaves
    // the ones posted meanwhile queued behind the step, and the load text read was whichever ran first (the editor
    // gate's flake under load); drained here, every one runs before the step
    if matches!(c, "form" | "players" | "unittab" | "formstate") {
        d::sync_load_thread();
    }
    let mut out = Vec::new();
    if x::run_step(code, &mut out) {
        Step::Done(out)
    } else {
        Step::Unfinished
    }
}

/// `%g` of a value the C++ printed as a float (kept for the unit tests' formats).
pub fn g_text(v: f32) -> String {
    g(v)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn sscanf_two_longs() {
        assert_eq!(two_longs("3,4"), Some((3, 4)));
        assert_eq!(two_longs(" -3, 4x"), Some((-3, 4)));
        assert_eq!(two_longs("3 ,4"), None);
        assert_eq!(two_longs("3"), None);
        assert_eq!(two_longs(",4"), None);
    }

    #[test]
    fn elevation_pattern() {
        let d = elevation_heights(3, 2);
        assert_eq!(d.len(), 6);
        // y 0: |0 - 2| * 500 / 2 = 500 -> 300 - 500 - 20 + (x * 37) % 41
        assert_eq!(d[0], -220);
        assert_eq!(d[1], -220 + 37);
        assert_eq!(d[3], 280);
    }

    #[test]
    fn placement_matrix() {
        let t = placed(0.0, 1.5, 2.5, 3.0);
        assert_eq!((t.m[0][3], t.m[1][3], t.m[2][3]), (1.5, 2.5, 3.0));
        assert_eq!(t.m[0][0], 1.0);
        assert_eq!(g_text(0.25), "0.25");
    }
}
