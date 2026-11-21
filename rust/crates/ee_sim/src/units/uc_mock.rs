//! TEST ONLY: a recording host for the `EEUnitClass` bodies (uc_create.rs, uc_anim.rs, uc_media.rs): each
//! statement asked (op, a, b, c), its answer from `answers` keyed by op (or by (op, a)), sequences popped front first,
//! the floats it writes from `floats`.

use crate::units::uc_anim::UcRegions;
use crate::units::uc_body::UcHost;
use crate::units::uc_create::UcCreateRust;
use crate::units::uc_produce::{FindPlacement, UcRust};
use std::collections::BTreeMap;

#[derive(Default)]
pub struct Mock {
    pub log: Vec<(i32, u64, i64, i64)>,
    /// answers by op (any a)
    pub by_op: BTreeMap<i32, i64>,
    /// answers by (op, a)
    pub by_op_a: BTreeMap<(i32, u64), i64>,
    pub sequences: BTreeMap<i32, Vec<i64>>,
    pub floats: BTreeMap<i32, [f32; 4]>,
    /// the floats each statement was handed
    pub floats_in: Vec<(i32, [f32; 4])>,
    pub placement: bool,
    pub placements: Vec<FindPlacement>,
    pub takes: bool,
    pub escape: Option<[f32; 2]>,
    pub regions: Vec<(bool, u32, u32, u64)>,
    pub region_size: i64,
    pub rings: Vec<(i32, [i32; 4], u64, bool)>,
}

impl Mock {
    pub fn answer(mut self, op: i32, value: i64) -> Self {
        self.by_op.insert(op, value);
        self
    }
    pub fn sequence(mut self, op: i32, values: &[i64]) -> Self {
        self.sequences.insert(op, values.to_vec());
        self
    }
    pub fn float(mut self, op: i32, f: [f32; 4]) -> Self {
        self.floats.insert(op, f);
        self
    }
    pub fn ops(&self) -> Vec<i32> {
        self.log.iter().map(|e| e.0).collect()
    }
    pub fn count(&self, op: i32) -> usize {
        self.log.iter().filter(|e| e.0 == op).count()
    }
    pub fn has(&self, op: i32) -> bool {
        self.count(op) > 0
    }
    /// the position of op's first ask
    pub fn at(&self, op: i32) -> usize {
        self.log.iter().position(|e| e.0 == op).unwrap_or_else(|| panic!("op {op} never asked"))
    }
    pub fn args(&self, op: i32) -> Vec<(u64, i64, i64)> {
        self.log.iter().filter(|e| e.0 == op).map(|e| (e.1, e.2, e.3)).collect()
    }
    pub fn floats_of(&self, op: i32) -> Vec<[f32; 4]> {
        self.floats_in.iter().filter(|e| e.0 == op).map(|e| e.1).collect()
    }
}

impl UcHost for Mock {
    fn uc(&mut self, op: i32, _class: u64, _unit: u64, a: u64, b: i64, c: i64, f: &mut [f32; 4]) -> i64 {
        self.log.push((op, a, b, c));
        self.floats_in.push((op, *f));
        if let Some(out) = self.floats.get(&op) {
            *f = *out;
        }
        if let Some(values) = self.sequences.get_mut(&op) {
            if !values.is_empty() {
                return values.remove(0);
            }
        }
        if let Some(v) = self.by_op_a.get(&(op, a)) {
            return *v;
        }
        *self.by_op.get(&op).unwrap_or(&0)
    }
}

impl UcRust for Mock {
    fn find_placement(&mut self, p: &FindPlacement, m: &mut [[f32; 4]; 4]) -> bool {
        self.placements.push(*p);
        m[2][3] = -1.0;
        self.placement
    }
    fn dock_ship_placement(
        &mut self,
        _point: [f32; 3],
        _class: u64,
        _unit: u64,
        _face: bool,
        _facing_only: bool,
        _m: &mut [[f32; 4]; 4],
    ) -> bool {
        unreachable!("no body here docks")
    }
    fn can_stand_on(&mut self, _x: i32, _y: i32, _unit: u64) -> bool {
        unreachable!("no body here tests standing")
    }
}

impl UcCreateRust for Mock {
    fn block_takes(&mut self, _player: u64, _unit_type: i32, _x: i32, _y: i32, _w: i32, _h: i32) -> bool {
        self.takes
    }
    fn escape_point(&mut self, _unit: u64) -> Option<[f32; 2]> {
        self.escape
    }
    fn chain(&mut self, _class: u64, _unit: u64, count: i32) -> Vec<[f32; 16]> {
        let mut identity = [0.0f32; 16];
        identity[0] = 1.0;
        identity[5] = 1.0;
        identity[10] = 1.0;
        identity[15] = 1.0;
        vec![identity; count.max(0) as usize]
    }
}

impl UcRegions for Mock {
    fn regions_size(&mut self, _table: u64) -> i64 {
        self.region_size
    }
    fn regions_add(&mut self, _table: u64, x: u32, y: u32, unit: u64) {
        self.regions.push((true, x, y, unit));
    }
    fn regions_remove(&mut self, _table: u64, x: u32, y: u32, unit: u64) {
        self.regions.push((false, x, y, unit));
    }
}

impl crate::units::ucb_release::UcbRust for Mock {
    fn block_ring(&mut self, corners: i32, ring: [i32; 4], tiles: u64, append: bool) {
        self.rings.push((corners, ring, tiles, append));
    }
}
