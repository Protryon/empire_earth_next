//! The interface's tech tree calls Rust answers itself (player/tech lane, round 4): the calls of
//! [`Cpp`](crate::screens::game_cpp::Cpp) marked `= tech_direct::name` in game_cpp.rs. Each was a one-statement C++
//! shim (EEUIShell.cpp, EEUIFMainGame.cpp, EEUIFMGMainGroup.cpp, EEUIBUpgrade.cpp, EEUICPortrait.cpp) reading a tree
//! or a node, which are Rust records (ee_sim tech tree_objects.rs, tech_tree objects.rs).

#![allow(clippy::unnecessary_cast)] // c_long is i32 on Windows, i64 on LP64

use core::ffi::c_long;

use ee_sim::tech::nodes::EeTechTreeFields;

use crate::tech_tree::objects::{class_at, get, global_tree, node_data, player_tree};

/// `gWorld.GetCurrentPlayer()` (0: none).
fn current_player() -> usize {
    // SAFETY: the interface's calls come from the game's thread inside a C++ host, which holds the world
    let t = unsafe { ee_sim::world::WorldRead::begin() };
    ee_sim::world::WorldView::global(&t).and_then(|w| w.current_player()).map_or(0, |p| p.handle() as usize)
}

/// `gWorld.mPlayers[index]` (0: an empty slot).
fn player_at(index: i64) -> usize {
    // SAFETY: as above
    let t = unsafe { ee_sim::world::WorldRead::begin() };
    usize::try_from(index)
        .ok()
        .and_then(|i| ee_sim::world::WorldView::global(&t).and_then(|w| w.player(i)))
        .map_or(0, |p| p.handle() as usize)
}

/// The tree `tree`'s record.
fn tree<'a>(tree: usize) -> &'a EeTechTreeFields {
    // SAFETY: a live tree (the C++ statement dereferenced it)
    unsafe { &*(tree as *const EeTechTreeFields) }
}

/// The node `id` of `tree`'s data (`tree->Get(id)->mData`).
fn data<'a>(tree: usize, id: i64) -> &'a ee_sim::tech::nodes::EeTechNodeDataFields {
    // SAFETY: a live node's data (as above)
    unsafe { &*node_data(get(tree, id as c_long)) }
}

/// `gTechTree.Get(technology)->mData->mNameID`.
pub(crate) fn sh_tech_name_id(technology: i64) -> i64 {
    data(global_tree(), technology).mNameID as i64
}

/// `gUnitClassTable[gTechTree.Get(technology)->mData->mUnitClass]->mNameStringID`.
pub(crate) fn sh_tech_class_name_id(technology: i64) -> i64 {
    let class = class_at(data(global_tree(), technology).mUnitClass as i64);
    // SAFETY: the interface's thread holds the world (as above); a live class (the C++ statement dereferenced it)
    let t = unsafe { ee_sim::world::WorldRead::begin() };
    unsafe { ee_sim::world::ClassView::of(&t, class as u64) }
        .map_or(0, |c| ee_sim::world_field!(c.fields(), mNameStringID) as i64)
}

/// `gWorld.mPlayers[selection + 1]->GetTechTree()`.
pub(crate) fn sh_player_tree(selection: u32) -> usize {
    player_tree(player_at(selection as i64 + 1))
}

/// `tree->Get(technology)->mData->mID`.
pub(crate) fn sh_tree_data_id(t: usize, technology: i64) -> i64 {
    data(t, technology).mID as i64
}

/// `gWorld.GetCurrentPlayer()->mTechnologies != NULL`.
pub(crate) fn mf_has_techs() -> bool {
    player_tree(current_player()) != 0
}

/// `gWorld.GetCurrentPlayer()->mTechnologies->mMediaID`.
pub(crate) fn mf_player_media_id() -> i64 {
    tree(player_tree(current_player())).mMediaID as i64
}

/// `player->GetTechTree()->Get(technology)`.
pub(crate) fn mg_player_node(player: usize, technology: i64) -> usize {
    get(player_tree(player), technology as c_long)
}

/// `gWorld.GetCurrentPlayer()->GetTechTree()->Get(technology)` (the upgrade button's).
pub(crate) fn up_node(technology: i64) -> usize {
    get(player_tree(current_player()), technology as c_long)
}

/// `gWorld.GetCurrentPlayer()->GetTechTree()->Get(id)` (the portrait's).
pub(crate) fn pt_node(id: i64) -> usize {
    get(player_tree(current_player()), id as c_long)
}

/// `gWorld.GetCurrentPlayer()->GetTechTree()->mStartEpoch`.
pub(crate) fn group_start_epoch() -> i32 {
    tree(player_tree(current_player())).mStartEpoch as i32
}

/// `gWorld.GetCurrentPlayer()->GetTechTree()->mEndEpoch`.
pub(crate) fn group_end_epoch() -> i32 {
    tree(player_tree(current_player())).mEndEpoch as i32
}

/// `gWorld.GetCurrentPlayer()->mUnitClassUpgrades[kind]->mButton != NULL` (player_upgrades.rs, the stats view).
pub(crate) fn player_class_has_button(kind: u32) -> bool {
    let upgrade = ee_sim::units::player_upgrades::at(current_player() as u64, kind as usize);
    // SAFETY: as above; the player's live upgrade (the C++ statement dereferenced it)
    let t = unsafe { ee_sim::world::WorldRead::begin() };
    unsafe { ee_sim::world::StatsView::of(&t, upgrade) }.is_some_and(|s| s.button_handle() != 0)
}
