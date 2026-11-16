use super::*;

/// (op, source, location, kind)
type Op = (i32, (c_long, c_long), (c_long, c_long), c_long);

/// A host recording each operation with the event's (source, location, kind).
struct Log {
    player: bool,
    local: bool,
    playing: bool,
    ops: Vec<Op>,
}

impl Log {
    fn new() -> Self {
        Log { player: true, local: true, playing: false, ops: Vec::new() }
    }
    fn kinds(&self) -> Vec<i32> {
        self.ops.iter().map(|o| o.0).collect()
    }
}

impl EventHost for Log {
    fn has_player(&self) -> bool {
        self.player
    }
    fn op(&mut self, op: i32, e: &EeUnitsPlayerEvent) -> i32 {
        self.ops.push((op, (e.source_x, e.source_y), (e.x, e.y), e.kind));
        match op {
            EE_UNITS_EVENTS_IS_LOCAL => i32::from(self.local),
            EE_UNITS_EVENTS_PLAYING => i32::from(self.playing),
            _ => 0,
        }
    }
}

const NONE: (c_long, c_long) = (-10000, -10000);

#[test]
fn an_attack_shares_its_location_and_is_announced_with_a_ping() {
    let mut l = EeUnitsPlayerEvents::default();
    let mut h = Log::new();
    l.add(&mut h, (10, 10), (12, 13), 1, -1, false);
    assert_eq!(
        h.kinds(),
        [
            EE_UNITS_EVENTS_SHOW,
            EE_UNITS_EVENTS_IS_LOCAL,
            EE_UNITS_EVENTS_STOP_PING,
            EE_UNITS_EVENTS_IS_LOCAL,
            EE_UNITS_EVENTS_PLAYING,
            EE_UNITS_EVENTS_SOUND,
            EE_UNITS_EVENTS_PING
        ]
    );
    assert_eq!(h.ops[0].2, (12, 13));
    assert!(l.events[0].1.active);
    assert_eq!(l.events[0].1.time, 15000.0);
    assert_eq!(l.ping, 10000.0);
    assert_eq!(l.current, Some(l.events[0].0));
}

#[test]
fn a_garrison_message_shows_its_text_and_a_flare_pings_longer_without_a_sound_while_one_plays() {
    let mut l = EeUnitsPlayerEvents::default();
    let mut h = Log::new();
    h.playing = true;
    l.add(&mut h, (1, 1), NONE, 2, 0x274e, false);
    assert_eq!(*h.kinds().last().unwrap(), EE_UNITS_EVENTS_TEXT);
    assert!(!h.kinds().contains(&EE_UNITS_EVENTS_SOUND));
    assert_eq!(l.events[0].1.time, 5000.0);
    l.add(&mut h, (50, 50), NONE, 3, -1, true);
    assert_eq!(l.ping, 20000.0);
    // not the local player: nothing announced
    let mut other = Log::new();
    other.local = false;
    l.add(&mut other, (90, 90), NONE, 3, -1, true);
    assert_eq!(other.kinds(), [EE_UNITS_EVENTS_IS_LOCAL, EE_UNITS_EVENTS_IS_LOCAL]);
}

#[test]
fn a_nearby_event_of_the_type_keeps_the_new_one_silently_until_it_has_expired() {
    let mut l = EeUnitsPlayerEvents::default();
    let mut h = Log::new();
    l.add(&mut h, (10, 10), NONE, 2, 5, false);
    h.ops.clear();
    // within squared 110 (7, 7 away: 98): made, the old one still running
    l.add(&mut h, (17, 17), NONE, 2, 6, false);
    assert_eq!(l.events.len(), 2);
    assert!(h.kinds().is_empty(), "a type 2 event makes no world call and the fold announces nothing");
    // the old one was the current one: the new one is
    assert_eq!(l.current, Some(l.events[0].0));
    // the nearest match (the newest, at the same tile) expired: dropped, the new one current and announced
    l.events[0].1.time = 0.0;
    l.add(&mut h, (17, 17), NONE, 2, 7, false);
    assert_eq!(l.events.len(), 2);
    assert_eq!(l.events[0].1.text, 7);
    assert_eq!(l.current, Some(l.events[0].0));
    assert_eq!(*h.kinds().last().unwrap(), EE_UNITS_EVENTS_TEXT);
    // further than squared 110: a new event (8, 7 away: 113)
    l.add(&mut h, (25, 24), NONE, 2, 8, false);
    assert_eq!(l.events.len(), 3);
}

#[test]
fn an_attack_event_stops_sharing_five_seconds_in_and_at_its_end() {
    let mut l = EeUnitsPlayerEvents::default();
    let mut h = Log::new();
    l.add(&mut h, (1, 1), (2, 2), 1, -1, true);
    h.ops.clear();
    l.tick(&mut h, 4000.0);
    assert!(l.events[0].1.active);
    l.tick(&mut h, 1500.0);
    assert!(!l.events[0].1.active);
    assert!(h.kinds().contains(&EE_UNITS_EVENTS_HIDE));
    assert_eq!(l.events[0].1.time, 9500.0);
    assert!(!h.kinds().contains(&EE_UNITS_EVENTS_STOP_PING));
    // the ping runs out (10 s): the local ping off
    l.tick(&mut h, 5000.0);
    assert!(h.kinds().contains(&EE_UNITS_EVENTS_STOP_PING));
}

#[test]
fn expired_events_past_fifteen_are_trimmed_from_the_back() {
    let mut l = EeUnitsPlayerEvents::default();
    let mut h = Log::new();
    for i in 0..18 {
        l.add(&mut h, (i * 100, 0), NONE, 3, -1, true);
    }
    l.events[17].1.time = -1.0;
    l.events[16].1.time = -1.0;
    l.events[14].1.time = -1.0;
    l.tick(&mut h, 0.0);
    // 17 and 16 dropped, 15 still running stops the trim
    assert_eq!(l.events.len(), 16);
}

#[test]
fn the_next_event_is_the_current_one_then_the_newest_in_turn() {
    let mut l = EeUnitsPlayerEvents::default();
    let mut h = Log::new();
    assert_eq!(l.next_event(), None);
    l.add(&mut h, (1, 0), NONE, 3, -1, true);
    l.add(&mut h, (2, 0), NONE, 3, -1, true);
    l.add(&mut h, (3, 0), NONE, 3, -1, true);
    // the current (the newest), then index 1, 2, then 0 again
    let order: Vec<c_long> = (0..4).map(|_| l.next_event().unwrap().source_x).collect();
    assert_eq!(order, [3, 2, 1, 3]);
    assert_eq!(l.current, None);
}

#[test]
fn hide_show_clear_and_repair() {
    let mut l = EeUnitsPlayerEvents::default();
    let mut h = Log::new();
    l.add(&mut h, (1, 1), (5, 5), 1, -1, true);
    l.add(&mut h, (2, 2), NONE, 2, -1, true);
    h.ops.clear();
    l.show(&mut h, false);
    assert_eq!(h.kinds(), [EE_UNITS_EVENTS_HIDE]);
    l.show(&mut h, true);
    assert_eq!(h.kinds(), [EE_UNITS_EVENTS_HIDE, EE_UNITS_EVENTS_SHOW]);
    l.repair(&mut h);
    assert_eq!(h.kinds().len(), 3);
    // the destructor's clear has no player: the events dropped, no world call
    let mut none = Log::new();
    none.player = false;
    l.clear(&mut none);
    assert!(l.events.is_empty() && none.ops.is_empty() && l.current.is_none());
}

/// A file of words (as player_stats_tests.rs).
#[derive(Default)]
struct File {
    words: Vec<i64>,
    at: usize,
}

impl File {
    fn prim(&mut self, op: i32, io: &mut EeUnitsPlayerSaveIo) -> i32 {
        match op {
            EE_UNITS_PLAYER_SAVE_WRITE_ULONG | EE_UNITS_PLAYER_SAVE_WRITE_LONG => self.words.push(io.i),
            EE_UNITS_PLAYER_SAVE_WRITE_FLOAT => self.words.push(i64::from(io.f.to_bits())),
            EE_UNITS_PLAYER_SAVE_WRITE_BOOL => self.words.push(i64::from(io.b)),
            _ => {
                let Some(&w) = self.words.get(self.at) else { return 5 };
                self.at += 1;
                match op {
                    EE_UNITS_PLAYER_SAVE_READ_FLOAT => io.f = f32::from_bits(w as u32),
                    EE_UNITS_PLAYER_SAVE_READ_BOOL => io.b = w as u8,
                    _ => io.i = w,
                }
            }
        }
        0
    }
}

#[test]
fn a_save_loads_back_equal() {
    let mut l = EeUnitsPlayerEvents::default();
    let mut h = Log::new();
    l.add(&mut h, (1, 2), (3, 4), 1, -1, true);
    l.add(&mut h, (5, 6), NONE, 2, 77, true);
    let mut f = File::default();
    assert_eq!(save(&l, |op, io| f.prim(op, io)), 0);
    assert_eq!(f.words.len(), 1 + 2 * 8);
    let mut m = EeUnitsPlayerEvents::default();
    assert_eq!(load(&mut m, |op, io| f.prim(op, io)), 0);
    let a: Vec<_> = l.events.iter().map(|x| x.1).collect();
    let b: Vec<_> = m.events.iter().map(|x| x.1).collect();
    assert_eq!(a, b);
    // a short file: the event pushed with what was read
    let mut g = File { words: vec![1, 9, 8, 1], at: 0 };
    let mut n = EeUnitsPlayerEvents::default();
    assert_eq!(load(&mut n, |op, io| g.prim(op, io)), 0);
    assert_eq!(n.events[0].1, EeUnitsPlayerEvent { source_x: 9, source_y: 8, kind: 1, ..Default::default() });
}

#[test]
fn the_digest_follows_the_captures_words() {
    let mut l = EeUnitsPlayerEvents::default();
    let empty = l.digest();
    let mut h = Log::new();
    l.add(&mut h, (1, 2), NONE, 3, -1, true);
    assert_ne!(l.digest(), empty);
    // an empty list: the count 0, the current -1, the index 0, the ping 0.0
    let mut x: u64 = 0xcbf29ce484222325;
    for w in [0u32, u32::MAX, 0, 0] {
        for b in w.to_le_bytes() {
            x = (x ^ u64::from(b)).wrapping_mul(0x100000001b3);
        }
    }
    assert_eq!(empty, x);
}
