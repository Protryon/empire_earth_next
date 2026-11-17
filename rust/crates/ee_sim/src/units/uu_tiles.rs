//! The unit utilities' tile geometry (the unit utilities lane, round 1): the pure leaves of `src/Empire Earth/User
//! Interface Thread/EEFOFindNewTargetNearOldTarget.cpp` (the original's "EEUnitUtilities.cpp"): the tile rings and
//! squares, the compass directions, the step pattern inside a tile, the wall pieces' frame sets. No world read, no
//! random draw: every function is the C++ body's statements over `i32` tiles (`long` on Windows) and `f32`s in the
//! C++'s order (RUST_PLAN 2.1).
//!
//! A tile is `[x, y]` (`U2DPoint<long>` / `EETileCoordinate`, one type in the port).

use ee_render::ge::um::{floor, lrintf, um_arctan2};
use std::f32::consts::{PI, TAU};

/// cbindgen:ignore
pub type Tile = [i32; 2];

/// `UMCeil`: rounds to nearest, then steps up when that went down.
#[inline]
pub fn ceil(value: f32) -> i32 {
    let result = lrintf(value);
    if (result as f32) < value {
        result.wrapping_add(1)
    } else {
        result
    }
}

/// The C++ `(long)` of a float on Windows (`cvttss2si`): towards zero; a NaN or a value out of the `long` range gives
/// `i32::MIN` (the "integer indefinite"), where `as` would saturate.
#[inline]
pub fn trunc_long(value: f32) -> i32 {
    if value > -2_147_483_904.0 && value < 2_147_483_648.0 {
        value as i32
    } else {
        i32::MIN
    }
}

/// `EEGetSubTileSteps`' result: the first spot's offset in the tile, the step between spots, and how many spots.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsUuSteps {
    pub start_x: f32,
    pub start_y: f32,
    pub step_x: f32,
    pub step_y: f32,
    pub count: i32,
}

/// `EEGetSubTileSteps` (0x23f697): the start (within the tile) and per-step offsets of the spots a unit of `size`
/// tries on a tile entered from the neighbour (`dx`, `dy`) away, and how many (the square of the spots per side for
/// `square`; one when `length` matches the leg). The C++ is one `switch` per `dx` with a case per `dy`; each arm
/// here names its start (`h`: half the size, `o`: one less half), its step and its count.
pub fn sub_tile_steps(size: f32, length: i32, dx: i32, dy: i32, square: bool) -> EeUnitsUuSteps {
    let h = size * 0.5;
    let o = 1.0 - h;
    let per_side = || floor(1.0 / size);
    let squared = || floor(1.0 / size).wrapping_mul(floor(1.0 / size));
    // the counts: SQ1 the square or one spot, SQN the square or one side's spots
    let sq1 = || if square { squared() } else { 1 };
    let sqn = || if square { squared() } else { per_side() };
    let s = |start_x: f32, start_y: f32, step_x: f32, step_y: f32, count: i32| EeUnitsUuSteps {
        start_x,
        start_y,
        step_x,
        step_y,
        count,
    };
    let leg = |matches: bool| if matches { 1 } else { sqn() };
    let found = match dx {
        -3 => match dy {
            -3 => Some(s(o, o, 0.0, 0.0, sq1())),
            -2..=3 => Some(s(o, o, 0.0, -size, sqn())),
            4 => Some(s(o, h, 0.0, size, sq1())),
            _ => None,
        },
        -2 => match dy {
            -3 => Some(s(h, o, size, 0.0, sqn())),
            -2 => Some(s(o, o, 0.0, 0.0, sq1())),
            -1..=2 => Some(s(o, o, 0.0, -size, sqn())),
            3 => Some(s(o, h, 0.0, size, sq1())),
            4 => Some(s(o, h, -size, 0.0, sqn())),
            _ => None,
        },
        -1 => match dy {
            -4..=-2 => Some(s(h, o, size, 0.0, sqn())),
            -1 => Some(s(o, o, 0.0, 0.0, sq1())),
            0 => Some(s(o, o, 0.0, -size, sqn())),
            1..=4 => Some(s(o, h, 0.0, -size, leg(length == dy))),
            _ => None,
        },
        0 => match dy {
            -3..=-1 => Some(s(h, o, size, 0.0, sqn())),
            0 => Some(s(h, h, 0.0, size, squared())),
            1..=4 => Some(s(o, h, -size, 0.0, sqn())),
            _ => None,
        },
        1 => match dy {
            -3 | -2 => Some(s(h, o, size, 0.0, sqn())),
            -1 => Some(s(h, o, size, 0.0, leg(length == 1))),
            0 => Some(s(h, h, 0.0, size, sqn())),
            1 => Some(s(h, h, 0.0, h, sq1())),
            2..=4 => Some(s(o, h, -size, 0.0, sqn())),
            _ => None,
        },
        2 => match dy {
            -3 | -2 => Some(s(h, o, size, 0.0, sqn())),
            -1 => Some(s(h, o, size, 0.0, leg(length == dx))),
            0 | 1 => Some(s(h, h, 0.0, size, sqn())),
            2 => Some(s(h, h, 0.0, h, sq1())),
            3 | 4 => Some(s(o, h, -size, 0.0, sqn())),
            _ => None,
        },
        3 => match dy {
            -3 => Some(s(h, o, size, 0.0, sqn())),
            -2 => Some(s(h, o, 0.0, -size, sq1())),
            -1 => Some(s(h, o, size, 0.0, leg(length == dx))),
            0..=2 => Some(s(h, h, 0.0, size, sqn())),
            3 => Some(s(h, h, 0.0, h, sq1())),
            4 => Some(s(o, h, -size, 0.0, sqn())),
            _ => None,
        },
        4 => match dy {
            -3 => Some(s(h, o, 0.0, -size, sq1())),
            -2 | 0..=3 => Some(s(h, h, 0.0, size, sqn())),
            4 => Some(s(h, h, 0.0, h, sq1())),
            -1 => Some(s(h, o, size, 0.0, leg(length == dx))),
            _ => None,
        },
        _ => None,
    };
    found.unwrap_or_else(|| {
        // the fall-through: `(long)(1.0f / inSize)` truncated, squared
        let count = trunc_long(1.0 / size);
        s(h, h, 0.0, size, count.wrapping_mul(count))
    })
}

/// `EEGetSquareRing` (0x24697b): the tiles around the `width` x `height` rectangle at (`x`, `y`), sides only: per row
/// the left and right tile, then per column the top and bottom tile.
pub fn square_ring(x: i32, y: i32, width: i32, height: i32) -> Vec<Tile> {
    let mut out = Vec::with_capacity(((width + height) * 2).max(0) as usize);
    let bottom = height.wrapping_add(y);
    let mut ty = y;
    while ty <= bottom.wrapping_sub(1) {
        out.push([x.wrapping_sub(1), ty]);
        out.push([x.wrapping_add(width), ty]);
        ty += 1;
    }
    let right = x.wrapping_sub(1).wrapping_add(width);
    let mut tx = x;
    while tx <= right {
        out.push([tx, y.wrapping_sub(1)]);
        out.push([tx, bottom]);
        tx += 1;
    }
    out
}

/// `EEGetSquareRingTiles` (0x2492c2): the ring of tiles at distance `radius` around (`x`, `y`): the top row left to
/// right, then per inner row (top down) its left and right tile, then the bottom row.
pub fn square_ring_tiles(x: i32, y: i32, radius: i32) -> Vec<Tile> {
    let side = radius.wrapping_mul(2).wrapping_add(1);
    let inner = radius.wrapping_mul(2).wrapping_sub(1);
    let mut out = Vec::new();
    let left = x.wrapping_sub(radius);
    for i in 0..side.max(0) {
        out.push([i.wrapping_add(left), y.wrapping_add(radius)]);
    }
    let mut ty = y.wrapping_sub(1).wrapping_add(radius);
    let mut j = inner;
    while j > 0 {
        out.push([left, ty]);
        out.push([x.wrapping_add(radius), ty]);
        ty = ty.wrapping_sub(1);
        j -= 1;
    }
    for i in 0..side.max(0) {
        out.push([left.wrapping_add(i), y.wrapping_sub(radius)]);
    }
    out
}

/// `EEGetTilesInRadius` (0x24b556): appends the square of tiles within `radius` of (`x`, `y`) (one less on the far
/// sides unless `inclusive`), column by column. `radius` is the C++'s `unsigned long`: the near edge is the wrapping
/// difference, a radius above `i32::MAX` adds nothing.
pub fn tiles_in_radius(x: i32, y: i32, radius: u32, inclusive: bool, out: &mut Vec<Tile>) {
    let far = if inclusive { radius } else { radius.wrapping_sub(1) } as i32;
    if (radius as i32) < 0 {
        return;
    }
    let mut tx = (x as u32).wrapping_sub(radius) as i32;
    while tx <= x.wrapping_add(far) {
        let mut ty = (y as u32).wrapping_sub(radius) as i32;
        while ty <= far.wrapping_add(y) {
            out.push([tx, ty]);
            ty += 1;
        }
        tx += 1;
    }
}

/// `EEGetTilesNearPoint` (0x24b212): appends the tiles whose nearest point lies within `distance` of (`px`, `py`)
/// (column by column), or the point's own tile for a distance of at most one.
pub fn tiles_near_point(px: f32, py: f32, distance: f32, out: &mut Vec<Tile>) {
    let squared = distance * distance;
    let left = floor(px - distance);
    let top = floor(py - distance);
    let right = floor(px + distance);
    let bottom = floor(py + distance);
    if distance > 1.0 {
        let mut tx = left;
        while tx <= right {
            let center_x = tx as f32 + 0.5;
            let mut ty = top;
            while ty <= bottom {
                let near_x = if center_x < px { tx.wrapping_add(1) as f32 } else { tx as f32 };
                let near_y = if ty as f32 + 0.5 < py { ty.wrapping_add(1) as f32 } else { ty as f32 };
                if (py - near_y) * (py - near_y) + (px - near_x) * (px - near_x) <= squared {
                    out.push([tx, ty]);
                }
                ty += 1;
            }
            tx += 1;
        }
    } else {
        out.push([floor(px), floor(py)]);
    }
}

/// `EEDropDiagonalCorners` (0x2457a3): walks the path back from its end and drops the tile between two tiles one
/// step apart that differ by exactly (+1, +1) (the C++ reads the last tile of an empty list: nothing to drop).
pub fn drop_diagonal_corners(tiles: &mut Vec<Tile>) {
    let mut count = tiles.len();
    if count == 0 {
        return;
    }
    let mut prev = [0, 0];
    let mut last = tiles[count - 1];
    if count > 1 {
        prev = tiles[count - 2];
    }
    let mut i = count as i64 - 3;
    while i >= 0 {
        let at = i as usize;
        if at < count {
            let [x, y] = tiles[at];
            if last[0].wrapping_sub(x) == 1 && last[1].wrapping_sub(y) == 1 {
                tiles.remove(at + 1);
                count = tiles.len();
            } else {
                last = prev;
            }
            prev = [x, y];
        }
        i -= 1;
    }
}

/// `EEDropRepeatedTiles` (0x24da0c): drops each tile equal to the one before it (the first compared with
/// (1000, 1000)).
pub fn drop_repeated_tiles(tiles: &mut Vec<Tile>) {
    let mut prev = [1000, 1000];
    let mut i = 0;
    while i < tiles.len() {
        if tiles[i] == prev {
            tiles.remove(i);
        } else {
            prev = tiles[i];
            i += 1;
        }
    }
}

/// `EETurnDirection` (0x24da82): the compass direction `steps` turns on (`clockwise` or not); out of 0..7 unchanged.
pub fn turn_direction(mut direction: i32, steps: i32, clockwise: bool) -> i32 {
    const CLOCKWISE: [i32; 8] = [4, 0, 1, 2, 5, 6, 7, 3];
    const COUNTER: [i32; 8] = [1, 2, 3, 7, 0, 4, 5, 6];
    if !(0..=7).contains(&direction) {
        return direction;
    }
    let mut i = steps.wrapping_sub(1);
    while i >= 0 {
        direction = if clockwise { CLOCKWISE } else { COUNTER }[direction as usize];
        i -= 1;
    }
    direction
}

/// `EEGetTileInDirection` (0x248e06): the tile `distance` away from (`x`, `y`) in a compass direction (itself for
/// another direction).
pub fn tile_in_direction(x: i32, y: i32, direction: i32, distance: i32) -> Tile {
    let (l, r) = (x.wrapping_sub(distance), x.wrapping_add(distance));
    let (u, d) = (y.wrapping_sub(distance), y.wrapping_add(distance));
    match direction {
        0 => [l, d],
        1 => [l, y],
        2 => [l, u],
        3 => [x, u],
        4 => [x, d],
        5 => [r, d],
        6 => [r, y],
        7 => [r, u],
        _ => [x, y],
    }
}

/// `EEMoveInDirection` (0x248efd): the point `distance` tiles away in a compass direction, its offset within the
/// tile kept.
pub fn move_in_direction(x: f32, y: f32, direction: i32, distance: i32) -> [f32; 2] {
    let tx = floor(x);
    let ty = floor(y);
    let offset_x = x - tx as f32;
    let offset_y = y - ty as f32;
    let [tx, ty] = tile_in_direction(tx, ty, direction, distance);
    [tx as f32 + offset_x, ty as f32 + offset_y]
}

// the C++ constants as written (clippy: their last digit is below f32's precision; the values are the C++'s floats)
#[allow(clippy::excessive_precision)]
const SECTOR_LOW: f32 = -0.392_699_09;
const SECTOR_SCALE: f32 = 1.273_239_5;
#[allow(clippy::excessive_precision)]
const SECTOR_HIGH: f32 = 0.392_699_09;

/// `EEGetTileDirection` (0x2451df): the compass sector (0..7) of the direction from one tile to another.
pub fn tile_direction(x1: i32, y1: i32, x2: i32, y2: i32) -> i32 {
    let angle = um_arctan2(x1.wrapping_sub(x2) as f32, y2.wrapping_sub(y1) as f32);
    let mut sector = 4;
    if angle > SECTOR_HIGH {
        sector = trunc_long((angle - SECTOR_HIGH) * SECTOR_SCALE);
    } else if angle < SECTOR_LOW {
        sector = trunc_long(5.0 - (SECTOR_HIGH + angle) * SECTOR_SCALE);
        if sector >= 8 {
            sector = 3;
        }
    }
    sector
}

/// `EEGetSlotTile` (0x24cbd7): the tile two tiles away in a direction 1..8 (itself for another). The C++ also reads
/// the town center's last ring and drops it.
pub fn slot_tile(tile: Tile, direction: i32) -> Tile {
    let (dx, dy) = match direction {
        1 => (-2, 2),
        2 => (0, 2),
        3 => (2, 2),
        4 => (-2, 0),
        5 => (2, 0),
        6 => (-2, -2),
        7 => (0, -2),
        8 => (2, -2),
        _ => (0, 0),
    };
    [tile[0].wrapping_add(dx), tile[1].wrapping_add(dy)]
}

/// `EEOrientWallPiece`'s choice (0x24e3f4): a wall piece's frame set for a corner (1..12, else a plain piece), and
/// the yaw its transformation turns to (`SetOrientationYPR(yaw, 0, 0)`), none for the corners that keep it.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeUnitsUuWallPiece {
    pub frame_set: i32,
    pub turns: u8,
    pub yaw: f32,
}

#[allow(clippy::excessive_precision)]
pub fn wall_piece(corner: i32) -> EeUnitsUuWallPiece {
    let (frame_set, yaw) = match corner {
        1 => (9, Some(1.5707964)),
        2 => (9, None),
        3 => (10, Some(0.78539819)),
        4 => (10, Some(2.3561945)),
        5 => (6, None),
        6 => (6, Some(1.5707964)),
        7 => (6, Some(PI)),
        8 => (6, Some(4.712389)),
        9 => (3, Some(3.926991)),
        10 => (3, Some(5.4977875)),
        11 => (3, Some(0.78539819)),
        12 => (3, Some(2.3561945)),
        _ => (11, None),
    };
    EeUnitsUuWallPiece { frame_set, turns: u8::from(yaw.is_some()), yaw: yaw.unwrap_or(0.0) }
}

/// `EEGetRingTiles`' order of a ring around a one- or two-tile square (0x24bb36): of the ring's sides and corners,
/// twelve tiles (a square of two) come in this order, eight (a square of one) after the centre in that one.
/// cbindgen:ignore
pub const RING_ORDER_12: [usize; 12] = [4, 6, 10, 1, 3, 11, 7, 5, 9, 2, 0, 8];
/// cbindgen:ignore
pub const RING_ORDER_9: [usize; 9] = [8, 2, 6, 1, 7, 3, 5, 0, 4];

/// `EEJitterHeading` (0x24db3f) after its draw: `angle` turned by `turn` percent of a full turn, kept in (-pi, pi].
pub fn jitter_heading(angle: f32, turn: f32) -> f32 {
    let mut a = TAU * (turn * 0.01) + angle;
    if a > TAU {
        let turns = a * 0.159_154_94;
        a = (ceil(turns) as f32 - turns) * TAU;
    }
    if a > PI {
        a -= TAU;
    } else if a < -PI {
        a += TAU;
    }
    a
}
