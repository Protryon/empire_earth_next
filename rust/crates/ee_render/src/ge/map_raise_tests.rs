//! map_raise.rs' tests over a fake square terrain: the raise's plateau and slopes, the rings it spreads and the
//! rectangle it reports, the tiles' heights and water, and the two tile tests.

use super::*;

/// An n x n tile map (n + 1 points a side), every point and tile valid; the calls the edits make, logged.
struct Grid {
    n: c_ulong,
    z: Vec<f32>,
    water: Vec<bool>,
    heights: Vec<f32>,
    log: Vec<String>,
}

impl Grid {
    fn new(n: c_ulong) -> Grid {
        let points = ((n + 1) * (n + 1)) as usize;
        Grid {
            n,
            z: vec![0.0; points],
            water: vec![false; (n * n) as usize],
            heights: vec![0.0; (n * n) as usize],
            log: vec![],
        }
    }
    fn p(&self, x: c_ulong, y: c_ulong) -> usize {
        (y * (self.n + 1) + x) as usize
    }
}

impl Terrain for Grid {
    fn valid_point(&self, x: c_ulong, y: c_ulong) -> bool {
        x <= self.n && y <= self.n
    }
    fn valid_tile(&self, x: c_ulong, y: c_ulong) -> bool {
        x < self.n && y < self.n
    }
    fn z(&self, x: c_ulong, y: c_ulong) -> f32 {
        self.z[self.p(x, y)]
    }
    fn set_z(&mut self, x: c_ulong, y: c_ulong, z: f32) {
        let i = self.p(x, y);
        self.z[i] = z;
    }
    fn set_water(&mut self, x: c_ulong, y: c_ulong, water: bool) {
        self.water[(y * self.n + x) as usize] = water;
    }
    fn vertex_normal(&mut self, x: c_ulong, y: c_ulong) {
        self.log.push(format!("n {x} {y}"));
    }
    fn shorelines(&mut self, x: c_ulong, y: c_ulong) {
        self.log.push(format!("s {x} {y}"));
    }
    fn has_tile(&self, x: c_ulong, y: c_ulong) -> bool {
        self.valid_tile(x, y)
    }
    fn settle_tile(&mut self, x: c_ulong, y: c_ulong, height: f32) {
        self.heights[(y * self.n + x) as usize] = height;
    }
    fn update_water(&mut self, x: c_long, y: c_long) {
        self.log.push(format!("w {x} {y}"));
    }
}

fn fields() -> EeUnitsRtsMapFields {
    EeUnitsRtsMapFields { mF1955F4: 1.0, ..Default::default() }
}

#[test]
fn a_raise_makes_a_plateau_with_slopes_and_reports_the_changed_rectangle() {
    let mut g = Grid::new(20);
    let mut f = fields();
    let mut bounds = [8, 10, 10, 8];
    raise(&mut g, &mut f, &mut bounds, 2.0, 0.0, 2.0, 0.5, true);
    // the rectangle's points all copied the first one's height (slope 0)
    for y in 8..=11 {
        for x in 8..=11 {
            assert_eq!(g.z(x, y), 2.0, "({x}, {y})");
        }
    }
    // the rings fall by the slope per point of distance: one ring out at most 2 - 0.5 x 1
    assert!(g.z(7, 9) < 2.0 && g.z(7, 9) > 0.0);
    assert!(g.z(12, 9) < 2.0 && g.z(12, 9) > 0.0);
    assert!(g.z(5, 9) <= g.z(7, 9));
    // the reported rectangle grew by the rings
    let [l, r, t, b] = bounds;
    assert!(l < 8 && r > 10 && t > 10 && b < 8, "{bounds:?}");
    // the tiles under the plateau took their lowest corner; the normals and shorelines around were recomputed
    assert_eq!(g.heights[(9 * 20 + 9) as usize], 2.0);
    assert!(g.log.iter().any(|s| s == "n 8 8") && g.log.iter().any(|s| s == "s 9 9"));
    assert!(!g.log.iter().any(|s| s.starts_with('w')), "nothing crossed the sea level");
}

#[test]
fn a_lowered_vertex_below_the_sea_marks_its_tiles_as_water_and_updates_the_water() {
    let mut g = Grid::new(6);
    assert_eq!(smooth_vertex(&mut g, 3, 3, 3, 4, 0.0), 1);
    let mut f = fields();
    let mut bounds = [2, 2, 2, 2];
    raise(&mut g, &mut f, &mut bounds, -1.0, 0.0, 1.0, 1.0, true);
    assert_eq!(g.z(2, 2), -1.0);
    assert!(g.water[(2 * 6 + 2) as usize]);
    assert!(g.log.iter().any(|s| s == "w 2 3"), "{:?}", g.log);
}

#[test]
fn a_smoothed_height_near_zero_is_zero() {
    let mut g = Grid::new(4);
    let i = g.p(1, 1);
    g.z[i] = 1.0;
    // below the neighbour by more than the slope: raised to 1 - 0.995 = 0.005, within the epsilon of 0: 0
    assert_eq!(smooth_vertex(&mut g, 0, 1, 1, 1, 0.995), -1);
    assert_eq!(g.z(0, 1), 0.0);
    let i = g.p(2, 1);
    g.z[i] = 3.0;
    assert_eq!(smooth_vertex(&mut g, 2, 1, 1, 1, 1.995), 1);
    assert_eq!(g.z(2, 1), 2.995);
    let i = g.p(3, 1);
    g.z[i] = 1.0;
    let j = g.p(3, 2);
    g.z[j] = 0.005;
    assert_eq!(smooth_vertex(&mut g, 3, 1, 3, 2, 1.0), 0, "within the slope");
    assert_eq!(smooth_vertex(&mut g, 3, 1, 3, 2, 0.99), 1);
    // a raise from -2 to 0.5 - 0.495 is near zero: 0, and the vertex left the sea
    let k = g.p(0, 3);
    g.z[k] = -2.0;
    let m = g.p(1, 3);
    g.z[m] = 0.5;
    assert_eq!(smooth_vertex(&mut g, 0, 3, 1, 3, 0.495), -1);
    assert_eq!(g.z(0, 3), 0.0);
    assert!(g.log.iter().any(|l| l == "w 0 3"));
}

#[test]
fn off_the_map_nothing_changes() {
    let mut g = Grid::new(4);
    assert_eq!(smooth_vertex(&mut g, 9, 9, 1, 1, 0.0), 0);
    assert_eq!(smooth_vertex(&mut g, 1, 1, (-1i64) as c_ulong, 1, 0.0), 0);
    assert_eq!(raised_corners(&g, 9, 0), 0);
    assert!(!tile_flat(&g, 1.0, 0, 9));
}

#[test]
fn the_tile_tests_read_the_corners() {
    let mut g = Grid::new(4);
    let i = g.p(2, 1);
    g.z[i] = 0.4;
    let j = g.p(2, 2);
    g.z[j] = 0.39;
    assert_eq!(raised_corners(&g, 1, 1), 1);
    assert!(tile_flat(&g, 1.0, 1, 1));
    assert!(!tile_flat(&g, 0.5, 1, 1), "0.4 above 0.7 x 0.5");
    assert_eq!(raised_corners(&g, 2, 1), 1, "the 0.4 corner over the 0 one; 0.39 is below the 0.4");
}

#[test]
fn the_spreads_keep_the_corner_rings() {
    let mut g = Grid::new(30);
    for y in 10..=16 {
        for x in 10..=16 {
            let i = g.p(x, y);
            g.z[i] = 3.0;
        }
    }
    let mut f = fields();
    let bottom = spread_down(&mut g, &mut f, 10, 15, 10, 3.0, 0.5);
    assert!(bottom < 10);
    assert!(f.mL195600 > 0 && f.mL195604 > 0, "the corners moved");
    let left = spread_left(&mut g, &f, 10, 15, 10, 3.0, 0.5);
    assert!(left <= 10 - f.mL195600);
}

fn rect(left: c_long, top: c_long, right: c_long, bottom: c_long) -> EeGeAreaRect {
    EeGeAreaRect { bottom, left, right, top }
}

#[test]
fn the_outlines_skip_the_edges_another_area_of_the_continent_covers() {
    let mut runs = vec![];
    // two touching areas of continent 1 side by side, one of continent 2 over them
    let areas = [rect(0, 0, 2, 1), rect(2, 0, 4, 1), rect(0, 0, 4, 1)];
    area_outlines(&areas, &[1, 1, 2], false, &mut |_, _| 0, &mut |r| runs.push(r));
    // area 0's right edge (x 2) lies in area 1, whose left differs: covered; area 1's left edge likewise; area 2 is
    // alone on its continent: its 4 + 4 + 1 + 1 edges
    assert!(!runs.contains(&[2, 0, 2, 1]));
    assert_eq!(runs.iter().filter(|r| r[0] == 0 && r[2] == 0).count(), 2, "the two left edges at x 0");
    assert_eq!(runs.len(), (2 + 2 + 1) * 2 + 4 + 4 + 1 + 1);
}

#[test]
fn on_a_space_map_an_edge_splits_into_the_runs_on_its_continent() {
    let mut runs = vec![];
    // a unit edge has two tiles: the second off the continent
    outline_edge((3, 5), (4, 5), 7, true, &mut |x, _| if x == 3 { 7 } else { 0 }, &mut |r| runs.push(r));
    assert_eq!(runs, vec![[3, 5, 4, 5]]);
    runs.clear();
    outline_edge((3, 5), (3, 6), 7, true, &mut |_, _| 7, &mut |r| runs.push(r));
    assert_eq!(runs, vec![[3, 5, 3, 6]]);
    runs.clear();
    outline_edge((3, 5), (3, 6), 7, true, &mut |_, _| 1, &mut |r| runs.push(r));
    assert!(runs.is_empty());
}
