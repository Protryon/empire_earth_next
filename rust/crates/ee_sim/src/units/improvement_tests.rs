//! The improvement record's bodies over a mock `ChangeLevel` and file.

use super::*;
use std::collections::VecDeque;

/// ChangeLevel raises the level and the points (one point a level); the file is a list of words.
struct Mock {
    levels: [i32; 16],
    total: c_long,
    calls: Vec<(u32, i64)>,
    file: VecDeque<i32>,
    written: Vec<i32>,
    fail_at: Option<usize>,
    reads: usize,
    name: i32,
}

impl Mock {
    fn new() -> Box<Mock> {
        Box::new(Mock {
            levels: [1; 16],
            total: 0,
            calls: Vec::new(),
            file: VecDeque::new(),
            written: Vec::new(),
            fail_at: None,
            reads: 0,
            name: 0,
        })
    }
    fn record(&mut self) -> Record {
        Record { levels: self.levels.as_mut_ptr(), total: &mut self.total }
    }
}

impl ImprovementHost for Mock {
    fn call(&mut self, op: i32, index: u32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        match op {
            EE_UNITS_IMPROVEMENT_CHANGE_LEVEL => {
                self.calls.push((index, io.i));
                self.levels[index as usize] += io.i as i32;
                self.total += io.i as c_long;
                0
            }
            EE_UNITS_IMPROVEMENT_NAME_LOAD | EE_UNITS_IMPROVEMENT_NAME_SAVE => self.name,
            EE_UNITS_PLAYER_SAVE_READ_LEVELS => {
                self.reads += 1;
                if self.fail_at == Some(self.reads) {
                    return 7;
                }
                let p = io.i as usize as *mut i32;
                for k in 0..16 {
                    // SAFETY: the body's 16 words
                    unsafe { p.add(k).write(self.file.pop_front().unwrap_or(0)) };
                }
                0
            }
            EE_UNITS_PLAYER_SAVE_READ_LONG => {
                io.i = self.file.pop_front().unwrap_or(0) as i64;
                0
            }
            EE_UNITS_PLAYER_SAVE_WRITE_LEVELS => {
                let p = io.i as usize as *const i32;
                // SAFETY: as above
                self.written.extend((0..16).map(|k| unsafe { p.add(k).read() }));
                0
            }
            EE_UNITS_PLAYER_SAVE_WRITE_LONG => {
                self.written.push(io.i as i32);
                0
            }
            _ => panic!("op {op}"),
        }
    }
}

#[test]
fn reapply_recounts_the_points_from_the_raised_levels() {
    let mut m = Mock::new();
    m.levels[3] = 4;
    m.levels[11] = 2;
    m.total = 99;
    let r = m.record();
    reapply(&mut *m, r);
    assert_eq!(m.calls, vec![(3, 3), (11, 1)]);
    assert_eq!((m.levels[3], m.levels[11], m.total), (4, 2, 4));
}

#[test]
fn copy_levels_raises_the_target_one_level_at_a_time() {
    let mut source = Mock::new();
    source.levels[2] = 3;
    let r = source.record();
    let mut target = Mock::new();
    copy_levels_to(&mut *target, r);
    assert_eq!(target.calls, vec![(2, 1), (2, 1)]);
    assert_eq!(target.levels[2], 3);
}

#[test]
fn a_save_round_trips_the_levels_and_points() {
    let mut m = Mock::new();
    m.levels[5] = 3;
    m.total = 2;
    let r = m.record();
    assert_eq!(upgrade_save(&mut *m, r), 0);
    assert_eq!(m.written.len(), 17);
    let mut n = Mock::new();
    n.file = m.written.iter().copied().collect();
    let r = n.record();
    reset(r);
    // the current version: the levels and the points (read and cleared), then each raised level applied again
    assert_eq!(upgrade_load(&mut *n, r, 0, 0x110), 0);
    assert_eq!(n.calls, vec![(5, 2)]);
    assert_eq!((n.levels[5], n.total), (3, 2));
}

#[test]
fn old_saves_read_the_old_words_and_drop_leadership() {
    let mut m = Mock::new();
    let mut words: Vec<i32> = vec![1; 16];
    words[LEADERSHIP] = 5;
    words[1] = 2;
    m.file = words.iter().copied().chain(std::iter::repeat_n(9, 16)).collect();
    let r = m.record();
    // 0x17 < minor <= 0x6a: the levels, then the 16 old words (that read's status); leadership back to 1
    assert_eq!(load(&mut *m, r, 0, 0x50), 0);
    assert_eq!(m.calls, vec![(1, 1)]);
    assert_eq!(m.levels[LEADERSHIP], 1);
    assert!(m.file.is_empty());
    // up to 0xa0: the levels only, no points read
    let mut m = Mock::new();
    m.file = words.iter().copied().collect();
    m.file.push_back(42);
    let r = m.record();
    assert_eq!(load(&mut *m, r, 0, 0x90), 0);
    assert_eq!(m.file, vec![42]);
    // a failed levels read skips the points and still applies what is there
    let mut m = Mock::new();
    m.fail_at = Some(1);
    m.levels[4] = 3;
    let r = m.record();
    assert_eq!(load(&mut *m, r, 0, 0x110), 7);
    assert_eq!(m.calls, vec![(4, 2)]);
    // a failed name read stops the upgrade's load
    let mut m = Mock::new();
    m.name = 3;
    let r = m.record();
    assert_eq!(upgrade_load(&mut *m, r, 0, 0x110), 3);
    assert_eq!(m.reads, 0);
}
