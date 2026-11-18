//! The world's objects as Rust reads them in place ("Rust layout, C++ allocation"; docs/progress/world-views.md).
//!
//! The records the C++ classes derive from are defined here ([`unit_fields`], [`unit_class_fields`],
//! [`stats_fields`], [`player_fields`], [`map_fields`]; ee_sim::units re-exports them at their old paths). The
//! objects stay C++ allocations: [`layout`] holds where the compiler put the records and the pointer members (the
//! C++ registers it once, `EEWorldLayout.cpp`), and [`view`] the typed read-only views over them (`UnitView`,
//! `ClassView`, `StatsView`, `PlayerView`, `MapView`, `TileView`, `WorldView`), each bound to a [`WorldRead`]
//! token. [`check`] is the TEST ONLY VWCK digest the C++ compares with its own every tick.

pub mod check;
pub mod layout;
pub mod map_fields;
pub mod player_fields;
pub mod stats_fields;
pub mod tech_view;
pub mod unit_class_fields;
pub mod unit_fields;
pub mod view;
#[cfg(test)]
mod view_tests;

pub use layout::EeWorldLayout;
pub use tech_view::{NodeView, TechTreeView};
pub use view::{ClassView, MapView, PlayerView, Record, StatsView, TileView, UnitView, WorldRead, WorldView};

/// The C++ registers the build's layout (`EEWorldLayoutRegister`, once at start-up). False for a layout of another
/// size (a stale header) or a second, different one.
///
/// # Safety
/// `layout` points to a valid layout.
#[no_mangle]
pub unsafe extern "C" fn ee_world_set_layout(layout: *const EeWorldLayout) -> bool {
    // SAFETY: the caller's contract
    layout::set(unsafe { *layout })
}

/// A C++ read of the world through Rust views begins on this thread (counted per thread in debug builds,
/// `view::read_depth`).
#[no_mangle]
pub extern "C" fn ee_world_read_begin() {
    std::mem::forget(
        // SAFETY: the C++ caller holds the world until its `ee_world_read_end` (the contract of the pair)
        unsafe { WorldRead::begin() },
    );
}

/// The read `ee_world_read_begin` opened ends.
#[no_mangle]
pub extern "C" fn ee_world_read_end() {
    WorldRead::end_forgotten();
}
