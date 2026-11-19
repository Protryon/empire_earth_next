//! The buildings' ground levelling of `EEUnitClass` (the unit classes cluster, round 1; EEUnitClass.cpp until this
//! round): `LevelGround` (a new building's height, the ground under a large land building flattened to its lowest
//! corner), the airport's runways (`CreateRunways` and the four `LevelRunway*` directions). The C++ members are
//! one-line calls of the entry points in cls_level_host.rs.
//!
//! The world is the [`LevelHost`]: the terrain mesh's corner heights, the tiles' heights, the class's records and
//! the runway pieces' creation, linking and facing. Determinism: the C++'s float sums in order (the granary's
//! average), `long` as `i64` (the loops' unsigned bounds as the C++'s).

// `c_long` casts are needed on LP64.
#![allow(clippy::unnecessary_cast)]

use crate::units::unit_class_fields::EeUnitsUnitClassFields;

/// The unit types the levelling tests.
/// cbindgen:ignore
mod k {
    pub const T_FARM: u64 = 0x2a;
    pub const T_GRANARY: u64 = 0x34;
    pub const T_AIRPORT_RUNWAY: u64 = 0xf0;
    pub const T_CAMERA_OBJECT: u64 = 0x167;
    pub const T_VOLCANO: u64 = 0x16e;
    pub const T_GRANARY_IMPROVED: u64 = 0x2ba;
    pub const T_SPACE_TURRET: u64 = 0x302;
    pub const T_ORBITAL_SPACE_STATION: u64 = 0x31f;
    /// `kBuildingZOffset`, the turret's and the station's heights (0x4cc9fc, 0x4cca00)
    pub const BUILDING_Z_OFFSET: f32 = 0.01;
    pub const WALL_HEIGHT: f32 = 0.7;
    pub const TOWER_HEIGHT: f32 = 1.5;
}
use k::*;

/// The world the levelling asks (the game's: cls_level_host.rs). Tiles are `EEMapTile*` (0: off the map).
pub trait LevelHost {
    /// `gWorld.GetMap()` (0: none); its `GetTile(x, y)`; a tile's `mHeight` and its store
    fn map(&mut self) -> u64;
    fn map_tile(&mut self, x: i64, y: i64) -> u64;
    fn tile_height(&mut self, tile: u64) -> f32;
    fn set_tile_height(&mut self, tile: u64, height: f32);
    /// the terrain mesh's `GetZ(x, y)` and `SetZ(x, y, z)` (`mesh` the mesh's address)
    fn mesh_z(&mut self, mesh: u64, x: i64, y: i64) -> f32;
    fn set_mesh_z(&mut self, mesh: u64, x: i64, y: i64, z: f32);
    /// `&gWorld.mMap->mTerrainMesh`
    fn map_mesh(&mut self) -> u64;
    /// a class object's record and its `IsLandBuilding()`; `gUnitClassTable[unit_type]`
    fn class(&mut self, class: u64) -> *const EeUnitsUnitClassFields;
    fn land_building(&mut self, class: u64) -> bool;
    fn class_of_type(&mut self, unit_type: u64) -> u64;
    /// a unit's tile, facing, class and player id
    fn unit_tile(&mut self, unit: u64) -> [i64; 2];
    fn unit_facing(&mut self, unit: u64) -> u64;
    fn unit_class(&mut self, unit: u64) -> u64;
    /// `class->CreateUnit(x, y, player, foundation, creator)->mComplexUnit`
    fn create_unit(&mut self, class: u64, x: i64, y: i64, player: i64, foundation: bool, creator: u64) -> u64;
    /// `unit->LinkPart(other)`, `unit->SetFacing(facing)`, `gCameraFollowing = false`
    fn link_part(&mut self, unit: u64, other: u64);
    fn set_facing(&mut self, unit: u64, facing: u64);
    fn camera_following_off(&mut self);
}

fn class<H: LevelHost + ?Sized>(h: &mut H, class: u64) -> EeUnitsUnitClassFields {
    // SAFETY: a live class's record (copied at once)
    unsafe { *h.class(class) }
}

/// `EEUnitClass::LevelGround(mesh, x, y, &z)`: a new building's height in `z` (left as it was for the classes
/// below), the ground under a large land building flattened to its lowest corner (a granary: the corners'
/// average); the space turret and the orbital station sit higher.
pub fn level_ground<H: LevelHost + ?Sized>(h: &mut H, cls: u64, mesh: u64, x0: u64, y0: u64, z: &mut f32) {
    let c = class(h, cls);
    let map = h.map();
    if map == 0 || h.map_tile(x0 as i64, y0 as i64) == 0 {
        return;
    }
    let unit_type = c.mUnitType as u64;
    let size = c.mTileSize as i64 as u64;
    let (xe, ye) = (x0.wrapping_add(size), y0.wrapping_add(size));
    if unit_type == T_GRANARY_IMPROVED || unit_type == T_GRANARY {
        let mut sum = 0.0f32;
        let mut count = 0u32;
        for x in x0..=xe {
            for y in y0..=ye {
                sum += h.mesh_z(mesh, x as i64, y as i64);
                count = count.wrapping_add(1);
            }
        }
        *z = sum / count as f32 + BUILDING_Z_OFFSET;
    } else if h.land_building(cls) && (c.mTileSize as i64) > 1 && unit_type != T_FARM && unit_type != T_VOLCANO {
        let mut minimum = f32::MAX;
        for x in x0..=xe {
            for y in y0..=ye {
                let corner = h.mesh_z(mesh, x as i64, y as i64);
                if corner < minimum && corner >= 0.0 {
                    minimum = corner;
                }
            }
        }
        if minimum == f32::MAX {
            minimum = 0.0;
        }
        for x in x0..=xe {
            for y in y0..=ye {
                if h.mesh_z(mesh, x as i64, y as i64) >= 0.0 {
                    h.set_mesh_z(mesh, x as i64, y as i64, minimum);
                }
            }
        }
        for x in x0..xe {
            for y in y0..ye {
                let tile = h.map_tile(x as i64, y as i64);
                if tile != 0 && h.tile_height(tile) >= 0.0 {
                    h.set_tile_height(tile, minimum);
                }
            }
        }
        *z = BUILDING_Z_OFFSET + minimum;
        h.camera_following_off();
    } else if h.land_building(cls)
        && c.mTileSize as i64 == 1
        && unit_type != T_FARM
        && unit_type != T_VOLCANO
        && unit_type != T_CAMERA_OBJECT
    {
        *z = h.mesh_z(mesh, x0 as i64, y0 as i64);
    }
    if unit_type == T_SPACE_TURRET {
        *z += WALL_HEIGHT;
    } else if unit_type == T_ORBITAL_SPACE_STATION {
        *z += TOWER_HEIGHT;
    }
}

/// One runway levelling: the mesh corners over `xs` x `ys` (inclusive) and the tiles inside to the airport tile's
/// height.
fn level<H: LevelHost + ?Sized>(h: &mut H, unit: u64, xs: (i64, i64), ys: (i64, i64)) {
    let at = h.unit_tile(unit);
    if h.map() == 0 {
        return;
    }
    let tile = h.map_tile(at[0], at[1]);
    if tile == 0 {
        return;
    }
    let height = h.tile_height(tile);
    let mesh = h.map_mesh();
    for y in ys.0..=ys.1 {
        for x in xs.0..=xs.1 {
            h.set_mesh_z(mesh, x, y, height);
        }
    }
    for y in ys.0..ys.1 {
        for x in xs.0..xs.1 {
            let tile = h.map_tile(x, y);
            h.set_tile_height(tile, height);
        }
    }
}

/// `EEUnitClass::LevelRunway{PosX,PosY,NegY,NegX}(airport)` by facing 2, 0, 1, else.
pub fn level_runway<H: LevelHost + ?Sized>(h: &mut H, unit: u64, facing: u64) {
    let unit_class = h.unit_class(unit);
    let size = class(h, unit_class).mTileSize as i64;
    let [x, y] = h.unit_tile(unit);
    match facing {
        0 => level(h, unit, (x, x + size), (y, y + size * 4)),
        1 => level(h, unit, (x, x + size), (y - size * 3, y + 1)),
        2 => level(h, unit, (x, x + size * 4), (y, y + size)),
        _ => level(h, unit, (x - size * 3, x + 1), (y, y + size)),
    }
}

/// `EEUnitClass::CreateRunways(airport, player, foundation)`: the three runway pieces in the airport's facing
/// direction on levelled ground, each linked to the one before and facing as the airport.
pub fn create_runways<H: LevelHost + ?Sized>(h: &mut H, cls: u64, unit: u64, player: i64, foundation: bool) {
    let heading = h.unit_facing(unit);
    let [mut x, mut y] = h.unit_tile(unit);
    let size = class(h, cls).mTileSize as i64;
    let runway = h.class_of_type(T_AIRPORT_RUNWAY);
    level_runway(h, unit, heading);
    let (dx, dy) = match heading {
        0 => (0, size),
        1 => (0, -size),
        2 => (size, 0),
        _ => (-size, 0),
    };
    x += dx;
    y += dy;
    let first = h.create_unit(runway, x, y, player, foundation, unit);
    x += dx;
    y += dy;
    let second = h.create_unit(runway, x, y, player, foundation, first);
    let third = h.create_unit(runway, x + dx, y + dy, player, foundation, second);
    h.link_part(third, second);
    h.link_part(second, first);
    h.link_part(first, unit);
    h.set_facing(first, heading);
    h.set_facing(second, heading);
    h.set_facing(third, heading);
    h.camera_following_off();
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::BTreeMap;

    #[derive(Default)]
    struct Mock {
        classes: Vec<EeUnitsUnitClassFields>,
        land: bool,
        mesh: BTreeMap<(i64, i64), f32>,
        tiles: BTreeMap<(i64, i64), f32>,
        log: Vec<String>,
        created: u64,
    }

    impl LevelHost for Mock {
        fn map(&mut self) -> u64 {
            1
        }
        fn map_tile(&mut self, x: i64, y: i64) -> u64 {
            if self.tiles.contains_key(&(x, y)) {
                ((x as u64) << 16 | y as u64) + 1
            } else {
                0
            }
        }
        fn tile_height(&mut self, tile: u64) -> f32 {
            let t = tile - 1;
            self.tiles[&((t >> 16) as i64, (t & 0xffff) as i64)]
        }
        fn set_tile_height(&mut self, tile: u64, height: f32) {
            if tile == 0 {
                return;
            }
            let t = tile - 1;
            self.tiles.insert(((t >> 16) as i64, (t & 0xffff) as i64), height);
        }
        fn mesh_z(&mut self, _mesh: u64, x: i64, y: i64) -> f32 {
            self.mesh.get(&(x, y)).copied().unwrap_or(1.0)
        }
        fn set_mesh_z(&mut self, _mesh: u64, x: i64, y: i64, z: f32) {
            self.mesh.insert((x, y), z);
        }
        fn map_mesh(&mut self) -> u64 {
            7
        }
        fn class(&mut self, class: u64) -> *const EeUnitsUnitClassFields {
            &self.classes[class as usize]
        }
        fn land_building(&mut self, _class: u64) -> bool {
            self.land
        }
        fn class_of_type(&mut self, _unit_type: u64) -> u64 {
            1
        }
        fn unit_tile(&mut self, _unit: u64) -> [i64; 2] {
            [5, 5]
        }
        fn unit_facing(&mut self, _unit: u64) -> u64 {
            1
        }
        fn unit_class(&mut self, _unit: u64) -> u64 {
            0
        }
        fn create_unit(&mut self, _class: u64, x: i64, y: i64, _player: i64, _foundation: bool, creator: u64) -> u64 {
            self.created += 1;
            self.log.push(format!("create {x} {y} after {creator}"));
            100 + self.created
        }
        fn link_part(&mut self, unit: u64, other: u64) {
            self.log.push(format!("link {unit} {other}"));
        }
        fn set_facing(&mut self, unit: u64, facing: u64) {
            self.log.push(format!("facing {unit} {facing}"));
        }
        fn camera_following_off(&mut self) {
            self.log.push("camera".into());
        }
    }

    fn mock(unit_type: u64, size: i64) -> Mock {
        let mut m = Mock::default();
        m.classes.push(EeUnitsUnitClassFields {
            mUnitType: unit_type as _,
            mTileSize: size as _,
            ..Default::default()
        });
        m.classes.push(EeUnitsUnitClassFields { mUnitType: T_AIRPORT_RUNWAY as _, ..Default::default() });
        for x in 0..20 {
            for y in 0..20 {
                m.tiles.insert((x, y), 1.0);
            }
        }
        m
    }

    #[test]
    fn a_granary_takes_the_average_and_a_large_building_the_lowest_corner() {
        let mut m = mock(T_GRANARY, 1);
        m.mesh.insert((2, 2), 3.0);
        let mut z = 9.0;
        level_ground(&mut m, 0, 7, 2, 2, &mut z);
        assert_eq!(z, 6.0 / 4.0 + 0.01);
        let mut m = mock(0x40, 2);
        m.land = true;
        m.mesh.insert((3, 3), 0.25);
        m.mesh.insert((4, 4), -1.0);
        let mut z = 9.0;
        level_ground(&mut m, 0, 7, 2, 2, &mut z);
        assert_eq!(z, 0.01 + 0.25);
        assert_eq!(m.mesh[&(2, 2)], 0.25);
        assert_eq!(m.mesh[&(4, 4)], -1.0);
        assert_eq!(m.tiles[&(3, 3)], 0.25);
        assert_eq!(m.tiles[&(4, 4)], 1.0);
        assert_eq!(m.log, ["camera"]);
    }

    #[test]
    fn small_buildings_and_space_heights() {
        let mut m = mock(0x40, 1);
        m.land = true;
        m.mesh.insert((2, 2), 0.5);
        let mut z = 9.0;
        level_ground(&mut m, 0, 7, 2, 2, &mut z);
        assert_eq!(z, 0.5);
        let mut m = mock(T_SPACE_TURRET, 1);
        let mut z = 1.0;
        level_ground(&mut m, 0, 7, 2, 2, &mut z);
        assert_eq!(z, 1.7);
        let mut z = 1.0;
        level_ground(&mut m, 0, 7, 30, 2, &mut z);
        assert_eq!(z, 1.0);
    }

    #[test]
    fn runways_level_and_link_toward_the_facing() {
        let mut m = mock(0x42, 2);
        m.tiles.insert((5, 5), 0.75);
        create_runways(&mut m, 0, 50, 3, false);
        // facing 1 (-y): corners y from 5 - 6 to 6, x 5..7
        assert_eq!(m.mesh[&(5, -1)], 0.75);
        assert_eq!(m.mesh[&(7, 6)], 0.75);
        assert_eq!(m.tiles[&(6, 0)], 0.75);
        assert_eq!(
            m.log,
            [
                "create 5 3 after 50",
                "create 5 1 after 101",
                "create 5 -1 after 102",
                "link 103 102",
                "link 102 101",
                "link 101 50",
                "facing 101 1",
                "facing 102 1",
                "facing 103 1",
                "camera"
            ]
        );
    }
}
