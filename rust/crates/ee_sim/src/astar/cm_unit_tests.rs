use super::*;

/// A fake world: continents and regions by tile, the calls asked in order.
#[derive(Default)]
struct Fake {
    on_grid: bool,
    unit_continent: u64,
    target_continent: u64,
    space_map: bool,
    planet: u16,
    complex_region: u16,
    target_region: u16,
    footprint: u16,
    asked: Vec<&'static str>,
}

impl ReachWorld for Fake {
    fn on_grid(&mut self, _from: (c_long, c_long), _to: (c_long, c_long)) -> bool {
        self.asked.push("grid");
        self.on_grid
    }
    fn continent_of_unit(&mut self) -> u64 {
        self.asked.push("from");
        self.unit_continent
    }
    fn continent_at(&mut self, _x: c_long, _y: c_long) -> u64 {
        self.asked.push("to");
        self.target_continent
    }
    fn space_map(&mut self) -> bool {
        self.asked.push("space");
        self.space_map
    }
    fn planet(&mut self) -> u16 {
        self.asked.push("planet");
        self.planet
    }
    fn region_of_complex(&mut self, kind: c_long) -> u16 {
        assert_eq!(kind, PATH_NON_SPACE);
        self.asked.push("complex region");
        self.complex_region
    }
    fn region_at(&mut self, _x: c_long, _y: c_long, _kind: c_long) -> u16 {
        self.asked.push("target region");
        self.target_region
    }
    fn footprint_region(&mut self, _kind: c_long) -> u16 {
        self.asked.push("footprint");
        self.footprint
    }
}

fn land() -> Reach {
    Reach { domains: 2, can_space: false, complex: Some((1, 2, 1)) }
}

#[test]
fn off_the_grid_is_never_reachable() {
    let mut w = Fake::default();
    assert!(!can_get_to(&mut w, land(), (0, 0), 1, 1));
    assert_eq!(w.asked, vec!["grid"]);
}

#[test]
fn the_same_continent_is_reachable_unless_air_blocked() {
    let mut w = Fake { on_grid: true, unit_continent: 5, target_continent: 5, ..Default::default() };
    assert!(can_get_to(&mut w, land(), (0, 0), 1, 1));
    assert_eq!(w.asked, vec!["grid", "from", "to"]);
    // an aircraft that cannot cross space is blocked even on one continent: its complex unit decides
    let air = Reach { domains: MOVEMENT_AIR, can_space: false, complex: Some((3, MOVEMENT_AIR, 9)) };
    let mut w = Fake { on_grid: true, unit_continent: 5, target_continent: 5, space_map: false, ..Default::default() };
    assert!(can_get_to(&mut w, air, (0, 0), 1, 1));
    assert_eq!(w.asked, vec!["grid", "from", "to", "space"]);
    // ... and without one it is not
    let mut w = Fake { on_grid: true, unit_continent: 5, target_continent: 5, ..Default::default() };
    assert!(!can_get_to(&mut w, Reach { complex: None, ..air }, (0, 0), 1, 1));
}

#[test]
fn a_land_unit_on_another_continent_cannot() {
    let mut w = Fake { on_grid: true, unit_continent: 5, target_continent: 6, ..Default::default() };
    assert!(!can_get_to(&mut w, land(), (0, 0), 1, 1));
    assert_eq!(w.asked, vec!["grid", "from", "to"]);
}

#[test]
fn planet_ships_always_and_airports_by_planet() {
    let mut w = Fake { on_grid: true, unit_continent: 5, target_continent: 6, ..Default::default() };
    assert!(can_get_to(&mut w, Reach { complex: Some((CATEGORY_PLANET_SPACESHIP, 8, 1)), ..land() }, (0, 0), 1, 1));
    // an airport, not air-blocked: reachable
    let airport = Reach { domains: 2, can_space: false, complex: Some((0x14, 2, TYPE_AIRPORT)) };
    let mut w = Fake { on_grid: true, unit_continent: 5, target_continent: 6, space_map: true, ..Default::default() };
    assert!(can_get_to(&mut w, airport, (0, 0), 1, 1));
    // an air-blocked aircraft on a space map: its planet against the target's region
    let air = Reach { domains: MOVEMENT_AIR, can_space: false, complex: Some((3, MOVEMENT_AIR, 9)) };
    let mut w = Fake {
        on_grid: true,
        unit_continent: 5,
        target_continent: 5,
        space_map: true,
        planet: 3,
        target_region: 3,
        ..Default::default()
    };
    assert!(can_get_to(&mut w, air, (0, 0), 1, 1));
    assert_eq!(w.asked, vec!["grid", "from", "to", "space", "planet", "target region"]);
    let mut w = Fake { on_grid: true, space_map: true, planet: 0, target_region: 0, ..Default::default() };
    assert!(!can_get_to(&mut w, air, (0, 0), 1, 1), "no planet");
    assert_eq!(w.asked, vec!["grid", "from", "to", "space", "planet"]);
}

#[test]
fn ships_and_spaceships_by_their_footprints_region() {
    let ship = Reach { domains: MOVEMENT_SEA, can_space: false, complex: Some((1, MOVEMENT_SEA, 2)) };
    let mut w = Fake {
        on_grid: true,
        unit_continent: 5,
        target_continent: 6,
        footprint: 4,
        target_region: 4,
        ..Default::default()
    };
    assert!(can_get_to(&mut w, ship, (0, 0), 1, 1));
    assert_eq!(w.asked, vec!["grid", "from", "to", "footprint", "target region"]);
    let mut w = Fake {
        on_grid: true,
        unit_continent: 5,
        target_continent: 6,
        footprint: 4,
        target_region: 7,
        ..Default::default()
    };
    assert!(!can_get_to(&mut w, ship, (0, 0), 1, 1));
    let mut w = Fake {
        on_grid: true,
        unit_continent: 5,
        target_continent: 6,
        footprint: 0,
        target_region: 0,
        ..Default::default()
    };
    assert!(!can_get_to(&mut w, ship, (0, 0), 1, 1), "no region");
    assert_eq!(w.asked, vec!["grid", "from", "to", "footprint"]);
}

#[test]
fn the_footprints_first_region() {
    assert_eq!(first_region([0, 0, 3, 4]), 3);
    assert_eq!(first_region([0, 0]), 0);
    assert_eq!(first_region([]), 0);
}
