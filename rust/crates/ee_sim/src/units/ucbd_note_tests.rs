use super::*;

#[derive(Default)]
struct World {
    on: bool,
    tick: u32,
    records: Vec<(u32, u32, u64)>,
}

impl UcbdWorld for World {
    fn active(&mut self) -> bool {
        self.on
    }
    fn ticks(&mut self) -> u32 {
        self.tick
    }
    fn record(&mut self, tick: u32, calls: u32, digest: u64) {
        self.records.push((tick, calls, digest));
    }
    fn counts(&mut self, _tick: u32, _kinds: &[u32; 64]) {}
}

#[test]
fn only_the_outermost_note_records_and_a_later_tick_writes_the_record() {
    let mut w = World { on: true, tick: 5, ..Default::default() };
    let outer = begin(&mut w);
    let inner = begin(&mut w);
    assert!(outer && !inner);
    end(&mut w, inner, &[9]);
    end(&mut w, outer, &[1, 2]);
    assert!(w.records.is_empty());
    w.tick = 6;
    let on = begin(&mut w);
    end(&mut w, on, &[3]);
    // the tick 5 record: one call of words 1, 2
    let mut digest = FNV_OFFSET;
    for word in [1u32, 2] {
        for b in 0..4 {
            digest ^= u64::from((word >> (8 * b)) & 0xff);
            digest = digest.wrapping_mul(FNV_PRIME);
        }
    }
    assert_eq!(w.records.last(), Some(&(5, 1, digest)));
}

#[test]
fn nothing_records_without_the_capture() {
    let mut w = World::default();
    let on = begin(&mut w);
    assert!(!on);
    end(&mut w, on, &[1]);
    assert!(w.records.is_empty());
}
