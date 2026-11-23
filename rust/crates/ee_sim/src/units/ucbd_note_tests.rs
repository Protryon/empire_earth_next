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

/// A world of three units (ids 100 + handle), class 7 of type 0x42, a two-waypoint path, a location goal.
struct Notes;

impl NoteWorld for Notes {
    fn unit_id(&mut self, unit: u64) -> u32 {
        100 + unit as u32
    }
    fn class_type(&mut self, class: u64) -> u32 {
        if class == 7 {
            0x42
        } else {
            0
        }
    }
    fn path(&mut self, _unit: u64) -> Vec<[f32; 5]> {
        vec![[1.0, 2.0, 3.0, 4.0, 5.0], [6.0, 7.0, 8.0, 9.0, 10.0]]
    }
    fn location(&mut self, _unit: u64) -> [f32; 3] {
        [0.5, 1.5, 2.5]
    }
    fn goal(&mut self, unit: u64) -> Option<(u8, Option<[f32; 3]>)> {
        match unit {
            1 => Some((1, Some([3.0, 4.0, 0.0]))),
            2 => Some((6, None)),
            _ => None,
        }
    }
    fn tiles(&mut self, _v: u64) -> Vec<[i32; 2]> {
        vec![[3, 4], [-1, 5]]
    }
    fn units(&mut self, _v: u64) -> Vec<u64> {
        vec![1, 0, 3]
    }
    fn home_base(&mut self, unit: u64) -> u64 {
        unit + 1
    }
    fn resource(&mut self, _unit: u64) -> (i32, u32) {
        (250, 2)
    }
}

fn on(kind: u32) -> Note {
    let mut n = Note { on: true, words: Vec::new() };
    n.i(i64::from(kind));
    n
}

fn bits(v: f32) -> u32 {
    v.to_bits()
}

#[test]
fn a_note_writes_the_wrappers_words_in_order() {
    let mut n = on(10);
    n.c(&mut Notes, 7).u(&mut Notes, 2).u(&mut Notes, 0).c(&mut Notes, 0).i(-3).f(1.5).t([2, -4]);
    assert_eq!(
        n.words(),
        [10, 0x42, 102, (-2i32) as u32, (-2i32) as u32, (-3i32) as u32, bits(1.5), 2, (-4i32) as u32]
    );
    let mut n = on(4);
    n.ts(&mut Notes, 9).us(&mut Notes, 9).home(&mut Notes, 2).resource(&mut Notes, 1);
    assert_eq!(n.words(), [4, 2, 3, 4, (-1i32) as u32, 5, 3, 101, (-2i32) as u32, 103, 103, 250, 2]);
}

#[test]
fn a_units_state_is_its_path_location_goal_and_point() {
    let mut n = on(19);
    n.s(&mut Notes, 1);
    let mut want = vec![19, 2];
    want.extend([1.0f32, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 0.5, 1.5, 2.5].map(bits));
    want.push(1);
    want.extend([3.0f32, 4.0, 0.0].map(bits));
    assert_eq!(n.words(), want);
    let mut n = on(19);
    n.s(&mut Notes, 2);
    assert_eq!(*n.words().last().unwrap(), 6);
    let mut n = on(19);
    n.s(&mut Notes, 3);
    assert_eq!(*n.words().last().unwrap(), (-1i32) as u32);
    let mut n = on(19);
    n.s(&mut Notes, 0);
    assert_eq!(n.words(), [19, (-2i32) as u32]);
}

#[test]
fn an_off_note_reads_nothing_and_keeps_no_words() {
    let mut n = Note { on: false, words: Vec::new() };
    n.i(1).c(&mut Notes, 7).u(&mut Notes, 1).s(&mut Notes, 1).ts(&mut Notes, 1).us(&mut Notes, 1);
    assert!(n.words().is_empty() && !n.is_on());
}
