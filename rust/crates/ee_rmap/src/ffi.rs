//! The C ABI of the generator: the adapter (`Default.cpp`, the `rmap` PIM entry points) copies `EERandomMap` into flat
//! arrays (`EeRmapState`), runs one step, and copies the flat arrays of the result back. The output's pointers live in
//! the returned handle until `ee_rmap_free`.

use crate::generator::{run_step, Step};
use crate::map::{
    Circle, Feature, Grid, Map, Object, Player, ResourceKind, ResourceSlot, Settings, SpacingRule, Team, URandomState,
};
use crate::math::Point;
use ee_random::URandom;
use std::ptr;

/// One player; `tiles_start`/`centers_start` index `EeRmapState::player_points`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeRmapPlayer {
    pub position: Point,
    pub radius: f32,
    pub index: u32,
    pub team: i32,
    pub tile_count: i32,
    pub is_neutral: u8,
    pub is_outer: u8,
    pub is_moon: u8,
    pub is_placed: u8,
    pub tiles_start: u32,
    pub tiles_len: u32,
    pub centers_start: u32,
    pub centers_len: u32,
}

/// One team; `start` indexes `team_players` and `team_positions`, which run parallel.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct EeRmapTeam {
    pub start: u32,
    pub len: u32,
}

/// The generator's state as flat arrays. Grids are `grid_n * grid_n` and indexed `[x][y]` by `x * grid_n + y`. A
/// pointer may be null when its length is 0.
#[repr(C)]
pub struct EeRmapState {
    pub settings: Settings,
    pub grid_n: u32,
    pub flat_tiles: *const u8,
    pub heights: *const f32,
    pub tiles: *const u32,
    pub claimed_tiles: *const u8,
    pub valid_tiles: *const u8,
    pub players: *const EeRmapPlayer,
    pub players_len: u32,
    pub player_points: *const Point,
    pub player_points_len: u32,
    pub teams: *const EeRmapTeam,
    pub teams_len: u32,
    pub team_players: *const i32,
    pub team_positions: *const Point,
    pub team_members_len: u32,
    pub objects: *const Object,
    pub objects_len: u32,
    pub water_points: *const Point,
    pub water_points_len: u32,
    pub features: *const Feature,
    pub features_len: u32,
    pub circle_counts: *const u32,
    pub circle_counts_len: u32,
    pub circles: *const Circle,
    pub circles_len: u32,
    pub tree_types: *const i32,
    pub tree_types_len: u32,
    pub resource_kinds: *const ResourceKind,
    pub resource_kinds_len: u32,
    pub resource_kind_ids: *const i32,
    pub resource_kind_ids_len: u32,
    pub resource_slots: *const ResourceSlot,
    pub resource_slots_len: u32,
    pub spacing_rules: *const SpacingRule,
    pub spacing_rules_len: u32,
    pub player_order: *const i32,
    pub player_order_len: u32,
}

/// The arrays an output `EeRmapState` points into.
pub struct EeRmapResult {
    map: Map,
    players: Vec<EeRmapPlayer>,
    player_points: Vec<Point>,
    teams: Vec<EeRmapTeam>,
    team_players: Vec<i32>,
    team_positions: Vec<Point>,
    circle_counts: Vec<u32>,
    circles: Vec<Circle>,
}

unsafe fn slice<'a, T>(data: *const T, len: u32) -> &'a [T] {
    if len == 0 || data.is_null() {
        &[]
    } else {
        // SAFETY: the caller of the C API passes valid arrays of `len` elements
        unsafe { std::slice::from_raw_parts(data, len as usize) }
    }
}

unsafe fn grid_from<T: Copy>(data: *const T, n: u32) -> Grid<T> {
    let len = n as usize * n as usize;
    if len == 0 || data.is_null() {
        Grid { size: n, data: Vec::new() }
    } else {
        // SAFETY: the caller of the C API passes `n * n` cells
        Grid { size: n, data: unsafe { std::slice::from_raw_parts(data, len) }.to_vec() }
    }
}

unsafe fn read_map(state: &EeRmapState) -> Map {
    // SAFETY (the whole function): the caller of the C API passes valid arrays of the stated lengths
    unsafe {
        let n = state.grid_n;
        let points = slice(state.player_points, state.player_points_len);
        let players = slice(state.players, state.players_len)
            .iter()
            .map(|p| Player {
                tiles: points[p.tiles_start as usize..(p.tiles_start + p.tiles_len) as usize].to_vec(),
                clump_centers: points[p.centers_start as usize..(p.centers_start + p.centers_len) as usize].to_vec(),
                position: p.position,
                radius: p.radius,
                index: p.index,
                team: p.team,
                tile_count: p.tile_count,
                is_neutral: p.is_neutral != 0,
                is_outer: p.is_outer != 0,
                is_moon: p.is_moon != 0,
                is_placed: p.is_placed != 0,
            })
            .collect();
        let team_players = slice(state.team_players, state.team_members_len);
        let team_positions = slice(state.team_positions, state.team_members_len);
        let teams = slice(state.teams, state.teams_len)
            .iter()
            .map(|t| Team {
                players: team_players[t.start as usize..(t.start + t.len) as usize].to_vec(),
                positions: team_positions[t.start as usize..(t.start + t.len) as usize].to_vec(),
            })
            .collect();
        let circles = slice(state.circles, state.circles_len);
        let mut start = 0usize;
        let clump_circles = slice(state.circle_counts, state.circle_counts_len)
            .iter()
            .map(|&count| {
                let list = circles[start..start + count as usize].to_vec();
                start += count as usize;
                list
            })
            .collect();
        Map {
            s: state.settings,
            random: URandomState(URandom { seed: state.settings.random_seed }),
            random_integers: URandomState(URandom { seed: state.settings.random_integers_seed }),
            flat_tiles: grid_from(state.flat_tiles, n),
            heights: grid_from(state.heights, n),
            tiles: grid_from(state.tiles, n),
            claimed_tiles: grid_from(state.claimed_tiles, n),
            valid_tiles: grid_from(state.valid_tiles, n),
            players,
            teams,
            objects: slice(state.objects, state.objects_len).to_vec(),
            water_points: slice(state.water_points, state.water_points_len).to_vec(),
            features: slice(state.features, state.features_len).to_vec(),
            clump_circles,
            tree_types: slice(state.tree_types, state.tree_types_len).to_vec(),
            resource_kinds: slice(state.resource_kinds, state.resource_kinds_len).to_vec(),
            resource_kind_ids: slice(state.resource_kind_ids, state.resource_kind_ids_len).to_vec(),
            resource_slots: slice(state.resource_slots, state.resource_slots_len).to_vec(),
            spacing_rules: slice(state.spacing_rules, state.spacing_rules_len).to_vec(),
            player_order: slice(state.player_order, state.player_order_len).to_vec(),
        }
    }
}

fn flatten(map: Map) -> Box<EeRmapResult> {
    let mut result = EeRmapResult {
        players: Vec::with_capacity(map.players.len()),
        player_points: Vec::new(),
        teams: Vec::with_capacity(map.teams.len()),
        team_players: Vec::new(),
        team_positions: Vec::new(),
        circle_counts: Vec::with_capacity(map.clump_circles.len()),
        circles: Vec::new(),
        map,
    };
    for p in &result.map.players {
        let tiles_start = result.player_points.len() as u32;
        result.player_points.extend_from_slice(&p.tiles);
        let centers_start = result.player_points.len() as u32;
        result.player_points.extend_from_slice(&p.clump_centers);
        result.players.push(EeRmapPlayer {
            position: p.position,
            radius: p.radius,
            index: p.index,
            team: p.team,
            tile_count: p.tile_count,
            is_neutral: u8::from(p.is_neutral),
            is_outer: u8::from(p.is_outer),
            is_moon: u8::from(p.is_moon),
            is_placed: u8::from(p.is_placed),
            tiles_start,
            tiles_len: p.tiles.len() as u32,
            centers_start,
            centers_len: p.clump_centers.len() as u32,
        });
    }
    for t in &result.map.teams {
        result.teams.push(EeRmapTeam { start: result.team_players.len() as u32, len: t.players.len() as u32 });
        result.team_players.extend_from_slice(&t.players);
        result.team_positions.extend_from_slice(&t.positions);
    }
    for list in &result.map.clump_circles {
        result.circle_counts.push(list.len() as u32);
        result.circles.extend_from_slice(list);
    }
    Box::new(result)
}

fn write_state(result: &EeRmapResult) -> EeRmapState {
    let m = &result.map;
    let mut settings = m.s;
    settings.random_seed = m.random.0.seed;
    settings.random_integers_seed = m.random_integers.0.seed;
    EeRmapState {
        settings,
        grid_n: m.tiles.size,
        flat_tiles: m.flat_tiles.data.as_ptr(),
        heights: m.heights.data.as_ptr(),
        tiles: m.tiles.data.as_ptr(),
        claimed_tiles: m.claimed_tiles.data.as_ptr(),
        valid_tiles: m.valid_tiles.data.as_ptr(),
        players: result.players.as_ptr(),
        players_len: result.players.len() as u32,
        player_points: result.player_points.as_ptr(),
        player_points_len: result.player_points.len() as u32,
        teams: result.teams.as_ptr(),
        teams_len: result.teams.len() as u32,
        team_players: result.team_players.as_ptr(),
        team_positions: result.team_positions.as_ptr(),
        team_members_len: result.team_players.len() as u32,
        objects: m.objects.as_ptr(),
        objects_len: m.objects.len() as u32,
        water_points: m.water_points.as_ptr(),
        water_points_len: m.water_points.len() as u32,
        features: m.features.as_ptr(),
        features_len: m.features.len() as u32,
        circle_counts: result.circle_counts.as_ptr(),
        circle_counts_len: result.circle_counts.len() as u32,
        circles: result.circles.as_ptr(),
        circles_len: result.circles.len() as u32,
        tree_types: m.tree_types.as_ptr(),
        tree_types_len: m.tree_types.len() as u32,
        resource_kinds: m.resource_kinds.as_ptr(),
        resource_kinds_len: m.resource_kinds.len() as u32,
        resource_kind_ids: m.resource_kind_ids.as_ptr(),
        resource_kind_ids_len: m.resource_kind_ids.len() as u32,
        resource_slots: m.resource_slots.as_ptr(),
        resource_slots_len: m.resource_slots.len() as u32,
        spacing_rules: m.spacing_rules.as_ptr(),
        spacing_rules_len: m.spacing_rules.len() as u32,
        player_order: m.player_order.as_ptr(),
        player_order_len: m.player_order.len() as u32,
    }
}

fn step_from(step: i32) -> Option<Step> {
    Some(match step {
        0 => Step::PlayerPositions,
        1 => Step::NeutralPositions,
        2 => Step::PlayerLandWater,
        3 => Step::FlatMap,
        4 => Step::HeightMap,
        5 => Step::FilterHeightMap,
        6 => Step::Shallows,
        7 => Step::Resources,
        8 => Step::Trees,
        _ => return None,
    })
}

/// Runs generator step `step` (0 player positions, 1 neutral positions, 2 land and water, 3 flat map, 4 height map,
/// 5 filter, 6 shallows, 7 resources, 8 trees) on `input`, fills `output` and returns the handle that owns `output`'s
/// arrays; null for an unknown step.
///
/// # Safety
/// `input` must point to a state whose pointers hold the stated lengths; `output` must be writable. Free the handle
/// with `ee_rmap_free`.
#[no_mangle]
pub unsafe extern "C" fn ee_rmap_run(
    step: i32,
    input: *const EeRmapState,
    output: *mut EeRmapState,
) -> *mut EeRmapResult {
    let Some(step) = step_from(step) else { return ptr::null_mut() };
    // SAFETY: the caller passes a valid input state
    let mut map = unsafe { read_map(&*input) };
    run_step(&mut map, step);
    let result = flatten(map);
    let state = write_state(&result);
    // SAFETY: the caller passes a writable output state
    unsafe { ptr::write(output, state) };
    Box::into_raw(result)
}

/// Frees a result from `ee_rmap_run` (null: nothing).
///
/// # Safety
/// `result` must be null or a handle from `ee_rmap_run`, freed once.
#[no_mangle]
pub unsafe extern "C" fn ee_rmap_free(result: *mut EeRmapResult) {
    if !result.is_null() {
        // SAFETY: the handle came from Box::into_raw in `ee_rmap_run`
        drop(unsafe { Box::from_raw(result) });
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::map::Ring;

    fn empty_state() -> EeRmapState {
        // a null-pointered state of a 0-sized grid
        // SAFETY: EeRmapState is plain data: zero is a valid value of every field
        unsafe { std::mem::zeroed() }
    }

    #[test]
    fn an_unknown_step_is_null() {
        let input = empty_state();
        let mut output = empty_state();
        // SAFETY: the states are valid
        assert!(unsafe { ee_rmap_run(99, &input, &mut output) }.is_null());
    }

    #[test]
    fn player_positions_round_trip_through_the_flat_state() {
        let mut input = empty_state();
        let n = 33u32;
        let valid = vec![1u8; (n * n) as usize];
        let zeros = vec![0u8; (n * n) as usize];
        let tiles = vec![0u32; (n * n) as usize];
        let heights = vec![0.5f32; (n * n) as usize];
        input.grid_n = n;
        input.flat_tiles = zeros.as_ptr();
        input.claimed_tiles = zeros.as_ptr();
        input.valid_tiles = valid.as_ptr();
        input.tiles = tiles.as_ptr();
        input.heights = heights.as_ptr();
        input.settings.grid_size = 32;
        input.settings.maximum_x = 32;
        input.settings.maximum_y = 32;
        input.settings.max_loop_count = 200;
        input.settings.convergent_sample_size = 2.0;
        input.settings.random_seed = 5;
        input.settings.random_integers_seed = 6;
        input.settings.ring = Ring { center: Point::new(16.0, 16.0), min_radius: 8.0, max_radius: 8.0 };
        let team_players = [1i32, 2];
        let team_positions = [Point::default(), Point::default()];
        let teams = [EeRmapTeam { start: 0, len: 1 }, EeRmapTeam { start: 1, len: 1 }];
        input.teams = teams.as_ptr();
        input.teams_len = 2;
        input.team_players = team_players.as_ptr();
        input.team_positions = team_positions.as_ptr();
        input.team_members_len = 2;
        let mut output = empty_state();
        // SAFETY: every pointer above is valid for its length, and the handle is freed below
        let handle = unsafe { ee_rmap_run(0, &input, &mut output) };
        assert!(!handle.is_null());
        assert_eq!(output.players_len, 2);
        assert_eq!(output.settings.num_team_players, 2);
        assert_ne!(output.settings.random_seed, 5); // the stream advanced
        assert_eq!(output.team_members_len, 2);
        // SAFETY: output's arrays live until the free
        let players = unsafe { std::slice::from_raw_parts(output.players, 2) };
        assert_eq!(players[1].index, 2);
        // SAFETY: the handle came from ee_rmap_run
        unsafe { ee_rmap_free(handle) };
    }

    #[test]
    fn a_player_keeps_its_tiles_and_centers_apart() {
        let points = [Point::new(1.0, 1.0), Point::new(2.0, 2.0), Point::new(3.0, 3.0)];
        let players = [EeRmapPlayer {
            tiles_start: 0,
            tiles_len: 2,
            centers_start: 2,
            centers_len: 1,
            is_moon: 1,
            ..EeRmapPlayer::default()
        }];
        let mut state = empty_state();
        state.players = players.as_ptr();
        state.players_len = 1;
        state.player_points = points.as_ptr();
        state.player_points_len = 3;
        // SAFETY: the arrays are valid for the lengths set
        let map = unsafe { read_map(&state) };
        assert_eq!(map.players[0].tiles, vec![points[0], points[1]]);
        assert_eq!(map.players[0].clump_centers, vec![points[2]]);
        assert!(map.players[0].is_moon);

        let result = flatten(map);
        assert_eq!(result.players[0].tiles_len, 2);
        assert_eq!(result.players[0].centers_start, 2);
        assert_eq!(result.player_points.len(), 3);
    }

    #[test]
    fn freeing_null_is_a_no_op() {
        // SAFETY: null is allowed
        unsafe { ee_rmap_free(std::ptr::null_mut()) };
    }
}
