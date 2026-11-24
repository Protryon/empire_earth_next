//! The game options' bodies (game core lane, round 11, lane B; were EEGameOptions.cpp's constructors,
//! `operator=`, `SetDefaults`, `Load`, `Save` and their host, `GetMapDimensions`, `GetCampaignMapName`,
//! `CanBuyCivilization` and the limits' reads): ee_game app game_options.rs's decisions over the C++ strings and
//! slots, each a one-statement case of EEGameLog.cpp's `EECore11OpB` ([`super::EeGameCpp`]'s `Eo` ops).
//!
//! `EEGameOptions` keeps its members in place (the UI, the lobby, the network messages and the saves read them):
//! the plain ones are `EeAppOptionsFields`, its base, each slot's `EeAppOptionsPlayerFields`; the strings and the
//! slots' names are `UWideString`s reached by address. The saves' gate (saveload) reads `Load` and `Save`.
#![allow(clippy::not_unsafe_ptr_arg_deref, clippy::missing_safety_doc)]

use std::ffi::{c_long, c_ulong, c_void};

use ee_sim::units::player_save::{
    EeUnitsPlayerSaveIo, EE_UNITS_PLAYER_SAVE_READ_BOOL, EE_UNITS_PLAYER_SAVE_READ_FLOAT,
    EE_UNITS_PLAYER_SAVE_READ_LONG, EE_UNITS_PLAYER_SAVE_READ_ULONG, EE_UNITS_PLAYER_SAVE_WRITE_BOOL,
    EE_UNITS_PLAYER_SAVE_WRITE_FLOAT, EE_UNITS_PLAYER_SAVE_WRITE_LONG, EE_UNITS_PLAYER_SAVE_WRITE_ULONG,
};

use crate::app::game_options::{
    self, EeAppOptionsFields, EeAppOptionsLimits, EeAppOptionsPlayerFields, EE_APP_OPTIONS_NAME_LOAD,
    EE_APP_OPTIONS_NAME_SAVE, EE_APP_OPTIONS_SLOTS, EE_APP_OPTIONS_STRING_LOAD, EE_APP_OPTIONS_STRING_SAVE,
    EE_APP_OPTIONS_VARIANT_KNOWN,
};
use crate::app::save_io::{
    EeAppSaveIo, SaveHost, EE_APP_SAVE_READ_BOOL, EE_APP_SAVE_READ_FLOAT, EE_APP_SAVE_READ_LONG,
    EE_APP_SAVE_READ_ULONG, EE_APP_SAVE_WRITE_BOOL, EE_APP_SAVE_WRITE_FLOAT, EE_APP_SAVE_WRITE_LONG,
    EE_APP_SAVE_WRITE_ULONG,
};

use super::preferences::set_text;
use super::global_objects as gobj;
use super::{h, hp, pa, ptr, O, P};

// ee_app save_io.rs mirrors EESavePrimitive's operations and record (ee_sim units player_save.rs)
const _: () = assert!(
    EE_APP_SAVE_READ_ULONG == EE_UNITS_PLAYER_SAVE_READ_ULONG
        && EE_APP_SAVE_READ_LONG == EE_UNITS_PLAYER_SAVE_READ_LONG
        && EE_APP_SAVE_READ_FLOAT == EE_UNITS_PLAYER_SAVE_READ_FLOAT
        && EE_APP_SAVE_READ_BOOL == EE_UNITS_PLAYER_SAVE_READ_BOOL
        && EE_APP_SAVE_WRITE_ULONG == EE_UNITS_PLAYER_SAVE_WRITE_ULONG
        && EE_APP_SAVE_WRITE_LONG == EE_UNITS_PLAYER_SAVE_WRITE_LONG
        && EE_APP_SAVE_WRITE_FLOAT == EE_UNITS_PLAYER_SAVE_WRITE_FLOAT
        && EE_APP_SAVE_WRITE_BOOL == EE_UNITS_PLAYER_SAVE_WRITE_BOOL
        && std::mem::size_of::<EeAppSaveIo>() == std::mem::size_of::<EeUnitsPlayerSaveIo>()
        && std::mem::offset_of!(EeAppSaveIo, f) == std::mem::offset_of!(EeUnitsPlayerSaveIo, f)
        && std::mem::offset_of!(EeAppSaveIo, b) == std::mem::offset_of!(EeUnitsPlayerSaveIo, b)
);

/// `kLoadTypeCount` (ee/EEGameOptions.h).
const LOAD_TYPES: c_ulong = 6;
/// The constructors' assertion: the file and the expression (ESVERIFY_AT's).
const OPTIONS_CPP: &std::ffi::CStr = c"C:\\EEX\\Empire Earth\\Empire Earth\\EEGameOptions.cpp";
const LOCK_EXPRESSION: &std::ffi::CStr = c"TSScheduler::CreateReadWriteLock(this->mLock) == keseOK";

/// The option strings by index (`EE_APP_OPTIONS_CAMPAIGN`, `_SCENARIO`, `_MAP_NAME`).
const STRINGS: [usize; 3] = [gobj::OPT_CAMPAIGN_NAME, gobj::OPT_SCENARIO_NAME, gobj::OPT_MAP_NAME];

fn fields<'a>(o: P) -> &'a mut EeAppOptionsFields {
    // SAFETY: the options' plain members (the base), used before the next C++ call that writes them
    unsafe { &mut *(o as *mut EeAppOptionsFields) }
}

fn slot(o: P, i: u32) -> *mut EeAppOptionsPlayerFields {
    gobj::opt_slot(o, i) as *mut EeAppOptionsPlayerFields
}

/// `EEGameOptionsDefaults`: what the constructors read (the rules and the epoch count only: the global options are
/// made before the tables, the variants and the AI manifest exist).
fn defaults() -> EeAppOptionsLimits {
    EeAppOptionsLimits {
        epochs: ee_sim::tech::tech_ids::ee_tech_epoch_count() as c_long,
        map_size_default: h(O::EoMapSizeDefault, 0, &[]) as c_ulong,
        speed_default: h(O::EoSpeedDefault, 0, &[]) as c_ulong,
        wonders_default: h(O::EoWondersDefault, 0, &[]) as c_ulong,
        ..Default::default()
    }
}

/// `EEGameOptionsLimits`: gameoptions.yaml's lists and numbers, the tables' sizes (the engine choices only for a
/// load).
fn limits(load: bool) -> EeAppOptionsLimits {
    EeAppOptionsLimits {
        load_types: LOAD_TYPES,
        resource_levels: h(O::EoResourceLevels, 0, &[]) as c_ulong,
        variants: h(O::EoVariants, 0, &[]) as c_ulong,
        epochs: ee_sim::tech::tech_ids::ee_tech_epoch_count() as c_long,
        map_sizes: h(O::EoMapSizes, 0, &[]) as c_ulong,
        map_size_default: h(O::EoMapSizeDefault, 0, &[]) as c_ulong,
        difficulties: h(O::EoDifficulties, 0, &[]) as c_ulong,
        speeds: h(O::EoSpeeds, 0, &[]) as c_ulong,
        speed_default: h(O::EoSpeedDefault, 0, &[]) as c_ulong,
        wonders_default: h(O::EoWondersDefault, 0, &[]) as c_ulong,
        wonders_maximum: h(O::EoWondersMaximum, 0, &[]) as c_ulong,
        unit_limit_minimum: h(O::EoUnitLimitMinimum, 0, &[]) as c_ulong,
        unit_limit_maximum: h(O::EoUnitLimitMaximum, 0, &[]) as c_ulong,
        unit_limit_default: h(O::EoUnitLimitDefault, 0, &[]) as c_ulong,
        engine_choices: if load { crate::cp_engine::ee_cp_engine_choice_count() as c_ulong } else { 0 },
    }
}

/// The lock (`TSScheduler::CreateReadWriteLock`: ee_base's Thread System, asserted at the constructor's line) and
/// the defaults' plain members.
fn construct(o: P, line: c_ulong) {
    let lock = (o + gobj::OPT_LOCK) as *mut *mut ee_base::ts::EeTsLock;
    // SAFETY: the options' lock member
    if unsafe { ee_base::ts::ee_ts_lock_new(lock) } != ee_base::ts::EE_TS_OK {
        // SAFETY: C strings
        unsafe { ee_base::error::ee_error_verify(OPTIONS_CPP.as_ptr(), line, LOCK_EXPRESSION.as_ptr()) };
    }
    let l = defaults();
    *fields(o) = game_options::options_default(&l);
}

/// The four strings "" and the sixteen slots empty (`EEGameOptionsPlayer()`).
fn clear_strings_and_slots(o: P) {
    for off in [gobj::OPT_MAP_TYPE, gobj::OPT_CAMPAIGN_NAME, gobj::OPT_SCENARIO_NAME, gobj::OPT_MAP_NAME] {
        set_text(o + off, true, b"");
    }
    for i in 0..EE_APP_OPTIONS_SLOTS {
        h(O::EoSlotEmpty, o, &[i64::from(i)]);
    }
}

/// `EEGameOptions()`: default options (random epochs, normal speed and map size, unit limit 50, three wonders to
/// win) with sixteen empty slots.
#[no_mangle]
pub extern "C" fn ee_gamecpp_options_init(options: *mut c_void) {
    let o = options as P;
    construct(o, 304);
    clear_strings_and_slots(o);
}

/// `EEGameOptions(const EEGameOptions&)`: its own lock, the defaults, then the assignment (the copy keeps the
/// defaults' mB407: the assignment leaves it).
#[no_mangle]
pub extern "C" fn ee_gamecpp_options_copy(options: *mut c_void, other: *const c_void) {
    construct(options as P, 349);
    ee_gamecpp_options_assign(options, other);
}

/// `operator=`: each out-of-range setting replaced with a default (game_options.rs assign); the names, flags and
/// the sixteen slots copied as they are. A new game's map type must be a known generator's, else the first one's.
#[no_mangle]
pub extern "C" fn ee_gamecpp_options_assign(options: *mut c_void, other: *const c_void) {
    let (o, src) = (options as P, other as P);
    let l = limits(false);
    let source = *fields(src);
    let mut target = *fields(o);
    game_options::assign(&mut target, &source, &l);
    *fields(o) = target;
    let map_type = src + gobj::OPT_MAP_TYPE;
    let mut index: c_ulong = 0;
    if source.mLoadType != 0 || h(O::EoFindGenerator, map_type, &[ptr(&mut index)]) == 0 {
        h(O::EpWideCopy, o + gobj::OPT_MAP_TYPE, &[pa(map_type)]);
    } else {
        first_generator(o);
    }
    for op in STRINGS {
        h(O::EpWideCopy, o + op, &[pa(src + op)]);
    }
    for i in 0..EE_APP_OPTIONS_SLOTS {
        h(O::EoSlotCopy, o, &[i64::from(i), pa(src)]);
    }
}

/// The map type the first generator's name ("" with none).
fn first_generator(o: P) {
    let map_type = o + gobj::OPT_MAP_TYPE;
    if h(O::EoGeneratorCount, 0, &[]) >= 1 {
        h(O::EoFirstGeneratorName, map_type, &[]);
    } else {
        set_text(map_type, true, b"");
    }
}

/// `SetDefaults`: the lobby's defaults (all resources, random epochs, the first map type, cheats on, victory off,
/// the preferred difficulty) with sixteen empty slots.
#[no_mangle]
pub extern "C" fn ee_gamecpp_options_set_defaults(options: *mut c_void) {
    let o = options as P;
    h(O::EoAssign, o, &[pa(hp(O::EoLobbyOptions, 0, &[]))]);
    let l = limits(false);
    game_options::set_defaults(fields(o), &l, h(O::EoDifficultyLevel, 0, &[]) as c_ulong);
    first_generator(o);
    for op in STRINGS {
        set_text(o + op, true, b"");
    }
    for i in 0..EE_APP_OPTIONS_SLOTS {
        h(O::EoSlotEmpty, o, &[i64::from(i)]);
    }
}

/// `GetMapDimensions`: the map's width and height in tiles for the map size (0 x 0 for an unknown size).
#[no_mangle]
pub unsafe extern "C" fn ee_gamecpp_options_map_dimensions(
    options: *mut c_void,
    width: *mut c_long,
    height: *mut c_long,
) {
    let size = fields(options as P).mMapSize;
    let tiles =
        if size < h(O::EoMapSizes, 0, &[]) as c_ulong { h(O::EoMapSizeTiles, 0, &[size as i64]) as c_long } else { 0 };
    // SAFETY: the caller's out values
    unsafe {
        *width = tiles;
        *height = tiles;
    }
}

/// `GetCampaignMapName`: the campaign scenario's map name when the options name a campaign with a checksum and a
/// map, else "".
#[no_mangle]
pub extern "C" fn ee_gamecpp_options_campaign_map_name(options: *mut c_void, name: *mut c_void) -> bool {
    let o = options as P;
    let found = h(O::EpWideLength, o + gobj::OPT_CAMPAIGN_NAME, &[]) > 0
        && fields(o).mCampaignChecksum > 0
        && h(O::EpWideLength, o + gobj::OPT_MAP_NAME, &[]) > 0;
    if found {
        h(O::EpWideCopy, name as P, &[pa(o + gobj::OPT_MAP_NAME)]);
    } else {
        set_text(name as P, true, b"");
    }
    found
}

/// `CanBuyCivilization`: not in the scenario editor and not in a scenario game.
#[no_mangle]
pub extern "C" fn ee_gamecpp_options_can_buy_civilization(options: *mut c_void) -> bool {
    h(O::EoInScenarioEditor, 0, &[]) == 0 && !fields(options as P).mIsScenarioGame
}

/// A saved game's file under the options: its primitives (`EESavePrimitive`), the slots' names and the strings.
struct OptionsFile {
    o: P,
    file: P,
}

impl SaveHost for OptionsFile {
    fn call(&mut self, op: i32, index: u32, io: &mut EeAppSaveIo) -> i32 {
        let i = i64::from(index);
        (match op {
            EE_APP_OPTIONS_NAME_LOAD => h(O::EoWideLoad, gobj::opt_slot_name(self.o, index), &[pa(self.file)]),
            EE_APP_OPTIONS_NAME_SAVE => h(O::EoWideSave, gobj::opt_slot_name(self.o, index), &[pa(self.file)]),
            EE_APP_OPTIONS_STRING_LOAD => h(O::EoWideLoad, self.o + STRINGS[index as usize], &[pa(self.file)]),
            EE_APP_OPTIONS_STRING_SAVE => h(O::EoWideSave, self.o + STRINGS[index as usize], &[pa(self.file)]),
            EE_APP_OPTIONS_VARIANT_KNOWN => h(O::EoVariantKnown, 0, &[i]),
            _ => h(O::EoPrimitive, self.file, &[i64::from(op), ptr(io)]),
        }) as i32
    }
}

fn slots_in(o: P) -> Vec<EeAppOptionsPlayerFields> {
    // SAFETY: the slots' plain members, copied in (the host calls touch only the C++ members)
    (0..EE_APP_OPTIONS_SLOTS).map(|i| unsafe { *slot(o, i) }).collect()
}

/// `Load`: the options and the sixteen slots from a saved game (game_options.rs load) -> the first read error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_options_load(
    options: *mut c_void,
    major: c_ulong,
    minor: c_ulong,
    file: *mut c_void,
) -> c_long {
    let o = options as P;
    let l = limits(true);
    let mut f = *fields(o);
    let mut p = slots_in(o);
    let r = game_options::load(&mut OptionsFile { o, file: file as P }, &mut f, &mut p, major as u32, minor as u32, &l);
    *fields(o) = f;
    for (i, v) in p.iter().enumerate() {
        // SAFETY: the slot's plain members
        unsafe { *slot(o, i as u32) = *v };
    }
    r as c_long
}

/// `Save`: the sixteen slots and the options to a saved game (game_options.rs save) -> the first write error.
#[no_mangle]
pub extern "C" fn ee_gamecpp_options_save(options: *mut c_void, file: *mut c_void) -> c_long {
    let o = options as P;
    let f = *fields(o);
    let p = slots_in(o);
    game_options::save(&mut OptionsFile { o, file: file as P }, &f, &p) as c_long
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::app::game_options::{EE_APP_OPTIONS_CAMPAIGN, EE_APP_OPTIONS_MAP_NAME, EE_APP_OPTIONS_SCENARIO};

    #[test]
    fn the_strings_are_in_the_files_order() {
        assert_eq!(STRINGS[EE_APP_OPTIONS_CAMPAIGN as usize], gobj::OPT_CAMPAIGN_NAME);
        assert_eq!(STRINGS[EE_APP_OPTIONS_SCENARIO as usize], gobj::OPT_SCENARIO_NAME);
        assert_eq!(STRINGS[EE_APP_OPTIONS_MAP_NAME as usize], gobj::OPT_MAP_NAME);
        assert_eq!(LOAD_TYPES, 6);
    }
}
