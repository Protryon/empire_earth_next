//! The herd over a fake world: the units by address, a scripted random stream, the calls recorded.

use std::collections::BTreeMap;

use ee_data::tables::EeDataAnimalFields;

use super::file::test_file;
use super::herd::*;

const HERD: u64 = 0x5000;
const PLAYER: u64 = 0x7000;
const ANIMAL: u64 = 0x9000;

#[derive(Default)]
struct World {
    units: BTreeMap<u64, EeHerdUnit>,
    refs: BTreeMap<u64, i64>,
    /// the draws `RANDOM_LONG` hands out in turn (then the minimum)
    draws: Vec<i64>,
    /// every long draw asked: (min, max)
    asked: Vec<(i64, i64)>,
    animal: Box<EeDataAnimalFields>,
    tiles: i64,
    hull: Vec<(i64, i64)>,
    media: BTreeMap<u64, i64>,
    off_map: bool,
    /// the goals given: (op, unit, target or the slot's x bits)
    goals: Vec<(i32, u64, u64)>,
    fled: Vec<u64>,
    generate: f32,
    /// the record FIELDS_OF hands out
    fields_of: u64,
    /// the formation's slots (x, y, z, heading)
    slots: Vec<[f32; 4]>,
}

impl World {
    fn new() -> World {
        let animal = EeDataAnimalFields {
            mAnimalID: 3,
            mHerdRadius: 6,
            mMinHerdSize: 4,
            mMaxHerdSize: 9,
            mMinBirthInterval: 100,
            mMaxBirthInterval: 200,
            mMinGrazeTime: 10,
            mMaxGrazeTime: 20,
            mMinTerritoryMargin: 2,
            mMaxTerritoryMargin: 6,
            mBreeds: true,
            ..Default::default()
        };
        World { animal: Box::new(animal), ..Default::default() }
    }
    fn unit(&mut self, p: u64, id: u32, x: i32, y: i32, hp: i32) {
        let u = EeHerdUnit {
            unit_id: id as _,
            handle: id,
            tile_x: x as _,
            tile_y: y as _,
            hit_points: hp as _,
            growth: 1.0,
            player: PLAYER,
            animal_id: 3,
            initialized: true,
            ..Default::default()
        };
        self.units.insert(p, u);
    }
}

impl HerdHost for World {
    fn this(&self) -> u64 {
        HERD
    }
    fn call(&mut self, op: i32, a: u64, b: u64, io: &mut EeHerdIo) -> i64 {
        match op {
            EE_CP_HERD_UNIT => {
                io.unit = self.units[&a];
                0
            }
            EE_CP_HERD_RESOLVE => {
                self.units.iter().find(|(_, u)| u.handle == io.v[0] as u32).map_or(0, |(&p, _)| p as i64)
            }
            EE_CP_HERD_REF => {
                *self.refs.entry(a).or_default() += 1;
                0
            }
            EE_CP_HERD_DEREF => {
                *self.refs.entry(a).or_default() -= 1;
                0
            }
            EE_CP_HERD_SET_HERD => {
                self.units.get_mut(&a).unwrap().herd = b;
                0
            }
            EE_CP_HERD_SET_GRAZE => {
                self.units.get_mut(&a).unwrap().graze_time = io.v[0] as _;
                0
            }
            EE_CP_HERD_RANDOM_LONG => {
                self.asked.push((io.v[0], io.v[1]));
                if self.draws.is_empty() {
                    io.v[0]
                } else {
                    self.draws.remove(0)
                }
            }
            EE_CP_HERD_GAME_TIME => 1000,
            EE_CP_HERD_GENERATE => {
                io.f[0] = self.generate;
                0
            }
            EE_CP_HERD_FIELDS_OF => self.fields_of as i64,
            EE_CP_HERD_DAMAGE_FACTOR => {
                io.f[0] = 1.0;
                0
            }
            EE_CP_HERD_FLEE => {
                self.fled.push(a);
                0
            }
            EE_CP_HERD_CLEAR_ATTACKER => {
                self.units.get_mut(&a).unwrap().last_attacker = 0;
                0
            }
            EE_CP_HERD_GOAL_ATTACK => {
                self.goals.push((op, a, b));
                1
            }
            EE_CP_HERD_GOAL_MOVE => {
                self.goals.push((op, a, u64::from(io.f[0].to_bits())));
                1
            }
            EE_CP_HERD_POSITION => {
                let u = self.units[&a];
                io.f[0] = u.tile_x as f32;
                io.f[1] = u.tile_y as f32;
                0
            }
            EE_CP_HERD_FORMATION => {
                // SAFETY: the operation's units and lists
                let units = unsafe { std::slice::from_raw_parts(io.input, io.input_len) };
                for &u in units {
                    // SAFETY: as above
                    unsafe { ee_cp_herd_push(io.list2, u as i64, 0, 0, 0) };
                }
                for s in &self.slots {
                    let b = |f: f32| i64::from(f.to_bits());
                    // SAFETY: as above
                    unsafe { ee_cp_herd_push(io.list, b(s[0]), b(s[1]), b(s[2]), b(s[3])) };
                }
                0
            }
            EE_CP_HERD_CONTINENT_OF | EE_CP_HERD_CONTINENT_AT => 1,
            EE_CP_HERD_ANIMAL => ANIMAL as i64,
            EE_CP_HERD_ANIMAL_FIELDS => &*self.animal as *const EeDataAnimalFields as i64,
            EE_CP_HERD_ANIMAL_ON_MAP => 1,
            EE_CP_HERD_NUM_TILES => self.tiles,
            EE_CP_HERD_AREA_CREATE => {
                // SAFETY: the herd hands its tiles' pairs
                let flat = unsafe { std::slice::from_raw_parts(io.input as *const i64, io.input_len * 2) };
                self.hull = flat.chunks(2).map(|c| (c[0], c[1])).collect();
                self.tiles = io.input_len as i64;
                0
            }
            EE_CP_HERD_HULL_BOUNDS => {
                let xs = self.hull.iter().map(|t| t.0);
                let ys = self.hull.iter().map(|t| t.1);
                let (l, r) = (xs.clone().min().unwrap_or(0), xs.max().unwrap_or(0));
                let (t, b) = (ys.clone().min().unwrap_or(0), ys.max().unwrap_or(0));
                io.v[..4].copy_from_slice(&[b, l, r, t]);
                0
            }
            EE_CP_HERD_MEDIA_LOAD => {
                *self.media.entry(a).or_default() += 1;
                0
            }
            EE_CP_HERD_MEDIA_UNLOAD => {
                *self.media.entry(a).or_default() -= 1;
                0
            }
            EE_CP_HERD_PHYSICAL => {
                let u = self.units[&a];
                io.f[0] = u.tile_x as f32 + 0.5;
                io.f[1] = u.tile_y as f32 + 0.5;
                0
            }
            EE_CP_HERD_MAP_TILE => i64::from(!self.off_map),
            EE_CP_HERD_PLAYER_ID => 4,
            EE_CP_HERD_SAVE_FLAGS => 3,
            EE_CP_HERD_TILES => {
                for &(x, y) in &self.hull {
                    // SAFETY: the operation's list
                    unsafe { ee_cp_herd_push(io.list, x, y, 0, 0) };
                }
                0
            }
            _ => 0,
        }
    }
}

fn record() -> EeHerdFields {
    EeHerdFields {
        mBounds2: EeHerdRect::default(),
        mP94: EeHerdPoint::default(),
        mCenter: EeHerdPoint::default(),
        mAnimal: 0,
        mPathRequest: 0,
        mPlayer: 0,
        mLeader: EE_CP_HERD_NONE,
        mTarget: EE_CP_HERD_NONE,
        mFB8: 0.0,
        mAnimalID: 0,
        mGrazeAreaID: 0,
        mState: 0,
        mLC8: 0,
        mBirthTimer: 0,
        mFollowTime: 0,
        mNextUpdateTime: 0,
        mLD8: 0,
        mChaseTime: 0,
        mHuntTime: 0,
        mLE4: 0,
        mRadiusSquared: 0,
        mNumYoung: 0,
        mMaxUnits: 0,
        mContinent: 0,
        mID: 0,
        mBreeds: false,
        mIsActive: false,
        mLists: new_lists(),
    }
}

fn units_of(f: &EeHerdFields) -> Vec<u32> {
    // SAFETY: the test's record
    unsafe { (*f.mLists).units.to_vec() }
}

fn free(f: &EeHerdFields) {
    // SAFETY: the test's record, dropped once
    unsafe { free_lists(f.mLists) };
}

/// A herd of the given units made by the constructor (draws at their minimum unless scripted).
fn herd_of(w: &mut World, units: &[u64]) -> EeHerdFields {
    let mut f = record();
    // SAFETY: f outlives the herd value
    let mut h = unsafe { Herd::new(w, &mut f) };
    h.construct(PLAYER, 3, 1);
    for &u in units {
        h.add_unit(u);
    }
    f
}

#[test]
fn construct_draws_the_birth_timer_then_the_size() {
    let mut w = World::new();
    w.draws = vec![150, 7];
    let f = herd_of(&mut w, &[]);
    assert_eq!(w.asked, vec![(100, 200), (4, 9)]);
    assert_eq!((f.mBirthTimer, f.mMaxUnits, f.mRadiusSquared, f.mID), (150, 7, 36, 1));
    assert_eq!((f.mAnimal, f.mPlayer, f.mAnimalID, f.mIsActive), (ANIMAL, PLAYER, 3, true));
    free(&f);
}

#[test]
fn units_join_lead_and_leave() {
    let mut w = World::new();
    w.unit(0x100, 11, 10, 10, 40);
    w.unit(0x200, 12, 12, 11, 90);
    let mut f = herd_of(&mut w, &[0x100, 0x200]);
    assert_eq!(units_of(&f), vec![11, 12]);
    // the second, stronger animal leads (referenced twice: the herd, the leader)
    assert_eq!(f.mLeader, 12);
    assert_eq!((w.refs[&0x100], w.refs[&0x200]), (1, 2));
    assert_eq!(w.units[&0x100].herd, HERD);
    // the graze time: the clock plus the draw (the minimum here)
    assert_eq!(w.units[&0x100].graze_time, 1010);
    // the area from the units' tiles, from the last
    assert_eq!(w.hull, vec![(12, 11), (10, 10)]);
    // SAFETY: f outlives the herd value
    let mut h = unsafe { Herd::new(&mut w, &mut f) };
    assert!(h.has_unit(0x200));
    assert!(h.remove_unit(0x200));
    assert!(!h.remove_unit(0x200));
    assert!(!h.has_unit(0x200));
    assert_eq!(units_of(&f), vec![11]);
    assert_eq!(f.mLeader, EE_CP_HERD_NONE);
    assert_eq!(w.refs[&0x200], 0);
    assert_eq!(w.units[&0x200].herd, 0);
    // every look loaded was unloaded but the one of the unit still in the herd
    assert_eq!(w.media.values().sum::<i64>(), 1);
    free(&f);
}

#[test]
fn the_territory_grows_by_a_margin() {
    let mut w = World::new();
    let mut f = herd_of(&mut w, &[]);
    w.draws = vec![3];
    // SAFETY: f outlives the herd value
    unsafe { Herd::new(&mut w, &mut f) }.create(&[EeHerdPoint { mX: 10, mY: 20 }, EeHerdPoint { mX: 14, mY: 22 }]);
    // the margin drawn from ((6 - 2) / 2, 6), then the corners pushed out by it
    assert_eq!(w.asked.last(), Some(&(2, 6)));
    assert_eq!(f.mBounds2, EeHerdRect { mBottom: 25, mLeft: 7, mRight: 17, mTop: 17 });
    free(&f);
}

#[test]
fn save_and_load_keep_the_record() {
    let mut w = World::new();
    w.unit(0x100, 11, 10, 10, 40);
    let mut f = herd_of(&mut w, &[0x100]);
    f.mState = 7;
    f.mCenter = EeHerdPoint { mX: 3, mY: 4 };
    f.mFB8 = 1.5;
    let mut bytes = test_file::Bytes::default();
    let mut file = test_file::file(&mut bytes);
    // SAFETY: f outlives the herd value
    assert_eq!(unsafe { Herd::new(&mut w, &mut f) }.save(0, 300, &mut file), 0);
    let mut g = record();
    bytes.at = 0;
    let mut file = test_file::file(&mut bytes);
    // SAFETY: g outlives the herd value
    assert_eq!(unsafe { Herd::new(&mut w, &mut g) }.load(0, 300, &mut file), 0);
    assert_eq!(bytes.at, bytes.data.len());
    // the animal and player as their saved ids, the leader as its unit id
    assert_eq!((g.mAnimal, g.mPlayer, g.mLeader, g.mTarget), (3, 4, 11, EE_CP_HERD_NONE));
    assert_eq!((g.mState, g.mCenter, g.mFB8, g.mID), (7, EeHerdPoint { mX: 3, mY: 4 }, 1.5, 1));
    assert_eq!((g.mBounds2, g.mMaxUnits, g.mBreeds), (f.mBounds2, f.mMaxUnits, f.mBreeds));
    assert_eq!(units_of(&g), vec![11]);
    free(&f);
    free(&g);
}

#[test]
fn a_shuffle_draws_once_a_position_from_the_second() {
    let mut w = World::new();
    for i in 0..4u32 {
        w.unit(0x100 * u64::from(i + 1), 20 + i, 10, 10, 40);
    }
    let mut f = herd_of(&mut w, &[0x100, 0x200, 0x300, 0x400]);
    w.asked.clear();
    f.mState = 7;
    // a chase with no target ends at once: the graze times reset after a shuffle, the chase timer drawn
    // SAFETY: f outlives the herd value
    unsafe { Herd::new(&mut w, &mut f) }.update(5000);
    assert_eq!(&w.asked[..3], &[(0, 1), (0, 2), (0, 3)]);
    assert!(w.units.values().all(|u| u.graze_time == 0));
    assert_eq!((f.mState, f.mNextUpdateTime), (0, 6000));
    free(&f);
}

#[test]
fn a_leaving_herd_is_gone_once_every_unit_is_off_the_map() {
    let mut w = World::new();
    w.unit(0x100, 11, 10, 10, 40);
    let mut f = herd_of(&mut w, &[0x100]);
    f.mState = 0xa;
    w.off_map = true;
    // SAFETY: f outlives the herd value
    unsafe { Herd::new(&mut w, &mut f) }.update(5000);
    assert_eq!(f.mState, 0xb);
    free(&f);
}

#[test]
fn the_random_point_falls_back_to_the_area_after_sixteen_tries() {
    let mut tries = 0;
    let p = random_area_point(|op, _| {
        let mut io = EeHerdIo::default();
        let r = match op {
            EE_CP_HERD_HAS_MAP => 1,
            EE_CP_HERD_MAP_TILE_AT => {
                tries += 1;
                0
            }
            EE_CP_HERD_BASE_POINT => (5 << 32) | 6,
            _ => 0,
        };
        (r, io)
    });
    assert_eq!((tries, p), (16, EeHerdPoint { mX: 5, mY: 6 }));
}

#[test]
fn a_hit_animal_flees_unless_a_predator_strikes_back() {
    let mut w = World::new();
    w.unit(0x100, 11, 10, 10, 40);
    w.unit(0x900, 50, 12, 10, 80);
    w.units.get_mut(&0x900).unwrap().can_be_attacked = true;
    let mut f = herd_of(&mut w, &[0x100]);
    w.fields_of = &f as *const EeHerdFields as u64;
    w.units.get_mut(&0x100).unwrap().last_attacker = 0x900;
    w.units.get_mut(&0x100).unwrap().attack = 5;
    // a harmless animal flees, and forgets its attacker
    // SAFETY: f outlives the herd value
    unsafe { Herd::new(&mut w, &mut f) }.handle_unit_hit(0x100);
    assert_eq!((w.fled.clone(), w.goals.len()), (vec![0x100], 0));
    assert_eq!(w.units[&0x100].last_attacker, 0);
    // a predator strikes back when the draw is not below 0.75
    w.animal.mIsPredator = true;
    w.generate = 0.8;
    w.units.get_mut(&0x100).unwrap().last_attacker = 0x900;
    // SAFETY: as above
    unsafe { Herd::new(&mut w, &mut f) }.handle_unit_hit(0x100);
    assert_eq!(w.goals, vec![(EE_CP_HERD_GOAL_ATTACK, 0x100, 0x900)]);
    assert_eq!(w.fled.len(), 1);
    free(&f);
}

#[test]
fn the_whole_herd_attacks_back_from_the_last() {
    let mut w = World::new();
    w.unit(0x100, 11, 10, 10, 40);
    w.unit(0x200, 12, 10, 11, 40);
    w.unit(0x900, 50, 12, 10, 80);
    w.units.get_mut(&0x900).unwrap().can_be_attacked = true;
    let mut f = herd_of(&mut w, &[0x100, 0x200]);
    w.units.get_mut(&0x100).unwrap().last_attacker = 0x900;
    w.units.get_mut(&0x100).unwrap().attack = 5;
    // SAFETY: f outlives the herd value
    unsafe { Herd::new(&mut w, &mut f) }.attack_back(0x100, true);
    assert_eq!(w.goals, vec![(EE_CP_HERD_GOAL_ATTACK, 0x200, 0x900), (EE_CP_HERD_GOAL_ATTACK, 0x100, 0x900)]);
    // an animal that cannot hurt does not
    w.goals.clear();
    w.units.get_mut(&0x100).unwrap().attack = 0;
    // SAFETY: as above
    unsafe { Herd::new(&mut w, &mut f) }.attack_back(0x100, true);
    assert!(w.goals.is_empty());
    free(&f);
}

#[test]
fn a_surrounding_herd_attacks_when_its_follow_time_is_up() {
    let mut w = World::new();
    w.unit(0x100, 11, 10, 10, 40);
    w.unit(0x200, 12, 20, 10, 40);
    w.unit(0x900, 50, 15, 10, 80);
    // the last slot (taken first) is by the second animal
    w.slots = vec![[10.0, 10.0, 0.0, 0.0], [20.0, 10.0, 0.0, 0.0]];
    let mut f = herd_of(&mut w, &[0x100, 0x200]);
    f.mTarget = 50;
    f.mState = 2;
    f.mFollowTime = 9000;
    // the target moved: the herd surrounds it, the nearest animal to each slot from the last
    // SAFETY: f outlives the herd value
    unsafe { Herd::new(&mut w, &mut f) }.update(5000);
    let moves: Vec<(u64, u64)> = w.goals.iter().map(|g| (g.1, g.2)).collect();
    assert_eq!(moves, vec![(0x200, u64::from(20.0f32.to_bits())), (0x100, u64::from(10.0f32.to_bits()))]);
    assert_eq!((f.mCenter, f.mState, f.mNextUpdateTime), (EeHerdPoint { mX: 15, mY: 10 }, 2, 7000));
    // the follow time is up: every animal attacks (from the last), the state is the attack's
    w.goals.clear();
    // SAFETY: as above
    unsafe { Herd::new(&mut w, &mut f) }.update(9000);
    assert_eq!(w.goals, vec![(EE_CP_HERD_GOAL_ATTACK, 0x200, 0x900), (EE_CP_HERD_GOAL_ATTACK, 0x100, 0x900)]);
    assert_eq!(f.mState, 3);
    free(&f);
}
