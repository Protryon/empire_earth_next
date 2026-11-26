//! The scenario manager's C++ methods whose bodies were glue around a Rust call, whole in Rust: the capture records'
//! wrappers (`Evaluate`, `Execute`, `SetPlayerAttribute`, `ExpandTokens`, `FindPlacement`, the area queries,
//! `EEChangeUnit`, the manager's `Load`), the host contexts and stream tables each built (`EESMHostContext`,
//! `EESMStream`, the list and round hosts), the elements' `Load`/`Save`, `Copy`, `ReplaceUnit`, `SetName`,
//! `SetParent`, the area's methods and the manager's list entries. Each C++ method is now one statement calling an
//! `ee_scenario_en_*` function; the C++ answers the members and the single statements by operation
//! (`ee_scenariocpp_entry`, `EE_SCENARIO_EN_*`, Scenario Manager/EEScenarioHost.cpp) and its host callbacks are
//! `extern "C"` (`ee_scenariocpp_host_*`, `ee_scenariocpp_stream_io`, `ee_scenariocpp_list_op`,
//! `ee_scenariocpp_round_op`).
//!
//! Each composition keeps the C++'s order of calls (the capture's seed before the run, the record after it).

// `c_ulong` and `c_long` are 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use std::ffi::c_void;

use crate::scenario::area_fields::EeScenarioAreaFields;
use crate::scenario::fields::EeScenarioMessageFields;

/// The host context's record (`EESMHostContext`, ee/EEScenarioHost.h: the same pointers, C++-typed there).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeScenarioHostContext {
    pub condition: *mut c_void,
    pub unit: *mut c_void,
    pub effect: *mut c_void,
    pub object: *mut c_void,
    pub cursor: *mut c_void,
    pub tiles: *mut c_void,
}

impl Default for EeScenarioHostContext {
    fn default() -> Self {
        let n = std::ptr::null_mut();
        EeScenarioHostContext { condition: n, unit: n, effect: n, object: n, cursor: n, tiles: n }
    }
}

/// The stream's context record (`EESMStream`, ee/EEScenarioHost.h).
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeScenarioStreamContext {
    pub file: *mut c_void,
    pub strings: *const *mut c_void,
    pub list: *mut c_void,
    pub trigger: *mut c_void,
    pub manager: *mut c_void,
    pub major: std::os::raw::c_ulong,
    pub minor: std::os::raw::c_ulong,
}

/// The list host's context record (`EESMListContext`, EEScenarioManager.cpp): the manager, the name list out.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeScenarioListContext {
    pub manager: *mut c_void,
    pub names: *mut c_void,
}

// ------------------------------------------------------------------------------------------- the C++ answers
// the members (`ee_scenariocpp_entry(op, object, a, 0)`: an address, a the slot of a string)
/// `static_cast<EeScenarioEffectFields*>(effect)`.
pub const EE_SCENARIO_EN_EFFECT_FIELDS: u32 = 0;
/// The effect's string `a` (Load's order: name, text, message, s64, s78, new name, file name, description).
pub const EE_SCENARIO_EN_EFFECT_STRING: u32 = 1;
/// `&effect->mObjects`.
pub const EE_SCENARIO_EN_EFFECT_OBJECTS: u32 = 2;
pub const EE_SCENARIO_EN_CONDITION_FIELDS: u32 = 3;
/// The condition's string `a` (name, text, chat text).
pub const EE_SCENARIO_EN_CONDITION_STRING: u32 = 4;
pub const EE_SCENARIO_EN_OBJECT_FIELDS: u32 = 5;
pub const EE_SCENARIO_EN_OBJECT_NAME: u32 = 6;
/// `&object->mUnits`.
pub const EE_SCENARIO_EN_OBJECT_UNITS: u32 = 7;
pub const EE_SCENARIO_EN_TRIGGER_FIELDS: u32 = 8;
/// The trigger's string `a` (name, text).
pub const EE_SCENARIO_EN_TRIGGER_STRING: u32 = 9;
/// The media strings' string `a` (aftermath, hints, history, movie file, sound file, instructions, map file).
pub const EE_SCENARIO_EN_MEDIA_STRING: u32 = 10;
pub const EE_SCENARIO_EN_MANAGER_FIELDS: u32 = 11;
/// `manager->mTimedEffects`.
pub const EE_SCENARIO_EN_MANAGER_TIMED: u32 = 12;
/// The manager's string `a` (export file name, last chat text).
pub const EE_SCENARIO_EN_MANAGER_STRING: u32 = 13;
/// `&manager->mMediaStrings`.
pub const EE_SCENARIO_EN_MANAGER_MEDIA: u32 = 14;
pub const EE_SCENARIO_EN_MESSAGE_FIELDS: u32 = 15;
/// The dialog message's string `a` (text, sound).
pub const EE_SCENARIO_EN_MESSAGE_STRING: u32 = 16;
pub const EE_SCENARIO_EN_AREA_FIELDS: u32 = 17;
/// The area's string `a` (name, text).
pub const EE_SCENARIO_EN_AREA_STRING: u32 = 18;
/// `&area->mTiles`.
pub const EE_SCENARIO_EN_AREA_TILES: u32 = 19;
// the single statements
/// `gWorld.mScenarioManager`.
pub const EE_SCENARIO_EN_MANAGER: u32 = 20;
/// `manager->Reset()`.
pub const EE_SCENARIO_EN_MANAGER_RESET: u32 = 21;
// (22, `effect->ExecuteUncaptured(a)`, went: the Rust makes the run, world_route.rs EE_SCENARIO_WORLD_RUN_*)
/// The save's version (major << 32 | minor), the capture's.
pub const EE_SCENARIO_EN_SAVE_VERSION: u32 = 23;
/// `unit->mUnitID`.
pub const EE_SCENARIO_EN_UNIT_ID: u32 = 24;
/// `gWorld.GetUnit(a)`.
pub const EE_SCENARIO_EN_UNIT_BY_ID: u32 = 25;
/// `gWorld.mScenarioManager->GetArea(a)`.
pub const EE_SCENARIO_EN_AREA_AT: u32 = 26;
/// `manager->mAreas.size()`.
pub const EE_SCENARIO_EN_AREA_COUNT: u32 = 27;
/// `manager->mAreas[a]`.
pub const EE_SCENARIO_EN_AREA_INDEX: u32 = 28;
/// A string's text (`(const char*)*string`).
pub const EE_SCENARIO_EN_STRING_TEXT: u32 = 29;
/// `*string = (const char*)a`.
pub const EE_SCENARIO_EN_STRING_SET_TEXT: u32 = 30;
/// `*string = *(const UWideString*)a`.
pub const EE_SCENARIO_EN_STRING_ASSIGN: u32 = 31;
/// An id list's (`std::vector<unsigned long>`) size.
pub const EE_SCENARIO_EN_UNITS_COUNT: u32 = 32;
/// An id list's element `a`.
pub const EE_SCENARIO_EN_UNITS_AT: u32 = 33;
/// An id list's element `a` set to `b`.
pub const EE_SCENARIO_EN_UNITS_SET: u32 = 34;
/// An id list assigned list `a`.
pub const EE_SCENARIO_EN_UNITS_ASSIGN: u32 = 35;
/// `gPhysics->mCurrentTick`.
pub const EE_SCENARIO_EN_PHYSICS_TICK: u32 = 36;
/// A tile list's (`std::vector<U2DPoint<long>>`) size.
pub const EE_SCENARIO_EN_TILES_COUNT: u32 = 37;
/// A tile list cleared.
pub const EE_SCENARIO_EN_TILES_CLEAR: u32 = 38;
/// Tile list `a` assigned the list.
pub const EE_SCENARIO_EN_TILES_COPY_TO: u32 = 39;
/// A tile list's data (`(const long*)data()`, x and y of each tile).
pub const EE_SCENARIO_EN_TILES_DATA: u32 = 40;

/// `manager->mPendingEffects.clear()`.
pub const EE_SCENARIO_EN_PENDING_CLEAR: u32 = 41;
/// The physics random stream's seed (the capture's `EE_SCENARIO_CAP_SEED` read).
pub const EE_SCENARIO_EN_CAPTURE_SEED: u32 = 42;
/// `gDataPath->FindFile(UWideString((const char*)a))` (0 when found).
pub const EE_SCENARIO_EN_DATA_FIND: u32 = 43;
/// The campaign's file prefix (`gU52A280`), its text.
pub const EE_SCENARIO_EN_CAMPAIGN_PREFIX: u32 = 44;
/// A string list (`std::vector<UWideString>`) appended `(const char*)a`.
pub const EE_SCENARIO_EN_STRINGS_PUSH_TEXT: u32 = 45;
/// `*string += (const char*)a`.
pub const EE_SCENARIO_EN_STRING_APPEND_TEXT: u32 = 46;
/// `ESVerify((const char*)p, a, (const char*)b)`: a failed check's report.
pub const EE_SCENARIO_EN_VERIFY: u32 = 47;

/// `IDS_SE_MAP_NONE` ("<NONE>"): a media name meaning no file.
pub const EE_SCENARIO_IDS_SE_MAP_NONE: u16 = 20009;

/// A media kind's folder and its two extensions (`GetMediaFile`: 0 movie, 1 sound, 2 map picture).
pub fn media_folder(kind: usize) -> (&'static str, [&'static str; 2]) {
    [("Movies\\", [".bik", ".bik"]), ("Sounds\\", [".wav", ".mp3"]), ("Scenarios\\", [".jpg", ".jpg"])][kind]
}

/// `GetMediaFile`'s search: no file for a name of `none` or empty; else the first of the two names found in the
/// folder (`find(path)` true when found), as found or, with `prefix`, with the campaign's prefix before its name;
/// `Err(first name)` when neither is found.
pub fn media_file(
    name: &str,
    none: &str,
    kind: usize,
    prefix: Option<&str>,
    find: &mut dyn FnMut(&str) -> bool,
) -> Result<Option<String>, String> {
    if name == none || name.is_empty() {
        return Ok(None);
    }
    let (folder, extensions) = media_folder(kind);
    let files = [format!("{name}{}", extensions[0]), format!("{name}{}", extensions[1])];
    for f in &files {
        let path = format!("{folder}{f}");
        if find(&path) {
            return Ok(Some(match prefix {
                Some(p) => format!("{folder}{p}{f}"),
                None => path,
            }));
        }
    }
    Err(files[0].clone())
}

/// The manager's list entries (`ee_scenario_en_list`).
pub const EE_SCENARIO_LS_RESET: u32 = 0;
pub const EE_SCENARIO_LS_SORT: u32 = 1;
pub const EE_SCENARIO_LS_NEW: u32 = 2;
/// `a` the index, `b` up.
pub const EE_SCENARIO_LS_MOVE: u32 = 3;
pub const EE_SCENARIO_LS_DELETE: u32 = 4;
pub const EE_SCENARIO_LS_DUPLICATE: u32 = 5;
/// `list` which, `p` the names out.
pub const EE_SCENARIO_LS_NAMES: u32 = 6;
/// `a` the id.
pub const EE_SCENARIO_LS_FIND: u32 = 7;
/// `p` the element.
pub const EE_SCENARIO_LS_INDEX_OF: u32 = 8;
/// `p` the object, `a` parent, `b` the value.
pub const EE_SCENARIO_LS_OBJECT_CHANGED: u32 = 9;

/// The manager's round entries (`ee_scenario_en_manager`).
pub const EE_SCENARIO_MN_UPDATE: u32 = 0;
pub const EE_SCENARIO_MN_SHOW_DIALOG: u32 = 1;
/// `Prepare`: the media unloaded, then the players' media prepared.
pub const EE_SCENARIO_MN_PREPARE: u32 = 2;
/// `Reset`: the numbers and the timed effects, the lists and their built-ins, the pending effects cleared.
pub const EE_SCENARIO_MN_RESET: u32 = 3;
// the manager's methods over its round host (`ee_scenario_en_round`; once each built the host in C++, EESMRoundHost)
/// The destructor's body (manager_compose.rs destroy).
pub const EE_SCENARIO_MN_DESTROY: u32 = 3000;
/// `EndCinematic`, `ExitCinematic`, `StartCinematic`, `ApplyPendingEffects` (cinematic.rs).
pub const EE_SCENARIO_MN_END_CINEMATIC: u32 = 3001;
pub const EE_SCENARIO_MN_EXIT_CINEMATIC: u32 = 3002;
pub const EE_SCENARIO_MN_START_CINEMATIC: u32 = 3003;
pub const EE_SCENARIO_MN_APPLY_PENDING: u32 = 3004;
/// `MoveCameraToStart` (manager_compose.rs).
pub const EE_SCENARIO_MN_MOVE_CAMERA: u32 = 3005;
/// `AddV134Effect(*effect)`, `SetTimedEffectHasRun(effect, a != 0)` (manager_compose.rs).
pub const EE_SCENARIO_MN_ADD_PENDING: u32 = 3006;
pub const EE_SCENARIO_MN_SET_TIMED_HAS_RUN: u32 = 3007;
/// `ReplaceUnit(a, b)` (cinematic.rs).
pub const EE_SCENARIO_MN_REPLACE_UNIT: u32 = 3008;

/// The text file host's context record (`EESMTextContext`, EEScenarioManager.cpp): the manager, the file.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct EeScenarioTextContext {
    pub manager: *mut c_void,
    pub file: *mut c_void,
}

/// The capture's wrapper of a run (the C++'s `if (!Active()) return run(); seed; r = run(); record(r, seed)`).
pub fn captured<R: Copy>(
    active: bool,
    seed: &mut dyn FnMut() -> u32,
    run: &mut dyn FnMut() -> R,
    record: &mut dyn FnMut(R, u32),
) -> R {
    if !active {
        return run();
    }
    let s = seed();
    let r = run();
    record(r, s);
    r
}

/// `ReplaceUnit`'s loop: each element equal to `old` (as the unsigned id) becomes `new`, by the list's statements.
pub fn replace_ids(
    count: usize,
    at: &mut dyn FnMut(usize) -> u64,
    set: &mut dyn FnMut(usize, u64),
    old: u64,
    new: u64,
) {
    for i in 0..count {
        if at(i) == old {
            set(i, new);
        }
    }
}

/// `EESMDialogMessage::EESMDialogMessage`'s numbers.
#[no_mangle]
pub extern "C" fn ee_scenario_message_defaults() -> EeScenarioMessageFields {
    EeScenarioMessageFields::default()
}

/// `EESMArea::EESMArea`'s numbers.
#[no_mangle]
pub extern "C" fn ee_scenario_area_defaults() -> EeScenarioAreaFields {
    EeScenarioAreaFields::defaults()
}

/// `EESMArea::IsUnset`.
///
/// # Safety
/// `f` a live area's record.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_area_is_unset(f: *const EeScenarioAreaFields) -> bool {
    // SAFETY: the caller's contract
    unsafe { (*f).is_unset() }
}

/// `EESMArea::IsOffMap`.
///
/// # Safety
/// `f` a live area's record.
#[no_mangle]
pub unsafe extern "C" fn ee_scenario_area_is_off_map(f: *const EeScenarioAreaFields) -> bool {
    // SAFETY: the caller's contract
    unsafe { (*f).is_off_map() }
}

#[cfg(not(test))]
pub use game::*;

#[cfg(not(test))]
// the entries take the C++ member's own pointers (see the SAFETY note below): their callers are the C++ methods
#[allow(clippy::not_unsafe_ptr_arg_deref)]
mod game {
    use std::ffi::{c_char, c_void};
    use std::os::raw::{c_long, c_ulong};

    use super::*;
    use crate::scenario::area::{self, EE_SCENARIO_WRITE_TILE};
    use crate::scenario::area_fields::{self, EeScenarioAreaRect};
    use crate::scenario::fields::{
        EeScenarioConditionFields, EeScenarioEffectFields, EeScenarioObjectFields, EeScenarioTriggerFields,
    };
    use crate::scenario::host::{EeScenarioHost, Host};
    use crate::scenario::lists::{self, EeScenarioListHost};
    use crate::scenario::manager::{EeScenarioManagerFields, EeScenarioTimedQueue};
    use crate::scenario::round::EeScenarioRoundHost;
    use crate::scenario::sm_capture::EeScenarioCapVersion;
    use crate::scenario::sm_capture_game as cap;
    use crate::scenario::stream::EeScenarioStream;
    use crate::scenario::{cinematic, ffi, placement, save, text, trigger_members};

    mod sys {
        use crate::scenario::host::EeScenarioTiles;
        use std::ffi::{c_char, c_void};
        use std::os::raw::{c_long, c_ulong};
        // the tile list is opaque to the C++ (it hands it back to ee_scenario_tiles_push)
        #[allow(improper_ctypes)]
        extern "C" {
            pub fn ee_scenariocpp_entry(op: u32, p: *mut c_void, a: i64, b: i64) -> i64;
            pub fn ee_scenariocpp_host_read(ctx: *mut c_void, op: u32, a: i32, b: i32, c: i32, d: i32) -> i64;
            #[allow(clippy::too_many_arguments)]
            pub fn ee_scenariocpp_host_players(
                ctx: *mut c_void,
                kind: i32,
                single: bool,
                result: i32,
                listed: bool,
                players: *const i32,
                count: usize,
                seed: u32,
            );
            pub fn ee_scenariocpp_host_write(ctx: *mut c_void, op: u32, player: c_long, b: i64, c: i64, d: i64);
            pub fn ee_scenariocpp_host_tiles(
                ctx: *mut c_void,
                op: u32,
                a: i32,
                b: i32,
                c: i32,
                list: *mut EeScenarioTiles,
            );
            pub fn ee_scenariocpp_host_world(ctx: *mut c_void, op: u32, args: *const i64) -> i64;
            pub fn ee_scenariocpp_world_cu(ctx: *mut c_void, op: u32, args: *const i64) -> i64;
            pub fn ee_scenariocpp_world_class(ctx: *mut c_void, op: u32, args: *const i64) -> i64;
            pub fn ee_scenariocpp_world_read(ctx: *mut c_void, op: u32, args: *const i64) -> i64;
            pub fn ee_scenariocpp_world_y(ctx: *mut c_void, op: u32, args: *const i64) -> i64;
            pub fn ee_scenariocpp_world_object(ctx: *mut c_void, op: u32, args: *const i64) -> i64;
            pub fn ee_scenariocpp_world_create(ctx: *mut c_void, op: u32, args: *const i64) -> i64;
            pub fn ee_scenariocpp_world_atom(ctx: *mut c_void, op: u32, args: *const i64) -> i64;
            pub fn ee_scenariocpp_host_object(
                ctx: *mut c_void,
                index: i32,
            ) -> *const crate::scenario::fields::EeScenarioObjectFields;
            pub fn ee_scenariocpp_stream_io(
                ctx: *mut c_void,
                op: u32,
                slot: u32,
                index: c_ulong,
                value: *mut c_void,
            ) -> i32;
            pub fn ee_scenariocpp_list_op(
                ctx: *mut c_void,
                op: u32,
                list: u32,
                index: usize,
                element: *mut c_void,
                text: *const c_char,
            ) -> *mut c_void;
            pub fn ee_scenariocpp_round_op(ctx: *mut c_void, op: u32, a: i64, b: i64, time: f32) -> i64;
            pub fn ee_scenariocpp_text_op(
                ctx: *mut c_void,
                op: u32,
                slot: u32,
                value: c_long,
                element: *mut c_void,
                text: *const c_char,
                text2: *const c_char,
            ) -> *mut c_void;
        }
    }

    /// One C++ statement or member.
    fn en(op: u32, p: *mut c_void, a: i64, b: i64) -> i64 {
        use crate::world::view::{UnitView, WorldRead, WorldView};
        // the unit's id and the physics' tick from the world views
        if op == EE_SCENARIO_EN_UNIT_ID || op == EE_SCENARIO_EN_PHYSICS_TICK {
            // SAFETY: inside the C++ call on the simulation thread; `p` the live unit the op names
            let t = unsafe { WorldRead::begin() };
            return if op == EE_SCENARIO_EN_UNIT_ID {
                // SAFETY: as above
                unsafe { UnitView::of(&t, p as usize as u64) }.map_or(0, |u| u.id() as i64)
            } else {
                WorldView::global(&t)
                    .and_then(|w| w.physics())
                    .map_or(0, |r| crate::world_field!(r, mCurrentTick) as i64)
            };
        }
        // SAFETY: the statement host's contract: `p` the live object its op names, `a` and `b` as the op names
        unsafe { sys::ee_scenariocpp_entry(op, p, a, b) }
    }
    fn en_ptr(op: u32, p: *mut c_void, a: i64) -> *mut c_void {
        en(op, p, a, 0) as usize as *mut c_void
    }
    fn active() -> bool {
        crate::app::capture::ee_app_capture_active()
    }
    fn seed() -> u32 {
        en(EE_SCENARIO_EN_CAPTURE_SEED, std::ptr::null_mut(), 0, 0) as u32
    }
    fn save_version() -> EeScenarioCapVersion {
        en(EE_SCENARIO_EN_SAVE_VERSION, std::ptr::null_mut(), 0, 0)
    }
    fn version(major: c_ulong, minor: c_ulong) -> EeScenarioCapVersion {
        ((major as u64 as i64) << 32) | (minor as u32 as i64)
    }

    /// A world call: the C++ host world_route.rs names for `op` (the C++ dispatcher's range tests, in its order).
    unsafe extern "C" fn world(ctx: *mut c_void, op: u32, args: *const i64) -> i64 {
        use crate::scenario::world_route::{route, EeScenarioWorldRoute as R};
        // the reads the world views answer (world_views.rs)
        // SAFETY: the host table's contract: six arguments, `ctx` the operation's live context
        if let Some(value) =
            unsafe { crate::scenario::world_views::answer(ctx, op, &*(args as *const [i64; 6]), &Look) }
        {
            return value;
        }
        let f = match route(op) {
            R::ChangeUnit => sys::ee_scenariocpp_world_cu,
            R::Class => sys::ee_scenariocpp_world_class,
            R::Read => sys::ee_scenariocpp_world_read,
            R::Text => sys::ee_scenariocpp_world_y,
            R::Object => sys::ee_scenariocpp_world_object,
            R::Create => sys::ee_scenariocpp_world_create,
            R::Atom => sys::ee_scenariocpp_world_atom,
            R::Run => sys::ee_scenariocpp_host_world,
        };
        // SAFETY: the host table's contract, passed through to the host that answers `op`
        unsafe { f(ctx, op, args) }
    }

    /// The world views' lookups through the C++ host: `gWorld.GetUnit(id)`, an effect's record.
    struct Look;
    impl crate::scenario::world_views::Lookups for Look {
        fn unit(&self, id: i64) -> u64 {
            en(EE_SCENARIO_EN_UNIT_BY_ID, std::ptr::null_mut(), id, 0) as u64
        }
        fn effect(&self, effect: *mut c_void) -> *const EeScenarioEffectFields {
            en_ptr(EE_SCENARIO_EN_EFFECT_FIELDS, effect, 0) as *const EeScenarioEffectFields
        }
        fn fields(&self, op: u32, element: *mut c_void) -> *const c_void {
            en_ptr(op, element, 0)
        }
        fn area(&self, index: i64) -> *const EeScenarioAreaFields {
            en_ptr(EE_SCENARIO_EN_AREA_FIELDS, en_ptr(EE_SCENARIO_EN_AREA_AT, std::ptr::null_mut(), index), 0)
                as *const EeScenarioAreaFields
        }
        fn object(&self, index: i64) -> *const EeScenarioObjectFields {
            // SAFETY: the host's object lookup (it reads the manager's list)
            unsafe { sys::ee_scenariocpp_host_object(std::ptr::null_mut(), index as i32) }
        }
        fn trigger(&self, index: i64) -> *const EeScenarioTriggerFields {
            use crate::scenario::read_compose::EE_SCENARIO_RX_TRIGGER;
            let args = [index, 0, 0, 0, 0, 0];
            // SAFETY: the host's trigger lookup (no context)
            let t =
                unsafe { sys::ee_scenariocpp_world_read(std::ptr::null_mut(), EE_SCENARIO_RX_TRIGGER, args.as_ptr()) };
            en_ptr(EE_SCENARIO_EN_TRIGGER_FIELDS, t as usize as *mut c_void, 0) as *const EeScenarioTriggerFields
        }
        fn change_unit(
            &self,
            attribute: c_ulong,
            percent: bool,
            unit: u64,
            minimum: c_long,
            maximum: c_ulong,
            random: bool,
            mode: c_long,
        ) {
            ee_scenario_en_change_unit(
                attribute,
                percent,
                unit as usize as *mut c_void,
                minimum,
                maximum,
                random,
                mode,
            );
        }
        fn area_center(&self, index: i64) -> i64 {
            let mut tile: [c_long; 2] = [0; 2];
            // SAFETY: the manager's area `index` (the operation's), its centre into `tile`
            unsafe {
                ee_scenario_en_area_center_tile(
                    en_ptr(EE_SCENARIO_EN_AREA_AT, std::ptr::null_mut(), index),
                    tile.as_mut_ptr(),
                )
            };
            crate::scenario::world_route::ee_scenario_pack_xy(tile[0], tile[1])
        }
    }

    /// The record `EE_SCENARIO_EN_*_FIELDS` operation `op` names of the element `element` (sm_capture_game.rs).
    pub(crate) fn element_record(op: u32, element: u64) -> u64 {
        en(op, element as usize as *mut c_void, 0, 0) as u64
    }

    /// `gWorld.GetUnit((unsigned long)id)`'s handle (sm_capture_game.rs).
    pub(crate) fn unit_by_id(id: i64) -> u64 {
        en(EE_SCENARIO_EN_UNIT_BY_ID, std::ptr::null_mut(), id, 0) as u64
    }

    /// A round operation: the world views' answer (world_views.rs round), else the C++ host's.
    unsafe extern "C" fn round_op(ctx: *mut c_void, op: u32, a: i64, b: i64, time: f32) -> i64 {
        // SAFETY: the round host's contract, passed through
        unsafe {
            crate::scenario::world_views::round(op, a)
                .unwrap_or_else(|| sys::ee_scenariocpp_round_op(ctx, op, a, b, time))
        }
    }

    /// The host table over the context `ctx` (it must outlive the calls).
    fn table(ctx: *mut c_void) -> EeScenarioHost {
        EeScenarioHost {
            ctx,
            read: sys::ee_scenariocpp_host_read,
            players: sys::ee_scenariocpp_host_players,
            write: sys::ee_scenariocpp_host_write,
            tiles: sys::ee_scenariocpp_host_tiles,
            world,
            object: sys::ee_scenariocpp_host_object,
        }
    }

    /// The host table over `ctx` (it must outlive the calls).
    fn host(ctx: &mut EeScenarioHostContext) -> EeScenarioHost {
        table(ctx as *mut EeScenarioHostContext as *mut c_void)
    }

    /// One effect run's call `op` (world_route.rs EE_SCENARIO_WORLD_RUN_*) on the run `run`, argument `a`.
    fn run_op(run: *mut c_void, op: u32, a: i64) -> i64 {
        let args = [a, 0, 0, 0, 0, 0];
        // SAFETY: the run host's contract: `run` a live EESMEffectRun (null for RUN_NEW), six arguments
        unsafe { sys::ee_scenariocpp_host_world(run, op, args.as_ptr()) }
    }

    /// `f` over a new effect run of effect `e` (the C++'s EESMEffectRun, the original's locals) and its host table;
    /// the run is freed after.
    fn with_run(e: *mut c_void, f: impl FnOnce(&EeScenarioHost, *mut c_void)) {
        use crate::scenario::world_route::{EE_SCENARIO_WORLD_RUN_FREE, EE_SCENARIO_WORLD_RUN_NEW};
        let run = run_op(null(), EE_SCENARIO_WORLD_RUN_NEW, e as usize as i64) as usize as *mut c_void;
        let h = table(run);
        f(&h, run);
        run_op(run, EE_SCENARIO_WORLD_RUN_FREE, 0);
    }

    /// `f` over a stream table of `file`, the strings `strings` (the member op's slots 0..count of `object`), the
    /// list, trigger and manager (null for none) and the version.
    #[allow(clippy::too_many_arguments)]
    fn with_stream<R>(
        file: *mut c_void,
        object: *mut c_void,
        strings: u32,
        count: u32,
        list: *mut c_void,
        trigger: *mut c_void,
        manager: *mut c_void,
        major: c_ulong,
        minor: c_ulong,
        f: impl FnOnce(&EeScenarioStream) -> R,
    ) -> R {
        let slots: Vec<*mut c_void> = (0..count).map(|k| en_ptr(strings, object, k as i64)).collect();
        let mut ctx = EeScenarioStreamContext { file, strings: slots.as_ptr(), list, trigger, manager, major, minor };
        let table = EeScenarioStream {
            ctx: &mut ctx as *mut EeScenarioStreamContext as *mut c_void,
            io: sys::ee_scenariocpp_stream_io,
        };
        f(&table)
    }

    /// The list host over `manager` (`names` the name list out, null for none).
    fn with_lists<R>(manager: *mut c_void, names: *mut c_void, f: impl FnOnce(&EeScenarioListHost) -> R) -> R {
        let mut ctx = EeScenarioListContext { manager, names };
        let host = EeScenarioListHost {
            ctx: &mut ctx as *mut EeScenarioListContext as *mut c_void,
            manager: en_ptr(EE_SCENARIO_EN_MANAGER_FIELDS, manager, 0) as *mut EeScenarioManagerFields,
            timed: en_ptr(EE_SCENARIO_EN_MANAGER_TIMED, manager, 0) as *mut EeScenarioTimedQueue,
            op: sys::ee_scenariocpp_list_op,
        };
        f(&host)
    }

    /// The round host over `manager` (`EESMRoundHost`).
    fn round_host(manager: *mut c_void) -> EeScenarioRoundHost {
        EeScenarioRoundHost {
            ctx: manager,
            fields: en_ptr(EE_SCENARIO_EN_MANAGER_FIELDS, manager, 0) as *mut EeScenarioManagerFields,
            queue: en_ptr(EE_SCENARIO_EN_MANAGER_TIMED, manager, 0) as *mut EeScenarioTimedQueue,
            op: round_op,
        }
    }

    fn null() -> *mut c_void {
        std::ptr::null_mut()
    }

    // SAFETY (every entry below): the C++ member passes its live `this` and its arguments as its own declaration
    // names them, on the thread that owns the object; no Rust reference lives across a call back into the C++.

    // --------------------------------------------------------------------------------------- the condition
    /// `EESMCondition::Evaluate` (with the SMCE record).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_condition_evaluate(c: *mut c_void) -> bool {
        let mut run = || {
            let mut ctx = EeScenarioHostContext { condition: c, ..Default::default() };
            let h = host(&mut ctx);
            let fields = en_ptr(EE_SCENARIO_EN_CONDITION_FIELDS, c, 0) as *const EeScenarioConditionFields;
            // SAFETY: as above; `h` and `fields` live through the call
            unsafe { ffi::ee_scenario_condition_evaluate(&h, fields) }
        };
        captured(active(), &mut seed, &mut run, &mut |r, s| cap::ee_scenario_cap_condition(c, r, s))
    }

    /// `EESMCondition::Load` / `Save`.
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_condition_io(c: *mut c_void, file: *mut c_void, save: bool) -> i32 {
        let fields = en_ptr(EE_SCENARIO_EN_CONDITION_FIELDS, c, 0) as *mut EeScenarioConditionFields;
        with_stream(file, c, EE_SCENARIO_EN_CONDITION_STRING, 3, null(), null(), null(), 0, 0, |t| {
            // SAFETY: as above
            unsafe {
                if save {
                    ffi::ee_scenario_condition_save(t, fields)
                } else {
                    ffi::ee_scenario_condition_load(t, fields)
                }
            }
        })
    }

    /// `EESMCondition::Copy`: the name, text and chat text, then every number.
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_condition_copy(c: *mut c_void, from: *mut c_void) {
        for k in 0..3 {
            let s = en(EE_SCENARIO_EN_CONDITION_STRING, from, k, 0);
            en(EE_SCENARIO_EN_STRING_ASSIGN, en_ptr(EE_SCENARIO_EN_CONDITION_STRING, c, k), s, 0);
        }
        let (to, from) = (
            en_ptr(EE_SCENARIO_EN_CONDITION_FIELDS, c, 0) as *mut EeScenarioConditionFields,
            en_ptr(EE_SCENARIO_EN_CONDITION_FIELDS, from, 0) as *const EeScenarioConditionFields,
        );
        // SAFETY: two live conditions' records
        unsafe { *to = *from };
    }

    // ------------------------------------------------------------------------------------------ the object
    fn object_fields(o: *mut c_void) -> *mut EeScenarioObjectFields {
        en_ptr(EE_SCENARIO_EN_OBJECT_FIELDS, o, 0) as *mut EeScenarioObjectFields
    }

    /// `EESMObject::Evaluate` (with the SMOB record).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_object_evaluate(o: *mut c_void, include_dead: bool) -> bool {
        let run = || {
            let mut ctx = EeScenarioHostContext { object: o, ..Default::default() };
            let h = host(&mut ctx);
            // SAFETY: as above
            unsafe { ffi::ee_scenario_object_evaluate(&h, object_fields(o), include_dead) }
        };
        if !active() {
            return run();
        }
        let before = cap::ee_scenario_cap_object_begin(o);
        let r = run();
        // SAFETY: `before` lives through the call
        unsafe { cap::ee_scenario_cap_object_end(o, include_dead, r, &before) };
        r
    }

    /// `EESMObject::PickUnitType`.
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_object_pick_unit_type(o: *mut c_void) -> c_long {
        let mut ctx = EeScenarioHostContext::default();
        let h = host(&mut ctx);
        // SAFETY: as above
        unsafe { ffi::ee_scenario_object_pick_unit_type(&h, object_fields(o)) as c_long }
    }

    /// `EESMObject::GetUnitAttribute` (0 for no unit).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_unit_attribute(attribute: c_ulong, percent: bool, unit: *mut c_void) -> c_long {
        if unit.is_null() {
            return 0;
        }
        let mut ctx = EeScenarioHostContext { unit, ..Default::default() };
        let h = host(&mut ctx);
        // SAFETY: as above
        unsafe { ffi::ee_scenario_unit_attribute(&h, attribute as u32, percent) }
    }

    /// `EESMObject::FindPlacement` (with the SMFP record; a retry is the whole call again without the random try).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_find_placement(
        o: *mut c_void,
        unit_type: c_ulong,
        x: *mut f32,
        y: *mut f32,
        try_random: bool,
    ) -> bool {
        if !active() {
            return find_placement_uncaptured(o, unit_type, x, y, try_random);
        }
        let s = seed();
        let r = find_placement_uncaptured(o, unit_type, x, y, try_random);
        // SAFETY: the location's two floats are live
        let (px, py) = if r { unsafe { (*x, *y) } } else { (0.0, 0.0) };
        cap::ee_scenario_cap_placement(o, unit_type, try_random, r, px, py, s);
        r
    }

    fn find_placement_uncaptured(o: *mut c_void, unit_type: c_ulong, x: *mut f32, y: *mut f32, random: bool) -> bool {
        // SAFETY: the object's record is live
        let area_index = unsafe { (*object_fields(o)).mArea };
        let area = en_ptr(EE_SCENARIO_EN_AREA_AT, null(), area_index as i64);
        let record = (!area.is_null()).then(|| area_fields_of(area).record());
        let mut ctx = EeScenarioHostContext::default();
        let h = host(&mut ctx);
        // SAFETY: the location's two floats are live
        let mut location = unsafe { [*x, *y] };
        let rp = record.as_ref().map_or(std::ptr::null(), |r| r as *const _);
        // SAFETY: as above; every pointer lives through the call
        let result = unsafe {
            ffi::ee_scenario_find_placement(&h, object_fields(o), rp, unit_type as u32, random, &mut location)
        };
        // SAFETY: as above
        unsafe {
            *x = location[0];
            *y = location[1];
        }
        if result == placement::EE_SCENARIO_PLACEMENT_RETRY {
            return ee_scenario_en_find_placement(o, unit_type, x, y, false);
        }
        result == placement::EE_SCENARIO_PLACEMENT_FOUND
    }

    /// `EESMObject::Load` / `Save` (the name, the unit ids).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_object_io(
        o: *mut c_void,
        file: *mut c_void,
        major: c_ulong,
        minor: c_ulong,
        save: bool,
    ) -> i32 {
        let units = en_ptr(EE_SCENARIO_EN_OBJECT_UNITS, o, 0);
        let fields = object_fields(o);
        with_stream(file, o, EE_SCENARIO_EN_OBJECT_NAME, 1, units, null(), null(), 0, 0, |t| {
            // SAFETY: as above
            unsafe {
                if save {
                    ffi::ee_scenario_object_save(t, fields, minor)
                } else {
                    ffi::ee_scenario_object_load(t, fields, major, minor)
                }
            }
        })
    }

    /// `EESMObject::ReplaceUnit`.
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_object_replace_unit(o: *mut c_void, old: c_long, new: c_long) {
        let units = en_ptr(EE_SCENARIO_EN_OBJECT_UNITS, o, 0);
        let count = en(EE_SCENARIO_EN_UNITS_COUNT, units, 0, 0) as usize;
        replace_ids(
            count,
            &mut |i| en(EE_SCENARIO_EN_UNITS_AT, units, i as i64, 0) as u64,
            &mut |i, v| {
                en(EE_SCENARIO_EN_UNITS_SET, units, i as i64, v as i64);
            },
            old as c_ulong as u64,
            new as c_ulong as u64,
        );
    }

    /// `EESMObject::Copy`: the name, every number, the units when selected on the map.
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_object_copy(o: *mut c_void, from: *mut c_void) {
        en(
            EE_SCENARIO_EN_STRING_ASSIGN,
            en_ptr(EE_SCENARIO_EN_OBJECT_NAME, o, 0),
            en(EE_SCENARIO_EN_OBJECT_NAME, from, 0, 0),
            0,
        );
        let (to, src) = (object_fields(o), object_fields(from));
        // SAFETY: two live objects' records
        let selected = unsafe {
            *to = *src;
            (*to).mIsSelectOnMap
        };
        if selected {
            let list = en(EE_SCENARIO_EN_OBJECT_UNITS, from, 0, 0);
            en(EE_SCENARIO_EN_UNITS_ASSIGN, en_ptr(EE_SCENARIO_EN_OBJECT_UNITS, o, 0), list, 0);
        }
    }

    /// `EESMObject::SetName` (the manager renames the conditions that use the object).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_object_set_name(o: *mut c_void, name: *const c_void) {
        en(EE_SCENARIO_EN_STRING_ASSIGN, en_ptr(EE_SCENARIO_EN_OBJECT_NAME, o, 0), name as usize as i64, 0);
        let manager = en_ptr(EE_SCENARIO_EN_MANAGER, null(), 0);
        if !manager.is_null() {
            ee_scenario_en_list(manager, EE_SCENARIO_LS_OBJECT_CHANGED, 0, 0, 0, o);
        }
    }

    /// `EESMObject::SetParent` (the manager breaks a cycle).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_object_set_parent(o: *mut c_void, parent: c_long) {
        // SAFETY: the object's record is live
        unsafe { (*object_fields(o)).mParent = parent as c_ulong };
        let manager = en_ptr(EE_SCENARIO_EN_MANAGER, null(), 0);
        if !manager.is_null() {
            ee_scenario_en_list(manager, EE_SCENARIO_LS_OBJECT_CHANGED, 0, 1, parent as i64, o);
        }
    }

    /// `EEChangeUnit` (with the SMCU record: the unit read again by id after, as a kill may free it).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_change_unit(
        attribute: c_ulong,
        percent: bool,
        unit: *mut c_void,
        minimum: c_long,
        maximum: c_ulong,
        random: bool,
        mode: c_long,
    ) {
        let s = if active() { seed() } else { 0 };
        let id: c_long = if unit.is_null() { -1 } else { en(EE_SCENARIO_EN_UNIT_ID, unit, 0, 0) as c_long };
        {
            let mut ctx = EeScenarioHostContext { unit, ..Default::default() };
            let h = host(&mut ctx);
            // SAFETY: as above
            unsafe { ffi::ee_scenario_change_unit(&h, attribute, percent, minimum, maximum, random, mode) };
        }
        if active() {
            let args: [c_long; 7] =
                [attribute as c_long, percent as c_long, id, minimum, maximum as c_long, random as c_long, mode];
            let after = if id >= 0 { en_ptr(EE_SCENARIO_EN_UNIT_BY_ID, null(), id as c_ulong as i64) } else { null() };
            // SAFETY: `args` lives through the call
            unsafe { cap::ee_scenario_cap_unit_change(0, args.as_ptr(), after, null(), null(), s, save_version()) };
        }
    }

    // ----------------------------------------------------------------------------------------- the trigger
    /// `EESMTrigger::Evaluate` (with the SMTE record).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_trigger_evaluate(t: *mut c_void, time: f32) {
        let run = || {
            let mut ctx = EeScenarioHostContext::default();
            let h = host(&mut ctx);
            // SAFETY: as above
            unsafe { trigger_members::ee_scenario_trigger_evaluate_member(&h, t, time) };
        };
        if !active() {
            run();
            return;
        }
        let before = cap::ee_scenario_cap_trigger_before(t);
        let s = seed();
        run();
        // SAFETY: `before` lives through the call
        unsafe { cap::ee_scenario_cap_trigger(t, &before, time, s) };
    }

    /// `EESMTrigger::Load` / `Save` (the name, the text, the entries through the trigger).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_trigger_io(t: *mut c_void, file: *mut c_void, save: bool) -> i32 {
        let fields = en_ptr(EE_SCENARIO_EN_TRIGGER_FIELDS, t, 0) as *mut EeScenarioTriggerFields;
        with_stream(file, t, EE_SCENARIO_EN_TRIGGER_STRING, 2, null(), t, null(), 0, 0, |s| {
            // SAFETY: as above
            unsafe {
                if save {
                    ffi::ee_scenario_trigger_save(s, fields)
                } else {
                    ffi::ee_scenario_trigger_load(s, fields)
                }
            }
        })
    }

    // ------------------------------------------------------------------------------------------ the effect
    fn effect_fields(e: *mut c_void) -> *mut EeScenarioEffectFields {
        en_ptr(EE_SCENARIO_EN_EFFECT_FIELDS, e, 0) as *mut EeScenarioEffectFields
    }

    /// `EESMEffect::SetPlayerAttribute` (with the SMPA record).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_effect_set_player_attribute(
        e: *mut c_void,
        player: c_long,
        attribute: c_ulong,
        mode: c_long,
        amount: c_long,
    ) {
        let mut run = || {
            let mut ctx = EeScenarioHostContext { effect: e, ..Default::default() };
            let h = host(&mut ctx);
            // SAFETY: as above
            unsafe { ffi::ee_scenario_set_player_attribute(&h, player, attribute, mode, amount) };
        };
        captured(active(), &mut seed, &mut run, &mut |(), s| {
            cap::ee_scenario_cap_player_attribute(player, attribute, mode, amount, s, save_version())
        });
    }

    /// `EESMEffect::Execute` (with the SMEX record; the run's context the C++'s EESMEffectRun).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_effect_execute(e: *mut c_void, cinematic: bool) {
        let mut run = || {
            let fields = en_ptr(EE_SCENARIO_EN_EFFECT_FIELDS, e, 0) as *const EeScenarioEffectFields;
            // SAFETY: the run's table lives for the call, the effect's numbers are live
            with_run(e, |h, _| unsafe { ffi::ee_scenario_effect_execute(h, fields, cinematic) });
        };
        captured(active(), &mut seed, &mut run, &mut |(), s| {
            cap::ee_scenario_cap_effect(e, cinematic, s, save_version())
        });
    }

    /// `EESMEffect::CreateUnits`: on its own effect run, the effect's object (none: nothing); create.rs's units, which
    /// become the object's units when it made any.
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_effect_create_units(e: *mut c_void) {
        use crate::scenario::world_route::{
            EE_SCENARIO_WORLD_RUN_CREATED, EE_SCENARIO_WORLD_RUN_OBJECT, EE_SCENARIO_WORLD_RUN_SET_UNITS,
        };
        with_run(e, |h, run| {
            let object = run_op(run, EE_SCENARIO_WORLD_RUN_OBJECT, 0) as usize as *mut c_void;
            if object.is_null() {
                return;
            }
            // SAFETY: the effect's and the object's numbers are live for the call
            let index =
                unsafe { (*(en_ptr(EE_SCENARIO_EN_EFFECT_FIELDS, e, 0) as *const EeScenarioEffectFields)).mObject };
            let fields = en_ptr(EE_SCENARIO_EN_OBJECT_FIELDS, object, 0) as *const EeScenarioObjectFields;
            // SAFETY: as above; the run's table lives for the call
            if unsafe { ffi::ee_scenario_create_units(h, index as i32, fields) }
                && run_op(run, EE_SCENARIO_WORLD_RUN_CREATED, 0) > 0
            {
                run_op(run, EE_SCENARIO_WORLD_RUN_SET_UNITS, 0);
            }
        });
    }

    /// `EESMEffect::ExpandTokens` (with the SMTK record): `text` and `out` the C++ strings (they may be one).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_effect_expand_tokens(e: *mut c_void, text: *mut c_void, out: *mut c_void) {
        let capture = active();
        let s = if capture { seed() } else { 0 };
        let input = en(EE_SCENARIO_EN_STRING_TEXT, text, 0, 0) as usize as *const c_char;
        // SAFETY: the string's text is live until the string changes; copied before that
        let kept = unsafe { std::ffi::CStr::from_ptr(input) }.to_owned();
        let mut ctx = EeScenarioHostContext { effect: e, ..Default::default() };
        let h = host(&mut ctx);
        // SAFETY: as above
        let result = unsafe { ffi::ee_scenario_expand_tokens(&h, kept.as_ptr()) };
        en(EE_SCENARIO_EN_STRING_SET_TEXT, out, result as usize as i64, 0);
        // SAFETY: the text the call returned
        unsafe { ffi::ee_scenario_text_free(result) };
        if capture {
            let done = en(EE_SCENARIO_EN_STRING_TEXT, out, 0, 0) as usize as *const c_char;
            // SAFETY: two live texts
            unsafe { cap::ee_scenario_cap_tokens(kept.as_ptr(), done, s) };
        }
    }

    /// `EESMEffect::Load` / `Save` (the eight strings, the object list).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_effect_io(
        e: *mut c_void,
        file: *mut c_void,
        major: c_ulong,
        minor: c_ulong,
        save: bool,
    ) -> i32 {
        let objects = en_ptr(EE_SCENARIO_EN_EFFECT_OBJECTS, e, 0);
        let fields = effect_fields(e);
        with_stream(file, e, EE_SCENARIO_EN_EFFECT_STRING, 8, objects, null(), null(), 0, 0, |t| {
            // SAFETY: as above
            unsafe {
                if save {
                    ffi::ee_scenario_effect_save(t, fields, major, minor)
                } else {
                    ffi::ee_scenario_effect_load(t, fields, major, minor)
                }
            }
        })
    }

    // ------------------------------------------------------------------------- the media strings, messages
    /// `EESMMediaStrings::Load` / `Save` (the seven strings).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_media_io(m: *mut c_void, file: *mut c_void, save: bool) -> i32 {
        with_stream(file, m, EE_SCENARIO_EN_MEDIA_STRING, 7, null(), null(), null(), 0, 0, |t| {
            // SAFETY: as above
            unsafe { ffi::ee_scenario_strings_io(t, 7, save) }
        })
    }

    /// `EESMDialogMessage::Load` / `Save`.
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_message_io(
        m: *mut c_void,
        file: *mut c_void,
        major: c_ulong,
        minor: c_ulong,
        save: bool,
    ) -> i32 {
        let f = en_ptr(EE_SCENARIO_EN_MESSAGE_FIELDS, m, 0) as *mut EeScenarioMessageFields;
        // SAFETY: the message's record is live; the pointers live through the call
        let numbers = unsafe {
            save::EeScenarioMessageNumbers {
                scroll_speed: &mut (*f).mScrollSpeed,
                unit_type: &mut (*f).mUnitType,
                duration: &mut (*f).mDuration,
                unit_id: &mut (*f).mUnitID,
                cinematic: &mut (*f).mIsCinematic,
            }
        };
        with_stream(file, m, EE_SCENARIO_EN_MESSAGE_STRING, 2, null(), null(), null(), 0, 0, |t| {
            // SAFETY: as above
            unsafe { ffi::ee_scenario_message_io(t, &numbers, major, minor, save) }
        })
    }

    // ----------------------------------------------------------------------------------------- the manager
    /// `EEScenarioManager::Load` (with the SMSV record): `Reset`, then the manager's save.
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_manager_load(
        m: *mut c_void,
        file: *mut c_void,
        major: c_ulong,
        minor: c_ulong,
    ) -> c_long {
        en(EE_SCENARIO_EN_MANAGER_RESET, m, 0, 0);
        let r = manager_io(m, file, major, minor, false);
        if active() {
            cap::ee_scenario_cap_saved(m, 0, r as c_long, version(major, minor));
        }
        r as c_long
    }

    /// `EEScenarioManager::Save`.
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_manager_save(
        m: *mut c_void,
        file: *mut c_void,
        major: c_ulong,
        minor: c_ulong,
    ) -> c_long {
        manager_io(m, file, major, minor, true) as c_long
    }

    fn manager_io(m: *mut c_void, file: *mut c_void, major: c_ulong, minor: c_ulong, save: bool) -> i32 {
        let fields = en_ptr(EE_SCENARIO_EN_MANAGER_FIELDS, m, 0) as *mut EeScenarioManagerFields;
        let timed = en_ptr(EE_SCENARIO_EN_MANAGER_TIMED, m, 0) as *mut EeScenarioTimedQueue;
        with_stream(file, m, EE_SCENARIO_EN_MANAGER_STRING, 2, null(), null(), m, major, minor, |t| {
            // SAFETY: as above
            unsafe {
                if save {
                    ffi::ee_scenario_manager_save(t, fields, timed, minor)
                } else {
                    ffi::ee_scenario_manager_load(t, fields, timed, minor)
                }
            }
        })
    }

    /// TEST ONLY: SMUP of manager `m` (`human` a human plays) with the save version.
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_cap_update(m: *const c_void, human: bool) {
        cap::ee_scenario_cap_update(m, human, save_version());
    }

    /// TEST ONLY: SMCU (`kind`, the 7 longs at `args`, the unit, player and upgrade, the seed) with the save version.
    ///
    /// # Safety
    /// `args` must hold 7 longs; the objects as `ee_scenario_cap_unit_change` takes them.
    #[no_mangle]
    pub unsafe extern "C" fn ee_scenario_en_cap_unit_change(
        kind: c_long,
        args: *const c_long,
        unit: *const c_void,
        player: *const c_void,
        upgrade: *const c_void,
        seed: u32,
    ) {
        // SAFETY: the caller's contract
        unsafe { cap::ee_scenario_cap_unit_change(kind, args, unit, player, upgrade, seed, save_version()) };
    }

    /// The manager's methods over its round host (`EE_SCENARIO_MN_DESTROY` ..): `effect` the effect of the pending and
    /// timed ones, `a`, `b` their numbers.
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_round(m: *mut c_void, op: u32, effect: *const c_void, a: c_long, b: c_long) {
        use crate::scenario::manager_compose as mc;
        let h = round_host(m);
        match op {
            EE_SCENARIO_MN_DESTROY => mc::ee_scenario_manager_destroy(h),
            EE_SCENARIO_MN_END_CINEMATIC => cinematic::ee_scenario_end_cinematic(h),
            EE_SCENARIO_MN_EXIT_CINEMATIC => cinematic::ee_scenario_exit_cinematic(h),
            EE_SCENARIO_MN_START_CINEMATIC => cinematic::ee_scenario_start_cinematic(h),
            EE_SCENARIO_MN_APPLY_PENDING => cinematic::ee_scenario_apply_pending(h),
            EE_SCENARIO_MN_MOVE_CAMERA => mc::ee_scenario_move_camera_to_start(h),
            EE_SCENARIO_MN_ADD_PENDING => mc::ee_scenario_add_pending(h, effect),
            EE_SCENARIO_MN_SET_TIMED_HAS_RUN => mc::ee_scenario_set_timed_has_run(h, effect, a != 0),
            EE_SCENARIO_MN_REPLACE_UNIT => cinematic::ee_scenario_replace_unit(h, a, b),
            _ => {}
        }
    }

    /// `EEScenarioManager::Update`, `ShowDialogMessage`, `Prepare` (`EE_SCENARIO_MN_*`).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_manager(m: *mut c_void, op: u32) {
        let h = round_host(m);
        match op {
            // SAFETY: as above
            EE_SCENARIO_MN_UPDATE => unsafe { ffi::ee_scenario_manager_update(&h) },
            // SAFETY: as above
            EE_SCENARIO_MN_SHOW_DIALOG => unsafe { ffi::ee_scenario_show_dialog(&h) },
            EE_SCENARIO_MN_RESET => {
                // SAFETY: as above
                unsafe { ffi::ee_scenario_manager_reset(h.fields, h.queue) };
                ee_scenario_en_list(m, EE_SCENARIO_LS_RESET, 0, 0, 0, null());
                en(EE_SCENARIO_EN_PENDING_CLEAR, m, 0, 0);
            }
            EE_SCENARIO_MN_PREPARE => {
                cinematic::ee_scenario_unload_media(h, true);
                with_lists(m, null(), |l| {
                    let mut ctx = EeScenarioHostContext::default();
                    let sh = host(&mut ctx);
                    // SAFETY: as above
                    unsafe { lists::ee_scenario_prepare_media(l, &sh) }
                });
            }
            _ => {}
        }
    }

    /// `EESMMediaStrings::ListMediaFiles`: the sound and the map picture (not the movie) of the media strings `m`
    /// appended to `files` in lower case (`PlatMbsLwr`), each missing one's first name and a line feed appended to
    /// `error`; false when one is missing. `scenario` the scenario file's name (checked for its three parts).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_media_files(
        m: *mut c_void,
        scenario: *mut c_void,
        files: *mut c_void,
        error: *mut c_void,
        prefix: bool,
    ) -> bool {
        let text = |s: *mut c_void| {
            let p = en(EE_SCENARIO_EN_STRING_TEXT, s, 0, 0) as usize as *const c_char;
            // SAFETY: a live string's text, copied at once
            unsafe { std::ffi::CStr::from_ptr(p) }.to_string_lossy().into_owned()
        };
        let none = ee_base::util::strings::resource_string(EE_SCENARIO_IDS_SE_MAP_NONE).unwrap_or("");
        let mut result = true;
        for kind in 1..3usize {
            // the sound file (string 4), the map file (string 6)
            let name = text(en_ptr(EE_SCENARIO_EN_MEDIA_STRING, m, if kind == 1 { 4 } else { 6 }));
            let campaign = if prefix {
                let p = en(EE_SCENARIO_EN_CAMPAIGN_PREFIX, null(), 0, 0) as usize as *const c_char;
                // SAFETY: the prefix's live text
                Some(unsafe { std::ffi::CStr::from_ptr(p) }.to_string_lossy().into_owned())
            } else {
                None
            };
            let mut find = |path: &str| {
                let c = std::ffi::CString::new(path).unwrap_or_default();
                en(EE_SCENARIO_EN_DATA_FIND, null(), c.as_ptr() as usize as i64, 0) == 0
            };
            match media_file(&name, none, kind, campaign.as_deref(), &mut find) {
                Ok(Some(file)) => {
                    let mut bytes = std::ffi::CString::new(file).unwrap_or_default().into_bytes_with_nul();
                    // SAFETY: a NUL-terminated buffer
                    unsafe { ee_base::plat_core::string::PlatMbsLwr(bytes.as_mut_ptr()) };
                    en(EE_SCENARIO_EN_STRINGS_PUSH_TEXT, files, bytes.as_ptr() as usize as i64, 0);
                }
                Ok(None) => {}
                Err(first) => {
                    let parts = ee_base::ustring::tokenize(text(scenario).as_bytes(), b"/.\\").len();
                    if parts != 3 {
                        let file = c"C:\\EEX\\Empire Earth\\Scenario Manager\\EESMMediaStrings.cpp";
                        let what = c"theTokens.size() == 3";
                        en(EE_SCENARIO_EN_VERIFY, file.as_ptr() as *mut c_void, 182, what.as_ptr() as usize as i64);
                    }
                    let line = std::ffi::CString::new(first + "\n").unwrap_or_default();
                    en(EE_SCENARIO_EN_STRING_APPEND_TEXT, error, line.as_ptr() as usize as i64, 0);
                    result = false;
                }
            }
        }
        result
    }

    /// `EEScenarioManager::ExportText` (`export`) or `ImportTriggerText` (`kind`, `validate`) of the open `file`.
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_text(
        m: *mut c_void,
        file: *mut c_void,
        export: bool,
        kind: c_ulong,
        validate: bool,
    ) -> c_long {
        let mut ctx = EeScenarioTextContext { manager: m, file };
        with_lists(m, null(), |l| {
            let h = text::EeScenarioTextHost {
                lists: l,
                ctx: &mut ctx as *mut EeScenarioTextContext as *mut c_void,
                op: sys::ee_scenariocpp_text_op,
            };
            // SAFETY: as above
            unsafe {
                if export {
                    text::ee_scenario_text_export(&h)
                } else {
                    text::ee_scenario_text_import(&h, kind, validate)
                }
            }
        })
    }

    /// The manager's list entries (`EE_SCENARIO_LS_*`; `Reset`'s lists, `SortList`, `NewEntry`, `MoveEntry`,
    /// `DeleteEntry`, `DuplicateEntry`, `GetNames`, the `Find*ByID`s, `GetObjectIndex`, `ObjectChanged`): an
    /// element's address, a bool or an index.
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_list(m: *mut c_void, op: u32, list: u32, a: i64, b: i64, p: *mut c_void) -> i64 {
        let names = if op == EE_SCENARIO_LS_NAMES { p } else { null() };
        with_lists(m, names, |l| {
            // SAFETY: as above
            unsafe {
                match op {
                    EE_SCENARIO_LS_RESET => {
                        lists::ee_scenario_list_reset(l);
                        0
                    }
                    EE_SCENARIO_LS_SORT => {
                        lists::ee_scenario_list_sort(l, list);
                        0
                    }
                    EE_SCENARIO_LS_NEW => lists::ee_scenario_list_new(l, list) as usize as i64,
                    EE_SCENARIO_LS_MOVE => {
                        lists::ee_scenario_list_move(l, list, a as c_ulong, b != 0);
                        0
                    }
                    EE_SCENARIO_LS_DELETE => lists::ee_scenario_list_delete(l, list, a as c_ulong) as i64,
                    EE_SCENARIO_LS_DUPLICATE => {
                        lists::ee_scenario_list_duplicate(l, list, a as c_ulong) as usize as i64
                    }
                    EE_SCENARIO_LS_NAMES => {
                        lists::ee_scenario_list_names(l, list);
                        0
                    }
                    EE_SCENARIO_LS_FIND => lists::ee_scenario_list_find(l, list, a as c_long) as usize as i64,
                    EE_SCENARIO_LS_INDEX_OF => lists::ee_scenario_list_index_of(l, list, p) as i64,
                    EE_SCENARIO_LS_OBJECT_CHANGED => {
                        crate::scenario::manager_compose::ee_scenario_object_changed(l, p, a != 0, b as c_long);
                        0
                    }
                    _ => 0,
                }
            }
        })
    }

    // -------------------------------------------------------------------------------------------- the area
    fn area_fields_of(a: *mut c_void) -> &'static mut EeScenarioAreaFields {
        // SAFETY: the area's record is live for the entry's call (the C++ object outlives it); each use ends before
        // the next call back into the C++
        unsafe { &mut *(en_ptr(EE_SCENARIO_EN_AREA_FIELDS, a, 0) as *mut EeScenarioAreaFields) }
    }

    /// `EESMArea::SetAnchor`.
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_area_set_anchor(a: *mut c_void, x: c_long, y: c_long) {
        area_fields_of(a).set_anchor(x, y);
        en(EE_SCENARIO_EN_TILES_CLEAR, en_ptr(EE_SCENARIO_EN_AREA_TILES, a, 0), 0, 0);
    }

    /// `EESMArea::SetRectangle` (`r` a `GERectangle`: bottom, left, right, top).
    ///
    /// # Safety
    /// `r` points to four floats.
    #[no_mangle]
    pub unsafe extern "C" fn ee_scenario_en_area_set_rectangle(a: *mut c_void, r: *const EeScenarioAreaRect) {
        // SAFETY: the caller's contract
        let r = unsafe { *r };
        area_fields_of(a).set_rectangle(r);
        en(EE_SCENARIO_EN_TILES_CLEAR, en_ptr(EE_SCENARIO_EN_AREA_TILES, a, 0), 0, 0);
    }

    /// `EESMArea::Copy`: the name, the text, the numbers but the tile cache.
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_area_copy(a: *mut c_void, from: *mut c_void) {
        for k in 0..2 {
            let s = en(EE_SCENARIO_EN_AREA_STRING, from, k, 0);
            en(EE_SCENARIO_EN_STRING_ASSIGN, en_ptr(EE_SCENARIO_EN_AREA_STRING, a, k), s, 0);
        }
        let src = *area_fields_of(from);
        area_fields_of(a).copy_from(&src);
    }

    /// `EESMArea::ContainsUnit` (with the SMAR record kind 0).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_area_contains_unit(a: *mut c_void, unit: *mut c_void) -> bool {
        let result = !unit.is_null() && {
            let mut ctx = EeScenarioHostContext { unit, ..Default::default() };
            let mut h = host(&mut ctx);
            let record = area_fields_of(a).record();
            area::contains_unit(&mut h, &record)
        };
        if active() {
            let id: c_long = if unit.is_null() { -1 } else { en(EE_SCENARIO_EN_UNIT_ID, unit, 0, 0) as c_long };
            cap::ee_scenario_cap_area(a, 0, id, 0, result as c_long, 0, 0);
        }
        result
    }

    /// `EESMArea::ContainsTile` (with the SMAR record kind 1).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_area_contains_tile(a: *mut c_void, x: c_long, y: c_long) -> bool {
        let result = {
            let mut ctx = EeScenarioHostContext::default();
            let mut h = host(&mut ctx);
            let record = area_fields_of(a).record();
            area::contains_tile(&mut h, &record, x as i32, y as i32)
        };
        if active() {
            cap::ee_scenario_cap_area(a, 1, x, y, result as c_long, 0, 0);
        }
        result
    }

    /// `EESMArea::GetCenterTile` (with the SMAR record kind 2): the centre computed once and cached.
    ///
    /// # Safety
    /// `out` points to two longs.
    #[no_mangle]
    pub unsafe extern "C" fn ee_scenario_en_area_center_tile(a: *mut c_void, out: *mut c_long) {
        let (x, y) = {
            let mut ctx = EeScenarioHostContext::default();
            let mut h = host(&mut ctx);
            let mut record = area_fields_of(a).record();
            let tile = area::center_tile(&mut h, &mut record);
            let f = area_fields_of(a);
            f.mCenterTile.mX = record.center_x as c_long;
            f.mCenterTile.mY = record.center_y as c_long;
            (tile.0 as c_long, tile.1 as c_long)
        };
        // SAFETY: the caller's contract
        unsafe {
            *out = x;
            *out.add(1) = y;
        }
        if active() {
            cap::ee_scenario_cap_area(a, 2, 0, 0, x, y, 0);
        }
    }

    /// `EESMArea::GetTiles` (with the SMAR record kind 3): the cache rebuilt when empty or unread for 3000 ticks,
    /// every read restarting the timer, copied out to `out` (a `std::vector<U2DPoint<long>>`).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_area_tiles(a: *mut c_void, out: *mut c_void) {
        let tiles = en_ptr(EE_SCENARIO_EN_AREA_TILES, a, 0);
        let tick = en(EE_SCENARIO_EN_PHYSICS_TICK, null(), 0, 0) as c_ulong;
        let count = en(EE_SCENARIO_EN_TILES_COUNT, tiles, 0, 0) as usize;
        if area_fields_of(a).tiles_stale(count, tick) {
            en(EE_SCENARIO_EN_TILES_CLEAR, tiles, 0, 0);
            let mut ctx = EeScenarioHostContext { tiles, ..Default::default() };
            let mut h = host(&mut ctx);
            let record = area_fields_of(a).record();
            for (x, y) in area::tiles(&mut h, &record) {
                h.write(EE_SCENARIO_WRITE_TILE, 0, x as i64, y as i64, 0);
            }
        }
        area_fields_of(a).mTilesTime = tick;
        en(EE_SCENARIO_EN_TILES_COPY_TO, tiles, out as usize as i64, 0);
        if active() {
            let n = en(EE_SCENARIO_EN_TILES_COUNT, out, 0, 0) as usize;
            let data = en(EE_SCENARIO_EN_TILES_DATA, out, 0, 0) as usize as *const c_long;
            // SAFETY: the list's `n` tiles
            let hash = unsafe { crate::scenario::object_reads::ee_scenario_tiles_hash(data, n) };
            let time = area_fields_of(a).mTilesTime as c_long;
            cap::ee_scenario_cap_area(a, 3, time, 0, n as c_long, 0, hash);
        }
    }

    /// `EESMArea::Load` / `Save` (the name, the text, the numbers through the save's copy).
    #[no_mangle]
    pub extern "C" fn ee_scenario_en_area_io(a: *mut c_void, file: *mut c_void, save: bool) -> i32 {
        let mut record = area_fields_of(a).save_record();
        let r = with_stream(file, a, EE_SCENARIO_EN_AREA_STRING, 2, null(), null(), null(), 0, 0, |t| {
            // SAFETY: as above
            unsafe { ffi::ee_scenario_area_io(t, &mut record, save) }
        });
        area_fields_of(a).store_save_record(&record);
        r
    }

    /// The scenario manager's areas as the continent manager's `BuildRegions` reads them (`EEAIContinentManager`:
    /// for each area, [`area_fields::region_input`]), with their ids; empty without a manager.
    pub fn region_inputs() -> (Vec<crate::astar::manager::EeScenarioArea>, Vec<c_long>) {
        let manager = en_ptr(EE_SCENARIO_EN_MANAGER, null(), 0);
        if manager.is_null() {
            return (Vec::new(), Vec::new());
        }
        let n = en(EE_SCENARIO_EN_AREA_COUNT, manager, 0, 0) as usize;
        (0..n)
            .map(|i| {
                let area = en_ptr(EE_SCENARIO_EN_AREA_INDEX, manager, i as i64);
                let fields = (!area.is_null()).then(|| *area_fields_of(area));
                area_fields::region_input(fields.as_ref())
            })
            .unzip()
    }

    /// [`region_inputs`] for the C++: the count of areas, and (when `capacity` holds them) each area's input and id.
    ///
    /// # Safety
    /// `inputs` and `ids` point to `capacity` entries (or `capacity` is 0).
    #[no_mangle]
    pub unsafe extern "C" fn ee_scenario_area_region_inputs(
        inputs: *mut crate::astar::manager::EeScenarioArea,
        ids: *mut c_long,
        capacity: usize,
    ) -> usize {
        let (a, i) = region_inputs();
        if a.len() <= capacity && !a.is_empty() {
            // SAFETY: the caller's contract
            unsafe {
                std::ptr::copy_nonoverlapping(a.as_ptr(), inputs, a.len());
                std::ptr::copy_nonoverlapping(i.as_ptr(), ids, i.len());
            }
        }
        a.len()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn captured_runs_once_without_the_capture() {
        let log = std::cell::RefCell::new(Vec::new());
        let r = captured(
            false,
            &mut || {
                log.borrow_mut().push("seed");
                7
            },
            &mut || {
                log.borrow_mut().push("run");
                true
            },
            &mut |_, _| log.borrow_mut().push("record"),
        );
        assert!(r);
        assert_eq!(*log.borrow(), vec!["run"]);
    }

    #[test]
    fn captured_takes_the_seed_before_the_run() {
        let log = std::cell::RefCell::new(Vec::new());
        let mut got = None;
        let r = captured(
            true,
            &mut || {
                log.borrow_mut().push("seed");
                7
            },
            &mut || {
                log.borrow_mut().push("run");
                5
            },
            &mut |r, s| {
                log.borrow_mut().push("record");
                got = Some((r, s));
            },
        );
        assert_eq!(r, 5);
        assert_eq!(got, Some((5, 7)));
        assert_eq!(*log.borrow(), vec!["seed", "run", "record"]);
    }

    #[test]
    fn media_files_are_searched_in_order() {
        let mut asked = Vec::new();
        // the second extension found, with the campaign prefix
        let r = media_file("intro", "<NONE>", 1, Some("pre_"), &mut |p| {
            asked.push(p.to_string());
            p.ends_with(".mp3")
        });
        assert_eq!(r, Ok(Some("Sounds\\pre_intro.mp3".to_string())));
        assert_eq!(asked, vec!["Sounds\\intro.wav", "Sounds\\intro.mp3"]);
        assert_eq!(media_file("map", "<NONE>", 2, None, &mut |_| true), Ok(Some("Scenarios\\map.jpg".into())));
        assert_eq!(media_file("<NONE>", "<NONE>", 2, None, &mut |_| true), Ok(None));
        assert_eq!(media_file("", "<NONE>", 1, None, &mut |_| true), Ok(None));
        assert_eq!(media_file("x", "<NONE>", 1, None, &mut |_| false), Err("x.wav".to_string()));
    }

    /// The trigger statements' entry ranges and the end operator (once EESMObject.cpp's static_assert).
    #[test]
    fn trigger_numbers() {
        use crate::scenario::trigger_members::{EE_SCENARIO_TR_CONDITIONS, EE_SCENARIO_TR_EFFECTS};
        assert_eq!(EE_SCENARIO_TR_EFFECTS - EE_SCENARIO_TR_CONDITIONS, 20);
        assert_eq!(crate::scenario::ids::EE_SCENARIO_SM_TRIGGER_OPERATOR_END, 0);
    }

    #[test]
    fn replace_ids_changes_every_match() {
        let mut v = vec![3u64, 5, 3, 7];
        let n = v.len();
        let copy = v.clone();
        replace_ids(n, &mut |i| copy[i], &mut |i, x| v[i] = x, 3, 9);
        assert_eq!(v, vec![9, 5, 9, 7]);
    }

    #[test]
    fn context_records_are_pointer_lists() {
        assert_eq!(std::mem::size_of::<EeScenarioHostContext>(), 6 * std::mem::size_of::<usize>());
        assert_eq!(std::mem::size_of::<EeScenarioListContext>(), 2 * std::mem::size_of::<usize>());
        assert_eq!(std::mem::size_of::<EeScenarioTextContext>(), 2 * std::mem::size_of::<usize>());
        assert!(EeScenarioHostContext::default().unit.is_null());
    }
}
