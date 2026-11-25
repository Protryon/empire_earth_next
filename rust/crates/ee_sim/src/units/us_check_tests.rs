use super::*;
use std::ffi::c_void;

/// A world of one player (handle 100, id 1) with units 11, 12 listed, the chain 11 -> 12, every checksum 3; the store
/// agrees unless told otherwise.
#[derive(Default)]
struct Mock {
    log: Vec<i32>,
    filled: Vec<(u64, i64, u64)>,
    records: Vec<Record>,
    off_map: u32,
    store_flags: (u64, u64),
}

impl UsHost for Mock {
    fn us(&mut self, op: i32, a: u64, b: i64, c: u64, out: *mut c_void) -> i64 {
        self.log.push(op);
        match op {
            EE_UNITS_US_PLAYER => i64::from(b == 1) * 100,
            EE_UNITS_US_PLAYER_ID => 1,
            EE_UNITS_US_PLAYER_RTS => 104,
            EE_UNITS_US_FIRST_UNIT | EE_UNITS_US_CHAIN_FIRST => 11,
            EE_UNITS_US_NEXT_UNIT | EE_UNITS_US_CHAIN_NEXT => i64::from(a == 11) * 12,
            EE_UNITS_US_FILL => {
                self.filled.push((a, b, c));
                // SAFETY: the check's record
                unsafe { (*out.cast::<EeUnitRecord>()).id = a as u32 };
                0
            }
            EE_UNITS_US_UNIT_ID => a as i64,
            EE_UNITS_US_CHECKSUM => 3,
            crate::units::us_fill::EE_UNITS_US_B_ENEMY => 9 * i64::from(b == 0),
            EE_UNITS_US_ORDER_COUNT => 2,
            EE_UNITS_US_ORDER_ID => 11 + b,
            _ => 0,
        }
    }
    fn store_check(&mut self, objects: &[EeUnitRecord]) -> EeUnitStoreCheck {
        EeUnitStoreCheck { compared: objects.len() as u32, ..Default::default() }
    }
    fn store_unlisted(&mut self) -> Vec<u64> {
        vec![]
    }
    fn store_check_unlisted(&mut self, _objects: &[EeUnitRecord], _check: &mut EeUnitStoreCheck) -> u32 {
        0
    }
    fn store_check_on_map(&mut self, ids: &[u32], objects: &[u64]) -> u32 {
        assert_eq!(ids, &[11, 12]);
        assert_eq!(objects, &[11, 12]);
        self.off_map
    }
    fn store_checksum_flags(&mut self) -> (u64, u64) {
        self.store_flags
    }
    fn player_check(&mut self, objects: &[u64], ids: &[u32], enemies: &[u32]) -> u32 {
        assert_eq!((objects, ids, enemies.len(), enemies[0]), (&[104u64][..], &[1u32][..], 17, 9));
        0
    }
    fn order_check(&mut self, ids: &[u32]) -> (u32, u32) {
        assert_eq!(ids, &[11, 12]);
        (0, 0)
    }
    fn order_count(&mut self) -> u32 {
        2
    }
    fn record(&mut self, r: &Record) {
        self.records.push(r.clone());
    }
}

#[test]
fn the_usck_tag_is_the_combat_tags_order() {
    assert_eq!(USCK, 0x4b43_5355);
}

#[test]
fn a_consistent_world_records_usck_and_goes_on() {
    let mut h = Mock { store_flags: (6, 6), ..Default::default() };
    assert_eq!(check(&mut h, 7), 2);
    assert_eq!(h.filled, vec![(11, 1, 104), (12, 1, 104)]);
    assert_eq!(h.records.len(), 1);
    assert_eq!(h.records[0].tag, USCK);
    assert_eq!(h.records[0].data, [7u32, 2, 0].iter().flat_map(|v| v.to_le_bytes()).collect::<Vec<u8>>());
    assert!(!h.log.contains(&EE_UNITS_US_EXIT));
}

#[test]
fn a_checksum_difference_stops_the_run() {
    let mut h = Mock { store_flags: (6, 5), ..Default::default() };
    check(&mut h, 1);
    assert_eq!(h.records[0].data[8], 1);
    assert_eq!(*h.log.last().unwrap(), EE_UNITS_US_EXIT);
}
