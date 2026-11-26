//! The gates' and the buildings' remaining bodies over their C++ statements (the units' lane, round 8; were
//! EEUnitClassBuildings.cpp): `EEBuildGate`, `GateGateReplaced`, `EEDoesWallGateFit` (its UCBD note stays C++),
//! `EEHighlightGateWalls`, `EEClearGateHighlights`, `EEFindGateWall`, `FindNearestTargetBuilding` (its note stays
//! C++), `GateGetSurroundingTiles`, `GateUnitCreated`, `SpaceBuildingHasArrived`, `SpaceBuildingUnitCreated`. Each
//! C++ statement is one `EE_UNITS_UCB_*` case of `ee_units_ucb_host` (the class host's statements pass through to
//! EEUnitClass.cpp's); the functions' vectors are the host's (`EE_UNITS_UCB_UNITS_VEC`, `EE_UNITS_UCB_TILES_VEC`: one
//! per local, so a nested call never shares one), cleared where the original declared a fresh local.

// `c_long` is 32 bits on Windows and 64 elsewhere: the casts are needed there
#![allow(clippy::unnecessary_cast)]

use crate::units::uc_body::*;
use crate::units::uc_create::*;
use ee_base::math::types::GeTransformation;
use ee_base::math::um::floor;
use std::os::raw::c_long;

// ---- the statements (1100..1199)
/// `&sUcbUnits[b]` (a `std::vector<EEUnit*>`)
pub const EE_UNITS_UCB_UNITS_VEC: i32 = 1100;
/// `&sUcbTiles[b]` (a `std::vector<U2DPoint<long> >`)
pub const EE_UNITS_UCB_TILES_VEC: i32 = 1101;
/// unit vector `a`'s `size()`
pub const EE_UNITS_UCB_UNITS_SIZE: i32 = 1102;
/// unit vector `a`'s `[b]`
pub const EE_UNITS_UCB_UNITS_AT: i32 = 1103;
/// unit vector `a`'s `clear()`
pub const EE_UNITS_UCB_UNITS_CLEAR: i32 = 1104;
/// tile vector `a`'s `clear()`
pub const EE_UNITS_UCB_GATE_TILES_CLEAR: i32 = 1106;
/// tile vector `a`'s `size()`
pub const EE_UNITS_UCB_TILES_SIZE: i32 = 1107;
/// tile vector `a`'s `[b].mX`
pub const EE_UNITS_UCB_TILE_X: i32 = 1108;
/// tile vector `a`'s `[b].mY`
pub const EE_UNITS_UCB_TILE_Y: i32 = 1109;
/// `EEGetWallGateTiles(U2DPoint<long>(b's low, high 32 bits), c, tile vector a)`
pub const EE_UNITS_UCB_WALL_GATE_TILES: i32 = 1110;
/// `ee_units_uu_units_on(0, tile vector a, unit vector b)`
pub const EE_UNITS_UCB_UNITS_ON: i32 = 1111;
/// unit `a`'s `GetUnitClass()`
pub const EE_UNITS_UCB_UNIT_CLASS_OF: i32 = 1112;
/// class `a`'s `mIsWall`
pub const EE_UNITS_UCB_CLASS_IS_WALL: i32 = 1113;
/// class `a`'s `GetUnitType()`
pub const EE_UNITS_UCB_CLASS_TYPE: i32 = 1114;
/// `EEDoesWallGateFit(EETileCoordinate(b's low, high 32 bits), c's low 32 bits, c's high 32 bits, unit vector a)`
pub const EE_UNITS_UCB_WALL_GATE_FIT: i32 = 1115;
/// `::gWorld.mPlayers[b]`
pub const EE_UNITS_UCB_PLAYER_AT: i32 = 1116;
/// `gUnitClassTable[player a->mTechnologies->Get(b)->mData->mUnitClass]`
pub const EE_UNITS_UCB_TECH_CLASS: i32 = 1117;
/// `f[0] = (float)` unit `a`'s `mHitPoints / mStats->mMaxHitPoints`
pub const EE_UNITS_UCB_HIT_RATIO: i32 = 1118;
/// player `a`'s `GetPlayerID()`
pub const EE_UNITS_UCB_PLAYER_ID: i32 = 1119;
/// `EEReplaceUnit(complex unit a, b, class c)`
pub const EE_UNITS_UCB_REPLACE_UNIT: i32 = 1120;
/// `EETurnGatePart(the transformation at a, b, c)`
pub const EE_UNITS_UCB_TURN_GATE_PART: i32 = 1121;
/// complex unit `a`'s `SetTransformation(the transformation at b, true)`
pub const EE_UNITS_UCB_SET_TRANSFORMATION_OF: i32 = 1122;
/// complex unit `a`'s `SetGateDirection(b)`
pub const EE_UNITS_UCB_SET_GATE_DIRECTION_OF: i32 = 1123;
/// unit `a`'s `GetComplexUnit()->GetPosition(f[0..3])`
pub const EE_UNITS_UCB_POSITION_OF: i32 = 1124;
/// `f[0] = ::gWorld.GetMap()->mTerrainMesh.GetMeshZ(f[0], f[1])`
pub const EE_UNITS_UCB_MESH_Z: i32 = 1125;
/// unit `a`'s `GetComplexUnit()->SetLocation(f[0..3], true)`
pub const EE_UNITS_UCB_SET_LOCATION_OF: i32 = 1126;
/// unit `a`'s `GetComplexUnit()->LinkPart(unit b's GetComplexUnit())`
pub const EE_UNITS_UCB_LINK_PART: i32 = 1127;
/// unit `a`'s `GetComplexUnit()->StartAction(b)`
pub const EE_UNITS_UCB_START_ACTION_OF: i32 = 1128;
/// unit `a`'s `SetHitPoints(UMFloatToLong(a->GetStats()->mMaxHitPoints * f[0]))`
pub const EE_UNITS_UCB_SET_HEALTH: i32 = 1129;
/// `gUIManager->mInScenarioEditor && !gGameScreenActive`
pub const EE_UNITS_UCB_IN_EDITOR: i32 = 1130;
/// `::gWorld.FlushQueuedPlacements()`
pub const EE_UNITS_UCB_FLUSH_PLACEMENTS: i32 = 1131;
/// player `a`'s `AddPendingUnits(0, true)`
pub const EE_UNITS_UCB_ADD_PENDING: i32 = 1132;
/// player `a`'s `RemoveKilledUnits()`
pub const EE_UNITS_UCB_REMOVE_KILLED: i32 = 1133;
/// player `a`'s `mTechnologies->Get(kTechGateSpace)->GetIsAvailable() && class b->GetUnitType() != kUnitTypeGatePalisades`
pub const EE_UNITS_UCB_SPACE_GATES: i32 = 1134;
/// `EECreateSpaceGateEnergyWall(EEUnitCast<EEComplexUnit>(unit a))`
pub const EE_UNITS_UCB_ENERGY_WALL: i32 = 1135;
/// unit `a`'s `GetComplexUnit()->GetCommandType()`
pub const EE_UNITS_UCB_COMMAND_TYPE_OF: i32 = 1136;
/// unit `a`'s `GetComplexUnit()->mGateDirection`
pub const EE_UNITS_UCB_GATE_DIRECTION_OF: i32 = 1137;
/// `class->GetPart(complex unit a, b)`
pub const EE_UNITS_UCB_GET_PART: i32 = 1138;
/// unit `a`'s `GetPlayer()->GetPlayerID()`
pub const EE_UNITS_UCB_UNIT_PLAYER_ID: i32 = 1139;
/// unit `a`'s `GetComplexUnit()`
pub const EE_UNITS_UCB_COMPLEX_OF: i32 = 1140;
/// the transformation at `b` = complex unit `a`'s `mPhysicalModel.GetLocation()`
pub const EE_UNITS_UCB_LOCATION_MATRIX_OF: i32 = 1141;
/// complex unit `a`'s `SetAnimation(b, -1, 0)`
pub const EE_UNITS_UCB_SET_ANIMATION_OF: i32 = 1142;
/// unit `a`'s `mComplexUnit->mIsBuilt && a->mHitPoints > 0`
pub const EE_UNITS_UCB_BUILT_ALIVE: i32 = 1143;
/// `EEAreTilesLevel(tile vector a)`
pub const EE_UNITS_UCB_TILES_LEVEL: i32 = 1144;
/// `ee_units_uu_units_of_type_on_tiles(tile vector a, b, unit vector c)`
pub const EE_UNITS_UCB_UNITS_OF_TYPE_ON_TILES: i32 = 1145;
/// `ee_units_uu_player_units_of_type(b, c, unit vector a)`
pub const EE_UNITS_UCB_PLAYER_UNITS_OF_TYPE: i32 = 1146;
/// unit `a`'s `mComplexUnit`
pub const EE_UNITS_UCB_COMPLEX_FIELD: i32 = 1147;
/// complex unit `a`'s `SetColor(b != 0 ? the highlight : the normal colour)`
pub const EE_UNITS_UCB_SET_COLOR: i32 = 1148;
/// complex unit `a`'s `SetIsTinted(b != 0)`
pub const EE_UNITS_UCB_SET_TINTED: i32 = 1149;
/// complex unit `a`'s `mIsTinted`
pub const EE_UNITS_UCB_IS_TINTED: i32 = 1150;
/// `EEDoesWallGateFitOnUnit(complex unit a, b's low 32 bits, b's high 32 bits, unit vector c)`
pub const EE_UNITS_UCB_FIT_ON_UNIT: i32 = 1151;
/// player `a`'s `mPlayerID`
pub const EE_UNITS_UCB_PLAYER_ID_FIELD: i32 = 1152;
/// `::gWorld.GetMap()->GetTile(b, c)`
pub const EE_UNITS_UCB_MAP_TILE: i32 = 1153;
/// tile `a`'s `mUnits`
pub const EE_UNITS_UCB_TILE_NODES: i32 = 1154;
/// node `a`'s `mUnit`
pub const EE_UNITS_UCB_GATE_NODE_UNIT: i32 = 1155;
/// node `a`'s `mNext`
pub const EE_UNITS_UCB_GATE_NODE_NEXT: i32 = 1156;
/// unit `a`'s `mUnitClass->mIsWall && mHitPoints > 0 && mTileNode != NULL && GetComplexUnit()->mIsTinted`
pub const EE_UNITS_UCB_TINTED_WALL: i32 = 1157;
/// `ee_units_uu_square_ring_tiles(b's low, high 32 bits, c, tile vector a)`
pub const EE_UNITS_UCB_RING: i32 = 1158;
/// unit `a`'s `GetTileX()`
pub const EE_UNITS_UCB_UNIT_TILE_X: i32 = 1159;
/// unit `a`'s `GetTileY()`
pub const EE_UNITS_UCB_UNIT_TILE_Y: i32 = 1160;
/// unit `a`'s `mUnitClass->IsLandBuilding() && mHitPoints > 0 && ee_units_uu_targetable(0, its class, 0)`
pub const EE_UNITS_UCB_TARGET_BUILDING: i32 = 1161;
/// complex unit `a`'s `mPreviousPart`
pub const EE_UNITS_UCB_PREVIOUS_PART: i32 = 1162;
/// complex unit `a`'s `mNextPart`
pub const EE_UNITS_UCB_NEXT_PART_OF: i32 = 1163;
/// `class->IsPartLeftOut(complex unit a)`
pub const EE_UNITS_UCB_PART_LEFT_OUT: i32 = 1164;
/// `class->BaseGetSurroundingTiles(complex unit a, tile vector b, c, true)`
pub const EE_UNITS_UCB_GATE_BASE_SURROUNDING: i32 = 1165;
/// `EESortUnique(tile vector a, less, equal_to)`
pub const EE_UNITS_UCB_SORT_UNIQUE: i32 = 1166;
/// `unit->mCollisionState`
pub const EE_UNITS_UCB_COLLISION_STATE: i32 = 1167;
/// `unit->GetNextWaypointPoint(f[0..3])`
pub const EE_UNITS_UCB_NEXT_WAYPOINT: i32 = 1168;
/// `unit->GetTileX()`
pub const EE_UNITS_UCB_SELF_TILE_X: i32 = 1169;
/// `unit->GetTileY()`
pub const EE_UNITS_UCB_SELF_TILE_Y: i32 = 1170;
/// `class->BuildingUnitCreated(unit)`
pub const EE_UNITS_UCB_BUILDING_UNIT_CREATED: i32 = 1171;
/// `unit->mPreviousPart == NULL && unit->mNextPart != NULL`
pub const EE_UNITS_UCB_CHAIN_HEAD: i32 = 1172;
/// `unit->StartAction(b)`
pub const EE_UNITS_UCB_START_ACTION: i32 = 1173;
/// `class->ActionChanged(unit)`
pub const EE_UNITS_UCB_ACTION_CHANGED: i32 = 1174;
/// `unit->SetBuilt(1, 0, 1)`
pub const EE_UNITS_UCB_SPACE_SET_BUILT: i32 = 1175;

// ---- the bodies (`ee_units_ucb_gate`'s `what`)
pub const EE_UNITS_UCB_DO_BUILD_GATE: i32 = 1;
pub const EE_UNITS_UCB_DO_GATE_REPLACED: i32 = 2;
pub const EE_UNITS_UCB_DO_WALL_GATE_FIT: i32 = 3;
pub const EE_UNITS_UCB_DO_HIGHLIGHT_GATE_WALLS: i32 = 4;
pub const EE_UNITS_UCB_DO_CLEAR_GATE_HIGHLIGHTS: i32 = 5;
pub const EE_UNITS_UCB_DO_FIND_GATE_WALL: i32 = 6;
pub const EE_UNITS_UCB_DO_NEAREST_TARGET_BUILDING: i32 = 7;
pub const EE_UNITS_UCB_DO_GATE_SURROUNDING: i32 = 8;
pub const EE_UNITS_UCB_DO_GATE_UNIT_CREATED: i32 = 9;
pub const EE_UNITS_UCB_DO_SPACE_ARRIVED: i32 = 10;
pub const EE_UNITS_UCB_DO_SPACE_UNIT_CREATED: i32 = 11;

const ACTION_IDLE: i64 = 5000;
const ACTION_CLOSED: i64 = 15001;
/// a slow request's speed (`SpaceBuildingHasArrived`)
const ARRIVED_SPEED: f32 = 0.05;

// the host's vectors, one per local
const U_BUILD_UNITS: i64 = 0;
const U_BUILD_SIDE: i64 = 1;
const U_HIGHLIGHT_UNITS: i64 = 2;
const U_HIGHLIGHT_SIDES: i64 = 3; // ..=6
const U_CLEAR_UNITS: i64 = 7;
const T_BUILD_TILES: i64 = 0;
const T_FIT_TILES: i64 = 1;
const T_RING_TILES: i64 = 2;

/// Two longs as one statement's operand (low, high 32 bits)
fn pair(x: i64, y: i64) -> i64 {
    (x as u32 as i64) | ((y as u32 as i64) << 32)
}

fn units<H: UcHost + ?Sized>(s: &mut S<'_, H>, which: i64) -> u64 {
    s.b(EE_UNITS_UCB_UNITS_VEC, which) as u64
}

fn tiles<H: UcHost + ?Sized>(s: &mut S<'_, H>, which: i64) -> u64 {
    s.b(EE_UNITS_UCB_TILES_VEC, which) as u64
}

/// A unit vector's units
fn list<H: UcHost + ?Sized>(s: &mut S<'_, H>, v: u64) -> Vec<u64> {
    let n = s.a(EE_UNITS_UCB_UNITS_SIZE, v) as usize;
    (0..n).map(|i| s.ab(EE_UNITS_UCB_UNITS_AT, v, i as i64) as u64).collect()
}

/// Replaces a piece: the new owner and class, turned for the gate (its action), the gate direction kept.
fn replace_piece<H: UcHost + ?Sized>(
    s: &mut S<'_, H>,
    old: u64,
    player: i64,
    class: u64,
    direction: i64,
    index: i64,
) -> u64 {
    let piece = s.abc(EE_UNITS_UCB_REPLACE_UNIT, old, player, class as i64) as u64;
    let mut t = GeTransformation::IDENTITY;
    s.ab(EE_UNITS_UCB_LOCATION_MATRIX_OF, piece, addr(&mut t.m) as i64);
    let action = s.abc(EE_UNITS_UCB_TURN_GATE_PART, addr(&mut t.m), direction, index);
    s.ab(EE_UNITS_UCB_SET_TRANSFORMATION_OF, piece, addr(&mut t.m) as i64);
    s.ab(EE_UNITS_UCB_SET_ANIMATION_OF, piece, action);
    s.ab(EE_UNITS_UCB_SET_GATE_DIRECTION_OF, piece, direction);
    piece
}

/// The five pieces linked post 4, post 0, door 3, door 1, gate 2
fn link<H: UcHost + ?Sized>(s: &mut S<'_, H>, p: &[u64]) {
    s.ab(EE_UNITS_UCB_LINK_PART, p[4], p[0] as i64);
    s.ab(EE_UNITS_UCB_LINK_PART, p[0], p[3] as i64);
    s.ab(EE_UNITS_UCB_LINK_PART, p[3], p[1] as i64);
    s.ab(EE_UNITS_UCB_LINK_PART, p[1], p[2] as i64);
}

/// `EEBuildGate(tile, player, technology, direction)`: the five wall pieces across a tile turned into a closed gate
/// of the technology's class for a player (the weakest wall's health, the posts on the lower ground); the energy
/// wall once space gates are available.
pub fn build_gate<H: UcHost + ?Sized>(
    h: &mut H,
    tile: [c_long; 2],
    player: c_long,
    technology: c_long,
    direction: c_long,
) {
    let mut s = S { h, class: 0, unit: 0 };
    let (tile_vec, unit_vec, side) =
        (tiles(&mut s, T_BUILD_TILES), units(&mut s, U_BUILD_UNITS), units(&mut s, U_BUILD_SIDE));
    s.a(EE_UNITS_UCB_GATE_TILES_CLEAR, tile_vec);
    s.a(EE_UNITS_UCB_UNITS_CLEAR, unit_vec);
    s.a(EE_UNITS_UCB_UNITS_CLEAR, side);
    let p = s.b(EE_UNITS_UCB_PLAYER_AT, i64::from(player)) as u64;
    let mut health = 1.0f32;
    let class = s.ab(EE_UNITS_UCB_TECH_CLASS, p, i64::from(technology)) as u64;
    let at = pair(i64::from(tile[0]), i64::from(tile[1]));
    s.abc(EE_UNITS_UCB_WALL_GATE_TILES, tile_vec, at, i64::from(direction));
    s.ab(EE_UNITS_UCB_UNITS_ON, tile_vec, unit_vec as i64);
    let mut walls = Vec::new();
    for u in list(&mut s, unit_vec) {
        let c = s.a(EE_UNITS_UCB_UNIT_CLASS_OF, u) as u64;
        if s.a(EE_UNITS_UCB_CLASS_IS_WALL, c) != 0 {
            walls.push(u);
        }
    }
    if walls.is_empty() {
        return;
    }
    let c = s.a(EE_UNITS_UCB_UNIT_CLASS_OF, walls[0]) as u64;
    let kind = s.a(EE_UNITS_UCB_CLASS_TYPE, c);
    if s.abc(EE_UNITS_UCB_WALL_GATE_FIT, side, at, pair(i64::from(direction), kind)) == 0 {
        return;
    }
    let mut gate = Vec::new();
    for (j, &wall) in walls.iter().take(5).enumerate() {
        let ratio = s.float(EE_UNITS_UCB_HIT_RATIO, wall, 0);
        if health >= ratio {
            health = ratio;
        }
        let id = s.a(EE_UNITS_UCB_PLAYER_ID, p);
        gate.push(replace_piece(&mut s, wall, id, class, i64::from(direction), j as i64));
    }
    let mut post4 = [0.0f32; 3];
    let mut post0 = [0.0f32; 3];
    s.point(EE_UNITS_UCB_POSITION_OF, gate[4], 0, 0, &mut post4);
    s.point(EE_UNITS_UCB_POSITION_OF, gate[0], 0, 0, &mut post0);
    let z4 = s.float_of(EE_UNITS_UCB_MESH_Z, [post4[0], post4[1], 0.0, 0.0]);
    let z0 = s.float_of(EE_UNITS_UCB_MESH_Z, [post0[0], post0[1], 0.0, 0.0]);
    // `min(a, b)`: the macro's `a < b ? a : b`
    let z = if z4 < z0 { z4 } else { z0 };
    post4[2] = z;
    post0[2] = z;
    s.point(EE_UNITS_UCB_SET_LOCATION_OF, gate[4], 0, 0, &mut post4);
    s.point(EE_UNITS_UCB_SET_LOCATION_OF, gate[0], 0, 0, &mut post0);
    link(&mut s, &gate);
    s.ab(EE_UNITS_UCB_START_ACTION_OF, gate[4], ACTION_IDLE);
    s.ab(EE_UNITS_UCB_START_ACTION_OF, gate[0], ACTION_IDLE);
    s.ab(EE_UNITS_UCB_START_ACTION_OF, gate[3], ACTION_CLOSED);
    s.ab(EE_UNITS_UCB_START_ACTION_OF, gate[1], ACTION_CLOSED);
    s.ab(EE_UNITS_UCB_START_ACTION_OF, gate[2], ACTION_CLOSED);
    s.with_float(EE_UNITS_UCB_SET_HEALTH, gate[2], health);
    if s.s(EE_UNITS_UCB_IN_EDITOR) != 0 {
        s.s(EE_UNITS_UCB_FLUSH_PLACEMENTS);
        s.a(EE_UNITS_UCB_ADD_PENDING, p);
        s.a(EE_UNITS_UCB_REMOVE_KILLED, p);
    }
    if s.ab(EE_UNITS_UCB_SPACE_GATES, p, class as i64) != 0 {
        s.a(EE_UNITS_UCB_ENERGY_WALL, gate[2]);
    }
}

/// `GateGateReplaced(gate, unit)`: the other four pieces of a gate whose gate piece was replaced (new owner and
/// class), turned and relinked; the posts idle, the doors on the new gate piece's action.
pub fn gate_replaced<H: UcHost + ?Sized>(h: &mut H, class: u64, gate: u64, unit: u64) {
    let mut s = S { h, class, unit: 0 };
    let new_class = s.a(EE_UNITS_UCB_UNIT_CLASS_OF, unit) as u64;
    let direction = s.a(EE_UNITS_UCB_GATE_DIRECTION_OF, unit);
    let mut pieces = [0u64; 5];
    pieces[2] = unit;
    for i in 0..=4 {
        if i == 2 {
            continue;
        }
        let complex = s.a(EE_UNITS_UCB_COMPLEX_OF, gate) as u64;
        let old = s.ab(EE_UNITS_UCB_GET_PART, complex, i) as u64;
        let id = s.a(EE_UNITS_UCB_UNIT_PLAYER_ID, unit);
        pieces[i as usize] = replace_piece(&mut s, old, id, new_class, direction, i);
    }
    link(&mut s, &pieces);
    s.ab(EE_UNITS_UCB_START_ACTION_OF, pieces[4], ACTION_IDLE);
    s.ab(EE_UNITS_UCB_START_ACTION_OF, pieces[0], ACTION_IDLE);
    let command = s.a(EE_UNITS_UCB_COMMAND_TYPE_OF, pieces[2]);
    s.ab(EE_UNITS_UCB_START_ACTION_OF, pieces[3], command);
    let command = s.a(EE_UNITS_UCB_COMMAND_TYPE_OF, pieces[2]);
    s.ab(EE_UNITS_UCB_START_ACTION_OF, pieces[1], command);
}

/// `EEDoesWallGateFit(tile, direction, unit type, out)`: five built, living walls of the type on level tiles across
/// the tile (else `out` emptied).
pub fn wall_gate_fit<H: UcHost + ?Sized>(h: &mut H, tile: [c_long; 2], direction: c_long, kind: i64, out: u64) -> bool {
    let mut s = S { h, class: 0, unit: 0 };
    let tile_vec = tiles(&mut s, T_FIT_TILES);
    s.a(EE_UNITS_UCB_GATE_TILES_CLEAR, tile_vec);
    let mut result = true;
    s.abc(EE_UNITS_UCB_WALL_GATE_TILES, tile_vec, pair(i64::from(tile[0]), i64::from(tile[1])), i64::from(direction));
    s.abc(EE_UNITS_UCB_UNITS_OF_TYPE_ON_TILES, tile_vec, kind, out as i64);
    for u in list(&mut s, out) {
        result = s.a(EE_UNITS_UCB_BUILT_ALIVE, u) != 0;
        if !result {
            break;
        }
    }
    if result && s.a(EE_UNITS_UCB_UNITS_SIZE, out) == 5 {
        s.a(EE_UNITS_UCB_TILES_LEVEL, tile_vec) != 0
    } else {
        s.a(EE_UNITS_UCB_UNITS_CLEAR, out);
        false
    }
}

/// `EEClearGateHighlights(player, unit type)`: the normal colour back on the player's walls of a type, their gate
/// direction cleared.
pub fn clear_gate_highlights<H: UcHost + ?Sized>(h: &mut H, player: u64, kind: i64) {
    let mut s = S { h, class: 0, unit: 0 };
    let v = units(&mut s, U_CLEAR_UNITS);
    s.a(EE_UNITS_UCB_UNITS_CLEAR, v);
    let id = s.a(EE_UNITS_UCB_PLAYER_ID_FIELD, player);
    s.abc(EE_UNITS_UCB_PLAYER_UNITS_OF_TYPE, v, kind, id);
    for u in list(&mut s, v) {
        let wall = s.a(EE_UNITS_UCB_COMPLEX_FIELD, u) as u64;
        if wall == 0 {
            continue;
        }
        s.ab(EE_UNITS_UCB_SET_COLOR, wall, 0);
        s.ab(EE_UNITS_UCB_SET_TINTED, wall, 0);
        s.ab(EE_UNITS_UCB_SET_GATE_DIRECTION_OF, wall, 0);
    }
}

/// `EEHighlightGateWalls(player, unit type)`: the middle wall of a gate line through each of the player's walls of a
/// type highlighted when one or three of the four directions fit (the first fitting direction's).
pub fn highlight_gate_walls<H: UcHost + ?Sized>(h: &mut H, player: u64, kind: i64) {
    clear_gate_highlights(&mut *h, player, kind);
    let mut s = S { h, class: 0, unit: 0 };
    let v = units(&mut s, U_HIGHLIGHT_UNITS);
    s.a(EE_UNITS_UCB_UNITS_CLEAR, v);
    let sides: Vec<u64> = (0..4).map(|d| units(&mut s, U_HIGHLIGHT_SIDES + d)).collect();
    for &side in &sides {
        s.a(EE_UNITS_UCB_UNITS_CLEAR, side);
    }
    let id = s.a(EE_UNITS_UCB_PLAYER_ID_FIELD, player);
    s.abc(EE_UNITS_UCB_PLAYER_UNITS_OF_TYPE, v, kind, id);
    for u in list(&mut s, v) {
        let mut fits = [false; 4];
        for d in 0..4 {
            fits[d] = s.abc(EE_UNITS_UCB_FIT_ON_UNIT, u, pair(d as i64 + 1, kind), sides[d] as i64) != 0;
        }
        if fits[0] ^ fits[1] ^ fits[2] ^ fits[3] {
            if let Some(d) = fits.iter().position(|&f| f) {
                let side = list(&mut s, sides[d]);
                let n = side.len();
                let middle = if n.is_multiple_of(2) { n / 2 - 1 } else { n / 2 };
                let wall = s.a(EE_UNITS_UCB_COMPLEX_FIELD, side[middle]) as u64;
                if s.a(EE_UNITS_UCB_IS_TINTED, wall) == 0 {
                    s.ab(EE_UNITS_UCB_SET_COLOR, wall, 1);
                    s.ab(EE_UNITS_UCB_SET_TINTED, wall, 1);
                    s.ab(EE_UNITS_UCB_SET_GATE_DIRECTION_OF, wall, d as i64 + 1);
                }
            }
        }
        for &side in &sides {
            s.a(EE_UNITS_UCB_UNITS_CLEAR, side);
        }
    }
}

/// `EEFindGateWall(tile, direction)`: a highlighted living wall on the tile that can take a gate, its direction.
pub fn find_gate_wall<H: UcHost + ?Sized>(h: &mut H, tile: [c_long; 2]) -> Option<i64> {
    let mut s = S { h, class: 0, unit: 0 };
    let t = s.abc(EE_UNITS_UCB_MAP_TILE, 0, i64::from(tile[0]), i64::from(tile[1])) as u64;
    if t == 0 {
        return None;
    }
    let mut node = s.a(EE_UNITS_UCB_TILE_NODES, t) as u64;
    while node != 0 {
        let u = s.a(EE_UNITS_UCB_GATE_NODE_UNIT, node) as u64;
        if u != 0 && s.a(EE_UNITS_UCB_TINTED_WALL, u) != 0 {
            return Some(s.a(EE_UNITS_UCB_GATE_DIRECTION_OF, u));
        }
        node = s.a(EE_UNITS_UCB_GATE_NODE_NEXT, node) as u64;
    }
    None
}

/// `FindNearestTargetBuilding(unit)`: the first living targetable land building in rings 1..19 around the unit's
/// tile.
pub fn nearest_target_building<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) -> u64 {
    let mut s = S { h, class, unit: 0 };
    let ring_vec = tiles(&mut s, T_RING_TILES);
    s.a(EE_UNITS_UCB_GATE_TILES_CLEAR, ring_vec);
    for ring in 1..0x14 {
        let x = s.a(EE_UNITS_UCB_UNIT_TILE_X, unit);
        let y = s.a(EE_UNITS_UCB_UNIT_TILE_Y, unit);
        s.abc(EE_UNITS_UCB_RING, ring_vec, pair(x, y), ring);
        let n = s.a(EE_UNITS_UCB_TILES_SIZE, ring_vec);
        for i in 0..n {
            let tx = s.ab(EE_UNITS_UCB_TILE_X, ring_vec, i);
            let ty = s.ab(EE_UNITS_UCB_TILE_Y, ring_vec, i);
            let t = s.abc(EE_UNITS_UCB_MAP_TILE, 0, tx, ty) as u64;
            if t == 0 {
                continue;
            }
            let mut node = s.a(EE_UNITS_UCB_TILE_NODES, t) as u64;
            while node != 0 {
                let u = s.a(EE_UNITS_UCB_GATE_NODE_UNIT, node) as u64;
                if u != 0 && s.a(EE_UNITS_UCB_TARGET_BUILDING, u) != 0 {
                    return u;
                }
                node = s.a(EE_UNITS_UCB_GATE_NODE_NEXT, node) as u64;
            }
        }
    }
    0
}

/// `GateGetSurroundingTiles(unit, out, corners, append)`: the footprint tiles of every gate piece not left out,
/// sorted and unique.
pub fn gate_surrounding<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, out: u64, corners: i64, append: bool) {
    let mut s = S { h, class, unit };
    if !append {
        s.a(EE_UNITS_UCB_GATE_TILES_CLEAR, out);
    }
    let mut piece = s.a(EE_UNITS_UCB_COMPLEX_OF, unit) as u64;
    loop {
        let previous = s.a(EE_UNITS_UCB_PREVIOUS_PART, piece) as u64;
        if previous == 0 {
            break;
        }
        piece = previous;
    }
    while piece != 0 {
        if s.a(EE_UNITS_UCB_PART_LEFT_OUT, piece) == 0 {
            s.abc(EE_UNITS_UCB_GATE_BASE_SURROUNDING, piece, out as i64, corners);
        }
        piece = s.a(EE_UNITS_UCB_NEXT_PART_OF, piece) as u64;
    }
    s.a(EE_UNITS_UCB_SORT_UNIQUE, out);
}

/// `GateUnitCreated(unit)`: a building's set-up; the chain's first piece starts closed.
pub fn gate_unit_created<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit };
    s.s(EE_UNITS_UCB_BUILDING_UNIT_CREATED);
    if s.s(EE_UNITS_UCB_CHAIN_HEAD) != 0 {
        s.b(EE_UNITS_UCB_START_ACTION, ACTION_CLOSED);
        s.s(EE_UNITS_UCB_ACTION_CHANGED);
    }
}

/// `SpaceBuildingUnitCreated(unit)`: a building's set-up, already built.
pub fn space_unit_created<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64) {
    let mut s = S { h, class, unit };
    s.s(EE_UNITS_UCB_BUILDING_UNIT_CREATED);
    s.s(EE_UNITS_UCB_SPACE_SET_BUILT);
}

/// `SpaceBuildingHasArrived(unit, value)`: already sinking, a slow request, or at the next waypoint's tile.
pub fn space_arrived<H: UcHost + ?Sized>(h: &mut H, class: u64, unit: u64, value: f32) -> bool {
    let mut s = S { h, class, unit };
    if s.s(EE_UNITS_UCB_COLLISION_STATE) == 8 || value <= ARRIVED_SPEED {
        return true;
    }
    let mut p = [0.0f32; 3];
    s.point(EE_UNITS_UCB_NEXT_WAYPOINT, 0, 0, 0, &mut p);
    s.s(EE_UNITS_UCB_SELF_TILE_X) as c_long == floor(p[0]) && s.s(EE_UNITS_UCB_SELF_TILE_Y) as c_long == floor(p[1])
}

impl<H: UcHost + ?Sized> S<'_, H> {
    /// a statement on the floats `f` answering `f[0]`
    fn float_of(&mut self, op: i32, mut f: [f32; 4]) -> f32 {
        self.h.uc(op, self.class, self.unit, 0, 0, 0, &mut f);
        f[0]
    }
}

/// Runs body `what` (`EE_UNITS_UCB_DO_*`) of `class` on `unit`; `a`, `b`, `c`, `f` its arguments (see each
/// [`ee_units_ucb_gate`] caller); the answer, or 0.
#[allow(clippy::too_many_arguments)]
pub fn run<H: UcHost + ?Sized>(h: &mut H, what: i32, class: u64, unit: u64, a: u64, b: i64, c: i64, f: f32) -> i64 {
    let tile = [b as u32 as c_long, (b >> 32) as u32 as c_long];
    match what {
        EE_UNITS_UCB_DO_BUILD_GATE => build_gate(h, tile, a as c_long, c as u32 as c_long, (c >> 32) as u32 as c_long),
        EE_UNITS_UCB_DO_GATE_REPLACED => gate_replaced(h, class, a, unit),
        EE_UNITS_UCB_DO_WALL_GATE_FIT => {
            return i64::from(wall_gate_fit(h, tile, c as u32 as c_long, i64::from((c >> 32) as u32), a))
        }
        EE_UNITS_UCB_DO_HIGHLIGHT_GATE_WALLS => highlight_gate_walls(h, a, b),
        EE_UNITS_UCB_DO_CLEAR_GATE_HIGHLIGHTS => clear_gate_highlights(h, a, b),
        EE_UNITS_UCB_DO_FIND_GATE_WALL => return find_gate_wall(h, tile).unwrap_or(-1),
        EE_UNITS_UCB_DO_NEAREST_TARGET_BUILDING => return nearest_target_building(h, class, a) as i64,
        EE_UNITS_UCB_DO_GATE_SURROUNDING => gate_surrounding(h, class, unit, a, b, c != 0),
        EE_UNITS_UCB_DO_GATE_UNIT_CREATED => gate_unit_created(h, class, unit),
        EE_UNITS_UCB_DO_SPACE_ARRIVED => return i64::from(space_arrived(h, class, unit, f)),
        EE_UNITS_UCB_DO_SPACE_UNIT_CREATED => space_unit_created(h, class, unit),
        _ => {}
    }
    0
}

#[cfg(not(test))]
pub(crate) mod game {
    use super::*;
    use crate::units::ucb_release::game::ee_units_ucb_host;
    use std::os::raw::c_void;

    struct GameG;

    impl UcHost for GameG {
        #[allow(clippy::too_many_arguments)]
        fn uc(&mut self, op: i32, class: u64, unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
            if let Some(v) =
                crate::units::uc_view::uc_view(crate::units::uc_view::Kind::Buildings, op, class, unit, a, b, c, f)
            {
                return v;
            }
            // SAFETY: the body runs inside the C++ function that handed over its live objects
            unsafe { ee_units_ucb_host(op, class as usize as *mut c_void, unit as usize as *mut c_void, a, b, c, f) }
        }
    }

    /// The gates' and buildings' body `what` (`EE_UNITS_UCB_DO_*`): `class` and `unit` the member's, `a` an object
    /// (a unit, a player, a vector), `b` a tile pair (low, high 32 bits) or a value, `c` a value pair, `f` a value;
    /// the answer, or 0 (`EE_UNITS_UCB_DO_FIND_GATE_WALL`: whether found, the direction written to the `long` at `a`).
    ///
    /// # Safety
    /// The objects are live, on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_ucb_gate(
        what: i32,
        class: *mut c_void,
        unit: *mut c_void,
        a: u64,
        b: i64,
        c: i64,
        f: f32,
    ) -> i64 {
        let r = run(&mut GameG, what, class as usize as u64, unit as usize as u64, a, b, c, f);
        if what != EE_UNITS_UCB_DO_FIND_GATE_WALL {
            return r;
        }
        if r < 0 {
            return 0;
        }
        // SAFETY: the caller's direction (a `long`)
        unsafe { *(a as usize as *mut c_long) = r as c_long };
        1
    }
}

#[cfg(test)]
#[path = "ucb_gate_tests.rs"]
mod tests;
