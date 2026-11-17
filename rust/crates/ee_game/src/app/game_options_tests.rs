//! game_options.rs: the defaults, the validating copy, the lobby defaults and the saved game's bytes.

use crate::app::game_options::*;
use crate::app::save_io::*;

/// A file as FSFile writes it (4-byte longs and floats, 1-byte bools) with the strings as length-prefixed bytes; the
/// unknown variants and the operations seen.
#[derive(Default)]
struct File {
    bytes: Vec<u8>,
    at: usize,
    names: Vec<String>,
    strings: [String; 3],
    unknown_variant: Option<u32>,
    ops: Vec<(i32, u32)>,
}

impl File {
    fn take(&mut self, n: usize) -> Option<&[u8]> {
        if self.at + n > self.bytes.len() {
            return None;
        }
        self.at += n;
        Some(&self.bytes[self.at - n..self.at])
    }
    fn put_string(&mut self, s: &str) {
        self.bytes.extend_from_slice(&(s.len() as u32).to_le_bytes());
        self.bytes.extend_from_slice(s.as_bytes());
    }
    fn get_string(&mut self) -> Option<String> {
        let n = u32::from_le_bytes(self.take(4)?.try_into().unwrap()) as usize;
        Some(String::from_utf8(self.take(n)?.to_vec()).unwrap())
    }
}

const EOF: i32 = 9;

impl SaveHost for File {
    fn call(&mut self, op: i32, index: u32, io: &mut EeAppSaveIo) -> i32 {
        self.ops.push((op, index));
        match op {
            EE_APP_SAVE_WRITE_ULONG | EE_APP_SAVE_WRITE_LONG => {
                self.bytes.extend_from_slice(&(io.i as u32).to_le_bytes())
            }
            EE_APP_SAVE_WRITE_FLOAT => self.bytes.extend_from_slice(&io.f.to_le_bytes()),
            EE_APP_SAVE_WRITE_BOOL => self.bytes.push(io.b),
            EE_APP_SAVE_READ_ULONG => match self.take(4) {
                Some(b) => io.i = u32::from_le_bytes(b.try_into().unwrap()) as i64,
                None => return EOF,
            },
            EE_APP_SAVE_READ_LONG => match self.take(4) {
                Some(b) => io.i = i32::from_le_bytes(b.try_into().unwrap()) as i64,
                None => return EOF,
            },
            EE_APP_SAVE_READ_FLOAT => match self.take(4) {
                Some(b) => io.f = f32::from_le_bytes(b.try_into().unwrap()),
                None => return EOF,
            },
            EE_APP_SAVE_READ_BOOL => match self.take(1) {
                Some(b) => io.b = b[0],
                None => return EOF,
            },
            EE_APP_OPTIONS_NAME_SAVE => {
                let s = self.names[index as usize].clone();
                self.put_string(&s)
            }
            EE_APP_OPTIONS_NAME_LOAD => match self.get_string() {
                Some(s) => self.names[index as usize] = s,
                None => return EOF,
            },
            EE_APP_OPTIONS_STRING_SAVE => {
                let s = self.strings[index as usize].clone();
                self.put_string(&s)
            }
            EE_APP_OPTIONS_STRING_LOAD => match self.get_string() {
                Some(s) => self.strings[index as usize] = s,
                None => return EOF,
            },
            EE_APP_OPTIONS_VARIANT_KNOWN => return i32::from(self.unknown_variant != Some(index)),
            _ => panic!("op {op}"),
        }
        0
    }
}

fn limits() -> EeAppOptionsLimits {
    EeAppOptionsLimits {
        load_types: 6,
        resource_levels: 4,
        variants: 3,
        epochs: 15,
        map_sizes: 6,
        map_size_default: 1,
        difficulties: 3,
        speeds: 4,
        speed_default: 1,
        wonders_default: 3,
        wonders_maximum: 10,
        unit_limit_minimum: 25,
        unit_limit_maximum: 1200,
        unit_limit_default: 500,
        engine_choices: 6,
    }
}

fn sample() -> (EeAppOptionsFields, Vec<EeAppOptionsPlayerFields>, File) {
    let mut o = options_default(&limits());
    o.mGameSpeed = 2;
    o.mLoadType = 2;
    o.mVariant = 1;
    o.mMapSize = 4;
    o.mEpochStart = 3;
    o.mEpochEnd = 11;
    o.mResources = 2;
    o.mUnitLimit = 700;
    o.mWondersForVictory = 5;
    o.mDifficulty = 2;
    o.mCampaignChecksum = 0xdead_beef;
    o.mLockSpeed = true;
    o.mCheatCodes = true;
    o.mIsMultiplayer = true;
    o.mIsScenarioGame = true;
    let mut players = vec![player_default(); EE_APP_OPTIONS_SLOTS as usize];
    for (i, p) in players.iter_mut().enumerate() {
        p.mType = (i % 4) as _;
        p.mPing = i as f32 * 1.5;
        p.mSlot = i as _;
        p.mColor = (15 - i) as _;
        p.mTeam = (i / 2) as _;
        p.mStartingCitizens = (i * 3) as _;
        p.mIsLocal = i == 0;
        p.mIsReady = i % 2 == 1;
        p.mB36 = i % 3 != 0;
        p.mEngine = (i % 6) as _;
    }
    let f = File {
        names: (0..EE_APP_OPTIONS_SLOTS).map(|i| format!("slot {i}")).collect(),
        strings: ["Roman".into(), "a conqueror".into(), "map x".into()],
        ..Default::default()
    };
    (o, players, f)
}

#[test]
fn defaults_are_the_constructors() {
    let p = player_default();
    assert_eq!((p.mType, p.mLoadedType, p.mB36, p.mEngine), (3, 3, true, 0));
    let o = options_default(&limits());
    assert_eq!((o.mEpochStart, o.mEpochEnd, o.mUnitLimit, o.mMapSize, o.mGameSpeed), (15, 14, 50, 1, 1));
    assert_eq!((o.mWondersForVictory, o.mVictory, o.mB407, o.mCheatCodes), (3, true, true, false));
}

#[test]
fn save_then_load_is_the_same_and_in_file_order() {
    let (o, players, mut f) = sample();
    assert_eq!(save(&mut f, &o, &players), 0);
    // 16 slots of (4 + name + 4*6 + 3 + 4) bytes, the options' 10 longs, 8 bools, 2 strings, a long, a bool, a string
    let names: usize = (0..16).map(|i| 4 + format!("slot {i}").len()).sum();
    let strings = 4 + 5 + 4 + 11 + 4 + 5;
    assert_eq!(f.bytes.len(), 16 * (4 + 20 + 3 + 4) + names + 40 + 8 + strings + 4 + 1);
    let bytes = f.bytes.clone();
    let mut g = File { bytes, names: vec![String::new(); 16], ..Default::default() };
    let mut o2 = options_default(&limits());
    let mut p2 = vec![player_default(); 16];
    assert_eq!(load(&mut g, &mut o2, &mut p2, 0, 280, &limits()), 0);
    assert_eq!(g.at, g.bytes.len());
    let mut want = players.clone();
    for p in &mut want {
        p.mLoadedType = p.mType as _;
    }
    assert_eq!(p2, want);
    let mut o_want = o;
    o_want.mB407 = o2.mB407; // not saved
    assert_eq!(o2, o_want);
    assert_eq!(g.names, f.names);
    assert_eq!(g.strings, f.strings);
}

#[test]
fn old_saves_renumber_the_engine_and_lack_the_map_name() {
    let (o, players, mut f) = sample();
    save(&mut f, &o, &players);
    // 0.252: 1 was the first AI, 2 random
    let mut g = File { bytes: f.bytes.clone(), names: vec![String::new(); 16], ..Default::default() };
    let mut p2 = vec![player_default(); 16];
    let mut o2 = options_default(&limits());
    load(&mut g, &mut o2, &mut p2, 0, 252, &limits());
    let engines: Vec<_> = p2.iter().map(|p| p.mEngine).collect();
    assert_eq!(&engines[..6], &[0, 2, 1, 0, 0, 0]);
    // before 0.200 the map name is not read (nor, before 0.251, the engine): the save without each slot's engine
    // and the map name
    let mut old = Vec::new();
    let mut at = 0;
    for i in 0..16 {
        let n = 4 + 4 + format!("slot {i}").len() + 20 + 3;
        old.extend_from_slice(&f.bytes[at..at + n]);
        at += n + 4;
    }
    old.extend_from_slice(&f.bytes[at..f.bytes.len() - (4 + 5)]);
    let mut g = File { bytes: old, names: vec![String::new(); 16], ..Default::default() };
    assert_eq!(load(&mut g, &mut o2, &mut p2, 0, 199, &limits()), 0);
    assert_eq!(g.at, g.bytes.len());
    assert!(!g.ops.contains(&(EE_APP_OPTIONS_STRING_LOAD, EE_APP_OPTIONS_MAP_NAME)));
    assert_eq!((o2.mCampaignChecksum, p2[5].mEngine, p2[15].mColor), (0xdead_beef, 0, 0));
    assert!(g.ops.contains(&(EE_APP_OPTIONS_STRING_LOAD, EE_APP_OPTIONS_SCENARIO)));
}

#[test]
fn load_falls_back_for_unknown_resources_and_variants() {
    let (mut o, players, mut f) = sample();
    o.mResources = 9;
    o.mVariant = 2;
    save(&mut f, &o, &players);
    let mut g =
        File { bytes: f.bytes.clone(), names: vec![String::new(); 16], unknown_variant: Some(2), ..Default::default() };
    let mut o2 = options_default(&limits());
    let mut p2 = vec![player_default(); 16];
    load(&mut g, &mut o2, &mut p2, 0, 280, &limits());
    assert_eq!((o2.mResources, o2.mVariant), (0, 0));
    // a variant out of range is not asked about
    o.mVariant = 7;
    let mut f = File { names: f.names.clone(), strings: f.strings.clone(), ..Default::default() };
    save(&mut f, &o, &players);
    let mut g = File { bytes: f.bytes.clone(), names: vec![String::new(); 16], ..Default::default() };
    load(&mut g, &mut o2, &mut p2, 0, 280, &limits());
    assert_eq!(o2.mVariant, 0);
    assert!(!g.ops.iter().any(|&(op, _)| op == EE_APP_OPTIONS_VARIANT_KNOWN));
}

#[test]
fn a_read_error_stops_the_load() {
    let (o, players, mut f) = sample();
    save(&mut f, &o, &players);
    f.bytes.truncate(10);
    let mut g = File { bytes: f.bytes.clone(), names: vec![String::new(); 16], ..Default::default() };
    let mut o2 = options_default(&limits());
    let mut p2 = vec![player_default(); 16];
    assert_eq!(load(&mut g, &mut o2, &mut p2, 0, 280, &limits()), EOF);
    // the first slot's type read, its name failed; nothing after the failure
    let reads = g.ops.iter().filter(|&&(op, _)| op != EE_APP_OPTIONS_VARIANT_KNOWN).count();
    assert_eq!(reads, 2);
}

#[test]
fn assign_validates_each_setting() {
    let l = limits();
    let mut src = options_default(&l);
    src.mLoadType = 6;
    src.mResources = 4;
    src.mVariant = 3;
    src.mEpochStart = 16;
    src.mEpochEnd = 2;
    src.mMapSize = 6;
    src.mDifficulty = 3;
    src.mGameSpeed = 4;
    src.mWondersForVictory = 11;
    src.mUnitLimit = 1201;
    src.mB407 = false;
    let mut dst = options_default(&l);
    assign(&mut dst, &src, &l);
    assert_eq!((dst.mLoadType, dst.mResources, dst.mVariant, dst.mEpochStart), (0, 0, 0, 0));
    // the end falls back to the start already chosen
    assert_eq!(dst.mEpochEnd, 0);
    assert_eq!((dst.mMapSize, dst.mDifficulty, dst.mGameSpeed), (1, 0, 1));
    assert_eq!((dst.mWondersForVictory, dst.mUnitLimit, dst.mB407), (0, 25, true));
    // random start epochs (== the count) keep any end below the count
    src.mEpochStart = 15;
    src.mEpochEnd = 3;
    assign(&mut dst, &src, &l);
    assert_eq!((dst.mEpochStart, dst.mEpochEnd), (15, 3));
    src.mEpochStart = 5;
    assign(&mut dst, &src, &l);
    assert_eq!((dst.mEpochStart, dst.mEpochEnd), (5, 5));
    let (s, _, _) = sample();
    let mut d = options_default(&l);
    assign(&mut d, &s, &l);
    assert_eq!(d, s);
}

#[test]
fn set_defaults_takes_the_preferred_difficulty_when_known() {
    let l = limits();
    let (mut o, _, _) = sample();
    set_defaults(&mut o, &l, 2);
    assert_eq!((o.mResources, o.mEpochStart, o.mEpochEnd, o.mUnitLimit, o.mDifficulty), (3, 15, 14, 500, 2));
    assert_eq!((o.mVictory, o.mCheatCodes, o.mIsMultiplayer, o.mIsScenarioGame), (false, true, false, false));
    // untouched: the checksum (the lobby's copy)
    assert_eq!(o.mCampaignChecksum, 0xdead_beef);
    set_defaults(&mut o, &l, 3);
    assert_eq!(o.mDifficulty, 0);
}
