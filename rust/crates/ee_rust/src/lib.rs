//! The game's Rust static library. `include/rust/ee_rust.h` is generated from the `extern "C"` items here
//! (`cargo run -p xtask -- cbindgen`).

use std::ffi::c_char;

use ee_codec as _;
use ee_fs as _;
// the Graphics Engine's math and the table trigonometry (ee_ge/src/ffi.rs), called by the C++ GE classes and UMath.cpp
use ee_ge as _;
// the Graphics Thread's and the camera's math (ee_gfx/src/ffi.rs), called by EECameraControl.cpp
use ee_gfx as _;
// the DX7 renderer plug-in behind the C++ trampolines (ee_dx7/src/ffi.rs); it imports them, absent from a test binary
#[cfg(not(test))]
use ee_dx7 as _;
#[cfg(not(test))] // its FFI imports the platform layer's window functions (absent from a test binary)
use ee_gx as _;
#[cfg(not(test))] // its FFI imports the C++ shim
use ee_mixer as _;
#[cfg(not(test))] // its FFI imports the platform layer, absent from a test binary
use ee_net_tcp as _;
use ee_random as _;
#[cfg(not(test))]
// the sound manager behind SESoundManager.cpp: its FFI imports the C++ trampolines (ee_soundcpp_*)
use ee_sound as _;
// the UI toolkit's algorithms behind the C++ UI* classes (ee_ui); its FFI imports C++ host functions, absent from a
// test binary
#[cfg(not(test))]
use ee_ui as _;
// the random map generator's `extern "C"` API (ee_rmap/src/ffi.rs) reaches the staticlib through this dependency
use ee_rmap as _;
// formations (ee_formations/src/ffi.rs): the geometry the formation classes call
use ee_formations as _;
// the tile A* searches' storage and machine (ee_astar/src/ffi.rs), called by the C++ EEAIAStar* classes
use ee_astar as _;
// the combat and unit formulas (ee_combat/src/ffi.rs) the C++ unit methods call (ee/EECombatFormulas.h)
use ee_combat as _;
// the tech tree's research and prerequisites, the event effects' arithmetic, the calamities' timing (ee_tech/src/ffi.rs),
// called by the C++ tech tree, EEEvent.cpp and the calamity classes
use ee_tech as _;
// the scenario manager's conditions, player choices and triggers (ee_scenario/src/ffi.rs), called by the C++
// EESMCondition, EESMTrigger and EEScenarioManager over their host (Scenario Manager/EEScenarioHost.cpp)
use ee_scenario as _;
// the data layer's `extern "C"` API (ee_data/src/ffi.rs) reaches the staticlib through this dependency
use ee_data as _;

// glTF -> .cem and PNG -> .sst (ee_openformats/src/ffi.rs): the open-format readers GEOpenFormatsFile.cpp calls
use ee_openformats as _;

// The platform layer's leaves: its `#[no_mangle]` functions and variables reach the staticlib through this
// dependency.
use ee_plat_core as _;

// The platform layer's PlatFile, PlatThread and PlatSocket: `#[no_mangle]` exports, as above.
use ee_plat_sys as _;

// The window (PlatWindow.h on SDL3) and the fonts (PlatFont.h): `#[no_mangle]` exports. Two
// crates so that a program using only text does not pull in an object file that references SDL (the linker pulls whole
// objects); ee_plat_sdl's exports are compiled out of `cargo test`, whose binaries do not link SDL.
#[cfg(not(test))]
use ee_plat_sdl as _;
use ee_plat_ui as _;

// the WON library's leaves (ee_won/src/ffi.rs): the buffers' and text helpers, CRC-16, MD5, Blowfish, ElGamal, the auth
// blocks and the CD keys behind the C++ forwarders of src/Empire Earth/WONAPI
use ee_won as _;

// the unit registry behind EEUnitHandle<T> and the unit store (ee_units/src/ffi.rs), called by RTSUnit.cpp and
// EEUnitStore.cpp; its host imports the C++ world calls (EEUnitHost.cpp), absent from a test binary
#[cfg(not(test))]
use ee_units as _;

/// The combat reads' bridge to the unit store (ee_combat store.rs: ee_combat cannot depend on ee_units, whose host
/// imports C++): the record of the unit object (an `EEUnit*`'s address: ee_units' object index) into `out`; a unit
/// the store has no record of (one out of every player's list) filled from the object by the store's own C++ fill.
/// False for no object (always in a test binary, which has no store's host).
///
/// # Safety
/// `out` must point to a writable record; `object` is 0 or a live unit.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_store_unit_of(object: u64, out: *mut ee_combat::store::EeCombatStoredUnit) -> bool {
    #[cfg(not(test))]
    {
        if object == 0 {
            return false;
        }
        let r = match ee_units::store::with_store(|s| s.by_object(object).copied()) {
            Some(r) => r,
            None => {
                let mut r = ee_units::store::EeUnitRecord::default();
                // SAFETY: the C++ fills one record of the live unit
                unsafe { host::ee_combat_host_unit_record(object, &mut r) };
                r
            }
        };
        // SAFETY: the function's contract
        unsafe { *out = combat_stored(&r) };
        true
    }
    #[cfg(test)]
    {
        let _ = (object, out);
        false
    }
}

/// The bridge of the stats table (ee_combat store.rs, ee_units stats.rs): the record of the stats object (an
/// `EEUnitClassUpgrade*`'s address, a unit record's `stats`) into `out`. False for none, or one the table lacks (always
/// in a test binary).
///
/// # Safety
/// `out` must point to a writable record.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_store_stats_of(
    object: u64,
    out: *mut ee_combat::store::EeCombatStoredStats,
) -> bool {
    #[cfg(not(test))]
    {
        let Some(s) = ee_units::stats::stats_of(object) else { return false };
        let stats = ee_combat::store::EeCombatStoredStats {
            scale: s.scale,
            range: s.range,
            max_velocity: s.max_velocity,
            attack: s.attack,
            flight_time: s.flight_time,
            reload_time: s.reload_time,
            max_hit_points: s.max_hit_points,
            healing_points: s.healing_points,
            armor: s.armor,
            can_fire_over_walls: s.can_fire_over_walls,
        };
        // SAFETY: the function's contract
        unsafe { *out = stats };
        true
    }
    #[cfg(test)]
    {
        let _ = (object, out);
        false
    }
}

/// The bridge of the players' tables (ee_combat store.rs): the table of the player object (an `EEPlayer*`'s address)
/// into `out`; one without a table read from the object by the C++. False for no object (always in a test binary).
///
/// # Safety
/// `out` must point to a writable table; `object` is 0 or a live player.
#[no_mangle]
pub unsafe extern "C" fn ee_combat_store_player_of(
    object: u64,
    out: *mut ee_combat::store::EeCombatStoredPlayer,
) -> bool {
    #[cfg(not(test))]
    {
        if object == 0 {
            return false;
        }
        let table = match ee_units::players::with_players(|p| p.get(object).copied()) {
            Some(p) => ee_combat::store::EeCombatStoredPlayer { id: p.id, enemies: p.enemies },
            None => {
                let mut t = ee_combat::store::EeCombatStoredPlayer::default();
                // SAFETY: the C++ reads the live player's id and 17 stances
                unsafe { host::ee_combat_host_player_table(object, &mut t.id, t.enemies.as_mut_ptr()) };
                t
            }
        };
        // SAFETY: the function's contract
        unsafe { *out = table };
        true
    }
    #[cfg(test)]
    {
        let _ = (object, out);
        false
    }
}

/// The store's C++ fills the bridges fall back on (EEUnitStore.cpp; declared nowhere else: cbindgen must not).
/// cbindgen:ignore
#[cfg(not(test))]
mod host {
    extern "C" {
        /// The record of the live unit `object` as its object is now (its `mPlayer` as the list's player).
        pub fn ee_combat_host_unit_record(object: u64, out: *mut ee_units::store::EeUnitRecord);
        /// The live player `object`'s `mPlayerID` and 17 `mEnemies` words.
        pub fn ee_combat_host_player_table(object: u64, id: *mut u32, enemies: *mut u32);
    }
}

#[cfg(not(test))]
fn combat_stored(r: &ee_units::store::EeUnitRecord) -> ee_combat::store::EeCombatStoredUnit {
    ee_combat::store::EeCombatStoredUnit {
        object: r.object,
        player: r.player,
        id: r.id,
        object_kind: r.object_kind,
        complex: r.complex,
        built: r.built,
        cloakable: r.cloakable,
        cloaked: r.cloaked,
        herd: r.herd,
        container: r.container,
        crusader: r.crusader,
        paratrooper_bomber: r.paratrooper_bomber,
        class_flags: r.class_flags,
        domains: r.domains,
        land_building: r.land_building,
        goal_kind: r.goal_kind,
        tile_x: r.tile_x,
        tile_y: r.tile_y,
        hit_points: r.hit_points,
        unit_type: r.unit_type,
        category: r.category,
        command: r.command,
        location: r.location,
        velocity: r.velocity,
        heal_threshold: r.heal_threshold,
        target_search_timer: r.target_search_timer,
        reload_timer: r.reload_timer,
        rotting_away_time: r.rotting_away_time,
        flight_time: r.flight_time,
        cloak_timer: r.cloak_timer,
        family: r.family,
        cloak_radius: r.cloak_radius,
        resource_amount: r.resource_amount,
        stats: r.stats,
    }
}

// the game messages' wire codec (ee_netmsg/src/ffi.rs), called by EEMessage.cpp's ExportMessage/ImportMessage
use ee_netmsg as _;
// the game screens' decisions (ee_screens: the settings form's presets and value tables) behind the C++ event classes
// (the menus' FFI imports the C++ host's calls, absent from a test binary)
#[cfg(not(test))]
use ee_screens as _;
// the computer player's leaves (ee_ai/src/ffi.rs): the next engine's save blocks, the AI manifest and the seat choice,
// the name tables, called by EECPNextLua.cpp, EECPNextUnits.cpp, EECPEngine.cpp and EECPNext.cpp; the Lua runtime
// (lua_ffi.rs), called by EECPLua.cpp (it imports the vendored Lua the C++ build compiles, absent from a test binary)
#[cfg(not(test))]
use ee_ai as _;

/// Version of the Rust library's C interface; bumped when a seam changes incompatibly.
pub const EE_RUST_ABI_VERSION: u32 = 1;

/// Returns `EE_RUST_ABI_VERSION`.
#[no_mangle]
pub extern "C" fn ee_rust_abi_version() -> u32 {
    EE_RUST_ABI_VERSION
}

/// Installs the panic hook: a Rust panic gives `log` one line, then the process aborts. Call once at startup.
///
/// # Safety
/// `log` must be callable from any thread for the life of the process, with a NUL-terminated line valid during the
/// call.
#[no_mangle]
pub unsafe extern "C" fn ee_rust_init(log: unsafe extern "C" fn(line: *const c_char)) {
    ee_panic::install(log);
    // the combat goals' bodies behind ee_units' goal dispatch (attack unit, attack ground: ee_combat goal_dispatch.rs)
    #[cfg(not(test))]
    ee_units::rust_goal::register_other_goal_kinds(ee_combat::goal_dispatch::rust_goal);
}

// ee_combat's mirrors of ee_units' goal host and block, and the operation numbers it copies
#[cfg(not(test))]
const _: () = {
    use std::mem::{align_of, size_of};
    assert!(size_of::<ee_units::rust_goal::EeUnitsGoalIo>() == size_of::<ee_combat::goal_objects::EeCombatGoalIo>());
    assert!(align_of::<ee_units::rust_goal::EeUnitsGoalIo>() == align_of::<ee_combat::goal_objects::EeCombatGoalIo>());
    assert!(
        size_of::<ee_units::rust_goal::EeUnitsGoalHost>() == size_of::<ee_combat::goal_objects::EeCombatGoalTable>()
    );
    assert!(ee_units::rust_goal::EE_UNITS_GOAL_SET_PATH_ATTEMPTS == 20);
    assert!(ee_units::rust_goal::EE_UNITS_GOAL_SUITS == 19);
    assert!(ee_units::rust_goal::EE_UNITS_GOAL_DESTROY == 1);
    assert!(ee_units::rust_goal::EE_UNITS_GOAL_HOST_NEW_GOAL == 16);
    assert!(ee_units::rust_goal::EE_UNITS_GOAL_HOST_APPEND == 20);
    assert!(ee_units::rust_goal::EE_UNITS_GOAL_HOST_UNIT_ID == 3);
    assert!(ee_units::rust_goal::EE_UNITS_GOAL_HOST_UNIT_TILE == 4);
    assert!(ee_units::rust_goal::EE_UNITS_GOAL_HOST_HIT_POINTS == 64);
};

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn abi_version_is_exported_unchanged() {
        assert_eq!(ee_rust_abi_version(), EE_RUST_ABI_VERSION);
    }
}
