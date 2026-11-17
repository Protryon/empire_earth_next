use super::*;
use crate::units::player::{EeUnitsPlayerFields, EeUnitsPlayerIo};
use crate::units::player_save::EeUnitsPlayerSaved;
use crate::units::player_stats::EeUnitsPlayerStats;
use crate::units::player_units::EeUnitsPlayerWonders;

/// Two players: this one (id 1) and the other (address 7, id 2) with its stance toward this one; every call but the
/// reads is logged.
struct World {
    other_stance: c_ulong,
    save_vis: bool,
    viewed_other: bool,
    current: i64,
    log: Vec<String>,
}

impl PlayerHost for World {
    fn call(&mut self, op: i32, io: &mut EeUnitsPlayerIo) -> i64 {
        let a = io.i;
        match op {
            EE_UNITS_PLAYER_VIEW => {
                let other = a[0] == 7;
                io.i[0] = if other { 2 } else { 1 };
                io.i[1] = i64::from(self.save_vis);
                io.i[2] = if other { self.other_stance as i64 } else { 0 };
                0
            }
            EE_UNITS_PLAYER_VIEWED_OF => i64::from(self.viewed_other),
            EE_UNITS_PLAYER_IS_CURRENT => i64::from(a[0] == self.current),
            _ => {
                self.log.push(format!("{op} {} {} {}", a[0], a[1], a[2]));
                0
            }
        }
    }
}

struct Records {
    saved: Box<EeUnitsPlayerSaved>,
    fields: Box<EeUnitsPlayerFields>,
    wonders: Box<EeUnitsPlayerWonders>,
    stats: EeUnitsPlayerStats,
}

impl Records {
    fn new() -> Self {
        let mut r = Records {
            saved: Box::default(),
            fields: Box::default(),
            wonders: Box::default(),
            stats: EeUnitsPlayerStats::default(),
        };
        r.fields.mWonderState = &mut *r.wonders;
        r.saved.mEnemies = [1; 17];
        r
    }
    fn run(&mut self, w: &mut World, body: impl FnOnce(&mut Player<World>)) {
        let mut p = Player { h: w, saved: &mut *self.saved, fields: &mut *self.fields, stats: &self.stats };
        body(&mut p)
    }
}

fn world(other_stance: c_ulong) -> World {
    World { other_stance, save_vis: true, viewed_other: false, current: 0, log: Vec::new() }
}

#[test]
fn a_mutual_alliance_moves_both_players_sight_and_lists_the_allies() {
    let mut w = world(0);
    let mut r = Records::new();
    r.wonders.powers = vec![5, 3, 5];
    r.run(&mut w, |p| p.set_diplomatic_stance(7, 0, true));
    assert_eq!(r.saved.mEnemies[2], 0);
    assert_eq!(
        w.log,
        [
            "68 7 0 0", // the enemy sight power, once per holding
            "68 7 0 0", "76 7 0 0", // the wonder timers
            "72 0 0 0", "73 0 0 0", "72 7 0 0", "73 7 0 0", "74 0 7 1", "74 7 0 1",
            "75 0 0 0", // the store's table at the alliance
            "72 0 1 0", "73 0 1 0", "72 7 1 0", "73 7 1 0",
            "81 0 0 0", // this player is the local one: its terrain
            "75 0 0 0", "67 1 0 0", "67 0 0 0", "82 0 0 0",
        ]
    );
}

#[test]
fn a_one_sided_alliance_only_changes_the_stance() {
    let mut w = world(1);
    w.current = 99;
    let mut r = Records::new();
    r.run(&mut w, |p| p.set_diplomatic_stance(7, 0, true));
    assert_eq!(r.saved.mEnemies[2], 0);
    assert_eq!(w.log, ["75 0 0 0", "67 1 0 0", "67 0 0 0", "82 0 0 0"]);
    // without a map the ally is listed at once
    let mut w = world(1);
    w.current = 99;
    let mut r = Records::new();
    r.run(&mut w, |p| p.set_diplomatic_stance(7, 0, false));
    assert_eq!(w.log[0], "74 0 7 1");
    // the same stance: nothing
    let mut w = world(1);
    r.run(&mut w, |p| p.set_diplomatic_stance(7, 0, false));
    assert!(w.log.is_empty());
}

#[test]
fn breaking_a_mutual_alliance_drops_the_allies_and_the_viewed_unit() {
    let mut w = world(0);
    w.viewed_other = true;
    w.current = 7;
    let mut r = Records::new();
    r.saved.mEnemies[2] = 0;
    r.wonders.powers = vec![5];
    r.run(&mut w, |p| p.set_diplomatic_stance(7, 2, true));
    assert_eq!(r.saved.mEnemies[2], 2);
    assert_eq!(
        w.log,
        [
            "68 7 1 0", "72 0 0 0", "72 7 0 0", "74 0 7 0", "74 7 0 0", "75 0 0 0", "72 0 1 0", "72 7 1 0", "79 0 0 0",
            "81 0 0 0", // the other player is the local one
            "75 0 0 0", "67 1 0 0", "67 0 0 0", "82 0 0 0",
        ]
    );
}

#[test]
fn without_the_saved_visibility_only_the_stance_changes() {
    let mut w = world(0);
    w.save_vis = false;
    w.current = 99;
    let mut r = Records::new();
    r.run(&mut w, |p| p.set_diplomatic_stance(7, 0, true));
    assert_eq!(w.log, ["75 0 0 0", "67 1 0 0", "67 0 0 0", "82 0 0 0"]);
}

#[test]
fn sight_sharers_and_the_shared_line_of_sight() {
    let mut w = world(0);
    let mut r = Records::new();
    r.run(&mut w, |p| {
        p.sight_sharer(7, true);
        p.sight_sharer(7, false);
    });
    assert_eq!(w.log, ["72 0 0 1", "77 7 1 0", "72 0 1 1", "72 0 0 1", "77 7 0 0", "72 0 1 1"]);
    w.log.clear();
    r.saved.mSharedLOS = true;
    r.run(&mut w, |p| {
        p.set_shared_los(true);
        p.set_shared_los(false);
    });
    assert!(!r.saved.mSharedLOS);
    assert_eq!(w.log, ["72 0 0 0", "73 0 0 0", "72 0 1 0", "73 0 1 0"]);
}
