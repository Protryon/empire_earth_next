//! minimap.rs over a mock host: the cells' counts, dots and pixels, the redraw's walk, the terrain colours.
#![allow(clippy::excessive_precision)]

use crate::app::minimap::{diagonal_order, EeAppMiniMap, EeAppMiniMapItem, MiniMapHost, TileSight, CELLS, OFF_MAP};
use ee_base::math::types::GeTransformation;
use ee_base::math::types::GeVector;
use std::collections::BTreeMap;
use std::os::raw::{c_long, c_ulong};

#[derive(Default)]
struct Mock {
    hidden: Vec<(c_long, c_long)>,
    sight: BTreeMap<(c_long, c_long), TileSight>,
    items: BTreeMap<(c_long, c_long), Vec<EeAppMiniMapItem>>,
    asked: Vec<(c_long, c_long)>,
}

impl MiniMapHost for Mock {
    fn unit_shown(&mut self, x: c_long, y: c_long) -> bool {
        !self.hidden.contains(&(x, y))
    }
    fn tile_sight(&mut self, x: c_long, y: c_long) -> Option<TileSight> {
        self.asked.push((x, y));
        self.sight.get(&(x, y)).copied()
    }
    fn tile_items(&mut self, x: c_long, y: c_long, out: &mut Vec<EeAppMiniMapItem>) {
        out.extend(self.items.get(&(x, y)).into_iter().flatten().copied());
    }
    fn terrain_color(&mut self, _id: c_ulong) -> [f32; 3] {
        [0.5, 1.0, 2.0]
    }
    fn valid_point(&mut self, x: c_ulong, y: c_ulong) -> bool {
        x < 8 && y < 8
    }
    fn valid_tile(&mut self, x: c_ulong, y: c_ulong) -> bool {
        x < 7 && y < 7
    }
    fn texture_set(&mut self, _x: c_ulong, _y: c_ulong) -> c_ulong {
        0
    }
    fn vertex_color(&mut self, _x: c_ulong, _y: c_ulong) -> [f32; 3] {
        [1.0, 0.5, 0.25]
    }
    fn normal(&mut self, x: c_ulong, _y: c_ulong) -> GeVector {
        // lit from x 4 on
        GeVector { x: 0.0, y: 0.0, z: if x >= 4 { 1.0 } else { -1.0 } }
    }
}

fn identity() -> GeTransformation {
    let mut t = GeTransformation::default();
    for i in 0..4 {
        t.m[i][i] = 1.0;
    }
    t
}

struct Fixture {
    map: EeAppMiniMap,
    pixels: Vec<u16>,
}

impl Fixture {
    fn new() -> Self {
        let mut f = Fixture { map: EeAppMiniMap::default(), pixels: vec![0x1234; 64 * 64] };
        f.map.pixels = f.pixels.as_mut_ptr();
        f.map.pitch = 64;
        f.map.map_to_cell = identity();
        f.map.map_to_pixel = identity();
        f.map.pixel_to_map = identity();
        f.map.pixel_width = 1;
        f.map.pixel_height = 1;
        f
    }
    fn pixel(&self, x: usize, y: usize) -> u16 {
        self.pixels[64 * y + x]
    }
}

fn building(player: u16, color: u16, flag: c_long) -> EeAppMiniMapItem {
    EeAppMiniMapItem { player, color, flag, tile_size: 1, shown: true, ..Default::default() }
}

#[test]
fn counts_start_at_two_and_pick_the_cell_pixel() {
    let mut f = Fixture::new();
    let mut h = Mock::default();
    // tile (2, 3): the cell of its centre (2.5, 3.5) rounded half to even, (2, 4)
    f.map.count_cell(&mut h, 2, 3, true);
    assert_eq!(f.map.counts[2 * 64 + 4], 2);
    assert_eq!(f.pixel(2, 4), 0x777);
    f.map.count_cell(&mut h, 2, 3, false);
    assert_eq!(f.map.counts[2 * 64 + 4], 1);
    assert_eq!(f.pixel(2, 4), 0x7000);
    f.map.count_cell(&mut h, 2, 3, false);
    assert_eq!(f.pixel(2, 4), 0xf000);
    f.map.count_cell(&mut h, 2, 3, false);
    assert_eq!(f.map.counts[2 * 64 + 4], 0xffff); // the original's unsigned short wraps
}

#[test]
fn buildings_keep_a_flagged_head_first_and_draw_the_first() {
    let mut f = Fixture::new();
    let mut h = Mock::default();
    // a tile size of 1: the centre (x + 0.5, y + 0.5); (4, 4) -> cell (4, 4)
    f.map.building(&mut h, &building(1, 0x111, 1), 4, 4, true);
    f.map.building(&mut h, &building(2, 0x222, 0), 4, 4, true);
    let cell = &f.map.dots[4 * 64 + 4];
    assert_eq!(cell.iter().map(|d| d.color).collect::<Vec<_>>(), vec![0x111, 0x222]);
    assert_eq!(f.pixel(4, 4), 0x111);
    // removal matches player and colour only
    f.map.building(&mut h, &building(1, 0x111, 0), 4, 4, false);
    assert_eq!(f.pixel(4, 4), 0x222);
    // without a flagged head, a new dot goes first
    f.map.building(&mut h, &building(3, 0x333, 0), 4, 4, true);
    assert_eq!(f.pixel(4, 4), 0x333);
    // a class without a colour: nothing
    let none = EeAppMiniMapItem { shown: false, ..building(5, 0x555, 0) };
    f.map.building(&mut h, &none, 4, 4, true);
    assert_eq!(f.map.dots[4 * 64 + 4].len(), 2);
}

#[test]
fn a_wall_adds_three_more_segments_along_its_facing() {
    let mut f = Fixture::new();
    let mut h = Mock::default();
    let wall = EeAppMiniMapItem { wall: true, facing: 2, tile_size: 2, ..building(1, 0x111, 0) };
    // tile size 2: the centre (x + 1, y + 1)
    f.map.building(&mut h, &wall, 10, 10, true);
    for x in [11, 13, 15, 17] {
        assert_eq!(f.map.dots[x * 64 + 11].len(), 1, "segment at {x}");
    }
}

#[test]
fn units_move_between_cells_and_hide_on_unseen_tiles() {
    let mut f = Fixture::new();
    let mut h = Mock::default();
    let unit = EeAppMiniMapItem { unit: true, ..building(1, 0x0a0, 0) };
    f.map.move_unit(&mut h, &unit, OFF_MAP, OFF_MAP, 6, 6);
    assert_eq!(f.pixel(6, 6), 0x0a0);
    // within the cell (6.5 -> 6, 7.5 -> 8: another cell); (6, 6) -> (6, 6) is no move at all
    f.map.move_unit(&mut h, &unit, 6, 6, 6, 7);
    assert!(f.map.units[6 * 64 + 6].is_empty());
    assert_eq!(f.map.units[6 * 64 + 8][0].y, 7);
    assert_eq!(f.pixel(6, 6), 0xf000);
    // a unit on a tile the local player does not see: the cell's count shows
    h.hidden.push((6, 7));
    f.map.redraw_cell(&mut h, 6, 8);
    assert_eq!(f.pixel(6, 8), 0xf000);
    // the same cell: the dot's tile updated, no redraw
    f.map.move_unit(&mut h, &unit, 6, 7, 5, 7);
    assert_eq!((f.map.units[6 * 64 + 8][0].x, f.map.units[6 * 64 + 8][0].y), (5, 7));
}

#[test]
fn recolor_skips_gaia() {
    let mut f = Fixture::new();
    let mut h = Mock::default();
    f.map.building(&mut h, &building(0, 0x100, 0), 1, 1, true);
    f.map.building(&mut h, &building(2, 0x200, 0), 3, 3, true);
    f.map.move_unit(&mut h, &EeAppMiniMapItem { unit: true, ..building(2, 0x300, 0) }, OFF_MAP, OFF_MAP, 5, 5);
    let mut units = [0u16; 17];
    let mut colors = [0u16; 17];
    units[2] = 0xa;
    colors[2] = 0xb;
    f.map.recolor(&units, &colors);
    assert_eq!(f.map.dots[2 * 64 + 2][0].color, 0x100);
    assert_eq!(f.map.dots[4 * 64 + 4][0].color, 0xb);
    assert_eq!(f.map.units[6 * 64 + 6][0].color, 0xa);
}

#[test]
fn the_redraw_walks_the_diagonals_then_the_edges() {
    // the C++ loops, written out
    let (w, h) = (2, 3);
    let mut expect = Vec::new();
    for i in 0..w {
        let (mut x, mut y) = (i, h + i);
        for _ in 0..h {
            expect.push((x, y));
            x += 1;
            expect.push((x, y));
            y -= 1;
        }
    }
    for x in 0..h {
        expect.push((x, h - 1 - x));
    }
    for i in 0..w {
        expect.push((h + i, i));
    }
    assert_eq!(diagonal_order(w, h), expect);
}

#[test]
fn the_redraw_clears_then_counts_and_adds_every_tile() {
    let mut f = Fixture::new();
    let mut h = Mock::default();
    f.map.counts[7] = 9;
    f.map.width = 1;
    f.map.height = 1;
    // order for 1 x 1: (0, 1), (1, 1), (0, 0), (1, 0)
    h.sight.insert((0, 1), TileSight { seen: true, all: false, visibility: 3, visible: false });
    h.sight.insert((1, 1), TileSight { seen: false, visible: true, ..Default::default() });
    h.items.insert(
        (0, 0),
        vec![building(1, 0x111, 0), EeAppMiniMapItem { unit: true, x: 0, y: 0, ..building(1, 0x0b0, 0) }],
    );
    f.map.redraw(&mut h);
    assert_eq!(h.asked, vec![(0, 1), (1, 1), (0, 0), (1, 0)]);
    assert_eq!(f.map.counts[7], 0);
    // (0, 1): seen, visibility 3: two counts on (from 0: 2 then 3)
    assert_eq!(f.map.counts[2], 3);
    // (1, 1): explored only: on then off (2 then 1)
    assert_eq!(f.map.counts[2 * 64 + 2], 1);
    assert_eq!(f.map.dots[0].len(), 1);
    assert_eq!(f.map.units[0].len(), 1);
    assert_eq!(f.pixel(0, 0), 0x0b0);
    assert_eq!(CELLS, 64 * 64);
}

#[test]
fn pixel_colours_average_the_lit_vertices_or_take_the_nearest() {
    let f = Fixture::new();
    let mut h = Mock::default();
    let light = GeVector { x: 0.0, y: 0.0, z: 1.0 };
    // pixel (5, 5): the vertex (5, 5) only (a span of one); lit: terrain * vertex colour * 1 + ambient, clamped
    let c = f.map.pixel_color(&mut h, light, 5, 5);
    assert_eq!(c, [0.5 * 1.0 + 0.102745101, 0.132549018 + 0.5 * 1.0, (0.188235298f32 + 0.25 * 2.0).min(1.0)]);
    let mut bits = vec![0u16; 8 * 8];
    f.map.redraw_pixels(&mut h, light, &mut bits, 8, 5, 6, 5, 6);
    assert_eq!(bits[8 * 5 + 5], ee_render::ge::pixel::pack_float(7, c[0], c[1], c[2], 1.0) as u16);
    // the nearest vertex unlit, or off the mesh: black
    assert_eq!(f.map.pixel_color(&mut h, light, 2, 2), [0.0, 0.0, 0.0]);
    assert_eq!(f.map.pixel_color(&mut h, light, 20, 20), [0.0, 0.0, 0.0]);
    // a pixel whose span holds its vertex (the map half a point lower): the average; an unlit vertex counts but adds
    // nothing, leaving the ambient
    let mut g = Fixture::new();
    g.map.pixel_to_map.m[1][3] = -1.0;
    assert_eq!(g.map.pixel_color(&mut h, light, 2, 2), [0.102745101, 0.132549018, 0.188235298]);
    assert_eq!(g.map.pixel_color(&mut h, light, 5, 5), c);
}

#[test]
fn the_pixel_rectangle_is_clamped_to_the_texture() {
    let f = Fixture::new();
    assert_eq!(f.map.pixel_rect(-5, 200, 3, 10), [0, 0x80, 3, 10]);
    assert_eq!(f.map.pixel_rect(4, 7, -1, 300), [4, 7, 0, 0x80]);
}
